//
// Analog helpers shared by the Linux HAL backends: IIO ADC and sysfs PWM.
// Copyright 2026 SedonaPro contributors
// Licensed under the Apache License, Version 2.0
//
#ifndef SPRO_HAL_LINUX_H
#define SPRO_HAL_LINUX_H

#include "spro_hal.h"

// read a number from a sysfs/IIO file; 0 = ok
int spro_linux_read_number(const char* path, float* value);
// write a string to a sysfs file; 0 = ok
int spro_linux_write_str(const char* path, const char* text);

// ai: dev = IIO raw file, absolute or relative to /sys/bus/iio/devices
//     ("iio:device0/in_voltage0_raw")
int spro_linux_ai_read(const spro_channel_t* c, float* raw);
// ao: dev = PWM chip ("pwmchip0" or /sys/class/pwm/pwmchip0), pin = PWM channel
int spro_linux_ao_open(const spro_channel_t* c);
int spro_linux_ao_write(const spro_channel_t* c, float duty);

#endif
