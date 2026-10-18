/*
 * handle_table.c — Integer-handle wrappers for MoonBit FFI.
 *
 * Stores opaque session pointers in fixed-size tables and exposes
 * integer slot indices. Slot 0 is reserved (MOONLINK_HANDLE_INVALID).
 */

#include "sessions_internal.h"
#include <stdio.h>

#include <stdlib.h>
#include <string.h>

/* ── Generic handle table ────────────────────────────────────────────── */

#define HANDLE_TABLE_CAPACITY 64

typedef struct handle_table {
  void *slots[HANDLE_TABLE_CAPACITY];
  CRITICAL_SECTION lock;
  BOOL initialized;
} handle_table;

static void handle_table_init(handle_table *table) {
  if (!table->initialized) {
    memset(table->slots, 0, sizeof(table->slots));
    InitializeCriticalSection(&table->lock);
    table->initialized = TRUE;
  }
}

static int handle_table_alloc(handle_table *table, void *ptr) {
  handle_table_init(table);
  EnterCriticalSection(&table->lock);
  for (int i = 1; i < HANDLE_TABLE_CAPACITY; i += 1) {
    if (table->slots[i] == NULL) {
      table->slots[i] = ptr;
      LeaveCriticalSection(&table->lock);
      return i;
    }
  }
  LeaveCriticalSection(&table->lock);
  return MOONLINK_HANDLE_INVALID;
}

static void *handle_table_get(handle_table *table, int index) {
  if (index <= 0 || index >= HANDLE_TABLE_CAPACITY) {
    return NULL;
  }
  handle_table_init(table);
  EnterCriticalSection(&table->lock);
  void *ptr = table->slots[index];
  LeaveCriticalSection(&table->lock);
  return ptr;
}

static void *handle_table_remove(handle_table *table, int index) {
  if (index <= 0 || index >= HANDLE_TABLE_CAPACITY) {
    return NULL;
  }
  handle_table_init(table);
  EnterCriticalSection(&table->lock);
  void *ptr = table->slots[index];
  table->slots[index] = NULL;
  LeaveCriticalSection(&table->lock);
  return ptr;
}

/* ── Per-type tables ─────────────────────────────────────────────────── */

static handle_table capture_table = { 0 };
static handle_table encoder_table = { 0 };
static handle_table transport_table = { 0 };
static handle_table decoder_table = { 0 };
static handle_table viewer_table = { 0 };

/* ══════════════════════════════════════════════════════════════════════
   Capture FFI wrappers
   ══════════════════════════════════════════════════════════════════════ */

int moonlink_capture_create_ffi(int output_index, int target_width, int target_height) {
  moonlink_capture_config config = { output_index, target_width, target_height };
  moonlink_capture_handle handle = NULL;
  moonlink_native_result result = moonlink_capture_create(&config, &handle);
  if (result != MOONLINK_RESULT_OK || handle == NULL) {
    return (int)result;
  }
  int slot = handle_table_alloc(&capture_table, handle);
  if (slot == MOONLINK_HANDLE_INVALID) {
    moonlink_capture_destroy(handle);
    return (int)MOONLINK_RESULT_OUT_OF_MEMORY;
  }
  return slot;
}

int moonlink_capture_next_frame_ffi(int handle, int timeout_ms) {
  moonlink_capture_session *session =
    (moonlink_capture_session *)handle_table_get(&capture_table, handle);
  if (session == NULL) {
    return (int)MOONLINK_RESULT_INVALID_HANDLE;
  }
  const moonlink_captured_frame *frame = NULL;
  moonlink_native_result result = moonlink_capture_next_frame(session, timeout_ms, &frame);
  return (int)result;
}

int moonlink_capture_frame_width_ffi(int handle) {
  moonlink_capture_session *session =
    (moonlink_capture_session *)handle_table_get(&capture_table, handle);
  if (session == NULL) {
    return 0;
  }
  return session->cached_frame.width;
}

int moonlink_capture_frame_height_ffi(int handle) {
  moonlink_capture_session *session =
    (moonlink_capture_session *)handle_table_get(&capture_table, handle);
  if (session == NULL) {
    return 0;
  }
  return session->cached_frame.height;
}

int moonlink_capture_frame_stride_y_ffi(int handle) {
  moonlink_capture_session *session =
    (moonlink_capture_session *)handle_table_get(&capture_table, handle);
  if (session == NULL) {
    return 0;
  }
  return session->cached_frame.stride_y;
}

int moonlink_capture_frame_stride_uv_ffi(int handle) {
  moonlink_capture_session *session =
    (moonlink_capture_session *)handle_table_get(&capture_table, handle);
  if (session == NULL) {
    return 0;
  }
  return session->cached_frame.stride_uv;
}

long long moonlink_capture_frame_timestamp_us_ffi(int handle) {
  moonlink_capture_session *session =
    (moonlink_capture_session *)handle_table_get(&capture_table, handle);
  if (session == NULL) {
    return 0;
  }
  return session->cached_frame.timestamp_us;
}

