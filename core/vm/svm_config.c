//
// SedonaPro SVM runtime configuration (see svm_config.h).
//

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "svm_config.h"

SvmConfig svmConfig =
{
  .clockMonotonic         = 1,
  .stackCells             = 4096,
  .stageValidate          = 0,
  .stageRollback          = 0,
  .stageTrialSeconds      = 120,
  .stageTrialAttempts     = 3,
  .saveAtomic             = 0,
  .saveAtomicSuffixes     = ".sab",
  .saveKeep               = 1,
  .crashHandler           = 0,
  .watchdog               = -1,     // auto
  .watchdogSeconds        = 10,
  .statsInterval          = 0,
  .statsFile              = "",
  .safebootAfter          = 0,
  .safebootHealthySeconds = 60,
  .safebootApp            = "",
  .rtMlockall             = 0,
  .rtSched                = "other",
  .rtCpus                 = "",
  .rtStrict               = 0,
  .scanResync             = 0,
  .scanResyncPeriods      = 1,
  .modbusSafe             = 0,
  .memFixes               = 0,
  .configFile             = "",
};

////////////////////////////////////////////////////////////////
// Value parsers
////////////////////////////////////////////////////////////////

static int parseBool(const char* v, int* out)
{
  if (!strcmp(v, "1") || !strcmp(v, "true") || !strcmp(v, "yes") || !strcmp(v, "on"))
    { *out = 1; return 0; }
  if (!strcmp(v, "0") || !strcmp(v, "false") || !strcmp(v, "no") || !strcmp(v, "off"))
    { *out = 0; return 0; }
  return -1;
}

static int parseInt(const char* v, long min, long max, int* out)
{
  char* end;
  long n;
  errno = 0;
  n = strtol(v, &end, 10);
  if (errno != 0 || end == v || *end != '\0' || n < min || n > max) return -1;
  *out = (int)n;
  return 0;
}

static int parseClock(const char* v, int* out)
{
  if (!strcmp(v, "monotonic")) { *out = 1; return 0; }
  if (!strcmp(v, "legacy"))    { *out = 0; return 0; }
  return -1;
}

static int parseAuto(const char* v, int* out)
{
  if (!strcmp(v, "auto")) { *out = -1; return 0; }
  return parseBool(v, out);
}

// "other" | "fifo:N" | "rr:N" with N in 1..99
static int parseSched(const char* v, char* out)
{
  int prio;
  char* end;
  if (!strcmp(v, "other")) { strcpy(out, v); return 0; }
  if (strncmp(v, "fifo:", 5) && strncmp(v, "rr:", 3)) return -1;
  prio = (int)strtol(strchr(v, ':') + 1, &end, 10);
  if (*end != '\0' || end == strchr(v, ':') + 1 || prio < 1 || prio > 99) return -1;
  if (strlen(v) >= 32) return -1;
  strcpy(out, v);
  return 0;
}

// "" or comma separated CPU numbers and ranges
static int parseCpus(const char* v, char* out)
{
  const char* p = v;
  if (strlen(v) >= SVM_CONFIG_STR_MAX) return -1;
  while (*p)
  {
    if (!isdigit((unsigned char)*p) && *p != ',' && *p != '-') return -1;
    ++p;
  }
  strcpy(out, v);
  return 0;
}

static int parseResync(const char* v, int* out)
{
  if (!strcmp(v, "off"))  { *out = 0; return 0; }
  if (!strcmp(v, "skip")) { *out = 1; return 0; }
  if (!strcmp(v, "now"))  { *out = 2; return 0; }
  return -1;
}

static int parseStr(const char* v, char* out)
{
  if (strlen(v) >= SVM_CONFIG_STR_MAX) return -1;
  strcpy(out, v);
  return 0;
}

////////////////////////////////////////////////////////////////
// Option table
////////////////////////////////////////////////////////////////

typedef enum { T_BOOL, T_INT, T_CLOCK, T_STR, T_AUTO, T_SCHED, T_CPUS, T_RESYNC } OptType;

