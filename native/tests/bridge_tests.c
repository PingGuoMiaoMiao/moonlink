#include "moonlink_native_bridge.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define COBJMACROS
#include <mfapi.h>
#include <mftransform.h>

HRESULT moonlink_native_create_mft_output_buffer(
  const MFT_OUTPUT_STREAM_INFO *stream_info,
  IMFMediaBuffer **buffer
);
HRESULT moonlink_native_shutdown_activation(IMFActivate *activation);

typedef struct moonlink_test_activation {
  IMFActivate activation;
  ULONG shutdown_call_count;
} moonlink_test_activation;

static HRESULT STDMETHODCALLTYPE moonlink_test_shutdown_activation(
  IMFActivate *activation
) {
  moonlink_test_activation *test_activation =
    (moonlink_test_activation *)activation;
  test_activation->shutdown_call_count += 1;
  return S_OK;
}

int main(void) {
  int failures = 0;

  /* ── ABI version ─────────────────────────────────────────────────────── */
  printf("ABI version: %d (expected %d)\n",
    moonlink_native_bridge_abi_version(), MOONLINK_NATIVE_ABI_VERSION);
  if (moonlink_native_bridge_abi_version() != MOONLINK_NATIVE_ABI_VERSION) {
    fprintf(stderr, "FAIL: ABI version mismatch\n");
    failures += 1;
  }

  /* ── Display count ───────────────────────────────────────────────────── */
  const int display_count = moonlink_native_display_count();
  printf("display_count=%d\n", display_count);
  if (display_count <= 0) {
    fprintf(stderr, "WARN: no displays (headless environment?)\n");
  }

  /* ── v1 probes (soft checks) ─────────────────────────────────────────── */
  const moonlink_native_capture_probe_result dup_probe =
    moonlink_native_desktop_duplication_probe();
  printf("desktop_duplication_probe=%d\n", (int)dup_probe);
  if (dup_probe != MOONLINK_NATIVE_CAPTURE_READY) {
    fprintf(stderr, "WARN: desktop duplication unavailable (%d)\n", (int)dup_probe);
  }

  const moonlink_native_capture_probe_result frame_probe =
    moonlink_native_desktop_frame_acquire_probe();
  printf("desktop_frame_acquire_probe=%d\n", (int)frame_probe);

  const moonlink_native_capture_probe_result texture_probe =
    moonlink_native_desktop_texture_probe();
  printf("desktop_texture_probe=%d\n", (int)texture_probe);

  const moonlink_native_viewer_probe_result viewer_probe =
    moonlink_native_d3d_viewer_probe();
  printf("d3d_viewer_probe=%d\n", (int)viewer_probe);
  if (viewer_probe != MOONLINK_NATIVE_VIEWER_READY) {
    fprintf(stderr, "WARN: D3D viewer probe failed (%d)\n", (int)viewer_probe);
  }

  const int sw_available = moonlink_native_h264_software_available();
  const int sw_encoded = moonlink_native_h264_software_encode_probe();
  const int hw_available = moonlink_native_h264_hardware_available();
  printf("h264: software_available=%d software_encoded=%d hardware_available=%d\n",
    sw_available, sw_encoded, hw_available);
  if (!sw_available || !sw_encoded) {
    fprintf(stderr, "FAIL: software H.264 encoding unavailable\n");
    failures += 1;
  }

  /* ── MFT buffer alignment ────────────────────────────────────────────── */
  if (SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_FULL))) {
    MFT_OUTPUT_STREAM_INFO stream_info = { 0, 64, MF_64_BYTE_ALIGNMENT };
    IMFMediaBuffer *output_buffer = NULL;
    if (SUCCEEDED(moonlink_native_create_mft_output_buffer(&stream_info, &output_buffer))) {
      BYTE *output_data = NULL;
      if (SUCCEEDED(IMFMediaBuffer_Lock(output_buffer, &output_data, NULL, NULL))) {
        if (((uintptr_t)output_data & 63u) != 0) {
          fprintf(stderr, "FAIL: MFT buffer not 64-byte aligned\n");
          failures += 1;
        }
        IMFMediaBuffer_Unlock(output_buffer);
      }
      IMFMediaBuffer_Release(output_buffer);
    }
    MFShutdown();
  }

  /* ── ShutdownObject helper ───────────────────────────────────────────── */
  IMFActivateVtbl activation_vtable = { 0 };
  activation_vtable.ShutdownObject = moonlink_test_shutdown_activation;
  moonlink_test_activation test_activation = { 0 };
  test_activation.activation.lpVtbl = &activation_vtable;
  if (FAILED(moonlink_native_shutdown_activation(&test_activation.activation))) {
    fprintf(stderr, "FAIL: shutdown_activation\n");
    failures += 1;
  }
  if (test_activation.shutdown_call_count != 1) {
    fprintf(stderr, "FAIL: shutdown_call_count=%lu\n", test_activation.shutdown_call_count);
    failures += 1;
  }

  /* ── Input gate ──────────────────────────────────────────────────────── */
  if (moonlink_native_input_allowed(MOONLINK_NATIVE_AWAITING_APPROVAL) != 0) {
    fprintf(stderr, "FAIL: input allowed in AWAITING_APPROVAL\n");
    failures += 1;
  }
  if (moonlink_native_input_allowed(MOONLINK_NATIVE_CONNECTED) != 1) {
    fprintf(stderr, "FAIL: input not allowed in CONNECTED\n");
    failures += 1;
  }
  if (moonlink_native_input_allowed(MOONLINK_NATIVE_DEGRADED) != 1) {
    fprintf(stderr, "FAIL: input not allowed in DEGRADED\n");
    failures += 1;
  }
  if (moonlink_native_input_allowed(MOONLINK_NATIVE_ENDED) != 0) {
    fprintf(stderr, "FAIL: input allowed in ENDED\n");
    failures += 1;
  }

  /* ── ABI v2 session handle tests ─────────────────────────────────────── */
  int session_failures = 0;

  /* Capture session (only if desktop duplication is available). */
  printf("testing capture session...\n");
  if (dup_probe != MOONLINK_NATIVE_CAPTURE_READY) {
    printf("SKIP: capture (desktop duplication unavailable)\n");
  } else {
    int capture = moonlink_capture_create_ffi(0, 1280, 720);
    if (capture <= 0) {
      fprintf(stderr, "FAIL: capture_create returned %d\n", capture);
      session_failures += 1;
    } else {
      int frame_result = moonlink_capture_next_frame_ffi(capture, 2000);
      if (frame_result == MOONLINK_RESULT_OK) {
        int w = moonlink_capture_frame_width_ffi(capture);
        int h = moonlink_capture_frame_height_ffi(capture);
        if (w <= 0 || h <= 0) {
          fprintf(stderr, "FAIL: capture frame dims %dx%d\n", w, h);
          session_failures += 1;
        } else {
          int nv12_size = w * h * 3 / 2;
          unsigned char *nv12_buf = (unsigned char *)malloc(nv12_size);
          if (nv12_buf != NULL) {
            int copied = moonlink_capture_frame_copy_nv12_ffi(capture, nv12_buf, nv12_size);
            if (copied != nv12_size) {
              fprintf(stderr, "FAIL: nv12 copy %d != %d\n", copied, nv12_size);
              session_failures += 1;
            }
            free(nv12_buf);
          }
          printf("capture: %dx%d frame acquired\n", w, h);
        }
      } else {
        printf("capture: next_frame result=%d (may be timeout)\n", frame_result);
      }
      if (moonlink_capture_stop_ffi(capture) != MOONLINK_RESULT_OK) {
        fprintf(stderr, "FAIL: capture_stop\n");
        session_failures += 1;
      }
      if (moonlink_capture_destroy_ffi(capture) != MOONLINK_RESULT_OK) {
        fprintf(stderr, "FAIL: capture_destroy\n");
        session_failures += 1;
      }
      if (moonlink_capture_destroy_ffi(capture) != MOONLINK_RESULT_INVALID_HANDLE) {
        fprintf(stderr, "FAIL: double destroy did not return INVALID_HANDLE\n");
        session_failures += 1;
      }
    }
  }

  /* Encoder session. */
  printf("testing encoder session...\n");
  int encoder = moonlink_encoder_create_ffi(1280, 720, 30, 2000000, 0);
  if (encoder <= 0) {
    fprintf(stderr, "FAIL: encoder_create returned %d\n", encoder);
    session_failures += 1;
  } else {
    if (moonlink_encoder_stop_ffi(encoder) != MOONLINK_RESULT_OK) {
      fprintf(stderr, "FAIL: encoder_stop\n");
      session_failures += 1;
    }
    if (moonlink_encoder_destroy_ffi(encoder) != MOONLINK_RESULT_OK) {
      fprintf(stderr, "FAIL: encoder_destroy\n");
      session_failures += 1;
    }
  }

  /* Decoder session. */
  printf("testing decoder session...\n");
  int decoder = moonlink_decoder_create_ffi(1280, 720);
  if (decoder <= 0) {
    fprintf(stderr, "FAIL: decoder_create returned %d\n", decoder);
    session_failures += 1;
  } else {
    if (moonlink_decoder_stop_ffi(decoder) != MOONLINK_RESULT_OK) {
      fprintf(stderr, "FAIL: decoder_stop\n");
      session_failures += 1;
    }
    if (moonlink_decoder_destroy_ffi(decoder) != MOONLINK_RESULT_OK) {
      fprintf(stderr, "FAIL: decoder_destroy\n");
      session_failures += 1;
    }
  }

  /* Viewer session. */
  printf("testing viewer session...\n");
  if (viewer_probe != MOONLINK_NATIVE_VIEWER_READY) {
    printf("SKIP: viewer (D3D viewer probe failed)\n");
  } else {
    int viewer = moonlink_viewer_create_ffi(1280, 720);
    if (viewer <= 0) {
      fprintf(stderr, "FAIL: viewer_create returned %d\n", viewer);
      session_failures += 1;
    } else {
      moonlink_viewer_pump_messages_ffi(viewer);
      if (moonlink_viewer_stop_ffi(viewer) != MOONLINK_RESULT_OK) {
        fprintf(stderr, "FAIL: viewer_stop\n");
        session_failures += 1;
      }
      if (moonlink_viewer_destroy_ffi(viewer) != MOONLINK_RESULT_OK) {
        fprintf(stderr, "FAIL: viewer_destroy\n");
        session_failures += 1;
      }
    }
  }

  /* Input session. */
  printf("testing input session...\n");
  int input = moonlink_input_create_ffi();
  if (input <= 0) {
    fprintf(stderr, "FAIL: input_create returned %d\n", input);
    session_failures += 1;
  } else {
    if (moonlink_input_stop_ffi(input) != MOONLINK_RESULT_OK) {
      fprintf(stderr, "FAIL: input_stop\n");
      session_failures += 1;
    }
    if (moonlink_input_destroy_ffi(input) != MOONLINK_RESULT_OK) {
      fprintf(stderr, "FAIL: input_destroy\n");
      session_failures += 1;
    }
  }

  failures += session_failures;
  printf(
    "\n=== RESULT: displays=%d h264_hw=%d abi=v%d failures=%d ===\n",
    display_count, hw_available, MOONLINK_NATIVE_ABI_VERSION, failures
  );
  return failures > 0 ? 1 : 0;
}
