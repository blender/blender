/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eduv
 */

#include <functional>
#include <vector>

#include "GEO_uv_parametrizer.hh"

#include "BLI_array.hh"
#include "BLI_convexhull_2d.hh"
#include "BLI_ghash.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_polyfill_2d.h"
#include "BLI_polyfill_2d_beautify.h"
#include "BLI_rand.h"

#ifdef WITH_UV_SLIM
#  include "slim_matrix_transfer.h"
#endif

#include "GEO_uv_pack.hh"

#include "eigen_capi.h"

/* Utils */

namespace blender::geometry {

#define param_warning(message) \
  {/* `printf("Warning %s:%d: %s\n", __FILE__, __LINE__, message);` */}(void)0

/* Prevent unused function warnings when slim is disabled. */
#ifdef WITH_UV_SLIM
#  define UNUSED_FUNCTION_NO_SLIM(x) x
#else
#  define UNUSED_FUNCTION_NO_SLIM UNUSED_FUNCTION
#endif

/* Special Purpose Hash */

using PHashKey = uintptr_t;

struct PHashLink {
  PHashLink *next;
  PHashKey key;
};

struct PHash {
  PHashLink **list;
  PHashLink **buckets;
  int size, cursize, cursize_id;
};

/* Simplices */

struct PVert {
  PVert *nextlink;

  union PVertUnion {
    PHashKey key;       /* Construct. */
    int id;             /* ABF/LSCM matrix index. */
    HeapNode *heaplink; /* Edge collapsing. */
  } u;

  struct PEdge *edge;
  float co[3];
  float uv[2];
  uint flag;

  float weight;
  bool on_boundary_flag;
  int slim_id;
};

struct PEdge {
  PEdge *nextlink;

  union PEdgeUnion {
    PHashKey key;        /* Construct. */
    int id;              /* ABF matrix index. */
    HeapNode *heaplink;  /* Fill holes. */
    PEdge *nextcollapse; /* Simplification. */
  } u;

  PVert *vert;
  PEdge *pair;
  PEdge *next;
  struct PFace *face;
  float *orig_uv, old_uv[2];
  uint flag;
};

struct PFace {
  PFace *nextlink;

  union PFaceUnion {
    PHashKey key; /* Construct. */
    int chart;    /* Construct splitting. */
    float area3d; /* Stretch. */
    int id;       /* ABF matrix index. */
  } u;

  PEdge *edge;
  uint flag;
};

enum PVertFlag {
  PVERT_PIN = 1,
  PVERT_SELECT = 2,
  PVERT_INTERIOR = 4,
  PVERT_COLLAPSE = 8,
  PVERT_SPLIT = 16,
};

enum PEdgeFlag {
  PEDGE_SEAM = 1,
  PEDGE_VERTEX_SPLIT = 2,
  PEDGE_PIN = 4,
  PEDGE_SELECT = 8,
  PEDGE_DONE = 16,
  PEDGE_FILLED = 32,
  PEDGE_COLLAPSE = 64,
  PEDGE_COLLAPSE_EDGE = 128,
  PEDGE_COLLAPSE_PAIR = 256,
};

/* for flipping faces */
#define PEDGE_VERTEX_FLAGS (PEDGE_PIN)

enum PFaceFlag {
  PFACE_CONNECTED = 1,
  PFACE_FILLED = 2,
  PFACE_COLLAPSE = 4,
  PFACE_DONE = 8,
};

/* Chart */

struct PChart {
  PVert *verts;
  PEdge *edges;
  PFace *faces;
  int nverts, nedges, nfaces;
  int nboundaries;

  PVert *collapsed_verts;
  PEdge *collapsed_edges;
  PFace *collapsed_faces;

  float area_uv;
  float area_3d;

  float origin[2];

  LinearSolver *context;
  float *abf_alpha;
  PVert *pin1;
  PVert *pin2;
  PVert *single_pin;