int moonlink_capture_frame_copy_nv12_ffi(int handle, unsigned char *dst, int dst_size) {
  moonlink_capture_session *session =
    (moonlink_capture_session *)handle_table_get(&capture_table, handle);
  if (session == NULL || dst == NULL) {
    return 0;
  }
  int frame_size = session->cached_frame.width * session->cached_frame.height * 3 / 2;
  if (dst_size < frame_size) {
    return 0;
  }
  memcpy(dst, session->cached_frame.nv12_data, frame_size);
  return frame_size;
}

int moonlink_capture_stop_ffi(int handle) {
  moonlink_capture_session *session =
    (moonlink_capture_session *)handle_table_get(&capture_table, handle);
  if (session == NULL) {
    return (int)MOONLINK_RESULT_INVALID_HANDLE;
  }
  return (int)moonlink_capture_stop(session);
}

int moonlink_capture_destroy_ffi(int handle) {
  moonlink_capture_session *session =
    (moonlink_capture_session *)handle_table_remove(&capture_table, handle);
  if (session == NULL) {
    return (int)MOONLINK_RESULT_INVALID_HANDLE;
  }
  return (int)moonlink_capture_destroy(session);
}

/* ══════════════════════════════════════════════════════════════════════
   Encoder FFI wrappers
   ══════════════════════════════════════════════════════════════════════ */

int moonlink_encoder_create_ffi(int width, int height, int fps, int bitrate, int use_hardware) {
  moonlink_encoder_config config = { width, height, fps, bitrate, use_hardware };
  moonlink_encoder_handle handle = NULL;
  moonlink_native_result result = moonlink_encoder_create(&config, &handle);
  if (result != MOONLINK_RESULT_OK || handle == NULL) {
    return (int)result;
  }
  int slot = handle_table_alloc(&encoder_table, handle);
  if (slot == MOONLINK_HANDLE_INVALID) {
    moonlink_encoder_destroy(handle);
    return (int)MOONLINK_RESULT_OUT_OF_MEMORY;
  }
  return slot;
}

int moonlink_encoder_encode_from_capture_ffi(int encoder_handle, int capture_handle) {
  moonlink_encoder_session *encoder =
    (moonlink_encoder_session *)handle_table_get(&encoder_table, encoder_handle);
  moonlink_capture_session *capture =
    (moonlink_capture_session *)handle_table_get(&capture_table, capture_handle);
  if (encoder == NULL || capture == NULL) {
    return (int)MOONLINK_RESULT_INVALID_HANDLE;
  }
  const moonlink_encoded_packet *packets = NULL;
  int packet_count = 0;
  moonlink_native_result result = moonlink_encoder_encode(
    encoder, &capture->cached_frame, &packets, &packet_count
  );
  if (result != MOONLINK_RESULT_OK) {
    return (int)result;
  }
  return packet_count;
}

int moonlink_encoder_packet_count_ffi(int handle) {
  moonlink_encoder_session *session =
    (moonlink_encoder_session *)handle_table_get(&encoder_table, handle);
  if (session == NULL) {
    return 0;
  }
  return session->cached_packet_count;
}

int moonlink_encoder_packet_size_ffi(int handle, int index) {
  moonlink_encoder_session *session =
    (moonlink_encoder_session *)handle_table_get(&encoder_table, handle);
  if (session == NULL || index < 0 || index >= session->cached_packet_count) {
    return 0;
  }
  return session->slots[index].packet.size;
}

int moonlink_encoder_packet_is_keyframe_ffi(int handle, int index) {
  moonlink_encoder_session *session =
    (moonlink_encoder_session *)handle_table_get(&encoder_table, handle);
  if (session == NULL || index < 0 || index >= session->cached_packet_count) {
    return 0;
  }
  return session->slots[index].packet.is_keyframe;
}

long long moonlink_encoder_packet_timestamp_us_ffi(int handle, int index) {
  moonlink_encoder_session *session =
    (moonlink_encoder_session *)handle_table_get(&encoder_table, handle);
  if (session == NULL || index < 0 || index >= session->cached_packet_count) {
    return 0;
  }
  return session->slots[index].packet.timestamp_us;
}

int moonlink_encoder_packet_copy_ffi(int handle, int index, unsigned char *dst, int dst_size) {
  moonlink_encoder_session *session =
    (moonlink_encoder_session *)handle_table_get(&encoder_table, handle);
  if (session == NULL || index < 0 || index >= session->cached_packet_count || dst == NULL) {
    return 0;
  }
  int size = session->slots[index].packet.size;
  if (dst_size < size) {
    return 0;
  }
  memcpy(dst, session->slots[index].packet.data, size);
  return size;
}

int moonlink_encoder_stop_ffi(int handle) {
  moonlink_encoder_session *session =
    (moonlink_encoder_session *)handle_table_get(&encoder_table, handle);
  if (session == NULL) {
    return (int)MOONLINK_RESULT_INVALID_HANDLE;
  }
  return (int)moonlink_encoder_stop(session);
}

int moonlink_encoder_destroy_ffi(int handle) {
  moonlink_encoder_session *session =
    (moonlink_encoder_session *)handle_table_remove(&encoder_table, handle);
  if (session == NULL) {
    return (int)MOONLINK_RESULT_INVALID_HANDLE;
  }
  return (int)moonlink_encoder_destroy(session);
}

