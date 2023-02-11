/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * This is separate from `BLI_index_mask.hh` because it includes headers just `IndexMask` shouldn't
 * depend on.
 */

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_index_mask.hh"
#include "BLI_task.hh"
#include "BLI_vector.hh"
#include "BLI_virtual_array.hh"

namespace blender::index_mask_ops {

namespace detail {
IndexMask find_indices_based_on_predicate__merge(
    IndexMask indices_to_check,
    threading::EnumerableThreadSpecific<Vector<Vector<int64_t>>> &sub_masks,
    Vector<int64_t> &r_indices);
}  // namespace detail

/**
 * Evaluate the #predicate for all indices in #indices_to_check and return a mask that contains all
 * indices where the predicate was true.
 *
 * #r_indices indices is only used if necessary.
 */
template<typename Predicate>
inline IndexMask find_indices_based_on_predicate(const IndexMask indices_to_check,
                                                 const int64_t parallel_grain_size,
                                                 Vector<int64_t> &r_indices,
                                                 const Predicate &predicate)
{
  /* Evaluate predicate in parallel. Since the size of the final mask is not known yet, many
   * smaller vectors have to be filled with all indices where the predicate is true. Those smaller
   * vectors are joined afterwards. */
  threading::EnumerableThreadSpecific<Vector<Vector<int64_t>>> sub_masks;
  threading::parallel_for(
      indices_to_check.index_range(), parallel_grain_size, [&](const IndexRange range) {
        const IndexMask sub_mask = indices_to_check.slice(range);
        Vector<int64_t> masked_indices;
        for (const int64_t i : sub_mask) {
          if (predicate(i)) {
            masked_indices.append(i);
          }
        }
        if (!masked_indices.is_empty()) {
          sub_masks.local().append(std::move(masked_indices));
        }
      });

  /* This part doesn't have to be in the header. */
  return detail::find_indices_based_on_predicate__merge(indices_to_check, sub_masks, r_indices);
}

/**
 * Find the true indices in a virtual array. This is a version of
 * #find_indices_based_on_predicate optimized for a virtual array input.
 *
 * \param parallel_grain_size: The grain size for when the virtual array isn't a span or a single
 * value internally. This should be adjusted based on the expected cost of evaluating the virtual
 * array-- more expensive virtual arrays should have smaller grain sizes.
 */
IndexMask find_indices_from_virtual_array(IndexMask indices_to_check,
                                          const VArray<bool> &virtual_array,
                                          int64_t parallel_grain_size,
                                          Vector<int64_t> &r_indices);

/**
 * Find the true indices in a boolean span.
 */
IndexMask find_indices_from_array(Span<bool> array, Vector<int64_t> &r_indices);

}  // namespace blender::index_mask_ops
