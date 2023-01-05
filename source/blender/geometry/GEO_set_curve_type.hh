/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_function_ref.hh"
#include "BLI_index_mask.hh"

#include "BKE_curves.hh"

namespace blender::geometry {

/**
 * Try the conversion to the #dst_type-- avoiding the majority of the work done in
 * #convert_curves by modifying an existing object in place rather than creating a new one.
 *
 * \note This function is necessary because attributes do not have proper support for CoW.
 *
 * \param get_writable_curves_fn: Should return the write-able curves to change directly if
 * possible. This is a function in order to avoid the cost of retrieval when unnecessary.
 */
bool try_curves_conversion_in_place(IndexMask selection,
                                    CurveType dst_type,
                                    FunctionRef<bke::CurvesGeometry &()> get_writable_curves_fn);

/**
 * Change the types of the selected curves, potentially changing the total point count.
 */
bke::CurvesGeometry convert_curves(const bke::CurvesGeometry &src_curves,
                                   IndexMask selection,
                                   CurveType dst_type,
                                   const bke::AnonymousAttributePropagationInfo &propagation_info);

}  // namespace blender::geometry
