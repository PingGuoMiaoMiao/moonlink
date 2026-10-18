#include "sessions_internal.h"

#include <mferror.h>
#include <strmif.h>
#include <string.h>

/* ── Helpers ─────────────────────────────────────────────────────────── */

/* CLSID of the Microsoft H.264 decoder MFT. Declared locally so the native
   bridge does not need to link the codec DSP UUID library. */
static const GUID moonlink_ms_h264_decoder_clsid = {
  0x62CE7E72, 0x4C71, 0x4D20,
  { 0xB1, 0x5D, 0x45, 0x28, 0x31, 0xA8, 0x7D, 0x9D }
};



static HRESULT decoder_set_attribute_pair(
  IMFAttributes *attributes,
  REFGUID key,
  UINT32 upper,
  UINT32 lower
) {
  return IMFAttributes_SetUINT64(
    attributes,
    key,
    ((UINT64)upper << 32) | (UINT64)lower
  );
}

static inline int clamp_int(int value, int min_val, int max_val) {
  if (value < min_val) return min_val;
  if (value > max_val) return max_val;
  return value;
}

static void convert_nv12_to_bgra(
  const uint8_t *nv12_y,
  int stride_y,
  const uint8_t *nv12_uv,
  int stride_uv,
  uint8_t *bgra,
  int bgra_stride,
  int width,
  int height
) {
  for (int row = 0; row < height; row += 1) {
    const uint8_t *y_row = nv12_y + row * stride_y;
    const uint8_t *uv_row = nv12_uv + (row / 2) * stride_uv;
    uint8_t *dst = bgra + row * bgra_stride;
    for (int col = 0; col < width; col += 1) {
      int y = (int)y_row[col];
      int uv_index = (col & ~1);
      int u = (int)uv_row[uv_index];
      int v = (int)uv_row[uv_index + 1];

      /* Integer arithmetic scaled by 1000 */
      int c = 1164 * (y - 16);
      int d = 1596 * (v - 128);
      int e = 813 * (v - 128);
      int f = 391 * (u - 128);
      int g = 2018 * (u - 128);

      int r = (c + d) / 1000;
      int gr = (c - e - f) / 1000;
      int b = (c + g) / 1000;

      dst[col * 4 + 0] = (uint8_t)clamp_int(b, 0, 255);
      dst[col * 4 + 1] = (uint8_t)clamp_int(gr, 0, 255);
      dst[col * 4 + 2] = (uint8_t)clamp_int(r, 0, 255);
      dst[col * 4 + 3] = 255;
    }
  }
}

/* ── Public API ──────────────────────────────────────────────────────── */

/* ICodecAPI and CODECAPI_AVDecVideoAcceleration_H264, declared locally so the
   bridge does not need to link the DirectShow UUID library. */
static const GUID moonlink_iid_icodecapi = {
  0x901DB4C7, 0x31CE, 0x41A2,
  { 0x85, 0xDC, 0x8F, 0xA8, 0xBF, 0x41, 0xD9, 0x04 }
};

static const GUID moonlink_codecapi_avdec_h264_accel = {
  0xF7DB8A2F, 0x4F48, 0x466D,
  { 0x8E, 0xD0, 0xAE, 0xC7, 0xC3, 0x24, 0x93, 0x1E }
};

/* The system H.264 decoder MFT defaults to DXVA, which only produces output
   when a D3D11 device manager is attached. Disabling hardware acceleration
   makes it decode in software into caller-provided buffers. */
static void decoder_disable_hardware_acceleration(IMFTransform *transform) {
  ICodecAPI *codec_api = NULL;
  if (SUCCEEDED(IMFTransform_QueryInterface(
    transform,
    &moonlink_iid_icodecapi,
    (void **)&codec_api
  ))) {
    VARIANT value;
    memset(&value, 0, sizeof(value));
    value.vt = VT_UI4;
    value.ulVal = 0;
    ICodecAPI_SetValue(codec_api, &moonlink_codecapi_avdec_h264_accel, &value);
    ICodecAPI_Release(codec_api);
  }
}

