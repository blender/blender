/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <algorithm>

#include "bvh/build.h"
#include "bvh/bvh.h"

#include "device/device.h"

#include "scene/attribute.h"
#include "scene/mesh.h"
#include "scene/object.h"
#include "scene/scene.h"
#include "scene/shader_graph.h"

#include "subd/split.h"

#include "util/log.h"
#include "util/set.h"

#include "mikktspace.hh"

CCL_NAMESPACE_BEGIN

/* Tangent Space */

struct MikkMeshWrapper {
  MikkMeshWrapper(const Mesh *mesh,
                  const packed_normal *vertex_normal,
                  const packed_normal *corner_normal,
                  const float2 *uv,
                  packed_float3 *tangent,
                  float *tangent_sign)
      : mesh(mesh),
        position(mesh->get_position()),
        vertex_normal(vertex_normal),
        corner_normal(corner_normal),
        uv(uv),
        tangent(tangent),
        tangent_sign(tangent_sign)
  {
  }

  int GetNumFaces()
  {
    return mesh->num_triangles();
  }

  int GetNumVerticesOfFace(const int /*face_num*/)
  {
    return 3;
  }

  int CornerIndex(const int face_num, const int vert_num)
  {
    return face_num * 3 + vert_num;
  }

  int VertexIndex(const int face_num, const int vert_num)
  {
    const int corner = CornerIndex(face_num, vert_num);
    return mesh->get_triangles()[corner];
  }

  mikk::float3 GetPosition(const int face_num, const int vert_num)
  {
    const float3 vP = float3(position[VertexIndex(face_num, vert_num)]);
    return mikk::float3(vP.x, vP.y, vP.z);
  }

  mikk::float3 GetTexCoord(const int face_num, const int vert_num)
  {
    /* TODO: Check whether introducing a template boolean in order to
     * turn this into a constexpr is worth it. */
    if (has_uv()) {
      const int corner_index = CornerIndex(face_num, vert_num);
      const float2 tfuv = uv[corner_index];
      return mikk::float3(tfuv.x, tfuv.y, 1.0f);
    }
    /* revert to vertex position */
    const float3 vP = float3(position[VertexIndex(face_num, vert_num)]);
    const float2 uv = map_to_sphere(vP);
    return mikk::float3(uv.x, uv.y, 1.0f);
  }

  mikk::float3 GetNormal(const int face_num, const int vert_num)
  {
    float3 vN;
    if (mesh->get_smooth()[face_num]) {
      vN = ((corner_normal) ? corner_normal[CornerIndex(face_num, vert_num)] :
                              vertex_normal[VertexIndex(face_num, vert_num)])
               .decode();
    }
    else {
      const Mesh::Triangle tri = mesh->get_triangle(face_num);
      vN = tri.compute_normal(position);
    }
    return mikk::float3(vN.x, vN.y, vN.z);
  }

  void SetTangentSpace(const int face_num, const int vert_num, mikk::float3 T, bool orientation)
  {
    const int corner_index = CornerIndex(face_num, vert_num);
    tangent[corner_index] = packed_float3(make_float3(T.x, T.y, T.z));
    if (tangent_sign != nullptr) {
      tangent_sign[corner_index] = orientation ? 1.0f : -1.0f;
    }
  }

  bool has_uv() const
  {
    return uv != nullptr;
  }

  const Mesh *mesh;
  const packed_float3 *position;

  const packed_normal *vertex_normal;
  const packed_normal *corner_normal;
  const float2 *uv;

  packed_float3 *tangent;
  float *tangent_sign;
};

