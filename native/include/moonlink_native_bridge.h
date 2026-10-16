#ifndef MOONLINK_NATIVE_BRIDGE_H
#define MOONLINK_NATIVE_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum moonlink_native_session_state {
  MOONLINK_NATIVE_AWAITING_APPROVAL = 0,
  MOONLINK_NATIVE_CONNECTED = 1,
  MOONLINK_NATIVE_DEGRADED = 2,
  MOONLINK_NATIVE_ENDED = 3,
} moonlink_native_session_state;

typedef enum moonlink_native_capture_probe_result {
  MOONLINK_NATIVE_CAPTURE_READY = 0,
  MOONLINK_NATIVE_CAPTURE_DEVICE_UNAVAILABLE = 1,
  MOONLINK_NATIVE_CAPTURE_ADAPTER_UNAVAILABLE = 2,
  MOONLINK_NATIVE_CAPTURE_OUTPUT_UNAVAILABLE = 3,
  MOONLINK_NATIVE_CAPTURE_DUPLICATION_UNAVAILABLE = 4,
  MOONLINK_NATIVE_CAPTURE_FRAME_UNAVAILABLE = 5,
} moonlink_native_capture_probe_result;

typedef enum moonlink_native_viewer_probe_result {
  MOONLINK_NATIVE_VIEWER_READY = 0,
  MOONLINK_NATIVE_VIEWER_WINDOW_UNAVAILABLE = 1,
  MOONLINK_NATIVE_VIEWER_DEVICE_UNAVAILABLE = 2,
  MOONLINK_NATIVE_VIEWER_SURFACE_UNAVAILABLE = 3,
  MOONLINK_NATIVE_VIEWER_PRESENT_UNAVAILABLE = 4,
} moonlink_native_viewer_probe_result;

int moonlink_native_bridge_abi_version(void);
int moonlink_native_display_count(void);
moonlink_native_capture_probe_result moonlink_native_desktop_duplication_probe(
  void
);
moonlink_native_capture_probe_result moonlink_native_desktop_frame_acquire_probe(
  void
);
moonlink_native_viewer_probe_result moonlink_native_d3d_viewer_probe(void);
int moonlink_native_h264_software_available(void);
int moonlink_native_h264_software_encode_probe(void);
int moonlink_native_h264_hardware_available(void);
int moonlink_native_input_allowed(moonlink_native_session_state state);

#ifdef __cplusplus
}
#endif

#endif
