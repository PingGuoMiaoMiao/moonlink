#include "sessions_internal.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

moonlink_native_result moonlink_viewer_create(
  const moonlink_viewer_config *config,
  moonlink_viewer_handle *out_handle
) {
  if (config == NULL || out_handle == NULL) {
    return MOONLINK_RESULT_INVALID_HANDLE;
  }
  if (config->width <= 0 || config->height <= 0) {
    return MOONLINK_RESULT_INVALID_HANDLE;
  }

  struct moonlink_viewer_session *session = calloc(1, sizeof(*session));
  if (session == NULL) {
    return MOONLINK_RESULT_OUT_OF_MEMORY;
  }

  session->width = config->width;
  session->height = config->height;
  session->stopped = FALSE;
  session->srv = NULL;

  /* Generate unique window class name using pointer address as suffix. */
  swprintf_s(
    session->class_name,
    _countof(session->class_name),
    L"MoonLinkViewer_%p",
    (void *)session
  );

  /* Register window class. */
  memset(&session->window_class, 0, sizeof(session->window_class));
  session->window_class.lpfnWndProc = DefWindowProcW;
  session->window_class.hInstance = GetModuleHandleW(NULL);
  session->window_class.lpszClassName = session->class_name;
  if (RegisterClassW(&session->window_class) == 0) {
    free(session);
    return MOONLINK_RESULT_VIEWER_UNAVAILABLE;
  }

  /* Calculate window rect to account for borders so client area matches config. */
  RECT window_rect = { 0, 0, config->width, config->height };
  DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
  AdjustWindowRect(&window_rect, style, FALSE);
  int adjusted_width = window_rect.right - window_rect.left;
  int adjusted_height = window_rect.bottom - window_rect.top;

  /* Center on screen. */
  int screen_width = GetSystemMetrics(SM_CXSCREEN);
  int screen_height = GetSystemMetrics(SM_CYSCREEN);
  int pos_x = (screen_width - adjusted_width) / 2;
  int pos_y = (screen_height - adjusted_height) / 2;

  session->window = CreateWindowExW(
    0,
    session->class_name,
    config->title != NULL ? config->title : L"MoonLink Viewer",
    style,
    pos_x,
    pos_y,
    adjusted_width,
    adjusted_height,
    NULL,
    NULL,
    session->window_class.hInstance,
    NULL
  );
  if (session->window == NULL) {
    UnregisterClassW(session->class_name, session->window_class.hInstance);
    free(session);
    return MOONLINK_RESULT_VIEWER_UNAVAILABLE;
  }

  /* Create D3D11 device and swap chain. */
  DXGI_SWAP_CHAIN_DESC swap_chain_desc = { 0 };
  swap_chain_desc.BufferDesc.Width = (UINT)config->width;
  swap_chain_desc.BufferDesc.Height = (UINT)config->height;
  swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  swap_chain_desc.BufferDesc.RefreshRate.Numerator = 60;
  swap_chain_desc.BufferDesc.RefreshRate.Denominator = 1;
  swap_chain_desc.SampleDesc.Count = 1;
  swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swap_chain_desc.BufferCount = 1;
  swap_chain_desc.OutputWindow = session->window;
  swap_chain_desc.Windowed = TRUE;
  swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

  HRESULT result = D3D11CreateDeviceAndSwapChain(
    NULL,
    D3D_DRIVER_TYPE_HARDWARE,
    NULL,
    0,
    NULL,
    0,
    D3D11_SDK_VERSION,
    &swap_chain_desc,
    &session->swap_chain,
    &session->device,
    NULL,
    &session->context
  );
  if (FAILED(result)) {
    DestroyWindow(session->window);
    UnregisterClassW(session->class_name, session->window_class.hInstance);
    free(session);
    return MOONLINK_RESULT_DEVICE_UNAVAILABLE;
  }

  /* Create CPU-writable BGRA staging texture. */
  D3D11_TEXTURE2D_DESC staging_desc = { 0 };
  staging_desc.Width = (UINT)config->width;
  staging_desc.Height = (UINT)config->height;
  staging_desc.MipLevels = 1;
  staging_desc.ArraySize = 1;
  staging_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  staging_desc.SampleDesc.Count = 1;
  staging_desc.Usage = D3D11_USAGE_STAGING;
  staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  staging_desc.BindFlags = 0;
  staging_desc.MiscFlags = 0;

  result = ID3D11Device_CreateTexture2D(
    session->device,
    &staging_desc,
    NULL,
    &session->staging_texture
  );
  if (FAILED(result)) {
    ID3D11DeviceContext_Release(session->context);
    ID3D11Device_Release(session->device);
    IDXGISwapChain_Release(session->swap_chain);
    DestroyWindow(session->window);
    UnregisterClassW(session->class_name, session->window_class.hInstance);
    free(session);
    return MOONLINK_RESULT_DEVICE_UNAVAILABLE;
  }

  *out_handle = session;
  return MOONLINK_RESULT_OK;
}

