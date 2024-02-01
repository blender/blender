/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>

#include "BLI_math_quaternion_types.hh"
#include "BLI_math_vector_types.hh"

struct Mesh;
namespace blender::bke {
struct GeometrySet;
}

namespace blender::geometry {

void transform_mesh(Mesh &mesh, float3 translation, math::Quaternion rotation, float3 scale);

struct TransformGeometryErrors {
  bool volume_too_small = false;
};

std::optional<TransformGeometryErrors> transform_geometry(bke::GeometrySet &geometry,
                                                          const float4x4 &transform);

void translate_geometry(bke::GeometrySet &geometry, const float3 translation);

}  // namespace blender::geometry
