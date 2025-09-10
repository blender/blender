/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_grease_pencil.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "NOD_rna_define.hh"

#include "RNA_enum_types.hh"

#include "GEO_foreach_geometry.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_grease_pencil_set_depth_mode {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_default_layout();
  b.add_input<decl::Geometry>("Grease Pencil")
      .supported_type(GeometryComponent::Type::GreasePencil)
      .description("Grease Pencil to set the depth order of");
  b.add_output<decl::Geometry>("Grease Pencil").propagate_all().align_with_previous();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "depth_order", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = 0;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Grease Pencil");

  geometry::foreach_real_geometry(geometry_set, [&](GeometrySet &geometry) {
    if (GreasePencil *grease_pencil = geometry.get_grease_pencil_for_write()) {
      SET_FLAG_FROM_TEST(grease_pencil->flag,
                         params.node().custom1 == GREASE_PENCIL_STROKE_ORDER_3D,
                         GREASE_PENCIL_STROKE_ORDER_3D);
    }
  });

  params.set_output("Grease Pencil", std::move(geometry_set));
}

static void node_rna(StructRNA *srna)
{
  RNA_def_node_enum(srna,
                    "depth_order",
                    "Depth Order",
                    "",
                    rna_enum_stroke_depth_order_items,
                    NOD_inline_enum_accessors(custom1));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeSetGreasePencilDepth");
  ntype.ui_name = "Set Grease Pencil Depth";
  ntype.ui_description = "Set the Grease Pencil depth order to use";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  ntype.draw_buttons = node_layout;
  bke::node_type_size(ntype, 180, 120, NODE_DEFAULT_MAX_WIDTH);
  blender::bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register);

}  // namespace blender::nodes::node_geo_grease_pencil_set_depth_mode
