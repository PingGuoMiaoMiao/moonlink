#include "moonlink_native_bridge.h"

int moonlink_native_bridge_abi_version(void) {
  return 1;
}

int moonlink_native_input_allowed(moonlink_native_session_state state) {
  return state == MOONLINK_NATIVE_CONNECTED || state == MOONLINK_NATIVE_DEGRADED;
}
