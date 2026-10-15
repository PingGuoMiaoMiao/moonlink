///|
pub(all) enum InputDelivery {
  Droppable
  OrderedReliable
}

///|
pub(all) enum ControllerEventKind {
  MouseMove
  MouseButton
  Keyboard
}

///|
pub fn ControllerEventKind::delivery(
  self : ControllerEventKind,
) -> InputDelivery {
  match self {
    MouseMove => Droppable
    MouseButton | Keyboard => OrderedReliable
  }
}

///|
pub(all) struct PointerMove {
  sequence : Int
  x : Int
  y : Int
}

///|
pub(all) struct PointerMoveFilter {
  mut latest_accepted : Int
}

///|
pub fn PointerMoveFilter::new() -> PointerMoveFilter {
  PointerMoveFilter::{ latest_accepted: -1, }
}

///|
pub fn PointerMoveFilter::should_forward(
  self : PointerMoveFilter,
  move_event : PointerMove,
) -> Bool {
  if move_event.sequence <= self.latest_accepted {
    false
  } else {
    self.latest_accepted = move_event.sequence
    true
  }
}

///|
pub fn PointerMoveFilter::latest_sequence(self : PointerMoveFilter) -> Int {
  self.latest_accepted
}
