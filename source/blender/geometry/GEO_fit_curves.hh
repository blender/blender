/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_curves.hh"

namespace blender::geometry {

enum class FitMethod {
  /**
   * Iteratively removes knots/control points with the least error starting with a dense curve.
   */
  Refit,
  /**
   * Uses a least squares solver to recursively find the control points.
   */
  Split
};

/**
 * Fit the selected curves to Bézier curves.
 *
 * \param src_curves: The input curves.
 * \param curve_selection: A selection of curves to fit. The selected curves will be replaced by
 * the fitted bézier curves and the unselected curves are copied to the output geometry.
 * \param thresholds: A error threshold (fit distance) for each input curve. The fitted curve
 * should be within this distance.
 * \param corners: Boolean value for each input point. When this is true, the point is treated as a
 * corner in the curve fitting. The resulting bézier curve will include this point and the handles
 * will be "free", resulting in a sharp corner.
 * \param method: The fitting algorithm to use. See #FitMethod.
 */
bke::CurvesGeometry fit_poly_to_bezier_curves(const bke::CurvesGeometry &src_curves,
                                              const IndexMask &curve_selection,
                                              const VArray<float> &thresholds,
                                              const VArray<bool> &corners,
                                              FitMethod method,
                                              const bke::AttributeFilter &attribute_filter);

}  // namespace blender::geometry
