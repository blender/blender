/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "bvh/bvh.h"
#include "bvh/bvh_build.h"

#include "render/camera.h"
#include "render/curves.h"
#include "device/device.h"
#include "render/graph.h"
#include "render/shader.h"
#include "render/light.h"
#include "render/mesh.h"
#include "render/nodes.h"
#include "render/object.h"
#include "render/scene.h"
#include "render/stats.h"

#include "kernel/osl/osl_globals.h"

#include "subd/subd_split.h"
#include "subd/subd_patch_table.h"

#include "util/util_foreach.h"
#include "util/util_logging.h"
#include "util/util_progress.h"
#include "util/util_set.h"

#ifdef WITH_EMBREE
#  include "bvh/bvh_embree.h"
#endif

CCL_NAMESPACE_BEGIN

/* Triangle */

void Mesh::Triangle::bounds_grow(const float3 *verts, BoundBox &bounds) const
{
  bounds.grow(verts[v[0]]);
  bounds.grow(verts[v[1]]);
  bounds.grow(verts[v[2]]);
}

void Mesh::Triangle::motion_verts(const float3 *verts,
                                  const float3 *vert_steps,
                                  size_t num_verts,
                                  size_t num_steps,
                                  float time,
                                  float3 r_verts[3]) const
{
  /* Figure out which steps we need to fetch and their interpolation factor. */
  const size_t max_step = num_steps - 1;
  const size_t step = min((int)(time * max_step), max_step - 1);
  const float t = time * max_step - step;
  /* Fetch vertex coordinates. */
  float3 curr_verts[3];
  float3 next_verts[3];
  verts_for_step(verts, vert_steps, num_verts, num_steps, step, curr_verts);
  verts_for_step(verts, vert_steps, num_verts, num_steps, step + 1, next_verts);
  /* Interpolate between steps. */
  r_verts[0] = (1.0f - t) * curr_verts[0] + t * next_verts[0];
  r_verts[1] = (1.0f - t) * curr_verts[1] + t * next_verts[1];
  r_verts[2] = (1.0f - t) * curr_verts[2] + t * next_verts[2];
}

void Mesh::Triangle::verts_for_step(const float3 *verts,
                                    const float3 *vert_steps,
                                    size_t num_verts,
                                    size_t num_steps,
                                    size_t step,
                                    float3 r_verts[3]) const
{
  const size_t center_step = ((num_steps - 1) / 2);
  if (step == center_step) {
    /* Center step: regular vertex location. */
    r_verts[0] = verts[v[0]];
    r_verts[1] = verts[v[1]];
    r_verts[2] = verts[v[2]];
  }
  else {
    /* Center step not stored in the attribute array array. */
    if (step > center_step) {
      step--;
    }
    size_t offset = step * num_verts;
    r_verts[0] = vert_steps[offset + v[0]];
    r_verts[1] = vert_steps[offset + v[1]];
    r_verts[2] = vert_steps[offset + v[2]];
  }
}

float3 Mesh::Triangle::compute_normal(const float3 *verts) const
{
  const float3 &v0 = verts[v[0]];
  const float3 &v1 = verts[v[1]];
  const float3 &v2 = verts[v[2]];
  const float3 norm = cross(v1 - v0, v2 - v0);
  const float normlen = len(norm);
  if (normlen == 0.0f) {
    return make_float3(1.0f, 0.0f, 0.0f);
  }
  return norm / normlen;
}

bool Mesh::Triangle::valid(const float3 *verts) const
{
  return isfinite3_safe(verts[v[0]]) && isfinite3_safe(verts[v[1]]) && isfinite3_safe(verts[v[2]]);
}

/* Curve */

void Mesh::Curve::bounds_grow(const int k,
                              const float3 *curve_keys,
                              const float *curve_radius,
                              BoundBox &bounds) const
{
  float3 P[4];

  P[0] = curve_keys[max(first_key + k - 1, first_key)];
  P[1] = curve_keys[first_key + k];
  P[2] = curve_keys[first_key + k + 1];
  P[3] = curve_keys[min(first_key + k + 2, first_key + num_keys - 1)];

  float3 lower;
  float3 upper;

  curvebounds(&lower.x, &upper.x, P, 0);
  curvebounds(&lower.y, &upper.y, P, 1);
  curvebounds(&lower.z, &upper.z, P, 2);

  float mr = max(curve_radius[first_key + k], curve_radius[first_key + k + 1]);

  bounds.grow(lower, mr);
  bounds.grow(upper, mr);
}

void Mesh::Curve::bounds_grow(const int k,
                              const float3 *curve_keys,
                              const float *curve_radius,
                              const Transform &aligned_space,
                              BoundBox &bounds) const
{
  float3 P[4];

  P[0] = curve_keys[max(first_key + k - 1, first_key)];
  P[1] = curve_keys[first_key + k];
  P[2] = curve_keys[first_key + k + 1];
  P[3] = curve_keys[min(first_key + k + 2, first_key + num_keys - 1)];

  P[0] = transform_point(&aligned_space, P[0]);
  P[1] = transform_point(&aligned_space, P[1]);
  P[2] = transform_point(&aligned_space, P[2]);
  P[3] = transform_point(&aligned_space, P[3]);

  float3 lower;
  float3 upper;

  curvebounds(&lower.x, &upper.x, P, 0);
  curvebounds(&lower.y, &upper.y, P, 1);
  curvebounds(&lower.z, &upper.z, P, 2);

  float mr = max(curve_radius[first_key + k], curve_radius[first_key + k + 1]);

  bounds.grow(lower, mr);
  bounds.grow(upper, mr);
}

void Mesh::Curve::bounds_grow(float4 keys[4], BoundBox &bounds) const
{
  float3 P[4] = {
      float4_to_float3(keys[0]),
      float4_to_float3(keys[1]),
      float4_to_float3(keys[2]),
      float4_to_float3(keys[3]),
  };

  float3 lower;
  float3 upper;

  curvebounds(&lower.x, &upper.x, P, 0);
  curvebounds(&lower.y, &upper.y, P, 1);
  curvebounds(&lower.z, &upper.z, P, 2);

  float mr = max(keys[1].w, keys[2].w);

  bounds.grow(lower, mr);
  bounds.grow(upper, mr);
}

void Mesh::Curve::motion_keys(const float3 *curve_keys,
                              const float *curve_radius,
                              const float3 *key_steps,
                              size_t num_curve_keys,
                              size_t num_steps,
                              float time,
                              size_t k0,
                              size_t k1,
                              float4 r_keys[2]) const
{
  /* Figure out which steps we need to fetch and their interpolation factor. */
  const size_t max_step = num_steps - 1;
  const size_t step = min((int)(time * max_step), max_step - 1);
  const float t = time * max_step - step;
  /* Fetch vertex coordinates. */
  float4 curr_keys[2];
  float4 next_keys[2];
  keys_for_step(
      curve_keys, curve_radius, key_steps, num_curve_keys, num_steps, step, k0, k1, curr_keys);
  keys_for_step(
      curve_keys, curve_radius, key_steps, num_curve_keys, num_steps, step + 1, k0, k1, next_keys);
  /* Interpolate between steps. */
  r_keys[0] = (1.0f - t) * curr_keys[0] + t * next_keys[0];
  r_keys[1] = (1.0f - t) * curr_keys[1] + t * next_keys[1];
}

void Mesh::Curve::cardinal_motion_keys(const float3 *curve_keys,
                                       const float *curve_radius,
                                       const float3 *key_steps,
                                       size_t num_curve_keys,
                                       size_t num_steps,
                                       float time,
                                       size_t k0,
                                       size_t k1,
                                       size_t k2,
                                       size_t k3,
                                       float4 r_keys[4]) const
{
  /* Figure out which steps we need to fetch and their interpolation factor. */
  const size_t max_step = num_steps - 1;
  const size_t step = min((int)(time * max_step), max_step - 1);
  const float t = time * max_step - step;
  /* Fetch vertex coordinates. */
  float4 curr_keys[4];
  float4 next_keys[4];
  cardinal_keys_for_step(curve_keys,
                         curve_radius,
                         key_steps,
                         num_curve_keys,
                         num_steps,
                         step,
                         k0,
                         k1,
                         k2,
                         k3,
                         curr_keys);
  cardinal_keys_for_step(curve_keys,
                         curve_radius,
                         key_steps,
                         num_curve_keys,
                         num_steps,
                         step + 1,
                         k0,
                         k1,
                         k2,
                         k3,
                         next_keys);
  /* Interpolate between steps. */
  r_keys[0] = (1.0f - t) * curr_keys[0] + t * next_keys[0];
  r_keys[1] = (1.0f - t) * curr_keys[1] + t * next_keys[1];
  r_keys[2] = (1.0f - t) * curr_keys[2] + t * next_keys[2];
  r_keys[3] = (1.0f - t) * curr_keys[3] + t * next_keys[3];
}

void Mesh::Curve::keys_for_step(const float3 *curve_keys,
                                const float *curve_radius,
                                const float3 *key_steps,
                                size_t num_curve_keys,
                                size_t num_steps,
                                size_t step,
                                size_t k0,
                                size_t k1,
                                float4 r_keys[2]) const
{
  k0 = max(k0, 0);
  k1 = min(k1, num_keys - 1);
  const size_t center_step = ((num_steps - 1) / 2);
  if (step == center_step) {
    /* Center step: regular key location. */
    /* TODO(sergey): Consider adding make_float4(float3, float)
     * function.
     */
    r_keys[0] = make_float4(curve_keys[first_key + k0].x,
                            curve_keys[first_key + k0].y,
                            curve_keys[first_key + k0].z,
                            curve_radius[first_key + k0]);
    r_keys[1] = make_float4(curve_keys[first_key + k1].x,
                            curve_keys[first_key + k1].y,
                            curve_keys[first_key + k1].z,
                            curve_radius[first_key + k1]);
  }
  else {
    /* Center step is not stored in this array. */
    if (step > center_step) {
      step--;
    }
    const size_t offset = first_key + step * num_curve_keys;
    r_keys[0] = make_float4(key_steps[offset + k0].x,
                            key_steps[offset + k0].y,
                            key_steps[offset + k0].z,
                            curve_radius[first_key + k0]);
    r_keys[1] = make_float4(key_steps[offset + k1].x,
                            key_steps[offset + k1].y,
                            key_steps[offset + k1].z,
                            curve_radius[first_key + k1]);
  }
}

