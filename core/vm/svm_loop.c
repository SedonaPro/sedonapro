//
// SedonaPro SVM: scan-cycle hooks, statistics, systemd notify/watchdog
// and safe boot (see svm_loop.h, SVM_CONFIG_RU.md).
//

#include "sedona.h"
#include "svm_config.h"
#include "svm_stage.h"
#include "svm_loop.h"

// SVM_SD_NOTIFY: systemd notify over an AF_UNIX datagram socket (Linux).
// On ESP-IDF the messages go to svmPlatformNotify() (esp32 port: task watchdog).
#if !defined(_WIN32) && !defined(ESP_PLATFORM)
#define SVM_SD_NOTIFY 1
#endif

#ifndef _WIN32
#include <errno.h>
#include <stddef.h>
#include <time.h>
#include <unistd.h>
#endif
#ifdef SVM_SD_NOTIFY
#include <sys/socket.h>
#include <sys/un.h>
#endif
#ifdef ESP_PLATFORM
int  svmPlatformNotifyInit(void);          // 0 when READY/WATCHDOG are supported
void svmPlatformNotify(const char* msg);
#endif

#define SCAN_METHOD   "sys::App.resumeApp"
#define BOOTS_FILE    "svm.boots"
#define OTHER_MAX     16

////////////////////////////////////////////////////////////////
// Scan sleep detection
////////////////////////////////////////////////////////////////

static void* scanMethod = NULL;
static void* otherMethods[OTHER_MAX];
static int   otherCount = 0;
static int   noDebugWarned = 0;

static int isScanSleep(SedonaVM* vm)
{
  void* m;
  if (vm == NULL || vm->fp == NULL) return 0;
  m = (vm->fp + 2)->aval;           // method address of the calling frame
  if (m == scanMethod) return 1;
  if (scanMethod != NULL) return 0;

#ifdef SCODE_DEBUG
  {
    int i;
    const char* name;
    for (i = 0; i < otherCount; ++i)
      if (otherMethods[i] == m) return 0;
    name = curMethod(vm, vm->fp);
    if (strcmp(name, SCAN_METHOD) == 0)
    {
      scanMethod = m;
      return 1;
    }
    if (strcmp(name, "unknown") != 0)
    {
      if (otherCount < OTHER_MAX) otherMethods[otherCount++] = m;
      return 0;
    }
  }
#endif
  // scode without debug info: method names are unavailable
  if (!noDebugWarned)
  {
    printf("svm loop: scode has no debug info, every Sys.sleep counts as a scan cycle\n");
    noDebugWarned = 1;
  }
  return 1;
}

////////////////////////////////////////////////////////////////
// systemd notify
////////////////////////////////////////////////////////////////

static int notifyFd = -1;               // >= 0: notify target available
#ifdef SVM_SD_NOTIFY
static struct sockaddr_un notifyAddr;
static socklen_t notifyAddrLen;
#endif

static int64_t wdIntervalNs = 0;      // 0 = watchdog off
static int64_t wdLastNs = 0;
static int readySent = 0;

static void notifyInit(void)
{
#ifdef ESP_PLATFORM
  notifyFd = svmPlatformNotifyInit() == 0 ? 0 : -1;
#endif
#ifdef SVM_SD_NOTIFY
  const char* path = getenv("NOTIFY_SOCKET");
  size_t len;
  if (path == NULL || (path[0] != '/' && path[0] != '@')) return;
  len = strlen(path);
  if (len >= sizeof(notifyAddr.sun_path)) return;
  memset(&notifyAddr, 0, sizeof(notifyAddr));
  notifyAddr.sun_family = AF_UNIX;
  memcpy(notifyAddr.sun_path, path, len);
  if (path[0] == '@') notifyAddr.sun_path[0] = '\0';   // abstract namespace
  notifyAddrLen = (socklen_t)(offsetof(struct sockaddr_un, sun_path) + len);
  notifyFd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
#endif
}