/* Copy one Annex-B NAL unit (payload only) into dst. Returns the payload size,
   or 0 when the requested NAL type is absent. */
static int decoder_find_nal(
  const uint8_t *data,
  int size,
  int wanted_type,
  uint8_t *dst,
  int dst_capacity
) {
  int i = 0;
  while (i + 3 < size) {
    int start = -1;
    if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1) {
      start = i + 3;
    } else if (
      i + 4 < size && data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 0 &&
      data[i + 3] == 1
    ) {
      start = i + 4;
    }
    if (start < 0) {
      i += 1;
      continue;
    }

    int end = size;
    for (int j = start; j + 3 <= size; j += 1) {
      if (data[j] == 0 && data[j + 1] == 0 && (data[j + 2] == 1 ||
          (j + 3 < size && data[j + 2] == 0 && data[j + 3] == 1))) {
        end = j;
        break;
      }
    }

    int payload_size = end - start;
    if (payload_size > 0 && (data[start] & 0x1F) == wanted_type &&
        payload_size <= dst_capacity) {
      memcpy(dst, data + start, (size_t)payload_size);
      return payload_size;
    }
    i = end;
  }
  return 0;
}

/* The Microsoft H.264 decoder configures its accelerator from the MPEG sequence
   header. Without it the transform accepts input and never emits a sample, so
   SPS and PPS are extracted from the first access unit and applied to the input
   media type before decoding starts. */
