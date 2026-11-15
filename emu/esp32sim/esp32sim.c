// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 SedonaPro contributors
//
// ESP32 simulator for the Sedona port: runs the firmware's Sedona code
// (src/vm, natives, platform/sys_esp32, platform/sproPlatformEsp32) as a Linux
// process with the ESP-IDF calls implemented here.
//
//   esp32sim [--flash DIR] [--heap BYTES] [--wdt SEC] [--tests]
//            [--restart-delay MS] [--max-resets N] [--no-restart]
//
// Emulated like on the chip:
//  * flash file system: DIR (default ./flash) plays the LittleFS partition
//    mounted at CONFIG_SEDONA_HOME; files survive "chip resets"
//  * chip reset (esp_restart, VM exit, watchdog): the process re-executes
//    itself; esp_reset_reason() reports why (SW, TASK_WDT, ...)
//  * task watchdog: after the first scan cycle the VM must feed it within
//    --wdt seconds, otherwise "Task watchdog got triggered" and a reset
//  * heap: --heap bytes of RAM (ESP32-S3 without PSRAM: ~320 KB free);
//    malloc fails beyond it and heap_caps_* report free / minimum free
//
// Debug: SIGUSR2 hangs the VM task inside its next delay (a stalled scan
// loop), so the task watchdog can be tested: kill -USR2 <pid>.
//
// Not emulated: Xtensa code generation and alignment, IRAM, Wi-Fi, timing
// of a 240 MHz core.
//
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <malloc.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "esp32sim.h"

int svm_main(int argc, char* argv[]);   // src/vm/main.c
extern char svmEspHome[64];             // svm_esp32.c

static char** origArgv;
static char selfExe[PATH_MAX];
static int64_t bootNs;
static esp_reset_reason_t resetReason = ESP_RST_POWERON;
static int resets = 0, maxResets = -1, noRestart = 0;

static int64_t monoNs(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

////////////////////////////////////////////////////////////////
// Log, time, reset
////////////////////////////////////////////////////////////////

void esp32simLog(char level, const char* tag, const char* fmt, ...)
{
  va_list ap;
  printf("%c (%lld) %s: ", level, (long long)(esp_timer_get_time() / 1000), tag);
  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);
  printf("\n");
  fflush(stdout);
}

int64_t esp_timer_get_time(void)
{
  return (monoNs() - bootNs) / 1000;
}

void ets_delay_us(uint32_t us)
{
  int64_t until = monoNs() + (int64_t)us * 1000;
  while (monoNs() < until)
    ;
}

static volatile sig_atomic_t hangVm = 0;
static pthread_t vmThread;

static void onUsr2(int sig)
{
  hangVm = 1;
}

void esp32simTaskDelayMs(uint32_t ms)
{
  struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
  while (nanosleep(&ts, &ts) != 0 && errno == EINTR)
    ;
  if (hangVm && pthread_equal(pthread_self(), vmThread))
  {
    esp32simLog('W', "esp32sim", "SIGUSR2: VM task hangs now");
    for (;;) sleep(1);
  }
}

esp_reset_reason_t esp_reset_reason(void)
{
  return resetReason;
}

static void chipReset(esp_reset_reason_t why) __attribute__((noreturn));
static void chipReset(esp_reset_reason_t why)
{
  char env[16];
  int fd;
  fflush(stdout);
  fflush(stderr);
  if (noRestart || (maxResets >= 0 && resets >= maxResets))
  {
    printf("esp32sim: chip reset (reason %d), not restarting\n", (int)why);
    fflush(stdout);
    _exit(100 + (int)why);
  }
  printf("esp32sim: chip reset (reason %d)\n\n", (int)why);
  fflush(stdout);
  // a real reset closes every socket: free the SOX port for the next boot
  for (fd = 3; fd < 1024; ++fd) close(fd);
  snprintf(env, sizeof(env), "%d", (int)why);
  setenv("ESP32SIM_RESET", env, 1);
  snprintf(env, sizeof(env), "%d", resets + 1);
  setenv("ESP32SIM_RESETS", env, 1);
  execv(selfExe, origArgv);        // real path: keeps the process name
  perror("esp32sim: execv");
  _exit(1);
}

void esp_restart(void)
{
  chipReset(ESP_RST_SW);
}

////////////////////////////////////////////////////////////////
// Heap
////////////////////////////////////////////////////////////////