static void mikk_compute_tangents(Attribute *attr_uv,
                                  Mesh *mesh,
                                  const bool need_sign,
                                  const AttributeStandard tangent_std,
                                  const AttributeStandard tangent_sign_std,
                                  const char *tangent_postfix,
                                  const char *tangent_sign_postfix)
{
  /* Create tangent attributes. */
  AttributeSet &attributes = mesh->attributes;

  Attribute *attr_vN = attributes.find(ATTR_STD_VERTEX_NORMAL);
  Attribute *attr_cN = attributes.find(ATTR_STD_CORNER_NORMAL);
  if (attr_vN == nullptr && attr_cN == nullptr) {
    /* no normals */
    return;
  }

  const packed_normal *vertex_normal = attr_vN ? attr_vN->data<packed_normal>() : nullptr;
  const packed_normal *corner_normal = attr_cN ? attr_cN->data<packed_normal>() : nullptr;
  const float2 *uv = (attr_uv) ? attr_uv->data<float2>() : nullptr;

  const ustring name = ustring((attr_uv) ? attr_uv->name.string() + tangent_postfix :
                                           Attribute::standard_name(tangent_std));
  Attribute *attr;
  if (attr_uv == nullptr || attr_uv->std == ATTR_STD_UV) {
    attr = attributes.add(tangent_std, name);
  }
  else {
    attr = attributes.add(name, TypeVector, ATTR_ELEMENT_CORNER);
  }
  packed_float3 *tangent = attr->data_for_write<packed_float3>();
  /* Create bitangent sign attribute. */
  float *tangent_sign = nullptr;
  if (need_sign) {
    const ustring name_sign = ustring((attr_uv) ? attr_uv->name.string() + tangent_sign_postfix :
                                                  Attribute::standard_name(tangent_sign_std));
    Attribute *attr_sign;
    if (attr_uv == nullptr || attr_uv->std == ATTR_STD_UV) {
      attr_sign = attributes.add(tangent_sign_std, name_sign);
    }
    else {
      attr_sign = attributes.add(name_sign, TypeFloat, ATTR_ELEMENT_CORNER);
    }
    tangent_sign = attr_sign->data_for_write<float>();
  }

  MikkMeshWrapper userdata(mesh, vertex_normal, corner_normal, uv, tangent, tangent_sign);
  /* Compute tangents. */
  mikk::Mikktspace(userdata).genTangSpace();
}

/* Triangle */

void Mesh::Triangle::bounds_grow(const packed_float3 *verts, BoundBox &bounds) const
{
  bounds.grow(verts[v[0]]);
  bounds.grow(verts[v[1]]);
  bounds.grow(verts[v[2]]);
}

void Mesh::Triangle::motion_verts(const Attribute *attr_P,
                                  const size_t num_steps,
                                  const float time,
                                  float3 r_verts[3]) const
{
  /* Figure out which steps we need to fetch and their interpolation factor. */
  const size_t max_step = num_steps - 1;
  const size_t step = min((size_t)(time * max_step), max_step - 1);
  const float t = time * max_step - step;
  /* Fetch vertex coordinates. */
  float3 curr_verts[3];
  float3 next_verts[3];
  verts_for_step(attr_P, step, curr_verts);
  verts_for_step(attr_P, step + 1, next_verts);
  /* Interpolate between steps. */
  r_verts[0] = (1.0f - t) * curr_verts[0] + t * next_verts[0];
  r_verts[1] = (1.0f - t) * curr_verts[1] + t * next_verts[1];
  r_verts[2] = (1.0f - t) * curr_verts[2] + t * next_verts[2];
}

void Mesh::Triangle::verts_for_step(const Attribute *attr_P,
                                    const size_t step,
                                    float3 r_verts[3]) const
{
  const packed_float3 *vert_step = attr_P->data_at_time_step<packed_float3>(
      step, attr_P->num_motion_steps());
  r_verts[0] = vert_step[v[0]];
  r_verts[1] = vert_step[v[1]];
  r_verts[2] = vert_step[v[2]];
}

float3 Mesh::Triangle::compute_normal(const packed_float3 *verts) const
{
  const float3 v0 = verts[v[0]];
  const float3 v1 = verts[v[1]];
  const float3 v2 = verts[v[2]];
  const float3 norm = cross(v1 - v0, v2 - v0);
  const float normlen = len(norm);
  if (normlen == 0.0f) {
    return make_float3(1.0f, 0.0f, 0.0f);
  }
  return norm / normlen;
}

bool Mesh::Triangle::valid(const packed_float3 *verts) const
{
  return isfinite_safe(float3(verts[v[0]])) && isfinite_safe(float3(verts[v[1]])) &&
         isfinite_safe(float3(verts[v[2]]));
}

/* SubdFace */

float3 Mesh::SubdFace::normal(const Mesh *mesh) const
{
  const packed_float3 *verts =
      mesh->subd_attributes.find(ATTR_STD_POSITION)->data<packed_float3>();
  const float3 v0 = verts[mesh->subd_face_corners[start_corner + 0]];
  const float3 v1 = verts[mesh->subd_face_corners[start_corner + 1]];
  const float3 v2 = verts[mesh->subd_face_corners[start_corner + 2]];

  return safe_normalize(cross(v1 - v0, v2 - v0));
}