  bool has_pins;
  bool skip_flush;
};

/* PHash
 * - special purpose hash that keeps all its elements in a single linked list.
 * - after construction, this hash is thrown away, and the list remains.
 * - removing elements is not possible efficiently.
 */

static int PHashSizes[] = {
    1,       3,       5,       11,      17,       37,       67,       131,       257,       521,
    1031,    2053,    4099,    8209,    16411,    32771,    65537,    131101,    262147,    524309,
    1048583, 2097169, 4194319, 8388617, 16777259, 33554467, 67108879, 134217757, 268435459,
};

#define PHASH_hash(ph, item) (uintptr_t(item) % uint((ph)->cursize))
#define PHASH_edge(v1, v2) (((v1) < (v2)) ? ((v1) * 39) ^ ((v2) * 31) : ((v1) * 31) ^ ((v2) * 39))

static PHash *phash_new(PHashLink **list, int sizehint)
{
  PHash *ph = MEM_callocN<PHash>("PHash");
  ph->size = 0;
  ph->cursize_id = 0;
  ph->list = list;

  while (PHashSizes[ph->cursize_id] < sizehint) {
    ph->cursize_id++;
  }

  ph->cursize = PHashSizes[ph->cursize_id];
  ph->buckets = MEM_calloc_arrayN<PHashLink *>(ph->cursize, "PHashBuckets");

  return ph;
}

static void phash_safe_delete(PHash **pph)
{
  if (!*pph) {
    return;
  }
  MEM_SAFE_FREE((*pph)->buckets);
  MEM_freeN(*pph);
  *pph = nullptr;
}

static int phash_size(PHash *ph)
{
  return ph->size;
}

static void phash_insert(PHash *ph, PHashLink *link)
{
  int size = ph->cursize;
  uintptr_t hash = PHASH_hash(ph, link->key);
  PHashLink *lookup = ph->buckets[hash];

  if (lookup == nullptr) {
    /* insert in front of the list */
    ph->buckets[hash] = link;
    link->next = *(ph->list);
    *(ph->list) = link;
  }
  else {
    /* insert after existing element */
    link->next = lookup->next;
    lookup->next = link;
  }

  ph->size++;

  if (ph->size > (size * 3)) {
    PHashLink *next = nullptr, *first = *(ph->list);

    ph->cursize = PHashSizes[++ph->cursize_id];
    MEM_freeN(ph->buckets);
    ph->buckets = (PHashLink **)MEM_callocN(ph->cursize * sizeof(*ph->buckets), "PHashBuckets");
    ph->size = 0;
    *(ph->list) = nullptr;

    for (link = first; link; link = next) {
      next = link->next;
      phash_insert(ph, link);
    }
  }
}

static PHashLink *phash_lookup(PHash *ph, PHashKey key)
{
  PHashLink *link;
  uintptr_t hash = PHASH_hash(ph, key);

  for (link = ph->buckets[hash]; link; link = link->next) {
    if (link->key == key) {
      return link;
    }
    if (PHASH_hash(ph, link->key) != hash) {
      return nullptr;
    }
  }

  return link;
}

static PHashLink *phash_next(PHash *ph, PHashKey key, PHashLink *link)
{
  uintptr_t hash = PHASH_hash(ph, key);

  for (link = link->next; link; link = link->next) {
    if (link->key == key) {
      return link;
    }
    if (PHASH_hash(ph, link->key) != hash) {
      return nullptr;
    }
  }

  return link;
}

/* Angles close to 0 or 180 degrees cause rows filled with zeros in the linear_solver.
 * The matrix will then be rank deficient and / or have poor conditioning.
 * => Reduce the maximum angle to 179 degrees, and spread the remainder to the other angles.
 */
static void fix_large_angle(const float v_fix[3],
                            const float v1[3],
                            const float v2[3],
                            double *r_fix,
                            double *r_a1,
                            double *r_a2)
{
  const double max_angle = DEG2RAD(179.0);
  const double fix_amount = *r_fix - max_angle;
  if (fix_amount < 0.0f) {
    return; /* angle is reasonable, i.e. less than 179 degrees. */
  }

  /* The triangle is probably degenerate, or close to it.
   * Without loss of generality, transform the triangle such that
   *   v_fix == {  0, s}, *r_fix = 180 degrees
   *   v1    == {-x1, 0}, *r_a1  = 0
   *   v2    == { x2, 0}, *r_a2  = 0
   *
   * With `s = 0`, `x1 > 0`, `x2 > 0`
   *
   * Now make `s` a small number and do some math:
   *  tan(*r_a1) = s / x1
   *  tan(*r_a2) = s / x2
   *
   * Remember that `tan(angle) ~= angle`
   *
   * Rearrange to obtain:
   *  *r_a1 = fix_amount * x2 / (x1 + x2)
   *  *r_a2 = fix_amount * x1 / (x1 + x2)
   */

  const double dist_v1 = len_v3v3(v_fix, v1);
  const double dist_v2 = len_v3v3(v_fix, v2);
  const double sum = dist_v1 + dist_v2;
  const double weight = (sum > 1e-20f) ? dist_v2 / sum : 0.5f;

  /* Ensure sum of angles in triangle is unchanged. */
  *r_fix -= fix_amount;
  *r_a1 += fix_amount * weight;
  *r_a2 += fix_amount * (1.0f - weight);
}

static void p_triangle_angles(const float v1[3],
                              const float v2[3],
                              const float v3[3],
                              double *r_a1,
                              double *r_a2,
                              double *r_a3)
{
  *r_a1 = angle_v3v3v3(v3, v1, v2);
  *r_a2 = angle_v3v3v3(v1, v2, v3);
  *r_a3 = angle_v3v3v3(v2, v3, v1);

  /* Fix for degenerate geometry e.g. v1 = sum(v2 + v3). See #100874 */
  fix_large_angle(v1, v2, v3, r_a1, r_a2, r_a3);
  fix_large_angle(v2, v3, v1, r_a2, r_a3, r_a1);
  fix_large_angle(v3, v1, v2, r_a3, r_a1, r_a2);

  /* Workaround for degenerate geometry, e.g. v1 == v2 == v3. */
  *r_a1 = max_dd(*r_a1, 0.001f);
  *r_a2 = max_dd(*r_a2, 0.001f);
  *r_a3 = max_dd(*r_a3, 0.001f);
}

static void p_face_angles(PFace *f, double *r_a1, double *r_a2, double *r_a3)
{
  PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;
  PVert *v1 = e1->vert, *v2 = e2->vert, *v3 = e3->vert;

  p_triangle_angles(v1->co, v2->co, v3->co, r_a1, r_a2, r_a3);
}

static float p_vec_cos(const float v1[3], const float v2[3], const float v3[3])
{
  return cos_v3v3v3(v1, v2, v3);
}

static void p_triangle_cos(const float v1[3],
                           const float v2[3],
                           const float v3[3],
                           float *r_cos1,
                           float *r_cos2,
                           float *r_cos3)
{
  *r_cos1 = p_vec_cos(v3, v1, v2);
  *r_cos2 = p_vec_cos(v1, v2, v3);
  *r_cos3 = p_vec_cos(v2, v3, v1);
}

static void UNUSED_FUNCTION(p_face_cos)(PFace *f, float *r_cos1, float *r_cos2, float *r_cos3)
{
  PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;
  PVert *v1 = e1->vert, *v2 = e2->vert, *v3 = e3->vert;

  p_triangle_cos(v1->co, v2->co, v3->co, r_cos1, r_cos2, r_cos3);
}

static float p_face_area(PFace *f)
{
  PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;
  PVert *v1 = e1->vert, *v2 = e2->vert, *v3 = e3->vert;

  return area_tri_v3(v1->co, v2->co, v3->co);
}

static float p_area_signed(const float v1[2], const float v2[2], const float v3[2])
{
  return 0.5f * (((v2[0] - v1[0]) * (v3[1] - v1[1])) - ((v3[0] - v1[0]) * (v2[1] - v1[1])));
}

static float p_face_uv_area_signed(PFace *f)
{
  PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;
  PVert *v1 = e1->vert, *v2 = e2->vert, *v3 = e3->vert;

  return 0.5f * (((v2->uv[0] - v1->uv[0]) * (v3->uv[1] - v1->uv[1])) -
                 ((v3->uv[0] - v1->uv[0]) * (v2->uv[1] - v1->uv[1])));
}

static float p_edge_length(PEdge *e)
{
  return len_v3v3(e->vert->co, e->next->vert->co);
}

static float p_edge_length_squared(PEdge *e)
{
  return len_squared_v3v3(e->vert->co, e->next->vert->co);
}

static float p_edge_uv_length(PEdge *e)
{
  return len_v2v2(e->vert->uv, e->next->vert->uv);
}

static void p_chart_uv_bbox(PChart *chart, float minv[2], float maxv[2])
{
  PVert *v;

  INIT_MINMAX2(minv, maxv);

  for (v = chart->verts; v; v = v->nextlink) {
    minmax_v2v2_v2(minv, maxv, v->uv);
  }
}

static float p_chart_uv_area(PChart *chart)
{
  float area = 0.0f;

  for (PFace *f = chart->faces; f; f = f->nextlink) {
    area += fabsf(p_face_uv_area_signed(f));
  }

  return area;
}

static void p_chart_uv_scale(PChart *chart, const float scale)
{
  if (scale == 1.0f) {
    return; /* Identity transform. */
  }

  for (PVert *v = chart->verts; v; v = v->nextlink) {
    v->uv[0] *= scale;
    v->uv[1] *= scale;
  }
}

static void uv_parametrizer_scale_x(ParamHandle *phandle, const float scale_x)
{
  if (scale_x == 1.0f) {
    return; /* Identity transform. */
  }

  /* Scale every chart. */
  for (int i = 0; i < phandle->ncharts; i++) {
    PChart *chart = phandle->charts[i];
    for (PVert *v = chart->verts; v; v = v->nextlink) {
      v->uv[0] *= scale_x; /* Only scale x axis. */
    }
  }
}

static void p_chart_uv_translate(PChart *chart, const float trans[2])
{
  for (PVert *v = chart->verts; v; v = v->nextlink) {
    v->uv[0] += trans[0];
    v->uv[1] += trans[1];
  }
}

static void p_chart_uv_transform(PChart *chart, const float mat[2][2])
{
  for (PVert *v = chart->verts; v; v = v->nextlink) {
    mul_m2_v2(mat, v->uv);
  }
}

static void p_chart_uv_to_array(PChart *chart, MutableSpan<float2> points)
{
  PVert *v;
  uint i = 0;

  for (v = chart->verts; v; v = v->nextlink) {
    copy_v2_v2(points[i++], v->uv);
  }
}

static bool p_intersect_line_2d_dir(const float v1[2],
                                    const float dir1[2],
                                    const float v2[2],
                                    const float dir2[2],
                                    float r_isect[2])
{
  float lmbda, div;

  div = dir2[0] * dir1[1] - dir2[1] * dir1[0];

  if (div == 0.0f) {
    return false;
  }

  lmbda = ((v1[1] - v2[1]) * dir1[0] - (v1[0] - v2[0]) * dir1[1]) / div;
  r_isect[0] = v1[0] + lmbda * dir2[0];
  r_isect[1] = v1[1] + lmbda * dir2[1];

  return true;
}

/* Topological Utilities */

static PEdge *p_wheel_edge_next(PEdge *e)
{
  return e->next->next->pair;
}

static const PEdge *p_wheel_edge_next(const PEdge *e)
{
  return e->next->next->pair;
}

static PEdge *p_wheel_edge_prev(PEdge *e)
{
  return (e->pair) ? e->pair->next : nullptr;
}

static PEdge *p_boundary_edge_next(PEdge *e)
{
  return e->next->vert->edge;
}

static PEdge *p_boundary_edge_prev(PEdge *e)
{
  PEdge *we = e, *last;

  do {
    last = we;
    we = p_wheel_edge_next(we);
  } while (we && (we != e));

  return last->next->next;
}

static bool p_vert_interior(PVert *v)
{
  return v->edge->pair;
}

static void p_face_flip(PFace *f)
{
  PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;
  PVert *v1 = e1->vert, *v2 = e2->vert, *v3 = e3->vert;
  int f1 = e1->flag, f2 = e2->flag, f3 = e3->flag;
  float *orig_uv1 = e1->orig_uv, *orig_uv2 = e2->orig_uv, *orig_uv3 = e3->orig_uv;

  e1->vert = v2;
  e1->next = e3;
  e1->orig_uv = orig_uv2;
  e1->flag = (f1 & ~PEDGE_VERTEX_FLAGS) | (f2 & PEDGE_VERTEX_FLAGS);

  e2->vert = v3;
  e2->next = e1;
  e2->orig_uv = orig_uv3;
  e2->flag = (f2 & ~PEDGE_VERTEX_FLAGS) | (f3 & PEDGE_VERTEX_FLAGS);

  e3->vert = v1;
  e3->next = e2;
  e3->orig_uv = orig_uv1;
  e3->flag = (f3 & ~PEDGE_VERTEX_FLAGS) | (f1 & PEDGE_VERTEX_FLAGS);
}

#if 0
static void p_chart_topological_sanity_check(PChart *chart)
{
  PVert *v;
  PEdge *e;

  for (v = chart->verts; v; v = v->nextlink) {
    GEO_uv_parametrizer_test_equals_ptr("v->edge->vert", v, v->edge->vert);
  }

  for (e = chart->edges; e; e = e->nextlink) {
    if (e->pair) {
      GEO_uv_parametrizer_test_equals_ptr("e->pair->pair", e, e->pair->pair);
      GEO_uv_parametrizer_test_equals_ptr("pair->vert", e->vert, e->pair->next->vert);
      GEO_uv_parametrizer_test_equals_ptr("pair->next->vert", e->next->vert, e->pair->vert);
    }
  }
}
#endif

/* Loading / Flushing */

static void p_vert_load_pin_select_uvs(ParamHandle *handle, PVert *v)
{
  PEdge *e;
  int nedges = 0, npins = 0;
  float pinuv[2];

  v->uv[0] = v->uv[1] = 0.0f;
  pinuv[0] = pinuv[1] = 0.0f;
  e = v->edge;
  do {
    if (e->orig_uv) {
      if (e->flag & PEDGE_SELECT) {
        v->flag |= PVERT_SELECT;
      }

      if (e->flag & PEDGE_PIN) {
        pinuv[0] += e->orig_uv[0] * handle->aspect_y;
        pinuv[1] += e->orig_uv[1];
        npins++;
      }
      else {
        v->uv[0] += e->orig_uv[0] * handle->aspect_y;
        v->uv[1] += e->orig_uv[1];
      }

      nedges++;
    }

    e = p_wheel_edge_next(e);
  } while (e && e != (v->edge));

  if (npins > 0) {
    v->uv[0] = pinuv[0] / npins;
    v->uv[1] = pinuv[1] / npins;
    v->flag |= PVERT_PIN;
  }
  else if (nedges > 0) {
    v->uv[0] /= nedges;
    v->uv[1] /= nedges;
  }
}

static void p_chart_flush_collapsed_uvs(PChart *chart);

static void p_flush_uvs(ParamHandle *handle, PChart *chart)
{
  const float blend = handle->blend;
  const float invblend = 1.0f - blend;
  const float invblend_x = invblend / handle->aspect_y;
  for (PEdge *e = chart->edges; e; e = e->nextlink) {
    if (e->orig_uv) {
      e->orig_uv[0] = blend * e->old_uv[0] + invblend_x * e->vert->uv[0];
      e->orig_uv[1] = blend * e->old_uv[1] + invblend * e->vert->uv[1];
    }
  }

  if (chart->collapsed_edges) {
    p_chart_flush_collapsed_uvs(chart);

    for (PEdge *e = chart->collapsed_edges; e; e = e->nextlink) {
      if (e->orig_uv) {
        e->orig_uv[0] = blend * e->old_uv[0] + invblend_x * e->vert->uv[0];
        e->orig_uv[1] = blend * e->old_uv[1] + invblend * e->vert->uv[1];
      }
    }
  }
}

static void p_face_backup_uvs(PFace *f)
{
  PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;

  if (e1->orig_uv) {
    e1->old_uv[0] = e1->orig_uv[0];
    e1->old_uv[1] = e1->orig_uv[1];
  }
  if (e2->orig_uv) {
    e2->old_uv[0] = e2->orig_uv[0];
    e2->old_uv[1] = e2->orig_uv[1];
  }
  if (e3->orig_uv) {
    e3->old_uv[0] = e3->orig_uv[0];
    e3->old_uv[1] = e3->orig_uv[1];
  }
}

static void p_face_restore_uvs(PFace *f)
{
  PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;

  if (e1->orig_uv) {
    e1->orig_uv[0] = e1->old_uv[0];
    e1->orig_uv[1] = e1->old_uv[1];
  }
  if (e2->orig_uv) {
    e2->orig_uv[0] = e2->old_uv[0];
    e2->orig_uv[1] = e2->old_uv[1];
  }
  if (e3->orig_uv) {
    e3->orig_uv[0] = e3->old_uv[0];
    e3->orig_uv[1] = e3->old_uv[1];
  }
}

/* Construction (use only during construction, relies on u.key being set */

static PVert *p_vert_add(
    ParamHandle *handle, PHashKey key, const float co[3], const float weight, PEdge *e)
{
  PVert *v = (PVert *)BLI_memarena_alloc(handle->arena, sizeof(*v));
  copy_v3_v3(v->co, co);
  v->weight = weight;

  /* Sanity check, a single nan/inf point causes the entire result to be invalid.
   * Note that values within the calculation may _become_ non-finite,
   * so the rest of the code still needs to take this possibility into account. */
  for (int i = 0; i < 3; i++) {
    if (UNLIKELY(!isfinite(v->co[i]))) {
      v->co[i] = 0.0f;
    }
  }

  v->u.key = key;
  v->edge = e;
  v->flag = 0;

  /* Unused, prevent uninitialized memory access on duplication. */
  v->on_boundary_flag = false;
  v->slim_id = 0;

  phash_insert(handle->hash_verts, (PHashLink *)v);

  return v;
}

static PVert *p_vert_lookup(
    ParamHandle *handle, PHashKey key, const float co[3], const float weight, PEdge *e)
{
  PVert *v = (PVert *)phash_lookup(handle->hash_verts, key);

  if (v) {
    return v;
  }
  return p_vert_add(handle, key, co, weight, e);
}

static PVert *p_vert_copy(ParamHandle *handle, PVert *v)
{
  PVert *nv = (PVert *)BLI_memarena_alloc(handle->arena, sizeof(*nv));

  copy_v3_v3(nv->co, v->co);
  nv->uv[0] = v->uv[0];
  nv->uv[1] = v->uv[1];
  nv->u.key = v->u.key;
  nv->edge = v->edge;
  nv->flag = v->flag;

  nv->weight = v->weight;
  nv->on_boundary_flag = v->on_boundary_flag;
  nv->slim_id = v->slim_id;

  return nv;
}

static PEdge *p_edge_lookup(ParamHandle *handle, const PHashKey *vkeys)
{
  PHashKey key = PHASH_edge(vkeys[0], vkeys[1]);
  PEdge *e = (PEdge *)phash_lookup(handle->hash_edges, key);

  while (e) {
    if ((e->vert->u.key == vkeys[0]) && (e->next->vert->u.key == vkeys[1])) {
      return e;
    }
    if ((e->vert->u.key == vkeys[1]) && (e->next->vert->u.key == vkeys[0])) {
      return e;
    }

    e = (PEdge *)phash_next(handle->hash_edges, key, (PHashLink *)e);
  }

  return nullptr;
}

static int p_face_exists(ParamHandle *handle, const ParamKey *pvkeys, int i1, int i2, int i3)
{
  PHashKey *vkeys = (PHashKey *)pvkeys;
  PHashKey key = PHASH_edge(vkeys[i1], vkeys[i2]);
  PEdge *e = (PEdge *)phash_lookup(handle->hash_edges, key);

  while (e) {
    if ((e->vert->u.key == vkeys[i1]) && (e->next->vert->u.key == vkeys[i2])) {
      if (e->next->next->vert->u.key == vkeys[i3]) {
        return true;
      }
    }
    else if ((e->vert->u.key == vkeys[i2]) && (e->next->vert->u.key == vkeys[i1])) {
      if (e->next->next->vert->u.key == vkeys[i3]) {
        return true;
      }
    }

    e = (PEdge *)phash_next(handle->hash_edges, key, (PHashLink *)e);
  }

  return false;
}

static bool p_edge_implicit_seam(PEdge *e, PEdge *ep)
{
  const float *uv1, *uv2, *uvp1, *uvp2;
  float limit[2];

  limit[0] = 0.00001;
  limit[1] = 0.00001;

  uv1 = e->orig_uv;
  uv2 = e->next->orig_uv;

  if (e->vert->u.key == ep->vert->u.key) {
    uvp1 = ep->orig_uv;
    uvp2 = ep->next->orig_uv;
  }
  else {
    uvp1 = ep->next->orig_uv;
    uvp2 = ep->orig_uv;
  }

  if ((fabsf(uv1[0] - uvp1[0]) > limit[0]) || (fabsf(uv1[1] - uvp1[1]) > limit[1])) {
    e->flag |= PEDGE_SEAM;
    ep->flag |= PEDGE_SEAM;
    return true;
  }
  if ((fabsf(uv2[0] - uvp2[0]) > limit[0]) || (fabsf(uv2[1] - uvp2[1]) > limit[1])) {
    e->flag |= PEDGE_SEAM;
    ep->flag |= PEDGE_SEAM;
    return true;
  }

  return false;
}

static bool p_edge_has_pair(ParamHandle *handle, PEdge *e, bool topology_from_uvs, PEdge **r_pair)
{
  PHashKey key;
  PEdge *pe;
  const PVert *v1, *v2;
  PHashKey key1 = e->vert->u.key;
  PHashKey key2 = e->next->vert->u.key;

  if (e->flag & PEDGE_SEAM) {
    return false;
  }

  key = PHASH_edge(key1, key2);
  pe = (PEdge *)phash_lookup(handle->hash_edges, key);
  *r_pair = nullptr;

  while (pe) {
    if (pe != e) {
      v1 = pe->vert;
      v2 = pe->next->vert;

      if (((v1->u.key == key1) && (v2->u.key == key2)) ||
          ((v1->u.key == key2) && (v2->u.key == key1)))
      {

        /* don't connect seams and t-junctions */
        if ((pe->flag & PEDGE_SEAM) || *r_pair ||
            (topology_from_uvs && p_edge_implicit_seam(e, pe)))
        {
          *r_pair = nullptr;
          return false;
        }

        *r_pair = pe;
      }
    }

    pe = (PEdge *)phash_next(handle->hash_edges, key, (PHashLink *)pe);
  }

  if (*r_pair && (e->vert == (*r_pair)->vert)) {
    if ((*r_pair)->next->pair || (*r_pair)->next->next->pair) {
      /* non unfoldable, maybe mobius ring or klein bottle */
      *r_pair = nullptr;
      return false;
    }
  }

  return (*r_pair != nullptr);
}

static bool p_edge_connect_pair(ParamHandle *handle,
                                PEdge *e,
                                bool topology_from_uvs,
                                PEdge ***stack)
{
  PEdge *pair = nullptr;

  if (!e->pair && p_edge_has_pair(handle, e, topology_from_uvs, &pair)) {
    if (e->vert == pair->vert) {
      p_face_flip(pair->face);
    }

    e->pair = pair;
    pair->pair = e;

    if (!(pair->face->flag & PFACE_CONNECTED)) {
      **stack = pair;
      (*stack)++;
    }
  }

  return (e->pair != nullptr);
}

static int p_connect_pairs(ParamHandle *handle, bool topology_from_uvs)
{
  PEdge **stackbase = MEM_malloc_arrayN<PEdge *>(size_t(phash_size(handle->hash_faces)),
                                                 "Pstackbase");
  PEdge **stack = stackbase;
  PFace *f, *first;
  PEdge *e, *e1, *e2;
  PChart *chart = handle->construction_chart;
  int ncharts = 0;

  /* Connect pairs, count edges, set vertex-edge pointer to a pair-less edge. */
  for (first = chart->faces; first; first = first->nextlink) {
    if (first->flag & PFACE_CONNECTED) {
      continue;
    }

    *stack = first->edge;
    stack++;

    while (stack != stackbase) {
      stack--;
      e = *stack;
      e1 = e->next;
      e2 = e1->next;

      f = e->face;
      f->flag |= PFACE_CONNECTED;

      /* assign verts to charts so we can sort them later */
      f->u.chart = ncharts;

      if (!p_edge_connect_pair(handle, e, topology_from_uvs, &stack)) {
        e->vert->edge = e;
      }
      if (!p_edge_connect_pair(handle, e1, topology_from_uvs, &stack)) {
        e1->vert->edge = e1;
      }
      if (!p_edge_connect_pair(handle, e2, topology_from_uvs, &stack)) {
        e2->vert->edge = e2;
      }
    }

    ncharts++;
  }

  MEM_freeN(stackbase);

  return ncharts;
}

static void p_split_vert(ParamHandle *handle, PChart *chart, PEdge *e)
{
  PEdge *we, *lastwe = nullptr;
  PVert *v = e->vert;
  bool copy = true;

  if (e->flag & PEDGE_PIN) {
    chart->has_pins = true;
  }

  if (e->flag & PEDGE_VERTEX_SPLIT) {
    return;
  }

  /* rewind to start */
  lastwe = e;
  for (we = p_wheel_edge_prev(e); we && (we != e); we = p_wheel_edge_prev(we)) {
    lastwe = we;
  }

  /* go over all edges in wheel */
  for (we = lastwe; we; we = p_wheel_edge_next(we)) {
    if (we->flag & PEDGE_VERTEX_SPLIT) {
      break;
    }

    we->flag |= PEDGE_VERTEX_SPLIT;

    if (we == v->edge) {
      /* found it, no need to copy */
      copy = false;
      v->nextlink = chart->verts;
      chart->verts = v;
      chart->nverts++;
    }
  }

  if (copy) {
    /* not found, copying */
    v->flag |= PVERT_SPLIT;
    v = p_vert_copy(handle, v);
    v->flag |= PVERT_SPLIT;

    v->nextlink = chart->verts;
    chart->verts = v;
    chart->nverts++;

    v->edge = lastwe;

    we = lastwe;
    do {
      we->vert = v;
      we = p_wheel_edge_next(we);
    } while (we && (we != lastwe));
  }
}

static PChart **p_split_charts(ParamHandle *handle, PChart *chart, int ncharts)
{
  PChart **charts = MEM_calloc_arrayN<PChart *>(ncharts, "PCharts");

  for (int i = 0; i < ncharts; i++) {
    charts[i] = MEM_callocN<PChart>("PChart");
  }

  PFace *f = chart->faces;
  while (f) {
    PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;
    PFace *nextf = f->nextlink;

    PChart *nchart = charts[f->u.chart];

    f->nextlink = nchart->faces;
    nchart->faces = f;
    e1->nextlink = nchart->edges;
    nchart->edges = e1;
    e2->nextlink = nchart->edges;
    nchart->edges = e2;
    e3->nextlink = nchart->edges;
    nchart->edges = e3;

    nchart->nfaces++;
    nchart->nedges += 3;

    p_split_vert(handle, nchart, e1);
    p_split_vert(handle, nchart, e2);
    p_split_vert(handle, nchart, e3);

    f = nextf;
  }

  return charts;
}

static PFace *p_face_add(ParamHandle *handle)
{
  PFace *f;

  /* allocate */
  f = (PFace *)BLI_memarena_alloc(handle->arena, sizeof(*f));
  f->flag = 0;

  PEdge *e1 = (PEdge *)BLI_memarena_calloc(handle->arena, sizeof(*e1));
  PEdge *e2 = (PEdge *)BLI_memarena_calloc(handle->arena, sizeof(*e2));
  PEdge *e3 = (PEdge *)BLI_memarena_calloc(handle->arena, sizeof(*e3));

  /* set up edges */
  f->edge = e1;
  e1->face = e2->face = e3->face = f;

  e1->next = e2;
  e2->next = e3;
  e3->next = e1;

  return f;
}

static PFace *p_face_add_construct(ParamHandle *handle,
                                   ParamKey key,
                                   const ParamKey *vkeys,
                                   const float **co,
                                   float **uv,
                                   const float *weight,
                                   int i1,
                                   int i2,
                                   int i3,
                                   const bool *pin,
                                   const bool *select)
{
  PFace *f = p_face_add(handle);
  PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;

  float weight1, weight2, weight3;
  if (weight) {
    weight1 = weight[i1];
    weight2 = weight[i2];
    weight3 = weight[i3];
  }
  else {
    weight1 = 1.0f;
    weight2 = 1.0f;
    weight3 = 1.0f;
  }

  e1->vert = p_vert_lookup(handle, vkeys[i1], co[i1], weight1, e1);
  e2->vert = p_vert_lookup(handle, vkeys[i2], co[i2], weight2, e2);
  e3->vert = p_vert_lookup(handle, vkeys[i3], co[i3], weight3, e3);

  e1->orig_uv = uv[i1];
  e2->orig_uv = uv[i2];
  e3->orig_uv = uv[i3];

  if (pin) {
    if (pin[i1]) {
      e1->flag |= PEDGE_PIN;
    }
    if (pin[i2]) {
      e2->flag |= PEDGE_PIN;
    }
    if (pin[i3]) {
      e3->flag |= PEDGE_PIN;
    }
  }

  if (select) {
    if (select[i1]) {
      e1->flag |= PEDGE_SELECT;
    }
    if (select[i2]) {
      e2->flag |= PEDGE_SELECT;
    }
    if (select[i3]) {
      e3->flag |= PEDGE_SELECT;
    }
  }

  f->u.key = key;
  phash_insert(handle->hash_faces, (PHashLink *)f);

  e1->u.key = PHASH_edge(vkeys[i1], vkeys[i2]);
  e2->u.key = PHASH_edge(vkeys[i2], vkeys[i3]);
  e3->u.key = PHASH_edge(vkeys[i3], vkeys[i1]);

  phash_insert(handle->hash_edges, (PHashLink *)e1);
  phash_insert(handle->hash_edges, (PHashLink *)e2);
  phash_insert(handle->hash_edges, (PHashLink *)e3);

  return f;
}

static PFace *p_face_add_fill(ParamHandle *handle, PChart *chart, PVert *v1, PVert *v2, PVert *v3)
{
  PFace *f = p_face_add(handle);
  PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;

  e1->vert = v1;
  e2->vert = v2;
  e3->vert = v3;

  e1->orig_uv = e2->orig_uv = e3->orig_uv = nullptr;

  f->nextlink = chart->faces;
  chart->faces = f;
  e1->nextlink = chart->edges;
  chart->edges = e1;
  e2->nextlink = chart->edges;
  chart->edges = e2;
  e3->nextlink = chart->edges;
  chart->edges = e3;

  chart->nfaces++;
  chart->nedges += 3;

  return f;
}

/* Construction: boundary filling */

static void p_chart_boundaries(PChart *chart, PEdge **r_outer)
{
  PEdge *e, *be;
  float len, maxlen = -1.0;

  chart->nboundaries = 0;
  if (r_outer) {
    *r_outer = nullptr;
  }

  for (e = chart->edges; e; e = e->nextlink) {
    if (e->pair || (e->flag & PEDGE_DONE)) {
      continue;
    }

    chart->nboundaries++;

    len = 0.0f;

    be = e;
    do {
      be->flag |= PEDGE_DONE;
      len += p_edge_length(be);
      be = be->next->vert->edge;
    } while (be != e);

    if (r_outer && (len > maxlen)) {
      *r_outer = e;
      maxlen = len;
    }
  }

  for (e = chart->edges; e; e = e->nextlink) {
    e->flag &= ~PEDGE_DONE;
  }
}

static float p_edge_boundary_angle(PEdge *e)
{
  PEdge *we;
  PVert *v, *v1, *v2;
  float angle;

  v = e->vert;

  /* concave angle check -- could be better */
  angle = M_PI;

  we = v->edge;
  do {
    v1 = we->next->vert;
    v2 = we->next->next->vert;
    angle -= angle_v3v3v3(v1->co, v->co, v2->co);

    we = we->next->next->pair;
  } while (we && (we != v->edge));

  return angle;
}

static void p_chart_fill_boundary(ParamHandle *handle, PChart *chart, PEdge *be, int nedges)
{
  PEdge *e, *e1, *e2;

  PFace *f;
  Heap *heap = BLI_heap_new();
  float angle;

  e = be;
  do {
    angle = p_edge_boundary_angle(e);
    e->u.heaplink = BLI_heap_insert(heap, angle, e);

    e = p_boundary_edge_next(e);
  } while (e != be);

  if (nedges == 2) {
    /* no real boundary, but an isolated seam */
    e = be->next->vert->edge;
    e->pair = be;
    be->pair = e;

    BLI_heap_remove(heap, e->u.heaplink);
    BLI_heap_remove(heap, be->u.heaplink);
  }
  else {
    while (nedges > 2) {
      PEdge *ne, *ne1, *ne2;

      e = (PEdge *)BLI_heap_pop_min(heap);

      e1 = p_boundary_edge_prev(e);
      e2 = p_boundary_edge_next(e);

      BLI_heap_remove(heap, e1->u.heaplink);
      BLI_heap_remove(heap, e2->u.heaplink);
      e->u.heaplink = e1->u.heaplink = e2->u.heaplink = nullptr;

      e->flag |= PEDGE_FILLED;
      e1->flag |= PEDGE_FILLED;

      f = p_face_add_fill(handle, chart, e->vert, e1->vert, e2->vert);
      f->flag |= PFACE_FILLED;

      ne = f->edge->next->next;
      ne1 = f->edge;
      ne2 = f->edge->next;

      ne->flag = ne1->flag = ne2->flag = PEDGE_FILLED;

      e->pair = ne;
      ne->pair = e;
      e1->pair = ne1;
      ne1->pair = e1;

      ne->vert = e2->vert;
      ne1->vert = e->vert;
      ne2->vert = e1->vert;

      if (nedges == 3) {
        e2->pair = ne2;
        ne2->pair = e2;
      }
      else {
        ne2->vert->edge = ne2;

        ne2->u.heaplink = BLI_heap_insert(heap, p_edge_boundary_angle(ne2), ne2);
        e2->u.heaplink = BLI_heap_insert(heap, p_edge_boundary_angle(e2), e2);
      }

      nedges--;
    }
  }

  BLI_heap_free(heap, nullptr);
}

static void p_chart_fill_boundaries(ParamHandle *handle, PChart *chart, const PEdge *outer)
{
  PEdge *e, *be; /* *enext - as yet unused */
  int nedges;

  for (e = chart->edges; e; e = e->nextlink) {
    /* enext = e->nextlink; - as yet unused */

    if (e->pair || (e->flag & PEDGE_FILLED)) {
      continue;
    }

    nedges = 0;
    be = e;
    do {
      be->flag |= PEDGE_FILLED;
      be = be->next->vert->edge;
      nedges++;
    } while (be != e);

    if (e != outer) {
      p_chart_fill_boundary(handle, chart, e, nedges);
    }
  }
}

#if 0
/* Polygon kernel for inserting uv's non overlapping */

static int p_polygon_point_in(const float cp1[2], const float cp2[2], const float p[2])
{
  if ((cp1[0] == p[0]) && (cp1[1] == p[1])) {
    return 2;
  }
  else if ((cp2[0] == p[0]) && (cp2[1] == p[1])) {
    return 3;
  }
  else {
    return (p_area_signed(cp1, cp2, p) >= 0.0f);
  }
}

static void p_polygon_kernel_clip(float (*oldpoints)[2],
                                  int noldpoints,
                                  float (*newpoints)[2],
                                  int *r_nnewpoints,
                                  const float cp1[2],
                                  const float cp2[2])
{
  float *p2, *p1, isect[2];
  int i, p2in, p1in;

  p1 = oldpoints[noldpoints - 1];
  p1in = p_polygon_point_in(cp1, cp2, p1);
  *r_nnewpoints = 0;

  for (i = 0; i < noldpoints; i++) {
    p2 = oldpoints[i];
    p2in = p_polygon_point_in(cp1, cp2, p2);

    if ((p2in >= 2) || (p1in && p2in)) {
      newpoints[*r_nnewpoints][0] = p2[0];
      newpoints[*r_nnewpoints][1] = p2[1];
      (*r_nnewpoints)++;
    }
    else if (p1in && !p2in) {
      if (p1in != 3) {
        p_intersect_line_2d(p1, p2, cp1, cp2, isect);
        newpoints[*r_nnewpoints][0] = isect[0];
        newpoints[*r_nnewpoints][1] = isect[1];
        (*r_nnewpoints)++;
      }
    }
    else if (!p1in && p2in) {
      p_intersect_line_2d(p1, p2, cp1, cp2, isect);
      newpoints[*r_nnewpoints][0] = isect[0];
      newpoints[*r_nnewpoints][1] = isect[1];
      (*r_nnewpoints)++;

      newpoints[*r_nnewpoints][0] = p2[0];
      newpoints[*r_nnewpoints][1] = p2[1];
      (*r_nnewpoints)++;
    }

    p1in = p2in;
    p1 = p2;
  }
}

static void p_polygon_kernel_center(float (*points)[2], int npoints, float *center)
{
  int i, size, nnewpoints = npoints;
  float(*oldpoints)[2], (*newpoints)[2], *p1, *p2;

  size = npoints * 3;
  oldpoints = MEM_malloc_arrayN<float[2]>(size_t(size), "PPolygonOldPoints");
  newpoints = MEM_malloc_arrayN<float[2]>(size_t(size), "PPolygonNewPoints");

  memcpy(oldpoints, points, sizeof(float[2]) * npoints);

  for (i = 0; i < npoints; i++) {
    p1 = points[i];
    p2 = points[(i + 1) % npoints];
    p_polygon_kernel_clip(oldpoints, nnewpoints, newpoints, &nnewpoints, p1, p2);

    if (nnewpoints == 0) {
      /* degenerate case, use center of original face */
      memcpy(oldpoints, points, sizeof(float[2]) * npoints);
      nnewpoints = npoints;
      break;
    }
    else if (nnewpoints == 1) {
      /* degenerate case, use remaining point */
      center[0] = newpoints[0][0];
      center[1] = newpoints[0][1];

      MEM_freeN(oldpoints);
      MEM_freeN(newpoints);

      return;
    }

    if (nnewpoints * 2 > size) {
      size *= 2;
      MEM_freeN(oldpoints);
      oldpoints = MEM_mallocN(sizeof(float[2]) * size, "oldpoints");
      memcpy(oldpoints, newpoints, sizeof(float[2]) * nnewpoints);
      MEM_freeN(newpoints);
      newpoints = MEM_mallocN(sizeof(float[2]) * size, "newpoints");
    }
    else {
      float(*sw_points)[2] = oldpoints;
      oldpoints = newpoints;
      newpoints = sw_points;
    }
  }

  center[0] = center[1] = 0.0f;

  for (i = 0; i < nnewpoints; i++) {
    center[0] += oldpoints[i][0];
    center[1] += oldpoints[i][1];
  }

  center[0] /= nnewpoints;
  center[1] /= nnewpoints;

  MEM_freeN(oldpoints);
  MEM_freeN(newpoints);
}
#endif

/* Simplify/Complexity
 *
 * This is currently used for eliminating degenerate vertex coordinates.
 * In the future this can be used for efficient unwrapping of high resolution
 * charts at lower resolution. */

#if 0

static float p_vert_cotan(const float v1[3], const float v2[3], const float v3[3])
{
  float a[3], b[3], c[3], clen;

  sub_v3_v3v3(a, v2, v1);
  sub_v3_v3v3(b, v3, v1);
  cross_v3_v3v3(c, a, b);

  clen = len_v3(c);

  if (clen == 0.0f) {
    return 0.0f;
  }

  return dot_v3v3(a, b) / clen;
}

static bool p_vert_flipped_wheel_triangle(PVert *v)
{
  PEdge *e = v->edge;

  do {
    if (p_face_uv_area_signed(e->face) < 0.0f) {
      return true;
    }

    e = p_wheel_edge_next(e);
  } while (e && (e != v->edge));

  return false;
}

static bool p_vert_map_harmonic_weights(PVert *v)
{
  float weightsum, positionsum[2], olduv[2];

  weightsum = 0.0f;
  positionsum[0] = positionsum[1] = 0.0f;

  if (p_vert_interior(v)) {
    PEdge *e = v->edge;

    do {
      float t1, t2, weight;
      PVert *v1, *v2;

      v1 = e->next->vert;
      v2 = e->next->next->vert;
      t1 = p_vert_cotan(v2->co, e->vert->co, v1->co);

      v1 = e->pair->next->vert;
      v2 = e->pair->next->next->vert;
      t2 = p_vert_cotan(v2->co, e->pair->vert->co, v1->co);

      weight = 0.5f * (t1 + t2);
      weightsum += weight;
      positionsum[0] += weight * e->pair->vert->uv[0];
      positionsum[1] += weight * e->pair->vert->uv[1];

      e = p_wheel_edge_next(e);
    } while (e && (e != v->edge));
  }
  else {
    PEdge *e = v->edge;

    do {
      float t1, t2;
      PVert *v1, *v2;

      v2 = e->next->vert;
      v1 = e->next->next->vert;

      t1 = p_vert_cotan(v1->co, v->co, v2->co);
      t2 = p_vert_cotan(v2->co, v->co, v1->co);

      weightsum += t1 + t2;
      positionsum[0] += (v2->uv[1] - v1->uv[1]) + (t1 * v2->uv[0] + t2 * v1->uv[0]);
      positionsum[1] += (v1->uv[0] - v2->uv[0]) + (t1 * v2->uv[1] + t2 * v1->uv[1]);

      e = p_wheel_edge_next(e);
    } while (e && (e != v->edge));
  }

  if (weightsum != 0.0f) {
    weightsum = 1.0f / weightsum;
    positionsum[0] *= weightsum;
    positionsum[1] *= weightsum;
  }

  olduv[0] = v->uv[0];
  olduv[1] = v->uv[1];
  v->uv[0] = positionsum[0];
  v->uv[1] = positionsum[1];

  if (p_vert_flipped_wheel_triangle(v)) {
    v->uv[0] = olduv[0];
    v->uv[1] = olduv[1];

    return false;
  }

  return true;
}

static void p_vert_harmonic_insert(PVert *v)
{
  PEdge *e;

  if (!p_vert_map_harmonic_weights(v)) {
    /* do face kernel center insertion: this is quite slow, but should
     * only be needed for 0.01 % of verts or so, when insert with harmonic
     * weights fails */

    int npoints = 0, i;
    float(*points)[2];

    e = v->edge;
    do {
      npoints++;
      e = p_wheel_edge_next(e);
    } while (e && (e != v->edge));

    if (e == nullptr) {
      npoints++;
    }

    points = MEM_malloc_arrayN<float[2]>(size_t(npoints), "PHarmonicPoints");

    e = v->edge;
    i = 0;
    do {
      PEdge *nexte = p_wheel_edge_next(e);

      points[i][0] = e->next->vert->uv[0];
      points[i][1] = e->next->vert->uv[1];

      if (nexte == nullptr) {
        i++;
        points[i][0] = e->next->next->vert->uv[0];
        points[i][1] = e->next->next->vert->uv[1];
        break;
      }

      e = nexte;
      i++;
    } while (e != v->edge);

    p_polygon_kernel_center(points, npoints, v->uv);

    MEM_freeN(points);
  }

  e = v->edge;
  do {
    if (!(e->next->vert->flag & PVERT_PIN)) {
      p_vert_map_harmonic_weights(e->next->vert);
    }
    e = p_wheel_edge_next(e);
  } while (e && (e != v->edge));

  p_vert_map_harmonic_weights(v);
}
#endif

static void p_vert_fix_edge_pointer(PVert *v)
{
  PEdge *start = v->edge;

  /* set v->edge pointer to the edge with no pair, if there is one */
  while (v->edge->pair) {
    v->edge = p_wheel_edge_prev(v->edge);

    if (v->edge == start) {
      break;
    }
  }
}

static void p_collapsing_verts(PEdge *edge, PEdge *pair, PVert **r_newv, PVert **r_keepv)
{
  /* the two vertices that are involved in the collapse */
  if (edge) {
    *r_newv = edge->vert;
    *r_keepv = edge->next->vert;
  }
  else {
    *r_newv = pair->next->vert;
    *r_keepv = pair->vert;
  }
}

static void p_collapse_edge(PEdge *edge, PEdge *pair)
{
  PVert *oldv, *keepv;
  PEdge *e;

  p_collapsing_verts(edge, pair, &oldv, &keepv);

  /* change e->vert pointers from old vertex to the target vertex */
  e = oldv->edge;
  do {
    if ((e != edge) && !(pair && pair->next == e)) {
      e->vert = keepv;
    }

    e = p_wheel_edge_next(e);
  } while (e && (e != oldv->edge));

  /* set keepv->edge pointer */
  if ((edge && (keepv->edge == edge->next)) || (keepv->edge == pair)) {
    if (edge && edge->next->pair) {
      keepv->edge = edge->next->pair->next;
    }
    else if (pair && pair->next->next->pair) {
      keepv->edge = pair->next->next->pair;
    }
    else if (edge && edge->next->next->pair) {
      keepv->edge = edge->next->next->pair;
    }
    else {
      keepv->edge = pair->next->pair->next;
    }
  }

  /* update pairs and v->edge pointers */
  if (edge) {
    PEdge *e1 = edge->next, *e2 = e1->next;

    if (e1->pair) {
      e1->pair->pair = e2->pair;
    }

    if (e2->pair) {
      e2->pair->pair = e1->pair;
      e2->vert->edge = p_wheel_edge_prev(e2);
    }
    else {
      e2->vert->edge = p_wheel_edge_next(e2);
    }

    p_vert_fix_edge_pointer(e2->vert);
  }

  if (pair) {
    PEdge *e1 = pair->next, *e2 = e1->next;

    if (e1->pair) {
      e1->pair->pair = e2->pair;
    }

    if (e2->pair) {
      e2->pair->pair = e1->pair;
      e2->vert->edge = p_wheel_edge_prev(e2);
    }
    else {
      e2->vert->edge = p_wheel_edge_next(e2);
    }

    p_vert_fix_edge_pointer(e2->vert);
  }

  p_vert_fix_edge_pointer(keepv);

  /* mark for move to collapsed list later */
  oldv->flag |= PVERT_COLLAPSE;

  if (edge) {
    PFace *f = edge->face;
    PEdge *e1 = edge->next, *e2 = e1->next;

    f->flag |= PFACE_COLLAPSE;
    edge->flag |= PEDGE_COLLAPSE;
    e1->flag |= PEDGE_COLLAPSE;
    e2->flag |= PEDGE_COLLAPSE;
  }

  if (pair) {
    PFace *f = pair->face;
    PEdge *e1 = pair->next, *e2 = e1->next;

    f->flag |= PFACE_COLLAPSE;
    pair->flag |= PEDGE_COLLAPSE;
    e1->flag |= PEDGE_COLLAPSE;
    e2->flag |= PEDGE_COLLAPSE;
  }
}

#if 0
static void p_split_vertex(PEdge *edge, PEdge *pair)
{
  PVert *newv, *keepv;
  PEdge *e;

  p_collapsing_verts(edge, pair, &newv, &keepv);

  /* update edge pairs */
  if (edge) {
    PEdge *e1 = edge->next, *e2 = e1->next;

    if (e1->pair) {
      e1->pair->pair = e1;
    }
    if (e2->pair) {
      e2->pair->pair = e2;
    }

    e2->vert->edge = e2;
    p_vert_fix_edge_pointer(e2->vert);
    keepv->edge = e1;
  }

  if (pair) {
    PEdge *e1 = pair->next, *e2 = e1->next;

    if (e1->pair) {
      e1->pair->pair = e1;
    }
    if (e2->pair) {
      e2->pair->pair = e2;
    }

    e2->vert->edge = e2;
    p_vert_fix_edge_pointer(e2->vert);
    keepv->edge = pair;
  }

  p_vert_fix_edge_pointer(keepv);

  /* set e->vert pointers to restored vertex */
  e = newv->edge;
  do {
    e->vert = newv;
    e = p_wheel_edge_next(e);
  } while (e && (e != newv->edge));
}
#endif

static bool p_collapse_allowed_topologic(PEdge *edge, PEdge *pair)
{
  PVert *oldv, *keepv;

  p_collapsing_verts(edge, pair, &oldv, &keepv);

  /* boundary edges */
  if (!edge || !pair) {
    /* avoid collapsing chart into an edge */
    if (edge && !edge->next->pair && !edge->next->next->pair) {
      return false;
    }
    if (pair && !pair->next->pair && !pair->next->next->pair) {
      return false;
    }
  }
  /* avoid merging two boundaries (oldv and keepv are on the 'other side' of
   * the chart) */
  else if (!p_vert_interior(oldv) && !p_vert_interior(keepv)) {
    return false;
  }

  return true;
}

#if 0
static bool p_collapse_normal_flipped(float *v1, float *v2, float *vold, float *vnew)
{
  float nold[3], nnew[3], sub1[3], sub2[3];

  sub_v3_v3v3(sub1, vold, v1);
  sub_v3_v3v3(sub2, vold, v2);
  cross_v3_v3v3(nold, sub1, sub2);

  sub_v3_v3v3(sub1, vnew, v1);
  sub_v3_v3v3(sub2, vnew, v2);
  cross_v3_v3v3(nnew, sub1, sub2);

  return (dot_v3v3(nold, nnew) <= 0.0f);
}

static bool p_collapse_allowed_geometric(PEdge *edge, PEdge *pair)
{
  PVert *oldv, *keepv;
  PEdge *e;
  float angulardefect, angle;

  p_collapsing_verts(edge, pair, &oldv, &keepv);

  angulardefect = 2 * M_PI;

  e = oldv->edge;
  do {
    float a[3], b[3], minangle, maxangle;
    PEdge *e1 = e->next, *e2 = e1->next;
    PVert *v1 = e1->vert, *v2 = e2->vert;
    int i;

    angle = p_vec_angle(v1->co, oldv->co, v2->co);
    angulardefect -= angle;

    /* skip collapsing faces */
    if (v1 == keepv || v2 == keepv) {
      e = p_wheel_edge_next(e);
      continue;
    }

    if (p_collapse_normal_flipped(v1->co, v2->co, oldv->co, keepv->co)) {
      return false;
    }

    a[0] = angle;
    a[1] = p_vec_angle(v2->co, v1->co, oldv->co);
    a[2] = M_PI - a[0] - a[1];

    b[0] = p_vec_angle(v1->co, keepv->co, v2->co);
    b[1] = p_vec_angle(v2->co, v1->co, keepv->co);
    b[2] = M_PI - b[0] - b[1];

    /* ABF criterion 1: avoid sharp and obtuse angles. */
    minangle = DEG2RADF(15.0);
    maxangle = M_PI - minangle;

    for (i = 0; i < 3; i++) {
      if ((b[i] < a[i]) && (b[i] < minangle)) {
        return false;
      }
      else if ((b[i] > a[i]) && (b[i] > maxangle)) {
        return false;
      }
    }

    e = p_wheel_edge_next(e);
  } while (e && (e != oldv->edge));

  if (p_vert_interior(oldv)) {
    /* HLSCM criterion: angular defect smaller than threshold. */
    if (fabsf(angulardefect) > DEG2RADF(30.0)) {
      return false;
    }
  }
  else {
    PVert *v1 = p_boundary_edge_next(oldv->edge)->vert;
    PVert *v2 = p_boundary_edge_prev(oldv->edge)->vert;

    /* ABF++ criterion 2: avoid collapsing verts inwards. */
    if (p_vert_interior(keepv)) {
      return false;
    }

    /* Don't collapse significant boundary changes. */
    angle = p_vec_angle(v1->co, oldv->co, v2->co);
    if (angle < DEG2RADF(160.0)) {
      return false;
    }
  }

  return true;
}

static bool p_collapse_allowed(PEdge *edge, PEdge *pair)
{
  PVert *oldv, *keepv;

  p_collapsing_verts(edge, pair, &oldv, &keepv);

  if (oldv->flag & PVERT_PIN) {
    return false;
  }

  return (p_collapse_allowed_topologic(edge, pair) && p_collapse_allowed_geometric(edge, pair));
}

static float p_collapse_cost(PEdge *edge, PEdge *pair)
{
  /* based on volume and boundary optimization from:
   * "Fast and Memory Efficient Polygonal Simplification" P. Lindstrom, G. Turk */

  PVert *oldv, *keepv;
  PEdge *e;
  PFace *oldf1, *oldf2;
  float volumecost = 0.0f, areacost = 0.0f, edgevec[3], cost, weight, elen;
  float shapecost = 0.0f;
  float shapeold = 0.0f, shapenew = 0.0f;
  int nshapeold = 0, nshapenew = 0;

  p_collapsing_verts(edge, pair, &oldv, &keepv);
  oldf1 = (edge) ? edge->face : nullptr;
  oldf2 = (pair) ? pair->face : nullptr;

  sub_v3_v3v3(edgevec, keepv->co, oldv->co);

  e = oldv->edge;
  do {
    float a1, a2, a3;
    float *co1 = e->next->vert->co;
    float *co2 = e->next->next->vert->co;

    if (!ELEM(e->face, oldf1, oldf2)) {
      float tetrav2[3], tetrav3[3];

      /* tetrahedron volume = (1/3!)*|a.(b x c)| */
      sub_v3_v3v3(tetrav2, co1, oldv->co);
      sub_v3_v3v3(tetrav3, co2, oldv->co);
      volumecost += fabsf(volume_tri_tetrahedron_signed_v3(tetrav2, tetrav3, edgevec));

#  if 0
      shapecost += dot_v3v3(co1, keepv->co);

      if (p_wheel_edge_next(e) == nullptr) {
        shapecost += dot_v3v3(co2, keepv->co);
      }
#  endif

      p_triangle_angles(oldv->co, co1, co2, &a1, &a2, &a3);
      a1 = a1 - M_PI / 3.0;
      a2 = a2 - M_PI / 3.0;
      a3 = a3 - M_PI / 3.0;
      shapeold = (a1 * a1 + a2 * a2 + a3 * a3) / (M_PI_2 * M_PI_2);

      nshapeold++;
    }
    else {
      p_triangle_angles(keepv->co, co1, co2, &a1, &a2, &a3);
      a1 = a1 - M_PI / 3.0;
      a2 = a2 - M_PI / 3.0;
      a3 = a3 - M_PI / 3.0;
      shapenew = (a1 * a1 + a2 * a2 + a3 * a3) / (M_PI_2 * M_PI_2);

      nshapenew++;
    }

    e = p_wheel_edge_next(e);
  } while (e && (e != oldv->edge));

  if (!p_vert_interior(oldv)) {
    PVert *v1 = p_boundary_edge_prev(oldv->edge)->vert;
    PVert *v2 = p_boundary_edge_next(oldv->edge)->vert;

    areacost = area_tri_v3(oldv->co, v1->co, v2->co);
  }

  elen = len_v3(edgevec);
  weight = 1.0f; /* 0.2f */
  cost = weight * volumecost * volumecost + elen * elen * areacost * areacost;
#  if 0
  cost += shapecost;
#  else
  shapeold /= nshapeold;
  shapenew /= nshapenew;
  shapecost = (shapeold + 0.00001) / (shapenew + 0.00001);

  cost *= shapecost;
#  endif

  return cost;
}
#endif

static void p_collapse_cost_vertex(
    PVert *vert,
    float *r_mincost,
    PEdge **r_mine,
    const std::function<float(PEdge *, PEdge *)> &collapse_cost_fn,
    const std::function<float(PEdge *, PEdge *)> &collapse_allowed_fn)
{
  PEdge *e, *enext, *pair;

  *r_mine = nullptr;
  *r_mincost = 0.0f;
  e = vert->edge;
  do {
    if (collapse_allowed_fn(e, e->pair)) {
      float cost = collapse_cost_fn(e, e->pair);

      if ((*r_mine == nullptr) || (cost < *r_mincost)) {
        *r_mincost = cost;
        *r_mine = e;
      }
    }

    enext = p_wheel_edge_next(e);

    if (enext == nullptr) {
      /* The other boundary edge, where we only have the pair half-edge. */
      pair = e->next->next;

      if (collapse_allowed_fn(nullptr, pair)) {
        float cost = collapse_cost_fn(nullptr, pair);

        if ((*r_mine == nullptr) || (cost < *r_mincost)) {
          *r_mincost = cost;
          *r_mine = pair;
        }
      }

      break;
    }

    e = enext;
  } while (e != vert->edge);
}

static void p_chart_post_collapse_flush(PChart *chart, PEdge *collapsed)
{
  /* Move to `collapsed_*`. */

  PVert *v, *nextv = nullptr, *verts = chart->verts;
  PEdge *e, *nexte = nullptr, *edges = chart->edges, *laste = nullptr;
  PFace *f, *nextf = nullptr, *faces = chart->faces;

  chart->verts = chart->collapsed_verts = nullptr;
  chart->edges = chart->collapsed_edges = nullptr;
  chart->faces = chart->collapsed_faces = nullptr;

  chart->nverts = chart->nedges = chart->nfaces = 0;

  for (v = verts; v; v = nextv) {
    nextv = v->nextlink;

    if (v->flag & PVERT_COLLAPSE) {
      v->nextlink = chart->collapsed_verts;
      chart->collapsed_verts = v;
    }
    else {
      v->nextlink = chart->verts;
      chart->verts = v;
      chart->nverts++;
    }
  }

  for (e = edges; e; e = nexte) {
    nexte = e->nextlink;

    if (!collapsed || !(e->flag & PEDGE_COLLAPSE_EDGE)) {
      if (e->flag & PEDGE_COLLAPSE) {
        e->nextlink = chart->collapsed_edges;
        chart->collapsed_edges = e;
      }
      else {
        e->nextlink = chart->edges;
        chart->edges = e;
        chart->nedges++;
      }
    }
  }

  /* these are added last so they can be popped of in the right order
   * for splitting */
  for (e = collapsed; e; e = e->nextlink) {
    e->nextlink = e->u.nextcollapse;
    laste = e;
  }
  if (laste) {
    laste->nextlink = chart->collapsed_edges;
    chart->collapsed_edges = collapsed;
  }

  for (f = faces; f; f = nextf) {
    nextf = f->nextlink;

    if (f->flag & PFACE_COLLAPSE) {
      f->nextlink = chart->collapsed_faces;
      chart->collapsed_faces = f;
    }
    else {
      f->nextlink = chart->faces;
      chart->faces = f;
      chart->nfaces++;
    }
  }
}

static void p_chart_simplify_compute(PChart *chart,
                                     std::function<float(PEdge *, PEdge *)> collapse_cost_fn,
                                     std::function<float(PEdge *, PEdge *)> collapse_allowed_fn)
{
  /* For debugging. */
  static const int MAX_SIMPLIFY = INT_MAX;

  /* Computes a list of edge collapses / vertex splits. The collapsed
   * simplices go in the `chart->collapsed_*` lists, The original and
   * collapsed may then be view as stacks, where the next collapse/split
   * is at the top of the respective lists. */

  Heap *heap = BLI_heap_new();
  PEdge *collapsededges = nullptr;
  int ncollapsed = 0;
  Vector<PVert *> wheelverts;
  wheelverts.reserve(16);

  /* insert all potential collapses into heap */
  for (PVert *v = chart->verts; v; v = v->nextlink) {
    float cost;
    PEdge *e = nullptr;

    p_collapse_cost_vertex(v, &cost, &e, collapse_cost_fn, collapse_allowed_fn);

    if (e) {
      v->u.heaplink = BLI_heap_insert(heap, cost, e);
    }
    else {
      v->u.heaplink = nullptr;
    }
  }

  for (PEdge *e = chart->edges; e; e = e->nextlink) {
    e->u.nextcollapse = nullptr;
  }

  /* pop edge collapse out of heap one by one */
  while (!BLI_heap_is_empty(heap)) {
    if (ncollapsed == MAX_SIMPLIFY) {
      break;
    }

    HeapNode *link = BLI_heap_top(heap);
    PEdge *edge = (PEdge *)BLI_heap_pop_min(heap), *pair = edge->pair;
    PVert *oldv, *keepv;
    PEdge *wheele, *nexte;

    /* remember the edges we collapsed */
    edge->u.nextcollapse = collapsededges;
    collapsededges = edge;

    if (edge->vert->u.heaplink != link) {
      edge->flag |= (PEDGE_COLLAPSE_EDGE | PEDGE_COLLAPSE_PAIR);
      edge->next->vert->u.heaplink = nullptr;
      std::swap(edge, pair);
    }
    else {
      edge->flag |= PEDGE_COLLAPSE_EDGE;
      edge->vert->u.heaplink = nullptr;
    }

    p_collapsing_verts(edge, pair, &oldv, &keepv);

    /* gather all wheel verts and remember them before collapse */
    wheelverts.clear();
    wheele = oldv->edge;

    do {
      wheelverts.append(wheele->next->vert);
      nexte = p_wheel_edge_next(wheele);

      if (nexte == nullptr) {
        wheelverts.append(wheele->next->next->vert);
      }

      wheele = nexte;
    } while (wheele && (wheele != oldv->edge));

    /* collapse */
    p_collapse_edge(edge, pair);

    for (PVert *v : wheelverts) {
      float cost;
      PEdge *collapse = nullptr;

      if (v->u.heaplink) {
        BLI_heap_remove(heap, v->u.heaplink);
        v->u.heaplink = nullptr;
      }

      p_collapse_cost_vertex(v, &cost, &collapse, collapse_cost_fn, collapse_allowed_fn);

      if (collapse) {
        v->u.heaplink = BLI_heap_insert(heap, cost, collapse);
      }
    }

    ncollapsed++;
  }

  BLI_heap_free(heap, nullptr);

  p_chart_post_collapse_flush(chart, collapsededges);
}

#if 0
static void p_chart_post_split_flush(PChart *chart)
{
  /* Move from `collapsed_*`. */

  PVert *v, *nextv = nullptr;
  PEdge *e, *nexte = nullptr;
  PFace *f, *nextf = nullptr;

  for (v = chart->collapsed_verts; v; v = nextv) {
    nextv = v->nextlink;
    v->nextlink = chart->verts;
    chart->verts = v;
    chart->nverts++;
  }

  for (e = chart->collapsed_edges; e; e = nexte) {
    nexte = e->nextlink;
    e->nextlink = chart->edges;
    chart->edges = e;
    chart->nedges++;
  }

  for (f = chart->collapsed_faces; f; f = nextf) {
    nextf = f->nextlink;
    f->nextlink = chart->faces;
    chart->faces = f;
    chart->nfaces++;
  }

  chart->collapsed_verts = nullptr;
  chart->collapsed_edges = nullptr;
  chart->collapsed_faces = nullptr;
}

static void p_chart_complexify(PChart *chart)
{
  /* For debugging. */
  static const int MAX_COMPLEXIFY = INT_MAX;

  PEdge *e, *pair, *edge;
  PVert *newv, *keepv;
  int x = 0;

  for (e = chart->collapsed_edges; e; e = e->nextlink) {
    if (!(e->flag & PEDGE_COLLAPSE_EDGE)) {
      break;
    }

    edge = e;
    pair = e->pair;

    if (edge->flag & PEDGE_COLLAPSE_PAIR) {
      std::swap(edge, pair);
    }

    p_split_vertex(edge, pair);
    p_collapsing_verts(edge, pair, &newv, &keepv);

    if (x >= MAX_COMPLEXIFY) {
      newv->uv[0] = keepv->uv[0];
      newv->uv[1] = keepv->uv[1];
    }
    else {
      p_vert_harmonic_insert(newv);
      x++;
    }
  }

  p_chart_post_split_flush(chart);
}

static void p_chart_simplify(PChart *chart)
{
  /* Not implemented, needs proper reordering in split_flush. */
}
#endif

/* ABF */

#define ABF_MAX_ITER 20

struct PAbfSystem {
  int ninterior, nfaces, nangles;
  float *alpha, *beta, *sine, *cosine, *weight;
  float *bAlpha, *bTriangle, *bInterior;
  float *lambdaTriangle, *lambdaPlanar, *lambdaLength;
  float (*J2dt)[3], *bstar, *dstar;
};

static void p_abf_setup_system(PAbfSystem *sys)
{
  int i;

  sys->alpha = MEM_malloc_arrayN<float>(size_t(sys->nangles), "ABFalpha");
  sys->beta = MEM_malloc_arrayN<float>(size_t(sys->nangles), "ABFbeta");
  sys->sine = MEM_malloc_arrayN<float>(size_t(sys->nangles), "ABFsine");
  sys->cosine = MEM_malloc_arrayN<float>(size_t(sys->nangles), "ABFcosine");
  sys->weight = MEM_malloc_arrayN<float>(size_t(sys->nangles), "ABFweight");

  sys->bAlpha = MEM_malloc_arrayN<float>(size_t(sys->nangles), "ABFbalpha");
  sys->bTriangle = MEM_malloc_arrayN<float>(size_t(sys->nfaces), "ABFbtriangle");
  sys->bInterior = MEM_malloc_arrayN<float>(2 * size_t(sys->ninterior), "ABFbinterior");

  sys->lambdaTriangle = MEM_calloc_arrayN<float>(sys->nfaces, "ABFlambdatri");
  sys->lambdaPlanar = MEM_calloc_arrayN<float>(sys->ninterior, "ABFlamdaplane");
  sys->lambdaLength = MEM_malloc_arrayN<float>(sys->ninterior, "ABFlambdalen");

  sys->J2dt = MEM_malloc_arrayN<float[3]>(size_t(sys->nangles), "ABFj2dt");
  sys->bstar = MEM_malloc_arrayN<float>(size_t(sys->nfaces), "ABFbstar");
  sys->dstar = MEM_malloc_arrayN<float>(size_t(sys->nfaces), "ABFdstar");

  for (i = 0; i < sys->ninterior; i++) {
    sys->lambdaLength[i] = 1.0;
  }
}

static void p_abf_free_system(PAbfSystem *sys)
{
  MEM_freeN(sys->alpha);
  MEM_freeN(sys->beta);
  MEM_freeN(sys->sine);
  MEM_freeN(sys->cosine);
  MEM_freeN(sys->weight);
  MEM_freeN(sys->bAlpha);
  MEM_freeN(sys->bTriangle);
  MEM_freeN(sys->bInterior);
  MEM_freeN(sys->lambdaTriangle);
  MEM_freeN(sys->lambdaPlanar);
  MEM_freeN(sys->lambdaLength);
  MEM_freeN(sys->J2dt);
  MEM_freeN(sys->bstar);
  MEM_freeN(sys->dstar);
}

static void p_abf_compute_sines(PAbfSystem *sys)
{
  float *sine = sys->sine;
  float *cosine = sys->cosine;
  const float *alpha = sys->alpha;

  for (int i = 0; i < sys->nangles; i++) {
    const double angle = alpha[i];
    sine[i] = sin(angle);
    cosine[i] = cos(angle);
  }
}

static float p_abf_compute_sin_product(PAbfSystem *sys, PVert *v, int aid)
{
  PEdge *e, *e1, *e2;
  float sin1, sin2;

  sin1 = sin2 = 1.0;

  e = v->edge;
  do {
    e1 = e->next;
    e2 = e->next->next;

    if (aid == e1->u.id) {
      /* we are computing a derivative for this angle,
       * so we use cos and drop the other part */
      sin1 *= sys->cosine[e1->u.id];
      sin2 = 0.0;
    }
    else {
      sin1 *= sys->sine[e1->u.id];
    }

    if (aid == e2->u.id) {
      /* see above */
      sin1 = 0.0;
      sin2 *= sys->cosine[e2->u.id];
    }
    else {
      sin2 *= sys->sine[e2->u.id];
    }

    e = e->next->next->pair;
  } while (e && (e != v->edge));

  return (sin1 - sin2);
}

static float p_abf_compute_grad_alpha(PAbfSystem *sys, PFace *f, PEdge *e)
{
  PVert *v = e->vert, *v1 = e->next->vert, *v2 = e->next->next->vert;
  float deriv;

  deriv = (sys->alpha[e->u.id] - sys->beta[e->u.id]) * sys->weight[e->u.id];
  deriv += sys->lambdaTriangle[f->u.id];

  if (v->flag & PVERT_INTERIOR) {
    deriv += sys->lambdaPlanar[v->u.id];
  }

  if (v1->flag & PVERT_INTERIOR) {
    float product = p_abf_compute_sin_product(sys, v1, e->u.id);
    deriv += sys->lambdaLength[v1->u.id] * product;
  }

  if (v2->flag & PVERT_INTERIOR) {
    float product = p_abf_compute_sin_product(sys, v2, e->u.id);
    deriv += sys->lambdaLength[v2->u.id] * product;
  }

  return deriv;
}

static float p_abf_compute_gradient(PAbfSystem *sys, PChart *chart)
{
  PFace *f;
  PEdge *e;
  PVert *v;
  float norm = 0.0;

  for (f = chart->faces; f; f = f->nextlink) {
    PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;
    float gtriangle, galpha1, galpha2, galpha3;

    galpha1 = p_abf_compute_grad_alpha(sys, f, e1);
    galpha2 = p_abf_compute_grad_alpha(sys, f, e2);
    galpha3 = p_abf_compute_grad_alpha(sys, f, e3);

    sys->bAlpha[e1->u.id] = -galpha1;
    sys->bAlpha[e2->u.id] = -galpha2;
    sys->bAlpha[e3->u.id] = -galpha3;

    norm += galpha1 * galpha1 + galpha2 * galpha2 + galpha3 * galpha3;

    gtriangle = sys->alpha[e1->u.id] + sys->alpha[e2->u.id] + sys->alpha[e3->u.id] - float(M_PI);
    sys->bTriangle[f->u.id] = -gtriangle;
    norm += gtriangle * gtriangle;
  }

  for (v = chart->verts; v; v = v->nextlink) {
    if (v->flag & PVERT_INTERIOR) {
      float gplanar = -2 * M_PI, glength;

      e = v->edge;
      do {
        gplanar += sys->alpha[e->u.id];
        e = e->next->next->pair;
      } while (e && (e != v->edge));

      sys->bInterior[v->u.id] = -gplanar;
      norm += gplanar * gplanar;

      glength = p_abf_compute_sin_product(sys, v, -1);
      sys->bInterior[sys->ninterior + v->u.id] = -glength;
      norm += glength * glength;
    }
  }

  return norm;
}

static void p_abf_adjust_alpha(PAbfSystem *sys,
                               const int id,
                               const float dlambda1,
                               const float pre)
{
  float alpha = sys->alpha[id];
  const float dalpha = (sys->bAlpha[id] - dlambda1);
  alpha += dalpha / sys->weight[id] - pre;
  sys->alpha[id] = clamp_f(alpha, 0.0f, float(M_PI));
}

static bool p_abf_matrix_invert(PAbfSystem *sys, PChart *chart)
{
  int ninterior = sys->ninterior;
  int nvar = 2 * ninterior;
  LinearSolver *context = EIG_linear_solver_new(0, nvar, 1);

  for (int i = 0; i < nvar; i++) {
    EIG_linear_solver_right_hand_side_add(context, 0, i, sys->bInterior[i]);
  }

  for (PFace *f = chart->faces; f; f = f->nextlink) {
    float wi1, wi2, wi3, b, si, beta[3], j2[3][3], W[3][3];
    float row1[6], row2[6], row3[6];
    int vid[6];
    PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;
    PVert *v1 = e1->vert, *v2 = e2->vert, *v3 = e3->vert;

    wi1 = 1.0f / sys->weight[e1->u.id];
    wi2 = 1.0f / sys->weight[e2->u.id];
    wi3 = 1.0f / sys->weight[e3->u.id];

    /* bstar1 = (J1*dInv*bAlpha - bTriangle) */
    b = sys->bAlpha[e1->u.id] * wi1;
    b += sys->bAlpha[e2->u.id] * wi2;
    b += sys->bAlpha[e3->u.id] * wi3;
    b -= sys->bTriangle[f->u.id];

    /* si = J1*d*J1t */
    si = 1.0f / (wi1 + wi2 + wi3);

    /* J1t*si*bstar1 - bAlpha */
    beta[0] = b * si - sys->bAlpha[e1->u.id];
    beta[1] = b * si - sys->bAlpha[e2->u.id];
    beta[2] = b * si - sys->bAlpha[e3->u.id];

    /* use this later for computing other lambda's */
    sys->bstar[f->u.id] = b;
    sys->dstar[f->u.id] = si;

    /* set matrix */
    W[0][0] = si - sys->weight[e1->u.id];
    W[0][1] = si;
    W[0][2] = si;
    W[1][0] = si;
    W[1][1] = si - sys->weight[e2->u.id];
    W[1][2] = si;
    W[2][0] = si;
    W[2][1] = si;
    W[2][2] = si - sys->weight[e3->u.id];

    vid[0] = vid[1] = vid[2] = vid[3] = vid[4] = vid[5] = -1;

    if (v1->flag & PVERT_INTERIOR) {
      vid[0] = v1->u.id;
      vid[3] = ninterior + v1->u.id;

      sys->J2dt[e1->u.id][0] = j2[0][0] = 1.0f * wi1;
      sys->J2dt[e2->u.id][0] = j2[1][0] = p_abf_compute_sin_product(sys, v1, e2->u.id) * wi2;
      sys->J2dt[e3->u.id][0] = j2[2][0] = p_abf_compute_sin_product(sys, v1, e3->u.id) * wi3;

      EIG_linear_solver_right_hand_side_add(context, 0, v1->u.id, j2[0][0] * beta[0]);
      EIG_linear_solver_right_hand_side_add(
          context, 0, ninterior + v1->u.id, j2[1][0] * beta[1] + j2[2][0] * beta[2]);

      row1[0] = j2[0][0] * W[0][0];
      row2[0] = j2[0][0] * W[1][0];
      row3[0] = j2[0][0] * W[2][0];

      row1[3] = j2[1][0] * W[0][1] + j2[2][0] * W[0][2];
      row2[3] = j2[1][0] * W[1][1] + j2[2][0] * W[1][2];
      row3[3] = j2[1][0] * W[2][1] + j2[2][0] * W[2][2];
    }

    if (v2->flag & PVERT_INTERIOR) {
      vid[1] = v2->u.id;
      vid[4] = ninterior + v2->u.id;

      sys->J2dt[e1->u.id][1] = j2[0][1] = p_abf_compute_sin_product(sys, v2, e1->u.id) * wi1;
      sys->J2dt[e2->u.id][1] = j2[1][1] = 1.0f * wi2;
      sys->J2dt[e3->u.id][1] = j2[2][1] = p_abf_compute_sin_product(sys, v2, e3->u.id) * wi3;

      EIG_linear_solver_right_hand_side_add(context, 0, v2->u.id, j2[1][1] * beta[1]);
      EIG_linear_solver_right_hand_side_add(
          context, 0, ninterior + v2->u.id, j2[0][1] * beta[0] + j2[2][1] * beta[2]);

      row1[1] = j2[1][1] * W[0][1];
      row2[1] = j2[1][1] * W[1][1];
      row3[1] = j2[1][1] * W[2][1];

      row1[4] = j2[0][1] * W[0][0] + j2[2][1] * W[0][2];
      row2[4] = j2[0][1] * W[1][0] + j2[2][1] * W[1][2];
      row3[4] = j2[0][1] * W[2][0] + j2[2][1] * W[2][2];
    }

    if (v3->flag & PVERT_INTERIOR) {
      vid[2] = v3->u.id;
      vid[5] = ninterior + v3->u.id;

      sys->J2dt[e1->u.id][2] = j2[0][2] = p_abf_compute_sin_product(sys, v3, e1->u.id) * wi1;
      sys->J2dt[e2->u.id][2] = j2[1][2] = p_abf_compute_sin_product(sys, v3, e2->u.id) * wi2;
      sys->J2dt[e3->u.id][2] = j2[2][2] = 1.0f * wi3;

      EIG_linear_solver_right_hand_side_add(context, 0, v3->u.id, j2[2][2] * beta[2]);
      EIG_linear_solver_right_hand_side_add(
          context, 0, ninterior + v3->u.id, j2[0][2] * beta[0] + j2[1][2] * beta[1]);

      row1[2] = j2[2][2] * W[0][2];
      row2[2] = j2[2][2] * W[1][2];
      row3[2] = j2[2][2] * W[2][2];

      row1[5] = j2[0][2] * W[0][0] + j2[1][2] * W[0][1];
      row2[5] = j2[0][2] * W[1][0] + j2[1][2] * W[1][1];
      row3[5] = j2[0][2] * W[2][0] + j2[1][2] * W[2][1];
    }

    for (int i = 0; i < 3; i++) {
      int r = vid[i];

      if (r == -1) {
        continue;
      }

      for (int j = 0; j < 6; j++) {
        int c = vid[j];

        if (c == -1) {
          continue;
        }

        if (i == 0) {
          EIG_linear_solver_matrix_add(context, r, c, j2[0][i] * row1[j]);
        }
        else {
          EIG_linear_solver_matrix_add(context, r + ninterior, c, j2[0][i] * row1[j]);
        }

        if (i == 1) {
          EIG_linear_solver_matrix_add(context, r, c, j2[1][i] * row2[j]);
        }
        else {
          EIG_linear_solver_matrix_add(context, r + ninterior, c, j2[1][i] * row2[j]);
        }

        if (i == 2) {
          EIG_linear_solver_matrix_add(context, r, c, j2[2][i] * row3[j]);
        }
        else {
          EIG_linear_solver_matrix_add(context, r + ninterior, c, j2[2][i] * row3[j]);
        }
      }
    }
  }

  bool success = EIG_linear_solver_solve(context);

  if (success) {
    for (PFace *f = chart->faces; f; f = f->nextlink) {
      float pre[3];
      PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;
      PVert *v1 = e1->vert, *v2 = e2->vert, *v3 = e3->vert;

      pre[0] = pre[1] = pre[2] = 0.0f;

      if (v1->flag & PVERT_INTERIOR) {
        float x = EIG_linear_solver_variable_get(context, 0, v1->u.id);
        float x2 = EIG_linear_solver_variable_get(context, 0, ninterior + v1->u.id);
        pre[0] += sys->J2dt[e1->u.id][0] * x;
        pre[1] += sys->J2dt[e2->u.id][0] * x2;
        pre[2] += sys->J2dt[e3->u.id][0] * x2;
      }

      if (v2->flag & PVERT_INTERIOR) {
        float x = EIG_linear_solver_variable_get(context, 0, v2->u.id);
        float x2 = EIG_linear_solver_variable_get(context, 0, ninterior + v2->u.id);
        pre[0] += sys->J2dt[e1->u.id][1] * x2;
        pre[1] += sys->J2dt[e2->u.id][1] * x;
        pre[2] += sys->J2dt[e3->u.id][1] * x2;
      }

      if (v3->flag & PVERT_INTERIOR) {
        float x = EIG_linear_solver_variable_get(context, 0, v3->u.id);
        float x2 = EIG_linear_solver_variable_get(context, 0, ninterior + v3->u.id);
        pre[0] += sys->J2dt[e1->u.id][2] * x2;
        pre[1] += sys->J2dt[e2->u.id][2] * x2;
        pre[2] += sys->J2dt[e3->u.id][2] * x;
      }

      float dlambda1 = pre[0] + pre[1] + pre[2];
      dlambda1 = sys->dstar[f->u.id] * (sys->bstar[f->u.id] - dlambda1);

      sys->lambdaTriangle[f->u.id] += dlambda1;

      p_abf_adjust_alpha(sys, e1->u.id, dlambda1, pre[0]);
      p_abf_adjust_alpha(sys, e2->u.id, dlambda1, pre[1]);
      p_abf_adjust_alpha(sys, e3->u.id, dlambda1, pre[2]);
    }

    for (int i = 0; i < ninterior; i++) {
      sys->lambdaPlanar[i] += float(EIG_linear_solver_variable_get(context, 0, i));
      sys->lambdaLength[i] += float(EIG_linear_solver_variable_get(context, 0, ninterior + i));
    }
  }

  EIG_linear_solver_delete(context);

  return success;
}

static bool p_chart_abf_solve(PChart *chart)
{
  PVert *v;
  PFace *f;
  PEdge *e, *e1, *e2, *e3;
  PAbfSystem sys;
  int i;
  float /* lastnorm, */ /* UNUSED */ limit = (chart->nfaces > 100) ? 1.0f : 0.001f;

  /* setup id's */
  sys.ninterior = sys.nfaces = sys.nangles = 0;

  for (v = chart->verts; v; v = v->nextlink) {
    if (p_vert_interior(v)) {
      v->flag |= PVERT_INTERIOR;
      v->u.id = sys.ninterior++;
    }
    else {
      v->flag &= ~PVERT_INTERIOR;
    }
  }

  for (f = chart->faces; f; f = f->nextlink) {
    e1 = f->edge;
    e2 = e1->next;
    e3 = e2->next;
    f->u.id = sys.nfaces++;

    /* angle id's are conveniently stored in half edges */
    e1->u.id = sys.nangles++;
    e2->u.id = sys.nangles++;
    e3->u.id = sys.nangles++;
  }

  p_abf_setup_system(&sys);

  /* compute initial angles */
  for (f = chart->faces; f; f = f->nextlink) {
    double a1, a2, a3;

    e1 = f->edge;
    e2 = e1->next;
    e3 = e2->next;
    p_face_angles(f, &a1, &a2, &a3);

    sys.alpha[e1->u.id] = sys.beta[e1->u.id] = a1;
    sys.alpha[e2->u.id] = sys.beta[e2->u.id] = a2;
    sys.alpha[e3->u.id] = sys.beta[e3->u.id] = a3;

    sys.weight[e1->u.id] = 2.0f / (a1 * a1);
    sys.weight[e2->u.id] = 2.0f / (a2 * a2);
    sys.weight[e3->u.id] = 2.0f / (a3 * a3);
  }

  for (v = chart->verts; v; v = v->nextlink) {
    if (v->flag & PVERT_INTERIOR) {
      float anglesum = 0.0, scale;

      e = v->edge;
      do {
        anglesum += sys.beta[e->u.id];
        e = e->next->next->pair;
      } while (e && (e != v->edge));

      scale = (anglesum == 0.0f) ? 0.0f : 2.0f * float(M_PI) / anglesum;

      e = v->edge;
      do {
        sys.beta[e->u.id] = sys.alpha[e->u.id] = sys.beta[e->u.id] * scale;
        e = e->next->next->pair;
      } while (e && (e != v->edge));
    }
  }

  if (sys.ninterior > 0) {
    p_abf_compute_sines(&sys);

    /* iteration */
    // lastnorm = 1e10; /* UNUSED. */

    for (i = 0; i < ABF_MAX_ITER; i++) {
      float norm = p_abf_compute_gradient(&sys, chart);

      // lastnorm = norm; /* UNUSED. */

      if (norm < limit) {
        break;
      }

      if (!p_abf_matrix_invert(&sys, chart)) {
        param_warning("ABF failed to invert matrix");
        p_abf_free_system(&sys);
        return false;
      }

      p_abf_compute_sines(&sys);
    }

    if (i == ABF_MAX_ITER) {
      param_warning("ABF maximum iterations reached");
      p_abf_free_system(&sys);
      return false;
    }
  }

  chart->abf_alpha = (float *)MEM_dupallocN(sys.alpha);
  p_abf_free_system(&sys);

  return true;
}

/* Least Squares Conformal Maps */

static void p_chart_pin_positions(PChart *chart, PVert **pin1, PVert **pin2)
{
  if (!*pin1 || !*pin2 || *pin1 == *pin2) {
    /* degenerate case */
    PFace *f = chart->faces;
    *pin1 = f->edge->vert;
    *pin2 = f->edge->next->vert;

    (*pin1)->uv[0] = 0.0f;
    (*pin1)->uv[1] = 0.5f;
    (*pin2)->uv[0] = 1.0f;
    (*pin2)->uv[1] = 0.5f;
  }
  else {
    int diru, dirv, dirx, diry;
    float sub[3];

    sub_v3_v3v3(sub, (*pin1)->co, (*pin2)->co);
    sub[0] = fabsf(sub[0]);
    sub[1] = fabsf(sub[1]);
    sub[2] = fabsf(sub[2]);

    if ((sub[0] > sub[1]) && (sub[0] > sub[2])) {
      dirx = 0;
      diry = (sub[1] > sub[2]) ? 1 : 2;
    }
    else if ((sub[1] > sub[0]) && (sub[1] > sub[2])) {
      dirx = 1;
      diry = (sub[0] > sub[2]) ? 0 : 2;
    }
    else {
      dirx = 2;
      diry = (sub[0] > sub[1]) ? 0 : 1;
    }

    if (dirx == 2) {
      diru = 1;
      dirv = 0;
    }
    else {
      diru = 0;
      dirv = 1;
    }

    (*pin1)->uv[diru] = (*pin1)->co[dirx];
    (*pin1)->uv[dirv] = (*pin1)->co[diry];
    (*pin2)->uv[diru] = (*pin2)->co[dirx];
    (*pin2)->uv[dirv] = (*pin2)->co[diry];
  }
}

static bool p_chart_symmetry_pins(PChart *chart, PEdge *outer, PVert **pin1, PVert **pin2)
{
  PEdge *be, *lastbe = nullptr, *maxe1 = nullptr, *maxe2 = nullptr, *be1, *be2;
  PEdge *cure = nullptr, *firste1 = nullptr, *firste2 = nullptr, *nextbe;
  float maxlen = 0.0f, curlen = 0.0f, totlen = 0.0f, firstlen = 0.0f;
  float len1, len2;

  /* find longest series of verts split in the chart itself, these are
   * marked during construction */
  be = outer;
  lastbe = p_boundary_edge_prev(be);
  do {
    float len = p_edge_length(be);
    totlen += len;

    nextbe = p_boundary_edge_next(be);

    if ((be->vert->flag & PVERT_SPLIT) || (lastbe->vert->flag & nextbe->vert->flag & PVERT_SPLIT))
    {
      if (!cure) {
        if (be == outer) {
          firste1 = be;
        }
        cure = be;
      }
      else {
        curlen += p_edge_length(lastbe);
      }
    }
    else if (cure) {
      if (curlen > maxlen) {
        maxlen = curlen;
        maxe1 = cure;
        maxe2 = lastbe;
      }

      if (firste1 == cure) {
        firstlen = curlen;
        firste2 = lastbe;
      }

      curlen = 0.0f;
      cure = nullptr;
    }

    lastbe = be;
    be = nextbe;
  } while (be != outer);

  /* make sure we also count a series of splits over the starting point */
  if (cure && (cure != outer)) {
    firstlen += curlen + p_edge_length(be);

    if (firstlen > maxlen) {
      maxlen = firstlen;
      maxe1 = cure;
      maxe2 = firste2;
    }
  }

  if (!maxe1 || !maxe2 || (maxlen < 0.5f * totlen)) {
    return false;
  }

  /* find pin1 in the split vertices */
  be1 = maxe1;
  be2 = maxe2;
  len1 = 0.0f;
  len2 = 0.0f;

  do {
    if (len1 < len2) {
      len1 += p_edge_length(be1);
      be1 = p_boundary_edge_next(be1);
    }
    else {
      be2 = p_boundary_edge_prev(be2);
      len2 += p_edge_length(be2);
    }
  } while (be1 != be2);

  *pin1 = be1->vert;

  /* find pin2 outside the split vertices */
  be1 = maxe1;
  be2 = maxe2;
  len1 = 0.0f;
  len2 = 0.0f;

  do {
    if (len1 < len2) {
      be1 = p_boundary_edge_prev(be1);
      len1 += p_edge_length(be1);
    }
    else {
      len2 += p_edge_length(be2);
      be2 = p_boundary_edge_next(be2);
    }
  } while (be1 != be2);

  *pin2 = be1->vert;

  p_chart_pin_positions(chart, pin1, pin2);

  return !equals_v3v3((*pin1)->co, (*pin2)->co);
}

static void p_chart_extrema_verts(PChart *chart, PVert **pin1, PVert **pin2)
{
  float minv[3], maxv[3], dirlen;
  PVert *v, *minvert[3], *maxvert[3];
  int i, dir;

  /* find minimum and maximum verts over x/y/z axes */
  minv[0] = minv[1] = minv[2] = 1e20;
  maxv[0] = maxv[1] = maxv[2] = -1e20;

  minvert[0] = minvert[1] = minvert[2] = nullptr;
  maxvert[0] = maxvert[1] = maxvert[2] = nullptr;

  for (v = chart->verts; v; v = v->nextlink) {
    for (i = 0; i < 3; i++) {
      if (v->co[i] < minv[i]) {
        minv[i] = v->co[i];
        minvert[i] = v;
      }
      if (v->co[i] > maxv[i]) {
        maxv[i] = v->co[i];
        maxvert[i] = v;
      }
    }
  }

  /* find axes with longest distance */
  dir = 0;
  dirlen = -1.0;

  for (i = 0; i < 3; i++) {
    if (maxv[i] - minv[i] > dirlen) {
      dir = i;
      dirlen = maxv[i] - minv[i];
    }
  }

  *pin1 = minvert[dir];
  *pin2 = maxvert[dir];

  p_chart_pin_positions(chart, pin1, pin2);
}

static void p_chart_lscm_begin(PChart *chart, bool live, bool abf)
{
  BLI_assert(chart->context == nullptr);

  bool select = false;
  bool deselect = false;
  int npins = 0;

  /* Give vertices matrix indices, count pins and check selections. */
  for (PVert *v = chart->verts; v; v = v->nextlink) {
    if (v->flag & PVERT_PIN) {
      npins++;
      if (v->flag & PVERT_SELECT) {
        select = true;
      }
    }

    if (!(v->flag & PVERT_SELECT)) {
      deselect = true;
    }
  }

  if (live && (!select || !deselect)) {
    chart->skip_flush = true;
    return;
  }
#if 0
  p_chart_simplify_compute(chart, p_collapse_cost, p_collapse_allowed);
  p_chart_topological_sanity_check(chart);
#endif

  if (npins == 1) {
    chart->area_uv = p_chart_uv_area(chart);
    for (PVert *v = chart->verts; v; v = v->nextlink) {
      if (v->flag & PVERT_PIN) {
        chart->single_pin = v;
        break;
      }
    }
  }

  if (abf) {
    if (!p_chart_abf_solve(chart)) {
      param_warning("ABF solving failed: falling back to LSCM.\n");
    }
  }

  /* ABF uses these indices for it's internal references.
   * Set the indices afterwards. */
  int id = 0;
  for (PVert *v = chart->verts; v; v = v->nextlink) {
    v->u.id = id++;
  }

  if (npins <= 1) {
    /* No pins, let's find some ourself. */
    PEdge *outer;
    p_chart_boundaries(chart, &outer);

    PVert *pin1, *pin2;
    /* Outer can be null with non-finite coordinates. */
    if (!(outer && p_chart_symmetry_pins(chart, outer, &pin1, &pin2))) {
      p_chart_extrema_verts(chart, &pin1, &pin2);
    }

    chart->pin1 = pin1;
    chart->pin2 = pin2;
  }

  chart->context = EIG_linear_least_squares_solver_new(2 * chart->nfaces, 2 * chart->nverts, 1);
}

static bool p_chart_lscm_solve(ParamHandle *handle, PChart *chart)
{
  LinearSolver *context = chart->context;

  for (PVert *v = chart->verts; v; v = v->nextlink) {
    if (v->flag & PVERT_PIN) {
      p_vert_load_pin_select_uvs(handle, v); /* Reload for Live Unwrap. */
    }
  }

  if (chart->single_pin) {
    /* If only one pin, save location as origin. */
    copy_v2_v2(chart->origin, chart->single_pin->uv);
  }

  if (chart->pin1) {
    PVert *pin1 = chart->pin1;
    PVert *pin2 = chart->pin2;
    EIG_linear_solver_variable_lock(context, 2 * pin1->u.id);
    EIG_linear_solver_variable_lock(context, 2 * pin1->u.id + 1);
    EIG_linear_solver_variable_lock(context, 2 * pin2->u.id);
    EIG_linear_solver_variable_lock(context, 2 * pin2->u.id + 1);

    EIG_linear_solver_variable_set(context, 0, 2 * pin1->u.id, pin1->uv[0]);
    EIG_linear_solver_variable_set(context, 0, 2 * pin1->u.id + 1, pin1->uv[1]);
    EIG_linear_solver_variable_set(context, 0, 2 * pin2->u.id, pin2->uv[0]);
    EIG_linear_solver_variable_set(context, 0, 2 * pin2->u.id + 1, pin2->uv[1]);
  }
  else {
    /* Set and lock the pins. */
    for (PVert *v = chart->verts; v; v = v->nextlink) {
      if (v->flag & PVERT_PIN) {
        EIG_linear_solver_variable_lock(context, 2 * v->u.id);
        EIG_linear_solver_variable_lock(context, 2 * v->u.id + 1);

        EIG_linear_solver_variable_set(context, 0, 2 * v->u.id, v->uv[0]);
        EIG_linear_solver_variable_set(context, 0, 2 * v->u.id + 1, v->uv[1]);
      }
    }
  }

  /* Detect "up" direction based on pinned vertices. */
  float area_pinned_up = 0.0f;
  float area_pinned_down = 0.0f;

  for (PFace *f = chart->faces; f; f = f->nextlink) {
    PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;
    PVert *v1 = e1->vert, *v2 = e2->vert, *v3 = e3->vert;

    if ((v1->flag & PVERT_PIN) && (v2->flag & PVERT_PIN) && (v3->flag & PVERT_PIN)) {
      const float area = p_face_uv_area_signed(f);

      if (area > 0.0f) {
        area_pinned_up += area;
      }
      else {
        area_pinned_down -= area;
      }
    }
  }

  const bool flip_faces = (area_pinned_down > area_pinned_up);

  /* Construct matrix. */
  const float *alpha = chart->abf_alpha;
  int row = 0;
  for (PFace *f = chart->faces; f; f = f->nextlink) {
    PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;
    PVert *v1 = e1->vert, *v2 = e2->vert, *v3 = e3->vert;
    double a1, a2, a3;

    if (alpha) {
      /* Use abf angles if present. */
      a1 = *(alpha++);
      a2 = *(alpha++);
      a3 = *(alpha++);
    }
    else {
      p_face_angles(f, &a1, &a2, &a3);
    }

    if (flip_faces) {
      std::swap(a2, a3);
      std::swap(e2, e3);
      std::swap(v2, v3);
    }

    double sina1 = sin(a1);
    double sina2 = sin(a2);
    double sina3 = sin(a3);

    const double sinmax = max_ddd(sina1, sina2, sina3);

    /* Shift vertices to find most stable order. */
    if (sina3 != sinmax) {
      SHIFT3(PVert *, v1, v2, v3);
      SHIFT3(double, a1, a2, a3);
      SHIFT3(double, sina1, sina2, sina3);

      if (sina2 == sinmax) {
        SHIFT3(PVert *, v1, v2, v3);
        SHIFT3(double, a1, a2, a3);
        SHIFT3(double, sina1, sina2, sina3);
      }
    }

    /* Angle based lscm formulation. */
    const double ratio = (sina3 == 0.0f) ? 1.0f : sina2 / sina3;
    const double cosine = cos(a1) * ratio;
    const double sine = sina1 * ratio;

    EIG_linear_solver_matrix_add(context, row, 2 * v1->u.id, cosine - 1.0f);
    EIG_linear_solver_matrix_add(context, row, 2 * v1->u.id + 1, -sine);
    EIG_linear_solver_matrix_add(context, row, 2 * v2->u.id, -cosine);
    EIG_linear_solver_matrix_add(context, row, 2 * v2->u.id + 1, sine);
    EIG_linear_solver_matrix_add(context, row, 2 * v3->u.id, 1.0);
    row++;

    EIG_linear_solver_matrix_add(context, row, 2 * v1->u.id, sine);
    EIG_linear_solver_matrix_add(context, row, 2 * v1->u.id + 1, cosine - 1.0f);
    EIG_linear_solver_matrix_add(context, row, 2 * v2->u.id, -sine);
    EIG_linear_solver_matrix_add(context, row, 2 * v2->u.id + 1, -cosine);
    EIG_linear_solver_matrix_add(context, row, 2 * v3->u.id + 1, 1.0);
    row++;
  }

  if (EIG_linear_solver_solve(context)) {
    for (PVert *v = chart->verts; v; v = v->nextlink) {
      v->uv[0] = EIG_linear_solver_variable_get(context, 0, 2 * v->u.id);
      v->uv[1] = EIG_linear_solver_variable_get(context, 0, 2 * v->u.id + 1);
    }
    return true;
  }

  for (PVert *v = chart->verts; v; v = v->nextlink) {
    v->uv[0] = 0.0f;
    v->uv[1] = 0.0f;
  }

  return false;
}

static void p_chart_lscm_transform_single_pin(PChart *chart)
{
  PVert *pin = chart->single_pin;

  /* If only one pin, keep UV area the same. */
  const float new_area = p_chart_uv_area(chart);
  if (new_area > 0.0f) {
    const float scale = chart->area_uv / new_area;
    if (scale > 0.0f) {
      p_chart_uv_scale(chart, sqrtf(scale));
    }
  }

  /* Translate to keep the pinned UV in place. */
  float offset[2];
  sub_v2_v2v2(offset, chart->origin, pin->uv);
  p_chart_uv_translate(chart, offset);
}

static void p_chart_lscm_end(PChart *chart)
{
  EIG_linear_solver_delete(chart->context);
  chart->context = nullptr;

  MEM_SAFE_FREE(chart->abf_alpha);

  chart->pin1 = nullptr;
  chart->pin2 = nullptr;
  chart->single_pin = nullptr;
}

/* Stretch */

#define P_STRETCH_ITER 20

static void p_stretch_pin_boundary(PChart *chart)
{
  PVert *v;

  for (v = chart->verts; v; v = v->nextlink) {
    if (v->edge->pair == nullptr) {
      v->flag |= PVERT_PIN;
    }
    else {
      v->flag &= ~PVERT_PIN;
    }
  }
}

static float p_face_stretch(PFace *f)
{
  float T, w, tmp[3];
  float Ps[3], Pt[3];
  float a, c, area;
  PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;
  PVert *v1 = e1->vert, *v2 = e2->vert, *v3 = e3->vert;

  area = p_face_uv_area_signed(f);

  if (area <= 0.0f) {
    /* When a face is flipped, provide a large penalty.
     * Add on a slight gradient to unflip the face, see also: #99781. */
    return 1e8f * (1.0f + p_edge_uv_length(e1) + p_edge_uv_length(e2) + p_edge_uv_length(e3));
  }

  w = 1.0f / (2.0f * area);

  /* compute derivatives */
  copy_v3_v3(Ps, v1->co);
  mul_v3_fl(Ps, (v2->uv[1] - v3->uv[1]));

  copy_v3_v3(tmp, v2->co);
  mul_v3_fl(tmp, (v3->uv[1] - v1->uv[1]));
  add_v3_v3(Ps, tmp);

  copy_v3_v3(tmp, v3->co);
  mul_v3_fl(tmp, (v1->uv[1] - v2->uv[1]));
  add_v3_v3(Ps, tmp);

  mul_v3_fl(Ps, w);

  copy_v3_v3(Pt, v1->co);
  mul_v3_fl(Pt, (v3->uv[0] - v2->uv[0]));

  copy_v3_v3(tmp, v2->co);
  mul_v3_fl(tmp, (v1->uv[0] - v3->uv[0]));
  add_v3_v3(Pt, tmp);

  copy_v3_v3(tmp, v3->co);
  mul_v3_fl(tmp, (v2->uv[0] - v1->uv[0]));
  add_v3_v3(Pt, tmp);

  mul_v3_fl(Pt, w);

  /* Sander Tensor */
  a = dot_v3v3(Ps, Ps);
  c = dot_v3v3(Pt, Pt);

  T = sqrtf(0.5f * (a + c));
  if (f->flag & PFACE_FILLED) {
    T *= 0.2f;
  }

  return T;
}

static float p_stretch_compute_vertex(PVert *v)
{
  PEdge *e = v->edge;
  float sum = 0.0f;

  do {
    sum += p_face_stretch(e->face);
    e = p_wheel_edge_next(e);
  } while (e && e != (v->edge));

  return sum;
}

static void p_chart_stretch_minimize(PChart *chart, RNG *rng)
{
  PVert *v;
  PEdge *e;
  int j, nedges;
  float orig_stretch, low, stretch_low, high, stretch_high, mid, stretch;
  float orig_uv[2], dir[2], random_angle, trusted_radius;

  for (v = chart->verts; v; v = v->nextlink) {
    if ((v->flag & PVERT_PIN) || !(v->flag & PVERT_SELECT)) {
      continue;
    }

    orig_stretch = p_stretch_compute_vertex(v);
    orig_uv[0] = v->uv[0];
    orig_uv[1] = v->uv[1];

    /* move vertex in a random direction */
    trusted_radius = 0.0f;
    nedges = 0;
    e = v->edge;

    do {
      trusted_radius += p_edge_uv_length(e);
      nedges++;

      e = p_wheel_edge_next(e);
    } while (e && e != (v->edge));

    trusted_radius /= 2 * nedges;

    random_angle = BLI_rng_get_float(rng) * 2.0f * float(M_PI);
    dir[0] = trusted_radius * cosf(random_angle);
    dir[1] = trusted_radius * sinf(random_angle);

    /* calculate old and new stretch */
    low = 0;
    stretch_low = orig_stretch;

    add_v2_v2v2(v->uv, orig_uv, dir);
    high = 1;
    stretch = stretch_high = p_stretch_compute_vertex(v);

    /* binary search for lowest stretch position */
    for (j = 0; j < P_STRETCH_ITER; j++) {
      mid = 0.5f * (low + high);
      v->uv[0] = orig_uv[0] + mid * dir[0];
      v->uv[1] = orig_uv[1] + mid * dir[1];
      stretch = p_stretch_compute_vertex(v);

      if (stretch_low < stretch_high) {
        high = mid;
        stretch_high = stretch;
      }
      else {
        low = mid;
        stretch_low = stretch;
      }
    }

    /* no luck, stretch has increased, reset to old values */
    if (stretch >= orig_stretch) {
      copy_v2_v2(v->uv, orig_uv);
    }
  }
}

/* Minimum area enclosing rectangle for packing */

static int p_compare_geometric_uv(const void *a, const void *b)
{
  const PVert *v1 = *(const PVert *const *)a;
  const PVert *v2 = *(const PVert *const *)b;

  if (v1->uv[0] < v2->uv[0]) {
    return -1;
  }
  if (v1->uv[0] == v2->uv[0]) {
    if (v1->uv[1] < v2->uv[1]) {
      return -1;
    }
    if (v1->uv[1] == v2->uv[1]) {
      return 0;
    }
    return 1;
  }
  return 1;
}

static bool p_chart_convex_hull(PChart *chart, PVert ***r_verts, int *r_nverts, int *r_right)
{
  /* Graham algorithm, taken from:
   * http://aspn.activestate.com/ASPN/Cookbook/Python/Recipe/117225 */

  PEdge *be, *e;
  int npoints = 0, i, ulen, llen;
  PVert **U, **L, **points, **p;

  p_chart_boundaries(chart, &be);

  if (!be) {
    return false;
  }

  e = be;
  do {
    npoints++;
    e = p_boundary_edge_next(e);
  } while (e != be);

  p = points = MEM_malloc_arrayN<PVert *>(2 * size_t(npoints), "PCHullpoints");
  U = MEM_malloc_arrayN<PVert *>(size_t(npoints), "PCHullU");
  L = MEM_malloc_arrayN<PVert *>(size_t(npoints), "PCHullL");

  e = be;
  do {
    *p = e->vert;
    p++;
    e = p_boundary_edge_next(e);
  } while (e != be);

  qsort(points, npoints, sizeof(PVert *), p_compare_geometric_uv);

  ulen = llen = 0;
  for (p = points, i = 0; i < npoints; i++, p++) {
    while ((ulen > 1) && (p_area_signed(U[ulen - 2]->uv, (*p)->uv, U[ulen - 1]->uv) <= 0)) {
      ulen--;
    }
    while ((llen > 1) && (p_area_signed(L[llen - 2]->uv, (*p)->uv, L[llen - 1]->uv) >= 0)) {
      llen--;
    }

    U[ulen] = *p;
    ulen++;
    L[llen] = *p;
    llen++;
  }

  npoints = 0;
  for (p = points, i = 0; i < ulen; i++, p++, npoints++) {
    *p = U[i];
  }

  /* the first and last point in L are left out, since they are also in U */
  for (i = llen - 2; i > 0; i--, p++, npoints++) {
    *p = L[i];
  }

  *r_verts = points;
  *r_nverts = npoints;
  *r_right = ulen - 1;

  MEM_freeN(U);
  MEM_freeN(L);

  return true;
}

static float p_rectangle_area(float *p1, float *dir, float *p2, float *p3, float *p4)
{
  /* given 4 points on the rectangle edges and the direction of on edge,
   * compute the area of the rectangle */

  float orthodir[2], corner1[2], corner2[2], corner3[2];

  orthodir[0] = dir[1];
  orthodir[1] = -dir[0];

  if (!p_intersect_line_2d_dir(p1, dir, p2, orthodir, corner1)) {
    return 1e10;
  }

  if (!p_intersect_line_2d_dir(p1, dir, p4, orthodir, corner2)) {
    return 1e10;
  }

  if (!p_intersect_line_2d_dir(p3, dir, p4, orthodir, corner3)) {
    return 1e10;
  }

  return len_v2v2(corner1, corner2) * len_v2v2(corner2, corner3);
}

static float p_chart_minimum_area_angle(PChart *chart)
{
  /* minimum area enclosing rectangle with rotating calipers, info:
   * http://cgm.cs.mcgill.ca/~orm/maer.html */

  float rotated, minarea, minangle, area, len;
  float *angles, miny, maxy, v[2], a[4], mina;
  int npoints, right, i_min, i_max, i, idx[4], nextidx;
  PVert **points, *p1, *p2, *p3, *p4, *p1n;

  /* compute convex hull */
  if (!p_chart_convex_hull(chart, &points, &npoints, &right)) {
    return 0.0;
  }

  /* find left/top/right/bottom points, and compute angle for each point */
  angles = MEM_malloc_arrayN<float>(size_t(npoints), "PMinAreaAngles");

  i_min = i_max = 0;
  miny = 1e10;
  maxy = -1e10;

  for (i = 0; i < npoints; i++) {
    p1 = (i == 0) ? points[npoints - 1] : points[i - 1];
    p2 = points[i];
    p3 = (i == npoints - 1) ? points[0] : points[i + 1];

    angles[i] = float(M_PI) - angle_v2v2v2(p1->uv, p2->uv, p3->uv);

    if (points[i]->uv[1] < miny) {
      miny = points[i]->uv[1];
      i_min = i;
    }
    if (points[i]->uv[1] > maxy) {
      maxy = points[i]->uv[1];
      i_max = i;
    }
  }

  /* left, top, right, bottom */
  idx[0] = 0;
  idx[1] = i_max;
  idx[2] = right;
  idx[3] = i_min;

  v[0] = points[idx[0]]->uv[0];
  v[1] = points[idx[0]]->uv[1] + 1.0f;
  a[0] = angle_v2v2v2(points[(idx[0] + 1) % npoints]->uv, points[idx[0]]->uv, v);

  v[0] = points[idx[1]]->uv[0] + 1.0f;
  v[1] = points[idx[1]]->uv[1];
  a[1] = angle_v2v2v2(points[(idx[1] + 1) % npoints]->uv, points[idx[1]]->uv, v);

  v[0] = points[idx[2]]->uv[0];
  v[1] = points[idx[2]]->uv[1] - 1.0f;
  a[2] = angle_v2v2v2(points[(idx[2] + 1) % npoints]->uv, points[idx[2]]->uv, v);

  v[0] = points[idx[3]]->uv[0] - 1.0f;
  v[1] = points[idx[3]]->uv[1];
  a[3] = angle_v2v2v2(points[(idx[3] + 1) % npoints]->uv, points[idx[3]]->uv, v);

  /* 4 rotating calipers */

  rotated = 0.0;
  minarea = 1e10;
  minangle = 0.0;

  while (rotated <= float(M_PI_2)) { /* INVESTIGATE: how far to rotate? */
    /* rotate with the smallest angle */
    i_min = 0;
    mina = 1e10;

    for (i = 0; i < 4; i++) {
      if (a[i] < mina) {
        mina = a[i];
        i_min = i;
      }
    }

    rotated += mina;
    nextidx = (idx[i_min] + 1) % npoints;

    a[i_min] = angles[nextidx];
    a[(i_min + 1) % 4] = a[(i_min + 1) % 4] - mina;
    a[(i_min + 2) % 4] = a[(i_min + 2) % 4] - mina;
    a[(i_min + 3) % 4] = a[(i_min + 3) % 4] - mina;

    /* compute area */
    p1 = points[idx[i_min]];
    p1n = points[nextidx];
    p2 = points[idx[(i_min + 1) % 4]];
    p3 = points[idx[(i_min + 2) % 4]];
    p4 = points[idx[(i_min + 3) % 4]];

    len = len_v2v2(p1->uv, p1n->uv);

    if (len > 0.0f) {
      len = 1.0f / len;
      v[0] = (p1n->uv[0] - p1->uv[0]) * len;
      v[1] = (p1n->uv[1] - p1->uv[1]) * len;

      area = p_rectangle_area(p1->uv, v, p2->uv, p3->uv, p4->uv);

      /* remember smallest area */
      if (area < minarea) {
        minarea = area;
        minangle = rotated;
      }
    }

    idx[i_min] = nextidx;
  }

  /* try keeping rotation as small as possible */
  if (minangle > float(M_PI_4)) {
    minangle -= float(M_PI_2);
  }

  MEM_freeN(angles);
  MEM_freeN(points);

  return minangle;
}

static void p_chart_rotate_minimum_area(PChart *chart)
{
  float angle = p_chart_minimum_area_angle(chart);
  float sine = sinf(angle);
  float cosine = cosf(angle);
  PVert *v;

  for (v = chart->verts; v; v = v->nextlink) {
    float oldu = v->uv[0], oldv = v->uv[1];
    v->uv[0] = cosine * oldu - sine * oldv;
    v->uv[1] = sine * oldu + cosine * oldv;
  }
}

static void p_chart_rotate_fit_aabb(PChart *chart)
{
  Array<float2> points(chart->nverts);

  p_chart_uv_to_array(chart, points);

  float angle = BLI_convexhull_aabb_fit_points_2d(points);

  if (angle != 0.0f) {
    float mat[2][2];
    angle_to_mat2(mat, angle);
    p_chart_uv_transform(chart, mat);
  }
}

ParamHandle::ParamHandle()
{
  arena = BLI_memarena_new(MEM_SIZE_OPTIMAL(1 << 16), "param construct arena");
  polyfill_arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, "param polyfill arena");
  polyfill_heap = BLI_heap_new_ex(BLI_POLYFILL_ALLOC_NGON_RESERVE);

