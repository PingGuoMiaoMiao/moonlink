///|
/// MoonLink Controller (控制端)
/// Usage: moon run cmd/controller --target native
fn main {
  println("=== MoonLink Controller ===")

  let host_ip = 0x7F000001 // 127.0.0.1 in host byte order
  let port = 9876

  // Create sessions.
  let decoder = match @native.DecoderSession::create(1280, 720) {
    Some(d) => d
    None => {
      println("ERROR: decoder create failed")
      return
    }
  }
  let viewer = match @native.ViewerSession::create(1280, 720) {
    Some(v) => v
    None => {
      println("ERROR: viewer create failed")
      let _ = decoder.destroy()
      return
    }
  }
  let transport = match @native.TransportSession::create(@native.Controller) {
    Some(t) => t
    None => {
      println("ERROR: transport create failed")
      cleanup(decoder, viewer)
      return
    }
  }

  // Connect to host via TCP signaling.
  println("Connecting to host 127.0.0.1:\{port}...")
  let signal = match @native.SignalSession::connect_ip(host_ip, port) {
    Some(s) => s
    None => {
      println("ERROR: connect failed (is host running?)")
      cleanup(decoder, viewer)
      let _ = transport.destroy()
      return
    }
  }
  println("Connected to host!")

  // SDP exchange: controller receives offer, sends answer.
  println("Receiving SDP offer from host...")
  match transport.set_remote_from_signal(signal) {
    @native.Ok => println("SDP offer received and set.")
    r => {
      println("ERROR: recv SDP failed: \{result_name(r)}")
      cleanup_all(decoder, viewer, transport, signal)
      return
    }
  }

  let _ = transport.start()
  println("Sending SDP answer...")
  match transport.send_local_desc_via_signal(signal) {
    @native.Ok => println("SDP answer sent.")
    r => {
      println("ERROR: send SDP failed: \{result_name(r)}")
      cleanup_all(decoder, viewer, transport, signal)
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
    cleanup_all(decoder, viewer, transport, signal)
    return
  }
  println("WebRTC connected! Receiving video...")

  // Allocate receive buffer.
  let buf = match @native.BufferSlot::alloc(262144) {
    Some(b) => b
    None => {
      println("ERROR: buffer alloc failed")
      cleanup_all(decoder, viewer, transport, signal)
      return
    }
  }

  // Main receive loop: transport → buffer → decode → present.
  let mut packets_received = 0
  let mut frames_decoded = 0
  let mut frames_presented = 0
  let max_iterations = 600 // ~10 seconds at 60fps

  for i in 0..<max_iterations {
    // Check for pending video packets from WebRTC.
    let pending = transport.pending_video_count()
    if pending > 0 {
      let pkt_size = transport.pop_video_to_buffer(buf)
      if pkt_size > 0 {
        packets_received += 1

        // Decode H.264 packet.
        let dr = decoder.decode_from_buffer(buf)
        if dr == 1 {
          frames_decoded += 1
          // Present decoded frame.
          match viewer.present_decoded(decoder) {
            @native.Ok => frames_presented += 1
            _ => ()
          }
        }
      }
    }

    // Pump viewer window messages.
    if !viewer.pump_messages() {
      println("Viewer window closed by user.")
      break
    }

    // Progress report.
    if i > 0 && i % 120 == 0 {
      println(
        "  \{packets_received} pkts, \{frames_decoded} decoded, \{frames_presented} presented",
      )
    }

    sleep_ms(16)
  }

  println("")
  println("Session complete:")
  println("  Packets received: \{packets_received}")
  println("  Frames decoded:   \{frames_decoded}")
  println("  Frames presented: \{frames_presented}")

  // Cleanup.
  let _ = buf.free()
  cleanup_all(decoder, viewer, transport, signal)
  println("Done.")
}

///|
fn cleanup(
  decoder : @native.DecoderSession,
  viewer : @native.ViewerSession,
) -> Unit {
  let _ = viewer.stop()
  let _ = viewer.destroy()
  let _ = decoder.stop()
  let _ = decoder.destroy()
}

///|
fn cleanup_all(
  decoder : @native.DecoderSession,
  viewer : @native.ViewerSession,
  transport : @native.TransportSession,
  signal : @native.SignalSession,
) -> Unit {
  let _ = signal.close()
  let _ = transport.stop()
  let _ = transport.destroy()
  cleanup(decoder, viewer)
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
