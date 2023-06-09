/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_endian_switch.h"
#include "BLI_sys_types.h"
#include "BLI_utildefines.h"

void BLI_endian_switch_int16_array(short *val, const int size)
{
  if (size > 0) {
    int i = size;
    while (i--) {
      BLI_endian_switch_int16(val++);
    }
  }
}

void BLI_endian_switch_uint16_array(ushort *val, const int size)
{
  if (size > 0) {
    int i = size;
    while (i--) {
      BLI_endian_switch_uint16(val++);
    }
  }
}

void BLI_endian_switch_int32_array(int *val, const int size)
{
  if (size > 0) {
    int i = size;
    while (i--) {
      BLI_endian_switch_int32(val++);
    }
  }
}

void BLI_endian_switch_uint32_array(uint *val, const int size)
{
  if (size > 0) {
    int i = size;
    while (i--) {
      BLI_endian_switch_uint32(val++);
    }
  }
}

void BLI_endian_switch_float_array(float *val, const int size)
{
  if (size > 0) {
    int i = size;
    while (i--) {
      BLI_endian_switch_float(val++);
    }
  }
}

void BLI_endian_switch_int64_array(int64_t *val, const int size)
{
  if (size > 0) {
    int i = size;
    while (i--) {
      BLI_endian_switch_int64(val++);
    }
  }
}

void BLI_endian_switch_uint64_array(uint64_t *val, const int size)
{
  if (size > 0) {
    int i = size;
    while (i--) {
      BLI_endian_switch_uint64(val++);
    }
  }
}

void BLI_endian_switch_double_array(double *val, const int size)
{
  if (size > 0) {
    int i = size;
    while (i--) {
      BLI_endian_switch_double(val++);
    }
  }
}
