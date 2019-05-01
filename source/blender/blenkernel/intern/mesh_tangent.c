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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 *
 * Functions to evaluate mesh tangents.
 */

#include <limits.h>

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_task.h"

#include "BKE_customdata.h"
#include "BKE_mesh.h"
#include "BKE_mesh_tangent.h"
#include "BKE_mesh_runtime.h"
#include "BKE_report.h"

#include "BLI_strict_flags.h"

#include "atomic_ops.h"
#include "mikktspace.h"

/* -------------------------------------------------------------------- */
/** \name Mesh Tangent Calculations (Single Layer)
 * \{ */

/* Tangent space utils. */

/* User data. */
typedef struct {
  const MPoly *mpolys;  /* faces */
  const MLoop *mloops;  /* faces's vertices */
  const MVert *mverts;  /* vertices */
  const MLoopUV *luvs;  /* texture coordinates */
  float (*lnors)[3];    /* loops' normals */
  float (*tangents)[4]; /* output tangents */
  int num_polys;        /* number of polygons */
} BKEMeshToTangent;

/* Mikktspace's API */
static int get_num_faces(const SMikkTSpaceContext *pContext)
{
  BKEMeshToTangent *p_mesh = (BKEMeshToTangent *)pContext->m_pUserData;
  return p_mesh->num_polys;
}

static int get_num_verts_of_face(const SMikkTSpaceContext *pContext, const int face_idx)
{
  BKEMeshToTangent *p_mesh = (BKEMeshToTangent *)pContext->m_pUserData;
  return p_mesh->mpolys[face_idx].totloop;
}

static void get_position(const SMikkTSpaceContext *pContext,
                         float r_co[3],
                         const int face_idx,
                         const int vert_idx)
{
  BKEMeshToTangent *p_mesh = (BKEMeshToTangent *)pContext->m_pUserData;
  const int loop_idx = p_mesh->mpolys[face_idx].loopstart + vert_idx;
  copy_v3_v3(r_co, p_mesh->mverts[p_mesh->mloops[loop_idx].v].co);
}

static void get_texture_coordinate(const SMikkTSpaceContext *pContext,
                                   float r_uv[2],
                                   const int face_idx,
                                   const int vert_idx)
{
  BKEMeshToTangent *p_mesh = (BKEMeshToTangent *)pContext->m_pUserData;
  copy_v2_v2(r_uv, p_mesh->luvs[p_mesh->mpolys[face_idx].loopstart + vert_idx].uv);
}

static void get_normal(const SMikkTSpaceContext *pContext,
                       float r_no[3],
                       const int face_idx,
                       const int vert_idx)
{
  BKEMeshToTangent *p_mesh = (BKEMeshToTangent *)pContext->m_pUserData;
  copy_v3_v3(r_no, p_mesh->lnors[p_mesh->mpolys[face_idx].loopstart + vert_idx]);
}

static void set_tspace(const SMikkTSpaceContext *pContext,
                       const float fv_tangent[3],
                       const float face_sign,
                       const int face_idx,
                       const int vert_idx)
{
  BKEMeshToTangent *p_mesh = (BKEMeshToTangent *)pContext->m_pUserData;
  float *p_res = p_mesh->tangents[p_mesh->mpolys[face_idx].loopstart + vert_idx];
  copy_v3_v3(p_res, fv_tangent);
  p_res[3] = face_sign;
}

/**
 * Compute simplified tangent space normals, i.e.
 * tangent vector + sign of bi-tangent one, which combined with
 * split normals can be used to recreate the full tangent space.
 * Note: * The mesh should be made of only tris and quads!
 */
