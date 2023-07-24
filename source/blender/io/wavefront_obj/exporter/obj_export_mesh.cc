/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#include "BKE_attribute.hh"
#include "BKE_customdata.h"
#include "BKE_deform.h"
#include "BKE_lib_id.h"
#include "BKE_material.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.h"
#include "BKE_object.h"

#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_math.h"
#include "BLI_sort.hh"

#include "DEG_depsgraph_query.h"

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "obj_export_mesh.hh"

#include "bmesh.h"
#include "bmesh_tools.h"

namespace blender::io::obj {
OBJMesh::OBJMesh(Depsgraph *depsgraph, const OBJExportParams &export_params, Object *mesh_object)
{
  /* We need to copy the object because it may be in temporary space. */
  Object *obj_eval = DEG_get_evaluated_object(depsgraph, mesh_object);
  export_object_eval_ = dna::shallow_copy(*obj_eval);
  export_mesh_ = export_params.apply_modifiers ?
                     BKE_object_get_evaluated_mesh(&export_object_eval_) :
                     BKE_object_get_pre_modified_mesh(&export_object_eval_);
  if (export_mesh_) {
    mesh_positions_ = export_mesh_->vert_positions();
    mesh_edges_ = export_mesh_->edges();
    mesh_faces_ = export_mesh_->faces();
    mesh_corner_verts_ = export_mesh_->corner_verts();
    sharp_faces_ = *export_mesh_->attributes().lookup_or_default<bool>(
        "sharp_face", ATTR_DOMAIN_FACE, false);
  }
  else {
    /* Curves and NURBS surfaces need a new mesh when they're
     * exported in the form of vertices and edges.
     */
    this->set_mesh(BKE_mesh_new_from_object(depsgraph, &export_object_eval_, true, true));
  }
  if (export_params.export_triangulated_mesh && export_object_eval_.type == OB_MESH) {
    this->triangulate_mesh_eval();
  }
  set_world_axes_transform(export_params.forward_axis, export_params.up_axis);
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
  mesh_positions_ = mesh->vert_positions();
  mesh_edges_ = mesh->edges();
  mesh_faces_ = mesh->faces();
  mesh_corner_verts_ = mesh->corner_verts();
  sharp_faces_ = *export_mesh_->attributes().lookup_or_default<bool>(
      "sharp_face", ATTR_DOMAIN_FACE, false);
}

void OBJMesh::clear()
{
  if (owned_export_mesh_) {
    BKE_id_free(nullptr, owned_export_mesh_);
    owned_export_mesh_ = nullptr;
  }
  export_mesh_ = nullptr;
  loop_to_uv_index_.clear_and_shrink();
  uv_coords_.clear_and_shrink();
  loop_to_normal_index_.clear_and_shrink();
  normal_coords_.clear_and_shrink();
  poly_order_.clear_and_shrink();
  if (poly_smooth_groups_) {
    MEM_freeN(poly_smooth_groups_);
    poly_smooth_groups_ = nullptr;
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

void OBJMesh::set_world_axes_transform(const eIOAxis forward, const eIOAxis up)
{
  float axes_transform[3][3];
  unit_m3(axes_transform);
  /* +Y-forward and +Z-up are the default Blender axis settings. */
  mat3_from_axis_conversion(forward, up, IO_AXIS_Y, IO_AXIS_Z, axes_transform);
  mul_m4_m3m4(world_and_axes_transform_, axes_transform, export_object_eval_.object_to_world);
  /* mul_m4_m3m4 does not transform last row of obmat, i.e. location data. */
  mul_v3_m3v3(
      world_and_axes_transform_[3], axes_transform, export_object_eval_.object_to_world[3]);
  world_and_axes_transform_[3][3] = export_object_eval_.object_to_world[3][3];

  /* Normals need inverse transpose of the regular matrix to handle non-uniform scale. */
  float normal_matrix[3][3];
  copy_m3_m4(normal_matrix, world_and_axes_transform_);
  invert_m3_m3(world_and_axes_normal_transform_, normal_matrix);
  transpose_m3(world_and_axes_normal_transform_);
  mirrored_transform_ = is_negative_m3(world_and_axes_normal_transform_);
}

int OBJMesh::tot_vertices() const
{
  return export_mesh_->totvert;
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
  return export_mesh_->totedge;
}

int16_t OBJMesh::tot_materials() const
{
  return export_mesh_->totcol;
}

int OBJMesh::tot_normal_indices() const
{
  return tot_normal_indices_;
}

int OBJMesh::ith_smooth_group(const int face_index) const
{
  /* Calculate smooth groups first: #OBJMesh::calc_smooth_groups. */
  BLI_assert(tot_smooth_groups_ != -NEGATIVE_INIT);
  BLI_assert(poly_smooth_groups_);
  return poly_smooth_groups_[face_index];
}

void OBJMesh::ensure_mesh_normals() const
{
  /* Constant cast can be removed when calculating face corner normals lazily is possible. */
  BKE_mesh_calc_normals_split(const_cast<Mesh *>(export_mesh_));
}

void OBJMesh::calc_smooth_groups(const bool use_bitflags)
{
  const bool *sharp_edges = static_cast<const bool *>(
      CustomData_get_layer_named(&export_mesh_->edata, CD_PROP_BOOL, "sharp_edge"));
  const bool *sharp_faces = static_cast<const bool *>(
      CustomData_get_layer_named(&export_mesh_->pdata, CD_PROP_BOOL, "sharp_face"));
  poly_smooth_groups_ = BKE_mesh_calc_smoothgroups(mesh_edges_.size(),
                                                   mesh_faces_.data(),
                                                   mesh_faces_.size(),
                                                   export_mesh_->corner_edges().data(),
                                                   export_mesh_->totloop,
                                                   sharp_edges,
                                                   sharp_faces,
                                                   &tot_smooth_groups_,
                                                   use_bitflags);
}

void OBJMesh::calc_poly_order()
{
  const bke::AttributeAccessor attributes = export_mesh_->attributes();
  const VArray<int> material_indices = *attributes.lookup_or_default<int>(
      "material_index", ATTR_DOMAIN_FACE, 0);
  if (material_indices.is_single() && material_indices.get_internal_single() == 0) {
    return;
  }
  const VArraySpan<int> material_indices_span(material_indices);

  poly_order_.resize(material_indices_span.size());
  for (const int i : material_indices_span.index_range()) {
    poly_order_[i] = i;
  }

  /* Sort polygons by their material index. */
  blender::parallel_sort(poly_order_.begin(), poly_order_.end(), [&](int a, int b) {
    int mat_a = material_indices_span[a];
    int mat_b = material_indices_span[b];
    if (mat_a != mat_b) {
      return mat_a < mat_b;
    }
    return a < b;
  });
}

const Material *OBJMesh::get_object_material(const int16_t mat_nr) const
{
  /**
   * The const_cast is safe here because #BKE_object_material_get_eval won't change the object
   * but it is a big can of worms to fix the declaration of that function right now.
   *
   * The call uses "+ 1" as material getter needs one-based indices.
   */
  Object *obj = const_cast<Object *>(&export_object_eval_);
  const Material *r_mat = BKE_object_material_get_eval(obj, mat_nr + 1);
  return r_mat;
}

bool OBJMesh::is_ith_poly_smooth(const int face_index) const
{
  return !sharp_faces_[face_index];
}

const char *OBJMesh::get_object_name() const
{
  return export_object_eval_.id.name + 2;
}

const char *OBJMesh::get_object_mesh_name() const
{
  return export_mesh_->id.name + 2;
}

const char *OBJMesh::get_object_material_name(const int16_t mat_nr) const
{
  const Material *mat = get_object_material(mat_nr);
  if (!mat) {
    return nullptr;
  }
  return mat->id.name + 2;
}

float3 OBJMesh::calc_vertex_coords(const int vert_index, const float global_scale) const
{
  float3 r_coords = mesh_positions_[vert_index];
  mul_m4_v3(world_and_axes_transform_, r_coords);
  mul_v3_fl(r_coords, global_scale);
  return r_coords;
}

Span<int> OBJMesh::calc_poly_vertex_indices(const int face_index) const
{
  return mesh_corner_verts_.slice(mesh_faces_[face_index]);
}

void OBJMesh::store_uv_coords_and_indices()
{
  const StringRef active_uv_name = CustomData_get_active_layer_name(&export_mesh_->ldata,
                                                                    CD_PROP_FLOAT2);
  if (active_uv_name.is_empty()) {
    uv_coords_.clear();
    return;
  }
  const bke::AttributeAccessor attributes = export_mesh_->attributes();
  const VArraySpan uv_map = *attributes.lookup<float2>(active_uv_name, ATTR_DOMAIN_CORNER);
  if (uv_map.is_empty()) {
    uv_coords_.clear();
    return;
  }

  Map<float2, int> uv_to_index;

  /* We don't know how many unique UVs there will be, but this is a guess. */
  uv_to_index.reserve(export_mesh_->totvert);
  uv_coords_.reserve(export_mesh_->totvert);

  loop_to_uv_index_.resize(uv_map.size());

  for (int index = 0; index < int(uv_map.size()); index++) {
    float2 uv = uv_map[index];
    int uv_index = uv_to_index.lookup_default(uv, -1);
    if (uv_index == -1) {
      uv_index = uv_to_index.size();
      uv_to_index.add(uv, uv_index);
      uv_coords_.append(uv);
    }
    loop_to_uv_index_[index] = uv_index;
  }
}

Span<int> OBJMesh::calc_poly_uv_indices(const int face_index) const
{
  if (uv_coords_.is_empty()) {
    return {};
  }
  BLI_assert(face_index < export_mesh_->faces_num);
  return loop_to_uv_index_.as_span().slice(mesh_faces_[face_index]);
}

float3 OBJMesh::calc_poly_normal(const int face_index) const
{
  float3 r_poly_normal = bke::mesh::face_normal_calc(
      mesh_positions_, mesh_corner_verts_.slice(mesh_faces_[face_index]));
  mul_m3_v3(world_and_axes_normal_transform_, r_poly_normal);
  normalize_v3(r_poly_normal);
  return r_poly_normal;
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
  int cur_normal_index = 0;
  Map<float3, int> normal_to_index;
  /* We don't know how many unique normals there will be, but this is a guess. */
  normal_to_index.reserve(export_mesh_->faces_num);
  loop_to_normal_index_.resize(export_mesh_->totloop);
  loop_to_normal_index_.fill(-1);
  const float(*lnors)[3] = static_cast<const float(*)[3]>(
      CustomData_get_layer(&export_mesh_->ldata, CD_NORMAL));
  for (int face_index = 0; face_index < export_mesh_->faces_num; ++face_index) {
    const IndexRange face = mesh_faces_[face_index];
    bool need_per_loop_normals = lnors != nullptr || !(sharp_faces_[face_index]);
    if (need_per_loop_normals) {
      for (const int corner : face) {
        float3 loop_normal;
        BLI_assert(corner < export_mesh_->totloop);
        copy_v3_v3(loop_normal, lnors[corner]);
        mul_m3_v3(world_and_axes_normal_transform_, loop_normal);
        normalize_v3(loop_normal);
        float3 rounded_loop_normal = round_float3_to_n_digits(loop_normal, round_digits);
        int loop_norm_index = normal_to_index.lookup_default(rounded_loop_normal, -1);
        if (loop_norm_index == -1) {
          loop_norm_index = cur_normal_index++;
          normal_to_index.add(rounded_loop_normal, loop_norm_index);
          normal_coords_.append(rounded_loop_normal);
        }
        loop_to_normal_index_[corner] = loop_norm_index;
      }
    }
    else {
      float3 poly_normal = calc_poly_normal(face_index);
      float3 rounded_poly_normal = round_float3_to_n_digits(poly_normal, round_digits);
      int poly_norm_index = normal_to_index.lookup_default(rounded_poly_normal, -1);
      if (poly_norm_index == -1) {
        poly_norm_index = cur_normal_index++;
        normal_to_index.add(rounded_poly_normal, poly_norm_index);
        normal_coords_.append(rounded_poly_normal);
      }
      for (const int corner : face) {
        BLI_assert(corner < export_mesh_->totloop);
        loop_to_normal_index_[corner] = poly_norm_index;
      }
    }
  }
  tot_normal_indices_ = cur_normal_index;
}

Vector<int> OBJMesh::calc_poly_normal_indices(const int face_index) const
{
  if (loop_to_normal_index_.is_empty()) {
    return {};
  }
  const IndexRange face = mesh_faces_[face_index];
  Vector<int> r_poly_normal_indices(face.size());
  for (const int i : IndexRange(face.size())) {
    r_poly_normal_indices[i] = loop_to_normal_index_[face[i]];
  }
  return r_poly_normal_indices;
}

int OBJMesh::tot_deform_groups() const
{
  if (!BKE_object_supports_vertex_groups(&export_object_eval_)) {
    return 0;
  }
  return BKE_object_defgroup_count(&export_object_eval_);
}

int16_t OBJMesh::get_poly_deform_group_index(const int face_index,
                                             MutableSpan<float> group_weights) const
{
  BLI_assert(face_index < export_mesh_->faces_num);
  BLI_assert(group_weights.size() == BKE_object_defgroup_count(&export_object_eval_));
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

const char *OBJMesh::get_poly_deform_group_name(const int16_t def_group_index) const
{
  const bDeformGroup &vertex_group = *(static_cast<bDeformGroup *>(
      BLI_findlink(BKE_object_defgroup_list(&export_object_eval_), def_group_index)));
  return vertex_group.name;
}

}  // namespace blender::io::obj
