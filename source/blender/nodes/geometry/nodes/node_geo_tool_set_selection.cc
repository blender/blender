/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_mesh.hh"

#include "NOD_rna_define.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "RNA_enum_types.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_tool_set_selection_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry");
  b.add_input<decl::Bool>("Selection").default_value(true).field_on_all();
  b.add_output<decl::Geometry>("Geometry");
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "domain", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = int16_t(AttrDomain::Point);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  if (!check_tool_context_and_error(params)) {
    return;
  }
  GeometrySet geometry = params.extract_input<GeometrySet>("Geometry");
  if (params.user_data()->call_data->operator_data->mode == OB_MODE_OBJECT) {
    params.error_message_add(NodeWarningType::Error,
                             "Selection control is not supported in object mode");
    params.set_output("Geometry", std::move(geometry));
    return;
  }
  const Field<bool> selection = params.extract_input<Field<bool>>("Selection");
  const AttrDomain domain = AttrDomain(params.node().custom1);
  geometry.modify_geometry_sets([&](GeometrySet &geometry) {
    if (Mesh *mesh = geometry.get_mesh_for_write()) {
      switch (domain) {
        case AttrDomain::Point:
          /* Remove attributes in case they are on the wrong domain, which can happen after
           * conversion to and from other geometry types. */
          mesh->attributes_for_write().remove(".select_edge");
          mesh->attributes_for_write().remove(".select_poly");
          bke::try_capture_field_on_geometry(geometry.get_component_for_write<MeshComponent>(),
                                             ".select_vert",
                                             AttrDomain::Point,
                                             selection);
          bke::mesh_select_vert_flush(*mesh);
          break;
        case AttrDomain::Edge:
          bke::try_capture_field_on_geometry(geometry.get_component_for_write<MeshComponent>(),
                                             ".select_edge",
                                             AttrDomain::Edge,
                                             selection);
          bke::mesh_select_edge_flush(*mesh);
          break;
        case AttrDomain::Face:
          /* Remove attributes in case they are on the wrong domain, which can happen after
           * conversion to and from other geometry types. */
          mesh->attributes_for_write().remove(".select_vert");
          mesh->attributes_for_write().remove(".select_edge");
          bke::try_capture_field_on_geometry(geometry.get_component_for_write<MeshComponent>(),
                                             ".select_poly",
                                             AttrDomain::Face,
                                             selection);
          bke::mesh_select_face_flush(*mesh);
          break;
        default:
          break;
      }
    }
    if (geometry.has_curves()) {
      if (ELEM(domain, AttrDomain::Point, AttrDomain::Curve)) {
        bke::try_capture_field_on_geometry(
            geometry.get_component_for_write<CurveComponent>(), ".selection", domain, selection);
      }
    }
    if (geometry.has_pointcloud()) {
      if (domain == AttrDomain::Point) {
        bke::try_capture_field_on_geometry(geometry.get_component_for_write<PointCloudComponent>(),
                                           ".selection",
                                           domain,
                                           selection);
      }
    }
  });
  params.set_output("Geometry", std::move(geometry));
}

static void node_rna(StructRNA *srna)
{
  RNA_def_node_enum(srna,
                    "domain",
                    "Domain",
                    "",
                    rna_enum_attribute_domain_point_edge_face_curve_items,
                    NOD_inline_enum_accessors(custom1),
                    int(AttrDomain::Point));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_TOOL_SET_SELECTION, "Set Selection", NODE_CLASS_GEOMETRY);
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  ntype.gather_link_search_ops = search_link_ops_for_tool_node;
  blender::bke::nodeRegisterType(&ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_tool_set_selection_cc
