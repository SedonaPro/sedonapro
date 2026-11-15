//
// SedonaPro HAL backend "sim": virtual pins with a web panel.
// Copyright 2026 SedonaPro contributors
// Licensed under the Apache License, Version 2.0
//
// Inputs (di, ai) are set from the panel or the HTTP API, outputs (do, ao)
// are shown there.  The panel thread starts with the first opened channel.
//
//   SPRO_GPIO_PORT   panel port, default 8090; 0 disables the panel
//   SPRO_GPIO_BIND   listen address, default 127.0.0.1
//
//   GET /                    panel page
//   GET /api/state           {"board":..,"channels":[{"name","kind","value","lo","hi"}]}
//   GET /api/set?ch=DI1&v=1  set an input (logical / engineering value)
//
#include "sedona.h"
#include "spro_hal.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define SIM_MAX 128

const char* const spro_hal_backend_name = "sim";

static float raw[SIM_MAX];
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t once = PTHREAD_ONCE_INIT;

static float toLogical(const spro_channel_t* c, float r)
{
  switch (c->kind)
  {
    case SPRO_KIND_DI:
    case SPRO_KIND_DO: return ((r != 0.0f) != (c->invert != 0)) ? 1.0f : 0.0f;
    case SPRO_KIND_AI: return c->rawMax > 0.0f ? c->lo + r / c->rawMax * (c->hi - c->lo) : r;
    case SPRO_KIND_AO: return c->lo + r * (c->hi - c->lo);
  }
  return r;
}

static float toRaw(const spro_channel_t* c, float v)
{
  switch (c->kind)
  {
    case SPRO_KIND_DI:
    case SPRO_KIND_DO: return ((v != 0.0f) != (c->invert != 0)) ? 1.0f : 0.0f;
    case SPRO_KIND_AI:
      if (c->hi == c->lo || c->rawMax <= 0.0f) return v;
      v = (v - c->lo) / (c->hi - c->lo) * c->rawMax;
      return v < 0.0f ? 0.0f : v > c->rawMax ? c->rawMax : v;
    case SPRO_KIND_AO:
      return c->hi != c->lo ? (v - c->lo) / (c->hi - c->lo) : 0.0f;
  }
  return v;
}

static const char* kindName(int k)
{
  static const char* names[] = { "none", "di", "do", "ai", "ao" };
  return k >= 0 && k <= 4 ? names[k] : "none";
}

// ---------------------------------------------------------------- panel

