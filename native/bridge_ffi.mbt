// ══════════════════════════════════════════════════════════════════════
// ABI v1 probe functions (unchanged)
// ══════════════════════════════════════════════════════════════════════

///|
extern "C" fn native_abi_version_ffi() -> Int = "moonlink_native_bridge_abi_version"

///|
extern "C" fn native_display_count_ffi() -> Int = "moonlink_native_display_count"

///|
extern "C" fn native_desktop_duplication_probe_result_ffi() -> Int = "moonlink_native_desktop_duplication_probe"

///|
extern "C" fn native_desktop_frame_acquire_probe_result_ffi() -> Int = "moonlink_native_desktop_frame_acquire_probe"

///|
extern "C" fn native_desktop_texture_probe_result_ffi() -> Int = "moonlink_native_desktop_texture_probe"

///|
extern "C" fn native_d3d_viewer_probe_result_ffi() -> Int = "moonlink_native_d3d_viewer_probe"

///|
extern "C" fn native_h264_software_available_ffi() -> Int = "moonlink_native_h264_software_available"

///|
extern "C" fn native_h264_software_encode_probe_ffi() -> Int = "moonlink_native_h264_software_encode_probe"

///|
extern "C" fn native_h264_hardware_available_ffi() -> Int = "moonlink_native_h264_hardware_available"

///|
extern "C" fn native_input_allowed_ffi(state : Int) -> Int = "moonlink_native_input_allowed"

// ══════════════════════════════════════════════════════════════════════
// ABI v2 session-handle FFI (integer handles via C-side handle table)
// ══════════════════════════════════════════════════════════════════════

// ── Capture ───────────────────────────────────────────────────────────

///|
extern "C" fn capture_create_ffi(
  output_index : Int,
  target_width : Int,
  target_height : Int,
) -> Int = "moonlink_capture_create_ffi"

///|
extern "C" fn capture_next_frame_ffi(handle : Int, timeout_ms : Int) -> Int = "moonlink_capture_next_frame_ffi"

///|
extern "C" fn capture_frame_width_ffi(handle : Int) -> Int = "moonlink_capture_frame_width_ffi"

///|
extern "C" fn capture_frame_height_ffi(handle : Int) -> Int = "moonlink_capture_frame_height_ffi"

///|
extern "C" fn capture_frame_stride_y_ffi(handle : Int) -> Int = "moonlink_capture_frame_stride_y_ffi"

///|
extern "C" fn capture_frame_stride_uv_ffi(handle : Int) -> Int = "moonlink_capture_frame_stride_uv_ffi"

///|
extern "C" fn capture_frame_timestamp_us_ffi(handle : Int) -> Int64 = "moonlink_capture_frame_timestamp_us_ffi"

///|
// DISABLED: extern "C" fn capture_frame_copy_nv12_ffi(handle : Int, dst : Bytes, dst_size : Int) -> Int = "moonlink_capture_frame_copy_nv12_ffi"

///|
extern "C" fn capture_stop_ffi(handle : Int) -> Int = "moonlink_capture_stop_ffi"

///|
extern "C" fn capture_destroy_ffi(handle : Int) -> Int = "moonlink_capture_destroy_ffi"

// ── Encoder ───────────────────────────────────────────────────────────

///|
extern "C" fn encoder_create_ffi(
  width : Int,
  height : Int,
  fps : Int,
  bitrate : Int,
  use_hardware : Int,
) -> Int = "moonlink_encoder_create_ffi"

///|
extern "C" fn encoder_encode_from_capture_ffi(
  encoder_handle : Int,
  capture_handle : Int,
) -> Int = "moonlink_encoder_encode_from_capture_ffi"

///|
extern "C" fn encoder_packet_count_ffi(handle : Int) -> Int = "moonlink_encoder_packet_count_ffi"

///|
extern "C" fn encoder_packet_size_ffi(handle : Int, index : Int) -> Int = "moonlink_encoder_packet_size_ffi"

