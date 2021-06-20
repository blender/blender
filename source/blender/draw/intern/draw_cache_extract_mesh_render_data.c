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
 *
 * The Original Code is Copyright (C) 2021 by Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup draw
 *
 * \brief Extraction of Mesh data into VBO to feed to GPU.
 */

#include "MEM_guardedalloc.h"

#include "BLI_bitmap.h"
#include "BLI_math.h"
#include "BLI_task.h"

#include "BKE_editmesh.h"
#include "BKE_editmesh_cache.h"
#include "BKE_mesh.h"

#include "GPU_batch.h"

#include "ED_mesh.h"

#include "draw_cache_extract_mesh_private.h"

/* ---------------------------------------------------------------------- */
/** \name Update Loose Geometry
 * \{ */

static void mesh_render_data_lverts_bm(const MeshRenderData *mr,
                                       MeshBufferExtractionCache *cache,
                                       BMesh *bm);
static void mesh_render_data_ledges_bm(const MeshRenderData *mr,
                                       MeshBufferExtractionCache *cache,
                                       BMesh *bm);
static void mesh_render_data_loose_geom_mesh(const MeshRenderData *mr,
                                             MeshBufferExtractionCache *cache);
static void mesh_render_data_loose_geom_build(const MeshRenderData *mr,
                                              MeshBufferExtractionCache *cache);

static void mesh_render_data_loose_geom_load(MeshRenderData *mr, MeshBufferExtractionCache *cache)
{
  mr->ledges = cache->loose_geom.edges;
  mr->lverts = cache->loose_geom.verts;
  mr->vert_loose_len = cache->loose_geom.vert_len;
  mr->edge_loose_len = cache->loose_geom.edge_len;

  mr->loop_loose_len = mr->vert_loose_len + (mr->edge_loose_len * 2);
}

static void mesh_render_data_loose_geom_ensure(const MeshRenderData *mr,
                                               MeshBufferExtractionCache *cache)
{
  /* Early exit: Are loose geometry already available.
   * Only checking for loose verts as loose edges and verts are calculated at the same time. */
  if (cache->loose_geom.verts) {
    return;
  }
  mesh_render_data_loose_geom_build(mr, cache);
}

static void mesh_render_data_loose_geom_build(const MeshRenderData *mr,
                                              MeshBufferExtractionCache *cache)
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

static void mesh_render_data_loose_geom_mesh(const MeshRenderData *mr,
                                             MeshBufferExtractionCache *cache)
{
  BLI_bitmap *lvert_map = BLI_BITMAP_NEW(mr->vert_len, __func__);

  cache->loose_geom.edges = MEM_mallocN(mr->edge_len * sizeof(*cache->loose_geom.edges), __func__);
  const MEdge *med = mr->medge;
  for (int med_index = 0; med_index < mr->edge_len; med_index++, med++) {
    if (med->flag & ME_LOOSEEDGE) {
      cache->loose_geom.edges[cache->loose_geom.edge_len++] = med_index;
    }
    /* Tag verts as not loose. */
    BLI_BITMAP_ENABLE(lvert_map, med->v1);
    BLI_BITMAP_ENABLE(lvert_map, med->v2);
  }
  if (cache->loose_geom.edge_len < mr->edge_len) {
    cache->loose_geom.edges = MEM_reallocN(
        cache->loose_geom.edges, cache->loose_geom.edge_len * sizeof(*cache->loose_geom.edges));
  }

  cache->loose_geom.verts = MEM_mallocN(mr->vert_len * sizeof(*cache->loose_geom.verts), __func__);
  for (int v = 0; v < mr->vert_len; v++) {
    if (!BLI_BITMAP_TEST(lvert_map, v)) {
      cache->loose_geom.verts[cache->loose_geom.vert_len++] = v;
    }
  }
  if (cache->loose_geom.vert_len < mr->vert_len) {
    cache->loose_geom.verts = MEM_reallocN(
        cache->loose_geom.verts, cache->loose_geom.vert_len * sizeof(*cache->loose_geom.verts));
  }

  MEM_freeN(lvert_map);
}