typedef struct
{
  const char* key;
  OptType type;
  void* target;
  long min, max;
  const char* help;
} Opt;

static const Opt opts[] =
{
  { "clock",                T_CLOCK, &svmConfig.clockMonotonic,     0, 0,
    "monotonic | legacy: time source of Sys.ticks/Sys.sleep" },
  { "stack.cells",          T_INT,   &svmConfig.stackCells,         1024, 1048576,
    "VM stack size in cells" },
  { "stage.validate",       T_BOOL,  &svmConfig.stageValidate,      0, 0,
    "check *.stage files before installing" },
  { "stage.rollback",       T_BOOL,  &svmConfig.stageRollback,      0, 0,
    "keep last-known-good files and roll back failed updates" },
  { "stage.trial.seconds",  T_INT,   &svmConfig.stageTrialSeconds,  5, 86400,
    "scan-loop run time that accepts an update" },
  { "stage.trial.attempts", T_INT,   &svmConfig.stageTrialAttempts, 1, 100,
    "process starts allowed before rollback" },
  { "save.atomic",          T_BOOL,  &svmConfig.saveAtomic,         0, 0,
    "write matching files via .tmp + fsync + rename" },
  { "save.atomic.suffixes", T_STR,   svmConfig.saveAtomicSuffixes,  0, 0,
    "comma separated suffixes for save.atomic" },
  { "save.keep",            T_INT,   &svmConfig.saveKeep,           1, 100,
    "backup generations: app.sab.bak, app.sab.bak.1 .. (1 = legacy)" },
  { "crash.handler",        T_BOOL,  &svmConfig.crashHandler,       0, 0,
    "log fatal signals with a native backtrace" },
  { "watchdog",             T_AUTO,  &svmConfig.watchdog,           0, 0,
    "auto | 1 | 0: systemd watchdog pings from the scan loop" },
  { "watchdog.seconds",     T_INT,   &svmConfig.watchdogSeconds,    1, 3600,
    "ping interval when WATCHDOG_USEC is not set" },
  { "stats.interval",       T_INT,   &svmConfig.statsInterval,      0, 86400,
    "seconds between scan statistics reports (0 = off)" },
  { "stats.file",           T_STR,   svmConfig.statsFile,           0, 0,
    "file that receives the last statistics report" },
  { "safeboot.after",       T_INT,   &svmConfig.safebootAfter,      0, 100,
    "failed starts before a safe start (0 = off)" },
  { "safeboot.healthy.seconds", T_INT, &svmConfig.safebootHealthySeconds, 5, 86400,
    "scan-loop run time that marks a start as healthy" },
  { "safeboot.app",         T_STR,   svmConfig.safebootApp,         0, 0,
    "app used at the second safe-boot level" },
  { "rt.mlockall",          T_BOOL,  &svmConfig.rtMlockall,         0, 0,
    "lock VM memory in RAM" },
  { "rt.sched",             T_SCHED, svmConfig.rtSched,             0, 0,
    "other | fifo:<1..99> | rr:<1..99>" },
  { "rt.cpus",              T_CPUS,  svmConfig.rtCpus,              0, 0,
    "CPU list for the VM, e.g. 1 or 0,2-3" },
  { "rt.strict",            T_BOOL,  &svmConfig.rtStrict,           0, 0,
    "refuse to start if an rt.* setting fails" },
  { "scan.resync",          T_RESYNC, &svmConfig.scanResync,        0, 0,
    "off | skip | now: scan deadline after an overrun" },
  { "scan.resync.periods",  T_INT,   &svmConfig.scanResyncPeriods,  1, 1000,
    "resynchronise when at least this many periods behind" },
  { "modbus.safe",          T_BOOL,  &svmConfig.modbusSafe,         0, 0,
    "Modbus Master: free removed registers, keep config during requests" },
  { "mem.fixes",            T_BOOL,  &svmConfig.memFixes,           0, 0,
    "free memory leaked by the expression parser and Modbus Slave" },
};

