/// The code is transient authorization state and is never sent to TechnicalTrace.

///|
pub(all) struct OneTimeCodeGate {
  secret : String
  expires_at_millis : Int
  mut consumed : Bool
}

///|
pub fn OneTimeCodeGate::new(
  secret : String,
  expires_at_millis : Int,
) -> OneTimeCodeGate {
  OneTimeCodeGate::{ secret, expires_at_millis, consumed: false, }
}

///|
pub fn OneTimeCodeGate::authorize(
  self : OneTimeCodeGate,
  submitted : String,
  now_millis : Int,
) -> Bool {
  if self.consumed ||
    now_millis > self.expires_at_millis ||
    self.secret != submitted {
    false
  } else {
    self.consumed = true
    true
  }
}
