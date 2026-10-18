#ifndef MOONLINK_NATIVE_BRIDGE_H
#define MOONLINK_NATIVE_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ── ABI version ─────────────────────────────────────────────────────── */

/* Bump to 2 when session-handle APIs are added. */
#define MOONLINK_NATIVE_ABI_VERSION 2

/* ── Session state (unchanged) ───────────────────────────────────────── */

typedef enum moonlink_native_session_state {
  MOONLINK_NATIVE_AWAITING_APPROVAL = 0,
  MOONLINK_NATIVE_CONNECTED = 1,
  MOONLINK_NATIVE_DEGRADED = 2,
  MOONLINK_NATIVE_ENDED = 3,
} moonlink_native_session_state;

/* ── Probe results (unchanged) ───────────────────────────────────────── */

typedef enum moonlink_native_capture_probe_result {
  MOONLINK_NATIVE_CAPTURE_READY = 0,
  MOONLINK_NATIVE_CAPTURE_DEVICE_UNAVAILABLE = 1,
  MOONLINK_NATIVE_CAPTURE_ADAPTER_UNAVAILABLE = 2,
  MOONLINK_NATIVE_CAPTURE_OUTPUT_UNAVAILABLE = 3,
  MOONLINK_NATIVE_CAPTURE_DUPLICATION_UNAVAILABLE = 4,
  MOONLINK_NATIVE_CAPTURE_FRAME_UNAVAILABLE = 5,
  MOONLINK_NATIVE_CAPTURE_TEXTURE_UNAVAILABLE = 6,
} moonlink_native_capture_probe_result;

typedef enum moonlink_native_viewer_probe_result {
  MOONLINK_NATIVE_VIEWER_READY = 0,
  MOONLINK_NATIVE_VIEWER_WINDOW_UNAVAILABLE = 1,
  MOONLINK_NATIVE_VIEWER_DEVICE_UNAVAILABLE = 2,
  MOONLINK_NATIVE_VIEWER_SURFACE_UNAVAILABLE = 3,
  MOONLINK_NATIVE_VIEWER_PRESENT_UNAVAILABLE = 4,
} moonlink_native_viewer_probe_result;

/* ── Session handle result codes ─────────────────────────────────────── */

typedef enum moonlink_native_result {
  MOONLINK_RESULT_OK = 0,
  MOONLINK_RESULT_INVALID_HANDLE = -1,
  MOONLINK_RESULT_OUT_OF_MEMORY = -2,
  MOONLINK_RESULT_DEVICE_UNAVAILABLE = -3,
  MOONLINK_RESULT_ENCODER_UNAVAILABLE = -4,
  MOONLINK_RESULT_DECODER_UNAVAILABLE = -5,
  MOONLINK_RESULT_TRANSPORT_UNAVAILABLE = -6,
  MOONLINK_RESULT_VIEWER_UNAVAILABLE = -7,
  MOONLINK_RESULT_ALREADY_STOPPED = -8,
  MOONLINK_RESULT_TIMEOUT = -9,
  MOONLINK_RESULT_INTERNAL_ERROR = -10,
} moonlink_native_result;

/* ── Opaque session handles ──────────────────────────────────────────── */

typedef struct moonlink_capture_session *moonlink_capture_handle;
typedef struct moonlink_encoder_session *moonlink_encoder_handle;
typedef struct moonlink_transport_session *moonlink_transport_handle;
typedef struct moonlink_decoder_session *moonlink_decoder_handle;
typedef struct moonlink_viewer_session *moonlink_viewer_handle;

/* ── Capture session config ──────────────────────────────────────────── */

typedef struct moonlink_capture_config {
  int output_index;      /* DXGI output index (0-based) */
  int target_width;      /* desired width after scaling, 0 = native */
  int target_height;     /* desired height after scaling, 0 = native */
} moonlink_capture_config;

/* Frame delivered by capture_next_frame. Caller must NOT free the buffer;
   it is owned by the capture session and valid until the next call or stop. */
typedef struct moonlink_captured_frame {
  const uint8_t *nv12_data;   /* NV12 pixel buffer (Y plane + interleaved UV) */
  int width;
  int height;
  int stride_y;               /* Y-plane row pitch in bytes */
  int stride_uv;              /* UV-plane row pitch in bytes */
  int64_t timestamp_us;       /* microsecond timestamp from QPC */
} moonlink_captured_frame;

/* ── Encoder session config ──────────────────────────────────────────── */