void Mesh::Curve::cardinal_keys_for_step(const float3 *curve_keys,
                                         const float *curve_radius,
                                         const float3 *key_steps,
                                         size_t num_curve_keys,
                                         size_t num_steps,
                                         size_t step,
                                         size_t k0,
                                         size_t k1,
                                         size_t k2,
                                         size_t k3,
                                         float4 r_keys[4]) const
{
  k0 = max(k0, 0);
  k3 = min(k3, num_keys - 1);
  const size_t center_step = ((num_steps - 1) / 2);
  if (step == center_step) {
    /* Center step: regular key location. */
    r_keys[0] = make_float4(curve_keys[first_key + k0].x,
                            curve_keys[first_key + k0].y,
                            curve_keys[first_key + k0].z,
                            curve_radius[first_key + k0]);
    r_keys[1] = make_float4(curve_keys[first_key + k1].x,
                            curve_keys[first_key + k1].y,
                            curve_keys[first_key + k1].z,
                            curve_radius[first_key + k1]);
    r_keys[2] = make_float4(curve_keys[first_key + k2].x,
                            curve_keys[first_key + k2].y,
                            curve_keys[first_key + k2].z,
                            curve_radius[first_key + k2]);
    r_keys[3] = make_float4(curve_keys[first_key + k3].x,
                            curve_keys[first_key + k3].y,
                            curve_keys[first_key + k3].z,
                            curve_radius[first_key + k3]);
  }
  else {
    /* Center step is not stored in this array. */
    if (step > center_step) {
      step--;
    }
    const size_t offset = first_key + step * num_curve_keys;
    r_keys[0] = make_float4(key_steps[offset + k0].x,
                            key_steps[offset + k0].y,
                            key_steps[offset + k0].z,
                            curve_radius[first_key + k0]);
    r_keys[1] = make_float4(key_steps[offset + k1].x,
                            key_steps[offset + k1].y,
                            key_steps[offset + k1].z,
                            curve_radius[first_key + k1]);
    r_keys[2] = make_float4(key_steps[offset + k2].x,
                            key_steps[offset + k2].y,
                            key_steps[offset + k2].z,
                            curve_radius[first_key + k2]);
    r_keys[3] = make_float4(key_steps[offset + k3].x,
                            key_steps[offset + k3].y,
                            key_steps[offset + k3].z,
                            curve_radius[first_key + k3]);
  }
}

/* SubdFace */

float3 Mesh::SubdFace::normal(const Mesh *mesh) const
{
  float3 v0 = mesh->verts[mesh->subd_face_corners[start_corner + 0]];
  float3 v1 = mesh->verts[mesh->subd_face_corners[start_corner + 1]];
  float3 v2 = mesh->verts[mesh->subd_face_corners[start_corner + 2]];

  return safe_normalize(cross(v1 - v0, v2 - v0));
}

/* Mesh */

NODE_DEFINE(Mesh)
{
  NodeType *type = NodeType::add("mesh", create);

  SOCKET_UINT(motion_steps, "Motion Steps", 3);
  SOCKET_BOOLEAN(use_motion_blur, "Use Motion Blur", false);

  SOCKET_INT_ARRAY(triangles, "Triangles", array<int>());
  SOCKET_POINT_ARRAY(verts, "Vertices", array<float3>());
  SOCKET_INT_ARRAY(shader, "Shader", array<int>());
  SOCKET_BOOLEAN_ARRAY(smooth, "Smooth", array<bool>());

  SOCKET_POINT_ARRAY(curve_keys, "Curve Keys", array<float3>());
  SOCKET_FLOAT_ARRAY(curve_radius, "Curve Radius", array<float>());
  SOCKET_INT_ARRAY(curve_first_key, "Curve First Key", array<int>());
  SOCKET_INT_ARRAY(curve_shader, "Curve Shader", array<int>());

  return type;
}

Mesh::Mesh() : Node(node_type)
{
  need_update = true;
  need_update_rebuild = false;
  transform_applied = false;
  transform_negative_scaled = false;
  transform_normal = transform_identity();
  bounds = BoundBox::empty;

  bvh = NULL;

  tri_offset = 0;
  vert_offset = 0;

  curve_offset = 0;
  curvekey_offset = 0;

  patch_offset = 0;
  face_offset = 0;
  corner_offset = 0;

  attr_map_offset = 0;

  num_subd_verts = 0;

  attributes.triangle_mesh = this;
  curve_attributes.curve_mesh = this;
  subd_attributes.subd_mesh = this;

  geometry_flags = GEOMETRY_NONE;

  volume_isovalue = 0.001f;
  has_volume = false;
  has_surface_bssrdf = false;

  num_ngons = 0;

  subdivision_type = SUBDIVISION_NONE;
  subd_params = NULL;

  patch_table = NULL;
}

Mesh::~Mesh()
{
  delete bvh;
  delete patch_table;
  delete subd_params;
}

void Mesh::resize_mesh(int numverts, int numtris)
{
  verts.resize(numverts);
  triangles.resize(numtris * 3);
  shader.resize(numtris);
  smooth.resize(numtris);

  if (subd_faces.size()) {
    triangle_patch.resize(numtris);
    vert_patch_uv.resize(numverts);
  }

  attributes.resize();
}

void Mesh::reserve_mesh(int numverts, int numtris)
{
  /* reserve space to add verts and triangles later */
  verts.reserve(numverts);
  triangles.reserve(numtris * 3);
  shader.reserve(numtris);
  smooth.reserve(numtris);

  if (subd_faces.size()) {
    triangle_patch.reserve(numtris);
    vert_patch_uv.reserve(numverts);
  }

  attributes.resize(true);
}

void Mesh::resize_curves(int numcurves, int numkeys)
{
  curve_keys.resize(numkeys);
  curve_radius.resize(numkeys);
  curve_first_key.resize(numcurves);
  curve_shader.resize(numcurves);

  curve_attributes.resize();
}

void Mesh::reserve_curves(int numcurves, int numkeys)
{
  curve_keys.reserve(numkeys);
  curve_radius.reserve(numkeys);
  curve_first_key.reserve(numcurves);
  curve_shader.reserve(numcurves);

  curve_attributes.resize(true);
}

void Mesh::resize_subd_faces(int numfaces, int num_ngons_, int numcorners)
{
  subd_faces.resize(numfaces);
  subd_face_corners.resize(numcorners);
  num_ngons = num_ngons_;

  subd_attributes.resize();
}

void Mesh::reserve_subd_faces(int numfaces, int num_ngons_, int numcorners)
{
  subd_faces.reserve(numfaces);
  subd_face_corners.reserve(numcorners);
  num_ngons = num_ngons_;

  subd_attributes.resize(true);
}

void Mesh::clear(bool preserve_voxel_data)
{
  /* clear all verts and triangles */
  verts.clear();
  triangles.clear();
  shader.clear();
  smooth.clear();

  triangle_patch.clear();
  vert_patch_uv.clear();

  curve_keys.clear();
  curve_radius.clear();
  curve_first_key.clear();
  curve_shader.clear();

  subd_faces.clear();
  subd_face_corners.clear();

  num_subd_verts = 0;

  subd_creases.clear();

  curve_attributes.clear();
  subd_attributes.clear();
  attributes.clear(preserve_voxel_data);

  used_shaders.clear();

  if (!preserve_voxel_data) {
    geometry_flags = GEOMETRY_NONE;
  }

  transform_applied = false;
  transform_negative_scaled = false;
  transform_normal = transform_identity();

  delete patch_table;
  patch_table = NULL;
}

void Mesh::add_vertex(float3 P)
{
  verts.push_back_reserved(P);

  if (subd_faces.size()) {
    vert_patch_uv.push_back_reserved(make_float2(0.0f, 0.0f));
  }
}

void Mesh::add_vertex_slow(float3 P)
{
  verts.push_back_slow(P);

  if (subd_faces.size()) {
    vert_patch_uv.push_back_slow(make_float2(0.0f, 0.0f));
  }
}

void Mesh::add_triangle(int v0, int v1, int v2, int shader_, bool smooth_)
{
  triangles.push_back_reserved(v0);
  triangles.push_back_reserved(v1);
  triangles.push_back_reserved(v2);
  shader.push_back_reserved(shader_);
  smooth.push_back_reserved(smooth_);

  if (subd_faces.size()) {
    triangle_patch.push_back_reserved(-1);
  }
}

void Mesh::add_curve_key(float3 co, float radius)
{
  curve_keys.push_back_reserved(co);
  curve_radius.push_back_reserved(radius);
}

void Mesh::add_curve(int first_key, int shader)
{
  curve_first_key.push_back_reserved(first_key);
  curve_shader.push_back_reserved(shader);
}

