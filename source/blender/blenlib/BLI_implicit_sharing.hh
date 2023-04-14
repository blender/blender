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
  ImplicitSharingInfo(const int initial_users) : users_(initial_users) {}

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
  ImplicitSharingMixin() : ImplicitSharingInfo(1) {}

 private:
  void delete_self_with_data() override
  {
    /* Can't use `delete this` here, because we don't know what allocator was used. */
    this->delete_self();
  }

  virtual void delete_self() = 0;
};

namespace implicit_sharing {

namespace detail {

void *resize_trivial_array_impl(void *old_data,
                                int64_t old_size,
                                int64_t new_size,
                                int64_t alignment,
                                ImplicitSharingInfo **sharing_info);
void *make_trivial_data_mutable_impl(void *old_data,
                                     int64_t size,
                                     int64_t alignment,
                                     ImplicitSharingInfo **sharing_info);

}  // namespace detail

/**
 * Copy shared data from the source to the destination, adding a user count.
 * \note Does not free any existing data in the destination.
 */
template<typename T>
void copy_shared_pointer(T *src_ptr,
                         ImplicitSharingInfo *src_sharing_info,
                         T **r_dst_ptr,
                         ImplicitSharingInfo **r_dst_sharing_info)
{
  *r_dst_ptr = src_ptr;
  *r_dst_sharing_info = src_sharing_info;
  if (*r_dst_ptr) {
    BLI_assert(*r_dst_sharing_info != nullptr);
    (*r_dst_sharing_info)->add_user();
  }
}

/**
 * Remove this reference to the shared data and remove dangling pointers.
 */
template<typename T> void free_shared_data(T **data, ImplicitSharingInfo **sharing_info)
{
  if (*sharing_info) {
    BLI_assert(*data != nullptr);
    (*sharing_info)->remove_user_and_delete_if_last();
  }
  *data = nullptr;
  *sharing_info = nullptr;
}

/**
 * Create an implicit sharing object that takes ownership of the data, allowing it to be shared.
 * When it is no longer used, the data is freed with #MEM_freeN, so it must be a trivial type.
 */
ImplicitSharingInfo *info_for_mem_free(void *data);

/**
 * Make data mutable (single-user) if it is shared. For trivially-copyable data only.
 */
template<typename T>
void make_trivial_data_mutable(T **data, ImplicitSharingInfo **sharing_info, const int64_t size)
{
  *data = static_cast<T *>(
      detail::make_trivial_data_mutable_impl(*data, sizeof(T) * size, alignof(T), sharing_info));
}

/**
 * Resize an array of shared data. For trivially-copyable data only. Any new values are not
 * initialized.
 */
template<typename T>
void resize_trivial_array(T **data,
                          ImplicitSharingInfo **sharing_info,
                          int64_t old_size,
                          int64_t new_size)
{
  *data = static_cast<T *>(detail::resize_trivial_array_impl(
      *data, sizeof(T) * old_size, sizeof(T) * new_size, alignof(T), sharing_info));
}

}  // namespace implicit_sharing

}  // namespace blender
