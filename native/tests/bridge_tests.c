#include "moonlink_native_bridge.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#define COBJMACROS
#include <mfapi.h>
#include <mftransform.h>

HRESULT moonlink_native_create_mft_output_buffer(
  const MFT_OUTPUT_STREAM_INFO *stream_info,
  IMFMediaBuffer **buffer
);
HRESULT moonlink_native_shutdown_activation(IMFActivate *activation);

typedef struct moonlink_test_activation {
  IMFActivate activation;
  ULONG shutdown_call_count;
} moonlink_test_activation;

static HRESULT STDMETHODCALLTYPE moonlink_test_shutdown_activation(
  IMFActivate *activation
) {
  moonlink_test_activation *test_activation =
    (moonlink_test_activation *)activation;
  test_activation->shutdown_call_count += 1;
  return S_OK;
}

int main(void) {
  const int display_count = moonlink_native_display_count();
  const int h264_hardware_available = moonlink_native_h264_hardware_available();
  assert(moonlink_native_bridge_abi_version() == 1);
  assert(display_count > 0);
  assert(
    moonlink_native_desktop_duplication_probe() ==
    MOONLINK_NATIVE_CAPTURE_READY
  );
  assert(
    moonlink_native_desktop_frame_acquire_probe() ==
    MOONLINK_NATIVE_CAPTURE_READY
  );
  assert(moonlink_native_d3d_viewer_probe() == MOONLINK_NATIVE_VIEWER_READY);
  assert(moonlink_native_h264_software_available() == 1);
  assert(moonlink_native_h264_software_encode_probe() == 1);

  assert(SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_FULL)));
  MFT_OUTPUT_STREAM_INFO stream_info = {
    0,
    64,
    MF_64_BYTE_ALIGNMENT,
  };
  IMFMediaBuffer *output_buffer = NULL;
  assert(
    SUCCEEDED(
      moonlink_native_create_mft_output_buffer(&stream_info, &output_buffer)
    )
  );
  BYTE *output_data = NULL;
  assert(SUCCEEDED(IMFMediaBuffer_Lock(output_buffer, &output_data, NULL, NULL)));
  assert(((uintptr_t)output_data & 63u) == 0);
  assert(SUCCEEDED(IMFMediaBuffer_Unlock(output_buffer)));
  IMFMediaBuffer_Release(output_buffer);
  MFShutdown();

  IMFActivateVtbl activation_vtable = { 0 };
  activation_vtable.ShutdownObject = moonlink_test_shutdown_activation;
  moonlink_test_activation test_activation = { 0 };
  test_activation.activation.lpVtbl = &activation_vtable;
  assert(
    SUCCEEDED(moonlink_native_shutdown_activation(&test_activation.activation))
  );
  assert(test_activation.shutdown_call_count == 1);

  assert(h264_hardware_available >= 0);
  assert(h264_hardware_available <= 1);
  assert(moonlink_native_input_allowed(MOONLINK_NATIVE_AWAITING_APPROVAL) == 0);
  assert(moonlink_native_input_allowed(MOONLINK_NATIVE_CONNECTED) == 1);
  assert(moonlink_native_input_allowed(MOONLINK_NATIVE_DEGRADED) == 1);
  assert(moonlink_native_input_allowed(MOONLINK_NATIVE_ENDED) == 0);
  printf(
    "displays=%d h264_hardware_available=%d\n",
    display_count,
    h264_hardware_available
  );
  return 0;
}
