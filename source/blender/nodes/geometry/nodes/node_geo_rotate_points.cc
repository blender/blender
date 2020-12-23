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

static bNodeSocketTemplate geo_node_rotate_points_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {SOCK_STRING, N_("Axis")},
    {SOCK_VECTOR, N_("Axis"), 0.0, 0.0, 1.0, 0.0, -FLT_MAX, FLT_MAX, PROP_XYZ},
    {SOCK_STRING, N_("Angle")},
    {SOCK_FLOAT, N_("Angle"), 0.0, 0.0, 0.0, 0.0, -FLT_MAX, FLT_MAX, PROP_ANGLE},
    {SOCK_STRING, N_("Rotation")},
    {SOCK_VECTOR, N_("Rotation"), 0.0, 0.0, 0.0, 0.0, -FLT_MAX, FLT_MAX, PROP_EULER},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_rotate_points_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

namespace blender::nodes {

static void rotate_points__axis_angle__object_space(const int domain_size,
                                                    const Float3ReadAttribute &axis,
                                                    const FloatReadAttribute &angles,
                                                    MutableSpan<float3> rotations)
{
  for (const int i : IndexRange(domain_size)) {
    float old_rotation[3][3];
    eul_to_mat3(old_rotation, rotations[i]);
    float rotation[3][3];
    axis_angle_to_mat3(rotation, axis[i], angles[i]);
    float new_rotation[3][3];
    mul_m3_m3m3(new_rotation, rotation, old_rotation);
    mat3_to_eul(rotations[i], new_rotation);
  }
}

static void rotate_points__axis_angle__point_space(const int domain_size,
                                                   const Float3ReadAttribute &axis,
                                                   const FloatReadAttribute &angles,
                                                   MutableSpan<float3> rotations)
{
  for (const int i : IndexRange(domain_size)) {
    float old_rotation[3][3];
    eul_to_mat3(old_rotation, rotations[i]);
    float rotation[3][3];
    axis_angle_to_mat3(rotation, axis[i], angles[i]);
    float new_rotation[3][3];
    mul_m3_m3m3(new_rotation, old_rotation, rotation);
    mat3_to_eul(rotations[i], new_rotation);
  }
}

static void rotate_points__euler__object_space(const int domain_size,
                                               const Float3ReadAttribute &eulers,
                                               MutableSpan<float3> rotations)
{
  for (const int i : IndexRange(domain_size)) {
    float old_rotation[3][3];
    eul_to_mat3(old_rotation, rotations[i]);
    float rotation[3][3];
    eul_to_mat3(rotation, eulers[i]);
    float new_rotation[3][3];
    mul_m3_m3m3(new_rotation, rotation, old_rotation);
    mat3_to_eul(rotations[i], new_rotation);
  }
}

static void rotate_points__euler__point_space(const int domain_size,
                                              const Float3ReadAttribute &eulers,
                                              MutableSpan<float3> rotations)
{
  for (const int i : IndexRange(domain_size)) {
    float old_rotation[3][3];
    eul_to_mat3(old_rotation, rotations[i]);
    float rotation[3][3];
    eul_to_mat3(rotation, eulers[i]);
    float new_rotation[3][3];
    mul_m3_m3m3(new_rotation, old_rotation, rotation);
    mat3_to_eul(rotations[i], new_rotation);
  }
}

static void rotate_points_on_component(GeometryComponent &component,
                                       const GeoNodeExecParams &params)
{
  const bNode &node = params.node();
  const NodeGeometryRotatePoints &storage = *(const NodeGeometryRotatePoints *)node.storage;

  WriteAttributePtr rotation_attribute = component.attribute_try_ensure_for_write(
      "rotation", ATTR_DOMAIN_POINT, CD_PROP_FLOAT3);
  if (!rotation_attribute) {
    return;
  }

  MutableSpan<float3> rotations = rotation_attribute->get_span().typed<float3>();
  const int domain_size = rotations.size();

  if (storage.type == GEO_NODE_ROTATE_POINTS_TYPE_AXIS_ANGLE) {
    Float3ReadAttribute axis = params.get_input_attribute<float3>(
        "Axis", component, ATTR_DOMAIN_POINT, {0, 0, 1});
    FloatReadAttribute angles = params.get_input_attribute<float>(
        "Angle", component, ATTR_DOMAIN_POINT, 0);

    if (storage.space == GEO_NODE_ROTATE_POINTS_SPACE_OBJECT) {
      rotate_points__axis_angle__object_space(domain_size, axis, angles, rotations);
    }
    else {
      rotate_points__axis_angle__point_space(domain_size, axis, angles, rotations);
    }
  }
  else {
    Float3ReadAttribute eulers = params.get_input_attribute<float3>(
        "Rotation", component, ATTR_DOMAIN_POINT, {0, 0, 0});

    if (storage.space == GEO_NODE_ROTATE_POINTS_SPACE_OBJECT) {
      rotate_points__euler__object_space(domain_size, eulers, rotations);
    }
    else {
      rotate_points__euler__point_space(domain_size, eulers, rotations);
    }
  }

  rotation_attribute->apply_span();
}

static void geo_node_rotate_points_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  if (geometry_set.has<MeshComponent>()) {
    rotate_points_on_component(geometry_set.get_component_for_write<MeshComponent>(), params);
  }
  if (geometry_set.has<PointCloudComponent>()) {
    rotate_points_on_component(geometry_set.get_component_for_write<PointCloudComponent>(),
                               params);
  }

