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
 * The Original Code is Copyright (C) 2017 by Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup draw
 *
 * \brief Extraction of Mesh data into VBO to feed to GPU.
 */

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_bitmap.h"
#include "BLI_buffer.h"
#include "BLI_edgehash.h"
#include "BLI_jitter_2d.h"
#include "BLI_math_bits.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_bvhutils.h"
#include "BKE_customdata.h"
#include "BKE_deform.h"
#include "BKE_editmesh.h"
#include "BKE_editmesh_bvh.h"
#include "BKE_editmesh_cache.h"
#include "BKE_editmesh_tangent.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_mesh_tangent.h"
#include "BKE_modifier.h"
#include "BKE_object_deform.h"

#include "atomic_ops.h"

#include "bmesh.h"

#include "GPU_batch.h"
#include "GPU_extensions.h"

#include "DRW_render.h"

#include "ED_mesh.h"
#include "ED_uvedit.h"

#include "draw_cache_impl.h"
#include "draw_cache_inline.h"

#include "draw_cache_extract.h"

// #define DEBUG_TIME

#ifdef DEBUG_TIME
#  include "PIL_time_utildefines.h"
#endif

/* ---------------------------------------------------------------------- */
/** \name Mesh/BMesh Interface (indirect, partially cached access to complex data).
 * \{ */

typedef struct MeshRenderData {
  eMRExtractType extract_type;

  int poly_len, edge_len, vert_len, loop_len;
  int edge_loose_len;
  int vert_loose_len;
  int loop_loose_len;
  int tri_len;
  int mat_len;

  bool use_hide;
  bool use_subsurf_fdots;
  bool use_final_mesh;

  /** Use for #MeshStatVis calculation which use world-space coords. */
  float obmat[4][4];

  const ToolSettings *toolsettings;
  /* HACK not supposed to be there but it's needed. */
  struct MeshBatchCache *cache;
  /** Edit Mesh */
  BMEditMesh *edit_bmesh;
  BMesh *bm;
  EditMeshData *edit_data;

  /* For deformed edit-mesh data. */
  /* Use for #ME_WRAPPER_TYPE_BMESH. */
  const float (*bm_vert_coords)[3];
  const float (*bm_vert_normals)[3];
  const float (*bm_poly_normals)[3];
  const float (*bm_poly_centers)[3];

  int *v_origindex, *e_origindex, *p_origindex;
  int crease_ofs;
  int bweight_ofs;
  int freestyle_edge_ofs;
  int freestyle_face_ofs;
  /** Mesh */
  Mesh *me;
  const MVert *mvert;
  const MEdge *medge;
  const MLoop *mloop;
  const MPoly *mpoly;
  BMVert *eve_act;
  BMEdge *eed_act;
  BMFace *efa_act;
  BMFace *efa_act_uv;
  /* Data created on-demand (usually not for bmesh-based data). */
  MLoopTri *mlooptri;
  float (*loop_normals)[3];
  float (*poly_normals)[3];
  int *lverts, *ledges;
} MeshRenderData;

static void mesh_render_data_update_loose_geom(MeshRenderData *mr,
                                               const eMRIterType iter_type,
                                               const eMRDataType UNUSED(data_flag))
{
  if (mr->extract_type != MR_EXTRACT_BMESH) {
    /* Mesh */
    if (iter_type & (MR_ITER_LEDGE | MR_ITER_LVERT)) {
      mr->vert_loose_len = 0;
      mr->edge_loose_len = 0;

      BLI_bitmap *lvert_map = BLI_BITMAP_NEW(mr->vert_len, "lvert map");

      mr->ledges = MEM_mallocN(mr->edge_len * sizeof(int), __func__);
      const MEdge *medge = mr->medge;
      for (int e = 0; e < mr->edge_len; e++, medge++) {
        if (medge->flag & ME_LOOSEEDGE) {
          mr->ledges[mr->edge_loose_len++] = e;
        }
        /* Tag verts as not loose. */
        BLI_BITMAP_ENABLE(lvert_map, medge->v1);
        BLI_BITMAP_ENABLE(lvert_map, medge->v2);
      }
      if (mr->edge_loose_len < mr->edge_len) {
        mr->ledges = MEM_reallocN(mr->ledges, mr->edge_loose_len * sizeof(*mr->ledges));
      }

      mr->lverts = MEM_mallocN(mr->vert_len * sizeof(*mr->lverts), __func__);
      for (int v = 0; v < mr->vert_len; v++) {
        if (!BLI_BITMAP_TEST(lvert_map, v)) {
          mr->lverts[mr->vert_loose_len++] = v;
        }
      }
      if (mr->vert_loose_len < mr->vert_len) {
        mr->lverts = MEM_reallocN(mr->lverts, mr->vert_loose_len * sizeof(*mr->lverts));
      }

      MEM_freeN(lvert_map);

      mr->loop_loose_len = mr->vert_loose_len + mr->edge_loose_len * 2;
    }
  }
  else {
    /* BMesh */
    BMesh *bm = mr->bm;
    if (iter_type & (MR_ITER_LEDGE | MR_ITER_LVERT)) {
      int elem_id;
      BMIter iter;
      BMVert *eve;
      BMEdge *ede;
      mr->vert_loose_len = 0;
      mr->edge_loose_len = 0;

      mr->lverts = MEM_mallocN(mr->vert_len * sizeof(*mr->lverts), __func__);
      BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, elem_id) {
        if (eve->e == NULL) {
          mr->lverts[mr->vert_loose_len++] = elem_id;
        }
      }
      if (mr->vert_loose_len < mr->vert_len) {
        mr->lverts = MEM_reallocN(mr->lverts, mr->vert_loose_len * sizeof(*mr->lverts));
      }

      mr->ledges = MEM_mallocN(mr->edge_len * sizeof(*mr->ledges), __func__);
      BM_ITER_MESH_INDEX (ede, &iter, bm, BM_EDGES_OF_MESH, elem_id) {
        if (ede->l == NULL) {
          mr->ledges[mr->edge_loose_len++] = elem_id;
        }
      }
      if (mr->edge_loose_len < mr->edge_len) {
        mr->ledges = MEM_reallocN(mr->ledges, mr->edge_loose_len * sizeof(*mr->ledges));
      }

      mr->loop_loose_len = mr->vert_loose_len + mr->edge_loose_len * 2;
    }
  }
}

/* Part of the creation of the MeshRenderData that happens in a thread. */
static void mesh_render_data_update_looptris(MeshRenderData *mr,
                                             const eMRIterType iter_type,
                                             const eMRDataType data_flag)
{
  Mesh *me = mr->me;
  if (mr->extract_type != MR_EXTRACT_BMESH) {
    /* Mesh */
    if ((iter_type & MR_ITER_LOOPTRI) || (data_flag & MR_DATA_LOOPTRI)) {
      mr->mlooptri = MEM_mallocN(sizeof(*mr->mlooptri) * mr->tri_len, "MR_DATATYPE_LOOPTRI");
      BKE_mesh_recalc_looptri(
          me->mloop, me->mpoly, me->mvert, me->totloop, me->totpoly, mr->mlooptri);
    }
  }
  else {
    /* BMesh */
    if ((iter_type & MR_ITER_LOOPTRI) || (data_flag & MR_DATA_LOOPTRI)) {
      /* Edit mode ensures this is valid, no need to calculate. */
      BLI_assert((mr->bm->totloop == 0) || (mr->edit_bmesh->looptris != NULL));
    }
  }
}

