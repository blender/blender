/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "BLI_task.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_rotate_instances_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Instances")).only_instances();
  b.add_input<decl::Bool>(N_("Selection")).default_value(true).hide_value().supports_field();
  b.add_input<decl::Vector>(N_("Rotation")).subtype(PROP_EULER).supports_field();
  b.add_input<decl::Vector>(N_("Pivot Point")).subtype(PROP_TRANSLATION).supports_field();
  b.add_input<decl::Bool>(N_("Local Space")).default_value(true).supports_field();
  b.add_output<decl::Geometry>(N_("Instances"));
}

static void rotate_instances(GeoNodeExecParams &params, InstancesComponent &instances_component)
{
  GeometryComponentFieldContext field_context{instances_component, ATTR_DOMAIN_INSTANCE};
  const int domain_size = instances_component.instances_amount();

  fn::FieldEvaluator evaluator{field_context, domain_size};
  evaluator.set_selection(params.extract_input<Field<bool>>("Selection"));
  evaluator.add(params.extract_input<Field<float3>>("Rotation"));
  evaluator.add(params.extract_input<Field<float3>>("Pivot Point"));
  evaluator.add(params.extract_input<Field<bool>>("Local Space"));
  evaluator.evaluate();

  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  const VArray<float3> &rotations = evaluator.get_evaluated<float3>(0);
  const VArray<float3> &pivots = evaluator.get_evaluated<float3>(1);
  const VArray<bool> &local_spaces = evaluator.get_evaluated<bool>(2);

  MutableSpan<float4x4> instance_transforms = instances_component.instance_transforms();

  threading::parallel_for(selection.index_range(), 512, [&](IndexRange range) {
    for (const int i_selection : range) {
      const int i = selection[i_selection];
      const float3 pivot = pivots[i];
      const float3 euler = rotations[i];
      float4x4 &instance_transform = instance_transforms[i];

      float4x4 rotation_matrix;
      float3 used_pivot;

      if (local_spaces[i]) {
        /* Find rotation axis from the matrix. This should work even if the instance is skewed. */
        const float3 rotation_axis_x = instance_transform.values[0];
        const float3 rotation_axis_y = instance_transform.values[1];
        const float3 rotation_axis_z = instance_transform.values[2];

        /* Create rotations around the individual axis. This could be optimized to skip some axis
         * when the angle is zero. */
        float rotation_x[3][3], rotation_y[3][3], rotation_z[3][3];
        axis_angle_to_mat3(rotation_x, rotation_axis_x, euler.x);
        axis_angle_to_mat3(rotation_y, rotation_axis_y, euler.y);
        axis_angle_to_mat3(rotation_z, rotation_axis_z, euler.z);

        /* Combine the previously computed rotations into the final rotation matrix. */
        float rotation[3][3];
        mul_m3_series(rotation, rotation_z, rotation_y, rotation_x);
        copy_m4_m3(rotation_matrix.values, rotation);

        /* Transform the passed in pivot into the local space of the instance. */
        used_pivot = instance_transform * pivot;
      }
      else {
        used_pivot = pivot;
        eul_to_mat4(rotation_matrix.values, euler);
      }
      /* Move the pivot to the origin so that we can rotate around it. */
      sub_v3_v3(instance_transform.values[3], used_pivot);
      /* Perform the actual rotation. */
      mul_m4_m4_pre(instance_transform.values, rotation_matrix.values);
      /* Undo the pivot shifting done before. */
      add_v3_v3(instance_transform.values[3], used_pivot);
    }
  });
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Instances");
  if (geometry_set.has_instances()) {
    InstancesComponent &instances = geometry_set.get_component_for_write<InstancesComponent>();
    rotate_instances(params, instances);
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
