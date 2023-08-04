/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_mesh.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_tool_set_selection_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry");
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_output<decl::Geometry>("Geometry");
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "domain", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = ATTR_DOMAIN_POINT;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  if (!check_tool_context_and_error(params)) {
    return;
  }
  const Field<bool> selection = params.extract_input<Field<bool>>("Selection");
  const eAttrDomain domain = eAttrDomain(params.node().custom1);
  GeometrySet geometry = params.extract_input<GeometrySet>("Geometry");
  geometry.modify_geometry_sets([&](GeometrySet &geometry) {
    if (Mesh *mesh = geometry.get_mesh_for_write()) {
      switch (domain) {
        case ATTR_DOMAIN_POINT:
          bke::try_capture_field_on_geometry(geometry.get_component_for_write<MeshComponent>(),
                                             ".select_vert",
                                             ATTR_DOMAIN_POINT,
                                             selection);
          BKE_mesh_flush_select_from_verts(mesh);
          break;
        case ATTR_DOMAIN_FACE:
          bke::try_capture_field_on_geometry(geometry.get_component_for_write<MeshComponent>(),
                                             ".select_poly",
                                             ATTR_DOMAIN_FACE,
                                             selection);
          BKE_mesh_flush_select_from_faces(mesh);
          break;
        default:
          break;
      }
    }
    if (geometry.has_curves()) {
      if (ELEM(domain, ATTR_DOMAIN_POINT, ATTR_DOMAIN_CURVE)) {
        bke::try_capture_field_on_geometry(
            geometry.get_component_for_write<CurveComponent>(), ".selection", domain, selection);
      }
    }
    if (geometry.has_pointcloud()) {
      if (domain == ATTR_DOMAIN_POINT) {
        bke::try_capture_field_on_geometry(geometry.get_component_for_write<PointCloudComponent>(),
                                           ".selection",
                                           domain,
                                           selection);
      }
    }
  });
  params.set_output("Geometry", std::move(geometry));
}

}  // namespace blender::nodes::node_geo_tool_set_selection_cc

void register_node_type_geo_tool_set_selection()
{
  namespace file_ns = blender::nodes::node_geo_tool_set_selection_cc;
  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_TOOL_SET_SELECTION, "Set Selection", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.initfunc = file_ns::node_init;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  ntype.gather_add_node_search_ops = blender::nodes::search_link_ops_for_for_tool_node;
  ntype.gather_link_search_ops = blender::nodes::search_link_ops_for_tool_node;
  nodeRegisterType(&ntype);
}