size_t Mesh::num_verts() const
{
  const Attribute *attr = attributes.find(ATTR_STD_POSITION);
  return attr ? attr->size : 0;
}

/* Mesh */

NODE_DEFINE(Mesh)
{
  NodeType *type = NodeType::add("mesh", create, NodeType::NONE, Geometry::get_node_base_type());

  SOCKET_INT_ARRAY(triangles, "Triangles", array<int>());
  SOCKET_INT_ARRAY(shader, "Shader", array<int>());
  SOCKET_BOOLEAN_ARRAY(smooth, "Smooth", array<bool>());

  static NodeEnum subdivision_type_enum;
  subdivision_type_enum.insert("none", SUBDIVISION_NONE);
  subdivision_type_enum.insert("linear", SUBDIVISION_LINEAR);
  subdivision_type_enum.insert("catmull_clark", SUBDIVISION_CATMULL_CLARK);
  SOCKET_ENUM(subdivision_type, "Subdivision Type", subdivision_type_enum, SUBDIVISION_NONE);

  static NodeEnum subdivision_boundary_interpolation_enum;
  subdivision_boundary_interpolation_enum.insert("none", SUBDIVISION_BOUNDARY_NONE);
  subdivision_boundary_interpolation_enum.insert("edge_only", SUBDIVISION_BOUNDARY_EDGE_ONLY);
  subdivision_boundary_interpolation_enum.insert("edge_and_corner",
                                                 SUBDIVISION_BOUNDARY_EDGE_AND_CORNER);
  SOCKET_ENUM(subdivision_boundary_interpolation,
              "Subdivision Boundary Interpolation",
              subdivision_boundary_interpolation_enum,
              SUBDIVISION_BOUNDARY_EDGE_AND_CORNER);

  static NodeEnum subdivision_fvar_interpolation_enum;
  subdivision_fvar_interpolation_enum.insert("none", SUBDIVISION_FVAR_LINEAR_NONE);
  subdivision_fvar_interpolation_enum.insert("corners_only", SUBDIVISION_FVAR_LINEAR_CORNERS_ONLY);
  subdivision_fvar_interpolation_enum.insert("corners_plus1",
                                             SUBDIVISION_FVAR_LINEAR_CORNERS_PLUS1);
  subdivision_fvar_interpolation_enum.insert("corners_plus2",
                                             SUBDIVISION_FVAR_LINEAR_CORNERS_PLUS2);
  subdivision_fvar_interpolation_enum.insert("boundaries", SUBDIVISION_FVAR_LINEAR_BOUNDARIES);
  subdivision_fvar_interpolation_enum.insert("all", SUBDIVISION_FVAR_LINEAR_ALL);
  SOCKET_ENUM(subdivision_fvar_interpolation,
              "Subdivision Face-Varying Interpolation",
              subdivision_fvar_interpolation_enum,
              SUBDIVISION_FVAR_LINEAR_BOUNDARIES);

  SOCKET_INT_ARRAY(subd_vert_creases, "Subdivision Vertex Crease", array<int>());
  SOCKET_FLOAT_ARRAY(
      subd_vert_creases_weight, "Subdivision Vertex Crease Weights", array<float>());
  SOCKET_INT_ARRAY(subd_creases_edge, "Subdivision Crease Edges", array<int>());
  SOCKET_FLOAT_ARRAY(subd_creases_weight, "Subdivision Crease Weights", array<float>());
  SOCKET_INT_ARRAY(subd_face_corners, "Subdivision Face Corners", array<int>());
  SOCKET_INT_ARRAY(subd_start_corner, "Subdivision Face Start Corner", array<int>());
  SOCKET_INT_ARRAY(subd_num_corners, "Subdivision Face Corner Count", array<int>());
  SOCKET_INT_ARRAY(subd_shader, "Subdivision Face Shader", array<int>());
  SOCKET_BOOLEAN_ARRAY(subd_smooth, "Subdivision Face Smooth", array<bool>());
  SOCKET_INT_ARRAY(subd_ptex_offset, "Subdivision Face PTex Offset", array<int>());

  /* Subdivisions parameters */
  static NodeEnum subd_adaptive_space_enum;
  subd_adaptive_space_enum.insert("pixel", SUBDIVISION_ADAPTIVE_SPACE_PIXEL);
  subd_adaptive_space_enum.insert("object", SUBDIVISION_ADAPTIVE_SPACE_OBJECT);

  SOCKET_ENUM(subd_adaptive_space,
              "Subdivision Adaptive Space",
              subd_adaptive_space_enum,
              SUBDIVISION_ADAPTIVE_SPACE_PIXEL);
  SOCKET_FLOAT(subd_dicing_rate, "Subdivision Dicing Rate", 1.0f)
  SOCKET_INT(subd_max_level, "Max Subdivision Level", 1);
  SOCKET_TRANSFORM(subd_objecttoworld, "Subdivision Object Transform", transform_identity());

  return type;
}