static void notify(const char* msg)
{
#ifdef ESP_PLATFORM
  if (notifyFd >= 0) svmPlatformNotify(msg);
#endif
#ifdef SVM_SD_NOTIFY
  if (notifyFd < 0) return;
  if (sendto(notifyFd, msg, strlen(msg), MSG_NOSIGNAL,
             (struct sockaddr*)&notifyAddr, notifyAddrLen) < 0)
  {
    static int warned = 0;
    if (!warned) { printf("svm notify: %s\n", strerror(errno)); warned = 1; }
  }
#endif
}

static void watchdogInit(void)
{
  const char* usecStr = getenv("WATCHDOG_USEC");
  const char* pidStr  = getenv("WATCHDOG_PID");
  int64_t usec = usecStr ? atoll(usecStr) : 0;

#ifdef SVM_SD_NOTIFY
  if (pidStr != NULL && atol(pidStr) != (long)getpid()) usec = 0;  // not for us
#else
  (void)pidStr;
#endif

  if (svmConfig.watchdog == 0) return;
  if (svmConfig.watchdog < 0 && usec <= 0) return;           // auto: only under systemd
  if (notifyFd < 0)
  {
    printf("svm watchdog: NOTIFY_SOCKET is not set, watchdog disabled\n");
    return;
  }
  wdIntervalNs = usec > 0 ? usec * 1000LL / 2
                          : (int64_t)svmConfig.watchdogSeconds * 1000000000LL;
  printf("svm watchdog: ping every %lld ms from the scan loop\n",
         (long long)(wdIntervalNs / 1000000LL));
}

////////////////////////////////////////////////////////////////
// Statistics
////////////////////////////////////////////////////////////////

typedef struct
{
  int64_t windowStartNs;
  int64_t lastCycleNs;      // 0 after a VM start
  uint64_t cycles;          // scan sleeps in the window
  uint64_t periods;         // measured cycle periods
  int64_t sumNs, minNs, maxNs;
  uint64_t overruns;
  int64_t maxLateNs;
  uint64_t totalCycles, totalOverruns;
  uint64_t resyncs, skippedPeriods;
} Stats;

// lateness of the current cycle reported by svmScanResync, which hides the
// overrun from the following Sys.sleep
static int64_t pendingLateNs = 0;

static Stats st;

static void statsReset(int64_t now)
{
  Stats old = st;
  memset(&st, 0, sizeof(st));
  st.windowStartNs = now;
  st.lastCycleNs = old.lastCycleNs;
  st.totalCycles = old.totalCycles;
  st.totalOverruns = old.totalOverruns;
  st.resyncs = old.resyncs;
  st.skippedPeriods = old.skippedPeriods;
}

static void statsReport(int64_t now)
{
  char line[640];
  double avg = st.periods ? (double)st.sumNs / st.periods / 1e6 : 0.0;
  int n = snprintf(line, sizeof(line),
      "cycles=%llu period_ms_avg=%.3f period_ms_min=%.3f period_ms_max=%.3f "
      "overruns=%llu late_ms_max=%.3f window_s=%.1f total_cycles=%llu total_overruns=%llu "
      "resyncs=%llu skipped_periods=%llu",
      (unsigned long long)st.cycles, avg,
      st.periods ? st.minNs / 1e6 : 0.0, st.periods ? st.maxNs / 1e6 : 0.0,
      (unsigned long long)st.overruns, st.maxLateNs / 1e6,
      (now - st.windowStartNs) / 1e9,
      (unsigned long long)st.totalCycles, (unsigned long long)st.totalOverruns,
      (unsigned long long)st.resyncs, (unsigned long long)st.skippedPeriods);

  printf("svm stats: %s\n", line);
  fflush(stdout);

  if (svmConfig.statsFile[0])
  {
    char buf[800];
    int i, m;
    // one key=value per line
    m = snprintf(buf, sizeof(buf), "time=%lld\n", (long long)time(NULL));
    for (i = 0; i < n && m < (int)sizeof(buf) - 2; ++i)
      buf[m++] = line[i] == ' ' ? '\n' : line[i];
    buf[m++] = '\n';
    svmWriteFileAtomic(svmConfig.statsFile, buf, (size_t)m);
  }

  if (notifyFd >= 0)
  {
    char status[160];
    snprintf(status, sizeof(status),
             "STATUS=scan avg %.1f ms, max %.1f ms, overruns %llu (total %llu)",
             avg, st.periods ? st.maxNs / 1e6 : 0.0,
             (unsigned long long)st.overruns, (unsigned long long)st.totalOverruns);
    notify(status);
  }
}

