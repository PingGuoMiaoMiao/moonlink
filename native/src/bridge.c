#include "sessions_internal.h"

#include <mferror.h>
#include <string.h>

enum {
  MOONLINK_NATIVE_H264_PROBE_WIDTH = 1280,
  MOONLINK_NATIVE_H264_PROBE_HEIGHT = 720,
  MOONLINK_NATIVE_H264_PROBE_FRAME_RATE_NUMERATOR = 30,
  MOONLINK_NATIVE_H264_PROBE_FRAME_RATE_DENOMINATOR = 1,
  MOONLINK_NATIVE_H264_PROBE_FRAME_DURATION = 333333,
  MOONLINK_NATIVE_H264_PROBE_FRAME_COUNT = 4,
  MOONLINK_NATIVE_H264_PROBE_AVERAGE_BITRATE = 2000000,
};

static HRESULT moonlink_native_set_attribute_pair(
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

static BOOL CALLBACK moonlink_count_display(
  HMONITOR monitor,
  HDC device_context,
  LPRECT bounds,
  LPARAM user_data
) {
  (void)monitor;
  (void)device_context;
  (void)bounds;
  int *count = (int *)user_data;
  *count += 1;
  return TRUE;
}

int moonlink_native_bridge_abi_version(void) {
  return MOONLINK_NATIVE_ABI_VERSION;
}

int moonlink_native_display_count(void) {
  int count = 0;
  if (!EnumDisplayMonitors(NULL, NULL, moonlink_count_display, (LPARAM)&count)) {
    return 0;
  }
  return count;
}

static moonlink_native_capture_probe_result moonlink_native_desktop_probe(
  BOOL acquire_frame,
  BOOL inspect_texture
) {
  ID3D11Device *device = NULL;
  ID3D11DeviceContext *device_context = NULL;
  IDXGIDevice *dxgi_device = NULL;
  IDXGIAdapter *adapter = NULL;
  IDXGIOutput *output = NULL;
  IDXGIOutput1 *output1 = NULL;
  IDXGIOutputDuplication *duplication = NULL;

  HRESULT result = D3D11CreateDevice(
    NULL,
    D3D_DRIVER_TYPE_HARDWARE,
    NULL,
    0,
    NULL,
    0,
    D3D11_SDK_VERSION,
    &device,
    NULL,
    &device_context
  );
  if (FAILED(result)) {
    return MOONLINK_NATIVE_CAPTURE_DEVICE_UNAVAILABLE;
  }

  result = ID3D11Device_QueryInterface(
    device,
    &IID_IDXGIDevice,
    (void **)&dxgi_device
  );
  if (FAILED(result)) {
    ID3D11DeviceContext_Release(device_context);
    ID3D11Device_Release(device);
    return MOONLINK_NATIVE_CAPTURE_ADAPTER_UNAVAILABLE;
  }

  result = IDXGIDevice_GetAdapter(dxgi_device, &adapter);
  if (FAILED(result)) {
    IDXGIDevice_Release(dxgi_device);
    ID3D11DeviceContext_Release(device_context);
    ID3D11Device_Release(device);
    return MOONLINK_NATIVE_CAPTURE_ADAPTER_UNAVAILABLE;
  }

  result = IDXGIAdapter_EnumOutputs(adapter, 0, &output);
  if (FAILED(result)) {
    IDXGIAdapter_Release(adapter);
    IDXGIDevice_Release(dxgi_device);
    ID3D11DeviceContext_Release(device_context);
    ID3D11Device_Release(device);
    return MOONLINK_NATIVE_CAPTURE_OUTPUT_UNAVAILABLE;
  }

  result = IDXGIOutput_QueryInterface(
    output,
    &IID_IDXGIOutput1,
    (void **)&output1
  );
  if (FAILED(result)) {
    IDXGIOutput_Release(output);
    IDXGIAdapter_Release(adapter);
    IDXGIDevice_Release(dxgi_device);
    ID3D11DeviceContext_Release(device_context);
    ID3D11Device_Release(device);
    return MOONLINK_NATIVE_CAPTURE_OUTPUT_UNAVAILABLE;
  }

  result = IDXGIOutput1_DuplicateOutput(output1, (IUnknown *)device, &duplication);
  if (FAILED(result)) {
    IDXGIOutput1_Release(output1);
    IDXGIOutput_Release(output);
    IDXGIAdapter_Release(adapter);
    IDXGIDevice_Release(dxgi_device);
    ID3D11DeviceContext_Release(device_context);
    ID3D11Device_Release(device);
    return MOONLINK_NATIVE_CAPTURE_DUPLICATION_UNAVAILABLE;
  }

  if (acquire_frame) {
    DXGI_OUTDUPL_FRAME_INFO frame_info = { 0 };
    IDXGIResource *desktop_resource = NULL;
    result = IDXGIOutputDuplication_AcquireNextFrame(
      duplication,
      1000,
      &frame_info,
      &desktop_resource
    );
    if (FAILED(result)) {
      IDXGIOutputDuplication_Release(duplication);
      IDXGIOutput1_Release(output1);
      IDXGIOutput_Release(output);
      IDXGIAdapter_Release(adapter);
      IDXGIDevice_Release(dxgi_device);
      ID3D11DeviceContext_Release(device_context);
      ID3D11Device_Release(device);
      return MOONLINK_NATIVE_CAPTURE_FRAME_UNAVAILABLE;
    }
    if (desktop_resource == NULL) {
      IDXGIOutputDuplication_ReleaseFrame(duplication);
      IDXGIOutputDuplication_Release(duplication);
      IDXGIOutput1_Release(output1);
      IDXGIOutput_Release(output);
      IDXGIAdapter_Release(adapter);
      IDXGIDevice_Release(dxgi_device);
      ID3D11DeviceContext_Release(device_context);
      ID3D11Device_Release(device);
      return MOONLINK_NATIVE_CAPTURE_FRAME_UNAVAILABLE;
    }
    moonlink_native_capture_probe_result frame_result = MOONLINK_NATIVE_CAPTURE_READY;
    if (inspect_texture) {
      ID3D11Texture2D *desktop_texture = NULL;
      result = IDXGIResource_QueryInterface(
        desktop_resource,
        &IID_ID3D11Texture2D,
        (void **)&desktop_texture
      );
      if (FAILED(result) || desktop_texture == NULL) {
        frame_result = MOONLINK_NATIVE_CAPTURE_TEXTURE_UNAVAILABLE;
      } else {
        D3D11_TEXTURE2D_DESC description = { 0 };
        ID3D11Texture2D_GetDesc(desktop_texture, &description);
        if (
          description.Width == 0 ||
          description.Height == 0 ||
          description.Format != DXGI_FORMAT_B8G8R8A8_UNORM
        ) {
          frame_result = MOONLINK_NATIVE_CAPTURE_TEXTURE_UNAVAILABLE;
        }
        ID3D11Texture2D_Release(desktop_texture);
      }
    }
    IDXGIResource_Release(desktop_resource);
    result = IDXGIOutputDuplication_ReleaseFrame(duplication);
    if (FAILED(result) || frame_result != MOONLINK_NATIVE_CAPTURE_READY) {
      IDXGIOutputDuplication_Release(duplication);
      IDXGIOutput1_Release(output1);
      IDXGIOutput_Release(output);
      IDXGIAdapter_Release(adapter);
      IDXGIDevice_Release(dxgi_device);
      ID3D11DeviceContext_Release(device_context);
      ID3D11Device_Release(device);
      return FAILED(result) ? MOONLINK_NATIVE_CAPTURE_FRAME_UNAVAILABLE : frame_result;
    }
  }

  IDXGIOutputDuplication_Release(duplication);
  IDXGIOutput1_Release(output1);
  IDXGIOutput_Release(output);
  IDXGIAdapter_Release(adapter);
  IDXGIDevice_Release(dxgi_device);
  ID3D11DeviceContext_Release(device_context);
  ID3D11Device_Release(device);
  return MOONLINK_NATIVE_CAPTURE_READY;
}

moonlink_native_capture_probe_result moonlink_native_desktop_duplication_probe(
  void
) {
  return moonlink_native_desktop_probe(FALSE, FALSE);
}

moonlink_native_capture_probe_result moonlink_native_desktop_frame_acquire_probe(
  void
) {
  return moonlink_native_desktop_probe(TRUE, FALSE);
}

moonlink_native_capture_probe_result moonlink_native_desktop_texture_probe(void) {
  return moonlink_native_desktop_probe(TRUE, TRUE);
}

moonlink_native_viewer_probe_result moonlink_native_d3d_viewer_probe(void) {
  const wchar_t *window_class_name = L"MoonLinkNativeViewerProbe";
  WNDCLASSW window_class = { 0 };
  window_class.lpfnWndProc = DefWindowProcW;
  window_class.hInstance = GetModuleHandleW(NULL);
  window_class.lpszClassName = window_class_name;
  if (RegisterClassW(&window_class) == 0) {
    return MOONLINK_NATIVE_VIEWER_WINDOW_UNAVAILABLE;
  }

  HWND window = CreateWindowExW(
    0,
    window_class_name,
    L"MoonLink native viewer probe",
    WS_OVERLAPPEDWINDOW,
    0,
    0,
    1,
    1,
    NULL,
    NULL,
    window_class.hInstance,
    NULL
  );
  if (window == NULL) {
    UnregisterClassW(window_class_name, window_class.hInstance);
    return MOONLINK_NATIVE_VIEWER_WINDOW_UNAVAILABLE;
  }

  DXGI_SWAP_CHAIN_DESC swap_chain_description = { 0 };
  swap_chain_description.BufferDesc.Width = 1;
  swap_chain_description.BufferDesc.Height = 1;
  swap_chain_description.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  swap_chain_description.BufferDesc.RefreshRate.Numerator = 60;
  swap_chain_description.BufferDesc.RefreshRate.Denominator = 1;
  swap_chain_description.SampleDesc.Count = 1;
  swap_chain_description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swap_chain_description.BufferCount = 1;
  swap_chain_description.OutputWindow = window;
  swap_chain_description.Windowed = TRUE;
  swap_chain_description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

  IDXGISwapChain *swap_chain = NULL;
  ID3D11Device *device = NULL;
  ID3D11DeviceContext *device_context = NULL;
  ID3D11Texture2D *back_buffer = NULL;
  ID3D11RenderTargetView *render_target = NULL;
  moonlink_native_viewer_probe_result probe_result =
    MOONLINK_NATIVE_VIEWER_READY;

  HRESULT result = D3D11CreateDeviceAndSwapChain(
    NULL,
    D3D_DRIVER_TYPE_HARDWARE,
    NULL,
    0,
    NULL,
    0,
    D3D11_SDK_VERSION,
    &swap_chain_description,
    &swap_chain,
    &device,
    NULL,
    &device_context
  );
  if (FAILED(result)) {
    probe_result = MOONLINK_NATIVE_VIEWER_DEVICE_UNAVAILABLE;
    goto cleanup;
  }

  result = IDXGISwapChain_GetBuffer(
    swap_chain,
    0,
    &IID_ID3D11Texture2D,
    (void **)&back_buffer
  );
  if (FAILED(result)) {
    probe_result = MOONLINK_NATIVE_VIEWER_SURFACE_UNAVAILABLE;
    goto cleanup;
  }

  result = ID3D11Device_CreateRenderTargetView(
    device,
    (ID3D11Resource *)back_buffer,
    NULL,
    &render_target
  );
  if (FAILED(result)) {
    probe_result = MOONLINK_NATIVE_VIEWER_SURFACE_UNAVAILABLE;
    goto cleanup;
  }

  const FLOAT clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
  ID3D11DeviceContext_ClearRenderTargetView(
    device_context,
    render_target,
    clear_color
  );
  result = IDXGISwapChain_Present(swap_chain, 0, 0);
  if (FAILED(result)) {
    probe_result = MOONLINK_NATIVE_VIEWER_PRESENT_UNAVAILABLE;
  }

cleanup:
  if (render_target != NULL) {
    ID3D11RenderTargetView_Release(render_target);
  }
  if (back_buffer != NULL) {
    ID3D11Texture2D_Release(back_buffer);
  }
  if (device_context != NULL) {
    ID3D11DeviceContext_Release(device_context);
  }
  if (device != NULL) {
    ID3D11Device_Release(device);
  }
  if (swap_chain != NULL) {
    IDXGISwapChain_Release(swap_chain);
  }
  DestroyWindow(window);
  UnregisterClassW(window_class_name, window_class.hInstance);
  return probe_result;
}

static int moonlink_native_h264_available(UINT32 enumeration_flags) {
  HRESULT result = MFStartup(MF_VERSION, MFSTARTUP_FULL);
  if (FAILED(result)) {
    return 0;
  }

  MFT_REGISTER_TYPE_INFO output_type = {
    MFMediaType_Video,
    MFVideoFormat_H264,
  };
  IMFActivate **activations = NULL;
  UINT32 activation_count = 0;
  result = MFTEnumEx(
    MFT_CATEGORY_VIDEO_ENCODER,
    enumeration_flags,
    NULL,
    &output_type,
    &activations,
    &activation_count
  );

  int available = 0;
  if (SUCCEEDED(result)) {
    for (UINT32 index = 0; index < activation_count; index += 1) {
      IMFTransform *transform = NULL;
      result = IMFActivate_ActivateObject(
        activations[index],
        &IID_IMFTransform,
        (void **)&transform
      );
      if (SUCCEEDED(result)) {
        available = 1;
        IMFTransform_Release(transform);
        break;
      }
    }
    for (UINT32 index = 0; index < activation_count; index += 1) {
      IMFActivate_Release(activations[index]);
    }
    CoTaskMemFree(activations);
  }
  MFShutdown();
  return available;
}

int moonlink_native_h264_software_available(void) {
  return moonlink_native_h264_available(
    MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_ASYNCMFT
  );
}

static HRESULT moonlink_native_h264_set_media_types(IMFTransform *transform) {
  IMFMediaType *output_type = NULL;
  IMFMediaType *input_type = NULL;
  HRESULT result = MFCreateMediaType(&output_type);
  if (FAILED(result)) {
    goto cleanup;
  }

  result = IMFMediaType_SetGUID(
    output_type,
    &MF_MT_MAJOR_TYPE,
    &MFMediaType_Video
  );
  if (FAILED(result)) {
    goto cleanup;
  }
  result = IMFMediaType_SetGUID(output_type, &MF_MT_SUBTYPE, &MFVideoFormat_H264);
  if (FAILED(result)) {
    goto cleanup;
  }
  result = IMFMediaType_SetUINT32(
    output_type,
    &MF_MT_AVG_BITRATE,
    MOONLINK_NATIVE_H264_PROBE_AVERAGE_BITRATE
  );
  if (FAILED(result)) {
    goto cleanup;
  }
  result = moonlink_native_set_attribute_pair(
    (IMFAttributes *)output_type,
    &MF_MT_FRAME_RATE,
    MOONLINK_NATIVE_H264_PROBE_FRAME_RATE_NUMERATOR,
    MOONLINK_NATIVE_H264_PROBE_FRAME_RATE_DENOMINATOR
  );
  if (FAILED(result)) {
    goto cleanup;
  }
  result = moonlink_native_set_attribute_pair(
    (IMFAttributes *)output_type,
    &MF_MT_FRAME_SIZE,
    MOONLINK_NATIVE_H264_PROBE_WIDTH,
    MOONLINK_NATIVE_H264_PROBE_HEIGHT
  );
  if (FAILED(result)) {
    goto cleanup;
  }
  result = IMFMediaType_SetUINT32(
    output_type,
    &MF_MT_INTERLACE_MODE,
    MFVideoInterlace_Progressive
  );
  if (FAILED(result)) {
    goto cleanup;
  }
  result = IMFTransform_SetOutputType(transform, 0, output_type, 0);
  if (FAILED(result)) {
    goto cleanup;
  }

  result = MFCreateMediaType(&input_type);
  if (FAILED(result)) {
    goto cleanup;
  }
  result = IMFMediaType_SetGUID(
    input_type,
    &MF_MT_MAJOR_TYPE,
    &MFMediaType_Video
  );
  if (FAILED(result)) {
    goto cleanup;
  }
  result = IMFMediaType_SetGUID(input_type, &MF_MT_SUBTYPE, &MFVideoFormat_NV12);
  if (FAILED(result)) {
    goto cleanup;
  }
  result = moonlink_native_set_attribute_pair(
    (IMFAttributes *)input_type,
    &MF_MT_FRAME_RATE,
    MOONLINK_NATIVE_H264_PROBE_FRAME_RATE_NUMERATOR,
    MOONLINK_NATIVE_H264_PROBE_FRAME_RATE_DENOMINATOR
  );
  if (FAILED(result)) {
    goto cleanup;
  }
  result = moonlink_native_set_attribute_pair(
    (IMFAttributes *)input_type,
    &MF_MT_FRAME_SIZE,
    MOONLINK_NATIVE_H264_PROBE_WIDTH,
    MOONLINK_NATIVE_H264_PROBE_HEIGHT
  );
  if (FAILED(result)) {
    goto cleanup;
  }
  result = IMFMediaType_SetUINT32(
    input_type,
    &MF_MT_INTERLACE_MODE,
    MFVideoInterlace_Progressive
  );
  if (FAILED(result)) {
    goto cleanup;
  }
  result = IMFTransform_SetInputType(transform, 0, input_type, 0);

cleanup:
  if (input_type != NULL) {
    IMFMediaType_Release(input_type);
  }
  if (output_type != NULL) {
    IMFMediaType_Release(output_type);
  }
  return result;
}

static HRESULT moonlink_native_h264_make_input_sample(
  int frame_index,
  IMFSample **sample
) {
  const DWORD frame_size =
    MOONLINK_NATIVE_H264_PROBE_WIDTH *
    MOONLINK_NATIVE_H264_PROBE_HEIGHT *
    3 /
    2;
  IMFMediaBuffer *buffer = NULL;
  BYTE *buffer_data = NULL;
  HRESULT result = MFCreateMemoryBuffer(frame_size, &buffer);
  if (FAILED(result)) {
    goto cleanup;
  }

  result = IMFMediaBuffer_Lock(buffer, &buffer_data, NULL, NULL);
  if (FAILED(result)) {
    goto cleanup;
  }
  memset(buffer_data, 0, frame_size);
  result = IMFMediaBuffer_Unlock(buffer);
  buffer_data = NULL;
  if (FAILED(result)) {
    goto cleanup;
  }
  result = IMFMediaBuffer_SetCurrentLength(buffer, frame_size);
  if (FAILED(result)) {
    goto cleanup;
  }

  result = MFCreateSample(sample);
  if (FAILED(result)) {
    goto cleanup;
  }
  result = IMFSample_AddBuffer(*sample, buffer);
  if (FAILED(result)) {
    IMFSample_Release(*sample);
    *sample = NULL;
    goto cleanup;
  }
  result = IMFSample_SetSampleTime(
    *sample,
    (LONGLONG)frame_index * MOONLINK_NATIVE_H264_PROBE_FRAME_DURATION
  );
  if (FAILED(result)) {
    IMFSample_Release(*sample);
    *sample = NULL;
    goto cleanup;
  }
  result = IMFSample_SetSampleDuration(
    *sample,
    MOONLINK_NATIVE_H264_PROBE_FRAME_DURATION
  );
  if (FAILED(result)) {
    IMFSample_Release(*sample);
    *sample = NULL;
  }

cleanup:
  if (buffer_data != NULL) {
    IMFMediaBuffer_Unlock(buffer);
  }
  if (buffer != NULL) {
    IMFMediaBuffer_Release(buffer);
  }
  return result;
}

HRESULT moonlink_native_create_mft_output_buffer(
  const MFT_OUTPUT_STREAM_INFO *stream_info,
  IMFMediaBuffer **buffer
) {
  if (stream_info == NULL || buffer == NULL) {
    return E_POINTER;
  }
  *buffer = NULL;
  if (stream_info->cbSize == 0) {
    return E_INVALIDARG;
  }
  return MFCreateAlignedMemoryBuffer(
    stream_info->cbSize,
    stream_info->cbAlignment,
    buffer
  );
}

HRESULT moonlink_native_shutdown_activation(IMFActivate *activation) {
  if (activation == NULL) {
    return E_POINTER;
  }
  return IMFActivate_ShutdownObject(activation);
}

static int moonlink_native_h264_process_output(
  IMFTransform *transform,
  const MFT_OUTPUT_STREAM_INFO *stream_info
) {
  MFT_OUTPUT_DATA_BUFFER output_data = { 0 };
  IMFSample *caller_sample = NULL;
  IMFMediaBuffer *caller_buffer = NULL;
  DWORD process_status = 0;
  HRESULT result;

  output_data.dwStreamID = 0;
  if (
    (stream_info->dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES) == 0 &&
    (stream_info->dwFlags & MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES) == 0
  ) {
    if (stream_info->cbSize == 0) {
      return -1;
    }
    result = moonlink_native_create_mft_output_buffer(
      stream_info,
      &caller_buffer
    );
    if (FAILED(result)) {
      return -1;
    }
    result = MFCreateSample(&caller_sample);
    if (FAILED(result)) {
      IMFMediaBuffer_Release(caller_buffer);
      return -1;
    }
    result = IMFSample_AddBuffer(caller_sample, caller_buffer);
    IMFMediaBuffer_Release(caller_buffer);
    caller_buffer = NULL;
    if (FAILED(result)) {
      IMFSample_Release(caller_sample);
      return -1;
    }
    output_data.pSample = caller_sample;
  }

  result = IMFTransform_ProcessOutput(
    transform,
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
    if (caller_sample != NULL && output_data.pSample != caller_sample) {
      IMFSample_Release(caller_sample);
    }
    return 0;
  }
  if (FAILED(result)) {
    if (output_data.pSample != NULL) {
      IMFSample_Release(output_data.pSample);
    }
    if (caller_sample != NULL && output_data.pSample != caller_sample) {
      IMFSample_Release(caller_sample);
    }
    return -1;
  }

  DWORD output_length = 0;
  if (output_data.pSample == NULL) {
    if (caller_sample != NULL) {
      IMFSample_Release(caller_sample);
    }
    return -1;
  }
  result = IMFSample_GetTotalLength(output_data.pSample, &output_length);
  IMFSample_Release(output_data.pSample);
  if (caller_sample != NULL && output_data.pSample != caller_sample) {
    IMFSample_Release(caller_sample);
  }
  if (FAILED(result)) {
    return -1;
  }
  return output_length > 0 ? 1 : -1;
}

int moonlink_native_h264_software_encode_probe(void) {
  IMFActivate **activations = NULL;
  UINT32 activation_count = 0;
  MFT_REGISTER_TYPE_INFO output_type = {
    MFMediaType_Video,
    MFVideoFormat_H264,
  };
  HRESULT result = MFStartup(MF_VERSION, MFSTARTUP_FULL);
  if (FAILED(result)) {
    return 0;
  }
  result = MFTEnumEx(
    MFT_CATEGORY_VIDEO_ENCODER,
    MFT_ENUM_FLAG_SYNCMFT,
    NULL,
    &output_type,
    &activations,
    &activation_count
  );
  if (FAILED(result)) {
    MFShutdown();
    return 0;
  }

  int encoded = 0;
  for (UINT32 index = 0; index < activation_count && encoded == 0; index += 1) {
    IMFTransform *transform = NULL;
    MFT_OUTPUT_STREAM_INFO stream_info = { 0 };
    result = IMFActivate_ActivateObject(
      activations[index],
      &IID_IMFTransform,
      (void **)&transform
    );
    if (FAILED(result)) {
      continue;
    }
    result = moonlink_native_h264_set_media_types(transform);
    if (SUCCEEDED(result)) {
      result = IMFTransform_GetOutputStreamInfo(transform, 0, &stream_info);
    }
    if (SUCCEEDED(result)) {
      result = IMFTransform_ProcessMessage(
        transform,
        MFT_MESSAGE_NOTIFY_BEGIN_STREAMING,
        0
      );
    }
    if (SUCCEEDED(result)) {
      for (
        int frame_index = 0;
        frame_index < MOONLINK_NATIVE_H264_PROBE_FRAME_COUNT && encoded == 0;
        frame_index += 1
      ) {
        IMFSample *input_sample = NULL;
        result = moonlink_native_h264_make_input_sample(
          frame_index,
          &input_sample
        );
        if (SUCCEEDED(result)) {
          result = IMFTransform_ProcessInput(transform, 0, input_sample, 0);
        }
        if (input_sample != NULL) {
          IMFSample_Release(input_sample);
        }
        if (FAILED(result)) {
          break;
        }

        for (;;) {
          const int output_result = moonlink_native_h264_process_output(
            transform,
            &stream_info
          );
          if (output_result < 0) {
            result = E_FAIL;
            break;
          }
          if (output_result == 0) {
            break;
          }
          encoded = 1;
        }
        if (FAILED(result)) {
          break;
        }
      }
    }
    if (SUCCEEDED(result)) {
      result = IMFTransform_ProcessMessage(
        transform,
        MFT_MESSAGE_NOTIFY_END_OF_STREAM,
        0
      );
    }
    if (SUCCEEDED(result)) {
      result = IMFTransform_ProcessMessage(
        transform,
        MFT_MESSAGE_COMMAND_DRAIN,
        0
      );
    }
    while (SUCCEEDED(result)) {
      const int output_result = moonlink_native_h264_process_output(
        transform,
        &stream_info
      );
      if (output_result < 0) {
        result = E_FAIL;
      } else if (output_result == 0) {
        break;
      } else {
        encoded = 1;
      }
    }
    IMFTransform_ProcessMessage(transform, MFT_MESSAGE_NOTIFY_END_STREAMING, 0);
    const HRESULT shutdown_result = moonlink_native_shutdown_activation(
      activations[index]
    );
    IMFTransform_Release(transform);
    if (FAILED(shutdown_result)) {
      encoded = 0;
    }
  }

  for (UINT32 index = 0; index < activation_count; index += 1) {
    IMFActivate_Release(activations[index]);
  }
  CoTaskMemFree(activations);
  MFShutdown();
  return encoded;
}

int moonlink_native_h264_hardware_available(void) {
  return moonlink_native_h264_available(MFT_ENUM_FLAG_HARDWARE);
}

int moonlink_native_input_allowed(moonlink_native_session_state state) {
  return state == MOONLINK_NATIVE_CONNECTED || state == MOONLINK_NATIVE_DEGRADED;
}

/* ══════════════════════════════════════════════════════════════════════
   Capture session (ABI v2)
   ══════════════════════════════════════════════════════════════════════ */

static void moonlink_capture_bgra_to_nv12(
  const uint8_t *bgra,
  int bgra_pitch,
  uint8_t *nv12,
  int width,
  int height,
  int stride_y,
  int stride_uv
) {
  uint8_t *y_plane = nv12;
  uint8_t *uv_plane = nv12 + stride_y * height;

  for (int row = 0; row < height; row += 1) {
    const uint8_t *src_row = bgra + row * bgra_pitch;
    uint8_t *y_row = y_plane + row * stride_y;

    for (int col = 0; col < width; col += 1) {
      const uint8_t *pixel = src_row + col * 4;
      const int b = pixel[0];
      const int g = pixel[1];
      const int r = pixel[2];

      int y = (66 * r + 129 * g + 25 * b + 128) >> 8;
      y += 16;
      if (y < 0) y = 0;
      if (y > 255) y = 255;
      y_row[col] = (uint8_t)y;
    }
  }

  for (int row = 0; row < height; row += 2) {
    const uint8_t *src_row_0 = bgra + row * bgra_pitch;
    const uint8_t *src_row_1 = bgra + (row + 1) * bgra_pitch;
    uint8_t *uv_row = uv_plane + (row / 2) * stride_uv;

    for (int col = 0; col < width; col += 2) {
      const uint8_t *p00 = src_row_0 + col * 4;
      const uint8_t *p10 = src_row_0 + (col + 1) * 4;
      const uint8_t *p01 = src_row_1 + col * 4;
      const uint8_t *p11 = src_row_1 + (col + 1) * 4;

      const int r = (p00[2] + p10[2] + p01[2] + p11[2]) / 4;
      const int g = (p00[1] + p10[1] + p01[1] + p11[1]) / 4;
      const int b = (p00[0] + p10[0] + p01[0] + p11[0]) / 4;

      int u = (-38 * r - 74 * g + 112 * b + 128) >> 8;
      u += 128;
      if (u < 0) u = 0;
      if (u > 255) u = 255;

      int v = (112 * r - 94 * g - 18 * b + 128) >> 8;
      v += 128;
      if (v < 0) v = 0;
      if (v > 255) v = 255;

      uv_row[col] = (uint8_t)u;
      uv_row[col + 1] = (uint8_t)v;
    }
  }
}

moonlink_native_result moonlink_capture_create(
  const moonlink_capture_config *config,
  moonlink_capture_handle *out_handle
) {
  if (config == NULL || out_handle == NULL) {
    return MOONLINK_RESULT_INVALID_HANDLE;
  }
  *out_handle = NULL;

  struct moonlink_capture_session *session = NULL;
  IDXGIDevice *dxgi_device = NULL;
  IDXGIAdapter *adapter = NULL;
  IDXGIOutput *output = NULL;
  IDXGIOutput1 *output1 = NULL;
  moonlink_native_result result_code = MOONLINK_RESULT_OK;

  session = (struct moonlink_capture_session *)calloc(
    1,
    sizeof(struct moonlink_capture_session)
  );
  if (session == NULL) {
    return MOONLINK_RESULT_OUT_OF_MEMORY;
  }

  HRESULT hr = D3D11CreateDevice(
    NULL,
    D3D_DRIVER_TYPE_HARDWARE,
    NULL,
    0,
    NULL,
    0,
    D3D11_SDK_VERSION,
    &session->device,
    NULL,
    &session->device_context
  );
  if (FAILED(hr)) {
    result_code = MOONLINK_RESULT_DEVICE_UNAVAILABLE;
    goto cleanup;
  }

  hr = ID3D11Device_QueryInterface(
    session->device,
    &IID_IDXGIDevice,
    (void **)&dxgi_device
  );
  if (FAILED(hr)) {
    result_code = MOONLINK_RESULT_DEVICE_UNAVAILABLE;
    goto cleanup;
  }

  hr = IDXGIDevice_GetAdapter(dxgi_device, &adapter);
  if (FAILED(hr)) {
    result_code = MOONLINK_RESULT_DEVICE_UNAVAILABLE;
    goto cleanup;
  }

  hr = IDXGIAdapter_EnumOutputs(adapter, config->output_index, &output);
  if (FAILED(hr)) {
    result_code = MOONLINK_RESULT_DEVICE_UNAVAILABLE;
    goto cleanup;
  }

  hr = IDXGIOutput_QueryInterface(
    output,
    &IID_IDXGIOutput1,
    (void **)&output1
  );
  if (FAILED(hr)) {
    result_code = MOONLINK_RESULT_DEVICE_UNAVAILABLE;
    goto cleanup;
  }

  hr = IDXGIOutput1_DuplicateOutput(
    output1,
    (IUnknown *)session->device,
    &session->duplication
  );
  if (FAILED(hr)) {
    result_code = MOONLINK_RESULT_DEVICE_UNAVAILABLE;
    goto cleanup;
  }

  DXGI_OUTPUT_DESC output_desc = { 0 };
  hr = IDXGIOutput_GetDesc(output, &output_desc);
  if (FAILED(hr)) {
    result_code = MOONLINK_RESULT_INTERNAL_ERROR;
    goto cleanup;
  }

  const int native_width = output_desc.DesktopCoordinates.right -
    output_desc.DesktopCoordinates.left;
  const int native_height = output_desc.DesktopCoordinates.bottom -
    output_desc.DesktopCoordinates.top;

  session->width = (config->target_width > 0) ? config->target_width : native_width;
  session->height = (config->target_height > 0) ? config->target_height : native_height;
  session->stride_y = session->width;
  session->stride_uv = session->width;

  D3D11_TEXTURE2D_DESC staging_desc = { 0 };
  staging_desc.Width = (UINT)session->width;
  staging_desc.Height = (UINT)session->height;
  staging_desc.MipLevels = 1;
  staging_desc.ArraySize = 1;
  staging_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  staging_desc.SampleDesc.Count = 1;
  staging_desc.Usage = D3D11_USAGE_STAGING;
  staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

  hr = ID3D11Device_CreateTexture2D(
    session->device,
    &staging_desc,
    NULL,
    &session->staging_texture
  );
  if (FAILED(hr)) {
    result_code = MOONLINK_RESULT_OUT_OF_MEMORY;
    goto cleanup;
  }

  const size_t nv12_size = (size_t)session->width * session->height * 3 / 2;
  session->nv12_buffer = (uint8_t *)malloc(nv12_size);
  if (session->nv12_buffer == NULL) {
    result_code = MOONLINK_RESULT_OUT_OF_MEMORY;
    goto cleanup;
  }

  QueryPerformanceFrequency(&session->qpc_frequency);
  session->stopped = FALSE;

  *out_handle = session;
  session = NULL;

cleanup:
  if (output1 != NULL) {
    IDXGIOutput1_Release(output1);
  }
  if (output != NULL) {
    IDXGIOutput_Release(output);
  }
  if (adapter != NULL) {
    IDXGIAdapter_Release(adapter);
  }
  if (dxgi_device != NULL) {
    IDXGIDevice_Release(dxgi_device);
  }
  if (session != NULL) {
    if (session->nv12_buffer != NULL) {
      free(session->nv12_buffer);
    }
    if (session->staging_texture != NULL) {
      ID3D11Texture2D_Release(session->staging_texture);
    }
    if (session->duplication != NULL) {
      IDXGIOutputDuplication_Release(session->duplication);
    }
    if (session->device_context != NULL) {
      ID3D11DeviceContext_Release(session->device_context);
    }
    if (session->device != NULL) {
      ID3D11Device_Release(session->device);
    }
    free(session);
  }
  return result_code;
}

moonlink_native_result moonlink_capture_next_frame(
  moonlink_capture_handle handle,
  int timeout_ms,
  const moonlink_captured_frame **out_frame
) {
  if (handle == NULL || out_frame == NULL) {
    return MOONLINK_RESULT_INVALID_HANDLE;
  }
  *out_frame = NULL;

  if (handle->stopped) {
    return MOONLINK_RESULT_ALREADY_STOPPED;
  }

  DXGI_OUTDUPL_FRAME_INFO frame_info = { 0 };
  IDXGIResource *desktop_resource = NULL;
  ID3D11Texture2D *desktop_texture = NULL;
  D3D11_MAPPED_SUBRESOURCE mapped = { 0 };
  moonlink_native_result result_code = MOONLINK_RESULT_OK;

  HRESULT hr = IDXGIOutputDuplication_AcquireNextFrame(
    handle->duplication,
    (UINT)timeout_ms,
    &frame_info,
    &desktop_resource
  );
  if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
    return MOONLINK_RESULT_TIMEOUT;
  }
  if (FAILED(hr)) {
    return MOONLINK_RESULT_INTERNAL_ERROR;
  }

  hr = IDXGIResource_QueryInterface(
    desktop_resource,
    &IID_ID3D11Texture2D,
    (void **)&desktop_texture
  );
  if (FAILED(hr)) {
    result_code = MOONLINK_RESULT_INTERNAL_ERROR;
    goto cleanup;
  }

  ID3D11DeviceContext_CopySubresourceRegion(
    handle->device_context,
    (ID3D11Resource *)handle->staging_texture,
    0,
    0,
    0,
    0,
    (ID3D11Resource *)desktop_texture,
    0,
    NULL
  );

  hr = ID3D11DeviceContext_Map(
    handle->device_context,
    (ID3D11Resource *)handle->staging_texture,
    0,
    D3D11_MAP_READ,
    0,
    &mapped
  );
  if (FAILED(hr)) {
    result_code = MOONLINK_RESULT_INTERNAL_ERROR;
    goto cleanup;
  }

  moonlink_capture_bgra_to_nv12(
    (const uint8_t *)mapped.pData,
    (int)mapped.RowPitch,
    handle->nv12_buffer,
    handle->width,
    handle->height,
    handle->stride_y,
    handle->stride_uv
  );

  ID3D11DeviceContext_Unmap(
    handle->device_context,
    (ID3D11Resource *)handle->staging_texture,
    0
  );
  mapped.pData = NULL;

  LARGE_INTEGER qpc_now = { 0 };
  QueryPerformanceCounter(&qpc_now);
  const int64_t timestamp_us = (int64_t)(
    (double)qpc_now.QuadPart /
    (double)handle->qpc_frequency.QuadPart *
    1000000.0
  );

  handle->cached_frame.nv12_data = handle->nv12_buffer;
  handle->cached_frame.width = handle->width;
  handle->cached_frame.height = handle->height;
  handle->cached_frame.stride_y = handle->stride_y;
  handle->cached_frame.stride_uv = handle->stride_uv;
  handle->cached_frame.timestamp_us = timestamp_us;

  *out_frame = &handle->cached_frame;

cleanup:
  if (mapped.pData != NULL) {
    ID3D11DeviceContext_Unmap(
      handle->device_context,
      (ID3D11Resource *)handle->staging_texture,
      0
    );
  }
  if (desktop_texture != NULL) {
    ID3D11Texture2D_Release(desktop_texture);
  }
  if (desktop_resource != NULL) {
    IDXGIResource_Release(desktop_resource);
  }
  IDXGIOutputDuplication_ReleaseFrame(handle->duplication);
  return result_code;
}

