/*
 * transport_session.c - WebRTC transport session for MoonLink native bridge.
 *
 * Implements the moonlink_transport_* API declared in moonlink_native_bridge.h
 * using libdatachannel C interface (rtc/rtc.h).
 */

#include <rtc/rtc.h>

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sessions_internal.h"

/* One-shot library initialisation guard */
static volatile LONG g_rtc_initialised = 0;

/* ════════════════════════════════════════════════════════════════════════════
   Static helper: ensure rtcInitLogger + rtcPreload run exactly once
   ════════════════════════════════════════════════════════════════════════════ */

static void moonlink_ensure_rtc_init(void)
{
    if (InterlockedCompareExchange(&g_rtc_initialised, 1, 0) == 0) {
        rtcInitLogger(RTC_LOG_WARNING, NULL);
        rtcPreload();
    }
}

/* ════════════════════════════════════════════════════════════════════════════
   Forward declarations of static rtc callbacks
   ════════════════════════════════════════════════════════════════════════════ */

static void RTC_API on_state_change(int pc, rtcState state, void *ptr);
static void RTC_API on_description(int pc, const char *sdp, const char *type, void *ptr);
static void RTC_API on_candidate(int pc, const char *candidate, const char *mid, void *ptr);
static void RTC_API on_data_channel(int pc, int dc, void *ptr);
static void RTC_API on_track(int pc, int tr, void *ptr);
static void RTC_API on_control_open(int id, void *ptr);
static void RTC_API on_control_message(int id, const char *message, int size, void *ptr);
static void RTC_API on_video_message(int id, const char *message, int size, void *ptr);

/* ════════════════════════════════════════════════════════════════════════════
   Callback implementations
   ════════════════════════════════════════════════════════════════════════════ */

/*
 * on_state_change — update the connected flag and forward to user callback.
 */
static void RTC_API on_state_change(int pc, rtcState state, void *ptr)
{
    (void)pc;
    struct moonlink_transport_session *session =
        (struct moonlink_transport_session *)ptr;

    if (state == RTC_CONNECTED) {
        InterlockedExchange(&session->connected, 1);
    } else if (state == RTC_DISCONNECTED || state == RTC_FAILED ||
               state == RTC_CLOSED) {
        InterlockedExchange(&session->connected, 0);
    }

    EnterCriticalSection(&session->callback_lock);
    moonlink_transport_state_callback cb   = session->state_cb;
    void                             *ud  = session->state_cb_data;
    LeaveCriticalSection(&session->callback_lock);

    if (cb) {
        int is_connected = (state == RTC_CONNECTED) ? 1 : 0;
        cb(is_connected, ud);
    }
}

/*
 * on_description — auto-set remote description on the other peer.
 * In production this would be exchanged out-of-band; kept here for loopback
 * testing convenience.  For real LAN usage the caller uses
 * moonlink_transport_get_local_description / set_remote_description instead.
 */
static void RTC_API on_description(int pc, const char *sdp, const char *type, void *ptr)
{
    (void)pc;
    (void)sdp;
    (void)type;
    (void)ptr;
    /* Intentionally empty for out-of-band signaling.
     * The application retrieves the local description via
     * moonlink_transport_get_local_description() and forwards it manually. */
}

/*
 * on_candidate — auto-forward candidate for loopback testing.
 * Same rationale as on_description above.
 */
static void RTC_API on_candidate(int pc, const char *candidate, const char *mid, void *ptr)
{
    (void)pc;
    (void)candidate;
    (void)mid;
    (void)ptr;
    /* Intentionally empty for out-of-band signaling.
     * The application retrieves candidates and forwards them manually. */
}

/*
 * on_data_channel — remote side received the "moonlink-control" channel.
 * Store the channel id and install open/message callbacks.
 */
static void RTC_API on_data_channel(int pc, int dc, void *ptr)
{
    (void)pc;
    struct moonlink_transport_session *session =
        (struct moonlink_transport_session *)ptr;

    session->control_channel = dc;

    rtcSetOpenCallback(dc, on_control_open);
    rtcSetMessageCallback(dc, on_control_message);
    rtcSetUserPointer(dc, session);
}

/*
 * on_track — remote side received a media track.
 * Store the track id and install a message callback that delivers raw H.264.
 */
static void RTC_API on_track(int pc, int tr, void *ptr)
{
    (void)pc;
    struct moonlink_transport_session *session =
        (struct moonlink_transport_session *)ptr;

    session->video_track = tr;

    rtcSetMessageCallback(tr, on_video_message);
    rtcSetUserPointer(tr, session);
}

