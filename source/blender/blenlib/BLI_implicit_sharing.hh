/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
  /**
   * Number of users that want to own the shared data. This can be in multiple states:
   * - 0: The data is expired and likely freed. It must not be accessed anymore. The
   *      #ImplicitSharingInfo may still be alive when there are weak users.
   * - 1: The data is mutable by the single owner.
   * - >1: The data is shared and therefore immutable.
   */
  mutable std::atomic<int> strong_users_ = 1;
  /**
   * Number of users that only keep a reference to the `ImplicitSharingInfo` but don't need to own
   * the shared data. One additional weak user is added as long as there is at least one strong
   * user. Together with the `version_` below this adds an efficient way to detect if data has been
   * changed.
   */
  mutable std::atomic<int> weak_users_ = 1;
  /**
   * The data referenced by an #ImplicitSharingInfo can change over time. This version is
   * incremented whenever the referenced data is about to be changed. This allows checking if the
   * data has been changed between points in time.
   */
  mutable std::atomic<int64_t> version_ = 0;

 public:
  virtual ~ImplicitSharingInfo()
  {
    BLI_assert(strong_users_ == 0);
    BLI_assert(weak_users_ == 0);
  }

  /** Whether the resource can be modified in place because there is only one owner. */
  bool is_mutable() const
  {
    return strong_users_.load(std::memory_order_relaxed) == 1;
  }

  /**
   * Weak users don't protect the referenced data from being freed. If the data is freed while
   * there is still a weak referenced, this returns true.
   */
  bool is_expired() const
  {
    return strong_users_.load(std::memory_order_acquire) == 0;
  }

  /** Call when a the data has a new additional owner. */
  void add_user() const
  {
    BLI_assert(!this->is_expired());
    strong_users_.fetch_add(1, std::memory_order_relaxed);
  }

  /**
   * Adding a weak owner prevents the #ImplicitSharingInfo from being freed but not the referenced
   * data.
   *
   * \note Unlike std::shared_ptr a weak user cannot be turned into a strong user. This is
   * because some code might change the referenced data assuming that there is only one strong user
   * while a new strong user is added by another thread.
   */
  void add_weak_user() const
  {
    weak_users_.fetch_add(1, std::memory_order_relaxed);
  }

  /**
   * Call this when making sure that the referenced data is mutable, which also implies that it is
   * about to be modified. This allows other code to detect whether data has not been changed very
   * efficiently.
   */
  void tag_ensured_mutable() const
  {
    BLI_assert(this->is_mutable());
    /* This might not need an atomic increment when the #version method below is only called when
     * the code calling it is a strong user of this sharing info. Better be safe and use an atomic
     * for now. */
    version_.fetch_add(1, std::memory_order_acq_rel);
  }

  /**
   * Get a version number that is increased when the data is modified. It can be used to detect if
   * data has been changed.
   */
  int64_t version() const
  {
    return version_.load(std::memory_order_acquire);
  }

  /**
   * Call when the data is no longer needed. This might just decrement the user count, or it might
   * also delete the data if this was the last user.
   */
  void remove_user_and_delete_if_last() const
  {
    const int old_user_count = strong_users_.fetch_sub(1, std::memory_order_acq_rel);
    BLI_assert(old_user_count >= 1);
    const bool was_last_user = old_user_count == 1;
    if (was_last_user) {
      const int old_weak_user_count = weak_users_.load(std::memory_order_acquire);
      BLI_assert(old_weak_user_count >= 1);
      if (old_weak_user_count == 1) {
        /* If the weak user count is 1 it means that there is no actual weak user. The 1 just
         * indicates that there was still at least one strong user. */
        weak_users_ = 0;
        const_cast<ImplicitSharingInfo *>(this)->delete_self_with_data();
      }
      else {
        /* There is still at least one actual weak user, so don't free the sharing info yet. The
         * data can be freed though. */
        const_cast<ImplicitSharingInfo *>(this)->delete_data_only();
        /* Also remove the "fake" weak user that indicated that there was at least one strong
         * user. */
        this->remove_weak_user_and_delete_if_last();
      }
    }
  }

  /**
   * This might just decrement the weak user count or might delete the data. Should be used in
   * conjunction with #add_weak_user.
   */
  void remove_weak_user_and_delete_if_last() const
  {
    const int old_weak_user_count = weak_users_.fetch_sub(1, std::memory_order_acq_rel);
    BLI_assert(old_weak_user_count >= 1);
    const bool was_last_weak_user = old_weak_user_count == 1;
    if (was_last_weak_user) {
      /* It's possible that the data has been freed before already, but now it is definitely freed
       * together with the sharing info. */
      const_cast<ImplicitSharingInfo *>(this)->delete_self_with_data();
    }
  }

 private:
  /** Has to free the #ImplicitSharingInfo and the referenced data. The data might have been freed
   * before by #delete_data_only already. This case should be handled here. */
  virtual void delete_self_with_data() = 0;
  /** Can free the referenced data but the #ImplicitSharingInfo still has to be kept alive. */
  virtual void delete_data_only() {}
};

/**
 * Makes it easy to embed implicit-sharing behavior into a struct. Structs that derive from this
 * class can be used with #ImplicitSharingPtr.
 */
class ImplicitSharingMixin : public ImplicitSharingInfo {

 private:
  void delete_self_with_data() override
  {
    /* Can't use `delete this` here, because we don't know what allocator was used. */
    this->delete_self();
  }

  virtual void delete_self() = 0;
};

/**
 * Utility that contains sharing information and the data that is shared.
 */
struct ImplicitSharingInfoAndData {
  const ImplicitSharingInfo *sharing_info = nullptr;
  const void *data = nullptr;
};

namespace implicit_sharing {

namespace detail {

void *resize_trivial_array_impl(void *old_data,
                                int64_t old_size,
                                int64_t new_size,
                                int64_t alignment,
                                const ImplicitSharingInfo **sharing_info);
void *make_trivial_data_mutable_impl(void *old_data,
                                     int64_t size,
                                     int64_t alignment,
                                     const ImplicitSharingInfo **sharing_info);

}  // namespace detail

/**
 * Copy shared data from the source to the destination, adding a user count.
 * \note Does not free any existing data in the destination.
 */
template<typename T>
void copy_shared_pointer(T *src_ptr,
                         const ImplicitSharingInfo *src_sharing_info,
                         T **r_dst_ptr,
                         const ImplicitSharingInfo **r_dst_sharing_info)
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
template<typename T> void free_shared_data(T **data, const ImplicitSharingInfo **sharing_info)
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
const ImplicitSharingInfo *info_for_mem_free(void *data);

/**
 * Make data mutable (single-user) if it is shared. For trivially-copyable data only.
 */
template<typename T>
void make_trivial_data_mutable(T **data,
                               const ImplicitSharingInfo **sharing_info,
                               const int64_t size)
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
                          const ImplicitSharingInfo **sharing_info,
                          int64_t old_size,
                          int64_t new_size)
{
  *data = static_cast<T *>(detail::resize_trivial_array_impl(
      *data, sizeof(T) * old_size, sizeof(T) * new_size, alignof(T), sharing_info));
}

}  // namespace implicit_sharing

}  // namespace blender
