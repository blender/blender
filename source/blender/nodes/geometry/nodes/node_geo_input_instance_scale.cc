/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "BLI_math_matrix.hh"

#include "BKE_instances.hh"

namespace blender::nodes::node_geo_input_instance_scale_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Vector>("Scale"_ustr).field_source();
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

  void hash_unique(UniqueHashBytes &hash, fn::FieldHashDeep & /*deep_hash_cache*/) const override
  {
    static constexpr int8_t id = 0;
    hash.add(&id);
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  params.set_output("Scale"_ustr, Field<float3>::from_input<InstanceScaleFieldInput>());
}

static void node_register()
{
  static bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeInputInstanceScale"_ustr, GEO_NODE_INPUT_INSTANCE_SCALE);
  ntype.ui_name = "Instance Scale";
  ntype.ui_description = "Retrieve the scale of each instance in the geometry";
  ntype.enum_name_legacy = "INPUT_INSTANCE_SCALE";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_input_instance_scale_cc