static HRESULT decoder_apply_sequence_header(
  moonlink_decoder_session *session,
  const uint8_t *data,
  int size
) {
  uint8_t sps[512];
  uint8_t pps[512];
  int sps_size = decoder_find_nal(data, size, 7, sps, (int)sizeof(sps));
  int pps_size = decoder_find_nal(data, size, 8, pps, (int)sizeof(pps));
  if (sps_size <= 0 || pps_size <= 0) {
    return E_FAIL;
  }

  uint8_t header[1024];
  int header_size = 0;
  const uint8_t start_code[4] = { 0x00, 0x00, 0x00, 0x01 };
  memcpy(header, start_code, 4);
  header_size += 4;
  memcpy(header + header_size, sps, (size_t)sps_size);
  header_size += sps_size;
  memcpy(header + header_size, start_code, 4);
  header_size += 4;
  memcpy(header + header_size, pps, (size_t)pps_size);
  header_size += pps_size;

  IMFMediaType *media_type = NULL;
  HRESULT result = MFCreateMediaType(&media_type);
  if (FAILED(result)) {
    return result;
  }
  result = IMFMediaType_SetGUID(media_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
  if (SUCCEEDED(result)) {
    result = IMFMediaType_SetGUID(media_type, &MF_MT_SUBTYPE, &MFVideoFormat_H264);
  }
  if (SUCCEEDED(result)) {
    result = decoder_set_attribute_pair(
      (IMFAttributes *)media_type,
      &MF_MT_FRAME_SIZE,
      (UINT32)session->width,
      (UINT32)session->height
    );
  }
  if (SUCCEEDED(result)) {
    result = IMFMediaType_SetUINT32(
      media_type,
      &MF_MT_INTERLACE_MODE,
      MFVideoInterlace_Progressive
    );
  }
  if (SUCCEEDED(result)) {
    result = IMFMediaType_SetBlob(
      media_type,
      &MF_MT_MPEG_SEQUENCE_HEADER,
      header,
      (UINT32)header_size
    );
  }
  if (SUCCEEDED(result)) {
    result = IMFTransform_SetInputType(session->decoder, 0, media_type, 0);
  }
  IMFMediaType_Release(media_type);
  return result;
}

/* Configure the H.264 input type and the RGB32 output type, falling back to
   NV12 when the decoder refuses RGB32. */
static HRESULT decoder_configure_transform(
  IMFTransform *transform,
  int width,
  int height,
  BOOL *out_is_nv12
) {
  IMFMediaType *media_type = NULL;
  HRESULT result;

  *out_is_nv12 = FALSE;

  result = MFCreateMediaType(&media_type);
  if (FAILED(result)) {
    return result;
  }
  result = IMFMediaType_SetGUID(media_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
  if (SUCCEEDED(result)) {
    result = IMFMediaType_SetGUID(media_type, &MF_MT_SUBTYPE, &MFVideoFormat_H264);
  }
  if (SUCCEEDED(result)) {
    result = decoder_set_attribute_pair(
      (IMFAttributes *)media_type,
      &MF_MT_FRAME_SIZE,
      (UINT32)width,
      (UINT32)height
    );
  }
  if (SUCCEEDED(result)) {
    result = IMFMediaType_SetUINT32(
      media_type,
      &MF_MT_INTERLACE_MODE,
      MFVideoInterlace_Progressive
    );
  }
  if (SUCCEEDED(result)) {
    result = IMFTransform_SetInputType(transform, 0, media_type, 0);
  }
  IMFMediaType_Release(media_type);
  if (FAILED(result)) {
    return result;
  }

  result = MFCreateMediaType(&media_type);
  if (FAILED(result)) {
    return result;
  }
  result = IMFMediaType_SetGUID(media_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
  if (SUCCEEDED(result)) {
    result = IMFMediaType_SetGUID(media_type, &MF_MT_SUBTYPE, &MFVideoFormat_RGB32);
  }
  if (SUCCEEDED(result)) {
    result = decoder_set_attribute_pair(
      (IMFAttributes *)media_type,
      &MF_MT_FRAME_SIZE,
      (UINT32)width,
      (UINT32)height
    );
  }
  if (SUCCEEDED(result)) {
    result = IMFMediaType_SetUINT32(
      media_type,
      &MF_MT_INTERLACE_MODE,
      MFVideoInterlace_Progressive
    );
  }
  if (SUCCEEDED(result)) {
    result = IMFTransform_SetOutputType(transform, 0, media_type, 0);
  }
  IMFMediaType_Release(media_type);
  if (SUCCEEDED(result)) {
    return S_OK;
  }

  result = MFCreateMediaType(&media_type);
  if (FAILED(result)) {
    return result;
  }
  result = IMFMediaType_SetGUID(media_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
  if (SUCCEEDED(result)) {
    result = IMFMediaType_SetGUID(media_type, &MF_MT_SUBTYPE, &MFVideoFormat_NV12);
  }
  if (SUCCEEDED(result)) {
    result = decoder_set_attribute_pair(
      (IMFAttributes *)media_type,
      &MF_MT_FRAME_SIZE,
      (UINT32)width,
      (UINT32)height
    );
  }
  if (SUCCEEDED(result)) {
    result = IMFMediaType_SetUINT32(
      media_type,
      &MF_MT_INTERLACE_MODE,
      MFVideoInterlace_Progressive
    );
  }
  if (SUCCEEDED(result)) {
    result = IMFTransform_SetOutputType(transform, 0, media_type, 0);
  }
  IMFMediaType_Release(media_type);
  if (SUCCEEDED(result)) {
    *out_is_nv12 = TRUE;
  }
  return result;
}

moonlink_native_result moonlink_decoder_create(
  const moonlink_decoder_config *config,
  moonlink_decoder_handle *out_handle
) {
  if (config == NULL || out_handle == NULL) {
    return MOONLINK_RESULT_INVALID_HANDLE;
  }
  *out_handle = NULL;

  struct moonlink_decoder_session *session = NULL;
  IMFActivate **activations = NULL;
  UINT32 activation_count = 0;
  HRESULT result;

  session = (struct moonlink_decoder_session *)calloc(1, sizeof(*session));
  if (session == NULL) {
    return MOONLINK_RESULT_OUT_OF_MEMORY;
  }
  session->width = config->width;
  session->height = config->height;
  session->stride = config->width * 4;
  session->stopped = FALSE;
  session->output_is_nv12 = FALSE;

  result = MFStartup(MF_VERSION, MFSTARTUP_FULL);
  if (FAILED(result)) {
    goto cleanup;
  }

  /* Create the decoder. The Microsoft software H.264 MFT is preferred:
     hardware decoder MFTs advertise MFT_OUTPUT_STREAM_PROVIDES_SAMPLES and
     need a D3D11 device manager, and without one they accept input but never
     emit a sample. */
  CoInitializeEx(NULL, COINIT_MULTITHREADED);

  IMFTransform *transform = NULL;
  BOOL is_nv12 = FALSE;

  if (SUCCEEDED(CoCreateInstance(
    &moonlink_ms_h264_decoder_clsid,
    NULL,
    CLSCTX_INPROC_SERVER,
    &IID_IMFTransform,
    (void **)&transform
  ))) {
    decoder_disable_hardware_acceleration(transform);
    if (FAILED(decoder_configure_transform(
      transform,
      config->width,
      config->height,
      &is_nv12
    ))) {
      IMFTransform_Release(transform);
      transform = NULL;
    }
  }

  if (transform == NULL) {
    /* Fall back to any synchronous decoder that accepts the types. */
    MFT_REGISTER_TYPE_INFO input_type_info = {
      MFMediaType_Video,
      MFVideoFormat_H264,
    };
    result = MFTEnumEx(
      MFT_CATEGORY_VIDEO_DECODER,
      MFT_ENUM_FLAG_SYNCMFT,
      &input_type_info,
      NULL,
      &activations,
      &activation_count
    );
    if (SUCCEEDED(result)) {
      for (UINT32 index = 0; index < activation_count; index += 1) {
        IMFTransform *candidate = NULL;
        if (FAILED(IMFActivate_ActivateObject(
          activations[index],
          &IID_IMFTransform,
          (void **)&candidate
        ))) {
          continue;
        }
        BOOL candidate_nv12 = FALSE;
        if (SUCCEEDED(decoder_configure_transform(
          candidate,
          config->width,
          config->height,
          &candidate_nv12
        ))) {
          transform = candidate;
          is_nv12 = candidate_nv12;
          IMFActivate_AddRef(activations[index]);
          session->activation = activations[index];
          break;
        }
        IMFTransform_Release(candidate);
      }
    }
  }

  if (transform == NULL) {
    result = E_FAIL;
    goto cleanup;
  }

  session->decoder = transform;
  session->output_is_nv12 = is_nv12;

  /* Get output stream info */
  result = IMFTransform_GetOutputStreamInfo(session->decoder, 0, &session->stream_info);
  if (FAILED(result)) {
    goto cleanup;
  }

  /* Begin streaming. START_OF_STREAM is required in addition to
     BEGIN_STREAMING: without it the H.264 decoder accepts input but never
     emits a sample. */
  result = IMFTransform_ProcessMessage(
    session->decoder,
    MFT_MESSAGE_NOTIFY_BEGIN_STREAMING,
    0
  );
  if (FAILED(result)) {
    goto cleanup;
  }
  result = IMFTransform_ProcessMessage(
    session->decoder,
    MFT_MESSAGE_NOTIFY_START_OF_STREAM,
    0
  );
  if (FAILED(result)) {
    goto cleanup;
  }
  session->next_timestamp = 0;
  session->frame_duration = 10 * 1000 * 1000 / 60;

  /* Allocate BGRA output buffer */
  session->bgra_buffer = (uint8_t *)calloc(
    (size_t)session->width * (size_t)session->height * 4, 1
  );
  if (session->bgra_buffer == NULL) {
    result = E_OUTOFMEMORY;
    goto cleanup;
  }

  /* If output is NV12, allocate conversion buffer */
  if (session->output_is_nv12) {
    size_t nv12_size = (size_t)session->width * (size_t)session->height * 3 / 2;
    session->nv12_buffer = (uint8_t *)calloc(nv12_size, 1);
    if (session->nv12_buffer == NULL) {
      result = E_OUTOFMEMORY;
      goto cleanup;
    }
  }

  /* Set up cached frame */
  session->cached_frame.bgra_data = session->bgra_buffer;
  session->cached_frame.width = session->width;
  session->cached_frame.height = session->height;
  session->cached_frame.stride = session->stride;

  *out_handle = session;
  session = NULL;
  result = S_OK;

cleanup:
  if (activations != NULL) {
    for (UINT32 index = 0; index < activation_count; index += 1) {
      IMFActivate_Release(activations[index]);
    }
    CoTaskMemFree(activations);
  }
  if (session != NULL) {
    if (session->decoder != NULL) {
      IMFTransform_Release(session->decoder);
    }
    if (session->activation != NULL) {
      IMFActivate_ShutdownObject(session->activation);
      IMFActivate_Release(session->activation);
    }
    free(session->bgra_buffer);
    free(session->nv12_buffer);
    free(session);
    MFShutdown();
  }
  if (FAILED(result) && *out_handle == NULL) {
    return MOONLINK_RESULT_DECODER_UNAVAILABLE;
  }
  return MOONLINK_RESULT_OK;
}

moonlink_native_result moonlink_decoder_decode(
  moonlink_decoder_handle handle,
  const uint8_t *data,
  int size,
  const moonlink_decoded_frame **out_frame
) {
  if (handle == NULL || out_frame == NULL) {
    return MOONLINK_RESULT_INVALID_HANDLE;
  }
  *out_frame = NULL;

  if (handle->stopped) {
    return MOONLINK_RESULT_ALREADY_STOPPED;
  }

  if (data == NULL || size <= 0) {
    return MOONLINK_RESULT_INVALID_HANDLE;
  }

  IMFMediaBuffer *input_buffer = NULL;
  IMFSample *input_sample = NULL;
  BYTE *buffer_data = NULL;
  HRESULT result;

  if (!handle->sequence_header_applied) {
    if (SUCCEEDED(decoder_apply_sequence_header(handle, data, size))) {
      handle->sequence_header_applied = TRUE;
    }
  }

  /* Create input sample from H.264 data */
  result = MFCreateMemoryBuffer((DWORD)size, &input_buffer);
  if (FAILED(result)) {
    goto cleanup;
  }

  result = IMFMediaBuffer_Lock(input_buffer, &buffer_data, NULL, NULL);
  if (FAILED(result)) {
    goto cleanup;
  }
  memcpy(buffer_data, data, (size_t)size);
  result = IMFMediaBuffer_Unlock(input_buffer);
  buffer_data = NULL;
  if (FAILED(result)) {
    goto cleanup;
  }

  result = IMFMediaBuffer_SetCurrentLength(input_buffer, (DWORD)size);
  if (FAILED(result)) {
    goto cleanup;
  }

  result = MFCreateSample(&input_sample);
  if (FAILED(result)) {
    goto cleanup;
  }

  result = IMFSample_AddBuffer(input_sample, input_buffer);
  if (FAILED(result)) {
    goto cleanup;
  }

  /* Timestamps are part of the decoder contract; monotonic sample times also
     let the decoder order output when a frame spans several packets. */
  result = IMFSample_SetSampleTime(input_sample, handle->next_timestamp);
  if (FAILED(result)) {
    goto cleanup;
  }
  result = IMFSample_SetSampleDuration(input_sample, handle->frame_duration);
  if (FAILED(result)) {
    goto cleanup;
  }
  handle->next_timestamp += handle->frame_duration;

  /* Feed into decoder */
  result = IMFTransform_ProcessInput(handle->decoder, 0, input_sample, 0);
  if (FAILED(result)) {
    goto cleanup;
  }

  /* Try to get output */
  for (;;) {
    MFT_OUTPUT_DATA_BUFFER output_data = { 0 };
    IMFSample *output_sample = NULL;
    IMFMediaBuffer *output_buffer_obj = NULL;
    DWORD process_status = 0;

    output_data.dwStreamID = 0;

    /* Only skip allocation when the MFT insists on allocating output itself.
       A decoder that merely *can* provide samples still returns S_OK with a
       null sample when the caller passes none, which silently drops frames. */
    if ((handle->stream_info.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES) == 0) {
      if (handle->stream_info.cbSize == 0) {
        break;
      }
      result = MFCreateAlignedMemoryBuffer(
        handle->stream_info.cbSize,
        handle->stream_info.cbAlignment,
        &output_buffer_obj
      );
      if (FAILED(result)) {
        break;
      }
      result = MFCreateSample(&output_sample);
      if (FAILED(result)) {
        IMFMediaBuffer_Release(output_buffer_obj);
        output_buffer_obj = NULL;
        break;
      }
      result = IMFSample_AddBuffer(output_sample, output_buffer_obj);
      IMFMediaBuffer_Release(output_buffer_obj);
      output_buffer_obj = NULL;
      if (FAILED(result)) {
        IMFSample_Release(output_sample);
        output_sample = NULL;
        break;
      }
      output_data.pSample = output_sample;
    }

    result = IMFTransform_ProcessOutput(
      handle->decoder,
      0,
      1,
      &output_data,
      &process_status
    );

    if (output_data.pEvents != NULL) {
      IMFCollection_Release(output_data.pEvents);
    }

    if (result == MF_E_TRANSFORM_NEED_MORE_INPUT) {
      if (output_data.pSample != NULL) {
        IMFSample_Release(output_data.pSample);
      }
      if (output_sample != NULL && output_data.pSample != output_sample) {
        IMFSample_Release(output_sample);
      }
      *out_frame = NULL;
      result = S_OK;
      goto cleanup;
    }

    if (FAILED(result)) {
      if (output_data.pSample != NULL) {
        IMFSample_Release(output_data.pSample);
      }
      if (output_sample != NULL && output_data.pSample != output_sample) {
        IMFSample_Release(output_sample);
      }
      goto cleanup;
    }

    /* Got an output sample */
    IMFSample *resolved_sample = output_data.pSample;
    if (resolved_sample == NULL) {
      if (output_sample != NULL) {
        IMFSample_Release(output_sample);
      }
      break;
    }

    IMFMediaBuffer *media_buffer = NULL;
    BYTE *locked_data = NULL;
    DWORD locked_length = 0;

    result = IMFSample_ConvertToContiguousBuffer(resolved_sample, &media_buffer);
    if (FAILED(result)) {
      IMFSample_Release(resolved_sample);
      if (output_sample != NULL && resolved_sample != output_sample) {
        IMFSample_Release(output_sample);
      }
      goto cleanup;
    }

    result = IMFMediaBuffer_Lock(media_buffer, &locked_data, NULL, &locked_length);
    if (FAILED(result)) {
      IMFMediaBuffer_Release(media_buffer);
      IMFSample_Release(resolved_sample);
      if (output_sample != NULL && resolved_sample != output_sample) {
        IMFSample_Release(output_sample);
      }
      goto cleanup;
    }

    if (handle->output_is_nv12) {
      /* Convert NV12 → BGRA */
      int y_size = handle->width * handle->height;
      const uint8_t *y_plane = (const uint8_t *)locked_data;
      const uint8_t *uv_plane = y_plane + y_size;
      convert_nv12_to_bgra(
        y_plane,
        handle->width,
        uv_plane,
        handle->width,
        handle->bgra_buffer,
        handle->stride,
        handle->width,
        handle->height
      );
    } else {
      /* Already BGRA — copy directly */
      size_t copy_size = (size_t)handle->width * (size_t)handle->height * 4;
      if (locked_length >= copy_size) {
        memcpy(handle->bgra_buffer, locked_data, copy_size);
      }
    }

    IMFMediaBuffer_Unlock(media_buffer);
    IMFMediaBuffer_Release(media_buffer);
    IMFSample_Release(resolved_sample);
    if (output_sample != NULL && resolved_sample != output_sample) {
      IMFSample_Release(output_sample);
    }

    /* Fill cached frame */
    handle->cached_frame.bgra_data = handle->bgra_buffer;
    handle->cached_frame.width = handle->width;
    handle->cached_frame.height = handle->height;
    handle->cached_frame.stride = handle->stride;
    *out_frame = &handle->cached_frame;
    result = S_OK;
    goto cleanup;
  }

  *out_frame = NULL;
  result = S_OK;

cleanup:
  if (buffer_data != NULL && input_buffer != NULL) {
    IMFMediaBuffer_Unlock(input_buffer);
  }
  if (input_sample != NULL) {
    IMFSample_Release(input_sample);
  }
  if (input_buffer != NULL) {
    IMFMediaBuffer_Release(input_buffer);
  }
  if (FAILED(result)) {
    return MOONLINK_RESULT_INTERNAL_ERROR;
  }
  return MOONLINK_RESULT_OK;
}

moonlink_native_result moonlink_decoder_stop(moonlink_decoder_handle handle) {
  if (handle == NULL) {
    return MOONLINK_RESULT_INVALID_HANDLE;
  }
  if (handle->stopped) {
    return MOONLINK_RESULT_ALREADY_STOPPED;
  }

  HRESULT result = IMFTransform_ProcessMessage(
    handle->decoder,
    MFT_MESSAGE_NOTIFY_END_OF_STREAM,
    0
  );
  if (SUCCEEDED(result)) {
    result = IMFTransform_ProcessMessage(
      handle->decoder,
      MFT_MESSAGE_COMMAND_DRAIN,
      0
    );
  }

  /* Drain remaining output (discard) */
  while (SUCCEEDED(result)) {
    MFT_OUTPUT_DATA_BUFFER output_data = { 0 };
    DWORD process_status = 0;
    output_data.dwStreamID = 0;

    IMFSample *drain_sample = NULL;
    IMFMediaBuffer *drain_buffer = NULL;

    /* Only skip allocation when the MFT insists on allocating output itself.
       A decoder that merely *can* provide samples still returns S_OK with a
       null sample when the caller passes none, which silently drops frames. */
    if ((handle->stream_info.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES) == 0) {
      if (handle->stream_info.cbSize == 0) {
        break;
      }
      result = MFCreateAlignedMemoryBuffer(
        handle->stream_info.cbSize,
        handle->stream_info.cbAlignment,
        &drain_buffer
      );
      if (FAILED(result)) {
        break;
      }
      result = MFCreateSample(&drain_sample);
      if (FAILED(result)) {
        IMFMediaBuffer_Release(drain_buffer);
        drain_buffer = NULL;
        break;
      }
      result = IMFSample_AddBuffer(drain_sample, drain_buffer);
      IMFMediaBuffer_Release(drain_buffer);
      drain_buffer = NULL;
      if (FAILED(result)) {
        IMFSample_Release(drain_sample);
        drain_sample = NULL;
        break;
      }
      output_data.pSample = drain_sample;
    }

    result = IMFTransform_ProcessOutput(
      handle->decoder,
      0,
      1,
      &output_data,
      &process_status
    );

    if (output_data.pEvents != NULL) {
      IMFCollection_Release(output_data.pEvents);
    }

    if (output_data.pSample != NULL) {
      IMFSample_Release(output_data.pSample);
    }
    if (drain_sample != NULL && output_data.pSample != drain_sample) {
      IMFSample_Release(drain_sample);
    }

    if (result == MF_E_TRANSFORM_NEED_MORE_INPUT) {
      break;
    }
    if (FAILED(result)) {
      break;
    }
  }

  handle->stopped = TRUE;
  return MOONLINK_RESULT_OK;
}

moonlink_native_result moonlink_decoder_destroy(moonlink_decoder_handle handle) {
  if (handle == NULL) {
    return MOONLINK_RESULT_INVALID_HANDLE;
  }

  if (!handle->stopped) {
    moonlink_decoder_stop(handle);
  }

  if (handle->decoder != NULL) {
    IMFTransform_ProcessMessage(
      handle->decoder,
      MFT_MESSAGE_NOTIFY_END_STREAMING,
      0
    );
  }

  if (handle->activation != NULL) {
    IMFActivate_ShutdownObject(handle->activation);
    IMFActivate_Release(handle->activation);
    handle->activation = NULL;
  }

  if (handle->decoder != NULL) {
    IMFTransform_Release(handle->decoder);
    handle->decoder = NULL;
  }

  free(handle->bgra_buffer);
  handle->bgra_buffer = NULL;

  free(handle->nv12_buffer);
  handle->nv12_buffer = NULL;

  free(handle);

  MFShutdown();
  return MOONLINK_RESULT_OK;
}