typedef struct moonlink_encoder_config {
  int width;
  int height;
  int fps;
  int bitrate;                /* bits per second */
  int use_hardware;           /* 1 = prefer HW, 0 = software only */
} moonlink_encoder_config;

/* Encoded H.264 bitstream chunk. Buffer owned by encoder session. */
typedef struct moonlink_encoded_packet {
  const uint8_t *data;
  int size;
  int is_keyframe;            /* 1 if IDR / key frame */
  int64_t timestamp_us;
} moonlink_encoded_packet;

/* ── Transport session config ────────────────────────────────────────── */

typedef enum moonlink_transport_role {
  MOONLINK_TRANSPORT_HOST = 0,    /* controlled side (sends video) */
  MOONLINK_TRANSPORT_CONTROLLER = 1, /* controller side (receives video) */
} moonlink_transport_role;

typedef struct moonlink_transport_config {
  moonlink_transport_role role;
  /* For direct LAN: bind address hint (NULL = any). */
  const char *bind_address;
} moonlink_transport_config;

/* Callback invoked on the transport's internal thread when an H.264 packet
   arrives (controller side only). The buffer is valid only for the duration
   of the callback. */
typedef void (*moonlink_transport_video_callback)(
  const uint8_t *data,
  int size,
  void *user_data
);

/* Callback invoked when the control data channel receives a message. */
typedef void (*moonlink_transport_control_callback)(
  const char *message,
  int size,
  void *user_data
);

/* Callback invoked when the transport state changes. */
typedef void (*moonlink_transport_state_callback)(
  int connected,             /* 1 = connected, 0 = disconnected */
  void *user_data
);

/* ── Decoder session config ──────────────────────────────────────────── */

typedef struct moonlink_decoder_config {
  int width;
  int height;
} moonlink_decoder_config;

/* Decoded BGRA frame. Buffer owned by decoder session. */
typedef struct moonlink_decoded_frame {
  const uint8_t *bgra_data;
  int width;
  int height;
  int stride;                 /* row pitch in bytes */
} moonlink_decoded_frame;

/* ── Viewer session config ───────────────────────────────────────────── */

typedef struct moonlink_viewer_config {
  int width;
  int height;
  const wchar_t *title;       /* window title (may be NULL) */
} moonlink_viewer_config;

/* ══════════════════════════════════════════════════════════════════════
   Existing probe / utility functions (ABI v1, unchanged)
   ══════════════════════════════════════════════════════════════════════ */

int moonlink_native_bridge_abi_version(void);
int moonlink_native_display_count(void);
moonlink_native_capture_probe_result moonlink_native_desktop_duplication_probe(void);
moonlink_native_capture_probe_result moonlink_native_desktop_frame_acquire_probe(void);
moonlink_native_capture_probe_result moonlink_native_desktop_texture_probe(void);
moonlink_native_viewer_probe_result moonlink_native_d3d_viewer_probe(void);
int moonlink_native_h264_software_available(void);
int moonlink_native_h264_software_encode_probe(void);
int moonlink_native_h264_hardware_available(void);
int moonlink_native_input_allowed(moonlink_native_session_state state);

/* ══════════════════════════════════════════════════════════════════════
   Session-handle APIs (ABI v2)
   ══════════════════════════════════════════════════════════════════════ */

/* ── Capture ─────────────────────────────────────────────────────────── */

moonlink_native_result moonlink_capture_create(
  const moonlink_capture_config *config,
  moonlink_capture_handle *out_handle
);

/* Block up to timeout_ms for the next desktop frame. Returns TIMEOUT if no
   frame arrived in time. The returned frame pointer is valid until the next
   call to capture_next_frame or capture_stop. */
moonlink_native_result moonlink_capture_next_frame(
  moonlink_capture_handle handle,
  int timeout_ms,
  const moonlink_captured_frame **out_frame
);

moonlink_native_result moonlink_capture_stop(moonlink_capture_handle handle);
moonlink_native_result moonlink_capture_destroy(moonlink_capture_handle handle);

/* ── Encoder ─────────────────────────────────────────────────────────── */

moonlink_native_result moonlink_encoder_create(
  const moonlink_encoder_config *config,
  moonlink_encoder_handle *out_handle
);

/* Feed one NV12 frame into the encoder. May produce zero or more packets. */
moonlink_native_result moonlink_encoder_encode(
  moonlink_encoder_handle handle,
  const moonlink_captured_frame *frame,
  const moonlink_encoded_packet **out_packets,
  int *out_packet_count
);

