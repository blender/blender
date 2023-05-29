/* SPDX-License-Identifier: GPL-2.0-or-later */

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
    auto scale_fn = [&](const int i) -> float3 {
      return math::to_scale(instances.transforms()[i]);
    };

    return VArray<float3>::ForFunc(instances.instances_num(), scale_fn);
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

}  // namespace blender::nodes::node_geo_input_instance_scale_cc

void register_node_type_geo_input_instance_scale()
{
  namespace file_ns = blender::nodes::node_geo_input_instance_scale_cc;

  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_INPUT_INSTANCE_SCALE, "Instance Scale", NODE_CLASS_INPUT);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
