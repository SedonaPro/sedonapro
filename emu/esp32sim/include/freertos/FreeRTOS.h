// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 SedonaPro contributors
#pragma once
#include "esp32sim.h"
typedef uint32_t TickType_t;
#define portTICK_PERIOD_MS 1          // CONFIG_FREERTOS_HZ=1000 as in the firmware
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
