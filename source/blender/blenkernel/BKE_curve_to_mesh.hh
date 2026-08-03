/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include "BLI_virtual_array_fwd.hh"

#include "BKE_attribute_filter.hh"

#include <optional>

namespace blender {

struct Mesh;

namespace bke {

class CurvesGeometry;

/**
 * Extrude all splines in the profile curve along the path of every spline in the curve input.
 * Transfer curve attributes to the mesh.
 *
 * \note Normal calculation is by far the slowest part of calculations relating to the result mesh.
 * Although it would be a sensible decision to use the better topology information available while
 * generating the mesh to also generate the normals, that work may wasted if the output mesh is
 * changed anyway in a way that affects the normals. So currently this code uses the safer /
 * simpler solution of deferring normal calculation to the rest of Blender.
 * \param miter_limit_angle: A corner's turn angle at which the miter scale stops growing. The
 * profile is scaled by `1 / cos(turn_angle / 2)` along the corner bisector to keep a constant
 * visual width. This caps that factor at `1 / cos(miter_limit_angle / 2)` so sharp angles don't
 * produce unbounded spikes.
 */
Mesh *curve_to_mesh_sweep(const CurvesGeometry &main,
                          const CurvesGeometry &profile,
                          const VArray<float> &scales,
                          bool fill_caps,
                          std::optional<float> miter_limit_angle,
                          const bke::AttributeFilter &attribute_filter = {});
/**
 * Create a loose-edge mesh based on the evaluated path of the curve's splines.
 * Transfer curve attributes to the mesh.
 */
Mesh *curve_to_wire_mesh(const CurvesGeometry &curve,
                         const bke::AttributeFilter &attribute_filter = {});

}  // namespace bke
}  // namespace blender
