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

#include "MEM_guardedalloc.h"
#include "BLI_sys_types.h"  // for intptr_t support

#include "BLI_utildefines.h" /* for BLI_assert */
#include "BLI_math.h"
#include "BLI_task.h"

#include "CCGSubSurf.h"
#include "CCGSubSurf_intern.h"

#define FACE_calcIFNo(f, lvl, S, x, y, no) \
  _face_calcIFNo(f, lvl, S, x, y, no, subdivLevels, vertDataSize)

/* TODO(sergey): Deduplicate the following functions/ */
static void *_edge_getCoVert(CCGEdge *e, CCGVert *v, int lvl, int x, int dataSize)
{
  int levelBase = ccg_edgebase(lvl);
  if (v == e->v0) {
    return &EDGE_getLevelData(e)[dataSize * (levelBase + x)];
  }
  else {
    return &EDGE_getLevelData(e)[dataSize * (levelBase + (1 << lvl) - x)];
  }
}
/* *************************************************** */

static int _edge_isBoundary(const CCGEdge *e)
{
  return e->numFaces < 2;
}

static int _vert_isBoundary(const CCGVert *v)
{
  int i;
  for (i = 0; i < v->numEdges; i++) {
    if (_edge_isBoundary(v->edges[i])) {
      return 1;
    }
  }
  return 0;
}

static CCGVert *_edge_getOtherVert(CCGEdge *e, CCGVert *vQ)
{
  if (vQ == e->v0) {
    return e->v1;
  }
  else {
    return e->v0;
  }
}

static float *_face_getIFNoEdge(CCGFace *f,
                                CCGEdge *e,
                                int f_ed_idx,
                                int lvl,
                                int eX,
                                int eY,
                                int levels,
                                int dataSize,
                                int normalDataOffset)
{
  return (float *)((byte *)ccg_face_getIFCoEdge(f, e, f_ed_idx, lvl, eX, eY, levels, dataSize) +
                   normalDataOffset);
}

static void _face_calcIFNo(
    CCGFace *f, int lvl, int S, int x, int y, float no[3], int levels, int dataSize)
{
  float *a = ccg_face_getIFCo(f, lvl, S, x + 0, y + 0, levels, dataSize);
  float *b = ccg_face_getIFCo(f, lvl, S, x + 1, y + 0, levels, dataSize);
  float *c = ccg_face_getIFCo(f, lvl, S, x + 1, y + 1, levels, dataSize);
  float *d = ccg_face_getIFCo(f, lvl, S, x + 0, y + 1, levels, dataSize);
  float a_cX = c[0] - a[0], a_cY = c[1] - a[1], a_cZ = c[2] - a[2];
  float b_dX = d[0] - b[0], b_dY = d[1] - b[1], b_dZ = d[2] - b[2];

  no[0] = b_dY * a_cZ - b_dZ * a_cY;
  no[1] = b_dZ * a_cX - b_dX * a_cZ;
  no[2] = b_dX * a_cY - b_dY * a_cX;

  Normalize(no);
}

static int VERT_seam(const CCGVert *v)
{
  return ((v->flags & Vert_eSeam) != 0);
}

static float EDGE_getSharpness(CCGEdge *e, int lvl)
{
  if (!lvl) {
    return e->crease;
  }
  else if (!e->crease) {
    return 0.0f;
  }
  else if (e->crease - lvl < 0.0f) {
    return 0.0f;
  }
  else {
    return e->crease - lvl;
  }
}

typedef struct CCGSubSurfCalcSubdivData {
  CCGSubSurf *ss;
  CCGVert **effectedV;
  CCGEdge **effectedE;
  CCGFace **effectedF;
  int numEffectedV;
  int numEffectedE;
  int numEffectedF;

  int curLvl;
} CCGSubSurfCalcSubdivData;

static void ccgSubSurf__calcVertNormals_faces_accumulate_cb(
    void *__restrict userdata, const int ptrIdx, const TaskParallelTLS *__restrict UNUSED(tls))
{
  CCGSubSurfCalcSubdivData *data = userdata;

  CCGSubSurf *ss = data->ss;
  CCGFace *f = data->effectedF[ptrIdx];

  const int subdivLevels = ss->subdivLevels;
  const int lvl = ss->subdivLevels;
  const int gridSize = ccg_gridsize(lvl);
  const int normalDataOffset = ss->normalDataOffset;
  const int vertDataSize = ss->meshIFC.vertDataSize;

  int S, x, y;
  float no[3];

  for (S = 0; S < f->numVerts; S++) {
    for (y = 0; y < gridSize - 1; y++) {
      for (x = 0; x < gridSize - 1; x++) {
        NormZero(FACE_getIFNo(f, lvl, S, x, y));
      }
    }

    if (FACE_getEdges(f)[(S - 1 + f->numVerts) % f->numVerts]->flags & Edge_eEffected) {
      for (x = 0; x < gridSize - 1; x++) {
        NormZero(FACE_getIFNo(f, lvl, S, x, gridSize - 1));
      }
    }
    if (FACE_getEdges(f)[S]->flags & Edge_eEffected) {
      for (y = 0; y < gridSize - 1; y++) {
        NormZero(FACE_getIFNo(f, lvl, S, gridSize - 1, y));
      }
    }
    if (FACE_getVerts(f)[S]->flags & Vert_eEffected) {
      NormZero(FACE_getIFNo(f, lvl, S, gridSize - 1, gridSize - 1));
    }
  }

  for (S = 0; S < f->numVerts; S++) {
    int yLimit = !(FACE_getEdges(f)[(S - 1 + f->numVerts) % f->numVerts]->flags & Edge_eEffected);
    int xLimit = !(FACE_getEdges(f)[S]->flags & Edge_eEffected);
    int yLimitNext = xLimit;
    int xLimitPrev = yLimit;

    for (y = 0; y < gridSize - 1; y++) {
      for (x = 0; x < gridSize - 1; x++) {
        int xPlusOk = (!xLimit || x < gridSize - 2);
        int yPlusOk = (!yLimit || y < gridSize - 2);

        FACE_calcIFNo(f, lvl, S, x, y, no);

        NormAdd(FACE_getIFNo(f, lvl, S, x + 0, y + 0), no);
        if (xPlusOk) {
          NormAdd(FACE_getIFNo(f, lvl, S, x + 1, y + 0), no);
        }
        if (yPlusOk) {
          NormAdd(FACE_getIFNo(f, lvl, S, x + 0, y + 1), no);
        }
        if (xPlusOk && yPlusOk) {
          if (x < gridSize - 2 || y < gridSize - 2 ||
              FACE_getVerts(f)[S]->flags & Vert_eEffected) {
            NormAdd(FACE_getIFNo(f, lvl, S, x + 1, y + 1), no);
          }
        }

        if (x == 0 && y == 0) {
          int K;

          if (!yLimitNext || 1 < gridSize - 1) {
            NormAdd(FACE_getIFNo(f, lvl, (S + 1) % f->numVerts, 0, 1), no);
          }
          if (!xLimitPrev || 1 < gridSize - 1) {
            NormAdd(FACE_getIFNo(f, lvl, (S - 1 + f->numVerts) % f->numVerts, 1, 0), no);
          }

          for (K = 0; K < f->numVerts; K++) {
            if (K != S) {
              NormAdd(FACE_getIFNo(f, lvl, K, 0, 0), no);
            }
          }
        }
        else if (y == 0) {
          NormAdd(FACE_getIFNo(f, lvl, (S + 1) % f->numVerts, 0, x), no);
          if (!yLimitNext || x < gridSize - 2) {
            NormAdd(FACE_getIFNo(f, lvl, (S + 1) % f->numVerts, 0, x + 1), no);
          }
        }
        else if (x == 0) {
          NormAdd(FACE_getIFNo(f, lvl, (S - 1 + f->numVerts) % f->numVerts, y, 0), no);
          if (!xLimitPrev || y < gridSize - 2) {
            NormAdd(FACE_getIFNo(f, lvl, (S - 1 + f->numVerts) % f->numVerts, y + 1, 0), no);
          }
        }
      }
    }
  }
}

