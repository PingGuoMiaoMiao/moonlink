/*
 * controller_main.c — MoonLink Controller (控制端) C application.
 *
 * Connects to host via TCP signaling, exchanges SDP, receives H.264
 * video over WebRTC, decodes and displays in a D3D11 window.
 *
 * Usage: moonlink_controller.exe [host_ip] [port]
 * Default: 127.0.0.1:9876
 */

#include "moonlink_native_bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define DEFAULT_PORT 9876
#define MAX_ITERATIONS 600  /* ~10 seconds at 60fps */
#define BUF_SIZE       262144

int main(int argc, char *argv[]) {
  unsigned int host_ip = 0x7F000001; /* 127.0.0.1 */
  int port = DEFAULT_PORT;

  if (argc > 1) {
    /* Parse dotted IP. */
    unsigned int a, b, c, d;
    if (sscanf(argv[1], "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
      host_ip = (a << 24) | (b << 16) | (c << 8) | d;
    }
  }
  if (argc > 2) {
    port = atoi(argv[2]);
    if (port <= 0) port = DEFAULT_PORT;
  }

  printf("=== MoonLink Controller ===\n");
  printf("Host: %u.%u.%u.%u:%d\n\n",
         (host_ip >> 24) & 0xFF, (host_ip >> 16) & 0xFF,
         (host_ip >> 8) & 0xFF, host_ip & 0xFF, port);

  /* ── Create sessions ─────────────────────────────────────────────────── */
  int decoder = moonlink_decoder_create_ffi(1280, 720);
  if (decoder <= 0) {
    printf("ERROR: Decoder create failed (%d)\n", decoder);
    return 1;
  }

  int viewer = moonlink_viewer_create_ffi(1280, 720);
  if (viewer <= 0) {
    printf("ERROR: Viewer create failed (%d)\n", viewer);
    moonlink_decoder_destroy_ffi(decoder);
    return 1;
  }

#ifdef MOONLINK_TRANSPORT_ENABLED
  int transport = moonlink_transport_create_ffi(1); /* CONTROLLER role */
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

  /* ── Connect to host via signaling ───────────────────────────────────── */
  printf("Connecting to host...\n");
  int signal = moonlink_signal_connect_ip_ffi(host_ip, port);
  if (signal <= 0) {
    printf("ERROR: Connect failed (%d). Is host running?\n", signal);
    goto fail_connect;
  }
  printf("Connected to host!\n");

#ifdef MOONLINK_TRANSPORT_ENABLED
  /* ── SDP exchange ────────────────────────────────────────────────────── */
  printf("Receiving SDP offer...\n");
  int rc = moonlink_transport_set_remote_from_signal_ffi(transport, signal);
  if (rc != MOONLINK_RESULT_OK) {
    printf("ERROR: Recv SDP failed (%d)\n", rc);
    goto fail_sdp;
  }
  printf("SDP offer received.\n");

  moonlink_transport_start_ffi(transport);

  printf("Sending SDP answer...\n");
  rc = moonlink_transport_send_local_desc_via_signal_ffi(transport, signal);
  if (rc != MOONLINK_RESULT_OK) {
    printf("ERROR: Send SDP failed (%d)\n", rc);
    goto fail_sdp;
  }
  printf("SDP answer sent.\n");

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
  printf("WebRTC connected! Receiving video...\n\n");

  /* ── Main receive loop ───────────────────────────────────────────────── */
  int packets_received = 0;
  int frames_decoded = 0;
  int frames_presented = 0;

  for (int i = 0; i < MAX_ITERATIONS; i++) {
    /* Check for pending video packets. */
    int pending = moonlink_transport_pending_video_count_ffi(transport);
    while (pending > 0) {
      int pkt_size = moonlink_transport_pop_video_to_buffer_ffi(transport, buf);
      if (pkt_size > 0) {
        packets_received++;

        /* Decode. */
        int dr = moonlink_decoder_decode_from_buffer_ffi(decoder, buf);
        if (dr == 1) {
          frames_decoded++;
          /* Present. */
          int pr = moonlink_viewer_present_decoded_ffi(viewer, decoder);
          if (pr == MOONLINK_RESULT_OK) {
            frames_presented++;
          }
        }
      }
      pending = moonlink_transport_pending_video_count_ffi(transport);
    }

    /* Pump viewer window messages. */
    if (moonlink_viewer_pump_messages_ffi(viewer) != 0) {
      printf("Viewer window closed by user.\n");
      break;
    }

    /* Progress report. */
    if (i > 0 && i % 120 == 0) {
      printf("  %d pkts, %d decoded, %d presented\n",
             packets_received, frames_decoded, frames_presented);
    }

    Sleep(16); /* ~60fps */
  }

  printf("\nSession complete:\n");
  printf("  Packets received: %d\n", packets_received);
  printf("  Frames decoded:   %d\n", frames_decoded);
  printf("  Frames presented: %d\n", frames_presented);

fail_sdp:
  moonlink_signal_close_ffi(signal);
  moonlink_transport_stop_ffi(transport);
  moonlink_transport_destroy_ffi(transport);
#else
  printf("WARN: Transport not available (libdatachannel not linked)\n");
  moonlink_signal_close_ffi(signal);
#endif

  moonlink_buffer_free_ffi(buf);
  moonlink_viewer_stop_ffi(viewer);
  moonlink_viewer_destroy_ffi(viewer);
  moonlink_decoder_stop_ffi(decoder);
  moonlink_decoder_destroy_ffi(decoder);
  printf("Done.\n");
  return 0;

fail_connect:
  moonlink_buffer_free_ffi(buf);
fail_no_buf:
#ifdef MOONLINK_TRANSPORT_ENABLED
  moonlink_transport_stop_ffi(transport);
  moonlink_transport_destroy_ffi(transport);
fail_no_transport:
#endif
  moonlink_viewer_stop_ffi(viewer);
  moonlink_viewer_destroy_ffi(viewer);
  moonlink_decoder_stop_ffi(decoder);
  moonlink_decoder_destroy_ffi(decoder);
  printf("Failed.\n");
  return 1;
}
