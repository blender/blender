/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

struct Mesh;
namespace blender::bke {
class AttributeIDRef;
}  // namespace blender::bke

namespace blender::geometry {

Mesh *create_grid_mesh(
    int verts_x, int verts_y, float size_x, float size_y, const bke::AttributeIDRef &uv_map_id);

}  // namespace blender::geometry