static const char PAGE[] =
"<!doctype html><html lang=en><head><meta charset=utf-8>"
"<meta name=viewport content='width=device-width,initial-scale=1'><title>SedonaPro GPIO</title><style>"
"body{font:15px system-ui,sans-serif;margin:16px;background:#f4f5f7;color:#1d2330}"
"@media(prefers-color-scheme:dark){body{background:#15181e;color:#e6e8ec}.c{background:#1f242c!important;border-color:#2c333d!important}}"
"h1{font-size:18px;margin:0 0 4px}small{opacity:.65}#g{display:grid;grid-template-columns:repeat(auto-fill,minmax(170px,1fr));gap:10px;margin-top:14px}"
".c{background:#fff;border:1px solid #dde1e7;border-radius:10px;padding:10px 12px}.n{font-weight:600}.k{font-size:12px;opacity:.6;margin-left:6px}"
".v{font-size:22px;margin:6px 0;font-variant-numeric:tabular-nums}button{font:inherit;padding:6px 14px;border-radius:8px;border:1px solid #8892a0;cursor:pointer}"
".on{background:#2f9e5b;color:#fff;border-color:#2f9e5b}.lamp{display:inline-block;width:18px;height:18px;border-radius:50%;background:#99a1ad;vertical-align:middle}"
".lamp.on{background:#f0b429;box-shadow:0 0 8px #f0b429}input[type=range]{width:100%}.bar{height:8px;background:#dde1e7;border-radius:4px}.bar i{display:block;height:8px;background:#3b82f6;border-radius:4px}"
"</style></head><body><h1>GPIO</h1><small id=b></small><div id=g></div><script>"
"let busy=0;async function set(n,v){busy=1;await fetch('/api/set?ch='+encodeURIComponent(n)+'&v='+v);busy=0;load()}"
"function f(x){return Math.abs(x)>=100?x.toFixed(0):x.toFixed(2)}"
"async function load(){if(busy)return;let s;try{s=await (await fetch('/api/state')).json()}catch(e){return}"
"b.textContent='board '+s.board+' \\u00b7 backend sim';let h='';for(const c of s.channels){h+='<div class=c><span class=n>'+c.name+'</span><span class=k>'+c.kind+'</span><div class=v>';"
"if(c.kind=='di')h+='<button class=\"'+(c.value?'on':'')+'\" onclick=\"set(\\''+c.name+'\\','+(c.value?0:1)+')\">'+(c.value?'ON':'OFF')+'</button>';"
"else if(c.kind=='do')h+='<span class=\"lamp '+(c.value?'on':'')+'\"></span> '+(c.value?'ON':'OFF');"
"else if(c.kind=='ai')h+=f(c.value)+'</div><input type=range min='+c.lo+' max='+c.hi+' step=any value='+c.value+' onchange=\"set(\\''+c.name+'\\',this.value)\">';"
"else h+=f(c.value)+'</div><div class=bar><i style=\"width:'+Math.max(0,Math.min(100,(c.value-c.lo)/(c.hi-c.lo)*100))+'%\"></i></div>';"
"if(c.kind=='di'||c.kind=='do')h+='</div>';h+='</div>'}"
"if(document.activeElement&&document.activeElement.type=='range')return;g.innerHTML=h}"
"load();setInterval(load,400)</script></body></html>";

static void sendAll(int fd, const char* p, size_t n)
{
  while (n > 0)
  {
    ssize_t w = send(fd, p, n, MSG_NOSIGNAL);
    if (w <= 0) return;
    p += w;
    n -= (size_t)w;
  }
}

static void reply(int fd, const char* status, const char* type, const char* body, size_t len)
{
  char head[200];
  int n = snprintf(head, sizeof(head), "HTTP/1.0 %s\r\nContent-Type: %s\r\nContent-Length: %u\r\n"
                   "Cache-Control: no-store\r\nConnection: close\r\n\r\n", status, type, (unsigned)len);
  sendAll(fd, head, (size_t)n);
  sendAll(fd, body, len);
}

static void state(int fd)
{
  char buf[16384];
  size_t n = 0;
  int i;
  n += snprintf(buf + n, sizeof(buf) - n, "{\"board\":\"%s\",\"channels\":[", spro_board_name);
  pthread_mutex_lock(&lock);
  for (i = 0; i < spro_board_channel_count && i < SIM_MAX && n < sizeof(buf) - 200; ++i)
  {
    const spro_channel_t* c = &spro_board_channels[i];
    n += snprintf(buf + n, sizeof(buf) - n, "%s{\"name\":\"%s\",\"kind\":\"%s\",\"value\":%g,\"lo\":%g,\"hi\":%g}",
                  i ? "," : "", c->name, kindName(c->kind), toLogical(c, raw[i]), c->lo, c->hi);
  }
  pthread_mutex_unlock(&lock);
  n += snprintf(buf + n, sizeof(buf) - n, "]}");
  reply(fd, "200 OK", "application/json", buf, n);
}

static int param(const char* query, const char* key, char* out, size_t max)
{
  size_t kl = strlen(key);
  const char* p = query;
  while (p && *p)
  {
    if (strncmp(p, key, kl) == 0 && p[kl] == '=')
    {
      size_t i = 0;
      p += kl + 1;
      while (*p && *p != '&' && *p != ' ' && i + 1 < max)
        out[i++] = *p++;
      out[i] = 0;
      return 1;
    }
    p = strchr(p, '&');
    if (p) p++;
  }
  return 0;
}

