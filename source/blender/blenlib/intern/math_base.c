/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

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

double double_round(double x, int ndigits)
{
  /* From Python 3.1 `floatobject.c`. */

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

float floor_power_of_10(float f)
{
  BLI_assert(!(f < 0.0f));
  if (f != 0.0f) {
    return 1.0f / (powf(10.0f, ceilf(log10f(1.0f / f))));
  }
  return 0.0f;
}

float ceil_power_of_10(float f)
{
  BLI_assert(!(f < 0.0f));
  if (f != 0.0f) {
    return 1.0f / (powf(10.0f, floorf(log10f(1.0f / f))));
  }
  return 0.0f;
}