void BKE_mesh_calc_loop_tangent_single_ex(const MVert *mverts,
                                          const int UNUSED(numVerts),
                                          const MLoop *mloops,
                                          float (*r_looptangent)[4],
                                          float (*loopnors)[3],
                                          const MLoopUV *loopuvs,
                                          const int UNUSED(numLoops),
                                          const MPoly *mpolys,
                                          const int numPolys,
                                          ReportList *reports)
{
  BKEMeshToTangent mesh_to_tangent = {NULL};
  SMikkTSpaceContext s_context = {NULL};
  SMikkTSpaceInterface s_interface = {NULL};

  const MPoly *mp;
  int mp_index;

  /* First check we do have a tris/quads only mesh. */
  for (mp = mpolys, mp_index = 0; mp_index < numPolys; mp++, mp_index++) {
    if (mp->totloop > 4) {
      BKE_report(
          reports, RPT_ERROR, "Tangent space can only be computed for tris/quads, aborting");
      return;
    }
  }

  /* Compute Mikktspace's tangent normals. */
  mesh_to_tangent.mpolys = mpolys;
  mesh_to_tangent.mloops = mloops;
  mesh_to_tangent.mverts = mverts;
  mesh_to_tangent.luvs = loopuvs;
  mesh_to_tangent.lnors = loopnors;
  mesh_to_tangent.tangents = r_looptangent;
  mesh_to_tangent.num_polys = numPolys;

  s_context.m_pUserData = &mesh_to_tangent;
  s_context.m_pInterface = &s_interface;
  s_interface.m_getNumFaces = get_num_faces;
  s_interface.m_getNumVerticesOfFace = get_num_verts_of_face;
  s_interface.m_getPosition = get_position;
  s_interface.m_getTexCoord = get_texture_coordinate;
  s_interface.m_getNormal = get_normal;
  s_interface.m_setTSpaceBasic = set_tspace;

  /* 0 if failed */
  if (genTangSpaceDefault(&s_context) == false) {
    BKE_report(reports, RPT_ERROR, "Mikktspace failed to generate tangents for this mesh!");
  }
}

/**
 * Wrapper around BKE_mesh_calc_loop_tangent_single_ex, which takes care of most boiling code.
 * \note
 * - There must be a valid loop's CD_NORMALS available.
 * - The mesh should be made of only tris and quads!
 */