  construction_chart = MEM_callocN<PChart>("PChart");

  hash_verts = phash_new((PHashLink **)&construction_chart->verts, 1);
  hash_edges = phash_new((PHashLink **)&construction_chart->edges, 1);
  hash_faces = phash_new((PHashLink **)&construction_chart->faces, 1);
}

ParamHandle::~ParamHandle()
{
  BLI_memarena_free(arena);
  arena = nullptr;
  BLI_memarena_free(polyfill_arena);
  polyfill_arena = nullptr;
  BLI_heap_free(polyfill_heap, nullptr);
  polyfill_heap = nullptr;

  MEM_SAFE_FREE(construction_chart);

  phash_safe_delete(&hash_verts);
  phash_safe_delete(&hash_edges);
  phash_safe_delete(&hash_faces);

  if (pin_hash) {
    BLI_ghash_free(pin_hash, nullptr, nullptr);
    pin_hash = nullptr;
  }

  for (int i = 0; i < ncharts; i++) {
    MEM_SAFE_FREE(charts[i]);
  }
  MEM_SAFE_FREE(charts);

  if (rng) {
    BLI_rng_free(rng);
    rng = nullptr;
  }
}

void uv_parametrizer_aspect_ratio(ParamHandle *phandle, const float aspect_y)
{
  BLI_assert(aspect_y > 0.0f);
  phandle->aspect_y = aspect_y;
}

