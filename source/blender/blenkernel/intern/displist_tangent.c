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

/** \name Tangent Space Calculation
 * \{ */

/* Necessary complexity to handle looptri's as quads for correct tangents */
#define USE_LOOPTRI_DETECT_QUADS

typedef struct {
  const DispList *dl;
  float (*tangent)[4]; /* destination */
} SGLSLDisplistToTangent;

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

void BKE_displist_tangent_calc(const DispList *dl, float (*fnormals)[3], float (**r_tangent)[4])
{
  UNUSED_VARS(fnormals);

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
#if 0
    int vert_len = dl->parts * dl->nr;
    if (*r_tangent == NULL) {
        *r_tangent = MEM_mallocN(sizeof(float[4]) * vert_len, "displist tangents");
    }
#endif
    /* TODO */
    BLI_assert(0);
  }
  else {
    /* Unsupported. */
    BLI_assert(0);
  }
}

/** \} */
