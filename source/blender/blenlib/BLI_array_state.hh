/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>

#include "BLI_array.hh"
#include "BLI_implicit_sharing_ptr.hh"
#include "BLI_virtual_array.hh"

namespace blender {

/**
 * Remembers the values in an array and allows checking if another array in the future has exactly
 * the same values. This is useful for checking if the topology of a mesh has changed.
 *
 * If possible, this class makes use of implicit-sharing to avoid creating unnecessary copies of
 * the data. This also allows detecting that the array is not changed in constant time in common
 * cases.
 */
template<typename T> class ArrayState {
 private:
  /**
   * The actual values in the remembered array. This may point to data owned by #sharing_info_ or
   * #cached_values_.
   */
  Span<T> values_;
  /** (Shared) ownership of the array in case it supports implicit-sharing. */
  ImplicitSharingPtr<> sharing_info_;
  /** Fallback-copy in the case when the array could not be shared. */
  std::optional<Array<T, 0>> cached_values_;

 public:
  ArrayState() = default;

  ArrayState(const VArray<T> &values, const ImplicitSharingInfo *sharing_info)
  {
    if (values.is_span() && sharing_info) {
      /* Don't create a copy of the array and just take shared ownership. */
      values_ = values.get_internal_span();
      sharing_info->add_user();
      sharing_info_ = ImplicitSharingPtr(sharing_info);
      return;
    }
    /* Create a copy of the array because sharing is not possible. */
    cached_values_.emplace(values.size(), NoInitialization{});
    values.materialize_to_uninitialized(*cached_values_);
    values_ = *cached_values_;
  }

  /**
   * True when the remembered array does not contain any values.
   */
  bool is_empty() const
  {
    return values_.is_empty();
  }

  /**
   * True when the remembered array contains the same values as the given array.
   * This is O(1) in the case when the array was shared and has not been modified.
   * If determining equality in constant time is not possible, the method falls back to comparing
   * the values individually which will take O(n) time.
   */
  bool same_as(const VArray<T> &other_values, const ImplicitSharingInfo *other_sharing_info) const
  {
    if (sharing_info_ && other_sharing_info) {
      if (sharing_info_ == other_sharing_info) {
        /* The data is still shared.*/
        return true;
      }
    }
    if (values_.size() != other_values.size()) {
      /* The arrays can't be the same if their sizes differ. */
      return false;
    }
    /* Need to actually compare all elements. */
    VArraySpan<T> other_values_span(other_values);
    return values_ == other_values_span;
  }
};

}  // namespace blender
