#include "moonlink_native_bridge.h"

#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <mfapi.h>
#include <mftransform.h>

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
  return 1;
}

int moonlink_native_display_count(void) {
  int count = 0;
  if (!EnumDisplayMonitors(NULL, NULL, moonlink_count_display, (LPARAM)&count)) {
    return 0;
  }
  return count;
}

static moonlink_native_capture_probe_result moonlink_native_desktop_probe(
  BOOL acquire_frame
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
    IDXGIResource_Release(desktop_resource);
    result = IDXGIOutputDuplication_ReleaseFrame(duplication);
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
  return moonlink_native_desktop_probe(FALSE);
}

moonlink_native_capture_probe_result moonlink_native_desktop_frame_acquire_probe(
  void
) {
  return moonlink_native_desktop_probe(TRUE);
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

int moonlink_native_h264_hardware_available(void) {
  return moonlink_native_h264_available(MFT_ENUM_FLAG_HARDWARE);
}

int moonlink_native_input_allowed(moonlink_native_session_state state) {
  return state == MOONLINK_NATIVE_CONNECTED || state == MOONLINK_NATIVE_DEGRADED;
}
