/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include <atomic>

#include "BLI_compiler_attrs.h"
#include "BLI_utildefines.h"
#include "BLI_utility_mixins.hh"

namespace blender {

/**
 * #ImplicitSharingInfo is the core data structure for implicit sharing in Blender. Implicit
 * sharing is a technique that avoids copying data when it is not necessary. This results in better
 * memory usage and performance. Only read-only data can be shared, because otherwise multiple
 * owners might want to change the data in conflicting ways.
 *
 * To determine whether data is shared, #ImplicitSharingInfo keeps a user count. If the count is 1,
 * the data only has a single owner and is therefore mutable. If some code wants to modify data
 * that is currently shared, it has to make a copy first.
 * This behavior is also called "copy on write".
 *
 * In addition to containing the reference count, #ImplicitSharingInfo also knows how to destruct
 * the referenced data. This is important because the code freeing the data in the end might not
 * know how it was allocated (for example, it doesn't know whether an array was allocated using the
 * system or guarded allocator).
 *
 * #ImplicitSharingInfo can be used in two ways:
 * - It can be allocated separately from the referenced data. This is used when the shared data is
 *   e.g. a plain data array.
 * - It can be embedded into another struct. For that it's best to use #ImplicitSharingMixin.
 */
class ImplicitSharingInfo : NonCopyable, NonMovable {
 private:
  mutable std::atomic<int> users_;

 public:
  ImplicitSharingInfo(const int initial_users) : users_(initial_users)
  {
  }

  virtual ~ImplicitSharingInfo()
  {
    BLI_assert(this->is_mutable());
  }

  /** True if there are other const references to the resource, meaning it cannot be modified. */
  bool is_shared() const
  {
    return users_.load(std::memory_order_relaxed) >= 2;
  }

  /** Whether the resource can be modified without a copy because there is only one owner. */
  bool is_mutable() const
  {
    return !this->is_shared();
  }

  /** Call when a the data has a new additional owner. */
  void add_user() const
  {
    users_.fetch_add(1, std::memory_order_relaxed);
  }

  /**
   * Call when the data is no longer needed. This might just decrement the user count, or it might
   * also delete the data if this was the last user.
   */
  void remove_user_and_delete_if_last() const
  {
    const int old_user_count = users_.fetch_sub(1, std::memory_order_acq_rel);
    BLI_assert(old_user_count >= 1);
    const bool was_last_user = old_user_count == 1;
    if (was_last_user) {
      const_cast<ImplicitSharingInfo *>(this)->delete_self_with_data();
    }
  }

 private:
  /** Has to free the #ImplicitSharingInfo and the referenced data. */
  virtual void delete_self_with_data() = 0;
};

/**
 * Makes it easy to embed implicit-sharing behavior into a struct. Structs that derive from this
 * class can be used with #ImplicitSharingPtr.
 */
class ImplicitSharingMixin : public ImplicitSharingInfo {
 public:
  ImplicitSharingMixin() : ImplicitSharingInfo(1)
  {
  }

 private:
  void delete_self_with_data() override
  {
    /* Can't use `delete this` here, because we don't know what allocator was used. */
    this->delete_self();
  }

  virtual void delete_self() = 0;
};

}  // namespace blender
