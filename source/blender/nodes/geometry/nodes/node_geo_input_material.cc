/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_material_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Material>("Material");
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "material", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Material *material = reinterpret_cast<Material *>(params.node().id);
  params.set_output("Material", material);
}

}  // namespace blender::nodes::node_geo_input_material_cc

void register_node_type_geo_input_material()
{
  namespace file_ns = blender::nodes::node_geo_input_material_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_INPUT_MATERIAL, "Material", NODE_CLASS_INPUT);
  ntype.draw_buttons = file_ns::node_layout;
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