void Mesh::add_subd_face(int *corners, int num_corners, int shader_, bool smooth_)
{
  int start_corner = subd_face_corners.size();

  for (int i = 0; i < num_corners; i++) {
    subd_face_corners.push_back_reserved(corners[i]);
  }

  int ptex_offset = 0;

  if (subd_faces.size()) {
    SubdFace &s = subd_faces[subd_faces.size() - 1];
    ptex_offset = s.ptex_offset + s.num_ptex_faces();
  }

  SubdFace face = {start_corner, num_corners, shader_, smooth_, ptex_offset};
  subd_faces.push_back_reserved(face);
}

void Mesh::compute_bounds()
{
  BoundBox bnds = BoundBox::empty;
  size_t verts_size = verts.size();
  size_t curve_keys_size = curve_keys.size();

  if (verts_size + curve_keys_size > 0) {
    for (size_t i = 0; i < verts_size; i++)
      bnds.grow(verts[i]);

    for (size_t i = 0; i < curve_keys_size; i++)
      bnds.grow(curve_keys[i], curve_radius[i]);

    Attribute *attr = attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
    if (use_motion_blur && attr) {
      size_t steps_size = verts.size() * (motion_steps - 1);
      float3 *vert_steps = attr->data_float3();

      for (size_t i = 0; i < steps_size; i++)
        bnds.grow(vert_steps[i]);
    }

    Attribute *curve_attr = curve_attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
    if (use_motion_blur && curve_attr) {
      size_t steps_size = curve_keys.size() * (motion_steps - 1);
      float3 *key_steps = curve_attr->data_float3();

      for (size_t i = 0; i < steps_size; i++)
        bnds.grow(key_steps[i]);
    }

    if (!bnds.valid()) {
      bnds = BoundBox::empty;

      /* skip nan or inf coordinates */
      for (size_t i = 0; i < verts_size; i++)
        bnds.grow_safe(verts[i]);

      for (size_t i = 0; i < curve_keys_size; i++)
        bnds.grow_safe(curve_keys[i], curve_radius[i]);

      if (use_motion_blur && attr) {
        size_t steps_size = verts.size() * (motion_steps - 1);
        float3 *vert_steps = attr->data_float3();

        for (size_t i = 0; i < steps_size; i++)
          bnds.grow_safe(vert_steps[i]);
      }

      if (use_motion_blur && curve_attr) {
        size_t steps_size = curve_keys.size() * (motion_steps - 1);
        float3 *key_steps = curve_attr->data_float3();

        for (size_t i = 0; i < steps_size; i++)
          bnds.grow_safe(key_steps[i]);
      }
    }
  }

  if (!bnds.valid()) {
    /* empty mesh */
    bnds.grow(make_float3(0.0f, 0.0f, 0.0f));
  }

  bounds = bnds;
}

void Mesh::add_face_normals()
{
  /* don't compute if already there */
  if (attributes.find(ATTR_STD_FACE_NORMAL))
    return;

  /* get attributes */
  Attribute *attr_fN = attributes.add(ATTR_STD_FACE_NORMAL);
  float3 *fN = attr_fN->data_float3();

  /* compute face normals */
  size_t triangles_size = num_triangles();

  if (triangles_size) {
    float3 *verts_ptr = verts.data();

    for (size_t i = 0; i < triangles_size; i++) {
      fN[i] = get_triangle(i).compute_normal(verts_ptr);
    }
  }

  /* expected to be in local space */
  if (transform_applied) {
    Transform ntfm = transform_inverse(transform_normal);

    for (size_t i = 0; i < triangles_size; i++)
      fN[i] = normalize(transform_direction(&ntfm, fN[i]));
  }
}

void Mesh::add_vertex_normals()
{
  bool flip = transform_negative_scaled;
  size_t verts_size = verts.size();
  size_t triangles_size = num_triangles();

  /* static vertex normals */
  if (!attributes.find(ATTR_STD_VERTEX_NORMAL) && triangles_size) {
    /* get attributes */
    Attribute *attr_fN = attributes.find(ATTR_STD_FACE_NORMAL);
    Attribute *attr_vN = attributes.add(ATTR_STD_VERTEX_NORMAL);

    float3 *fN = attr_fN->data_float3();
    float3 *vN = attr_vN->data_float3();

    /* compute vertex normals */
    memset(vN, 0, verts.size() * sizeof(float3));

    for (size_t i = 0; i < triangles_size; i++) {
      for (size_t j = 0; j < 3; j++) {
        vN[get_triangle(i).v[j]] += fN[i];
      }
    }

    for (size_t i = 0; i < verts_size; i++) {
      vN[i] = normalize(vN[i]);
      if (flip) {
        vN[i] = -vN[i];
      }
    }
  }

  /* motion vertex normals */
  Attribute *attr_mP = attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
  Attribute *attr_mN = attributes.find(ATTR_STD_MOTION_VERTEX_NORMAL);

  if (has_motion_blur() && attr_mP && !attr_mN && triangles_size) {
    /* create attribute */
    attr_mN = attributes.add(ATTR_STD_MOTION_VERTEX_NORMAL);

    for (int step = 0; step < motion_steps - 1; step++) {
      float3 *mP = attr_mP->data_float3() + step * verts.size();
      float3 *mN = attr_mN->data_float3() + step * verts.size();

      /* compute */
      memset(mN, 0, verts.size() * sizeof(float3));

      for (size_t i = 0; i < triangles_size; i++) {
        for (size_t j = 0; j < 3; j++) {
          float3 fN = get_triangle(i).compute_normal(mP);
          mN[get_triangle(i).v[j]] += fN;
        }
      }

      for (size_t i = 0; i < verts_size; i++) {
        mN[i] = normalize(mN[i]);
        if (flip) {
          mN[i] = -mN[i];
        }
      }
    }
  }

  /* subd vertex normals */
  if (!subd_attributes.find(ATTR_STD_VERTEX_NORMAL) && subd_faces.size()) {
    /* get attributes */
    Attribute *attr_vN = subd_attributes.add(ATTR_STD_VERTEX_NORMAL);
    float3 *vN = attr_vN->data_float3();

    /* compute vertex normals */
    memset(vN, 0, verts.size() * sizeof(float3));

    for (size_t i = 0; i < subd_faces.size(); i++) {
      SubdFace &face = subd_faces[i];
      float3 fN = face.normal(this);

      for (size_t j = 0; j < face.num_corners; j++) {
        size_t corner = subd_face_corners[face.start_corner + j];
        vN[corner] += fN;
      }
    }

    for (size_t i = 0; i < verts_size; i++) {
      vN[i] = normalize(vN[i]);
      if (flip) {
        vN[i] = -vN[i];
      }
    }
  }
}

void Mesh::add_undisplaced()
{
  AttributeSet &attrs = (subdivision_type == SUBDIVISION_NONE) ? attributes : subd_attributes;

  /* don't compute if already there */
  if (attrs.find(ATTR_STD_POSITION_UNDISPLACED)) {
    return;
  }

  /* get attribute */
  Attribute *attr = attrs.add(ATTR_STD_POSITION_UNDISPLACED);
  attr->flags |= ATTR_SUBDIVIDED;

  float3 *data = attr->data_float3();

  /* copy verts */
  size_t size = attr->buffer_size(
      this, (subdivision_type == SUBDIVISION_NONE) ? ATTR_PRIM_TRIANGLE : ATTR_PRIM_SUBD);

  /* Center points for ngons aren't stored in Mesh::verts but are included in size since they will
   * be calculated later, we subtract them from size here so we don't have an overflow while
   * copying.
   */
  size -= num_ngons * attr->data_sizeof();

  if (size) {
    memcpy(data, verts.data(), size);
  }
}

void Mesh::pack_shaders(Scene *scene, uint *tri_shader)
{
  uint shader_id = 0;
  uint last_shader = -1;
  bool last_smooth = false;

  size_t triangles_size = num_triangles();
  int *shader_ptr = shader.data();

  for (size_t i = 0; i < triangles_size; i++) {
    if (shader_ptr[i] != last_shader || last_smooth != smooth[i]) {
      last_shader = shader_ptr[i];
      last_smooth = smooth[i];
      Shader *shader = (last_shader < used_shaders.size()) ? used_shaders[last_shader] :
                                                             scene->default_surface;
      shader_id = scene->shader_manager->get_shader_id(shader, last_smooth);
    }

    tri_shader[i] = shader_id;
  }
}

void Mesh::pack_normals(float4 *vnormal)
{
  Attribute *attr_vN = attributes.find(ATTR_STD_VERTEX_NORMAL);
  if (attr_vN == NULL) {
    /* Happens on objects with just hair. */
    return;
  }

  bool do_transform = transform_applied;
  Transform ntfm = transform_normal;

  float3 *vN = attr_vN->data_float3();
  size_t verts_size = verts.size();

  for (size_t i = 0; i < verts_size; i++) {
    float3 vNi = vN[i];

    if (do_transform)
      vNi = safe_normalize(transform_direction(&ntfm, vNi));

    vnormal[i] = make_float4(vNi.x, vNi.y, vNi.z, 0.0f);
  }
}

void Mesh::pack_verts(const vector<uint> &tri_prim_index,
                      uint4 *tri_vindex,
                      uint *tri_patch,
                      float2 *tri_patch_uv,
                      size_t vert_offset,
                      size_t tri_offset)
{
  size_t verts_size = verts.size();

  if (verts_size && subd_faces.size()) {
    float2 *vert_patch_uv_ptr = vert_patch_uv.data();

    for (size_t i = 0; i < verts_size; i++) {
      tri_patch_uv[i] = vert_patch_uv_ptr[i];
    }
  }

  size_t triangles_size = num_triangles();

  for (size_t i = 0; i < triangles_size; i++) {
    Triangle t = get_triangle(i);
    tri_vindex[i] = make_uint4(t.v[0] + vert_offset,
                               t.v[1] + vert_offset,
                               t.v[2] + vert_offset,
                               tri_prim_index[i + tri_offset]);

    tri_patch[i] = (!subd_faces.size()) ? -1 : (triangle_patch[i] * 8 + patch_offset);
  }
}