static void mesh_render_data_update_normals(MeshRenderData *mr,
                                            const eMRIterType UNUSED(iter_type),
                                            const eMRDataType data_flag)
{
  Mesh *me = mr->me;
  const bool is_auto_smooth = (me->flag & ME_AUTOSMOOTH) != 0;
  const float split_angle = is_auto_smooth ? me->smoothresh : (float)M_PI;

  if (mr->extract_type != MR_EXTRACT_BMESH) {
    /* Mesh */
    if (data_flag & (MR_DATA_POLY_NOR | MR_DATA_LOOP_NOR | MR_DATA_TAN_LOOP_NOR)) {
      mr->poly_normals = MEM_mallocN(sizeof(*mr->poly_normals) * mr->poly_len, __func__);
      BKE_mesh_calc_normals_poly((MVert *)mr->mvert,
                                 NULL,
                                 mr->vert_len,
                                 mr->mloop,
                                 mr->mpoly,
                                 mr->loop_len,
                                 mr->poly_len,
                                 mr->poly_normals,
                                 true);
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
    /* BMesh */
    if (data_flag & MR_DATA_POLY_NOR) {
      /* Use bmface->no instead. */
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
      int clnors_offset = CustomData_get_offset(&mr->bm->ldata, CD_CUSTOMLOOPNORMAL);
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

static MeshRenderData *mesh_render_data_create(Mesh *me,
                                               const bool is_editmode,
                                               const bool is_paint_mode,
                                               const float obmat[4][4],
                                               const bool do_final,
                                               const bool do_uvedit,
                                               const DRW_MeshCDMask *UNUSED(cd_used),
                                               const ToolSettings *ts,
                                               const eMRIterType iter_type,
                                               const eMRDataType data_flag)
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
    mr->edit_data = mr->me->runtime.edit_data;

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

    bool has_mdata = (mr->me->runtime.wrapper_type == ME_WRAPPER_TYPE_MDATA);
    bool use_mapped = has_mdata && !do_uvedit && mr->me && !mr->me->runtime.is_original;

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
    /* BMesh */
    BMesh *bm = mr->bm;

    mr->vert_len = bm->totvert;
    mr->edge_len = bm->totedge;
    mr->loop_len = bm->totloop;
    mr->poly_len = bm->totface;
    mr->tri_len = poly_to_tri_count(mr->poly_len, mr->loop_len);
  }
  mesh_render_data_update_loose_geom(mr, iter_type, data_flag);

  return mr;
}

static void mesh_render_data_free(MeshRenderData *mr)
{
  MEM_SAFE_FREE(mr->mlooptri);
  MEM_SAFE_FREE(mr->poly_normals);
  MEM_SAFE_FREE(mr->loop_normals);

  MEM_SAFE_FREE(mr->lverts);
  MEM_SAFE_FREE(mr->ledges);

  MEM_freeN(mr);
}

BLI_INLINE BMFace *bm_original_face_get(const MeshRenderData *mr, int idx)
{
  return ((mr->p_origindex != NULL) && (mr->p_origindex[idx] != ORIGINDEX_NONE) && mr->bm) ?
             BM_face_at_index(mr->bm, mr->p_origindex[idx]) :
             NULL;
}

BLI_INLINE BMEdge *bm_original_edge_get(const MeshRenderData *mr, int idx)
{
  return ((mr->e_origindex != NULL) && (mr->e_origindex[idx] != ORIGINDEX_NONE) && mr->bm) ?
             BM_edge_at_index(mr->bm, mr->e_origindex[idx]) :
             NULL;
}

BLI_INLINE BMVert *bm_original_vert_get(const MeshRenderData *mr, int idx)
{
  return ((mr->v_origindex != NULL) && (mr->v_origindex[idx] != ORIGINDEX_NONE) && mr->bm) ?
             BM_vert_at_index(mr->bm, mr->v_origindex[idx]) :
             NULL;
}

BLI_INLINE const float *bm_vert_co_get(const MeshRenderData *mr, const BMVert *eve)
{
  const float(*vert_coords)[3] = mr->bm_vert_coords;
  if (vert_coords != NULL) {
    return vert_coords[BM_elem_index_get(eve)];
  }
  else {
    UNUSED_VARS(mr);
    return eve->co;
  }
}

BLI_INLINE const float *bm_vert_no_get(const MeshRenderData *mr, const BMVert *eve)
{
  const float(*vert_normals)[3] = mr->bm_vert_normals;
  if (vert_normals != NULL) {
    return vert_normals[BM_elem_index_get(eve)];
  }
  else {
    UNUSED_VARS(mr);
    return eve->co;
  }
}

BLI_INLINE const float *bm_face_no_get(const MeshRenderData *mr, const BMFace *efa)
{
  const float(*poly_normals)[3] = mr->bm_poly_normals;
  if (poly_normals != NULL) {
    return poly_normals[BM_elem_index_get(efa)];
  }
  else {
    UNUSED_VARS(mr);
    return efa->no;
  }
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Mesh Elements Extract Iter
 * \{ */

typedef void *(ExtractInitFn)(const MeshRenderData *mr, void *buffer);
typedef void(ExtractEditTriFn)(const MeshRenderData *mr, int t, BMLoop **e, void *data);
typedef void(ExtractEditLoopFn)(const MeshRenderData *mr, int l, BMLoop *el, void *data);
typedef void(ExtractEditLedgeFn)(const MeshRenderData *mr, int e, BMEdge *ed, void *data);
typedef void(ExtractEditLvertFn)(const MeshRenderData *mr, int v, BMVert *ev, void *data);
typedef void(ExtractTriFn)(const MeshRenderData *mr, int t, const MLoopTri *mlt, void *data);
typedef void(ExtractLoopFn)(
    const MeshRenderData *mr, int l, const MLoop *mloop, int p, const MPoly *mpoly, void *data);
typedef void(ExtractLedgeFn)(const MeshRenderData *mr, int e, const MEdge *medge, void *data);
typedef void(ExtractLvertFn)(const MeshRenderData *mr, int v, const MVert *mvert, void *data);
typedef void(ExtractFinishFn)(const MeshRenderData *mr, void *buffer, void *data);

typedef struct MeshExtract {
  /** Executed on main thread and return user data for iter functions. */
  ExtractInitFn *init;
  /** Executed on one (or more if use_threading) worker thread(s). */
  ExtractEditTriFn *iter_looptri_bm;
  ExtractTriFn *iter_looptri;
  ExtractEditLoopFn *iter_loop_bm;
  ExtractLoopFn *iter_loop;
  ExtractEditLedgeFn *iter_ledge_bm;
  ExtractLedgeFn *iter_ledge;
  ExtractEditLvertFn *iter_lvert_bm;
  ExtractLvertFn *iter_lvert;
  /** Executed on one worker thread after all elements iterations. */
  ExtractFinishFn *finish;
  /** Used to request common data. */
  const eMRDataType data_flag;
  /** Used to know if the element callbacks are threadsafe and can be parallelized. */
  const bool use_threading;
} MeshExtract;

BLI_INLINE eMRIterType mesh_extract_iter_type(const MeshExtract *ext)
{
  eMRIterType type = 0;
  SET_FLAG_FROM_TEST(type, (ext->iter_looptri_bm || ext->iter_looptri), MR_ITER_LOOPTRI);
  SET_FLAG_FROM_TEST(type, (ext->iter_loop_bm || ext->iter_loop), MR_ITER_LOOP);
  SET_FLAG_FROM_TEST(type, (ext->iter_ledge_bm || ext->iter_ledge), MR_ITER_LEDGE);
  SET_FLAG_FROM_TEST(type, (ext->iter_lvert_bm || ext->iter_lvert), MR_ITER_LVERT);
  return type;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Triangles Indices
 * \{ */

typedef struct MeshExtract_Tri_Data {
  GPUIndexBufBuilder elb;
  int *tri_mat_start;
  int *tri_mat_end;
} MeshExtract_Tri_Data;

static void *extract_tris_init(const MeshRenderData *mr, void *UNUSED(ibo))
{
  MeshExtract_Tri_Data *data = MEM_callocN(sizeof(*data), __func__);

  size_t mat_tri_idx_size = sizeof(int) * mr->mat_len;
  data->tri_mat_start = MEM_callocN(mat_tri_idx_size, __func__);
  data->tri_mat_end = MEM_callocN(mat_tri_idx_size, __func__);

  int *mat_tri_len = data->tri_mat_start;
  /* Count how many triangle for each material. */
  if (mr->extract_type == MR_EXTRACT_BMESH) {
    BMIter iter;
    BMFace *efa;
    BM_ITER_MESH (efa, &iter, mr->bm, BM_FACES_OF_MESH) {
      if (!BM_elem_flag_test(efa, BM_ELEM_HIDDEN)) {
        int mat = min_ii(efa->mat_nr, mr->mat_len - 1);
        mat_tri_len[mat] += efa->len - 2;
      }
    }
  }
  else {
    const MPoly *mpoly = mr->mpoly;
    for (int p = 0; p < mr->poly_len; p++, mpoly++) {
      if (!(mr->use_hide && (mpoly->flag & ME_HIDE))) {
        int mat = min_ii(mpoly->mat_nr, mr->mat_len - 1);
        mat_tri_len[mat] += mpoly->totloop - 2;
      }
    }
  }
  /* Accumulate tri len per mat to have correct offsets. */
  int ofs = mat_tri_len[0];
  mat_tri_len[0] = 0;
  for (int i = 1; i < mr->mat_len; i++) {
    int tmp = mat_tri_len[i];
    mat_tri_len[i] = ofs;
    ofs += tmp;
  }

  memcpy(data->tri_mat_end, mat_tri_len, mat_tri_idx_size);

  int visible_tri_tot = ofs;
  GPU_indexbuf_init(&data->elb, GPU_PRIM_TRIS, visible_tri_tot, mr->loop_len);

  return data;
}

static void extract_tris_looptri_bmesh(const MeshRenderData *mr,
                                       int UNUSED(t),
                                       BMLoop **elt,
                                       void *_data)
{
  if (!BM_elem_flag_test(elt[0]->f, BM_ELEM_HIDDEN)) {
    MeshExtract_Tri_Data *data = _data;
    int *mat_tri_ofs = data->tri_mat_end;
    int mat = min_ii(elt[0]->f->mat_nr, mr->mat_len - 1);
    GPU_indexbuf_set_tri_verts(&data->elb,
                               mat_tri_ofs[mat]++,
                               BM_elem_index_get(elt[0]),
                               BM_elem_index_get(elt[1]),
                               BM_elem_index_get(elt[2]));
  }
}

static void extract_tris_looptri_mesh(const MeshRenderData *mr,
                                      int UNUSED(t),
                                      const MLoopTri *mlt,
                                      void *_data)
{
  const MPoly *mpoly = &mr->mpoly[mlt->poly];
  if (!(mr->use_hide && (mpoly->flag & ME_HIDE))) {
    MeshExtract_Tri_Data *data = _data;
    int *mat_tri_ofs = data->tri_mat_end;
    int mat = min_ii(mpoly->mat_nr, mr->mat_len - 1);
    GPU_indexbuf_set_tri_verts(
        &data->elb, mat_tri_ofs[mat]++, mlt->tri[0], mlt->tri[1], mlt->tri[2]);
  }
}

static void extract_tris_finish(const MeshRenderData *mr, void *ibo, void *_data)
{
  MeshExtract_Tri_Data *data = _data;
  GPU_indexbuf_build_in_place(&data->elb, ibo);
  /* HACK Create ibo subranges and assign them to each GPUBatch. */
  if (mr->use_final_mesh && mr->cache->surface_per_mat && mr->cache->surface_per_mat[0]) {
    BLI_assert(mr->cache->surface_per_mat[0]->elem == ibo);
    for (int i = 0; i < mr->mat_len; i++) {
      /* Multiply by 3 because these are triangle indices. */
      int start = data->tri_mat_start[i] * 3;
      int len = data->tri_mat_end[i] * 3 - data->tri_mat_start[i] * 3;
      GPUIndexBuf *sub_ibo = GPU_indexbuf_create_subrange(ibo, start, len);
      /* WARNING: We modify the GPUBatch here! */
      GPU_batch_elembuf_set(mr->cache->surface_per_mat[i], sub_ibo, true);
    }
  }
  MEM_freeN(data->tri_mat_start);
  MEM_freeN(data->tri_mat_end);
  MEM_freeN(data);
}

static const MeshExtract extract_tris = {
    extract_tris_init,
    extract_tris_looptri_bmesh,
    extract_tris_looptri_mesh,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    extract_tris_finish,
    0,
    false,
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Edges Indices
 * \{ */

static void *extract_lines_init(const MeshRenderData *mr, void *UNUSED(buf))
{
  GPUIndexBufBuilder *elb = MEM_mallocN(sizeof(*elb), __func__);
  /* Put loose edges at the end. */
  GPU_indexbuf_init(
      elb, GPU_PRIM_LINES, mr->edge_len + mr->edge_loose_len, mr->loop_len + mr->loop_loose_len);
  return elb;
}

static void extract_lines_loop_bmesh(const MeshRenderData *UNUSED(mr),
                                     int l,
                                     BMLoop *loop,
                                     void *elb)
{
  if (!BM_elem_flag_test(loop->e, BM_ELEM_HIDDEN)) {
    GPU_indexbuf_set_line_verts(elb, BM_elem_index_get(loop->e), l, BM_elem_index_get(loop->next));
  }
  else {
    GPU_indexbuf_set_line_restart(elb, BM_elem_index_get(loop->e));
  }
}

static void extract_lines_loop_mesh(const MeshRenderData *mr,
                                    int l,
                                    const MLoop *mloop,
                                    int UNUSED(p),
                                    const MPoly *mpoly,
                                    void *elb)
{
  const MEdge *medge = &mr->medge[mloop->e];
  if (!((mr->use_hide && (medge->flag & ME_HIDE)) ||
        ((mr->extract_type == MR_EXTRACT_MAPPED) && (mr->e_origindex) &&
         (mr->e_origindex[mloop->e] == ORIGINDEX_NONE)))) {
    int loopend = mpoly->totloop + mpoly->loopstart - 1;
    int other_loop = (l == loopend) ? mpoly->loopstart : (l + 1);
    GPU_indexbuf_set_line_verts(elb, mloop->e, l, other_loop);
  }
  else {
    GPU_indexbuf_set_line_restart(elb, mloop->e);
  }
}

static void extract_lines_ledge_bmesh(const MeshRenderData *mr, int e, BMEdge *eed, void *elb)
{
  int ledge_idx = mr->edge_len + e;
  if (!BM_elem_flag_test(eed, BM_ELEM_HIDDEN)) {
    int l = mr->loop_len + e * 2;
    GPU_indexbuf_set_line_verts(elb, ledge_idx, l, l + 1);
  }
  else {
    GPU_indexbuf_set_line_restart(elb, ledge_idx);
  }
  /* Don't render the edge twice. */
  GPU_indexbuf_set_line_restart(elb, BM_elem_index_get(eed));
}

static void extract_lines_ledge_mesh(const MeshRenderData *mr,
                                     int e,
                                     const MEdge *medge,
                                     void *elb)
{
  int ledge_idx = mr->edge_len + e;
  int edge_idx = mr->ledges[e];
  if (!((mr->use_hide && (medge->flag & ME_HIDE)) ||
        ((mr->extract_type == MR_EXTRACT_MAPPED) && (mr->e_origindex) &&
         (mr->e_origindex[edge_idx] == ORIGINDEX_NONE)))) {
    int l = mr->loop_len + e * 2;
    GPU_indexbuf_set_line_verts(elb, ledge_idx, l, l + 1);
  }
  else {
    GPU_indexbuf_set_line_restart(elb, ledge_idx);
  }
  /* Don't render the edge twice. */
  GPU_indexbuf_set_line_restart(elb, edge_idx);
}

static void extract_lines_finish(const MeshRenderData *UNUSED(mr), void *ibo, void *elb)
{
  GPU_indexbuf_build_in_place(elb, ibo);
  MEM_freeN(elb);
}

static const MeshExtract extract_lines = {
    extract_lines_init,
    NULL,
    NULL,
    extract_lines_loop_bmesh,
    extract_lines_loop_mesh,
    extract_lines_ledge_bmesh,
    extract_lines_ledge_mesh,
    NULL,
    NULL,
    extract_lines_finish,
    0,
    false,
};
/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Loose Edges Sub Buffer
 * \{ */

static void extract_lines_loose_subbuffer(const MeshRenderData *mr)
{
  BLI_assert(mr->cache->final.ibo.lines);
  /* Multiply by 2 because these are edges indices. */
  const int start = mr->edge_len * 2;
  const int len = mr->edge_loose_len * 2;
  GPU_indexbuf_create_subrange_in_place(
      mr->cache->final.ibo.lines_loose, mr->cache->final.ibo.lines, start, len);
  mr->cache->no_loose_wire = (len == 0);
}

static void extract_lines_with_lines_loose_finish(const MeshRenderData *mr, void *ibo, void *elb)
{
  GPU_indexbuf_build_in_place(elb, ibo);
  extract_lines_loose_subbuffer(mr);
  MEM_freeN(elb);
}

static const MeshExtract extract_lines_with_lines_loose = {
    extract_lines_init,
    NULL,
    NULL,
    extract_lines_loop_bmesh,
    extract_lines_loop_mesh,
    extract_lines_ledge_bmesh,
    extract_lines_ledge_mesh,
    NULL,
    NULL,
    extract_lines_with_lines_loose_finish,
    0,
    false,
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Point Indices
 * \{ */

static void *extract_points_init(const MeshRenderData *mr, void *UNUSED(buf))
{
  GPUIndexBufBuilder *elb = MEM_mallocN(sizeof(*elb), __func__);
  GPU_indexbuf_init(elb, GPU_PRIM_POINTS, mr->vert_len, mr->loop_len + mr->loop_loose_len);
  return elb;
}

BLI_INLINE void vert_set_bmesh(GPUIndexBufBuilder *elb, BMVert *eve, int loop)
{
  int vert_idx = BM_elem_index_get(eve);
  if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
    GPU_indexbuf_set_point_vert(elb, vert_idx, loop);
  }
  else {
    GPU_indexbuf_set_point_restart(elb, vert_idx);
  }
}

BLI_INLINE void vert_set_mesh(GPUIndexBufBuilder *elb,
                              const MeshRenderData *mr,
                              int vert_idx,
                              int loop)
{
  const MVert *mvert = &mr->mvert[vert_idx];
  if (!((mr->use_hide && (mvert->flag & ME_HIDE)) ||
        ((mr->extract_type == MR_EXTRACT_MAPPED) && (mr->v_origindex) &&
         (mr->v_origindex[vert_idx] == ORIGINDEX_NONE)))) {
    GPU_indexbuf_set_point_vert(elb, vert_idx, loop);
  }
  else {
    GPU_indexbuf_set_point_restart(elb, vert_idx);
  }
}

static void extract_points_loop_bmesh(const MeshRenderData *UNUSED(mr),
                                      int l,
                                      BMLoop *loop,
                                      void *elb)
{
  vert_set_bmesh(elb, loop->v, l);
}

static void extract_points_loop_mesh(const MeshRenderData *mr,
                                     int l,
                                     const MLoop *mloop,
                                     int UNUSED(p),
                                     const MPoly *UNUSED(mpoly),
                                     void *elb)
{
  vert_set_mesh(elb, mr, mloop->v, l);
}

static void extract_points_ledge_bmesh(const MeshRenderData *mr, int e, BMEdge *eed, void *elb)
{
  vert_set_bmesh(elb, eed->v1, mr->loop_len + e * 2);
  vert_set_bmesh(elb, eed->v2, mr->loop_len + e * 2 + 1);
}

static void extract_points_ledge_mesh(const MeshRenderData *mr,
                                      int e,
                                      const MEdge *medge,
                                      void *elb)
{
  vert_set_mesh(elb, mr, medge->v1, mr->loop_len + e * 2);
  vert_set_mesh(elb, mr, medge->v2, mr->loop_len + e * 2 + 1);
}

static void extract_points_lvert_bmesh(const MeshRenderData *mr, int v, BMVert *eve, void *elb)
{
  vert_set_bmesh(elb, eve, mr->loop_len + mr->edge_loose_len * 2 + v);
}

static void extract_points_lvert_mesh(const MeshRenderData *mr,
                                      int v,
                                      const MVert *UNUSED(mvert),
                                      void *elb)
{
  vert_set_mesh(elb, mr, mr->lverts[v], mr->loop_len + mr->edge_loose_len * 2 + v);
}

static void extract_points_finish(const MeshRenderData *UNUSED(mr), void *ibo, void *elb)
{
  GPU_indexbuf_build_in_place(elb, ibo);
  MEM_freeN(elb);
}

static const MeshExtract extract_points = {
    extract_points_init,
    NULL,
    NULL,
    extract_points_loop_bmesh,
    extract_points_loop_mesh,
    extract_points_ledge_bmesh,
    extract_points_ledge_mesh,
    extract_points_lvert_bmesh,
    extract_points_lvert_mesh,
    extract_points_finish,
    0,
    false,
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Facedots Indices
 * \{ */

static void *extract_fdots_init(const MeshRenderData *mr, void *UNUSED(buf))
{
  GPUIndexBufBuilder *elb = MEM_mallocN(sizeof(*elb), __func__);
  GPU_indexbuf_init(elb, GPU_PRIM_POINTS, mr->poly_len, mr->poly_len);
  return elb;
}

static void extract_fdots_loop_bmesh(const MeshRenderData *UNUSED(mr),
                                     int UNUSED(l),
                                     BMLoop *loop,
                                     void *elb)
{
  int face_idx = BM_elem_index_get(loop->f);
  if (!BM_elem_flag_test(loop->f, BM_ELEM_HIDDEN)) {
    GPU_indexbuf_set_point_vert(elb, face_idx, face_idx);
  }
  else {
    GPU_indexbuf_set_point_restart(elb, face_idx);
  }
}

static void extract_fdots_loop_mesh(const MeshRenderData *mr,
                                    int UNUSED(l),
                                    const MLoop *mloop,
                                    int p,
                                    const MPoly *mpoly,
                                    void *elb)
{
  const MVert *mvert = &mr->mvert[mloop->v];
  if ((!mr->use_subsurf_fdots || (mvert->flag & ME_VERT_FACEDOT)) &&
      !(mr->use_hide && (mpoly->flag & ME_HIDE))) {
    GPU_indexbuf_set_point_vert(elb, p, p);
  }
  else {
    GPU_indexbuf_set_point_restart(elb, p);
  }
}

static void extract_fdots_finish(const MeshRenderData *UNUSED(mr), void *ibo, void *elb)
{
  GPU_indexbuf_build_in_place(elb, ibo);
  MEM_freeN(elb);
}

static const MeshExtract extract_fdots = {
    extract_fdots_init,
    NULL,
    NULL,
    extract_fdots_loop_bmesh,
    extract_fdots_loop_mesh,
    NULL,
    NULL,
    NULL,
    NULL,
    extract_fdots_finish,
    0,
    false,
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Paint Mask Line Indices
 * \{ */

typedef struct MeshExtract_LinePaintMask_Data {
  GPUIndexBufBuilder elb;
  /** One bit per edge set if face is selected. */
  BLI_bitmap select_map[0];
} MeshExtract_LinePaintMask_Data;

static void *extract_lines_paint_mask_init(const MeshRenderData *mr, void *UNUSED(buf))
{
  size_t bitmap_size = BLI_BITMAP_SIZE(mr->edge_len);
  MeshExtract_LinePaintMask_Data *data = MEM_callocN(sizeof(*data) + bitmap_size, __func__);
  GPU_indexbuf_init(&data->elb, GPU_PRIM_LINES, mr->edge_len, mr->loop_len);
  return data;
}

static void extract_lines_paint_mask_loop_mesh(const MeshRenderData *mr,
                                               int l,
                                               const MLoop *mloop,
                                               int UNUSED(p),
                                               const MPoly *mpoly,
                                               void *_data)
{
  MeshExtract_LinePaintMask_Data *data = (MeshExtract_LinePaintMask_Data *)_data;
  const int edge_idx = mloop->e;
  const MEdge *medge = &mr->medge[edge_idx];
  if (!((mr->use_hide && (medge->flag & ME_HIDE)) ||
        ((mr->extract_type == MR_EXTRACT_MAPPED) && (mr->e_origindex) &&
         (mr->e_origindex[edge_idx] == ORIGINDEX_NONE)))) {

    int loopend = mpoly->totloop + mpoly->loopstart - 1;
    int other_loop = (l == loopend) ? mpoly->loopstart : (l + 1);
    if (mpoly->flag & ME_FACE_SEL) {
      if (BLI_BITMAP_TEST_AND_SET_ATOMIC(data->select_map, edge_idx)) {
        /* Hide edge as it has more than 2 selected loop. */
        GPU_indexbuf_set_line_restart(&data->elb, edge_idx);
      }
      else {
        /* First selected loop. Set edge visible, overwritting any unsel loop. */
        GPU_indexbuf_set_line_verts(&data->elb, edge_idx, l, other_loop);
      }
    }
    else {
      /* Set theses unselected loop only if this edge has no other selected loop. */
      if (!BLI_BITMAP_TEST(data->select_map, edge_idx)) {
        GPU_indexbuf_set_line_verts(&data->elb, edge_idx, l, other_loop);
      }
    }
  }
  else {
    GPU_indexbuf_set_line_restart(&data->elb, edge_idx);
  }
}
static void extract_lines_paint_mask_finish(const MeshRenderData *UNUSED(mr),
                                            void *ibo,
                                            void *_data)
{
  MeshExtract_LinePaintMask_Data *data = (MeshExtract_LinePaintMask_Data *)_data;

  GPU_indexbuf_build_in_place(&data->elb, ibo);
  MEM_freeN(data);
}

static const MeshExtract extract_lines_paint_mask = {
    extract_lines_paint_mask_init,
    NULL,
    NULL,
    NULL,
    extract_lines_paint_mask_loop_mesh,
    NULL,
    NULL,
    NULL,
    NULL,
    extract_lines_paint_mask_finish,
    0,
    false,
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Line Adjacency Indices
 * \{ */

#define NO_EDGE INT_MAX

typedef struct MeshExtract_LineAdjacency_Data {
  GPUIndexBufBuilder elb;
  EdgeHash *eh;
  bool is_manifold;
  /* Array to convert vert index to any loop index of this vert. */
  uint vert_to_loop[0];
} MeshExtract_LineAdjacency_Data;

static void *extract_lines_adjacency_init(const MeshRenderData *mr, void *UNUSED(buf))
{
  /* Similar to poly_to_tri_count().
   * There is always loop + tri - 1 edges inside a polygon.
   * Accumulate for all polys and you get : */
  uint tess_edge_len = mr->loop_len + mr->tri_len - mr->poly_len;

  size_t vert_to_loop_size = sizeof(uint) * mr->vert_len;

  MeshExtract_LineAdjacency_Data *data = MEM_callocN(sizeof(*data) + vert_to_loop_size, __func__);
  GPU_indexbuf_init(&data->elb, GPU_PRIM_LINES_ADJ, tess_edge_len, mr->loop_len);
  data->eh = BLI_edgehash_new_ex(__func__, tess_edge_len);
  data->is_manifold = true;
  return data;
}

BLI_INLINE void lines_adjacency_triangle(
    uint v1, uint v2, uint v3, uint l1, uint l2, uint l3, MeshExtract_LineAdjacency_Data *data)
{
  GPUIndexBufBuilder *elb = &data->elb;
  /* Iter around the triangle's edges. */
  for (int e = 0; e < 3; e++) {
    SHIFT3(uint, v3, v2, v1);
    SHIFT3(uint, l3, l2, l1);

    bool inv_indices = (v2 > v3);
    void **pval;
    bool value_is_init = BLI_edgehash_ensure_p(data->eh, v2, v3, &pval);
    int v_data = POINTER_AS_INT(*pval);
    if (!value_is_init || v_data == NO_EDGE) {
      /* Save the winding order inside the sign bit. Because the
       * edgehash sort the keys and we need to compare winding later. */
      int value = (int)l1 + 1; /* 0 cannot be signed so add one. */
      *pval = POINTER_FROM_INT((inv_indices) ? -value : value);
      /* Store loop indices for remaining non-manifold edges. */
      data->vert_to_loop[v2] = l2;
      data->vert_to_loop[v3] = l3;
    }
    else {
      /* HACK Tag as not used. Prevent overhead of BLI_edgehash_remove. */
      *pval = POINTER_FROM_INT(NO_EDGE);
      bool inv_opposite = (v_data < 0);
      uint l_opposite = (uint)abs(v_data) - 1;
      /* TODO Make this part threadsafe. */
      if (inv_opposite == inv_indices) {
        /* Don't share edge if triangles have non matching winding. */
        GPU_indexbuf_add_line_adj_verts(elb, l1, l2, l3, l1);
        GPU_indexbuf_add_line_adj_verts(elb, l_opposite, l2, l3, l_opposite);
        data->is_manifold = false;
      }
      else {
        GPU_indexbuf_add_line_adj_verts(elb, l1, l2, l3, l_opposite);
      }
    }
  }
}

static void extract_lines_adjacency_looptri_bmesh(const MeshRenderData *UNUSED(mr),
                                                  int UNUSED(t),
                                                  BMLoop **elt,
                                                  void *data)
{
  if (!BM_elem_flag_test(elt[0]->f, BM_ELEM_HIDDEN)) {
    lines_adjacency_triangle(BM_elem_index_get(elt[0]->v),
                             BM_elem_index_get(elt[1]->v),
                             BM_elem_index_get(elt[2]->v),
                             BM_elem_index_get(elt[0]),
                             BM_elem_index_get(elt[1]),
                             BM_elem_index_get(elt[2]),
                             data);
  }
}

static void extract_lines_adjacency_looptri_mesh(const MeshRenderData *mr,
                                                 int UNUSED(t),
                                                 const MLoopTri *mlt,
                                                 void *data)
{
  const MPoly *mpoly = &mr->mpoly[mlt->poly];
  if (!(mr->use_hide && (mpoly->flag & ME_HIDE))) {
    lines_adjacency_triangle(mr->mloop[mlt->tri[0]].v,
                             mr->mloop[mlt->tri[1]].v,
                             mr->mloop[mlt->tri[2]].v,
                             mlt->tri[0],
                             mlt->tri[1],
                             mlt->tri[2],
                             data);
  }
}

static void extract_lines_adjacency_finish(const MeshRenderData *mr, void *ibo, void *_data)
{
  MeshExtract_LineAdjacency_Data *data = (MeshExtract_LineAdjacency_Data *)_data;
  /* Create edges for remaining non manifold edges. */
  EdgeHashIterator *ehi = BLI_edgehashIterator_new(data->eh);
  for (; !BLI_edgehashIterator_isDone(ehi); BLI_edgehashIterator_step(ehi)) {
    uint v2, v3, l1, l2, l3;
    int v_data = POINTER_AS_INT(BLI_edgehashIterator_getValue(ehi));
    if (v_data != NO_EDGE) {
      BLI_edgehashIterator_getKey(ehi, &v2, &v3);
      l1 = (uint)abs(v_data) - 1;
      if (v_data < 0) { /* inv_opposite  */
        SWAP(uint, v2, v3);
      }
      l2 = data->vert_to_loop[v2];
      l3 = data->vert_to_loop[v3];
      GPU_indexbuf_add_line_adj_verts(&data->elb, l1, l2, l3, l1);
      data->is_manifold = false;
    }
  }
  BLI_edgehashIterator_free(ehi);
  BLI_edgehash_free(data->eh, NULL);

  mr->cache->is_manifold = data->is_manifold;

  GPU_indexbuf_build_in_place(&data->elb, ibo);
  MEM_freeN(data);
}

#undef NO_EDGE

static const MeshExtract extract_lines_adjacency = {
    extract_lines_adjacency_init,
    extract_lines_adjacency_looptri_bmesh,
    extract_lines_adjacency_looptri_mesh,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    extract_lines_adjacency_finish,
    0,
    false,
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Edit UV Triangles Indices
 * \{ */

typedef struct MeshExtract_EditUvElem_Data {
  GPUIndexBufBuilder elb;
  bool sync_selection;
} MeshExtract_EditUvElem_Data;

static void *extract_edituv_tris_init(const MeshRenderData *mr, void *UNUSED(ibo))
{
  MeshExtract_EditUvElem_Data *data = MEM_callocN(sizeof(*data), __func__);
  GPU_indexbuf_init(&data->elb, GPU_PRIM_TRIS, mr->tri_len, mr->loop_len);
  data->sync_selection = (mr->toolsettings->uv_flag & UV_SYNC_SELECTION) != 0;
  return data;
}

BLI_INLINE void edituv_tri_add(
    MeshExtract_EditUvElem_Data *data, bool hidden, bool selected, int v1, int v2, int v3)
{
  if (!hidden && (data->sync_selection || selected)) {
    GPU_indexbuf_add_tri_verts(&data->elb, v1, v2, v3);
  }
}

static void extract_edituv_tris_looptri_bmesh(const MeshRenderData *UNUSED(mr),
                                              int UNUSED(t),
                                              BMLoop **elt,
                                              void *data)
{
  edituv_tri_add(data,
                 BM_elem_flag_test(elt[0]->f, BM_ELEM_HIDDEN),
                 BM_elem_flag_test(elt[0]->f, BM_ELEM_SELECT),
                 BM_elem_index_get(elt[0]),
                 BM_elem_index_get(elt[1]),
                 BM_elem_index_get(elt[2]));
}

static void extract_edituv_tris_looptri_mesh(const MeshRenderData *mr,
                                             int UNUSED(t),
                                             const MLoopTri *mlt,
                                             void *data)
{
  const MPoly *mpoly = &mr->mpoly[mlt->poly];
  edituv_tri_add(data,
                 (mpoly->flag & ME_HIDE) != 0,
                 (mpoly->flag & ME_FACE_SEL) != 0,
                 mlt->tri[0],
                 mlt->tri[1],
                 mlt->tri[2]);
}

static void extract_edituv_tris_finish(const MeshRenderData *UNUSED(mr), void *ibo, void *data)
{
  MeshExtract_EditUvElem_Data *extract_data = (MeshExtract_EditUvElem_Data *)data;
  GPU_indexbuf_build_in_place(&extract_data->elb, ibo);
  MEM_freeN(extract_data);
}

static const MeshExtract extract_edituv_tris = {
    extract_edituv_tris_init,
    extract_edituv_tris_looptri_bmesh,
    extract_edituv_tris_looptri_mesh,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    extract_edituv_tris_finish,
    0,
    false,
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Edit UV Line Indices around faces
 * \{ */

static void *extract_edituv_lines_init(const MeshRenderData *mr, void *UNUSED(ibo))
{
  MeshExtract_EditUvElem_Data *data = MEM_callocN(sizeof(*data), __func__);
  GPU_indexbuf_init(&data->elb, GPU_PRIM_LINES, mr->loop_len, mr->loop_len);

  data->sync_selection = (mr->toolsettings->uv_flag & UV_SYNC_SELECTION) != 0;
  return data;
}

BLI_INLINE void edituv_edge_add(
    MeshExtract_EditUvElem_Data *data, bool hidden, bool selected, int v1, int v2)
{
  if (!hidden && (data->sync_selection || selected)) {
    GPU_indexbuf_add_line_verts(&data->elb, v1, v2);
  }
}

static void extract_edituv_lines_loop_bmesh(const MeshRenderData *UNUSED(mr),
                                            int l,
                                            BMLoop *loop,
                                            void *data)
{
  edituv_edge_add(data,
                  BM_elem_flag_test(loop->f, BM_ELEM_HIDDEN),
                  BM_elem_flag_test(loop->f, BM_ELEM_SELECT),
                  l,
                  BM_elem_index_get(loop->next));
}

static void extract_edituv_lines_loop_mesh(const MeshRenderData *mr,
                                           int loop_idx,
                                           const MLoop *mloop,
                                           int UNUSED(p),
                                           const MPoly *mpoly,
                                           void *data)
{
  int loopend = mpoly->totloop + mpoly->loopstart - 1;
  int loop_next_idx = (loop_idx == loopend) ? mpoly->loopstart : (loop_idx + 1);
  const bool real_edge = (mr->e_origindex == NULL || mr->e_origindex[mloop->e] != ORIGINDEX_NONE);
  edituv_edge_add(data,
                  (mpoly->flag & ME_HIDE) != 0 || !real_edge,
                  (mpoly->flag & ME_FACE_SEL) != 0,
                  loop_idx,
                  loop_next_idx);
}

static void extract_edituv_lines_finish(const MeshRenderData *UNUSED(mr), void *ibo, void *data)
{
  MeshExtract_EditUvElem_Data *extract_data = (MeshExtract_EditUvElem_Data *)data;
  GPU_indexbuf_build_in_place(&extract_data->elb, ibo);
  MEM_freeN(extract_data);
}

static const MeshExtract extract_edituv_lines = {
    extract_edituv_lines_init,
    NULL,
    NULL,
    extract_edituv_lines_loop_bmesh,
    extract_edituv_lines_loop_mesh,
    NULL,
    NULL,
    NULL,
    NULL,
    extract_edituv_lines_finish,
    0,
    false,
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Edit UV Points Indices
 * \{ */

static void *extract_edituv_points_init(const MeshRenderData *mr, void *UNUSED(ibo))
{
  MeshExtract_EditUvElem_Data *data = MEM_callocN(sizeof(*data), __func__);
  GPU_indexbuf_init(&data->elb, GPU_PRIM_POINTS, mr->loop_len, mr->loop_len);

  data->sync_selection = (mr->toolsettings->uv_flag & UV_SYNC_SELECTION) != 0;
  return data;
}

BLI_INLINE void edituv_point_add(MeshExtract_EditUvElem_Data *data,
                                 bool hidden,
                                 bool selected,
                                 int v1)
{
  if (!hidden && (data->sync_selection || selected)) {
    GPU_indexbuf_add_point_vert(&data->elb, v1);
  }
}

static void extract_edituv_points_loop_bmesh(const MeshRenderData *UNUSED(mr),
                                             int l,
                                             BMLoop *loop,
                                             void *data)
{
  edituv_point_add(data,
                   BM_elem_flag_test(loop->f, BM_ELEM_HIDDEN),
                   BM_elem_flag_test(loop->f, BM_ELEM_SELECT),
                   l);
}

static void extract_edituv_points_loop_mesh(const MeshRenderData *mr,
                                            int l,
                                            const MLoop *mloop,
                                            int UNUSED(p),
                                            const MPoly *mpoly,
                                            void *data)
{
  const bool real_vert = (mr->extract_type == MR_EXTRACT_MAPPED && (mr->v_origindex) &&
                          mr->v_origindex[mloop->v] != ORIGINDEX_NONE);
  edituv_point_add(
      data, ((mpoly->flag & ME_HIDE) != 0) || !real_vert, (mpoly->flag & ME_FACE_SEL) != 0, l);
}

static void extract_edituv_points_finish(const MeshRenderData *UNUSED(mr), void *ibo, void *data)
{
  MeshExtract_EditUvElem_Data *extract_data = (MeshExtract_EditUvElem_Data *)data;
  GPU_indexbuf_build_in_place(&extract_data->elb, ibo);
  MEM_freeN(extract_data);
}

static const MeshExtract extract_edituv_points = {
    extract_edituv_points_init,
    NULL,
    NULL,
    extract_edituv_points_loop_bmesh,
    extract_edituv_points_loop_mesh,
    NULL,
    NULL,
    NULL,
    NULL,
    extract_edituv_points_finish,
    0,
    false,
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Edit UV Facedots Indices
 * \{ */

static void *extract_edituv_fdots_init(const MeshRenderData *mr, void *UNUSED(ibo))
{
  MeshExtract_EditUvElem_Data *data = MEM_callocN(sizeof(*data), __func__);
  GPU_indexbuf_init(&data->elb, GPU_PRIM_POINTS, mr->poly_len, mr->poly_len);

  data->sync_selection = (mr->toolsettings->uv_flag & UV_SYNC_SELECTION) != 0;
  return data;
}

BLI_INLINE void edituv_facedot_add(MeshExtract_EditUvElem_Data *data,
                                   bool hidden,
                                   bool selected,
                                   int face_idx)
{
  if (!hidden && (data->sync_selection || selected)) {
    GPU_indexbuf_set_point_vert(&data->elb, face_idx, face_idx);
  }
  else {
    GPU_indexbuf_set_point_restart(&data->elb, face_idx);
  }
}

static void extract_edituv_fdots_loop_bmesh(const MeshRenderData *UNUSED(mr),
                                            int UNUSED(l),
                                            BMLoop *loop,
                                            void *data)
{
  edituv_facedot_add(data,
                     BM_elem_flag_test(loop->f, BM_ELEM_HIDDEN),
                     BM_elem_flag_test(loop->f, BM_ELEM_SELECT),
                     BM_elem_index_get(loop->f));
}

static void extract_edituv_fdots_loop_mesh(const MeshRenderData *mr,
                                           int UNUSED(l),
                                           const MLoop *mloop,
                                           int p,
                                           const MPoly *mpoly,
                                           void *data)
{
  const bool real_fdot = (mr->extract_type == MR_EXTRACT_MAPPED && mr->p_origindex &&
                          mr->p_origindex[p] != ORIGINDEX_NONE);
  const bool subd_fdot = (!mr->use_subsurf_fdots ||
                          (mr->mvert[mloop->v].flag & ME_VERT_FACEDOT) != 0);
  edituv_facedot_add(data,
                     ((mpoly->flag & ME_HIDE) != 0) || !real_fdot || !subd_fdot,
                     (mpoly->flag & ME_FACE_SEL) != 0,
                     p);
}

static void extract_edituv_fdots_finish(const MeshRenderData *UNUSED(mr), void *ibo, void *_data)
{
  MeshExtract_EditUvElem_Data *data = (MeshExtract_EditUvElem_Data *)_data;
  GPU_indexbuf_build_in_place(&data->elb, ibo);
  MEM_freeN(data);
}

static const MeshExtract extract_edituv_fdots = {
    extract_edituv_fdots_init,
    NULL,
    NULL,
    extract_edituv_fdots_loop_bmesh,
    extract_edituv_fdots_loop_mesh,
    NULL,
    NULL,
    NULL,
    NULL,
    extract_edituv_fdots_finish,
    0,
    false,
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Position and Vertex Normal
 * \{ */

typedef struct PosNorLoop {
  float pos[3];
  GPUPackedNormal nor;
} PosNorLoop;

typedef struct MeshExtract_PosNor_Data {
  PosNorLoop *vbo_data;
  GPUPackedNormal packed_nor[];
} MeshExtract_PosNor_Data;

static void *extract_pos_nor_init(const MeshRenderData *mr, void *buf)
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    /* WARNING Adjust PosNorLoop struct accordingly. */
    GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    GPU_vertformat_attr_add(&format, "nor", GPU_COMP_I10, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
    GPU_vertformat_alias_add(&format, "vnor");
  }
  GPUVertBuf *vbo = buf;
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len + mr->loop_loose_len);

  /* Pack normals per vert, reduce amount of computation. */
  size_t packed_nor_len = sizeof(GPUPackedNormal) * mr->vert_len;
  MeshExtract_PosNor_Data *data = MEM_mallocN(sizeof(*data) + packed_nor_len, __func__);
  data->vbo_data = (PosNorLoop *)vbo->data;

  /* Quicker than doing it for each loop. */
  if (mr->extract_type == MR_EXTRACT_BMESH) {
    BMIter iter;
    BMVert *eve;
    int v;
    BM_ITER_MESH_INDEX (eve, &iter, mr->bm, BM_VERTS_OF_MESH, v) {
      data->packed_nor[v] = GPU_normal_convert_i10_v3(bm_vert_no_get(mr, eve));
    }
  }
  else {
    const MVert *mvert = mr->mvert;
    for (int v = 0; v < mr->vert_len; v++, mvert++) {
      data->packed_nor[v] = GPU_normal_convert_i10_s3(mvert->no);
    }
  }
  return data;
}

static void extract_pos_nor_loop_bmesh(const MeshRenderData *mr, int l, BMLoop *loop, void *_data)
{
  MeshExtract_PosNor_Data *data = _data;
  PosNorLoop *vert = data->vbo_data + l;
  copy_v3_v3(vert->pos, bm_vert_co_get(mr, loop->v));
  vert->nor = data->packed_nor[BM_elem_index_get(loop->v)];
  BMFace *efa = loop->f;
  vert->nor.w = BM_elem_flag_test(efa, BM_ELEM_HIDDEN) ? -1 : 0;
}

static void extract_pos_nor_loop_mesh(const MeshRenderData *mr,
                                      int l,
                                      const MLoop *mloop,
                                      int UNUSED(p),
                                      const MPoly *mpoly,
                                      void *_data)
{
  MeshExtract_PosNor_Data *data = _data;
  PosNorLoop *vert = data->vbo_data + l;
  const MVert *mvert = &mr->mvert[mloop->v];
  copy_v3_v3(vert->pos, mvert->co);
  vert->nor = data->packed_nor[mloop->v];
  /* Flag for paint mode overlay. */
  if (mpoly->flag & ME_HIDE || mvert->flag & ME_HIDE ||
      ((mr->extract_type == MR_EXTRACT_MAPPED) && (mr->v_origindex) &&
       (mr->v_origindex[mloop->v] == ORIGINDEX_NONE))) {
    vert->nor.w = -1;
  }
  else if (mvert->flag & SELECT) {
    vert->nor.w = 1;
  }
  else {
    vert->nor.w = 0;
  }
}

static void extract_pos_nor_ledge_bmesh(const MeshRenderData *mr, int e, BMEdge *eed, void *_data)
{
  int l = mr->loop_len + e * 2;
  MeshExtract_PosNor_Data *data = _data;
  PosNorLoop *vert = data->vbo_data + l;
  copy_v3_v3(vert[0].pos, bm_vert_co_get(mr, eed->v1));
  copy_v3_v3(vert[1].pos, bm_vert_co_get(mr, eed->v2));
  vert[0].nor = data->packed_nor[BM_elem_index_get(eed->v1)];
  vert[1].nor = data->packed_nor[BM_elem_index_get(eed->v2)];
}

static void extract_pos_nor_ledge_mesh(const MeshRenderData *mr,
                                       int e,
                                       const MEdge *medge,
                                       void *_data)
{
  int l = mr->loop_len + e * 2;
  MeshExtract_PosNor_Data *data = _data;
  PosNorLoop *vert = data->vbo_data + l;
  copy_v3_v3(vert[0].pos, mr->mvert[medge->v1].co);
  copy_v3_v3(vert[1].pos, mr->mvert[medge->v2].co);
  vert[0].nor = data->packed_nor[medge->v1];
  vert[1].nor = data->packed_nor[medge->v2];
}

static void extract_pos_nor_lvert_bmesh(const MeshRenderData *mr, int v, BMVert *eve, void *_data)
{
  int l = mr->loop_len + mr->edge_loose_len * 2 + v;
  MeshExtract_PosNor_Data *data = _data;
  PosNorLoop *vert = data->vbo_data + l;
  copy_v3_v3(vert->pos, bm_vert_co_get(mr, eve));
  vert->nor = data->packed_nor[BM_elem_index_get(eve)];
}

static void extract_pos_nor_lvert_mesh(const MeshRenderData *mr,
                                       int v,
                                       const MVert *mvert,
                                       void *_data)
{
  int l = mr->loop_len + mr->edge_loose_len * 2 + v;
  int v_idx = mr->lverts[v];
  MeshExtract_PosNor_Data *data = _data;
  PosNorLoop *vert = data->vbo_data + l;
  copy_v3_v3(vert->pos, mvert->co);
  vert->nor = data->packed_nor[v_idx];
}

static void extract_pos_nor_finish(const MeshRenderData *UNUSED(mr), void *UNUSED(vbo), void *data)
{
  MEM_freeN(data);
}

static const MeshExtract extract_pos_nor = {
    extract_pos_nor_init,
    NULL,
    NULL,
    extract_pos_nor_loop_bmesh,
    extract_pos_nor_loop_mesh,
    extract_pos_nor_ledge_bmesh,
    extract_pos_nor_ledge_mesh,
    extract_pos_nor_lvert_bmesh,
    extract_pos_nor_lvert_mesh,
    extract_pos_nor_finish,
    0,
    true,
};
/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract HQ Loop Normal
 * \{ */

typedef struct gpuHQNor {
  short x, y, z, w;
} gpuHQNor;

static void *extract_lnor_hq_init(const MeshRenderData *mr, void *buf)
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "nor", GPU_COMP_I16, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
    GPU_vertformat_alias_add(&format, "lnor");
  }
  GPUVertBuf *vbo = buf;
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len);

  return vbo->data;
}

static void extract_lnor_hq_loop_bmesh(const MeshRenderData *mr, int l, BMLoop *loop, void *data)
{
  if (mr->loop_normals) {
    normal_float_to_short_v3(&((gpuHQNor *)data)[l].x, mr->loop_normals[l]);
  }
  else if (BM_elem_flag_test(loop->f, BM_ELEM_SMOOTH)) {
    normal_float_to_short_v3(&((gpuHQNor *)data)[l].x, bm_vert_no_get(mr, loop->v));
  }
  else {
    normal_float_to_short_v3(&((gpuHQNor *)data)[l].x, bm_face_no_get(mr, loop->f));
  }
}

static void extract_lnor_hq_loop_mesh(
    const MeshRenderData *mr, int l, const MLoop *mloop, int p, const MPoly *mpoly, void *data)
{
  gpuHQNor *lnor_data = &((gpuHQNor *)data)[l];
  if (mr->loop_normals) {
    normal_float_to_short_v3(&lnor_data->x, mr->loop_normals[l]);
  }
  else if (mpoly->flag & ME_SMOOTH) {
    copy_v3_v3_short(&lnor_data->x, mr->mvert[mloop->v].no);
  }
  else {
    normal_float_to_short_v3(&lnor_data->x, mr->poly_normals[p]);
  }

  /* Flag for paint mode overlay.
   * Only use MR_EXTRACT_MAPPED in edit mode where it is used to display the edge-normals. In
   * paint mode it will use the unmapped data to draw the wireframe. */
  if (mpoly->flag & ME_HIDE ||
      (mr->edit_bmesh && mr->extract_type == MR_EXTRACT_MAPPED && (mr->v_origindex) &&
       mr->v_origindex[mloop->v] == ORIGINDEX_NONE)) {
    lnor_data->w = -1;
  }
  else if (mpoly->flag & ME_FACE_SEL) {
    lnor_data->w = 1;
  }
  else {
    lnor_data->w = 0;
  }
}

static const MeshExtract extract_lnor_hq = {
    extract_lnor_hq_init,
    NULL,
    NULL,
    extract_lnor_hq_loop_bmesh,
    extract_lnor_hq_loop_mesh,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    MR_DATA_LOOP_NOR,
    true,
};

/** \} */
/* ---------------------------------------------------------------------- */
/** \name Extract Loop Normal
 * \{ */

static void *extract_lnor_init(const MeshRenderData *mr, void *buf)
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "nor", GPU_COMP_I10, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
    GPU_vertformat_alias_add(&format, "lnor");
  }
  GPUVertBuf *vbo = buf;
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len);

  return vbo->data;
}

static void extract_lnor_loop_bmesh(const MeshRenderData *mr, int l, BMLoop *loop, void *data)
{
  if (mr->loop_normals) {
    ((GPUPackedNormal *)data)[l] = GPU_normal_convert_i10_v3(mr->loop_normals[l]);
  }
  else if (BM_elem_flag_test(loop->f, BM_ELEM_SMOOTH)) {
    ((GPUPackedNormal *)data)[l] = GPU_normal_convert_i10_v3(bm_vert_no_get(mr, loop->v));
  }
  else {
    ((GPUPackedNormal *)data)[l] = GPU_normal_convert_i10_v3(bm_face_no_get(mr, loop->f));
  }
  BMFace *efa = loop->f;
  ((GPUPackedNormal *)data)[l].w = BM_elem_flag_test(efa, BM_ELEM_HIDDEN) ? -1 : 0;
}

static void extract_lnor_loop_mesh(
    const MeshRenderData *mr, int l, const MLoop *mloop, int p, const MPoly *mpoly, void *data)
{
  GPUPackedNormal *lnor_data = &((GPUPackedNormal *)data)[l];
  if (mr->loop_normals) {
    *lnor_data = GPU_normal_convert_i10_v3(mr->loop_normals[l]);
  }
  else if (mpoly->flag & ME_SMOOTH) {
    *lnor_data = GPU_normal_convert_i10_s3(mr->mvert[mloop->v].no);
  }
  else {
    *lnor_data = GPU_normal_convert_i10_v3(mr->poly_normals[p]);
  }

  /* Flag for paint mode overlay.
   * Only use MR_EXTRACT_MAPPED in edit mode where it is used to display the edge-normals. In
   * paint mode it will use the unmapped data to draw the wireframe. */
  if (mpoly->flag & ME_HIDE ||
      (mr->edit_bmesh && mr->extract_type == MR_EXTRACT_MAPPED && (mr->v_origindex) &&
       mr->v_origindex[mloop->v] == ORIGINDEX_NONE)) {
    lnor_data->w = -1;
  }
  else if (mpoly->flag & ME_FACE_SEL) {
    lnor_data->w = 1;
  }
  else {
    lnor_data->w = 0;
  }
}

static const MeshExtract extract_lnor = {
    extract_lnor_init,
    NULL,
    NULL,
    extract_lnor_loop_bmesh,
    extract_lnor_loop_mesh,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    MR_DATA_LOOP_NOR,
    true,
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract UV  layers
 * \{ */

static void *extract_uv_init(const MeshRenderData *mr, void *buf)
{
  GPUVertFormat format = {0};
  GPU_vertformat_deinterleave(&format);

  CustomData *cd_ldata = (mr->extract_type == MR_EXTRACT_BMESH) ? &mr->bm->ldata : &mr->me->ldata;
  uint32_t uv_layers = mr->cache->cd_used.uv;

  /* HACK to fix T68857 */
  if (mr->extract_type == MR_EXTRACT_BMESH && mr->cache->cd_used.edit_uv == 1) {
    int layer = CustomData_get_active_layer(cd_ldata, CD_MLOOPUV);
    if (layer != -1) {
      uv_layers |= (1 << layer);
    }
  }

  for (int i = 0; i < MAX_MTFACE; i++) {
    if (uv_layers & (1 << i)) {
      char attr_name[32], attr_safe_name[GPU_MAX_SAFE_ATTR_NAME];
      const char *layer_name = CustomData_get_layer_name(cd_ldata, CD_MLOOPUV, i);

      GPU_vertformat_safe_attr_name(layer_name, attr_safe_name, GPU_MAX_SAFE_ATTR_NAME);
      /* UV layer name. */
      BLI_snprintf(attr_name, sizeof(attr_name), "u%s", attr_safe_name);
      GPU_vertformat_attr_add(&format, attr_name, GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
      /* Auto layer name. */
      BLI_snprintf(attr_name, sizeof(attr_name), "a%s", attr_safe_name);
      GPU_vertformat_alias_add(&format, attr_name);
      /* Active render layer name. */
      if (i == CustomData_get_render_layer(cd_ldata, CD_MLOOPUV)) {
        GPU_vertformat_alias_add(&format, "u");
      }
      /* Active display layer name. */
      if (i == CustomData_get_active_layer(cd_ldata, CD_MLOOPUV)) {
        GPU_vertformat_alias_add(&format, "au");
        /* Alias to pos for edit uvs. */
        GPU_vertformat_alias_add(&format, "pos");
      }
      /* Stencil mask uv layer name. */
      if (i == CustomData_get_stencil_layer(cd_ldata, CD_MLOOPUV)) {
        GPU_vertformat_alias_add(&format, "mu");
      }
    }
  }

  int v_len = mr->loop_len;
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "dummy", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    /* VBO will not be used, only allocate minimum of memory. */
    v_len = 1;
  }

  GPUVertBuf *vbo = buf;
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, v_len);

  float(*uv_data)[2] = (float(*)[2])vbo->data;
  for (int i = 0; i < MAX_MTFACE; i++) {
    if (uv_layers & (1 << i)) {
      if (mr->extract_type == MR_EXTRACT_BMESH) {
        int cd_ofs = CustomData_get_n_offset(cd_ldata, CD_MLOOPUV, i);
        BMIter f_iter, l_iter;
        BMFace *efa;
        BMLoop *loop;
        BM_ITER_MESH (efa, &f_iter, mr->bm, BM_FACES_OF_MESH) {
          BM_ITER_ELEM (loop, &l_iter, efa, BM_LOOPS_OF_FACE) {
            MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(loop, cd_ofs);
            memcpy(uv_data, luv->uv, sizeof(*uv_data));
            uv_data++;
          }
        }
      }
      else {
        MLoopUV *layer_data = CustomData_get_layer_n(cd_ldata, CD_MLOOPUV, i);
        for (int l = 0; l < mr->loop_len; l++, uv_data++, layer_data++) {
          memcpy(uv_data, layer_data->uv, sizeof(*uv_data));
        }
      }
    }
  }

  return NULL;
}

static const MeshExtract extract_uv = {
    extract_uv_init,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    0,
    false,
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Tangent layers
 * \{ */

static void extract_tan_ex(const MeshRenderData *mr, GPUVertBuf *vbo, const bool do_hq)
{
  GPUVertCompType comp_type = do_hq ? GPU_COMP_I16 : GPU_COMP_I10;
  GPUVertFetchMode fetch_mode = GPU_FETCH_INT_TO_FLOAT_UNIT;

  GPUVertFormat format = {0};
  GPU_vertformat_deinterleave(&format);

  CustomData *cd_ldata = (mr->extract_type == MR_EXTRACT_BMESH) ? &mr->bm->ldata : &mr->me->ldata;
  CustomData *cd_vdata = (mr->extract_type == MR_EXTRACT_BMESH) ? &mr->bm->vdata : &mr->me->vdata;
  uint32_t tan_layers = mr->cache->cd_used.tan;
  float(*orco)[3] = CustomData_get_layer(cd_vdata, CD_ORCO);
  bool orco_allocated = false;
  const bool use_orco_tan = mr->cache->cd_used.tan_orco != 0;

  int tan_len = 0;
  char tangent_names[MAX_MTFACE][MAX_CUSTOMDATA_LAYER_NAME];

  for (int i = 0; i < MAX_MTFACE; i++) {
    if (tan_layers & (1 << i)) {
      char attr_name[32], attr_safe_name[GPU_MAX_SAFE_ATTR_NAME];
      const char *layer_name = CustomData_get_layer_name(cd_ldata, CD_MLOOPUV, i);
      GPU_vertformat_safe_attr_name(layer_name, attr_safe_name, GPU_MAX_SAFE_ATTR_NAME);
      /* Tangent layer name. */
      BLI_snprintf(attr_name, sizeof(attr_name), "t%s", attr_safe_name);
      GPU_vertformat_attr_add(&format, attr_name, comp_type, 4, fetch_mode);
      /* Active render layer name. */
      if (i == CustomData_get_render_layer(cd_ldata, CD_MLOOPUV)) {
        GPU_vertformat_alias_add(&format, "t");
      }
      /* Active display layer name. */
      if (i == CustomData_get_active_layer(cd_ldata, CD_MLOOPUV)) {
        GPU_vertformat_alias_add(&format, "at");
      }

      BLI_strncpy(tangent_names[tan_len++], layer_name, MAX_CUSTOMDATA_LAYER_NAME);
    }
  }
  if (use_orco_tan && orco == NULL) {
    /* If orco is not available compute it ourselves */
    orco_allocated = true;
    orco = MEM_mallocN(sizeof(*orco) * mr->vert_len, __func__);

    if (mr->extract_type == MR_EXTRACT_BMESH) {
      BMesh *bm = mr->bm;
      for (int v = 0; v < mr->vert_len; v++) {
        const BMVert *eve = BM_vert_at_index(bm, v);
        /* Exceptional case where #bm_vert_co_get can be avoided, as we want the original coords.
         * not the distorted ones. */
        copy_v3_v3(orco[v], eve->co);
      }
    }
    else {
      const MVert *mvert = mr->mvert;
      for (int v = 0; v < mr->vert_len; v++, mvert++) {
        copy_v3_v3(orco[v], mvert->co);
      }
    }
    BKE_mesh_orco_verts_transform(mr->me, orco, mr->vert_len, 0);
  }

  /* Start Fresh */
  CustomData_free_layers(cd_ldata, CD_TANGENT, mr->loop_len);

  if (tan_len != 0 || use_orco_tan) {
    short tangent_mask = 0;
    bool calc_active_tangent = false;
    if (mr->extract_type == MR_EXTRACT_BMESH) {
      BKE_editmesh_loop_tangent_calc(mr->edit_bmesh,
                                     calc_active_tangent,
                                     tangent_names,
                                     tan_len,
                                     mr->poly_normals,
                                     mr->loop_normals,
                                     orco,
                                     cd_ldata,
                                     mr->loop_len,
                                     &tangent_mask);
    }
    else {
      BKE_mesh_calc_loop_tangent_ex(mr->mvert,
                                    mr->mpoly,
                                    mr->poly_len,
                                    mr->mloop,
                                    mr->mlooptri,
                                    mr->tri_len,
                                    cd_ldata,
                                    calc_active_tangent,
                                    tangent_names,
                                    tan_len,
                                    mr->poly_normals,
                                    mr->loop_normals,
                                    orco,
                                    cd_ldata,
                                    mr->loop_len,
                                    &tangent_mask);
    }
  }

  if (use_orco_tan) {
    char attr_name[32], attr_safe_name[GPU_MAX_SAFE_ATTR_NAME];
    const char *layer_name = CustomData_get_layer_name(cd_ldata, CD_TANGENT, 0);
    GPU_vertformat_safe_attr_name(layer_name, attr_safe_name, GPU_MAX_SAFE_ATTR_NAME);
    BLI_snprintf(attr_name, sizeof(*attr_name), "t%s", attr_safe_name);
    GPU_vertformat_attr_add(&format, attr_name, comp_type, 4, fetch_mode);
    GPU_vertformat_alias_add(&format, "t");
    GPU_vertformat_alias_add(&format, "at");
  }

  if (orco_allocated) {
    MEM_SAFE_FREE(orco);
  }

  int v_len = mr->loop_len;
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "dummy", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    /* VBO will not be used, only allocate minimum of memory. */
    v_len = 1;
  }

  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, v_len);

  if (do_hq) {
    short(*tan_data)[4] = (short(*)[4])vbo->data;
    for (int i = 0; i < tan_len; i++) {
      const char *name = tangent_names[i];
      float(*layer_data)[4] = (float(*)[4])CustomData_get_layer_named(cd_ldata, CD_TANGENT, name);
      for (int l = 0; l < mr->loop_len; l++) {
        normal_float_to_short_v3(*tan_data, layer_data[l]);
        (*tan_data)[3] = (layer_data[l][3] > 0.0f) ? SHRT_MAX : SHRT_MIN;
        tan_data++;
      }
    }
    if (use_orco_tan) {
      float(*layer_data)[4] = (float(*)[4])CustomData_get_layer_n(cd_ldata, CD_TANGENT, 0);
      for (int l = 0; l < mr->loop_len; l++) {
        normal_float_to_short_v3(*tan_data, layer_data[l]);
        (*tan_data)[3] = (layer_data[l][3] > 0.0f) ? SHRT_MAX : SHRT_MIN;
        tan_data++;
      }
    }
  }
  else {
    GPUPackedNormal *tan_data = (GPUPackedNormal *)vbo->data;
    for (int i = 0; i < tan_len; i++) {
      const char *name = tangent_names[i];
      float(*layer_data)[4] = (float(*)[4])CustomData_get_layer_named(cd_ldata, CD_TANGENT, name);
      for (int l = 0; l < mr->loop_len; l++) {
        *tan_data = GPU_normal_convert_i10_v3(layer_data[l]);
        tan_data->w = (layer_data[l][3] > 0.0f) ? 1 : -2;
        tan_data++;
      }
    }
    if (use_orco_tan) {
      float(*layer_data)[4] = (float(*)[4])CustomData_get_layer_n(cd_ldata, CD_TANGENT, 0);
      for (int l = 0; l < mr->loop_len; l++) {
        *tan_data = GPU_normal_convert_i10_v3(layer_data[l]);
        tan_data->w = (layer_data[l][3] > 0.0f) ? 1 : -2;
        tan_data++;
      }
    }
  }

  CustomData_free_layers(cd_ldata, CD_TANGENT, mr->loop_len);
}

static void *extract_tan_init(const MeshRenderData *mr, void *buf)
{
  extract_tan_ex(mr, buf, false);
  return NULL;
}

static const MeshExtract extract_tan = {
    extract_tan_init,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    MR_DATA_POLY_NOR | MR_DATA_TAN_LOOP_NOR | MR_DATA_LOOPTRI,
    false,
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract HQ Tangent layers
 * \{ */

static void *extract_tan_hq_init(const MeshRenderData *mr, void *buf)
{
  extract_tan_ex(mr, buf, true);
  return NULL;
}

static const MeshExtract extract_tan_hq = {
    extract_tan_hq_init,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    MR_DATA_POLY_NOR | MR_DATA_TAN_LOOP_NOR | MR_DATA_LOOPTRI,
    false,
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract VCol
 * \{ */

static void *extract_vcol_init(const MeshRenderData *mr, void *buf)
{
  GPUVertFormat format = {0};
  GPU_vertformat_deinterleave(&format);

  CustomData *cd_ldata = (mr->extract_type == MR_EXTRACT_BMESH) ? &mr->bm->ldata : &mr->me->ldata;
  uint32_t vcol_layers = mr->cache->cd_used.vcol;

  for (int i = 0; i < MAX_MCOL; i++) {
    if (vcol_layers & (1 << i)) {
      char attr_name[32], attr_safe_name[GPU_MAX_SAFE_ATTR_NAME];
      const char *layer_name = CustomData_get_layer_name(cd_ldata, CD_MLOOPCOL, i);
      GPU_vertformat_safe_attr_name(layer_name, attr_safe_name, GPU_MAX_SAFE_ATTR_NAME);

      BLI_snprintf(attr_name, sizeof(attr_name), "c%s", attr_safe_name);
      GPU_vertformat_attr_add(&format, attr_name, GPU_COMP_U16, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);

      if (i == CustomData_get_render_layer(cd_ldata, CD_MLOOPCOL)) {
        GPU_vertformat_alias_add(&format, "c");
      }
      if (i == CustomData_get_active_layer(cd_ldata, CD_MLOOPCOL)) {
        GPU_vertformat_alias_add(&format, "ac");
      }
      /* Gather number of auto layers. */
      /* We only do vcols that are not overridden by uvs */
      if (CustomData_get_named_layer_index(cd_ldata, CD_MLOOPUV, layer_name) == -1) {
        BLI_snprintf(attr_name, sizeof(attr_name), "a%s", attr_safe_name);
        GPU_vertformat_alias_add(&format, attr_name);
      }
    }
  }
  GPUVertBuf *vbo = buf;
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len);

  typedef struct gpuMeshVcol {
    ushort r, g, b, a;
  } gpuMeshVcol;

  gpuMeshVcol *vcol_data = (gpuMeshVcol *)vbo->data;
  for (int i = 0; i < MAX_MCOL; i++) {
    if (vcol_layers & (1 << i)) {
      if (mr->extract_type == MR_EXTRACT_BMESH) {
        int cd_ofs = CustomData_get_n_offset(cd_ldata, CD_MLOOPCOL, i);
        BMIter f_iter, l_iter;
        BMFace *efa;
        BMLoop *loop;
        BM_ITER_MESH (efa, &f_iter, mr->bm, BM_FACES_OF_MESH) {
          BM_ITER_ELEM (loop, &l_iter, efa, BM_LOOPS_OF_FACE) {
            const MLoopCol *mloopcol = BM_ELEM_CD_GET_VOID_P(loop, cd_ofs);
            vcol_data->r = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mloopcol->r]);
            vcol_data->g = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mloopcol->g]);
            vcol_data->b = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mloopcol->b]);
            vcol_data->a = unit_float_to_ushort_clamp(mloopcol->a * (1.0f / 255.0f));
            vcol_data++;
          }
        }
      }
      else {
        const MLoopCol *mloopcol = (MLoopCol *)CustomData_get_layer_n(cd_ldata, CD_MLOOPCOL, i);
        for (int l = 0; l < mr->loop_len; l++, mloopcol++, vcol_data++) {
          vcol_data->r = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mloopcol->r]);
          vcol_data->g = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mloopcol->g]);
          vcol_data->b = unit_float_to_ushort_clamp(BLI_color_from_srgb_table[mloopcol->b]);
          vcol_data->a = unit_float_to_ushort_clamp(mloopcol->a * (1.0f / 255.0f));
        }
      }
    }
  }
  return NULL;
}

static const MeshExtract extract_vcol = {
    extract_vcol_init,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    0,
    false,
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Orco
 * \{ */

typedef struct MeshExtract_Orco_Data {
  float (*vbo_data)[4];
  float (*orco)[3];
} MeshExtract_Orco_Data;

static void *extract_orco_init(const MeshRenderData *mr, void *buf)
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    /* FIXME(fclem): We use the last component as a way to differentiate from generic vertex
     * attributes. This is a substantial waste of Vram and should be done another way.
     * Unfortunately, at the time of writing, I did not found any other "non disruptive"
     * alternative. */
    GPU_vertformat_attr_add(&format, "orco", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  }

  GPUVertBuf *vbo = buf;
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len);

  CustomData *cd_vdata = &mr->me->vdata;

  MeshExtract_Orco_Data *data = MEM_mallocN(sizeof(*data), __func__);
  data->vbo_data = (float(*)[4])vbo->data;
  data->orco = CustomData_get_layer(cd_vdata, CD_ORCO);
  /* Make sure orco layer was requested only if needed! */
  BLI_assert(data->orco);
  return data;
}

static void extract_orco_loop_bmesh(const MeshRenderData *UNUSED(mr),
                                    int l,
                                    BMLoop *loop,
                                    void *data)
{
  MeshExtract_Orco_Data *orco_data = (MeshExtract_Orco_Data *)data;
  float *loop_orco = orco_data->vbo_data[l];
  copy_v3_v3(loop_orco, orco_data->orco[BM_elem_index_get(loop->v)]);
  loop_orco[3] = 0.0; /* Tag as not a generic attribute. */
}

static void extract_orco_loop_mesh(const MeshRenderData *UNUSED(mr),
                                   int l,
                                   const MLoop *mloop,
                                   int UNUSED(p),
                                   const MPoly *UNUSED(mpoly),
                                   void *data)
{
  MeshExtract_Orco_Data *orco_data = (MeshExtract_Orco_Data *)data;
  float *loop_orco = orco_data->vbo_data[l];
  copy_v3_v3(loop_orco, orco_data->orco[mloop->v]);
  loop_orco[3] = 0.0; /* Tag as not a generic attribute. */
}

static void extract_orco_finish(const MeshRenderData *UNUSED(mr), void *UNUSED(buf), void *data)
{
  MEM_freeN(data);
}

static const MeshExtract extract_orco = {
    extract_orco_init,
    NULL,
    NULL,
    extract_orco_loop_bmesh,
    extract_orco_loop_mesh,
    NULL,
    NULL,
    NULL,
    NULL,
    extract_orco_finish,
    0,
    true,
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Edge Factor
 * Defines how much an edge is visible.
 * \{ */

typedef struct MeshExtract_EdgeFac_Data {
  uchar *vbo_data;
  bool use_edge_render;
  /* Number of loop per edge. */
  uchar edge_loop_count[0];
} MeshExtract_EdgeFac_Data;

static float loop_edge_factor_get(const float f_no[3],
                                  const float v_co[3],
                                  const float v_no[3],
                                  const float v_next_co[3])
{
  float enor[3], evec[3];
  sub_v3_v3v3(evec, v_next_co, v_co);
  cross_v3_v3v3(enor, v_no, evec);
  normalize_v3(enor);
  float d = fabsf(dot_v3v3(enor, f_no));
  /* Rescale to the slider range. */
  d *= (1.0f / 0.065f);
  CLAMP(d, 0.0f, 1.0f);
  return d;
}

static void *extract_edge_fac_init(const MeshRenderData *mr, void *buf)
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "wd", GPU_COMP_U8, 1, GPU_FETCH_INT_TO_FLOAT_UNIT);
  }
  GPUVertBuf *vbo = buf;
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len + mr->loop_loose_len);

  MeshExtract_EdgeFac_Data *data;

  if (mr->extract_type == MR_EXTRACT_MESH) {
    size_t edge_loop_count_size = sizeof(uint32_t) * mr->edge_len;
    data = MEM_callocN(sizeof(*data) + edge_loop_count_size, __func__);

    /* HACK(fclem) Detecting the need for edge render.
     * We could have a flag in the mesh instead or check the modifier stack. */
    const MEdge *medge = mr->medge;
    for (int e = 0; e < mr->edge_len; e++, medge++) {
      if ((medge->flag & ME_EDGERENDER) == 0) {
        data->use_edge_render = true;
        break;
      }
    }
  }
  else {
    data = MEM_callocN(sizeof(*data), __func__);
    /* HACK to bypass non-manifold check in mesh_edge_fac_finish(). */
    data->use_edge_render = true;
  }

  data->vbo_data = vbo->data;
  return data;
}

static void extract_edge_fac_loop_bmesh(const MeshRenderData *mr, int l, BMLoop *loop, void *_data)
{
  MeshExtract_EdgeFac_Data *data = (MeshExtract_EdgeFac_Data *)_data;
  if (BM_edge_is_manifold(loop->e)) {
    float ratio = loop_edge_factor_get(bm_face_no_get(mr, loop->f),
                                       bm_vert_co_get(mr, loop->v),
                                       bm_vert_no_get(mr, loop->v),
                                       bm_vert_co_get(mr, loop->next->v));
    data->vbo_data[l] = ratio * 253 + 1;
  }
  else {
    data->vbo_data[l] = 255;
  }
}

static void extract_edge_fac_loop_mesh(
    const MeshRenderData *mr, int l, const MLoop *mloop, int p, const MPoly *mpoly, void *_data)
{
  MeshExtract_EdgeFac_Data *data = (MeshExtract_EdgeFac_Data *)_data;
  if (data->use_edge_render) {
    const MEdge *medge = &mr->medge[mloop->e];
    data->vbo_data[l] = (medge->flag & ME_EDGERENDER) ? 255 : 0;
  }
  else {
    /* Count loop per edge to detect non-manifold. */
    if (data->edge_loop_count[mloop->e] < 3) {
      data->edge_loop_count[mloop->e]++;
    }
    if (data->edge_loop_count[mloop->e] == 2) {
      /* Manifold */
      int loopend = mpoly->totloop + mpoly->loopstart - 1;
      int other_loop = (l == loopend) ? mpoly->loopstart : (l + 1);
      const MLoop *mloop_next = &mr->mloop[other_loop];
      const MVert *v1 = &mr->mvert[mloop->v];
      const MVert *v2 = &mr->mvert[mloop_next->v];
      float vnor_f[3];
      normal_short_to_float_v3(vnor_f, v1->no);
      float ratio = loop_edge_factor_get(mr->poly_normals[p], v1->co, vnor_f, v2->co);
      data->vbo_data[l] = ratio * 253 + 1;
    }
    else {
      /* Non-manifold */
      data->vbo_data[l] = 255;
    }
  }
}

static void extract_edge_fac_ledge_bmesh(const MeshRenderData *mr,
                                         int e,
                                         BMEdge *UNUSED(eed),
                                         void *_data)
{
  MeshExtract_EdgeFac_Data *data = (MeshExtract_EdgeFac_Data *)_data;
  data->vbo_data[mr->loop_len + e * 2 + 0] = 255;
  data->vbo_data[mr->loop_len + e * 2 + 1] = 255;
}

static void extract_edge_fac_ledge_mesh(const MeshRenderData *mr,
                                        int e,
                                        const MEdge *UNUSED(edge),
                                        void *_data)
{
  MeshExtract_EdgeFac_Data *data = (MeshExtract_EdgeFac_Data *)_data;
  data->vbo_data[mr->loop_len + e * 2 + 0] = 255;
  data->vbo_data[mr->loop_len + e * 2 + 1] = 255;
}

static void extract_edge_fac_finish(const MeshRenderData *mr, void *buf, void *_data)
{
  MeshExtract_EdgeFac_Data *data = (MeshExtract_EdgeFac_Data *)_data;

  if (GPU_crappy_amd_driver()) {
    GPUVertBuf *vbo = (GPUVertBuf *)buf;
    /* Some AMD drivers strangely crash with VBOs with a one byte format.
     * To workaround we reinit the vbo with another format and convert
     * all bytes to floats. */
    static GPUVertFormat format = {0};
    if (format.attr_len == 0) {
      GPU_vertformat_attr_add(&format, "wd", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    }
    /* We keep the data reference in data->vbo_data. */
    vbo->data = NULL;
    GPU_vertbuf_clear(vbo);

    int buf_len = mr->loop_len + mr->loop_loose_len;
    GPU_vertbuf_init_with_format(vbo, &format);
    GPU_vertbuf_data_alloc(vbo, buf_len);

    float *fdata = (float *)vbo->data;
    for (int l = 0; l < buf_len; l++, fdata++) {
      *fdata = data->vbo_data[l] / 255.0f;
    }
    /* Free old byte data. */
    MEM_freeN(data->vbo_data);
  }
  MEM_freeN(data);
}

static const MeshExtract extract_edge_fac = {
    extract_edge_fac_init,
    NULL,
    NULL,
    extract_edge_fac_loop_bmesh,
    extract_edge_fac_loop_mesh,
    extract_edge_fac_ledge_bmesh,
    extract_edge_fac_ledge_mesh,
    NULL,
    NULL,
    extract_edge_fac_finish,
    MR_DATA_POLY_NOR,
    false,
};

/** \} */
/* ---------------------------------------------------------------------- */
/** \name Extract Vertex Weight
 * \{ */

typedef struct MeshExtract_Weight_Data {
  float *vbo_data;
  const DRW_MeshWeightState *wstate;
  const MDeformVert *dvert; /* For Mesh. */
  int cd_ofs;               /* For BMesh. */
} MeshExtract_Weight_Data;

static float evaluate_vertex_weight(const MDeformVert *dvert, const DRW_MeshWeightState *wstate)
{
  /* Error state. */
  if ((wstate->defgroup_active < 0) && (wstate->defgroup_len > 0)) {
    return -2.0f;
  }
  else if (dvert == NULL) {
    return (wstate->alert_mode != OB_DRAW_GROUPUSER_NONE) ? -1.0f : 0.0f;
  }

  float input = 0.0f;
  if (wstate->flags & DRW_MESH_WEIGHT_STATE_MULTIPAINT) {
    /* Multi-Paint feature */
    bool is_normalized = (wstate->flags & (DRW_MESH_WEIGHT_STATE_AUTO_NORMALIZE |
                                           DRW_MESH_WEIGHT_STATE_LOCK_RELATIVE));
    input = BKE_defvert_multipaint_collective_weight(dvert,
                                                     wstate->defgroup_len,
                                                     wstate->defgroup_sel,
                                                     wstate->defgroup_sel_count,
                                                     is_normalized);
    /* make it black if the selected groups have no weight on a vertex */
    if (input == 0.0f) {
      return -1.0f;
    }
  }
  else {
    /* default, non tricky behavior */
    input = BKE_defvert_find_weight(dvert, wstate->defgroup_active);

    if (input == 0.0f) {
      switch (wstate->alert_mode) {
        case OB_DRAW_GROUPUSER_ACTIVE:
          return -1.0f;
          break;
        case OB_DRAW_GROUPUSER_ALL:
          if (BKE_defvert_is_weight_zero(dvert, wstate->defgroup_len)) {
            return -1.0f;
          }
          break;
      }
    }
  }

  /* Lock-Relative: display the fraction of current weight vs total unlocked weight. */
  if (wstate->flags & DRW_MESH_WEIGHT_STATE_LOCK_RELATIVE) {
    input = BKE_defvert_lock_relative_weight(
        input, dvert, wstate->defgroup_len, wstate->defgroup_locked, wstate->defgroup_unlocked);
  }

  CLAMP(input, 0.0f, 1.0f);
  return input;
}

static void *extract_weights_init(const MeshRenderData *mr, void *buf)
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "weight", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
  }
  GPUVertBuf *vbo = buf;
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len + mr->loop_loose_len);

  MeshExtract_Weight_Data *data = MEM_callocN(sizeof(*data), __func__);
  data->vbo_data = (float *)vbo->data;
  data->wstate = &mr->cache->weight_state;

  if (data->wstate->defgroup_active == -1) {
    /* Nothing to show. */
    data->dvert = NULL;
    data->cd_ofs = -1;
  }
  else if (mr->extract_type == MR_EXTRACT_BMESH) {
    data->dvert = NULL;
    data->cd_ofs = CustomData_get_offset(&mr->bm->vdata, CD_MDEFORMVERT);
  }
  else {
    data->dvert = CustomData_get_layer(&mr->me->vdata, CD_MDEFORMVERT);
    data->cd_ofs = -1;
  }
  return data;
}

static void extract_weights_loop_bmesh(const MeshRenderData *UNUSED(mr),
                                       int l,
                                       BMLoop *loop,
                                       void *_data)
{
  MeshExtract_Weight_Data *data = (MeshExtract_Weight_Data *)_data;
  const MDeformVert *dvert = (data->cd_ofs != -1) ? BM_ELEM_CD_GET_VOID_P(loop->v, data->cd_ofs) :
                                                    NULL;
  data->vbo_data[l] = evaluate_vertex_weight(dvert, data->wstate);
}

static void extract_weights_loop_mesh(const MeshRenderData *UNUSED(mr),
                                      int l,
                                      const MLoop *mloop,
                                      int UNUSED(p),
                                      const MPoly *UNUSED(mpoly),
                                      void *_data)
{
  MeshExtract_Weight_Data *data = (MeshExtract_Weight_Data *)_data;
  const MDeformVert *dvert = data->dvert ? &data->dvert[mloop->v] : NULL;
  data->vbo_data[l] = evaluate_vertex_weight(dvert, data->wstate);
}

static void extract_weights_finish(const MeshRenderData *UNUSED(mr), void *UNUSED(buf), void *data)
{
  MEM_freeN(data);
}

static const MeshExtract extract_weights = {
    extract_weights_init,
    NULL,
    NULL,
    extract_weights_loop_bmesh,
    extract_weights_loop_mesh,
    NULL,
    NULL,
    NULL,
    NULL,
    extract_weights_finish,
    0,
    true,
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Edit Mode Data / Flags
 * \{ */

typedef struct EditLoopData {
  uchar v_flag;
  uchar e_flag;
  uchar crease;
  uchar bweight;
} EditLoopData;

static void mesh_render_data_face_flag(const MeshRenderData *mr,
                                       BMFace *efa,
                                       const int cd_ofs,
                                       EditLoopData *eattr)
{
  if (efa == mr->efa_act) {
    eattr->v_flag |= VFLAG_FACE_ACTIVE;
  }
  if (BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
    eattr->v_flag |= VFLAG_FACE_SELECTED;
  }

  if (efa == mr->efa_act_uv) {
    eattr->v_flag |= VFLAG_FACE_UV_ACTIVE;
  }
  if ((cd_ofs != -1) && uvedit_face_select_test_ex(mr->toolsettings, (BMFace *)efa, cd_ofs)) {
    eattr->v_flag |= VFLAG_FACE_UV_SELECT;
  }

#ifdef WITH_FREESTYLE
  if (mr->freestyle_face_ofs != -1) {
    const FreestyleFace *ffa = BM_ELEM_CD_GET_VOID_P(efa, mr->freestyle_face_ofs);
    if (ffa->flag & FREESTYLE_FACE_MARK) {
      eattr->v_flag |= VFLAG_FACE_FREESTYLE;
    }
  }
#endif
}

static void mesh_render_data_edge_flag(const MeshRenderData *mr, BMEdge *eed, EditLoopData *eattr)
{
  const ToolSettings *ts = mr->toolsettings;
  const bool is_vertex_select_mode = (ts != NULL) && (ts->selectmode & SCE_SELECT_VERTEX) != 0;
  const bool is_face_only_select_mode = (ts != NULL) && (ts->selectmode == SCE_SELECT_FACE);

  if (eed == mr->eed_act) {
    eattr->e_flag |= VFLAG_EDGE_ACTIVE;
  }
  if (!is_vertex_select_mode && BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
    eattr->e_flag |= VFLAG_EDGE_SELECTED;
  }
  if (is_vertex_select_mode && BM_elem_flag_test(eed->v1, BM_ELEM_SELECT) &&
      BM_elem_flag_test(eed->v2, BM_ELEM_SELECT)) {
    eattr->e_flag |= VFLAG_EDGE_SELECTED;
    eattr->e_flag |= VFLAG_VERT_SELECTED;
  }
  if (BM_elem_flag_test(eed, BM_ELEM_SEAM)) {
    eattr->e_flag |= VFLAG_EDGE_SEAM;
  }
  if (!BM_elem_flag_test(eed, BM_ELEM_SMOOTH)) {
    eattr->e_flag |= VFLAG_EDGE_SHARP;
  }

  /* Use active edge color for active face edges because
   * specular highlights make it hard to see T55456#510873.
   *
   * This isn't ideal since it can't be used when mixing edge/face modes
   * but it's still better then not being able to see the active face. */
  if (is_face_only_select_mode) {
    if (mr->efa_act != NULL) {
      if (BM_edge_in_face(eed, mr->efa_act)) {
        eattr->e_flag |= VFLAG_EDGE_ACTIVE;
      }
    }
  }

  /* Use a byte for value range */
  if (mr->crease_ofs != -1) {
    float crease = BM_ELEM_CD_GET_FLOAT(eed, mr->crease_ofs);
    if (crease > 0) {
      eattr->crease = (uchar)(crease * 255.0f);
    }
  }
  /* Use a byte for value range */
  if (mr->bweight_ofs != -1) {
    float bweight = BM_ELEM_CD_GET_FLOAT(eed, mr->bweight_ofs);
    if (bweight > 0) {
      eattr->bweight = (uchar)(bweight * 255.0f);
    }
  }
#ifdef WITH_FREESTYLE
  if (mr->freestyle_edge_ofs != -1) {
    const FreestyleEdge *fed = BM_ELEM_CD_GET_VOID_P(eed, mr->freestyle_edge_ofs);
    if (fed->flag & FREESTYLE_EDGE_MARK) {
      eattr->e_flag |= VFLAG_EDGE_FREESTYLE;
    }
  }
#endif
}

static void mesh_render_data_loop_flag(const MeshRenderData *mr,
                                       BMLoop *loop,
                                       const int cd_ofs,
                                       EditLoopData *eattr)
{
  if (cd_ofs == -1) {
    return;
  }
  MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(loop, cd_ofs);
  if (luv != NULL && (luv->flag & MLOOPUV_PINNED)) {
    eattr->v_flag |= VFLAG_VERT_UV_PINNED;
  }
  if (uvedit_uv_select_test_ex(mr->toolsettings, loop, cd_ofs)) {
    eattr->v_flag |= VFLAG_VERT_UV_SELECT;
  }
}

static void mesh_render_data_loop_edge_flag(const MeshRenderData *mr,
                                            BMLoop *loop,
                                            const int cd_ofs,
                                            EditLoopData *eattr)
{
  if (cd_ofs == -1) {
    return;
  }
  if (uvedit_edge_select_test_ex(mr->toolsettings, loop, cd_ofs)) {
    eattr->v_flag |= VFLAG_EDGE_UV_SELECT;
    eattr->v_flag |= VFLAG_VERT_UV_SELECT;
  }
}

static void mesh_render_data_vert_flag(const MeshRenderData *mr, BMVert *eve, EditLoopData *eattr)
{
  if (eve == mr->eve_act) {
    eattr->e_flag |= VFLAG_VERT_ACTIVE;
  }
  if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
    eattr->e_flag |= VFLAG_VERT_SELECTED;
  }
}

static void *extract_edit_data_init(const MeshRenderData *mr, void *buf)
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    /* WARNING Adjust EditLoopData struct accordingly. */
    GPU_vertformat_attr_add(&format, "data", GPU_COMP_U8, 4, GPU_FETCH_INT);
    GPU_vertformat_alias_add(&format, "flag");
  }
  GPUVertBuf *vbo = buf;
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len + mr->loop_loose_len);
  return vbo->data;
}

static void extract_edit_data_loop_bmesh(const MeshRenderData *mr,
                                         int l,
                                         BMLoop *loop,
                                         void *_data)
{
  EditLoopData *data = (EditLoopData *)_data + l;
  memset(data, 0x0, sizeof(*data));
  mesh_render_data_face_flag(mr, loop->f, -1, data);
  mesh_render_data_edge_flag(mr, loop->e, data);
  mesh_render_data_vert_flag(mr, loop->v, data);
}

static void extract_edit_data_loop_mesh(const MeshRenderData *mr,
                                        int l,
                                        const MLoop *mloop,
                                        int p,
                                        const MPoly *UNUSED(mpoly),
                                        void *_data)
{
  EditLoopData *data = (EditLoopData *)_data + l;
  memset(data, 0x0, sizeof(*data));
  BMFace *efa = bm_original_face_get(mr, p);
  BMEdge *eed = bm_original_edge_get(mr, mloop->e);
  BMVert *eve = bm_original_vert_get(mr, mloop->v);
  if (efa) {
    mesh_render_data_face_flag(mr, efa, -1, data);
  }
  if (eed) {
    mesh_render_data_edge_flag(mr, eed, data);
  }
  if (eve) {
    mesh_render_data_vert_flag(mr, eve, data);
  }
}

static void extract_edit_data_ledge_bmesh(const MeshRenderData *mr,
                                          int e,
                                          BMEdge *eed,
                                          void *_data)
{
  EditLoopData *data = (EditLoopData *)_data + mr->loop_len + e * 2;
  memset(data, 0x0, sizeof(*data) * 2);
  mesh_render_data_edge_flag(mr, eed, &data[0]);
  data[1] = data[0];
  mesh_render_data_vert_flag(mr, eed->v1, &data[0]);
  mesh_render_data_vert_flag(mr, eed->v2, &data[1]);
}

static void extract_edit_data_ledge_mesh(const MeshRenderData *mr,
                                         int e,
                                         const MEdge *edge,
                                         void *_data)
{
  EditLoopData *data = (EditLoopData *)_data + mr->loop_len + e * 2;
  memset(data, 0x0, sizeof(*data) * 2);
  int e_idx = mr->ledges[e];
  BMEdge *eed = bm_original_edge_get(mr, e_idx);
  BMVert *eve1 = bm_original_vert_get(mr, edge->v1);
  BMVert *eve2 = bm_original_vert_get(mr, edge->v2);
  if (eed) {
    mesh_render_data_edge_flag(mr, eed, &data[0]);
    data[1] = data[0];
  }
  if (eve1) {
    mesh_render_data_vert_flag(mr, eve1, &data[0]);
  }
  if (eve2) {
    mesh_render_data_vert_flag(mr, eve2, &data[1]);
  }
}

static void extract_edit_data_lvert_bmesh(const MeshRenderData *mr,
                                          int v,
                                          BMVert *eve,
                                          void *_data)
{
  EditLoopData *data = (EditLoopData *)_data + mr->loop_len + mr->edge_loose_len * 2 + v;
  memset(data, 0x0, sizeof(*data));
  mesh_render_data_vert_flag(mr, eve, data);
}

static void extract_edit_data_lvert_mesh(const MeshRenderData *mr,
                                         int v,
                                         const MVert *UNUSED(mvert),
                                         void *_data)
{
  EditLoopData *data = (EditLoopData *)_data + mr->loop_len + mr->edge_loose_len * 2 + v;
  memset(data, 0x0, sizeof(*data));
  int v_idx = mr->lverts[v];
  BMVert *eve = bm_original_vert_get(mr, v_idx);
  if (eve) {
    mesh_render_data_vert_flag(mr, eve, data);
  }
}

static const MeshExtract extract_edit_data = {
    extract_edit_data_init,
    NULL,
    NULL,
    extract_edit_data_loop_bmesh,
    extract_edit_data_loop_mesh,
    extract_edit_data_ledge_bmesh,
    extract_edit_data_ledge_mesh,
    extract_edit_data_lvert_bmesh,
    extract_edit_data_lvert_mesh,
    NULL,
    0,
    true,
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Edit UV Data / Flags
 * \{ */

typedef struct MeshExtract_EditUVData_Data {
  EditLoopData *vbo_data;
  int cd_ofs;
} MeshExtract_EditUVData_Data;

static void *extract_edituv_data_init(const MeshRenderData *mr, void *buf)
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    /* WARNING Adjust EditLoopData struct accordingly. */
    GPU_vertformat_attr_add(&format, "data", GPU_COMP_U8, 4, GPU_FETCH_INT);
    GPU_vertformat_alias_add(&format, "flag");
  }

  GPUVertBuf *vbo = buf;
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len);

  CustomData *cd_ldata = (mr->extract_type == MR_EXTRACT_BMESH) ? &mr->bm->ldata : &mr->me->ldata;

  MeshExtract_EditUVData_Data *data = MEM_callocN(sizeof(*data), __func__);
  data->vbo_data = (EditLoopData *)vbo->data;
  data->cd_ofs = CustomData_get_offset(cd_ldata, CD_MLOOPUV);
  return data;
}

static void extract_edituv_data_loop_bmesh(const MeshRenderData *mr,
                                           int l,
                                           BMLoop *loop,
                                           void *_data)
{
  MeshExtract_EditUVData_Data *data = (MeshExtract_EditUVData_Data *)_data;
  EditLoopData *eldata = data->vbo_data + l;
  memset(eldata, 0x0, sizeof(*eldata));
  mesh_render_data_loop_flag(mr, loop, data->cd_ofs, eldata);
  mesh_render_data_face_flag(mr, loop->f, data->cd_ofs, eldata);
  mesh_render_data_loop_edge_flag(mr, loop, data->cd_ofs, eldata);
}

static void extract_edituv_data_loop_mesh(
    const MeshRenderData *mr, int l, const MLoop *mloop, int p, const MPoly *mpoly, void *_data)
{
  MeshExtract_EditUVData_Data *data = (MeshExtract_EditUVData_Data *)_data;
  EditLoopData *eldata = data->vbo_data + l;
  memset(eldata, 0x0, sizeof(*eldata));
  BMFace *efa = bm_original_face_get(mr, p);
  if (efa) {
    BMEdge *eed = bm_original_edge_get(mr, mloop->e);
    BMVert *eve = bm_original_vert_get(mr, mloop->v);
    if (eed && eve) {
      /* Loop on an edge endpoint. */
      BMLoop *loop = BM_face_edge_share_loop(efa, eed);
      mesh_render_data_loop_flag(mr, loop, data->cd_ofs, eldata);
      mesh_render_data_loop_edge_flag(mr, loop, data->cd_ofs, eldata);
    }
    else {
      if (eed == NULL) {
        /* Find if the loop's vert is not part of an edit edge.
         * For this, we check if the previous loop was on an edge. */
        int loopend = mpoly->loopstart + mpoly->totloop - 1;
        int l_prev = (l == mpoly->loopstart) ? loopend : (l - 1);
        const MLoop *mloop_prev = &mr->mloop[l_prev];
        eed = bm_original_edge_get(mr, mloop_prev->e);
      }
      if (eed) {
        /* Mapped points on an edge between two edit verts. */
        BMLoop *loop = BM_face_edge_share_loop(efa, eed);
        mesh_render_data_loop_edge_flag(mr, loop, data->cd_ofs, eldata);
      }
    }
  }
}

static void extract_edituv_data_finish(const MeshRenderData *UNUSED(mr),
                                       void *UNUSED(buf),
                                       void *data)
{
  MEM_freeN(data);
}

static const MeshExtract extract_edituv_data = {
    extract_edituv_data_init,
    NULL,
    NULL,
    extract_edituv_data_loop_bmesh,
    extract_edituv_data_loop_mesh,
    NULL,
    NULL,
    NULL,
    NULL,
    extract_edituv_data_finish,
    0,
    true,
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Edit UV area stretch
 * \{ */

static void *extract_stretch_area_init(const MeshRenderData *mr, void *buf)
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "ratio", GPU_COMP_I16, 1, GPU_FETCH_INT_TO_FLOAT_UNIT);
  }

  GPUVertBuf *vbo = buf;
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len);

  return NULL;
}

BLI_INLINE float area_ratio_get(float area, float uvarea)
{
  if (area >= FLT_EPSILON && uvarea >= FLT_EPSILON) {
    /* Tag inversion by using the sign. */
    return (area > uvarea) ? (uvarea / area) : -(area / uvarea);
  }
  return 0.0f;
}

BLI_INLINE float area_ratio_to_stretch(float ratio, float tot_ratio, float inv_tot_ratio)
{
  ratio *= (ratio > 0.0f) ? tot_ratio : -inv_tot_ratio;
  return (ratio > 1.0f) ? (1.0f / ratio) : ratio;
}

static void mesh_stretch_area_finish(const MeshRenderData *mr, void *buf, void *UNUSED(data))
{
  float tot_area = 0.0f, tot_uv_area = 0.0f;
  float *area_ratio = MEM_mallocN(sizeof(float) * mr->poly_len, __func__);

  if (mr->extract_type == MR_EXTRACT_BMESH) {
    CustomData *cd_ldata = &mr->bm->ldata;
    int uv_ofs = CustomData_get_offset(cd_ldata, CD_MLOOPUV);

    BMFace *efa;
    BMIter f_iter;
    int f;
    BM_ITER_MESH_INDEX (efa, &f_iter, mr->bm, BM_FACES_OF_MESH, f) {
      float area = BM_face_calc_area(efa);
      float uvarea = BM_face_calc_area_uv(efa, uv_ofs);
      tot_area += area;
      tot_uv_area += uvarea;
      area_ratio[f] = area_ratio_get(area, uvarea);
    }
  }
  else if (mr->extract_type == MR_EXTRACT_MAPPED) {
    const MLoopUV *uv_data = CustomData_get_layer(&mr->me->ldata, CD_MLOOPUV);
    const MPoly *mpoly = mr->mpoly;
    for (int p = 0; p < mr->poly_len; p++, mpoly++) {
      float area = BKE_mesh_calc_poly_area(mpoly, &mr->mloop[mpoly->loopstart], mr->mvert);
      float uvarea = BKE_mesh_calc_poly_uv_area(mpoly, uv_data);
      tot_area += area;
      tot_uv_area += uvarea;
      area_ratio[p] = area_ratio_get(area, uvarea);
    }
  }
  else {
    /* Should not happen. */
    BLI_assert(0);
  }

  mr->cache->tot_area = tot_area;
  mr->cache->tot_uv_area = tot_uv_area;

  /* Convert in place to avoid an extra allocation */
  uint16_t *poly_stretch = (uint16_t *)area_ratio;
  for (int p = 0; p < mr->poly_len; p++) {
    poly_stretch[p] = area_ratio[p] * SHRT_MAX;
  }

  /* Copy face data for each loop. */
  GPUVertBuf *vbo = buf;
  uint16_t *loop_stretch = (uint16_t *)vbo->data;

  if (mr->extract_type == MR_EXTRACT_BMESH) {
    BMFace *efa;
    BMIter f_iter;
    int f, l = 0;
    BM_ITER_MESH_INDEX (efa, &f_iter, mr->bm, BM_FACES_OF_MESH, f) {
      for (int i = 0; i < efa->len; i++, l++) {
        loop_stretch[l] = poly_stretch[f];
      }
    }
  }
  else if (mr->extract_type == MR_EXTRACT_MAPPED) {
    const MPoly *mpoly = mr->mpoly;
    for (int p = 0, l = 0; p < mr->poly_len; p++, mpoly++) {
      for (int i = 0; i < mpoly->totloop; i++, l++) {
        loop_stretch[l] = poly_stretch[p];
      }
    }
  }
  else {
    /* Should not happen. */
    BLI_assert(0);
  }

  MEM_freeN(area_ratio);
}

static const MeshExtract extract_stretch_area = {
    extract_stretch_area_init,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    mesh_stretch_area_finish,
    0,
    false,
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Edit UV angle stretch
 * \{ */

typedef struct UVStretchAngle {
  int16_t angle;
  int16_t uv_angles[2];
} UVStretchAngle;

typedef struct MeshExtract_StretchAngle_Data {
  UVStretchAngle *vbo_data;
  MLoopUV *luv;
  float auv[2][2], last_auv[2];
  float av[2][3], last_av[3];
  int cd_ofs;
} MeshExtract_StretchAngle_Data;

static void compute_normalize_edge_vectors(float auv[2][2],
                                           float av[2][3],
                                           const float uv[2],
                                           const float uv_prev[2],
                                           const float co[3],
                                           const float co_prev[3])
{
  /* Move previous edge. */
  copy_v2_v2(auv[0], auv[1]);
  copy_v3_v3(av[0], av[1]);
  /* 2d edge */
  sub_v2_v2v2(auv[1], uv_prev, uv);
  normalize_v2(auv[1]);
  /* 3d edge */
  sub_v3_v3v3(av[1], co_prev, co);
  normalize_v3(av[1]);
}

static short v2_to_short_angle(float v[2])
{
  return atan2f(v[1], v[0]) * (float)M_1_PI * SHRT_MAX;
}

static void edituv_get_stretch_angle(float auv[2][2], float av[2][3], UVStretchAngle *r_stretch)
{
  /* Send uvs to the shader and let it compute the aspect corrected angle. */
  r_stretch->uv_angles[0] = v2_to_short_angle(auv[0]);
  r_stretch->uv_angles[1] = v2_to_short_angle(auv[1]);
  /* Compute 3D angle here. */
  r_stretch->angle = angle_normalized_v3v3(av[0], av[1]) * (float)M_1_PI * SHRT_MAX;

#if 0 /* here for reference, this is done in shader now. */
  float uvang = angle_normalized_v2v2(auv0, auv1);
  float ang = angle_normalized_v3v3(av0, av1);
  float stretch = fabsf(uvang - ang) / (float)M_PI;
  return 1.0f - pow2f(1.0f - stretch);
#endif
}

static void *extract_stretch_angle_init(const MeshRenderData *mr, void *buf)
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    /* WARNING Adjust UVStretchAngle struct accordingly. */
    GPU_vertformat_attr_add(&format, "angle", GPU_COMP_I16, 1, GPU_FETCH_INT_TO_FLOAT_UNIT);
    GPU_vertformat_attr_add(&format, "uv_angles", GPU_COMP_I16, 2, GPU_FETCH_INT_TO_FLOAT_UNIT);
  }

  GPUVertBuf *vbo = buf;
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len);

  MeshExtract_StretchAngle_Data *data = MEM_callocN(sizeof(*data), __func__);
  data->vbo_data = (UVStretchAngle *)vbo->data;

  /* Special iter nneded to save about half of the computing cost. */
  if (mr->extract_type == MR_EXTRACT_BMESH) {
    data->cd_ofs = CustomData_get_offset(&mr->bm->ldata, CD_MLOOPUV);
  }
  else if (mr->extract_type == MR_EXTRACT_MAPPED) {
    data->luv = CustomData_get_layer(&mr->me->ldata, CD_MLOOPUV);
  }
  else {
    BLI_assert(0);
  }
  return data;
}

static void extract_stretch_angle_loop_bmesh(const MeshRenderData *mr,
                                             int l,
                                             BMLoop *loop,
                                             void *_data)
{
  MeshExtract_StretchAngle_Data *data = (MeshExtract_StretchAngle_Data *)_data;
  float(*auv)[2] = data->auv, *last_auv = data->last_auv;
  float(*av)[3] = data->av, *last_av = data->last_av;
  const MLoopUV *luv, *luv_next;
  BMLoop *l_next = loop->next;
  BMFace *efa = loop->f;
  if (loop == efa->l_first) {
    /* First loop in face. */
    BMLoop *l_tmp = loop->prev;
    BMLoop *l_next_tmp = loop;
    luv = BM_ELEM_CD_GET_VOID_P(l_tmp, data->cd_ofs);
    luv_next = BM_ELEM_CD_GET_VOID_P(l_next_tmp, data->cd_ofs);
    compute_normalize_edge_vectors(auv,
                                   av,
                                   luv->uv,
                                   luv_next->uv,
                                   bm_vert_co_get(mr, l_tmp->v),
                                   bm_vert_co_get(mr, l_next_tmp->v));
    /* Save last edge. */
    copy_v2_v2(last_auv, auv[1]);
    copy_v3_v3(last_av, av[1]);
  }
  if (l_next == efa->l_first) {
    /* Move previous edge. */
    copy_v2_v2(auv[0], auv[1]);
    copy_v3_v3(av[0], av[1]);
    /* Copy already calculated last edge. */
    copy_v2_v2(auv[1], last_auv);
    copy_v3_v3(av[1], last_av);
  }
  else {
    luv = BM_ELEM_CD_GET_VOID_P(loop, data->cd_ofs);
    luv_next = BM_ELEM_CD_GET_VOID_P(l_next, data->cd_ofs);
    compute_normalize_edge_vectors(auv,
                                   av,
                                   luv->uv,
                                   luv_next->uv,
                                   bm_vert_co_get(mr, loop->v),
                                   bm_vert_co_get(mr, l_next->v));
  }
  edituv_get_stretch_angle(auv, av, data->vbo_data + l);
}

static void extract_stretch_angle_loop_mesh(const MeshRenderData *mr,
                                            int l,
                                            const MLoop *UNUSED(mloop),
                                            int UNUSED(p),
                                            const MPoly *mpoly,
                                            void *_data)
{
  MeshExtract_StretchAngle_Data *data = (MeshExtract_StretchAngle_Data *)_data;
  float(*auv)[2] = data->auv, *last_auv = data->last_auv;
  float(*av)[3] = data->av, *last_av = data->last_av;
  int l_next = l + 1, loopend = mpoly->loopstart + mpoly->totloop;
  const MVert *v, *v_next;
  if (l == mpoly->loopstart) {
    /* First loop in face. */
    int l_tmp = loopend - 1;
    int l_next_tmp = mpoly->loopstart;
    v = &mr->mvert[mr->mloop[l_tmp].v];
    v_next = &mr->mvert[mr->mloop[l_next_tmp].v];
    compute_normalize_edge_vectors(
        auv, av, data->luv[l_tmp].uv, data->luv[l_next_tmp].uv, v->co, v_next->co);
    /* Save last edge. */
    copy_v2_v2(last_auv, auv[1]);
    copy_v3_v3(last_av, av[1]);
  }
  if (l_next == loopend) {
    l_next = mpoly->loopstart;
    /* Move previous edge. */
    copy_v2_v2(auv[0], auv[1]);
    copy_v3_v3(av[0], av[1]);
    /* Copy already calculated last edge. */
    copy_v2_v2(auv[1], last_auv);
    copy_v3_v3(av[1], last_av);
  }
  else {
    v = &mr->mvert[mr->mloop[l].v];
    v_next = &mr->mvert[mr->mloop[l_next].v];
    compute_normalize_edge_vectors(
        auv, av, data->luv[l].uv, data->luv[l_next].uv, v->co, v_next->co);
  }
  edituv_get_stretch_angle(auv, av, data->vbo_data + l);
}

static void extract_stretch_angle_finish(const MeshRenderData *UNUSED(mr),
                                         void *UNUSED(buf),
                                         void *data)
{
  MEM_freeN(data);
}

static const MeshExtract extract_stretch_angle = {
    extract_stretch_angle_init,
    NULL,
    NULL,
    extract_stretch_angle_loop_bmesh,
    extract_stretch_angle_loop_mesh,
    NULL,
    NULL,
    NULL,
    NULL,
    extract_stretch_angle_finish,
    0,
    false,
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Edit Mesh Analysis Colors
 * \{ */

static void *extract_mesh_analysis_init(const MeshRenderData *mr, void *buf)
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "weight", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
  }

  GPUVertBuf *vbo = buf;
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len);

  return NULL;
}

static void axis_from_enum_v3(float v[3], const char axis)
{
  zero_v3(v);
  if (axis < 3) {
    v[axis] = 1.0f;
  }
  else {
    v[axis - 3] = -1.0f;
  }
}

BLI_INLINE float overhang_remap(float fac, float min, float max, float minmax_irange)
{
  if (fac < min) {
    fac = 1.0f;
  }
  else if (fac > max) {
    fac = -1.0f;
  }
  else {
    fac = (fac - min) * minmax_irange;
    fac = 1.0f - fac;
    CLAMP(fac, 0.0f, 1.0f);
  }
  return fac;
}

static void statvis_calc_overhang(const MeshRenderData *mr, float *r_overhang)
{
  const MeshStatVis *statvis = &mr->toolsettings->statvis;
  const float min = statvis->overhang_min / (float)M_PI;
  const float max = statvis->overhang_max / (float)M_PI;
  const char axis = statvis->overhang_axis;
  BMEditMesh *em = mr->edit_bmesh;
  BMIter iter;
  BMesh *bm = em->bm;
  BMFace *f;
  float dir[3];
  const float minmax_irange = 1.0f / (max - min);

  BLI_assert(min <= max);

  axis_from_enum_v3(dir, axis);

  /* now convert into global space */
  mul_transposed_mat3_m4_v3(mr->obmat, dir);
  normalize_v3(dir);

  if (mr->extract_type == MR_EXTRACT_BMESH) {
    int l = 0;
    BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
      float fac = angle_normalized_v3v3(bm_face_no_get(mr, f), dir) / (float)M_PI;
      fac = overhang_remap(fac, min, max, minmax_irange);
      for (int i = 0; i < f->len; i++, l++) {
        r_overhang[l] = fac;
      }
    }
  }
  else {
    const MPoly *mpoly = mr->mpoly;
    for (int p = 0, l = 0; p < mr->poly_len; p++, mpoly++) {
      float fac = angle_normalized_v3v3(mr->poly_normals[p], dir) / (float)M_PI;
      fac = overhang_remap(fac, min, max, minmax_irange);
      for (int i = 0; i < mpoly->totloop; i++, l++) {
        r_overhang[l] = fac;
      }
    }
  }
}

/* so we can use jitter values for face interpolation */
static void uv_from_jitter_v2(float uv[2])
{
  uv[0] += 0.5f;
  uv[1] += 0.5f;
  if (uv[0] + uv[1] > 1.0f) {
    uv[0] = 1.0f - uv[0];
    uv[1] = 1.0f - uv[1];
  }

  clamp_v2(uv, 0.0f, 1.0f);
}

BLI_INLINE float thickness_remap(float fac, float min, float max, float minmax_irange)
{
  /* important not '<=' */
  if (fac < max) {
    fac = (fac - min) * minmax_irange;
    fac = 1.0f - fac;
    CLAMP(fac, 0.0f, 1.0f);
  }
  else {
    fac = -1.0f;
  }
  return fac;
}

static void statvis_calc_thickness(const MeshRenderData *mr, float *r_thickness)
{
  const float eps_offset = 0.00002f; /* values <= 0.00001 give errors */
  /* cheating to avoid another allocation */
  float *face_dists = r_thickness + (mr->loop_len - mr->poly_len);
  BMEditMesh *em = mr->edit_bmesh;
  const float scale = 1.0f / mat4_to_scale(mr->obmat);
  const MeshStatVis *statvis = &mr->toolsettings->statvis;
  const float min = statvis->thickness_min * scale;
  const float max = statvis->thickness_max * scale;
  const float minmax_irange = 1.0f / (max - min);
  const int samples = statvis->thickness_samples;
  float jit_ofs[32][2];
  BLI_assert(samples <= 32);
  BLI_assert(min <= max);

  copy_vn_fl(face_dists, mr->poly_len, max);

  BLI_jitter_init(jit_ofs, samples);
  for (int j = 0; j < samples; j++) {
    uv_from_jitter_v2(jit_ofs[j]);
  }

  if (mr->extract_type == MR_EXTRACT_BMESH) {
    BMesh *bm = em->bm;
    BM_mesh_elem_index_ensure(bm, BM_FACE);

    struct BMBVHTree *bmtree = BKE_bmbvh_new_from_editmesh(em, 0, NULL, false);
    struct BMLoop *(*looptris)[3] = em->looptris;
    for (int i = 0; i < mr->tri_len; i++) {
      BMLoop **ltri = looptris[i];
      const int index = BM_elem_index_get(ltri[0]->f);
      const float *cos[3] = {
          bm_vert_co_get(mr, ltri[0]->v),
          bm_vert_co_get(mr, ltri[1]->v),
          bm_vert_co_get(mr, ltri[2]->v),
      };
      float ray_co[3];
      float ray_no[3];

      normal_tri_v3(ray_no, cos[2], cos[1], cos[0]);

      for (int j = 0; j < samples; j++) {
        float dist = face_dists[index];
        interp_v3_v3v3v3_uv(ray_co, cos[0], cos[1], cos[2], jit_ofs[j]);
        madd_v3_v3fl(ray_co, ray_no, eps_offset);

        BMFace *f_hit = BKE_bmbvh_ray_cast(bmtree, ray_co, ray_no, 0.0f, &dist, NULL, NULL);
        if (f_hit && dist < face_dists[index]) {
          float angle_fac = fabsf(
              dot_v3v3(bm_face_no_get(mr, ltri[0]->f), bm_face_no_get(mr, f_hit)));
          angle_fac = 1.0f - angle_fac;
          angle_fac = angle_fac * angle_fac * angle_fac;
          angle_fac = 1.0f - angle_fac;
          dist /= angle_fac;
          if (dist < face_dists[index]) {
            face_dists[index] = dist;
          }
        }
      }
    }
    BKE_bmbvh_free(bmtree);

    BMIter iter;
    BMFace *f;
    int l = 0;
    BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
      float fac = face_dists[BM_elem_index_get(f)];
      fac = thickness_remap(fac, min, max, minmax_irange);
      for (int i = 0; i < f->len; i++, l++) {
        r_thickness[l] = fac;
      }
    }
  }
  else {
    BVHTreeFromMesh treeData = {NULL};

    BVHTree *tree = BKE_bvhtree_from_mesh_get(&treeData, mr->me, BVHTREE_FROM_LOOPTRI, 4);
    const MLoopTri *mlooptri = mr->mlooptri;
    for (int i = 0; i < mr->tri_len; i++, mlooptri++) {
      const int index = mlooptri->poly;
      const float *cos[3] = {mr->mvert[mr->mloop[mlooptri->tri[0]].v].co,
                             mr->mvert[mr->mloop[mlooptri->tri[1]].v].co,
                             mr->mvert[mr->mloop[mlooptri->tri[2]].v].co};
      float ray_co[3];
      float ray_no[3];

      normal_tri_v3(ray_no, cos[2], cos[1], cos[0]);

      for (int j = 0; j < samples; j++) {
        interp_v3_v3v3v3_uv(ray_co, cos[0], cos[1], cos[2], jit_ofs[j]);
        madd_v3_v3fl(ray_co, ray_no, eps_offset);

        BVHTreeRayHit hit;
        hit.index = -1;
        hit.dist = face_dists[index];
        if ((BLI_bvhtree_ray_cast(
                 tree, ray_co, ray_no, 0.0f, &hit, treeData.raycast_callback, &treeData) != -1) &&
            hit.dist < face_dists[index]) {
          float angle_fac = fabsf(dot_v3v3(mr->poly_normals[index], hit.no));
          angle_fac = 1.0f - angle_fac;
          angle_fac = angle_fac * angle_fac * angle_fac;
          angle_fac = 1.0f - angle_fac;
          hit.dist /= angle_fac;
          if (hit.dist < face_dists[index]) {
            face_dists[index] = hit.dist;
          }
        }
      }
    }

    const MPoly *mpoly = mr->mpoly;
    for (int p = 0, l = 0; p < mr->poly_len; p++, mpoly++) {
      float fac = face_dists[p];
      fac = thickness_remap(fac, min, max, minmax_irange);
      for (int i = 0; i < mpoly->totloop; i++, l++) {
        r_thickness[l] = fac;
      }
    }
  }
}