void BKE_mesh_calc_loop_tangent_single(Mesh *mesh,
                                       const char *uvmap,
                                       float (*r_looptangents)[4],
                                       ReportList *reports)
{
  MLoopUV *loopuvs;
  float(*loopnors)[3];

  /* Check we have valid texture coordinates first! */
  if (uvmap) {
    loopuvs = CustomData_get_layer_named(&mesh->ldata, CD_MLOOPUV, uvmap);
  }
  else {
    loopuvs = CustomData_get_layer(&mesh->ldata, CD_MLOOPUV);
  }
  if (!loopuvs) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Tangent space computation needs an UVMap, \"%s\" not found, aborting",
                uvmap);
    return;
  }

  loopnors = CustomData_get_layer(&mesh->ldata, CD_NORMAL);
  if (!loopnors) {
    BKE_report(
        reports, RPT_ERROR, "Tangent space computation needs loop normals, none found, aborting");
    return;
  }

  BKE_mesh_calc_loop_tangent_single_ex(mesh->mvert,
                                       mesh->totvert,
                                       mesh->mloop,
                                       r_looptangents,
                                       loopnors,
                                       loopuvs,
                                       mesh->totloop,
                                       mesh->mpoly,
                                       mesh->totpoly,
                                       reports);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Tangent Calculations (All Layers)
 * \{ */

/* Necessary complexity to handle looptri's as quads for correct tangents */
#define USE_LOOPTRI_DETECT_QUADS

typedef struct {
  const float (*precomputedFaceNormals)[3];
  const float (*precomputedLoopNormals)[3];
  const MLoopTri *looptri;
  MLoopUV *mloopuv;   /* texture coordinates */
  const MPoly *mpoly; /* indices */
  const MLoop *mloop; /* indices */
  const MVert *mvert; /* vertices & normals */
  const float (*orco)[3];
  float (*tangent)[4]; /* destination */
  int numTessFaces;

#ifdef USE_LOOPTRI_DETECT_QUADS
  /* map from 'fake' face index to looptri,
   * quads will point to the first looptri of the quad */
  const int *face_as_quad_map;
  int num_face_as_quad_map;
#endif

} SGLSLMeshToTangent;

/* interface */
static int dm_ts_GetNumFaces(const SMikkTSpaceContext *pContext)
{
  SGLSLMeshToTangent *pMesh = pContext->m_pUserData;

#ifdef USE_LOOPTRI_DETECT_QUADS
  return pMesh->num_face_as_quad_map;
#else
  return pMesh->numTessFaces;
#endif
}

static int dm_ts_GetNumVertsOfFace(const SMikkTSpaceContext *pContext, const int face_num)
{
#ifdef USE_LOOPTRI_DETECT_QUADS
  SGLSLMeshToTangent *pMesh = pContext->m_pUserData;
  if (pMesh->face_as_quad_map) {
    const MLoopTri *lt = &pMesh->looptri[pMesh->face_as_quad_map[face_num]];
    const MPoly *mp = &pMesh->mpoly[lt->poly];
    if (mp->totloop == 4) {
      return 4;
    }
  }
  return 3;
#else
  UNUSED_VARS(pContext, face_num);
  return 3;
#endif
}

static void dm_ts_GetPosition(const SMikkTSpaceContext *pContext,
                              float r_co[3],
                              const int face_num,
                              const int vert_index)
{
  // assert(vert_index >= 0 && vert_index < 4);
  SGLSLMeshToTangent *pMesh = pContext->m_pUserData;
  const MLoopTri *lt;
  uint loop_index;
  const float *co;

#ifdef USE_LOOPTRI_DETECT_QUADS
  if (pMesh->face_as_quad_map) {
    lt = &pMesh->looptri[pMesh->face_as_quad_map[face_num]];
    const MPoly *mp = &pMesh->mpoly[lt->poly];
    if (mp->totloop == 4) {
      loop_index = (uint)(mp->loopstart + vert_index);
      goto finally;
    }
    /* fall through to regular triangle */
  }
  else {
    lt = &pMesh->looptri[face_num];
  }
#else
  lt = &pMesh->looptri[face_num];
#endif
  loop_index = lt->tri[vert_index];

finally:
  co = pMesh->mvert[pMesh->mloop[loop_index].v].co;
  copy_v3_v3(r_co, co);
}

static void dm_ts_GetTextureCoordinate(const SMikkTSpaceContext *pContext,
                                       float r_uv[2],
                                       const int face_num,
                                       const int vert_index)
{
  // assert(vert_index >= 0 && vert_index < 4);
  SGLSLMeshToTangent *pMesh = pContext->m_pUserData;
  const MLoopTri *lt;
  uint loop_index;

#ifdef USE_LOOPTRI_DETECT_QUADS
  if (pMesh->face_as_quad_map) {
    lt = &pMesh->looptri[pMesh->face_as_quad_map[face_num]];
    const MPoly *mp = &pMesh->mpoly[lt->poly];
    if (mp->totloop == 4) {
      loop_index = (uint)(mp->loopstart + vert_index);
      goto finally;
    }
    /* fall through to regular triangle */
  }
  else {
    lt = &pMesh->looptri[face_num];
  }
#else
  lt = &pMesh->looptri[face_num];
#endif
  loop_index = lt->tri[vert_index];

finally:
  if (pMesh->mloopuv != NULL) {
    const float *uv = pMesh->mloopuv[loop_index].uv;
    copy_v2_v2(r_uv, uv);
  }
  else {
    const float *orco = pMesh->orco[pMesh->mloop[loop_index].v];
    map_to_sphere(&r_uv[0], &r_uv[1], orco[0], orco[1], orco[2]);
  }
}

static void dm_ts_GetNormal(const SMikkTSpaceContext *pContext,
                            float r_no[3],
                            const int face_num,
                            const int vert_index)
{
  // assert(vert_index >= 0 && vert_index < 4);
  SGLSLMeshToTangent *pMesh = (SGLSLMeshToTangent *)pContext->m_pUserData;
  const MLoopTri *lt;
  uint loop_index;

#ifdef USE_LOOPTRI_DETECT_QUADS
  if (pMesh->face_as_quad_map) {
    lt = &pMesh->looptri[pMesh->face_as_quad_map[face_num]];
    const MPoly *mp = &pMesh->mpoly[lt->poly];
    if (mp->totloop == 4) {
      loop_index = (uint)(mp->loopstart + vert_index);
      goto finally;
    }
    /* fall through to regular triangle */
  }
  else {
    lt = &pMesh->looptri[face_num];
  }
#else
  lt = &pMesh->looptri[face_num];
#endif
  loop_index = lt->tri[vert_index];

finally:
  if (pMesh->precomputedLoopNormals) {
    copy_v3_v3(r_no, pMesh->precomputedLoopNormals[loop_index]);
  }
  else if ((pMesh->mpoly[lt->poly].flag & ME_SMOOTH) == 0) { /* flat */
    if (pMesh->precomputedFaceNormals) {
      copy_v3_v3(r_no, pMesh->precomputedFaceNormals[lt->poly]);
    }
    else {
#ifdef USE_LOOPTRI_DETECT_QUADS
      const MPoly *mp = &pMesh->mpoly[lt->poly];
      if (mp->totloop == 4) {
        normal_quad_v3(r_no,
                       pMesh->mvert[pMesh->mloop[mp->loopstart + 0].v].co,
                       pMesh->mvert[pMesh->mloop[mp->loopstart + 1].v].co,
                       pMesh->mvert[pMesh->mloop[mp->loopstart + 2].v].co,
                       pMesh->mvert[pMesh->mloop[mp->loopstart + 3].v].co);
      }
      else
#endif
      {
        normal_tri_v3(r_no,
                      pMesh->mvert[pMesh->mloop[lt->tri[0]].v].co,
                      pMesh->mvert[pMesh->mloop[lt->tri[1]].v].co,
                      pMesh->mvert[pMesh->mloop[lt->tri[2]].v].co);
      }
    }
  }
  else {
    const short *no = pMesh->mvert[pMesh->mloop[loop_index].v].no;
    normal_short_to_float_v3(r_no, no);
  }
}

static void dm_ts_SetTSpace(const SMikkTSpaceContext *pContext,
                            const float fvTangent[3],
                            const float fSign,
                            const int face_num,
                            const int vert_index)
{
  // assert(vert_index >= 0 && vert_index < 4);
  SGLSLMeshToTangent *pMesh = (SGLSLMeshToTangent *)pContext->m_pUserData;
  const MLoopTri *lt;
  uint loop_index;

#ifdef USE_LOOPTRI_DETECT_QUADS
  if (pMesh->face_as_quad_map) {
    lt = &pMesh->looptri[pMesh->face_as_quad_map[face_num]];
    const MPoly *mp = &pMesh->mpoly[lt->poly];
    if (mp->totloop == 4) {
      loop_index = (uint)(mp->loopstart + vert_index);
      goto finally;
    }
    /* fall through to regular triangle */
  }
  else {
    lt = &pMesh->looptri[face_num];
  }
#else
  lt = &pMesh->looptri[face_num];
#endif
  loop_index = lt->tri[vert_index];

  float *pRes;

finally:
  pRes = pMesh->tangent[loop_index];
  copy_v3_v3(pRes, fvTangent);
  pRes[3] = fSign;
}

static void DM_calc_loop_tangents_thread(TaskPool *__restrict UNUSED(pool),
                                         void *taskdata,
                                         int UNUSED(threadid))
{
  struct SGLSLMeshToTangent *mesh2tangent = taskdata;
  /* new computation method */
  {
    SMikkTSpaceContext sContext = {NULL};
    SMikkTSpaceInterface sInterface = {NULL};

    sContext.m_pUserData = mesh2tangent;
    sContext.m_pInterface = &sInterface;
    sInterface.m_getNumFaces = dm_ts_GetNumFaces;
    sInterface.m_getNumVerticesOfFace = dm_ts_GetNumVertsOfFace;
    sInterface.m_getPosition = dm_ts_GetPosition;
    sInterface.m_getTexCoord = dm_ts_GetTextureCoordinate;
    sInterface.m_getNormal = dm_ts_GetNormal;
    sInterface.m_setTSpaceBasic = dm_ts_SetTSpace;

    /* 0 if failed */
    genTangSpaceDefault(&sContext);
  }
}

void BKE_mesh_add_loop_tangent_named_layer_for_uv(CustomData *uv_data,
                                                  CustomData *tan_data,
                                                  int numLoopData,
                                                  const char *layer_name)
{
  if (CustomData_get_named_layer_index(tan_data, CD_TANGENT, layer_name) == -1 &&
      CustomData_get_named_layer_index(uv_data, CD_MLOOPUV, layer_name) != -1) {
    CustomData_add_layer_named(tan_data, CD_TANGENT, CD_CALLOC, NULL, numLoopData, layer_name);
  }
}

/**
 * Here we get some useful information such as active uv layer name and
 * search if it is already in tangent_names.
 * Also, we calculate tangent_mask that works as a descriptor of tangents state.
 * If tangent_mask has changed, then recalculate tangents.
 */
void BKE_mesh_calc_loop_tangent_step_0(const CustomData *loopData,
                                       bool calc_active_tangent,
                                       const char (*tangent_names)[MAX_NAME],
                                       int tangent_names_count,
                                       bool *rcalc_act,
                                       bool *rcalc_ren,
                                       int *ract_uv_n,
                                       int *rren_uv_n,
                                       char *ract_uv_name,
                                       char *rren_uv_name,
                                       short *rtangent_mask)
{
  /* Active uv in viewport */
  int layer_index = CustomData_get_layer_index(loopData, CD_MLOOPUV);
  *ract_uv_n = CustomData_get_active_layer(loopData, CD_MLOOPUV);
  ract_uv_name[0] = 0;
  if (*ract_uv_n != -1) {
    strcpy(ract_uv_name, loopData->layers[*ract_uv_n + layer_index].name);
  }

  /* Active tangent in render */
  *rren_uv_n = CustomData_get_render_layer(loopData, CD_MLOOPUV);
  rren_uv_name[0] = 0;
  if (*rren_uv_n != -1) {
    strcpy(rren_uv_name, loopData->layers[*rren_uv_n + layer_index].name);
  }

  /* If active tangent not in tangent_names we take it into account */
  *rcalc_act = false;
  *rcalc_ren = false;
  for (int i = 0; i < tangent_names_count; i++) {
    if (tangent_names[i][0] == 0) {
      calc_active_tangent = true;
    }
  }
  if (calc_active_tangent) {
    *rcalc_act = true;
    *rcalc_ren = true;
    for (int i = 0; i < tangent_names_count; i++) {
      if (STREQ(ract_uv_name, tangent_names[i])) {
        *rcalc_act = false;
      }
      if (STREQ(rren_uv_name, tangent_names[i])) {
        *rcalc_ren = false;
      }
    }
  }
  *rtangent_mask = 0;

  const int uv_layer_num = CustomData_number_of_layers(loopData, CD_MLOOPUV);
  for (int n = 0; n < uv_layer_num; n++) {
    const char *name = CustomData_get_layer_name(loopData, CD_MLOOPUV, n);
    bool add = false;
    for (int i = 0; i < tangent_names_count; i++) {
      if (tangent_names[i][0] && STREQ(tangent_names[i], name)) {
        add = true;
        break;
      }
    }
    if (!add && ((*rcalc_act && ract_uv_name[0] && STREQ(ract_uv_name, name)) ||
                 (*rcalc_ren && rren_uv_name[0] && STREQ(rren_uv_name, name)))) {
      add = true;
    }
    if (add) {
      *rtangent_mask |= (short)(1 << n);
    }
  }

  if (uv_layer_num == 0) {
    *rtangent_mask |= DM_TANGENT_MASK_ORCO;
  }
}

/**
 * See: #BKE_editmesh_loop_tangent_calc (matching logic).
 */
void BKE_mesh_calc_loop_tangent_ex(const MVert *mvert,
                                   const MPoly *mpoly,
                                   const uint mpoly_len,
                                   const MLoop *mloop,
                                   const MLoopTri *looptri,
                                   const uint looptri_len,

                                   CustomData *loopdata,
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
  int act_uv_n = -1;
  int ren_uv_n = -1;
  bool calc_act = false;
  bool calc_ren = false;
  char act_uv_name[MAX_NAME];
  char ren_uv_name[MAX_NAME];
  short tangent_mask = 0;
  short tangent_mask_curr = *tangent_mask_curr_p;

  BKE_mesh_calc_loop_tangent_step_0(loopdata,
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
    /* Check we have all the needed layers */
    /* Allocate needed tangent layers */
    for (int i = 0; i < tangent_names_len; i++) {
      if (tangent_names[i][0]) {
        BKE_mesh_add_loop_tangent_named_layer_for_uv(
            loopdata, loopdata_out, (int)loopdata_out_len, tangent_names[i]);
      }
    }
    if ((tangent_mask & DM_TANGENT_MASK_ORCO) &&
        CustomData_get_named_layer_index(loopdata, CD_TANGENT, "") == -1) {
      CustomData_add_layer_named(
          loopdata_out, CD_TANGENT, CD_CALLOC, NULL, (int)loopdata_out_len, "");
    }
    if (calc_act && act_uv_name[0]) {
      BKE_mesh_add_loop_tangent_named_layer_for_uv(
          loopdata, loopdata_out, (int)loopdata_out_len, act_uv_name);
    }
    if (calc_ren && ren_uv_name[0]) {
      BKE_mesh_add_loop_tangent_named_layer_for_uv(
          loopdata, loopdata_out, (int)loopdata_out_len, ren_uv_name);
    }

#ifdef USE_LOOPTRI_DETECT_QUADS
    int num_face_as_quad_map;
    int *face_as_quad_map = NULL;

    /* map faces to quads */
    if (looptri_len != mpoly_len) {
      /* over alloc, since we dont know how many ngon or quads we have */

      /* map fake face index to looptri */
      face_as_quad_map = MEM_mallocN(sizeof(int) * looptri_len, __func__);
      int k, j;
      for (k = 0, j = 0; j < (int)looptri_len; k++, j++) {
        face_as_quad_map[k] = j;
        /* step over all quads */
        if (mpoly[looptri[j].poly].totloop == 4) {
          j++; /* skips the nest looptri */
        }
      }
      num_face_as_quad_map = k;
    }
    else {
      num_face_as_quad_map = (int)looptri_len;
    }
#endif

    /* Calculation */
    if (looptri_len != 0) {
      TaskScheduler *scheduler = BLI_task_scheduler_get();
      TaskPool *task_pool;
      task_pool = BLI_task_pool_create(scheduler, NULL);

      tangent_mask_curr = 0;
      /* Calculate tangent layers */
      SGLSLMeshToTangent data_array[MAX_MTFACE];
      const int tangent_layer_num = CustomData_number_of_layers(loopdata_out, CD_TANGENT);
      for (int n = 0; n < tangent_layer_num; n++) {
        int index = CustomData_get_layer_index_n(loopdata_out, CD_TANGENT, n);
        BLI_assert(n < MAX_MTFACE);
        SGLSLMeshToTangent *mesh2tangent = &data_array[n];
        mesh2tangent->numTessFaces = (int)looptri_len;
#ifdef USE_LOOPTRI_DETECT_QUADS
        mesh2tangent->face_as_quad_map = face_as_quad_map;
        mesh2tangent->num_face_as_quad_map = num_face_as_quad_map;
#endif
        mesh2tangent->mvert = mvert;
        mesh2tangent->mpoly = mpoly;
        mesh2tangent->mloop = mloop;
        mesh2tangent->looptri = looptri;
        /* Note, we assume we do have tessellated loop normals at this point
         * (in case it is object-enabled), have to check this is valid. */
        mesh2tangent->precomputedLoopNormals = loop_normals;
        mesh2tangent->precomputedFaceNormals = poly_normals;

        mesh2tangent->orco = NULL;
        mesh2tangent->mloopuv = CustomData_get_layer_named(
            loopdata, CD_MLOOPUV, loopdata_out->layers[index].name);

        /* Fill the resulting tangent_mask */
        if (!mesh2tangent->mloopuv) {
          mesh2tangent->orco = vert_orco;
          if (!mesh2tangent->orco) {
            continue;
          }

          tangent_mask_curr |= DM_TANGENT_MASK_ORCO;
        }
        else {
          int uv_ind = CustomData_get_named_layer_index(
              loopdata, CD_MLOOPUV, loopdata_out->layers[index].name);
          int uv_start = CustomData_get_layer_index(loopdata, CD_MLOOPUV);
          BLI_assert(uv_ind != -1 && uv_start != -1);
          BLI_assert(uv_ind - uv_start < MAX_MTFACE);
          tangent_mask_curr |= (short)(1 << (uv_ind - uv_start));
        }

        mesh2tangent->tangent = loopdata_out->layers[index].data;
        BLI_task_pool_push(
            task_pool, DM_calc_loop_tangents_thread, mesh2tangent, false, TASK_PRIORITY_LOW);
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

    *tangent_mask_curr_p = tangent_mask_curr;

    /* Update active layer index */
    int act_uv_index = CustomData_get_layer_index_n(loopdata, CD_MLOOPUV, act_uv_n);
    if (act_uv_index != -1) {
      int tan_index = CustomData_get_named_layer_index(
          loopdata, CD_TANGENT, loopdata->layers[act_uv_index].name);
      CustomData_set_layer_active_index(loopdata, CD_TANGENT, tan_index);
    } /* else tangent has been built from orco */

    /* Update render layer index */
    int ren_uv_index = CustomData_get_layer_index_n(loopdata, CD_MLOOPUV, ren_uv_n);
    if (ren_uv_index != -1) {
      int tan_index = CustomData_get_named_layer_index(
          loopdata, CD_TANGENT, loopdata->layers[ren_uv_index].name);
      CustomData_set_layer_render_index(loopdata, CD_TANGENT, tan_index);
    } /* else tangent has been built from orco */
  }
}

void BKE_mesh_calc_loop_tangents(Mesh *me_eval,
                                 bool calc_active_tangent,
                                 const char (*tangent_names)[MAX_NAME],
                                 int tangent_names_len)
{
  BKE_mesh_runtime_looptri_ensure(me_eval);

  /* TODO(campbell): store in Mesh.runtime to avoid recalculation. */
  short tangent_mask = 0;
  BKE_mesh_calc_loop_tangent_ex(me_eval->mvert,
                                me_eval->mpoly,
                                (uint)me_eval->totpoly,
                                me_eval->mloop,
                                me_eval->runtime.looptris.array,
                                (uint)me_eval->runtime.looptris.len,
                                &me_eval->ldata,
                                calc_active_tangent,
                                tangent_names,
                                tangent_names_len,
                                CustomData_get_layer(&me_eval->pdata, CD_NORMAL),
                                CustomData_get_layer(&me_eval->ldata, CD_NORMAL),
                                CustomData_get_layer(&me_eval->vdata, CD_ORCO), /* may be NULL */
                                /* result */
                                &me_eval->ldata,
                                (uint)me_eval->totloop,
                                &tangent_mask);
}

/** \} */
