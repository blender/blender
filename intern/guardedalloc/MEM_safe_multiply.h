/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstddef>
#include <cstdint>

/** Safe multiplication guarding against overflow, fast in the average case. */
[[nodiscard]] inline bool MEM_size_safe_multiply(size_t a, size_t b, size_t *result)
{
  /* A size_t with its high-half bits all set to 1. */
  const size_t high_bits = SIZE_MAX << (sizeof(size_t) * 8 / 2);
  const size_t product = a * b;

  if (product == 0) {
    if (a == 0 || b == 0) {
      *result = 0;
      return true;
    }
    *result = 0;
    return false;
  }

  /* If both A and B fit in N/2 bits, their product fits in N bits without
   * the possibility of overflow. Otherwise verify with a divide. */
  if ((high_bits & (a | b)) == 0 || product / b == a) {
    *result = product;
    return true;
  }
  *result = 0;
  return false;
}

/** Same as above for signed 64-bit integers. */
[[nodiscard]] inline bool MEM_size_safe_multiply(int64_t a, int64_t b, int64_t *result)
{
  /* Fast path for almost all cases. */
  if ((uint64_t(a) | uint64_t(b)) >> 31 == 0) {
    *result = a * b;
    return true;
  }

  /* Check if resulting size is within range. */
  *result = 0;
  if (a < 0 || b < 0) {
    return false;
  }
  if constexpr (sizeof(size_t) < sizeof(int64_t)) {
    if (uint64_t(a) > SIZE_MAX || uint64_t(b) > SIZE_MAX) {
      return false;
    }
  }

  /* Compute with size_t version. */
  size_t product;
  if (!MEM_size_safe_multiply(size_t(a), size_t(b), &product) ||
      uint64_t(product) > uint64_t(INT64_MAX))
  {
    return false;
  }
  *result = int64_t(product);
  return true;
}