struct BVHTree_OverlapData {
  const Mesh *me;
  const MLoopTri *mlooptri;
  float epsilon;
};

static bool bvh_overlap_cb(void *userdata, int index_a, int index_b, int UNUSED(thread))
{
  struct BVHTree_OverlapData *data = userdata;
  const Mesh *me = data->me;

  const MLoopTri *tri_a = &data->mlooptri[index_a];
  const MLoopTri *tri_b = &data->mlooptri[index_b];

  if (UNLIKELY(tri_a->poly == tri_b->poly)) {
    return false;
  }

  const float *tri_a_co[3] = {me->mvert[me->mloop[tri_a->tri[0]].v].co,
                              me->mvert[me->mloop[tri_a->tri[1]].v].co,
                              me->mvert[me->mloop[tri_a->tri[2]].v].co};
  const float *tri_b_co[3] = {me->mvert[me->mloop[tri_b->tri[0]].v].co,
                              me->mvert[me->mloop[tri_b->tri[1]].v].co,
                              me->mvert[me->mloop[tri_b->tri[2]].v].co};
  float ix_pair[2][3];
  int verts_shared = 0;

  verts_shared = (ELEM(tri_a_co[0], UNPACK3(tri_b_co)) + ELEM(tri_a_co[1], UNPACK3(tri_b_co)) +
                  ELEM(tri_a_co[2], UNPACK3(tri_b_co)));

  /* if 2 points are shared, bail out */
  if (verts_shared >= 2) {
    return false;
  }

  return (isect_tri_tri_epsilon_v3(
              UNPACK3(tri_a_co), UNPACK3(tri_b_co), ix_pair[0], ix_pair[1], data->epsilon) &&
          /* if we share a vertex, check the intersection isn't a 'point' */
          ((verts_shared == 0) || (len_squared_v3v3(ix_pair[0], ix_pair[1]) > data->epsilon)));
}