void Mesh::pack_curves(Scene *scene,
                       float4 *curve_key_co,
                       float4 *curve_data,
                       size_t curvekey_offset)
{
  size_t curve_keys_size = curve_keys.size();

  /* pack curve keys */
  if (curve_keys_size) {
    float3 *keys_ptr = curve_keys.data();
    float *radius_ptr = curve_radius.data();

    for (size_t i = 0; i < curve_keys_size; i++)
      curve_key_co[i] = make_float4(keys_ptr[i].x, keys_ptr[i].y, keys_ptr[i].z, radius_ptr[i]);
  }

  /* pack curve segments */
  size_t curve_num = num_curves();

  for (size_t i = 0; i < curve_num; i++) {
    Curve curve = get_curve(i);
    int shader_id = curve_shader[i];
    Shader *shader = (shader_id < used_shaders.size()) ? used_shaders[shader_id] :
                                                         scene->default_surface;
    shader_id = scene->shader_manager->get_shader_id(shader, false);

    curve_data[i] = make_float4(__int_as_float(curve.first_key + curvekey_offset),
                                __int_as_float(curve.num_keys),
                                __int_as_float(shader_id),
                                0.0f);
  }
}

void Mesh::pack_patches(uint *patch_data, uint vert_offset, uint face_offset, uint corner_offset)
{
  size_t num_faces = subd_faces.size();
  int ngons = 0;

  for (size_t f = 0; f < num_faces; f++) {
    SubdFace face = subd_faces[f];

    if (face.is_quad()) {
      int c[4];
      memcpy(c, &subd_face_corners[face.start_corner], sizeof(int) * 4);

      *(patch_data++) = c[0] + vert_offset;
      *(patch_data++) = c[1] + vert_offset;
      *(patch_data++) = c[2] + vert_offset;
      *(patch_data++) = c[3] + vert_offset;

      *(patch_data++) = f + face_offset;
      *(patch_data++) = face.num_corners;
      *(patch_data++) = face.start_corner + corner_offset;
      *(patch_data++) = 0;
    }
    else {
      for (int i = 0; i < face.num_corners; i++) {
        int c[4];
        c[0] = subd_face_corners[face.start_corner + mod(i + 0, face.num_corners)];
        c[1] = subd_face_corners[face.start_corner + mod(i + 1, face.num_corners)];
        c[2] = verts.size() - num_subd_verts + ngons;
        c[3] = subd_face_corners[face.start_corner + mod(i - 1, face.num_corners)];

        *(patch_data++) = c[0] + vert_offset;
        *(patch_data++) = c[1] + vert_offset;
        *(patch_data++) = c[2] + vert_offset;
        *(patch_data++) = c[3] + vert_offset;

        *(patch_data++) = f + face_offset;
        *(patch_data++) = face.num_corners | (i << 16);
        *(patch_data++) = face.start_corner + corner_offset;
        *(patch_data++) = subd_face_corners.size() + ngons + corner_offset;
      }

      ngons++;
    }
  }
}

void Mesh::compute_bvh(
    Device *device, DeviceScene *dscene, SceneParams *params, Progress *progress, int n, int total)
{
  if (progress->get_cancel())
    return;

  compute_bounds();

  if (need_build_bvh()) {
    string msg = "Updating Mesh BVH ";
    if (name == "")
      msg += string_printf("%u/%u", (uint)(n + 1), (uint)total);
    else
      msg += string_printf("%s %u/%u", name.c_str(), (uint)(n + 1), (uint)total);

    Object object;
    object.mesh = this;

    vector<Object *> objects;
    objects.push_back(&object);

    if (bvh && !need_update_rebuild) {
      progress->set_status(msg, "Refitting BVH");
      bvh->objects = objects;
      bvh->refit(*progress);
    }
    else {
      progress->set_status(msg, "Building BVH");

      BVHParams bparams;
      bparams.use_spatial_split = params->use_bvh_spatial_split;
      bparams.bvh_layout = BVHParams::best_bvh_layout(params->bvh_layout,
                                                      device->get_bvh_layout_mask());
      bparams.use_unaligned_nodes = dscene->data.bvh.have_curves &&
                                    params->use_bvh_unaligned_nodes;
      bparams.num_motion_triangle_steps = params->num_bvh_time_steps;
      bparams.num_motion_curve_steps = params->num_bvh_time_steps;
      bparams.bvh_type = params->bvh_type;
      bparams.curve_flags = dscene->data.curve.curveflags;
      bparams.curve_subdivisions = dscene->data.curve.subdivisions;

      delete bvh;
      bvh = BVH::create(bparams, objects);
      MEM_GUARDED_CALL(progress, bvh->build, *progress);
    }
  }

  need_update = false;
  need_update_rebuild = false;
}

void Mesh::tag_update(Scene *scene, bool rebuild)
{
  need_update = true;

  if (rebuild) {
    need_update_rebuild = true;
    scene->light_manager->need_update = true;
  }
  else {
    foreach (Shader *shader, used_shaders)
      if (shader->has_surface_emission)
        scene->light_manager->need_update = true;
  }

  scene->mesh_manager->need_update = true;
  scene->object_manager->need_update = true;
}

bool Mesh::has_motion_blur() const
{
  return (use_motion_blur && (attributes.find(ATTR_STD_MOTION_VERTEX_POSITION) ||
                              curve_attributes.find(ATTR_STD_MOTION_VERTEX_POSITION)));
}

bool Mesh::has_true_displacement() const
{
  foreach (Shader *shader, used_shaders) {
    if (shader->has_displacement && shader->displacement_method != DISPLACE_BUMP) {
      return true;
    }
  }

  return false;
}

float Mesh::motion_time(int step) const
{
  return (motion_steps > 1) ? 2.0f * step / (motion_steps - 1) - 1.0f : 0.0f;
}

int Mesh::motion_step(float time) const
{
  if (motion_steps > 1) {
    int attr_step = 0;

    for (int step = 0; step < motion_steps; step++) {
      float step_time = motion_time(step);
      if (step_time == time) {
        return attr_step;
      }

      /* Center step is stored in a separate attribute. */
      if (step != motion_steps / 2) {
        attr_step++;
      }
    }
  }

  return -1;
}

bool Mesh::need_build_bvh() const
{
  return !transform_applied || has_surface_bssrdf;
}

bool Mesh::is_instanced() const
{
  /* Currently we treat subsurface objects as instanced.
   *
   * While it might be not very optimal for ray traversal, it avoids having
   * duplicated BVH in the memory, saving quite some space.
   */
  return !transform_applied || has_surface_bssrdf;
}

/* Mesh Manager */

MeshManager::MeshManager()
{
  need_update = true;
  need_flags_update = true;
}

MeshManager::~MeshManager()
{
}

void MeshManager::update_osl_attributes(Device *device,
                                        Scene *scene,
                                        vector<AttributeRequestSet> &mesh_attributes)
{
#ifdef WITH_OSL
  /* for OSL, a hash map is used to lookup the attribute by name. */
  OSLGlobals *og = (OSLGlobals *)device->osl_memory();

  og->object_name_map.clear();
  og->attribute_map.clear();
  og->object_names.clear();

  og->attribute_map.resize(scene->objects.size() * ATTR_PRIM_TYPES);

  for (size_t i = 0; i < scene->objects.size(); i++) {
    /* set object name to object index map */
    Object *object = scene->objects[i];
    og->object_name_map[object->name] = i;
    og->object_names.push_back(object->name);

    /* set object attributes */
    foreach (ParamValue &attr, object->attributes) {
      OSLGlobals::Attribute osl_attr;

      osl_attr.type = attr.type();
      osl_attr.desc.element = ATTR_ELEMENT_OBJECT;
      osl_attr.value = attr;
      osl_attr.desc.offset = 0;
      osl_attr.desc.flags = 0;

      og->attribute_map[i * ATTR_PRIM_TYPES + ATTR_PRIM_TRIANGLE][attr.name()] = osl_attr;
      og->attribute_map[i * ATTR_PRIM_TYPES + ATTR_PRIM_CURVE][attr.name()] = osl_attr;
      og->attribute_map[i * ATTR_PRIM_TYPES + ATTR_PRIM_SUBD][attr.name()] = osl_attr;
    }

    /* find mesh attributes */
    size_t j;

    for (j = 0; j < scene->meshes.size(); j++)
      if (scene->meshes[j] == object->mesh)
        break;

    AttributeRequestSet &attributes = mesh_attributes[j];

    /* set object attributes */
    foreach (AttributeRequest &req, attributes.requests) {
      OSLGlobals::Attribute osl_attr;

      if (req.triangle_desc.element != ATTR_ELEMENT_NONE) {
        osl_attr.desc = req.triangle_desc;

        if (req.triangle_type == TypeDesc::TypeFloat)
          osl_attr.type = TypeDesc::TypeFloat;
        else if (req.triangle_type == TypeDesc::TypeMatrix)
          osl_attr.type = TypeDesc::TypeMatrix;
        else if (req.triangle_type == TypeFloat2)
          osl_attr.type = TypeFloat2;
        else
          osl_attr.type = TypeDesc::TypeColor;

        if (req.std != ATTR_STD_NONE) {
          /* if standard attribute, add lookup by geom: name convention */
          ustring stdname(string("geom:") + string(Attribute::standard_name(req.std)));
          og->attribute_map[i * ATTR_PRIM_TYPES + ATTR_PRIM_TRIANGLE][stdname] = osl_attr;
        }
        else if (req.name != ustring()) {
          /* add lookup by mesh attribute name */
          og->attribute_map[i * ATTR_PRIM_TYPES + ATTR_PRIM_TRIANGLE][req.name] = osl_attr;
        }
      }

      if (req.curve_desc.element != ATTR_ELEMENT_NONE) {
        osl_attr.desc = req.curve_desc;

        if (req.curve_type == TypeDesc::TypeFloat)
          osl_attr.type = TypeDesc::TypeFloat;
        else if (req.curve_type == TypeDesc::TypeMatrix)
          osl_attr.type = TypeDesc::TypeMatrix;
        else if (req.curve_type == TypeFloat2)
          osl_attr.type = TypeFloat2;
        else
          osl_attr.type = TypeDesc::TypeColor;

        if (req.std != ATTR_STD_NONE) {
          /* if standard attribute, add lookup by geom: name convention */
          ustring stdname(string("geom:") + string(Attribute::standard_name(req.std)));
          og->attribute_map[i * ATTR_PRIM_TYPES + ATTR_PRIM_CURVE][stdname] = osl_attr;
        }
        else if (req.name != ustring()) {
          /* add lookup by mesh attribute name */
          og->attribute_map[i * ATTR_PRIM_TYPES + ATTR_PRIM_CURVE][req.name] = osl_attr;
        }
      }

      if (req.subd_desc.element != ATTR_ELEMENT_NONE) {
        osl_attr.desc = req.subd_desc;

        if (req.subd_type == TypeDesc::TypeFloat)
          osl_attr.type = TypeDesc::TypeFloat;
        else if (req.subd_type == TypeDesc::TypeMatrix)
          osl_attr.type = TypeDesc::TypeMatrix;
        else if (req.subd_type == TypeFloat2)
          osl_attr.type = TypeFloat2;
        else
          osl_attr.type = TypeDesc::TypeColor;

        if (req.std != ATTR_STD_NONE) {
          /* if standard attribute, add lookup by geom: name convention */
          ustring stdname(string("geom:") + string(Attribute::standard_name(req.std)));
          og->attribute_map[i * ATTR_PRIM_TYPES + ATTR_PRIM_SUBD][stdname] = osl_attr;
        }
        else if (req.name != ustring()) {
          /* add lookup by mesh attribute name */
          og->attribute_map[i * ATTR_PRIM_TYPES + ATTR_PRIM_SUBD][req.name] = osl_attr;
        }
      }
    }
  }
#else
  (void)device;
  (void)scene;
  (void)mesh_attributes;
#endif
}

