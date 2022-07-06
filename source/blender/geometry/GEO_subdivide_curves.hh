/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_function_ref.hh"
#include "BLI_index_mask.hh"

#include "BKE_curves.hh"

class CurveComponent;

namespace blender::geometry {

/**
 * Add more points along each segment, with the amount of points to add in each segment described
 * by the #cuts input. The new points are equidistant in parameter space, but not in the actual
 * distances.
 *
 * \param selection: A selection of curves to consider when subdividing.
 */
Curves *subdivide_curves(const CurveComponent &src_component,
                         const bke::CurvesGeometry &src_curves,
                         IndexMask selection,
                         const VArray<int> &cuts);

}  // namespace blender::geometry