/*
 * on_control_open — the control data channel is now open.
 * We rely on on_state_change for the high-level connected flag; this is
 * available for additional bookkeeping if needed later.
 */
static void RTC_API on_control_open(int id, void *ptr)
{
    (void)id;
    (void)ptr;
    /* Ready to send/receive control messages. */
}

/*
 * on_control_message — deliver a control-channel message to the user.
 *
 * Media never travels on this channel: video is carried by the negotiated
 * H.264 track and arrives through on_video_message as complete access units.
 * Keeping the data channel free of media avoids SCTP back-pressure failures
 * and leaves the channel for control events.
 */
static void RTC_API on_control_message(int id, const char *message, int size, void *ptr)
{
    (void)id;
    struct moonlink_transport_session *session =
        (struct moonlink_transport_session *)ptr;

    if (size <= 0 || !message) {
        return;
    }
    EnterCriticalSection(&session->callback_lock);
    moonlink_transport_control_callback cb  = session->control_cb;
    void                               *ud = session->control_cb_data;
    LeaveCriticalSection(&session->callback_lock);

    if (cb) {
        cb(message, size, ud);
    }
}

/*
 * push_video_frame — queue one Annex-B access unit for the polling consumer.
 */
static void push_video_frame(
    struct moonlink_transport_session *session,
    const uint8_t *data,
    int size
) {
    if (!data || size <= 0 || size > VIDEO_RING_MAX_PACKET) {
        return;
    }
    LONG head = InterlockedCompareExchange(&session->video_ring_head, 0, 0);
    LONG tail = InterlockedCompareExchange(&session->video_ring_tail, 0, 0);
    LONG next_head = (head + 1) % VIDEO_RING_CAPACITY;
    if (next_head == tail) {
        return; /* consumer is behind: drop this frame */
    }
    uint8_t *pkt = (uint8_t *)malloc((size_t)size);
    if (!pkt) {
        return;
    }
    memcpy(pkt, data, (size_t)size);
    session->video_ring[head] = pkt;
    session->video_ring_size[head] = size;
    InterlockedExchange(&session->video_ring_head, next_head);
}

/*
 * on_video_message — receive one RTP packet from the H.264 track.
 *
 * The C API of libdatachannel provides packetizers but no depacketizer, so the
 * callback is handed raw RTP. The depacketizer reassembles Annex-B access
 * units, which is what the Media Foundation decoder expects.
 */
static void RTC_API on_video_message(int id, const char *message, int size, void *ptr)
{
    (void)id;
    struct moonlink_transport_session *session =
        (struct moonlink_transport_session *)ptr;

    if (!message || size <= 0) {
        return;
    }

    int complete = moonlink_h264_depack_feed(
        &session->depack, (const uint8_t *)message, size);
    if (complete < 0) {
        /* Malformed or unsupported packet: drop the partial access unit so the
           next keyframe can resynchronise. */
        moonlink_h264_depack_reset_frame(&session->depack);
        return;
    }
    if (complete == 0) {
        return;
    }

    push_video_frame(session, session->depack.frame, session->depack.frame_size);

    EnterCriticalSection(&session->callback_lock);
    moonlink_transport_video_callback cb  = session->video_cb;
    void                             *ud = session->video_cb_data;
    LeaveCriticalSection(&session->callback_lock);

    if (cb && session->depack.frame_size > 0) {
        cb(session->depack.frame, session->depack.frame_size, ud);
    }

    moonlink_h264_depack_reset_frame(&session->depack);
}

/* ════════════════════════════════════════════════════════════════════════════
   Public API — creation & destruction
   ════════════════════════════════════════════════════════════════════════════ */

