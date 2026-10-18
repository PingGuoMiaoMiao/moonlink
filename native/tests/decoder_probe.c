/*
 * decoder_probe.c — Enumerate the H.264 decoder MFTs available on this host.
 *
 * Diagnostic helper: the native bridge must pick a decoder that actually emits
 * samples without a D3D11 device manager, so this prints every candidate and
 * the output-stream contract it advertises.
 */

#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mfapi.h>
#include <mftransform.h>
#include <mferror.h>
#include <stdio.h>

int main(void) {
  HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
  if (FAILED(hr)) {
    printf("MFStartup failed 0x%08lX\n", (unsigned long)hr);
    return 1;
  }

  MFT_REGISTER_TYPE_INFO input_info = { MFMediaType_Video, MFVideoFormat_H264 };
  IMFActivate **activations = NULL;
  UINT32 count = 0;

  hr = MFTEnumEx(
    MFT_CATEGORY_VIDEO_DECODER,
    MFT_ENUM_FLAG_ALL,
    &input_info,
    NULL,
    &activations,
    &count
  );
  printf("enum hr=0x%08lX count=%lu\n", (unsigned long)hr, (unsigned long)count);

  for (UINT32 i = 0; i < count; i += 1) {
    WCHAR name[256] = { 0 };
    UINT32 name_len = 0;
    IMFActivate_GetString(activations[i], &MFT_FRIENDLY_NAME_Attribute, name, 256, &name_len);

    UINT32 flags = 0;
    IMFActivate_GetUINT32(activations[i], &MFT_TRANSFORM_CLSID_Attribute, &flags);
    UINT32 async_flag = 0;
    IMFActivate_GetUINT32(activations[i], &MF_TRANSFORM_ASYNC, &async_flag);

    IMFTransform *transform = NULL;
    HRESULT activate_hr = IMFActivate_ActivateObject(
      activations[i], &IID_IMFTransform, (void **)&transform);

    printf("[%lu] %ls (clsid_flag=%lu async=%lu activate=0x%08lX)\n",
           (unsigned long)i, name, (unsigned long)flags,
           (unsigned long)async_flag, (unsigned long)activate_hr);

    if (SUCCEEDED(activate_hr)) {
      MFT_OUTPUT_STREAM_INFO info = { 0 };
      if (SUCCEEDED(IMFTransform_GetOutputStreamInfo(transform, 0, &info))) {
        printf("     output flags=0x%lX cbSize=%lu\n",
               (unsigned long)info.dwFlags, (unsigned long)info.cbSize);
      }
      IMFTransform_Release(transform);
    }
    IMFActivate_Release(activations[i]);
  }

  if (activations != NULL) {
    CoTaskMemFree(activations);
  }
  MFShutdown();
  return 0;
}
