/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include <memory>

#include "BLI_utildefines.h"

namespace blender {

/**
 * A #GenericKey allows different kinds of keys to be used in the same data-structure like a #Set
 * or #Map.
 *
 * Typically, the key is stored as `std::reference_wrapper<const GenericKey>` in the
 * data-structure. That implies that one has to make sure that the key is not destructed while it's
 * still in use.
 */
class GenericKey {
 public:
  virtual ~GenericKey() = default;

  /** The hash function has to be implemented by the non-abstract subclass. */
  virtual uint64_t hash() const = 0;

  /**
   * Check if the other key is equal to this one. Usually that involves a dynamic_cast to check if
   * it has the same type.
   */
  virtual bool equal_to(const GenericKey &other) const = 0;

  /**
   * For efficiency, it can be good to not always allocate the key if it's just used for lookup.
   * This method allows the key to be converted into a heap-allocated version that can be stored
   * safely if necessary.
   */
  virtual std::unique_ptr<GenericKey> to_storable() const = 0;

  friend bool operator==(const GenericKey &a, const GenericKey &b)
  {
    const bool are_equal = a.equal_to(b);
    /* Ensure that equality check is symmetric. */
    BLI_assert(are_equal == b.equal_to(a));
    return are_equal;
  }

  friend bool operator!=(const GenericKey &a, const GenericKey &b)
  {
    return !(a == b);
  }
};

}  // namespace blender
