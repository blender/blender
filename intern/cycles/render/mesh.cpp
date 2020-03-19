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

#include "device/device.h"

#include "render/graph.h"
#include "render/hair.h"
#include "render/mesh.h"
#include "render/object.h"
#include "render/scene.h"

#include "subd/subd_patch_table.h"
#include "subd/subd_split.h"

#include "util/util_foreach.h"
#include "util/util_logging.h"
#include "util/util_progress.h"
#include "util/util_set.h"

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
  NodeType *type = NodeType::add("mesh", create, NodeType::NONE, Geometry::node_base_type);

  SOCKET_INT_ARRAY(triangles, "Triangles", array<int>());
  SOCKET_POINT_ARRAY(verts, "Vertices", array<float3>());
  SOCKET_INT_ARRAY(shader, "Shader", array<int>());
  SOCKET_BOOLEAN_ARRAY(smooth, "Smooth", array<bool>());

  return type;
}

Mesh::Mesh() : Geometry(node_type, Geometry::MESH), subd_attributes(this, ATTR_PRIM_SUBD)
{
  vert_offset = 0;

  patch_offset = 0;
  face_offset = 0;
  corner_offset = 0;

  num_subd_verts = 0;

  volume_clipping = 0.001f;
  volume_step_size = 0.0f;
  volume_object_space = false;

  num_ngons = 0;

  subdivision_type = SUBDIVISION_NONE;
  subd_params = NULL;

  patch_table = NULL;
}

Mesh::~Mesh()
{
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
  Geometry::clear();

  /* clear all verts and triangles */
  verts.clear();
  triangles.clear();
  shader.clear();
  smooth.clear();

  triangle_patch.clear();
  vert_patch_uv.clear();

  subd_faces.clear();
  subd_face_corners.clear();

  num_subd_verts = 0;

  subd_creases.clear();

  subd_attributes.clear();
  attributes.clear(preserve_voxel_data);

  vert_to_stitching_key_map.clear();
  vert_stitching_map.clear();

  delete patch_table;
  patch_table = NULL;
}

void Mesh::clear()
{
  clear(false);
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

void Mesh::copy_center_to_motion_step(const int motion_step)
{
  Attribute *attr_mP = attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);

  if (attr_mP) {
    Attribute *attr_mN = attributes.find(ATTR_STD_MOTION_VERTEX_NORMAL);
    Attribute *attr_N = attributes.find(ATTR_STD_VERTEX_NORMAL);
    float3 *P = &verts[0];
    float3 *N = (attr_N) ? attr_N->data_float3() : NULL;
    size_t numverts = verts.size();

    memcpy(attr_mP->data_float3() + motion_step * numverts, P, sizeof(float3) * numverts);
    if (attr_mN)
      memcpy(attr_mN->data_float3() + motion_step * numverts, N, sizeof(float3) * numverts);
  }
}

void Mesh::get_uv_tiles(ustring map, unordered_set<int> &tiles)
{
  Attribute *attr, *subd_attr;

  if (map.empty()) {
    attr = attributes.find(ATTR_STD_UV);
    subd_attr = subd_attributes.find(ATTR_STD_UV);
  }
  else {
    attr = attributes.find(map);
    subd_attr = subd_attributes.find(map);
  }

  if (attr) {
    attr->get_uv_tiles(this, ATTR_PRIM_GEOMETRY, tiles);
  }
  if (subd_attr) {
    subd_attr->get_uv_tiles(this, ATTR_PRIM_SUBD, tiles);
  }
}

void Mesh::compute_bounds()
{
  BoundBox bnds = BoundBox::empty;
  size_t verts_size = verts.size();

  if (verts_size > 0) {
    for (size_t i = 0; i < verts_size; i++)
      bnds.grow(verts[i]);

    Attribute *attr = attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
    if (use_motion_blur && attr) {
      size_t steps_size = verts.size() * (motion_steps - 1);
      float3 *vert_steps = attr->data_float3();

      for (size_t i = 0; i < steps_size; i++)
        bnds.grow(vert_steps[i]);
    }

    if (!bnds.valid()) {
      bnds = BoundBox::empty;

      /* skip nan or inf coordinates */
      for (size_t i = 0; i < verts_size; i++)
        bnds.grow_safe(verts[i]);

      if (use_motion_blur && attr) {
        size_t steps_size = verts.size() * (motion_steps - 1);
        float3 *vert_steps = attr->data_float3();

        for (size_t i = 0; i < steps_size; i++)
          bnds.grow_safe(vert_steps[i]);
      }
    }
  }

  if (!bnds.valid()) {
    /* empty mesh */
    bnds.grow(make_float3(0.0f, 0.0f, 0.0f));
  }

  bounds = bnds;
}

void Mesh::apply_transform(const Transform &tfm, const bool apply_to_motion)
{
  transform_normal = transform_transposed_inverse(tfm);

  /* apply to mesh vertices */
  for (size_t i = 0; i < verts.size(); i++)
    verts[i] = transform_point(&tfm, verts[i]);

  if (apply_to_motion) {
    Attribute *attr = attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);

    if (attr) {
      size_t steps_size = verts.size() * (motion_steps - 1);
      float3 *vert_steps = attr->data_float3();

      for (size_t i = 0; i < steps_size; i++)
        vert_steps[i] = transform_point(&tfm, vert_steps[i]);
    }

    Attribute *attr_N = attributes.find(ATTR_STD_MOTION_VERTEX_NORMAL);

    if (attr_N) {
      Transform ntfm = transform_normal;
      size_t steps_size = verts.size() * (motion_steps - 1);
      float3 *normal_steps = attr_N->data_float3();

      for (size_t i = 0; i < steps_size; i++)
        normal_steps[i] = normalize(transform_direction(&ntfm, normal_steps[i]));
    }
  }
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
  size_t size = attr->buffer_size(this, attrs.prim);

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

CCL_NAMESPACE_END
