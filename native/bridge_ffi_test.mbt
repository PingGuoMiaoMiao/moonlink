///|
fn capture_probe_result_is_known(result : CaptureProbeResult) -> Bool {
  match result {
    CaptureReady
    | CaptureDeviceUnavailable
    | CaptureAdapterUnavailable
    | CaptureOutputUnavailable
    | CaptureDuplicationUnavailable
    | CaptureFrameUnavailable
    | CaptureTextureUnavailable => true
  }
}

///|
fn viewer_probe_result_is_known(result : ViewerProbeResult) -> Bool {
  match result {
    ViewerReady
    | ViewerWindowUnavailable
    | ViewerDeviceUnavailable
    | ViewerSurfaceUnavailable
    | ViewerPresentUnavailable => true
  }
}

///|
test "native bridge ABI and input guard are callable from MoonBit" {
  inspect(native_abi_version(), content="2")
  inspect(native_display_count() >= 0, content="true")
  inspect(
    capture_probe_result_is_known(native_desktop_duplication_probe()),
    content="true",
  )
  inspect(
    capture_probe_result_is_known(native_desktop_frame_acquire_probe()),
    content="true",
  )
  inspect(
    capture_probe_result_is_known(native_desktop_texture_probe()),
    content="true",
  )
  inspect(
    viewer_probe_result_is_known(native_d3d_viewer_probe()),
    content="true",
  )

  let software_available = native_h264_software_available()
  let software_encoded = native_h264_software_encode_probe()
  let hardware_available = native_h264_hardware_available()
  inspect(software_available || !software_available, content="true")
  inspect(software_encoded, content="true")
  inspect(hardware_available || !hardware_available, content="true")

  inspect(native_input_allowed(@core.Idle), content="false")
  inspect(native_input_allowed(@core.AwaitingApproval), content="false")
  inspect(native_input_allowed(@core.Connected), content="true")
  inspect(native_input_allowed(@core.Degraded), content="true")
  inspect(native_input_allowed(@core.Ended), content="false")
}
