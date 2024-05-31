/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_geometry_fields.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_instance_transform_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Matrix>("Transform").field_source();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<float4x4> position_field{AttributeFieldInput::Create<float4x4>("instance_transform")};
  params.set_output("Transform", std::move(position_field));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_INPUT_INSTANCE_TRANSFORM, "Instance Transform", NODE_CLASS_INPUT);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  blender::bke::nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_input_instance_transform_cc