bool Mesh::need_tesselation()
{
  return (subdivision_type != SUBDIVISION_NONE) &&
         (position_is_modified() || subd_dicing_rate_is_modified() ||
          subd_adaptive_space_is_modified() || subd_objecttoworld_is_modified() ||
          subd_max_level_is_modified());
}

Mesh::Mesh(const NodeType *node_type, Type geom_type_)
    : Geometry(node_type, geom_type_), subd_attributes(this, ATTR_PRIM_SUBD)
{
  face_offset = 0;
  corner_offset = 0;

  num_subd_added_verts = 0;
  num_subd_faces = 0;

  subdivision_type = SUBDIVISION_NONE;

  add_builtin_attributes();
}

Mesh::Mesh() : Mesh(get_node_type(), Geometry::MESH) {}

void Mesh::add_builtin_attributes()
{
  attributes.add(ATTR_STD_POSITION);
}

void Mesh::resize_mesh(const int numverts, const int numtris)
{
  Attribute *attr_P = attributes.add(ATTR_STD_POSITION);
  attr_P->resize(numverts);
  triangles.resize(numtris * 3);
  shader.resize(numtris);
  smooth.resize(numtris);

  attributes.resize();
}

void Mesh::resize_subd_faces(const int numfaces, const int numcorners)
{
  subd_start_corner.resize(numfaces);
  subd_num_corners.resize(numfaces);
  subd_shader.resize(numfaces);
  subd_smooth.resize(numfaces);
  subd_ptex_offset.resize(numfaces);
  subd_face_corners.resize(numcorners);
  num_subd_faces = numfaces;

  subd_attributes.resize();
}

void Mesh::reserve_subd_creases(const size_t num_creases)
{
  subd_creases_edge.reserve(num_creases * 2);
  subd_creases_weight.reserve(num_creases);
}

void Mesh::clear_non_sockets()
{
  Geometry::clear(true);

  num_subd_added_verts = 0;
  num_subd_faces = 0;
}

void Mesh::clear(bool preserve_shaders, bool preserve_voxel_data)
{
  Geometry::clear(preserve_shaders);

  /* clear all verts and triangles */
  triangles.clear();
  shader.clear();
  smooth.clear();

  subd_start_corner.clear();
  subd_num_corners.clear();
  subd_shader.clear();
  subd_smooth.clear();
  subd_ptex_offset.clear();
  subd_face_corners.clear();

  subd_creases_edge.clear();
  subd_creases_weight.clear();

  subd_attributes.clear();
  attributes.clear(preserve_voxel_data);
  add_builtin_attributes();

  subdivision_type = SubdivisionType::SUBDIVISION_NONE;

  clear_non_sockets();
}

void Mesh::clear(bool preserve_shaders)
{
  clear(preserve_shaders, false);
}

Mesh::SubdFace Mesh::get_subd_face(const size_t index) const
{
  Mesh::SubdFace s;
  s.shader = subd_shader[index];
  s.num_corners = subd_num_corners[index];
  s.smooth = subd_smooth[index];
  s.ptex_offset = subd_ptex_offset[index];
  s.start_corner = subd_start_corner[index];
  return s;
}

void Mesh::add_edge_crease(const int v0, const int v1, const float weight)
{
  subd_creases_edge.push_back_slow(v0);
  subd_creases_edge.push_back_slow(v1);
  subd_creases_weight.push_back_slow(weight);

  tag_subd_creases_edge_modified();
  tag_subd_creases_edge_modified();
  tag_subd_creases_weight_modified();
}

void Mesh::add_vertex_crease(const int v, const float weight)
{
  subd_vert_creases.push_back_slow(v);
  subd_vert_creases_weight.push_back_slow(weight);

  tag_subd_vert_creases_modified();
  tag_subd_vert_creases_weight_modified();
}

