/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_material.h"

#include "GEO_mesh_primitive_grid.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_mesh_primitive_grid_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Size X")
      .default_value(1.0f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .description("Side length of the plane in the X direction");
  b.add_input<decl::Float>("Size Y")
      .default_value(1.0f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .description("Side length of the plane in the Y direction");
  b.add_input<decl::Int>("Vertices X")
      .default_value(3)
      .min(2)
      .max(1000)
      .description("Number of vertices in the X direction");
  b.add_input<decl::Int>("Vertices Y")
      .default_value(3)
      .min(2)
      .max(1000)
      .description("Number of vertices in the Y direction");
  b.add_output<decl::Geometry>("Mesh");
  b.add_output<decl::Vector>("UV Map").field_on_all();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const float size_x = params.extract_input<float>("Size X");
  const float size_y = params.extract_input<float>("Size Y");
  const int verts_x = params.extract_input<int>("Vertices X");
  const int verts_y = params.extract_input<int>("Vertices Y");
  if (verts_x < 1 || verts_y < 1) {
    params.set_default_remaining_outputs();
    return;
  }

  AnonymousAttributeIDPtr uv_map_id = params.get_output_anonymous_attribute_id_if_needed("UV Map");

  Mesh *mesh = geometry::create_grid_mesh(verts_x, verts_y, size_x, size_y, uv_map_id.get());
  BKE_id_material_eval_ensure_default_slot(reinterpret_cast<ID *>(mesh));

  params.set_output("Mesh", GeometrySet::from_mesh(mesh));
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_MESH_PRIMITIVE_GRID, "Grid", NODE_CLASS_GEOMETRY);
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_mesh_primitive_grid_cc
