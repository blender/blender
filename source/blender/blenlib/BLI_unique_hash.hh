/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_vector.hh"

namespace blender {

/**
 * A hash that uniquely identifies data. The hash has to have enough bits to make collisions
 * practically impossible.
 */
struct UniqueHash {
  uint64_t v1;
  uint64_t v2;

  uint64_t hash() const
  {
    return v1;
  }
  friend bool operator==(const UniqueHash &a, const UniqueHash &b) = default;
};

/**
 * Used to collect data to build a #UniqueHash. It is more efficient to hash a span of bytes in
 * one step than to update a hash with more data piece by piece.
 */
struct UniqueHashBytes {
  Vector<std::byte, 256> data;
  /**
   * Add bytes representing a value to the hash. Note that this doesn't account for types with non-
   * unique object representations (i.e. float -0 and +0, or padding bytes).
   */
  template<typename T> void add(const T &value)
  {
    static_assert(std::is_trivially_copyable_v<T>);
    data.extend(reinterpret_cast<const std::byte *>(&value), sizeof(T));
  }
};

template<typename T> void hash_unique_default(const T &value, UniqueHashBytes &hash);

template<typename T>
inline void hash_unique_default(const T &value, UniqueHashBytes &hash)
  requires(std::is_trivially_copyable_v<T>)
{
  hash.add(value);
}

}  // namespace blender