moonlink_native_result moonlink_encoder_stop(moonlink_encoder_handle handle);
moonlink_native_result moonlink_encoder_destroy(moonlink_encoder_handle handle);

/* ── Transport ───────────────────────────────────────────────────────── */

moonlink_native_result moonlink_transport_create(
  const moonlink_transport_config *config,
  moonlink_transport_handle *out_handle
);

/* Set callbacks before calling transport_start. */
moonlink_native_result moonlink_transport_set_video_callback(
  moonlink_transport_handle handle,
  moonlink_transport_video_callback cb,
  void *user_data
);

moonlink_native_result moonlink_transport_set_control_callback(
  moonlink_transport_handle handle,
  moonlink_transport_control_callback cb,
  void *user_data
);

moonlink_native_result moonlink_transport_set_state_callback(
  moonlink_transport_handle handle,
  moonlink_transport_state_callback cb,
  void *user_data
);

/* Start ICE gathering and auto-negotiation. */
moonlink_native_result moonlink_transport_start(
  moonlink_transport_handle handle
);

/* Send SDP offer/answer to the remote peer via out-of-band signaling.
   The caller is responsible for exchanging these strings over LAN. */
moonlink_native_result moonlink_transport_get_local_description(
  moonlink_transport_handle handle,
  char *buffer,
  int buffer_size,
  int *out_length
);

moonlink_native_result moonlink_transport_set_remote_description(
  moonlink_transport_handle handle,
  const char *sdp,
  const char *type
);

moonlink_native_result moonlink_transport_add_remote_candidate(
  moonlink_transport_handle handle,
  const char *candidate,
  const char *mid
);

/* Host side: send one encoded H.264 packet. */
moonlink_native_result moonlink_transport_send_video(
  moonlink_transport_handle handle,
  const uint8_t *data,
  int size
);

/* Send a control-channel message (both sides). */
moonlink_native_result moonlink_transport_send_control(
  moonlink_transport_handle handle,
  const char *message,
  int size
);

moonlink_native_result moonlink_transport_stop(moonlink_transport_handle handle);
moonlink_native_result moonlink_transport_destroy(moonlink_transport_handle handle);

/* ── Decoder ─────────────────────────────────────────────────────────── */

moonlink_native_result moonlink_decoder_create(
  const moonlink_decoder_config *config,
  moonlink_decoder_handle *out_handle
);

/* Feed one H.264 packet. May produce zero or one decoded frame. */
moonlink_native_result moonlink_decoder_decode(
  moonlink_decoder_handle handle,
  const uint8_t *data,
  int size,
  const moonlink_decoded_frame **out_frame
);

moonlink_native_result moonlink_decoder_stop(moonlink_decoder_handle handle);
moonlink_native_result moonlink_decoder_destroy(moonlink_decoder_handle handle);

/* ── Viewer ──────────────────────────────────────────────────────────── */

moonlink_native_result moonlink_viewer_create(
  const moonlink_viewer_config *config,
  moonlink_viewer_handle *out_handle
);

/* Present one decoded BGRA frame to the D3D11 window. */
moonlink_native_result moonlink_viewer_present(
  moonlink_viewer_handle handle,
  const moonlink_decoded_frame *frame
);

/* Process window messages. Returns 0 while the window is alive, 1 if closed. */
int moonlink_viewer_pump_messages(moonlink_viewer_handle handle);

moonlink_native_result moonlink_viewer_stop(moonlink_viewer_handle handle);
moonlink_native_result moonlink_viewer_destroy(moonlink_viewer_handle handle);

/* ══════════════════════════════════════════════════════════════════════
   Integer-handle API for MoonBit FFI (ABI v2)

   MoonBit's native FFI only supports value types (Int, Bool). These
   wrappers store opaque session pointers in a C-side handle table and
   expose integer slot indices to MoonBit. Slot 0 is reserved (invalid).
   ══════════════════════════════════════════════════════════════════════ */

#define MOONLINK_HANDLE_INVALID 0

/* ── Capture (integer handles) ───────────────────────────────────────── */