moonlink_native_result moonlink_transport_create(
    const moonlink_transport_config *config,
    moonlink_transport_handle       *out_handle)
{
    if (!config || !out_handle) {
        return MOONLINK_RESULT_INVALID_HANDLE;
    }

    moonlink_ensure_rtc_init();

    struct moonlink_transport_session *session =
        (struct moonlink_transport_session *)calloc(1, sizeof(*session));
    if (!session) {
        return MOONLINK_RESULT_OUT_OF_MEMORY;
    }

    session->peer_connection = -1;
    session->video_track     = -1;
    session->control_channel = -1;
    session->role            = config->role;

    InitializeCriticalSection(&session->callback_lock);
    moonlink_h264_depack_init(&session->depack);

    /* ── Create PeerConnection ─────────────────────────────────────────── */
    rtcConfiguration rtc_config = { 0 };
    /* No ICE servers — direct LAN connectivity only.
       Disable auto-negotiation since we exchange SDP via signaling. */
    rtc_config.disableAutoNegotiation = true;
    /* RTP media must fit the path MTU: libdatachannel derives the H.264
       fragment size from the default MTU, and an oversized MTU makes the
       ICE layer hand datagrams to the socket that cannot be sent. */
    rtc_config.mtu = RTC_DEFAULT_MTU;
    rtc_config.maxMessageSize = 65536; /* SCTP control messages stay whole */
    rtc_config.bindAddress = config->bind_address;

    int pc = rtcCreatePeerConnection(&rtc_config);
    if (pc < 0) {
        DeleteCriticalSection(&session->callback_lock);
        free(session);
        return MOONLINK_RESULT_TRANSPORT_UNAVAILABLE;
    }
    session->peer_connection = pc;

    /* Store session pointer so rtc callbacks can retrieve it. */
    rtcSetUserPointer(pc, session);

    /* ── Install peer-connection-level callbacks ───────────────────────── */
    rtcSetStateChangeCallback(pc, on_state_change);
    rtcSetLocalDescriptionCallback(pc, on_description);
    rtcSetLocalCandidateCallback(pc, on_candidate);
    rtcSetDataChannelCallback(pc, on_data_channel);
    rtcSetTrackCallback(pc, on_track);

    /* ── Host role: create outgoing resources ──────────────────────────── */
    if (config->role == MOONLINK_TRANSPORT_HOST) {
        /* Control data channel */
        rtcDataChannelInit dc_init = { 0 };
        dc_init.protocol = "moonlink-control";

        int dc = rtcCreateDataChannelEx(pc, "moonlink-control", &dc_init);
        if (dc < 0) {
            rtcDeletePeerConnection(pc);
            DeleteCriticalSection(&session->callback_lock);
            free(session);
            return MOONLINK_RESULT_TRANSPORT_UNAVAILABLE;
        }
        session->control_channel = dc;
        rtcSetUserPointer(dc, session);
        rtcSetOpenCallback(dc, on_control_open);
        rtcSetMessageCallback(dc, on_control_message);

        /* H.264 video track (sendonly) */
        rtcTrackInit track_init = { 0 };
        track_init.direction   = RTC_DIRECTION_SENDONLY;
        track_init.codec       = RTC_CODEC_H264;
        track_init.payloadType = 96;
        track_init.ssrc        = 1;
        track_init.mid         = "video";

        int tr = rtcAddTrackEx(pc, &track_init);
        if (tr < 0) {
            rtcDeleteDataChannel(dc);
            rtcDeletePeerConnection(pc);
            DeleteCriticalSection(&session->callback_lock);
            free(session);
            return MOONLINK_RESULT_TRANSPORT_UNAVAILABLE;
        }
        session->video_track = tr;
        rtcSetUserPointer(tr, session);

        /* Configure H.264 packetizer. The RTP configuration requires a CNAME,
           so it must be provided or the packetizer is never installed and the
           raw access unit is handed to SRTP as a single oversized datagram. */
        rtcPacketizerInit pkt_init = { 0 };
        pkt_init.ssrc          = 1;
        pkt_init.cname         = "moonlink";
        pkt_init.payloadType   = 96;
        pkt_init.clockRate     = 90000;  /* standard H.264 clock rate */
        pkt_init.nalSeparator  = RTC_NAL_SEPARATOR_START_SEQUENCE;
        pkt_init.maxFragmentSize = RTC_DEFAULT_MAX_FRAGMENT_SIZE;

        int pkt_rc = rtcSetH264Packetizer(tr, &pkt_init);
        if (pkt_rc != RTC_ERR_SUCCESS) {
            rtcDeleteTrack(tr);
            rtcDeleteDataChannel(dc);
            rtcDeletePeerConnection(pc);
            DeleteCriticalSection(&session->callback_lock);
            free(session);
            return MOONLINK_RESULT_TRANSPORT_UNAVAILABLE;
        }

        /* Chain RTCP receiving session for receiver reports / PLI */
        rtcChainRtcpReceivingSession(tr);
    }

    *out_handle = session;
    return MOONLINK_RESULT_OK;
}

/* ════════════════════════════════════════════════════════════════════════════
   Public API — callback setters
   ════════════════════════════════════════════════════════════════════════════ */

moonlink_native_result moonlink_transport_set_video_callback(
    moonlink_transport_handle        handle,
    moonlink_transport_video_callback cb,
    void                             *user_data)
{
    if (!handle) return MOONLINK_RESULT_INVALID_HANDLE;

    EnterCriticalSection(&handle->callback_lock);
    handle->video_cb      = cb;
    handle->video_cb_data = user_data;
    LeaveCriticalSection(&handle->callback_lock);

    return MOONLINK_RESULT_OK;
}