struct GeoUVPinIndex {
  GeoUVPinIndex *next;
  float uv[2];
  ParamKey reindex;
};

ParamKey uv_find_pin_index(ParamHandle *handle, const int bmvertindex, const float uv[2])
{
  if (!handle->pin_hash) {
    return bmvertindex; /* No verts pinned. */
  }

  const GeoUVPinIndex *pinuvlist = (const GeoUVPinIndex *)BLI_ghash_lookup(
      handle->pin_hash, POINTER_FROM_INT(bmvertindex));
  if (!pinuvlist) {
    return bmvertindex; /* Vert not pinned. */
  }

  /* At least one of the UVs associated with bmvertindex is pinned. Find the best one. */
  float bestdistsquared = len_squared_v2v2(pinuvlist->uv, uv);
  ParamKey bestkey = pinuvlist->reindex;
  pinuvlist = pinuvlist->next;
  while (pinuvlist) {
    const float distsquared = len_squared_v2v2(pinuvlist->uv, uv);
    if (bestdistsquared > distsquared) {
      bestdistsquared = distsquared;
      bestkey = pinuvlist->reindex;
    }
    pinuvlist = pinuvlist->next;
  }
  return bestkey;
}

static GeoUVPinIndex *new_geo_uv_pinindex(ParamHandle *handle, const float uv[2])
{
  GeoUVPinIndex *pinuv = (GeoUVPinIndex *)BLI_memarena_alloc(handle->arena, sizeof(*pinuv));
  pinuv->next = nullptr;
  copy_v2_v2(pinuv->uv, uv);
  pinuv->reindex = PARAM_KEY_MAX - (handle->unique_pin_count++);
  return pinuv;
}

