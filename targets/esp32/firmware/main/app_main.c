// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 SedonaPro contributors
//
// SedonaPro Sedona for ESP32-S3: mounts the Sedona file system, starts the
// network and runs the shared svm main() (src/vm/main.c) in its own task.
//
// When the VM exits (error, or a platform restart that main() does not handle
// itself) the chip restarts after CONFIG_SEDONA_RESTART_DELAY_MS, which plays
// the role of systemd Restart= on Linux; svm safeboot/stage rollback count
// these restarts through svm.boots / svm.trial on the file system.
//
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_littlefs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "net.h"

static const char* TAG = "sedona";

int svm_main(int argc, char* argv[]);   // src/vm/main.c

static esp_err_t mountFs(void)
{
  esp_vfs_littlefs_conf_t conf = {
    .base_path = CONFIG_SEDONA_HOME,
    .partition_label = "storage",
    .format_if_mount_failed = true,
    .dont_mount = false,
  };
  size_t total = 0, used = 0;
  esp_err_t err = esp_vfs_littlefs_register(&conf);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "LittleFS mount failed: %s", esp_err_to_name(err));
    return err;
  }
  esp_littlefs_info(conf.partition_label, &total, &used);
  ESP_LOGI(TAG, "LittleFS %s: %u of %u bytes used", CONFIG_SEDONA_HOME,
           (unsigned)used, (unsigned)total);
  return ESP_OK;
}

static void armWatchdog(void)
{
#if CONFIG_SEDONA_WATCHDOG_TIMEOUT > 0
  // svm_loop.c reads WATCHDOG_USEC like under systemd: watchdog=auto turns on
  // and pings every timeout/2 from the scan loop (svm_esp32.c feeds the TWDT)
  char usec[24];
  esp_task_wdt_config_t cfg = {
    .timeout_ms = CONFIG_SEDONA_WATCHDOG_TIMEOUT * 1000,
    .idle_core_mask = 0,
    .trigger_panic = true,
  };
  if (esp_task_wdt_reconfigure(&cfg) != ESP_OK) esp_task_wdt_init(&cfg);
  snprintf(usec, sizeof(usec), "%lld", (long long)CONFIG_SEDONA_WATCHDOG_TIMEOUT * 1000000LL);
  setenv("WATCHDOG_USEC", usec, 1);
#endif
}

static void vmTask(void* arg)
{
  int r;
#if CONFIG_SEDONA_BOOT_TESTS
  char* argv[] = { "svm", "tests.scode", "-test", NULL };
#else
  char* argv[] = { "svm", "--plat", NULL };
#endif
  r = svm_main((int)(sizeof(argv) / sizeof(argv[0])) - 1, argv);
  fflush(stdout);
#if CONFIG_SEDONA_BOOT_TESTS
  printf("SEDONA TESTS EXIT %d\n", r);
  fflush(stdout);
  esp_task_wdt_delete(NULL);
  vTaskDelete(NULL);
#else
  ESP_LOGE(TAG, "svm exited (%d), restarting in %d ms", r, CONFIG_SEDONA_RESTART_DELAY_MS);
  esp_task_wdt_delete(NULL);
  vTaskDelay(pdMS_TO_TICKS(CONFIG_SEDONA_RESTART_DELAY_MS));
  esp_restart();
#endif
}

// The Sedona interpreter reads 16/32-bit operands at odd addresses of the
// scode image.  The ESP32-S3 core does that in hardware; this check catches a
// core or emulator that does not (the VM would then misbehave in odd ways).
static bool unalignedLoadsWork(void)
{
  static uint8_t b[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
  volatile uint8_t* p = b;
  uint16_t u2 = *(volatile uint16_t*)(p + 1);
  uint32_t u4 = *(volatile uint32_t*)(p + 1);
  if (u2 == 0x0302 && u4 == 0x05040302)
    return true;
  ESP_LOGE("svm", "unaligned loads broken: u2=%04x (want 0302) u4=%08lx (want 05040302)",
           u2, (unsigned long)u4);
  return false;
}

void app_main(void)
{
  unalignedLoadsWork();
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    nvs_flash_erase();
    nvs_flash_init();
  }
  ESP_LOGI(TAG, "reset reason %d, free heap %u", (int)esp_reset_reason(),
           (unsigned)esp_get_free_heap_size());

  if (mountFs() != ESP_OK)
  {
    vTaskDelay(pdMS_TO_TICKS(CONFIG_SEDONA_RESTART_DELAY_MS));
    esp_restart();
  }
  netStart();          // non-blocking: SOX binds INADDR_ANY before DHCP is done
#if !CONFIG_SEDONA_BOOT_TESTS
  armWatchdog();
#endif

  xTaskCreatePinnedToCore(vmTask, "sedona", CONFIG_SEDONA_TASK_STACK, NULL,
                          CONFIG_SEDONA_TASK_PRIORITY, NULL, CONFIG_SEDONA_TASK_CORE);
}