static void setInput(int fd, const char* query)
{
  char name[64], val[32];
  int i;
  if (!param(query, "ch", name, sizeof(name)) || !param(query, "v", val, sizeof(val)))
  {
    reply(fd, "400 Bad Request", "text/plain", "ch and v required\n", 18);
    return;
  }
  for (i = 0; i < spro_board_channel_count && i < SIM_MAX; ++i)
  {
    const spro_channel_t* c = &spro_board_channels[i];
    if (strcmp(c->name, name) != 0)
      continue;
    if (c->kind != SPRO_KIND_DI && c->kind != SPRO_KIND_AI)
    {
      reply(fd, "409 Conflict", "text/plain", "not an input\n", 13);
      return;
    }
    pthread_mutex_lock(&lock);
    raw[i] = toRaw(c, (float)atof(val));
    pthread_mutex_unlock(&lock);
    state(fd);
    return;
  }
  reply(fd, "404 Not Found", "text/plain", "no such channel\n", 16);
}

static void* panel(void* arg)
{
  int srv = (int)(intptr_t)arg;
  for (;;)
  {
    char req[1024];
    ssize_t n;
    int fd = accept(srv, NULL, NULL);
    if (fd < 0)
    {
      if (errno == EINTR) continue;
      return NULL;
    }
    n = recv(fd, req, sizeof(req) - 1, 0);
    if (n > 0)
    {
      req[n] = 0;
      if (strncmp(req, "GET /api/state", 14) == 0)
        state(fd);
      else if (strncmp(req, "GET /api/set?", 13) == 0)
        setInput(fd, req + 13);
      else if (strncmp(req, "GET / ", 6) == 0 || strncmp(req, "GET /index.html", 15) == 0)
        reply(fd, "200 OK", "text/html; charset=utf-8", PAGE, sizeof(PAGE) - 1);
      else
        reply(fd, "404 Not Found", "text/plain", "not found\n", 10);
    }
    close(fd);
  }
}

static void startPanel(void)
{
  const char* p = getenv("SPRO_GPIO_PORT");
  const char* bind_addr = getenv("SPRO_GPIO_BIND");
  int port = p ? atoi(p) : 8090;
  int srv, one = 1, i;
  struct sockaddr_in a;
  pthread_t t;

  for (i = 0; i < spro_board_channel_count && i < SIM_MAX; ++i)
  {
    const spro_channel_t* c = &spro_board_channels[i];
    raw[i] = toRaw(c, c->initial);
  }
  if (port <= 0)
    return;
  srv = socket(AF_INET, SOCK_STREAM, 0);
  if (srv < 0)
    return;
  setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  memset(&a, 0, sizeof(a));
  a.sin_family = AF_INET;
  a.sin_port = htons((uint16_t)port);
  a.sin_addr.s_addr = inet_addr(bind_addr ? bind_addr : "127.0.0.1");
  if (bind(srv, (struct sockaddr*)&a, sizeof(a)) != 0 || listen(srv, 4) != 0)
  {
    printf("-- sproHal sim: GPIO panel port %d busy (%s)\n", port, strerror(errno));
    fflush(stdout);
    close(srv);
    return;
  }
  if (pthread_create(&t, NULL, panel, (void*)(intptr_t)srv) == 0)
  {
    pthread_detach(t);
    printf("-- sproHal sim: GPIO panel http://%s:%d/\n", bind_addr ? bind_addr : "127.0.0.1", port);
    fflush(stdout);
  }
}

// ---------------------------------------------------------------- backend

int spro_hal_backend_open(int index, const spro_channel_t* c)
{
  pthread_once(&once, startPanel);
  return index < SIM_MAX ? 0 : -1;
}

int spro_hal_backend_read(int index, const spro_channel_t* c, float* value)
{
  if (index >= SIM_MAX) return -1;
  pthread_mutex_lock(&lock);
  *value = raw[index];
  pthread_mutex_unlock(&lock);
  return 0;
}

int spro_hal_backend_write(int index, const spro_channel_t* c, float value)
{
  if (index >= SIM_MAX) return -1;
  pthread_mutex_lock(&lock);
  raw[index] = value;
  pthread_mutex_unlock(&lock);
  return 0;
}
