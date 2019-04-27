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
 * \ingroup bke
 */

#include "BLI_math.h"
#include "BLI_task.h"

#include "DNA_defs.h"
#include "DNA_customdata_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_mesh.h"
#include "BKE_mesh_tangent.h" /* for utility functions */
#include "BKE_editmesh.h"
#include "BKE_editmesh_tangent.h"

#include "MEM_guardedalloc.h"

/* interface */
#include "mikktspace.h"

/** \name Tangent Space Calculation
 * \{ */

/* Necessary complexity to handle looptri's as quads for correct tangents */
#define USE_LOOPTRI_DETECT_QUADS

typedef struct {
  const float (*precomputedFaceNormals)[3];
  const float (*precomputedLoopNormals)[3];
  const BMLoop *(*looptris)[3];
  int cd_loop_uv_offset; /* texture coordinates */
  const float (*orco)[3];
  float (*tangent)[4]; /* destination */
  int numTessFaces;

#ifdef USE_LOOPTRI_DETECT_QUADS
  /* map from 'fake' face index to looptri,
   * quads will point to the first looptri of the quad */
  const int *face_as_quad_map;
  int num_face_as_quad_map;
#endif

} SGLSLEditMeshToTangent;

#ifdef USE_LOOPTRI_DETECT_QUADS
/* seems weak but only used on quads */
static const BMLoop *bm_loop_at_face_index(const BMFace *f, int vert_index)
{
  const BMLoop *l = BM_FACE_FIRST_LOOP(f);
  while (vert_index--) {
    l = l->next;
  }
  return l;
}
#endif

static int emdm_ts_GetNumFaces(const SMikkTSpaceContext *pContext)
{
  SGLSLEditMeshToTangent *pMesh = pContext->m_pUserData;

#ifdef USE_LOOPTRI_DETECT_QUADS
  return pMesh->num_face_as_quad_map;
#else
  return pMesh->numTessFaces;
#endif
}

static int emdm_ts_GetNumVertsOfFace(const SMikkTSpaceContext *pContext, const int face_num)
{
#ifdef USE_LOOPTRI_DETECT_QUADS
  SGLSLEditMeshToTangent *pMesh = pContext->m_pUserData;
  if (pMesh->face_as_quad_map) {
    const BMLoop **lt = pMesh->looptris[pMesh->face_as_quad_map[face_num]];
    if (lt[0]->f->len == 4) {
      return 4;
    }
  }
  return 3;
#else
  UNUSED_VARS(pContext, face_num);
  return 3;
#endif
}

static void emdm_ts_GetPosition(const SMikkTSpaceContext *pContext,
                                float r_co[3],
                                const int face_num,
                                const int vert_index)
{
  //assert(vert_index >= 0 && vert_index < 4);
  SGLSLEditMeshToTangent *pMesh = pContext->m_pUserData;
  const BMLoop **lt;
  const BMLoop *l;

#ifdef USE_LOOPTRI_DETECT_QUADS
  if (pMesh->face_as_quad_map) {
    lt = pMesh->looptris[pMesh->face_as_quad_map[face_num]];
    if (lt[0]->f->len == 4) {
      l = bm_loop_at_face_index(lt[0]->f, vert_index);
      goto finally;
    }
    /* fall through to regular triangle */
  }
  else {
    lt = pMesh->looptris[face_num];
  }
#else
  lt = pMesh->looptris[face_num];
#endif
  l = lt[vert_index];

  const float *co;

finally:
  co = l->v->co;
  copy_v3_v3(r_co, co);
}