///|
extern "C" fn encoder_packet_is_keyframe_ffi(handle : Int, index : Int) -> Int = "moonlink_encoder_packet_is_keyframe_ffi"

///|
extern "C" fn encoder_packet_timestamp_us_ffi(
  handle : Int,
  index : Int,
) -> Int64 = "moonlink_encoder_packet_timestamp_us_ffi"

///|
// DISABLED: extern "C" fn encoder_packet_copy_ffi(handle : Int, index : Int, dst : Bytes, dst_size : Int) -> Int = "moonlink_encoder_packet_copy_ffi"

///|
extern "C" fn encoder_stop_ffi(handle : Int) -> Int = "moonlink_encoder_stop_ffi"

///|
extern "C" fn encoder_destroy_ffi(handle : Int) -> Int = "moonlink_encoder_destroy_ffi"

// ── Transport ─────────────────────────────────────────────────────────

///|
extern "C" fn transport_create_ffi(role : Int) -> Int = "moonlink_transport_create_ffi"

///|
extern "C" fn transport_start_ffi(handle : Int) -> Int = "moonlink_transport_start_ffi"

///|
// DISABLED: extern "C" fn transport_get_local_description_ffi(handle : Int, buffer : Bytes, buffer_size : Int) -> Int = "moonlink_transport_get_local_description_ffi"

///|
// DISABLED: extern "C" fn transport_set_remote_description_ffi(handle : Int, sdp : String, type_ : String) -> Int = "moonlink_transport_set_remote_description_ffi"

///|
// DISABLED: extern "C" fn transport_add_remote_candidate_ffi(handle : Int, candidate : String, mid : String) -> Int = "moonlink_transport_add_remote_candidate_ffi"

///|
// DISABLED: extern "C" fn transport_send_video_ffi(handle : Int, data : Bytes, size : Int) -> Int = "moonlink_transport_send_video_ffi"

///|
// DISABLED: extern "C" fn transport_send_control_ffi(handle : Int, message : String, size : Int) -> Int = "moonlink_transport_send_control_ffi"

///|
extern "C" fn transport_is_connected_ffi(handle : Int) -> Int = "moonlink_transport_is_connected_ffi"

///|
extern "C" fn transport_stop_ffi(handle : Int) -> Int = "moonlink_transport_stop_ffi"

///|
extern "C" fn transport_destroy_ffi(handle : Int) -> Int = "moonlink_transport_destroy_ffi"

// ── Decoder ───────────────────────────────────────────────────────────

///|
extern "C" fn decoder_create_ffi(width : Int, height : Int) -> Int = "moonlink_decoder_create_ffi"

///|
// DISABLED: extern "C" fn decoder_decode_ffi(handle : Int, data : Bytes, size : Int) -> Int = "moonlink_decoder_decode_ffi"

///|
extern "C" fn decoder_frame_width_ffi(handle : Int) -> Int = "moonlink_decoder_frame_width_ffi"

///|
extern "C" fn decoder_frame_height_ffi(handle : Int) -> Int = "moonlink_decoder_frame_height_ffi"

///|
extern "C" fn decoder_frame_stride_ffi(handle : Int) -> Int = "moonlink_decoder_frame_stride_ffi"

///|
// DISABLED: extern "C" fn decoder_frame_copy_bgra_ffi(handle : Int, dst : Bytes, dst_size : Int) -> Int = "moonlink_decoder_frame_copy_bgra_ffi"

///|
extern "C" fn decoder_stop_ffi(handle : Int) -> Int = "moonlink_decoder_stop_ffi"

///|
extern "C" fn decoder_destroy_ffi(handle : Int) -> Int = "moonlink_decoder_destroy_ffi"

// ── Viewer ────────────────────────────────────────────────────────────

///|
extern "C" fn viewer_create_ffi(width : Int, height : Int) -> Int = "moonlink_viewer_create_ffi"

///|
extern "C" fn viewer_present_from_decoder_ffi(
  viewer_handle : Int,
  decoder_handle : Int,
) -> Int = "moonlink_viewer_present_from_decoder_ffi"