static void mesh_render_data_lverts_bm(const MeshRenderData *mr,
                                       MeshBufferExtractionCache *cache,
                                       BMesh *bm)
{
  int elem_id;
  BMIter iter;
  BMVert *eve;
  cache->loose_geom.verts = MEM_mallocN(mr->vert_len * sizeof(*cache->loose_geom.verts), __func__);
  BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, elem_id) {
    if (eve->e == NULL) {
      cache->loose_geom.verts[cache->loose_geom.vert_len++] = elem_id;
    }
  }
  if (cache->loose_geom.vert_len < mr->vert_len) {
    cache->loose_geom.verts = MEM_reallocN(
        cache->loose_geom.verts, cache->loose_geom.vert_len * sizeof(*cache->loose_geom.verts));
  }
}

static void mesh_render_data_ledges_bm(const MeshRenderData *mr,
                                       MeshBufferExtractionCache *cache,
                                       BMesh *bm)
{
  int elem_id;
  BMIter iter;
  BMEdge *ede;
  cache->loose_geom.edges = MEM_mallocN(mr->edge_len * sizeof(*cache->loose_geom.edges), __func__);
  BM_ITER_MESH_INDEX (ede, &iter, bm, BM_EDGES_OF_MESH, elem_id) {
    if (ede->l == NULL) {
      cache->loose_geom.edges[cache->loose_geom.edge_len++] = elem_id;
    }
  }
  if (cache->loose_geom.edge_len < mr->edge_len) {
    cache->loose_geom.edges = MEM_reallocN(
        cache->loose_geom.edges, cache->loose_geom.edge_len * sizeof(*cache->loose_geom.edges));
  }
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Material Offsets
 *
 * Material offsets contains the offset of a material after sorting tris based on their material.
 *
 * \{ */
static void mesh_render_data_mat_offset_load(MeshRenderData *mr,
                                             const MeshBufferExtractionCache *cache);
static void mesh_render_data_mat_offset_ensure(MeshRenderData *mr,
                                               MeshBufferExtractionCache *cache);
static void mesh_render_data_mat_offset_build(MeshRenderData *mr,
                                              MeshBufferExtractionCache *cache);
static void mesh_render_data_mat_offset_build_bm(MeshRenderData *mr,
                                                 MeshBufferExtractionCache *cache);
static void mesh_render_data_mat_offset_build_mesh(MeshRenderData *mr,
                                                   MeshBufferExtractionCache *cache);
static void mesh_render_data_mat_offset_apply_offset(MeshRenderData *mr,
                                                     MeshBufferExtractionCache *cache);

void mesh_render_data_update_mat_offsets(MeshRenderData *mr,
                                         MeshBufferExtractionCache *cache,
                                         const eMRDataType data_flag)
{
  if (data_flag & MR_DATA_MAT_OFFSETS) {
    mesh_render_data_mat_offset_ensure(mr, cache);
    mesh_render_data_mat_offset_load(mr, cache);
  }
}

static void mesh_render_data_mat_offset_load(MeshRenderData *mr,
                                             const MeshBufferExtractionCache *cache)
{
  mr->mat_offsets.tri = cache->mat_offsets.tri;
  mr->mat_offsets.visible_tri_len = cache->mat_offsets.visible_tri_len;
}

static void mesh_render_data_mat_offset_ensure(MeshRenderData *mr,
                                               MeshBufferExtractionCache *cache)
{
  if (cache->mat_offsets.tri) {
    return;
  }
  mesh_render_data_mat_offset_build(mr, cache);
}

static void mesh_render_data_mat_offset_build(MeshRenderData *mr, MeshBufferExtractionCache *cache)
{
  size_t mat_tri_idx_size = sizeof(int) * mr->mat_len;
  cache->mat_offsets.tri = MEM_callocN(mat_tri_idx_size, __func__);

  /* Count how many triangles for each material. */
  if (mr->extract_type == MR_EXTRACT_BMESH) {
    mesh_render_data_mat_offset_build_bm(mr, cache);
  }
  else {
    mesh_render_data_mat_offset_build_mesh(mr, cache);
  }

  mesh_render_data_mat_offset_apply_offset(mr, cache);
}

typedef struct MatOffsetUserData {
  MeshRenderData *mr;
  /** This struct is extended during allocation to hold mat_tri_len for each material. */
  int mat_tri_len[0];
} MatOffsetUserData;

static void mesh_render_data_mat_offset_reduce(const void *__restrict UNUSED(userdata),
                                               void *__restrict chunk_join,
                                               void *__restrict chunk)
{
  MatOffsetUserData *dst = chunk_join;
  MatOffsetUserData *src = chunk;
  int *dst_mat_len = dst->mat_tri_len;
  int *src_mat_len = src->mat_tri_len;
  for (int i = 0; i < dst->mr->mat_len; i++) {
    dst_mat_len[i] += src_mat_len[i];
  }
}

static void mesh_render_data_mat_offset_build_threaded(MeshRenderData *mr,
                                                       MeshBufferExtractionCache *cache,
                                                       int face_len,
                                                       TaskParallelRangeFunc range_func)
{
  /* Extending the #MatOffsetUserData with an int per material slot. */
  size_t userdata_size = sizeof(MatOffsetUserData) +
                         (mr->mat_len) * sizeof(*cache->mat_offsets.tri);
  MatOffsetUserData *userdata = MEM_callocN(userdata_size, __func__);
  userdata->mr = mr;
  TaskParallelSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.userdata_chunk = userdata;
  settings.userdata_chunk_size = userdata_size;
  settings.min_iter_per_thread = MIN_RANGE_LEN;
  settings.func_reduce = mesh_render_data_mat_offset_reduce;
  BLI_task_parallel_range(0, face_len, NULL, range_func, &settings);

  memcpy(cache->mat_offsets.tri,
         &userdata->mat_tri_len,
         (mr->mat_len) * sizeof(*cache->mat_offsets.tri));
  MEM_freeN(userdata);
}

static void mesh_render_data_mat_offset_bm_range(void *__restrict UNUSED(userdata),
                                                 const int iter,
                                                 const TaskParallelTLS *__restrict tls)
{
  MatOffsetUserData *mat_offset_userdata = tls->userdata_chunk;
  MeshRenderData *mr = mat_offset_userdata->mr;
  int *mat_tri_len = mat_offset_userdata->mat_tri_len;

  BMesh *bm = mr->bm;
  BMFace *efa = BM_face_at_index(bm, iter);
  if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
    int mat = min_ii(efa->mat_nr, mr->mat_len - 1);
    mat_tri_len[mat] += efa->len - 2;
  }
}

static void mesh_render_data_mat_offset_build_bm(MeshRenderData *mr,
                                                 MeshBufferExtractionCache *cache)
{
  BMesh *bm = mr->bm;
  mesh_render_data_mat_offset_build_threaded(
      mr, cache, bm->totface, mesh_render_data_mat_offset_bm_range);
}

static void mesh_render_data_mat_offset_mesh_range(void *__restrict UNUSED(userdata),
                                                   const int iter,
                                                   const TaskParallelTLS *__restrict tls)
{
  MatOffsetUserData *mat_offset_userdata = tls->userdata_chunk;
  const MeshRenderData *mr = mat_offset_userdata->mr;
  int *mat_tri_len = mat_offset_userdata->mat_tri_len;

  const MPoly *mp = &mr->mpoly[iter];
  if (!(mr->use_hide && (mp->flag & ME_HIDE))) {
    int mat = min_ii(mp->mat_nr, mr->mat_len - 1);
    mat_tri_len[mat] += mp->totloop - 2;
  }
}

static void mesh_render_data_mat_offset_build_mesh(MeshRenderData *mr,
                                                   MeshBufferExtractionCache *cache)
{
  mesh_render_data_mat_offset_build_threaded(
      mr, cache, mr->poly_len, mesh_render_data_mat_offset_mesh_range);
}

static void mesh_render_data_mat_offset_apply_offset(MeshRenderData *mr,
                                                     MeshBufferExtractionCache *cache)
{
  int *mat_tri_len = cache->mat_offsets.tri;
  int ofs = mat_tri_len[0];
  mat_tri_len[0] = 0;
  for (int i = 1; i < mr->mat_len; i++) {
    int tmp = mat_tri_len[i];
    mat_tri_len[i] = ofs;
    ofs += tmp;
  }
  cache->mat_offsets.visible_tri_len = ofs;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Mesh/BMesh Interface (indirect, partially cached access to complex data).
 * \{ */

/**
 * Part of the creation of the #MeshRenderData that happens in a thread.
 */
void mesh_render_data_update_looptris(MeshRenderData *mr,
                                      const eMRIterType iter_type,
                                      const eMRDataType data_flag)
{
  Mesh *me = mr->me;
  if (mr->extract_type != MR_EXTRACT_BMESH) {
    /* Mesh */
    if ((iter_type & MR_ITER_LOOPTRI) || (data_flag & MR_DATA_LOOPTRI)) {
      /* NOTE(campbell): It's possible to skip allocating tessellation,
       * the tessellation can be calculated as part of the iterator, see: P2188.
       * The overall advantage is small (around 1%), so keep this as-is. */
      mr->mlooptri = MEM_mallocN(sizeof(*mr->mlooptri) * mr->tri_len, "MR_DATATYPE_LOOPTRI");
      if (mr->poly_normals != NULL) {
        BKE_mesh_recalc_looptri_with_normals(me->mloop,
                                             me->mpoly,
                                             me->mvert,
                                             me->totloop,
                                             me->totpoly,
                                             mr->mlooptri,
                                             mr->poly_normals);
      }
      else {
        BKE_mesh_recalc_looptri(
            me->mloop, me->mpoly, me->mvert, me->totloop, me->totpoly, mr->mlooptri);
      }
    }
  }
  else {
    /* #BMesh */
    if ((iter_type & MR_ITER_LOOPTRI) || (data_flag & MR_DATA_LOOPTRI)) {
      /* Edit mode ensures this is valid, no need to calculate. */
      BLI_assert((mr->bm->totloop == 0) || (mr->edit_bmesh->looptris != NULL));
    }
  }
}

void mesh_render_data_update_normals(MeshRenderData *mr, const eMRDataType data_flag)
{
  Mesh *me = mr->me;
  const bool is_auto_smooth = (me->flag & ME_AUTOSMOOTH) != 0;
  const float split_angle = is_auto_smooth ? me->smoothresh : (float)M_PI;

  if (mr->extract_type != MR_EXTRACT_BMESH) {
    /* Mesh */
    if (data_flag & (MR_DATA_POLY_NOR | MR_DATA_LOOP_NOR | MR_DATA_TAN_LOOP_NOR)) {
      BKE_mesh_ensure_normals_for_display(mr->me);
      mr->poly_normals = CustomData_get_layer(&mr->me->pdata, CD_NORMAL);
    }
    if (((data_flag & MR_DATA_LOOP_NOR) && is_auto_smooth) || (data_flag & MR_DATA_TAN_LOOP_NOR)) {
      mr->loop_normals = MEM_mallocN(sizeof(*mr->loop_normals) * mr->loop_len, __func__);
      short(*clnors)[2] = CustomData_get_layer(&mr->me->ldata, CD_CUSTOMLOOPNORMAL);
      BKE_mesh_normals_loop_split(mr->me->mvert,
                                  mr->vert_len,
                                  mr->me->medge,
                                  mr->edge_len,
                                  mr->me->mloop,
                                  mr->loop_normals,
                                  mr->loop_len,
                                  mr->me->mpoly,
                                  mr->poly_normals,
                                  mr->poly_len,
                                  is_auto_smooth,
                                  split_angle,
                                  NULL,
                                  clnors,
                                  NULL);
    }
  }
  else {
    /* #BMesh */
    if (data_flag & MR_DATA_POLY_NOR) {
      /* Use #BMFace.no instead. */
    }
    if (((data_flag & MR_DATA_LOOP_NOR) && is_auto_smooth) || (data_flag & MR_DATA_TAN_LOOP_NOR)) {

      const float(*vert_coords)[3] = NULL;
      const float(*vert_normals)[3] = NULL;
      const float(*poly_normals)[3] = NULL;

      if (mr->edit_data && mr->edit_data->vertexCos) {
        vert_coords = mr->bm_vert_coords;
        vert_normals = mr->bm_vert_normals;
        poly_normals = mr->bm_poly_normals;
      }

      mr->loop_normals = MEM_mallocN(sizeof(*mr->loop_normals) * mr->loop_len, __func__);
      const int clnors_offset = CustomData_get_offset(&mr->bm->ldata, CD_CUSTOMLOOPNORMAL);
      BM_loops_calc_normal_vcos(mr->bm,
                                vert_coords,
                                vert_normals,
                                poly_normals,
                                is_auto_smooth,
                                split_angle,
                                mr->loop_normals,
                                NULL,
                                NULL,
                                clnors_offset,
                                false);
    }
  }
}

/**
 * \param is_mode_active: When true, use the modifiers from the edit-data,
 * otherwise don't use modifiers as they are not from this object.
 */
MeshRenderData *mesh_render_data_create(Mesh *me,
                                        MeshBufferExtractionCache *cache,
                                        const bool is_editmode,
                                        const bool is_paint_mode,
                                        const bool is_mode_active,
                                        const float obmat[4][4],
                                        const bool do_final,
                                        const bool do_uvedit,
                                        const ToolSettings *ts,
                                        const eMRIterType iter_type)
{
  MeshRenderData *mr = MEM_callocN(sizeof(*mr), __func__);
  mr->toolsettings = ts;
  mr->mat_len = mesh_render_mat_len_get(me);

  copy_m4_m4(mr->obmat, obmat);

  if (is_editmode) {
    BLI_assert(me->edit_mesh->mesh_eval_cage && me->edit_mesh->mesh_eval_final);
    mr->bm = me->edit_mesh->bm;
    mr->edit_bmesh = me->edit_mesh;
    mr->me = (do_final) ? me->edit_mesh->mesh_eval_final : me->edit_mesh->mesh_eval_cage;
    mr->edit_data = is_mode_active ? mr->me->runtime.edit_data : NULL;

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

    bool has_mdata = is_mode_active && (mr->me->runtime.wrapper_type == ME_WRAPPER_TYPE_MDATA);
    bool use_mapped = is_mode_active &&
                      (has_mdata && !do_uvedit && mr->me && !mr->me->runtime.is_original);

    int bm_ensure_types = BM_VERT | BM_EDGE | BM_LOOP | BM_FACE;

    BM_mesh_elem_index_ensure(mr->bm, bm_ensure_types);
    BM_mesh_elem_table_ensure(mr->bm, bm_ensure_types & ~BM_LOOP);

    mr->efa_act_uv = EDBM_uv_active_face_get(mr->edit_bmesh, false, false);
    mr->efa_act = BM_mesh_active_face_get(mr->bm, false, true);
    mr->eed_act = BM_mesh_active_edge_get(mr->bm);
    mr->eve_act = BM_mesh_active_vert_get(mr->bm);

    mr->crease_ofs = CustomData_get_offset(&mr->bm->edata, CD_CREASE);
    mr->bweight_ofs = CustomData_get_offset(&mr->bm->edata, CD_BWEIGHT);
#ifdef WITH_FREESTYLE
    mr->freestyle_edge_ofs = CustomData_get_offset(&mr->bm->edata, CD_FREESTYLE_EDGE);
    mr->freestyle_face_ofs = CustomData_get_offset(&mr->bm->pdata, CD_FREESTYLE_FACE);
#endif

    if (use_mapped) {
      mr->v_origindex = CustomData_get_layer(&mr->me->vdata, CD_ORIGINDEX);
      mr->e_origindex = CustomData_get_layer(&mr->me->edata, CD_ORIGINDEX);
      mr->p_origindex = CustomData_get_layer(&mr->me->pdata, CD_ORIGINDEX);

      use_mapped = (mr->v_origindex || mr->e_origindex || mr->p_origindex);
    }

    mr->extract_type = use_mapped ? MR_EXTRACT_MAPPED : MR_EXTRACT_BMESH;

    /* Seems like the mesh_eval_final do not have the right origin indices.
     * Force not mapped in this case. */
    if (has_mdata && do_final && me->edit_mesh->mesh_eval_final != me->edit_mesh->mesh_eval_cage) {
      // mr->edit_bmesh = NULL;
      mr->extract_type = MR_EXTRACT_MESH;
    }
  }
  else {
    mr->me = me;
    mr->edit_bmesh = NULL;

    bool use_mapped = is_paint_mode && mr->me && !mr->me->runtime.is_original;
    if (use_mapped) {
      mr->v_origindex = CustomData_get_layer(&mr->me->vdata, CD_ORIGINDEX);
      mr->e_origindex = CustomData_get_layer(&mr->me->edata, CD_ORIGINDEX);
      mr->p_origindex = CustomData_get_layer(&mr->me->pdata, CD_ORIGINDEX);

      use_mapped = (mr->v_origindex || mr->e_origindex || mr->p_origindex);
    }

    mr->extract_type = use_mapped ? MR_EXTRACT_MAPPED : MR_EXTRACT_MESH;
  }

  if (mr->extract_type != MR_EXTRACT_BMESH) {
    /* Mesh */
    mr->vert_len = mr->me->totvert;
    mr->edge_len = mr->me->totedge;
    mr->loop_len = mr->me->totloop;
    mr->poly_len = mr->me->totpoly;
    mr->tri_len = poly_to_tri_count(mr->poly_len, mr->loop_len);

    mr->mvert = CustomData_get_layer(&mr->me->vdata, CD_MVERT);
    mr->medge = CustomData_get_layer(&mr->me->edata, CD_MEDGE);
    mr->mloop = CustomData_get_layer(&mr->me->ldata, CD_MLOOP);
    mr->mpoly = CustomData_get_layer(&mr->me->pdata, CD_MPOLY);

    mr->v_origindex = CustomData_get_layer(&mr->me->vdata, CD_ORIGINDEX);
    mr->e_origindex = CustomData_get_layer(&mr->me->edata, CD_ORIGINDEX);
    mr->p_origindex = CustomData_get_layer(&mr->me->pdata, CD_ORIGINDEX);
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

  if (iter_type & (MR_ITER_LEDGE | MR_ITER_LVERT)) {
    mesh_render_data_loose_geom_ensure(mr, cache);
    mesh_render_data_loose_geom_load(mr, cache);
  }

  return mr;
}

void mesh_render_data_free(MeshRenderData *mr)
{
  MEM_SAFE_FREE(mr->mlooptri);
  MEM_SAFE_FREE(mr->loop_normals);

  /* Loose geometry are owned by #MeshBufferExtractionCache. */
  mr->ledges = NULL;
  mr->lverts = NULL;

  MEM_freeN(mr);
}

/** \} */
