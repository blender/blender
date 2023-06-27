/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.hh"
#include "BLI_task.hh"

#include "BKE_instances.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_rotate_instances_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Instances").only_instances();
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_input<decl::Vector>("Rotation").subtype(PROP_EULER).field_on_all();
  b.add_input<decl::Vector>("Pivot Point").subtype(PROP_TRANSLATION).field_on_all();
  b.add_input<decl::Bool>("Local Space").default_value(true).field_on_all();
  b.add_output<decl::Geometry>("Instances").propagate_all();
}

static void rotate_instances(GeoNodeExecParams &params, bke::Instances &instances)
{
  using namespace blender::math;

  const bke::InstancesFieldContext context{instances};
  fn::FieldEvaluator evaluator{context, instances.instances_num()};
  evaluator.set_selection(params.extract_input<Field<bool>>("Selection"));
  evaluator.add(params.extract_input<Field<float3>>("Rotation"));
  evaluator.add(params.extract_input<Field<float3>>("Pivot Point"));
  evaluator.add(params.extract_input<Field<bool>>("Local Space"));
  evaluator.evaluate();

  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  const VArray<float3> rotations = evaluator.get_evaluated<float3>(0);
  const VArray<float3> pivots = evaluator.get_evaluated<float3>(1);
  const VArray<bool> local_spaces = evaluator.get_evaluated<bool>(2);

  MutableSpan<float4x4> transforms = instances.transforms();

  selection.foreach_index(GrainSize(512), [&](const int64_t i) {
    const float3 pivot = pivots[i];
    const float3 euler = rotations[i];
    float4x4 &instance_transform = transforms[i];

    float4x4 rotation_matrix;
    float3 used_pivot;

    if (local_spaces[i]) {
      /* Find rotation axis from the matrix. This should work even if the instance is skewed. */
      /* Create rotations around the individual axis. This could be optimized to skip some axis
       * when the angle is zero. */
      const float3x3 rotation_x = from_rotation<float3x3>(
          AxisAngle(normalize(instance_transform.x_axis()), euler.x));
      const float3x3 rotation_y = from_rotation<float3x3>(
          AxisAngle(normalize(instance_transform.y_axis()), euler.y));
      const float3x3 rotation_z = from_rotation<float3x3>(
          AxisAngle(normalize(instance_transform.z_axis()), euler.z));

      /* Combine the previously computed rotations into the final rotation matrix. */
      rotation_matrix = float4x4(rotation_z * rotation_y * rotation_x);

      /* Transform the passed in pivot into the local space of the instance. */
      used_pivot = transform_point(instance_transform, pivot);
    }
    else {
      used_pivot = pivot;
      rotation_matrix = from_rotation<float4x4>(EulerXYZ(euler));
    }
    /* Move the pivot to the origin so that we can rotate around it. */
    instance_transform.location() -= used_pivot;
    /* Perform the actual rotation. */
    instance_transform = rotation_matrix * instance_transform;
    /* Undo the pivot shifting done before. */
    instance_transform.location() += used_pivot;
  });
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Instances");
  if (bke::Instances *instances = geometry_set.get_instances_for_write()) {
    rotate_instances(params, *instances);
  }
  params.set_output("Instances", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_rotate_instances_cc

void register_node_type_geo_rotate_instances()
{
  namespace file_ns = blender::nodes::node_geo_rotate_instances_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_ROTATE_INSTANCES, "Rotate Instances", NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
