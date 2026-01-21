/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "NOD_geometry_nodes_bundle.hh"
#include "NOD_socket_search_link.hh"

namespace blender::nodes::node_geo_set_geometry_bundle {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Geometry>("Geometry").description("Geometry to override the bundle of");
  b.add_output<decl::Geometry>("Geometry").propagate_all().align_with_previous();
  b.add_input<decl::Bundle>("Bundle");
}

static void node_geo_exec(GeoNodeExecParams params)
{
  if (!U.experimental.use_geometry_bundle) {
    params.error_message_add(NodeWarningType::Error,
                             TIP_("The experimental option for this node is disabled"));
    params.set_default_remaining_outputs();
    return;
  }
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  BundlePtr bundle = params.extract_input<BundlePtr>("Bundle");
  geometry_set.bundle_ptr() = bundle;
  params.set_output("Geometry", std::move(geometry_set));
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  if (!U.experimental.use_geometry_bundle) {
    return;
  }
  search_link_ops_for_basic_node(params);
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeSetGeometryBundle");
  ntype.ui_name = "Set Geometry Bundle";
  ntype.ui_description = "Set the bundle of a geometry";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  ntype.gather_link_search_ops = node_gather_link_searches;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_set_geometry_bundle