/* ══════════════════════════════════════════════════════════════════════
   Transport FFI wrappers (only when libdatachannel is available)
   ══════════════════════════════════════════════════════════════════════ */

#ifdef MOONLINK_TRANSPORT_ENABLED

int moonlink_transport_create_ffi(int role) {
  moonlink_transport_config config = {
    (moonlink_transport_role)role,
    NULL,
  };
  moonlink_transport_handle handle = NULL;
  moonlink_native_result result = moonlink_transport_create(&config, &handle);
  if (result != MOONLINK_RESULT_OK || handle == NULL) {
    return (int)result;
  }
  int slot = handle_table_alloc(&transport_table, handle);
  if (slot == MOONLINK_HANDLE_INVALID) {
    moonlink_transport_destroy(handle);
    return (int)MOONLINK_RESULT_OUT_OF_MEMORY;
  }
  return slot;
}

int moonlink_transport_start_ffi(int handle) {
  moonlink_transport_session *session =
    (moonlink_transport_session *)handle_table_get(&transport_table, handle);
  if (session == NULL) {
    return (int)MOONLINK_RESULT_INVALID_HANDLE;
  }
  return (int)moonlink_transport_start(session);
}

int moonlink_transport_get_local_description_ffi(int handle, char *buffer, int buffer_size) {
  moonlink_transport_session *session =
    (moonlink_transport_session *)handle_table_get(&transport_table, handle);
  if (session == NULL) {
    return (int)MOONLINK_RESULT_INVALID_HANDLE;
  }
  int length = 0;
  moonlink_native_result result = moonlink_transport_get_local_description(
    session, buffer, buffer_size, &length
  );
  if (result != MOONLINK_RESULT_OK) {
    return (int)result;
  }
  return length;
}

int moonlink_transport_set_remote_description_ffi(int handle, const char *sdp, const char *type) {
  moonlink_transport_session *session =
    (moonlink_transport_session *)handle_table_get(&transport_table, handle);
  if (session == NULL) {
    return (int)MOONLINK_RESULT_INVALID_HANDLE;
  }
  return (int)moonlink_transport_set_remote_description(session, sdp, type);
}

int moonlink_transport_add_remote_candidate_ffi(int handle, const char *candidate, const char *mid) {
  moonlink_transport_session *session =
    (moonlink_transport_session *)handle_table_get(&transport_table, handle);
  if (session == NULL) {
    return (int)MOONLINK_RESULT_INVALID_HANDLE;
  }
  return (int)moonlink_transport_add_remote_candidate(session, candidate, mid);
}

int moonlink_transport_send_video_ffi(int handle, const unsigned char *data, int size) {
  moonlink_transport_session *session =
    (moonlink_transport_session *)handle_table_get(&transport_table, handle);
  if (session == NULL) {
    return (int)MOONLINK_RESULT_INVALID_HANDLE;
  }
  return (int)moonlink_transport_send_video(session, data, size);
}

int moonlink_transport_send_control_ffi(int handle, const char *message, int size) {
  moonlink_transport_session *session =
    (moonlink_transport_session *)handle_table_get(&transport_table, handle);
  if (session == NULL) {
    return (int)MOONLINK_RESULT_INVALID_HANDLE;
  }
  return (int)moonlink_transport_send_control(session, message, size);
}

int moonlink_transport_is_connected_ffi(int handle) {
  moonlink_transport_session *session =
    (moonlink_transport_session *)handle_table_get(&transport_table, handle);
  if (session == NULL) {
    return 0;
  }
  return InterlockedCompareExchange(&session->connected, 0, 0) != 0 ? 1 : 0;
}

int moonlink_transport_stop_ffi(int handle) {
  moonlink_transport_session *session =
    (moonlink_transport_session *)handle_table_get(&transport_table, handle);
  if (session == NULL) {
    return (int)MOONLINK_RESULT_INVALID_HANDLE;
  }
  return (int)moonlink_transport_stop(session);
}

int moonlink_transport_destroy_ffi(int handle) {
  moonlink_transport_session *session =
    (moonlink_transport_session *)handle_table_remove(&transport_table, handle);
  if (session == NULL) {
    return (int)MOONLINK_RESULT_INVALID_HANDLE;
  }
  return (int)moonlink_transport_destroy(session);
}

#else /* !MOONLINK_TRANSPORT_ENABLED -- stubs returning TRANSPORT_UNAVAILABLE */