///|
extern "C" fn viewer_pump_messages_ffi(handle : Int) -> Int = "moonlink_viewer_pump_messages_ffi"

///|
extern "C" fn viewer_stop_ffi(handle : Int) -> Int = "moonlink_viewer_stop_ffi"

///|
extern "C" fn viewer_destroy_ffi(handle : Int) -> Int = "moonlink_viewer_destroy_ffi"

///|
pub(all) enum CaptureProbeResult {
  CaptureReady
  CaptureDeviceUnavailable
  CaptureAdapterUnavailable
  CaptureOutputUnavailable
  CaptureDuplicationUnavailable
  CaptureFrameUnavailable
  CaptureTextureUnavailable
}

///|
pub(all) enum ViewerProbeResult {
  ViewerReady
  ViewerWindowUnavailable
  ViewerDeviceUnavailable
  ViewerSurfaceUnavailable
  ViewerPresentUnavailable
}

///|
/// Returns the version of the value-only C ABI.
pub fn native_abi_version() -> Int {
  native_abi_version_ffi()
}

///|
/// Returns the monitor count reported by the Windows probe.
pub fn native_display_count() -> Int {
  native_display_count_ffi()
}

///|
/// Returns the desktop-duplication probe result without exposing C enum values.
pub fn native_desktop_duplication_probe() -> CaptureProbeResult {
  match native_desktop_duplication_probe_result_ffi() {
    0 => CaptureReady
    1 => CaptureDeviceUnavailable
    2 => CaptureAdapterUnavailable
    3 => CaptureOutputUnavailable
    4 => CaptureDuplicationUnavailable
    5 => CaptureFrameUnavailable
    6 => CaptureTextureUnavailable
    _ => abort("unexpected native desktop-duplication probe result")
  }
}

///|
/// Returns the result of acquiring and releasing one desktop frame.
pub fn native_desktop_frame_acquire_probe() -> CaptureProbeResult {
  match native_desktop_frame_acquire_probe_result_ffi() {
    0 => CaptureReady
    1 => CaptureDeviceUnavailable
    2 => CaptureAdapterUnavailable
    3 => CaptureOutputUnavailable
    4 => CaptureDuplicationUnavailable
    5 => CaptureFrameUnavailable
    6 => CaptureTextureUnavailable
    _ => abort("unexpected native desktop-frame probe result")
  }
}

///|
/// Confirms that one acquired desktop frame exposes a BGRA D3D11 texture.
pub fn native_desktop_texture_probe() -> CaptureProbeResult {
  match native_desktop_texture_probe_result_ffi() {
    0 => CaptureReady
    1 => CaptureDeviceUnavailable
    2 => CaptureAdapterUnavailable
    3 => CaptureOutputUnavailable
    4 => CaptureDuplicationUnavailable
    5 => CaptureFrameUnavailable
    6 => CaptureTextureUnavailable
    _ => abort("unexpected native desktop-texture probe result")
  }
}

///|
/// Returns the D3D viewer probe result without exposing C enum values.
pub fn native_d3d_viewer_probe() -> ViewerProbeResult {
  match native_d3d_viewer_probe_result_ffi() {
    0 => ViewerReady
    1 => ViewerWindowUnavailable
    2 => ViewerDeviceUnavailable
    3 => ViewerSurfaceUnavailable
    4 => ViewerPresentUnavailable
    _ => abort("unexpected native D3D viewer probe result")
  }
}

///|
/// Reports whether Windows activated a software H.264 encoder MFT.
pub fn native_h264_software_available() -> Bool {
  native_h264_software_available_ffi() == 1
}

///|
/// Runs the software H.264 probe and reports whether it produced a nonempty sample.
pub fn native_h264_software_encode_probe() -> Bool {
  native_h264_software_encode_probe_ffi() == 1
}

///|
/// Reports whether Windows activated a hardware H.264 encoder MFT.
pub fn native_h264_hardware_available() -> Bool {
  native_h264_hardware_available_ffi() == 1
}

