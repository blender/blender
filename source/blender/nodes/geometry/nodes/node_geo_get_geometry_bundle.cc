/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "NOD_geometry_nodes_bundle.hh"
#include "NOD_socket_search_link.hh"

namespace blender::nodes::node_geo_get_geometry_bundle {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  b.add_input<decl::Geometry>("Geometry"_ustr).description("Geometry to get the bundle of");
  b.add_output<decl::Geometry>("Geometry"_ustr).propagate_all().align_with_previous();
  b.add_output<decl::Bundle>("Bundle"_ustr).propagate_all();
  b.add_input<decl::Bool>("Remove"_ustr)
      .default_value(false)
      .description(
          "Removing the bundle from the geometry can be beneficial to avoid unnecessary data "
          "copies");
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry"_ustr);
  const bool remove = params.extract_input<bool>("Remove"_ustr);
  BundlePtr bundle;
  if (remove) {
    bundle = std::move(geometry_set.bundle_ptr());
  }
  else {
    bundle = geometry_set.bundle_ptr();
  }
  params.set_output("Geometry"_ustr, std::move(geometry_set));
  params.set_output("Bundle"_ustr, std::move(bundle));
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeGetGeometryBundle"_ustr);
  ntype.ui_name = "Get Geometry Bundle";
  ntype.ui_description = "Get the bundle of a geometry";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  ntype.default_width = bke::NodeWidth::_160;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_get_geometry_bundle