static void statsCycle(int64_t now, int64_t ns)
{
  st.cycles++;
  st.totalCycles++;
  if (st.lastCycleNs != 0)
  {
    int64_t p = now - st.lastCycleNs;
    if (st.periods == 0 || p < st.minNs) st.minNs = p;
    if (p > st.maxNs) st.maxNs = p;
    st.sumNs += p;
    st.periods++;
  }
  st.lastCycleNs = now;
  if (ns < 0 && -ns > pendingLateNs) pendingLateNs = -ns;
  if (pendingLateNs > 0)
  {
    st.overruns++;
    st.totalOverruns++;
    if (pendingLateNs > st.maxLateNs) st.maxLateNs = pendingLateNs;
    pendingLateNs = 0;
  }
  if (svmConfig.statsInterval > 0 &&
      now - st.windowStartNs >= (int64_t)svmConfig.statsInterval * 1000000000LL)
  {
    statsReport(now);
    statsReset(now);
  }
}

////////////////////////////////////////////////////////////////
// Scan resynchronisation
////////////////////////////////////////////////////////////////

int64_t svmScanResync(int64_t nsUntilDeadline, int64_t periodNs)
{
  static int64_t lastLogNs = 0;
  static uint64_t unlogged = 0;
  int64_t late, missed, adj;

  if (svmConfig.scanResync == 0 || nsUntilDeadline >= 0 || periodNs <= 0) return 0;
  late = -nsUntilDeadline;
  if (late < (int64_t)svmConfig.scanResyncPeriods * periodNs) return 0;

  missed = late / periodNs;                  // whole periods that were lost
  if (svmConfig.scanResync == 1)
    adj = (missed + 1) * periodNs;           // next point of the original grid
  else
    adj = late;                              // next cycle starts now

  st.resyncs++;
  st.skippedPeriods += (uint64_t)missed;
  pendingLateNs = late;

  // log at most every 10 s
  {
    int64_t now = svmMonotonicNs();
    if (lastLogNs == 0 || now - lastLogNs >= 10000000000LL)
    {
      printf("svm scan: overrun %.1f ms, %lld period(s) skipped (%s)",
             late / 1e6, (long long)missed, svmConfig.scanResync == 1 ? "skip" : "now");
      if (unlogged) printf(", %llu more since last report", (unsigned long long)unlogged);
      printf("\n");
      fflush(stdout);
      lastLogNs = now;
      unlogged = 0;
    }
    else unlogged++;
  }
  return adj;
}

////////////////////////////////////////////////////////////////
// Safe boot
////////////////////////////////////////////////////////////////

static int64_t vmStartNs = 0;
static int healthyMarked = 0;
static int safebootCounting = 0;

static int readBoots(void)
{
  int n = 0;
  FILE* f = fopen(BOOTS_FILE, "r");
  if (f == NULL) return 0;
  if (fscanf(f, "%d", &n) != 1) n = 0;
  fclose(f);
  return n < 0 ? 0 : n;
}

static void writeBoots(int n)
{
  char buf[32];
  int len = snprintf(buf, sizeof(buf), "%d\n", n);
  if (svmWriteFileAtomic(BOOTS_FILE, buf, (size_t)len) != 0)
    printf("svm safeboot: cannot write %s: %s\n", BOOTS_FILE, strerror(errno));
}