static void ccgSubSurf__calcVertNormals_faces_finalize_cb(
    void *__restrict userdata, const int ptrIdx, const TaskParallelTLS *__restrict UNUSED(tls))
{
  CCGSubSurfCalcSubdivData *data = userdata;

  CCGSubSurf *ss = data->ss;
  CCGFace *f = data->effectedF[ptrIdx];

  const int subdivLevels = ss->subdivLevels;
  const int lvl = ss->subdivLevels;
  const int gridSize = ccg_gridsize(lvl);
  const int normalDataOffset = ss->normalDataOffset;
  const int vertDataSize = ss->meshIFC.vertDataSize;

  int S, x, y;

  for (S = 0; S < f->numVerts; S++) {
    NormCopy(FACE_getIFNo(f, lvl, (S + 1) % f->numVerts, 0, gridSize - 1),
             FACE_getIFNo(f, lvl, S, gridSize - 1, 0));
  }

  for (S = 0; S < f->numVerts; S++) {
    for (y = 0; y < gridSize; y++) {
      for (x = 0; x < gridSize; x++) {
        float *no = FACE_getIFNo(f, lvl, S, x, y);
        Normalize(no);
      }
    }

    VertDataCopy((float *)((byte *)FACE_getCenterData(f) + normalDataOffset),
                 FACE_getIFNo(f, lvl, S, 0, 0),
                 ss);

    for (x = 1; x < gridSize - 1; x++) {
      NormCopy(FACE_getIENo(f, lvl, S, x), FACE_getIFNo(f, lvl, S, x, 0));
    }
  }
}

static void ccgSubSurf__calcVertNormals_edges_accumulate_cb(
    void *__restrict userdata, const int ptrIdx, const TaskParallelTLS *__restrict UNUSED(tls))
{
  CCGSubSurfCalcSubdivData *data = userdata;

  CCGSubSurf *ss = data->ss;
  CCGEdge *e = data->effectedE[ptrIdx];

  const int subdivLevels = ss->subdivLevels;
  const int lvl = ss->subdivLevels;
  const int edgeSize = ccg_edgesize(lvl);
  const int normalDataOffset = ss->normalDataOffset;
  const int vertDataSize = ss->meshIFC.vertDataSize;

  if (e->numFaces) {
    CCGFace *fLast = e->faces[e->numFaces - 1];
    int x, i;

    for (i = 0; i < e->numFaces - 1; i++) {
      CCGFace *f = e->faces[i];
      const int f_ed_idx = ccg_face_getEdgeIndex(f, e);
      const int f_ed_idx_last = ccg_face_getEdgeIndex(fLast, e);

      for (x = 1; x < edgeSize - 1; x++) {
        NormAdd(
            _face_getIFNoEdge(
                fLast, e, f_ed_idx_last, lvl, x, 0, subdivLevels, vertDataSize, normalDataOffset),
            _face_getIFNoEdge(
                f, e, f_ed_idx, lvl, x, 0, subdivLevels, vertDataSize, normalDataOffset));
      }
    }

    for (i = 0; i < e->numFaces - 1; i++) {
      CCGFace *f = e->faces[i];
      const int f_ed_idx = ccg_face_getEdgeIndex(f, e);
      const int f_ed_idx_last = ccg_face_getEdgeIndex(fLast, e);

      for (x = 1; x < edgeSize - 1; x++) {
        NormCopy(
            _face_getIFNoEdge(
                f, e, f_ed_idx, lvl, x, 0, subdivLevels, vertDataSize, normalDataOffset),
            _face_getIFNoEdge(
                fLast, e, f_ed_idx_last, lvl, x, 0, subdivLevels, vertDataSize, normalDataOffset));
      }
    }
  }
}

