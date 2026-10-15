///|
test "static registration replaces an implementation for the same role" {
  let registry = CapabilityRegistry::new()
  registry.register(CapabilityModule::{
    role: Encoder,
    implementation: HardwareH264Encoder,
  })
  registry.register(CapabilityModule::{
    role: Encoder,
    implementation: SoftwareH264Encoder,
  })

  inspect(registry.count(), content="1")
  match registry.implementation_for(Encoder) {
    Some(SoftwareH264Encoder) => ()
    _ => fail("encoder replacement was not retained")
  }
}

///|
test "each static capability role retains its own implementation" {
  let registry = CapabilityRegistry::new()
  registry.register(CapabilityModule::{
    role: Capture,
    implementation: WindowsDesktopCapture,
  })
  registry.register(CapabilityModule::{
    role: Transport,
    implementation: LibdatachannelTransport,
  })

  inspect(registry.count(), content="2")
  match registry.implementation_for(Capture) {
    Some(WindowsDesktopCapture) => ()
    _ => fail("capture implementation was not retained")
  }
  match registry.implementation_for(Transport) {
    Some(LibdatachannelTransport) => ()
    _ => fail("transport implementation was not retained")
  }
}
