/*
 * h264_depacketizer.h — Minimal H.264 RTP depacketizer for MoonLink.
 *
 * libdatachannel exposes packetizers but no depacketizer through its C API, so
 * a received track delivers raw RTP packets to the message callback. This
 * module turns those packets back into Annex-B access units that a Media
 * Foundation H.264 decoder can consume.
 *
 * Supported payloads: single NAL unit, STAP-A, FU-A. FU-B and interleaved
 * payloads are rejected because MoonLink only ever negotiates plain H.264
 * packetization-mode 1 with its own sender.
 */

#ifndef MOONLINK_H264_DEPACKETIZER_H
#define MOONLINK_H264_DEPACKETIZER_H

#include <stdint.h>

typedef struct moonlink_h264_depack {
  /* Completed access unit, Annex-B (start codes between NAL units). */
  uint8_t *frame;
  int frame_size;
  int frame_capacity;
  /* In-progress FU-A fragment. */
  uint8_t *fu;
  int fu_size;
  int fu_capacity;
  int fu_active;
  int fu_nal_header;
} moonlink_h264_depack;

void moonlink_h264_depack_init(moonlink_h264_depack *depack);

void moonlink_h264_depack_free(moonlink_h264_depack *depack);

/*
 * Feed one RTP packet.
 *
 * Returns 1 when the packet completed an access unit: the caller reads
 * depack->frame / depack->frame_size and then calls
 * moonlink_h264_depack_reset_frame(). Returns 0 when more packets are needed
 * and a negative value when the packet is malformed or unsupported.
 */
int moonlink_h264_depack_feed(
  moonlink_h264_depack *depack,
  const uint8_t *packet,
  int size
);

void moonlink_h264_depack_reset_frame(moonlink_h264_depack *depack);

#endif /* MOONLINK_H264_DEPACKETIZER_H */
