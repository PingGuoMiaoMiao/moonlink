///|
pub(all) enum SessionPhase {
  Idle
  AwaitingApproval
  Connected
  Degraded
  Ended
}

///|
pub(all) struct Session {
  phase : SessionPhase
}

///|
pub fn Session::idle() -> Session {
  Session::{ phase: Idle, }
}

///|
pub fn Session::request_control_with_code(
  self : Session,
  gate : OneTimeCodeGate,
  submitted_code : String,
  now_millis : Int,
) -> Session {
  match self.phase {
    Idle =>
      if gate.authorize(submitted_code, now_millis) {
        Session::{ phase: AwaitingApproval, }
      } else {
        self
      }
    _ => self
  }
}

///|
pub fn Session::approve(self : Session) -> Session {
  match self.phase {
    AwaitingApproval => Session::{ phase: Connected, }
    _ => self
  }
}

///|
pub fn Session::reject(self : Session) -> Session {
  match self.phase {
    AwaitingApproval => Session::{ phase: Ended, }
    _ => self
  }
}

///|
pub fn Session::degrade(self : Session) -> Session {
  match self.phase {
    Connected => Session::{ phase: Degraded, }
    _ => self
  }
}

///|
pub fn Session::restore(self : Session) -> Session {
  match self.phase {
    Degraded => Session::{ phase: Connected, }
    _ => self
  }
}

///|
pub fn Session::stop(self : Session) -> Session {
  match self.phase {
    Idle | AwaitingApproval | Connected | Degraded | Ended =>
      Session::{ phase: Ended, }
  }
}

///|
pub fn Session::phase_name(self : Session) -> String {
  match self.phase {
    Idle => "Idle"
    AwaitingApproval => "AwaitingApproval"
    Connected => "Connected"
    Degraded => "Degraded"
    Ended => "Ended"
  }
}

///|
pub fn Session::phase(self : Session) -> SessionPhase {
  self.phase
}

///|
pub fn Session::accepts_input(self : Session) -> Bool {
  match self.phase {
    Connected | Degraded => true
    _ => false
  }
}
