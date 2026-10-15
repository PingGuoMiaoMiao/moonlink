///|
test "one-time code accepts one matching attempt before expiry" {
  let gate = OneTimeCodeGate::new("417208", 1000)

  inspect(gate.authorize("wrong", 500), content="false")
  inspect(gate.authorize("417208", 500), content="true")
  inspect(gate.authorize("417208", 501), content="false")
}

///|
test "one-time code rejects a matching attempt after expiry" {
  let gate = OneTimeCodeGate::new("417208", 1000)
  inspect(gate.authorize("417208", 1001), content="false")
  inspect(gate.authorize("417208", 999), content="true")
}