#define OPT_COUNT ((int)(sizeof(opts) / sizeof(opts[0])))

static int setOption(const char* key, const char* value, const char* origin)
{
  int i, r = -1;
  for (i = 0; i < OPT_COUNT; ++i)
  {
    const Opt* o = &opts[i];
    if (strcmp(o->key, key) != 0) continue;
    switch (o->type)
    {
      case T_BOOL:  r = parseBool(value, (int*)o->target); break;
      case T_INT:   r = parseInt(value, o->min, o->max, (int*)o->target); break;
      case T_CLOCK: r = parseClock(value, (int*)o->target); break;
      case T_STR:   r = parseStr(value, (char*)o->target); break;
      case T_AUTO:  r = parseAuto(value, (int*)o->target); break;
      case T_SCHED: r = parseSched(value, (char*)o->target); break;
      case T_CPUS:  r = parseCpus(value, (char*)o->target); break;
      case T_RESYNC: r = parseResync(value, (int*)o->target); break;
    }
    if (r != 0)
      fprintf(stderr, "svm config: invalid value '%s' for %s (%s)\n", value, key, origin);
    return r;
  }
  fprintf(stderr, "svm config: unknown option '%s' (%s)\n", key, origin);
  return -1;
}

static char* trim(char* s)
{
  char* e;
  while (isspace((unsigned char)*s)) ++s;
  e = s + strlen(s);
  while (e > s && isspace((unsigned char)e[-1])) --e;
  *e = '\0';
  return s;
}

// Parse "key=value" in place; returns 0 on success.
static int setPair(char* pair, const char* origin)
{
  char* eq = strchr(pair, '=');
  if (eq == NULL)
  {
    fprintf(stderr, "svm config: expected key=value, got '%s' (%s)\n", pair, origin);
    return -1;
  }
  *eq = '\0';
  return setOption(trim(pair), trim(eq + 1), origin);
}

static int loadFile(const char* path, int required)
{
  FILE* f;
  char line[512];
  char origin[SVM_CONFIG_STR_MAX + 32];
  int lineNo = 0, errors = 0;

  f = fopen(path, "r");
  if (f == NULL)
  {
    if (!required) return 0;
    fprintf(stderr, "svm config: cannot open %s: %s\n", path, strerror(errno));
    return -1;
  }
  if (strlen(path) < SVM_CONFIG_STR_MAX) strcpy(svmConfig.configFile, path);

  while (fgets(line, sizeof(line), f) != NULL)
  {
    char* s;
    ++lineNo;
    if (strchr(line, '\n') == NULL && !feof(f))
    {
      fprintf(stderr, "svm config: %s:%d: line too long\n", path, lineNo);
      errors++;
      break;
    }
    s = trim(line);
    if (*s == '\0' || *s == '#' || *s == ';') continue;
    snprintf(origin, sizeof(origin), "%s:%d", path, lineNo);
    if (setPair(s, origin) != 0) errors++;
  }
  fclose(f);
  return errors ? -1 : 0;
}

static int loadEnv(void)
{
  int i, errors = 0;
  for (i = 0; i < OPT_COUNT; ++i)
  {
    char name[64] = "SVM_";
    const char* k = opts[i].key;
    char* p = name + 4;
    const char* v;
    for (; *k && p < name + sizeof(name) - 1; ++k, ++p)
      *p = (*k == '.') ? '_' : (char)toupper((unsigned char)*k);
    *p = '\0';
    v = getenv(name);
    if (v != NULL && setOption(opts[i].key, v, name) != 0) errors++;
  }
  return errors ? -1 : 0;
}

////////////////////////////////////////////////////////////////
// Public API
////////////////////////////////////////////////////////////////

int svmConfigIsOwnArg(const char* arg)
{
  return strncmp(arg, "--config=", 9) == 0 || strncmp(arg, "--opt=", 6) == 0;
}

