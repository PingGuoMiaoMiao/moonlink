/*
 * signaling.c — TCP-based SDP signaling for MoonLink LAN direct connection.
 *
 * Protocol: 4-byte big-endian length prefix followed by UTF-8 payload.
 */

#include "sessions_internal.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "ws2_32.lib")

/* ── Winsock init guard ──────────────────────────────────────────────── */

static volatile LONG g_wsa_initialized = 0;

static void moonlink_ensure_wsa(void) {
  if (InterlockedCompareExchange(&g_wsa_initialized, 1, 0) == 0) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
  }
}

/* ── Handle tables ───────────────────────────────────────────────────── */

#define SIGNAL_TABLE_CAPACITY 32

typedef struct signal_table {
  void *slots[SIGNAL_TABLE_CAPACITY];
  CRITICAL_SECTION lock;
  BOOL initialized;
} signal_table;

static void signal_table_init(signal_table *t) {
  if (!t->initialized) {
    memset(t->slots, 0, sizeof(t->slots));
    InitializeCriticalSection(&t->lock);
    t->initialized = TRUE;
  }
}

static int signal_table_alloc(signal_table *t, void *ptr) {
  signal_table_init(t);
  EnterCriticalSection(&t->lock);
  for (int i = 1; i < SIGNAL_TABLE_CAPACITY; i++) {
    if (t->slots[i] == NULL) {
      t->slots[i] = ptr;
      LeaveCriticalSection(&t->lock);
      return i;
    }
  }
  LeaveCriticalSection(&t->lock);
  return MOONLINK_HANDLE_INVALID;
}

static void *signal_table_get(signal_table *t, int index) {
  if (index <= 0 || index >= SIGNAL_TABLE_CAPACITY) return NULL;
  signal_table_init(t);
  EnterCriticalSection(&t->lock);
  void *p = t->slots[index];
  LeaveCriticalSection(&t->lock);
  return p;
}

static void *signal_table_remove(signal_table *t, int index) {
  if (index <= 0 || index >= SIGNAL_TABLE_CAPACITY) return NULL;
  signal_table_init(t);
  EnterCriticalSection(&t->lock);
  void *p = t->slots[index];
  t->slots[index] = NULL;
  LeaveCriticalSection(&t->lock);
  return p;
}

/* ── Session structs ─────────────────────────────────────────────────── */

typedef struct signal_listener {
  SOCKET sock;
  BOOL stopped;
} signal_listener;

typedef struct signal_session {
  SOCKET sock;
  BOOL stopped;
} signal_session;

static signal_table listener_table = { 0 };
static signal_table session_table = { 0 };

/* ── Helpers ─────────────────────────────────────────────────────────── */

static int send_all(SOCKET s, const char *buf, int len) {
  int sent = 0;
  while (sent < len) {
    int n = send(s, buf + sent, len - sent, 0);
    if (n <= 0) return -1;
    sent += n;
  }
  return 0;
}

static int recv_all(SOCKET s, char *buf, int len) {
  int received = 0;
  while (received < len) {
    int n = recv(s, buf + received, len - received, 0);
    if (n <= 0) return -1;
    received += n;
  }
  return 0;
}

/* ── Public API ──────────────────────────────────────────────────────── */

int moonlink_signal_listen_ffi(int port) {
  moonlink_ensure_wsa();

  signal_listener *listener = (signal_listener *)calloc(1, sizeof(signal_listener));
  if (!listener) return (int)MOONLINK_RESULT_OUT_OF_MEMORY;

  listener->sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listener->sock == INVALID_SOCKET) {
    free(listener);
    return (int)MOONLINK_RESULT_TRANSPORT_UNAVAILABLE;
  }

  /* Allow address reuse. */
  BOOL optval = TRUE;
  setsockopt(listener->sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&optval, sizeof(optval));

  struct sockaddr_in addr = { 0 };
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons((u_short)port);

  if (bind(listener->sock, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
    closesocket(listener->sock);
    free(listener);
    return (int)MOONLINK_RESULT_TRANSPORT_UNAVAILABLE;
  }

  if (listen(listener->sock, 1) == SOCKET_ERROR) {
    closesocket(listener->sock);
    free(listener);
    return (int)MOONLINK_RESULT_TRANSPORT_UNAVAILABLE;
  }

  listener->stopped = FALSE;
  int slot = signal_table_alloc(&listener_table, listener);
  if (slot == MOONLINK_HANDLE_INVALID) {
    closesocket(listener->sock);
    free(listener);
    return (int)MOONLINK_RESULT_OUT_OF_MEMORY;
  }
  return slot;
}

