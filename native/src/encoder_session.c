#include "sessions_internal.h"

#include <mferror.h>
#include <string.h>

/* ── Helpers ───────────────────────────────────────────────────────────── */

static HRESULT encoder_set_attribute_pair(
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

static void encoder_free_slots(struct moonlink_encoder_session *session) {
  for (int i = 0; i < ENCODER_MAX_CACHED_PACKETS; i += 1) {
    if (session->slots[i].buffer != NULL) {
      free(session->slots[i].buffer);
      session->slots[i].buffer = NULL;
      session->slots[i].capacity = 0;
    }
  }
  session->cached_packet_count = 0;
}

/* ── moonlink_encoder_create ───────────────────────────────────────────── */

moonlink_native_result moonlink_encoder_create(
  const moonlink_encoder_config *config,
  moonlink_encoder_handle *out_handle
) {
  if (config == NULL || out_handle == NULL) {
    return MOONLINK_RESULT_INVALID_HANDLE;
  }
  *out_handle = NULL;

  struct moonlink_encoder_session *session = calloc(1, sizeof(*session));
  if (session == NULL) {
    return MOONLINK_RESULT_OUT_OF_MEMORY;
  }

  session->width = config->width;
  session->height = config->height;
  session->fps = config->fps;
  session->bitrate = config->bitrate;
  session->use_hardware = config->use_hardware ? TRUE : FALSE;
  session->stopped = FALSE;
  session->frame_duration = (LONGLONG)10000000 / (LONGLONG)config->fps;
  session->next_timestamp = 0;
  session->cached_packet_count = 0;

  IMFActivate **activations = NULL;
  UINT32 activation_count = 0;
  IMFMediaType *output_type = NULL;
  IMFMediaType *input_type = NULL;
  HRESULT hr;
  moonlink_native_result result = MOONLINK_RESULT_OK;

  /* MFStartup */
  hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
  if (FAILED(hr)) {
    result = MOONLINK_RESULT_ENCODER_UNAVAILABLE;
    goto cleanup;
  }

  /* Enumerate H.264 encoders */
  MFT_REGISTER_TYPE_INFO output_type_info = {
    MFMediaType_Video,
    MFVideoFormat_H264,
  };
  UINT32 enum_flags = session->use_hardware
    ? MFT_ENUM_FLAG_HARDWARE
    : (MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_ASYNCMFT);

  hr = MFTEnumEx(
    MFT_CATEGORY_VIDEO_ENCODER,
    enum_flags,
    NULL,
    &output_type_info,
    &activations,
    &activation_count
  );
  if (FAILED(hr) || activation_count == 0) {
    result = MOONLINK_RESULT_ENCODER_UNAVAILABLE;
    goto cleanup;
  }

  /* Try each activation until one works */
  for (UINT32 index = 0; index < activation_count; index += 1) {
    IMFTransform *transform = NULL;
    hr = IMFActivate_ActivateObject(
      activations[index],
      &IID_IMFTransform,
      (void **)&transform
    );
    if (FAILED(hr)) {
      continue;
    }

    /* Configure output type: H.264 */
    hr = MFCreateMediaType(&output_type);
    if (FAILED(hr)) {
      IMFTransform_Release(transform);
      continue;
    }
    hr = IMFMediaType_SetGUID(output_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
    if (FAILED(hr)) {
      IMFMediaType_Release(output_type);
      output_type = NULL;
      IMFTransform_Release(transform);
      continue;
    }
    hr = IMFMediaType_SetGUID(output_type, &MF_MT_SUBTYPE, &MFVideoFormat_H264);
    if (FAILED(hr)) {
      IMFMediaType_Release(output_type);
      output_type = NULL;
      IMFTransform_Release(transform);
      continue;
    }
    hr = IMFMediaType_SetUINT32(
      output_type, &MF_MT_AVG_BITRATE, (UINT32)session->bitrate
    );
    if (FAILED(hr)) {
      IMFMediaType_Release(output_type);
      output_type = NULL;
      IMFTransform_Release(transform);
      continue;
    }
    hr = encoder_set_attribute_pair(
      (IMFAttributes *)output_type,
      &MF_MT_FRAME_RATE,
      (UINT32)session->fps,
      1
    );
    if (FAILED(hr)) {
      IMFMediaType_Release(output_type);
      output_type = NULL;
      IMFTransform_Release(transform);
      continue;
    }
    hr = encoder_set_attribute_pair(
      (IMFAttributes *)output_type,
      &MF_MT_FRAME_SIZE,
      (UINT32)session->width,
      (UINT32)session->height
    );
    if (FAILED(hr)) {
      IMFMediaType_Release(output_type);
      output_type = NULL;
      IMFTransform_Release(transform);
      continue;
    }
    hr = IMFMediaType_SetUINT32(
      output_type, &MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive
    );
    if (FAILED(hr)) {
      IMFMediaType_Release(output_type);
      output_type = NULL;
      IMFTransform_Release(transform);
      continue;
    }
    hr = IMFTransform_SetOutputType(transform, 0, output_type, 0);
    IMFMediaType_Release(output_type);
    output_type = NULL;
    if (FAILED(hr)) {
      IMFTransform_Release(transform);
      continue;
    }

    /* Configure input type: NV12 */
    hr = MFCreateMediaType(&input_type);
    if (FAILED(hr)) {
      IMFTransform_Release(transform);
      continue;
    }
    hr = IMFMediaType_SetGUID(input_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
    if (FAILED(hr)) {
      IMFMediaType_Release(input_type);
      input_type = NULL;
      IMFTransform_Release(transform);
      continue;
    }
    hr = IMFMediaType_SetGUID(input_type, &MF_MT_SUBTYPE, &MFVideoFormat_NV12);
    if (FAILED(hr)) {
      IMFMediaType_Release(input_type);
      input_type = NULL;
      IMFTransform_Release(transform);
      continue;
    }
    hr = encoder_set_attribute_pair(
      (IMFAttributes *)input_type,
      &MF_MT_FRAME_RATE,
      (UINT32)session->fps,
      1
    );
    if (FAILED(hr)) {
      IMFMediaType_Release(input_type);
      input_type = NULL;
      IMFTransform_Release(transform);
      continue;
    }
    hr = encoder_set_attribute_pair(
      (IMFAttributes *)input_type,
      &MF_MT_FRAME_SIZE,
      (UINT32)session->width,
      (UINT32)session->height
    );
    if (FAILED(hr)) {
      IMFMediaType_Release(input_type);
      input_type = NULL;
      IMFTransform_Release(transform);
      continue;
    }
    hr = IMFMediaType_SetUINT32(
      input_type, &MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive
    );
    if (FAILED(hr)) {
      IMFMediaType_Release(input_type);
      input_type = NULL;
      IMFTransform_Release(transform);
      continue;
    }
    hr = IMFTransform_SetInputType(transform, 0, input_type, 0);
    IMFMediaType_Release(input_type);
    input_type = NULL;
    if (FAILED(hr)) {
      IMFTransform_Release(transform);
      continue;
    }

    /* Get output stream info */
    MFT_OUTPUT_STREAM_INFO stream_info = { 0 };
    hr = IMFTransform_GetOutputStreamInfo(transform, 0, &stream_info);
    if (FAILED(hr)) {
      IMFTransform_Release(transform);
      continue;
    }

    /* Begin streaming */
    hr = IMFTransform_ProcessMessage(
      transform,
      MFT_MESSAGE_NOTIFY_BEGIN_STREAMING,
      0
    );
    if (FAILED(hr)) {
      IMFTransform_Release(transform);
      continue;
    }

    /* Success — store in session */
    session->encoder = transform;
    session->activation = activations[index];
    IMFActivate_AddRef(activations[index]);
    memcpy(&session->stream_info, &stream_info, sizeof(stream_info));
    break;
  }

  if (session->encoder == NULL) {
    result = MOONLINK_RESULT_ENCODER_UNAVAILABLE;
    goto cleanup;
  }

  *out_handle = session;
  session = NULL; /* ownership transferred */

cleanup:
  if (input_type != NULL) {
    IMFMediaType_Release(input_type);
  }
  if (output_type != NULL) {
    IMFMediaType_Release(output_type);
  }
  if (activations != NULL) {
    for (UINT32 index = 0; index < activation_count; index += 1) {
      IMFActivate_Release(activations[index]);
    }
    CoTaskMemFree(activations);
  }
  if (session != NULL) {
    MFShutdown();
    free(session);
  }
  return result;
}

/* ── moonlink_encoder_encode ───────────────────────────────────────────── */

moonlink_native_result moonlink_encoder_encode(
  moonlink_encoder_handle handle,
  const moonlink_captured_frame *frame,
  const moonlink_encoded_packet **out_packets,
  int *out_packet_count
) {
  if (handle == NULL || frame == NULL || out_packets == NULL || out_packet_count == NULL) {
    return MOONLINK_RESULT_INVALID_HANDLE;
  }
  *out_packets = NULL;
  *out_packet_count = 0;

  if (handle->stopped) {
    return MOONLINK_RESULT_ALREADY_STOPPED;
  }

  /* Reset cached packet count for this encode call */
  handle->cached_packet_count = 0;

  /* Calculate NV12 frame size from strides */
  DWORD y_plane_size = (DWORD)frame->stride_y * (DWORD)frame->height;
  DWORD uv_plane_size = (DWORD)frame->stride_uv * ((DWORD)frame->height / 2);
  DWORD total_size = y_plane_size + uv_plane_size;

  /* Create memory buffer and copy NV12 data respecting strides */
  IMFMediaBuffer *input_buffer = NULL;
  BYTE *buffer_data = NULL;
  IMFSample *input_sample = NULL;
  HRESULT hr;
  moonlink_native_result result = MOONLINK_RESULT_OK;

  hr = MFCreateMemoryBuffer(total_size, &input_buffer);
  if (FAILED(hr)) {
    result = MOONLINK_RESULT_OUT_OF_MEMORY;
    goto cleanup;
  }

  hr = IMFMediaBuffer_Lock(input_buffer, &buffer_data, NULL, NULL);
  if (FAILED(hr)) {
    result = MOONLINK_RESULT_INTERNAL_ERROR;
    goto cleanup;
  }

  /* Copy Y plane row by row to respect source stride */
  {
    const uint8_t *src_y = frame->nv12_data;
    uint8_t *dst_y = buffer_data;
    for (int row = 0; row < frame->height; row += 1) {
      memcpy(dst_y, src_y, (size_t)frame->width);
      src_y += frame->stride_y;
      dst_y += frame->width;
    }
  }

  /* Copy UV plane row by row to respect source stride */
  {
    const uint8_t *src_uv = frame->nv12_data + (size_t)frame->stride_y * (size_t)frame->height;
    uint8_t *dst_uv = buffer_data + (size_t)frame->width * (size_t)frame->height;
    int uv_rows = frame->height / 2;
    for (int row = 0; row < uv_rows; row += 1) {
      memcpy(dst_uv, src_uv, (size_t)frame->width);
      src_uv += frame->stride_uv;
      dst_uv += frame->width;
    }
  }

  hr = IMFMediaBuffer_Unlock(input_buffer);
  buffer_data = NULL;
  if (FAILED(hr)) {
    result = MOONLINK_RESULT_INTERNAL_ERROR;
    goto cleanup;
  }

  hr = IMFMediaBuffer_SetCurrentLength(input_buffer, total_size);
  if (FAILED(hr)) {
    result = MOONLINK_RESULT_INTERNAL_ERROR;
    goto cleanup;
  }

  /* Create sample and attach buffer */
  hr = MFCreateSample(&input_sample);
  if (FAILED(hr)) {
    result = MOONLINK_RESULT_OUT_OF_MEMORY;
    goto cleanup;
  }

  hr = IMFSample_AddBuffer(input_sample, input_buffer);
  if (FAILED(hr)) {
    result = MOONLINK_RESULT_INTERNAL_ERROR;
    goto cleanup;
  }

  /* Set timestamps */
  hr = IMFSample_SetSampleTime(input_sample, handle->next_timestamp);
  if (FAILED(hr)) {
    result = MOONLINK_RESULT_INTERNAL_ERROR;
    goto cleanup;
  }
  hr = IMFSample_SetSampleDuration(input_sample, handle->frame_duration);
  if (FAILED(hr)) {
    result = MOONLINK_RESULT_INTERNAL_ERROR;
    goto cleanup;
  }

  /* Feed into encoder */
  hr = IMFTransform_ProcessInput(handle->encoder, 0, input_sample, 0);
  if (FAILED(hr)) {
    result = MOONLINK_RESULT_INTERNAL_ERROR;
    goto cleanup;
  }

  handle->next_timestamp += handle->frame_duration;

  /* Drain output packets */
  for (;;) {
    if (handle->cached_packet_count >= ENCODER_MAX_CACHED_PACKETS) {
      break;
    }

    /* Prepare output buffer if the MFT does not provide its own samples */
    MFT_OUTPUT_DATA_BUFFER output_data = { 0 };
    IMFSample *caller_sample = NULL;
    IMFMediaBuffer *caller_buffer = NULL;
    BOOL caller_owns_sample = FALSE;

    output_data.dwStreamID = 0;

    if (
      (handle->stream_info.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES) == 0 &&
      (handle->stream_info.dwFlags & MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES) == 0
    ) {
      if (handle->stream_info.cbSize == 0) {
        break;
      }
      hr = MFCreateAlignedMemoryBuffer(
        handle->stream_info.cbSize,
        handle->stream_info.cbAlignment,
        &caller_buffer
      );
      if (FAILED(hr)) {
        break;
      }
      hr = MFCreateSample(&caller_sample);
      if (FAILED(hr)) {
        IMFMediaBuffer_Release(caller_buffer);
        break;
      }
      hr = IMFSample_AddBuffer(caller_sample, caller_buffer);
      IMFMediaBuffer_Release(caller_buffer);
      caller_buffer = NULL;
      if (FAILED(hr)) {
        IMFSample_Release(caller_sample);
        break;
      }
      output_data.pSample = caller_sample;
      caller_owns_sample = TRUE;
    }

    DWORD process_status = 0;
    hr = IMFTransform_ProcessOutput(
      handle->encoder,
      0,
      1,
      &output_data,
      &process_status
    );

    /* Release event collection if present */
    if (output_data.pEvents != NULL) {
      IMFCollection_Release(output_data.pEvents);
    }

    if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
      if (output_data.pSample != NULL) {
        IMFSample_Release(output_data.pSample);
      }
      if (caller_owns_sample && output_data.pSample != caller_sample) {
        IMFSample_Release(caller_sample);
      }
      break;
    }

    if (FAILED(hr)) {
      if (output_data.pSample != NULL) {
        IMFSample_Release(output_data.pSample);
      }
      if (caller_owns_sample && output_data.pSample != caller_sample) {
        IMFSample_Release(caller_sample);
      }
      result = MOONLINK_RESULT_INTERNAL_ERROR;
      goto cleanup;
    }

    /* Extract encoded data from the output sample */
    IMFSample *out_sample = output_data.pSample;
    if (out_sample == NULL) {
      if (caller_owns_sample) {
        IMFSample_Release(caller_sample);
      }
      break;
    }

    IMFMediaBuffer *out_buffer = NULL;
    hr = IMFSample_ConvertToContiguousBuffer(out_sample, &out_buffer);
    if (FAILED(hr)) {
      IMFSample_Release(out_sample);
      if (caller_owns_sample && out_sample != caller_sample) {
        IMFSample_Release(caller_sample);
      }
      result = MOONLINK_RESULT_INTERNAL_ERROR;
      goto cleanup;
    }

    BYTE *encoded_data = NULL;
    DWORD encoded_max_length = 0;
    DWORD encoded_length = 0;
    hr = IMFMediaBuffer_Lock(out_buffer, &encoded_data, &encoded_max_length, NULL);
    /* Lock reports the buffer capacity, not the produced payload length.
       Reading the current length is required, otherwise trailing padding from
       the MFT output buffer is shipped as if it were H.264 data. */
    if (SUCCEEDED(hr)) {
      hr = IMFMediaBuffer_GetCurrentLength(out_buffer, &encoded_length);
    }
    if (FAILED(hr) || encoded_length == 0) {
      if (SUCCEEDED(hr)) {
        IMFMediaBuffer_Unlock(out_buffer);
      }
      IMFMediaBuffer_Release(out_buffer);
      IMFSample_Release(out_sample);
      if (caller_owns_sample && out_sample != caller_sample) {
        IMFSample_Release(caller_sample);
      }
      continue;
    }

    /* Check for keyframe via CleanPoint attribute */
    UINT32 clean_point = 0;
    IMFSample_GetUINT32(out_sample, &MFSampleExtension_CleanPoint, &clean_point);

    /* Get sample timestamp and convert from 100ns to microseconds */
    LONGLONG sample_time = 0;
    IMFSample_GetSampleTime(out_sample, &sample_time);
    int64_t timestamp_us = (int64_t)(sample_time / 10);

    /* Grow slot buffer if needed */
    int slot_index = handle->cached_packet_count;
    encoder_packet_slot *slot = &handle->slots[slot_index];
    if (slot->capacity < encoded_length) {
      uint8_t *new_buf = realloc(slot->buffer, encoded_length);
      if (new_buf == NULL) {
        IMFMediaBuffer_Unlock(out_buffer);
        IMFMediaBuffer_Release(out_buffer);
        IMFSample_Release(out_sample);
        if (caller_owns_sample && out_sample != caller_sample) {
          IMFSample_Release(caller_sample);
        }
        result = MOONLINK_RESULT_OUT_OF_MEMORY;
        goto cleanup;
      }
      slot->buffer = new_buf;
      slot->capacity = encoded_length;
    }

    memcpy(slot->buffer, encoded_data, encoded_length);
    IMFMediaBuffer_Unlock(out_buffer);
    IMFMediaBuffer_Release(out_buffer);
    IMFSample_Release(out_sample);
    if (caller_owns_sample && out_sample != caller_sample) {
      IMFSample_Release(caller_sample);
    }

    slot->packet.data = slot->buffer;
    slot->packet.size = (int)encoded_length;
    slot->packet.is_keyframe = clean_point ? 1 : 0;
    slot->packet.timestamp_us = timestamp_us;
    handle->cached_packet_count += 1;
  }

  if (handle->cached_packet_count > 0) {
    *out_packets = &handle->slots[0].packet;
    *out_packet_count = handle->cached_packet_count;
  }

cleanup:
  if (buffer_data != NULL) {
    IMFMediaBuffer_Unlock(input_buffer);
  }
  if (input_sample != NULL) {
    IMFSample_Release(input_sample);
  }
  if (input_buffer != NULL) {
    IMFMediaBuffer_Release(input_buffer);
  }
  return result;
}

/* ── moonlink_encoder_stop ─────────────────────────────────────────────── */

moonlink_native_result moonlink_encoder_stop(moonlink_encoder_handle handle) {
  if (handle == NULL) {
    return MOONLINK_RESULT_INVALID_HANDLE;
  }
  if (handle->stopped) {
    return MOONLINK_RESULT_ALREADY_STOPPED;
  }

  IMFTransform_ProcessMessage(
    handle->encoder,
    MFT_MESSAGE_NOTIFY_END_OF_STREAM,
    0
  );
  IMFTransform_ProcessMessage(
    handle->encoder,
    MFT_MESSAGE_COMMAND_DRAIN,
    0
  );

  handle->stopped = TRUE;
  return MOONLINK_RESULT_OK;
}

/* ── moonlink_encoder_destroy ──────────────────────────────────────────── */

moonlink_native_result moonlink_encoder_destroy(moonlink_encoder_handle handle) {
  if (handle == NULL) {
    return MOONLINK_RESULT_INVALID_HANDLE;
  }

  /* Shut down the activation object */
  if (handle->activation != NULL) {
    IMFActivate_ShutdownObject(handle->activation);
    IMFActivate_Release(handle->activation);
    handle->activation = NULL;
  }

  /* Release the MFT */
  if (handle->encoder != NULL) {
    IMFTransform_Release(handle->encoder);
    handle->encoder = NULL;
  }

  /* Free cached packet buffers */
  encoder_free_slots(handle);

  /* Shut down Media Foundation */
  MFShutdown();

  free(handle);
  return MOONLINK_RESULT_OK;
}
