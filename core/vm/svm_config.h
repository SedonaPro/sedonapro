//
// SedonaPro SVM runtime configuration.
//
// Every option is optional.  Sources, lowest priority first:
//   1. built-in defaults (legacy behaviour, except clock=monotonic);
//   2. config file: --config=<path>, else $SVM_CONFIG, else ./svm.conf if present;
//   3. environment: SVM_<KEY> with '.' replaced by '_' and upper-cased,
//      e.g. SVM_STAGE_VALIDATE=1;
//   4. command line: --opt=<key>=<value> (repeatable).
//
// See SVM_CONFIG_RU.md for the option reference.
//

#ifndef __SVM_CONFIG_H
#define __SVM_CONFIG_H

#include <stdint.h>

#define SVM_CONFIG_STR_MAX 256

typedef struct
{
  // clock: 1 = CLOCK_MONOTONIC for Sys.ticks/Sys.sleep, 0 = legacy times()
  int clockMonotonic;

  // stack.cells: VM stack size in cells
  int stackCells;

  // stage.validate: check *.stage files before installing them
  int stageValidate;

  // stage.rollback: keep last-known-good files and roll back failed updates
  int stageRollback;
  // stage.trial.seconds: scan-loop run time after which an update is accepted
  int stageTrialSeconds;
  // stage.trial.attempts: process starts allowed during the trial
  int stageTrialAttempts;

  // save.atomic: write matching files through <name>.tmp + fsync + rename
  int saveAtomic;
  // save.atomic.suffixes: comma separated file name suffixes
  char saveAtomicSuffixes[SVM_CONFIG_STR_MAX];
  // save.keep: generations of app.sab.bak kept (1 = only app.sab.bak)
  int saveKeep;

  // crash.handler: log fatal signals with a native backtrace
  int crashHandler;

  // watchdog: -1 auto (ping when systemd sets WATCHDOG_USEC), 0 off, 1 on
  int watchdog;
  // watchdog.seconds: ping interval when systemd gives none
  int watchdogSeconds;

  // stats.interval: seconds between scan statistics reports, 0 = off
  int statsInterval;
  // stats.file: also write the last report to this file ("" = no)
  char statsFile[SVM_CONFIG_STR_MAX];

  // safeboot.after: failed starts before a safe start, 0 = off
  int safebootAfter;
  // safeboot.healthy.seconds: scan-loop run time that resets the counter
  int safebootHealthySeconds;
  // safeboot.app: app file used at the second safe-boot level ("" = none)
  char safebootApp[SVM_CONFIG_STR_MAX];

  // rt.mlockall: lock VM memory in RAM
  int rtMlockall;
  // rt.sched: "other", "fifo:<1..99>" or "rr:<1..99>"
  char rtSched[32];
  // rt.cpus: CPU list for the VM process, e.g. "1" or "0,2-3" ("" = all)
  char rtCpus[SVM_CONFIG_STR_MAX];
  // rt.strict: refuse to start if an rt.* setting cannot be applied
  int rtStrict;

  // scan.resync: 0 off (catch up missed cycles), 1 skip (keep the period
  // grid), 2 now (start the next cycle immediately)
  int scanResync;
  // scan.resync.periods: resynchronise only when at least this many full
  // periods behind
  int scanResyncPeriods;

  // modbus.safe: Modbus Master frees registers of removed points and keeps
  // configuration changes made while a request is in flight
  int modbusSafe;

  // mem.fixes: free memory the original code leaks (expression parser on a
  // syntax error, Modbus Slave register table when the service is removed)
  int memFixes;

  // informational: path of the config file that was loaded ("" if none)
  char configFile[SVM_CONFIG_STR_MAX];
} SvmConfig;

extern SvmConfig svmConfig;

// Returns 0 on success, non-zero if the configuration is invalid.
// argv options recognised here (--config=, --opt=) are left in place;
// use svmConfigIsOwnArg to skip them.
int svmConfigLoad(int argc, char* argv[]);

// Non-zero if arg is a configuration option consumed by svmConfigLoad.
int svmConfigIsOwnArg(const char* arg);

// Print the effective configuration.
void svmConfigPrint(void);

// Non-zero if name ends with one of save.atomic.suffixes.
int svmConfigAtomicName(const char* name);

// Monotonic nanoseconds, independent of clock option (used internally).
int64_t svmMonotonicNs(void);

// Install handlers for fatal signals (crash.handler).
void svmCrashHandlerInstall(void);

// Apply rt.* settings; returns non-zero if rt.strict and one failed.
int svmRtApply(void);

#endif
