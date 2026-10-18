///|
/// MoonLink Host (被控端)
/// Usage: moon run cmd/host --target native
fn main {
  println("=== MoonLink Host ===")

  let display_count = @native.native_display_count()
  if display_count <= 0 {
    println("ERROR: No displays.")
    return
  }
  println("Displays: \{display_count}")

  let hw = @native.native_h264_hardware_available()
  let fps = if hw { 60 } else { 30 }
  println("Encoder: \{if hw { "HW" } else { "SW" }} @ \{fps}fps")

  // Create sessions.
  let capture = match @native.CaptureSession::create(0, 1280, 720) {
    Some(c) => c
    None => {
      println("ERROR: capture create failed")
      return
    }
  }
  let encoder = match
    @native.EncoderSession::create(1280, 720, fps, 2_000_000, hw) {
    Some(e) => e
    None =>
      // Fallback to SW.
      match @native.EncoderSession::create(1280, 720, 30, 2_000_000, false) {
        Some(e) => e
        None => {
          println("ERROR: encoder create failed")
          let _ = capture.destroy()
          return
        }
      }
  }
  let transport = match @native.TransportSession::create(@native.Host) {
    Some(t) => t
    None => {
      println("ERROR: transport create failed")
      cleanup(capture, encoder)
      return
    }
  }

  // Start signaling listener.
  let port = 9876
  println("Listening on port \{port}...")
  let listener = match @native.SignalListener::listen(port) {
    Some(l) => l
    None => {
      println("ERROR: listen failed")
      cleanup(capture, encoder)
      let _ = transport.destroy()
      return
    }
  }

  println("Waiting for controller to connect...")
  let signal = match listener.accept() {
    Some(s) => s
    None => {
      println("ERROR: accept failed")
      let _ = listener.stop()
      cleanup(capture, encoder)
      let _ = transport.destroy()
      return
    }
  }
  let _ = listener.stop()
  println("Controller connected!")

  // SDP exchange: host sends offer.
  let _ = transport.start()
  println("Sending SDP offer...")
  match transport.send_local_desc_via_signal(signal) {
    @native.Ok => println("SDP offer sent.")
    r => {
      println("ERROR: send SDP failed: \{result_name(r)}")
      cleanup_all(capture, encoder, transport, signal)
      return
    }
  }

  // Receive answer from controller.
  println("Waiting for SDP answer...")
  match transport.set_remote_from_signal(signal) {
    @native.Ok => println("SDP answer received and set.")
    r => {
      println("ERROR: recv SDP failed: \{result_name(r)}")
      cleanup_all(capture, encoder, transport, signal)
      return
    }
  }

  // Wait for WebRTC connection.
  println("Waiting for WebRTC connection...")
  let mut connected = false
  for _ in 0..<100 {
    if transport.is_connected() {
      connected = true
      break
    }
    sleep_ms(50)
  }
  if !connected {
    println("ERROR: WebRTC connection timeout")
    cleanup_all(capture, encoder, transport, signal)
    return
  }
  println("WebRTC connected! Streaming...")

  // Allocate a buffer for encoded packets.
  let buf = match @native.BufferSlot::alloc(262144) {
    Some(b) => b
    None => {
      println("ERROR: buffer alloc failed")
      cleanup_all(capture, encoder, transport, signal)
      return
    }
  }

  // Main streaming loop: capture → encode → buffer → send via WebRTC.
  let mut frame_count = 0
  let mut sent_count = 0
  let mut total_bytes = 0
  let max_frames = 300

  while frame_count < max_frames {
    match capture.next_frame(100) {
      @native.Ok => {
        frame_count += 1
        let pkt_size = encoder.encode_to_buffer(capture, buf)
        if pkt_size > 0 {
          match transport.send_video_from_buffer(buf) {
            @native.Ok => {
              sent_count += 1
              total_bytes += pkt_size
            }
            r =>
              println("  Send error at frame \{frame_count}: \{result_name(r)}")
          }
        }
        if frame_count % 60 == 0 && frame_count > 0 {
          println(
            "  \{frame_count} frames captured, \{sent_count} sent (\{total_bytes / 1024}KB)",
          )
        }
      }
      @native.Timeout => ()
      _ => {
        println("Capture error, stopping.")
        break
      }
    }
  }

  println(
    "Streaming complete: \{frame_count} frames, \{sent_count} sent, \{total_bytes / 1024}KB total",
  )

  // Cleanup.
  let _ = buf.free()
  cleanup_all(capture, encoder, transport, signal)
  println("Done.")
}

///|
fn cleanup(
  capture : @native.CaptureSession,
  encoder : @native.EncoderSession,
) -> Unit {
  let _ = encoder.stop()
  let _ = encoder.destroy()
  let _ = capture.stop()
  let _ = capture.destroy()
}

///|
fn cleanup_all(
  capture : @native.CaptureSession,
  encoder : @native.EncoderSession,
  transport : @native.TransportSession,
  signal : @native.SignalSession,
) -> Unit {
  let _ = signal.close()
  let _ = transport.stop()
  let _ = transport.destroy()
  cleanup(capture, encoder)
}

///|
fn result_name(r : @native.NativeResult) -> String {
  match r {
    @native.Ok => "OK"
    @native.InvalidHandle => "InvalidHandle"
    @native.OutOfMemory => "OutOfMemory"
    @native.DeviceUnavailable => "DeviceUnavailable"
    @native.EncoderUnavailable => "EncoderUnavailable"
    @native.DecoderUnavailable => "DecoderUnavailable"
    @native.TransportUnavailable => "TransportUnavailable"
    @native.ViewerUnavailable => "ViewerUnavailable"
    @native.AlreadyStopped => "AlreadyStopped"
    @native.Timeout => "Timeout"
    @native.InternalError => "InternalError"
  }
}

///|
extern "C" fn c_sleep_ms(ms : Int) -> Unit = "Sleep"

///|
fn sleep_ms(ms : Int) -> Unit {
  c_sleep_ms(ms)
}
