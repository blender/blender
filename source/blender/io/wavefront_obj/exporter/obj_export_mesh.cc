/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#include "BKE_attribute.hh"
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
    mesh_polys_ = export_mesh_->polys();
    mesh_loops_ = export_mesh_->loops();
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
  mesh_polys_ = mesh->polys();
  mesh_loops_ = mesh->loops();
}

void OBJMesh::clear()
{
  if (owned_export_mesh_) {
    BKE_id_free(nullptr, owned_export_mesh_);
    owned_export_mesh_ = nullptr;
  }
  export_mesh_ = nullptr;
  uv_indices_.clear_and_shrink();
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
  if (export_mesh_->totpoly <= 0) {
    return;
  }
  const BMeshCreateParams bm_create_params = {false};
  BMeshFromMeshParams bm_convert_params{};
  bm_convert_params.calc_face_normal = true;
  bm_convert_params.calc_vert_normal = true;
  bm_convert_params.add_key_index = false;
  bm_convert_params.use_shapekey = false;

  /* Lower threshold where triangulation of a polygon starts, i.e. a quadrilateral will be
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

int OBJMesh::tot_polygons() const
{
  return export_mesh_->totpoly;
}

int OBJMesh::tot_uv_vertices() const
{
  return tot_uv_vertices_;
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

int OBJMesh::ith_smooth_group(const int poly_index) const
{
  /* Calculate smooth groups first: #OBJMesh::calc_smooth_groups. */
  BLI_assert(tot_smooth_groups_ != -NEGATIVE_INIT);
  BLI_assert(poly_smooth_groups_);
  return poly_smooth_groups_[poly_index];
}

void OBJMesh::ensure_mesh_normals() const
{
  /* Const cast can be removed when calculating face corner normals lazily is possible. */
  BKE_mesh_calc_normals_split(const_cast<Mesh *>(export_mesh_));
}

void OBJMesh::calc_smooth_groups(const bool use_bitflags)
{
  poly_smooth_groups_ = BKE_mesh_calc_smoothgroups(mesh_edges_.data(),
                                                   mesh_edges_.size(),
                                                   mesh_polys_.data(),
                                                   mesh_polys_.size(),
                                                   mesh_loops_.data(),
                                                   mesh_loops_.size(),
                                                   &tot_smooth_groups_,
                                                   use_bitflags);
}