static void ccgSubSurf__calcVertNormals(CCGSubSurf *ss,
                                        CCGVert **effectedV,
                                        CCGEdge **effectedE,
                                        CCGFace **effectedF,
                                        int numEffectedV,
                                        int numEffectedE,
                                        int numEffectedF)
{
  int i, ptrIdx;
  const int subdivLevels = ss->subdivLevels;
  const int lvl = ss->subdivLevels;
  const int edgeSize = ccg_edgesize(lvl);
  const int gridSize = ccg_gridsize(lvl);
  const int normalDataOffset = ss->normalDataOffset;
  const int vertDataSize = ss->meshIFC.vertDataSize;

  CCGSubSurfCalcSubdivData data = {
      .ss = ss,
      .effectedV = effectedV,
      .effectedE = effectedE,
      .effectedF = effectedF,
      .numEffectedV = numEffectedV,
      .numEffectedE = numEffectedE,
      .numEffectedF = numEffectedF,
  };

  {
    TaskParallelSettings settings;
    BLI_parallel_range_settings_defaults(&settings);
    settings.min_iter_per_thread = CCG_TASK_LIMIT;
    BLI_task_parallel_range(
        0, numEffectedF, &data, ccgSubSurf__calcVertNormals_faces_accumulate_cb, &settings);
  }

  /* XXX can I reduce the number of normalisations here? */
  for (ptrIdx = 0; ptrIdx < numEffectedV; ptrIdx++) {
    CCGVert *v = (CCGVert *)effectedV[ptrIdx];
    float *no = VERT_getNo(v, lvl);

    NormZero(no);

    for (i = 0; i < v->numFaces; i++) {
      CCGFace *f = v->faces[i];
      NormAdd(no, FACE_getIFNo(f, lvl, ccg_face_getVertIndex(f, v), gridSize - 1, gridSize - 1));
    }

    if (UNLIKELY(v->numFaces == 0)) {
      NormCopy(no, VERT_getCo(v, lvl));
    }

    Normalize(no);

    for (i = 0; i < v->numFaces; i++) {
      CCGFace *f = v->faces[i];
      NormCopy(FACE_getIFNo(f, lvl, ccg_face_getVertIndex(f, v), gridSize - 1, gridSize - 1), no);
    }
  }

  {
    TaskParallelSettings settings;
    BLI_parallel_range_settings_defaults(&settings);
    settings.min_iter_per_thread = CCG_TASK_LIMIT;
    BLI_task_parallel_range(
        0, numEffectedE, &data, ccgSubSurf__calcVertNormals_edges_accumulate_cb, &settings);
  }

  {
    TaskParallelSettings settings;
    BLI_parallel_range_settings_defaults(&settings);
    settings.min_iter_per_thread = CCG_TASK_LIMIT;
    BLI_task_parallel_range(
        0, numEffectedF, &data, ccgSubSurf__calcVertNormals_faces_finalize_cb, &settings);
  }

  for (ptrIdx = 0; ptrIdx < numEffectedE; ptrIdx++) {
    CCGEdge *e = (CCGEdge *)effectedE[ptrIdx];

    if (e->numFaces) {
      CCGFace *f = e->faces[0];
      int x;
      const int f_ed_idx = ccg_face_getEdgeIndex(f, e);

      for (x = 0; x < edgeSize; x++) {
        NormCopy(EDGE_getNo(e, lvl, x),
                 _face_getIFNoEdge(
                     f, e, f_ed_idx, lvl, x, 0, subdivLevels, vertDataSize, normalDataOffset));
      }
    }
    else {
      /* set to zero here otherwise the normals are uninitialized memory
       * render: tests/animation/knight.blend with valgrind.
       * we could be more clever and interpolate vertex normals but these are
       * most likely not used so just zero out. */
      int x;

      for (x = 0; x < edgeSize; x++) {
        float *no = EDGE_getNo(e, lvl, x);
        NormCopy(no, EDGE_getCo(e, lvl, x));
        Normalize(no);
      }
    }
  }
}

static void ccgSubSurf__calcSubdivLevel_interior_faces_edges_midpoints_cb(
    void *__restrict userdata, const int ptrIdx, const TaskParallelTLS *__restrict UNUSED(tls))
{
  CCGSubSurfCalcSubdivData *data = userdata;

  CCGSubSurf *ss = data->ss;
  CCGFace *f = data->effectedF[ptrIdx];

  const int subdivLevels = ss->subdivLevels;
  const int curLvl = data->curLvl;
  const int nextLvl = curLvl + 1;
  const int gridSize = ccg_gridsize(curLvl);
  const int vertDataSize = ss->meshIFC.vertDataSize;

  int S, x, y;

  /* interior face midpoints
   * - old interior face points
   */
  for (S = 0; S < f->numVerts; S++) {
    for (y = 0; y < gridSize - 1; y++) {
      for (x = 0; x < gridSize - 1; x++) {
        int fx = 1 + 2 * x;
        int fy = 1 + 2 * y;
        const float *co0 = FACE_getIFCo(f, curLvl, S, x + 0, y + 0);
        const float *co1 = FACE_getIFCo(f, curLvl, S, x + 1, y + 0);
        const float *co2 = FACE_getIFCo(f, curLvl, S, x + 1, y + 1);
        const float *co3 = FACE_getIFCo(f, curLvl, S, x + 0, y + 1);
        float *co = FACE_getIFCo(f, nextLvl, S, fx, fy);

        VertDataAvg4(co, co0, co1, co2, co3, ss);
      }
    }
  }

  /* interior edge midpoints
   * - old interior edge points
   * - new interior face midpoints
   */
  for (S = 0; S < f->numVerts; S++) {
    for (x = 0; x < gridSize - 1; x++) {
      int fx = x * 2 + 1;
      const float *co0 = FACE_getIECo(f, curLvl, S, x + 0);
      const float *co1 = FACE_getIECo(f, curLvl, S, x + 1);
      const float *co2 = FACE_getIFCo(f, nextLvl, (S + 1) % f->numVerts, 1, fx);
      const float *co3 = FACE_getIFCo(f, nextLvl, S, fx, 1);
      float *co = FACE_getIECo(f, nextLvl, S, fx);

      VertDataAvg4(co, co0, co1, co2, co3, ss);
    }

    /* interior face interior edge midpoints
     * - old interior face points
     * - new interior face midpoints
     */

    /* vertical */
    for (x = 1; x < gridSize - 1; x++) {
      for (y = 0; y < gridSize - 1; y++) {
        int fx = x * 2;
        int fy = y * 2 + 1;
        const float *co0 = FACE_getIFCo(f, curLvl, S, x, y + 0);
        const float *co1 = FACE_getIFCo(f, curLvl, S, x, y + 1);
        const float *co2 = FACE_getIFCo(f, nextLvl, S, fx - 1, fy);
        const float *co3 = FACE_getIFCo(f, nextLvl, S, fx + 1, fy);
        float *co = FACE_getIFCo(f, nextLvl, S, fx, fy);

        VertDataAvg4(co, co0, co1, co2, co3, ss);
      }
    }

    /* horizontal */
    for (y = 1; y < gridSize - 1; y++) {
      for (x = 0; x < gridSize - 1; x++) {
        int fx = x * 2 + 1;
        int fy = y * 2;
        const float *co0 = FACE_getIFCo(f, curLvl, S, x + 0, y);
        const float *co1 = FACE_getIFCo(f, curLvl, S, x + 1, y);
        const float *co2 = FACE_getIFCo(f, nextLvl, S, fx, fy - 1);
        const float *co3 = FACE_getIFCo(f, nextLvl, S, fx, fy + 1);
        float *co = FACE_getIFCo(f, nextLvl, S, fx, fy);

        VertDataAvg4(co, co0, co1, co2, co3, ss);
      }
    }
  }
}

