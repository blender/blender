/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "BLI_math_matrix.hh"

#include "BKE_instances.hh"

namespace blender::nodes::node_geo_input_instance_rotation_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Vector>("Rotation").field_source();
}

class InstanceRotationFieldInput final : public bke::InstancesFieldInput {
 public:
  InstanceRotationFieldInput() : bke::InstancesFieldInput(CPPType::get<float3>(), "Rotation") {}

  GVArray get_varray_for_context(const bke::Instances &instances,
                                 const IndexMask & /*mask*/) const final
  {
    auto rotation_fn = [&](const int i) -> float3 {
      return float3(math::to_euler(math::normalize(instances.transforms()[i])));
    };

    return VArray<float3>::ForFunc(instances.instances_num(), rotation_fn);
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
  Field<float3> rotation{std::make_shared<InstanceRotationFieldInput>()};
  params.set_output("Rotation", std::move(rotation));
}

}  // namespace blender::nodes::node_geo_input_instance_rotation_cc

void register_node_type_geo_input_instance_rotation()
{
  namespace file_ns = blender::nodes::node_geo_input_instance_rotation_cc;

  static bNodeType ntype;
  geo_node_type_base(
      &ntype, GEO_NODE_INPUT_INSTANCE_ROTATION, "Instance Rotation", NODE_CLASS_INPUT);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
