/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup geo
 */

#pragma once

#include <optional>

#include "BLI_math_quaternion_types.hh"
#include "BLI_math_vector_types.hh"

namespace blender {

struct Mesh;
namespace bke {
struct GeometrySet;
}

namespace geometry {

void transform_mesh(Mesh &mesh, float3 translation, math::Quaternion rotation, float3 scale);

struct TransformGeometryErrors {
  bool volume_too_small = false;
  bool bad_volume_transform = false;
};

std::optional<TransformGeometryErrors> transform_geometry(bke::GeometrySet &geometry,
                                                          const float4x4 &transform);

/**
 * \param translate_grease_pencil_layers: If true, the layer transform in grease pencil geometries
 * is changed instead of the point positions.
 */
void translate_geometry(bke::GeometrySet &geometry,
                        const float3 translation,
                        bool translate_grease_pencil_layers = true);

}  // namespace geometry
}  // namespace blender
