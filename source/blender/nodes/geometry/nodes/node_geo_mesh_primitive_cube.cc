/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_constants.h"
#include "BLI_math_euler.hh"

#include "DNA_mesh_types.h"

#include "BKE_material.hh"

#include "GEO_mesh_primitive_cuboid.hh"
#include "GEO_mesh_primitive_grid.hh"
#include "GEO_mesh_primitive_line.hh"
#include "GEO_transform.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_mesh_primitive_cube_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>("Size")
      .default_value(float3(1))
      .min(0.0f)
      .subtype(PROP_TRANSLATION)
      .description("Side length along each axis");
  b.add_input<decl::Int>("Vertices X")
      .default_value(2)
      .min(2)
      .max(1000)
      .description("Number of vertices for the X side of the shape");
  b.add_input<decl::Int>("Vertices Y")
      .default_value(2)
      .min(2)
      .max(1000)
      .description("Number of vertices for the Y side of the shape");
  b.add_input<decl::Int>("Vertices Z")
      .default_value(2)
      .min(2)
      .max(1000)
      .description("Number of vertices for the Z side of the shape");
  b.add_output<decl::Geometry>("Mesh");
  b.add_output<decl::Vector>("UV Map").field_on_all();
}

static Mesh *create_cube_mesh(const float3 size,
                              const int verts_x,
                              const int verts_y,
                              const int verts_z,
                              const std::optional<StringRef> &uv_map_id)
{
  const int dimensions = (verts_x - 1 > 0) + (verts_y - 1 > 0) + (verts_z - 1 > 0);
  if (dimensions == 0) {
    return geometry::create_line_mesh(float3(0), float3(0), 1);
  }
  if (dimensions == 1) {
    float3 start;
    float3 delta;
    if (verts_x > 1) {
      start = {-size.x / 2.0f, 0, 0};
      delta = {size.x / (verts_x - 1), 0, 0};
    }
    else if (verts_y > 1) {
      start = {0, -size.y / 2.0f, 0};
      delta = {0, size.y / (verts_y - 1), 0};
    }
    else {
      start = {0, 0, -size.z / 2.0f};
      delta = {0, 0, size.z / (verts_z - 1)};
    }

    return geometry::create_line_mesh(start, delta, verts_x * verts_y * verts_z);
  }
  if (dimensions == 2) {
    if (verts_z == 1) { /* XY plane. */
      return geometry::create_grid_mesh(verts_x, verts_y, size.x, size.y, uv_map_id);
    }
    if (verts_y == 1) { /* XZ plane. */
      Mesh *mesh = geometry::create_grid_mesh(verts_x, verts_z, size.x, size.z, uv_map_id);
      geometry::transform_mesh(
          *mesh, float3(0), math::to_quaternion(math::EulerXYZ(M_PI_2, 0.0f, 0.0f)), float3(1));
      return mesh;
    }
    /* YZ plane. */
    Mesh *mesh = geometry::create_grid_mesh(verts_z, verts_y, size.z, size.y, uv_map_id);
    geometry::transform_mesh(
        *mesh, float3(0), math::to_quaternion(math::EulerXYZ(0.0f, M_PI_2, 0.0f)), float3(1));
    return mesh;
  }

  return geometry::create_cuboid_mesh(size, verts_x, verts_y, verts_z, uv_map_id);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const float3 size = params.extract_input<float3>("Size");
  const int verts_x = params.extract_input<int>("Vertices X");
  const int verts_y = params.extract_input<int>("Vertices Y");
  const int verts_z = params.extract_input<int>("Vertices Z");
  if (verts_x < 1 || verts_y < 1 || verts_z < 1) {
    params.error_message_add(NodeWarningType::Info, TIP_("Vertices must be at least 1"));
    params.set_default_remaining_outputs();
    return;
  }

  std::optional<std::string> uv_map_id = params.get_output_anonymous_attribute_id_if_needed(
      "UV Map");

  Mesh *mesh = create_cube_mesh(size, verts_x, verts_y, verts_z, uv_map_id);
  BKE_id_material_eval_ensure_default_slot(&mesh->id);

  params.set_output("Mesh", GeometrySet::from_mesh(mesh));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeMeshCube", GEO_NODE_MESH_PRIMITIVE_CUBE);
  ntype.ui_name = "Cube";
  ntype.ui_description = "Generate a cuboid mesh with variable side lengths and subdivisions";
  ntype.enum_name_legacy = "MESH_PRIMITIVE_CUBE";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_mesh_primitive_cube_cc