moonlink_native_result moonlink_capture_stop(moonlink_capture_handle handle) {
  if (handle == NULL) {
    return MOONLINK_RESULT_INVALID_HANDLE;
  }
  if (handle->stopped) {
    return MOONLINK_RESULT_ALREADY_STOPPED;
  }
  handle->stopped = TRUE;
  if (handle->duplication != NULL) {
    IDXGIOutputDuplication_Release(handle->duplication);
    handle->duplication = NULL;
  }
  return MOONLINK_RESULT_OK;
}

moonlink_native_result moonlink_capture_destroy(moonlink_capture_handle handle) {
  if (handle == NULL) {
    return MOONLINK_RESULT_INVALID_HANDLE;
  }
  if (!handle->stopped) {
    moonlink_capture_stop(handle);
  }
  if (handle->staging_texture != NULL) {
    ID3D11Texture2D_Release(handle->staging_texture);
    handle->staging_texture = NULL;
  }
  if (handle->device_context != NULL) {
    ID3D11DeviceContext_Release(handle->device_context);
    handle->device_context = NULL;
  }
  if (handle->device != NULL) {
    ID3D11Device_Release(handle->device);
    handle->device = NULL;
  }
  if (handle->nv12_buffer != NULL) {
    free(handle->nv12_buffer);
    handle->nv12_buffer = NULL;
  }
  free(handle);
  return MOONLINK_RESULT_OK;
}
