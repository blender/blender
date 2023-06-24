/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "bvh/bvh.h"

#include "scene/pointcloud.h"
#include "scene/scene.h"

CCL_NAMESPACE_BEGIN

/* PointCloud Point */

void PointCloud::Point::bounds_grow(const float3 *points,
                                    const float *radius,
                                    BoundBox &bounds) const
{
  bounds.grow(points[index], radius[index]);
}

void PointCloud::Point::bounds_grow(const float3 *points,
                                    const float *radius,
                                    const Transform &aligned_space,
                                    BoundBox &bounds) const
{
  float3 P = transform_point(&aligned_space, points[index]);
  bounds.grow(P, radius[index]);
}

void PointCloud::Point::bounds_grow(const float4 &point, BoundBox &bounds) const
{
  bounds.grow(float4_to_float3(point), point.w);
}

float4 PointCloud::Point::motion_key(const float3 *points,
                                     const float *radius,
                                     const float3 *point_steps,
                                     size_t num_points,
                                     size_t num_steps,
                                     float time,
                                     size_t p) const
{
  /* Figure out which steps we need to fetch and their
   * interpolation factor. */
  const size_t max_step = num_steps - 1;
  const size_t step = min((size_t)(time * max_step), max_step - 1);
  const float t = time * max_step - step;
  /* Fetch vertex coordinates. */
  const float4 curr_key = point_for_step(
      points, radius, point_steps, num_points, num_steps, step, p);
  const float4 next_key = point_for_step(
      points, radius, point_steps, num_points, num_steps, step + 1, p);
  /* Interpolate between steps. */
  return (1.0f - t) * curr_key + t * next_key;
}

float4 PointCloud::Point::point_for_step(const float3 *points,
                                         const float *radius,
                                         const float3 *point_steps,
                                         size_t num_points,
                                         size_t num_steps,
                                         size_t step,
                                         size_t p) const
{
  const size_t center_step = ((num_steps - 1) / 2);
  if (step == center_step) {
    /* Center step: regular key location. */
    return make_float4(points[p].x, points[p].y, points[p].z, radius[p]);
  }
  else {
    /* Center step is not stored in this array. */
    if (step > center_step) {
      step--;
    }
    const size_t offset = step * num_points;
    return make_float4(point_steps[offset + p].x,
                       point_steps[offset + p].y,
                       point_steps[offset + p].z,
                       radius[offset + p]);
  }
}

/* PointCloud */

NODE_DEFINE(PointCloud)
{
  NodeType *type = NodeType::add(
      "pointcloud", create, NodeType::NONE, Geometry::get_node_base_type());

  SOCKET_POINT_ARRAY(points, "Points", array<float3>());
  SOCKET_FLOAT_ARRAY(radius, "Radius", array<float>());
  SOCKET_INT_ARRAY(shader, "Shader", array<int>());

  return type;
}

PointCloud::PointCloud() : Geometry(node_type, Geometry::POINTCLOUD) {}

PointCloud::~PointCloud() {}

void PointCloud::resize(int numpoints)
{
  points.resize(numpoints);
  radius.resize(numpoints);
  shader.resize(numpoints);
  attributes.resize();

  tag_points_modified();
  tag_radius_modified();
  tag_shader_modified();
}

void PointCloud::reserve(int numpoints)
{
  points.reserve(numpoints);
  radius.reserve(numpoints);
  shader.reserve(numpoints);
  attributes.resize(true);
}

void PointCloud::clear(const bool preserve_shaders)
{
  Geometry::clear(preserve_shaders);

  points.clear();
  radius.clear();
  shader.clear();
  attributes.clear();

  tag_points_modified();
  tag_radius_modified();
  tag_shader_modified();
}

void PointCloud::add_point(float3 co, float r, int shader_index)
{
  points.push_back_reserved(co);
  radius.push_back_reserved(r);
  shader.push_back_reserved(shader_index);

  tag_points_modified();
  tag_radius_modified();
  tag_shader_modified();
}

void PointCloud::copy_center_to_motion_step(const int motion_step)
{
  Attribute *attr_mP = attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
  if (attr_mP) {
    float3 *points_data = points.data();
    size_t numpoints = points.size();
    float *radius_data = radius.data();

    float4 *attrib_P = attr_mP->data_float4() + motion_step * numpoints;
    for (int i = 0; i < numpoints; i++) {
      float3 P = points_data[i];
      float r = radius_data[i];
      attrib_P[i] = make_float4(P.x, P.y, P.z, r);
    }
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
  size_t numpoints = points.size();

  if (numpoints > 0) {
    for (size_t i = 0; i < numpoints; i++) {
      bnds.grow(points[i], radius[i]);
    }

    Attribute *attr = attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
    if (use_motion_blur && attr) {
      size_t steps_size = points.size() * (motion_steps - 1);
      float3 *point_steps = attr->data_float3();

      for (size_t i = 0; i < steps_size; i++)
        bnds.grow(point_steps[i]);
    }

    if (!bnds.valid()) {
      bnds = BoundBox::empty;

      /* skip nan or inf coordinates */
      for (size_t i = 0; i < numpoints; i++)
        bnds.grow_safe(points[i], radius[i]);

      if (use_motion_blur && attr) {
        size_t steps_size = points.size() * (motion_steps - 1);
        float3 *point_steps = attr->data_float3();

        for (size_t i = 0; i < steps_size; i++)
          bnds.grow_safe(point_steps[i]);
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
  float3 c0 = transform_get_column(&tfm, 0);
  float3 c1 = transform_get_column(&tfm, 1);
  float3 c2 = transform_get_column(&tfm, 2);
  float scalar = powf(fabsf(dot(cross(c0, c1), c2)), 1.0f / 3.0f);

  /* apply transform to curve keys */
  for (size_t i = 0; i < points.size(); i++) {
    float3 co = transform_point(&tfm, points[i]);
    float r = radius[i] * scalar;

    /* scale for curve radius is only correct for uniform scale
     */
    points[i] = co;
    radius[i] = r;
  }

  if (apply_to_motion) {
    Attribute *attr = attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);

    if (attr) {
      /* apply transform to motion curve keys */
      size_t steps_size = points.size() * (motion_steps - 1);
      float4 *point_steps = attr->data_float4();

      for (size_t i = 0; i < steps_size; i++) {
        float3 co = transform_point(&tfm, float4_to_float3(point_steps[i]));
        float radius = point_steps[i].w * scalar;

        /* scale for curve radius is only correct for uniform
         * scale */
        point_steps[i] = float3_to_float4(co);
        point_steps[i].w = radius;
      }
    }
  }
}

void PointCloud::pack(Scene *scene, float4 *packed_points, uint *packed_shader)
{
  size_t numpoints = points.size();
  float3 *points_data = points.data();
  float *radius_data = radius.data();
  int *shader_data = shader.data();

  for (size_t i = 0; i < numpoints; i++) {
    packed_points[i] = make_float4(
        points_data[i].x, points_data[i].y, points_data[i].z, radius_data[i]);
  }

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