  params.set_output("Geometry", geometry_set);
}

static void geo_node_rotate_points_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeGeometryRotatePoints *node_storage = (NodeGeometryRotatePoints *)MEM_callocN(
      sizeof(NodeGeometryRotatePoints), __func__);

  node_storage->type = GEO_NODE_ROTATE_POINTS_TYPE_EULER;
  node_storage->space = GEO_NODE_ROTATE_POINTS_SPACE_OBJECT;
  node_storage->input_type_axis = GEO_NODE_ATTRIBUTE_INPUT_VECTOR;
  node_storage->input_type_angle = GEO_NODE_ATTRIBUTE_INPUT_FLOAT;
  node_storage->input_type_rotation = GEO_NODE_ATTRIBUTE_INPUT_VECTOR;

  node->storage = node_storage;
}

static void geo_node_rotate_points_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeGeometryRotatePoints *node_storage = (NodeGeometryRotatePoints *)node->storage;
  update_attribute_input_socket_availabilities(
      *node,
      "Axis",
      (GeometryNodeAttributeInputMode)node_storage->input_type_axis,
      node_storage->type == GEO_NODE_ROTATE_POINTS_TYPE_AXIS_ANGLE);
  update_attribute_input_socket_availabilities(
      *node,
      "Angle",
      (GeometryNodeAttributeInputMode)node_storage->input_type_angle,
      node_storage->type == GEO_NODE_ROTATE_POINTS_TYPE_AXIS_ANGLE);
  update_attribute_input_socket_availabilities(
      *node,
      "Rotation",
      (GeometryNodeAttributeInputMode)node_storage->input_type_rotation,
      node_storage->type == GEO_NODE_ROTATE_POINTS_TYPE_EULER);
}

}  // namespace blender::nodes

void register_node_type_geo_rotate_points()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_ROTATE_POINTS, "Rotate Points", NODE_CLASS_GEOMETRY, 0);
  node_type_socket_templates(&ntype, geo_node_rotate_points_in, geo_node_rotate_points_out);
  node_type_init(&ntype, blender::nodes::geo_node_rotate_points_init);
  node_type_update(&ntype, blender::nodes::geo_node_rotate_points_update);
  node_type_storage(
      &ntype, "NodeGeometryRotatePoints", node_free_standard_storage, node_copy_standard_storage);
  ntype.geometry_node_execute = blender::nodes::geo_node_rotate_points_exec;
  nodeRegisterType(&ntype);
}