void OBJMesh::calc_poly_order()
{
  const bke::AttributeAccessor attributes = export_mesh_->attributes();
  const VArray<int> material_indices = attributes.lookup_or_default<int>(
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

bool OBJMesh::is_ith_poly_smooth(const int poly_index) const
{
  return mesh_polys_[poly_index].flag & ME_SMOOTH;
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

Vector<int> OBJMesh::calc_poly_vertex_indices(const int poly_index) const
{
  const MPoly &mpoly = mesh_polys_[poly_index];
  const MLoop *mloop = &mesh_loops_[mpoly.loopstart];
  const int totloop = mpoly.totloop;
  Vector<int> r_poly_vertex_indices(totloop);
  for (int loop_index = 0; loop_index < totloop; loop_index++) {
    r_poly_vertex_indices[loop_index] = mloop[loop_index].v;
  }
  return r_poly_vertex_indices;
}

void OBJMesh::store_uv_coords_and_indices()
{
  const int totvert = export_mesh_->totvert;
  const StringRef active_uv_name = CustomData_get_active_layer_name(&export_mesh_->ldata,
                                                                    CD_PROP_FLOAT2);
  if (active_uv_name.is_empty()) {
    tot_uv_vertices_ = 0;
    return;
  }
  const bke::AttributeAccessor attributes = export_mesh_->attributes();
  const VArraySpan<float2> uv_map = attributes.lookup<float2>(active_uv_name, ATTR_DOMAIN_CORNER);

  const float limit[2] = {STD_UV_CONNECT_LIMIT, STD_UV_CONNECT_LIMIT};

  UvVertMap *uv_vert_map = BKE_mesh_uv_vert_map_create(
      mesh_polys_.data(),
      nullptr,
      nullptr,
      mesh_loops_.data(),
      reinterpret_cast<const float(*)[2]>(uv_map.data()),
      mesh_polys_.size(),
      totvert,
      limit,
      false,
      false);

  uv_indices_.resize(mesh_polys_.size());
  /* At least total vertices of a mesh will be present in its texture map. So
   * reserve minimum space early. */
  uv_coords_.reserve(totvert);

  tot_uv_vertices_ = 0;
  for (int vertex_index = 0; vertex_index < totvert; vertex_index++) {
    const UvMapVert *uv_vert = BKE_mesh_uv_vert_map_get_vert(uv_vert_map, vertex_index);
    for (; uv_vert; uv_vert = uv_vert->next) {
      if (uv_vert->separate) {
        tot_uv_vertices_ += 1;
      }
      const int verts_in_poly = mesh_polys_[uv_vert->poly_index].totloop;

      /* Store UV vertex coordinates. */
      uv_coords_.resize(tot_uv_vertices_);
      const int loopstart = mesh_polys_[uv_vert->poly_index].loopstart;
      Span<float> vert_uv_coords(uv_map[loopstart + uv_vert->loop_of_poly_index], 2);
      uv_coords_[tot_uv_vertices_ - 1] = float2(vert_uv_coords[0], vert_uv_coords[1]);

      /* Store UV vertex indices. */
      uv_indices_[uv_vert->poly_index].resize(verts_in_poly);
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
  BLI_assert(poly_index < export_mesh_->totpoly);
  BLI_assert(poly_index < uv_indices_.size());
  return uv_indices_[poly_index];
}

float3 OBJMesh::calc_poly_normal(const int poly_index) const
{
  float3 r_poly_normal;
  const MPoly &poly = mesh_polys_[poly_index];
  BKE_mesh_calc_poly_normal(&poly,
                            &mesh_loops_[poly.loopstart],
                            reinterpret_cast<const float(*)[3]>(mesh_positions_.data()),
                            r_poly_normal);
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
  normal_to_index.reserve(export_mesh_->totpoly);
  loop_to_normal_index_.resize(export_mesh_->totloop);
  loop_to_normal_index_.fill(-1);
  const float(*lnors)[3] = static_cast<const float(*)[3]>(
      CustomData_get_layer(&export_mesh_->ldata, CD_NORMAL));
  for (int poly_index = 0; poly_index < export_mesh_->totpoly; ++poly_index) {
    const MPoly &mpoly = mesh_polys_[poly_index];
    bool need_per_loop_normals = lnors != nullptr || (mpoly.flag & ME_SMOOTH);
    if (need_per_loop_normals) {
      for (int loop_of_poly = 0; loop_of_poly < mpoly.totloop; ++loop_of_poly) {
        float3 loop_normal;
        int loop_index = mpoly.loopstart + loop_of_poly;
        BLI_assert(loop_index < export_mesh_->totloop);
        copy_v3_v3(loop_normal, lnors[loop_index]);
        mul_m3_v3(world_and_axes_normal_transform_, loop_normal);
        normalize_v3(loop_normal);
        float3 rounded_loop_normal = round_float3_to_n_digits(loop_normal, round_digits);
        int loop_norm_index = normal_to_index.lookup_default(rounded_loop_normal, -1);
        if (loop_norm_index == -1) {
          loop_norm_index = cur_normal_index++;
          normal_to_index.add(rounded_loop_normal, loop_norm_index);
          normal_coords_.append(rounded_loop_normal);
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
        normal_coords_.append(rounded_poly_normal);
      }
      for (int i = 0; i < mpoly.totloop; ++i) {
        int loop_index = mpoly.loopstart + i;
        BLI_assert(loop_index < export_mesh_->totloop);
        loop_to_normal_index_[loop_index] = poly_norm_index;
      }
    }
  }
  tot_normal_indices_ = cur_normal_index;
}

Vector<int> OBJMesh::calc_poly_normal_indices(const int poly_index) const
{
  if (loop_to_normal_index_.is_empty()) {
    return {};
  }
  const MPoly &mpoly = mesh_polys_[poly_index];
  const int totloop = mpoly.totloop;
  Vector<int> r_poly_normal_indices(totloop);
  for (int poly_loop_index = 0; poly_loop_index < totloop; poly_loop_index++) {
    int loop_index = mpoly.loopstart + poly_loop_index;
    r_poly_normal_indices[poly_loop_index] = loop_to_normal_index_[loop_index];
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

int16_t OBJMesh::get_poly_deform_group_index(const int poly_index,
                                             MutableSpan<float> group_weights) const
{
  BLI_assert(poly_index < export_mesh_->totpoly);
  BLI_assert(group_weights.size() == BKE_object_defgroup_count(&export_object_eval_));
  const Span<MDeformVert> dverts = export_mesh_->deform_verts();
  if (dverts.is_empty()) {
    return NOT_FOUND;
  }

  group_weights.fill(0);
  bool found_any_group = false;
  const MPoly &mpoly = mesh_polys_[poly_index];
  const MLoop *mloop = &mesh_loops_[mpoly.loopstart];
  for (int loop_i = 0; loop_i < mpoly.totloop; ++loop_i, ++mloop) {
    const MDeformVert &dv = dverts[mloop->v];
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