static void statvis_calc_intersect(const MeshRenderData *mr, float *r_intersect)
{
  BMEditMesh *em = mr->edit_bmesh;

  for (int l = 0; l < mr->loop_len; l++) {
    r_intersect[l] = -1.0f;
  }

  if (mr->extract_type == MR_EXTRACT_BMESH) {
    uint overlap_len;
    BMesh *bm = em->bm;

    BM_mesh_elem_index_ensure(bm, BM_FACE);

    struct BMBVHTree *bmtree = BKE_bmbvh_new_from_editmesh(em, 0, NULL, false);
    BVHTreeOverlap *overlap = BKE_bmbvh_overlap(bmtree, bmtree, &overlap_len);

    if (overlap) {
      for (int i = 0; i < overlap_len; i++) {
        BMFace *f_hit_pair[2] = {
            em->looptris[overlap[i].indexA][0]->f,
            em->looptris[overlap[i].indexB][0]->f,
        };
        for (int j = 0; j < 2; j++) {
          BMFace *f_hit = f_hit_pair[j];
          BMLoop *l_first = BM_FACE_FIRST_LOOP(f_hit);
          int l = BM_elem_index_get(l_first);
          for (int k = 0; k < f_hit->len; k++, l++) {
            r_intersect[l] = 1.0f;
          }
        }
      }
      MEM_freeN(overlap);
    }

    BKE_bmbvh_free(bmtree);
  }
  else {
    uint overlap_len;
    BVHTreeFromMesh treeData = {NULL};

    BVHTree *tree = BKE_bvhtree_from_mesh_get(&treeData, mr->me, BVHTREE_FROM_LOOPTRI, 4);

    struct BVHTree_OverlapData data = {
        .me = mr->me, .mlooptri = mr->mlooptri, .epsilon = BLI_bvhtree_get_epsilon(tree)};

    BVHTreeOverlap *overlap = BLI_bvhtree_overlap(tree, tree, &overlap_len, bvh_overlap_cb, &data);
    if (overlap) {
      for (int i = 0; i < overlap_len; i++) {
        const MPoly *f_hit_pair[2] = {
            &mr->mpoly[mr->mlooptri[overlap[i].indexA].poly],
            &mr->mpoly[mr->mlooptri[overlap[i].indexB].poly],
        };
        for (int j = 0; j < 2; j++) {
          const MPoly *f_hit = f_hit_pair[j];
          int l = f_hit->loopstart;
          for (int k = 0; k < f_hit->totloop; k++, l++) {
            r_intersect[l] = 1.0f;
          }
        }
      }
      MEM_freeN(overlap);
    }
  }
}

