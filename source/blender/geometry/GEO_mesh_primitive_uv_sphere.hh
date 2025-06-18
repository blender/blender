/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>

#include "BLI_bounds_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_string_ref.hh"

struct Mesh;

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
                            std::optional<StringRef> uv_map_id);

}  // namespace blender::geometry