static void ccgSubSurf__calcSubdivLevel_interior_faces_edges_centerpoints_shift_cb(
    void *__restrict userdata, const int ptrIdx, const TaskParallelTLS *__restrict UNUSED(tls))
{
  CCGSubSurfCalcSubdivData *data = userdata;

  CCGSubSurf *ss = data->ss;
  CCGFace *f = data->effectedF[ptrIdx];

  const int subdivLevels = ss->subdivLevels;
  const int curLvl = data->curLvl;
  const int nextLvl = curLvl + 1;
  const int gridSize = ccg_gridsize(curLvl);
  const int vertDataSize = ss->meshIFC.vertDataSize;

  float *q_thread = alloca(vertDataSize);
  float *r_thread = alloca(vertDataSize);

  int S, x, y;

  /* interior center point shift
   * - old face center point (shifting)
   * - old interior edge points
   * - new interior face midpoints
   */
  VertDataZero(q_thread, ss);
  for (S = 0; S < f->numVerts; S++) {
    VertDataAdd(q_thread, FACE_getIFCo(f, nextLvl, S, 1, 1), ss);
  }
  VertDataMulN(q_thread, 1.0f / f->numVerts, ss);
  VertDataZero(r_thread, ss);
  for (S = 0; S < f->numVerts; S++) {
    VertDataAdd(r_thread, FACE_getIECo(f, curLvl, S, 1), ss);
  }
  VertDataMulN(r_thread, 1.0f / f->numVerts, ss);

  VertDataMulN((float *)FACE_getCenterData(f), f->numVerts - 2.0f, ss);
  VertDataAdd((float *)FACE_getCenterData(f), q_thread, ss);
  VertDataAdd((float *)FACE_getCenterData(f), r_thread, ss);
  VertDataMulN((float *)FACE_getCenterData(f), 1.0f / f->numVerts, ss);

  for (S = 0; S < f->numVerts; S++) {
    /* interior face shift
     * - old interior face point (shifting)
     * - new interior edge midpoints
     * - new interior face midpoints
     */
    for (x = 1; x < gridSize - 1; x++) {
      for (y = 1; y < gridSize - 1; y++) {
        int fx = x * 2;
        int fy = y * 2;
        const float *co = FACE_getIFCo(f, curLvl, S, x, y);
        float *nCo = FACE_getIFCo(f, nextLvl, S, fx, fy);

        VertDataAvg4(q_thread,
                     FACE_getIFCo(f, nextLvl, S, fx - 1, fy - 1),
                     FACE_getIFCo(f, nextLvl, S, fx + 1, fy - 1),
                     FACE_getIFCo(f, nextLvl, S, fx + 1, fy + 1),
                     FACE_getIFCo(f, nextLvl, S, fx - 1, fy + 1),
                     ss);

        VertDataAvg4(r_thread,
                     FACE_getIFCo(f, nextLvl, S, fx - 1, fy + 0),
                     FACE_getIFCo(f, nextLvl, S, fx + 1, fy + 0),
                     FACE_getIFCo(f, nextLvl, S, fx + 0, fy - 1),
                     FACE_getIFCo(f, nextLvl, S, fx + 0, fy + 1),
                     ss);

        VertDataCopy(nCo, co, ss);
        VertDataSub(nCo, q_thread, ss);
        VertDataMulN(nCo, 0.25f, ss);
        VertDataAdd(nCo, r_thread, ss);
      }
    }

    /* interior edge interior shift
     * - old interior edge point (shifting)
     * - new interior edge midpoints
     * - new interior face midpoints
     */
    for (x = 1; x < gridSize - 1; x++) {
      int fx = x * 2;
      const float *co = FACE_getIECo(f, curLvl, S, x);
      float *nCo = FACE_getIECo(f, nextLvl, S, fx);

      VertDataAvg4(q_thread,
                   FACE_getIFCo(f, nextLvl, (S + 1) % f->numVerts, 1, fx - 1),
                   FACE_getIFCo(f, nextLvl, (S + 1) % f->numVerts, 1, fx + 1),
                   FACE_getIFCo(f, nextLvl, S, fx + 1, +1),
                   FACE_getIFCo(f, nextLvl, S, fx - 1, +1),
                   ss);

      VertDataAvg4(r_thread,
                   FACE_getIECo(f, nextLvl, S, fx - 1),
                   FACE_getIECo(f, nextLvl, S, fx + 1),
                   FACE_getIFCo(f, nextLvl, (S + 1) % f->numVerts, 1, fx),
                   FACE_getIFCo(f, nextLvl, S, fx, 1),
                   ss);

      VertDataCopy(nCo, co, ss);
      VertDataSub(nCo, q_thread, ss);
      VertDataMulN(nCo, 0.25f, ss);
      VertDataAdd(nCo, r_thread, ss);
    }
  }
}

static void ccgSubSurf__calcSubdivLevel_verts_copydata_cb(
    void *__restrict userdata, const int ptrIdx, const TaskParallelTLS *__restrict UNUSED(tls))
{
  CCGSubSurfCalcSubdivData *data = userdata;

  CCGSubSurf *ss = data->ss;
  CCGFace *f = data->effectedF[ptrIdx];

  const int subdivLevels = ss->subdivLevels;
  const int nextLvl = data->curLvl + 1;
  const int gridSize = ccg_gridsize(nextLvl);
  const int cornerIdx = gridSize - 1;
  const int vertDataSize = ss->meshIFC.vertDataSize;

  int S, x;

  for (S = 0; S < f->numVerts; S++) {
    CCGEdge *e = FACE_getEdges(f)[S];
    CCGEdge *prevE = FACE_getEdges(f)[(S + f->numVerts - 1) % f->numVerts];

    VertDataCopy(FACE_getIFCo(f, nextLvl, S, 0, 0), (float *)FACE_getCenterData(f), ss);
    VertDataCopy(FACE_getIECo(f, nextLvl, S, 0), (float *)FACE_getCenterData(f), ss);
    VertDataCopy(FACE_getIFCo(f, nextLvl, S, cornerIdx, cornerIdx),
                 VERT_getCo(FACE_getVerts(f)[S], nextLvl),
                 ss);
    VertDataCopy(FACE_getIECo(f, nextLvl, S, cornerIdx),
                 EDGE_getCo(FACE_getEdges(f)[S], nextLvl, cornerIdx),
                 ss);
    for (x = 1; x < gridSize - 1; x++) {
      float *co = FACE_getIECo(f, nextLvl, S, x);
      VertDataCopy(FACE_getIFCo(f, nextLvl, S, x, 0), co, ss);
      VertDataCopy(FACE_getIFCo(f, nextLvl, (S + 1) % f->numVerts, 0, x), co, ss);
    }
    for (x = 0; x < gridSize - 1; x++) {
      int eI = gridSize - 1 - x;
      VertDataCopy(FACE_getIFCo(f, nextLvl, S, cornerIdx, x),
                   _edge_getCoVert(e, FACE_getVerts(f)[S], nextLvl, eI, vertDataSize),
                   ss);
      VertDataCopy(FACE_getIFCo(f, nextLvl, S, x, cornerIdx),
                   _edge_getCoVert(prevE, FACE_getVerts(f)[S], nextLvl, eI, vertDataSize),
                   ss);
    }
  }
}