void uv_prepare_pin_index(ParamHandle *handle, const int bmvertindex, const float uv[2])
{
  if (!handle->pin_hash) {
    handle->pin_hash = BLI_ghash_int_new("uv pin reindex");
  }

  GeoUVPinIndex *pinuvlist = (GeoUVPinIndex *)BLI_ghash_lookup(handle->pin_hash,
                                                               POINTER_FROM_INT(bmvertindex));
  if (!pinuvlist) {
    BLI_ghash_insert(
        handle->pin_hash, POINTER_FROM_INT(bmvertindex), new_geo_uv_pinindex(handle, uv));
    return;
  }

  while (true) {
    if (equals_v2v2(pinuvlist->uv, uv)) {
      return;
    }
    if (!pinuvlist->next) {
      pinuvlist->next = new_geo_uv_pinindex(handle, uv);
      return;
    }
    pinuvlist = pinuvlist->next;
  }
}

static void p_add_ngon(ParamHandle *handle,
                       const ParamKey key,
                       const int nverts,
                       const ParamKey *vkeys,
                       const float **co,
                       float **uv, /* Output will eventually be written to `uv`. */
                       const float *weight,
                       const bool *pin,
                       const bool *select)
{
  /* Allocate memory for polyfill. */
  MemArena *arena = handle->polyfill_arena;
  Heap *heap = handle->polyfill_heap;
  uint nfilltri = nverts - 2;
  uint(*tris)[3] = static_cast<uint(*)[3]>(
      BLI_memarena_alloc(arena, sizeof(*tris) * size_t(nfilltri)));
  float (*projverts)[2] = static_cast<float (*)[2]>(
      BLI_memarena_alloc(arena, sizeof(*projverts) * size_t(nverts)));

  /* Calc normal, flipped: to get a positive 2d cross product. */
  float normal[3];
  zero_v3(normal);

  const float *co_curr, *co_prev = co[nverts - 1];
  for (int j = 0; j < nverts; j++) {
    co_curr = co[j];
    add_newell_cross_v3_v3v3(normal, co_prev, co_curr);
    co_prev = co_curr;
  }
  if (UNLIKELY(normalize_v3(normal) == 0.0f)) {
    normal[2] = 1.0f;
  }

  /* Project verts to 2d. */
  float axis_mat[3][3];
  axis_dominant_v3_to_m3_negate(axis_mat, normal);
  for (int j = 0; j < nverts; j++) {
    mul_v2_m3v3(projverts[j], axis_mat, co[j]);
  }

  BLI_polyfill_calc_arena(projverts, nverts, 1, tris, arena);

  /* Beautify helps avoid thin triangles that give numerical problems. */
  BLI_polyfill_beautify(projverts, nverts, tris, arena, heap);

  /* Add triangles. */
  for (uint j = 0; j < nfilltri; j++) {
    uint *tri = tris[j];
    uint v0 = tri[0];
    uint v1 = tri[1];
    uint v2 = tri[2];

    const ParamKey tri_vkeys[3] = {vkeys[v0], vkeys[v1], vkeys[v2]};
    const float *tri_co[3] = {co[v0], co[v1], co[v2]};
    float *tri_uv[3] = {uv[v0], uv[v1], uv[v2]};

    const float *tri_weight = nullptr;
    float tri_weight_tmp[3];

    if (weight) {
      tri_weight_tmp[0] = weight[v0];
      tri_weight_tmp[1] = weight[v1];
      tri_weight_tmp[2] = weight[v2];
      tri_weight = tri_weight_tmp;
    };

    const bool tri_pin[3] = {pin[v0], pin[v1], pin[v2]};
    const bool tri_select[3] = {select[v0], select[v1], select[v2]};

    uv_parametrizer_face_add(
        handle, key, 3, tri_vkeys, tri_co, tri_uv, tri_weight, tri_pin, tri_select);
  }

  BLI_memarena_clear(arena);
}

