///|
pub(all) enum CapabilityRole {
  Capture
  Encoder
  Transport
  Input
  Viewer
  Telemetry
} derive(Eq)

///|
pub(all) enum CapabilityImplementation {
  WindowsDesktopCapture
  HardwareH264Encoder
  SoftwareH264Encoder
  LibdatachannelTransport
  WindowsInputExecutor
  D3DViewer
  TechnicalMetricsCollector
}

///|
pub(all) struct CapabilityModule {
  role : CapabilityRole
  implementation : CapabilityImplementation
}

///|
pub(all) struct CapabilityRegistry {
  mut modules : Array[CapabilityModule]
}

///|
pub fn CapabilityRegistry::new() -> CapabilityRegistry {
  CapabilityRegistry::{ modules: [], }
}

///|
pub fn CapabilityRegistry::register(
  self : CapabilityRegistry,
  registration : CapabilityModule,
) -> Unit {
  let retained : Array[CapabilityModule] = []
  for existing in self.modules {
    if existing.role != registration.role {
      retained.push(existing)
    }
  }
  retained.push(registration)
  self.modules = retained
}

///|
pub fn CapabilityRegistry::count(self : CapabilityRegistry) -> Int {
  self.modules.length()
}

///|
pub fn CapabilityRegistry::implementation_for(
  self : CapabilityRegistry,
  role : CapabilityRole,
) -> CapabilityImplementation? {
  for registration in self.modules {
    if registration.role == role {
      return Some(registration.implementation)
    }
  }
  None
}
