/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_material_index_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Int>("Material Index").field_source();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<int> material_index_field = AttributeFieldInput::Create<int>("material_index");
  params.set_output("Material Index", std::move(material_index_field));
}

}  // namespace blender::nodes::node_geo_input_material_index_cc

void register_node_type_geo_input_material_index()
{
  namespace file_ns = blender::nodes::node_geo_input_material_index_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_INPUT_MATERIAL_INDEX, "Material Index", NODE_CLASS_INPUT);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