void uv_parametrizer_face_add(ParamHandle *phandle,
                              const ParamKey key,
                              const int nverts,
                              const ParamKey *vkeys,
                              const float **co,
                              float **uv,
                              const float *weight,
                              const bool *pin,
                              const bool *select)
{
  BLI_assert(nverts >= 3);
  BLI_assert(phandle->state == PHANDLE_STATE_ALLOCATED);

  if (nverts > 3) {
    /* Protect against (manifold) geometry which has a non-manifold triangulation.
     * See #102543. */

    Vector<int, 32> permute;
    permute.reserve(nverts);
    for (int i = 0; i < nverts; i++) {
      permute.append_unchecked(i);
    }

    for (int i = nverts - 1; i >= 0;) {
      /* Just check the "ears" of the n-gon.
       * For quads, this is sufficient.
       * For pentagons and higher, we might miss internal duplicate triangles, but note
       * that such cases are rare if the source geometry is manifold and non-intersecting. */
      const int pm = int(permute.size());
      BLI_assert(pm > 3);
      int i0 = permute[i];
      int i1 = permute[(i + 1) % pm];
      int i2 = permute[(i + 2) % pm];
      if (!p_face_exists(phandle, vkeys, i0, i1, i2)) {
        i--; /* All good. */
        continue;
      }

      /* An existing triangle has already been inserted.
       * As a heuristic, attempt to add the *previous* triangle.
       * NOTE: Should probably call `uv_parametrizer_face_add`
       * instead of `p_face_add_construct`. */
      int iprev = permute[(i + pm - 1) % pm];
      p_face_add_construct(phandle, key, vkeys, co, uv, weight, iprev, i0, i1, pin, select);

      permute.remove(i);
      if (permute.size() == 3) {
        break;
      }
    }
    if (permute.size() != nverts) {
      const int pm = int(permute.size());
      /* Add the remaining `pm-gon` data. */
      Array<ParamKey> vkeys_sub(pm);
      Array<const float *> co_sub(pm);
      Array<float *> uv_sub(pm);
      Array<float> weight_sub(weight ? pm : 0);
      Array<bool> pin_sub(pm);
      Array<bool> select_sub(pm);
      for (int i = 0; i < pm; i++) {
        int j = permute[i];
        vkeys_sub[i] = vkeys[j];
        co_sub[i] = co[j];
        uv_sub[i] = uv[j];
        if (weight) {
          weight_sub[i] = weight[j];
        }
        pin_sub[i] = pin && pin[j];
        select_sub[i] = select && select[j];
      }
      p_add_ngon(phandle,
                 key,
                 pm,
                 &vkeys_sub.first(),
                 &co_sub.first(),
                 &uv_sub.first(),
                 weight ? &weight_sub.first() : nullptr,
                 &pin_sub.first(),
                 &select_sub.first());
      return; /* Nothing more to do. */
    }
    /* No "ears" have previously been inserted. Continue as normal. */
  }
  if (nverts > 3) {
    /* ngon */
    p_add_ngon(phandle, key, nverts, vkeys, co, uv, weight, pin, select);
  }
  else if (!p_face_exists(phandle, vkeys, 0, 1, 2)) {
    /* triangle */
    p_face_add_construct(phandle, key, vkeys, co, uv, weight, 0, 1, 2, pin, select);
  }
}

void uv_parametrizer_edge_set_seam(ParamHandle *phandle, const ParamKey *vkeys)
{
  BLI_assert(phandle->state == PHANDLE_STATE_ALLOCATED);

  PEdge *e = p_edge_lookup(phandle, vkeys);
  if (e) {
    e->flag |= PEDGE_SEAM;
  }
}

void uv_parametrizer_construct_end(ParamHandle *phandle,
                                   bool fill_holes,
                                   bool topology_from_uvs,
                                   int *r_count_failed)
{
  int i, j;

  BLI_assert(phandle->state == PHANDLE_STATE_ALLOCATED);

  phandle->ncharts = p_connect_pairs(phandle, topology_from_uvs);
  phandle->charts = p_split_charts(phandle, phandle->construction_chart, phandle->ncharts);

  MEM_freeN(phandle->construction_chart);
  phandle->construction_chart = nullptr;

  phash_safe_delete(&phandle->hash_verts);
  phash_safe_delete(&phandle->hash_edges);
  phash_safe_delete(&phandle->hash_faces);

  for (i = j = 0; i < phandle->ncharts; i++) {
    PChart *chart = phandle->charts[i];

    PEdge *outer;
    p_chart_boundaries(chart, &outer);

    if (!topology_from_uvs && chart->nboundaries == 0) {
      MEM_freeN(chart);
      if (r_count_failed) {
        *r_count_failed += 1;
      }
      continue;
    }

    phandle->charts[j++] = chart;

    if (fill_holes && chart->nboundaries > 1) {
      p_chart_fill_boundaries(phandle, chart, outer);
    }

    for (PVert *v = chart->verts; v; v = v->nextlink) {
      p_vert_load_pin_select_uvs(phandle, v);
    }
  }

  phandle->ncharts = j;

  phandle->state = PHANDLE_STATE_CONSTRUCTED;
}

