/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup draw
 *
 * \brief Extraction of Mesh data into VBO to feed to GPU.
 */

#include "MEM_guardedalloc.h"

#include "BLI_array.hh"
#include "BLI_bitmap.h"
#include "BLI_math.h"
#include "BLI_task.h"

#include "BKE_attribute.hh"
#include "BKE_editmesh.h"
#include "BKE_editmesh_cache.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"

#include "GPU_batch.h"

#include "ED_mesh.h"

#include "mesh_extractors/extract_mesh.hh"

/* ---------------------------------------------------------------------- */
/** \name Update Loose Geometry
 * \{ */

static void mesh_render_data_lverts_bm(const MeshRenderData *mr,
                                       MeshBufferCache *cache,
                                       BMesh *bm);
static void mesh_render_data_ledges_bm(const MeshRenderData *mr,
                                       MeshBufferCache *cache,
                                       BMesh *bm);
static void mesh_render_data_loose_geom_mesh(const MeshRenderData *mr, MeshBufferCache *cache);
static void mesh_render_data_loose_geom_build(const MeshRenderData *mr, MeshBufferCache *cache);

static void mesh_render_data_loose_geom_load(MeshRenderData *mr, MeshBufferCache *cache)
{
  mr->ledges = cache->loose_geom.edges;
  mr->lverts = cache->loose_geom.verts;
  mr->vert_loose_len = cache->loose_geom.vert_len;
  mr->edge_loose_len = cache->loose_geom.edge_len;

  mr->loop_loose_len = mr->vert_loose_len + (mr->edge_loose_len * 2);
}

static void mesh_render_data_loose_geom_ensure(const MeshRenderData *mr, MeshBufferCache *cache)
{
  /* Early exit: Are loose geometry already available.
   * Only checking for loose verts as loose edges and verts are calculated at the same time. */
  if (cache->loose_geom.verts) {
    return;
  }
  mesh_render_data_loose_geom_build(mr, cache);
}

static void mesh_render_data_loose_geom_build(const MeshRenderData *mr, MeshBufferCache *cache)
{
  cache->loose_geom.vert_len = 0;
  cache->loose_geom.edge_len = 0;

  if (mr->extract_type != MR_EXTRACT_BMESH) {
    /* Mesh */
    mesh_render_data_loose_geom_mesh(mr, cache);
  }
  else {
    /* #BMesh */
    BMesh *bm = mr->bm;
    mesh_render_data_lverts_bm(mr, cache, bm);
    mesh_render_data_ledges_bm(mr, cache, bm);
  }
}

static void mesh_render_data_loose_geom_mesh(const MeshRenderData *mr, MeshBufferCache *cache)
{
  using namespace blender;
  BLI_bitmap *lvert_map = BLI_BITMAP_NEW(mr->vert_len, __func__);

  const bke::LooseEdgeCache &loose_edges = mr->me->loose_edges();
  if (loose_edges.count > 0) {
    cache->loose_geom.edges = static_cast<int *>(
        MEM_malloc_arrayN(loose_edges.count, sizeof(int), __func__));

    cache->loose_geom.edge_len = 0;
    for (const int64_t i : loose_edges.is_loose_bits.index_range()) {
      if (loose_edges.is_loose_bits[i]) {
        cache->loose_geom.edges[cache->loose_geom.edge_len] = int(i);
        cache->loose_geom.edge_len++;
      }
    }
  }

  /* Tag verts as not loose. */
  const Span<MEdge> edges(mr->medge, mr->edge_len);
  for (const MEdge &edge : edges) {
    BLI_BITMAP_ENABLE(lvert_map, edge.v1);
    BLI_BITMAP_ENABLE(lvert_map, edge.v2);
  }

  cache->loose_geom.verts = static_cast<int *>(
      MEM_mallocN(mr->vert_len * sizeof(*cache->loose_geom.verts), __func__));
  for (int v = 0; v < mr->vert_len; v++) {
    if (!BLI_BITMAP_TEST(lvert_map, v)) {
      cache->loose_geom.verts[cache->loose_geom.vert_len++] = v;
    }
  }
  if (cache->loose_geom.vert_len < mr->vert_len) {
    cache->loose_geom.verts = static_cast<int *>(MEM_reallocN(
        cache->loose_geom.verts, cache->loose_geom.vert_len * sizeof(*cache->loose_geom.verts)));
  }

  MEM_freeN(lvert_map);
}