void Mesh::copy_center_to_motion_step(const int motion_step)
{
  const int attr_step = motion_step + 1;
  for (AttributeSet *attr_set : {&attributes, &subd_attributes}) {
    Attribute *attr_P = attr_set->find(ATTR_STD_POSITION);
    if (attr_P && attr_P->has_motion()) {
      const packed_float3 *P = attr_P->data<packed_float3>();
      std::copy_n(P, attr_P->size, attr_P->data_for_write<packed_float3>(attr_step));
    }

    Attribute *attr_N = attr_set->find(ATTR_STD_VERTEX_NORMAL);
    if (attr_N && attr_N->has_motion()) {
      const packed_normal *N = attr_N->data<packed_normal>();
      std::copy_n(N, attr_N->size, attr_N->data_for_write<packed_normal>(attr_step));
    }
  }

  Attribute *attr_cN = attributes.find(ATTR_STD_CORNER_NORMAL);
  if (attr_cN && attr_cN->has_motion()) {
    const size_t numcorners = triangles.size();
    const packed_normal *N = attr_cN->data<packed_normal>();
    std::copy_n(N, numcorners, attr_cN->data_for_write<packed_normal>(attr_step));
  }
}

void Mesh::get_uv_tiles(ustring map, unordered_set<int> &tiles)
{
  Attribute *attr;
  Attribute *subd_attr;

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
  const size_t verts_size = num_verts();
  const packed_float3 *verts = get_position();

  if (verts_size > 0) {
    for (size_t i = 0; i < verts_size; i++) {
      bnds.grow(verts[i]);
    }

    Attribute *attr_P = attributes.find(ATTR_STD_POSITION);
    if (use_motion_blur && attr_P->has_motion()) {
      for (int attr_step = 1; attr_step < attr_P->num_motion_steps(); attr_step++) {
        const packed_float3 *vert_step = attr_P->data<packed_float3>(attr_step);
        for (size_t i = 0; i < verts_size; i++) {
          bnds.grow(vert_step[i]);
        }
      }
    }

    if (!bnds.valid()) {
      bnds = BoundBox::empty;

      /* skip nan or inf coordinates */
      for (size_t i = 0; i < verts_size; i++) {
        bnds.grow_safe(verts[i]);
      }

      if (use_motion_blur && attr_P->has_motion()) {
        for (int attr_step = 1; attr_step < attr_P->num_motion_steps(); attr_step++) {
          const packed_float3 *vert_step = attr_P->data<packed_float3>(attr_step);
          for (size_t i = 0; i < verts_size; i++) {
            bnds.grow_safe(vert_step[i]);
          }
        }
      }
    }
  }

  if (!bnds.valid()) {
    /* empty mesh */
    bnds.grow(zero_float3());
  }

  bounds = bnds;
}