///|
/// Applies the native input policy to the MoonBit session phase.
pub fn native_input_allowed(phase : @core.SessionPhase) -> Bool {
  let native_state = match phase {
    @core.Idle | @core.AwaitingApproval => 0
    @core.Connected => 1
    @core.Degraded => 2
    @core.Ended => 3
  }
  native_input_allowed_ffi(native_state) == 1
}

// ══════════════════════════════════════════════════════════════════════
// ABI v2 safe wrappers
// ══════════════════════════════════════════════════════════════════════

///|
pub(all) enum NativeResult {
  Ok
  InvalidHandle
  OutOfMemory
  DeviceUnavailable
  EncoderUnavailable
  DecoderUnavailable
  TransportUnavailable
  ViewerUnavailable
  AlreadyStopped
  Timeout
  InternalError
}

///|
fn int_to_result(code : Int) -> NativeResult {
  match code {
    0 => Ok
    -1 => InvalidHandle
    -2 => OutOfMemory
    -3 => DeviceUnavailable
    -4 => EncoderUnavailable
    -5 => DecoderUnavailable
    -6 => TransportUnavailable
    -7 => ViewerUnavailable
    -8 => AlreadyStopped
    -9 => Timeout
    _ => InternalError
  }
}

///|
pub(all) struct CaptureSession {
  handle : Int
}

///|
pub fn CaptureSession::create(
  output_index : Int,
  target_width : Int,
  target_height : Int,
) -> CaptureSession? {
  let h = capture_create_ffi(output_index, target_width, target_height)
  if h <= 0 {
    None
  } else {
    Some(CaptureSession::{ handle: h, })
  }
}

///|
pub fn CaptureSession::next_frame(
  self : CaptureSession,
  timeout_ms : Int,
) -> NativeResult {
  int_to_result(capture_next_frame_ffi(self.handle, timeout_ms))
}

///|
pub fn CaptureSession::frame_width(self : CaptureSession) -> Int {
  capture_frame_width_ffi(self.handle)
}

///|
pub fn CaptureSession::frame_height(self : CaptureSession) -> Int {
  capture_frame_height_ffi(self.handle)
}

///|
pub fn CaptureSession::copy_nv12(self : CaptureSession, dst : Bytes) -> Int {
  // TODO: buffer copy requires CPtr FFI support
  0
}

///|
pub fn CaptureSession::stop(self : CaptureSession) -> NativeResult {
  int_to_result(capture_stop_ffi(self.handle))
}

///|
pub fn CaptureSession::destroy(self : CaptureSession) -> NativeResult {
  int_to_result(capture_destroy_ffi(self.handle))
}

///|
pub(all) struct EncoderSession {
  handle : Int
}

///|
pub fn EncoderSession::create(
  width : Int,
  height : Int,
  fps : Int,
  bitrate : Int,
  use_hardware : Bool,
) -> EncoderSession? {
  let h = encoder_create_ffi(
    width,
    height,
    fps,
    bitrate,
    if use_hardware {
      1
    } else {
      0
    },
  )
  if h <= 0 {
    None
  } else {
    Some(EncoderSession::{ handle: h, })
  }
}

///|
pub fn EncoderSession::encode_from_capture(
  self : EncoderSession,
  capture : CaptureSession,
) -> Int {
  encoder_encode_from_capture_ffi(self.handle, capture.handle)
}

///|
pub fn EncoderSession::packet_count(self : EncoderSession) -> Int {
  encoder_packet_count_ffi(self.handle)
}

///|
pub fn EncoderSession::packet_size(self : EncoderSession, index : Int) -> Int {
  encoder_packet_size_ffi(self.handle, index)
}

///|
pub fn EncoderSession::packet_is_keyframe(
  self : EncoderSession,
  index : Int,
) -> Bool {
  encoder_packet_is_keyframe_ffi(self.handle, index) == 1
}

///|
pub fn EncoderSession::packet_copy(
  self : EncoderSession,
  index : Int,
  dst : Bytes,
) -> Int {
  // TODO: buffer copy requires CPtr FFI support
  0
}

