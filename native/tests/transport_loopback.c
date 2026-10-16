#include <rtc/rtc.h>

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <windows.h>

typedef struct moonlink_loopback_peer {
  int connection;
  int control_channel;
  int video_track;
  volatile LONG control_open;
  volatile LONG control_received;
  volatile LONG video_track_received;
  volatile LONG callback_failed;
  struct moonlink_loopback_peer *remote;
} moonlink_loopback_peer;

static void moonlink_record_callback_failure(moonlink_loopback_peer *peer) {
  InterlockedExchange(&peer->callback_failed, 1);
}

static bool moonlink_flag_is_set(volatile LONG *flag) {
  return InterlockedCompareExchange(flag, 0, 0) != 0;
}

static void RTC_API moonlink_description_callback(
  int connection,
  const char *sdp,
  const char *type,
  void *user_data
) {
  (void)connection;
  moonlink_loopback_peer *peer = (moonlink_loopback_peer *)user_data;
  if (rtcSetRemoteDescription(peer->remote->connection, sdp, type) != RTC_ERR_SUCCESS) {
    moonlink_record_callback_failure(peer);
  }
}

static void RTC_API moonlink_candidate_callback(
  int connection,
  const char *candidate,
  const char *mid,
  void *user_data
) {
  (void)connection;
  moonlink_loopback_peer *peer = (moonlink_loopback_peer *)user_data;
  if (rtcAddRemoteCandidate(peer->remote->connection, candidate, mid) != RTC_ERR_SUCCESS) {
    moonlink_record_callback_failure(peer);
  }
}

static void RTC_API moonlink_control_open_callback(int channel, void *user_data) {
  (void)channel;
  moonlink_loopback_peer *peer = (moonlink_loopback_peer *)user_data;
  InterlockedExchange(&peer->control_open, 1);
}

static void RTC_API moonlink_control_message_callback(
  int channel,
  const char *message,
  int size,
  void *user_data
) {
  (void)channel;
  moonlink_loopback_peer *peer = (moonlink_loopback_peer *)user_data;
  if (size == 12 && memcmp(message, "control-ping", 12) == 0) {
    InterlockedExchange(&peer->control_received, 1);
  }
}

static void RTC_API moonlink_data_channel_callback(
  int connection,
  int channel,
  void *user_data
) {
  (void)connection;
  moonlink_loopback_peer *peer = (moonlink_loopback_peer *)user_data;
  peer->control_channel = channel;
  if (rtcSetOpenCallback(channel, moonlink_control_open_callback) != RTC_ERR_SUCCESS ||
      rtcSetMessageCallback(channel, moonlink_control_message_callback) != RTC_ERR_SUCCESS) {
    moonlink_record_callback_failure(peer);
  }
}

static void RTC_API moonlink_track_callback(int connection, int track, void *user_data) {
  (void)connection;
  moonlink_loopback_peer *peer = (moonlink_loopback_peer *)user_data;
  peer->video_track = track;
  InterlockedExchange(&peer->video_track_received, 1);
}

static void moonlink_configure_peer(
  moonlink_loopback_peer *peer,
  bool disable_auto_negotiation
) {
  rtcConfiguration configuration = { 0 };
  configuration.disableAutoNegotiation = disable_auto_negotiation;
  peer->connection = rtcCreatePeerConnection(&configuration);
  assert(peer->connection >= 0);
  rtcSetUserPointer(peer->connection, peer);
  assert(rtcSetLocalDescriptionCallback(peer->connection, moonlink_description_callback) == RTC_ERR_SUCCESS);
  assert(rtcSetLocalCandidateCallback(peer->connection, moonlink_candidate_callback) == RTC_ERR_SUCCESS);
  assert(rtcSetDataChannelCallback(peer->connection, moonlink_data_channel_callback) == RTC_ERR_SUCCESS);
  assert(rtcSetTrackCallback(peer->connection, moonlink_track_callback) == RTC_ERR_SUCCESS);
}

static void moonlink_destroy_peer(moonlink_loopback_peer *peer) {
  if (peer->control_channel >= 0) {
    rtcDeleteDataChannel(peer->control_channel);
  }
  if (peer->video_track >= 0) {
    rtcDeleteTrack(peer->video_track);
  }
  if (peer->connection >= 0) {
    rtcDeletePeerConnection(peer->connection);
  }
}

int main(void) {
  moonlink_loopback_peer controller = { .connection = -1, .control_channel = -1, .video_track = -1 };
  moonlink_loopback_peer host = { .connection = -1, .control_channel = -1, .video_track = -1 };
  controller.remote = &host;
  host.remote = &controller;

  rtcInitLogger(RTC_LOG_WARNING, NULL);
  rtcPreload();
  moonlink_configure_peer(&controller, true);
  moonlink_configure_peer(&host, false);

  rtcDataChannelInit control_init = { 0 };
  control_init.protocol = "moonlink-control";
  controller.control_channel = rtcCreateDataChannelEx(
    controller.connection,
    "moonlink-control",
    &control_init
  );
  assert(controller.control_channel >= 0);
  assert(rtcSetOpenCallback(controller.control_channel, moonlink_control_open_callback) == RTC_ERR_SUCCESS);
  assert(rtcSetMessageCallback(controller.control_channel, moonlink_control_message_callback) == RTC_ERR_SUCCESS);

  rtcTrackInit video_init = { 0 };
  video_init.direction = RTC_DIRECTION_SENDONLY;
  video_init.codec = RTC_CODEC_H264;
  video_init.payloadType = 96;
  video_init.ssrc = 1;
  video_init.mid = "video";
  controller.video_track = rtcAddTrackEx(controller.connection, &video_init);
  assert(controller.video_track >= 0);
  assert(rtcSetLocalDescription(controller.connection, NULL) == RTC_ERR_SUCCESS);

  for (int attempt = 0; attempt < 1000; attempt += 1) {
    if (moonlink_flag_is_set(&controller.callback_failed) ||
        moonlink_flag_is_set(&host.callback_failed) ||
        (moonlink_flag_is_set(&controller.control_open) &&
         moonlink_flag_is_set(&host.control_open) &&
         moonlink_flag_is_set(&host.video_track_received))) {
      break;
    }
    Sleep(10);
  }
  assert(!moonlink_flag_is_set(&controller.callback_failed));
  assert(!moonlink_flag_is_set(&host.callback_failed));
  assert(moonlink_flag_is_set(&controller.control_open));
  assert(moonlink_flag_is_set(&host.control_open));
  assert(moonlink_flag_is_set(&host.video_track_received));
  assert(rtcSendMessage(controller.control_channel, "control-ping", 12) == RTC_ERR_SUCCESS);

  for (
    int attempt = 0;
    attempt < 1000 && !moonlink_flag_is_set(&host.control_received);
    attempt += 1
  ) {
    Sleep(10);
  }
  assert(moonlink_flag_is_set(&host.control_received));

  moonlink_destroy_peer(&controller);
  moonlink_destroy_peer(&host);
  rtcCleanup();
  return 0;
}
