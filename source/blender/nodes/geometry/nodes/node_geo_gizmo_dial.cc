/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "NOD_rna_define.hh"

#include "RNA_enum_types.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_geo_gizmo_dial_cc {

NODE_STORAGE_FUNCS(NodeGeometryDialGizmo)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Value").hide_value().multi_input();
  b.add_input<decl::Vector>("Position").subtype(PROP_TRANSLATION);
  b.add_input<decl::Vector>("Up").default_value({0, 0, 1}).subtype(PROP_XYZ);
  b.add_input<decl::Bool>("Screen Space")
      .default_value(true)
      .description(
          "If true, the gizmo is displayed in screen space. Otherwise it's in object space");
  b.add_input<decl::Float>("Radius").default_value(1.0f);
  b.add_output<decl::Geometry>("Transform");
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryDialGizmo *storage = MEM_callocN<NodeGeometryDialGizmo>(__func__);
  node->storage = storage;
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "color_id", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_rna(StructRNA *srna)
{
  RNA_def_node_enum(srna,
                    "color_id",
                    "Color",
                    "",
                    rna_enum_geometry_nodes_gizmo_color_items,
                    NOD_storage_enum_accessors(color_id));
}

static void node_register()
{
  static bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeGizmoDial", GEO_NODE_GIZMO_DIAL);
  ntype.ui_name = "Dial Gizmo";
  ntype.ui_description = "Show a dial gizmo in the viewport for a value";
  ntype.enum_name_legacy = "GIZMO_DIAL";
  ntype.nclass = NODE_CLASS_INTERFACE;
  bke::node_type_storage(
      ntype, "NodeGeometryDialGizmo", node_free_standard_storage, node_copy_standard_storage);
  ntype.declare = node_declare;
  ntype.draw_buttons = node_layout;
  ntype.initfunc = node_init;
  bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_gizmo_dial_cc