///|
pub fn EncoderSession::stop(self : EncoderSession) -> NativeResult {
  int_to_result(encoder_stop_ffi(self.handle))
}

///|
pub fn EncoderSession::destroy(self : EncoderSession) -> NativeResult {
  int_to_result(encoder_destroy_ffi(self.handle))
}

///|
pub(all) struct TransportSession {
  handle : Int
}

///|
pub(all) enum TransportRole {
  Host
  Controller
}

///|
pub fn TransportSession::create(role : TransportRole) -> TransportSession? {
  let r = match role {
    Host => 0
    Controller => 1
  }
  let h = transport_create_ffi(r)
  if h <= 0 {
    None
  } else {
    Some(TransportSession::{ handle: h, })
  }
}

///|
pub fn TransportSession::start(self : TransportSession) -> NativeResult {
  int_to_result(transport_start_ffi(self.handle))
}

///|
pub fn TransportSession::is_connected(self : TransportSession) -> Bool {
  transport_is_connected_ffi(self.handle) == 1
}

///|
pub fn TransportSession::send_video(
  self : TransportSession,
  data : Bytes,
) -> NativeResult {
  // TODO: buffer send requires CPtr FFI support
  Ok
}

///|
pub fn TransportSession::send_control(
  self : TransportSession,
  message : String,
) -> NativeResult {
  // TODO: string send requires CPtr FFI support
  Ok
}

///|
pub fn TransportSession::stop(self : TransportSession) -> NativeResult {
  int_to_result(transport_stop_ffi(self.handle))
}

///|
pub fn TransportSession::destroy(self : TransportSession) -> NativeResult {
  int_to_result(transport_destroy_ffi(self.handle))
}

///|
pub(all) struct DecoderSession {
  handle : Int
}

///|
pub fn DecoderSession::create(width : Int, height : Int) -> DecoderSession? {
  let h = decoder_create_ffi(width, height)
  if h <= 0 {
    None
  } else {
    Some(DecoderSession::{ handle: h, })
  }
}

///|
pub fn DecoderSession::decode(self : DecoderSession, data : Bytes) -> Int {
  // TODO: buffer decode requires CPtr FFI support
  0
}

///|
pub fn DecoderSession::frame_width(self : DecoderSession) -> Int {
  decoder_frame_width_ffi(self.handle)
}

///|
pub fn DecoderSession::frame_height(self : DecoderSession) -> Int {
  decoder_frame_height_ffi(self.handle)
}

///|
pub fn DecoderSession::copy_bgra(self : DecoderSession, dst : Bytes) -> Int {
  // TODO: buffer copy requires CPtr FFI support
  0
}

///|
pub fn DecoderSession::stop(self : DecoderSession) -> NativeResult {
  int_to_result(decoder_stop_ffi(self.handle))
}

///|
pub fn DecoderSession::destroy(self : DecoderSession) -> NativeResult {
  int_to_result(decoder_destroy_ffi(self.handle))
}

///|
pub(all) struct ViewerSession {
  handle : Int
}

///|
pub fn ViewerSession::create(width : Int, height : Int) -> ViewerSession? {
  let h = viewer_create_ffi(width, height)
  if h <= 0 {
    None
  } else {
    Some(ViewerSession::{ handle: h, })
  }
}

///|
pub fn ViewerSession::present_from_decoder(
  self : ViewerSession,
  decoder : DecoderSession,
) -> NativeResult {
  int_to_result(viewer_present_from_decoder_ffi(self.handle, decoder.handle))
}

///|
pub fn ViewerSession::pump_messages(self : ViewerSession) -> Bool {
  viewer_pump_messages_ffi(self.handle) == 0
}

///|
pub fn ViewerSession::stop(self : ViewerSession) -> NativeResult {
  int_to_result(viewer_stop_ffi(self.handle))
}

///|
pub fn ViewerSession::destroy(self : ViewerSession) -> NativeResult {
  int_to_result(viewer_destroy_ffi(self.handle))
}

