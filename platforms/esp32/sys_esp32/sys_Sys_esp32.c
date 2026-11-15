// SPDX-License-Identifier: AFL-3.0
// Copyright 2026 SedonaPro contributors
//
// sys::Sys natives for ESP-IDF: replaces src/sys/native/unix/sys_Sys_unix.c.
//
// Sys.ticks: esp_timer (64-bit microseconds since boot, never wraps in
// practice) in nanoseconds.  The clock option has no legacy variant here.
// Sys.sleep: FreeRTOS delay for whole ticks, busy wait for the rest (the
// scan deadline stays accurate with a 1 kHz tick); the svm scan hooks run
// first, as on Linux.
//
#include "sedona.h"
#include "svm_config.h"
#include "svm_loop.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "rom/ets_sys.h"

Cell sys_Sys_platformType(SedonaVM* vm, Cell* params)
{
  Cell result;
#ifdef PLATFORM_TYPE
  result.aval = PLATFORM_TYPE;
#else
  result.aval = "sproPlatformEsp32::Esp32PlatformService";
#endif
  return result;
}

Cell sys_Sys_sleep(SedonaVM* vm, Cell* params)
{
  int64_t ns = *(int64_t*)params;
  int64_t until, left;

  svmSleepHook(vm, ns);
  if (ns <= 0)
  {
    taskYIELD();
    return nullCell;
  }

  until = esp_timer_get_time() + ns / 1000;
  left  = ns / 1000;
  if (left >= portTICK_PERIOD_MS * 1000)
    vTaskDelay((TickType_t)(left / (portTICK_PERIOD_MS * 1000)));
  left = until - esp_timer_get_time();
  if (left > 0) ets_delay_us((uint32_t)left);
  return nullCell;
}

int64_t sys_Sys_ticks(SedonaVM* vm, Cell* params)
{
  return esp_timer_get_time() * 1000LL;
}