void uv_parametrizer_lscm_begin(ParamHandle *phandle, bool live, bool abf)
{
  BLI_assert(phandle->state == PHANDLE_STATE_CONSTRUCTED);
  phandle->state = PHANDLE_STATE_LSCM;

  for (int i = 0; i < phandle->ncharts; i++) {
    for (PFace *f = phandle->charts[i]->faces; f; f = f->nextlink) {
      p_face_backup_uvs(f);
    }
    p_chart_lscm_begin(phandle->charts[i], live, abf);
  }
}

void uv_parametrizer_lscm_solve(ParamHandle *phandle, int *count_changed, int *count_failed)
{
  BLI_assert(phandle->state == PHANDLE_STATE_LSCM);

  for (int i = 0; i < phandle->ncharts; i++) {
    PChart *chart = phandle->charts[i];

    if (!chart->context) {
      continue;
    }
    const bool result = p_chart_lscm_solve(phandle, chart);

    if (result && !chart->has_pins) {
      /* Every call to LSCM will eventually call uv_pack, so rotating here might be redundant. */
      p_chart_rotate_minimum_area(chart);
    }
    else if (result && chart->single_pin) {
      p_chart_rotate_fit_aabb(chart);
      p_chart_lscm_transform_single_pin(chart);
    }

    if (!result || !chart->has_pins) {
      p_chart_lscm_end(chart);
    }

    if (result) {
      if (count_changed != nullptr) {
        *count_changed += 1;
      }
    }
    else {
      if (count_failed != nullptr) {
        *count_failed += 1;
      }
    }
  }
}

void uv_parametrizer_lscm_end(ParamHandle *phandle)
{
  BLI_assert(phandle->state == PHANDLE_STATE_LSCM);

  for (int i = 0; i < phandle->ncharts; i++) {
    p_chart_lscm_end(phandle->charts[i]);
#if 0
    p_chart_complexify(phandle->charts[i]);
#endif
  }

  phandle->state = PHANDLE_STATE_CONSTRUCTED;
}

void uv_parametrizer_stretch_begin(ParamHandle *phandle)
{
  BLI_assert(phandle->state == PHANDLE_STATE_CONSTRUCTED);
  phandle->state = PHANDLE_STATE_STRETCH;

  phandle->rng = BLI_rng_new(31415926);
  phandle->blend = 0.0f;

  for (int i = 0; i < phandle->ncharts; i++) {
    PChart *chart = phandle->charts[i];

    for (PVert *v = chart->verts; v; v = v->nextlink) {
      v->flag &= ~PVERT_PIN; /* don't use user-defined pins */
    }

    p_stretch_pin_boundary(chart);

    for (PFace *f = chart->faces; f; f = f->nextlink) {
      p_face_backup_uvs(f);
      f->u.area3d = p_face_area(f);
    }
  }
}

void uv_parametrizer_stretch_blend(ParamHandle *phandle, float blend)
{
  BLI_assert(phandle->state == PHANDLE_STATE_STRETCH);
  phandle->blend = blend;
}

void uv_parametrizer_stretch_iter(ParamHandle *phandle)
{
  BLI_assert(phandle->state == PHANDLE_STATE_STRETCH);
  for (int i = 0; i < phandle->ncharts; i++) {
    p_chart_stretch_minimize(phandle->charts[i], phandle->rng);
  }
}

void uv_parametrizer_stretch_end(ParamHandle *phandle)
{
  BLI_assert(phandle->state == PHANDLE_STATE_STRETCH);
  phandle->state = PHANDLE_STATE_CONSTRUCTED;
}

void uv_parametrizer_pack(ParamHandle *handle, const UVPackIsland_Params &params)
{
  if (handle->ncharts == 0) {
    return;
  }

  uv_parametrizer_scale_x(handle, 1.0f / handle->aspect_y);

  Vector<PackIsland *> pack_island_vector;

  for (int i = 0; i < handle->ncharts; i++) {
    PChart *chart = handle->charts[i];
    if (params.pin_method == ED_UVPACK_PIN_NONE && chart->has_pins) {
      continue;
    }

    geometry::PackIsland *pack_island = new geometry::PackIsland();
    pack_island->caller_index = i;
    pack_island->aspect_y = handle->aspect_y;
    pack_island->pinned = chart->has_pins;

    for (PFace *f = chart->faces; f; f = f->nextlink) {
      PVert *v0 = f->edge->vert;
      PVert *v1 = f->edge->next->vert;
      PVert *v2 = f->edge->next->next->vert;
      pack_island->add_triangle(v0->uv, v1->uv, v2->uv);
    }

    pack_island_vector.append(pack_island);
  }

  const float scale = pack_islands(pack_island_vector, params);

  for (const int64_t i : pack_island_vector.index_range()) {
    PackIsland *pack_island = pack_island_vector[i];
    const float island_scale = pack_island->can_scale_(params) ? scale : 1.0f;
    PChart *chart = handle->charts[pack_island->caller_index];

    float matrix[2][2];
    pack_island->build_transformation(island_scale, pack_island->angle, matrix);
    for (PVert *v = chart->verts; v; v = v->nextlink) {
      geometry::mul_v2_m2_add_v2v2(v->uv, matrix, v->uv, pack_island->pre_translate);
    }
    geometry::p_chart_uv_translate(chart, params.udim_base_offset);

    pack_island_vector[i] = nullptr;
    delete pack_island;
  }

  uv_parametrizer_scale_x(handle, handle->aspect_y);
}

void uv_parametrizer_average(ParamHandle *phandle, bool ignore_pinned, bool scale_uv, bool shear)
{
  int i;
  float tot_area_3d = 0.0f;
  float tot_area_uv = 0.0f;
  float minv[2], maxv[2], trans[2];

  if (phandle->ncharts == 0) {
    return;
  }

  for (i = 0; i < phandle->ncharts; i++) {
    PChart *chart = phandle->charts[i];

    if (ignore_pinned && chart->has_pins) {
      continue;
    }

    /* Store original bounding box midpoint. */
    p_chart_uv_bbox(chart, minv, maxv);
    mid_v2_v2v2(chart->origin, minv, maxv);

    if (scale_uv || shear) {
      /* It's possible that for some "bad" inputs, the following iteration will converge slowly or
       * perhaps even diverge. Rather than infinite loop, we only iterate a maximum of `max_iter`
       * times. (Also useful when making changes to the calculation.) */
      int max_iter = 10;
      for (int j = 0; j < max_iter; j++) {
        /* An island could contain millions of polygons. When summing many small values, we need to
         * use double precision in the accumulator to maintain accuracy. Note that the individual
         * calculations only need to be at single precision. */
        double scale_cou = 0;
        double scale_cov = 0;
        double scale_cross = 0;
        double weight_sum = 0;
        for (PFace *f = chart->faces; f; f = f->nextlink) {
          float m[2][2], s[2][2];
          PVert *va = f->edge->vert;
          PVert *vb = f->edge->next->vert;
          PVert *vc = f->edge->next->next->vert;
          s[0][0] = va->uv[0] - vc->uv[0];
          s[0][1] = va->uv[1] - vc->uv[1];
          s[1][0] = vb->uv[0] - vc->uv[0];
          s[1][1] = vb->uv[1] - vc->uv[1];
          /* Find the "U" axis and "V" axis in triangle coordinates. Normally this would require
           * SVD, but in 2D we can use a cheaper matrix inversion instead. */
          if (!invert_m2_m2(m, s)) {
            continue;
          }
          float cou[3], cov[3]; /* i.e. Texture "U" and texture "V" in 3D co-ordinates. */
          for (int k = 0; k < 3; k++) {
            cou[k] = m[0][0] * (va->co[k] - vc->co[k]) + m[0][1] * (vb->co[k] - vc->co[k]);
            cov[k] = m[1][0] * (va->co[k] - vc->co[k]) + m[1][1] * (vb->co[k] - vc->co[k]);
          }
          const float weight = p_face_area(f);
          scale_cou += len_v3(cou) * weight;
          scale_cov += len_v3(cov) * weight;
          if (shear) {
            normalize_v3(cov);
            normalize_v3(cou);

            /* Why is scale_cross called `cross` when we call `dot`? The next line calculates:
             * `scale_cross += length(cross(cross(cou, face_normal), cov))`
             * By construction, both `cou` and `cov` are orthogonal to the face normal.
             * By definition, the normal vector has unit length. */
            scale_cross += dot_v3v3(cou, cov) * weight;
          }
          weight_sum += weight;
        }
        if (scale_cou * scale_cov < 1e-10f) {
          break;
        }
        const float scale_factor_u = scale_uv ? sqrtf(scale_cou / scale_cov) : 1.0f;

        /* Compute correction transform. */
        float t[2][2];
        t[0][0] = scale_factor_u;
        t[1][0] = clamp_f(float(scale_cross / weight_sum), -0.5f, 0.5f);
        t[0][1] = 0;
        t[1][1] = 1.0f / scale_factor_u;

        /* Apply the correction. */
        p_chart_uv_transform(chart, t);

        /* How far from the identity transform are we? [[1,0],[0,1]] */
        const float err = fabsf(t[0][0] - 1.0f) + fabsf(t[1][0]) + fabsf(t[0][1]) +
                          fabsf(t[1][1] - 1.0f);

        const float tolerance = 1e-6f; /* Trade accuracy for performance. */
        if (err < tolerance) {
          /* Too slow? Use Richardson Extrapolation to accelerate the convergence. */
          break;
        }
      }
    }

    chart->area_3d = 0.0f;
    chart->area_uv = 0.0f;

    for (PFace *f = chart->faces; f; f = f->nextlink) {
      chart->area_3d += p_face_area(f);
      chart->area_uv += fabsf(p_face_uv_area_signed(f));
    }

    tot_area_3d += chart->area_3d;
    tot_area_uv += chart->area_uv;
  }

  if (tot_area_3d == 0.0f || tot_area_uv == 0.0f) {
    /* Prevent divide by zero. */
    return;
  }

  const float tot_fac = tot_area_3d / tot_area_uv;

  for (i = 0; i < phandle->ncharts; i++) {
    PChart *chart = phandle->charts[i];

    if (ignore_pinned && chart->has_pins) {
      continue;
    }

    if (chart->area_3d != 0.0f && chart->area_uv != 0.0f) {
      const float fac = chart->area_3d / chart->area_uv;

      /* Average scale. */
      p_chart_uv_scale(chart, sqrtf(fac / tot_fac));

      /* Get the current island bounding box. */
      p_chart_uv_bbox(chart, minv, maxv);

      /* Move back to original midpoint. */
      mid_v2_v2v2(trans, minv, maxv);
      sub_v2_v2v2(trans, chart->origin, trans);

      p_chart_uv_translate(chart, trans);
    }
  }
}

void uv_parametrizer_flush(ParamHandle *phandle)
{
  for (int i = 0; i < phandle->ncharts; i++) {
    PChart *chart = phandle->charts[i];
    if (chart->skip_flush) {
      continue;
    }

    p_flush_uvs(phandle, chart);
  }
}

void uv_parametrizer_flush_restore(ParamHandle *phandle)
{
  for (int i = 0; i < phandle->ncharts; i++) {
    PChart *chart = phandle->charts[i];
    for (PFace *f = chart->faces; f; f = f->nextlink) {
      p_face_restore_uvs(f);
    }
  }
}

/* -------------------------------------------------------------------- */
/** \name Degenerate Geometry Fixing
 * \{ */

static bool p_collapse_doubles_allowed(PEdge *edge, PEdge *pair, float threshold_squared)
{
  PVert *oldv, *keepv;

  p_collapsing_verts(edge, pair, &oldv, &keepv);

  /* Do not collapse a pinned vertex unless the target vertex
   * is also pinned. */
  if ((oldv->flag & PVERT_PIN) && !(keepv->flag & PVERT_PIN)) {
    return false;
  }

  if (!p_collapse_allowed_topologic(edge, pair)) {
    return false;
  }

  PEdge *collapse_e = edge ? edge : pair;
  return p_edge_length_squared(collapse_e) < threshold_squared;
}

static float p_collapse_doubles_cost(PEdge *edge, PEdge *pair)
{
  PEdge *collapse_e = edge ? edge : pair;
  return p_edge_length_squared(collapse_e);
}

static void UNUSED_FUNCTION(p_chart_collapse_doubles)(PChart *chart, const float threshold)
{
  const float threshold_squared = threshold * threshold;

  p_chart_simplify_compute(chart, p_collapse_doubles_cost, [=](PEdge *e, PEdge *pair) {
    return p_collapse_doubles_allowed(e, pair, threshold_squared);
  });
}

static void p_chart_flush_collapsed_uvs(PChart *chart)
{
  PEdge *e, *pair, *edge;
  PVert *newv, *keepv;

  for (e = chart->collapsed_edges; e; e = e->nextlink) {
    if (!(e->flag & PEDGE_COLLAPSE_EDGE)) {
      break;
    }
    edge = e;
    pair = e->pair;
    if (edge->flag & PEDGE_COLLAPSE_PAIR) {
      std::swap(edge, pair);
    }
    p_collapsing_verts(edge, pair, &newv, &keepv);

    if (!(newv->flag & PVERT_PIN)) {
      newv->uv[0] = keepv->uv[0];
      newv->uv[1] = keepv->uv[1];
    }
  }
}

static bool p_validate_corrected_coords_point(const PEdge *corr_e,
                                              const float corr_co1[3],
                                              const float corr_co2[3],
                                              const float min_area,
                                              const float min_angle_cos)
{
  /* Check whether the given corrected coordinates don't result in any other triangle with area
   * lower than min_area. */

  const PVert *corr_v = corr_e->vert;
  const PEdge *e = corr_v->edge;

  do {
    float area;

    if (e == corr_e) {
      continue;
    }

    if (!(e->face->flag & PFACE_DONE)) {
      continue;
    }

    if (e->next->next == corr_e->pair) {
      PVert *other_v = e->next->next->vert;
      area = area_tri_v3(corr_co1, corr_co2, other_v->co);
    }
    else {
      const PVert *other_v1 = e->next->vert;
      const PVert *other_v2 = e->next->next->vert;
      area = area_tri_v3(corr_co1, other_v1->co, other_v2->co);
    }

    if (area < min_area) {
      return false;
    }

    float f_cos[3];

    if (e->next->next == corr_e->pair) {
      PVert *other_v = e->next->next->vert;
      p_triangle_cos(corr_co1, corr_co2, other_v->co, f_cos, f_cos + 1, f_cos + 2);
    }
    else {
      const PVert *other_v1 = e->next->vert;
      const PVert *other_v2 = e->next->next->vert;
      p_triangle_cos(corr_co1, other_v1->co, other_v2->co, f_cos, f_cos + 1, f_cos + 2);
    }

    for (int i = 0; i < 3; i++) {
      if (f_cos[i] > min_angle_cos) {
        return false;
      }
    }

  } while ((e = p_wheel_edge_next(e)) && (e != corr_v->edge));

  return true;
}

static bool p_validate_corrected_coords(const PEdge *corr_e,
                                        const float corr_co[3],
                                        float min_area,
                                        float min_angle_cos)
{
  /* Check whether the given corrected coordinates don't result in any other triangle with area
   * lower than min_area. */

  const PVert *corr_v = corr_e->vert;
  const PEdge *e = corr_v->edge;

  do {
    if (!(e->face->flag & PFACE_DONE) && (e != corr_e)) {
      continue;
    }

    const PVert *other_v1 = e->next->vert;
    const PVert *other_v2 = e->next->next->vert;

    const float area = area_tri_v3(corr_co, other_v1->co, other_v2->co);

    if (area < min_area) {
      return false;
    }

    float f_cos[3];
    p_triangle_cos(corr_co, other_v1->co, other_v2->co, f_cos, f_cos + 1, f_cos + 2);

    for (int i = 0; i < 3; i++) {
      if (f_cos[i] > min_angle_cos) {
        return false;
      }
    }

  } while ((e = p_wheel_edge_next(e)) && (e != corr_v->edge));

  return true;
}

static bool p_edge_matrix(float R[3][3], const float edge_dir[3])
{
  static const constexpr float eps = 1.0e-5;
  static const constexpr float n1[3] = {0.0f, 0.0f, 1.0f};
  static const constexpr float n2[3] = {0.0f, 1.0f, 0.0f};

  float edge_len = len_v3(edge_dir);
  if (edge_len < eps) {
    return false;
  }

  float edge_dir_norm[3];
  copy_v3_v3(edge_dir_norm, edge_dir);
  mul_v3_fl(edge_dir_norm, 1.0f / edge_len);

  float normal_dir[3];
  cross_v3_v3v3(normal_dir, edge_dir_norm, n1);
  float normal_len = len_v3(normal_dir);

  if (normal_len < eps) {
    cross_v3_v3v3(normal_dir, edge_dir_norm, n2);
    normal_len = len_v3(normal_dir);

    if (normal_len < eps) {
      return false;
    }
  }

  mul_v3_fl(normal_dir, 1.0f / normal_len);

  float tangent_dir[3];
  cross_v3_v3v3(tangent_dir, edge_dir_norm, normal_dir);

  R[0][0] = edge_dir_norm[0];
  R[1][0] = edge_dir_norm[1];
  R[2][0] = edge_dir_norm[2];

  R[0][1] = normal_dir[0];
  R[1][1] = normal_dir[1];
  R[2][1] = normal_dir[2];

  R[0][2] = tangent_dir[0];
  R[1][2] = tangent_dir[1];
  R[2][2] = tangent_dir[2];

  return true;
}

static bool p_edge_matrix(float R[3][3], const PEdge *e)
{
  float edge_dir[3];
  copy_v3_v3(edge_dir, e->next->vert->co);
  sub_v3_v3(edge_dir, e->vert->co);

  return p_edge_matrix(R, edge_dir);
}

static const float CORR_ZERO_AREA_EPS = 1.0e-10f;

static bool p_chart_correct_degenerate_triangle_point(PFace *f,
                                                      float min_area,
                                                      float min_angle_cos)
{
  static const float3 ref_edges[] = {
      {1.0f, 0.0f, 0.0f},
      {0.0f, 1.0f, 0.0f},
      {0.0f, 0.0f, 1.0f},
      {0.0f, 1.0f, 1.0f},
      {1.0f, 0.0f, 1.0f},
      {1.0f, 1.0f, 0.0f},

      {0.0f, 0.5f, 1.0f},
      {0.5f, 0.0f, 1.0f},
      {0.5f, 1.0f, 0.0f},

      {0.0f, 1.0f, 0.5f},
      {1.0f, 0.0f, 0.5f},
      {1.0f, 0.5f, 0.0f},
  };
  static const int ref_edge_count = ARRAY_SIZE(ref_edges);
  static const int LEN_MULTIPLIER_COUNT = 3;
  bool corr_co_found = false;

  float corr_len = 2.0f * std::sqrt((min_area + CORR_ZERO_AREA_EPS) / 3.0f / std::sqrt(3.0f));

  for (int m = 0; m < LEN_MULTIPLIER_COUNT; m++) {
    for (int re = 0; re < ref_edge_count; re++) {
      float M[3][3];

      if (!p_edge_matrix(M, ref_edges[re])) {
        continue;
      }

      float corr_co[3][3];
      PEdge *e = f->edge;

      for (int i = 0; i < 3; i++) {
        const float angle = (float(i) / 3.0f) * float(2.0 * M_PI);
        float corr_dir[3] = {0.0f, cos(angle), sin(angle)};

        float corr_len_multiplied = corr_len * (m + 1);

        mul_m3_v3(M, corr_dir);
        mul_v3_fl(corr_dir, corr_len_multiplied);

        copy_v3_v3(corr_co[i], e->vert->co);
        add_v3_v3(corr_co[i], corr_dir);
        e = e->next;
      }

      e = f->edge;
      for (int i = 0; i < 3; i++) {
        if (!p_validate_corrected_coords_point(
                e, corr_co[i], corr_co[(i + 1) % 3], min_area, min_angle_cos))
        {
          return false;
        }

        e = e->next;
      }

      e = f->edge;
      for (int i = 0; i < 3; i++) {
        copy_v3_v3(e->vert->co, corr_co[i]);
        e = e->next;
      }

      corr_co_found = true;
      break;
    }

    if (corr_co_found) {
      break;
    }
  }

  if (!corr_co_found) {
    return false;
  }

  f->flag |= PFACE_DONE;
  return true;
}

