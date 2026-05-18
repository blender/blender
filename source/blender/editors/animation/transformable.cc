/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edanimation
 */

#include "BLI_math_base.h"

#include "ED_transformable.hh"

namespace blender::ed {

Array<float> property_interpolated(const Span<float> a, const Span<float> b, const float factor)
{
  BLI_assert(a.size() == b.size());
  Array<float> interpolated(a.size());
  for (const int i : a.index_range()) {
    interpolated[i] = interpf(b[i], a[i], factor);
  }
  return interpolated;
}

}  // namespace blender::ed