static void mesh_render_data_lverts_bm(const MeshRenderData *mr, MeshBufferCache *cache, BMesh *bm)
{
  int elem_id;
  BMIter iter;
  BMVert *eve;
  cache->loose_geom.verts = static_cast<int *>(
      MEM_mallocN(mr->vert_len * sizeof(*cache->loose_geom.verts), __func__));
  BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, elem_id) {
    if (eve->e == nullptr) {
      cache->loose_geom.verts[cache->loose_geom.vert_len++] = elem_id;
    }
  }
  if (cache->loose_geom.vert_len < mr->vert_len) {
    cache->loose_geom.verts = static_cast<int *>(MEM_reallocN(
        cache->loose_geom.verts, cache->loose_geom.vert_len * sizeof(*cache->loose_geom.verts)));
  }
}

static void mesh_render_data_ledges_bm(const MeshRenderData *mr, MeshBufferCache *cache, BMesh *bm)
{
  int elem_id;
  BMIter iter;
  BMEdge *ede;
  cache->loose_geom.edges = static_cast<int *>(
      MEM_mallocN(mr->edge_len * sizeof(*cache->loose_geom.edges), __func__));
  BM_ITER_MESH_INDEX (ede, &iter, bm, BM_EDGES_OF_MESH, elem_id) {
    if (ede->l == nullptr) {
      cache->loose_geom.edges[cache->loose_geom.edge_len++] = elem_id;
    }
  }
  if (cache->loose_geom.edge_len < mr->edge_len) {
    cache->loose_geom.edges = static_cast<int *>(MEM_reallocN(
        cache->loose_geom.edges, cache->loose_geom.edge_len * sizeof(*cache->loose_geom.edges)));
  }
}

