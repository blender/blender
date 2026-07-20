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

enum class Mode : int8_t {
  Layers2D = 0,
  Location3D = GREASE_PENCIL_STROKE_ORDER_3D,
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Geometry>("Grease Pencil"_ustr)
      .supported_type(GeometryComponent::Type::GreasePencil)
      .description("Grease Pencil to set the depth order of");
  b.add_output<decl::Geometry>("Grease Pencil"_ustr)
      .propagate_all_geometry()
      .align_with_previous();
  b.add_input<decl::Menu>("Depth Order"_ustr)
      .static_items(rna_enum_stroke_depth_order_items)
      .optional_label();
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = 0;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const auto depth_order = params.get_input<Mode>("Depth Order"_ustr);
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Grease Pencil"_ustr);

  geometry::foreach_real_geometry(geometry_set, [&](GeometrySet &geometry) {
    if (GreasePencil *grease_pencil = geometry.get_grease_pencil_for_write()) {
      SET_FLAG_FROM_TEST(
          grease_pencil->flag, depth_order == Mode::Location3D, GREASE_PENCIL_STROKE_ORDER_3D);
    }
  });

  params.set_output("Grease Pencil"_ustr, std::move(geometry_set));
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeSetGreasePencilDepth"_ustr);
  ntype.ui_name = "Set Grease Pencil Depth";
  ntype.ui_description = "Set the Grease Pencil depth order to use";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  ntype.default_width = bke::NodeWidth::_180;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register);

}  // namespace blender::nodes::node_geo_grease_pencil_set_depth_mode