static void ccgSubSurf__calcSubdivLevel(CCGSubSurf *ss,
                                        CCGVert **effectedV,
                                        CCGEdge **effectedE,
                                        CCGFace **effectedF,
                                        const int numEffectedV,
                                        const int numEffectedE,
                                        const int numEffectedF,
                                        const int curLvl)
{
  const int subdivLevels = ss->subdivLevels;
  const int nextLvl = curLvl + 1;
  int edgeSize = ccg_edgesize(curLvl);
  int ptrIdx, i;
  const int vertDataSize = ss->meshIFC.vertDataSize;
  float *q = ss->q, *r = ss->r;

  CCGSubSurfCalcSubdivData data = {
      .ss = ss,
      .effectedV = effectedV,
      .effectedE = effectedE,
      .effectedF = effectedF,
      .numEffectedV = numEffectedV,
      .numEffectedE = numEffectedE,
      .numEffectedF = numEffectedF,
      .curLvl = curLvl,
  };

  {
    TaskParallelSettings settings;
    BLI_parallel_range_settings_defaults(&settings);
    settings.min_iter_per_thread = CCG_TASK_LIMIT;
    BLI_task_parallel_range(0,
                            numEffectedF,
                            &data,
                            ccgSubSurf__calcSubdivLevel_interior_faces_edges_midpoints_cb,
                            &settings);
  }

  /* exterior edge midpoints
   * - old exterior edge points
   * - new interior face midpoints
   */
  /* Not worth parallelizing. */
  for (ptrIdx = 0; ptrIdx < numEffectedE; ptrIdx++) {
    CCGEdge *e = (CCGEdge *)effectedE[ptrIdx];
    float sharpness = EDGE_getSharpness(e, curLvl);
    int x, j;

    if (_edge_isBoundary(e) || sharpness > 1.0f) {
      for (x = 0; x < edgeSize - 1; x++) {
        int fx = x * 2 + 1;
        const float *co0 = EDGE_getCo(e, curLvl, x + 0);
        const float *co1 = EDGE_getCo(e, curLvl, x + 1);
        float *co = EDGE_getCo(e, nextLvl, fx);

        VertDataCopy(co, co0, ss);
        VertDataAdd(co, co1, ss);
        VertDataMulN(co, 0.5f, ss);
      }
    }
    else {
      for (x = 0; x < edgeSize - 1; x++) {
        int fx = x * 2 + 1;
        const float *co0 = EDGE_getCo(e, curLvl, x + 0);
        const float *co1 = EDGE_getCo(e, curLvl, x + 1);
        float *co = EDGE_getCo(e, nextLvl, fx);
        int numFaces = 0;

        VertDataCopy(q, co0, ss);
        VertDataAdd(q, co1, ss);

        for (j = 0; j < e->numFaces; j++) {
          CCGFace *f = e->faces[j];
          const int f_ed_idx = ccg_face_getEdgeIndex(f, e);
          VertDataAdd(
              q,
              ccg_face_getIFCoEdge(f, e, f_ed_idx, nextLvl, fx, 1, subdivLevels, vertDataSize),
              ss);
          numFaces++;
        }

        VertDataMulN(q, 1.0f / (2.0f + numFaces), ss);

        VertDataCopy(r, co0, ss);
        VertDataAdd(r, co1, ss);
        VertDataMulN(r, 0.5f, ss);

        VertDataCopy(co, q, ss);
        VertDataSub(r, q, ss);
        VertDataMulN(r, sharpness, ss);
        VertDataAdd(co, r, ss);
      }
    }
  }

  /* exterior vertex shift
   * - old vertex points (shifting)
   * - old exterior edge points
   * - new interior face midpoints
   */
  /* Not worth parallelizing. */
  for (ptrIdx = 0; ptrIdx < numEffectedV; ptrIdx++) {
    CCGVert *v = (CCGVert *)effectedV[ptrIdx];
    const float *co = VERT_getCo(v, curLvl);
    float *nCo = VERT_getCo(v, nextLvl);
    int sharpCount = 0, allSharp = 1;
    float avgSharpness = 0.0;
    int j, seam = VERT_seam(v), seamEdges = 0;

    for (j = 0; j < v->numEdges; j++) {
      CCGEdge *e = v->edges[j];
      float sharpness = EDGE_getSharpness(e, curLvl);

      if (seam && _edge_isBoundary(e)) {
        seamEdges++;
      }

      if (sharpness != 0.0f) {
        sharpCount++;
        avgSharpness += sharpness;
      }
      else {
        allSharp = 0;
      }
    }

    if (sharpCount) {
      avgSharpness /= sharpCount;
      if (avgSharpness > 1.0f) {
        avgSharpness = 1.0f;
      }
    }

    if (seamEdges < 2 || seamEdges != v->numEdges) {
      seam = 0;
    }

    if (!v->numEdges || ss->meshIFC.simpleSubdiv) {
      VertDataCopy(nCo, co, ss);
    }
    else if (_vert_isBoundary(v)) {
      int numBoundary = 0;

      VertDataZero(r, ss);
      for (j = 0; j < v->numEdges; j++) {
        CCGEdge *e = v->edges[j];
        if (_edge_isBoundary(e)) {
          VertDataAdd(r, _edge_getCoVert(e, v, curLvl, 1, vertDataSize), ss);
          numBoundary++;
        }
      }

      VertDataCopy(nCo, co, ss);
      VertDataMulN(nCo, 0.75f, ss);
      VertDataMulN(r, 0.25f / numBoundary, ss);
      VertDataAdd(nCo, r, ss);
    }
    else {
      const int cornerIdx = (1 + (1 << (curLvl))) - 2;
      int numEdges = 0, numFaces = 0;

      VertDataZero(q, ss);
      for (j = 0; j < v->numFaces; j++) {
        CCGFace *f = v->faces[j];
        VertDataAdd(
            q, FACE_getIFCo(f, nextLvl, ccg_face_getVertIndex(f, v), cornerIdx, cornerIdx), ss);
        numFaces++;
      }
      VertDataMulN(q, 1.0f / numFaces, ss);
      VertDataZero(r, ss);
      for (j = 0; j < v->numEdges; j++) {
        CCGEdge *e = v->edges[j];
        VertDataAdd(r, _edge_getCoVert(e, v, curLvl, 1, vertDataSize), ss);
        numEdges++;
      }
      VertDataMulN(r, 1.0f / numEdges, ss);

      VertDataCopy(nCo, co, ss);
      VertDataMulN(nCo, numEdges - 2.0f, ss);
      VertDataAdd(nCo, q, ss);
      VertDataAdd(nCo, r, ss);
      VertDataMulN(nCo, 1.0f / numEdges, ss);
    }

    if ((sharpCount > 1 && v->numFaces) || seam) {
      VertDataZero(q, ss);

      if (seam) {
        avgSharpness = 1.0f;
        sharpCount = seamEdges;
        allSharp = 1;
      }

      for (j = 0; j < v->numEdges; j++) {
        CCGEdge *e = v->edges[j];
        float sharpness = EDGE_getSharpness(e, curLvl);

        if (seam) {
          if (_edge_isBoundary(e)) {
            VertDataAdd(q, _edge_getCoVert(e, v, curLvl, 1, vertDataSize), ss);
          }
        }
        else if (sharpness != 0.0f) {
          VertDataAdd(q, _edge_getCoVert(e, v, curLvl, 1, vertDataSize), ss);
        }
      }

      VertDataMulN(q, (float)1 / sharpCount, ss);

      if (sharpCount != 2 || allSharp) {
        /* q = q + (co - q) * avgSharpness */
        VertDataCopy(r, co, ss);
        VertDataSub(r, q, ss);
        VertDataMulN(r, avgSharpness, ss);
        VertDataAdd(q, r, ss);
      }

      /* r = co * 0.75 + q * 0.25 */
      VertDataCopy(r, co, ss);
      VertDataMulN(r, 0.75f, ss);
      VertDataMulN(q, 0.25f, ss);
      VertDataAdd(r, q, ss);

      /* nCo = nCo + (r - nCo) * avgSharpness */
      VertDataSub(r, nCo, ss);
      VertDataMulN(r, avgSharpness, ss);
      VertDataAdd(nCo, r, ss);
    }
  }

  /* exterior edge interior shift
   * - old exterior edge midpoints (shifting)
   * - old exterior edge midpoints
   * - new interior face midpoints
   */
  /* Not worth parallelizing. */
  for (ptrIdx = 0; ptrIdx < numEffectedE; ptrIdx++) {
    CCGEdge *e = (CCGEdge *)effectedE[ptrIdx];
    float sharpness = EDGE_getSharpness(e, curLvl);
    int sharpCount = 0;
    float avgSharpness = 0.0;
    int x, j;

    if (sharpness != 0.0f) {
      sharpCount = 2;
      avgSharpness += sharpness;

      if (avgSharpness > 1.0f) {
        avgSharpness = 1.0f;
      }
    }
    else {
      sharpCount = 0;
      avgSharpness = 0;
    }

    if (_edge_isBoundary(e)) {
      for (x = 1; x < edgeSize - 1; x++) {
        int fx = x * 2;
        const float *co = EDGE_getCo(e, curLvl, x);
        float *nCo = EDGE_getCo(e, nextLvl, fx);

        /* Average previous level's endpoints */
        VertDataCopy(r, EDGE_getCo(e, curLvl, x - 1), ss);
        VertDataAdd(r, EDGE_getCo(e, curLvl, x + 1), ss);
        VertDataMulN(r, 0.5f, ss);

        /* nCo = nCo * 0.75 + r * 0.25 */
        VertDataCopy(nCo, co, ss);
        VertDataMulN(nCo, 0.75f, ss);
        VertDataMulN(r, 0.25f, ss);
        VertDataAdd(nCo, r, ss);
      }
    }
    else {
      for (x = 1; x < edgeSize - 1; x++) {
        int fx = x * 2;
        const float *co = EDGE_getCo(e, curLvl, x);
        float *nCo = EDGE_getCo(e, nextLvl, fx);
        int numFaces = 0;

        VertDataZero(q, ss);
        VertDataZero(r, ss);
        VertDataAdd(r, EDGE_getCo(e, curLvl, x - 1), ss);
        VertDataAdd(r, EDGE_getCo(e, curLvl, x + 1), ss);
        for (j = 0; j < e->numFaces; j++) {
          CCGFace *f = e->faces[j];
          int f_ed_idx = ccg_face_getEdgeIndex(f, e);
          VertDataAdd(
              q,
              ccg_face_getIFCoEdge(f, e, f_ed_idx, nextLvl, fx - 1, 1, subdivLevels, vertDataSize),
              ss);
          VertDataAdd(
              q,
              ccg_face_getIFCoEdge(f, e, f_ed_idx, nextLvl, fx + 1, 1, subdivLevels, vertDataSize),
              ss);

          VertDataAdd(
              r,
              ccg_face_getIFCoEdge(f, e, f_ed_idx, curLvl, x, 1, subdivLevels, vertDataSize),
              ss);
          numFaces++;
        }
        VertDataMulN(q, 1.0f / (numFaces * 2.0f), ss);
        VertDataMulN(r, 1.0f / (2.0f + numFaces), ss);

        VertDataCopy(nCo, co, ss);
        VertDataMulN(nCo, (float)numFaces, ss);
        VertDataAdd(nCo, q, ss);
        VertDataAdd(nCo, r, ss);
        VertDataMulN(nCo, 1.0f / (2 + numFaces), ss);

        if (sharpCount == 2) {
          VertDataCopy(q, co, ss);
          VertDataMulN(q, 6.0f, ss);
          VertDataAdd(q, EDGE_getCo(e, curLvl, x - 1), ss);
          VertDataAdd(q, EDGE_getCo(e, curLvl, x + 1), ss);
          VertDataMulN(q, 1 / 8.0f, ss);

          VertDataSub(q, nCo, ss);
          VertDataMulN(q, avgSharpness, ss);
          VertDataAdd(nCo, q, ss);
        }
      }
    }
  }

  {
    TaskParallelSettings settings;
    BLI_parallel_range_settings_defaults(&settings);
    settings.min_iter_per_thread = CCG_TASK_LIMIT;
    BLI_task_parallel_range(0,
                            numEffectedF,
                            &data,
                            ccgSubSurf__calcSubdivLevel_interior_faces_edges_centerpoints_shift_cb,
                            &settings);
  }

  /* copy down */
  edgeSize = ccg_edgesize(nextLvl);

  /* Not worth parallelizing. */
  for (i = 0; i < numEffectedE; i++) {
    CCGEdge *e = effectedE[i];
    VertDataCopy(EDGE_getCo(e, nextLvl, 0), VERT_getCo(e->v0, nextLvl), ss);
    VertDataCopy(EDGE_getCo(e, nextLvl, edgeSize - 1), VERT_getCo(e->v1, nextLvl), ss);
  }

  {
    TaskParallelSettings settings;
    BLI_parallel_range_settings_defaults(&settings);
    settings.min_iter_per_thread = CCG_TASK_LIMIT;
    BLI_task_parallel_range(
        0, numEffectedF, &data, ccgSubSurf__calcSubdivLevel_verts_copydata_cb, &settings);
  }
}