moonlink_native_result moonlink_transport_set_control_callback(
    moonlink_transport_handle          handle,
    moonlink_transport_control_callback cb,
    void                               *user_data)
{
    if (!handle) return MOONLINK_RESULT_INVALID_HANDLE;

    EnterCriticalSection(&handle->callback_lock);
    handle->control_cb      = cb;
    handle->control_cb_data = user_data;
    LeaveCriticalSection(&handle->callback_lock);

    return MOONLINK_RESULT_OK;
}

moonlink_native_result moonlink_transport_set_state_callback(
    moonlink_transport_handle        handle,
    moonlink_transport_state_callback cb,
    void                             *user_data)
{
    if (!handle) return MOONLINK_RESULT_INVALID_HANDLE;

    EnterCriticalSection(&handle->callback_lock);
    handle->state_cb      = cb;
    handle->state_cb_data = user_data;
    LeaveCriticalSection(&handle->callback_lock);

    return MOONLINK_RESULT_OK;
}

/* ════════════════════════════════════════════════════════════════════════════
   Public API — start / signaling
   ════════════════════════════════════════════════════════════════════════════ */

moonlink_native_result moonlink_transport_start(
    moonlink_transport_handle handle)
{
    if (!handle) return MOONLINK_RESULT_INVALID_HANDLE;

    /* With disableAutoNegotiation, we must explicitly set local description.
     * Host creates offer, Controller creates answer (after remote is set). */
    const char *type = (handle->role == MOONLINK_TRANSPORT_HOST) ? "offer" : "answer";
    int err = rtcSetLocalDescription(handle->peer_connection, type);
    if (err != RTC_ERR_SUCCESS) {
        return MOONLINK_RESULT_TRANSPORT_UNAVAILABLE;
    }

    return MOONLINK_RESULT_OK;
}

moonlink_native_result moonlink_transport_get_local_description(
    moonlink_transport_handle handle,
    char                     *buffer,
    int                       buffer_size,
    int                      *out_length)
{
    if (!handle || !out_length) return MOONLINK_RESULT_INVALID_HANDLE;

    /* First call with NULL buffer to query required size */
    int len = rtcGetLocalDescription(handle->peer_connection, buffer, buffer_size);
    if (len < 0) {
        return MOONLINK_RESULT_TRANSPORT_UNAVAILABLE;
    }

    *out_length = len;
    return MOONLINK_RESULT_OK;
}

moonlink_native_result moonlink_transport_set_remote_description(
    moonlink_transport_handle handle,
    const char               *sdp,
    const char               *type)
{
    if (!handle || !sdp) return MOONLINK_RESULT_INVALID_HANDLE;

    int err = rtcSetRemoteDescription(handle->peer_connection, sdp, type);
    if (err != RTC_ERR_SUCCESS) {
        return MOONLINK_RESULT_TRANSPORT_UNAVAILABLE;
    }

    return MOONLINK_RESULT_OK;
}

moonlink_native_result moonlink_transport_add_remote_candidate(
    moonlink_transport_handle handle,
    const char               *candidate,
    const char               *mid)
{
    if (!handle || !candidate) return MOONLINK_RESULT_INVALID_HANDLE;

    int err = rtcAddRemoteCandidate(handle->peer_connection, candidate, mid);
    if (err != RTC_ERR_SUCCESS) {
        return MOONLINK_RESULT_TRANSPORT_UNAVAILABLE;
    }

    return MOONLINK_RESULT_OK;
}

/* ════════════════════════════════════════════════════════════════════════════
   Public API — send
   ════════════════════════════════════════════════════════════════════════════ */

