/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_function_ref.hh"
#include "BLI_index_mask.hh"

#include "BKE_curves.hh"

namespace blender::geometry {

/**
 * Change the types of the selected curves, potentially changing the total point count.
 */
bke::CurvesGeometry convert_curves(const bke::CurvesGeometry &src_curves,
                                   const IndexMask &selection,
                                   CurveType dst_type,
                                   const bke::AnonymousAttributePropagationInfo &propagation_info);

}  // namespace blender::geometry