int moonlink_capture_create_ffi(int output_index, int target_width, int target_height);
int moonlink_capture_next_frame_ffi(int handle, int timeout_ms);
/* After a successful next_frame, query the frame fields: */
int moonlink_capture_frame_width_ffi(int handle);
int moonlink_capture_frame_height_ffi(int handle);
int moonlink_capture_frame_stride_y_ffi(int handle);
int moonlink_capture_frame_stride_uv_ffi(int handle);
long long moonlink_capture_frame_timestamp_us_ffi(int handle);
/* Copy NV12 data into a caller-provided buffer. Returns bytes copied. */
int moonlink_capture_frame_copy_nv12_ffi(int handle, unsigned char *dst, int dst_size);
int moonlink_capture_stop_ffi(int handle);
int moonlink_capture_destroy_ffi(int handle);

/* ── Encoder (integer handles) ───────────────────────────────────────── */

int moonlink_encoder_create_ffi(int width, int height, int fps, int bitrate, int use_hardware);
/* Encode the current captured frame from capture_handle. Returns packet count. */
int moonlink_encoder_encode_from_capture_ffi(int encoder_handle, int capture_handle);
/* Query encoded packets after encode. Index 0..count-1. */
int moonlink_encoder_packet_count_ffi(int handle);
int moonlink_encoder_packet_size_ffi(int handle, int index);
int moonlink_encoder_packet_is_keyframe_ffi(int handle, int index);
long long moonlink_encoder_packet_timestamp_us_ffi(int handle, int index);
int moonlink_encoder_packet_copy_ffi(int handle, int index, unsigned char *dst, int dst_size);
int moonlink_encoder_stop_ffi(int handle);
int moonlink_encoder_destroy_ffi(int handle);

/* ── Transport (integer handles) ─────────────────────────────────────── */

int moonlink_transport_create_ffi(int role); /* 0=host, 1=controller */
int moonlink_transport_start_ffi(int handle);
int moonlink_transport_get_local_description_ffi(int handle, char *buffer, int buffer_size);
int moonlink_transport_set_remote_description_ffi(int handle, const char *sdp, const char *type);
int moonlink_transport_add_remote_candidate_ffi(int handle, const char *candidate, const char *mid);
int moonlink_transport_send_video_ffi(int handle, const unsigned char *data, int size);
int moonlink_transport_send_control_ffi(int handle, const char *message, int size);
int moonlink_transport_is_connected_ffi(int handle);
int moonlink_transport_stop_ffi(int handle);
int moonlink_transport_destroy_ffi(int handle);

/* ── Decoder (integer handles) ───────────────────────────────────────── */

int moonlink_decoder_create_ffi(int width, int height);
/* Decode H.264 data. Returns 1 if a frame is available, 0 if needs more input, negative on error. */
int moonlink_decoder_decode_ffi(int handle, const unsigned char *data, int size);
int moonlink_decoder_frame_width_ffi(int handle);
int moonlink_decoder_frame_height_ffi(int handle);
int moonlink_decoder_frame_stride_ffi(int handle);
int moonlink_decoder_frame_copy_bgra_ffi(int handle, unsigned char *dst, int dst_size);
int moonlink_decoder_stop_ffi(int handle);
int moonlink_decoder_destroy_ffi(int handle);

/* ── Viewer (integer handles) ────────────────────────────────────────── */

int moonlink_viewer_create_ffi(int width, int height);
/* Present the current decoded frame from decoder_handle. */
int moonlink_viewer_present_from_decoder_ffi(int viewer_handle, int decoder_handle);
int moonlink_viewer_pump_messages_ffi(int handle);
int moonlink_viewer_stop_ffi(int handle);
int moonlink_viewer_destroy_ffi(int handle);

/* ── Input executor (integer handles) ────────────────────────────────── */

/* Input event types matching core/ControllerEventKind */
#define MOONLINK_INPUT_MOUSE_MOVE    0
#define MOONLINK_INPUT_MOUSE_BUTTON  1
#define MOONLINK_INPUT_KEYBOARD      2

/* Mouse button actions */
#define MOONLINK_MOUSE_DOWN   0
#define MOONLINK_MOUSE_UP     1

/* Mouse buttons */
#define MOONLINK_MOUSE_LEFT    0
#define MOONLINK_MOUSE_RIGHT   1
#define MOONLINK_MOUSE_MIDDLE  2

int moonlink_input_create_ffi(void);
/* Execute a mouse move at absolute screen coordinates */
int moonlink_input_mouse_move_ffi(int handle, int x, int y);
/* Execute a mouse button press/release */
int moonlink_input_mouse_button_ffi(int handle, int button, int action);
/* Execute a keyboard key press/release. vk_code is Windows virtual key code, down=1 press, down=0 release */
int moonlink_input_key_ffi(int handle, int vk_code, int down);
int moonlink_input_stop_ffi(int handle);
int moonlink_input_destroy_ffi(int handle);

