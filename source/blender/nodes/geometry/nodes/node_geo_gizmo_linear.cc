/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "NOD_rna_define.hh"

#include "RNA_enum_types.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_geo_gizmo_linear_cc {

NODE_STORAGE_FUNCS(NodeGeometryLinearGizmo)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Value").hide_value().multi_input();
  b.add_input<decl::Vector>("Position").subtype(PROP_TRANSLATION);
  b.add_input<decl::Vector>("Direction").default_value({0, 0, 1}).subtype(PROP_XYZ);
  b.add_output<decl::Geometry>("Transform");
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryLinearGizmo *storage = MEM_cnew<NodeGeometryLinearGizmo>(__func__);
  node->storage = storage;
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "color_id", UI_ITEM_NONE, "", ICON_NONE);
  uiItemR(layout, ptr, "draw_style", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_rna(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_node_enum(srna,
                    "color_id",
                    "Color",
                    "",
                    rna_enum_geometry_nodes_gizmo_color_items,
                    NOD_storage_enum_accessors(color_id));
  prop = RNA_def_node_enum(srna,
                           "draw_style",
                           "Draw Style",
                           "",
                           rna_enum_geometry_nodes_linear_gizmo_draw_style_items,
                           NOD_storage_enum_accessors(draw_style));
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_NODETREE);
}

static void node_register()
{
  static bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeGizmoLinear", GEO_NODE_GIZMO_LINEAR);
  ntype.ui_name = "Linear Gizmo";
  ntype.ui_description = "Show a linear gizmo in the viewport for a value";
  ntype.enum_name_legacy = "GIZMO_LINEAR";
  ntype.nclass = NODE_CLASS_INTERFACE;
  bke::node_type_storage(
      &ntype, "NodeGeometryLinearGizmo", node_free_standard_storage, node_copy_standard_storage);
  ntype.declare = node_declare;
  ntype.draw_buttons = node_layout;
  ntype.initfunc = node_init;
  bke::node_register_type(&ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_gizmo_linear_cc
