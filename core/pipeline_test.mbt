///|
test "pipeline state transitions" {
  let s = PipelineState::new()
  inspect(s.phase_name(), content="Uninitialized")

  let s1 = s.transition(Ready)
  inspect(s1.phase_name(), content="Ready")

  let s2 = s1.transition(Connecting)
  inspect(s2.phase_name(), content="Connecting")

  let s3 = s2.transition(Streaming)
  inspect(s3.phase_name(), content="Streaming")
  inspect(s3.is_streaming(), content="true")

  let s4 = s3.transition(DegradedStreaming)
  inspect(s4.phase_name(), content="DegradedStreaming")
  inspect(s4.is_streaming(), content="true")

  let s5 = s4.transition(Streaming)
  inspect(s5.phase_name(), content="Streaming")

  let s6 = s5.transition(Stopped)
  inspect(s6.phase_name(), content="Stopped")
  inspect(s6.is_streaming(), content="false")
}

///|
test "pipeline invalid transitions are rejected" {
  let s = PipelineState::new()
  // Cannot go directly from Uninitialized to Streaming.
  let bad = s.transition(Streaming)
  inspect(bad.phase_name(), content="Uninitialized")

  // Cannot go backwards from Stopped.
  let stopped = PipelineState::{ phase: Stopped, error_message: "", }
  let bad2 = stopped.transition(Ready)
  inspect(bad2.phase_name(), content="Stopped")
}

///|
test "pipeline error sets stopped" {
  let s = PipelineState::new().transition(Ready).transition(Connecting)
  let err = s.set_error("test failure")
  inspect(err.phase_name(), content="Stopped")
  inspect(err.error_message, content="test failure")
}
