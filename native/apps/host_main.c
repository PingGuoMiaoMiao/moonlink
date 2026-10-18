/*
 * host_main.c — MoonLink Host (被控端) C application.
 *
 * Captures desktop, encodes H.264, exchanges SDP via TCP signaling,
 * sends encoded video over WebRTC to the controller.
 *
 * Usage: moonlink_host.exe [port]
 * Default port: 9876
 */

#include "moonlink_native_bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define DEFAULT_PORT 9876
#define MAX_FRAMES   600   /* ~10 seconds at 60fps */
#define BUF_SIZE     262144

int main(int argc, char *argv[]) {
  int port = DEFAULT_PORT;
  if (argc > 1) {
    port = atoi(argv[1]);
    if (port <= 0) port = DEFAULT_PORT;
  }

  printf("=== MoonLink Host ===\n");
  printf("Port: %d\n\n", port);

  /* ── Check prerequisites ─────────────────────────────────────────────── */
  int displays = moonlink_native_display_count();
  if (displays <= 0) {
    printf("ERROR: No displays detected.\n");
    return 1;
  }
  printf("Displays: %d\n", displays);

  int hw = moonlink_native_h264_hardware_available();
  int fps = hw ? 60 : 30;
  printf("Encoder: %s @ %dfps\n", hw ? "HW" : "SW", fps);

  /* ── Create sessions ─────────────────────────────────────────────────── */
  int capture = moonlink_capture_create_ffi(0, 1280, 720);
  if (capture <= 0) {
    printf("ERROR: Capture create failed (%d)\n", capture);
    printf("(Desktop Duplication may require an active desktop session)\n");
    return 1;
  }

  int encoder = moonlink_encoder_create_ffi(1280, 720, fps, 2000000, hw);
  if (encoder <= 0) {
    /* Fallback to SW. */
    encoder = moonlink_encoder_create_ffi(1280, 720, 30, 2000000, 0);
  }
  if (encoder <= 0) {
    printf("ERROR: Encoder create failed\n");
    moonlink_capture_destroy_ffi(capture);
    return 1;
  }

#ifdef MOONLINK_TRANSPORT_ENABLED
  int transport = moonlink_transport_create_ffi(0); /* HOST role */
  if (transport <= 0) {
    printf("ERROR: Transport create failed\n");
    goto fail_no_transport;
  }
#endif

  int buf = moonlink_buffer_alloc_ffi(BUF_SIZE);
  if (buf <= 0) {
    printf("ERROR: Buffer alloc failed\n");
    goto fail_no_buf;
  }

  /* ── Signaling: listen for controller ────────────────────────────────── */
  printf("\nListening on port %d...\n", port);
  int listener = moonlink_signal_listen_ffi(port);
  if (listener <= 0) {
    printf("ERROR: Listen failed (%d)\n", listener);
    goto fail_listen;
  }

  printf("Waiting for controller to connect...\n");
  int signal = moonlink_signal_accept_ffi(listener);
  if (signal <= 0) {
    printf("ERROR: Accept failed (%d)\n", signal);
    moonlink_signal_stop_listen_ffi(listener);
    goto fail_listen;
  }
  moonlink_signal_stop_listen_ffi(listener);
  printf("Controller connected!\n");

#ifdef MOONLINK_TRANSPORT_ENABLED
  /* ── SDP exchange ────────────────────────────────────────────────────── */
  moonlink_transport_start_ffi(transport);

  printf("Sending SDP offer...\n");
  int rc = moonlink_transport_send_local_desc_via_signal_ffi(transport, signal);
  if (rc != MOONLINK_RESULT_OK) {
    printf("ERROR: Send SDP failed (%d)\n", rc);
    goto fail_sdp;
  }
  printf("SDP offer sent.\n");

  printf("Waiting for SDP answer...\n");
  rc = moonlink_transport_set_remote_from_signal_ffi(transport, signal);
  if (rc != MOONLINK_RESULT_OK) {
    printf("ERROR: Recv SDP failed (%d)\n", rc);
    goto fail_sdp;
  }
  printf("SDP answer received.\n");

  /* Wait for WebRTC connection. */
  printf("Waiting for WebRTC connection...\n");
  int connected = 0;
  for (int i = 0; i < 200; i++) {
    if (moonlink_transport_is_connected_ffi(transport)) {
      connected = 1;
      break;
    }
    Sleep(50);
  }
  if (!connected) {
    printf("ERROR: WebRTC connection timeout\n");
    goto fail_sdp;
  }
  printf("WebRTC connected! Streaming...\n\n");

  /* ── Main streaming loop ─────────────────────────────────────────────── */
  int frame_count = 0;
  int sent_count = 0;
  int total_bytes = 0;

  while (frame_count < MAX_FRAMES) {
    int fr = moonlink_capture_next_frame_ffi(capture, 100);
    if (fr == MOONLINK_RESULT_OK) {
      frame_count++;
      int pkt_size = moonlink_encoder_encode_to_buffer_ffi(encoder, capture, buf);
      if (pkt_size > 0) {
        int sr = moonlink_transport_send_video_from_buffer_ffi(transport, buf);
        if (sr == MOONLINK_RESULT_OK) {
          sent_count++;
          total_bytes += pkt_size;
        }
      }
      if (frame_count % 60 == 0 && frame_count > 0) {
        printf("  %d frames, %d sent, %dKB\n",
               frame_count, sent_count, total_bytes / 1024);
      }
    } else if (fr != MOONLINK_RESULT_TIMEOUT) {
      printf("Capture error (%d), stopping.\n", fr);
      break;
    }
  }

  printf("\nStreaming complete: %d frames, %d sent, %dKB total\n",
         frame_count, sent_count, total_bytes / 1024);

fail_sdp:
  /* Stop transport first to flush pending sends, then close signal */
  moonlink_transport_stop_ffi(transport);
  Sleep(200);
  moonlink_signal_close_ffi(signal);
  moonlink_transport_destroy_ffi(transport);
#else
  printf("WARN: Transport not available (libdatachannel not linked)\n");
  moonlink_signal_close_ffi(signal);
#endif

  moonlink_buffer_free_ffi(buf);
  moonlink_encoder_stop_ffi(encoder);
  moonlink_encoder_destroy_ffi(encoder);
  moonlink_capture_stop_ffi(capture);
  moonlink_capture_destroy_ffi(capture);
  printf("Done.\n");
  return 0;

fail_listen:
  moonlink_buffer_free_ffi(buf);
fail_no_buf:
#ifdef MOONLINK_TRANSPORT_ENABLED
  moonlink_transport_stop_ffi(transport);
  moonlink_transport_destroy_ffi(transport);
fail_no_transport:
#endif
  moonlink_encoder_stop_ffi(encoder);
  moonlink_encoder_destroy_ffi(encoder);
  moonlink_capture_stop_ffi(capture);
  moonlink_capture_destroy_ffi(capture);
  printf("Failed.\n");
  return 1;
}
