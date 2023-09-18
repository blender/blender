/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"

#include "BLI_task.hh"

#include "BKE_instances.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_scale_instances_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Instances").only_instances();
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_input<decl::Vector>("Scale").subtype(PROP_XYZ).default_value({1, 1, 1}).field_on_all();
  b.add_input<decl::Vector>("Center").subtype(PROP_TRANSLATION).field_on_all();
  b.add_input<decl::Bool>("Local Space").default_value(true).field_on_all();
  b.add_output<decl::Geometry>("Instances").propagate_all();
}

static void scale_instances(GeoNodeExecParams &params, bke::Instances &instances)
{
  const bke::InstancesFieldContext context{instances};
  fn::FieldEvaluator evaluator{context, instances.instances_num()};
  evaluator.set_selection(params.extract_input<Field<bool>>("Selection"));
  evaluator.add(params.extract_input<Field<float3>>("Scale"));
  evaluator.add(params.extract_input<Field<float3>>("Center"));
  evaluator.add(params.extract_input<Field<bool>>("Local Space"));
  evaluator.evaluate();

  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  const VArray<float3> scales = evaluator.get_evaluated<float3>(0);
  const VArray<float3> pivots = evaluator.get_evaluated<float3>(1);
  const VArray<bool> local_spaces = evaluator.get_evaluated<bool>(2);

  MutableSpan<float4x4> transforms = instances.transforms();

  selection.foreach_index(GrainSize(512), [&](const int64_t i) {
    const float3 pivot = pivots[i];
    float4x4 &instance_transform = transforms[i];

    if (local_spaces[i]) {
      instance_transform *= math::from_location<float4x4>(pivot);
      rescale_m4(instance_transform.ptr(), scales[i]);
      instance_transform *= math::from_location<float4x4>(-pivot);
    }
    else {
      const float4x4 original_transform = instance_transform;
      instance_transform = math::from_location<float4x4>(pivot);
      rescale_m4(instance_transform.ptr(), scales[i]);
      instance_transform *= math::from_location<float4x4>(-pivot);
      instance_transform *= original_transform;
    }
  });
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Instances");
  if (bke::Instances *instances = geometry_set.get_instances_for_write()) {
    scale_instances(params, *instances);
  }
  params.set_output("Instances", std::move(geometry_set));
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SCALE_INSTANCES, "Scale Instances", NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_scale_instances_cc
