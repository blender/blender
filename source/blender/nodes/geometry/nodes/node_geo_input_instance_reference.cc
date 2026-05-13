/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_instance_reference_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Int>("Reference Index"_ustr).field_source();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<int> reference_index{AttributeFieldInput::get_field<int, ".reference_index">()};
  params.set_output("Reference Index"_ustr, std::move(reference_index));
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeInputInstanceReference"_ustr);
  ntype.ui_name = "Instance Reference";
  ntype.ui_description = "Output the reference index of the instance";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_input_instance_reference_cc