BLI_INLINE float distort_remap(float fac, float min, float UNUSED(max), float minmax_irange)
{
  if (fac >= min) {
    fac = (fac - min) * minmax_irange;
    CLAMP(fac, 0.0f, 1.0f);
  }
  else {
    /* fallback */
    fac = -1.0f;
  }
  return fac;
}

static void statvis_calc_distort(const MeshRenderData *mr, float *r_distort)
{
  BMEditMesh *em = mr->edit_bmesh;
  const MeshStatVis *statvis = &mr->toolsettings->statvis;
  const float min = statvis->distort_min;
  const float max = statvis->distort_max;
  const float minmax_irange = 1.0f / (max - min);

  if (mr->extract_type == MR_EXTRACT_BMESH) {
    BMIter iter;
    BMesh *bm = em->bm;
    BMFace *f;

    if (mr->bm_vert_coords != NULL) {
      BKE_editmesh_cache_ensure_poly_normals(em, mr->edit_data);

      /* Most likely this is already valid, ensure just in case.
       * Needed for #BM_loop_calc_face_normal_safe_vcos. */
      BM_mesh_elem_index_ensure(em->bm, BM_VERT);
    }

    int l = 0;
    int p = 0;
    BM_ITER_MESH_INDEX (f, &iter, bm, BM_FACES_OF_MESH, p) {
      float fac = -1.0f;

      if (f->len > 3) {
        BMLoop *l_iter, *l_first;

        fac = 0.0f;
        l_iter = l_first = BM_FACE_FIRST_LOOP(f);
        do {
          const float *no_face;
          float no_corner[3];
          if (mr->bm_vert_coords != NULL) {
            no_face = mr->bm_poly_normals[p];
            BM_loop_calc_face_normal_safe_vcos(l_iter, no_face, mr->bm_vert_coords, no_corner);
          }
          else {
            no_face = f->no;
            BM_loop_calc_face_normal_safe(l_iter, no_corner);
          }

          /* simple way to detect (what is most likely) concave */
          if (dot_v3v3(no_face, no_corner) < 0.0f) {
            negate_v3(no_corner);
          }
          fac = max_ff(fac, angle_normalized_v3v3(no_face, no_corner));

        } while ((l_iter = l_iter->next) != l_first);
        fac *= 2.0f;
      }

      fac = distort_remap(fac, min, max, minmax_irange);
      for (int i = 0; i < f->len; i++, l++) {
        r_distort[l] = fac;
      }
    }
  }
  else {
    const MPoly *mpoly = mr->mpoly;
    for (int p = 0, l = 0; p < mr->poly_len; p++, mpoly++) {
      float fac = -1.0f;

      if (mpoly->totloop > 3) {
        float *f_no = mr->poly_normals[p];
        fac = 0.0f;

        for (int i = 1; i <= mpoly->totloop; i++) {
          const MLoop *l_prev = &mr->mloop[mpoly->loopstart + (i - 1) % mpoly->totloop];
          const MLoop *l_curr = &mr->mloop[mpoly->loopstart + (i + 0) % mpoly->totloop];
          const MLoop *l_next = &mr->mloop[mpoly->loopstart + (i + 1) % mpoly->totloop];
          float no_corner[3];
          normal_tri_v3(no_corner,
                        mr->mvert[l_prev->v].co,
                        mr->mvert[l_curr->v].co,
                        mr->mvert[l_next->v].co);
          /* simple way to detect (what is most likely) concave */
          if (dot_v3v3(f_no, no_corner) < 0.0f) {
            negate_v3(no_corner);
          }
          fac = max_ff(fac, angle_normalized_v3v3(f_no, no_corner));
        }
        fac *= 2.0f;
      }

      fac = distort_remap(fac, min, max, minmax_irange);
      for (int i = 0; i < mpoly->totloop; i++, l++) {
        r_distort[l] = fac;
      }
    }
  }
}