void Mesh::apply_transform(const Transform &tfm, const bool apply_to_motion)
{
  transform_normal = transform_transposed_inverse(tfm);

  /* apply to mesh vertices */
  packed_float3 *verts = get_position_for_write();
  const size_t num_verts = this->num_verts();
  for (size_t i = 0; i < num_verts; i++) {
    verts[i] = transform_point(&tfm, verts[i]);
  }

  tag_position_modified();

  Attribute *attr_vN = attributes.find(ATTR_STD_VERTEX_NORMAL);
  if (attr_vN) {
    const Transform ntfm = transform_normal;
    const size_t num_verts = this->num_verts();
    packed_normal *vN = attr_vN->data_for_write<packed_normal>();

    for (size_t i = 0; i < num_verts; i++) {
      vN[i] = packed_normal(normalize(transform_direction(&ntfm, vN[i].decode())));
    }
  }

  Attribute *attr_cN = attributes.find(ATTR_STD_CORNER_NORMAL);
  if (attr_cN) {
    const Transform ntfm = transform_normal;
    const size_t num_corners = triangles.size();
    packed_normal *cN = attr_cN->data_for_write<packed_normal>();

    for (size_t i = 0; i < num_corners; i++) {
      cN[i] = packed_normal(normalize(transform_direction(&ntfm, cN[i].decode())));
    }
  }

  Attribute *attr_uN = attributes.find(ATTR_STD_NORMAL_UNDISPLACED);
  if (attr_uN) {
    const Transform ntfm = transform_normal;
    const size_t size = attr_uN->buffer_size(this, ATTR_PRIM_GEOMETRY) / sizeof(packed_normal);
    packed_normal *uN = attr_uN->data_for_write<packed_normal>();

    for (size_t i = 0; i < size; i++) {
      uN[i] = packed_normal(normalize(transform_direction(&ntfm, uN[i].decode())));
    }
  }

  if (apply_to_motion) {
    Attribute *attr_P = attributes.find(ATTR_STD_POSITION);

    if (attr_P->has_motion()) {
      const size_t num_verts = this->num_verts();
      for (int step = 1; step <= int(attr_P->motion.size()); step++) {
        packed_float3 *vert_step = attr_P->data_for_write<packed_float3>(step);
        for (size_t i = 0; i < num_verts; i++) {
          vert_step[i] = transform_point(&tfm, vert_step[i]);
        }
      }
    }

    Attribute *attr_mN = attributes.find(ATTR_STD_VERTEX_NORMAL);

    if (attr_mN && attr_mN->has_motion()) {
      const Transform ntfm = transform_normal;
      const size_t num_verts = this->num_verts();
      for (int step = 1; step <= int(attr_mN->motion.size()); step++) {
        packed_normal *normal_step = attr_mN->data_for_write<packed_normal>(step);
        for (size_t i = 0; i < num_verts; i++) {
          normal_step[i] = packed_normal(
              normalize(transform_direction(&ntfm, normal_step[i].decode())));
        }
      }
    }

    Attribute *attr_mcN = attributes.find(ATTR_STD_CORNER_NORMAL);

    if (attr_mcN && attr_mcN->has_motion()) {
      const Transform ntfm = transform_normal;
      const size_t nc = triangles.size();
      for (int step = 1; step <= int(attr_mcN->motion.size()); step++) {
        packed_normal *normal_step = attr_mcN->data_for_write<packed_normal>(step);
        for (size_t i = 0; i < nc; i++) {
          normal_step[i] = packed_normal(
              normalize(transform_direction(&ntfm, normal_step[i].decode())));
        }
      }
    }
  }
}

void Mesh::add_vertex_normals()
{
  Attribute *attr_cN = attributes.find(ATTR_STD_CORNER_NORMAL);
  if (attr_cN) {
    /* Not needed if we already have corner normals overriding these.
     * If there is motion blur without motion corner normals we can't
     * render correctly, discard corner normals. */
    if (has_motion_blur() && !attr_cN->has_motion()) {
      attributes.remove(ATTR_STD_CORNER_NORMAL);
    }
    else {
      return;
    }
  }

  const bool flip = transform_negative_scaled;
  const size_t verts_size = num_verts();
  const size_t triangles_size = num_triangles();

  /* static vertex normals */
  if (!attributes.find(ATTR_STD_VERTEX_NORMAL) && triangles_size) {
    /* get attributes */
    Attribute *attr_vN = attributes.add(ATTR_STD_VERTEX_NORMAL);

    const packed_float3 *verts_ptr = get_position();
    packed_normal *vN = attr_vN->data_for_write<packed_normal>();

    /* compute vertex normals */
    vector<float3> vN_float(verts_size, zero_float3());

    for (size_t i = 0; i < triangles_size; i++) {
      const float3 fN = get_triangle(i).compute_normal(verts_ptr);
      for (size_t j = 0; j < 3; j++) {
        vN_float[get_triangle(i).v[j]] += fN;
      }
    }

    if (flip) {
      for (size_t i = 0; i < verts_size; i++) {
        vN[i] = packed_normal(-normalize(vN_float[i]));
      }
    }
    else {
      for (size_t i = 0; i < verts_size; i++) {
        vN[i] = packed_normal(normalize(vN_float[i]));
      }
    }
  }

  /* motion vertex normals */
  Attribute *attr_P = attributes.find(ATTR_STD_POSITION);
  Attribute *attr_N = attributes.find(ATTR_STD_VERTEX_NORMAL);

  if (has_motion_blur() && attr_P->has_motion() && !(attr_N && attr_N->has_motion()) &&
      triangles_size)
  {
    if (!attr_N) {
      attr_N = attributes.add(ATTR_STD_VERTEX_NORMAL);
    }
    attr_N->add_motion(this);

    for (int attr_step = 1; attr_step < attr_P->num_motion_steps(); attr_step++) {
      const packed_float3 *mP = attr_P->data<packed_float3>(attr_step);
      packed_normal *mN = attr_N->data_for_write<packed_normal>(attr_step);

      /* compute */
      vector<float3> mN_float(verts_size, zero_float3());

      for (size_t i = 0; i < triangles_size; i++) {
        const Triangle tri = get_triangle(i);
        const float3 fN = tri.compute_normal(mP);
        for (size_t j = 0; j < 3; j++) {
          mN_float[tri.v[j]] += fN;
        }
      }

      if (flip) {
        for (size_t i = 0; i < verts_size; i++) {
          mN[i] = packed_normal(-normalize(mN_float[i]));
        }
      }
      else {
        for (size_t i = 0; i < verts_size; i++) {
          mN[i] = packed_normal(normalize(mN_float[i]));
        }
      }
    }
  }

  /* subd vertex normals */
  if (!subd_attributes.find(ATTR_STD_VERTEX_NORMAL) && get_num_subd_faces()) {
    /* get attributes */
    Attribute *attr_vN = subd_attributes.add(ATTR_STD_VERTEX_NORMAL);
    packed_normal *vN = attr_vN->data_for_write<packed_normal>();

    /* compute vertex normals */
    vector<float3> vN_float(verts_size, zero_float3());

    for (size_t i = 0; i < get_num_subd_faces(); i++) {
      const SubdFace face = get_subd_face(i);
      const float3 fN = face.normal(this);

      for (size_t j = 0; j < face.num_corners; j++) {
        const size_t corner = subd_face_corners[face.start_corner + j];
        vN_float[corner] += fN;
      }
    }

    if (flip) {
      for (size_t i = 0; i < verts_size; i++) {
        vN[i] = packed_normal(-normalize(vN_float[i]));
      }
    }
    else {
      for (size_t i = 0; i < verts_size; i++) {
        vN[i] = packed_normal(normalize(vN_float[i]));
      }
    }
  }
}