moonlink_native_result moonlink_viewer_present(
  moonlink_viewer_handle handle,
  const moonlink_decoded_frame *frame
) {
  if (handle == NULL || frame == NULL) {
    return MOONLINK_RESULT_INVALID_HANDLE;
  }
  if (handle->stopped) {
    return MOONLINK_RESULT_ALREADY_STOPPED;
  }

  /* Map staging texture for writing. */
  D3D11_MAPPED_SUBRESOURCE mapped = { 0 };
  HRESULT result = ID3D11DeviceContext_Map(
    handle->context,
    (ID3D11Resource *)handle->staging_texture,
    0,
    D3D11_MAP_WRITE,
    0,
    &mapped
  );
  if (FAILED(result)) {
    return MOONLINK_RESULT_INTERNAL_ERROR;
  }

  /* Copy frame data row by row, respecting both source stride and dest RowPitch. */
  const uint8_t *src = frame->bgra_data;
  uint8_t *dst = (uint8_t *)mapped.pData;
  int row_bytes = frame->width * 4; /* BGRA = 4 bytes per pixel */
  for (int y = 0; y < frame->height; y += 1) {
    memcpy(dst, src, (size_t)row_bytes);
    src += frame->stride;
    dst += mapped.RowPitch;
  }

  ID3D11DeviceContext_Unmap(
    handle->context,
    (ID3D11Resource *)handle->staging_texture,
    0
  );

  /* Get back buffer from swap chain. */
  ID3D11Texture2D *back_buffer = NULL;
  result = IDXGISwapChain_GetBuffer(
    handle->swap_chain,
    0,
    &IID_ID3D11Texture2D,
    (void **)&back_buffer
  );
  if (FAILED(result)) {
    return MOONLINK_RESULT_INTERNAL_ERROR;
  }

  /* Copy staging texture to back buffer. */
  ID3D11DeviceContext_CopySubresourceRegion(
    handle->context,
    (ID3D11Resource *)back_buffer,
    0,
    0,
    0,
    0,
    (ID3D11Resource *)handle->staging_texture,
    0,
    NULL
  );

  ID3D11Texture2D_Release(back_buffer);

  /* Present with vsync. May fail if window is minimized — treat as non-fatal. */
  result = IDXGISwapChain_Present(handle->swap_chain, 1, 0);
  if (result == DXGI_STATUS_OCCLUDED) {
    /* Window is minimized or occluded; not an error. */
    return MOONLINK_RESULT_OK;
  }
  if (FAILED(result)) {
    return MOONLINK_RESULT_INTERNAL_ERROR;
  }

  return MOONLINK_RESULT_OK;
}

int moonlink_viewer_pump_messages(moonlink_viewer_handle handle) {
  if (handle == NULL) {
    return 1;
  }

  MSG msg;
  while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
    if (msg.message == WM_QUIT) {
      return 1;
    }
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  /* Check if our window was destroyed externally. */
  if (!IsWindow(handle->window)) {
    handle->stopped = TRUE;
    return 1;
  }

  return 0;
}

moonlink_native_result moonlink_viewer_stop(moonlink_viewer_handle handle) {
  if (handle == NULL) {
    return MOONLINK_RESULT_INVALID_HANDLE;
  }
  if (handle->stopped) {
    return MOONLINK_RESULT_ALREADY_STOPPED;
  }

  handle->stopped = TRUE;
  if (IsWindow(handle->window)) {
    DestroyWindow(handle->window);
  }
  handle->window = NULL;

  return MOONLINK_RESULT_OK;
}

moonlink_native_result moonlink_viewer_destroy(moonlink_viewer_handle handle) {
  if (handle == NULL) {
    return MOONLINK_RESULT_INVALID_HANDLE;
  }

  /* Ensure window is destroyed before releasing D3D resources. */
  if (!handle->stopped && IsWindow(handle->window)) {
    DestroyWindow(handle->window);
  }
  handle->window = NULL;

  if (handle->srv != NULL) {
    ID3D11ShaderResourceView_Release(handle->srv);
    handle->srv = NULL;
  }
  if (handle->staging_texture != NULL) {
    ID3D11Texture2D_Release(handle->staging_texture);
    handle->staging_texture = NULL;
  }
  if (handle->context != NULL) {
    ID3D11DeviceContext_Release(handle->context);
    handle->context = NULL;
  }
  if (handle->device != NULL) {
    ID3D11Device_Release(handle->device);
    handle->device = NULL;
  }
  if (handle->swap_chain != NULL) {
    IDXGISwapChain_Release(handle->swap_chain);
    handle->swap_chain = NULL;
  }

  UnregisterClassW(handle->class_name, handle->window_class.hInstance);

  free(handle);
  return MOONLINK_RESULT_OK;
}
