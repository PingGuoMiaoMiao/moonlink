/*
 * depacketizer_tests.c — Unit tests for the H.264 RTP depacketizer.
 *
 * The depacketizer is the only piece that turns received RTP into the Annex-B
 * access units consumed by the Media Foundation decoder, so it is covered
 * independently of any network transport.
 */

#include "h264_depacketizer.h"

#include <stdio.h>
#include <string.h>

static int g_failures = 0;

#define CHECK(condition)                                                        \
  do {                                                                          \
    if (!(condition)) {                                                         \
      printf("  FAIL: %s:%d: %s\n", __FILE__, __LINE__, #condition);             \
      g_failures += 1;                                                          \
    }                                                                           \
  } while (0)

/* Build a 12-byte RTP header followed by the given payload. */
static int build_rtp(
  uint8_t *out,
  int capacity,
  const uint8_t *payload,
  int payload_size,
  int marker
) {
  if (capacity < 12 + payload_size) {
    return -1;
  }
  memset(out, 0, 12);
  out[0] = 0x80; /* version 2, no padding, no extension, no CSRC */
  out[1] = (uint8_t)((marker ? 0x80 : 0x00) | 96);
  out[2] = 0x00;
  out[3] = 0x01;
  out[4] = 0x00;
  out[5] = 0x00;
  out[6] = 0x00;
  out[7] = 0x01;
  out[8] = 0x00;
  out[9] = 0x00;
  out[10] = 0x00;
  out[11] = 0x01;
  memcpy(out + 12, payload, (size_t)payload_size);
  return 12 + payload_size;
}

static int annex_b_matches(const uint8_t *frame, int size, const uint8_t *expected, int expected_size) {
  return size == expected_size && memcmp(frame, expected, (size_t)expected_size) == 0;
}

static void test_single_nal(void) {
  printf("[1] single NAL unit\n");
  moonlink_h264_depack depack;
  moonlink_h264_depack_init(&depack);

  const uint8_t nal[] = { 0x65, 0x88, 0x84, 0x00, 0x21 };
  uint8_t rtp[64];
  int size = build_rtp(rtp, sizeof(rtp), nal, (int)sizeof(nal), 0);
  CHECK(size > 0);
  /* Without the marker bit the access unit is still open. */
  CHECK(moonlink_h264_depack_feed(&depack, rtp, size) == 0);
  CHECK(depack.frame_size == 4 + (int)sizeof(nal));

  const uint8_t tail[] = { 0x41, 0x9A };
  size = build_rtp(rtp, sizeof(rtp), tail, (int)sizeof(tail), 1);
  CHECK(moonlink_h264_depack_feed(&depack, rtp, size) == 1);

  const uint8_t expected[] = {
    0x00, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84, 0x00, 0x21,
    0x00, 0x00, 0x00, 0x01, 0x41, 0x9A,
  };
  CHECK(annex_b_matches(depack.frame, depack.frame_size, expected, (int)sizeof(expected)));
  moonlink_h264_depack_free(&depack);
}

static void test_stap_a(void) {
  printf("[2] STAP-A aggregation\n");
  moonlink_h264_depack depack;
  moonlink_h264_depack_init(&depack);

  const uint8_t payload[] = {
    24,             /* STAP-A */
    0x00, 0x02,     /* first NAL size */
    0x67, 0x42,     /* SPS */
    0x00, 0x02,     /* second NAL size */
    0x68, 0xCE,     /* PPS */
  };
  uint8_t rtp[64];
  int size = build_rtp(rtp, sizeof(rtp), payload, (int)sizeof(payload), 1);
  CHECK(moonlink_h264_depack_feed(&depack, rtp, size) == 1);

  const uint8_t expected[] = {
    0x00, 0x00, 0x00, 0x01, 0x67, 0x42,
    0x00, 0x00, 0x00, 0x01, 0x68, 0xCE,
  };
  CHECK(annex_b_matches(depack.frame, depack.frame_size, expected, (int)sizeof(expected)));
  moonlink_h264_depack_free(&depack);
}

static void test_fu_a(void) {
  printf("[3] FU-A fragmentation\n");
  moonlink_h264_depack depack;
  moonlink_h264_depack_init(&depack);

  /* NAL header 0x65 split across three fragments. */
  const uint8_t frag1[] = { 0x7C, 0x85, 0xAA, 0xBB };
  const uint8_t frag2[] = { 0x7C, 0x05, 0xCC, 0xDD };
  const uint8_t frag3[] = { 0x7C, 0x45, 0xEE };

  uint8_t rtp[64];
  int size = build_rtp(rtp, sizeof(rtp), frag1, (int)sizeof(frag1), 0);
  CHECK(moonlink_h264_depack_feed(&depack, rtp, size) == 0);

  size = build_rtp(rtp, sizeof(rtp), frag2, (int)sizeof(frag2), 0);
  CHECK(moonlink_h264_depack_feed(&depack, rtp, size) == 0);

  size = build_rtp(rtp, sizeof(rtp), frag3, (int)sizeof(frag3), 1);
  CHECK(moonlink_h264_depack_feed(&depack, rtp, size) == 1);

  const uint8_t expected[] = {
    0x00, 0x00, 0x00, 0x01,
    0x65,             /* reconstructed NAL header: 0x60 | type 5 */
    0xAA, 0xBB, 0xCC, 0xDD, 0xEE,
  };
  CHECK(annex_b_matches(depack.frame, depack.frame_size, expected, (int)sizeof(expected)));
  moonlink_h264_depack_free(&depack);
}

static void test_header_extension_and_padding(void) {
  printf("[4] CSRC, extension and padding\n");
  moonlink_h264_depack depack;
  moonlink_h264_depack_init(&depack);

  uint8_t packet[64];
  memset(packet, 0, sizeof(packet));
  packet[0] = 0x80 | 0x20 | 0x10 | 0x01; /* padding, extension, one CSRC */
  packet[1] = 0x80 | 96;
  /* 12-byte header, then 4-byte CSRC, then 4-byte extension header. */
  packet[12] = 0xDE;
  packet[13] = 0xAD;
  packet[14] = 0xBE;
  packet[15] = 0xEF;
  packet[16] = 0xBE;
  packet[17] = 0xDE;
  packet[18] = 0x00;
  packet[19] = 0x00; /* zero extension words */
  packet[20] = 0x41; /* NAL header: non-IDR slice */
  packet[21] = 0x9A;
  packet[22] = 0x00; /* padding bytes */
  packet[23] = 0x02; /* two padding bytes */

  CHECK(moonlink_h264_depack_feed(&depack, packet, 24) == 1);
  const uint8_t expected[] = { 0x00, 0x00, 0x00, 0x01, 0x41, 0x9A };
  CHECK(annex_b_matches(depack.frame, depack.frame_size, expected, (int)sizeof(expected)));
  moonlink_h264_depack_free(&depack);
}

static void test_rejects_malformed(void) {
  printf("[5] malformed input\n");
  moonlink_h264_depack depack;
  moonlink_h264_depack_init(&depack);

  const uint8_t short_packet[4] = { 0x80, 0x60, 0x00, 0x01 };
  CHECK(moonlink_h264_depack_feed(&depack, short_packet, 4) < 0);

  uint8_t rtp[64];
  const uint8_t fu_b[] = { 0x7D, 0x85, 0xAA };
  int size = build_rtp(rtp, sizeof(rtp), fu_b, (int)sizeof(fu_b), 1);
  CHECK(moonlink_h264_depack_feed(&depack, rtp, size) < 0);

  /* An orphan FU-A continuation must be ignored, not appended. */
  const uint8_t orphan[] = { 0x7C, 0x05, 0xCC };
  size = build_rtp(rtp, sizeof(rtp), orphan, (int)sizeof(orphan), 0);
  CHECK(moonlink_h264_depack_feed(&depack, rtp, size) == 0);
  CHECK(depack.frame_size == 0);
  moonlink_h264_depack_free(&depack);
}

static void test_multi_nal_frame(void) {
  printf("[6] frame with several single-NAL packets\n");
  moonlink_h264_depack depack;
  moonlink_h264_depack_init(&depack);

  uint8_t rtp[64];
  const uint8_t aud[] = { 0x09, 0x10 };
  int size = build_rtp(rtp, sizeof(rtp), aud, (int)sizeof(aud), 0);
  CHECK(moonlink_h264_depack_feed(&depack, rtp, size) == 0);

  const uint8_t slice[] = { 0x41, 0x9A, 0x22 };
  size = build_rtp(rtp, sizeof(rtp), slice, (int)sizeof(slice), 1);
  CHECK(moonlink_h264_depack_feed(&depack, rtp, size) == 1);

  const uint8_t expected[] = {
    0x00, 0x00, 0x00, 0x01, 0x09, 0x10,
    0x00, 0x00, 0x00, 0x01, 0x41, 0x9A, 0x22,
  };
  CHECK(annex_b_matches(depack.frame, depack.frame_size, expected, (int)sizeof(expected)));

  moonlink_h264_depack_reset_frame(&depack);
  CHECK(depack.frame_size == 0);
  moonlink_h264_depack_free(&depack);
}

int main(void) {
  printf("=== H.264 depacketizer tests ===\n");
  test_single_nal();
  test_stap_a();
  test_fu_a();
  test_header_extension_and_padding();
  test_rejects_malformed();
  test_multi_nal_frame();

  if (g_failures == 0) {
    printf("=== ALL DEPACKETIZER TESTS PASSED ===\n");
    return 0;
  }
  printf("=== %d FAILURE(S) ===\n", g_failures);
  return 1;
}