void MeshManager::update_svm_attributes(Device *,
                                        DeviceScene *dscene,
                                        Scene *scene,
                                        vector<AttributeRequestSet> &mesh_attributes)
{
  /* for SVM, the attributes_map table is used to lookup the offset of an
   * attribute, based on a unique shader attribute id. */

  /* compute array stride */
  int attr_map_size = 0;

  for (size_t i = 0; i < scene->meshes.size(); i++) {
    Mesh *mesh = scene->meshes[i];
    mesh->attr_map_offset = attr_map_size;
    attr_map_size += (mesh_attributes[i].size() + 1) * ATTR_PRIM_TYPES;
  }

  if (attr_map_size == 0)
    return;

  /* create attribute map */
  uint4 *attr_map = dscene->attributes_map.alloc(attr_map_size);
  memset(attr_map, 0, dscene->attributes_map.size() * sizeof(uint));

  for (size_t i = 0; i < scene->meshes.size(); i++) {
    Mesh *mesh = scene->meshes[i];
    AttributeRequestSet &attributes = mesh_attributes[i];

    /* set object attributes */
    int index = mesh->attr_map_offset;

    foreach (AttributeRequest &req, attributes.requests) {
      uint id;

      if (req.std == ATTR_STD_NONE)
        id = scene->shader_manager->get_attribute_id(req.name);
      else
        id = scene->shader_manager->get_attribute_id(req.std);

      if (mesh->num_triangles()) {
        attr_map[index].x = id;
        attr_map[index].y = req.triangle_desc.element;
        attr_map[index].z = as_uint(req.triangle_desc.offset);

        if (req.triangle_type == TypeDesc::TypeFloat)
          attr_map[index].w = NODE_ATTR_FLOAT;
        else if (req.triangle_type == TypeDesc::TypeMatrix)
          attr_map[index].w = NODE_ATTR_MATRIX;
        else if (req.triangle_type == TypeFloat2)
          attr_map[index].w = NODE_ATTR_FLOAT2;
        else
          attr_map[index].w = NODE_ATTR_FLOAT3;

        attr_map[index].w |= req.triangle_desc.flags << 8;
      }

      index++;

      if (mesh->num_curves()) {
        attr_map[index].x = id;
        attr_map[index].y = req.curve_desc.element;
        attr_map[index].z = as_uint(req.curve_desc.offset);

        if (req.curve_type == TypeDesc::TypeFloat)
          attr_map[index].w = NODE_ATTR_FLOAT;
        else if (req.curve_type == TypeDesc::TypeMatrix)
          attr_map[index].w = NODE_ATTR_MATRIX;
        else if (req.curve_type == TypeFloat2)
          attr_map[index].w = NODE_ATTR_FLOAT2;
        else
          attr_map[index].w = NODE_ATTR_FLOAT3;

        attr_map[index].w |= req.curve_desc.flags << 8;
      }

      index++;

      if (mesh->subd_faces.size()) {
        attr_map[index].x = id;
        attr_map[index].y = req.subd_desc.element;
        attr_map[index].z = as_uint(req.subd_desc.offset);

        if (req.subd_type == TypeDesc::TypeFloat)
          attr_map[index].w = NODE_ATTR_FLOAT;
        else if (req.subd_type == TypeDesc::TypeMatrix)
          attr_map[index].w = NODE_ATTR_MATRIX;
        else if (req.subd_type == TypeFloat2)
          attr_map[index].w = NODE_ATTR_FLOAT2;
        else
          attr_map[index].w = NODE_ATTR_FLOAT3;

        attr_map[index].w |= req.subd_desc.flags << 8;
      }

      index++;
    }

    /* terminator */
    for (int j = 0; j < ATTR_PRIM_TYPES; j++) {
      attr_map[index].x = ATTR_STD_NONE;
      attr_map[index].y = 0;
      attr_map[index].z = 0;
      attr_map[index].w = 0;

      index++;
    }
  }

  /* copy to device */
  dscene->attributes_map.copy_to_device();
}

static void update_attribute_element_size(Mesh *mesh,
                                          Attribute *mattr,
                                          AttributePrimitive prim,
                                          size_t *attr_float_size,
                                          size_t *attr_float2_size,
                                          size_t *attr_float3_size,
                                          size_t *attr_uchar4_size)
{
  if (mattr) {
    size_t size = mattr->element_size(mesh, prim);

    if (mattr->element == ATTR_ELEMENT_VOXEL) {
      /* pass */
    }
    else if (mattr->element == ATTR_ELEMENT_CORNER_BYTE) {
      *attr_uchar4_size += size;
    }
    else if (mattr->type == TypeDesc::TypeFloat) {
      *attr_float_size += size;
    }
    else if (mattr->type == TypeFloat2) {
      *attr_float2_size += size;
    }
    else if (mattr->type == TypeDesc::TypeMatrix) {
      *attr_float3_size += size * 4;
    }
    else {
      *attr_float3_size += size;
    }
  }
}

static void update_attribute_element_offset(Mesh *mesh,
                                            device_vector<float> &attr_float,
                                            size_t &attr_float_offset,
                                            device_vector<float2> &attr_float2,
                                            size_t &attr_float2_offset,
                                            device_vector<float4> &attr_float3,
                                            size_t &attr_float3_offset,
                                            device_vector<uchar4> &attr_uchar4,
                                            size_t &attr_uchar4_offset,
                                            Attribute *mattr,
                                            AttributePrimitive prim,
                                            TypeDesc &type,
                                            AttributeDescriptor &desc)
{
  if (mattr) {
    /* store element and type */
    desc.element = mattr->element;
    desc.flags = mattr->flags;
    type = mattr->type;

    /* store attribute data in arrays */
    size_t size = mattr->element_size(mesh, prim);

    AttributeElement &element = desc.element;
    int &offset = desc.offset;

    if (mattr->element == ATTR_ELEMENT_VOXEL) {
      /* store slot in offset value */
      VoxelAttribute *voxel_data = mattr->data_voxel();
      offset = voxel_data->slot;
    }
    else if (mattr->element == ATTR_ELEMENT_CORNER_BYTE) {
      uchar4 *data = mattr->data_uchar4();
      offset = attr_uchar4_offset;

      assert(attr_uchar4.size() >= offset + size);
      for (size_t k = 0; k < size; k++) {
        attr_uchar4[offset + k] = data[k];
      }
      attr_uchar4_offset += size;
    }
    else if (mattr->type == TypeDesc::TypeFloat) {
      float *data = mattr->data_float();
      offset = attr_float_offset;

      assert(attr_float.size() >= offset + size);
      for (size_t k = 0; k < size; k++) {
        attr_float[offset + k] = data[k];
      }
      attr_float_offset += size;
    }
    else if (mattr->type == TypeFloat2) {
      float2 *data = mattr->data_float2();
      offset = attr_float2_offset;

      assert(attr_float2.size() >= offset + size);
      for (size_t k = 0; k < size; k++) {
        attr_float2[offset + k] = data[k];
      }
      attr_float2_offset += size;
    }
    else if (mattr->type == TypeDesc::TypeMatrix) {
      Transform *tfm = mattr->data_transform();
      offset = attr_float3_offset;

      assert(attr_float3.size() >= offset + size * 3);
      for (size_t k = 0; k < size * 3; k++) {
        attr_float3[offset + k] = (&tfm->x)[k];
      }
      attr_float3_offset += size * 3;
    }
    else {
      float4 *data = mattr->data_float4();
      offset = attr_float3_offset;

      assert(attr_float3.size() >= offset + size);
      for (size_t k = 0; k < size; k++) {
        attr_float3[offset + k] = data[k];
      }
      attr_float3_offset += size;
    }

    /* mesh vertex/curve index is global, not per object, so we sneak
     * a correction for that in here */
    if (mesh->subdivision_type == Mesh::SUBDIVISION_CATMULL_CLARK &&
        desc.flags & ATTR_SUBDIVIDED) {
      /* indices for subdivided attributes are retrieved
       * from patch table so no need for correction here*/
    }
    else if (element == ATTR_ELEMENT_VERTEX)
      offset -= mesh->vert_offset;
    else if (element == ATTR_ELEMENT_VERTEX_MOTION)
      offset -= mesh->vert_offset;
    else if (element == ATTR_ELEMENT_FACE) {
      if (prim == ATTR_PRIM_TRIANGLE)
        offset -= mesh->tri_offset;
      else
        offset -= mesh->face_offset;
    }
    else if (element == ATTR_ELEMENT_CORNER || element == ATTR_ELEMENT_CORNER_BYTE) {
      if (prim == ATTR_PRIM_TRIANGLE)
        offset -= 3 * mesh->tri_offset;
      else
        offset -= mesh->corner_offset;
    }
    else if (element == ATTR_ELEMENT_CURVE)
      offset -= mesh->curve_offset;
    else if (element == ATTR_ELEMENT_CURVE_KEY)
      offset -= mesh->curvekey_offset;
    else if (element == ATTR_ELEMENT_CURVE_KEY_MOTION)
      offset -= mesh->curvekey_offset;
  }
  else {
    /* attribute not found */
    desc.element = ATTR_ELEMENT_NONE;
    desc.offset = 0;
  }
}

