/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "bvh/bvh.h"

#include "scene/pointcloud.h"
#include "scene/scene.h"

CCL_NAMESPACE_BEGIN

/* PointCloud Point */

void PointCloud::Point::bounds_grow(const packed_float3 *points,
                                    const float *radius,
                                    BoundBox &bounds) const
{
  bounds.grow(points[index], radius[index]);
}

void PointCloud::Point::bounds_grow(const packed_float3 *points,
                                    const float *radius,
                                    const Transform &aligned_space,
                                    BoundBox &bounds) const
{
  const float3 P = transform_point(&aligned_space, float3(points[index]));
  bounds.grow(P, radius[index]);
}

void PointCloud::Point::bounds_grow(const float4 &point, BoundBox &bounds) const
{
  bounds.grow(make_float3(point), point.w);
}

float4 PointCloud::Point::motion_key(const float *radius,
                                     const Attribute *attr_P,
                                     const Attribute *attr_R,
                                     const size_t num_steps,
                                     const float time,
                                     size_t p) const
{
  /* Figure out which steps we need to fetch and their
   * interpolation factor. */
  const size_t max_step = num_steps - 1;
  const size_t step = min((size_t)(time * max_step), max_step - 1);
  const float t = time * max_step - step;
  /* Fetch vertex coordinates. */
  const float4 curr_key = point_for_step(radius, attr_P, attr_R, step, p);
  const float4 next_key = point_for_step(radius, attr_P, attr_R, step + 1, p);
  /* Interpolate between steps. */
  return (1.0f - t) * curr_key + t * next_key;
}

float4 PointCloud::Point::point_for_step(const float *radius,
                                         const Attribute *attr_P,
                                         const Attribute *attr_R,
                                         const size_t step,
                                         const size_t p) const
{
  const int num_steps = attr_P->num_motion_steps();
  const float r = attr_R ? attr_R->data_at_time_step<float>(step, num_steps)[p] : radius[p];
  return make_float4(float3(attr_P->data_at_time_step<packed_float3>(step, num_steps)[p]), r);
}

/* PointCloud */

NODE_DEFINE(PointCloud)
{
  NodeType *type = NodeType::add(
      "pointcloud", create, NodeType::NONE, Geometry::get_node_base_type());

  SOCKET_INT_ARRAY(shader, "Shader", array<int>());

  return type;
}

PointCloud::PointCloud() : Geometry(get_node_type(), Geometry::POINTCLOUD)
{
  add_builtin_attributes();
}

PointCloud::~PointCloud() = default;

void PointCloud::add_builtin_attributes()
{
  attributes.add(ATTR_STD_POSITION);
  attributes.add(ATTR_STD_RADIUS);
}

void PointCloud::resize(const int numpoints)
{
  Attribute *attr_P = attributes.add(ATTR_STD_POSITION);
  attr_P->resize(numpoints);
  Attribute *attr_R = attributes.add(ATTR_STD_RADIUS);
  attr_R->resize(numpoints);
  shader.resize(numpoints);
  attributes.resize();

  tag_position_modified();
  tag_radius_modified();
  tag_shader_modified();
}

void PointCloud::clear_non_sockets()
{
  Geometry::clear(true);
}

void PointCloud::clear(const bool preserve_shaders)
{
  Geometry::clear(preserve_shaders);

  shader.clear();
  attributes.clear();
  add_builtin_attributes();

  tag_position_modified();
  tag_radius_modified();
  tag_shader_modified();
}

void PointCloud::copy_center_to_motion_step(const int motion_step)
{
  const int attr_step = motion_step + 1;
  const size_t numpoints = num_points();

  Attribute *attr_P = attributes.find(ATTR_STD_POSITION);
  if (attr_P->has_motion()) {
    std::copy_n(get_position(), numpoints, attr_P->data_for_write<packed_float3>(attr_step));
  }

  Attribute *attr_R = attributes.find(ATTR_STD_RADIUS);
  if (attr_R->has_motion()) {
    std::copy_n(get_radius(), numpoints, attr_R->data_for_write<float>(attr_step));
  }
}

void PointCloud::get_uv_tiles(ustring map, unordered_set<int> &tiles)
{
  Attribute *attr;

  if (map.empty()) {
    attr = attributes.find(ATTR_STD_UV);
  }
  else {
    attr = attributes.find(map);
  }

  if (attr) {
    attr->get_uv_tiles(this, ATTR_PRIM_GEOMETRY, tiles);
  }
}

