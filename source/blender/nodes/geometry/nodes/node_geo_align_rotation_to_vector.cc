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

#include "node_geometry_util.hh"

#include "BLI_math_rotation.h"

static bNodeSocketTemplate geo_node_align_rotation_to_vector_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {SOCK_STRING, N_("Factor")},
    {SOCK_FLOAT, N_("Factor"), 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, PROP_FACTOR},
    {SOCK_STRING, N_("Vector")},
    {SOCK_VECTOR, N_("Vector"), 0.0, 0.0, 1.0, 0.0, -FLT_MAX, FLT_MAX, PROP_ANGLE},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_align_rotation_to_vector_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

namespace blender::nodes {

static void align_rotations_auto_pivot(const Float3ReadAttribute &vectors,
                                       const FloatReadAttribute &factors,
                                       const float3 local_main_axis,
                                       MutableSpan<float3> rotations)
{
  for (const int i : IndexRange(vectors.size())) {
    const float3 vector = vectors[i];
    if (is_zero_v3(vector)) {
      continue;
    }

    float old_rotation[3][3];
    eul_to_mat3(old_rotation, rotations[i]);
    float3 old_axis;
    mul_v3_m3v3(old_axis, old_rotation, local_main_axis);

    const float3 new_axis = vector.normalized();
    const float3 rotation_axis = float3::cross_high_precision(old_axis, new_axis);
    const float full_angle = angle_normalized_v3v3(old_axis, new_axis);
    const float angle = factors[i] * full_angle;

    float rotation[3][3];
    axis_angle_to_mat3(rotation, rotation_axis, angle);

    float new_rotation_matrix[3][3];
    mul_m3_m3m3(new_rotation_matrix, rotation, old_rotation);

    float3 new_rotation;
    mat3_to_eul(new_rotation, new_rotation_matrix);

    rotations[i] = new_rotation;
  }
}

static void align_rotations_fixed_pivot(const Float3ReadAttribute &vectors,
                                        const FloatReadAttribute &factors,
                                        const float3 local_main_axis,
                                        const float3 local_pivot_axis,
                                        MutableSpan<float3> rotations)
{
  if (local_main_axis == local_pivot_axis) {
    /* Can't compute any meaningful rotation angle in this case. */
    return;
  }

  for (const int i : IndexRange(vectors.size())) {
    const float3 vector = vectors[i];
    if (is_zero_v3(vector)) {
      continue;
    }

    float old_rotation[3][3];
    eul_to_mat3(old_rotation, rotations[i]);
    float3 old_axis;
    mul_v3_m3v3(old_axis, old_rotation, local_main_axis);
    float3 pivot_axis;
    mul_v3_m3v3(pivot_axis, old_rotation, local_pivot_axis);

    float full_angle = angle_signed_on_axis_v3v3_v3(vector, old_axis, pivot_axis);
    if (full_angle > M_PI) {
      /* Make sure the point is rotated as little as possible. */
      full_angle -= 2.0f * M_PI;
    }
    const float angle = factors[i] * full_angle;

    float rotation[3][3];
    axis_angle_to_mat3(rotation, pivot_axis, angle);

    float new_rotation_matrix[3][3];
    mul_m3_m3m3(new_rotation_matrix, rotation, old_rotation);

    float3 new_rotation;
    mat3_to_eul(new_rotation, new_rotation_matrix);

    rotations[i] = new_rotation;
  }
}

static void align_rotations_on_component(GeometryComponent &component,
                                         const GeoNodeExecParams &params)
{
  const bNode &node = params.node();
  const NodeGeometryAlignRotationToVector &storage = *(const NodeGeometryAlignRotationToVector *)
                                                          node.storage;

  OutputAttributePtr rotation_attribute = component.attribute_try_get_for_output(
      "rotation", ATTR_DOMAIN_POINT, CD_PROP_FLOAT3);
  if (!rotation_attribute) {
    return;
  }
  MutableSpan<float3> rotations = rotation_attribute->get_span<float3>();

  FloatReadAttribute factors = params.get_input_attribute<float>(
      "Factor", component, ATTR_DOMAIN_POINT, 1.0f);
  Float3ReadAttribute vectors = params.get_input_attribute<float3>(
      "Vector", component, ATTR_DOMAIN_POINT, {0, 0, 1});

  float3 local_main_axis{0, 0, 0};
  local_main_axis[storage.axis] = 1;
  if (storage.pivot_axis == GEO_NODE_ALIGN_ROTATION_TO_VECTOR_PIVOT_AXIS_AUTO) {
    align_rotations_auto_pivot(vectors, factors, local_main_axis, rotations);
  }
  else {
    float3 local_pivot_axis{0, 0, 0};
    local_pivot_axis[storage.pivot_axis - 1] = 1;
    align_rotations_fixed_pivot(vectors, factors, local_main_axis, local_pivot_axis, rotations);
  }

  rotation_attribute.apply_span_and_save();
}

static void geo_node_align_rotation_to_vector_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  if (geometry_set.has<MeshComponent>()) {
    align_rotations_on_component(geometry_set.get_component_for_write<MeshComponent>(), params);
  }
  if (geometry_set.has<PointCloudComponent>()) {
    align_rotations_on_component(geometry_set.get_component_for_write<PointCloudComponent>(),
                                 params);
  }

  params.set_output("Geometry", geometry_set);
}

static void geo_node_align_rotation_to_vector_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeGeometryAlignRotationToVector *node_storage = (NodeGeometryAlignRotationToVector *)
      MEM_callocN(sizeof(NodeGeometryAlignRotationToVector), __func__);

  node_storage->axis = GEO_NODE_ALIGN_ROTATION_TO_VECTOR_AXIS_X;
  node_storage->input_type_factor = GEO_NODE_ATTRIBUTE_INPUT_FLOAT;
  node_storage->input_type_vector = GEO_NODE_ATTRIBUTE_INPUT_VECTOR;

  node->storage = node_storage;
}

static void geo_node_align_rotation_to_vector_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeGeometryAlignRotationToVector *node_storage = (NodeGeometryAlignRotationToVector *)
                                                        node->storage;
  update_attribute_input_socket_availabilities(
      *node, "Factor", (GeometryNodeAttributeInputMode)node_storage->input_type_factor);
  update_attribute_input_socket_availabilities(
      *node, "Vector", (GeometryNodeAttributeInputMode)node_storage->input_type_vector);
}

}  // namespace blender::nodes

void register_node_type_geo_align_rotation_to_vector()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype,
                     GEO_NODE_ALIGN_ROTATION_TO_VECTOR,
                     "Align Rotation to Vector",
                     NODE_CLASS_GEOMETRY,
                     0);
  node_type_socket_templates(
      &ntype, geo_node_align_rotation_to_vector_in, geo_node_align_rotation_to_vector_out);
  node_type_init(&ntype, blender::nodes::geo_node_align_rotation_to_vector_init);
  node_type_update(&ntype, blender::nodes::geo_node_align_rotation_to_vector_update);
  node_type_storage(&ntype,
                    "NodeGeometryAlignRotationToVector",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  ntype.geometry_node_execute = blender::nodes::geo_node_align_rotation_to_vector_exec;
  nodeRegisterType(&ntype);
}
