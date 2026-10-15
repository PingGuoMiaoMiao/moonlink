#ifndef MOONLINK_NATIVE_BRIDGE_H
#define MOONLINK_NATIVE_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum moonlink_native_session_state {
  MOONLINK_NATIVE_AWAITING_APPROVAL = 0,
  MOONLINK_NATIVE_CONNECTED = 1,
  MOONLINK_NATIVE_DEGRADED = 2,
  MOONLINK_NATIVE_ENDED = 3,
} moonlink_native_session_state;

int moonlink_native_bridge_abi_version(void);
int moonlink_native_input_allowed(moonlink_native_session_state state);

#ifdef __cplusplus
}
#endif

#endif
