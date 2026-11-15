// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 SedonaPro contributors
//
// MQTT 3.1.1 client, see spro_mqtt.h.  Everything is non-blocking except the
// DNS lookup of a host name (an IP address is used directly); lookups are
// rate limited by the reconnect back-off (1 s doubling up to 30 s).
//
#include "spro_mqtt.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

#if defined(ESP_PLATFORM) && !defined(ESP32SIM)
#include "esp_mac.h"
#include "esp_random.h"
#endif

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#define MAX_SUBS        32
#define MAX_INFLIGHT    8
#define CONNECT_TIMEOUT 10000
#define BACKOFF_MIN     1000
#define BACKOFF_MAX     30000

// packet types
#define CONNECT     0x10
#define CONNACK     0x20
#define PUBLISH     0x30
#define PUBACK      0x40
#define SUBSCRIBE   0x82
#define SUBACK      0x90
#define UNSUBSCRIBE 0xA2
#define UNSUBACK    0xB0
#define PINGREQ     0xC0
#define PINGRESP    0xD0
#define DISCONNECT  0xE0

typedef struct
{
  char* filter;
  int qos;
  uint8_t* payload;
  int cap, len;
  uint32_t seq;
} sub_t;

typedef struct
{
  uint16_t id;
  int len;
  uint8_t* pkt;   // whole PUBLISH packet, resent with DUP after a reconnect
} inflight_t;

struct spro_mqtt
{
  // configuration
  char host[96];
  int port, keepAlive, enabled;
  char clientId[64], user[64], password[96];
  char willTopic[128], offline[32], online[32];

  // connection
  int fd, state, error, tcpUp;
  int64_t since, retryAt, lastTx, lastRx, pingAt;
  int backoff;
  uint16_t nextId;

  uint8_t* tx;   int txLen, cap;
  uint8_t* rx;   int rxLen;
  int discard;   // bytes left of an oversized incoming packet

  sub_t subs[MAX_SUBS];
  inflight_t inflight[MAX_INFLIGHT];
  int stats[7];
};