// ══════════════════════════════════════════════════════════════════════
// Signaling FFI (TCP-based SDP exchange for LAN)
// ══════════════════════════════════════════════════════════════════════

// Note: signaling uses C string pointers directly. MoonBit passes
// String/Bytes as raw pointers via the native FFI convention.

///|
extern "C" fn signal_listen_ffi(port : Int) -> Int = "moonlink_signal_listen_ffi"

///|
extern "C" fn signal_accept_ffi(listener_handle : Int) -> Int = "moonlink_signal_accept_ffi"

///|
extern "C" fn signal_connect_ffi(address : Int, port : Int) -> Int = "moonlink_signal_connect_ffi"

///|
extern "C" fn signal_send_ffi(handle : Int, data : Int, size : Int) -> Int = "moonlink_signal_send_ffi"

///|
extern "C" fn signal_recv_ffi(
  handle : Int,
  buffer : Int,
  buffer_size : Int,
) -> Int = "moonlink_signal_recv_ffi"

///|
extern "C" fn signal_close_ffi(handle : Int) -> Int = "moonlink_signal_close_ffi"

///|
extern "C" fn signal_stop_listen_ffi(handle : Int) -> Int = "moonlink_signal_stop_listen_ffi"

///|
pub(all) struct SignalSession {
  handle : Int
}

///|
pub(all) struct SignalListener {
  handle : Int
}

///|
pub fn SignalListener::listen(port : Int) -> SignalListener? {
  let h = signal_listen_ffi(port)
  if h <= 0 {
    None
  } else {
    Some(SignalListener::{ handle: h, })
  }
}

///|
pub fn SignalListener::accept(self : SignalListener) -> SignalSession? {
  let h = signal_accept_ffi(self.handle)
  if h <= 0 {
    None
  } else {
    Some(SignalSession::{ handle: h, })
  }
}

///|
pub fn SignalListener::stop(self : SignalListener) -> NativeResult {
  int_to_result(signal_stop_listen_ffi(self.handle))
}

///|
pub fn SignalSession::connect(address_ptr : Int, port : Int) -> SignalSession? {
  let h = signal_connect_ffi(address_ptr, port)
  if h <= 0 {
    None
  } else {
    Some(SignalSession::{ handle: h, })
  }
}

///|
pub fn SignalSession::send_raw(
  self : SignalSession,
  data_ptr : Int,
  size : Int,
) -> NativeResult {
  int_to_result(signal_send_ffi(self.handle, data_ptr, size))
}

///|
pub fn SignalSession::recv_raw(
  self : SignalSession,
  buffer_ptr : Int,
  buffer_size : Int,
) -> Int {
  signal_recv_ffi(self.handle, buffer_ptr, buffer_size)
}

///|
pub fn SignalSession::close(self : SignalSession) -> NativeResult {
  int_to_result(signal_close_ffi(self.handle))
}

// ══════════════════════════════════════════════════════════════════════
// Buffer pool FFI
// ══════════════════════════════════════════════════════════════════════

///|
extern "C" fn buffer_alloc_ffi(size : Int) -> Int = "moonlink_buffer_alloc_ffi"

///|
extern "C" fn buffer_size_ffi(slot : Int) -> Int = "moonlink_buffer_size_ffi"

///|
extern "C" fn buffer_free_ffi(slot : Int) -> Int = "moonlink_buffer_free_ffi"

///|
pub(all) struct BufferSlot {
  slot : Int
}

///|
pub fn BufferSlot::alloc(size : Int) -> BufferSlot? {
  let s = buffer_alloc_ffi(size)
  if s <= 0 {
    None
  } else {
    Some(BufferSlot::{ slot: s, })
  }
}

///|
pub fn BufferSlot::size(self : BufferSlot) -> Int {
  buffer_size_ffi(self.slot)
}

///|
pub fn BufferSlot::free(self : BufferSlot) -> NativeResult {
  int_to_result(buffer_free_ffi(self.slot))
}

// ══════════════════════════════════════════════════════════════════════
// Transport ↔ Signaling bridge FFI
// ══════════════════════════════════════════════════════════════════════

