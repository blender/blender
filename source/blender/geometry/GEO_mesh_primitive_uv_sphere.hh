/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_bounds_types.hh"
#include "BLI_math_vector_types.hh"

struct Mesh;
namespace blender::bke {
class AttributeIDRef;
}  // namespace blender::bke

namespace blender::geometry {

/**
 * Calculates the bounds of a radial primitive.
 * The algorithm assumes X-axis symmetry of primitives.
 */
Bounds<float3> calculate_bounds_radial_primitive(float radius_top,
                                                 float radius_bottom,
                                                 int segments,
                                                 float height);

Mesh *create_uv_sphere_mesh(float radius,
                            int segments,
                            int rings,
                            const bke::AttributeIDRef &uv_map_id);

}  // namespace blender::geometry