void ccgSubSurf__sync_legacy(CCGSubSurf *ss)
{
  CCGVert **effectedV;
  CCGEdge **effectedE;
  CCGFace **effectedF;
  int numEffectedV, numEffectedE, numEffectedF;
  int subdivLevels = ss->subdivLevels;
  int vertDataSize = ss->meshIFC.vertDataSize;
  int i, j, ptrIdx, S;
  int curLvl, nextLvl;
  void *q = ss->q, *r = ss->r;

  effectedV = MEM_mallocN(sizeof(*effectedV) * ss->vMap->numEntries, "CCGSubsurf effectedV");
  effectedE = MEM_mallocN(sizeof(*effectedE) * ss->eMap->numEntries, "CCGSubsurf effectedE");
  effectedF = MEM_mallocN(sizeof(*effectedF) * ss->fMap->numEntries, "CCGSubsurf effectedF");
  numEffectedV = numEffectedE = numEffectedF = 0;
  for (i = 0; i < ss->vMap->curSize; i++) {
    CCGVert *v = (CCGVert *)ss->vMap->buckets[i];
    for (; v; v = v->next) {
      if (v->flags & Vert_eEffected) {
        effectedV[numEffectedV++] = v;

        for (j = 0; j < v->numEdges; j++) {
          CCGEdge *e = v->edges[j];
          if (!(e->flags & Edge_eEffected)) {
            effectedE[numEffectedE++] = e;
            e->flags |= Edge_eEffected;
          }
        }

        for (j = 0; j < v->numFaces; j++) {
          CCGFace *f = v->faces[j];
          if (!(f->flags & Face_eEffected)) {
            effectedF[numEffectedF++] = f;
            f->flags |= Face_eEffected;
          }
        }
      }
    }
  }

  curLvl = 0;
  nextLvl = curLvl + 1;

  for (ptrIdx = 0; ptrIdx < numEffectedF; ptrIdx++) {
    CCGFace *f = effectedF[ptrIdx];
    void *co = FACE_getCenterData(f);
    VertDataZero(co, ss);
    for (i = 0; i < f->numVerts; i++) {
      VertDataAdd(co, VERT_getCo(FACE_getVerts(f)[i], curLvl), ss);
    }
    VertDataMulN(co, 1.0f / f->numVerts, ss);

    f->flags = 0;
  }
  for (ptrIdx = 0; ptrIdx < numEffectedE; ptrIdx++) {
    CCGEdge *e = effectedE[ptrIdx];
    void *co = EDGE_getCo(e, nextLvl, 1);
    float sharpness = EDGE_getSharpness(e, curLvl);

    if (_edge_isBoundary(e) || sharpness >= 1.0f) {
      VertDataCopy(co, VERT_getCo(e->v0, curLvl), ss);
      VertDataAdd(co, VERT_getCo(e->v1, curLvl), ss);
      VertDataMulN(co, 0.5f, ss);
    }
    else {
      int numFaces = 0;
      VertDataCopy(q, VERT_getCo(e->v0, curLvl), ss);
      VertDataAdd(q, VERT_getCo(e->v1, curLvl), ss);
      for (i = 0; i < e->numFaces; i++) {
        CCGFace *f = e->faces[i];
        VertDataAdd(q, (float *)FACE_getCenterData(f), ss);
        numFaces++;
      }
      VertDataMulN(q, 1.0f / (2.0f + numFaces), ss);

      VertDataCopy(r, VERT_getCo(e->v0, curLvl), ss);
      VertDataAdd(r, VERT_getCo(e->v1, curLvl), ss);
      VertDataMulN(r, 0.5f, ss);

      VertDataCopy(co, q, ss);
      VertDataSub(r, q, ss);
      VertDataMulN(r, sharpness, ss);
      VertDataAdd(co, r, ss);
    }

    /* edge flags cleared later */
  }
  for (ptrIdx = 0; ptrIdx < numEffectedV; ptrIdx++) {
    CCGVert *v = effectedV[ptrIdx];
    void *co = VERT_getCo(v, curLvl);
    void *nCo = VERT_getCo(v, nextLvl);
    int sharpCount = 0, allSharp = 1;
    float avgSharpness = 0.0;
    int seam = VERT_seam(v), seamEdges = 0;

    for (i = 0; i < v->numEdges; i++) {
      CCGEdge *e = v->edges[i];
      float sharpness = EDGE_getSharpness(e, curLvl);

      if (seam && _edge_isBoundary(e)) {
        seamEdges++;
      }

      if (sharpness != 0.0f) {
        sharpCount++;
        avgSharpness += sharpness;
      }
      else {
        allSharp = 0;
      }
    }

    if (sharpCount) {
      avgSharpness /= sharpCount;
      if (avgSharpness > 1.0f) {
        avgSharpness = 1.0f;
      }
    }

    if (seamEdges < 2 || seamEdges != v->numEdges) {
      seam = 0;
    }

    if (!v->numEdges || ss->meshIFC.simpleSubdiv) {
      VertDataCopy(nCo, co, ss);
    }
    else if (_vert_isBoundary(v)) {
      int numBoundary = 0;

      VertDataZero(r, ss);
      for (i = 0; i < v->numEdges; i++) {
        CCGEdge *e = v->edges[i];
        if (_edge_isBoundary(e)) {
          VertDataAdd(r, VERT_getCo(_edge_getOtherVert(e, v), curLvl), ss);
          numBoundary++;
        }
      }
      VertDataCopy(nCo, co, ss);
      VertDataMulN(nCo, 0.75f, ss);
      VertDataMulN(r, 0.25f / numBoundary, ss);
      VertDataAdd(nCo, r, ss);
    }
    else {
      int numEdges = 0, numFaces = 0;

      VertDataZero(q, ss);
      for (i = 0; i < v->numFaces; i++) {
        CCGFace *f = v->faces[i];
        VertDataAdd(q, (float *)FACE_getCenterData(f), ss);
        numFaces++;
      }
      VertDataMulN(q, 1.0f / numFaces, ss);
      VertDataZero(r, ss);
      for (i = 0; i < v->numEdges; i++) {
        CCGEdge *e = v->edges[i];
        VertDataAdd(r, VERT_getCo(_edge_getOtherVert(e, v), curLvl), ss);
        numEdges++;
      }
      VertDataMulN(r, 1.0f / numEdges, ss);

      VertDataCopy(nCo, co, ss);
      VertDataMulN(nCo, numEdges - 2.0f, ss);
      VertDataAdd(nCo, q, ss);
      VertDataAdd(nCo, r, ss);
      VertDataMulN(nCo, 1.0f / numEdges, ss);
    }

    if (sharpCount > 1 || seam) {
      VertDataZero(q, ss);

      if (seam) {
        avgSharpness = 1.0f;
        sharpCount = seamEdges;
        allSharp = 1;
      }

      for (i = 0; i < v->numEdges; i++) {
        CCGEdge *e = v->edges[i];
        float sharpness = EDGE_getSharpness(e, curLvl);

        if (seam) {
          if (_edge_isBoundary(e)) {
            CCGVert *oV = _edge_getOtherVert(e, v);
            VertDataAdd(q, VERT_getCo(oV, curLvl), ss);
          }
        }
        else if (sharpness != 0.0f) {
          CCGVert *oV = _edge_getOtherVert(e, v);
          VertDataAdd(q, VERT_getCo(oV, curLvl), ss);
        }
      }

      VertDataMulN(q, (float)1 / sharpCount, ss);

      if (sharpCount != 2 || allSharp) {
        /* q = q + (co - q) * avgSharpness */
        VertDataCopy(r, co, ss);
        VertDataSub(r, q, ss);
        VertDataMulN(r, avgSharpness, ss);
        VertDataAdd(q, r, ss);
      }

      /* r = co * 0.75 + q * 0.25 */
      VertDataCopy(r, co, ss);
      VertDataMulN(r, 0.75f, ss);
      VertDataMulN(q, 0.25f, ss);
      VertDataAdd(r, q, ss);

      /* nCo = nCo + (r - nCo) * avgSharpness */
      VertDataSub(r, nCo, ss);
      VertDataMulN(r, avgSharpness, ss);
      VertDataAdd(nCo, r, ss);
    }

    /* vert flags cleared later */
  }

  if (ss->useAgeCounts) {
    for (i = 0; i < numEffectedV; i++) {
      CCGVert *v = effectedV[i];
      byte *userData = ccgSubSurf_getVertUserData(ss, v);
      *((int *)&userData[ss->vertUserAgeOffset]) = ss->currentAge;
    }

    for (i = 0; i < numEffectedE; i++) {
      CCGEdge *e = effectedE[i];
      byte *userData = ccgSubSurf_getEdgeUserData(ss, e);
      *((int *)&userData[ss->edgeUserAgeOffset]) = ss->currentAge;
    }

    for (i = 0; i < numEffectedF; i++) {
      CCGFace *f = effectedF[i];
      byte *userData = ccgSubSurf_getFaceUserData(ss, f);
      *((int *)&userData[ss->faceUserAgeOffset]) = ss->currentAge;
    }
  }

  for (i = 0; i < numEffectedE; i++) {
    CCGEdge *e = effectedE[i];
    VertDataCopy(EDGE_getCo(e, nextLvl, 0), VERT_getCo(e->v0, nextLvl), ss);
    VertDataCopy(EDGE_getCo(e, nextLvl, 2), VERT_getCo(e->v1, nextLvl), ss);
  }
  for (i = 0; i < numEffectedF; i++) {
    CCGFace *f = effectedF[i];
    for (S = 0; S < f->numVerts; S++) {
      CCGEdge *e = FACE_getEdges(f)[S];
      CCGEdge *prevE = FACE_getEdges(f)[(S + f->numVerts - 1) % f->numVerts];

      VertDataCopy(FACE_getIFCo(f, nextLvl, S, 0, 0), (float *)FACE_getCenterData(f), ss);
      VertDataCopy(FACE_getIECo(f, nextLvl, S, 0), (float *)FACE_getCenterData(f), ss);
      VertDataCopy(
          FACE_getIFCo(f, nextLvl, S, 1, 1), VERT_getCo(FACE_getVerts(f)[S], nextLvl), ss);
      VertDataCopy(
          FACE_getIECo(f, nextLvl, S, 1), EDGE_getCo(FACE_getEdges(f)[S], nextLvl, 1), ss);

      VertDataCopy(FACE_getIFCo(f, nextLvl, S, 1, 0),
                   _edge_getCoVert(e, FACE_getVerts(f)[S], nextLvl, 1, vertDataSize),
                   ss);
      VertDataCopy(FACE_getIFCo(f, nextLvl, S, 0, 1),
                   _edge_getCoVert(prevE, FACE_getVerts(f)[S], nextLvl, 1, vertDataSize),
                   ss);
    }
  }

  for (curLvl = 1; curLvl < subdivLevels; curLvl++) {
    ccgSubSurf__calcSubdivLevel(
        ss, effectedV, effectedE, effectedF, numEffectedV, numEffectedE, numEffectedF, curLvl);
  }

  if (ss->calcVertNormals) {
    ccgSubSurf__calcVertNormals(
        ss, effectedV, effectedE, effectedF, numEffectedV, numEffectedE, numEffectedF);
  }

  for (ptrIdx = 0; ptrIdx < numEffectedV; ptrIdx++) {
    CCGVert *v = effectedV[ptrIdx];
    v->flags = 0;
  }
  for (ptrIdx = 0; ptrIdx < numEffectedE; ptrIdx++) {
    CCGEdge *e = effectedE[ptrIdx];
    e->flags = 0;
  }

  MEM_freeN(effectedF);
  MEM_freeN(effectedE);
  MEM_freeN(effectedV);

#ifdef DUMP_RESULT_GRIDS
  ccgSubSurf__dumpCoords(ss);
#endif
}

