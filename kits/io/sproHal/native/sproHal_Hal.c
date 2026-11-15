//
// sproHal::Hal natives: channel lookup, inversion and scaling on top of a backend.
// Copyright 2026 SedonaPro contributors
// Licensed under the Apache License, Version 2.0
//
#include "sedona.h"
#include "spro_hal.h"
#include <math.h>
#include <string.h>

static const spro_channel_t* channel(int32_t ch)
{
  return (ch >= 0 && ch < spro_board_channel_count) ? &spro_board_channels[ch] : NULL;
}

// int Hal.count()
Cell sproHal_Hal_count(SedonaVM* vm, Cell* params)
{
  Cell r;
  r.ival = spro_board_channel_count;
  return r;
}

// int Hal.find(Str name)
Cell sproHal_Hal_find(SedonaVM* vm, Cell* params)
{
  const char* name = (const char*)params[0].aval;
  Cell r;
  int i;
  r.ival = -1;
  if (name)
    for (i = 0; i < spro_board_channel_count; ++i)
      if (strcmp(spro_board_channels[i].name, name) == 0)
      {
        r.ival = i;
        break;
      }
  return r;
}

// int Hal.kind(int ch)
Cell sproHal_Hal_kind(SedonaVM* vm, Cell* params)
{
  const spro_channel_t* c = channel(params[0].ival);
  Cell r;
  r.ival = c ? c->kind : SPRO_KIND_NONE;
  return r;
}

// Str Hal.name(int ch)
Cell sproHal_Hal_name(SedonaVM* vm, Cell* params)
{
  const spro_channel_t* c = channel(params[0].ival);
  Cell r;
  r.aval = (void*)(c ? c->name : "");
  return r;
}

// int Hal.open(int ch)
Cell sproHal_Hal_open(SedonaVM* vm, Cell* params)
{
  int32_t ch = params[0].ival;
  const spro_channel_t* c = channel(ch);
  Cell r;
  if (!c)
    r.ival = SPRO_HAL_NO_CHANNEL;
  else
    r.ival = spro_hal_backend_open(ch, c) == 0 ? SPRO_HAL_OK : SPRO_HAL_IO_ERROR;
  return r;
}

// float Hal.read(int ch): engineering value, NaN on error
Cell sproHal_Hal_read(SedonaVM* vm, Cell* params)
{
  int32_t ch = params[0].ival;
  const spro_channel_t* c = channel(ch);
  float raw;
  Cell r;
  r.fval = NAN;
  if (!c || spro_hal_backend_read(ch, c, &raw) != 0)
    return r;
  switch (c->kind)
  {
    case SPRO_KIND_DI:
    case SPRO_KIND_DO:
      r.fval = ((raw != 0.0f) != (c->invert != 0)) ? 1.0f : 0.0f;
      break;
    case SPRO_KIND_AI:
      r.fval = c->rawMax > 0.0f ? c->lo + raw / c->rawMax * (c->hi - c->lo) : raw;
      break;
    case SPRO_KIND_AO:
      r.fval = c->lo + raw * (c->hi - c->lo);
      break;
  }
  return r;
}

// int Hal.write(int ch, float value)
Cell sproHal_Hal_write(SedonaVM* vm, Cell* params)
{
  int32_t ch = params[0].ival;
  float v = params[1].fval;
  const spro_channel_t* c = channel(ch);
  float raw;
  Cell r;
  if (!c)
  {
    r.ival = SPRO_HAL_NO_CHANNEL;
    return r;
  }
  if (c->kind == SPRO_KIND_DO)
    raw = ((v != 0.0f) != (c->invert != 0)) ? 1.0f : 0.0f;
  else if (c->kind == SPRO_KIND_AO)
  {
    if (isnan(v))
      v = c->lo;
    raw = c->hi != c->lo ? (v - c->lo) / (c->hi - c->lo) : 0.0f;
    if (raw < 0.0f) raw = 0.0f;
    if (raw > 1.0f) raw = 1.0f;
  }
  else
  {
    r.ival = SPRO_HAL_WRONG_KIND;
    return r;
  }
  r.ival = spro_hal_backend_write(ch, c, raw) == 0 ? SPRO_HAL_OK : SPRO_HAL_IO_ERROR;
  return r;
}

// Str Hal.board()
Cell sproHal_Hal_board(SedonaVM* vm, Cell* params)
{
  Cell r;
  r.aval = (void*)spro_board_name;
  return r;
}

// Str Hal.backend()
Cell sproHal_Hal_backend(SedonaVM* vm, Cell* params)
{
  Cell r;
  r.aval = (void*)spro_hal_backend_name;
  return r;
}

// int Hal.serialCount(): serial ports of the board file (serial = [...])
Cell sproHal_Hal_serialCount(SedonaVM* vm, Cell* params)
{
  Cell r;
  r.ival = spro_board_serial_count;
  return r;
}

// Str Hal.serialName(int i): device of port i (0-based), "" if none
Cell sproHal_Hal_serialName(SedonaVM* vm, Cell* params)
{
  int32_t i = params[0].ival;
  Cell r;
  r.aval = (void*)((i >= 0 && i < spro_board_serial_count) ? spro_board_serial[i] : "");
  return r;
}