static void emdm_ts_GetTextureCoordinate(const SMikkTSpaceContext *pContext,
                                         float r_uv[2],
                                         const int face_num,
                                         const int vert_index)
{
  //assert(vert_index >= 0 && vert_index < 4);
  SGLSLEditMeshToTangent *pMesh = pContext->m_pUserData;
  const BMLoop **lt;
  const BMLoop *l;

#ifdef USE_LOOPTRI_DETECT_QUADS
  if (pMesh->face_as_quad_map) {
    lt = pMesh->looptris[pMesh->face_as_quad_map[face_num]];
    if (lt[0]->f->len == 4) {
      l = bm_loop_at_face_index(lt[0]->f, vert_index);
      goto finally;
    }
    /* fall through to regular triangle */
  }
  else {
    lt = pMesh->looptris[face_num];
  }
#else
  lt = pMesh->looptris[face_num];
#endif
  l = lt[vert_index];

finally:
  if (pMesh->cd_loop_uv_offset != -1) {
    const float *uv = BM_ELEM_CD_GET_VOID_P(l, pMesh->cd_loop_uv_offset);
    copy_v2_v2(r_uv, uv);
  }
  else {
    const float *orco = pMesh->orco[BM_elem_index_get(l->v)];
    map_to_sphere(&r_uv[0], &r_uv[1], orco[0], orco[1], orco[2]);
  }
}

static void emdm_ts_GetNormal(const SMikkTSpaceContext *pContext,
                              float r_no[3],
                              const int face_num,
                              const int vert_index)
{
  //assert(vert_index >= 0 && vert_index < 4);
  SGLSLEditMeshToTangent *pMesh = pContext->m_pUserData;
  const BMLoop **lt;
  const BMLoop *l;

#ifdef USE_LOOPTRI_DETECT_QUADS
  if (pMesh->face_as_quad_map) {
    lt = pMesh->looptris[pMesh->face_as_quad_map[face_num]];
    if (lt[0]->f->len == 4) {
      l = bm_loop_at_face_index(lt[0]->f, vert_index);
      goto finally;
    }
    /* fall through to regular triangle */
  }
  else {
    lt = pMesh->looptris[face_num];
  }
#else
  lt = pMesh->looptris[face_num];
#endif
  l = lt[vert_index];

finally:
  if (pMesh->precomputedLoopNormals) {
    copy_v3_v3(r_no, pMesh->precomputedLoopNormals[BM_elem_index_get(l)]);
  }
  else if (BM_elem_flag_test(l->f, BM_ELEM_SMOOTH) == 0) { /* flat */
    if (pMesh->precomputedFaceNormals) {
      copy_v3_v3(r_no, pMesh->precomputedFaceNormals[BM_elem_index_get(l->f)]);
    }
    else {
      copy_v3_v3(r_no, l->f->no);
    }
  }
  else {
    copy_v3_v3(r_no, l->v->no);
  }
}

static void emdm_ts_SetTSpace(const SMikkTSpaceContext *pContext,
                              const float fvTangent[3],
                              const float fSign,
                              const int face_num,
                              const int vert_index)
{
  //assert(vert_index >= 0 && vert_index < 4);
  SGLSLEditMeshToTangent *pMesh = pContext->m_pUserData;
  const BMLoop **lt;
  const BMLoop *l;

#ifdef USE_LOOPTRI_DETECT_QUADS
  if (pMesh->face_as_quad_map) {
    lt = pMesh->looptris[pMesh->face_as_quad_map[face_num]];
    if (lt[0]->f->len == 4) {
      l = bm_loop_at_face_index(lt[0]->f, vert_index);
      goto finally;
    }
    /* fall through to regular triangle */
  }
  else {
    lt = pMesh->looptris[face_num];
  }
#else
  lt = pMesh->looptris[face_num];
#endif
  l = lt[vert_index];

  float *pRes;

finally:
  pRes = pMesh->tangent[BM_elem_index_get(l)];
  copy_v3_v3(pRes, fvTangent);
  pRes[3] = fSign;
}

static void emDM_calc_loop_tangents_thread(TaskPool *__restrict UNUSED(pool),
                                           void *taskdata,
                                           int UNUSED(threadid))
{
  struct SGLSLEditMeshToTangent *mesh2tangent = taskdata;
  /* new computation method */
  {
    SMikkTSpaceContext sContext = {NULL};
    SMikkTSpaceInterface sInterface = {NULL};
    sContext.m_pUserData = mesh2tangent;
    sContext.m_pInterface = &sInterface;
    sInterface.m_getNumFaces = emdm_ts_GetNumFaces;
    sInterface.m_getNumVerticesOfFace = emdm_ts_GetNumVertsOfFace;
    sInterface.m_getPosition = emdm_ts_GetPosition;
    sInterface.m_getTexCoord = emdm_ts_GetTextureCoordinate;
    sInterface.m_getNormal = emdm_ts_GetNormal;
    sInterface.m_setTSpaceBasic = emdm_ts_SetTSpace;
    /* 0 if failed */
    genTangSpaceDefault(&sContext);
  }
}

