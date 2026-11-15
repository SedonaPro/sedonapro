//
// Analog helpers shared by the Linux HAL backends: IIO ADC and sysfs PWM.
// Copyright 2026 SedonaPro contributors
// Licensed under the Apache License, Version 2.0
//
#include "spro_hal_linux.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int spro_linux_read_number(const char* path, float* value)
{
  char buf[64];
  FILE* f = fopen(path, "r");
  if (!f)
    return -1;
  if (!fgets(buf, sizeof(buf), f))
  {
    fclose(f);
    return -1;
  }
  fclose(f);
  *value = (float)atof(buf);
  return 0;
}

int spro_linux_write_str(const char* path, const char* text)
{
  FILE* f = fopen(path, "w");
  int ok;
  if (!f)
    return -1;
  ok = fputs(text, f) >= 0;
  return (fclose(f) == 0 && ok) ? 0 : -1;
}

int spro_linux_ai_read(const spro_channel_t* c, float* raw)
{
  char path[256];
  if (!c->dev)
    return -1;
  if (c->dev[0] == '/')
    snprintf(path, sizeof(path), "%s", c->dev);
  else
    snprintf(path, sizeof(path), "/sys/bus/iio/devices/%s", c->dev);
  return spro_linux_read_number(path, raw);
}

static void pwmDir(const spro_channel_t* c, char* out, size_t max)
{
  const char* chip = c->dev ? c->dev : "pwmchip0";
  if (chip[0] == '/')
    snprintf(out, max, "%s/pwm%d", chip, c->pin);
  else
    snprintf(out, max, "/sys/class/pwm/%s/pwm%d", chip, c->pin);
}

static unsigned long periodNs(const spro_channel_t* c)
{
  return 1000000000UL / (c->pwmHz ? c->pwmHz : 1000);
}

int spro_linux_ao_open(const spro_channel_t* c)
{
  char dir[200], path[256], num[32];
  pwmDir(c, dir, sizeof(dir));
  if (access(dir, F_OK) != 0)
  {
    const char* chip = c->dev ? c->dev : "pwmchip0";
    if (chip[0] == '/')
      snprintf(path, sizeof(path), "%s/export", chip);
    else
      snprintf(path, sizeof(path), "/sys/class/pwm/%s/export", chip);
    snprintf(num, sizeof(num), "%d", c->pin);
    if (spro_linux_write_str(path, num) != 0)
      return -1;
  }
  snprintf(path, sizeof(path), "%s/period", dir);
  snprintf(num, sizeof(num), "%lu", periodNs(c));
  if (spro_linux_write_str(path, num) != 0)
    return -1;
  snprintf(path, sizeof(path), "%s/enable", dir);
  return spro_linux_write_str(path, "1");
}

int spro_linux_ao_write(const spro_channel_t* c, float duty)
{
  char dir[200], path[256], num[32];
  pwmDir(c, dir, sizeof(dir));
  snprintf(path, sizeof(path), "%s/duty_cycle", dir);
  snprintf(num, sizeof(num), "%lu", (unsigned long)(duty * (float)periodNs(c)));
  return spro_linux_write_str(path, num);
}
