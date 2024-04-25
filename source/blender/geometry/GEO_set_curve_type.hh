/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_function_ref.hh"
#include "BLI_index_mask.hh"

#include "BKE_curves.hh"

namespace blender::geometry {

struct ConvertCurvesOptions {
  bool convert_bezier_handles_to_poly_points = false;
  bool convert_bezier_handles_to_catmull_rom_points = false;
  /**
   * Make the nurb curve behave like a bezier curve and also keep the handle positions as control
   * points.
   */
  bool keep_bezier_shape_as_nurbs = true;
  /**
   * Keep the exact shape of the catmull rom curve by inserting extra handle control points in the
   * nurbs curve.
   */
  bool keep_catmull_rom_shape_as_nurbs = true;
};

/**
 * Change the types of the selected curves, potentially changing the total point count.
 */
bke::CurvesGeometry convert_curves(const bke::CurvesGeometry &src_curves,
                                   const IndexMask &selection,
                                   CurveType dst_type,
                                   const bke::AnonymousAttributePropagationInfo &propagation_info,
                                   const ConvertCurvesOptions &options = {});

}  // namespace blender::geometry