void MeshManager::device_update_attributes(Device *device,
                                           DeviceScene *dscene,
                                           Scene *scene,
                                           Progress &progress)
{
  progress.set_status("Updating Mesh", "Computing attributes");

  /* gather per mesh requested attributes. as meshes may have multiple
   * shaders assigned, this merges the requested attributes that have
   * been set per shader by the shader manager */
  vector<AttributeRequestSet> mesh_attributes(scene->meshes.size());

  for (size_t i = 0; i < scene->meshes.size(); i++) {
    Mesh *mesh = scene->meshes[i];

    scene->need_global_attributes(mesh_attributes[i]);

    foreach (Shader *shader, mesh->used_shaders) {
      mesh_attributes[i].add(shader->attributes);
    }
  }

  /* mesh attribute are stored in a single array per data type. here we fill
   * those arrays, and set the offset and element type to create attribute
   * maps next */

  /* Pre-allocate attributes to avoid arrays re-allocation which would
   * take 2x of overall attribute memory usage.
   */
  size_t attr_float_size = 0;
  size_t attr_float2_size = 0;
  size_t attr_float3_size = 0;
  size_t attr_uchar4_size = 0;
  for (size_t i = 0; i < scene->meshes.size(); i++) {
    Mesh *mesh = scene->meshes[i];
    AttributeRequestSet &attributes = mesh_attributes[i];
    foreach (AttributeRequest &req, attributes.requests) {
      Attribute *triangle_mattr = mesh->attributes.find(req);
      Attribute *curve_mattr = mesh->curve_attributes.find(req);
      Attribute *subd_mattr = mesh->subd_attributes.find(req);

      update_attribute_element_size(mesh,
                                    triangle_mattr,
                                    ATTR_PRIM_TRIANGLE,
                                    &attr_float_size,
                                    &attr_float2_size,
                                    &attr_float3_size,
                                    &attr_uchar4_size);
      update_attribute_element_size(mesh,
                                    curve_mattr,
                                    ATTR_PRIM_CURVE,
                                    &attr_float_size,
                                    &attr_float2_size,
                                    &attr_float3_size,
                                    &attr_uchar4_size);
      update_attribute_element_size(mesh,
                                    subd_mattr,
                                    ATTR_PRIM_SUBD,
                                    &attr_float_size,
                                    &attr_float2_size,
                                    &attr_float3_size,
                                    &attr_uchar4_size);
    }
  }

  dscene->attributes_float.alloc(attr_float_size);
  dscene->attributes_float2.alloc(attr_float2_size);
  dscene->attributes_float3.alloc(attr_float3_size);
  dscene->attributes_uchar4.alloc(attr_uchar4_size);

  size_t attr_float_offset = 0;
  size_t attr_float2_offset = 0;
  size_t attr_float3_offset = 0;
  size_t attr_uchar4_offset = 0;

  /* Fill in attributes. */
  for (size_t i = 0; i < scene->meshes.size(); i++) {
    Mesh *mesh = scene->meshes[i];
    AttributeRequestSet &attributes = mesh_attributes[i];

    /* todo: we now store std and name attributes from requests even if
     * they actually refer to the same mesh attributes, optimize */
    foreach (AttributeRequest &req, attributes.requests) {
      Attribute *triangle_mattr = mesh->attributes.find(req);
      Attribute *curve_mattr = mesh->curve_attributes.find(req);
      Attribute *subd_mattr = mesh->subd_attributes.find(req);

      update_attribute_element_offset(mesh,
                                      dscene->attributes_float,
                                      attr_float_offset,
                                      dscene->attributes_float2,
                                      attr_float2_offset,
                                      dscene->attributes_float3,
                                      attr_float3_offset,
                                      dscene->attributes_uchar4,
                                      attr_uchar4_offset,
                                      triangle_mattr,
                                      ATTR_PRIM_TRIANGLE,
                                      req.triangle_type,
                                      req.triangle_desc);

      update_attribute_element_offset(mesh,
                                      dscene->attributes_float,
                                      attr_float_offset,
                                      dscene->attributes_float2,
                                      attr_float2_offset,
                                      dscene->attributes_float3,
                                      attr_float3_offset,
                                      dscene->attributes_uchar4,
                                      attr_uchar4_offset,
                                      curve_mattr,
                                      ATTR_PRIM_CURVE,
                                      req.curve_type,
                                      req.curve_desc);

      update_attribute_element_offset(mesh,
                                      dscene->attributes_float,
                                      attr_float_offset,
                                      dscene->attributes_float2,
                                      attr_float2_offset,
                                      dscene->attributes_float3,
                                      attr_float3_offset,
                                      dscene->attributes_uchar4,
                                      attr_uchar4_offset,
                                      subd_mattr,
                                      ATTR_PRIM_SUBD,
                                      req.subd_type,
                                      req.subd_desc);

      if (progress.get_cancel())
        return;
    }
  }

  /* create attribute lookup maps */
  if (scene->shader_manager->use_osl())
    update_osl_attributes(device, scene, mesh_attributes);

  update_svm_attributes(device, dscene, scene, mesh_attributes);

  if (progress.get_cancel())
    return;

  /* copy to device */
  progress.set_status("Updating Mesh", "Copying Attributes to device");

  if (dscene->attributes_float.size()) {
    dscene->attributes_float.copy_to_device();
  }
  if (dscene->attributes_float2.size()) {
    dscene->attributes_float2.copy_to_device();
  }
  if (dscene->attributes_float3.size()) {
    dscene->attributes_float3.copy_to_device();
  }
  if (dscene->attributes_uchar4.size()) {
    dscene->attributes_uchar4.copy_to_device();
  }

  if (progress.get_cancel())
    return;

  /* After mesh attributes and patch tables have been copied to device memory,
   * we need to update offsets in the objects. */
  scene->object_manager->device_update_mesh_offsets(device, dscene, scene);
}

void MeshManager::mesh_calc_offset(Scene *scene)
{
  size_t vert_size = 0;
  size_t tri_size = 0;

  size_t curve_key_size = 0;
  size_t curve_size = 0;

  size_t patch_size = 0;
  size_t face_size = 0;
  size_t corner_size = 0;

  foreach (Mesh *mesh, scene->meshes) {
    mesh->vert_offset = vert_size;
    mesh->tri_offset = tri_size;

    mesh->curvekey_offset = curve_key_size;
    mesh->curve_offset = curve_size;

    mesh->patch_offset = patch_size;
    mesh->face_offset = face_size;
    mesh->corner_offset = corner_size;

    vert_size += mesh->verts.size();
    tri_size += mesh->num_triangles();

    curve_key_size += mesh->curve_keys.size();
    curve_size += mesh->num_curves();

    if (mesh->subd_faces.size()) {
      Mesh::SubdFace &last = mesh->subd_faces[mesh->subd_faces.size() - 1];
      patch_size += (last.ptex_offset + last.num_ptex_faces()) * 8;

      /* patch tables are stored in same array so include them in patch_size */
      if (mesh->patch_table) {
        mesh->patch_table_offset = patch_size;
        patch_size += mesh->patch_table->total_size();
      }
    }
    face_size += mesh->subd_faces.size();
    corner_size += mesh->subd_face_corners.size();
  }
}