///|
extern "C" fn transport_set_remote_from_signal_ffi(
  transport_handle : Int,
  signal_handle : Int,
) -> Int = "moonlink_transport_set_remote_from_signal_ffi"

///|
extern "C" fn transport_send_local_desc_via_signal_ffi(
  transport_handle : Int,
  signal_handle : Int,
) -> Int = "moonlink_transport_send_local_desc_via_signal_ffi"

///|
extern "C" fn transport_send_video_from_buffer_ffi(
  transport_handle : Int,
  buf_slot : Int,
) -> Int = "moonlink_transport_send_video_from_buffer_ffi"

///|
extern "C" fn transport_pending_video_count_ffi(transport_handle : Int) -> Int = "moonlink_transport_pending_video_count_ffi"

///|
extern "C" fn transport_pop_video_to_buffer_ffi(
  transport_handle : Int,
  buf_slot : Int,
) -> Int = "moonlink_transport_pop_video_to_buffer_ffi"

///|
pub fn TransportSession::set_remote_from_signal(
  self : TransportSession,
  signal : SignalSession,
) -> NativeResult {
  int_to_result(
    transport_set_remote_from_signal_ffi(self.handle, signal.handle),
  )
}

///|
pub fn TransportSession::send_local_desc_via_signal(
  self : TransportSession,
  signal : SignalSession,
) -> NativeResult {
  int_to_result(
    transport_send_local_desc_via_signal_ffi(self.handle, signal.handle),
  )
}

///|
pub fn TransportSession::send_video_from_buffer(
  self : TransportSession,
  buf : BufferSlot,
) -> NativeResult {
  int_to_result(transport_send_video_from_buffer_ffi(self.handle, buf.slot))
}

///|
pub fn TransportSession::pending_video_count(self : TransportSession) -> Int {
  transport_pending_video_count_ffi(self.handle)
}

///|
pub fn TransportSession::pop_video_to_buffer(
  self : TransportSession,
  buf : BufferSlot,
) -> Int {
  transport_pop_video_to_buffer_ffi(self.handle, buf.slot)
}

// ══════════════════════════════════════════════════════════════════════
// Encoder/Decoder ↔ Buffer bridge FFI
// ══════════════════════════════════════════════════════════════════════

///|
extern "C" fn encoder_encode_to_buffer_ffi(
  encoder_handle : Int,
  capture_handle : Int,
  buf_slot : Int,
) -> Int = "moonlink_encoder_encode_to_buffer_ffi"

///|
extern "C" fn decoder_decode_from_buffer_ffi(
  decoder_handle : Int,
  buf_slot : Int,
) -> Int = "moonlink_decoder_decode_from_buffer_ffi"

///|
extern "C" fn viewer_present_decoded_ffi(
  viewer_handle : Int,
  decoder_handle : Int,
) -> Int = "moonlink_viewer_present_decoded_ffi"

///|
extern "C" fn signal_connect_ip_ffi(ip : Int, port : Int) -> Int = "moonlink_signal_connect_ip_ffi"

///|
pub fn EncoderSession::encode_to_buffer(
  self : EncoderSession,
  capture : CaptureSession,
  buf : BufferSlot,
) -> Int {
  encoder_encode_to_buffer_ffi(self.handle, capture.handle, buf.slot)
}

///|
pub fn DecoderSession::decode_from_buffer(
  self : DecoderSession,
  buf : BufferSlot,
) -> Int {
  decoder_decode_from_buffer_ffi(self.handle, buf.slot)
}

///|
pub fn ViewerSession::present_decoded(
  self : ViewerSession,
  decoder : DecoderSession,
) -> NativeResult {
  int_to_result(viewer_present_decoded_ffi(self.handle, decoder.handle))
}

///|
pub fn SignalSession::connect_ip(ip : Int, port : Int) -> SignalSession? {
  let h = signal_connect_ip_ffi(ip, port)
  if h <= 0 {
    None
  } else {
    Some(SignalSession::{ handle: h, })
  }
}