static size_t heapSize = 320 * 1024;
static size_t heapBase = 0;          // host allocations before the "chip" starts
static size_t heapMinFree = (size_t)-1;
static pthread_mutex_t heapLock = PTHREAD_MUTEX_INITIALIZER;

static size_t heapUsed(void)
{
  struct mallinfo2 mi = mallinfo2();
  return mi.uordblks > heapBase ? mi.uordblks - heapBase : 0;
}

static size_t heapFree(void)
{
  size_t used = heapUsed();
  size_t f = used < heapSize ? heapSize - used : 0;
  if (f < heapMinFree) heapMinFree = f;
  return f;
}

size_t heap_caps_get_free_size(uint32_t caps)
{
  return heapFree();
}

size_t heap_caps_get_minimum_free_size(uint32_t caps)
{
  heapFree();
  return heapMinFree;
}

void* __real_malloc(size_t n);
void* __real_calloc(size_t a, size_t b);
void* __real_realloc(void* p, size_t n);

static int heapFits(size_t n)
{
  int ok;
  pthread_mutex_lock(&heapLock);
  ok = heapBase == 0 || heapUsed() + n <= heapSize;
  if (ok && heapBase) heapFree();
  pthread_mutex_unlock(&heapLock);
  return ok;
}

void* __wrap_malloc(size_t n)
{
  return heapFits(n) ? __real_malloc(n) : NULL;
}

void* __wrap_calloc(size_t a, size_t b)
{
  return heapFits(a * b) ? __real_calloc(a, b) : NULL;
}

void* __wrap_realloc(void* p, size_t n)
{
  return heapFits(n) ? __real_realloc(p, n) : NULL;
}

// ESP32SIM_PORTS="1876=21877,1502=21502": the app's ports are bound as the
// mapped ones, and connections to 127.0.0.1 on them are redirected too, so the
// simulator runs next to QEMU, a real svm or other programs on this machine.
// Defined here (not --wrap) so that calls from shared libraries such as
// libmodbus are redirected as well; the real functions come from libc.
typedef int (*sockFn)(int, const struct sockaddr*, socklen_t);

static int realCall(const char* name, int fd, const struct sockaddr* a, socklen_t len)
{
  sockFn f = (sockFn)dlsym(RTLD_NEXT, name);
  if (!f)
  {
    errno = ENOSYS;
    return -1;
  }
  return f(fd, a, len);
}

static uint16_t mapPort(uint16_t port)
{
  const char* p = getenv("ESP32SIM_PORTS");
  while (p && *p)
  {
    char* end;
    long from = strtol(p, &end, 10), to;
    if (*end != '=') break;
    to = strtol(end + 1, &end, 10);
    if (from == port && to > 0 && to < 65536) return (uint16_t)to;
    p = *end == ',' ? end + 1 : NULL;
  }
  return port;
}

static int remap(const struct sockaddr* a, socklen_t len, struct sockaddr_in* out, int loopbackOnly)
{
  uint16_t port, to;
  if (!a || a->sa_family != AF_INET || len < (socklen_t)sizeof(struct sockaddr_in))
    return 0;
  *out = *(const struct sockaddr_in*)a;
  if (loopbackOnly && (ntohl(out->sin_addr.s_addr) >> 24) != 127)
    return 0;
  port = ntohs(out->sin_port);
  to = mapPort(port);
  if (to == port) return 0;
  out->sin_port = htons(to);
  return 1;
}

int bind(int fd, const struct sockaddr* a, socklen_t len)
{
  struct sockaddr_in in;
  if (remap(a, len, &in, 0))
    return realCall("bind", fd, (const struct sockaddr*)&in, sizeof(in));
  return realCall("bind", fd, a, len);
}

int connect(int fd, const struct sockaddr* a, socklen_t len)
{
  struct sockaddr_in in;
  if (remap(a, len, &in, 1))
    return realCall("connect", fd, (const struct sockaddr*)&in, sizeof(in));
  return realCall("connect", fd, a, len);
}

////////////////////////////////////////////////////////////////
// Task watchdog
////////////////////////////////////////////////////////////////

static int wdtSec = 15;
static volatile int wdtSubscribed = 0;
static volatile int64_t wdtFedNs = 0;

esp_err_t esp_task_wdt_add(void* task)
{
  if (wdtSec <= 0) return ESP_ERR_INVALID_STATE;
  wdtFedNs = monoNs();
  wdtSubscribed = 1;
  return ESP_OK;
}

esp_err_t esp_task_wdt_reset(void)
{
  wdtFedNs = monoNs();
  return ESP_OK;
}

