/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_generic_span.hh"
#include "BLI_generic_virtual_array.hh"
#include "BLI_index_mask.hh"
#include "BLI_task.hh"
#include "BLI_virtual_array.hh"

namespace blender::array_utils {

/**
 * Fill the destination span by copying masked values from the `src` array. Threaded based on
 * grain-size.
 */
void copy(const GVArray &src, IndexMask selection, GMutableSpan dst, int64_t grain_size = 4096);

/**
 * Fill the destination span by copying values from the `src` array. Threaded based on
 * grain-size.
 */
template<typename T>
inline void copy(const Span<T> src,
                 const IndexMask selection,
                 MutableSpan<T> dst,
                 const int64_t grain_size = 4096)
{
  BLI_assert(src.size() == dst.size());
  threading::parallel_for(selection.index_range(), grain_size, [&](const IndexRange range) {
    for (const int64_t index : selection.slice(range)) {
      dst[index] = src[index];
    }
  });
}

/**
 * Fill the destination span by gathering indexed values from the `src` array.
 */
void gather(const GVArray &src, IndexMask indices, GMutableSpan dst, int64_t grain_size = 4096);

/**
 * Fill the destination span by gathering indexed values from the `src` array.
 */
void gather(GSpan src, IndexMask indices, GMutableSpan dst, int64_t grain_size = 4096);

/**
 * Fill the destination span by gathering indexed values from the `src` array.
 */
template<typename T>
inline void gather(const VArray<T> &src,
                   const IndexMask indices,
                   MutableSpan<T> dst,
                   const int64_t grain_size = 4096)
{
  BLI_assert(indices.size() == dst.size());
  threading::parallel_for(indices.index_range(), grain_size, [&](const IndexRange range) {
    src.materialize_compressed_to_uninitialized(indices.slice(range), dst.slice(range).data());
  });
}

/**
 * Fill the destination span by gathering indexed values from the `src` array.
 */
template<typename T, typename IndexT>
inline void gather(const Span<T> src,
                   const IndexMask indices,
                   MutableSpan<T> dst,
                   const int64_t grain_size = 4096)
{
  BLI_assert(indices.size() == dst.size());
  threading::parallel_for(indices.index_range(), grain_size, [&](const IndexRange range) {
    for (const int64_t i : range) {
      dst[i] = src[indices[i]];
    }
  });
}

/**
 * Fill the destination span by gathering indexed values from the `src` array.
 */
template<typename T, typename IndexT>
inline void gather(const Span<T> src,
                   const Span<IndexT> indices,
                   MutableSpan<T> dst,
                   const int64_t grain_size = 4096)
{
  BLI_assert(indices.size() == dst.size());
  threading::parallel_for(indices.index_range(), grain_size, [&](const IndexRange range) {
    for (const int64_t i : range) {
      dst[i] = src[indices[i]];
    }
  });
}

/**
 * Fill the destination span by gathering indexed values from the `src` array.
 */
template<typename T, typename IndexT>
inline void gather(const VArray<T> &src,
                   const Span<IndexT> indices,
                   MutableSpan<T> dst,
                   const int64_t grain_size = 4096)
{
  BLI_assert(indices.size() == dst.size());
  devirtualize_varray(src, [&](const auto &src) {
    threading::parallel_for(indices.index_range(), grain_size, [&](const IndexRange range) {
      for (const int64_t i : range) {
        dst[i] = src[indices[i]];
      }
    });
  });
}

void invert_booleans(MutableSpan<bool> span);

}  // namespace blender::array_utils