static bool p_chart_correct_degenerate_triangles2(PChart *chart, float min_area, float min_angle)
{
  static const float eps = 1.0e-6;

  float min_angle_cos = std::cos(min_angle);
  float min_angle_sin = std::sin(min_angle + CORR_ZERO_AREA_EPS);

  for (PFace *f = chart->faces; f; f = f->nextlink) {
    if (f->flag & PFACE_DONE) {
      continue;
    }

    float face_area = p_face_area(f);

    PEdge *max_edge = nullptr;
    float max_edge_len = -std::numeric_limits<float>::infinity();

    PEdge *min_edge = nullptr;
    float min_edge_len = std::numeric_limits<float>::infinity();

    PEdge *middle_edge = nullptr;

    PEdge *e = f->edge;
    do {
      float len = p_edge_length(e);

      if (len > max_edge_len) {
        max_edge = e;
        max_edge_len = len;

        middle_edge = max_edge->next == min_edge ? min_edge->next : max_edge->next;
      }

      if (len < min_edge_len) {
        min_edge = e;
        min_edge_len = len;

        middle_edge = min_edge->next == max_edge ? max_edge->next : min_edge->next;
      }

      e = e->next;
    } while (e != f->edge);

    BLI_assert(max_edge);
    BLI_assert(min_edge);
    BLI_assert(middle_edge);

    bool small_uniside_tri = (face_area <= min_area) && (min_edge == max_edge);

    if ((max_edge_len < eps) || small_uniside_tri) {
      p_chart_correct_degenerate_triangle_point(f, min_area, min_angle_cos);
      continue;
    }

    if (min_edge == max_edge) {
      BLI_assert(face_area > min_area);
      f->flag |= PFACE_DONE;
      continue;
    }

    BLI_assert(middle_edge != max_edge);
    BLI_assert(middle_edge != min_edge);

    float M[3][3];
    if (!p_edge_matrix(M, max_edge)) {
      continue;
    }

    float max_face_cos =
        middle_edge->next == max_edge ?
            p_vec_cos(middle_edge->vert->co, max_edge->vert->co, min_edge->vert->co) :
            p_vec_cos(max_edge->vert->co, middle_edge->vert->co, min_edge->vert->co);

    float angle_corr_len = max_face_cos > min_angle_cos ?
                               p_edge_length(middle_edge) * min_angle_sin :
                               0.0f;

    if ((face_area > min_area) && (angle_corr_len == 0.0f)) {
      f->flag |= PFACE_DONE;
      continue;
    }

    float corr_len = (min_area + CORR_ZERO_AREA_EPS) * 2.0f / max_edge_len;
    corr_len = std::max(corr_len, angle_corr_len);

    PEdge *corr_e = max_edge->next->next;
    PVert *corr_v = corr_e->vert;

    /* Check 4 distinct directions. */
    static const constexpr int DIR_COUNT = 16;
    static const constexpr int LEN_MULTIPLIER_COUNT = 2;
    float corr_co[3];
    bool corr_co_found = false;

    for (int m = 0; m < LEN_MULTIPLIER_COUNT; m++) {
      for (int d = 0; d < DIR_COUNT; d++) {
        const float angle = (float(d) / DIR_COUNT) * float(2.0 * M_PI);
        float corr_dir[3] = {0.0f, cos(angle), sin(angle)};

        const float corr_len_multiplied = corr_len * (m + 1);

        mul_m3_v3(M, corr_dir);
        mul_v3_fl(corr_dir, corr_len_multiplied);

        copy_v3_v3(corr_co, corr_v->co);
        add_v3_v3(corr_co, corr_dir);

        if (p_validate_corrected_coords(corr_e, corr_co, min_area, min_angle_cos)) {
          corr_co_found = true;
          break;
        }
      }

      if (corr_co_found) {
        break;
      }
    }

    if (!corr_co_found) {
      continue;
    }

    f->flag |= PFACE_DONE;
    copy_v3_v3(corr_v->co, corr_co);
  }

  return true;
}

#ifndef NDEBUG

static bool p_validate_triangle_angles(const PVert *vert1,
                                       const PVert *vert2,
                                       const PVert *vert3,
                                       const float min_angle_cos)
{
  float t_cos[3];
  p_triangle_cos(vert1->co, vert2->co, vert3->co, t_cos, t_cos + 1, t_cos + 2);

  for (int i = 0; i < 3; i++) {
    if (t_cos[i] > min_angle_cos) {
      return false;
    }
  }

  return true;
}

#endif

static bool UNUSED_FUNCTION_NO_SLIM(p_chart_correct_degenerate_triangles)(PChart *chart,
                                                                          const float min_area,
                                                                          const float min_angle)
{
  /* Look for degenerate triangles: triangles with angles lower than `min_angle` or having area
   * lower than `min_area` and try to correct vertex coordinates so that the resulting triangle is
   * not degenerate.
   *
   * The return value indicates whether all triangles could be corrected.
   */

  bool ret = p_chart_correct_degenerate_triangles2(chart, min_area, min_angle);

#ifndef NDEBUG
  float min_angle_cos = std::cos(min_angle - CORR_ZERO_AREA_EPS);
#endif

  for (PFace *f = chart->faces; f; f = f->nextlink) {
    if (!(f->flag & PFACE_DONE)) {
      ret = false;
    }
    else {
#ifndef NDEBUG
      float f_area = p_face_area(f);
      BLI_assert(f_area > (min_area - CORR_ZERO_AREA_EPS));

      PVert *vert1 = f->edge->vert;
      PVert *vert2 = f->edge->next->vert;
      PVert *vert3 = f->edge->next->next->vert;

      BLI_assert(p_validate_triangle_angles(vert1, vert2, vert3, min_angle_cos));
#endif
    }

    f->flag &= ~PFACE_DONE;
  }

  return ret;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name SLIM Implementation (Private API)
 * \{ */

#ifdef WITH_UV_SLIM

/**
 * Get SLIM parameters from the scene.
 */
static slim::MatrixTransfer *slim_matrix_transfer(const ParamSlimOptions *slim_options)
{
  slim::MatrixTransfer *mt = new slim::MatrixTransfer();

  mt->use_weights = slim_options->weight_influence > 0.0f;
  mt->weight_influence = slim_options->weight_influence;
  mt->n_iterations = slim_options->iterations;
  mt->reflection_mode = slim_options->no_flip;
  mt->skip_initialization = slim_options->skip_init;

  return mt;
}

/**
 * For one chart, allocate memory. If no accurate estimate
 * (e.g. for number of pinned vertices) overestimate and correct later.
 */
static void slim_allocate_matrices(const PChart *chart, slim::MatrixTransferChart *mt_chart)
{
  mt_chart->verts_num = chart->nverts;
  mt_chart->faces_num = chart->nfaces;
  mt_chart->edges_num = chart->nedges;

  mt_chart->v_matrices.resize(mt_chart->verts_num * 3);
  mt_chart->uv_matrices.resize(mt_chart->verts_num * 2);
  mt_chart->pp_matrices.resize(mt_chart->verts_num * 2);

  mt_chart->f_matrices.resize(mt_chart->faces_num * 3);
  mt_chart->p_matrices.resize(mt_chart->verts_num);
  mt_chart->b_vectors.resize(mt_chart->verts_num);
  /* Also clear memory for weight vectors, hence 'new' followed by `()`. */
  mt_chart->w_vectors.resize(mt_chart->verts_num, 0.0);

  mt_chart->e_matrices.resize(mt_chart->edges_num * 2 * 2);
  mt_chart->el_vectors.resize(mt_chart->edges_num * 2);
}

/**
 * Transfer edges and edge lengths.
 */
static void slim_transfer_edges(PChart *chart, slim::MatrixTransferChart *mt_chart)
{
  std::vector<int> &E = mt_chart->e_matrices;
  std::vector<double> &EL = mt_chart->el_vectors;

  PEdge *outer;
  p_chart_boundaries(chart, &outer);

  PEdge *be = outer;
  int eid = 0;

  static const float DOUBLED_VERT_THRESHOLD = 1.0e-5;

  do {
    E[eid] = be->vert->slim_id;
    const float edge_len = p_edge_length(be);
    EL[eid] = edge_len;

    /* Temporary solution : SLIM doesn't support doubled vertices for now. */
    if (edge_len < DOUBLED_VERT_THRESHOLD) {
      mt_chart->succeeded = false;
    }

    be = p_boundary_edge_next(be);
    E[eid + mt_chart->edges_num + mt_chart->boundary_vertices_num] = be->vert->slim_id;
    eid++;

  } while (be != outer);

  for (PEdge *e = chart->edges; e; e = e->nextlink) {
    PEdge *e1 = e->next;

    E[eid] = e->vert->slim_id;
    const float edge_len = p_edge_length(e);
    EL[eid] = edge_len;

    /* Temporary solution : SLIM doesn't support doubled vertices for now. */
    if (edge_len < DOUBLED_VERT_THRESHOLD) {
      mt_chart->succeeded = false;
    }

    E[eid + mt_chart->edges_num + mt_chart->boundary_vertices_num] = e1->vert->slim_id;
    eid++;
  }
}

/**
 * Transfer vertices and pinned information.
 */
static void slim_transfer_vertices(const PChart *chart,
                                   slim::MatrixTransferChart *mt_chart,
                                   slim::MatrixTransfer *mt)
{
  int r = mt_chart->verts_num;
  std::vector<double> &v_mat = mt_chart->v_matrices;
  std::vector<double> &uv_mat = mt_chart->uv_matrices;
  std::vector<int> &p_mat = mt_chart->p_matrices;
  std::vector<double> &pp_mat = mt_chart->pp_matrices;
  std::vector<float> &w_vec = mt_chart->w_vectors;

  int p_vid = 0;
  int vid = mt_chart->boundary_vertices_num;

  /* For every vertex, fill up V matrix and P matrix (pinned vertices) */
  for (PVert *v = chart->verts; v; v = v->nextlink) {
    if (!v->on_boundary_flag) {
      if (mt->use_weights) {
        w_vec[vid] = v->weight;
      }

      v->slim_id = vid;
      vid++;
    }

    v_mat[v->slim_id] = v->co[0];
    v_mat[r + v->slim_id] = v->co[1];
    v_mat[2 * r + v->slim_id] = v->co[2];

    uv_mat[v->slim_id] = v->uv[0];
    uv_mat[r + v->slim_id] = v->uv[1];

    if (v->flag & PVERT_PIN || (mt->is_minimize_stretch && !(v->flag & PVERT_SELECT))) {
      mt_chart->pinned_vertices_num += 1;

      p_mat[p_vid] = v->slim_id;
      pp_mat[2 * p_vid] = double(v->uv[0]);
      pp_mat[2 * p_vid + 1] = double(v->uv[1]);
      p_vid += 1;
    }
  }
}

/**
 * Transfer boundary vertices.
 */
static void slim_transfer_boundary_vertices(PChart *chart,
                                            slim::MatrixTransferChart *mt_chart,
                                            const slim::MatrixTransfer *mt)
{
  std::vector<int> &b_vec = mt_chart->b_vectors;
  std::vector<float> &w_vec = mt_chart->w_vectors;

  /* For every vertex, set slim_flag to 0 */
  for (PVert *v = chart->verts; v; v = v->nextlink) {
    v->on_boundary_flag = false;
  }

  /* Find vertices on boundary and save into vector B */
  PEdge *outer;
  p_chart_boundaries(chart, &outer);

  PEdge *be = outer;
  int vid = 0;

  do {
    if (mt->use_weights) {
      w_vec[vid] = be->vert->weight;
    }

    mt_chart->boundary_vertices_num += 1;
    be->vert->slim_id = vid;
    be->vert->on_boundary_flag = true;
    b_vec[vid] = vid;

    vid += 1;
    be = p_boundary_edge_next(be);

  } while (be != outer);
}

/**
 * Transfer faces.
 */
static void slim_transfer_faces(const PChart *chart, slim::MatrixTransferChart *mt_chart)
{
  int fid = 0;

  for (PFace *f = chart->faces; f; f = f->nextlink) {
    PEdge *e = f->edge;
    PEdge *e1 = e->next;
    PEdge *e2 = e1->next;

    int r = mt_chart->faces_num;
    std::vector<int> &F = mt_chart->f_matrices;

    F[fid] = e->vert->slim_id;
    F[r + fid] = e1->vert->slim_id;
    F[2 * r + fid] = e2->vert->slim_id;
    fid++;
  }
}

/**
 * Conversion Function to build matrix for SLIM Parametrization.
 */
static void slim_convert_blender(ParamHandle *phandle, slim::MatrixTransfer *mt)
{
  static const float SLIM_CORR_MIN_AREA = 1.0e-8;
  static const float SLIM_CORR_MIN_ANGLE = DEG2RADF(1.0f);

  mt->charts.resize(phandle->ncharts);

  for (int i = 0; i < phandle->ncharts; i++) {
    PChart *chart = phandle->charts[i];
    slim::MatrixTransferChart *mt_chart = &mt->charts[i];

    p_chart_correct_degenerate_triangles(chart, SLIM_CORR_MIN_AREA, SLIM_CORR_MIN_ANGLE);

    mt_chart->succeeded = true;
    mt_chart->pinned_vertices_num = 0;
    mt_chart->boundary_vertices_num = 0;

    /* Allocate memory for matrices of Vertices,Faces etc. for each chart. */
    slim_allocate_matrices(chart, mt_chart);

    /* For each chart, fill up matrices. */
    slim_transfer_boundary_vertices(chart, mt_chart, mt);
    slim_transfer_vertices(chart, mt_chart, mt);
    slim_transfer_edges(chart, mt_chart);
    slim_transfer_faces(chart, mt_chart);

    mt_chart->pp_matrices.resize(mt_chart->pinned_vertices_num * 2);
    mt_chart->pp_matrices.shrink_to_fit();

    mt_chart->p_matrices.resize(mt_chart->pinned_vertices_num);
    mt_chart->p_matrices.shrink_to_fit();

    mt_chart->b_vectors.resize(mt_chart->boundary_vertices_num);
    mt_chart->b_vectors.shrink_to_fit();

    mt_chart->e_matrices.resize((mt_chart->edges_num + mt_chart->boundary_vertices_num) * 2);
    mt_chart->e_matrices.shrink_to_fit();
  }
}

static void slim_transfer_data_to_slim(ParamHandle *phandle, const ParamSlimOptions *slim_options)
{
  slim::MatrixTransfer *mt = slim_matrix_transfer(slim_options);

  slim_convert_blender(phandle, mt);
  phandle->slim_mt = mt;
}

/**
 * Set UV on each vertex after SLIM parametrization, for each chart.
 */
static void slim_flush_uvs(ParamHandle *phandle,
                           slim::MatrixTransfer *mt,
                           int *count_changed,
                           int *count_failed,
                           bool live = false)
{
  int vid;
  PVert *v;

  for (int i = 0; i < phandle->ncharts; i++) {
    PChart *chart = phandle->charts[i];
    slim::MatrixTransferChart *mt_chart = &mt->charts[i];

    if (mt_chart->succeeded) {
      if (count_changed) {
        (*count_changed)++;
      }

      const std::vector<double> &UV = mt_chart->uv_matrices;
      for (v = chart->verts; v; v = v->nextlink) {
        if (v->flag & PVERT_PIN) {
          continue;
        }

        vid = v->slim_id;
        v->uv[0] = UV[vid];
        v->uv[1] = UV[mt_chart->verts_num + vid];
      }
    }
    else {
      if (count_failed) {
        (*count_failed)++;
      }

      if (!live) {
        for (v = chart->verts; v; v = v->nextlink) {
          v->uv[0] = 0.0f;
          v->uv[1] = 0.0f;
        }
      }
    }
  }
}

/**
 * Cleanup memory.
 */
static void slim_free_matrix_transfer(ParamHandle *phandle)
{
  delete phandle->slim_mt;
  phandle->slim_mt = nullptr;
}

static void slim_get_pinned_vertex_data(ParamHandle *phandle,
                                        PChart *chart,
                                        slim::MatrixTransferChart &mt_chart,
                                        slim::PinnedVertexData &pinned_vertex_data)
{
  std::vector<int> &pinned_vertex_indices = pinned_vertex_data.pinned_vertex_indices;
  std::vector<double> &pinned_vertex_positions_2D = pinned_vertex_data.pinned_vertex_positions_2D;
  std::vector<int> &selected_pins = pinned_vertex_data.selected_pins;

  pinned_vertex_indices.clear();
  pinned_vertex_positions_2D.clear();
  selected_pins.clear();

  /* Boundary vertices have lower slim_ids, process them first. */
  PEdge *outer;
  p_chart_boundaries(chart, &outer);
  PEdge *be = outer;
  do {
    if (be->vert->flag & PVERT_PIN) {
      /* Reload vertex position. */
      p_vert_load_pin_select_uvs(phandle, be->vert);

      if (be->vert->flag & PVERT_SELECT) {
        selected_pins.push_back(be->vert->slim_id);
      }

      pinned_vertex_indices.push_back(be->vert->slim_id);
      pinned_vertex_positions_2D.push_back(be->vert->uv[0]);
      pinned_vertex_positions_2D.push_back(be->vert->uv[1]);
    }
    be = p_boundary_edge_next(be);
  } while (be != outer);

  PVert *v;
  for (v = chart->verts; v; v = v->nextlink) {
    if (!v->on_boundary_flag && (v->flag & PVERT_PIN)) {
      /* Reload `v`. */
      p_vert_load_pin_select_uvs(phandle, v);

      if (v->flag & PVERT_SELECT) {
        selected_pins.push_back(v->slim_id);
      }
      pinned_vertex_indices.push_back(v->slim_id);
      pinned_vertex_positions_2D.push_back(v->uv[0]);
      pinned_vertex_positions_2D.push_back(v->uv[1]);
    }
  }

  mt_chart.pinned_vertices_num = pinned_vertex_indices.size();
}

#endif /* WITH_UV_SLIM */

/** \} */

/* -------------------------------------------------------------------- */
/** \name SLIM Integration (Public API)
 * \{ */

void uv_parametrizer_slim_solve(ParamHandle *phandle,
                                const ParamSlimOptions *slim_options,
                                int *count_changed,
                                int *count_failed)
{
#ifdef WITH_UV_SLIM
  slim_transfer_data_to_slim(phandle, slim_options);
  slim::MatrixTransfer *mt = phandle->slim_mt;

  mt->parametrize();

  slim_flush_uvs(phandle, mt, count_changed, count_failed);
  slim_free_matrix_transfer(phandle);
#else
  *count_changed = 0;
  *count_failed = 0;
  UNUSED_VARS(phandle, slim_options, count_changed, count_failed);
#endif /* !WITH_UV_SLIM */
}

void uv_parametrizer_slim_live_begin(ParamHandle *phandle, const ParamSlimOptions *slim_options)
{
#ifdef WITH_UV_SLIM
  slim_transfer_data_to_slim(phandle, slim_options);
  slim::MatrixTransfer *mt = phandle->slim_mt;

  for (int i = 0; i < phandle->ncharts; i++) {
    for (PFace *f = phandle->charts[i]->faces; f; f = f->nextlink) {
      p_face_backup_uvs(f);
    }
  }

  for (int i = 0; i < phandle->ncharts; i++) {
    PChart *chart = phandle->charts[i];
    slim::MatrixTransferChart &mt_chart = mt->charts[i];

    bool select = false, deselect = false;

    /* Give vertices matrix indices and count pins. */
    for (PVert *v = chart->verts; v; v = v->nextlink) {
      if (v->flag & PVERT_PIN) {
        if (v->flag & PVERT_SELECT) {
          select = true;
        }
      }

      if (!(v->flag & PVERT_SELECT)) {
        deselect = true;
      }
    }

    if (!select || !deselect) {
      chart->skip_flush = true;
      mt_chart.succeeded = false;
      continue;
    }

    mt->setup_slim_data(mt_chart);
  }
#else
  UNUSED_VARS(phandle, slim_options);
#endif /* !WITH_UV_SLIM */
}

void uv_parametrizer_slim_stretch_iteration(ParamHandle *phandle, const float blend)
{
#ifdef WITH_UV_SLIM
  slim::MatrixTransfer *mt = phandle->slim_mt;

  /* Do one iteration and transfer UVs. */
  for (int i = 0; i < phandle->ncharts; i++) {
    mt->charts[i].parametrize_single_iteration();
    mt->charts[i].transfer_uvs_blended(blend);
  }

  /* Assign new UVs back to each vertex. */
  slim_flush_uvs(phandle, mt, nullptr, nullptr);
#else
  UNUSED_VARS(phandle, blend);
#endif /* !WITH_UV_SLIM */
}

void uv_parametrizer_slim_live_solve_iteration(ParamHandle *phandle)
{
#ifdef WITH_UV_SLIM
  slim::MatrixTransfer *mt = phandle->slim_mt;

  /* Do one iteration and transfer UVs */
  for (int i = 0; i < phandle->ncharts; i++) {
    PChart *chart = phandle->charts[i];
    slim::MatrixTransferChart &mt_chart = mt->charts[i];

    if (!mt_chart.data) {
      continue;
    }

    slim_get_pinned_vertex_data(phandle, chart, mt_chart, mt->pinned_vertex_data);

    mt->parametrize_live(mt_chart, mt->pinned_vertex_data);
  }

  /* Assign new UVs back to each vertex. */
  const bool live = true;
  slim_flush_uvs(phandle, mt, nullptr, nullptr, live);
#else
  UNUSED_VARS(phandle);
#endif /* !WITH_UV_SLIM */
}

void uv_parametrizer_slim_live_end(ParamHandle *phandle)
{
#ifdef WITH_UV_SLIM
  slim::MatrixTransfer *mt = phandle->slim_mt;

  for (int i = 0; i < phandle->ncharts; i++) {
    slim::MatrixTransferChart *mt_chart = &mt->charts[i];
    mt_chart->free_slim_data();
  }

  slim_free_matrix_transfer(phandle);
#else
  UNUSED_VARS(phandle);
#endif /* WITH_UV_SLIM */
}

bool uv_parametrizer_is_slim(const ParamHandle *phandle)
{
#ifdef WITH_UV_SLIM
  return phandle->slim_mt != nullptr;
#else
  UNUSED_VARS(phandle);
  return false;
#endif
}

/** \} */

}  // namespace blender::geometry
