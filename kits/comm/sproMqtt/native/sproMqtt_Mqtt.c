// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 SedonaPro contributors
//
// sproMqtt::Mqtt natives: the client (spro_mqtt.c) and text/JSON helpers.
//
#include "sedona.h"
#include "spro_mqtt.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define M(i) ((spro_mqtt_t*)params[i].aval)
#define S(i) ((const char*)params[i].aval)
#define B(i) ((uint8_t*)params[i].aval)

static Cell ret(int32_t v)
{
  Cell r;
  r.ival = v;
  return r;
}

// Obj open(int bufSize)
Cell sproMqtt_Mqtt_open(SedonaVM* vm, Cell* params)
{
  Cell r;
  r.aval = spro_mqtt_new(params[0].ival);
  return r;
}

// void close(Obj m)
Cell sproMqtt_Mqtt_close(SedonaVM* vm, Cell* params)
{
  if (M(0)) spro_mqtt_free(M(0));
  return nullCell;
}

// void server(Obj m, Str host, int port, int keepAlive)
Cell sproMqtt_Mqtt_server(SedonaVM* vm, Cell* params)
{
  if (M(0)) spro_mqtt_server(M(0), S(1), params[2].ival, params[3].ival);
  return nullCell;
}

// void auth(Obj m, Str clientId, Str user, Str password)
Cell sproMqtt_Mqtt_auth(SedonaVM* vm, Cell* params)
{
  if (M(0)) spro_mqtt_auth(M(0), S(1), S(2), S(3));
  return nullCell;
}

// void will(Obj m, Str topic, Str offline, Str online)
Cell sproMqtt_Mqtt_will(SedonaVM* vm, Cell* params)
{
  if (M(0)) spro_mqtt_will(M(0), S(1), S(2), S(3));
  return nullCell;
}

// void enable(Obj m, bool on)
Cell sproMqtt_Mqtt_enable(SedonaVM* vm, Cell* params)
{
  if (M(0)) spro_mqtt_enable(M(0), params[1].ival);
  return nullCell;
}

// void restart(Obj m)
Cell sproMqtt_Mqtt_restart(SedonaVM* vm, Cell* params)
{
  if (M(0)) spro_mqtt_restart(M(0));
  return nullCell;
}

// void poll(Obj m)
Cell sproMqtt_Mqtt_poll(SedonaVM* vm, Cell* params)
{
  if (M(0)) spro_mqtt_poll(M(0));
  return nullCell;
}

// int publish(Obj m, Str topic, byte[] payload, int len, int qos, bool retain)
Cell sproMqtt_Mqtt_publish(SedonaVM* vm, Cell* params)
{
  if (!M(0)) return ret(SPRO_MQTT_PUB_OFFLINE);
  return ret(spro_mqtt_publish(M(0), S(1), B(2), params[3].ival, params[4].ival, params[5].ival));
}

// int subscribe(Obj m, Str filter, int qos, int payloadMax)
Cell sproMqtt_Mqtt_subscribe(SedonaVM* vm, Cell* params)
{
  if (!M(0)) return ret(-1);
  return ret(spro_mqtt_subscribe(M(0), S(1), params[2].ival, params[3].ival));
}

// void unsubscribe(Obj m, int slot)
Cell sproMqtt_Mqtt_unsubscribe(SedonaVM* vm, Cell* params)
{
  if (M(0)) spro_mqtt_unsubscribe(M(0), params[1].ival);
  return nullCell;
}

// int stat(Obj m, int which)
Cell sproMqtt_Mqtt_stat(SedonaVM* vm, Cell* params)
{
  return ret(M(0) ? spro_mqtt_stat(M(0), params[1].ival) : 0);
}

// bool topicMatch(Str filter, Str topic)
Cell sproMqtt_Mqtt_topicMatch(SedonaVM* vm, Cell* params)
{
  return ret(spro_mqtt_topic_match(S(0), S(1)));
}

// int defaultClientId(byte[] out, int cap)
Cell sproMqtt_Mqtt_defaultClientId(SedonaVM* vm, Cell* params)
{
  if (params[1].ival < 1) return ret(0);
  spro_mqtt_default_client_id((char*)B(0), params[1].ival);
  return ret((int32_t)strlen((char*)B(0)));
}

// int expandTopic(byte[] out, int cap, Str topic, Str base): "~/x" -> "<base>/x",
// "~" -> "<base>"; returns the length, -1 if it does not fit
Cell sproMqtt_Mqtt_expandTopic(SedonaVM* vm, Cell* params)
{
  char* out = (char*)B(0);
  int cap = params[1].ival, n;
  const char* t = S(2);
  if (t[0] == '~' && (t[1] == 0 || t[1] == '/'))
    n = snprintf(out, (size_t)cap, "%s%s", S(3), t + 1);
  else
    n = snprintf(out, (size_t)cap, "%s", t);
  return ret(n < cap ? n : -1);
}

