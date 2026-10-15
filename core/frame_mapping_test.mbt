///|
test "letterbox preserves a 5 by 4 display inside 16 by 9 video" {
  let mapping = FrameMapping::fit(1280, 1024, 1280, 720)
  inspect(mapping.content_width, content="900")
  inspect(mapping.content_height, content="720")
  inspect(mapping.offset_x, content="190")
  inspect(mapping.offset_y, content="0")
}

///|
test "letterbox maps only points in the visible content" {
  let mapping = FrameMapping::fit(1280, 1024, 1280, 720)

  match mapping.to_source(640, 360) {
    None => panic()
    Some(point) => {
      inspect(point.x, content="640")
      inspect(point.y, content="512")
    }
  }

  match mapping.to_source(189, 100) {
    None => println("left letterbox ignored")
    Some(_) => panic()
  }
}