/* ** Public API exposed to other areas which depends on old CCG code. ** */

/* Update normals for specified faces. */
CCGError ccgSubSurf_updateNormals(CCGSubSurf *ss, CCGFace **effectedF, int numEffectedF)
{
  CCGVert **effectedV;
  CCGEdge **effectedE;
  int i, numEffectedV, numEffectedE, freeF;

  ccgSubSurf__allFaces(ss, &effectedF, &numEffectedF, &freeF);
  ccgSubSurf__effectedFaceNeighbours(
      ss, effectedF, numEffectedF, &effectedV, &numEffectedV, &effectedE, &numEffectedE);

  if (ss->calcVertNormals) {
    ccgSubSurf__calcVertNormals(
        ss, effectedV, effectedE, effectedF, numEffectedV, numEffectedE, numEffectedF);
  }

  for (i = 0; i < numEffectedV; i++) {
    effectedV[i]->flags = 0;
  }
  for (i = 0; i < numEffectedE; i++) {
    effectedE[i]->flags = 0;
  }
  for (i = 0; i < numEffectedF; i++) {
    effectedF[i]->flags = 0;
  }

  MEM_freeN(effectedE);
  MEM_freeN(effectedV);
  if (freeF) {
    MEM_freeN(effectedF);
  }

  return eCCGError_None;
}

