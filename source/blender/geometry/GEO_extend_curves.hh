/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_span.hh"
#include "DNA_node_types.h"

#include "BKE_curves.hh"
#include "BKE_curves_utils.hh"
#include "BKE_geometry_set.hh"

namespace blender::geometry {

/*
 * Extend curves from their end-points, selectively allow curvature from the original curve to
 * influence extended segments.
 */
bke::CurvesGeometry extend_curves(bke::CurvesGeometry &src_curves,
                                  const IndexMask &selection,
                                  const VArray<float> &start_lengths,
                                  const VArray<float> &end_lengths,
                                  float overshoot_fac,
                                  bool follow_curvature,
                                  float point_density,
                                  float segment_influence,
                                  float max_angle,
                                  bool invert_curvature,
                                  GeometryNodeCurveSampleMode sample_mode,
                                  const bke::AnonymousAttributePropagationInfo &propagation_info);

}  // namespace blender::geometry