int moonlink_transport_create_ffi(int role) { (void)role; return (int)MOONLINK_RESULT_TRANSPORT_UNAVAILABLE; }
int moonlink_transport_start_ffi(int handle) { (void)handle; return (int)MOONLINK_RESULT_TRANSPORT_UNAVAILABLE; }
int moonlink_transport_get_local_description_ffi(int handle, char *buffer, int buffer_size) { (void)handle; (void)buffer; (void)buffer_size; return (int)MOONLINK_RESULT_TRANSPORT_UNAVAILABLE; }
int moonlink_transport_set_remote_description_ffi(int handle, const char *sdp, const char *type) { (void)handle; (void)sdp; (void)type; return (int)MOONLINK_RESULT_TRANSPORT_UNAVAILABLE; }
int moonlink_transport_add_remote_candidate_ffi(int handle, const char *candidate, const char *mid) { (void)handle; (void)candidate; (void)mid; return (int)MOONLINK_RESULT_TRANSPORT_UNAVAILABLE; }
int moonlink_transport_send_video_ffi(int handle, const unsigned char *data, int size) { (void)handle; (void)data; (void)size; return (int)MOONLINK_RESULT_TRANSPORT_UNAVAILABLE; }
int moonlink_transport_send_control_ffi(int handle, const char *message, int size) { (void)handle; (void)message; (void)size; return (int)MOONLINK_RESULT_TRANSPORT_UNAVAILABLE; }
int moonlink_transport_is_connected_ffi(int handle) { (void)handle; return 0; }
int moonlink_transport_stop_ffi(int handle) { (void)handle; return (int)MOONLINK_RESULT_TRANSPORT_UNAVAILABLE; }
int moonlink_transport_destroy_ffi(int handle) { (void)handle; return (int)MOONLINK_RESULT_TRANSPORT_UNAVAILABLE; }

#endif /* MOONLINK_TRANSPORT_ENABLED */

/* ══════════════════════════════════════════════════════════════════════
   Decoder FFI wrappers
   ══════════════════════════════════════════════════════════════════════ */

int moonlink_decoder_create_ffi(int width, int height) {
  moonlink_decoder_config config = { width, height };
  moonlink_decoder_handle handle = NULL;
  moonlink_native_result result = moonlink_decoder_create(&config, &handle);
  if (result != MOONLINK_RESULT_OK || handle == NULL) {
    return (int)result;
  }
  int slot = handle_table_alloc(&decoder_table, handle);
  if (slot == MOONLINK_HANDLE_INVALID) {
    moonlink_decoder_destroy(handle);
    return (int)MOONLINK_RESULT_OUT_OF_MEMORY;
  }
  return slot;
}

int moonlink_decoder_decode_ffi(int handle, const unsigned char *data, int size) {
  moonlink_decoder_session *session =
    (moonlink_decoder_session *)handle_table_get(&decoder_table, handle);
  if (session == NULL) {
    return (int)MOONLINK_RESULT_INVALID_HANDLE;
  }
  const moonlink_decoded_frame *frame = NULL;
  moonlink_native_result result = moonlink_decoder_decode(session, data, size, &frame);
  if (result != MOONLINK_RESULT_OK) {
    return (int)result;
  }
  return frame != NULL ? 1 : 0;
}

int moonlink_decoder_frame_width_ffi(int handle) {
  moonlink_decoder_session *session =
    (moonlink_decoder_session *)handle_table_get(&decoder_table, handle);
  if (session == NULL) {
    return 0;
  }
  return session->cached_frame.width;
}

int moonlink_decoder_frame_height_ffi(int handle) {
  moonlink_decoder_session *session =
    (moonlink_decoder_session *)handle_table_get(&decoder_table, handle);
  if (session == NULL) {
    return 0;
  }
  return session->cached_frame.height;
}

int moonlink_decoder_frame_stride_ffi(int handle) {
  moonlink_decoder_session *session =
    (moonlink_decoder_session *)handle_table_get(&decoder_table, handle);
  if (session == NULL) {
    return 0;
  }
  return session->cached_frame.stride;
}

int moonlink_decoder_frame_copy_bgra_ffi(int handle, unsigned char *dst, int dst_size) {
  moonlink_decoder_session *session =
    (moonlink_decoder_session *)handle_table_get(&decoder_table, handle);
  if (session == NULL || dst == NULL) {
    return 0;
  }
  int frame_size = session->cached_frame.height * session->cached_frame.stride;
  if (dst_size < frame_size) {
    return 0;
  }
  memcpy(dst, session->cached_frame.bgra_data, frame_size);
  return frame_size;
}

int moonlink_decoder_stop_ffi(int handle) {
  moonlink_decoder_session *session =
    (moonlink_decoder_session *)handle_table_get(&decoder_table, handle);
  if (session == NULL) {
    return (int)MOONLINK_RESULT_INVALID_HANDLE;
  }
  return (int)moonlink_decoder_stop(session);
}

int moonlink_decoder_destroy_ffi(int handle) {
  moonlink_decoder_session *session =
    (moonlink_decoder_session *)handle_table_remove(&decoder_table, handle);
  if (session == NULL) {
    return (int)MOONLINK_RESULT_INVALID_HANDLE;
  }
  return (int)moonlink_decoder_destroy(session);
}

/* ══════════════════════════════════════════════════════════════════════
   Viewer FFI wrappers
   ══════════════════════════════════════════════════════════════════════ */

int moonlink_viewer_create_ffi(int width, int height) {
  moonlink_viewer_config config = { width, height, NULL };
  moonlink_viewer_handle handle = NULL;
  moonlink_native_result result = moonlink_viewer_create(&config, &handle);
  if (result != MOONLINK_RESULT_OK || handle == NULL) {
    return (int)result;
  }
  int slot = handle_table_alloc(&viewer_table, handle);
  if (slot == MOONLINK_HANDLE_INVALID) {
    moonlink_viewer_destroy(handle);
    return (int)MOONLINK_RESULT_OUT_OF_MEMORY;
  }
  return slot;
}

