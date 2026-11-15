//
// SedonaPro HAL backend "linux-gpiod": GPIO character device (kernel uAPI v2,
// Linux 5.10+), no library needed.  ai: IIO, ao: sysfs PWM (spro_hal_linux.c).
// Copyright 2026 SedonaPro contributors
// Licensed under the Apache License, Version 2.0
//
// Board file: dev = "gpiochip0" (default) or "/dev/gpiochipN", pin = line offset.
//
#include "sedona.h"
#include "spro_hal.h"
#include "spro_hal_linux.h"

#include <fcntl.h>
#include <linux/gpio.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define MAX_LINES 128

const char* const spro_hal_backend_name = "linux-gpiod";

static int lineFd[MAX_LINES];
static int inited = 0;

static int openLine(const spro_channel_t* c, int output, int value)
{
  char path[64];
  const char* chip = c->dev ? c->dev : "gpiochip0";
  struct gpio_v2_line_request req;
  int fd, r;

  if (chip[0] == '/')
    snprintf(path, sizeof(path), "%s", chip);
  else
    snprintf(path, sizeof(path), "/dev/%s", chip);
  fd = open(path, O_RDWR | O_CLOEXEC);
  if (fd < 0)
  {
    printf("-- sproHal gpiod: %s: cannot open %s\n", c->name, path);
    fflush(stdout);
    return -1;
  }
  memset(&req, 0, sizeof(req));
  req.offsets[0] = (__u32)c->pin;
  req.num_lines = 1;
  snprintf(req.consumer, sizeof(req.consumer), "sedonapro %s", c->name);
  if (output)
  {
    req.config.flags = GPIO_V2_LINE_FLAG_OUTPUT;
    req.config.num_attrs = 1;
    req.config.attrs[0].attr.id = GPIO_V2_LINE_ATTR_ID_OUTPUT_VALUES;
    req.config.attrs[0].attr.values = value ? 1 : 0;
    req.config.attrs[0].mask = 1;
  }
  else
  {
    req.config.flags = GPIO_V2_LINE_FLAG_INPUT |
      (c->pull == SPRO_PULL_UP ? GPIO_V2_LINE_FLAG_BIAS_PULL_UP :
       c->pull == SPRO_PULL_DOWN ? GPIO_V2_LINE_FLAG_BIAS_PULL_DOWN : 0);
  }
  r = ioctl(fd, GPIO_V2_GET_LINE_IOCTL, &req);
  close(fd);
  if (r < 0)
  {
    printf("-- sproHal gpiod: %s: line %d of %s is busy or missing\n", c->name, c->pin, path);
    fflush(stdout);
    return -1;
  }
  return req.fd;
}

int spro_hal_backend_open(int index, const spro_channel_t* c)
{
  int i;
  if (!inited)
  {
    for (i = 0; i < MAX_LINES; ++i)
      lineFd[i] = -1;
    inited = 1;
  }
  if (index >= MAX_LINES)
    return -1;
  switch (c->kind)
  {
    case SPRO_KIND_DI:
    case SPRO_KIND_DO:
      if (lineFd[index] >= 0)
        return 0;
      lineFd[index] = openLine(c, c->kind == SPRO_KIND_DO,
                               ((c->initial != 0.0f) != (c->invert != 0)));
      return lineFd[index] >= 0 ? 0 : -1;
    case SPRO_KIND_AI:
      return c->dev ? 0 : -1;
    case SPRO_KIND_AO:
      return spro_linux_ao_open(c);
  }
  return -1;
}

int spro_hal_backend_read(int index, const spro_channel_t* c, float* raw)
{
  struct gpio_v2_line_values v;
  switch (c->kind)
  {
    case SPRO_KIND_DI:
    case SPRO_KIND_DO:
      if (index >= MAX_LINES || lineFd[index] < 0)
        return -1;
      memset(&v, 0, sizeof(v));
      v.mask = 1;
      if (ioctl(lineFd[index], GPIO_V2_LINE_GET_VALUES_IOCTL, &v) < 0)
        return -1;
      *raw = (v.bits & 1) ? 1.0f : 0.0f;
      return 0;
    case SPRO_KIND_AI:
      return spro_linux_ai_read(c, raw);
  }
  return -1;
}

int spro_hal_backend_write(int index, const spro_channel_t* c, float raw)
{
  struct gpio_v2_line_values v;
  if (c->kind == SPRO_KIND_DO)
  {
    if (index >= MAX_LINES || lineFd[index] < 0)
      return -1;
    v.mask = 1;
    v.bits = raw != 0.0f ? 1 : 0;
    return ioctl(lineFd[index], GPIO_V2_LINE_SET_VALUES_IOCTL, &v) < 0 ? -1 : 0;
  }
  if (c->kind == SPRO_KIND_AO)
    return spro_linux_ao_write(c, raw);
  return -1;
}
