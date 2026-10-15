///|
test "hardware H264 selects the 720p 60fps profile" {
  let profile = VideoProfile::for_h264_capability(true)
  inspect(profile.width, content="1280")
  inspect(profile.height, content="720")
  inspect(profile.frames_per_second, content="60")
  inspect(profile.is_degraded(), content="false")
}

///|
test "software H264 falls back to the 720p 30fps profile" {
  let profile = VideoProfile::for_h264_capability(false)
  inspect(profile.width, content="1280")
  inspect(profile.height, content="720")
  inspect(profile.frames_per_second, content="30")
  inspect(profile.is_degraded(), content="true")
}
