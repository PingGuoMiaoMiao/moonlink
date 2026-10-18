/*
 * h264_depacketizer.c — Minimal H.264 RTP depacketizer for MoonLink.
 *
 * See h264_depacketizer.h for the contract. The implementation is deliberately
 * small and allocation-light: buffers grow on demand and are reused for every
 * frame, so the steady-state receive path performs no allocation per packet.
 */

#include "h264_depacketizer.h"

#include <stdlib.h>
#include <string.h>

#define MOONLINK_NAL_TYPE_STAP_A 24
#define MOONLINK_NAL_TYPE_FU_A 28

static const uint8_t k_start_code[4] = { 0x00, 0x00, 0x00, 0x01 };

static int reserve(uint8_t **buffer, int *capacity, int needed) {
  if (needed <= *capacity) {
    return 1;
  }
  int target = (*capacity > 0) ? *capacity : 4096;
  while (target < needed) {
    target *= 2;
  }
  uint8_t *grown = (uint8_t *)realloc(*buffer, (size_t)target);
  if (grown == NULL) {
    return 0;
  }
  *buffer = grown;
  *capacity = target;
  return 1;
}

static int append(moonlink_h264_depack *depack, const uint8_t *data, int size) {
  if (size <= 0) {
    return 1;
  }
  int needed = depack->frame_size + size;
  if (!reserve(&depack->frame, &depack->frame_capacity, needed)) {
    return 0;
  }
  memcpy(depack->frame + depack->frame_size, data, (size_t)size);
  depack->frame_size = needed;
  return 1;
}

static int append_nal(moonlink_h264_depack *depack, const uint8_t *nal, int size) {
  if (size <= 0) {
    return 1;
  }
  if (!append(depack, k_start_code, 4)) {
    return 0;
  }
  return append(depack, nal, size);
}

static int append_fu(moonlink_h264_depack *depack, const uint8_t *data, int size) {
  if (size <= 0) {
    return 1;
  }
  int needed = depack->fu_size + size;
  if (!reserve(&depack->fu, &depack->fu_capacity, needed)) {
    return 0;
  }
  memcpy(depack->fu + depack->fu_size, data, (size_t)size);
  depack->fu_size = needed;
  return 1;
}

void moonlink_h264_depack_init(moonlink_h264_depack *depack) {
  if (depack == NULL) {
    return;
  }
  memset(depack, 0, sizeof(*depack));
}

void moonlink_h264_depack_free(moonlink_h264_depack *depack) {
  if (depack == NULL) {
    return;
  }
  free(depack->frame);
  free(depack->fu);
  memset(depack, 0, sizeof(*depack));
}

void moonlink_h264_depack_reset_frame(moonlink_h264_depack *depack) {
  if (depack == NULL) {
    return;
  }
  depack->frame_size = 0;
}

int moonlink_h264_depack_feed(
  moonlink_h264_depack *depack,
  const uint8_t *packet,
  int size
) {
  if (depack == NULL || packet == NULL) {
    return -1;
  }
  if (size < 12) {
    return -1;
  }
  if ((packet[0] >> 6) != 2) {
    return -1; /* not RTP version 2 */
  }

  const int padding = (packet[0] >> 5) & 0x01;
  const int extension = (packet[0] >> 4) & 0x01;
  const int csrc_count = packet[0] & 0x0F;
  const int marker = (packet[1] >> 7) & 0x01;

  int offset = 12 + 4 * csrc_count;
  if (offset > size) {
    return -1;
  }
  if (extension) {
    if (offset + 4 > size) {
      return -1;
    }
    int extension_words = ((int)packet[offset + 2] << 8) | (int)packet[offset + 3];
    offset += 4 + 4 * extension_words;
    if (offset > size) {
      return -1;
    }
  }

  int payload_size = size - offset;
  if (padding) {
    int padding_size = packet[size - 1];
    if (padding_size <= 0 || padding_size > payload_size) {
      return -1;
    }
    payload_size -= padding_size;
  }
  if (payload_size <= 0) {
    return 0;
  }

  const uint8_t *payload = packet + offset;
  const int nal_type = payload[0] & 0x1F;

  if (nal_type == MOONLINK_NAL_TYPE_STAP_A) {
    int cursor = 1;
    while (cursor + 2 <= payload_size) {
      int nal_size = ((int)payload[cursor] << 8) | (int)payload[cursor + 1];
      cursor += 2;
      if (nal_size <= 0 || cursor + nal_size > payload_size) {
        return -1;
      }
      if (!append_nal(depack, payload + cursor, nal_size)) {
        return -1;
      }
      cursor += nal_size;
    }
  } else if (nal_type == MOONLINK_NAL_TYPE_FU_A) {
    if (payload_size < 3) {
      return -1;
    }
    const uint8_t fu_indicator = payload[0];
    const uint8_t fu_header = payload[1];
    const int start = (fu_header & 0x80) != 0;
    const int end = (fu_header & 0x40) != 0;
    const int inner_type = fu_header & 0x1F;

    if (start) {
      depack->fu_active = 1;
      depack->fu_nal_header = (fu_indicator & 0xE0) | inner_type;
      depack->fu_size = 0;
      if (!append_fu(depack, (const uint8_t *)&depack->fu_nal_header, 1)) {
        return -1;
      }
    } else if (!depack->fu_active) {
      return 0; /* orphan fragment: drop until a start fragment arrives */
    }

    if (!append_fu(depack, payload + 2, payload_size - 2)) {
      return -1;
    }

    if (end) {
      depack->fu_active = 0;
      if (!append_nal(depack, depack->fu, depack->fu_size)) {
        return -1;
      }
      depack->fu_size = 0;
    }
  } else if (nal_type >= 1 && nal_type <= 23) {
    if (!append_nal(depack, payload, payload_size)) {
      return -1;
    }
  } else {
    return -1; /* FU-B, STAP-B, MTAP and reserved types are unsupported */
  }

  if (marker && depack->frame_size > 0) {
    return 1;
  }
  return 0;
}
