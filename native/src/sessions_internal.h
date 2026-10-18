/*
 * sessions_internal.h — Internal struct definitions shared between
 * session implementation files and handle_table.c.
 *
 * NOT part of the public ABI. Only included by native/src/*.c files.
 */

#ifndef MOONLINK_SESSIONS_INTERNAL_H
#define MOONLINK_SESSIONS_INTERNAL_H

#include "moonlink_native_bridge.h"
#include "h264_depacketizer.h"

#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <mfapi.h>
#include <mftransform.h>

/* ── Capture session ─────────────────────────────────────────────────── */

typedef struct moonlink_capture_session moonlink_capture_session;
struct moonlink_capture_session {
  ID3D11Device *device;
  ID3D11DeviceContext *device_context;
  IDXGIOutputDuplication *duplication;
  ID3D11Texture2D *staging_texture;
  uint8_t *nv12_buffer;
  int width;
  int height;
  int stride_y;
  int stride_uv;
  LARGE_INTEGER qpc_frequency;
  BOOL stopped;
  moonlink_captured_frame cached_frame;
};

/* ── Encoder session ─────────────────────────────────────────────────── */

#define ENCODER_MAX_CACHED_PACKETS 8

typedef struct moonlink_encoder_session moonlink_encoder_session;

typedef struct encoder_packet_slot {
  moonlink_encoded_packet packet;
  uint8_t *buffer;
  DWORD capacity;
} encoder_packet_slot;

struct moonlink_encoder_session {
  IMFTransform *encoder;
  IMFActivate *activation;
  MFT_OUTPUT_STREAM_INFO stream_info;
  int width;
  int height;
  int fps;
  int bitrate;
  BOOL use_hardware;
  BOOL stopped;
  LONGLONG frame_duration;
  LONGLONG next_timestamp;
  encoder_packet_slot slots[ENCODER_MAX_CACHED_PACKETS];
  int cached_packet_count;
};

/* ── Transport session ───────────────────────────────────────────────── */

typedef struct moonlink_transport_session moonlink_transport_session;
#define VIDEO_RING_CAPACITY 256
#define VIDEO_RING_MAX_PACKET 524288  /* 512 KB per access unit */

struct moonlink_transport_session {
  int peer_connection;
  int video_track;
  int control_channel;
  moonlink_transport_role role;
  volatile LONG connected;
  volatile LONG stopped;
  moonlink_transport_video_callback video_cb;
  void *video_cb_data;
  moonlink_transport_control_callback control_cb;
  void *control_cb_data;
  moonlink_transport_state_callback state_cb;
  void *state_cb_data;
  CRITICAL_SECTION callback_lock;
  /* Video receive ring buffer (written by rtc callback, read by MoonBit) */
  uint8_t *video_ring[VIDEO_RING_CAPACITY];
  int video_ring_size[VIDEO_RING_CAPACITY];
  volatile LONG video_ring_head;
  volatile LONG video_ring_tail;
  /* RTP -> Annex-B reassembly for the received H.264 track. */
  moonlink_h264_depack depack;
};

/* ── Decoder session ─────────────────────────────────────────────────── */

typedef struct moonlink_decoder_session moonlink_decoder_session;
struct moonlink_decoder_session {
  IMFTransform *decoder;
  IMFActivate *activation;
  MFT_OUTPUT_STREAM_INFO stream_info;
  int width;
  int height;
  BOOL stopped;
  uint8_t *bgra_buffer;
  int stride;
  moonlink_decoded_frame cached_frame;
  BOOL output_is_nv12;
  uint8_t *nv12_buffer;
  LONGLONG next_timestamp;
  LONGLONG frame_duration;
  BOOL sequence_header_applied;
};

/* ── Viewer session ──────────────────────────────────────────────────── */

typedef struct moonlink_viewer_session moonlink_viewer_session;
struct moonlink_viewer_session {
  HWND window;
  ID3D11Device *device;
  ID3D11DeviceContext *context;
  IDXGISwapChain *swap_chain;
  ID3D11Texture2D *staging_texture;
  ID3D11ShaderResourceView *srv;
  int width;
  int height;
  BOOL stopped;
  WNDCLASSW window_class;
  wchar_t class_name[64];
};

/* ── Input session ───────────────────────────────────────────────────── */

typedef struct moonlink_input_session moonlink_input_session;
struct moonlink_input_session {
  BOOL stopped;
  int screen_width;
  int screen_height;
};

/* ── Signaling raw access (for transport bridge) ─────────────────────── */

/* These are implemented in signaling.c and used by handle_table.c
   for the transport↔signaling bridge functions. */
int moonlink_signal_raw_send(int handle, const char *data, int size);
int moonlink_signal_raw_recv(int handle, char *buf, int buf_size);

#endif /* MOONLINK_SESSIONS_INTERNAL_H */
