///|
extern "C" fn native_abi_version_ffi() -> Int = "moonlink_native_bridge_abi_version"

///|
extern "C" fn native_display_count_ffi() -> Int = "moonlink_native_display_count"

///|
extern "C" fn native_desktop_duplication_probe_result_ffi() -> Int = "moonlink_native_desktop_duplication_probe"

///|
extern "C" fn native_desktop_frame_acquire_probe_result_ffi() -> Int = "moonlink_native_desktop_frame_acquire_probe"

///|
extern "C" fn native_d3d_viewer_probe_result_ffi() -> Int = "moonlink_native_d3d_viewer_probe"

///|
extern "C" fn native_h264_software_available_ffi() -> Int = "moonlink_native_h264_software_available"

///|
extern "C" fn native_h264_software_encode_probe_ffi() -> Int = "moonlink_native_h264_software_encode_probe"

///|
extern "C" fn native_h264_hardware_available_ffi() -> Int = "moonlink_native_h264_hardware_available"

///|
extern "C" fn native_input_allowed_ffi(state : Int) -> Int = "moonlink_native_input_allowed"

///|
pub(all) enum CaptureProbeResult {
  CaptureReady
  CaptureDeviceUnavailable
  CaptureAdapterUnavailable
  CaptureOutputUnavailable
  CaptureDuplicationUnavailable
  CaptureFrameUnavailable
}

///|
pub(all) enum ViewerProbeResult {
  ViewerReady
  ViewerWindowUnavailable
  ViewerDeviceUnavailable
  ViewerSurfaceUnavailable
  ViewerPresentUnavailable
}

///|
/// Returns the version of the value-only C ABI.
pub fn native_abi_version() -> Int {
  native_abi_version_ffi()
}

///|
/// Returns the monitor count reported by the Windows probe.
pub fn native_display_count() -> Int {
  native_display_count_ffi()
}

///|
/// Returns the desktop-duplication probe result without exposing C enum values.
pub fn native_desktop_duplication_probe() -> CaptureProbeResult {
  match native_desktop_duplication_probe_result_ffi() {
    0 => CaptureReady
    1 => CaptureDeviceUnavailable
    2 => CaptureAdapterUnavailable
    3 => CaptureOutputUnavailable
    4 => CaptureDuplicationUnavailable
    5 => CaptureFrameUnavailable
    _ => abort("unexpected native desktop-duplication probe result")
  }
}

///|
/// Returns the result of acquiring and releasing one desktop frame.
pub fn native_desktop_frame_acquire_probe() -> CaptureProbeResult {
  match native_desktop_frame_acquire_probe_result_ffi() {
    0 => CaptureReady
    1 => CaptureDeviceUnavailable
    2 => CaptureAdapterUnavailable
    3 => CaptureOutputUnavailable
    4 => CaptureDuplicationUnavailable
    5 => CaptureFrameUnavailable
    _ => abort("unexpected native desktop-frame probe result")
  }
}

///|
/// Returns the D3D viewer probe result without exposing C enum values.
pub fn native_d3d_viewer_probe() -> ViewerProbeResult {
  match native_d3d_viewer_probe_result_ffi() {
    0 => ViewerReady
    1 => ViewerWindowUnavailable
    2 => ViewerDeviceUnavailable
    3 => ViewerSurfaceUnavailable
    4 => ViewerPresentUnavailable
    _ => abort("unexpected native D3D viewer probe result")
  }
}

///|
/// Reports whether Windows activated a software H.264 encoder MFT.
pub fn native_h264_software_available() -> Bool {
  native_h264_software_available_ffi() == 1
}

///|
/// Runs the software H.264 probe and reports whether it produced a nonempty sample.
pub fn native_h264_software_encode_probe() -> Bool {
  native_h264_software_encode_probe_ffi() == 1
}

///|
/// Reports whether Windows activated a hardware H.264 encoder MFT.
pub fn native_h264_hardware_available() -> Bool {
  native_h264_hardware_available_ffi() == 1
}

///|
/// Applies the native input policy to the MoonBit session phase.
pub fn native_input_allowed(phase : @core.SessionPhase) -> Bool {
  let native_state = match phase {
    @core.Idle | @core.AwaitingApproval => 0
    @core.Connected => 1
    @core.Degraded => 2
    @core.Ended => 3
  }
  native_input_allowed_ffi(native_state) == 1
}
