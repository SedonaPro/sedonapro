// SPDX-License-Identifier: AFL-3.0
// Copyright 2026 SedonaPro contributors
//
// sys::PlatformService natives for ESP-IDF.
//
#include "sedona.h"
#include "sedonaPlatform.h"
#include "esp_heap_caps.h"

Cell sys_PlatformService_doPlatformId(SedonaVM* vm, Cell* params)
{
  Cell result;
  result.aval = PLATFORM_ID;
  return result;
}

Cell sys_PlatformService_getPlatVersion(SedonaVM* vm, Cell* params)
{
  Cell result;
  result.aval = PLAT_BUILD_VERSION;
  return result;
}

// free heap in bytes (internal RAM and PSRAM)
int64_t sys_PlatformService_getNativeMemAvailable(SedonaVM* vm, Cell* params)
{
  return (int64_t)heap_caps_get_free_size(MALLOC_CAP_8BIT);
}
