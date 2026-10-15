///|
test "technical trace records only typed technical fields" {
  let trace = TechnicalTrace::new()
  trace.record(TechnicalEvent::{
    kind: SessionConnected,
    elapsed_millis: 12,
    metric_value: 0,
  })
  trace.record(TechnicalEvent::{
    kind: VideoProfileDegraded,
    elapsed_millis: 34,
    metric_value: 30,
  })

  inspect(trace.count(), content="2")
  let first = trace.at(0)
  inspect(first.elapsed_millis, content="12")
  inspect(first.metric_value, content="0")
  match first.kind {
    SessionConnected => ()
    _ => fail("first trace event changed kind")
  }

  let second = trace.at(1)
  inspect(second.elapsed_millis, content="34")
  inspect(second.metric_value, content="30")
  match second.kind {
    VideoProfileDegraded => ()
    _ => fail("second trace event changed kind")
  }
}