int moonlink_signal_accept_ffi(int listener_handle) {
  signal_listener *listener = (signal_listener *)signal_table_get(&listener_table, listener_handle);
  if (!listener || listener->stopped) return (int)MOONLINK_RESULT_INVALID_HANDLE;

  struct sockaddr_in client_addr = { 0 };
  int addr_len = sizeof(client_addr);
  SOCKET client = accept(listener->sock, (struct sockaddr *)&client_addr, &addr_len);
  if (client == INVALID_SOCKET) {
    return (int)MOONLINK_RESULT_TRANSPORT_UNAVAILABLE;
  }

  signal_session *session = (signal_session *)calloc(1, sizeof(signal_session));
  if (!session) {
    closesocket(client);
    return (int)MOONLINK_RESULT_OUT_OF_MEMORY;
  }
  session->sock = client;
  session->stopped = FALSE;

  int slot = signal_table_alloc(&session_table, session);
  if (slot == MOONLINK_HANDLE_INVALID) {
    closesocket(client);
    free(session);
    return (int)MOONLINK_RESULT_OUT_OF_MEMORY;
  }
  return slot;
}

int moonlink_signal_connect_ffi(const char *address, int port) {
  if (!address) return (int)MOONLINK_RESULT_INVALID_HANDLE;
  moonlink_ensure_wsa();

  signal_session *session = (signal_session *)calloc(1, sizeof(signal_session));
  if (!session) return (int)MOONLINK_RESULT_OUT_OF_MEMORY;

  session->sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (session->sock == INVALID_SOCKET) {
    free(session);
    return (int)MOONLINK_RESULT_TRANSPORT_UNAVAILABLE;
  }

  struct sockaddr_in addr = { 0 };
  addr.sin_family = AF_INET;
  addr.sin_port = htons((u_short)port);
  if (inet_pton(AF_INET, address, &addr.sin_addr) != 1) {
    closesocket(session->sock);
    free(session);
    return (int)MOONLINK_RESULT_INVALID_HANDLE;
  }

  if (connect(session->sock, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
    closesocket(session->sock);
    free(session);
    return (int)MOONLINK_RESULT_TRANSPORT_UNAVAILABLE;
  }

  session->stopped = FALSE;
  int slot = signal_table_alloc(&session_table, session);
  if (slot == MOONLINK_HANDLE_INVALID) {
    closesocket(session->sock);
    free(session);
    return (int)MOONLINK_RESULT_OUT_OF_MEMORY;
  }
  return slot;
}

int moonlink_signal_send_ffi(int handle, const char *data, int size) {
  signal_session *session = (signal_session *)signal_table_get(&session_table, handle);
  if (!session || session->stopped || !data || size <= 0) {
    return (int)MOONLINK_RESULT_INVALID_HANDLE;
  }
  if (size > MOONLINK_SIGNAL_MAX_MESSAGE) {
    return (int)MOONLINK_RESULT_INVALID_HANDLE;
  }

  /* Send 4-byte big-endian length prefix. */
  unsigned char len_buf[4];
  len_buf[0] = (unsigned char)((size >> 24) & 0xFF);
  len_buf[1] = (unsigned char)((size >> 16) & 0xFF);
  len_buf[2] = (unsigned char)((size >> 8) & 0xFF);
  len_buf[3] = (unsigned char)(size & 0xFF);

  if (send_all(session->sock, (const char *)len_buf, 4) != 0) {
    return (int)MOONLINK_RESULT_TRANSPORT_UNAVAILABLE;
  }
  if (send_all(session->sock, data, size) != 0) {
    return (int)MOONLINK_RESULT_TRANSPORT_UNAVAILABLE;
  }
  return (int)MOONLINK_RESULT_OK;
}

int moonlink_signal_recv_ffi(int handle, char *buffer, int buffer_size) {
  signal_session *session = (signal_session *)signal_table_get(&session_table, handle);
  if (!session || session->stopped || !buffer || buffer_size <= 0) {
    return (int)MOONLINK_RESULT_INVALID_HANDLE;
  }

  /* Receive 4-byte length prefix. */
  unsigned char len_buf[4];
  if (recv_all(session->sock, (char *)len_buf, 4) != 0) {
    return (int)MOONLINK_RESULT_TRANSPORT_UNAVAILABLE;
  }

  int msg_len = ((int)len_buf[0] << 24) | ((int)len_buf[1] << 16) |
                ((int)len_buf[2] << 8) | (int)len_buf[3];

  if (msg_len <= 0 || msg_len > buffer_size || msg_len > MOONLINK_SIGNAL_MAX_MESSAGE) {
    return (int)MOONLINK_RESULT_INVALID_HANDLE;
  }

  if (recv_all(session->sock, buffer, msg_len) != 0) {
    return (int)MOONLINK_RESULT_TRANSPORT_UNAVAILABLE;
  }

  return msg_len;
}

int moonlink_signal_close_ffi(int handle) {
  signal_session *session = (signal_session *)signal_table_remove(&session_table, handle);
  if (!session) return (int)MOONLINK_RESULT_INVALID_HANDLE;
  if (session->sock != INVALID_SOCKET) {
    closesocket(session->sock);
  }
  free(session);
  return (int)MOONLINK_RESULT_OK;
}

int moonlink_signal_stop_listen_ffi(int listener_handle) {
  signal_listener *listener = (signal_listener *)signal_table_remove(&listener_table, listener_handle);
  if (!listener) return (int)MOONLINK_RESULT_INVALID_HANDLE;
  listener->stopped = TRUE;
  if (listener->sock != INVALID_SOCKET) {
    closesocket(listener->sock);
  }
  free(listener);
  return (int)MOONLINK_RESULT_OK;
}

/* ── Raw access for transport bridge ─────────────────────────────────── */

int moonlink_signal_raw_send(int handle, const char *data, int size) {
  return moonlink_signal_send_ffi(handle, data, size);
}

int moonlink_signal_raw_recv(int handle, char *buf, int buf_size) {
  return moonlink_signal_recv_ffi(handle, buf, buf_size);
}


/* ── Connect by IP integer ───────────────────────────────────────────── */

int moonlink_signal_connect_ip_ffi(unsigned int ip, int port) {
  moonlink_ensure_wsa();

  signal_session *session = (signal_session *)calloc(1, sizeof(signal_session));
  if (!session) return (int)MOONLINK_RESULT_OUT_OF_MEMORY;

  session->sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (session->sock == INVALID_SOCKET) {
    free(session);
    return (int)MOONLINK_RESULT_TRANSPORT_UNAVAILABLE;
  }

  struct sockaddr_in addr = { 0 };
  addr.sin_family = AF_INET;
  addr.sin_port = htons((u_short)port);
  addr.sin_addr.s_addr = htonl(ip);

  if (connect(session->sock, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
    closesocket(session->sock);
    free(session);
    return (int)MOONLINK_RESULT_TRANSPORT_UNAVAILABLE;
  }

  session->stopped = FALSE;
  int slot = signal_table_alloc(&session_table, session);
  if (slot == MOONLINK_HANDLE_INVALID) {
    closesocket(session->sock);
    free(session);
    return (int)MOONLINK_RESULT_OUT_OF_MEMORY;
  }
  return slot;
}
