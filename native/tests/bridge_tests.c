#include "moonlink_native_bridge.h"

#include <assert.h>

int main(void) {
  assert(moonlink_native_bridge_abi_version() == 1);
  assert(moonlink_native_input_allowed(MOONLINK_NATIVE_AWAITING_APPROVAL) == 0);
  assert(moonlink_native_input_allowed(MOONLINK_NATIVE_CONNECTED) == 1);
  assert(moonlink_native_input_allowed(MOONLINK_NATIVE_DEGRADED) == 1);
  assert(moonlink_native_input_allowed(MOONLINK_NATIVE_ENDED) == 0);
  return 0;
}
