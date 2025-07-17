/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "BLI_math_matrix.hh"

#include "BKE_instances.hh"

namespace blender::nodes::node_geo_input_instance_scale_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Vector>("Scale").field_source();
}

class InstanceScaleFieldInput final : public bke::InstancesFieldInput {
 public:
  InstanceScaleFieldInput() : bke::InstancesFieldInput(CPPType::get<float3>(), "Scale") {}

  GVArray get_varray_for_context(const bke::Instances &instances,
                                 const IndexMask & /*mask*/) const final
  {
    const Span<float4x4> transforms = instances.transforms();
    return VArray<float3>::from_func(instances.instances_num(), [transforms](const int i) {
      return math::to_scale<true>(transforms[i]);
    });
  }

  uint64_t hash() const override
  {
    return 8346343;
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    return dynamic_cast<const InstanceScaleFieldInput *>(&other) != nullptr;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<float3> scale{std::make_shared<InstanceScaleFieldInput>()};
  params.set_output("Scale", std::move(scale));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeInputInstanceScale", GEO_NODE_INPUT_INSTANCE_SCALE);
  ntype.ui_name = "Instance Scale";
  ntype.ui_description = "Retrieve the scale of each instance in the geometry";
  ntype.enum_name_legacy = "INPUT_INSTANCE_SCALE";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_input_instance_scale_cc
