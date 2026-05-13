/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_resource_scope.hh"
#include "BLI_virtual_array.hh"

namespace blender {

/**
 * This allows efficiently accessing a single-value virtual array as span, assuming that only ever
 * a partial span is necessary. Specifically, this is more efficient than materializing the full
 * single-value #VArray into a large array.
 */
template<typename T> class VArrayRangeSpans {
 private:
  int64_t max_span_size_ = 0;
  std::optional<Span<T>> full_span_;
  std::optional<Span<T>> chunk_span_;

 public:
  VArrayRangeSpans() = default;

  VArrayRangeSpans(ResourceScope &scope, const VArray<T> &varray, const int max_range_size)
      : max_span_size_(max_range_size)
  {
    if (varray.is_span()) {
      /* This copy is generally very cheap. */
      if (varray.common_info().may_have_ownership) {
        const VArray<T> &owned_varray = scope.construct<VArray<T>>(varray);
        full_span_ = owned_varray.get_internal_span();
      }
      else {
        full_span_ = varray.get_internal_span();
      }
    }
    else if (const std::optional<T> single_value = varray.get_if_single()) {
      chunk_span_ = scope.allocator().construct_array<T>(max_range_size, *single_value);
    }
    else {
      MutableSpan<T> full_span = scope.allocator().allocate_array<T>(varray.size());
      varray.materialize_to_uninitialized(full_span);
      full_span_ = full_span;
      scope.add_destruct_call([full_span]() { destruct_n(full_span.data(), full_span.size()); });
    }
  }

  Span<T> get_span_for_range(const IndexRange range) const
  {
    BLI_assert(full_span_ || chunk_span_);
    const int range_size = range.size();
    BLI_assert(range_size <= max_span_size_);
    if (full_span_) {
      return full_span_->slice(range);
    }
    return chunk_span_->take_front(range_size);
  }
};

}  // namespace blender
