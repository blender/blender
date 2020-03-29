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

#include "BKE_displist.h"
#include "BKE_displist_tangent.h"

#include "MEM_guardedalloc.h"

/* interface */
#include "mikktspace.h"

typedef struct {
  const DispList *dl;
  float (*tangent)[4]; /* destination */
  /** Face normal for flat shading. */
  float (*fnormals)[3];
  /** Use by surfaces. Size of the surface in faces. */
  int u_len, v_len;
} SGLSLDisplistToTangent;

/** \} */

/* ---------------------------------------------------------------------- */
/** \name DL_INDEX3 tangents
 * \{ */

static int dl3_ts_GetNumFaces(const SMikkTSpaceContext *pContext)
{
  SGLSLDisplistToTangent *dlt = pContext->m_pUserData;

  return dlt->dl->parts;
}

static int dl3_ts_GetNumVertsOfFace(const SMikkTSpaceContext *pContext, const int face_num)
{
  UNUSED_VARS(pContext, face_num);

  return 3;
}

static void dl3_ts_GetPosition(const SMikkTSpaceContext *pContext,
                               float r_co[3],
                               const int face_num,
                               const int vert_index)
{
  SGLSLDisplistToTangent *dlt = pContext->m_pUserData;
  const float(*verts)[3] = (float(*)[3])dlt->dl->verts;
  const int(*idx)[3] = (int(*)[3])dlt->dl->index;

  copy_v3_v3(r_co, verts[idx[face_num][vert_index]]);
}

static void dl3_ts_GetTextureCoordinate(const SMikkTSpaceContext *pContext,
                                        float r_uv[2],
                                        const int face_num,
                                        const int vert_index)
{
  SGLSLDisplistToTangent *dlt = pContext->m_pUserData;
  const int(*idx)[3] = (int(*)[3])dlt->dl->index;

  r_uv[0] = idx[face_num][vert_index] / (float)(dlt->dl->nr - 1);
  r_uv[1] = 0.0f;
}

static void dl3_ts_GetNormal(const SMikkTSpaceContext *pContext,
                             float r_no[3],
                             const int face_num,
                             const int vert_index)
{
  SGLSLDisplistToTangent *dlt = pContext->m_pUserData;
  UNUSED_VARS(face_num, vert_index);

  copy_v3_v3(r_no, dlt->dl->nors);
}

