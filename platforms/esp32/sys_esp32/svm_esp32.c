// SPDX-License-Identifier: AFL-3.0
// Copyright 2026 SedonaPro contributors
//
// ESP-IDF glue for the shared svm sources (src/vm):
//
//  * relative paths: ESP-IDF has no current directory, so the firmware links
//    with -Wl,--wrap=<fn> and these wrappers put svmEspHome ("/sedona", the
//    LittleFS mount point) in front of relative names; chdir() changes it.
//  * svmCrashHandlerInstall / svmRtApply: the ESP-IDF panic handler already
//    prints a backtrace; task priority and core come from Kconfig.
//  * svm_loop notify: READY=1 subscribes the VM task to the task watchdog,
//    WATCHDOG=1 feeds it (svm.conf watchdog / watchdog.seconds, Kconfig
//    SEDONA_WATCHDOG_TIMEOUT).
//
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "esp_task_wdt.h"
#include "esp_log.h"

#define PATH_LEN 160
static const char* TAG = "svm";

char svmEspHome[64] = "/sedona";

static const char* fix(const char* p, char* buf)
{
  if (p == NULL || p[0] == '/') return p;
  if (strcmp(p, ".") == 0 || p[0] == '\0') return svmEspHome;
  if (p[0] == '.' && p[1] == '/') p += 2;
  snprintf(buf, PATH_LEN, "%s/%s", svmEspHome, p);
  return buf;
}

FILE* __real_fopen(const char* p, const char* mode);
int   __real_rename(const char* a, const char* b);
int   __real_remove(const char* p);
int   __real_unlink(const char* p);
int   __real_stat(const char* p, struct stat* st);
int   __real_open(const char* p, int flags, ...);
int   __real_mkdir(const char* p, mode_t mode);

FILE* __wrap_fopen(const char* p, const char* mode)
{
  char b[PATH_LEN];
  return __real_fopen(fix(p, b), mode);
}

int __wrap_rename(const char* from, const char* to)
{
  char a[PATH_LEN], b[PATH_LEN];
  return __real_rename(fix(from, a), fix(to, b));
}

int __wrap_remove(const char* p)
{
  char b[PATH_LEN];
  return __real_remove(fix(p, b));
}

int __wrap_unlink(const char* p)
{
  char b[PATH_LEN];
  return __real_unlink(fix(p, b));
}

int __wrap_stat(const char* p, struct stat* st)
{
  char b[PATH_LEN];
  return __real_stat(fix(p, b), st);
}

int __wrap_open(const char* p, int flags, ...)
{
  char b[PATH_LEN];
  int mode = 0;
  if (flags & O_CREAT)
  {
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, int);
    va_end(ap);
  }
  return __real_open(fix(p, b), flags, mode);
}

int __wrap_mkdir(const char* p, mode_t mode)
{
  char b[PATH_LEN];
  return __real_mkdir(fix(p, b), mode);
}

// svm --home=<dir>
int __wrap_chdir(const char* p)
{
  char b[PATH_LEN];
  struct stat st;
  const char* full = fix(p, b);
  if (full == NULL || __real_stat(full, &st) != 0 || !S_ISDIR(st.st_mode)) return -1;
  snprintf(svmEspHome, sizeof(svmEspHome), "%s", full);
  return 0;
}

////////////////////////////////////////////////////////////////
// svm_config / svm_loop hooks
////////////////////////////////////////////////////////////////

void svmCrashHandlerInstall(void) { }

int svmRtApply(void) { return 0; }

static int wdSubscribed = 0;

int svmPlatformNotifyInit(void)
{
  return 0;
}

void svmPlatformNotify(const char* msg)
{
  if (strcmp(msg, "WATCHDOG=1") == 0)
  {
    if (wdSubscribed) esp_task_wdt_reset();
  }
  else if (strcmp(msg, "READY=1") == 0)
  {
    // the first scan cycle ran: from now on a stalled scan loop resets the
    // chip (only when svm watchdog pings are on, see app_main.c)
    if (getenv("WATCHDOG_USEC") != NULL && esp_task_wdt_add(NULL) == ESP_OK)
    {
      wdSubscribed = 1;
      ESP_LOGI(TAG, "scan loop running, task watchdog armed");
    }
  }
  else if (strncmp(msg, "STATUS=", 7) == 0)
  {
    ESP_LOGD(TAG, "%s", msg + 7);
  }
}