/**
 * \see #BKE_mesh_calc_loop_tangent, same logic but used arrays instead of #BMesh data.
 *
 * \note This function is not so normal, its using `bm->ldata` as input,
 * but output's to `dm->loopData`.
 * This is done because #CD_TANGENT is cache data used only for drawing.
 */
void BKE_editmesh_loop_tangent_calc(BMEditMesh *em,
                                    bool calc_active_tangent,
                                    const char (*tangent_names)[MAX_NAME],
                                    int tangent_names_len,
                                    const float (*poly_normals)[3],
                                    const float (*loop_normals)[3],
                                    const float (*vert_orco)[3],
                                    /* result */
                                    CustomData *loopdata_out,
                                    const uint loopdata_out_len,
                                    short *tangent_mask_curr_p)
{
  BMesh *bm = em->bm;

  int act_uv_n = -1;
  int ren_uv_n = -1;
  bool calc_act = false;
  bool calc_ren = false;
  char act_uv_name[MAX_NAME];
  char ren_uv_name[MAX_NAME];
  short tangent_mask = 0;
  short tangent_mask_curr = *tangent_mask_curr_p;

  BKE_mesh_calc_loop_tangent_step_0(&bm->ldata,
                                    calc_active_tangent,
                                    tangent_names,
                                    tangent_names_len,
                                    &calc_act,
                                    &calc_ren,
                                    &act_uv_n,
                                    &ren_uv_n,
                                    act_uv_name,
                                    ren_uv_name,
                                    &tangent_mask);

  if ((tangent_mask_curr | tangent_mask) != tangent_mask_curr) {
    for (int i = 0; i < tangent_names_len; i++) {
      if (tangent_names[i][0]) {
        BKE_mesh_add_loop_tangent_named_layer_for_uv(
            &bm->ldata, loopdata_out, (int)loopdata_out_len, tangent_names[i]);
      }
    }
    if ((tangent_mask & DM_TANGENT_MASK_ORCO) &&
        CustomData_get_named_layer_index(loopdata_out, CD_TANGENT, "") == -1) {
      CustomData_add_layer_named(
          loopdata_out, CD_TANGENT, CD_CALLOC, NULL, (int)loopdata_out_len, "");
    }
    if (calc_act && act_uv_name[0]) {
      BKE_mesh_add_loop_tangent_named_layer_for_uv(
          &bm->ldata, loopdata_out, (int)loopdata_out_len, act_uv_name);
    }
    if (calc_ren && ren_uv_name[0]) {
      BKE_mesh_add_loop_tangent_named_layer_for_uv(
          &bm->ldata, loopdata_out, (int)loopdata_out_len, ren_uv_name);
    }
    int totface = em->tottri;
#ifdef USE_LOOPTRI_DETECT_QUADS
    int num_face_as_quad_map;
    int *face_as_quad_map = NULL;

    /* map faces to quads */
    if (em->tottri != bm->totface) {
      /* over alloc, since we dont know how many ngon or quads we have */

      /* map fake face index to looptri */
      face_as_quad_map = MEM_mallocN(sizeof(int) * totface, __func__);
      int i, j;
      for (i = 0, j = 0; j < totface; i++, j++) {
        face_as_quad_map[i] = j;
        /* step over all quads */
        if (em->looptris[j][0]->f->len == 4) {
          j++; /* skips the nest looptri */
        }
      }
      num_face_as_quad_map = i;
    }
    else {
      num_face_as_quad_map = totface;
    }
#endif
    /* Calculation */
    if (em->tottri != 0) {
      TaskScheduler *scheduler = BLI_task_scheduler_get();
      TaskPool *task_pool;
      task_pool = BLI_task_pool_create(scheduler, NULL);

      tangent_mask_curr = 0;
      /* Calculate tangent layers */
      SGLSLEditMeshToTangent data_array[MAX_MTFACE];
      int index = 0;
      int n = 0;
      CustomData_update_typemap(loopdata_out);
      const int tangent_layer_num = CustomData_number_of_layers(loopdata_out, CD_TANGENT);
      for (n = 0; n < tangent_layer_num; n++) {
        index = CustomData_get_layer_index_n(loopdata_out, CD_TANGENT, n);
        BLI_assert(n < MAX_MTFACE);
        SGLSLEditMeshToTangent *mesh2tangent = &data_array[n];
        mesh2tangent->numTessFaces = em->tottri;
#ifdef USE_LOOPTRI_DETECT_QUADS
        mesh2tangent->face_as_quad_map = face_as_quad_map;
        mesh2tangent->num_face_as_quad_map = num_face_as_quad_map;
#endif
        mesh2tangent->precomputedFaceNormals = poly_normals;
        /* Note, we assume we do have tessellated loop normals at this point
         * (in case it is object-enabled), have to check this is valid. */
        mesh2tangent->precomputedLoopNormals = loop_normals;
        mesh2tangent->cd_loop_uv_offset = CustomData_get_n_offset(&bm->ldata, CD_MLOOPUV, n);

        /* needed for indexing loop-tangents */
        int htype_index = BM_LOOP;
        if (mesh2tangent->cd_loop_uv_offset == -1) {
          mesh2tangent->orco = vert_orco;
          if (!mesh2tangent->orco) {
            continue;
          }
          /* needed for orco lookups */
          htype_index |= BM_VERT;
          tangent_mask_curr |= DM_TANGENT_MASK_ORCO;
        }
        else {
          /* Fill the resulting tangent_mask */
          int uv_ind = CustomData_get_named_layer_index(
              &bm->ldata, CD_MLOOPUV, loopdata_out->layers[index].name);
          int uv_start = CustomData_get_layer_index(&bm->ldata, CD_MLOOPUV);
          BLI_assert(uv_ind != -1 && uv_start != -1);
          BLI_assert(uv_ind - uv_start < MAX_MTFACE);
          tangent_mask_curr |= 1 << (uv_ind - uv_start);
        }
        if (mesh2tangent->precomputedFaceNormals) {
          /* needed for face normal lookups */
          htype_index |= BM_FACE;
        }
        BM_mesh_elem_index_ensure(bm, htype_index);

        mesh2tangent->looptris = (const BMLoop *(*)[3])em->looptris;
        mesh2tangent->tangent = loopdata_out->layers[index].data;

        BLI_task_pool_push(
            task_pool, emDM_calc_loop_tangents_thread, mesh2tangent, false, TASK_PRIORITY_LOW);
      }

      BLI_assert(tangent_mask_curr == tangent_mask);
      BLI_task_pool_work_and_wait(task_pool);
      BLI_task_pool_free(task_pool);
    }
    else {
      tangent_mask_curr = tangent_mask;
    }
#ifdef USE_LOOPTRI_DETECT_QUADS
    if (face_as_quad_map) {
      MEM_freeN(face_as_quad_map);
    }
#  undef USE_LOOPTRI_DETECT_QUADS
#endif
  }

  *tangent_mask_curr_p = tangent_mask_curr;

  int act_uv_index = CustomData_get_layer_index_n(&bm->ldata, CD_MLOOPUV, act_uv_n);
  if (act_uv_index >= 0) {
    int tan_index = CustomData_get_named_layer_index(
        loopdata_out, CD_TANGENT, bm->ldata.layers[act_uv_index].name);
    CustomData_set_layer_active_index(loopdata_out, CD_TANGENT, tan_index);
  } /* else tangent has been built from orco */

  /* Update render layer index */
  int ren_uv_index = CustomData_get_layer_index_n(&bm->ldata, CD_MLOOPUV, ren_uv_n);
  if (ren_uv_index >= 0) {
    int tan_index = CustomData_get_named_layer_index(
        loopdata_out, CD_TANGENT, bm->ldata.layers[ren_uv_index].name);
    CustomData_set_layer_render_index(loopdata_out, CD_TANGENT, tan_index);
  } /* else tangent has been built from orco */
}

/** \} */