BLI_INLINE float sharp_remap(float fac, float min, float UNUSED(max), float minmax_irange)
{
  /* important not '>=' */
  if (fac > min) {
    fac = (fac - min) * minmax_irange;
    CLAMP(fac, 0.0f, 1.0f);
  }
  else {
    /* fallback */
    fac = -1.0f;
  }
  return fac;
}

static void statvis_calc_sharp(const MeshRenderData *mr, float *r_sharp)
{
  BMEditMesh *em = mr->edit_bmesh;
  const MeshStatVis *statvis = &mr->toolsettings->statvis;
  const float min = statvis->sharp_min;
  const float max = statvis->sharp_max;
  const float minmax_irange = 1.0f / (max - min);

  /* Can we avoid this extra allocation? */
  float *vert_angles = MEM_mallocN(sizeof(float) * mr->vert_len, __func__);
  copy_vn_fl(vert_angles, mr->vert_len, -M_PI);

  if (mr->extract_type == MR_EXTRACT_BMESH) {
    BMIter iter, l_iter;
    BMesh *bm = em->bm;
    BMFace *efa;
    BMEdge *e;
    BMLoop *loop;
    /* first assign float values to verts */
    BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
      float angle = BM_edge_calc_face_angle_signed(e);
      float *col1 = &vert_angles[BM_elem_index_get(e->v1)];
      float *col2 = &vert_angles[BM_elem_index_get(e->v2)];
      *col1 = max_ff(*col1, angle);
      *col2 = max_ff(*col2, angle);
    }
    /* Copy vert value to loops. */
    BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
      BM_ITER_ELEM (loop, &l_iter, efa, BM_LOOPS_OF_FACE) {
        int l = BM_elem_index_get(loop);
        int v = BM_elem_index_get(loop->v);
        r_sharp[l] = sharp_remap(vert_angles[v], min, max, minmax_irange);
      }
    }
  }
  else {
    /* first assign float values to verts */
    const MPoly *mpoly = mr->mpoly;

    EdgeHash *eh = BLI_edgehash_new_ex(__func__, mr->edge_len);

    for (int p = 0; p < mr->poly_len; p++, mpoly++) {
      for (int i = 0; i < mpoly->totloop; i++) {
        const MLoop *l_curr = &mr->mloop[mpoly->loopstart + (i + 0) % mpoly->totloop];
        const MLoop *l_next = &mr->mloop[mpoly->loopstart + (i + 1) % mpoly->totloop];
        const MVert *v_curr = &mr->mvert[l_curr->v];
        const MVert *v_next = &mr->mvert[l_next->v];
        float angle;
        void **pval;
        bool value_is_init = BLI_edgehash_ensure_p(eh, l_curr->v, l_next->v, &pval);
        if (!value_is_init) {
          *pval = mr->poly_normals[p];
          /* non-manifold edge, yet... */
          continue;
        }
        else if (*pval != NULL) {
          const float *f1_no = mr->poly_normals[p];
          const float *f2_no = *pval;
          angle = angle_normalized_v3v3(f1_no, f2_no);
          angle = is_edge_convex_v3(v_curr->co, v_next->co, f1_no, f2_no) ? angle : -angle;
          /* Tag as manifold. */
          *pval = NULL;
        }
        else {
          /* non-manifold edge */
          angle = DEG2RADF(90.0f);
        }
        float *col1 = &vert_angles[l_curr->v];
        float *col2 = &vert_angles[l_next->v];
        *col1 = max_ff(*col1, angle);
        *col2 = max_ff(*col2, angle);
      }
    }
    /* Remaining non manifold edges. */
    EdgeHashIterator *ehi = BLI_edgehashIterator_new(eh);
    for (; !BLI_edgehashIterator_isDone(ehi); BLI_edgehashIterator_step(ehi)) {
      if (BLI_edgehashIterator_getValue(ehi) != NULL) {
        uint v1, v2;
        const float angle = DEG2RADF(90.0f);
        BLI_edgehashIterator_getKey(ehi, &v1, &v2);
        float *col1 = &vert_angles[v1];
        float *col2 = &vert_angles[v2];
        *col1 = max_ff(*col1, angle);
        *col2 = max_ff(*col2, angle);
      }
    }
    BLI_edgehashIterator_free(ehi);
    BLI_edgehash_free(eh, NULL);

    const MLoop *mloop = mr->mloop;
    for (int l = 0; l < mr->loop_len; l++, mloop++) {
      r_sharp[l] = sharp_remap(vert_angles[mloop->v], min, max, minmax_irange);
    }
  }

  MEM_freeN(vert_angles);
}