void MeshManager::device_update_mesh(
    Device *, DeviceScene *dscene, Scene *scene, bool for_displacement, Progress &progress)
{
  /* Count. */
  size_t vert_size = 0;
  size_t tri_size = 0;

  size_t curve_key_size = 0;
  size_t curve_size = 0;

  size_t patch_size = 0;

  foreach (Mesh *mesh, scene->meshes) {
    vert_size += mesh->verts.size();
    tri_size += mesh->num_triangles();

    curve_key_size += mesh->curve_keys.size();
    curve_size += mesh->num_curves();

    if (mesh->subd_faces.size()) {
      Mesh::SubdFace &last = mesh->subd_faces[mesh->subd_faces.size() - 1];
      patch_size += (last.ptex_offset + last.num_ptex_faces()) * 8;

      /* patch tables are stored in same array so include them in patch_size */
      if (mesh->patch_table) {
        mesh->patch_table_offset = patch_size;
        patch_size += mesh->patch_table->total_size();
      }
    }
  }

  /* Create mapping from triangle to primitive triangle array. */
  vector<uint> tri_prim_index(tri_size);
  if (for_displacement) {
    /* For displacement kernels we do some trickery to make them believe
     * we've got all required data ready. However, that data is different
     * from final render kernels since we don't have BVH yet, so can't
     * really use same semantic of arrays.
     */
    foreach (Mesh *mesh, scene->meshes) {
      for (size_t i = 0; i < mesh->num_triangles(); ++i) {
        tri_prim_index[i + mesh->tri_offset] = 3 * (i + mesh->tri_offset);
      }
    }
  }
  else {
    for (size_t i = 0; i < dscene->prim_index.size(); ++i) {
      if ((dscene->prim_type[i] & PRIMITIVE_ALL_TRIANGLE) != 0) {
        tri_prim_index[dscene->prim_index[i]] = dscene->prim_tri_index[i];
      }
    }
  }

  /* Fill in all the arrays. */
  if (tri_size != 0) {
    /* normals */
    progress.set_status("Updating Mesh", "Computing normals");

    uint *tri_shader = dscene->tri_shader.alloc(tri_size);
    float4 *vnormal = dscene->tri_vnormal.alloc(vert_size);
    uint4 *tri_vindex = dscene->tri_vindex.alloc(tri_size);
    uint *tri_patch = dscene->tri_patch.alloc(tri_size);
    float2 *tri_patch_uv = dscene->tri_patch_uv.alloc(vert_size);

    foreach (Mesh *mesh, scene->meshes) {
      mesh->pack_shaders(scene, &tri_shader[mesh->tri_offset]);
      mesh->pack_normals(&vnormal[mesh->vert_offset]);
      mesh->pack_verts(tri_prim_index,
                       &tri_vindex[mesh->tri_offset],
                       &tri_patch[mesh->tri_offset],
                       &tri_patch_uv[mesh->vert_offset],
                       mesh->vert_offset,
                       mesh->tri_offset);
      if (progress.get_cancel())
        return;
    }

    /* vertex coordinates */
    progress.set_status("Updating Mesh", "Copying Mesh to device");

    dscene->tri_shader.copy_to_device();
    dscene->tri_vnormal.copy_to_device();
    dscene->tri_vindex.copy_to_device();
    dscene->tri_patch.copy_to_device();
    dscene->tri_patch_uv.copy_to_device();
  }

  if (curve_size != 0) {
    progress.set_status("Updating Mesh", "Copying Strands to device");

    float4 *curve_keys = dscene->curve_keys.alloc(curve_key_size);
    float4 *curves = dscene->curves.alloc(curve_size);

    foreach (Mesh *mesh, scene->meshes) {
      mesh->pack_curves(scene,
                        &curve_keys[mesh->curvekey_offset],
                        &curves[mesh->curve_offset],
                        mesh->curvekey_offset);
      if (progress.get_cancel())
        return;
    }

    dscene->curve_keys.copy_to_device();
    dscene->curves.copy_to_device();
  }

  if (patch_size != 0) {
    progress.set_status("Updating Mesh", "Copying Patches to device");

    uint *patch_data = dscene->patches.alloc(patch_size);

    foreach (Mesh *mesh, scene->meshes) {
      mesh->pack_patches(&patch_data[mesh->patch_offset],
                         mesh->vert_offset,
                         mesh->face_offset,
                         mesh->corner_offset);

      if (mesh->patch_table) {
        mesh->patch_table->copy_adjusting_offsets(&patch_data[mesh->patch_table_offset],
                                                  mesh->patch_table_offset);
      }

      if (progress.get_cancel())
        return;
    }

    dscene->patches.copy_to_device();
  }

  if (for_displacement) {
    float4 *prim_tri_verts = dscene->prim_tri_verts.alloc(tri_size * 3);
    foreach (Mesh *mesh, scene->meshes) {
      for (size_t i = 0; i < mesh->num_triangles(); ++i) {
        Mesh::Triangle t = mesh->get_triangle(i);
        size_t offset = 3 * (i + mesh->tri_offset);
        prim_tri_verts[offset + 0] = float3_to_float4(mesh->verts[t.v[0]]);
        prim_tri_verts[offset + 1] = float3_to_float4(mesh->verts[t.v[1]]);
        prim_tri_verts[offset + 2] = float3_to_float4(mesh->verts[t.v[2]]);
      }
    }
    dscene->prim_tri_verts.copy_to_device();
  }
}

void MeshManager::device_update_bvh(Device *device,
                                    DeviceScene *dscene,
                                    Scene *scene,
                                    Progress &progress)
{
  /* bvh build */
  progress.set_status("Updating Scene BVH", "Building");

  BVHParams bparams;
  bparams.top_level = true;
  bparams.bvh_layout = BVHParams::best_bvh_layout(scene->params.bvh_layout,
                                                  device->get_bvh_layout_mask());
  bparams.use_spatial_split = scene->params.use_bvh_spatial_split;
  bparams.use_unaligned_nodes = dscene->data.bvh.have_curves &&
                                scene->params.use_bvh_unaligned_nodes;
  bparams.num_motion_triangle_steps = scene->params.num_bvh_time_steps;
  bparams.num_motion_curve_steps = scene->params.num_bvh_time_steps;
  bparams.bvh_type = scene->params.bvh_type;
  bparams.curve_flags = dscene->data.curve.curveflags;
  bparams.curve_subdivisions = dscene->data.curve.subdivisions;

  VLOG(1) << "Using " << bvh_layout_name(bparams.bvh_layout) << " layout.";

#ifdef WITH_EMBREE
  if (bparams.bvh_layout == BVH_LAYOUT_EMBREE) {
    if (dscene->data.bvh.scene) {
      BVHEmbree::destroy(dscene->data.bvh.scene);
    }
  }
#endif

  BVH *bvh = BVH::create(bparams, scene->objects);
  bvh->build(progress, &device->stats);

  if (progress.get_cancel()) {
#ifdef WITH_EMBREE
    if (bparams.bvh_layout == BVH_LAYOUT_EMBREE) {
      if (dscene->data.bvh.scene) {
        BVHEmbree::destroy(dscene->data.bvh.scene);
      }
    }
#endif
    delete bvh;
    return;
  }

  /* copy to device */
  progress.set_status("Updating Scene BVH", "Copying BVH to device");

  PackedBVH &pack = bvh->pack;

  if (pack.nodes.size()) {
    dscene->bvh_nodes.steal_data(pack.nodes);
    dscene->bvh_nodes.copy_to_device();
  }
  if (pack.leaf_nodes.size()) {
    dscene->bvh_leaf_nodes.steal_data(pack.leaf_nodes);
    dscene->bvh_leaf_nodes.copy_to_device();
  }
  if (pack.object_node.size()) {
    dscene->object_node.steal_data(pack.object_node);
    dscene->object_node.copy_to_device();
  }
  if (pack.prim_tri_index.size()) {
    dscene->prim_tri_index.steal_data(pack.prim_tri_index);
    dscene->prim_tri_index.copy_to_device();
  }
  if (pack.prim_tri_verts.size()) {
    dscene->prim_tri_verts.steal_data(pack.prim_tri_verts);
    dscene->prim_tri_verts.copy_to_device();
  }
  if (pack.prim_type.size()) {
    dscene->prim_type.steal_data(pack.prim_type);
    dscene->prim_type.copy_to_device();
  }
  if (pack.prim_visibility.size()) {
    dscene->prim_visibility.steal_data(pack.prim_visibility);
    dscene->prim_visibility.copy_to_device();
  }
  if (pack.prim_index.size()) {
    dscene->prim_index.steal_data(pack.prim_index);
    dscene->prim_index.copy_to_device();
  }
  if (pack.prim_object.size()) {
    dscene->prim_object.steal_data(pack.prim_object);
    dscene->prim_object.copy_to_device();
  }
  if (pack.prim_time.size()) {
    dscene->prim_time.steal_data(pack.prim_time);
    dscene->prim_time.copy_to_device();
  }

  dscene->data.bvh.root = pack.root_index;
  dscene->data.bvh.bvh_layout = bparams.bvh_layout;
  dscene->data.bvh.use_bvh_steps = (scene->params.num_bvh_time_steps != 0);

#ifdef WITH_EMBREE
  if (bparams.bvh_layout == BVH_LAYOUT_EMBREE) {
    dscene->data.bvh.scene = ((BVHEmbree *)bvh)->scene;
  }
  else {
    dscene->data.bvh.scene = NULL;
  }
#endif

  delete bvh;
}

void MeshManager::device_update_preprocess(Device *device, Scene *scene, Progress &progress)
{
  if (!need_update && !need_flags_update) {
    return;
  }

  progress.set_status("Updating Meshes Flags");

  /* Update flags. */
  bool volume_images_updated = false;

  foreach (Mesh *mesh, scene->meshes) {
    mesh->has_volume = false;

    foreach (const Shader *shader, mesh->used_shaders) {
      if (shader->has_volume) {
        mesh->has_volume = true;
      }
      if (shader->has_surface_bssrdf) {
        mesh->has_surface_bssrdf = true;
      }
    }

    if (need_update && mesh->has_volume) {
      /* Create volume meshes if there is voxel data. */
      bool has_voxel_attributes = false;

      foreach (Attribute &attr, mesh->attributes.attributes) {
        if (attr.element == ATTR_ELEMENT_VOXEL) {
          has_voxel_attributes = true;
        }
      }

      if (has_voxel_attributes) {
        if (!volume_images_updated) {
          progress.set_status("Updating Meshes Volume Bounds");
          device_update_volume_images(device, scene, progress);
          volume_images_updated = true;
        }

        create_volume_mesh(scene, mesh, progress);
      }
    }
  }

  need_flags_update = false;
}

