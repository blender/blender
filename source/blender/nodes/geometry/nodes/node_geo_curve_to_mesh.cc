/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"

#include "BKE_curve_to_mesh.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_curve_to_mesh_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Curve").supported_type(GeometryComponent::Type::Curve);
  b.add_input<decl::Geometry>("Profile Curve")
      .only_realized_data()
      .supported_type(GeometryComponent::Type::Curve);
  b.add_input<decl::Bool>("Fill Caps")
      .description(
          "If the profile spline is cyclic, fill the ends of the generated mesh with N-gons");
  b.add_output<decl::Geometry>("Mesh").propagate_all();
}

static void geometry_set_curve_to_mesh(GeometrySet &geometry_set,
                                       const GeometrySet &profile_set,
                                       const bool fill_caps,
                                       const AnonymousAttributePropagationInfo &propagation_info)
{
  const Curves &curves = *geometry_set.get_curves();
  const Curves *profile_curves = profile_set.get_curves();

  bke::GeometryComponentEditData::remember_deformed_curve_positions_if_necessary(geometry_set);

  if (profile_curves == nullptr) {
    Mesh *mesh = bke::curve_to_wire_mesh(curves.geometry.wrap(), propagation_info);
    geometry_set.replace_mesh(mesh);
  }
  else {
    Mesh *mesh = bke::curve_to_mesh_sweep(
        curves.geometry.wrap(), profile_curves->geometry.wrap(), fill_caps, propagation_info);
    geometry_set.replace_mesh(mesh);
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet curve_set = params.extract_input<GeometrySet>("Curve");
  GeometrySet profile_set = params.extract_input<GeometrySet>("Profile Curve");
  const bool fill_caps = params.extract_input<bool>("Fill Caps");

  curve_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (geometry_set.has_curves()) {
      geometry_set_curve_to_mesh(
          geometry_set, profile_set, fill_caps, params.get_output_propagation_info("Mesh"));
    }
    geometry_set.keep_only_during_modify({GeometryComponent::Type::Mesh});
  });

  params.set_output("Mesh", std::move(curve_set));
}

}  // namespace blender::nodes::node_geo_curve_to_mesh_cc

void register_node_type_geo_curve_to_mesh()
{
  namespace file_ns = blender::nodes::node_geo_curve_to_mesh_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_CURVE_TO_MESH, "Curve to Mesh", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
