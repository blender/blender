/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#include "BKE_attribute.hh"
#include "BKE_customdata.hh"
#include "BKE_deform.hh"
#include "BKE_lib_id.hh"
#include "BKE_material.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"
#include "BKE_object.hh"

#include "BLI_array_utils.hh"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.h"
#include "BLI_sort.hh"
#include "BLI_vector_set.hh"

#include "DEG_depsgraph_query.hh"

#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "obj_export_mesh.hh"

#include "bmesh.hh"
#include "bmesh_tools.hh"

namespace blender::io::obj {
OBJMesh::OBJMesh(Depsgraph *depsgraph, const OBJExportParams &export_params, Object *mesh_object)
{
  /* We need to copy the object because it may be in temporary space. */
  Object *obj_eval = DEG_get_evaluated(depsgraph, mesh_object);
  object_name_ = obj_eval->id.name + 2;
  export_mesh_ = nullptr;

  if (obj_eval->type == OB_MESH) {
    export_mesh_ = export_params.apply_modifiers ? BKE_object_get_evaluated_mesh(obj_eval) :
                                                   BKE_object_get_pre_modified_mesh(obj_eval);
  }

  if (export_mesh_) {
    mesh_edges_ = export_mesh_->edges();
    mesh_faces_ = export_mesh_->faces();
    mesh_corner_verts_ = export_mesh_->corner_verts();
    sharp_faces_ = *export_mesh_->attributes().lookup_or_default<bool>(
        "sharp_face", bke::AttrDomain::Face, false);
  }
  else {
    /* Curves and NURBS surfaces need a new mesh when they're
     * exported in the form of vertices and edges.
     */
    this->set_mesh(BKE_mesh_new_from_object(depsgraph, obj_eval, true, true, true));
  }
  if (export_params.export_triangulated_mesh && obj_eval->type == OB_MESH) {
    this->triangulate_mesh_eval();
  }

  this->materials.reinitialize(export_mesh_->totcol);
  for (const int i : this->materials.index_range()) {
    this->materials[i] = BKE_object_material_get_eval(obj_eval, i + 1);
  }

  set_world_axes_transform(*obj_eval,
                           export_params.forward_axis,
                           export_params.up_axis,
                           export_params.global_scale,
                           export_params.apply_transform);
}

/**
 * Free new meshes allocated for triangulated meshes, or Curve converted to Mesh.
 */
OBJMesh::~OBJMesh()
{
  clear();
}

void OBJMesh::set_mesh(Mesh *mesh)
{
  if (owned_export_mesh_) {
    BKE_id_free(nullptr, owned_export_mesh_);
  }
  owned_export_mesh_ = mesh;
  export_mesh_ = owned_export_mesh_;
  mesh_edges_ = mesh->edges();
  mesh_faces_ = mesh->faces();
  mesh_corner_verts_ = mesh->corner_verts();
  sharp_faces_ = *export_mesh_->attributes().lookup_or_default<bool>(
      "sharp_face", bke::AttrDomain::Face, false);
}

void OBJMesh::clear()
{
  if (owned_export_mesh_) {
    BKE_id_free(nullptr, owned_export_mesh_);
    owned_export_mesh_ = nullptr;
  }
  export_mesh_ = nullptr;
  corner_to_uv_index_ = {};
  uv_coords_.clear_and_shrink();
  corner_to_normal_index_ = {};
  normal_coords_ = {};
  face_order_ = {};
  if (face_smooth_groups_) {
    MEM_freeN(face_smooth_groups_);
    face_smooth_groups_ = nullptr;
  }
}

void OBJMesh::triangulate_mesh_eval()
{
  if (export_mesh_->faces_num <= 0) {
    return;
  }
  const BMeshCreateParams bm_create_params = {false};
  BMeshFromMeshParams bm_convert_params{};
  bm_convert_params.calc_face_normal = true;
  bm_convert_params.calc_vert_normal = true;
  bm_convert_params.add_key_index = false;
  bm_convert_params.use_shapekey = false;

  /* Lower threshold where triangulation of a face starts, i.e. a quadrilateral will be
   * triangulated here. */
  const int triangulate_min_verts = 4;

  BMesh *bmesh = BKE_mesh_to_bmesh_ex(export_mesh_, &bm_create_params, &bm_convert_params);
  BM_mesh_triangulate(bmesh,
                      MOD_TRIANGULATE_NGON_BEAUTY,
                      MOD_TRIANGULATE_QUAD_SHORTEDGE,
                      triangulate_min_verts,
                      false,
                      nullptr,
                      nullptr,
                      nullptr);
  Mesh *triangulated = BKE_mesh_from_bmesh_for_eval_nomain(bmesh, nullptr, export_mesh_);
  BM_mesh_free(bmesh);
  this->set_mesh(triangulated);
}

void OBJMesh::set_world_axes_transform(const Object &obj_eval,
                                       const eIOAxis forward,
                                       const eIOAxis up,
                                       const float global_scale,
                                       const bool apply_transform)
{
  float3x3 axes_transform;
  /* +Y-forward and +Z-up are the default Blender axis settings. */
  mat3_from_axis_conversion(forward, up, IO_AXIS_Y, IO_AXIS_Z, axes_transform.ptr());

  const float4x4 &object_to_world = apply_transform ? obj_eval.object_to_world() :
                                                      float4x4::identity();
  const float3x3 transform = axes_transform * float3x3(object_to_world);

  world_and_axes_transform_ = float4x4(transform);
  world_and_axes_transform_.location() = axes_transform * object_to_world.location();
  world_and_axes_transform_[3][3] = object_to_world[3][3];

  world_and_axes_transform_ = math::from_scale<float4x4>(float3(global_scale)) *
                              world_and_axes_transform_;

  /* Normals need inverse transpose of the regular matrix to handle non-uniform scale. */
  world_and_axes_normal_transform_ = math::transpose(math::invert(transform));

  mirrored_transform_ = math::is_negative(world_and_axes_normal_transform_);
}

int OBJMesh::tot_vertices() const
{
  return export_mesh_->verts_num;
}

int OBJMesh::tot_faces() const
{
  return export_mesh_->faces_num;
}

int OBJMesh::tot_uv_vertices() const
{
  return int(uv_coords_.size());
}

int OBJMesh::tot_edges() const
{
  return export_mesh_->edges_num;
}

int16_t OBJMesh::tot_materials() const
{
  return this->materials.size();
}

int OBJMesh::ith_smooth_group(const int face_index) const
{
  /* Calculate smooth groups first: #OBJMesh::calc_smooth_groups. */
  BLI_assert(tot_smooth_groups_ != -NEGATIVE_INIT);
  BLI_assert(face_smooth_groups_);
  return face_smooth_groups_[face_index];
}

void OBJMesh::calc_smooth_groups(const bool use_bitflags)
{
  const bke::AttributeAccessor attributes = export_mesh_->attributes();
  const VArraySpan sharp_edges = *attributes.lookup<bool>("sharp_edge", bke::AttrDomain::Edge);
  const VArraySpan sharp_faces = *attributes.lookup<bool>("sharp_face", bke::AttrDomain::Face);
  if (use_bitflags) {
    face_smooth_groups_ = BKE_mesh_calc_smoothgroups_bitflags(mesh_edges_.size(),
                                                              export_mesh_->verts_num,
                                                              mesh_faces_,
                                                              export_mesh_->corner_edges(),
                                                              export_mesh_->corner_verts(),
                                                              sharp_edges,
                                                              sharp_faces,
                                                              true,
                                                              &tot_smooth_groups_);
  }
  else {
    face_smooth_groups_ = BKE_mesh_calc_smoothgroups(mesh_edges_.size(),
                                                     mesh_faces_,
                                                     export_mesh_->corner_edges(),
                                                     sharp_edges,
                                                     sharp_faces,
                                                     &tot_smooth_groups_);
  }
}

void OBJMesh::calc_face_order()
{
  const bke::AttributeAccessor attributes = export_mesh_->attributes();
  const VArray<int> material_indices = *attributes.lookup_or_default<int>(
      "material_index", bke::AttrDomain::Face, 0);
  if (material_indices.is_single() && material_indices.get_internal_single() == 0) {
    return;
  }
  const VArraySpan<int> material_indices_span(material_indices);

  /* Sort faces by their material index. */
  face_order_.reinitialize(material_indices_span.size());
  array_utils::fill_index_range(face_order_.as_mutable_span());
  blender::parallel_sort(face_order_.begin(), face_order_.end(), [&](int a, int b) {
    int mat_a = material_indices_span[a];
    int mat_b = material_indices_span[b];
    if (mat_a != mat_b) {
      return mat_a < mat_b;
    }
    return a < b;
  });
}

bool OBJMesh::is_ith_face_smooth(const int face_index) const
{
  return !sharp_faces_[face_index];
}

StringRef OBJMesh::get_object_name() const
{
  return object_name_;
}

StringRef OBJMesh::get_object_mesh_name() const
{
  return export_mesh_->id.name + 2;
}

void OBJMesh::store_uv_coords_and_indices()
{
  const StringRef active_uv_name = export_mesh_->active_uv_map_name();
  if (active_uv_name.is_empty()) {
    uv_coords_.clear();
    return;
  }
  const bke::AttributeAccessor attributes = export_mesh_->attributes();
  const VArraySpan uv_map = *attributes.lookup<float2>(active_uv_name, bke::AttrDomain::Corner);
  if (uv_map.is_empty()) {
    uv_coords_.clear();
    return;
  }

  Map<float2, int> uv_to_index;

  /* We don't know how many unique UVs there will be, but this is a guess. */
  uv_to_index.reserve(export_mesh_->verts_num);
  uv_coords_.reserve(export_mesh_->verts_num);

  corner_to_uv_index_.reinitialize(uv_map.size());

  for (int index = 0; index < int(uv_map.size()); index++) {
    float2 uv = uv_map[index];
    int uv_index = uv_to_index.lookup_default(uv, -1);
    if (uv_index == -1) {
      uv_index = uv_to_index.size();
      uv_to_index.add(uv, uv_index);
      uv_coords_.append(uv);
    }
    corner_to_uv_index_[index] = uv_index;
  }
}

/** Round \a f to \a round_digits decimal digits. */
static float round_float_to_n_digits(const float f, int round_digits)
{
  float scale = powf(10.0, round_digits);
  return ceilf(scale * f - 0.49999999f) / scale;
}

static float3 round_float3_to_n_digits(const float3 &v, int round_digits)
{
  float3 ans;
  ans.x = round_float_to_n_digits(v.x, round_digits);
  ans.y = round_float_to_n_digits(v.y, round_digits);
  ans.z = round_float_to_n_digits(v.z, round_digits);
  return ans;
}

void OBJMesh::store_normal_coords_and_indices()
{
  /* We'll round normal components to 4 digits.
   * This will cover up some minor differences
   * between floating point calculations on different platforms.
   * Since normals are normalized, there will be no perceptible loss
   * of precision when rounding to 4 digits. */
  constexpr int round_digits = 4;
  VectorSet<float3> unique_normals;
  /* We don't know how many unique normals there will be, but this is a guess. */
  unique_normals.reserve(export_mesh_->faces_num);
  corner_to_normal_index_.reinitialize(export_mesh_->corners_num);

  /* Normals need inverse transpose of the regular matrix to handle non-uniform scale. */
  const float3x3 transform = world_and_axes_normal_transform_;
  auto add_normal = [&](const float3 &normal) {
    const float3 transformed = math::normalize(transform * normal);
    const float3 rounded = round_float3_to_n_digits(transformed, round_digits);
    return unique_normals.index_of_or_add(rounded);
  };

  switch (export_mesh_->normals_domain()) {
    case bke::MeshNormalDomain::Face: {
      const Span<float3> face_normals = export_mesh_->face_normals();
      for (const int face : mesh_faces_.index_range()) {
        const int index = add_normal(face_normals[face]);
        corner_to_normal_index_.as_mutable_span().slice(mesh_faces_[face]).fill(index);
      }
      break;
    }
    case bke::MeshNormalDomain::Point: {
      const Span<float3> vert_normals = export_mesh_->vert_normals();
      Array<int> vert_normal_indices(vert_normals.size());
      const bke::LooseVertCache &verts_no_face = export_mesh_->verts_no_face();
      if (verts_no_face.count == 0) {
        for (const int vert : vert_normals.index_range()) {
          vert_normal_indices[vert] = add_normal(vert_normals[vert]);
        }
      }
      else {
        for (const int vert : vert_normals.index_range()) {
          if (!verts_no_face.is_loose_bits[vert]) {
            vert_normal_indices[vert] = add_normal(vert_normals[vert]);
          }
        }
      }
      array_utils::gather(vert_normal_indices.as_span(),
                          mesh_corner_verts_,
                          corner_to_normal_index_.as_mutable_span());
      break;
    }
    case bke::MeshNormalDomain::Corner: {
      const Span<float3> corner_normals = export_mesh_->corner_normals();
      for (const int corner : corner_normals.index_range()) {
        corner_to_normal_index_[corner] = add_normal(corner_normals[corner]);
      }
      break;
    }
  }

  normal_coords_ = unique_normals.as_span();
}

int OBJMesh::tot_deform_groups() const
{
  return BLI_listbase_count(&export_mesh_->vertex_group_names);
}

int16_t OBJMesh::get_face_deform_group_index(const int face_index,
                                             MutableSpan<float> group_weights) const
{
  BLI_assert(face_index < export_mesh_->faces_num);
  BLI_assert(group_weights.size() == BLI_listbase_count(&export_mesh_->vertex_group_names));
  const Span<MDeformVert> dverts = export_mesh_->deform_verts();
  if (dverts.is_empty()) {
    return NOT_FOUND;
  }

  group_weights.fill(0);
  bool found_any_group = false;
  for (const int vert : mesh_corner_verts_.slice(mesh_faces_[face_index])) {
    const MDeformVert &dv = dverts[vert];
    for (int weight_i = 0; weight_i < dv.totweight; ++weight_i) {
      const auto group = dv.dw[weight_i].def_nr;
      if (group < group_weights.size()) {
        group_weights[group] += dv.dw[weight_i].weight;
        found_any_group = true;
      }
    }
  }

  if (!found_any_group) {
    return NOT_FOUND;
  }
  /* Index of the group with maximum vertices. */
  int16_t max_idx = std::max_element(group_weights.begin(), group_weights.end()) -
                    group_weights.begin();
  return max_idx;
}

const char *OBJMesh::get_face_deform_group_name(const int16_t def_group_index) const
{
  const bDeformGroup &vertex_group = *(static_cast<bDeformGroup *>(
      BLI_findlink(&export_mesh_->vertex_group_names, def_group_index)));
  return vertex_group.name;
}

}  // namespace blender::io::obj
