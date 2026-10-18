/*
 * decoder_loop_tests.c — Encoder/decoder round-trip test.
 *
 * Captures the desktop, encodes real H.264 access units and feeds every
 * encoded packet back into a decoder. This covers the sample timing and stream
 * start-up contracts that the Media Foundation decoder enforces; without them
 * the decoder emits a single frame and then stalls.
 */

#include "moonlink_native_bridge.h"

#include <stdio.h>

#define TARGET_FRAMES 40

int main(void) {
  int failures = 0;
  printf("=== MoonLink decoder round-trip test ===\n\n");

  int capture = moonlink_capture_create_ffi(0, 1280, 720);
  if (capture <= 0) {
    printf("SKIP: capture unavailable (%d)\n", capture);
    return 0;
  }

  int hw = moonlink_native_h264_hardware_available();
  int encoder = moonlink_encoder_create_ffi(1280, 720, hw ? 60 : 30, 2000000, hw);
  if (encoder <= 0) {
    encoder = moonlink_encoder_create_ffi(1280, 720, 30, 2000000, 0);
  }
  if (encoder <= 0) {
    printf("FAIL: encoder create (%d)\n", encoder);
    moonlink_capture_destroy_ffi(capture);
    return 1;
  }

  int decoder = moonlink_decoder_create_ffi(1280, 720);
  if (decoder <= 0) {
    printf("FAIL: decoder create (%d)\n", decoder);
    moonlink_encoder_destroy_ffi(encoder);
    moonlink_capture_destroy_ffi(capture);
    return 1;
  }

  int buf = moonlink_buffer_alloc_ffi(262144);
  if (buf <= 0) {
    printf("FAIL: buffer alloc\n");
    failures += 1;
    goto cleanup;
  }

  if (moonlink_capture_next_frame_ffi(capture, 500) == MOONLINK_RESULT_OK) {
    printf("Capture frame: %dx%d\n",
           moonlink_capture_frame_width_ffi(capture),
           moonlink_capture_frame_height_ffi(capture));
  }

  int packets = 0;
  int decoded = 0;
  for (int i = 0; i < TARGET_FRAMES; i += 1) {
    if (moonlink_capture_next_frame_ffi(capture, 500) != MOONLINK_RESULT_OK) {
      continue;
    }
    int size = moonlink_encoder_encode_to_buffer_ffi(encoder, capture, buf);
    if (size <= 0) {
      continue;
    }
    packets += 1;
    if (moonlink_decoder_decode_from_buffer_ffi(decoder, buf) == 1) {
      decoded += 1;
    }
  }

  printf("Encoded packets: %d\n", packets);
  printf("Decoded frames:  %d\n", decoded);
  if (packets == 0) {
    printf("FAIL: encoder produced no packets\n");
    failures += 1;
  } else if (decoded < packets / 2) {
    printf("FAIL: decoder stalled (%d of %d packets decoded)\n", decoded, packets);
    failures += 1;
  } else {
    printf("PASS\n");
  }

  moonlink_buffer_free_ffi(buf);
cleanup:
  moonlink_decoder_destroy_ffi(decoder);
  moonlink_encoder_destroy_ffi(encoder);
  moonlink_capture_destroy_ffi(capture);

  printf("\n=== %s ===\n", failures == 0 ? "ALL TESTS PASSED" : "FAILURES");
  return failures == 0 ? 0 : 1;
}