int moonlink_viewer_present_from_decoder_ffi(int viewer_handle, int decoder_handle) {
  moonlink_viewer_session *viewer =
    (moonlink_viewer_session *)handle_table_get(&viewer_table, viewer_handle);
  moonlink_decoder_session *decoder =
    (moonlink_decoder_session *)handle_table_get(&decoder_table, decoder_handle);
  if (viewer == NULL || decoder == NULL) {
    return (int)MOONLINK_RESULT_INVALID_HANDLE;
  }
  return (int)moonlink_viewer_present(viewer, &decoder->cached_frame);
}

int moonlink_viewer_pump_messages_ffi(int handle) {
  moonlink_viewer_session *session =
    (moonlink_viewer_session *)handle_table_get(&viewer_table, handle);
  if (session == NULL) {
    return 1;
  }
  return moonlink_viewer_pump_messages(session);
}

int moonlink_viewer_stop_ffi(int handle) {
  moonlink_viewer_session *session =
    (moonlink_viewer_session *)handle_table_get(&viewer_table, handle);
  if (session == NULL) {
    return (int)MOONLINK_RESULT_INVALID_HANDLE;
  }
  return (int)moonlink_viewer_stop(session);
}

int moonlink_viewer_destroy_ffi(int handle) {
  moonlink_viewer_session *session =
    (moonlink_viewer_session *)handle_table_remove(&viewer_table, handle);
  if (session == NULL) {
    return (int)MOONLINK_RESULT_INVALID_HANDLE;
  }
  return (int)moonlink_viewer_destroy(session);
}

/* ======================================================================
   Input executor FFI wrappers
   ====================================================================== */

static handle_table input_table = { 0 };

int moonlink_input_create_ffi(void) {
  moonlink_input_session *session =
    (moonlink_input_session *)calloc(1, sizeof(moonlink_input_session));
  if (session == NULL) {
    return (int)MOONLINK_RESULT_OUT_OF_MEMORY;
  }
  session->stopped = FALSE;
  session->screen_width = GetSystemMetrics(SM_CXSCREEN);
  session->screen_height = GetSystemMetrics(SM_CYSCREEN);
  int slot = handle_table_alloc(&input_table, session);
  if (slot == MOONLINK_HANDLE_INVALID) {
    free(session);
    return (int)MOONLINK_RESULT_OUT_OF_MEMORY;
  }
  return slot;
}

int moonlink_input_mouse_move_ffi(int handle, int x, int y) {
  moonlink_input_session *session =
    (moonlink_input_session *)handle_table_get(&input_table, handle);
  if (session == NULL || session->stopped) {
    return (int)MOONLINK_RESULT_INVALID_HANDLE;
  }
  INPUT input = { 0 };
  input.type = INPUT_MOUSE;
  input.mi.dx = (LONG)((x * 65535) / (session->screen_width > 1 ? session->screen_width - 1 : 1));
  input.mi.dy = (LONG)((y * 65535) / (session->screen_height > 1 ? session->screen_height - 1 : 1));
  input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
  SendInput(1, &input, sizeof(INPUT));
  return (int)MOONLINK_RESULT_OK;
}

int moonlink_input_mouse_button_ffi(int handle, int button, int action) {
  moonlink_input_session *session =
    (moonlink_input_session *)handle_table_get(&input_table, handle);
  if (session == NULL || session->stopped) {
    return (int)MOONLINK_RESULT_INVALID_HANDLE;
  }
  INPUT input = { 0 };
  input.type = INPUT_MOUSE;
  DWORD down_flag = 0, up_flag = 0;
  switch (button) {
    case MOONLINK_MOUSE_LEFT:   down_flag = MOUSEEVENTF_LEFTDOWN;  up_flag = MOUSEEVENTF_LEFTUP;   break;
    case MOONLINK_MOUSE_RIGHT:  down_flag = MOUSEEVENTF_RIGHTDOWN; up_flag = MOUSEEVENTF_RIGHTUP;  break;
    case MOONLINK_MOUSE_MIDDLE: down_flag = MOUSEEVENTF_MIDDLEDOWN;up_flag = MOUSEEVENTF_MIDDLEUP; break;
    default: return (int)MOONLINK_RESULT_INVALID_HANDLE;
  }
  input.mi.dwFlags = (action == MOONLINK_MOUSE_DOWN) ? down_flag : up_flag;
  SendInput(1, &input, sizeof(INPUT));
  return (int)MOONLINK_RESULT_OK;
}

int moonlink_input_key_ffi(int handle, int vk_code, int down) {
  moonlink_input_session *session =
    (moonlink_input_session *)handle_table_get(&input_table, handle);
  if (session == NULL || session->stopped) {
    return (int)MOONLINK_RESULT_INVALID_HANDLE;
  }
  INPUT input = { 0 };
  input.type = INPUT_KEYBOARD;
  input.ki.wVk = (WORD)vk_code;
  input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
  SendInput(1, &input, sizeof(INPUT));
  return (int)MOONLINK_RESULT_OK;
}

