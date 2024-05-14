/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_material.h"

#include "GEO_mesh_primitive_uv_sphere.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_mesh_primitive_uv_sphere_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>("Segments")
      .default_value(32)
      .min(3)
      .max(1024)
      .description("Horizontal resolution of the sphere");
  b.add_input<decl::Int>("Rings").default_value(16).min(2).max(1024).description(
      "The number of horizontal rings");
  b.add_input<decl::Float>("Radius")
      .default_value(1.0f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .description("Distance from the generated points to the origin");
  b.add_output<decl::Geometry>("Mesh");
  b.add_output<decl::Vector>("UV Map").field_on_all();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const int segments_num = params.extract_input<int>("Segments");
  const int rings_num = params.extract_input<int>("Rings");
  if (segments_num < 3 || rings_num < 2) {
    if (segments_num < 3) {
      params.error_message_add(NodeWarningType::Info, TIP_("Segments must be at least 3"));
    }
    if (rings_num < 3) {
      params.error_message_add(NodeWarningType::Info, TIP_("Rings must be at least 3"));
    }
    params.set_default_remaining_outputs();
    return;
  }

  const float radius = params.extract_input<float>("Radius");

  AnonymousAttributeIDPtr uv_map_id = params.get_output_anonymous_attribute_id_if_needed("UV Map");

  Mesh *mesh = geometry::create_uv_sphere_mesh(radius, segments_num, rings_num, uv_map_id.get());
  BKE_id_material_eval_ensure_default_slot(reinterpret_cast<ID *>(mesh));
  params.set_output("Mesh", GeometrySet::from_mesh(mesh));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_MESH_PRIMITIVE_UV_SPHERE, "UV Sphere", NODE_CLASS_GEOMETRY);
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_mesh_primitive_uv_sphere_cc
