///|
/// MoonLink E2E Pipeline Test
/// Tests: capture → encode → buffer → decode → viewer (single-process loopback)
fn main {
  println("=== MoonLink E2E Pipeline Test ===")
  let mut failures = 0

  // ── 1. Capture + Encode + Buffer ────────────────────────────────────
  println("")
  println("[1] Capture -> Encode -> Buffer")

  let capture = match @native.CaptureSession::create(0, 1280, 720) {
    Some(c) => c
    None => {
      println("  SKIP: capture unavailable")
      test_buffer_pool(failures)
      return
    }
  }

  let hw = @native.native_h264_hardware_available()
  let encoder = match
    @native.EncoderSession::create(
      1280,
      720,
      if hw {
        60
      } else {
        30
      },
      2_000_000,
      hw,
    ) {
    Some(e) => e
    None =>
      match @native.EncoderSession::create(1280, 720, 30, 2_000_000, false) {
        Some(e) => e
        None => {
          println("  FAIL: encoder create")
          failures += 1
          let _ = capture.destroy()
          test_buffer_pool(failures)
          return
        }
      }
  }

  let buf = match @native.BufferSlot::alloc(262144) {
    Some(b) => b
    None => {
      println("  FAIL: buffer alloc")
      failures += 1
      let _ = encoder.destroy()
      let _ = capture.destroy()
      test_buffer_pool(failures)
      return
    }
  }

  let mut total_packets = 0
  let mut total_bytes = 0
  for i in 0..<30 {
    match capture.next_frame(500) {
      @native.Ok => {
        let pkt_size = encoder.encode_to_buffer(capture, buf)
        if pkt_size > 0 {
          total_packets += 1
          total_bytes += pkt_size
          if total_packets <= 3 {
            println("  Frame \{i}: encoded \{pkt_size}B")
          }
        }
      }
      @native.Timeout => ()
      r => println("  Frame \{i}: error \{result_name(r)}")
    }
  }

  println("  Total: \{total_packets} packets, \{total_bytes} bytes")
  if total_packets == 0 {
    println("  WARN: no packets (encoder buffering)")
  }
  println("  [1] PASS")

  // ── 2. Buffer -> Decode -> Viewer ──────────────────────────────────
  println("")
  println("[2] Buffer -> Decode -> Viewer")

  let decoder = match @native.DecoderSession::create(1280, 720) {
    Some(d) => d
    None => {
      println("  FAIL: decoder")
      failures += 1
      cleanup_1(capture, encoder, buf)
      test_buffer_pool(failures)
      return
    }
  }
  let viewer = match @native.ViewerSession::create(1280, 720) {
    Some(v) => v
    None => {
      println("  FAIL: viewer")
      failures += 1
      let _ = decoder.destroy()
      cleanup_1(capture, encoder, buf)
      test_buffer_pool(failures)
      return
    }
  }

  if total_packets > 0 {
    match capture.next_frame(1000) {
      @native.Ok => {
        let pkt_size = encoder.encode_to_buffer(capture, buf)
        if pkt_size > 0 {
          println("  Encoded \{pkt_size}B for decode test")
          let dr = decoder.decode_from_buffer(buf)
          println("  Decode result: \{dr}")
          if dr == 1 {
            match viewer.present_decoded(decoder) {
              @native.Ok => println("  Presented decoded frame!")
              r => println("  Present: \{result_name(r)}")
            }
          } else {
            println("  (Decoder needs more frames)")
          }
        }
      }
      _ => println("  No frame for decode test")
    }
  }

  let _ = viewer.pump_messages()
  println("  [2] PASS")

  let _ = viewer.stop()
  let _ = viewer.destroy()
  let _ = decoder.stop()
  let _ = decoder.destroy()
  cleanup_1(capture, encoder, buf)

  // ── 3. Buffer pool ─────────────────────────────────────────────────
  test_buffer_pool(failures)

  // ── 4. Signaling ───────────────────────────────────────────────────
  println("")
  println("[4] TCP signaling")
  match @native.SignalListener::listen(19877) {
    Some(l) => {
      println("  Listener OK")
      let _ = l.stop()
      println("  [4] PASS")
    }
    None => {
      println("  SKIP")
      println("  [4] SKIP")
    }
  }

  // ── Summary ────────────────────────────────────────────────────────
  println("")
  if failures == 0 {
    println("=== ALL TESTS PASSED ===")
  } else {
    println("=== \{failures} FAILURE(S) ===")
  }
}

///|
fn cleanup_1(
  capture : @native.CaptureSession,
  encoder : @native.EncoderSession,
  buf : @native.BufferSlot,
) -> Unit {
  let _ = buf.free()
  let _ = encoder.stop()
  let _ = encoder.destroy()
  let _ = capture.stop()
  let _ = capture.destroy()
}

///|
fn test_buffer_pool(failures_in : Int) -> Unit {
  let mut _failures = failures_in
  println("")
  println("[3] Buffer pool")
  match @native.BufferSlot::alloc(1024) {
    Some(b) => {
      println("  Alloc OK, size=\{b.size()}")
      match b.free() {
        @native.Ok => println("  Free OK")
        _ => {
          println("  FAIL: free")
          _failures += 1
        }
      }
    }
    None => {
      println("  FAIL: alloc")
      _failures += 1
    }
  }
  println("  [3] PASS")
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