int moonlink_input_stop_ffi(int handle) {
  moonlink_input_session *session =
    (moonlink_input_session *)handle_table_get(&input_table, handle);
  if (session == NULL) {
    return (int)MOONLINK_RESULT_INVALID_HANDLE;
  }
  session->stopped = TRUE;
  return (int)MOONLINK_RESULT_OK;
}

int moonlink_input_destroy_ffi(int handle) {
  moonlink_input_session *session =
    (moonlink_input_session *)handle_table_remove(&input_table, handle);
  if (session == NULL) {
    return (int)MOONLINK_RESULT_INVALID_HANDLE;
  }
  free(session);
  return (int)MOONLINK_RESULT_OK;
}

/* ======================================================================
   Shared buffer pool for FFI data exchange
   ====================================================================== */

typedef struct buffer_slot {
  uint8_t *data;
  int size;
  int capacity;
} buffer_slot;

static buffer_slot g_buffers[MOONLINK_BUFFER_POOL_SIZE];
static CRITICAL_SECTION g_buffer_lock;
static volatile LONG g_buffer_initialized = 0;

static void buffer_pool_init(void) {
  if (InterlockedCompareExchange(&g_buffer_initialized, 1, 0) == 0) {
    memset(g_buffers, 0, sizeof(g_buffers));
    InitializeCriticalSection(&g_buffer_lock);
  }
}

int moonlink_buffer_alloc_ffi(int size) {
  if (size <= 0 || size > MOONLINK_BUFFER_MAX_SIZE) return (int)MOONLINK_RESULT_INVALID_HANDLE;
  buffer_pool_init();
  EnterCriticalSection(&g_buffer_lock);
  for (int i = 1; i < MOONLINK_BUFFER_POOL_SIZE; i++) {
    if (g_buffers[i].data == NULL) {
      g_buffers[i].data = (uint8_t *)malloc(size);
      if (!g_buffers[i].data) {
        LeaveCriticalSection(&g_buffer_lock);
        return (int)MOONLINK_RESULT_OUT_OF_MEMORY;
      }
      g_buffers[i].capacity = size;
      g_buffers[i].size = 0;
      LeaveCriticalSection(&g_buffer_lock);
      return i;
    }
  }
  LeaveCriticalSection(&g_buffer_lock);
  return (int)MOONLINK_RESULT_OUT_OF_MEMORY;
}

int moonlink_buffer_write_ffi(int slot, const char *src, int size) {
  if (slot <= 0 || slot >= MOONLINK_BUFFER_POOL_SIZE || !src || size <= 0) return 0;
  buffer_pool_init();
  EnterCriticalSection(&g_buffer_lock);
  buffer_slot *b = &g_buffers[slot];
  if (!b->data) { LeaveCriticalSection(&g_buffer_lock); return 0; }
  if (size > b->capacity) {
    uint8_t *new_data = (uint8_t *)realloc(b->data, size);
    if (!new_data) { LeaveCriticalSection(&g_buffer_lock); return 0; }
    b->data = new_data;
    b->capacity = size;
  }
  memcpy(b->data, src, size);
  b->size = size;
  LeaveCriticalSection(&g_buffer_lock);
  return size;
}

int moonlink_buffer_read_ffi(int slot, char *dst, int max_size) {
  if (slot <= 0 || slot >= MOONLINK_BUFFER_POOL_SIZE || !dst || max_size <= 0) return 0;
  buffer_pool_init();
  EnterCriticalSection(&g_buffer_lock);
  buffer_slot *b = &g_buffers[slot];
  if (!b->data || b->size <= 0) { LeaveCriticalSection(&g_buffer_lock); return 0; }
  int copy_size = (b->size < max_size) ? b->size : max_size;
  memcpy(dst, b->data, copy_size);
  LeaveCriticalSection(&g_buffer_lock);
  return copy_size;
}

int moonlink_buffer_size_ffi(int slot) {
  if (slot <= 0 || slot >= MOONLINK_BUFFER_POOL_SIZE) return 0;
  buffer_pool_init();
  EnterCriticalSection(&g_buffer_lock);
  int s = g_buffers[slot].size;
  LeaveCriticalSection(&g_buffer_lock);
  return s;
}

int moonlink_buffer_free_ffi(int slot) {
  if (slot <= 0 || slot >= MOONLINK_BUFFER_POOL_SIZE) return (int)MOONLINK_RESULT_INVALID_HANDLE;
  buffer_pool_init();
  EnterCriticalSection(&g_buffer_lock);
  if (g_buffers[slot].data) {
    free(g_buffers[slot].data);
    g_buffers[slot].data = NULL;
    g_buffers[slot].size = 0;
    g_buffers[slot].capacity = 0;
  }
  LeaveCriticalSection(&g_buffer_lock);
  return (int)MOONLINK_RESULT_OK;
}

/* ======================================================================
   Transport <-> Signaling bridge + video receive
   ====================================================================== */

#ifdef MOONLINK_TRANSPORT_ENABLED

#include <rtc/rtc.h>

