///|
fn admitted_request() -> Session {
  let gate = OneTimeCodeGate::new("417208", 1000)
  Session::idle().request_control_with_code(gate, "417208", 500)
}

///|
test "approval enables input and stop revokes it" {
  let idle = Session::idle()
  inspect(idle.phase_name(), content="Idle")
  inspect(idle.accepts_input(), content="false")

  let gate = OneTimeCodeGate::new("417208", 1000)
  let waiting = idle.request_control_with_code(gate, "417208", 500)
  inspect(waiting.phase_name(), content="AwaitingApproval")
  inspect(waiting.accepts_input(), content="false")

  let connected = waiting.approve()
  inspect(connected.phase_name(), content="Connected")
  inspect(connected.accepts_input(), content="true")

  let stopped = connected.stop()
  inspect(stopped.phase_name(), content="Ended")
  inspect(stopped.accepts_input(), content="false")
}

///|
test "rejection ends the session without enabling input" {
  let rejected = admitted_request().reject()
  inspect(rejected.phase_name(), content="Ended")
  inspect(rejected.accepts_input(), content="false")
}

///|
test "approval outside a request does not enable input" {
  let idle_approval = Session::idle().approve()
  inspect(idle_approval.phase_name(), content="Idle")
  inspect(idle_approval.accepts_input(), content="false")

  let ended_approval = Session::idle().stop().approve()
  inspect(ended_approval.phase_name(), content="Ended")
  inspect(ended_approval.accepts_input(), content="false")
}

///|
test "degraded transport keeps an authorized control session alive" {
  let connected = admitted_request().approve()
  let degraded = connected.degrade()
  inspect(degraded.phase_name(), content="Degraded")
  inspect(degraded.accepts_input(), content="true")

  let restored = degraded.restore()
  inspect(restored.phase_name(), content="Connected")
  inspect(restored.accepts_input(), content="true")

  let stopped = degraded.stop()
  inspect(stopped.phase_name(), content="Ended")
  inspect(stopped.accepts_input(), content="false")
}

///|
test "session request requires an unconsumed unexpired code" {
  let gate = OneTimeCodeGate::new("417208", 1000)
  let denied = Session::idle().request_control_with_code(gate, "wrong", 500)
  inspect(denied.phase_name(), content="Idle")

  let allowed = Session::idle().request_control_with_code(gate, "417208", 500)
  inspect(allowed.phase_name(), content="AwaitingApproval")

  let replayed = Session::idle().request_control_with_code(gate, "417208", 501)
  inspect(replayed.phase_name(), content="Idle")
}
