//
// SedonaPro SVM: scan-cycle hooks (stage 2).
//
// Sys.sleep is called by sys::App.resumeApp once per scan cycle, but also by
// other code (e.g. sproPlatformLinux::UnixPlatformService sleeps 2 ms in the
// free time of a cycle).  The VM stores the frame of the method that calls a
// native (SedonaVM.fp); svmSleepHook uses it to recognise the scan sleep and
// drives from there: update trial, safeboot health, statistics, systemd
// readiness and watchdog.
//

#ifndef __SVM_LOOP_H
#define __SVM_LOOP_H

#include <stdint.h>
#include "sedona.h"

// Process start, after the configuration is loaded.
void svmLoopInit(void);

// A VM run is about to start (also after ERR_RESTART).
void svmLoopVmStarting(void);

// Called at the start of Sys.sleep(ns); vm is NULL for calls from main.c.
void svmSleepHook(SedonaVM* vm, int64_t ns);

// Platform mode, once per process start (option safeboot.*).
void svmSafebootProcessStart(const char* app);

// Mark the current boot history as healthy (counter = 0).
void svmSafebootReset(void);

// sys::App.scanResync: deadline adjustment after an overrun (scan.resync).
int64_t svmScanResync(int64_t nsUntilDeadline, int64_t periodNs);

#endif
