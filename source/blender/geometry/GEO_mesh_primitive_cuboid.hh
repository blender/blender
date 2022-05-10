/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

struct Mesh;
struct float3;
namespace blender {
namespace bke {
class AttributeIDRef;
}
}  // namespace blender

namespace blender::geometry {

Mesh *create_cuboid_mesh(
    const float3 &size, int verts_x, int verts_y, int verts_z, const bke::AttributeIDRef &uv_id);

}  // namespace blender::geometry
