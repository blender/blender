/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "GEO_mesh_primitive_cuboid.hh"
#include "GEO_transform.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_bounding_box_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry");
  b.add_output<decl::Geometry>("Bounding Box");
  b.add_output<decl::Vector>("Min");
  b.add_output<decl::Vector>("Max");
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  /* Compute the min and max of all realized geometry for the two
   * vector outputs, which are only meant to consider real geometry. */
  const std::optional<Bounds<float3>> bounds = geometry_set.compute_boundbox_without_instances();
  if (!bounds) {
    params.set_output("Min", float3(0));
    params.set_output("Max", float3(0));
  }
  else {
    params.set_output("Min", bounds->min);
    params.set_output("Max", bounds->max);
  }

  /* Generate the bounding box meshes inside each unique geometry set (including individually for
   * every instance). Because geometry components are reference counted anyway, we can just
   * repurpose the original geometry sets for the output. */
  if (params.output_is_required("Bounding Box")) {
    geometry_set.modify_geometry_sets([&](GeometrySet &sub_geometry) {
      std::optional<Bounds<float3>> sub_bounds;

      /* Reuse the min and max calculation if this is the main "real" geometry set. */
      if (&sub_geometry == &geometry_set) {
        sub_bounds = bounds;
      }
      else {
        sub_bounds = sub_geometry.compute_boundbox_without_instances();
      }

      if (!sub_bounds) {
        sub_geometry.remove_geometry_during_modify();
      }
      else {
        const float3 scale = sub_bounds->max - sub_bounds->min;
        const float3 center = sub_bounds->min + scale / 2.0f;
        Mesh *mesh = geometry::create_cuboid_mesh(scale, 2, 2, 2, "uv_map");
        geometry::transform_mesh(*mesh, center, math::Quaternion::identity(), float3(1));
        sub_geometry.replace_mesh(mesh);
        sub_geometry.keep_only_during_modify({GeometryComponent::Type::Mesh});
      }
    });

    params.set_output("Bounding Box", std::move(geometry_set));
  }
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_BOUNDING_BOX, "Bounding Box", NODE_CLASS_GEOMETRY);
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_bounding_box_cc