static int fmtFloat(char* out, int cap, float v, int decimals)
{
  if (isnan(v) || isinf(v))
    return snprintf(out, (size_t)cap, "null");
  if (decimals < 0)
    return snprintf(out, (size_t)cap, "%.7g", (double)v);
  return snprintf(out, (size_t)cap, "%.*f", decimals > 9 ? 9 : decimals, (double)v);
}

// int fmtFloat(byte[] out, int cap, float v, int decimals): decimals -1 = up to 7 digits
Cell sproMqtt_Mqtt_fmtFloat(SedonaVM* vm, Cell* params)
{
  int cap = params[1].ival, n = fmtFloat((char*)B(0), cap, params[2].fval, params[3].ival);
  return ret(n < cap ? n : -1);
}

// int fmtInt(byte[] out, int cap, int v)
Cell sproMqtt_Mqtt_fmtInt(SedonaVM* vm, Cell* params)
{
  int cap = params[1].ival, n = snprintf((char*)B(0), (size_t)cap, "%ld", (long)params[2].ival);
  return ret(n < cap ? n : -1);
}

// int jsonPut(byte[] buf, int cap, int len, Str key, float v, int decimals):
// adds "key":v to the JSON object in buf[0..len) (len 0 = start a new one);
// returns the new length or -1 if it does not fit (buf unchanged)
Cell sproMqtt_Mqtt_jsonPut(SedonaVM* vm, Cell* params)
{
  char* b = (char*)B(0);
  int cap = params[1].ival, len = params[2].ival, n;
  const char* k = S(3);
  char tmp[160];
  char* p = tmp;
  *p++ = len > 0 ? ',' : '{';
  *p++ = '"';
  for (; *k && p < tmp + 120; ++k)
  {
    if (*k == '"' || *k == '\\') *p++ = '\\';
    if ((unsigned char)*k >= 0x20) *p++ = *k;
  }
  *p++ = '"';
  *p++ = ':';
  n = fmtFloat(p, (int)(tmp + sizeof(tmp) - p - 2), params[4].fval, params[5].ival);
  p += n;
  *p++ = '}';
  *p = 0;
  n = (int)(p - tmp);
  if (len > 0) len--;                          // drop the closing brace
  if (len + n + 1 > cap)
    return ret(-1);
  memcpy(b + len, tmp, (size_t)n + 1);
  return ret(len + n);
}

// ---------------------------------------------------------------- JSON reading

static const char* ws(const char* p, const char* e)
{
  while (p < e && isspace((unsigned char)*p)) p++;
  return p;
}

static const char* skipString(const char* p, const char* e)
{
  for (p++; p < e; ++p)
  {
    if (*p == '\\') p++;
    else if (*p == '"') return p + 1;
  }
  return NULL;
}

static const char* skipValue(const char* p, const char* e)
{
  int depth = 0;
  p = ws(p, e);
  if (p >= e) return NULL;
  if (*p == '"') return skipString(p, e);
  if (*p != '{' && *p != '[')
  {
    while (p < e && *p != ',' && *p != '}' && *p != ']' && !isspace((unsigned char)*p)) p++;
    return p;
  }
  for (; p < e; ++p)
  {
    if (*p == '"')
    {
      p = skipString(p, e);
      if (!p) return NULL;
      p--;
    }
    else if (*p == '{' || *p == '[') depth++;
    else if ((*p == '}' || *p == ']') && --depth == 0) return p + 1;
  }
  return NULL;
}

// find member/element 'name' (length n) in the object/array at p
static const char* member(const char* p, const char* e, const char* name, int n)
{
  p = ws(p, e);
  if (p >= e) return NULL;
  if (*p == '{')
  {
    p = ws(p + 1, e);
    while (p < e && *p == '"')
    {
      const char* ks = p + 1;
      const char* ke = skipString(p, e);
      if (!ke) return NULL;
      p = ws(ke, e);
      if (p >= e || *p != ':') return NULL;
      p = ws(p + 1, e);
      if (ke - 1 - ks == n && memcmp(ks, name, (size_t)n) == 0) return p;
      p = skipValue(p, e);
      if (!p) return NULL;
      p = ws(p, e);
      if (p < e && *p == ',') p = ws(p + 1, e);
    }
    return NULL;
  }
  if (*p == '[')
  {
    int idx = 0, i;
    for (i = 0; i < n; ++i)
    {
      if (!isdigit((unsigned char)name[i])) return NULL;
      idx = idx * 10 + (name[i] - '0');
    }
    p = ws(p + 1, e);
    for (i = 0; p < e && *p != ']'; ++i)
    {
      if (i == idx) return p;
      p = skipValue(p, e);
      if (!p) return NULL;
      p = ws(p, e);
      if (p < e && *p == ',') p = ws(p + 1, e);
    }
  }
  return NULL;
}

