///|
pub(all) enum EncoderPath {
  HardwareH264
  SoftwareH264
}

///|
pub(all) struct VideoProfile {
  width : Int
  height : Int
  frames_per_second : Int
  encoder_path : EncoderPath
}

///|
pub fn VideoProfile::for_h264_capability(
  hardware_available : Bool,
) -> VideoProfile {
  if hardware_available {
    VideoProfile::{
      width: 1280,
      height: 720,
      frames_per_second: 60,
      encoder_path: HardwareH264,
    }
  } else {
    VideoProfile::{
      width: 1280,
      height: 720,
      frames_per_second: 30,
      encoder_path: SoftwareH264,
    }
  }
}

///|
pub fn VideoProfile::is_degraded(self : VideoProfile) -> Bool {
  match self.encoder_path {
    HardwareH264 => false
    SoftwareH264 => true
  }
}
