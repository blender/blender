/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Range2f {
  float min;
  float max;
} Range2f;

BLI_INLINE bool range2f_in_range(const Range2f *range, const float value)
{
  return IN_RANGE(value, range->min, range->max);
}

#ifdef __cplusplus
}
#endif