int moonlink_transport_set_remote_from_signal_ffi(int transport_handle, int signal_handle) {
  moonlink_transport_session *ts =
    (moonlink_transport_session *)handle_table_get(&transport_table, transport_handle);
  if (!ts) return (int)MOONLINK_RESULT_INVALID_HANDLE;

  char buf[MOONLINK_SIGNAL_MAX_MESSAGE];
  int received = moonlink_signal_raw_recv(signal_handle, buf, sizeof(buf));
  if (received <= 0) return (int)MOONLINK_RESULT_TRANSPORT_UNAVAILABLE;

  /* Parse SDP type from first line. */
  const char *type = "offer";
  if (strstr(buf, "ANSWER") || strstr(buf, "answer")) type = "answer";

  int rc = rtcSetRemoteDescription(ts->peer_connection, buf, type);
  return (rc == RTC_ERR_SUCCESS) ? (int)MOONLINK_RESULT_OK : (int)MOONLINK_RESULT_TRANSPORT_UNAVAILABLE;
}

int moonlink_transport_send_local_desc_via_signal_ffi(int transport_handle, int signal_handle) {
  moonlink_transport_session *ts =
    (moonlink_transport_session *)handle_table_get(&transport_table, transport_handle);
  if (!ts) return (int)MOONLINK_RESULT_INVALID_HANDLE;

  char buf[MOONLINK_SIGNAL_MAX_MESSAGE];
  int len = rtcGetLocalDescription(ts->peer_connection, buf, sizeof(buf));
  if (len <= 0) return (int)MOONLINK_RESULT_TRANSPORT_UNAVAILABLE;

  int rc = moonlink_signal_raw_send(signal_handle, buf, len);
  return (rc == (int)MOONLINK_RESULT_OK) ? (int)MOONLINK_RESULT_OK : (int)MOONLINK_RESULT_TRANSPORT_UNAVAILABLE;
}

int moonlink_transport_send_video_from_buffer_ffi(int transport_handle, int buf_slot) {
  moonlink_transport_session *ts =
    (moonlink_transport_session *)handle_table_get(&transport_table, transport_handle);
  if (!ts) return (int)MOONLINK_RESULT_INVALID_HANDLE;
  if (!InterlockedCompareExchange(&ts->connected, 0, 0)) return (int)MOONLINK_RESULT_TRANSPORT_UNAVAILABLE;
  /* Media must travel on the negotiated H.264 track. The control data channel is
     reserved for control events and cannot carry full frames. */
  if (ts->video_track < 0) return (int)MOONLINK_RESULT_TRANSPORT_UNAVAILABLE;

  buffer_pool_init();
  EnterCriticalSection(&g_buffer_lock);
  buffer_slot *b = &g_buffers[buf_slot];
  if (!b || !b->data || b->size <= 0) {
    LeaveCriticalSection(&g_buffer_lock);
    return (int)MOONLINK_RESULT_INVALID_HANDLE;
  }
  int data_size = b->size;
  uint8_t *data_copy = (uint8_t *)malloc(data_size);
  if (!data_copy) { LeaveCriticalSection(&g_buffer_lock); return (int)MOONLINK_RESULT_OUT_OF_MEMORY; }
  memcpy(data_copy, b->data, data_size);
  LeaveCriticalSection(&g_buffer_lock);

  /* The track is configured with RTC_NAL_SEPARATOR_START_SEQUENCE, so the
     Annex-B byte stream produced by the Media Foundation encoder is handed to
     libdatachannel as one access unit and fragmented into RTP by the packetizer. */
  int rc = rtcSendMessage(ts->video_track, (const char *)data_copy, data_size);
  free(data_copy);
  return (rc >= 0) ? (int)MOONLINK_RESULT_OK : (int)MOONLINK_RESULT_TRANSPORT_UNAVAILABLE;
}

int moonlink_transport_pending_video_count_ffi(int transport_handle) {
  moonlink_transport_session *ts =
    (moonlink_transport_session *)handle_table_get(&transport_table, transport_handle);
  if (!ts) return 0;
  LONG head = InterlockedCompareExchange(&ts->video_ring_head, 0, 0);
  LONG tail = InterlockedCompareExchange(&ts->video_ring_tail, 0, 0);
  int count = (head - tail + VIDEO_RING_CAPACITY) % VIDEO_RING_CAPACITY;
  return count;
}

int moonlink_transport_pop_video_to_buffer_ffi(int transport_handle, int buf_slot) {
  moonlink_transport_session *ts =
    (moonlink_transport_session *)handle_table_get(&transport_table, transport_handle);
  if (!ts) return 0;

  LONG head = InterlockedCompareExchange(&ts->video_ring_head, 0, 0);
  LONG tail = InterlockedCompareExchange(&ts->video_ring_tail, 0, 0);
  if (head == tail) return 0; /* empty */

  uint8_t *pkt = ts->video_ring[tail];
  int pkt_size = ts->video_ring_size[tail];
  ts->video_ring[tail] = NULL;
  ts->video_ring_size[tail] = 0;
  InterlockedExchange(&ts->video_ring_tail, (tail + 1) % VIDEO_RING_CAPACITY);

  if (!pkt || pkt_size <= 0) return 0;

  /* Write into buffer slot. */
  buffer_pool_init();
  EnterCriticalSection(&g_buffer_lock);
  buffer_slot *b = &g_buffers[buf_slot];
  if (!b || !b->data) {
    LeaveCriticalSection(&g_buffer_lock);
    free(pkt);
    return 0;
  }
  if (pkt_size > b->capacity) {
    uint8_t *new_data = (uint8_t *)realloc(b->data, pkt_size);
    if (!new_data) { LeaveCriticalSection(&g_buffer_lock); free(pkt); return 0; }
    b->data = new_data;
    b->capacity = pkt_size;
  }
  memcpy(b->data, pkt, pkt_size);
  b->size = pkt_size;
  LeaveCriticalSection(&g_buffer_lock);
  free(pkt);
  return pkt_size;
}