static void extract_mesh_analysis_finish(const MeshRenderData *mr, void *buf, void *UNUSED(data))
{
  BLI_assert(mr->edit_bmesh);

  GPUVertBuf *vbo = buf;
  float *l_weight = (float *)vbo->data;

  switch (mr->toolsettings->statvis.type) {
    case SCE_STATVIS_OVERHANG:
      statvis_calc_overhang(mr, l_weight);
      break;
    case SCE_STATVIS_THICKNESS:
      statvis_calc_thickness(mr, l_weight);
      break;
    case SCE_STATVIS_INTERSECT:
      statvis_calc_intersect(mr, l_weight);
      break;
    case SCE_STATVIS_DISTORT:
      statvis_calc_distort(mr, l_weight);
      break;
    case SCE_STATVIS_SHARP:
      statvis_calc_sharp(mr, l_weight);
      break;
  }
}

static const MeshExtract extract_mesh_analysis = {
    extract_mesh_analysis_init,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    extract_mesh_analysis_finish,
    /* This is not needed for all vis type.
     * Maybe split into different extract. */
    MR_DATA_POLY_NOR | MR_DATA_LOOPTRI,
    false,
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Facedots positions
 * \{ */

static void *extract_fdots_pos_init(const MeshRenderData *mr, void *buf)
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  }
  GPUVertBuf *vbo = buf;
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->poly_len);
  if (!mr->use_subsurf_fdots) {
    /* Clear so we can accumulate on it. */
    memset(vbo->data, 0x0, mr->poly_len * vbo->format.stride);
  }
  return vbo->data;
}

static void extract_fdots_pos_loop_bmesh(const MeshRenderData *mr,
                                         int UNUSED(l),
                                         BMLoop *loop,
                                         void *data)
{
  float(*center)[3] = (float(*)[3])data;
  float w = 1.0f / (float)loop->f->len;
  madd_v3_v3fl(center[BM_elem_index_get(loop->f)], bm_vert_co_get(mr, loop->v), w);
}

static void extract_fdots_pos_loop_mesh(const MeshRenderData *mr,
                                        int UNUSED(l),
                                        const MLoop *mloop,
                                        int p,
                                        const MPoly *mpoly,
                                        void *data)
{
  float(*center)[3] = (float(*)[3])data;
  const MVert *mvert = &mr->mvert[mloop->v];
  if (mr->use_subsurf_fdots) {
    if (mvert->flag & ME_VERT_FACEDOT) {
      copy_v3_v3(center[p], mvert->co);
    }
  }
  else {
    float w = 1.0f / (float)mpoly->totloop;
    madd_v3_v3fl(center[p], mvert->co, w);
  }
}

static const MeshExtract extract_fdots_pos = {
    extract_fdots_pos_init,
    NULL,
    NULL,
    extract_fdots_pos_loop_bmesh,
    extract_fdots_pos_loop_mesh,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    0,
    true,
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Facedots Normal and edit flag
 * \{ */
#define NOR_AND_FLAG_DEFAULT 0
#define NOR_AND_FLAG_SELECT 1
#define NOR_AND_FLAG_ACTIVE -1
#define NOR_AND_FLAG_HIDDEN -2

static void *extract_fdots_nor_init(const MeshRenderData *mr, void *buf)
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "norAndFlag", GPU_COMP_I10, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);
  }
  GPUVertBuf *vbo = buf;
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->poly_len);

  return NULL;
}

static void extract_fdots_nor_finish(const MeshRenderData *mr, void *buf, void *UNUSED(data))
{
  static float invalid_normal[3] = {0.0f, 0.0f, 0.0f};
  GPUVertBuf *vbo = buf;
  GPUPackedNormal *nor = (GPUPackedNormal *)vbo->data;
  BMFace *efa;

  /* Quicker than doing it for each loop. */
  if (mr->extract_type == MR_EXTRACT_BMESH) {
    for (int f = 0; f < mr->poly_len; f++) {
      efa = BM_face_at_index(mr->bm, f);
      const bool is_face_hidden = BM_elem_flag_test(efa, BM_ELEM_HIDDEN);
      if (is_face_hidden || (mr->extract_type == MR_EXTRACT_MAPPED && mr->p_origindex &&
                             mr->p_origindex[f] == ORIGINDEX_NONE)) {
        nor[f] = GPU_normal_convert_i10_v3(invalid_normal);
        nor[f].w = NOR_AND_FLAG_HIDDEN;
      }
      else {
        nor[f] = GPU_normal_convert_i10_v3(bm_face_no_get(mr, efa));
        /* Select / Active Flag. */
        nor[f].w = (BM_elem_flag_test(efa, BM_ELEM_SELECT) ?
                        ((efa == mr->efa_act) ? NOR_AND_FLAG_ACTIVE : NOR_AND_FLAG_SELECT) :
                        NOR_AND_FLAG_DEFAULT);
      }
    }
  }
  else {
    for (int f = 0; f < mr->poly_len; f++) {
      efa = bm_original_face_get(mr, f);
      const bool is_face_hidden = efa && BM_elem_flag_test(efa, BM_ELEM_HIDDEN);
      if (is_face_hidden || (mr->extract_type == MR_EXTRACT_MAPPED && mr->p_origindex &&
                             mr->p_origindex[f] == ORIGINDEX_NONE)) {
        nor[f] = GPU_normal_convert_i10_v3(invalid_normal);
        nor[f].w = NOR_AND_FLAG_HIDDEN;
      }
      else {
        nor[f] = GPU_normal_convert_i10_v3(bm_face_no_get(mr, efa));
        /* Select / Active Flag. */
        nor[f].w = (BM_elem_flag_test(efa, BM_ELEM_SELECT) ?
                        ((efa == mr->efa_act) ? NOR_AND_FLAG_ACTIVE : NOR_AND_FLAG_SELECT) :
                        NOR_AND_FLAG_DEFAULT);
      }
    }
  }
}

static const MeshExtract extract_fdots_nor = {
    extract_fdots_nor_init,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    extract_fdots_nor_finish,
    MR_DATA_POLY_NOR,
    false,
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Facedots Normal and edit flag
 * \{ */

typedef struct MeshExtract_FdotUV_Data {
  float (*vbo_data)[2];
  MLoopUV *uv_data;
  int cd_ofs;
} MeshExtract_FdotUV_Data;

static void *extract_fdots_uv_init(const MeshRenderData *mr, void *buf)
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "u", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    GPU_vertformat_alias_add(&format, "au");
    GPU_vertformat_alias_add(&format, "pos");
  }
  GPUVertBuf *vbo = buf;
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->poly_len);

  if (!mr->use_subsurf_fdots) {
    /* Clear so we can accumulate on it. */
    memset(vbo->data, 0x0, mr->poly_len * vbo->format.stride);
  }

  MeshExtract_FdotUV_Data *data = MEM_callocN(sizeof(*data), __func__);
  data->vbo_data = (float(*)[2])vbo->data;

  if (mr->extract_type == MR_EXTRACT_BMESH) {
    data->cd_ofs = CustomData_get_offset(&mr->bm->ldata, CD_MLOOPUV);
  }
  else {
    data->uv_data = CustomData_get_layer(&mr->me->ldata, CD_MLOOPUV);
  }
  return data;
}

static void extract_fdots_uv_loop_bmesh(const MeshRenderData *UNUSED(mr),
                                        int UNUSED(l),
                                        BMLoop *loop,
                                        void *_data)
{
  MeshExtract_FdotUV_Data *data = (MeshExtract_FdotUV_Data *)_data;
  float w = 1.0f / (float)loop->f->len;
  const MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(loop, data->cd_ofs);
  madd_v2_v2fl(data->vbo_data[BM_elem_index_get(loop->f)], luv->uv, w);
}

static void extract_fdots_uv_loop_mesh(
    const MeshRenderData *mr, int l, const MLoop *mloop, int p, const MPoly *mpoly, void *_data)
{
  MeshExtract_FdotUV_Data *data = (MeshExtract_FdotUV_Data *)_data;
  if (mr->use_subsurf_fdots) {
    const MVert *mvert = &mr->mvert[mloop->v];
    if (mvert->flag & ME_VERT_FACEDOT) {
      copy_v2_v2(data->vbo_data[p], data->uv_data[l].uv);
    }
  }
  else {
    float w = 1.0f / (float)mpoly->totloop;
    madd_v2_v2fl(data->vbo_data[p], data->uv_data[l].uv, w);
  }
}

static void extract_fdots_uv_finish(const MeshRenderData *UNUSED(mr),
                                    void *UNUSED(buf),
                                    void *data)
{
  MEM_freeN(data);
}

static const MeshExtract extract_fdots_uv = {
    extract_fdots_uv_init,
    NULL,
    NULL,
    extract_fdots_uv_loop_bmesh,
    extract_fdots_uv_loop_mesh,
    NULL,
    NULL,
    NULL,
    NULL,
    extract_fdots_uv_finish,
    0,
    true,
};
/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Facedots  Edit UV flag
 * \{ */

typedef struct MeshExtract_EditUVFdotData_Data {
  EditLoopData *vbo_data;
  int cd_ofs;
} MeshExtract_EditUVFdotData_Data;

static void *extract_fdots_edituv_data_init(const MeshRenderData *mr, void *buf)
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "flag", GPU_COMP_U8, 4, GPU_FETCH_INT);
  }
  GPUVertBuf *vbo = buf;
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->poly_len);

  MeshExtract_EditUVFdotData_Data *data = MEM_callocN(sizeof(*data), __func__);
  data->vbo_data = (EditLoopData *)vbo->data;
  data->cd_ofs = CustomData_get_offset(&mr->bm->ldata, CD_MLOOPUV);
  return data;
}

static void extract_fdots_edituv_data_loop_bmesh(const MeshRenderData *mr,
                                                 int UNUSED(l),
                                                 BMLoop *loop,
                                                 void *_data)
{
  MeshExtract_EditUVFdotData_Data *data = (MeshExtract_EditUVFdotData_Data *)_data;
  EditLoopData *eldata = data->vbo_data + BM_elem_index_get(loop->f);
  memset(eldata, 0x0, sizeof(*eldata));
  mesh_render_data_face_flag(mr, loop->f, data->cd_ofs, eldata);
}

static void extract_fdots_edituv_data_loop_mesh(const MeshRenderData *mr,
                                                int UNUSED(l),
                                                const MLoop *UNUSED(mloop),
                                                int p,
                                                const MPoly *UNUSED(mpoly),
                                                void *_data)
{
  MeshExtract_EditUVFdotData_Data *data = (MeshExtract_EditUVFdotData_Data *)_data;
  EditLoopData *eldata = data->vbo_data + p;
  memset(eldata, 0x0, sizeof(*eldata));
  BMFace *efa = bm_original_face_get(mr, p);
  if (efa) {
    mesh_render_data_face_flag(mr, efa, data->cd_ofs, eldata);
  }
}

static void extract_fdots_edituv_data_finish(const MeshRenderData *UNUSED(mr),
                                             void *UNUSED(buf),
                                             void *data)
{
  MEM_freeN(data);
}

