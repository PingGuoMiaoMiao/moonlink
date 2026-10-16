///|
fn capture_probe_ready(result : @native.CaptureProbeResult) -> Bool {
  match result {
    @native.CaptureReady => true
    _ => false
  }
}

///|
fn viewer_probe_ready(result : @native.ViewerProbeResult) -> Bool {
  match result {
    @native.ViewerReady => true
    _ => false
  }
}

///|
fn main {
  if @native.native_abi_version() != 1 {
    println("MoonLink native diagnostics: ABI mismatch")
    return
  }

  if @native.native_display_count() <= 0 {
    println("MoonLink native diagnostics: no display reported")
    return
  }

  let capture_ready = capture_probe_ready(
    @native.native_desktop_duplication_probe(),
  )
  let frame_acquire_ready = capture_probe_ready(
    @native.native_desktop_frame_acquire_probe(),
  )
  let viewer_ready = viewer_probe_ready(@native.native_d3d_viewer_probe())
  let software_h264 = @native.native_h264_software_available()
  let software_h264_encoded = @native.native_h264_software_encode_probe()
  let hardware_h264 = @native.native_h264_hardware_available()

  if !capture_ready ||
    !frame_acquire_ready ||
    !viewer_ready ||
    !software_h264 ||
    !software_h264_encoded {
    println("MoonLink native diagnostics: a required probe is unavailable")
    return
  }

  if hardware_h264 {
    println(
      "MoonLink native diagnostics: software H.264 encoding and hardware H.264 activation ready",
    )
  } else {
    println(
      "MoonLink native diagnostics: software H.264 encoding fallback ready",
    )
  }
}