esp_err_t esp_task_wdt_delete(void* task)
{
  wdtSubscribed = 0;
  return ESP_OK;
}

static void* wdtThread(void* arg)
{
  for (;;)
  {
    esp32simTaskDelayMs(100);
    if (wdtSubscribed && monoNs() - wdtFedNs > (int64_t)wdtSec * 1000000000LL)
    {
      esp32simLog('E', "task_wdt", "Task watchdog got triggered. The following tasks "
                  "did not reset the watchdog in time:");
      esp32simLog('E', "task_wdt", " - sedona (CPU 1)");
      chipReset(ESP_RST_TASK_WDT);
    }
  }
  return NULL;
}

////////////////////////////////////////////////////////////////
// "app_main"
////////////////////////////////////////////////////////////////

static int usage(void)
{
  printf("esp32sim [--flash DIR] [--heap BYTES] [--wdt SEC] [--tests]\n"
         "         [--restart-delay MS] [--max-resets N] [--no-restart]\n");
  return 2;
}

int main(int argc, char* argv[])
{
  const char* flash = "flash";
  int tests = 0, restartDelayMs = 3000, i, r;
  char path[PATH_MAX];
  struct stat st;
  pthread_t wdt;
  const char* env;

  bootNs = monoNs();
  origArgv = argv;
  if (readlink("/proc/self/exe", selfExe, sizeof(selfExe) - 1) < 0) strcpy(selfExe, "/proc/self/exe");
  setvbuf(stdout, NULL, _IOLBF, 0);
  if ((env = getenv("ESP32SIM_RESET")) != NULL) resetReason = (esp_reset_reason_t)atoi(env);
  if ((env = getenv("ESP32SIM_RESETS")) != NULL) resets = atoi(env);

  for (i = 1; i < argc; ++i)
  {
    const char* a = argv[i];
    const char* v = i + 1 < argc ? argv[i + 1] : NULL;
    if (!strcmp(a, "--flash") && v) { flash = v; ++i; }
    else if (!strcmp(a, "--heap") && v) { heapSize = (size_t)atol(v); ++i; }
    else if (!strcmp(a, "--wdt") && v) { wdtSec = atoi(v); ++i; }
    else if (!strcmp(a, "--restart-delay") && v) { restartDelayMs = atoi(v); ++i; }
    else if (!strcmp(a, "--max-resets") && v) { maxResets = atoi(v); ++i; }
    else if (!strcmp(a, "--no-restart")) noRestart = 1;
    else if (!strcmp(a, "--tests")) tests = 1;
    else return usage();
  }

  if (realpath(flash, path) == NULL || stat(path, &st) != 0 || !S_ISDIR(st.st_mode))
  {
    printf("esp32sim: flash directory '%s' not found\n", flash);
    return 2;
  }
  snprintf(svmEspHome, 64, "%s", path);
  if (strlen(path) >= 64) { printf("esp32sim: flash path too long\n"); return 2; }

  printf("ESP-ROM:esp32s3 (simulated)  rst:%d  boot #%d\n", (int)resetReason, resets);
  esp32simLog('I', "sedona", "reset reason %d, heap %u bytes, flash %s", (int)resetReason,
              (unsigned)heapSize, path);

  heapBase = mallinfo2().uordblks;            // RAM accounting starts here
  if (heapBase == 0) heapBase = 1;

  if (!tests && wdtSec > 0)
  {
    char usec[24];
    snprintf(usec, sizeof(usec), "%lld", (long long)wdtSec * 1000000LL);
    setenv("WATCHDOG_USEC", usec, 1);
  }
  else
    unsetenv("WATCHDOG_USEC");
  vmThread = pthread_self();
  signal(SIGUSR2, onUsr2);
  pthread_create(&wdt, NULL, wdtThread, NULL);

  if (tests)
  {
    char* vmArgv[] = { "svm", "tests.scode", "-test", NULL };
    r = svm_main(3, vmArgv);
    printf("SEDONA TESTS EXIT %d\n", r);
    return r;
  }
  else
  {
    char* vmArgv[] = { "svm", "--plat", NULL };
    r = svm_main(2, vmArgv);
  }
  esp_task_wdt_delete(NULL);
  esp32simLog('E', "sedona", "svm exited (%d), restarting in %d ms", r, restartDelayMs);
  esp32simTaskDelayMs((uint32_t)restartDelayMs);
  chipReset(ESP_RST_SW);
}
