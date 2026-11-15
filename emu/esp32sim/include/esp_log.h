// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 SedonaPro contributors
#pragma once
#include "esp32sim.h"
#define ESP_LOGE(tag, ...) esp32simLog('E', tag, __VA_ARGS__)
#define ESP_LOGW(tag, ...) esp32simLog('W', tag, __VA_ARGS__)
#define ESP_LOGI(tag, ...) esp32simLog('I', tag, __VA_ARGS__)
#define ESP_LOGD(tag, ...) do { } while (0)
