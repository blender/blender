/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "BLI_math_matrix.hh"

#include "BKE_instances.hh"

namespace blender::nodes::node_geo_input_instance_rotation_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Rotation>("Rotation").field_source();
}

class InstanceRotationFieldInput final : public bke::InstancesFieldInput {
 public:
  InstanceRotationFieldInput()
      : bke::InstancesFieldInput(CPPType::get<math::Quaternion>(), "Rotation")
  {
  }

  GVArray get_varray_for_context(const bke::Instances &instances,
                                 const IndexMask & /*mask*/) const final
  {
    const Span<float4x4> transforms = instances.transforms();
    return VArray<math::Quaternion>::from_func(
        instances.instances_num(),
        [transforms](const int i) { return math::to_quaternion(math::normalize(transforms[i])); });
  }

  uint64_t hash() const override
  {
    return 22374372;
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    return dynamic_cast<const InstanceRotationFieldInput *>(&other) != nullptr;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<math::Quaternion> rotation{std::make_shared<InstanceRotationFieldInput>()};
  params.set_output("Rotation", std::move(rotation));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(
      &ntype, "GeometryNodeInputInstanceRotation", GEO_NODE_INPUT_INSTANCE_ROTATION);
  ntype.ui_name = "Instance Rotation";
  ntype.ui_description = "Retrieve the rotation of each instance in the geometry";
  ntype.enum_name_legacy = "INPUT_INSTANCE_ROTATION";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_input_instance_rotation_cc
