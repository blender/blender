/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_instance_scale_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Vector>(N_("Scale")).field_source();
}

class VectorFieldInput final : public GeometryFieldInput {
 public:
  VectorFieldInput() : GeometryFieldInput(CPPType::get<float3>(), "Scale")
  {
  }

  GVArray get_varray_for_context(const GeometryComponent &component,
                                 const eAttrDomain UNUSED(domain),
                                 IndexMask UNUSED(mask)) const final
  {
    if (component.type() != GEO_COMPONENT_TYPE_INSTANCES) {
      return {};
    }

    const InstancesComponent &instance_component = static_cast<const InstancesComponent &>(
        component);

    auto scale_fn = [&](const int i) -> float3 {
      return instance_component.instance_transforms()[i].scale();
    };

    return VArray<float3>::ForFunc(instance_component.instances_num(), scale_fn);
  }

  uint64_t hash() const override
  {
    return 8346343;
  }

  bool is_equal_to(const fn::FieldNode &other) const override
  {
    return dynamic_cast<const VectorFieldInput *>(&other) != nullptr;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  Field<float3> scale{std::make_shared<VectorFieldInput>()};
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
