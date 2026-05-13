/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup io
 *
 * Validation utilities for importers.
 */

#include <climits>
#include <cstdint>

namespace blender::io::validate {

/** Check if size fits in an `int` as used by Blender geometry types. */
inline bool size_fits_in_int(const int64_t size)
{
  return size >= 0 && size <= INT_MAX;
}

/** Check if index fits in the range. */
inline bool index_in_range(const int64_t index, const int64_t size)
{
  return (index >= 0 && index < size);
}

}  // namespace blender::io::validate
