//
// SedonaPro HAL backend "linux-sysfs": legacy /sys/class/gpio (old kernels,
// BeagleBone/i.MX6 images).  New boards should use linux-gpiod.
// Copyright 2026 SedonaPro contributors
// Licensed under the Apache License, Version 2.0
//
// Board file: pin = global GPIO number (as in /sys/class/gpio/gpioN).
//
#include "sedona.h"
#include "spro_hal.h"
#include "spro_hal_linux.h"

#include <stdio.h>
#include <unistd.h>

const char* const spro_hal_backend_name = "linux-sysfs";

static void gpioPath(const spro_channel_t* c, const char* file, char* out, size_t max)
{
  snprintf(out, max, "/sys/class/gpio/gpio%d/%s", c->pin, file);
}

int spro_hal_backend_open(int index, const spro_channel_t* c)
{
  char path[96], num[16];
  if (c->kind == SPRO_KIND_AI)
    return c->dev ? 0 : -1;
  if (c->kind == SPRO_KIND_AO)
    return spro_linux_ao_open(c);
  gpioPath(c, "value", path, sizeof(path));
  if (access(path, F_OK) != 0)
  {
    snprintf(num, sizeof(num), "%d", c->pin);
    if (spro_linux_write_str("/sys/class/gpio/export", num) != 0)
      return -1;
  }
  gpioPath(c, "direction", path, sizeof(path));
  if (c->kind == SPRO_KIND_DO)
    return spro_linux_write_str(path, ((c->initial != 0.0f) != (c->invert != 0)) ? "high" : "low");
  return spro_linux_write_str(path, "in");
}

int spro_hal_backend_read(int index, const spro_channel_t* c, float* raw)
{
  char path[96];
  if (c->kind == SPRO_KIND_AI)
    return spro_linux_ai_read(c, raw);
  gpioPath(c, "value", path, sizeof(path));
  return spro_linux_read_number(path, raw);
}

int spro_hal_backend_write(int index, const spro_channel_t* c, float raw)
{
  char path[96];
  if (c->kind == SPRO_KIND_AO)
    return spro_linux_ao_write(c, raw);
  gpioPath(c, "value", path, sizeof(path));
  return spro_linux_write_str(path, raw != 0.0f ? "1" : "0");
}