void Mesh::add_undisplaced(Scene *scene)
{
  if (need_attribute(scene, ATTR_STD_POSITION_UNDISPLACED) &&
      !attributes.find(ATTR_STD_POSITION_UNDISPLACED))
  {
    /* Copy position to attribute. */
    Attribute *attr = attributes.add(ATTR_STD_POSITION_UNDISPLACED);

    size_t size = attr->buffer_size(this, ATTR_PRIM_GEOMETRY) / sizeof(packed_float3);
    std::copy_n(get_position(), size, attr->data_for_write<packed_float3>());
  }

  if (need_attribute(scene, ATTR_STD_NORMAL_UNDISPLACED) &&
      !attributes.find(ATTR_STD_NORMAL_UNDISPLACED))
  {
    /* Copy corner or vertex normal to attribute, using the matching element type
     * so the kernel reads and interpolates it correctly. */
    Attribute *attr_N = attributes.find(ATTR_STD_CORNER_NORMAL);
    if (!attr_N) {
      attr_N = attributes.find(ATTR_STD_VERTEX_NORMAL);
    }
    if (attr_N) {
      Attribute *attr = attributes.add(
          ustring(Attribute::standard_name(ATTR_STD_NORMAL_UNDISPLACED)),
          TypeNormal,
          attr_N->element);
      attr->std = ATTR_STD_NORMAL_UNDISPLACED;

      size_t size = attr->buffer_size(this, ATTR_PRIM_GEOMETRY) / sizeof(packed_normal);
      std::copy_n(attr_N->data<packed_normal>(), size, attr->data_for_write<packed_normal>());
    }
  }
}

void Mesh::update_generated(Scene *scene)
{
  if (!num_triangles()) {
    return;
  }

  AttributeSet &attrs = num_subd_faces ? subd_attributes : attributes;

  /* apply generated attributes if needed or missing */
  if (need_attribute(scene, ATTR_STD_GENERATED) && !attrs.find(ATTR_STD_GENERATED)) {
    const size_t verts_size = num_verts();
    const packed_float3 *verts = get_position();
    Attribute *attr_generated = attrs.add(ATTR_STD_GENERATED);
    packed_float3 *generated = attr_generated->data_for_write<packed_float3>();
    for (size_t i = 0; i < verts_size; ++i) {
      generated[i] = verts[i];
    }
  }
}