// value at "a.b.0" in JSON as text (strings without quotes, escapes decoded);
// path "" = the whole text; returns the length or -1
static int jsonFind(const char* p, int len, const char* path, char* out, int cap)
{
  const char* e = p + len;
  const char* end;
  int n = 0;
  if (cap < 1) return -1;
  if (!*path)
  {
    n = len < cap - 1 ? len : cap - 1;
    memcpy(out, p, (size_t)n);
    out[n] = 0;
    return n;
  }
  while (*path)
  {
    const char* dot = strchr(path, '.');
    int l = dot ? (int)(dot - path) : (int)strlen(path);
    p = member(p, e, path, l);
    if (!p) return -1;
    path += l + (dot ? 1 : 0);
  }
  p = ws(p, e);
  if (p >= e) return -1;
  if (*p == '"')
  {
    for (p++; p < e && *p != '"' && n < cap - 1; ++p)
    {
      char c = *p;
      if (c == '\\' && p + 1 < e)
      {
        c = *++p;
        if (c == 'n') c = '\n';
        else if (c == 't') c = '\t';
        else if (c == 'r') c = '\r';
        else if (c == 'u') { c = '?'; p += 4; }
      }
      out[n++] = c;
    }
    out[n] = 0;
    return n;
  }
  end = skipValue(p, e);
  if (!end) return -1;
  n = (int)(end - p) < cap - 1 ? (int)(end - p) : cap - 1;
  memcpy(out, p, (size_t)n);
  out[n] = 0;
  return n;
}

// int jsonGet(byte[] json, int len, Str path, byte[] out, int cap)
Cell sproMqtt_Mqtt_jsonGet(SedonaVM* vm, Cell* params)
{
  return ret(jsonFind((const char*)B(0), params[1].ival, S(2), (char*)B(3), params[4].ival));
}

// int fetch(Obj m, int slot, Str path, byte[] out, int cap): sequence number of
// the last message of the subscription (0 = none yet).  The payload, or the JSON
// value at path, goes to out zero terminated; its length via fetchLen() (-1 =
// path not in the message)
static int lastLen;
static uint8_t payload[1024];

Cell sproMqtt_Mqtt_fetch(SedonaVM* vm, Cell* params)
{
  int len, seq;
  lastLen = -1;
  if (!M(0)) return ret(0);
  seq = spro_mqtt_fetch(M(0), params[1].ival, payload, sizeof(payload), &len);
  if (seq)
    lastLen = jsonFind((const char*)payload, len, S(2), (char*)B(3), params[4].ival);
  return ret(seq);
}

// int fetchLen()
Cell sproMqtt_Mqtt_fetchLen(SedonaVM* vm, Cell* params)
{
  return ret(lastLen);
}

static int text(const uint8_t* s, int len, char* buf, int cap)
{
  int a = 0, b = len;
  while (a < b && isspace(s[a])) a++;
  while (b > a && isspace(s[b - 1])) b--;
  if (b - a >= cap) return 0;
  memcpy(buf, s + a, (size_t)(b - a));
  buf[b - a] = 0;
  return 1;
}

// float parseFloat(byte[] s, int len): null (NaN) if not a number
Cell sproMqtt_Mqtt_parseFloat(SedonaVM* vm, Cell* params)
{
  char buf[48], *end;
  Cell r;
  r.fval = NAN;
  if (text(B(0), params[1].ival, buf, sizeof(buf)) && buf[0])
  {
    float v = strtof(buf, &end);
    if (*end == 0) r.fval = v;
    else if (!strcmp(buf, "true")) r.fval = 1.0f;
    else if (!strcmp(buf, "false")) r.fval = 0.0f;
  }
  return r;
}

// int parseInt(byte[] s, int len): 0 if not a number (check with parseFloat)
Cell sproMqtt_Mqtt_parseInt(SedonaVM* vm, Cell* params)
{
  char buf[48], *end;
  if (text(B(0), params[1].ival, buf, sizeof(buf)) && buf[0])
  {
    long v = strtol(buf, &end, 10);
    if (*end == 0) return ret((int32_t)v);
    return ret((int32_t)lroundf(strtof(buf, NULL)));
  }
  return ret(0);
}

// int parseBool(byte[] s, int len): 1, 0 or -1 (true/false, on/off, yes/no, numbers)
Cell sproMqtt_Mqtt_parseBool(SedonaVM* vm, Cell* params)
{
  static const char* yes[] = { "true", "on", "yes", "1", NULL };
  static const char* no[] = { "false", "off", "no", "0", NULL };
  char buf[16], *end;
  int i;
  if (!text(B(0), params[1].ival, buf, sizeof(buf)) || !buf[0]) return ret(-1);
  for (i = 0; buf[i]; ++i) buf[i] = (char)tolower((unsigned char)buf[i]);
  for (i = 0; yes[i]; ++i) if (!strcmp(buf, yes[i])) return ret(1);
  for (i = 0; no[i]; ++i) if (!strcmp(buf, no[i])) return ret(0);
  {
    float v = strtof(buf, &end);
    if (*end == 0) return ret(v != 0.0f);
  }
  return ret(-1);
}