/* ── LAN Signaling (integer handles) ─────────────────────────────────── */

/* Simple TCP-based SDP exchange for LAN direct connection.
   Protocol: 4-byte big-endian length prefix + UTF-8 payload. */

#define MOONLINK_SIGNAL_DEFAULT_PORT 9876
#define MOONLINK_SIGNAL_MAX_MESSAGE  65536

/* Listener: binds and listens on a TCP port. Returns listener handle or negative error. */
int moonlink_signal_listen_ffi(int port);
/* Accept one incoming connection (blocking). Returns session handle or negative error. */
int moonlink_signal_accept_ffi(int listener_handle);
/* Connect to remote host:port. Returns session handle or negative error. */
int moonlink_signal_connect_ffi(const char *address, int port);
/* Send a length-prefixed message. Returns MOONLINK_RESULT_OK or error. */
int moonlink_signal_send_ffi(int handle, const char *data, int size);
/* Receive a length-prefixed message into buffer. Returns bytes received or negative error. */
int moonlink_signal_recv_ffi(int handle, char *buffer, int buffer_size);
/* Close a signaling session. */
int moonlink_signal_close_ffi(int handle);
/* Stop listening and close the listener socket. */
int moonlink_signal_stop_listen_ffi(int listener_handle);

/* ── Shared buffer pool for FFI data exchange ────────────────────────── */

#define MOONLINK_BUFFER_POOL_SIZE 64
#define MOONLINK_BUFFER_MAX_SIZE  262144  /* 256 KB */

int moonlink_buffer_alloc_ffi(int size);
int moonlink_buffer_write_ffi(int slot, const char *src, int size);
int moonlink_buffer_read_ffi(int slot, char *dst, int max_size);
int moonlink_buffer_size_ffi(int slot);
int moonlink_buffer_free_ffi(int slot);

/* ── Transport ↔ Signaling bridge (pure Int) ─────────────────────────── */

#ifdef MOONLINK_TRANSPORT_ENABLED
/* Read SDP from signaling session, set as remote description on transport. */
int moonlink_transport_set_remote_from_signal_ffi(int transport_handle, int signal_handle);
/* Get local description from transport, send via signaling session. */
int moonlink_transport_send_local_desc_via_signal_ffi(int transport_handle, int signal_handle);
/* Send encoded H.264 data from a buffer slot via transport video track. */
int moonlink_transport_send_video_from_buffer_ffi(int transport_handle, int buffer_slot);
/* Number of pending received video packets. */
int moonlink_transport_pending_video_count_ffi(int transport_handle);
/* Pop oldest received video packet into a buffer slot. Returns size or 0. */
int moonlink_transport_pop_video_to_buffer_ffi(int transport_handle, int buffer_slot);
#else
int moonlink_transport_set_remote_from_signal_ffi(int t, int s);
int moonlink_transport_send_local_desc_via_signal_ffi(int t, int s);
int moonlink_transport_send_video_from_buffer_ffi(int t, int b);
int moonlink_transport_pending_video_count_ffi(int t);
int moonlink_transport_pop_video_to_buffer_ffi(int t, int b);
#endif

/* ── Encoder/Decoder ↔ Buffer bridge (pure Int) ─────────────────────── */

/* Encode the current captured frame and write the first output packet
   directly into a buffer slot. Returns packet size or 0 if no output. */
int moonlink_encoder_encode_to_buffer_ffi(int encoder_handle, int capture_handle, int buf_slot);

/* Feed H.264 data from a buffer slot into the decoder.
   Returns 1 if a decoded frame is available, 0 if needs more input, negative on error. */
int moonlink_decoder_decode_from_buffer_ffi(int decoder_handle, int buf_slot);

/* Present the current decoded frame from decoder to viewer. Pure Int wrapper. */
int moonlink_viewer_present_decoded_ffi(int viewer_handle, int decoder_handle);

/* ── Signaling connect by IP integer (pure Int) ──────────────────────── */

/* Connect to remote host using IP as a 32-bit integer (network byte order).
   E.g. 127.0.0.1 = 0x7F000001. Returns session handle or negative error. */
int moonlink_signal_connect_ip_ffi(unsigned int ip, int port);

#ifdef __cplusplus
}
#endif

#endif