static int sameFile(const char* a, const char* b)
{
  FILE *fa = fopen(a, "rb"), *fb = fopen(b, "rb");
  int same = fa != NULL && fb != NULL;
  while (same)
  {
    int ca = fgetc(fa), cb = fgetc(fb);
    if (ca != cb) same = 0;
    if (ca == EOF) break;
  }
  if (fa) fclose(fa);
  if (fb) fclose(fb);
  return same;
}

// Replace app with source, keeping the current app as keep.
static int swapApp(const char* app, const char* source, const char* keep)
{
  char why[128];
  if (svmValidateSab(source, why, sizeof(why)) != 0)
  {
    printf("svm safeboot: %s is not usable: %s\n", source, why);
    return -1;
  }
  if (sameFile(app, source))
  {
    printf("svm safeboot: %s is identical to %s, nothing to switch\n", source, app);
    return -1;
  }
  if (rename(app, keep) != 0 && errno != ENOENT)
  {
    printf("svm safeboot: cannot keep %s as %s: %s\n", app, keep, strerror(errno));
    return -1;
  }
  if (svmCopyFileAtomic(source, app) != 0)
  {
    printf("svm safeboot: cannot copy %s: %s\n", source, strerror(errno));
    rename(keep, app);
    return -1;
  }
  printf("svm safeboot: started with %s, previous app kept as %s\n", source, keep);
  return 0;
}

void svmSafebootProcessStart(const char* app)
{
  int n, after = svmConfig.safebootAfter;
  if (after <= 0) return;
  if (svmStageTrialActive())
  {
    printf("svm safeboot: update trial in progress, start not counted\n");
    return;
  }
  safebootCounting = 1;
  n = readBoots() + 1;
  writeBoots(n);
  if (n <= after) return;

  printf("svm safeboot: %d starts without %d s of healthy operation\n",
         n - 1, svmConfig.safebootHealthySeconds);
  if (n == after + 1)
  {
    char bak[256];
    snprintf(bak, sizeof(bak), "%s.bak", app);
    printf("svm safeboot: level 1, switching to the backup app\n");
    swapApp(app, bak, "app.sab.safeboot1");
  }
  else if (n == 2 * after + 1)
  {
    printf("svm safeboot: level 2, switching to safeboot.app\n");
    if (svmConfig.safebootApp[0])
      swapApp(app, svmConfig.safebootApp, "app.sab.safeboot2");
    else
      printf("svm safeboot: safeboot.app is not configured\n");
  }
  else
  {
    printf("svm safeboot: no further safe-boot level, starting normally\n");
  }
  if (notifyFd >= 0) notify("STATUS=safe boot");
  fflush(stdout);
}

void svmSafebootReset(void)
{
  if (svmConfig.safebootAfter <= 0) return;
  if (readBoots() != 0) writeBoots(0);
}

static void safebootCycle(int64_t now)
{
  if (healthyMarked || !safebootCounting) return;
  if (now - vmStartNs < (int64_t)svmConfig.safebootHealthySeconds * 1000000000LL) return;
  healthyMarked = 1;
  svmSafebootReset();
  printf("svm safeboot: start is healthy\n");
  fflush(stdout);
}

////////////////////////////////////////////////////////////////
// Public API
////////////////////////////////////////////////////////////////

void svmLoopInit(void)
{
  notifyInit();
  watchdogInit();
  statsReset(svmMonotonicNs());
}

void svmLoopVmStarting(void)
{
  // code is reloaded on (re)start: method addresses change
  scanMethod = NULL;
  otherCount = 0;
  st.lastCycleNs = 0;
  vmStartNs = svmMonotonicNs();
  healthyMarked = 0;
}

void svmSleepHook(SedonaVM* vm, int64_t ns)
{
  int64_t now;
  if (!isScanSleep(vm)) return;
  now = svmMonotonicNs();

  svmTrialTick();
  safebootCycle(now);

  if (!readySent && notifyFd >= 0)
  {
    notify("READY=1");
    readySent = 1;
  }
  if (wdIntervalNs > 0 && now - wdLastNs >= wdIntervalNs)
  {
    notify("WATCHDOG=1");
    wdLastNs = now;
  }
  statsCycle(now, ns);
}
