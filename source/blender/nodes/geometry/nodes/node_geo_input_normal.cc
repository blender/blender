/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_geo_input_normal_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Vector>("Normal").field_source();
  b.add_output<decl::Vector>("True Normal")
      .field_source()
      .description(
          "For meshes, outputs normals without custom normal attributes taken into account");
}

static void node_layout_ex(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "legacy_corner_normals", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const bool legacy_corner_normals = bool(params.node().custom1);
  if (params.output_is_required("Normal")) {
    params.set_output(
        "Normal",
        Field<float3>{std::make_shared<bke::NormalFieldInput>(legacy_corner_normals, false)});
  }
  if (params.output_is_required("True Normal")) {
    params.set_output("True Normal",
                      Field<float3>{std::make_shared<bke::NormalFieldInput>(false, true)});
  }
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeInputNormal", GEO_NODE_INPUT_NORMAL);
  ntype.ui_name = "Normal";
  ntype.ui_description =
      "Retrieve a unit length vector indicating the direction pointing away from the geometry at "
      "each element";
  ntype.enum_name_legacy = "INPUT_NORMAL";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  ntype.draw_buttons_ex = node_layout_ex;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_input_normal_cc