moonlink_native_result moonlink_transport_send_video(
    moonlink_transport_handle handle,
    const uint8_t            *data,
    int                       size)
{
    if (!handle || !data || size <= 0) return MOONLINK_RESULT_INVALID_HANDLE;

    /* Only send when connected */
    if (InterlockedCompareExchange(&handle->connected, 0, 0) == 0) {
        return MOONLINK_RESULT_TRANSPORT_UNAVAILABLE;
    }

    if (handle->video_track < 0) {
        return MOONLINK_RESULT_TRANSPORT_UNAVAILABLE;
    }

    /* Split Annex B H.264 stream into individual NAL units and send each.
     * This avoids exceeding UDP/datagram size limits. */
    const uint8_t *p = (const uint8_t *)data;
    const uint8_t *end = p + size;
    int sent_any = 0;

    while (p < end) {
        /* Find start code (0x000001 or 0x00000001) */
        const uint8_t *nal_start = NULL;
        const uint8_t *scan = p;
        while (scan < end - 2) {
            if (scan[0] == 0 && scan[1] == 0) {
                if (scan[2] == 1) {
                    nal_start = scan + 3;
                    break;
                } else if (scan < end - 3 && scan[2] == 0 && scan[3] == 1) {
                    nal_start = scan + 4;
                    break;
                }
            }
            scan++;
        }

        if (!nal_start) break;

        /* Find next start code to determine NAL unit length */
        const uint8_t *nal_end = end;
        scan = nal_start;
        while (scan < end - 2) {
            if (scan[0] == 0 && scan[1] == 0) {
                if (scan[2] == 1 || (scan < end - 3 && scan[2] == 0 && scan[3] == 1)) {
                    nal_end = scan;
                    break;
                }
            }
            scan++;
        }

        int nal_size = (int)(nal_end - nal_start);
        if (nal_size > 0) {
            /* Send with 4-byte length prefix for the packetizer */
            uint8_t *pkt = (uint8_t *)malloc(nal_size + 4);
            if (pkt) {
                pkt[0] = (uint8_t)((nal_size >> 24) & 0xFF);
                pkt[1] = (uint8_t)((nal_size >> 16) & 0xFF);
                pkt[2] = (uint8_t)((nal_size >> 8) & 0xFF);
                pkt[3] = (uint8_t)(nal_size & 0xFF);
                memcpy(pkt + 4, nal_start, nal_size);
                int err = rtcSendMessage(handle->video_track, (const char *)pkt, nal_size + 4);
                free(pkt);
                if (err == RTC_ERR_SUCCESS) sent_any = 1;
            }
        }
        p = nal_end;
    }

    return sent_any ? MOONLINK_RESULT_OK : MOONLINK_RESULT_TRANSPORT_UNAVAILABLE;
}

moonlink_native_result moonlink_transport_send_control(
    moonlink_transport_handle handle,
    const char               *message,
    int                       size)
{
    if (!handle || !message) return MOONLINK_RESULT_INVALID_HANDLE;

    if (handle->control_channel < 0) {
        return MOONLINK_RESULT_TRANSPORT_UNAVAILABLE;
    }

    int err = rtcSendMessage(handle->control_channel, message, size);
    if (err != RTC_ERR_SUCCESS) {
        return MOONLINK_RESULT_TRANSPORT_UNAVAILABLE;
    }

    return MOONLINK_RESULT_OK;
}

/* ════════════════════════════════════════════════════════════════════════════
   Public API — stop & destroy
   ════════════════════════════════════════════════════════════════════════════ */

moonlink_native_result moonlink_transport_stop(
    moonlink_transport_handle handle)
{
    if (!handle) return MOONLINK_RESULT_INVALID_HANDLE;

    /* Guard against double-stop */
    if (InterlockedExchange(&handle->stopped, 1) != 0) {
        return MOONLINK_RESULT_ALREADY_STOPPED;
    }

    InterlockedExchange(&handle->connected, 0);

    if (handle->peer_connection >= 0) {
        rtcClosePeerConnection(handle->peer_connection);
    }

    return MOONLINK_RESULT_OK;
}

moonlink_native_result moonlink_transport_destroy(
    moonlink_transport_handle handle)
{
    if (!handle) return MOONLINK_RESULT_INVALID_HANDLE;

    /* Ensure stopped before tearing down resources */
    if (InterlockedCompareExchange(&handle->stopped, 0, 0) == 0) {
        moonlink_transport_stop(handle);
    }

    /* Delete in reverse order of creation: track → channel → pc */
    if (handle->video_track >= 0) {
        rtcDeleteTrack(handle->video_track);
        handle->video_track = -1;
    }

    if (handle->control_channel >= 0) {
        rtcDeleteDataChannel(handle->control_channel);
        handle->control_channel = -1;
    }

    if (handle->peer_connection >= 0) {
    moonlink_h264_depack_free(&handle->depack);
    /* Free ring buffer. */
    for (int i = 0; i < VIDEO_RING_CAPACITY; i++) {
        if (handle->video_ring[i]) {
            free(handle->video_ring[i]);
            handle->video_ring[i] = NULL;
        }
    }
    rtcDeletePeerConnection(handle->peer_connection);
        handle->peer_connection = -1;
    }

    DeleteCriticalSection(&handle->callback_lock);
    free(handle);

    return MOONLINK_RESULT_OK;
}
