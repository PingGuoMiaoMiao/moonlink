///|
pub(all) enum TechnicalEventKind {
  SessionConnected
  VideoProfileDegraded
  TransportCongested
  TransportRecovered
  SessionEnded
}

/// TechnicalEvent intentionally has no user-provided content fields.

///|
pub(all) struct TechnicalEvent {
  kind : TechnicalEventKind
  elapsed_millis : Int
  metric_value : Int
}

///|
pub(all) struct TechnicalTrace {
  mut events : Array[TechnicalEvent]
}

///|
pub fn TechnicalTrace::new() -> TechnicalTrace {
  TechnicalTrace::{ events: [], }
}

///|
pub fn TechnicalTrace::record(
  self : TechnicalTrace,
  event : TechnicalEvent,
) -> Unit {
  self.events.push(event)
}

///|
pub fn TechnicalTrace::count(self : TechnicalTrace) -> Int {
  self.events.length()
}

///|
pub fn TechnicalTrace::at(self : TechnicalTrace, index : Int) -> TechnicalEvent {
  self.events[index]
}