static int64_t nowMs(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void copyStr(char* dst, int cap, const char* src)
{
  snprintf(dst, (size_t)cap, "%s", src ? src : "");
}

// ---------------------------------------------------------------- encoding

static int varLenSize(int n)
{
  return n < 128 ? 1 : n < 16384 ? 2 : n < 2097152 ? 3 : 4;
}

static uint8_t* putVarLen(uint8_t* p, int n)
{
  do
  {
    uint8_t b = (uint8_t)(n % 128);
    n /= 128;
    *p++ = n > 0 ? (uint8_t)(b | 128) : b;
  } while (n > 0);
  return p;
}

static uint8_t* putU16(uint8_t* p, int v)
{
  *p++ = (uint8_t)(v >> 8);
  *p++ = (uint8_t)v;
  return p;
}

static uint8_t* putStr(uint8_t* p, const char* s, int n)
{
  p = putU16(p, n);
  memcpy(p, s, (size_t)n);
  return p + n;
}

// room for a packet with this remaining length in the send buffer?
static uint8_t* txReserve(spro_mqtt_t* m, int remaining)
{
  int total = 1 + varLenSize(remaining) + remaining;
  if (m->txLen + total > m->cap)
    return NULL;
  return m->tx + m->txLen;
}

static void txCommit(spro_mqtt_t* m, uint8_t* end)
{
  m->txLen = (int)(end - m->tx);
}

static uint16_t newId(spro_mqtt_t* m)
{
  if (++m->nextId == 0)
    m->nextId = 1;
  return m->nextId;
}

// ---------------------------------------------------------------- connection

static void closeSocket(spro_mqtt_t* m)
{
  if (m->fd >= 0)
    close(m->fd);
  m->fd = -1;
  m->tcpUp = 0;
  m->txLen = m->rxLen = m->discard = 0;
  m->pingAt = 0;
}

static void fail(spro_mqtt_t* m, int error)
{
  closeSocket(m);
  m->error = error;
  m->state = m->enabled ? SPRO_MQTT_WAITING : SPRO_MQTT_DISABLED;
  m->retryAt = nowMs() + m->backoff;
  m->backoff = m->backoff * 2 > BACKOFF_MAX ? BACKOFF_MAX : m->backoff * 2;
}

static void queueConnect(spro_mqtt_t* m)
{
  int cid = (int)strlen(m->clientId), u = (int)strlen(m->user), pw = (int)strlen(m->password);
  int wt = (int)strlen(m->willTopic), wm = (int)strlen(m->offline);
  int rem = 10 + 2 + cid;
  uint8_t flags = 0x02;                       // clean session
  uint8_t* p;
  if (wt)
  {
    rem += 2 + wt + 2 + wm;
    flags |= 0x04 | 0x08 | 0x20;              // will, QoS 1, retain
  }
  if (u)
  {
    rem += 2 + u;
    flags |= 0x80;
  }
  if (pw)
  {
    rem += 2 + pw;
    flags |= 0x40;
  }
  p = txReserve(m, rem);
  if (!p)
  {
    fail(m, SPRO_MQTT_E_NOMEM);
    return;
  }
  *p++ = CONNECT;
  p = putVarLen(p, rem);
  p = putStr(p, "MQTT", 4);
  *p++ = 4;                                    // protocol level 3.1.1
  *p++ = flags;
  p = putU16(p, m->keepAlive);
  p = putStr(p, m->clientId, cid);
  if (wt)
  {
    p = putStr(p, m->willTopic, wt);
    p = putStr(p, m->offline, wm);
  }
  if (u) p = putStr(p, m->user, u);
  if (pw) p = putStr(p, m->password, pw);
  txCommit(m, p);
}

static void startConnect(spro_mqtt_t* m)
{
  struct sockaddr_in a;
  int one = 1;
  if (!m->host[0])
  {
    m->error = SPRO_MQTT_E_CONFIG;
    m->state = SPRO_MQTT_WAITING;
    m->retryAt = nowMs() + BACKOFF_MAX;
    return;
  }
  memset(&a, 0, sizeof(a));
  a.sin_family = AF_INET;
  a.sin_port = htons((uint16_t)m->port);
  if (inet_pton(AF_INET, m->host, &a.sin_addr) != 1)
  {
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(m->host, NULL, &hints, &res) != 0 || !res)
    {
      fail(m, SPRO_MQTT_E_DNS);
      return;
    }
    a.sin_addr = ((struct sockaddr_in*)res->ai_addr)->sin_addr;
    freeaddrinfo(res);
  }
  m->fd = socket(AF_INET, SOCK_STREAM, 0);
  if (m->fd < 0)
  {
    fail(m, SPRO_MQTT_E_NOMEM);
    return;
  }
  fcntl(m->fd, F_SETFL, fcntl(m->fd, F_GETFL, 0) | O_NONBLOCK);
  setsockopt(m->fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
  m->state = SPRO_MQTT_CONNECTING;
  m->since = nowMs();
  m->tcpUp = 0;
  if (connect(m->fd, (struct sockaddr*)&a, sizeof(a)) != 0 && errno != EINPROGRESS)
    fail(m, SPRO_MQTT_E_CONNECT);
}

static int writable(int fd)
{
  fd_set w;
  struct timeval tv = { 0, 0 };
  FD_ZERO(&w);
  FD_SET(fd, &w);
  return select(fd + 1, NULL, &w, NULL, &tv) > 0;
}

static void flush(spro_mqtt_t* m)
{
  while (m->fd >= 0 && m->tcpUp && m->txLen > 0)
  {
    ssize_t n = send(m->fd, m->tx, (size_t)m->txLen, MSG_NOSIGNAL);
    if (n < 0)
    {
      if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
        fail(m, SPRO_MQTT_E_CLOSED);
      return;
    }
    memmove(m->tx, m->tx + n, (size_t)(m->txLen - n));
    m->txLen -= (int)n;
    m->lastTx = nowMs();
  }
}

static int queueSubscribe(spro_mqtt_t* m, int slot)
{
  sub_t* s = &m->subs[slot];
  int n = (int)strlen(s->filter);
  uint8_t* p = txReserve(m, 2 + 2 + n + 1);
  if (!p) return 0;
  *p++ = SUBSCRIBE;
  p = putVarLen(p, 2 + 2 + n + 1);
  p = putU16(p, newId(m));
  p = putStr(p, s->filter, n);
  *p++ = (uint8_t)s->qos;
  txCommit(m, p);
  return 1;
}

static void connected(spro_mqtt_t* m)
{
  int i;
  m->state = SPRO_MQTT_CONNECTED;
  m->error = SPRO_MQTT_OK;
  m->backoff = BACKOFF_MIN;
  m->stats[SPRO_MQTT_STAT_CONNECTS]++;
  for (i = 0; i < MAX_SUBS; ++i)
    if (m->subs[i].filter)
      queueSubscribe(m, i);
  // QoS 1 messages not acknowledged before the connection was lost
  for (i = 0; i < MAX_INFLIGHT; ++i)
    if (m->inflight[i].pkt && m->txLen + m->inflight[i].len <= m->cap)
    {
      m->inflight[i].pkt[0] |= 0x08;           // DUP
      memcpy(m->tx + m->txLen, m->inflight[i].pkt, (size_t)m->inflight[i].len);
      m->txLen += m->inflight[i].len;
    }
  if (m->willTopic[0])
    spro_mqtt_publish(m, m->willTopic, (const uint8_t*)m->online, (int)strlen(m->online), 1, 1);
}

static void onPublish(spro_mqtt_t* m, uint8_t h, const uint8_t* b, int len)
{
  int qos = (h >> 1) & 3, tl, i, off;
  char topic[256];
  uint16_t id = 0;
  if (len < 2) { fail(m, SPRO_MQTT_E_PROTOCOL); return; }
  tl = (b[0] << 8) | b[1];
  off = 2 + tl;
  if (off > len) { fail(m, SPRO_MQTT_E_PROTOCOL); return; }
  if (qos > 0)
  {
    if (off + 2 > len) { fail(m, SPRO_MQTT_E_PROTOCOL); return; }
    id = (uint16_t)((b[off] << 8) | b[off + 1]);
    off += 2;
  }
  m->stats[SPRO_MQTT_STAT_RX]++;
  if (tl < (int)sizeof(topic))
  {
    memcpy(topic, b + 2, (size_t)tl);
    topic[tl] = 0;
    for (i = 0; i < MAX_SUBS; ++i)
    {
      sub_t* s = &m->subs[i];
      if (s->filter && spro_mqtt_topic_match(s->filter, topic))
      {
        s->len = len - off > s->cap ? s->cap : len - off;
        memcpy(s->payload, b + off, (size_t)s->len);
        if (++s->seq == 0) s->seq = 1;
      }
    }
  }
  if (qos == 1)
  {
    uint8_t* p = txReserve(m, 2);
    if (p)
    {
      *p++ = PUBACK;
      *p++ = 2;
      p = putU16(p, id);
      txCommit(m, p);
    }
  }
  else if (qos == 2)
    fail(m, SPRO_MQTT_E_PROTOCOL);             // never requested
}

static void onPacket(spro_mqtt_t* m, uint8_t h, const uint8_t* b, int len)
{
  int i;
  switch (h & 0xF0)
  {
    case CONNACK:
      if (len < 2 || m->state != SPRO_MQTT_CONNECTING) { fail(m, SPRO_MQTT_E_PROTOCOL); return; }
      if (b[1] == 0) connected(m);
      else fail(m, (b[1] == 4 || b[1] == 5) ? SPRO_MQTT_E_AUTH : SPRO_MQTT_E_REJECTED);
      return;
    case PUBLISH:
      onPublish(m, h, b, len);
      return;
    case PUBACK:
      if (len >= 2)
      {
        uint16_t id = (uint16_t)((b[0] << 8) | b[1]);
        for (i = 0; i < MAX_INFLIGHT; ++i)
          if (m->inflight[i].pkt && m->inflight[i].id == id)
          {
            free(m->inflight[i].pkt);
            m->inflight[i].pkt = NULL;
          }
      }
      return;
    case PINGRESP:
      m->pingAt = 0;
      return;
    case SUBACK:
    case UNSUBACK:
      return;
    default:
      fail(m, SPRO_MQTT_E_PROTOCOL);
  }
}

// parse complete packets in rx
static void parse(spro_mqtt_t* m)
{
  int pos = 0;
  while (m->state != SPRO_MQTT_WAITING && m->state != SPRO_MQTT_DISABLED)
  {
    int avail = m->rxLen - pos, rem = 0, mult = 1, i = 1, total;
    if (m->discard > 0)
    {
      int n = avail < m->discard ? avail : m->discard;
      pos += n;
      m->discard -= n;
      if (m->discard > 0) break;
      continue;
    }
    if (avail < 2) break;
    for (;;)
    {
      uint8_t c;
      if (i >= avail) goto incomplete;
      if (i > 4) { fail(m, SPRO_MQTT_E_PROTOCOL); return; }
      c = m->rx[pos + i++];
      rem += (c & 127) * mult;
      mult *= 128;
      if (!(c & 128)) break;
    }
    total = i + rem;
    if (total > m->cap)
    {
      // bigger than the buffer: skip it
      m->stats[SPRO_MQTT_STAT_DROPPED]++;
      m->discard = total;
      continue;
    }
    if (total > avail) break;
    onPacket(m, m->rx[pos], m->rx + pos + i, rem);
    pos += total;
  }
incomplete:
  if (m->fd < 0) return;                       // fail() reset the buffers
  if (pos > 0)
  {
    memmove(m->rx, m->rx + pos, (size_t)(m->rxLen - pos));
    m->rxLen -= pos;
  }
}

static void receive(spro_mqtt_t* m)
{
  while (m->fd >= 0 && m->tcpUp && m->rxLen < m->cap)
  {
    ssize_t n = recv(m->fd, m->rx + m->rxLen, (size_t)(m->cap - m->rxLen), 0);
    if (n == 0) { fail(m, SPRO_MQTT_E_CLOSED); return; }
    if (n < 0)
    {
      if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
        fail(m, SPRO_MQTT_E_CLOSED);
      return;
    }
    m->rxLen += (int)n;
    m->lastRx = nowMs();
    parse(m);
    if (m->rxLen >= m->cap && m->discard == 0)
    {
      fail(m, SPRO_MQTT_E_PROTOCOL);
      return;
    }
  }
}

// ---------------------------------------------------------------- public

spro_mqtt_t* spro_mqtt_new(int bufSize)
{
  spro_mqtt_t* m = (spro_mqtt_t*)calloc(1, sizeof(spro_mqtt_t));
  if (!m) return NULL;
  m->cap = bufSize < 256 ? 256 : bufSize;
  m->tx = (uint8_t*)malloc((size_t)m->cap);
  m->rx = (uint8_t*)malloc((size_t)m->cap);
  if (!m->tx || !m->rx)
  {
    spro_mqtt_free(m);
    return NULL;
  }
  m->fd = -1;
  m->port = 1883;
  m->keepAlive = 30;
  m->backoff = BACKOFF_MIN;
  m->state = SPRO_MQTT_DISABLED;
  return m;
}

void spro_mqtt_free(spro_mqtt_t* m)
{
  int i;
  if (!m) return;
  spro_mqtt_enable(m, 0);
  for (i = 0; i < MAX_SUBS; ++i)
    spro_mqtt_unsubscribe(m, i);
  for (i = 0; i < MAX_INFLIGHT; ++i)
    free(m->inflight[i].pkt);
  free(m->tx);
  free(m->rx);
  free(m);
}

void spro_mqtt_server(spro_mqtt_t* m, const char* host, int port, int keepAliveSec)
{
  copyStr(m->host, sizeof(m->host), host);
  m->port = port > 0 && port < 65536 ? port : 1883;
  m->keepAlive = keepAliveSec < 0 ? 0 : keepAliveSec > 65535 ? 65535 : keepAliveSec;
}

void spro_mqtt_auth(spro_mqtt_t* m, const char* clientId, const char* user, const char* password)
{
  if (clientId && clientId[0])
    copyStr(m->clientId, sizeof(m->clientId), clientId);
  else
    spro_mqtt_default_client_id(m->clientId, sizeof(m->clientId));
  copyStr(m->user, sizeof(m->user), user);
  copyStr(m->password, sizeof(m->password), password);
}

void spro_mqtt_will(spro_mqtt_t* m, const char* topic, const char* offline, const char* online)
{
  copyStr(m->willTopic, sizeof(m->willTopic), topic);
  copyStr(m->offline, sizeof(m->offline), offline);
  copyStr(m->online, sizeof(m->online), online);
}

void spro_mqtt_enable(spro_mqtt_t* m, int enabled)
{
  if (!enabled && m->fd >= 0 && m->state == SPRO_MQTT_CONNECTED)
  {
    // clean disconnect: the broker does not publish the will, so say offline
    if (m->willTopic[0])
      spro_mqtt_publish(m, m->willTopic, (const uint8_t*)m->offline, (int)strlen(m->offline), 0, 1);
    uint8_t* p = txReserve(m, 0);
    if (p)
    {
      *p++ = DISCONNECT;
      *p++ = 0;
      txCommit(m, p);
    }
    flush(m);
  }
  m->enabled = enabled;
  if (!enabled)
  {
    closeSocket(m);
    m->state = SPRO_MQTT_DISABLED;
  }
  else if (m->state == SPRO_MQTT_DISABLED)
  {
    m->state = SPRO_MQTT_WAITING;
    m->retryAt = 0;
    m->backoff = BACKOFF_MIN;
  }
}

void spro_mqtt_restart(spro_mqtt_t* m)
{
  int en = m->enabled;
  spro_mqtt_enable(m, 0);
  spro_mqtt_enable(m, en);
}

void spro_mqtt_poll(spro_mqtt_t* m)
{
  int64_t now = nowMs();
  if (!m->clientId[0])
    spro_mqtt_default_client_id(m->clientId, sizeof(m->clientId));
  switch (m->state)
  {
    case SPRO_MQTT_DISABLED:
      return;
    case SPRO_MQTT_WAITING:
      if (now >= m->retryAt)
        startConnect(m);
      return;
    case SPRO_MQTT_CONNECTING:
      if (now - m->since > CONNECT_TIMEOUT)
      {
        fail(m, m->tcpUp ? SPRO_MQTT_E_TIMEOUT : SPRO_MQTT_E_CONNECT);
        return;
      }
      if (!m->tcpUp)
      {
        int err = 0;
        socklen_t el = sizeof(err);
        if (!writable(m->fd))
          return;
        if (getsockopt(m->fd, SOL_SOCKET, SO_ERROR, &err, &el) != 0 || err != 0)
        {
          fail(m, SPRO_MQTT_E_CONNECT);
          return;
        }
        m->tcpUp = 1;
        m->lastRx = now;
        m->txLen = 0;
        queueConnect(m);
      }
      break;
    case SPRO_MQTT_CONNECTED:
      if (m->keepAlive > 0)
      {
        if (m->pingAt && now - m->pingAt > (int64_t)m->keepAlive * 1000)
        {
          fail(m, SPRO_MQTT_E_TIMEOUT);
          return;
        }
        if (!m->pingAt && now - m->lastTx >= (int64_t)m->keepAlive * 750)
        {
          uint8_t* p = txReserve(m, 0);
          if (p)
          {
            *p++ = PINGREQ;
            *p++ = 0;
            txCommit(m, p);
            m->pingAt = now;
          }
        }
      }
      break;
  }
  flush(m);
  receive(m);
  flush(m);
}

int spro_mqtt_publish(spro_mqtt_t* m, const char* topic, const uint8_t* payload, int len, int qos, int retain)
{
  int tl = (int)strlen(topic), rem, slot = -1, i;
  uint8_t *p, *start;
  uint16_t id = 0;
  if (m->state != SPRO_MQTT_CONNECTED)
    return SPRO_MQTT_PUB_OFFLINE;
  qos = qos > 0 ? 1 : 0;
  rem = 2 + tl + (qos ? 2 : 0) + len;
  if (1 + varLenSize(rem) + rem > m->cap)
    return SPRO_MQTT_PUB_TOOBIG;
  if (qos)
  {
    for (i = 0; i < MAX_INFLIGHT && slot < 0; ++i)
      if (!m->inflight[i].pkt) slot = i;
    if (slot < 0)
      return SPRO_MQTT_PUB_BUSY;
  }
  p = start = txReserve(m, rem);
  if (!p)
  {
    flush(m);
    p = start = txReserve(m, rem);
    if (!p) return SPRO_MQTT_PUB_BUSY;
  }
  *p++ = (uint8_t)(PUBLISH | (qos << 1) | (retain ? 1 : 0));
  p = putVarLen(p, rem);
  p = putStr(p, topic, tl);
  if (qos)
  {
    id = newId(m);
    p = putU16(p, id);
  }
  memcpy(p, payload, (size_t)len);
  p += len;
  if (qos)
  {
    int n = (int)(p - start);
    m->inflight[slot].pkt = (uint8_t*)malloc((size_t)n);
    if (!m->inflight[slot].pkt)
      return SPRO_MQTT_PUB_BUSY;
    memcpy(m->inflight[slot].pkt, start, (size_t)n);
    m->inflight[slot].len = n;
    m->inflight[slot].id = id;
  }
  txCommit(m, p);
  m->stats[SPRO_MQTT_STAT_TX]++;
  flush(m);
  return SPRO_MQTT_PUB_OK;
}

int spro_mqtt_subscribe(spro_mqtt_t* m, const char* filter, int qos, int payloadMax)
{
  int i;
  for (i = 0; i < MAX_SUBS; ++i)
  {
    sub_t* s = &m->subs[i];
    if (s->filter) continue;
    s->filter = strdup(filter);
    s->payload = (uint8_t*)malloc((size_t)(payloadMax > 0 ? payloadMax : 1));
    if (!s->filter || !s->payload)
    {
      free(s->filter);
      free(s->payload);
      s->filter = NULL;
      s->payload = NULL;
      return -1;
    }
    s->qos = qos > 0 ? 1 : 0;
    s->cap = payloadMax;
    s->len = 0;
    s->seq = 0;
    if (m->state == SPRO_MQTT_CONNECTED)
    {
      queueSubscribe(m, i);
      flush(m);
    }
    return i;
  }
  return -1;
}

void spro_mqtt_unsubscribe(spro_mqtt_t* m, int slot)
{
  sub_t* s;
  if (slot < 0 || slot >= MAX_SUBS || !m->subs[slot].filter) return;
  s = &m->subs[slot];
  if (m->state == SPRO_MQTT_CONNECTED)
  {
    int n = (int)strlen(s->filter), j, shared = 0;
    for (j = 0; j < MAX_SUBS; ++j)
      if (j != slot && m->subs[j].filter && strcmp(m->subs[j].filter, s->filter) == 0)
        shared = 1;
    if (!shared)
    {
      uint8_t* p = txReserve(m, 2 + 2 + n);
      if (p)
      {
        *p++ = UNSUBSCRIBE;
        p = putVarLen(p, 2 + 2 + n);
        p = putU16(p, newId(m));
        p = putStr(p, s->filter, n);
        txCommit(m, p);
        flush(m);
      }
    }
  }
  free(s->filter);
  free(s->payload);
  memset(s, 0, sizeof(*s));
}

int spro_mqtt_fetch(spro_mqtt_t* m, int slot, uint8_t* out, int cap, int* len)
{
  sub_t* s;
  int n;
  *len = 0;
  if (slot < 0 || slot >= MAX_SUBS || !m->subs[slot].filter) return 0;
  s = &m->subs[slot];
  n = s->len < cap ? s->len : cap;
  memcpy(out, s->payload, (size_t)n);
  if (n < cap) out[n] = 0;
  *len = n;
  return (int)s->seq;
}

int spro_mqtt_stat(spro_mqtt_t* m, int which)
{
  int i, n = 0;
  switch (which)
  {
    case SPRO_MQTT_STAT_STATE: return m->state;
    case SPRO_MQTT_STAT_ERROR: return m->error;
    case SPRO_MQTT_STAT_INFLIGHT:
      for (i = 0; i < MAX_INFLIGHT; ++i)
        if (m->inflight[i].pkt) n++;
      return n;
    default:
      return which >= 0 && which < 7 ? m->stats[which] : 0;
  }
}

int spro_mqtt_topic_match(const char* f, const char* t)
{
  if (*t == '$' && (*f == '+' || *f == '#'))
    return 0;                                  // $SYS etc. only by explicit filters
  for (;;)
  {
    if (*f == '#')
      return f[1] == 0;
    if (*f == '+')
    {
      while (*t && *t != '/') t++;
      f++;
    }
    else
    {
      while (*f && *f != '/' && *f == *t) { f++; t++; }
      if ((*f && *f != '/') || (*t && *t != '/'))
        return 0;
    }
    if (*f == 0 && *t == 0) return 1;
    if (*f == '/' && *t == '/') { f++; t++; continue; }
    // "a/#" also matches "a"
    if (*t == 0 && f[0] == '/' && f[1] == '#' && f[2] == 0) return 1;
    return 0;
  }
}

void spro_mqtt_default_client_id(char* out, int cap)
{
#if defined(ESP_PLATFORM) && !defined(ESP32SIM)
  uint8_t mac[6] = { 0 };
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  if (!(mac[0] | mac[1] | mac[2] | mac[3] | mac[4] | mac[5]))
  {
    // no MAC (blank eFuse, e.g. QEMU): random, stable until reboot
    uint32_t r = esp_random();
    memcpy(mac + 2, &r, 4);
  }
  snprintf(out, (size_t)cap, "spro-%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
#else
  char host[48] = "host";
  gethostname(host, sizeof(host) - 1);
  host[sizeof(host) - 1] = 0;
  snprintf(out, (size_t)cap, "spro-%s-%d", host, (int)getpid());
#endif
}
