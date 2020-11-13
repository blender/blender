/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: some of this file.
 *
 * */

/** \file
 * \ingroup bli
 */

#include "BLI_math.h"

#include "BLI_strict_flags.h"

int pow_i(int base, int exp)
{
  int result = 1;
  BLI_assert(exp >= 0);
  while (exp) {
    if (exp & 1) {
      result *= base;
    }
    exp >>= 1;
    base *= base;
  }

  return result;
}

/* from python 3.1 floatobject.c
 * ndigits must be between 0 and 21 */
double double_round(double x, int ndigits)
{
  double pow1, pow2, y, z;
  if (ndigits >= 0) {
    pow1 = pow(10.0, (double)ndigits);
    pow2 = 1.0;
    y = (x * pow1) * pow2;
    /* if y overflows, then rounded value is exactly x */
    if (!isfinite(y)) {
      return x;
    }
  }
  else {
    pow1 = pow(10.0, (double)-ndigits);
    pow2 = 1.0; /* unused; silences a gcc compiler warning */
    y = x / pow1;
  }

  z = round(y);
  if (fabs(y - z) == 0.5) {
    /* halfway between two integers; use round-half-even */
    z = 2.0 * round(y / 2.0);
  }

  if (ndigits >= 0) {
    z = (z / pow2) / pow1;
  }
  else {
    z *= pow1;
  }

  /* if computation resulted in overflow, raise OverflowError */
  return z;
}

/**
 * Floor to the nearest power of 10, e.g.:
 * - 15.0 -> 10.0
 * - 0.015 -> 0.01
 * - 1.0 -> 1.0
 *
 * \param f: Value to floor, must be over 0.0.
 * \note If we wanted to support signed values we could if this becomes necessary.
 */
float floor_power_of_10(float f)
{
  BLI_assert(!(f < 0.0f));
  if (f != 0.0f) {
    return 1.0f / (powf(10.0f, ceilf(log10f(1.0f / f))));
  }
  return 0.0f;
}

/**
 * Ceiling to the nearest power of 10, e.g.:
 * - 15.0 -> 100.0
 * - 0.015 -> 0.1
 * - 1.0 -> 1.0
 *
 * \param f: Value to ceiling, must be over 0.0.
 * \note If we wanted to support signed values we could if this becomes necessary.
 */
float ceil_power_of_10(float f)
{
  BLI_assert(!(f < 0.0f));
  if (f != 0.0f) {
    return 1.0f / (powf(10.0f, floorf(log10f(1.0f / f))));
  }
  return 0.0f;
}