int svmConfigLoad(int argc, char* argv[])
{
  const char* file = NULL;
  int i, errors = 0;

  for (i = 1; i < argc; ++i)
    if (strncmp(argv[i], "--config=", 9) == 0) file = argv[i] + 9;

  if (file != NULL)
    errors += loadFile(file, 1) != 0;
  else if ((file = getenv("SVM_CONFIG")) != NULL && *file)
    errors += loadFile(file, 1) != 0;
  else
    errors += loadFile("svm.conf", 0) != 0;

  errors += loadEnv() != 0;

  for (i = 1; i < argc; ++i)
  {
    if (strncmp(argv[i], "--opt=", 6) == 0)
    {
      char buf[512];
      if (strlen(argv[i] + 6) >= sizeof(buf))
      {
        fprintf(stderr, "svm config: option too long: %s\n", argv[i]);
        errors++;
        continue;
      }
      strcpy(buf, argv[i] + 6);
      errors += setPair(buf, "command line") != 0;
    }
  }
  return errors;
}

void svmConfigPrint(void)
{
  printf("config:    %s\n", svmConfig.configFile[0] ? svmConfig.configFile : "(defaults)");
  printf("  clock=%s stack.cells=%d crash.handler=%d\n",
         svmConfig.clockMonotonic ? "monotonic" : "legacy",
         svmConfig.stackCells, svmConfig.crashHandler);
  printf("  stage.validate=%d stage.rollback=%d stage.trial.seconds=%d stage.trial.attempts=%d\n",
         svmConfig.stageValidate, svmConfig.stageRollback,
         svmConfig.stageTrialSeconds, svmConfig.stageTrialAttempts);
  printf("  save.atomic=%d save.atomic.suffixes=%s save.keep=%d\n",
         svmConfig.saveAtomic, svmConfig.saveAtomicSuffixes, svmConfig.saveKeep);
  printf("  watchdog=%s watchdog.seconds=%d stats.interval=%d stats.file=%s\n",
         svmConfig.watchdog < 0 ? "auto" : (svmConfig.watchdog ? "1" : "0"),
         svmConfig.watchdogSeconds, svmConfig.statsInterval,
         svmConfig.statsFile[0] ? svmConfig.statsFile : "-");
  printf("  safeboot.after=%d safeboot.healthy.seconds=%d safeboot.app=%s\n",
         svmConfig.safebootAfter, svmConfig.safebootHealthySeconds,
         svmConfig.safebootApp[0] ? svmConfig.safebootApp : "-");
  printf("  rt.mlockall=%d rt.sched=%s rt.cpus=%s rt.strict=%d\n",
         svmConfig.rtMlockall, svmConfig.rtSched,
         svmConfig.rtCpus[0] ? svmConfig.rtCpus : "all", svmConfig.rtStrict);
  printf("  scan.resync=%s scan.resync.periods=%d modbus.safe=%d mem.fixes=%d\n",
         svmConfig.scanResync == 0 ? "off" : (svmConfig.scanResync == 1 ? "skip" : "now"),
         svmConfig.scanResyncPeriods, svmConfig.modbusSafe, svmConfig.memFixes);
}

int svmConfigAtomicName(const char* name)
{
  const char* s = svmConfig.saveAtomicSuffixes;
  size_t nameLen;
  if (!svmConfig.saveAtomic || name == NULL) return 0;
  nameLen = strlen(name);
  while (*s)
  {
    const char* comma = strchr(s, ',');
    size_t len = comma ? (size_t)(comma - s) : strlen(s);
    // skip blanks around the item
    while (len > 0 && isspace((unsigned char)*s)) { ++s; --len; }
    while (len > 0 && isspace((unsigned char)s[len - 1])) --len;
    if (len > 0 && len <= nameLen && strncmp(name + nameLen - len, s, len) == 0)
      return 1;
    if (!comma) break;
    s = comma + 1;
  }
  return 0;
}

int64_t svmMonotonicNs(void)
{
#ifdef _WIN32
  return 0;
#else
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
#endif
}
