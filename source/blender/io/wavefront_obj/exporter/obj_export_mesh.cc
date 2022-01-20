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

/** \file
 * \ingroup obj
 */

#include "BKE_customdata.h"
#include "BKE_deform.h"
#include "BKE_lib_id.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_object.h"

#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_math.h"

#include "DEG_depsgraph_query.h"

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "obj_export_mesh.hh"

namespace blender::io::obj {
OBJMesh::OBJMesh(Depsgraph *depsgraph, const OBJExportParams &export_params, Object *mesh_object)
{
  export_object_eval_ = DEG_get_evaluated_object(depsgraph, mesh_object);
  export_mesh_eval_ = BKE_object_get_evaluated_mesh(export_object_eval_);
  mesh_eval_needs_free_ = false;

  if (!export_mesh_eval_) {
    /* Curves and NURBS surfaces need a new mesh when they're
     * exported in the form of vertices and edges.
     */
    export_mesh_eval_ = BKE_mesh_new_from_object(depsgraph, export_object_eval_, true, true);
    /* Since a new mesh been allocated, it needs to be freed in the destructor. */
    mesh_eval_needs_free_ = true;
  }
  if (export_params.export_triangulated_mesh &&
      ELEM(export_object_eval_->type, OB_MESH, OB_SURF)) {
    std::tie(export_mesh_eval_, mesh_eval_needs_free_) = triangulate_mesh_eval();
  }
  set_world_axes_transform(export_params.forward_axis, export_params.up_axis);
}

/**
 * Free new meshes allocated for triangulated meshes, or Curve converted to Mesh.
 */
OBJMesh::~OBJMesh()
{
  free_mesh_if_needed();
  if (poly_smooth_groups_) {
    MEM_freeN(poly_smooth_groups_);
  }
}

void OBJMesh::free_mesh_if_needed()
{
  if (mesh_eval_needs_free_ && export_mesh_eval_) {
    BKE_id_free(nullptr, export_mesh_eval_);
  }
}

std::pair<Mesh *, bool> OBJMesh::triangulate_mesh_eval()
{
  if (export_mesh_eval_->totpoly <= 0) {
    return {export_mesh_eval_, false};
  }
  const struct BMeshCreateParams bm_create_params = {0u};
  const struct BMeshFromMeshParams bm_convert_params = {1u, 0, 0, 0};
  /* Lower threshold where triangulation of a polygon starts, i.e. a quadrilateral will be
   * triangulated here. */
  const int triangulate_min_verts = 4;

  unique_bmesh_ptr bmesh(
      BKE_mesh_to_bmesh_ex(export_mesh_eval_, &bm_create_params, &bm_convert_params));
  BM_mesh_triangulate(bmesh.get(),
                      MOD_TRIANGULATE_NGON_BEAUTY,
                      MOD_TRIANGULATE_QUAD_SHORTEDGE,
                      triangulate_min_verts,
                      false,
                      nullptr,
                      nullptr,
                      nullptr);

  Mesh *triangulated = BKE_mesh_from_bmesh_for_eval_nomain(
      bmesh.get(), nullptr, export_mesh_eval_);
  free_mesh_if_needed();
  return {triangulated, true};
}

void OBJMesh::set_world_axes_transform(const eTransformAxisForward forward,
                                       const eTransformAxisUp up)
{
  float axes_transform[3][3];
  unit_m3(axes_transform);
  /* +Y-forward and +Z-up are the default Blender axis settings. */
  mat3_from_axis_conversion(OBJ_AXIS_Y_FORWARD, OBJ_AXIS_Z_UP, forward, up, axes_transform);
  /* mat3_from_axis_conversion returns a transposed matrix! */
  transpose_m3(axes_transform);
  mul_m4_m3m4(world_and_axes_transform_, axes_transform, export_object_eval_->obmat);
  /* mul_m4_m3m4 does not transform last row of obmat, i.e. location data. */
  mul_v3_m3v3(world_and_axes_transform_[3], axes_transform, export_object_eval_->obmat[3]);
  world_and_axes_transform_[3][3] = export_object_eval_->obmat[3][3];
}

int OBJMesh::tot_vertices() const
{
  return export_mesh_eval_->totvert;
}

int OBJMesh::tot_polygons() const
{
  return export_mesh_eval_->totpoly;
}

int OBJMesh::tot_uv_vertices() const
{
  return tot_uv_vertices_;
}

int OBJMesh::tot_edges() const
{
  return export_mesh_eval_->totedge;
}

int16_t OBJMesh::tot_materials() const
{
  return export_mesh_eval_->totcol;
}

int OBJMesh::tot_normal_indices() const
{
  return tot_normal_indices_;
}

int OBJMesh::ith_smooth_group(const int poly_index) const
{
  /* Calculate smooth groups first: #OBJMesh::calc_smooth_groups. */
  BLI_assert(tot_smooth_groups_ != -NEGATIVE_INIT);
  BLI_assert(poly_smooth_groups_);
  return poly_smooth_groups_[poly_index];
}

void OBJMesh::ensure_mesh_normals() const
{
  BKE_mesh_calc_normals_split(export_mesh_eval_);
}

void OBJMesh::ensure_mesh_edges() const
{
  BKE_mesh_calc_edges(export_mesh_eval_, true, false);
  BKE_mesh_calc_edges_loose(export_mesh_eval_);
}

void OBJMesh::calc_smooth_groups(const bool use_bitflags)
{
  poly_smooth_groups_ = BKE_mesh_calc_smoothgroups(export_mesh_eval_->medge,
                                                   export_mesh_eval_->totedge,
                                                   export_mesh_eval_->mpoly,
                                                   export_mesh_eval_->totpoly,
                                                   export_mesh_eval_->mloop,
                                                   export_mesh_eval_->totloop,
                                                   &tot_smooth_groups_,
                                                   use_bitflags);
}

const Material *OBJMesh::get_object_material(const int16_t mat_nr) const
{
  /* "+ 1" as material getter needs one-based indices.  */
  const Material *r_mat = BKE_object_material_get(export_object_eval_, mat_nr + 1);
#ifdef DEBUG
  if (!r_mat) {
    std::cerr << "Material not found for mat_nr = " << mat_nr << std::endl;
  }
#endif
  return r_mat;
}

bool OBJMesh::is_ith_poly_smooth(const int poly_index) const
{
  return export_mesh_eval_->mpoly[poly_index].flag & ME_SMOOTH;
}

int16_t OBJMesh::ith_poly_matnr(const int poly_index) const
{
  BLI_assert(poly_index < export_mesh_eval_->totpoly);
  const int16_t r_mat_nr = export_mesh_eval_->mpoly[poly_index].mat_nr;
  return r_mat_nr >= 0 ? r_mat_nr : NOT_FOUND;
}

const char *OBJMesh::get_object_name() const
{
  return export_object_eval_->id.name + 2;
}

const char *OBJMesh::get_object_mesh_name() const
{
  return export_mesh_eval_->id.name + 2;
}

const char *OBJMesh::get_object_material_name(const int16_t mat_nr) const
{
  const Material *mat = get_object_material(mat_nr);
  if (!mat) {
    return nullptr;
  }
  return mat->id.name + 2;
}

float3 OBJMesh::calc_vertex_coords(const int vert_index, const float scaling_factor) const
{
  float3 r_coords;
  copy_v3_v3(r_coords, export_mesh_eval_->mvert[vert_index].co);
  mul_v3_fl(r_coords, scaling_factor);
  mul_m4_v3(world_and_axes_transform_, r_coords);
  return r_coords;
}

Vector<int> OBJMesh::calc_poly_vertex_indices(const int poly_index) const
{
  const MPoly &mpoly = export_mesh_eval_->mpoly[poly_index];
  const MLoop *mloop = &export_mesh_eval_->mloop[mpoly.loopstart];
  const int totloop = mpoly.totloop;
  Vector<int> r_poly_vertex_indices(totloop);
  for (int loop_index = 0; loop_index < totloop; loop_index++) {
    r_poly_vertex_indices[loop_index] = mloop[loop_index].v;
  }
  return r_poly_vertex_indices;
}

void OBJMesh::store_uv_coords_and_indices(Vector<std::array<float, 2>> &r_uv_coords)
{
  const MPoly *mpoly = export_mesh_eval_->mpoly;
  const MLoop *mloop = export_mesh_eval_->mloop;
  const int totpoly = export_mesh_eval_->totpoly;
  const int totvert = export_mesh_eval_->totvert;
  const MLoopUV *mloopuv = static_cast<MLoopUV *>(
      CustomData_get_layer(&export_mesh_eval_->ldata, CD_MLOOPUV));
  if (!mloopuv) {
    tot_uv_vertices_ = 0;
    return;
  }
  const float limit[2] = {STD_UV_CONNECT_LIMIT, STD_UV_CONNECT_LIMIT};

  UvVertMap *uv_vert_map = BKE_mesh_uv_vert_map_create(
      mpoly, mloop, mloopuv, totpoly, totvert, limit, false, false);

  uv_indices_.resize(totpoly);
  /* At least total vertices of a mesh will be present in its texture map. So
   * reserve minimum space early. */
  r_uv_coords.reserve(totvert);

  tot_uv_vertices_ = 0;
  for (int vertex_index = 0; vertex_index < totvert; vertex_index++) {
    const UvMapVert *uv_vert = BKE_mesh_uv_vert_map_get_vert(uv_vert_map, vertex_index);
    for (; uv_vert; uv_vert = uv_vert->next) {
      if (uv_vert->separate) {
        tot_uv_vertices_ += 1;
      }
      const int vertices_in_poly = mpoly[uv_vert->poly_index].totloop;

      /* Store UV vertex coordinates. */
      r_uv_coords.resize(tot_uv_vertices_);
      const int loopstart = mpoly[uv_vert->poly_index].loopstart;
      Span<float> vert_uv_coords(mloopuv[loopstart + uv_vert->loop_of_poly_index].uv, 2);
      r_uv_coords[tot_uv_vertices_ - 1][0] = vert_uv_coords[0];
      r_uv_coords[tot_uv_vertices_ - 1][1] = vert_uv_coords[1];

      /* Store UV vertex indices. */
      uv_indices_[uv_vert->poly_index].resize(vertices_in_poly);
      /* Keep indices zero-based and let the writer handle the "+ 1" as per OBJ spec. */
      uv_indices_[uv_vert->poly_index][uv_vert->loop_of_poly_index] = tot_uv_vertices_ - 1;
    }
  }
  BKE_mesh_uv_vert_map_free(uv_vert_map);
}

Span<int> OBJMesh::calc_poly_uv_indices(const int poly_index) const
{
  if (uv_indices_.size() <= 0) {
    return {};
  }
  BLI_assert(poly_index < export_mesh_eval_->totpoly);
  BLI_assert(poly_index < uv_indices_.size());
  return uv_indices_[poly_index];
}

float3 OBJMesh::calc_poly_normal(const int poly_index) const
{
  float3 r_poly_normal;
  const MPoly &poly = export_mesh_eval_->mpoly[poly_index];
  const MLoop &mloop = export_mesh_eval_->mloop[poly.loopstart];
  const MVert &mvert = *(export_mesh_eval_->mvert);
  BKE_mesh_calc_poly_normal(&poly, &mloop, &mvert, r_poly_normal);
  mul_mat3_m4_v3(world_and_axes_transform_, r_poly_normal);
  return r_poly_normal;
}

/** Round \a f to \a round_digits decimal digits. */
static float round_float_to_n_digits(const float f, int round_digits)
{
  float scale = powf(10.0, round_digits);
  return ceilf((scale * f - 0.49999999f)) / scale;
}

static float3 round_float3_to_n_digits(const float3 &v, int round_digits)
{
  float3 ans;
  ans.x = round_float_to_n_digits(v.x, round_digits);
  ans.y = round_float_to_n_digits(v.y, round_digits);
  ans.z = round_float_to_n_digits(v.z, round_digits);
  return ans;
}

void OBJMesh::store_normal_coords_and_indices(Vector<float3> &r_normal_coords)
{
  /* We'll round normal components to 4 digits.
   * This will cover up some minor differences
   * between floating point calculations on different platforms.
   * Since normals are normalized, there will be no perceptible loss
   * of precision when rounding to 4 digits. */
  constexpr int round_digits = 4;
  int cur_normal_index = 0;
  Map<float3, int> normal_to_index;
  /* We don't know how many unique normals there will be, but this is a guess.*/
  normal_to_index.reserve(export_mesh_eval_->totpoly);
  loop_to_normal_index_.resize(export_mesh_eval_->totloop);
  loop_to_normal_index_.fill(-1);
  const float(
      *lnors)[3] = (const float(*)[3])(CustomData_get_layer(&export_mesh_eval_->ldata, CD_NORMAL));
  for (int poly_index = 0; poly_index < export_mesh_eval_->totpoly; ++poly_index) {
    const MPoly &mpoly = export_mesh_eval_->mpoly[poly_index];
    bool need_per_loop_normals = is_ith_poly_smooth(poly_index);
    if (need_per_loop_normals) {
      for (int loop_of_poly = 0; loop_of_poly < mpoly.totloop; ++loop_of_poly) {
        float3 loop_normal;
        int loop_index = mpoly.loopstart + loop_of_poly;
        BLI_assert(loop_index < export_mesh_eval_->totloop);
        copy_v3_v3(loop_normal, lnors[loop_index]);
        mul_mat3_m4_v3(world_and_axes_transform_, loop_normal);
        float3 rounded_loop_normal = round_float3_to_n_digits(loop_normal, round_digits);
        int loop_norm_index = normal_to_index.lookup_default(rounded_loop_normal, -1);
        if (loop_norm_index == -1) {
          loop_norm_index = cur_normal_index++;
          normal_to_index.add(rounded_loop_normal, loop_norm_index);
          r_normal_coords.append(rounded_loop_normal);
        }
        loop_to_normal_index_[loop_index] = loop_norm_index;
      }
    }
    else {
      float3 poly_normal = calc_poly_normal(poly_index);
      float3 rounded_poly_normal = round_float3_to_n_digits(poly_normal, round_digits);
      int poly_norm_index = normal_to_index.lookup_default(rounded_poly_normal, -1);
      if (poly_norm_index == -1) {
        poly_norm_index = cur_normal_index++;
        normal_to_index.add(rounded_poly_normal, poly_norm_index);
        r_normal_coords.append(rounded_poly_normal);
      }
      for (int i = 0; i < mpoly.totloop; ++i) {
        int loop_index = mpoly.loopstart + i;
        BLI_assert(loop_index < export_mesh_eval_->totloop);
        loop_to_normal_index_[loop_index] = poly_norm_index;
      }
    }
  }
  tot_normal_indices_ = cur_normal_index;
}

Vector<int> OBJMesh::calc_poly_normal_indices(const int poly_index) const
{
  const MPoly &mpoly = export_mesh_eval_->mpoly[poly_index];
  const int totloop = mpoly.totloop;
  Vector<int> r_poly_normal_indices(totloop);
  for (int poly_loop_index = 0; poly_loop_index < totloop; poly_loop_index++) {
    int loop_index = mpoly.loopstart + poly_loop_index;
    r_poly_normal_indices[poly_loop_index] = loop_to_normal_index_[loop_index];
  }
  return r_poly_normal_indices;
}

int16_t OBJMesh::get_poly_deform_group_index(const int poly_index) const
{
  BLI_assert(poly_index < export_mesh_eval_->totpoly);
  const MPoly &mpoly = export_mesh_eval_->mpoly[poly_index];
  const MLoop *mloop = &export_mesh_eval_->mloop[mpoly.loopstart];
  const Object *obj = export_object_eval_;
  const int tot_deform_groups = BKE_object_defgroup_count(obj);
  /* Indices of the vector index into deform groups of an object; values are the]
   * number of vertex members in one deform group. */
  Vector<int16_t> deform_group_members(tot_deform_groups, 0);
  /* Whether at least one vertex in the polygon belongs to any group. */
  bool found_group = false;

  const MDeformVert *dvert_orig = static_cast<MDeformVert *>(
      CustomData_get_layer(&export_mesh_eval_->vdata, CD_MDEFORMVERT));
  if (!dvert_orig) {
    return NOT_FOUND;
  }

  const MDeformWeight *curr_weight = nullptr;
  const MDeformVert *dvert = nullptr;
  for (int loop_index = 0; loop_index < mpoly.totloop; loop_index++) {
    dvert = &dvert_orig[(mloop + loop_index)->v];
    curr_weight = dvert->dw;
    if (curr_weight) {
      bDeformGroup *vertex_group = static_cast<bDeformGroup *>(
          BLI_findlink(BKE_object_defgroup_list(obj), curr_weight->def_nr));
      if (vertex_group) {
        deform_group_members[curr_weight->def_nr] += 1;
        found_group = true;
      }
    }
  }

  if (!found_group) {
    return NOT_FOUND;
  }
  /* Index of the group with maximum vertices. */
  int16_t max_idx = std::max_element(deform_group_members.begin(), deform_group_members.end()) -
                    deform_group_members.begin();
  return max_idx;
}

const char *OBJMesh::get_poly_deform_group_name(const int16_t def_group_index) const
{
  const bDeformGroup &vertex_group = *(static_cast<bDeformGroup *>(
      BLI_findlink(BKE_object_defgroup_list(export_object_eval_), def_group_index)));
  return vertex_group.name;
}

std::optional<std::array<int, 2>> OBJMesh::calc_loose_edge_vert_indices(const int edge_index) const
{
  const MEdge &edge = export_mesh_eval_->medge[edge_index];
  if (edge.flag & ME_LOOSEEDGE) {
    return std::array<int, 2>{static_cast<int>(edge.v1), static_cast<int>(edge.v2)};
  }
  return std::nullopt;
}
}  // namespace blender::io::obj
