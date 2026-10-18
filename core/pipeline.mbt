/// Pipeline orchestration state machine for host and controller.
/// Tracks the lifecycle of capture→encode→transport (host) and
/// transport→decode→view (controller) pipelines.

///|
pub(all) enum PipelinePhase {
  /// Initial state, no resources allocated.
  Uninitialized
  /// Resources created, waiting for signaling connection.
  Ready
  /// SDP exchange in progress.
  Connecting
  /// Media flowing.
  Streaming
  /// Degraded (e.g. software fallback or congestion).
  DegradedStreaming
  /// Stopped or error.
  Stopped
}

///|
pub(all) struct PipelineState {
  phase : PipelinePhase
  mut error_message : String
}

///|
pub fn PipelineState::new() -> PipelineState {
  PipelineState::{ phase: Uninitialized, error_message: "", }
}

///|
pub fn PipelineState::transition(
  self : PipelineState,
  target : PipelinePhase,
) -> PipelineState {
  let valid = match (self.phase, target) {
    (Uninitialized, Ready) => true
    (Ready, Connecting) => true
    (Connecting, Streaming) => true
    (Connecting, DegradedStreaming) => true
    (Streaming, DegradedStreaming) => true
    (DegradedStreaming, Streaming) => true
    (Streaming, Stopped) => true
    (DegradedStreaming, Stopped) => true
    (Connecting, Stopped) => true
    (Ready, Stopped) => true
    (_, _) => false
  }
  if valid {
    PipelineState::{ phase: target, error_message: self.error_message, }
  } else {
    self
  }
}

///|
pub fn PipelineState::set_error(
  _self : PipelineState,
  msg : String,
) -> PipelineState {
  PipelineState::{ phase: Stopped, error_message: msg, }
}

///|
pub fn PipelineState::is_streaming(self : PipelineState) -> Bool {
  match self.phase {
    Streaming | DegradedStreaming => true
    _ => false
  }
}

///|
pub fn PipelineState::phase_name(self : PipelineState) -> String {
  match self.phase {
    Uninitialized => "Uninitialized"
    Ready => "Ready"
    Connecting => "Connecting"
    Streaming => "Streaming"
    DegradedStreaming => "DegradedStreaming"
    Stopped => "Stopped"
  }
}
