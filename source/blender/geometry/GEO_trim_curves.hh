/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_span.hh"
#include "DNA_node_types.h"

#include "BKE_curves.hh"
#include "BKE_curves_utils.hh"
#include "BKE_geometry_set.hh"

namespace blender::geometry {

/*
 * Create a new Curves instance by trimming the input curves. Copying the selected splines
 * between the start and end points.
 */
bke::CurvesGeometry trim_curves(const bke::CurvesGeometry &src_curves,
                                const IndexMask &selection,
                                const VArray<float> &starts,
                                const VArray<float> &ends,
                                GeometryNodeCurveSampleMode mode,
                                const bke::AnonymousAttributePropagationInfo &propagation_info);

}  // namespace blender::geometry