void mesh_render_data_update_loose_geom(MeshRenderData *mr,
                                        MeshBufferCache *cache,
                                        const eMRIterType iter_type,
                                        const eMRDataType data_flag)
{
  if ((iter_type & (MR_ITER_LEDGE | MR_ITER_LVERT)) || (data_flag & MR_DATA_LOOSE_GEOM)) {
    mesh_render_data_loose_geom_ensure(mr, cache);
    mesh_render_data_loose_geom_load(mr, cache);
  }
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Polygons sorted per material
 *
 * Contains polygon indices sorted based on their material.
 * \{ */

static void mesh_render_data_polys_sorted_load(MeshRenderData *mr, const MeshBufferCache *cache);
static void mesh_render_data_polys_sorted_ensure(MeshRenderData *mr, MeshBufferCache *cache);
static void mesh_render_data_polys_sorted_build(MeshRenderData *mr, MeshBufferCache *cache);
static int *mesh_render_data_mat_tri_len_build(MeshRenderData *mr);

void mesh_render_data_update_polys_sorted(MeshRenderData *mr,
                                          MeshBufferCache *cache,
                                          const eMRDataType data_flag)
{
  if (data_flag & MR_DATA_POLYS_SORTED) {
    mesh_render_data_polys_sorted_ensure(mr, cache);
    mesh_render_data_polys_sorted_load(mr, cache);
  }
}

static void mesh_render_data_polys_sorted_load(MeshRenderData *mr, const MeshBufferCache *cache)
{
  mr->poly_sorted.tri_first_index = cache->poly_sorted.tri_first_index;
  mr->poly_sorted.mat_tri_len = cache->poly_sorted.mat_tri_len;
  mr->poly_sorted.visible_tri_len = cache->poly_sorted.visible_tri_len;
}

static void mesh_render_data_polys_sorted_ensure(MeshRenderData *mr, MeshBufferCache *cache)
{
  if (cache->poly_sorted.tri_first_index) {
    return;
  }
  mesh_render_data_polys_sorted_build(mr, cache);
}

static void mesh_render_data_polys_sorted_build(MeshRenderData *mr, MeshBufferCache *cache)
{
  int *tri_first_index = static_cast<int *>(
      MEM_mallocN(sizeof(*tri_first_index) * mr->poly_len, __func__));
  int *mat_tri_len = mesh_render_data_mat_tri_len_build(mr);

  /* Apply offset. */
  int visible_tri_len = 0;
  blender::Array<int, 32> mat_tri_offs(mr->mat_len);
  {
    for (int i = 0; i < mr->mat_len; i++) {
      mat_tri_offs[i] = visible_tri_len;
      visible_tri_len += mat_tri_len[i];
    }
  }

  /* Sort per material. */
  int mat_last = mr->mat_len - 1;
  if (mr->extract_type == MR_EXTRACT_BMESH) {
    BMIter iter;
    BMFace *f;
    int i;
    BM_ITER_MESH_INDEX (f, &iter, mr->bm, BM_FACES_OF_MESH, i) {
      if (!BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
        const int mat = min_ii(f->mat_nr, mat_last);
        tri_first_index[i] = mat_tri_offs[mat];
        mat_tri_offs[mat] += f->len - 2;
      }
      else {
        tri_first_index[i] = -1;
      }
    }
  }
  else {
    for (int i = 0; i < mr->poly_len; i++) {
      if (!(mr->use_hide && mr->hide_poly && mr->hide_poly[i])) {
        const MPoly *mp = &mr->mpoly[i];
        const int mat = min_ii(mr->material_indices ? mr->material_indices[i] : 0, mat_last);
        tri_first_index[i] = mat_tri_offs[mat];
        mat_tri_offs[mat] += mp->totloop - 2;
      }
      else {
        tri_first_index[i] = -1;
      }
    }
  }

  cache->poly_sorted.tri_first_index = tri_first_index;
  cache->poly_sorted.mat_tri_len = mat_tri_len;
  cache->poly_sorted.visible_tri_len = visible_tri_len;
}

static void mesh_render_data_mat_tri_len_bm_range_fn(void *__restrict userdata,
                                                     const int iter,
                                                     const TaskParallelTLS *__restrict tls)
{
  MeshRenderData *mr = static_cast<MeshRenderData *>(userdata);
  int *mat_tri_len = static_cast<int *>(tls->userdata_chunk);

  BMesh *bm = mr->bm;
  BMFace *efa = BM_face_at_index(bm, iter);
  if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
    int mat = min_ii(efa->mat_nr, mr->mat_len - 1);
    mat_tri_len[mat] += efa->len - 2;
  }
}

static void mesh_render_data_mat_tri_len_mesh_range_fn(void *__restrict userdata,
                                                       const int iter,
                                                       const TaskParallelTLS *__restrict tls)
{
  MeshRenderData *mr = static_cast<MeshRenderData *>(userdata);
  int *mat_tri_len = static_cast<int *>(tls->userdata_chunk);

  const MPoly *mp = &mr->mpoly[iter];
  if (!(mr->use_hide && mr->hide_poly && mr->hide_poly[iter])) {
    int mat = min_ii(mr->material_indices ? mr->material_indices[iter] : 0, mr->mat_len - 1);
    mat_tri_len[mat] += mp->totloop - 2;
  }
}

static void mesh_render_data_mat_tri_len_reduce_fn(const void *__restrict userdata,
                                                   void *__restrict chunk_join,
                                                   void *__restrict chunk)
{
  const MeshRenderData *mr = static_cast<const MeshRenderData *>(userdata);
  int *dst_mat_len = static_cast<int *>(chunk_join);
  int *src_mat_len = static_cast<int *>(chunk);
  for (int i = 0; i < mr->mat_len; i++) {
    dst_mat_len[i] += src_mat_len[i];
  }
}

static int *mesh_render_data_mat_tri_len_build_threaded(MeshRenderData *mr,
                                                        int face_len,
                                                        TaskParallelRangeFunc range_func)
{
  /* Extending the #MatOffsetUserData with an int per material slot. */
  size_t mat_tri_len_size = sizeof(int) * mr->mat_len;
  int *mat_tri_len = static_cast<int *>(MEM_callocN(mat_tri_len_size, __func__));

  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.userdata_chunk = mat_tri_len;
  settings.userdata_chunk_size = mat_tri_len_size;
  settings.min_iter_per_thread = MIN_RANGE_LEN;
  settings.func_reduce = mesh_render_data_mat_tri_len_reduce_fn;
  BLI_task_parallel_range(0, face_len, mr, range_func, &settings);

  return mat_tri_len;
}

/* Count how many triangles for each material. */
static int *mesh_render_data_mat_tri_len_build(MeshRenderData *mr)
{
  if (mr->extract_type == MR_EXTRACT_BMESH) {
    BMesh *bm = mr->bm;
    return mesh_render_data_mat_tri_len_build_threaded(
        mr, bm->totface, mesh_render_data_mat_tri_len_bm_range_fn);
  }
  return mesh_render_data_mat_tri_len_build_threaded(
      mr, mr->poly_len, mesh_render_data_mat_tri_len_mesh_range_fn);
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Mesh/BMesh Interface (indirect, partially cached access to complex data).
 * \{ */

void mesh_render_data_update_looptris(MeshRenderData *mr,
                                      const eMRIterType iter_type,
                                      const eMRDataType data_flag)
{
  if (mr->extract_type != MR_EXTRACT_BMESH) {
    /* Mesh */
    if ((iter_type & MR_ITER_LOOPTRI) || (data_flag & MR_DATA_LOOPTRI)) {
      mr->mlooptri = BKE_mesh_runtime_looptri_ensure(mr->me);
    }
  }
  else {
    /* #BMesh */
    if ((iter_type & MR_ITER_LOOPTRI) || (data_flag & MR_DATA_LOOPTRI)) {
      /* Edit mode ensures this is valid, no need to calculate. */
      BLI_assert((mr->bm->totloop == 0) || (mr->edit_bmesh->looptris != nullptr));
    }
  }
}

void mesh_render_data_update_normals(MeshRenderData *mr, const eMRDataType data_flag)
{
  Mesh *me = mr->me;
  const bool is_auto_smooth = (me->flag & ME_AUTOSMOOTH) != 0;
  const float split_angle = is_auto_smooth ? me->smoothresh : float(M_PI);

  if (mr->extract_type != MR_EXTRACT_BMESH) {
    /* Mesh */
    mr->vert_normals = BKE_mesh_vertex_normals_ensure(mr->me);
    if (data_flag & (MR_DATA_POLY_NOR | MR_DATA_LOOP_NOR | MR_DATA_TAN_LOOP_NOR)) {
      mr->poly_normals = BKE_mesh_poly_normals_ensure(mr->me);
    }
    if (((data_flag & MR_DATA_LOOP_NOR) && is_auto_smooth) || (data_flag & MR_DATA_TAN_LOOP_NOR)) {
      mr->loop_normals = static_cast<float(*)[3]>(
          MEM_mallocN(sizeof(*mr->loop_normals) * mr->loop_len, __func__));
      short(*clnors)[2] = static_cast<short(*)[2]>(
          CustomData_get_layer(&mr->me->ldata, CD_CUSTOMLOOPNORMAL));
      BKE_mesh_normals_loop_split(mr->mvert,
                                  mr->vert_normals,
                                  mr->vert_len,
                                  mr->medge,
                                  mr->edge_len,
                                  mr->mloop,
                                  mr->loop_normals,
                                  mr->loop_len,
                                  mr->mpoly,
                                  mr->poly_normals,
                                  mr->poly_len,
                                  is_auto_smooth,
                                  split_angle,
                                  nullptr,
                                  nullptr,
                                  clnors);
    }
  }
  else {
    /* #BMesh */
    if (data_flag & MR_DATA_POLY_NOR) {
      /* Use #BMFace.no instead. */
    }
    if (((data_flag & MR_DATA_LOOP_NOR) && is_auto_smooth) || (data_flag & MR_DATA_TAN_LOOP_NOR)) {

      const float(*vert_coords)[3] = nullptr;
      const float(*vert_normals)[3] = nullptr;
      const float(*poly_normals)[3] = nullptr;

      if (mr->edit_data && mr->edit_data->vertexCos) {
        vert_coords = mr->bm_vert_coords;
        vert_normals = mr->bm_vert_normals;
        poly_normals = mr->bm_poly_normals;
      }

      mr->loop_normals = static_cast<float(*)[3]>(
          MEM_mallocN(sizeof(*mr->loop_normals) * mr->loop_len, __func__));
      const int clnors_offset = CustomData_get_offset(&mr->bm->ldata, CD_CUSTOMLOOPNORMAL);
      BM_loops_calc_normal_vcos(mr->bm,
                                vert_coords,
                                vert_normals,
                                poly_normals,
                                is_auto_smooth,
                                split_angle,
                                mr->loop_normals,
                                nullptr,
                                nullptr,
                                clnors_offset,
                                false);
    }
  }
}

static void retrieve_active_attribute_names(MeshRenderData &mr,
                                            const Object &object,
                                            const Mesh &mesh)
{
  const Mesh *mesh_final = editmesh_final_or_this(&object, &mesh);
  mr.active_color_name = mesh_final->active_color_attribute;
  mr.default_color_name = mesh_final->default_color_attribute;
}

MeshRenderData *mesh_render_data_create(Object *object,
                                        Mesh *me,
                                        const bool is_editmode,
                                        const bool is_paint_mode,
                                        const bool is_mode_active,
                                        const float obmat[4][4],
                                        const bool do_final,
                                        const bool do_uvedit,
                                        const ToolSettings *ts)
{
  MeshRenderData *mr = static_cast<MeshRenderData *>(MEM_callocN(sizeof(*mr), __func__));
  mr->toolsettings = ts;
  mr->mat_len = mesh_render_mat_len_get(object, me);

  copy_m4_m4(mr->obmat, obmat);

  if (is_editmode) {
    Mesh *editmesh_eval_final = BKE_object_get_editmesh_eval_final(object);
    Mesh *editmesh_eval_cage = BKE_object_get_editmesh_eval_cage(object);

    BLI_assert(editmesh_eval_cage && editmesh_eval_final);
    mr->bm = me->edit_mesh->bm;
    mr->edit_bmesh = me->edit_mesh;
    mr->me = (do_final) ? editmesh_eval_final : editmesh_eval_cage;
    mr->edit_data = is_mode_active ? mr->me->runtime->edit_data : nullptr;

    if (mr->edit_data) {
      EditMeshData *emd = mr->edit_data;
      if (emd->vertexCos) {
        BKE_editmesh_cache_ensure_vert_normals(mr->edit_bmesh, emd);
        BKE_editmesh_cache_ensure_poly_normals(mr->edit_bmesh, emd);
      }

      mr->bm_vert_coords = mr->edit_data->vertexCos;
      mr->bm_vert_normals = mr->edit_data->vertexNos;
      mr->bm_poly_normals = mr->edit_data->polyNos;
      mr->bm_poly_centers = mr->edit_data->polyCos;
    }

    int bm_ensure_types = BM_VERT | BM_EDGE | BM_LOOP | BM_FACE;

    BM_mesh_elem_index_ensure(mr->bm, bm_ensure_types);
    BM_mesh_elem_table_ensure(mr->bm, bm_ensure_types & ~BM_LOOP);

    mr->efa_act_uv = EDBM_uv_active_face_get(mr->edit_bmesh, false, false);
    mr->efa_act = BM_mesh_active_face_get(mr->bm, false, true);
    mr->eed_act = BM_mesh_active_edge_get(mr->bm);
    mr->eve_act = BM_mesh_active_vert_get(mr->bm);

    mr->vert_crease_ofs = CustomData_get_offset(&mr->bm->vdata, CD_CREASE);
    mr->edge_crease_ofs = CustomData_get_offset(&mr->bm->edata, CD_CREASE);
    mr->bweight_ofs = CustomData_get_offset(&mr->bm->edata, CD_BWEIGHT);
#ifdef WITH_FREESTYLE
    mr->freestyle_edge_ofs = CustomData_get_offset(&mr->bm->edata, CD_FREESTYLE_EDGE);
    mr->freestyle_face_ofs = CustomData_get_offset(&mr->bm->pdata, CD_FREESTYLE_FACE);
#endif

    /* Use bmesh directly when the object is in edit mode unchanged by any modifiers.
     * For non-final UVs, always use original bmesh since the UV editor does not support
     * using the cage mesh with deformed coordinates. */
    if ((is_mode_active && mr->me->runtime->is_original_bmesh &&
         mr->me->runtime->wrapper_type == ME_WRAPPER_TYPE_BMESH) ||
        (do_uvedit && !do_final)) {
      mr->extract_type = MR_EXTRACT_BMESH;
    }
    else {
      mr->extract_type = MR_EXTRACT_MESH;

      /* Use mapping from final to original mesh when the object is in edit mode. */
      if (is_mode_active && do_final) {
        mr->v_origindex = static_cast<const int *>(
            CustomData_get_layer(&mr->me->vdata, CD_ORIGINDEX));
        mr->e_origindex = static_cast<const int *>(
            CustomData_get_layer(&mr->me->edata, CD_ORIGINDEX));
        mr->p_origindex = static_cast<const int *>(
            CustomData_get_layer(&mr->me->pdata, CD_ORIGINDEX));
      }
      else {
        mr->v_origindex = nullptr;
        mr->e_origindex = nullptr;
        mr->p_origindex = nullptr;
      }
    }
  }
  else {
    mr->me = me;
    mr->edit_bmesh = nullptr;
    mr->extract_type = MR_EXTRACT_MESH;

    if (is_paint_mode && mr->me) {
      mr->v_origindex = static_cast<const int *>(
          CustomData_get_layer(&mr->me->vdata, CD_ORIGINDEX));
      mr->e_origindex = static_cast<const int *>(
          CustomData_get_layer(&mr->me->edata, CD_ORIGINDEX));
      mr->p_origindex = static_cast<const int *>(
          CustomData_get_layer(&mr->me->pdata, CD_ORIGINDEX));
    }
    else {
      mr->v_origindex = nullptr;
      mr->e_origindex = nullptr;
      mr->p_origindex = nullptr;
    }
  }

  if (mr->extract_type != MR_EXTRACT_BMESH) {
    /* Mesh */
    mr->vert_len = mr->me->totvert;
    mr->edge_len = mr->me->totedge;
    mr->loop_len = mr->me->totloop;
    mr->poly_len = mr->me->totpoly;
    mr->tri_len = poly_to_tri_count(mr->poly_len, mr->loop_len);

    mr->mvert = BKE_mesh_verts(mr->me);
    mr->medge = BKE_mesh_edges(mr->me);
    mr->mpoly = BKE_mesh_polys(mr->me);
    mr->mloop = BKE_mesh_loops(mr->me);

    mr->v_origindex = static_cast<const int *>(CustomData_get_layer(&mr->me->vdata, CD_ORIGINDEX));
    mr->e_origindex = static_cast<const int *>(CustomData_get_layer(&mr->me->edata, CD_ORIGINDEX));
    mr->p_origindex = static_cast<const int *>(CustomData_get_layer(&mr->me->pdata, CD_ORIGINDEX));

    mr->material_indices = static_cast<const int *>(
        CustomData_get_layer_named(&me->pdata, CD_PROP_INT32, "material_index"));

    mr->hide_vert = static_cast<const bool *>(
        CustomData_get_layer_named(&me->vdata, CD_PROP_BOOL, ".hide_vert"));
    mr->hide_edge = static_cast<const bool *>(
        CustomData_get_layer_named(&me->edata, CD_PROP_BOOL, ".hide_edge"));
    mr->hide_poly = static_cast<const bool *>(
        CustomData_get_layer_named(&me->pdata, CD_PROP_BOOL, ".hide_poly"));

    mr->select_vert = static_cast<const bool *>(
        CustomData_get_layer_named(&me->vdata, CD_PROP_BOOL, ".select_vert"));
    mr->select_edge = static_cast<const bool *>(
        CustomData_get_layer_named(&me->edata, CD_PROP_BOOL, ".select_edge"));
    mr->select_poly = static_cast<const bool *>(
        CustomData_get_layer_named(&me->pdata, CD_PROP_BOOL, ".select_poly"));
  }
  else {
    /* #BMesh */
    BMesh *bm = mr->bm;

    mr->vert_len = bm->totvert;
    mr->edge_len = bm->totedge;
    mr->loop_len = bm->totloop;
    mr->poly_len = bm->totface;
    mr->tri_len = poly_to_tri_count(mr->poly_len, mr->loop_len);
  }

  retrieve_active_attribute_names(*mr, *object, *me);

  return mr;
}

void mesh_render_data_free(MeshRenderData *mr)
{
  MEM_SAFE_FREE(mr->loop_normals);

  /* Loose geometry are owned by #MeshBufferCache. */
  mr->ledges = nullptr;
  mr->lverts = nullptr;

  MEM_freeN(mr);
}

/** \} */
