//
// SedonaPro HAL backend "esp32": ESP-IDF drivers.
//   di/do  driver/gpio       pin = GPIO number, pull up/down, do read back
//   ai     esp_adc oneshot   pin = ADC-capable GPIO, raw 0..4095 (12 dB attenuation)
//   ao     driver/ledc PWM   pin = GPIO, pwmHz; 10-bit duty; up to 8 channels,
//                            one LEDC timer per distinct frequency (max 4)
// Copyright 2026 SedonaPro contributors
// Licensed under the Apache License, Version 2.0
//
#include "sedona.h"
#include "spro_hal.h"

#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#if CONFIG_SEDONA_HAL_ADC
#include "esp_adc/adc_oneshot.h"
#endif

#define MAX_CH 128
static const char* TAG = "sproHal";

const char* const spro_hal_backend_name = "esp32";

#if CONFIG_SEDONA_HAL_ADC
static adc_oneshot_unit_handle_t adcUnit[2];
#endif
static int8_t adcChan[MAX_CH];
static int8_t adcUnitOf[MAX_CH];
static int8_t ledcChan[MAX_CH];
static uint32_t timerHz[LEDC_TIMER_MAX];
static int nextLedc = 0;
static int inited = 0;

static void init(void)
{
  int i;
  for (i = 0; i < MAX_CH; ++i)
    adcChan[i] = adcUnitOf[i] = ledcChan[i] = -1;
  inited = 1;
}

static int openAdc(int index, const spro_channel_t* c)
{
#if CONFIG_SEDONA_HAL_ADC
  adc_unit_t unit;
  adc_channel_t chan;
  adc_oneshot_chan_cfg_t cfg = { .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT };
  if (adc_oneshot_io_to_channel(c->pin, &unit, &chan) != ESP_OK)
  {
    ESP_LOGE(TAG, "%s: GPIO%d has no ADC", c->name, c->pin);
    return -1;
  }
  if (!adcUnit[unit])
  {
    adc_oneshot_unit_init_cfg_t ucfg = { .unit_id = unit };
    if (adc_oneshot_new_unit(&ucfg, &adcUnit[unit]) != ESP_OK)
      return -1;
  }
  if (adc_oneshot_config_channel(adcUnit[unit], chan, &cfg) != ESP_OK)
    return -1;
  adcUnitOf[index] = (int8_t)unit;
  adcChan[index] = (int8_t)chan;
  return 0;
#else
  ESP_LOGW(TAG, "%s: ADC disabled (CONFIG_SEDONA_HAL_ADC)", c->name);
  return -1;
#endif
}

static int openPwm(int index, const spro_channel_t* c)
{
  uint32_t hz = c->pwmHz ? c->pwmHz : 1000;
  int t;
  if (ledcChan[index] >= 0)
    return 0;
  if (nextLedc >= LEDC_CHANNEL_MAX)
  {
    ESP_LOGE(TAG, "%s: no free LEDC channel", c->name);
    return -1;
  }
  for (t = 0; t < LEDC_TIMER_MAX && timerHz[t] && timerHz[t] != hz; ++t)
    ;
  if (t == LEDC_TIMER_MAX)
  {
    ESP_LOGE(TAG, "%s: more than %d PWM frequencies", c->name, LEDC_TIMER_MAX);
    return -1;
  }
  if (!timerHz[t])
  {
    ledc_timer_config_t tc = {
      .speed_mode = LEDC_LOW_SPEED_MODE, .duty_resolution = LEDC_TIMER_10_BIT,
      .timer_num = (ledc_timer_t)t, .freq_hz = hz, .clk_cfg = LEDC_AUTO_CLK };
    if (ledc_timer_config(&tc) != ESP_OK)
      return -1;
    timerHz[t] = hz;
  }
  ledc_channel_config_t cc = {
    .gpio_num = c->pin, .speed_mode = LEDC_LOW_SPEED_MODE, .channel = (ledc_channel_t)nextLedc,
    .timer_sel = (ledc_timer_t)t, .duty = 0, .hpoint = 0 };
  if (ledc_channel_config(&cc) != ESP_OK)
    return -1;
  ledcChan[index] = (int8_t)nextLedc++;
  return 0;
}

int spro_hal_backend_open(int index, const spro_channel_t* c)
{
  if (!inited)
    init();
  if (index >= MAX_CH || c->pin < 0)
    return -1;
  switch (c->kind)
  {
    case SPRO_KIND_DI:
      gpio_reset_pin(c->pin);
      gpio_set_direction(c->pin, GPIO_MODE_INPUT);
      gpio_set_pull_mode(c->pin, c->pull == SPRO_PULL_UP ? GPIO_PULLUP_ONLY :
                                 c->pull == SPRO_PULL_DOWN ? GPIO_PULLDOWN_ONLY : GPIO_FLOATING);
      return 0;
    case SPRO_KIND_DO:
      gpio_reset_pin(c->pin);
      gpio_set_level(c->pin, (c->initial != 0.0f) != (c->invert != 0));
      return gpio_set_direction(c->pin, GPIO_MODE_INPUT_OUTPUT) == ESP_OK ? 0 : -1;
    case SPRO_KIND_AI:
      return adcChan[index] >= 0 ? 0 : openAdc(index, c);
    case SPRO_KIND_AO:
      return openPwm(index, c);
  }
  return -1;
}

int spro_hal_backend_read(int index, const spro_channel_t* c, float* raw)
{
  int v;
  switch (c->kind)
  {
    case SPRO_KIND_DI:
    case SPRO_KIND_DO:
      *raw = gpio_get_level(c->pin) ? 1.0f : 0.0f;
      return 0;
    case SPRO_KIND_AI:
#if CONFIG_SEDONA_HAL_ADC
      if (adcChan[index] < 0 || adc_oneshot_read(adcUnit[adcUnitOf[index]], adcChan[index], &v) != ESP_OK)
        return -1;
      *raw = (float)v;
      return 0;
#else
      (void)v;
      return -1;
#endif
    case SPRO_KIND_AO:
      if (ledcChan[index] < 0)
        return -1;
      *raw = (float)ledc_get_duty(LEDC_LOW_SPEED_MODE, ledcChan[index]) / 1023.0f;
      return 0;
  }
  return -1;
}

int spro_hal_backend_write(int index, const spro_channel_t* c, float raw)
{
  if (c->kind == SPRO_KIND_DO)
    return gpio_set_level(c->pin, raw != 0.0f) == ESP_OK ? 0 : -1;
  if (c->kind == SPRO_KIND_AO && ledcChan[index] >= 0)
  {
    if (ledc_set_duty(LEDC_LOW_SPEED_MODE, ledcChan[index], (uint32_t)(raw * 1023.0f + 0.5f)) != ESP_OK)
      return -1;
    return ledc_update_duty(LEDC_LOW_SPEED_MODE, ledcChan[index]) == ESP_OK ? 0 : -1;
  }
  return -1;
}