static const MeshExtract extract_fdots_edituv_data = {
    extract_fdots_edituv_data_init,
    NULL,
    NULL,
    extract_fdots_edituv_data_loop_bmesh,
    extract_fdots_edituv_data_loop_mesh,
    NULL,
    NULL,
    NULL,
    NULL,
    extract_fdots_edituv_data_finish,
    0,
    true,
};
/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Skin Modifier Roots
 * \{ */

typedef struct SkinRootData {
  float size;
  float local_pos[3];
} SkinRootData;

static void *extract_skin_roots_init(const MeshRenderData *mr, void *buf)
{
  /* Exclusively for edit mode. */
  BLI_assert(mr->bm);

  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    GPU_vertformat_attr_add(&format, "size", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
    GPU_vertformat_attr_add(&format, "local_pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  }
  GPUVertBuf *vbo = buf;
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->bm->totvert);

  SkinRootData *vbo_data = (SkinRootData *)vbo->data;

  int root_len = 0;
  int cd_ofs = CustomData_get_offset(&mr->bm->vdata, CD_MVERT_SKIN);

  BMIter iter;
  BMVert *eve;
  BM_ITER_MESH (eve, &iter, mr->bm, BM_VERTS_OF_MESH) {
    const MVertSkin *vs = BM_ELEM_CD_GET_VOID_P(eve, cd_ofs);
    if (vs->flag & MVERT_SKIN_ROOT) {
      vbo_data->size = (vs->radius[0] + vs->radius[1]) * 0.5f;
      copy_v3_v3(vbo_data->local_pos, bm_vert_co_get(mr, eve));
      vbo_data++;
      root_len++;
    }
  }

  /* It's really unlikely that all verts will be roots. Resize to avoid loosing VRAM. */
  GPU_vertbuf_data_len_set(vbo, root_len);

  return NULL;
}

static const MeshExtract extract_skin_roots = {
    extract_skin_roots_init,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    0,
    false,
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Extract Selection Index
 * \{ */

static void *extract_select_idx_init(const MeshRenderData *mr, void *buf)
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    /* TODO rename "color" to something more descriptive. */
    GPU_vertformat_attr_add(&format, "color", GPU_COMP_U32, 1, GPU_FETCH_INT);
  }
  GPUVertBuf *vbo = buf;
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->loop_len + mr->loop_loose_len);
  return vbo->data;
}

/* TODO Use glVertexID to get loop index and use the data structure on the CPU to retrieve the
 * select element associated with this loop ID. This would remove the need for this separate
 * index VBOs. We could upload the p/e/v_origindex as a buffer texture and sample it inside the
 * shader to output original index. */

static void extract_poly_idx_loop_bmesh(const MeshRenderData *UNUSED(mr),
                                        int l,
                                        BMLoop *loop,
                                        void *data)
{
  ((uint32_t *)data)[l] = BM_elem_index_get(loop->f);
}

static void extract_edge_idx_loop_bmesh(const MeshRenderData *UNUSED(mr),
                                        int l,
                                        BMLoop *loop,
                                        void *data)
{
  ((uint32_t *)data)[l] = BM_elem_index_get(loop->e);
}

static void extract_vert_idx_loop_bmesh(const MeshRenderData *UNUSED(mr),
                                        int l,
                                        BMLoop *loop,
                                        void *data)
{
  ((uint32_t *)data)[l] = BM_elem_index_get(loop->v);
}

static void extract_edge_idx_ledge_bmesh(const MeshRenderData *mr, int e, BMEdge *eed, void *data)
{
  ((uint32_t *)data)[mr->loop_len + e * 2 + 0] = BM_elem_index_get(eed);
  ((uint32_t *)data)[mr->loop_len + e * 2 + 1] = BM_elem_index_get(eed);
}

static void extract_vert_idx_ledge_bmesh(const MeshRenderData *mr, int e, BMEdge *eed, void *data)
{
  ((uint32_t *)data)[mr->loop_len + e * 2 + 0] = BM_elem_index_get(eed->v1);
  ((uint32_t *)data)[mr->loop_len + e * 2 + 1] = BM_elem_index_get(eed->v2);
}

static void extract_vert_idx_lvert_bmesh(const MeshRenderData *mr, int v, BMVert *eve, void *data)
{
  ((uint32_t *)data)[mr->loop_len + mr->edge_loose_len * 2 + v] = BM_elem_index_get(eve);
}

static void extract_poly_idx_loop_mesh(const MeshRenderData *mr,
                                       int l,
                                       const MLoop *UNUSED(mloop),
                                       int p,
                                       const MPoly *UNUSED(mpoly),
                                       void *data)
{
  ((uint32_t *)data)[l] = (mr->p_origindex) ? mr->p_origindex[p] : p;
}

static void extract_edge_idx_loop_mesh(const MeshRenderData *mr,
                                       int l,
                                       const MLoop *mloop,
                                       int UNUSED(p),
                                       const MPoly *UNUSED(mpoly),
                                       void *data)
{
  ((uint32_t *)data)[l] = (mr->e_origindex) ? mr->e_origindex[mloop->e] : mloop->e;
}

static void extract_vert_idx_loop_mesh(const MeshRenderData *mr,
                                       int l,
                                       const MLoop *mloop,
                                       int UNUSED(p),
                                       const MPoly *UNUSED(mpoly),
                                       void *data)
{
  ((uint32_t *)data)[l] = (mr->v_origindex) ? mr->v_origindex[mloop->v] : mloop->v;
}

static void extract_edge_idx_ledge_mesh(const MeshRenderData *mr,
                                        int e,
                                        const MEdge *UNUSED(medge),
                                        void *data)
{
  int e_idx = mr->ledges[e];
  int e_orig = (mr->e_origindex) ? mr->e_origindex[e_idx] : e_idx;
  ((uint32_t *)data)[mr->loop_len + e * 2 + 0] = e_orig;
  ((uint32_t *)data)[mr->loop_len + e * 2 + 1] = e_orig;
}

static void extract_vert_idx_ledge_mesh(const MeshRenderData *mr,
                                        int e,
                                        const MEdge *medge,
                                        void *data)
{
  int v1_orig = (mr->v_origindex) ? mr->v_origindex[medge->v1] : medge->v1;
  int v2_orig = (mr->v_origindex) ? mr->v_origindex[medge->v2] : medge->v2;
  ((uint32_t *)data)[mr->loop_len + e * 2 + 0] = v1_orig;
  ((uint32_t *)data)[mr->loop_len + e * 2 + 1] = v2_orig;
}

static void extract_vert_idx_lvert_mesh(const MeshRenderData *mr,
                                        int v,
                                        const MVert *UNUSED(mvert),
                                        void *data)
{
  int v_idx = mr->lverts[v];
  int v_orig = (mr->v_origindex) ? mr->v_origindex[v_idx] : v_idx;
  ((uint32_t *)data)[mr->loop_len + mr->edge_loose_len * 2 + v] = v_orig;
}

static const MeshExtract extract_poly_idx = {
    extract_select_idx_init,
    NULL,
    NULL,
    extract_poly_idx_loop_bmesh,
    extract_poly_idx_loop_mesh,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    0,
    true,
};

static const MeshExtract extract_edge_idx = {
    extract_select_idx_init,
    NULL,
    NULL,
    extract_edge_idx_loop_bmesh,
    extract_edge_idx_loop_mesh,
    extract_edge_idx_ledge_bmesh,
    extract_edge_idx_ledge_mesh,
    NULL,
    NULL,
    NULL,
    0,
    true,
};

static const MeshExtract extract_vert_idx = {
    extract_select_idx_init,
    NULL,
    NULL,
    extract_vert_idx_loop_bmesh,
    extract_vert_idx_loop_mesh,
    extract_vert_idx_ledge_bmesh,
    extract_vert_idx_ledge_mesh,
    extract_vert_idx_lvert_bmesh,
    extract_vert_idx_lvert_mesh,
    NULL,
    0,
    true,
};

static void *extract_select_fdot_idx_init(const MeshRenderData *mr, void *buf)
{
  static GPUVertFormat format = {0};
  if (format.attr_len == 0) {
    /* TODO rename "color" to something more descriptive. */
    GPU_vertformat_attr_add(&format, "color", GPU_COMP_U32, 1, GPU_FETCH_INT);
  }
  GPUVertBuf *vbo = buf;
  GPU_vertbuf_init_with_format(vbo, &format);
  GPU_vertbuf_data_alloc(vbo, mr->poly_len);
  return vbo->data;
}

static void extract_fdot_idx_loop_bmesh(const MeshRenderData *UNUSED(mr),
                                        int UNUSED(l),
                                        BMLoop *loop,
                                        void *data)
{
  ((uint32_t *)data)[BM_elem_index_get(loop->f)] = BM_elem_index_get(loop->f);
}

static void extract_fdot_idx_loop_mesh(const MeshRenderData *mr,
                                       int UNUSED(l),
                                       const MLoop *UNUSED(mloop),
                                       int p,
                                       const MPoly *UNUSED(mpoly),
                                       void *data)
{
  ((uint32_t *)data)[p] = (mr->p_origindex) ? mr->p_origindex[p] : p;
}

static const MeshExtract extract_fdot_idx = {
    extract_select_fdot_idx_init,
    NULL,
    NULL,
    extract_fdot_idx_loop_bmesh,
    extract_fdot_idx_loop_mesh,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    0,
    true,
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name ExtractTaskData
 * \{ */
typedef struct ExtractUserData {
  void *user_data;
} ExtractUserData;

typedef enum ExtractTaskDataType {
  EXTRACT_MESH_EXTRACT,
  EXTRACT_LINES_LOOSE,
} ExtractTaskDataType;

typedef struct ExtractTaskData {
  void *next, *prev;
  const MeshRenderData *mr;
  const MeshExtract *extract;
  ExtractTaskDataType tasktype;
  eMRIterType iter_type;
  int start, end;
  /** Decremented each time a task is finished. */
  int32_t *task_counter;
  void *buf;
  ExtractUserData *user_data;
} ExtractTaskData;

static ExtractTaskData *extract_task_data_create_mesh_extract(const MeshRenderData *mr,
                                                              const MeshExtract *extract,
                                                              void *buf,
                                                              int32_t *task_counter)
{
  ExtractTaskData *taskdata = MEM_mallocN(sizeof(*taskdata), __func__);
  taskdata->next = NULL;
  taskdata->prev = NULL;
  taskdata->tasktype = EXTRACT_MESH_EXTRACT;
  taskdata->mr = mr;
  taskdata->extract = extract;
  taskdata->buf = buf;

  /* ExtractUserData is shared between the iterations as it holds counters to detect if the
   * extraction is finished. To make sure the duplication of the userdata does not create a new
   * instance of the counters we allocate the userdata in its own container.
   *
   * This structure makes sure that when extract_init is called, that the user data of all
   * iterations are updated. */
  taskdata->user_data = MEM_callocN(sizeof(ExtractUserData), __func__);
  taskdata->iter_type = mesh_extract_iter_type(extract);
  taskdata->task_counter = task_counter;
  taskdata->start = 0;
  taskdata->end = INT_MAX;
  return taskdata;
}

static ExtractTaskData *extract_task_data_create_lines_loose(const MeshRenderData *mr)
{
  ExtractTaskData *taskdata = MEM_callocN(sizeof(*taskdata), __func__);
  taskdata->tasktype = EXTRACT_LINES_LOOSE;
  taskdata->mr = mr;
  return taskdata;
}

static void extract_task_data_free(void *data)
{
  ExtractTaskData *task_data = data;
  MEM_SAFE_FREE(task_data->user_data);
  MEM_freeN(task_data);
}

BLI_INLINE void mesh_extract_iter(const MeshRenderData *mr,
                                  const eMRIterType iter_type,
                                  int start,
                                  int end,
                                  const MeshExtract *extract,
                                  void *user_data)
{
  switch (mr->extract_type) {
    case MR_EXTRACT_BMESH:
      if (iter_type & MR_ITER_LOOPTRI) {
        int t_end = min_ii(mr->tri_len, end);
        for (int t = start; t < t_end; t++) {
          BMLoop **elt = &mr->edit_bmesh->looptris[t][0];
          extract->iter_looptri_bm(mr, t, elt, user_data);
        }
      }
      if (iter_type & MR_ITER_LOOP) {
        int l_end = min_ii(mr->poly_len, end);
        for (int f = start; f < l_end; f++) {
          BMFace *efa = BM_face_at_index(mr->bm, f);
          BMLoop *loop;
          BMIter l_iter;
          BM_ITER_ELEM (loop, &l_iter, efa, BM_LOOPS_OF_FACE) {
            extract->iter_loop_bm(mr, BM_elem_index_get(loop), loop, user_data);
          }
        }
      }
      if (iter_type & MR_ITER_LEDGE) {
        int le_end = min_ii(mr->edge_loose_len, end);
        for (int e = start; e < le_end; e++) {
          BMEdge *eed = BM_edge_at_index(mr->bm, mr->ledges[e]);
          extract->iter_ledge_bm(mr, e, eed, user_data);
        }
      }
      if (iter_type & MR_ITER_LVERT) {
        int lv_end = min_ii(mr->vert_loose_len, end);
        for (int v = start; v < lv_end; v++) {
          BMVert *eve = BM_vert_at_index(mr->bm, mr->lverts[v]);
          extract->iter_lvert_bm(mr, v, eve, user_data);
        }
      }
      break;
    case MR_EXTRACT_MAPPED:
    case MR_EXTRACT_MESH:
      if (iter_type & MR_ITER_LOOPTRI) {
        int t_end = min_ii(mr->tri_len, end);
        for (int t = start; t < t_end; t++) {
          extract->iter_looptri(mr, t, &mr->mlooptri[t], user_data);
        }
      }
      if (iter_type & MR_ITER_LOOP) {
        int l_end = min_ii(mr->poly_len, end);
        for (int p = start; p < l_end; p++) {
          const MPoly *mpoly = &mr->mpoly[p];
          int l = mpoly->loopstart;
          for (int i = 0; i < mpoly->totloop; i++, l++) {
            extract->iter_loop(mr, l, &mr->mloop[l], p, mpoly, user_data);
          }
        }
      }
      if (iter_type & MR_ITER_LEDGE) {
        int le_end = min_ii(mr->edge_loose_len, end);
        for (int e = start; e < le_end; e++) {
          extract->iter_ledge(mr, e, &mr->medge[mr->ledges[e]], user_data);
        }
      }
      if (iter_type & MR_ITER_LVERT) {
        int lv_end = min_ii(mr->vert_loose_len, end);
        for (int v = start; v < lv_end; v++) {
          extract->iter_lvert(mr, v, &mr->mvert[mr->lverts[v]], user_data);
        }
      }
      break;
  }
}

static void extract_init(ExtractTaskData *data)
{
  if (data->tasktype == EXTRACT_MESH_EXTRACT) {
    data->user_data->user_data = data->extract->init(data->mr, data->buf);
  }
}

static void extract_run(void *__restrict taskdata)
{
  ExtractTaskData *data = (ExtractTaskData *)taskdata;
  if (data->tasktype == EXTRACT_MESH_EXTRACT) {
    mesh_extract_iter(data->mr,
                      data->iter_type,
                      data->start,
                      data->end,
                      data->extract,
                      data->user_data->user_data);

    /* If this is the last task, we do the finish function. */
    int remainin_tasks = atomic_sub_and_fetch_int32(data->task_counter, 1);
    if (remainin_tasks == 0 && data->extract->finish != NULL) {
      data->extract->finish(data->mr, data->buf, data->user_data->user_data);
    }
  }
  else if (data->tasktype == EXTRACT_LINES_LOOSE) {
    extract_lines_loose_subbuffer(data->mr);
  }
}

static void extract_init_and_run(void *__restrict taskdata)
{
  extract_init((ExtractTaskData *)taskdata);
  extract_run(taskdata);
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Task Node - Update Mesh Render Data
 * \{ */
typedef struct MeshRenderDataUpdateTaskData {
  MeshRenderData *mr;
  eMRIterType iter_type;
  eMRDataType data_flag;
} MeshRenderDataUpdateTaskData;

static void mesh_render_data_update_task_data_free(MeshRenderDataUpdateTaskData *taskdata)
{
  BLI_assert(taskdata);
  MeshRenderData *mr = taskdata->mr;
  mesh_render_data_free(mr);
  MEM_freeN(taskdata);
}

static void mesh_extract_render_data_node_exec(void *__restrict task_data)
{
  MeshRenderDataUpdateTaskData *update_task_data = task_data;
  MeshRenderData *mr = update_task_data->mr;
  const eMRIterType iter_type = update_task_data->iter_type;
  const eMRDataType data_flag = update_task_data->data_flag;

  mesh_render_data_update_normals(mr, iter_type, data_flag);
  mesh_render_data_update_looptris(mr, iter_type, data_flag);
}

static struct TaskNode *mesh_extract_render_data_node_create(struct TaskGraph *task_graph,
                                                             MeshRenderData *mr,
                                                             const eMRIterType iter_type,
                                                             const eMRDataType data_flag)
{
  MeshRenderDataUpdateTaskData *task_data = MEM_mallocN(sizeof(MeshRenderDataUpdateTaskData),
                                                        __func__);
  task_data->mr = mr;
  task_data->iter_type = iter_type;
  task_data->data_flag = data_flag;

  struct TaskNode *task_node = BLI_task_graph_node_create(
      task_graph,
      mesh_extract_render_data_node_exec,
      task_data,
      (TaskGraphNodeFreeFunction)mesh_render_data_update_task_data_free);
  return task_node;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Task Node - Extract Single Threaded
 * \{ */
typedef struct ExtractSingleThreadedTaskData {
  ListBase task_datas;
} ExtractSingleThreadedTaskData;

static void extract_single_threaded_task_data_free(ExtractSingleThreadedTaskData *taskdata)
{
  BLI_assert(taskdata);
  LISTBASE_FOREACH_MUTABLE (ExtractTaskData *, td, &taskdata->task_datas) {
    extract_task_data_free(td);
  }
  BLI_listbase_clear(&taskdata->task_datas);
  MEM_freeN(taskdata);
}

static void extract_single_threaded_task_node_exec(void *__restrict task_data)
{
  ExtractSingleThreadedTaskData *extract_task_data = task_data;
  LISTBASE_FOREACH (ExtractTaskData *, td, &extract_task_data->task_datas) {
    extract_init_and_run(td);
  }
}

static struct TaskNode *extract_single_threaded_task_node_create(
    struct TaskGraph *task_graph, ExtractSingleThreadedTaskData *task_data)
{
  struct TaskNode *task_node = BLI_task_graph_node_create(
      task_graph,
      extract_single_threaded_task_node_exec,
      task_data,
      (TaskGraphNodeFreeFunction)extract_single_threaded_task_data_free);
  return task_node;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Task Node - UserData Initializer
 * \{ */
typedef struct UserDataInitTaskData {
  ListBase task_datas;
  int32_t *task_counters;

} UserDataInitTaskData;

static void user_data_init_task_data_free(UserDataInitTaskData *taskdata)
{
  BLI_assert(taskdata);
  LISTBASE_FOREACH_MUTABLE (ExtractTaskData *, td, &taskdata->task_datas) {
    extract_task_data_free(td);
  }
  BLI_listbase_clear(&taskdata->task_datas);
  MEM_SAFE_FREE(taskdata->task_counters);
  MEM_freeN(taskdata);
}

static void user_data_init_task_data_exec(void *__restrict task_data)
{
  UserDataInitTaskData *extract_task_data = task_data;
  LISTBASE_FOREACH (ExtractTaskData *, td, &extract_task_data->task_datas) {
    extract_init(td);
  }
}

static struct TaskNode *user_data_init_task_node_create(struct TaskGraph *task_graph,
                                                        UserDataInitTaskData *task_data)
{
  struct TaskNode *task_node = BLI_task_graph_node_create(
      task_graph,
      user_data_init_task_data_exec,
      task_data,
      (TaskGraphNodeFreeFunction)user_data_init_task_data_free);
  return task_node;
}

/** \} */
/* ---------------------------------------------------------------------- */
/** \name Extract Loop
 * \{ */

static void extract_range_task_create(struct TaskGraph *task_graph,
                                      struct TaskNode *task_node_user_data_init,
                                      ExtractTaskData *taskdata,
                                      const eMRIterType type,
                                      int start,
                                      int length)
{
  taskdata = MEM_dupallocN(taskdata);
  atomic_add_and_fetch_int32(taskdata->task_counter, 1);
  taskdata->iter_type = type;
  taskdata->start = start;
  taskdata->end = start + length;
  struct TaskNode *task_node = BLI_task_graph_node_create(
      task_graph, extract_run, taskdata, MEM_freeN);
  BLI_task_graph_edge_create(task_node_user_data_init, task_node);
}

static void extract_task_create(struct TaskGraph *task_graph,
                                struct TaskNode *task_node_mesh_render_data,
                                struct TaskNode *task_node_user_data_init,
                                ListBase *single_threaded_task_datas,
                                ListBase *user_data_init_task_datas,
                                const Scene *scene,
                                const MeshRenderData *mr,
                                const MeshExtract *extract,
                                void *buf,
                                int32_t *task_counter)
{
  BLI_assert(scene != NULL);
  const bool do_hq_normals = (scene->r.perf_flag & SCE_PERF_HQ_NORMALS) != 0;
  if (do_hq_normals && (extract == &extract_lnor)) {
    extract = &extract_lnor_hq;
  }
  if (do_hq_normals && (extract == &extract_tan)) {
    extract = &extract_tan_hq;
  }

  /* Divide extraction of the VBO/IBO into sensible chunks of works. */
  ExtractTaskData *taskdata = extract_task_data_create_mesh_extract(
      mr, extract, buf, task_counter);

  /* Simple heuristic. */
  const int chunk_size = 8192;
  const bool use_thread = (mr->loop_len + mr->loop_loose_len) > chunk_size;
  if (use_thread && extract->use_threading) {

    /* Divide task into sensible chunks. */
    if (taskdata->iter_type & MR_ITER_LOOPTRI) {
      for (int i = 0; i < mr->tri_len; i += chunk_size) {
        extract_range_task_create(
            task_graph, task_node_user_data_init, taskdata, MR_ITER_LOOPTRI, i, chunk_size);
      }
    }
    if (taskdata->iter_type & MR_ITER_LOOP) {
      for (int i = 0; i < mr->poly_len; i += chunk_size) {
        extract_range_task_create(
            task_graph, task_node_user_data_init, taskdata, MR_ITER_LOOP, i, chunk_size);
      }
    }
    if (taskdata->iter_type & MR_ITER_LEDGE) {
      for (int i = 0; i < mr->edge_loose_len; i += chunk_size) {
        extract_range_task_create(
            task_graph, task_node_user_data_init, taskdata, MR_ITER_LEDGE, i, chunk_size);
      }
    }
    if (taskdata->iter_type & MR_ITER_LVERT) {
      for (int i = 0; i < mr->vert_loose_len; i += chunk_size) {
        extract_range_task_create(
            task_graph, task_node_user_data_init, taskdata, MR_ITER_LVERT, i, chunk_size);
      }
    }
    BLI_addtail(user_data_init_task_datas, taskdata);
  }
  else if (use_thread) {
    /* One task for the whole VBO. */
    (*task_counter)++;
    struct TaskNode *one_task = BLI_task_graph_node_create(
        task_graph, extract_init_and_run, taskdata, extract_task_data_free);
    BLI_task_graph_edge_create(task_node_mesh_render_data, one_task);
  }
  else {
    /* Single threaded extraction. */
    (*task_counter)++;
    BLI_addtail(single_threaded_task_datas, taskdata);
  }
}

void mesh_buffer_cache_create_requested(struct TaskGraph *task_graph,
                                        MeshBatchCache *cache,
                                        MeshBufferCache mbc,
                                        Mesh *me,

                                        const bool is_editmode,
                                        const bool is_paint_mode,
                                        const float obmat[4][4],
                                        const bool do_final,
                                        const bool do_uvedit,
                                        const bool use_subsurf_fdots,
                                        const DRW_MeshCDMask *cd_layer_used,
                                        const Scene *scene,
                                        const ToolSettings *ts,
                                        const bool use_hide)
{
  /* For each mesh where batches needs to be updated a sub-graph will be added to the task_graph.
   * This sub-graph starts with an extract_render_data_node. This fills/converts the required data
   * from Mesh.
   *
   * Small extractions and extractions that can't be multi-threaded are grouped in a single
   * `extract_single_threaded_task_node`.
   *
   * Other extractions will create a node for each loop exceeding 8192 items. these nodes are
   * linked to the `user_data_init_task_node`. the `user_data_init_task_node` prepares the userdata
   * needed for the extraction based on the data extracted from the mesh. counters are used to
   * check if the finalize of a task has to be called.
   *
   *                           Mesh extraction sub graph
   *
   *                                                       +----------------------+
   *                                               +-----> | extract_task1_loop_1 |
   *                                               |       +----------------------+
   * +------------------+     +----------------------+     +----------------------+
   * | mesh_render_data | --> |                      | --> | extract_task1_loop_2 |
   * +------------------+     |                      |     +----------------------+
   *   |                      |                      |     +----------------------+
   *   |                      |    user_data_init    | --> | extract_task2_loop_1 |
   *   v                      |                      |     +----------------------+
   * +------------------+     |                      |     +----------------------+
   * | single_threaded  |     |                      | --> | extract_task2_loop_2 |
   * +------------------+     +----------------------+     +----------------------+
   *                                               |       +----------------------+
   *                                               +-----> | extract_task2_loop_3 |
   *                                                       +----------------------+
   */
  eMRIterType iter_flag = 0;
  eMRDataType data_flag = 0;

  const bool do_lines_loose_subbuffer = mbc.ibo.lines_loose != NULL;

#define TEST_ASSIGN(type, type_lowercase, name) \
  do { \
    if (DRW_TEST_ASSIGN_##type(mbc.type_lowercase.name)) { \
      iter_flag |= mesh_extract_iter_type(&extract_##name); \
      data_flag |= extract_##name.data_flag; \
    } \
  } while (0)

  TEST_ASSIGN(VBO, vbo, pos_nor);
  TEST_ASSIGN(VBO, vbo, lnor);
  TEST_ASSIGN(VBO, vbo, uv);
  TEST_ASSIGN(VBO, vbo, tan);
  TEST_ASSIGN(VBO, vbo, vcol);
  TEST_ASSIGN(VBO, vbo, orco);
  TEST_ASSIGN(VBO, vbo, edge_fac);
  TEST_ASSIGN(VBO, vbo, weights);
  TEST_ASSIGN(VBO, vbo, edit_data);
  TEST_ASSIGN(VBO, vbo, edituv_data);
  TEST_ASSIGN(VBO, vbo, stretch_area);
  TEST_ASSIGN(VBO, vbo, stretch_angle);
  TEST_ASSIGN(VBO, vbo, mesh_analysis);
  TEST_ASSIGN(VBO, vbo, fdots_pos);
  TEST_ASSIGN(VBO, vbo, fdots_nor);
  TEST_ASSIGN(VBO, vbo, fdots_uv);
  TEST_ASSIGN(VBO, vbo, fdots_edituv_data);
  TEST_ASSIGN(VBO, vbo, poly_idx);
  TEST_ASSIGN(VBO, vbo, edge_idx);
  TEST_ASSIGN(VBO, vbo, vert_idx);
  TEST_ASSIGN(VBO, vbo, fdot_idx);
  TEST_ASSIGN(VBO, vbo, skin_roots);

  TEST_ASSIGN(IBO, ibo, tris);
  TEST_ASSIGN(IBO, ibo, lines);
  TEST_ASSIGN(IBO, ibo, points);
  TEST_ASSIGN(IBO, ibo, fdots);
  TEST_ASSIGN(IBO, ibo, lines_paint_mask);
  TEST_ASSIGN(IBO, ibo, lines_adjacency);
  TEST_ASSIGN(IBO, ibo, edituv_tris);
  TEST_ASSIGN(IBO, ibo, edituv_lines);
  TEST_ASSIGN(IBO, ibo, edituv_points);
  TEST_ASSIGN(IBO, ibo, edituv_fdots);

  if (do_lines_loose_subbuffer) {
    iter_flag |= MR_ITER_LEDGE;
  }

#undef TEST_ASSIGN

#ifdef DEBUG_TIME
  double rdata_start = PIL_check_seconds_timer();
#endif

  MeshRenderData *mr = mesh_render_data_create(me,
                                               is_editmode,
                                               is_paint_mode,
                                               obmat,
                                               do_final,
                                               do_uvedit,
                                               cd_layer_used,
                                               ts,
                                               iter_flag,
                                               data_flag);
  mr->cache = cache; /* HACK */
  mr->use_hide = use_hide;
  mr->use_subsurf_fdots = use_subsurf_fdots;
  mr->use_final_mesh = do_final;

#ifdef DEBUG_TIME
  double rdata_end = PIL_check_seconds_timer();
#endif

  size_t counters_size = (sizeof(mbc) / sizeof(void *)) * sizeof(int32_t);
  int32_t *task_counters = MEM_callocN(counters_size, __func__);
  int counter_used = 0;

  struct TaskNode *task_node_mesh_render_data = mesh_extract_render_data_node_create(
      task_graph, mr, iter_flag, data_flag);
  ExtractSingleThreadedTaskData *single_threaded_task_data = MEM_callocN(
      sizeof(ExtractSingleThreadedTaskData), __func__);
  UserDataInitTaskData *user_data_init_task_data = MEM_callocN(sizeof(UserDataInitTaskData),
                                                               __func__);
  user_data_init_task_data->task_counters = task_counters;
  struct TaskNode *task_node_user_data_init = user_data_init_task_node_create(
      task_graph, user_data_init_task_data);

#define EXTRACT(buf, name) \
  if (mbc.buf.name) { \
    extract_task_create(task_graph, \
                        task_node_mesh_render_data, \
                        task_node_user_data_init, \
                        &single_threaded_task_data->task_datas, \
                        &user_data_init_task_data->task_datas, \
                        scene, \
                        mr, \
                        &extract_##name, \
                        mbc.buf.name, \
                        &task_counters[counter_used++]); \
  } \
  ((void)0)

  EXTRACT(vbo, pos_nor);
  EXTRACT(vbo, lnor);
  EXTRACT(vbo, uv);
  EXTRACT(vbo, tan);
  EXTRACT(vbo, vcol);
  EXTRACT(vbo, orco);
  EXTRACT(vbo, edge_fac);
  EXTRACT(vbo, weights);
  EXTRACT(vbo, edit_data);
  EXTRACT(vbo, edituv_data);
  EXTRACT(vbo, stretch_area);
  EXTRACT(vbo, stretch_angle);
  EXTRACT(vbo, mesh_analysis);
  EXTRACT(vbo, fdots_pos);
  EXTRACT(vbo, fdots_nor);
  EXTRACT(vbo, fdots_uv);
  EXTRACT(vbo, fdots_edituv_data);
  EXTRACT(vbo, poly_idx);
  EXTRACT(vbo, edge_idx);
  EXTRACT(vbo, vert_idx);
  EXTRACT(vbo, fdot_idx);
  EXTRACT(vbo, skin_roots);

  EXTRACT(ibo, tris);
  if (mbc.ibo.lines) {
    /* When `lines` and `lines_loose` are requested, schedule lines extraction that also creates
     * the `lines_loose` sub-buffer. */
    const MeshExtract *lines_extractor = do_lines_loose_subbuffer ?
                                             &extract_lines_with_lines_loose :
                                             &extract_lines;
    extract_task_create(task_graph,
                        task_node_mesh_render_data,
                        task_node_user_data_init,
                        &single_threaded_task_data->task_datas,
                        &user_data_init_task_data->task_datas,
                        scene,
                        mr,
                        lines_extractor,
                        mbc.ibo.lines,
                        &task_counters[counter_used++]);
  }
  else {
    if (do_lines_loose_subbuffer) {
      ExtractTaskData *taskdata = extract_task_data_create_lines_loose(mr);
      BLI_addtail(&single_threaded_task_data->task_datas, taskdata);
    }
  }
  EXTRACT(ibo, points);
  EXTRACT(ibo, fdots);
  EXTRACT(ibo, lines_paint_mask);
  EXTRACT(ibo, lines_adjacency);
  EXTRACT(ibo, edituv_tris);
  EXTRACT(ibo, edituv_lines);
  EXTRACT(ibo, edituv_points);
  EXTRACT(ibo, edituv_fdots);

  /* Only create the edge when there is user data that needs to be initialized.
   * The task is still part of the graph so the task_data will be freed when the graph is freed.
   */
  if (!BLI_listbase_is_empty(&user_data_init_task_data->task_datas)) {
    BLI_task_graph_edge_create(task_node_mesh_render_data, task_node_user_data_init);
  }

  if (!BLI_listbase_is_empty(&single_threaded_task_data->task_datas)) {
    struct TaskNode *task_node = extract_single_threaded_task_node_create(
        task_graph, single_threaded_task_data);
    BLI_task_graph_edge_create(task_node_mesh_render_data, task_node);
  }
  else {
    extract_single_threaded_task_data_free(single_threaded_task_data);
  }

  /* Trigger the sub-graph for this mesh. */
  BLI_task_graph_node_push_work(task_node_mesh_render_data);

#undef EXTRACT

#ifdef DEBUG_TIME
  BLI_task_graph_work_and_wait(task_graph);
  double end = PIL_check_seconds_timer();

  static double avg = 0;
  static double avg_fps = 0;
  static double avg_rdata = 0;
  static double end_prev = 0;

  if (end_prev == 0) {
    end_prev = end;
  }

  avg = avg * 0.95 + (end - rdata_end) * 0.05;
  avg_fps = avg_fps * 0.95 + (end - end_prev) * 0.05;
  avg_rdata = avg_rdata * 0.95 + (rdata_end - rdata_start) * 0.05;

  printf(
      "rdata %.0fms iter %.0fms (frame %.0fms)\n", avg_rdata * 1000, avg * 1000, avg_fps * 1000);

  end_prev = end;
#endif
}

/** \} */