/* compute subdivision levels from a given starting point, used by
 * multires subdivide/propagate, by filling in coordinates at a
 * certain level, and then subdividing that up to the highest level */
CCGError ccgSubSurf_updateLevels(CCGSubSurf *ss, int lvl, CCGFace **effectedF, int numEffectedF)
{
  CCGVert **effectedV;
  CCGEdge **effectedE;
  int numEffectedV, numEffectedE, freeF, i;
  int curLvl, subdivLevels = ss->subdivLevels;

  ccgSubSurf__allFaces(ss, &effectedF, &numEffectedF, &freeF);
  ccgSubSurf__effectedFaceNeighbours(
      ss, effectedF, numEffectedF, &effectedV, &numEffectedV, &effectedE, &numEffectedE);

  for (curLvl = lvl; curLvl < subdivLevels; curLvl++) {
    ccgSubSurf__calcSubdivLevel(
        ss, effectedV, effectedE, effectedF, numEffectedV, numEffectedE, numEffectedF, curLvl);
  }

  for (i = 0; i < numEffectedV; i++) {
    effectedV[i]->flags = 0;
  }
  for (i = 0; i < numEffectedE; i++) {
    effectedE[i]->flags = 0;
  }
  for (i = 0; i < numEffectedF; i++) {
    effectedF[i]->flags = 0;
  }

  MEM_freeN(effectedE);
  MEM_freeN(effectedV);
  if (freeF) {
    MEM_freeN(effectedF);
  }

  return eCCGError_None;
}