void Mesh::update_tangents(Scene *scene, bool undisplaced)
{
  if (!num_triangles()) {
    return;
  }

  assert(attributes.find(ATTR_STD_VERTEX_NORMAL) || attributes.find(ATTR_STD_CORNER_NORMAL));

  ccl::set<ustring> uv_maps;
  Attribute *attr_std_uv = attributes.find(ATTR_STD_UV);

  AttributeStandard tangent_std = (undisplaced) ? ATTR_STD_UV_TANGENT_UNDISPLACED :
                                                  ATTR_STD_UV_TANGENT;
  AttributeStandard tangent_sign_std = (undisplaced) ? ATTR_STD_UV_TANGENT_SIGN_UNDISPLACED :
                                                       ATTR_STD_UV_TANGENT_SIGN;
  const char *tangent_postfix = (undisplaced) ? ".undisplaced_tangent" : ".tangent";
  const char *tangent_sign_postfix = (undisplaced) ? ".undisplaced_tangent_sign" : ".tangent_sign";

  /* standard UVs */
  if ((need_attribute(scene, tangent_std) || need_attribute(scene, tangent_sign_std)) &&
      !attributes.find(tangent_std))
  {
    mikk_compute_tangents(attr_std_uv,
                          this,
                          true,
                          tangent_std,
                          tangent_sign_std,
                          tangent_postfix,
                          tangent_sign_postfix); /* sign */
  }

  /* now generate for any other UVs requested */
  for (Attribute &attr : attributes.attributes) {
    if (!(attr.type == TypeFloat2 && attr.element == ATTR_ELEMENT_CORNER)) {
      continue;
    }

    const ustring tangent_name = ustring(attr.name.string() + tangent_postfix);
    const ustring tangent_sign_name = ustring(attr.name.string() + tangent_sign_postfix);

    if ((need_attribute(scene, tangent_name) || need_attribute(scene, tangent_sign_name)) &&
        !attributes.find(tangent_name))
    {
      mikk_compute_tangents(&attr,
                            this,
                            true,
                            tangent_std,
                            tangent_sign_std,
                            tangent_postfix,
                            tangent_sign_postfix); /* sign */
    }
  }
}

void Mesh::pack_shaders(Scene *scene, uint *tri_shader)
{
  uint shader_id = 0;
  uint last_shader = -1;
  bool last_smooth = false;

  const size_t triangles_size = num_triangles();
  const int *shader_ptr = shader.data();

  /* Corner normals override the smooth flag, as the flatness is already
   * encoded in the corner normals and we always interpolate them. */
  const bool use_corner_normals = attributes.find(ATTR_STD_CORNER_NORMAL) != nullptr;
  const bool *smooth_ptr = (use_corner_normals) ? nullptr : smooth.data();
  const bool smooth_constant = (use_corner_normals) ? true : false;

  for (size_t i = 0; i < triangles_size; i++) {
    const int new_shader = shader_ptr ? shader_ptr[i] : INT_MAX;
    const bool new_smooth = smooth_ptr ? smooth_ptr[i] : smooth_constant;

    if (new_shader != last_shader || last_smooth != new_smooth) {
      last_shader = new_shader;
      last_smooth = new_smooth;
      Shader *shader = (last_shader < used_shaders.size()) ?
                           static_cast<Shader *>(used_shaders[last_shader]) :
                           scene->default_surface;
      shader_id = scene->shader_manager->get_shader_id(shader, last_smooth);
    }

    tri_shader[i] = shader_id;
  }
}

void Mesh::pack_triangles(packed_uint3 *tri_vindex)
{
  const size_t triangles_size = num_triangles();
  const int *p_tris = triangles.data();
  int off = 0;
  for (size_t i = 0; i < triangles_size; i++) {
    tri_vindex[i] = make_packed_uint3(p_tris[off + 0], p_tris[off + 1], p_tris[off + 2]);
    off += 3;
  }
}

bool Mesh::has_motion_blur() const
{
  Attribute *attr_P = attributes.find(ATTR_STD_POSITION);
  Attribute *subd_attr_P = subd_attributes.find(ATTR_STD_POSITION);
  return use_motion_blur &&
         (attr_P->has_motion() || (get_subdivision_type() != Mesh::SUBDIVISION_NONE &&
                                   subd_attr_P && subd_attr_P->has_motion()));
}

PrimitiveType Mesh::primitive_type() const
{
  return has_motion_blur() ? PRIMITIVE_MOTION_TRIANGLE : PRIMITIVE_TRIANGLE;
}

CCL_NAMESPACE_END
