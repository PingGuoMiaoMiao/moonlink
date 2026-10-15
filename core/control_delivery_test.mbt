///|
test "older pointer moves are discarded" {
  let filter = PointerMoveFilter::new()
  inspect(
    filter.should_forward(PointerMove::{ sequence: 4, x: 600, y: 320, }),
    content="true",
  )
  inspect(
    filter.should_forward(PointerMove::{ sequence: 3, x: 120, y: 90, }),
    content="false",
  )
  inspect(filter.latest_sequence(), content="4")
}

///|
test "input event classes keep buttons and keys ordered and reliable" {
  match ControllerEventKind::MouseMove.delivery() {
    Droppable => ()
    _ => fail("mouse moves must remain droppable")
  }
  match ControllerEventKind::MouseButton.delivery() {
    OrderedReliable => ()
    _ => fail("mouse buttons must remain ordered and reliable")
  }
  match ControllerEventKind::Keyboard.delivery() {
    OrderedReliable => ()
    _ => fail("keyboard events must remain ordered and reliable")
  }
}
