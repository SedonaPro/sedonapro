// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 SedonaPro contributors
// ESP32 simulator: the ESP-IDF API subset used by the Sedona port, on Linux.
#pragma once
#include <stdint.h>
#include <stddef.h>
typedef int esp_err_t;
#define ESP_OK    0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_STATE 0x103

typedef enum {
  ESP_RST_UNKNOWN, ESP_RST_POWERON, ESP_RST_EXT, ESP_RST_SW, ESP_RST_PANIC,
  ESP_RST_INT_WDT, ESP_RST_TASK_WDT, ESP_RST_WDT, ESP_RST_DEEPSLEEP, ESP_RST_BROWNOUT
} esp_reset_reason_t;

int64_t esp_timer_get_time(void);
void esp_restart(void) __attribute__((noreturn));
esp_reset_reason_t esp_reset_reason(void);
void ets_delay_us(uint32_t us);
size_t heap_caps_get_free_size(uint32_t caps);
size_t heap_caps_get_minimum_free_size(uint32_t caps);
esp_err_t esp_task_wdt_add(void* task);
esp_err_t esp_task_wdt_reset(void);
esp_err_t esp_task_wdt_delete(void* task);
void esp32simTaskDelayMs(uint32_t ms);
void esp32simLog(char level, const char* tag, const char* fmt, ...)
  __attribute__((format(printf, 3, 4)));
