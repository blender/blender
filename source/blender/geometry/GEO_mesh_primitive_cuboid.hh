/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <optional>

#include "BLI_math_vector_types.hh"
#include "BLI_string_ref.hh"

struct Mesh;

namespace blender::geometry {

Mesh *create_cuboid_mesh(
    const float3 &size, int verts_x, int verts_y, int verts_z, std::optional<StringRef> uv_id);

Mesh *create_cuboid_mesh(const float3 &size, int verts_x, int verts_y, int verts_z);

}  // namespace blender::geometry