void MeshManager::device_update_displacement_images(Device *device,
                                                    Scene *scene,
                                                    Progress &progress)
{
  progress.set_status("Updating Displacement Images");
  TaskPool pool;
  ImageManager *image_manager = scene->image_manager;
  set<int> bump_images;
  foreach (Mesh *mesh, scene->meshes) {
    if (mesh->need_update) {
      foreach (Shader *shader, mesh->used_shaders) {
        if (!shader->has_displacement || shader->displacement_method == DISPLACE_BUMP) {
          continue;
        }
        foreach (ShaderNode *node, shader->graph->nodes) {
          if (node->special_type != SHADER_SPECIAL_TYPE_IMAGE_SLOT) {
            continue;
          }

          ImageSlotTextureNode *image_node = static_cast<ImageSlotTextureNode *>(node);
          int slot = image_node->slot;
          if (slot != -1) {
            bump_images.insert(slot);
          }
        }
      }
    }
  }
  foreach (int slot, bump_images) {
    pool.push(function_bind(
        &ImageManager::device_update_slot, image_manager, device, scene, slot, &progress));
  }
  pool.wait_work();
}

void MeshManager::device_update_volume_images(Device *device, Scene *scene, Progress &progress)
{
  progress.set_status("Updating Volume Images");
  TaskPool pool;
  ImageManager *image_manager = scene->image_manager;
  set<int> volume_images;

  foreach (Mesh *mesh, scene->meshes) {
    if (!mesh->need_update) {
      continue;
    }

    foreach (Attribute &attr, mesh->attributes.attributes) {
      if (attr.element != ATTR_ELEMENT_VOXEL) {
        continue;
      }

      VoxelAttribute *voxel = attr.data_voxel();

      if (voxel->slot != -1) {
        volume_images.insert(voxel->slot);
      }
    }
  }

  foreach (int slot, volume_images) {
    pool.push(function_bind(
        &ImageManager::device_update_slot, image_manager, device, scene, slot, &progress));
  }
  pool.wait_work();
}

void MeshManager::device_update(Device *device,
                                DeviceScene *dscene,
                                Scene *scene,
                                Progress &progress)
{
  if (!need_update)
    return;

  VLOG(1) << "Total " << scene->meshes.size() << " meshes.";

  bool true_displacement_used = false;
  size_t total_tess_needed = 0;

  foreach (Mesh *mesh, scene->meshes) {
    foreach (Shader *shader, mesh->used_shaders) {
      if (shader->need_update_mesh)
        mesh->need_update = true;
    }

    if (mesh->need_update) {
      /* Update normals. */
      mesh->add_face_normals();
      mesh->add_vertex_normals();

      if (mesh->need_attribute(scene, ATTR_STD_POSITION_UNDISPLACED)) {
        mesh->add_undisplaced();
      }

      /* Test if we need tessellation. */
      if (mesh->subdivision_type != Mesh::SUBDIVISION_NONE && mesh->num_subd_verts == 0 &&
          mesh->subd_params) {
        total_tess_needed++;
      }

      /* Test if we need displacement. */
      if (mesh->has_true_displacement()) {
        true_displacement_used = true;
      }

      if (progress.get_cancel())
        return;
    }
  }

  /* Tessellate meshes that are using subdivision */
  if (total_tess_needed) {
    Camera *dicing_camera = scene->dicing_camera;
    dicing_camera->update(scene);

    size_t i = 0;
    foreach (Mesh *mesh, scene->meshes) {
      if (mesh->need_update && mesh->subdivision_type != Mesh::SUBDIVISION_NONE &&
          mesh->num_subd_verts == 0 && mesh->subd_params) {
        string msg = "Tessellating ";
        if (mesh->name == "")
          msg += string_printf("%u/%u", (uint)(i + 1), (uint)total_tess_needed);
        else
          msg += string_printf(
              "%s %u/%u", mesh->name.c_str(), (uint)(i + 1), (uint)total_tess_needed);

        progress.set_status("Updating Mesh", msg);

        mesh->subd_params->camera = dicing_camera;
        DiagSplit dsplit(*mesh->subd_params);
        mesh->tessellate(&dsplit);

        i++;

        if (progress.get_cancel())
          return;
      }
    }
  }

  /* Update images needed for true displacement. */
  bool old_need_object_flags_update = false;
  if (true_displacement_used) {
    VLOG(1) << "Updating images used for true displacement.";
    device_update_displacement_images(device, scene, progress);
    old_need_object_flags_update = scene->object_manager->need_flags_update;
    scene->object_manager->device_update_flags(device, dscene, scene, progress, false);
  }

  /* Device update. */
  device_free(device, dscene);

  mesh_calc_offset(scene);
  if (true_displacement_used) {
    device_update_mesh(device, dscene, scene, true, progress);
  }
  if (progress.get_cancel())
    return;

  device_update_attributes(device, dscene, scene, progress);
  if (progress.get_cancel())
    return;

  /* Update displacement. */
  bool displacement_done = false;
  size_t num_bvh = 0;

  foreach (Mesh *mesh, scene->meshes) {
    if (mesh->need_update) {
      if (displace(device, dscene, scene, mesh, progress)) {
        displacement_done = true;
      }

      if (mesh->need_build_bvh()) {
        num_bvh++;
      }
    }

    if (progress.get_cancel())
      return;
  }

  /* Device re-update after displacement. */
  if (displacement_done) {
    device_free(device, dscene);

    device_update_attributes(device, dscene, scene, progress);
    if (progress.get_cancel())
      return;
  }

  TaskPool pool;

  size_t i = 0;
  foreach (Mesh *mesh, scene->meshes) {
    if (mesh->need_update) {
      pool.push(function_bind(
          &Mesh::compute_bvh, mesh, device, dscene, &scene->params, &progress, i, num_bvh));
      if (mesh->need_build_bvh()) {
        i++;
      }
    }
  }

  TaskPool::Summary summary;
  pool.wait_work(&summary);
  VLOG(2) << "Objects BVH build pool statistics:\n" << summary.full_report();

  foreach (Shader *shader, scene->shaders) {
    shader->need_update_mesh = false;
  }

  Scene::MotionType need_motion = scene->need_motion();
  bool motion_blur = need_motion == Scene::MOTION_BLUR;

  /* Update objects. */
  vector<Object *> volume_objects;
  foreach (Object *object, scene->objects) {
    object->compute_bounds(motion_blur);
  }

  if (progress.get_cancel())
    return;

  device_update_bvh(device, dscene, scene, progress);
  if (progress.get_cancel())
    return;

  device_update_mesh(device, dscene, scene, false, progress);
  if (progress.get_cancel())
    return;

  need_update = false;

  if (true_displacement_used) {
    /* Re-tag flags for update, so they're re-evaluated
     * for meshes with correct bounding boxes.
     *
     * This wouldn't cause wrong results, just true
     * displacement might be less optimal ot calculate.
     */
    scene->object_manager->need_flags_update = old_need_object_flags_update;
  }
}

void MeshManager::device_free(Device *device, DeviceScene *dscene)
{
  dscene->bvh_nodes.free();
  dscene->bvh_leaf_nodes.free();
  dscene->object_node.free();
  dscene->prim_tri_verts.free();
  dscene->prim_tri_index.free();
  dscene->prim_type.free();
  dscene->prim_visibility.free();
  dscene->prim_index.free();
  dscene->prim_object.free();
  dscene->prim_time.free();
  dscene->tri_shader.free();
  dscene->tri_vnormal.free();
  dscene->tri_vindex.free();
  dscene->tri_patch.free();
  dscene->tri_patch_uv.free();
  dscene->curves.free();
  dscene->curve_keys.free();
  dscene->patches.free();
  dscene->attributes_map.free();
  dscene->attributes_float.free();
  dscene->attributes_float2.free();
  dscene->attributes_float3.free();
  dscene->attributes_uchar4.free();

  /* Signal for shaders like displacement not to do ray tracing. */
  dscene->data.bvh.bvh_layout = BVH_LAYOUT_NONE;

#ifdef WITH_OSL
  OSLGlobals *og = (OSLGlobals *)device->osl_memory();

  if (og) {
    og->object_name_map.clear();
    og->attribute_map.clear();
    og->object_names.clear();
  }
#else
  (void)device;
#endif
}

void MeshManager::tag_update(Scene *scene)
{
  need_update = true;
  scene->object_manager->need_update = true;
}

void MeshManager::collect_statistics(const Scene *scene, RenderStats *stats)
{
  foreach (Mesh *mesh, scene->meshes) {
    stats->mesh.geometry.add_entry(
        NamedSizeEntry(string(mesh->name.c_str()), mesh->get_total_size_in_bytes()));
  }
}

bool Mesh::need_attribute(Scene *scene, AttributeStandard std)
{
  if (std == ATTR_STD_NONE)
    return false;

  if (scene->need_global_attribute(std))
    return true;

  foreach (Shader *shader, used_shaders)
    if (shader->attributes.find(std))
      return true;

  return false;
}

bool Mesh::need_attribute(Scene * /*scene*/, ustring name)
{
  if (name == ustring())
    return false;

  foreach (Shader *shader, used_shaders)
    if (shader->attributes.find(name))
      return true;

  return false;
}

CCL_NAMESPACE_END