void PointCloud::compute_bounds()
{
  BoundBox bnds = BoundBox::empty;
  const size_t numpoints = num_points();
  const packed_float3 *points_data = get_position();
  const float *radius_data = get_radius();

  if (numpoints > 0) {
    for (size_t i = 0; i < numpoints; i++) {
      bnds.grow(points_data[i], radius_data[i]);
    }

    const Attribute *attr_P = attributes.find(ATTR_STD_POSITION);
    const Attribute *attr_R = attributes.find(ATTR_STD_RADIUS);
    if (use_motion_blur && attr_P->has_motion()) {
      for (int attr_step = 1; attr_step < attr_P->num_motion_steps(); attr_step++) {
        const packed_float3 *motion_P = attr_P->data<packed_float3>(attr_step);
        const float *motion_R = attr_R->data<float>(attr_step);
        for (size_t i = 0; i < numpoints; i++) {
          bnds.grow(motion_P[i], motion_R[i]);
        }
      }
    }

    if (!bnds.valid()) {
      bnds = BoundBox::empty;

      /* skip nan or inf coordinates */
      for (size_t i = 0; i < numpoints; i++) {
        bnds.grow_safe(points_data[i], radius_data[i]);
      }

      if (use_motion_blur && attr_P->has_motion()) {
        for (int attr_step = 1; attr_step < attr_P->num_motion_steps(); attr_step++) {
          const packed_float3 *motion_P = attr_P->data<packed_float3>(attr_step);
          const float *motion_R = attr_R->data<float>(attr_step);
          for (size_t i = 0; i < numpoints; i++) {
            bnds.grow_safe(motion_P[i], motion_R[i]);
          }
        }
      }
    }
  }

  if (!bnds.valid()) {
    /* empty mesh */
    bnds.grow(make_float3(0.0f, 0.0f, 0.0f));
  }

  bounds = bnds;
}

void PointCloud::apply_transform(const Transform &tfm, const bool apply_to_motion)
{
  /* compute uniform scale */
  const float3 c0 = transform_get_column(&tfm, 0);
  const float3 c1 = transform_get_column(&tfm, 1);
  const float3 c2 = transform_get_column(&tfm, 2);
  const float scalar = powf(fabsf(dot(cross(c0, c1), c2)), 1.0f / 3.0f);

  /* apply transform to points */
  packed_float3 *points_data = get_position_for_write();
  float *radius_data = get_radius_for_write();
  const size_t numpoints = num_points();
  for (size_t i = 0; i < numpoints; i++) {
    const float3 co = transform_point(&tfm, points_data[i]);
    const float r = radius_data[i] * scalar;

    /* scale for radius is only correct for uniform scale */
    points_data[i] = co;
    radius_data[i] = r;
  }

  if (apply_to_motion) {
    Attribute *attr_P = attributes.find(ATTR_STD_POSITION);
    Attribute *attr_R = attributes.find(ATTR_STD_RADIUS);

    if (attr_P->has_motion()) {
      const bool has_motion_radius = attr_R->has_motion();
      for (int step = 1; step <= int(attr_P->motion.size()); step++) {
        packed_float3 *motion_P = attr_P->data_for_write<packed_float3>(step);
        float *motion_R = has_motion_radius ? attr_R->data_for_write<float>(step) : nullptr;
        for (size_t i = 0; i < numpoints; i++) {
          motion_P[i] = transform_point(&tfm, motion_P[i]);
          if (motion_R) {
            /* scale for curve radius is only correct for uniform scale */
            motion_R[i] *= scalar;
          }
        }
      }
    }
  }
}

void PointCloud::pack(Scene *scene, uint *packed_shader)
{
  const size_t numpoints = num_points();
  int *shader_data = shader.data();

  uint shader_id = 0;
  uint last_shader = -1;
  for (size_t i = 0; i < numpoints; i++) {
    if (last_shader != shader_data[i]) {
      last_shader = shader_data[i];
      Shader *shader = (last_shader < used_shaders.size()) ?
                           static_cast<Shader *>(used_shaders[last_shader]) :
                           scene->default_surface;
      shader_id = scene->shader_manager->get_shader_id(shader);
    }
    packed_shader[i] = shader_id;
  }
}

PrimitiveType PointCloud::primitive_type() const
{
  return has_motion_blur() ? PRIMITIVE_MOTION_POINT : PRIMITIVE_POINT;
}

CCL_NAMESPACE_END