#else /* !MOONLINK_TRANSPORT_ENABLED */

int moonlink_transport_set_remote_from_signal_ffi(int t, int s) { (void)t; (void)s; return (int)MOONLINK_RESULT_TRANSPORT_UNAVAILABLE; }
int moonlink_transport_send_local_desc_via_signal_ffi(int t, int s) { (void)t; (void)s; return (int)MOONLINK_RESULT_TRANSPORT_UNAVAILABLE; }
int moonlink_transport_send_video_from_buffer_ffi(int t, int b) { (void)t; (void)b; return (int)MOONLINK_RESULT_TRANSPORT_UNAVAILABLE; }
int moonlink_transport_pending_video_count_ffi(int t) { (void)t; return 0; }
int moonlink_transport_pop_video_to_buffer_ffi(int t, int b) { (void)t; (void)b; return 0; }

#endif /* MOONLINK_TRANSPORT_ENABLED */

/* ======================================================================
   Encoder/Decoder <-> Buffer bridge
   ====================================================================== */

int moonlink_encoder_encode_to_buffer_ffi(int encoder_handle, int capture_handle, int buf_slot) {
  moonlink_encoder_session *enc =
    (moonlink_encoder_session *)handle_table_get(&encoder_table, encoder_handle);
  moonlink_capture_session *cap =
    (moonlink_capture_session *)handle_table_get(&capture_table, capture_handle);
  if (!enc || !cap) return 0;

  const moonlink_encoded_packet *packets = NULL;
  int packet_count = 0;
  moonlink_native_result result = moonlink_encoder_encode(enc, &cap->cached_frame, &packets, &packet_count);
  if (result != MOONLINK_RESULT_OK || packet_count <= 0 || !packets) return 0;

  /* Write first packet into buffer slot. */
  buffer_pool_init();
  EnterCriticalSection(&g_buffer_lock);
  buffer_slot *b = &g_buffers[buf_slot];
  if (!b || !b->data) { LeaveCriticalSection(&g_buffer_lock); return 0; }
  int pkt_size = packets[0].size;
  if (pkt_size > b->capacity) {
    uint8_t *new_data = (uint8_t *)realloc(b->data, pkt_size);
    if (!new_data) { LeaveCriticalSection(&g_buffer_lock); return 0; }
    b->data = new_data;
    b->capacity = pkt_size;
  }
  memcpy(b->data, packets[0].data, pkt_size);
  b->size = pkt_size;
  LeaveCriticalSection(&g_buffer_lock);
  return pkt_size;
}

int moonlink_decoder_decode_from_buffer_ffi(int decoder_handle, int buf_slot) {
  moonlink_decoder_session *dec =
    (moonlink_decoder_session *)handle_table_get(&decoder_table, decoder_handle);
  if (!dec) return (int)MOONLINK_RESULT_INVALID_HANDLE;

  buffer_pool_init();
  EnterCriticalSection(&g_buffer_lock);
  buffer_slot *b = &g_buffers[buf_slot];
  if (!b || !b->data || b->size <= 0) { LeaveCriticalSection(&g_buffer_lock); return 0; }
  /* Copy data out since decoder may hold a reference. */
  uint8_t *tmp = (uint8_t *)malloc(b->size);
  if (!tmp) { LeaveCriticalSection(&g_buffer_lock); return (int)MOONLINK_RESULT_OUT_OF_MEMORY; }
  memcpy(tmp, b->data, b->size);
  int tmp_size = b->size;
  LeaveCriticalSection(&g_buffer_lock);

  const moonlink_decoded_frame *frame = NULL;
  moonlink_native_result result = moonlink_decoder_decode(dec, tmp, tmp_size, &frame);
  free(tmp);
  if (result != MOONLINK_RESULT_OK) return (int)result;
  return frame != NULL ? 1 : 0;
}

int moonlink_viewer_present_decoded_ffi(int viewer_handle, int decoder_handle) {
  moonlink_viewer_session *viewer =
    (moonlink_viewer_session *)handle_table_get(&viewer_table, viewer_handle);
  moonlink_decoder_session *dec =
    (moonlink_decoder_session *)handle_table_get(&decoder_table, decoder_handle);
  if (!viewer || !dec) return (int)MOONLINK_RESULT_INVALID_HANDLE;
  return (int)moonlink_viewer_present(viewer, &dec->cached_frame);
}
