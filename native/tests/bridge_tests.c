#include "moonlink_native_bridge.h"

#include <assert.h>
#include <stdio.h>

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
