///|
pub(all) struct SourcePoint {
  x : Int
  y : Int
}

///|
pub(all) struct FrameMapping {
  source_width : Int
  source_height : Int
  content_width : Int
  content_height : Int
  offset_x : Int
  offset_y : Int
}

///|
pub fn FrameMapping::fit(
  source_width : Int,
  source_height : Int,
  viewer_width : Int,
  viewer_height : Int,
) -> FrameMapping {
  if source_width * viewer_height > source_height * viewer_width {
    let content_height = source_height * viewer_width / source_width
    FrameMapping::{
      source_width,
      source_height,
      content_width: viewer_width,
      content_height,
      offset_x: 0,
      offset_y: (viewer_height - content_height) / 2,
    }
  } else {
    let content_width = source_width * viewer_height / source_height
    FrameMapping::{
      source_width,
      source_height,
      content_width,
      content_height: viewer_height,
      offset_x: (viewer_width - content_width) / 2,
      offset_y: 0,
    }
  }
}

///|
pub fn FrameMapping::to_source(
  self : FrameMapping,
  viewer_x : Int,
  viewer_y : Int,
) -> SourcePoint? {
  if viewer_x < self.offset_x ||
    viewer_x >= self.offset_x + self.content_width ||
    viewer_y < self.offset_y ||
    viewer_y >= self.offset_y + self.content_height {
    None
  } else {
    Some(SourcePoint::{
      x: (viewer_x - self.offset_x) * self.source_width / self.content_width,
      y: (viewer_y - self.offset_y) * self.source_height / self.content_height,
    })
  }
}
