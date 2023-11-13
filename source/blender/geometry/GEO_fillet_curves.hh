/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_function_ref.hh"
#include "BLI_index_mask.hh"

#include "BKE_curves.hh"

namespace blender::geometry {

bke::CurvesGeometry fillet_curves_poly(
    const bke::CurvesGeometry &src_curves,
    const IndexMask &curve_selection,
    const VArray<float> &radius,
    const VArray<int> &counts,
    bool limit_radius,
    const bke::AnonymousAttributePropagationInfo &propagation_info);

bke::CurvesGeometry fillet_curves_bezier(
    const bke::CurvesGeometry &src_curves,
    const IndexMask &curve_selection,
    const VArray<float> &radius,
    bool limit_radius,
    const bke::AnonymousAttributePropagationInfo &propagation_info);

}  // namespace blender::geometry