static void dl3_ts_SetTSpace(const SMikkTSpaceContext *pContext,
                             const float fvTangent[3],
                             const float fSign,
                             const int face_num,
                             const int vert_index)
{
  SGLSLDisplistToTangent *dlt = pContext->m_pUserData;
  UNUSED_VARS(face_num, vert_index);

  copy_v3_v3(dlt->tangent[0], fvTangent);
  dlt->tangent[0][3] = fSign;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name DL_SURF tangents
 * \{ */

static int dlsurf_ts_GetNumFaces(const SMikkTSpaceContext *pContext)
{
  SGLSLDisplistToTangent *dlt = pContext->m_pUserData;

  return dlt->v_len * dlt->u_len;
}

static int dlsurf_ts_GetNumVertsOfFace(const SMikkTSpaceContext *pContext, const int face_num)
{
  UNUSED_VARS(pContext, face_num);

  return 4;
}

static int face_to_vert_index(SGLSLDisplistToTangent *dlt,
                              const int face_num,
                              const int vert_index)
{
  int u = face_num % dlt->u_len;
  int v = face_num / dlt->u_len;

  if (vert_index == 0) {
    u += 1;
  }
  else if (vert_index == 1) {
    u += 1;
    v += 1;
  }
  else if (vert_index == 2) {
    v += 1;
  }

  /* Cyclic correction. */
  u = u % dlt->dl->nr;
  v = v % dlt->dl->parts;

  return v * dlt->dl->nr + u;
}

static void dlsurf_ts_GetPosition(const SMikkTSpaceContext *pContext,
                                  float r_co[3],
                                  const int face_num,
                                  const int vert_index)
{
  SGLSLDisplistToTangent *dlt = pContext->m_pUserData;
  const float(*verts)[3] = (float(*)[3])dlt->dl->verts;

  copy_v3_v3(r_co, verts[face_to_vert_index(dlt, face_num, vert_index)]);
}

static void dlsurf_ts_GetTextureCoordinate(const SMikkTSpaceContext *pContext,
                                           float r_uv[2],
                                           const int face_num,
                                           const int vert_index)
{
  SGLSLDisplistToTangent *dlt = pContext->m_pUserData;

  int idx = face_to_vert_index(dlt, face_num, vert_index);

  /* Note: For some reason the shading U and V are swapped compared to the
   * one described in the surface format. */
  r_uv[0] = (idx / dlt->dl->nr) / (float)(dlt->v_len);
  r_uv[1] = (idx % dlt->dl->nr) / (float)(dlt->u_len);

  if (r_uv[0] == 0.0f && ELEM(vert_index, 1, 2)) {
    r_uv[0] = 1.0f;
  }
  if (r_uv[1] == 0.0f && ELEM(vert_index, 0, 1)) {
    r_uv[1] = 1.0f;
  }
}

static void dlsurf_ts_GetNormal(const SMikkTSpaceContext *pContext,
                                float r_no[3],
                                const int face_num,
                                const int vert_index)
{
  SGLSLDisplistToTangent *dlt = pContext->m_pUserData;
  const float(*nors)[3] = (float(*)[3])dlt->dl->nors;

  if (dlt->fnormals) {
    copy_v3_v3(r_no, dlt->fnormals[face_num]);
  }
  else {
    copy_v3_v3(r_no, nors[face_to_vert_index(dlt, face_num, vert_index)]);
  }
}

static void dlsurf_ts_SetTSpace(const SMikkTSpaceContext *pContext,
                                const float fvTangent[3],
                                const float fSign,
                                const int face_num,
                                const int vert_index)
{
  SGLSLDisplistToTangent *dlt = pContext->m_pUserData;
  UNUSED_VARS(face_num, vert_index);

  float *r_tan = dlt->tangent[face_num * 4 + vert_index];
  copy_v3_v3(r_tan, fvTangent);
  r_tan[3] = fSign;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Entry point
 * \{ */

void BKE_displist_tangent_calc(const DispList *dl, float (*fnormals)[3], float (**r_tangent)[4])
{
  if (dl->type == DL_INDEX3) {
    /* INDEX3 have only one tangent so we don't need actual allocation. */
    BLI_assert(*r_tangent != NULL);

    SGLSLDisplistToTangent mesh2tangent = {
        .tangent = *r_tangent,
        .dl = dl,
    };
    SMikkTSpaceContext sContext = {NULL};
    SMikkTSpaceInterface sInterface = {NULL};
    sContext.m_pUserData = &mesh2tangent;
    sContext.m_pInterface = &sInterface;
    sInterface.m_getNumFaces = dl3_ts_GetNumFaces;
    sInterface.m_getNumVerticesOfFace = dl3_ts_GetNumVertsOfFace;
    sInterface.m_getPosition = dl3_ts_GetPosition;
    sInterface.m_getTexCoord = dl3_ts_GetTextureCoordinate;
    sInterface.m_getNormal = dl3_ts_GetNormal;
    sInterface.m_setTSpaceBasic = dl3_ts_SetTSpace;
    /* 0 if failed */
    genTangSpaceDefault(&sContext);
  }
  else if (dl->type == DL_SURF) {
    SGLSLDisplistToTangent mesh2tangent = {
        .dl = dl,
        .u_len = dl->nr - ((dl->flag & DL_CYCL_U) ? 0 : 1),
        .v_len = dl->parts - ((dl->flag & DL_CYCL_V) ? 0 : 1),
        .fnormals = fnormals,
    };

    int loop_len = mesh2tangent.u_len * mesh2tangent.v_len * 4;

    if (*r_tangent == NULL) {
      *r_tangent = MEM_mallocN(sizeof(float[4]) * loop_len, "displist tangents");
    }
    mesh2tangent.tangent = *r_tangent;
    SMikkTSpaceContext sContext = {NULL};
    SMikkTSpaceInterface sInterface = {NULL};
    sContext.m_pUserData = &mesh2tangent;
    sContext.m_pInterface = &sInterface;
    sInterface.m_getNumFaces = dlsurf_ts_GetNumFaces;
    sInterface.m_getNumVerticesOfFace = dlsurf_ts_GetNumVertsOfFace;
    sInterface.m_getPosition = dlsurf_ts_GetPosition;
    sInterface.m_getTexCoord = dlsurf_ts_GetTextureCoordinate;
    sInterface.m_getNormal = dlsurf_ts_GetNormal;
    sInterface.m_setTSpaceBasic = dlsurf_ts_SetTSpace;
    /* 0 if failed */
    genTangSpaceDefault(&sContext);
  }
  else {
    /* Unsupported. */
    BLI_assert(0);
  }
}

/** \} */
