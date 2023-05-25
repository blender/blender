#include "MEM_guardedalloc.h"

#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"

#include "BLI_alloca.h"
#include "BLI_array.hh"
#include "BLI_asan.h"
#include "BLI_bitmap.h"
#include "BLI_buffer.h"
#include "BLI_compiler_attrs.h"
#include "BLI_compiler_compat.h"
#include "BLI_ghash.h"
#include "BLI_heap.h"
#include "BLI_heap_minmax.hh"
#include "BLI_heap_simple.h"
#include "BLI_index_range.hh"
#include "BLI_linklist.h"
#include "BLI_map.hh"
#include "BLI_math.h"
#include "BLI_math_vector_types.hh"
#include "BLI_memarena.h"
#include "BLI_rand.h"
#include "BLI_rand.hh"
#include "BLI_set.hh"
#include "BLI_smallhash.h"
#include "BLI_task.h"
#include "BLI_task.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "PIL_time.h"
#include "atomic_ops.h"

#include "BKE_customdata.h"
#include "BKE_dyntopo.hh"
#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BKE_sculpt.hh"

#include "bmesh.h"
#include "bmesh_log.h"

#include "dyntopo_intern.hh"
#include "pbvh_intern.hh"

#include <cstdio>

//#define CLEAR_TAGS_IN_THREAD

namespace blender::bke::dyntopo {

using blender::float2;
using blender::float3;
using blender::float4;
using blender::IndexRange;
using blender::Map;
using blender::Set;
using blender::Vector;
using namespace blender::bke::sculpt;

static void pbvh_split_edges(
    struct EdgeQueueContext *eq_ctx, PBVH *pbvh, BMesh *bm, BMEdge **edges, int totedge);

static void edge_queue_create_local(EdgeQueueContext *eq_ctx,
                                    PBVH *pbvh,
                                    PBVHTopologyUpdateMode local_mode);

static void surface_smooth_v_safe(
    SculptSession *ss, PBVH *pbvh, BMVert *v, float fac, bool reproject_cdata)
{
  float co[3];
  float origco[3], origco1[3];
  float origno1[3];
  float tan[3];
  float tot = 0.0;

  PBVHVertRef vertex = {reinterpret_cast<intptr_t>(v)};
  if (stroke_id_test_no_update(ss, vertex, STROKEID_USER_ORIGINAL)) {
    copy_v3_v3(origco1, v->co);
    copy_v3_v3(origno1, v->no);
  }
  else {
    copy_v3_v3(origco1, blender::bke::paint::vertex_attr_ptr<float>(vertex, ss->attrs.orig_co));
    copy_v3_v3(origno1, blender::bke::paint::vertex_attr_ptr<float>(vertex, ss->attrs.orig_no));
  }

  zero_v3(co);
  zero_v3(origco);

  /* This is a manual edge walk. */

  BMEdge *e = v->e;
  if (!e) {
    return;
  }

  if (pbvh_boundary_needs_update_bmesh(pbvh, v)) {
    pbvh_check_vert_boundary(pbvh, v);
  }

  int boundmask = SCULPTVERT_SMOOTH_BOUNDARY;
  int cornermask = SCULPTVERT_SMOOTH_CORNER;

  int boundflag = BM_ELEM_CD_GET_INT(v, pbvh->cd_boundary_flag);
  int bound1 = boundflag & boundmask;

  if (boundflag & (cornermask | SCULPT_BOUNDARY_SHARP_ANGLE)) {
    return;
  }

  if (bound1) {
    fac *= 0.1;
  }

  do {
    BMVert *v2 = e->v1 == v ? e->v2 : e->v1;
    PBVHVertRef vertex2 = {reinterpret_cast<intptr_t>(v2)};

    float w = len_squared_v3v3(e->v1->co, e->v2->co);

    /* Note: we can't validate the boundary flags from with a thread
     * so they may not be up to date.
     */

    int boundflag2 = BM_ELEM_CD_GET_INT(v2, pbvh->cd_boundary_flag);
    int bound2 = boundflag2 & boundmask;

    if (bound1 && !bound2) {
      continue;
    }

    sub_v3_v3v3(tan, v2->co, v->co);
    float d = dot_v3v3(tan, v->no);

    madd_v3_v3fl(tan, v->no, -d * 0.95f);
    madd_v3_v3fl(co, tan, w);

    if (!stroke_id_test_no_update(ss, vertex2, STROKEID_USER_ORIGINAL)) {
      sub_v3_v3v3(
          tan, blender::bke::paint::vertex_attr_ptr<float>(vertex2, ss->attrs.orig_co), origco1);
    }
    else {
      sub_v3_v3v3(tan, v2->co, origco1);
    }

    d = dot_v3v3(tan, origno1);
    madd_v3_v3fl(tan, origno1, -d * 0.95f);
    madd_v3_v3fl(origco, tan, w);

    tot += w;

  } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);

  if (tot == 0.0f) {
    return;
  }

  float startco[3];
  float startno[3];
  if (reproject_cdata) {
    copy_v3_v3(startco, v->co);
    copy_v3_v3(startno, v->no);
  }

  mul_v3_fl(co, 1.0f / tot);
  mul_v3_fl(origco, 1.0f / tot);

  volatile float x = v->co[0], y = v->co[1], z = v->co[2];
  volatile float nx = x + co[0] * fac, ny = y + co[1] * fac, nz = z + co[2] * fac;

  /* Conflicts here should be pretty rare. */
  atomic_cas_float(&v->co[0], x, nx);
  atomic_cas_float(&v->co[1], y, ny);
  atomic_cas_float(&v->co[2], z, nz);

  /* Note: threading is disabled if reproject_cdata is on. */
  if (reproject_cdata) {
    BKE_sculpt_reproject_cdata(ss, vertex, startco, startno);
  }

  float *start_origco = blender::bke::paint::vertex_attr_ptr<float>(vertex, ss->attrs.orig_co);

  /* Conflicts here should be pretty rare. */
  x = start_origco[0];
  y = start_origco[1];
  z = start_origco[2];

  nx = x + origco[0] * fac;
  ny = y + origco[1] * fac;
  nz = z + origco[2] * fac;

  atomic_cas_float(&start_origco[0], x, nx);
  atomic_cas_float(&start_origco[1], y, ny);
  atomic_cas_float(&start_origco[2], z, nz);

  PBVH_CHECK_NAN(start_origco);
  PBVH_CHECK_NAN(v->co);
  // atomic_cas_int32(&mv1->stroke_id, stroke_id, pbvh->stroke_id);
}

/****************************** EdgeQueue *****************************/

static float maskcb_get(EdgeQueueContext *eq_ctx, BMVert *v1, BMVert *v2)
{
  if (eq_ctx->mask_cb) {
    PBVHVertRef sv1 = {(intptr_t)v1};
    PBVHVertRef sv2 = {(intptr_t)v2};

    float w1 = eq_ctx->mask_cb(sv1, eq_ctx->mask_cb_data);
    float w2 = eq_ctx->mask_cb(sv2, eq_ctx->mask_cb_data);

    return min_ff(w1, w2);
  }

  return 1.0f;
}

enum WeightMode {
  SPLIT = -1,
  COLLAPSE = 1,
};

BLI_INLINE float calc_weighted_length(EdgeQueueContext *eq_ctx,
                                      BMVert *v1,
                                      BMVert *v2,
                                      WeightMode mode)
{
  float w = 1.0 - maskcb_get(eq_ctx, v1, v2);
  float len = len_squared_v3v3(v1->co, v2->co);

  w = 1.0 + w * float(mode);
  return len * w;
}

static PBVHTopologyUpdateMode edge_queue_test(EdgeQueueContext *eq_ctx,
                                              PBVH * /*pbvh*/,
                                              BMEdge *e,
                                              float *r_w)
{
  float len1 = calc_weighted_length(eq_ctx, e->v1, e->v2, SPLIT);
  if ((eq_ctx->mode & PBVH_Subdivide) && len1 > eq_ctx->limit_len_max_sqr) {
    if (r_w) {
      *r_w = len1;
    }
    return PBVH_Subdivide;
  }

  float len2 = calc_weighted_length(eq_ctx, e->v1, e->v2, COLLAPSE);
  if ((eq_ctx->mode & PBVH_Collapse) && len2 < eq_ctx->limit_len_min_sqr) {
    if (r_w) {
      *r_w = len2;
    }
    return PBVH_Collapse;
  }

  return PBVH_None;
}

static void edge_queue_insert_unified(EdgeQueueContext *eq_ctx, BMEdge *e, float w)
{
  if (!(e->head.hflag & BM_ELEM_TAG)) {
    eq_ctx->edge_heap.insert(w, e);
    e->head.hflag |= BM_ELEM_TAG;
  }
}

static void edge_queue_insert_val34_vert(EdgeQueueContext *eq_ctx, BMVert *v)
{
  eq_ctx->used_verts.append(v);
}

/*
  Profiling revealed the accurate distance to tri in blenlib was too slow,
  so we use a simpler version here
  */
/* reduce script

on factor;
off period;

load_package "avector";

comment: origin at p;

p := avec(0, 0, 0);
n :=- avec(nx, ny, nz);
v1 := avec(v1x, v1y, v1z);
v2 := avec(v2x, v2y, v2z);
v3 := avec(v3x, v3y, v3z);

comment: -((p - v1) dot n);simplified to this;
fac := v1 dot n;

co := fac*n;

a := co - v1;
b := co - v2;
c := co - v3;

a := v1;
b := v2;
c := v3;

t1 := a cross b;
t2 := b cross c;
t3 := c cross a;

on fort;
w1 := t1 dot n;
w2 := t2 dot n;
w3 := t3 dot n;
off fort;

inside := sign(a dot n) + sign(b dot n) + sign(c dot n);


*/
static bool point_in_tri_v3(float p[3], float v1[3], float v2[3], float v3[3], float n[3])
{
  float t1[3], t2[3], t3[3];
  sub_v3_v3v3(t1, v1, p);
  sub_v3_v3v3(t2, v2, p);
  sub_v3_v3v3(t3, v3, p);

  float c1[3], c2[3], c3[3];
  cross_v3_v3v3(c1, t1, t2);
  cross_v3_v3v3(c2, t2, t3);
  cross_v3_v3v3(c3, t3, t1);

  bool w1 = dot_v3v3(c1, n) >= 0.0f;
  bool w2 = dot_v3v3(c2, n) >= 0.0f;
  bool w3 = dot_v3v3(c3, n) >= 0.0f;

  return w1 == w2 && w2 == w3;

#if 0
  const float nx = n[0], ny = n[1], nz = n[2];

  float v1x = v1[0] - p[0], v1y = v1[1] - p[1], v1z = v1[2] - p[2];
  float v2x = v2[0] - p[0], v2y = v2[1] - p[1], v2z = v2[2] - p[2];
  float v3x = v3[0] - p[0], v3y = v3[1] - p[1], v3z = v3[2] - p[2];

  const float w1 = -(nx * v1y * v2z - nx * v1z * v2y - ny * v1x * v2z + ny * v1z * v2x +
                     nz * v1x * v2y - nz * v1y * v2x);
  const float w2 = -(nx * v2y * v3z - nx * v2z * v3y - ny * v2x * v3z + ny * v2z * v3x +
                     nz * v2x * v3y - nz * v2y * v3x);
  const float w3 = nx * v1y * v3z - nx * v1z * v3y - ny * v1x * v3z + ny * v1z * v3x +
                   nz * v1x * v3y - nz * v1y * v3x;
  return !((w1 >= 0.0f) && (w2 >= 0.0f) && (w3 >= 0.0f));
#endif
}

float dist_to_tri_sphere_simple(float p[3], float v1[3], float v2[3], float v3[3], float n[3])
{
  float co[3];
  float t1[3], t2[3], t3[3];

  if (dot_v3v3(n, n) == 0.0f) {
    normal_tri_v3(n, v1, v2, v3);
  }

  if (point_in_tri_v3(p, v1, v2, v3, n)) {
    sub_v3_v3v3(co, p, v2);

    float dist = dot_v3v3(co, n);
    return dist * dist;
  }

  sub_v3_v3v3(co, p, v1);
  madd_v3_v3fl(co, n, -dot_v3v3(n, co));

  sub_v3_v3v3(t1, v1, co);
  sub_v3_v3v3(t2, v2, co);
  sub_v3_v3v3(t3, v3, co);

  float dis = len_squared_v3v3(p, v1);
  dis = fmin(dis, len_squared_v3v3(p, v2));
  dis = fmin(dis, len_squared_v3v3(p, v3));

  add_v3_v3v3(co, v1, v2);
  mul_v3_fl(co, 0.5f);
  dis = fmin(dis, len_squared_v3v3(p, co));

  add_v3_v3v3(co, v2, v3);
  mul_v3_fl(co, 0.5f);
  dis = fmin(dis, len_squared_v3v3(p, co));

  add_v3_v3v3(co, v3, v1);
  mul_v3_fl(co, 0.5f);
  dis = fmin(dis, len_squared_v3v3(p, co));

  add_v3_v3v3(co, v1, v2);
  add_v3_v3(co, v3);
  mul_v3_fl(co, 1.0f / 3.0f);
  dis = fmin(dis, len_squared_v3v3(p, co));

  return dis;
}

static bool skinny_bad_edge(BMEdge *e, const float limit = 3.0f)
{
  float len1 = len_v3v3(e->v1->co, e->v2->co);

  BMLoop *l = e->l;
  do {
    float len2 = len_v3v3(l->next->v->co, l->next->next->v->co);
    if (len1 > 0.0f && len2 / len1 > limit) {
      return true;
    }

    len2 = len_v3v3(l->v->co, l->prev->v->co);
    if (len1 > 0.0f && len2 / len1 > limit) {
      return true;
    }
  } while ((l = l->radial_next) != e->l);

  return false;
}

#if 0
static void add_split_edge_recursive(
    EdgeQueueContext *eq_ctx, BMLoop *l_edge, const float len_sq, float limit_len, int depth)
{
  struct StackItem {
    BMLoop *l_edge;
    float len_sq;
    float limit_len;
    int depth;

    StackItem(BMLoop *l, float len, float limit, int d)
        : l_edge(l), len_sq(len), limit_len(limit), depth(d)
    {
    }
  };

  Vector<StackItem, 32> stack;
  stack.append(StackItem(l_edge, len_sq, limit_len, depth));

  while (stack.size() > 0) {
#  if 0  // stack
    StackItem item = stack.pop_last();
#  else  // queue
    StackItem item = stack[0];
    stack.remove_and_reorder(0);
#  endif

    int depth = item.depth;
    float len_sq = item.len_sq, limit_len = item.limit_len;
    BMLoop *l_edge = item.l_edge;

    if (depth > DEPTH_START_LIMIT && eq_ctx->use_view_normal) {
      if (dot_v3v3(l_edge->f->no, eq_ctx->view_normal) < 0.0f) {
        continue;
      }
    }

    if (!skinny_bad_edge(l_edge->e)) {
      edge_queue_insert_unified(eq_ctx, l_edge->e, len_sq);
    }

    if ((l_edge->radial_next != l_edge)) {
      const float len_sq_cmp = len_sq * EVEN_EDGELEN_THRESHOLD;

      limit_len *= EVEN_GENERATION_SCALE;
      const float limit_len_sq = square_f(limit_len);

      BMLoop *l_iter = l_edge;
      do {
        BMLoop *l_adjacent[2] = {l_iter->next, l_iter->prev};
        for (int i = 0; i < (int)ARRAY_SIZE(l_adjacent); i++) {
          if (l_adjacent[i]->e->head.hflag & BM_ELEM_TAG) {
            continue;
          }

          float len_sq_other = calc_weighted_length(
              eq_ctx, l_adjacent[i]->e->v1, l_adjacent[i]->e->v2, SPLIT);

          bool insert_ok = len_sq_other > max_ff(len_sq_cmp, limit_len_sq);
          if (!insert_ok) {
            continue;
          }

          stack.append(StackItem(l_adjacent[i]->radial_next, len_sq_other, limit_len, depth + 1));
        }
      } while ((l_iter = l_iter->radial_next) != l_edge);
    }
  }
}
#else
static void add_split_edge_recursive(
    EdgeQueueContext *eq_ctx, BMLoop *l_edge, const float len_sq, float limit_len, int depth)
{
  if (depth > DEPTH_START_LIMIT && eq_ctx->use_view_normal) {
    if (dot_v3v3(l_edge->f->no, eq_ctx->view_normal) < 0.0f) {
      return;
    }
  }

  if (!skinny_bad_edge(l_edge->e)) {
    edge_queue_insert_unified(eq_ctx, l_edge->e, len_sq);
  }

  if ((l_edge->radial_next != l_edge)) {
    const float len_sq_cmp = len_sq * EVEN_EDGELEN_THRESHOLD;

    limit_len *= EVEN_GENERATION_SCALE;
    const float limit_len_sq = square_f(limit_len);

    BMLoop *l_iter = l_edge;
    do {
      BMLoop *l_adjacent[2] = {l_iter->next, l_iter->prev};
      for (int i = 0; i < (int)ARRAY_SIZE(l_adjacent); i++) {
        if (l_adjacent[i]->e->head.hflag & BM_ELEM_TAG) {
          continue;
        }

        float len_sq_other = calc_weighted_length(
            eq_ctx, l_adjacent[i]->e->v1, l_adjacent[i]->e->v2, SPLIT);

        bool insert_ok = len_sq_other > max_ff(len_sq_cmp, limit_len_sq);
        if (!insert_ok) {
          continue;
        }

        add_split_edge_recursive(
            eq_ctx, l_adjacent[i]->radial_next, len_sq_other, limit_len, depth + 1);
      }
    } while ((l_iter = l_iter->radial_next) != l_edge);
  }
}
#endif

typedef struct EdgeQueueThreadData {
  PBVH *pbvh;
  PBVHNode *node;
  Vector<BMEdge *> edges;
  EdgeQueueContext *eq_ctx;
  int size;
  bool is_collapse;
  int seed;
} EdgeQueueThreadData;

static void edge_thread_data_insert(EdgeQueueThreadData *tdata, BMEdge *e)
{
  tdata->edges.append(e);

  BMElem elem;
  memcpy(&elem, (BMElem *)e, sizeof(BMElem));

  elem.head.hflag = e->head.hflag | BM_ELEM_TAG;
  int64_t iold = *((int64_t *)&e->head.index);
  int64_t inew = *((int64_t *)&elem.head.index);

  atomic_cas_int64((int64_t *)&e->head.index, iold, inew);
}

static void add_split_edge_recursive_threaded(EdgeQueueThreadData *tdata,
                                              BMLoop *l_edge,
                                              BMLoop *l_end,
                                              const float len_sq,
                                              float limit_len,
                                              int depth,
                                              bool insert)
{
  BLI_assert(len_sq > square_f(limit_len));

  BMLoop *l = l_edge;
  int count = 0;
  do {
    if (count++ > 5) {
      printf("%s: topology error: highly non-manifold edge %p\n", __func__, l_edge->e);
      BM_vert_select_set(tdata->pbvh->header.bm, l_edge->e->v1, true);
      BM_vert_select_set(tdata->pbvh->header.bm, l_edge->e->v2, true);
      BM_edge_select_set(tdata->pbvh->header.bm, l_edge->e, true);
      return;
    }
  } while ((l = l->radial_next) != l_edge);

  if (l_edge->e->head.hflag & BM_ELEM_TAG) {
    // return;
  }

#ifdef USE_EDGEQUEUE_FRONTFACE
  if (depth > DEPTH_START_LIMIT && tdata->eq_ctx->use_view_normal) {
    if (dot_v3v3(l_edge->f->no, tdata->eq_ctx->view_normal) < 0.0f) {
      return;
    }
  }
#endif

  if (insert) {
    edge_thread_data_insert(tdata, l_edge->e);
  }

  if ((l_edge->radial_next != l_edge)) {
    const float len_sq_cmp = len_sq * EVEN_EDGELEN_THRESHOLD;

    limit_len *= EVEN_GENERATION_SCALE;
    const float limit_len_sq = square_f(limit_len);

    BMLoop *l_iter = l_edge;
    do {
      BMLoop *l_adjacent[2] = {l_iter->next, l_iter->prev};
      for (int i = 0; i < (int)ARRAY_SIZE(l_adjacent); i++) {
        float len_sq_other = calc_weighted_length(
            tdata->eq_ctx, l_adjacent[i]->e->v1, l_adjacent[i]->e->v2, SPLIT);

        bool insert_ok = len_sq_other > max_ff(len_sq_cmp, limit_len_sq);
#ifdef EVEN_NO_TEST_DEPTH_LIMIT
        if (!insert_ok && depth >= EVEN_NO_TEST_DEPTH_LIMIT) {
          continue;
        }
#else
        if (!insert_ok) {
          continue;
        }
#endif

        add_split_edge_recursive_threaded(tdata,
                                          l_adjacent[i]->radial_next,
                                          l_adjacent[i],
                                          len_sq_other,
                                          limit_len,
                                          depth + 1,
                                          insert_ok);
      }
    } while ((l_iter = l_iter->radial_next) != l_end);
  }
}

static void unified_edge_queue_task_cb(void *__restrict userdata,
                                       const int n,
                                       const TaskParallelTLS *__restrict /*tls*/)
{
  blender::RandomNumberGenerator rand(uint32_t(n + PIL_check_seconds_timer() * 100000.0f));
  EdgeQueueThreadData *tdata = ((EdgeQueueThreadData *)userdata) + n;
  PBVH *pbvh = tdata->pbvh;
  PBVHNode *node = tdata->node;
  EdgeQueueContext *eq_ctx = tdata->eq_ctx;

  bool do_smooth = eq_ctx->surface_smooth_fac > 0.0f;

  BKE_pbvh_bmesh_check_tris(pbvh, node);
  int ni = node - pbvh->nodes;

  const char facetag = BM_ELEM_TAG_ALT;

  /* Only do reprojection if UVs exist. */
  bool reproject_cdata = eq_ctx->reproject_cdata;

/*
 * Clear edge flags.
 *
 * We care more about convergence to accurate results
 * then accuracy in any individual runs.  Profiling
 * has shown this loop overwhelms the L3 cache,
 * so randomly skip bits of it.
 */
#ifdef CLEAR_TAGS_IN_THREAD
  BMFace *f;
  TGSET_ITER (f, node->bm_faces) {
    BMLoop *l = f->l_first;

    /* Note that f itself is owned by this node. */
    f->head.hflag &= ~facetag;

    /* Stochastically skip faces. */
    if (rand.get_uint32() > (1 << 16)) {
      continue;
    }

    do {
      /* Kind of tricky to atomicly update flags here. We probably
       * don't need to do this on x86, but I'm not sure about ARM.
       */
      BMEdge edge = *l->e;
      edge.head.hflag &= ~BM_ELEM_TAG;

      int64_t *t1 = (int64_t *)&edge.head.index;
      int64_t *t2 = (int64_t *)&l->e->head.index;

      atomic_cas_int64(t2, *t2, *t1);

      l = l->next;
    } while (l != f->l_first);
  }
  TGSET_ITER_END
#endif

  PBVHTriBuf *tribuf = node->tribuf;
  for (int i = 0; i < node->tribuf->tottri; i++) {
    PBVHTri *tri = node->tribuf->tris + i;
    BMFace *f = (BMFace *)tri->f.i;

    if (f->head.hflag & facetag) {
      continue;
    }

#ifdef USE_EDGEQUEUE_FRONTFACE
    if (eq_ctx->use_view_normal) {
      if (dot_v3v3(f->no, eq_ctx->view_normal) < 0.0f) {
        continue;
      }
    }
#endif

    BMVert *vs[3] = {(BMVert *)tribuf->verts[tri->v[0]].i,
                     (BMVert *)tribuf->verts[tri->v[1]].i,
                     (BMVert *)tribuf->verts[tri->v[2]].i};
    if (eq_ctx->brush_tester->tri_in_range(vs, f->no)) {
      f->head.hflag |= facetag;

      /* Check each edge of the face. */
      BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
      BMLoop *l_iter = l_first;
      do {
        /* Are we owned by this node? if so, make sure origdata is up to date. */
        if (BM_ELEM_CD_GET_INT(l_iter->v, pbvh->cd_vert_node_offset) == ni) {
          BKE_pbvh_bmesh_check_origdata(eq_ctx->ss, l_iter->v, pbvh->stroke_id);
        }

        /* Try to improve convergence by applying a small amount of smoothing to topology,
         * but tangentially to surface.  We can stochastically skip this and still get the
         * benefit to convergence.
         */
        int randval = rand.get_uint32();

        if (do_smooth && randval > (1 << 16)) {
          PBVHVertRef sv = {(intptr_t)l_iter->v};
          surface_smooth_v_safe(eq_ctx->ss,
                                tdata->pbvh,
                                l_iter->v,
                                eq_ctx->surface_smooth_fac *
                                    eq_ctx->mask_cb(sv, eq_ctx->mask_cb_data),
                                reproject_cdata);
        }

        float w = 0.0f;
        PBVHTopologyUpdateMode mode = edge_queue_test(eq_ctx, pbvh, l_iter->e, &w);

        /* Subdivide walks the mesh a bit for better transitions in the topology. */
        if (mode == PBVH_Subdivide) {
          add_split_edge_recursive_threaded(
              tdata, l_iter->radial_next, l_iter, w, eq_ctx->limit_len_max, 0, true);
        }
        else if (mode == PBVH_Collapse) {
          edge_thread_data_insert(tdata, l_iter->e);
        }
      } while ((l_iter = l_iter->next) != l_first);
    }
  }
}

bool check_face_is_tri(PBVH *pbvh, BMFace *f)
{
#if DYNTOPO_DISABLE_FLAG & DYNTOPO_DISABLE_TRIANGULATOR
  return true;
#endif

  if (f->len == 3) {
    return true;
  }

  if (f->len < 3) {
    printf("pbvh had < 3 vert face!\n");
    BKE_pbvh_bmesh_remove_face(pbvh, f, false);
    return false;
  }

  bm_logstack_push();

  LinkNode *dbl = nullptr;

  Vector<BMFace *, 32> fs;
  Vector<BMEdge *, 32> es;

  BMLoop *l = f->l_first;
  do {
    validate_vert(pbvh, pbvh->header.bm, l->v, true, true);

    if (l->e->head.index == -1) {
      l->e->head.index = 0;
    }
  } while ((l = l->next) != f->l_first);

  // BKE_pbvh_bmesh_remove_face(pbvh, f, true);
  pbvh_bmesh_face_remove(pbvh, f, false, true, true);
  BM_log_face_removed(pbvh->header.bm, pbvh->bm_log, f);
  BM_idmap_release(pbvh->bm_idmap, (BMElem *)f, true);

  int len = (f->len - 2) * 3;

  fs.resize(len);
  es.resize(len);

  int totface = 0;
  int totedge = 0;
  MemArena *arena = nullptr;
  struct Heap *heap = nullptr;

  arena = BLI_memarena_new(512, "ngon arena");
  heap = BLI_heap_new();

  BM_face_triangulate(pbvh->header.bm,
                      f,
                      fs.data(),
                      &totface,
                      es.data(),
                      &totedge,
                      &dbl,
                      MOD_TRIANGULATE_QUAD_FIXED,
                      MOD_TRIANGULATE_NGON_BEAUTY,
                      false,
                      arena,
                      heap);

  while (totface && dbl) {
    BMFace *f2 = (BMFace *)dbl->link;
    LinkNode *next = dbl->next;

    for (int i = 0; i < totface; i++) {
      if (fs[i] == f2) {
        fs[i] = nullptr;
      }
    }

    if (f == f2) {
      BM_log_face_added(pbvh->header.bm, pbvh->bm_log, f);
      BM_log_face_removed_no_check(pbvh->header.bm, pbvh->bm_log, f);
      f = nullptr;
    }

    BM_idmap_release(pbvh->bm_idmap, (BMElem *)dbl->link, true);
    BM_face_kill(pbvh->header.bm, (BMFace *)dbl->link);

    MEM_freeN(dbl);
    dbl = next;
  }

  for (int i = 0; i < totface; i++) {
    BMFace *f2 = fs[i];

    if (!f2) {
      continue;
    }

    BMLoop *l = f2->l_first;
    do {
      dyntopo_add_flag(pbvh, l->v, SCULPTFLAG_NEED_DISK_SORT | SCULPTFLAG_NEED_VALENCE);
    } while ((l = l->next) != f2->l_first);
  }

  for (int i = 0; i < totface; i++) {
    BMFace *f2 = fs[i];

    if (!f2) {
      continue;
    }

    if (f == f2) {
      printf("%s: error\n", __func__);
      continue;
    }

    /* Detect new edges. */
    BMLoop *l = f2->l_first;
    do {
      if (l->e->head.index == -1) {
        BM_log_edge_added(pbvh->header.bm, pbvh->bm_log, l->e);
        l->e->head.index = 0;
      }
    } while ((l = l->next) != f2->l_first);

    validate_face(pbvh, pbvh->header.bm, f2, false, true);

    BKE_pbvh_bmesh_add_face(pbvh, f2, false, true);
    // BM_log_face_post(pbvh->bm_log, f2);
    BM_log_face_added(pbvh->header.bm, pbvh->bm_log, f2);
  }

  if (f) {
    BKE_pbvh_bmesh_add_face(pbvh, f, false, true);
    BM_log_face_added(pbvh->header.bm, pbvh->bm_log, f);
  }

  if (arena) {
    BLI_memarena_free(arena);
  }

  if (heap) {
    BLI_heap_free(heap, nullptr);
  }

  bm_logstack_pop();

  return false;
}

bool destroy_nonmanifold_fins(PBVH *pbvh, BMEdge *e_root)
{
#if !(DYNTOPO_DISABLE_FLAG & DYNTOPO_DISABLE_FIN_REMOVAL)
  bm_logstack_push();

  static int max_faces = 64;
  Vector<BMFace *, 32> stack;

  BMLoop *l = e_root->l;
  Vector<BMLoop *, 5> ls;
  Vector<BMFace *, 32> minfs;

  if (!l) {
    bm_logstack_pop();
    return false;
  }

  do {
    ls.append(l);
  } while ((l = l->radial_next) != e_root->l);

  for (int i = 0; i < ls.size(); i++) {
    Set<BMFace *, 64> visit;

    BMLoop *l = ls[i];
    BMFace *f = l->f;
    Vector<BMFace *, 32> fs2;
    stack.clear();

    stack.append(f);
    fs2.append(f);

    visit.add(f);

    bool bad = false;

    while (stack.size() > 0) {
      f = stack.pop_last();
      BMLoop *l = f->l_first;

      do {
        if (l->radial_next == l || l->radial_next->radial_next != l) {
          continue;
        }

        BMFace *f2 = l->radial_next->f;

        if (visit.add(f2)) {
          if (fs2.size() > max_faces) {
            bad = true;
            break;
          }

          stack.append(f2);
          fs2.append(f2);
        }
      } while ((l = l->next) != f->l_first);

      if (bad) {
        break;
      }
    }

    if (!bad && fs2.size() && (minfs.size() == 0 || fs2.size() < minfs.size())) {
      minfs = fs2;
    }
  }

  int nupdateflag = PBVH_UpdateOtherVerts | PBVH_UpdateDrawBuffers | PBVH_UpdateBB |
                    PBVH_UpdateTriAreas;
  nupdateflag = nupdateflag | PBVH_UpdateNormals | PBVH_UpdateTris | PBVH_RebuildDrawBuffers;

  if (!minfs.size()) {
    bm_logstack_pop();
    return false;
  }

  // printf("manifold fin size: %d\n", (int)minfs.size());
  const int tag = BM_ELEM_TAG_ALT;

  for (int i = 0; i < minfs.size(); i++) {
    BMFace *f = minfs[i];

    BMLoop *l = f->l_first;
    do {
      BMLoop *l2 = l;
      do {
        BMLoop *l3 = l2;
        do {
          l3->v->head.hflag &= ~tag;
          l3->e->head.hflag &= ~tag;
        } while ((l3 = l3->next) != l2);
      } while ((l2 = l2->radial_next) != l);

      l->v->head.hflag &= ~tag;
      l->e->head.hflag &= ~tag;
    } while ((l = l->next) != f->l_first);
  }

  Vector<BMVert *, 32> vs;
  Vector<BMEdge *, 32> es;

  for (int i = 0; i < minfs.size(); i++) {
    BMFace *f = minfs[i];

    BMLoop *l = f->l_first;
    do {
      if (!(l->v->head.hflag & tag)) {
        l->v->head.hflag |= tag;
        vs.append(l->v);
      }

      if (!(l->e->head.hflag & tag)) {
        l->e->head.hflag |= tag;
        es.append(l->e);
      }
    } while ((l = l->next) != f->l_first);
  }

  for (int i = 0; i < minfs.size(); i++) {
    for (int j = 0; j < minfs.size(); j++) {
      if (i != j && minfs[i] == minfs[j]) {
        printf("%s: duplicate faces\n", __func__);
        continue;
      }
    }
  }

  for (int i = 0; i < minfs.size(); i++) {
    BMFace *f = minfs[i];

    if (f->head.htype != BM_FACE) {
      printf("%s: corruption!\n", __func__);
      continue;
    }

    int ni = BM_ELEM_CD_GET_INT(f, pbvh->cd_face_node_offset);
    if (ni >= 0 && ni < pbvh->totnode) {
      pbvh->nodes[ni].flag |= (PBVHNodeFlags)nupdateflag;
    }

    pbvh_bmesh_face_remove(pbvh, f, true, false, false);
    BM_idmap_release(pbvh->bm_idmap, (BMElem *)f, true);
    BM_face_kill(pbvh->header.bm, f);
  }

  const int mupdateflag = SCULPTFLAG_NEED_DISK_SORT | SCULPTFLAG_NEED_VALENCE;

  for (int i = 0; i < es.size(); i++) {
    BMEdge *e = es[i];

    if (!e->l) {
      BM_log_edge_removed(pbvh->header.bm, pbvh->bm_log, e);
      BM_idmap_release(pbvh->bm_idmap, (BMElem *)e, true);
      BM_edge_kill(pbvh->header.bm, e);
    }
  }

  for (int i = 0; i < vs.size(); i++) {
    BMVert *v = vs[i];

    if (!v->e) {
      pbvh_bmesh_vert_remove(pbvh, v);

      BM_log_vert_removed(pbvh->header.bm, pbvh->bm_log, v);
      BM_idmap_release(pbvh->bm_idmap, (BMElem *)v, true);
      BM_vert_kill(pbvh->header.bm, v);
    }
    else {
      pbvh_boundary_update_bmesh(pbvh, v);
      dyntopo_add_flag(pbvh, v, mupdateflag);
    }
  }

  bm_logstack_pop();
  return true;
#else
  return false;
#endif
}

bool check_for_fins(PBVH *pbvh, BMVert *v)
{
  BMEdge *e = v->e;
  if (!e) {
    return false;
  }

  bm_logstack_push();

  do {
    if (!e) {
      printf("%s: e was nullptr\n", __func__);
      break;
    }
    if (e->l) {
      BMLoop *l = e->l->f->l_first;

      do {
        if (l != l->radial_next && l != l->radial_next->radial_next) {
          if (destroy_nonmanifold_fins(pbvh, e)) {
            bm_logstack_pop();
            return true;
          }
        }
      } while ((l = l->next) != e->l->f->l_first);
    }
  } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);

  bm_logstack_pop();
  return false;
}

bool check_vert_fan_are_tris(PBVH *pbvh, BMVert *v)
{
  uint8_t *flag = BM_ELEM_CD_PTR<uint8_t *>(v, pbvh->cd_flag);

  if (!(*flag & SCULPTFLAG_NEED_TRIANGULATE)) {
    return true;
  }

  bm_log_message("  == triangulate == ");

  Vector<BMFace *, 32> fs;

  validate_vert(pbvh, pbvh->header.bm, v, true, true);

  if (v->head.htype != BM_VERT) {
    printf("non-vert %p fed to %s\n", v, __func__);
    return false;
  }

  BMIter iter;
  BMFace *f;
  BM_ITER_ELEM (f, &iter, v, BM_FACES_OF_VERT) {
    BMLoop *l = f->l_first;

    do {
      pbvh_boundary_update_bmesh(pbvh, l->v);
      dyntopo_add_flag(pbvh, l->v, SCULPTFLAG_NEED_VALENCE | SCULPTFLAG_NEED_DISK_SORT);
    } while ((l = l->next) != f->l_first);
    fs.append(f);

    if (BM_elem_is_free((BMElem *)f, BM_FACE)) {
      printf("%s: corrupted face error!\n", __func__);
    }
  }

  *flag &= ~SCULPTFLAG_NEED_TRIANGULATE;

  for (int i = 0; i < fs.size(); i++) {
    /* Triangulation can sometimes delete a face. */
    if (!BM_elem_is_free((BMElem *)fs[i], BM_FACE)) {
      check_face_is_tri(pbvh, fs[i]);
    }
  }

  return false;
}

/* Create a priority queue containing vertex pairs connected by a long
 * edge as defined by PBVH.bm_max_edge_len.
 *
 * Only nodes marked for topology update are checked, and in those
 * nodes only edges used by a face intersecting the (center, radius)
 * sphere are checked.
 *
 * The highest priority (lowest number) is given to the longest edge.
 */
static void unified_edge_queue_create(EdgeQueueContext *eq_ctx,
                                      PBVH *pbvh,
                                      PBVHTopologyUpdateMode local_mode)
{
  if (local_mode) {
    edge_queue_create_local(eq_ctx, pbvh, local_mode);
    return;
  }

#ifdef USE_EDGEQUEUE_TAG_VERIFY
  pbvh_bmesh_edge_tag_verify(pbvh);
#endif

  Vector<EdgeQueueThreadData> tdata;

  bool push_subentry = false;

  for (int n = 0; n < pbvh->totnode; n++) {
    PBVHNode *node = &pbvh->nodes[n];

    /* Check leaf nodes marked for topology update */
    bool ok = ((node->flag & PBVH_Leaf) && (node->flag & PBVH_UpdateTopology) &&
               !(node->flag & PBVH_FullyHidden));

    if (!ok) {
      continue;
    }

    EdgeQueueThreadData td = {};

    td.seed = BLI_thread_rand(0);
    td.pbvh = pbvh;
    td.node = node;
    td.eq_ctx = eq_ctx;

    tdata.append(td);
  }

  int count = tdata.size();

  TaskParallelSettings settings;

  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = !eq_ctx->reproject_cdata;

#ifdef DYNTOPO_NO_THREADING
  settings.use_threading = false;
#endif

#ifndef CLEAR_TAGS_IN_THREAD
  for (int i : IndexRange(pbvh->totnode)) {
    PBVHNode *node = &pbvh->nodes[i];

    if (!(node->flag & PBVH_Leaf) || !(node->flag & PBVH_UpdateTopology)) {
      continue;
    }

    BMFace *f;

    TGSET_ITER (f, node->bm_faces) {
      BMLoop *l = f->l_first;
      do {
        l->e->head.hflag &= ~BM_ELEM_TAG;
        l->v->head.hflag &= ~BM_ELEM_TAG;
        l->f->head.hflag &= ~(BM_ELEM_TAG | BM_ELEM_TAG_ALT);
      } while ((l = l->next) != f->l_first);
    }
    TGSET_ITER_END;
  }
#endif

  BLI_task_parallel_range(0, count, (void *)tdata.data(), unified_edge_queue_task_cb, &settings);

  for (int i = 0; i < count; i++) {
    for (BMEdge *e : tdata[i].edges) {
      e->head.hflag &= ~BM_ELEM_TAG;
    }
  }

  Vector<BMVert *> verts;
  for (int i = 0; i < count; i++) {
    for (BMEdge *e : tdata[i].edges) {
      if (bm_elem_is_free((BMElem *)e, BM_EDGE)) {
        continue;
      }

      e->head.hflag &= ~BM_ELEM_TAG;

      if (e->l && e->l != e->l->radial_next->radial_next) {
        /* Fix non-manifold "fins". */
        destroy_nonmanifold_fins(pbvh, e);
        push_subentry = true;

        if (bm_elem_is_free((BMElem *)e, BM_EDGE)) {
          continue;
        }
      }

      if (dyntopo_test_flag(pbvh, e->v1, SCULPTFLAG_NEED_VALENCE)) {
        BKE_pbvh_bmesh_update_valence(pbvh, {(intptr_t)e->v1});
      }

      if (dyntopo_test_flag(pbvh, e->v2, SCULPTFLAG_NEED_VALENCE)) {
        BKE_pbvh_bmesh_update_valence(pbvh, {(intptr_t)e->v2});
      }

      if (eq_ctx->use_view_normal && (dot_v3v3(e->v1->no, eq_ctx->view_normal) < 0.0f &&
                                      dot_v3v3(e->v2->no, eq_ctx->view_normal) < 0.0f))
      {
        continue;
      }

      verts.append(e->v1);
      verts.append(e->v2);

      e->v1->head.hflag |= BM_ELEM_TAG;
      e->v2->head.hflag |= BM_ELEM_TAG;

      float w;
      if (edge_queue_test(eq_ctx, pbvh, e, &w)) {
        edge_queue_insert_unified(eq_ctx, e, w);
      }
    }
  }

  for (BMVert *v : verts) {
    if (v->head.hflag & BM_ELEM_TAG) {
      v->head.hflag &= ~BM_ELEM_TAG;

      edge_queue_insert_val34_vert(eq_ctx, v);
    }
  }

  for (BMEdge *e : eq_ctx->edge_heap.values()) {
    e->head.hflag |= BM_ELEM_TAG;
  }

  if (push_subentry) {
    BM_log_entry_add_ex(pbvh->header.bm, pbvh->bm_log, true);
  }
}

static void short_edge_queue_task_cb_local(void *__restrict userdata,
                                           const int n,
                                           const TaskParallelTLS *__restrict /*tls*/)
{
  EdgeQueueThreadData *tdata = ((EdgeQueueThreadData *)userdata) + n;
  PBVHNode *node = tdata->node;
  EdgeQueueContext *eq_ctx = tdata->eq_ctx;

  BMFace *f;

  TGSET_ITER (f, node->bm_faces) {
#ifdef USE_EDGEQUEUE_FRONTFACE
    if (eq_ctx->use_view_normal) {
      if (dot_v3v3(f->no, eq_ctx->view_normal) < 0.0f) {
        continue;
      }
    }
#endif

    BMVert *vs[3] = {f->l_first->v, f->l_first->next->v, f->l_first->next->next->v};
    if (eq_ctx->brush_tester->tri_in_range(vs, f->no)) {
      BMLoop *l = f->l_first;

      do {
        edge_thread_data_insert(tdata, l->e);

      } while ((l = l->next) != f->l_first);
    }
  }
  TGSET_ITER_END
}

static void edge_queue_create_local(EdgeQueueContext *eq_ctx,
                                    PBVH *pbvh,
                                    PBVHTopologyUpdateMode local_mode)
{
  eq_ctx->local_mode = true;

  Vector<EdgeQueueThreadData> tdata;

  for (int n = 0; n < pbvh->totnode; n++) {
    PBVHNode *node = &pbvh->nodes[n];
    EdgeQueueThreadData td;

    if ((node->flag & PBVH_Leaf) && (node->flag & PBVH_UpdateTopology) &&
        !(node->flag & PBVH_FullyHidden))
    {
      memset(&td, 0, sizeof(td));
      td.pbvh = pbvh;
      td.node = node;
      td.is_collapse = local_mode & PBVH_LocalCollapse;
      td.eq_ctx = eq_ctx;

      tdata.append(td);
    }
  }

  int count = tdata.size();

  TaskParallelSettings settings;

  BLI_parallel_range_settings_defaults(&settings);
#ifdef DYNTOPO_NO_THREADING
  settings.use_threading = false;
#endif

  BLI_task_parallel_range(
      0, count, (void *)tdata.data(), short_edge_queue_task_cb_local, &settings);

  Vector<float> lens;
  Vector<BMEdge *> edges;

  for (int i = 0; i < count; i++) {
    for (BMEdge *e : tdata[i].edges) {
      e->head.hflag &= ~BM_ELEM_TAG;
    }
  }

  for (int i = 0; i < count; i++) {
    for (BMEdge *e : tdata[i].edges) {
      e->v1->head.hflag &= ~BM_ELEM_TAG;
      e->v2->head.hflag &= ~BM_ELEM_TAG;

      if (!(e->head.hflag & BM_ELEM_TAG)) {
        edges.append(e);
        e->head.hflag |= BM_ELEM_TAG;
      }
    }
  }

  for (int i = 0; i < edges.size(); i++) {
    BMEdge *e = edges[i];
    float len = len_v3v3(e->v1->co, e->v2->co);

    for (int j = 0; j < 2; j++) {
      BMVert *v = j ? e->v2 : e->v1;

      if (!(local_mode & PBVH_LocalCollapse)) {
        if (!(v->head.hflag & BM_ELEM_TAG)) {
          v->head.hflag |= BM_ELEM_TAG;

          if (dyntopo_test_flag(pbvh, v, SCULPTFLAG_NEED_VALENCE)) {
            BKE_pbvh_bmesh_update_valence(pbvh, {(intptr_t)v});
          }

          edge_queue_insert_val34_vert(eq_ctx, v);
        }
      }
    }

    e->head.index = i;
    lens.append(len);
  }

  /* Make sure tags around border edges are unmarked. */
  for (int i = 0; i < edges.size(); i++) {
    BMEdge *e = edges[i];

    for (int j = 0; j < 2; j++) {
      BMVert *v1 = j ? e->v2 : e->v1;
      BMEdge *e1 = v1->e;

      do {
        e1->head.hflag &= ~BM_ELEM_TAG;

        e1 = BM_DISK_EDGE_NEXT(e1, v1);
      } while (e1 != v1->e);
    }
  }

  /* Re-tag edge list. */
  for (int i = 0; i < edges.size(); i++) {
    edges[i]->head.hflag |= BM_ELEM_TAG;
  }

  int totstep = 3;

  /* Blur edge lengths. */
  for (int step = 0; step < totstep; step++) {
    for (int i = 0; i < edges.size(); i++) {
      BMEdge *e = edges[i];

      float len = lens[i];
      float totlen = 0.0f;

      for (int j = 0; j < 2; j++) {
        BMVert *v1 = j ? e->v2 : e->v1;
        BMEdge *e1 = v1->e;

        do {
          if (e1->head.hflag & BM_ELEM_TAG) {
            len += lens[e1->head.index];
            totlen += 1.0f;
          }

          e1 = BM_DISK_EDGE_NEXT(e1, v1);
        } while (e1 != v1->e);
      }

      if (totlen != 0.0f) {
        len /= totlen;
        lens[i] += (len - lens[i]) * 0.5;
      }
    }
  }

  pbvh->header.bm->elem_index_dirty |= BM_EDGE;

  float limit = 0.0f;
  float tot = 0.0f;

  for (int i = 0; i < edges.size(); i++) {
    BMEdge *e = edges[i];

    e->head.hflag &= ~BM_ELEM_TAG;

    pbvh_check_vert_boundary(pbvh, e->v1);
    pbvh_check_vert_boundary(pbvh, e->v2);

    int boundflag1 = BM_ELEM_CD_GET_INT(e->v1, pbvh->cd_boundary_flag);
    int boundflag2 = BM_ELEM_CD_GET_INT(e->v2, pbvh->cd_boundary_flag);

    if ((boundflag1 & SCULPTVERT_ALL_CORNER) || (boundflag2 & SCULPTVERT_ALL_CORNER)) {
      continue;
    }

    if ((boundflag1 & SCULPTVERT_ALL_BOUNDARY) != (boundflag2 & SCULPTVERT_ALL_BOUNDARY)) {
      continue;
    }

    limit += lens[i];
    tot += 1.0f;
  }

  if (tot > 0.0f) {
    limit /= tot;

    if (local_mode & PBVH_LocalCollapse) {
      eq_ctx->limit_len_min = limit * pbvh->bm_detail_range;
      eq_ctx->limit_len_min_sqr = eq_ctx->limit_len_min * eq_ctx->limit_len_min;
    }

    if (local_mode & PBVH_LocalSubdivide) {
      eq_ctx->limit_len_max = limit;
      eq_ctx->limit_len_max_sqr = eq_ctx->limit_len_max * eq_ctx->limit_len_max;
    }
  }

  for (int i = 0; i < edges.size(); i++) {
    BMEdge *e = edges[i];

    int boundflag1 = BM_ELEM_CD_GET_INT(e->v1, pbvh->cd_boundary_flag);
    int boundflag2 = BM_ELEM_CD_GET_INT(e->v2, pbvh->cd_boundary_flag);

    if ((boundflag1 & SCULPTVERT_ALL_CORNER) || (boundflag2 & SCULPTVERT_ALL_CORNER)) {
      continue;
    }

    if ((boundflag1 & SCULPTVERT_ALL_BOUNDARY) != (boundflag2 & SCULPTVERT_ALL_BOUNDARY)) {
      continue;
    }

    bool ok = false;

    bool a = eq_ctx->mode & (PBVH_Subdivide | PBVH_LocalSubdivide);
    bool b = eq_ctx->mode & (PBVH_Collapse | PBVH_LocalCollapse);

    float len1 = calc_weighted_length(eq_ctx, e->v1, e->v2, COLLAPSE);
    float len2 = calc_weighted_length(eq_ctx, e->v1, e->v2, SPLIT);
    float w = 0.0f;

    if (a && b) {
      ok = len1 < eq_ctx->limit_len_min_sqr || len1 > eq_ctx->limit_len_max_sqr;
      ok = ok || (len2 < pbvh->bm_min_edge_len || len2 > pbvh->bm_max_edge_len);
      w = (len1 + len2) * 0.5;
    }
    else if (a) {
      ok = len1 > eq_ctx->limit_len_max || len1 > pbvh->bm_max_edge_len;
      w = len1;
    }
    else if (b) {
      ok = len2 < eq_ctx->limit_len_min || len2 < pbvh->bm_min_edge_len;
      w = len2;
    }

    if (!ok) {
      continue;
    }

    edge_queue_insert_unified(eq_ctx, e, w);
  }
}

static bool cleanup_valence_3_4(EdgeQueueContext *ectx, PBVH *pbvh)
{
  bool modified = false;

  bm_logstack_push();

  bm_log_message("  == cleanup_valence_3_4 == ");

  const int cd_vert_node = pbvh->cd_vert_node_offset;

  int updateflag = SCULPTFLAG_NEED_DISK_SORT | SCULPTFLAG_NEED_VALENCE;

  for (BMVert *v : ectx->used_verts) {
    if (bm_elem_is_free((BMElem *)v, BM_VERT)) {
      continue;
    }

    const int n = BM_ELEM_CD_GET_INT(v, cd_vert_node);
    if (n == DYNTOPO_NODE_NONE) {
      continue;
    }

    PBVHVertRef sv = {(intptr_t)v};
    if (!ectx->brush_tester->vert_in_range(v) || !v->e ||
        ectx->mask_cb(sv, ectx->mask_cb_data) < 0.5f) {
      continue;
    }

    validate_vert(pbvh, pbvh->header.bm, v, false, true);
    check_vert_fan_are_tris(pbvh, v);
    pbvh_check_vert_boundary(pbvh, v);
    validate_vert(pbvh, pbvh->header.bm, v, true, true);

    BKE_pbvh_bmesh_check_valence(pbvh, {(intptr_t)v});

    int val = BM_ELEM_CD_GET_INT(v, pbvh->cd_valence);
    if (val != 4 && val != 3) {
      continue;
    }

    int boundflag = BM_ELEM_CD_GET_INT(v, pbvh->cd_boundary_flag);

    if (boundflag & SCULPTVERT_ALL_BOUNDARY) {
      continue;
    }

    BMIter iter;
    BMLoop *l;
    BMLoop *ls[4];
    BMVert *vs[4];

    l = v->e->l;

    if (!l) {
      continue;
    }

    if (l->v != v) {
      l = l->next;
    }

    bool bad = false;
    int ls_i = 0;

    /* Don't dissolve verts if attached to long edges, to avoid
     * preventing subdivision convergence.
     */
    BMEdge *e = v->e;
    do {
      float len = calc_weighted_length(ectx, e->v1, e->v2, COLLAPSE);
      if (sqrtf(len) > ectx->limit_len_max * 1.2f) {
        bad = true;
        break;
      }
    } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);

    if (bad) {
      continue;
    }

    for (int j = 0; j < val; j++) {
      ls[ls_i++] = l->v == v ? l->next : l;

      if (l->v == v) {
        dyntopo_add_flag(pbvh, l->next->v, updateflag);
        pbvh_boundary_update_bmesh(pbvh, l->next->v);
      }
      else {
        dyntopo_add_flag(pbvh, l->v, updateflag);
        pbvh_boundary_update_bmesh(pbvh, l->v);
      }

      l = l->prev->radial_next;

      if (l->v != v) {
        l = l->next;
      }

      /* Ignore non-manifold edges along with ones flagged as sharp. */
      if (l->radial_next == l || l->radial_next->radial_next != l ||
          !(l->e->head.hflag & BM_ELEM_SMOOTH))
      {
        bad = true;
        break;
      }

      if (l->radial_next != l && l->radial_next->v == l->v) {
        bad = true; /* Bad normals. */
        break;
      }

      for (int k = 0; k < j; k++) {
        if (ls[k]->v == ls[j]->v) {
          if (ls[j]->next->v != v) {
            ls[j] = ls[j]->next;
          }
          else {
            bad = true;
            break;
          }
        }

        /* Check for non-manifold edges. */
        if (ls[k] != ls[k]->radial_next->radial_next) {
          bad = true;
          break;
        }

        if (ls[k]->f == ls[j]->f) {
          bad = true;
          break;
        }
      }
    }

    if (bad) {
      continue;
    }

    int ni = BM_ELEM_CD_GET_INT(v, pbvh->cd_vert_node_offset);

    if (ni < 0) {
      continue;
    }

    pbvh_bmesh_vert_remove(pbvh, v);

    BMFace *f;
    BM_ITER_ELEM (f, &iter, v, BM_FACES_OF_VERT) {
      int ni2 = BM_ELEM_CD_GET_INT(f, pbvh->cd_face_node_offset);

      if (ni2 != DYNTOPO_NODE_NONE) {
        pbvh_bmesh_face_remove(pbvh, f, true, true, true);
      }
      else {
        BM_log_face_removed(pbvh->header.bm, pbvh->bm_log, f);
      }
    }

    modified = true;

    if (!v->e) {
      printf("mesh error!\n");
      continue;
    }

    validate_vert(pbvh, pbvh->header.bm, v, false, true);

    l = v->e->l;

    if (val == 4) {
      /* Check which quad diagonal to use to split quad;
       * try to preserve hard edges.
       */

      float n1[3], n2[3], th1, th2;
      normal_tri_v3(n1, ls[0]->v->co, ls[1]->v->co, ls[2]->v->co);
      normal_tri_v3(n2, ls[0]->v->co, ls[2]->v->co, ls[3]->v->co);

      th1 = dot_v3v3(n1, n2);

      normal_tri_v3(n1, ls[1]->v->co, ls[2]->v->co, ls[3]->v->co);
      normal_tri_v3(n2, ls[1]->v->co, ls[3]->v->co, ls[0]->v->co);

      th2 = dot_v3v3(n1, n2);

      if (th1 > th2) {
        BMLoop *ls2[4] = {ls[0], ls[1], ls[2], ls[3]};

        for (int j = 0; j < 4; j++) {
          ls[j] = ls2[(j + 1) % 4];
        }
      }

      if (!BM_edge_exists(ls[0]->v, ls[2]->v)) {
        BMEdge *e_diag = BM_edge_create(
            pbvh->header.bm, ls[0]->v, ls[2]->v, nullptr, BM_CREATE_NOP);
        BM_idmap_check_assign(pbvh->bm_idmap, reinterpret_cast<BMElem *>(e_diag));
        BM_log_edge_added(pbvh->header.bm, pbvh->bm_log, e_diag);
      }
    }

    vs[0] = ls[0]->v;
    vs[1] = ls[1]->v;
    vs[2] = ls[2]->v;

    validate_vert(pbvh, pbvh->header.bm, v, false, false);

    pbvh_boundary_update_bmesh(pbvh, vs[0]);
    pbvh_boundary_update_bmesh(pbvh, vs[1]);
    pbvh_boundary_update_bmesh(pbvh, vs[2]);

    dyntopo_add_flag(pbvh, vs[0], updateflag);
    dyntopo_add_flag(pbvh, vs[1], updateflag);
    dyntopo_add_flag(pbvh, vs[2], updateflag);

    BMFace *f1 = nullptr;
    bool ok1 = vs[0] != vs[1] && vs[1] != vs[2] && vs[0] != vs[2];
    ok1 = ok1 && !BM_face_exists(vs, 3);

    if (ok1) {
      f1 = pbvh_bmesh_face_create(pbvh, n, vs, nullptr, l->f, true, false);
      BM_idmap_check_assign(pbvh->bm_idmap, reinterpret_cast<BMElem *>(f1));

      normal_tri_v3(
          f1->no, f1->l_first->v->co, f1->l_first->next->v->co, f1->l_first->prev->v->co);

      validate_face(pbvh, pbvh->header.bm, f1, false, false);
    }

    BMFace *f2 = nullptr;

    if (val == 4) {
      vs[0] = ls[0]->v;
      vs[1] = ls[2]->v;
      vs[2] = ls[3]->v;
    }

    bool ok2 = val == 4 && vs[0] != vs[2] && vs[2] != vs[3] && vs[0] != vs[3];
    ok2 = ok2 && !BM_face_exists(vs, 3);

    if (ok2) {
      pbvh_boundary_update_bmesh(pbvh, vs[0]);
      pbvh_boundary_update_bmesh(pbvh, vs[1]);
      pbvh_boundary_update_bmesh(pbvh, vs[2]);

      dyntopo_add_flag(pbvh, vs[0], updateflag);
      dyntopo_add_flag(pbvh, vs[1], updateflag);
      dyntopo_add_flag(pbvh, vs[2], updateflag);

      BMFace *example = nullptr;
      if (v->e && v->e->l) {
        example = v->e->l->f;
      }

      f2 = pbvh_bmesh_face_create(pbvh, n, vs, nullptr, example, true, false);
      BM_idmap_check_assign(pbvh->bm_idmap, reinterpret_cast<BMElem *>(f2));

      CustomData_bmesh_swap_data_simple(&pbvh->header.bm->ldata,
                                        &f2->l_first->prev->head.data,
                                        &ls[3]->head.data,
                                        pbvh->bm_idmap->cd_id_off[BM_LOOP]);
      CustomData_bmesh_copy_data(&pbvh->header.bm->ldata,
                                 &pbvh->header.bm->ldata,
                                 ls[0]->head.data,
                                 &f2->l_first->head.data);
      CustomData_bmesh_copy_data(&pbvh->header.bm->ldata,
                                 &pbvh->header.bm->ldata,
                                 ls[2]->head.data,
                                 &f2->l_first->next->head.data);

      normal_tri_v3(
          f2->no, f2->l_first->v->co, f2->l_first->next->v->co, f2->l_first->prev->v->co);
      BM_log_face_added(pbvh->header.bm, pbvh->bm_log, f2);

      validate_face(pbvh, pbvh->header.bm, f2, false, false);
    }

    if (f1) {
      CustomData_bmesh_swap_data_simple(&pbvh->header.bm->ldata,
                                        &f1->l_first->head.data,
                                        &ls[0]->head.data,
                                        pbvh->bm_idmap->cd_id_off[BM_LOOP]);
      CustomData_bmesh_swap_data_simple(&pbvh->header.bm->ldata,
                                        &f1->l_first->next->head.data,
                                        &ls[1]->head.data,
                                        pbvh->bm_idmap->cd_id_off[BM_LOOP]);
      CustomData_bmesh_swap_data_simple(&pbvh->header.bm->ldata,
                                        &f1->l_first->prev->head.data,
                                        &ls[2]->head.data,
                                        pbvh->bm_idmap->cd_id_off[BM_LOOP]);

      BM_log_face_added(pbvh->header.bm, pbvh->bm_log, f1);
    }

    validate_vert(pbvh, pbvh->header.bm, v, false, false);
    pbvh_kill_vert(pbvh, v, true, true);

    if (f1 && !bm_elem_is_free((BMElem *)f1, BM_FACE)) {
      check_face_is_manifold(pbvh, pbvh->header.bm, f1);
    }

    if (f2 && !bm_elem_is_free((BMElem *)f2, BM_FACE)) {
      check_face_is_manifold(pbvh, pbvh->header.bm, f2);
    }
  }

  if (modified) {
    pbvh->header.bm->elem_index_dirty |= BM_VERT | BM_FACE | BM_EDGE;
    pbvh->header.bm->elem_table_dirty |= BM_VERT | BM_FACE | BM_EDGE;
  }

  bm_logstack_pop();

  return modified;
}

static bool do_cleanup_3_4(EdgeQueueContext *eq_ctx, PBVH *pbvh)
{
  bool modified = false;

  eq_ctx->used_verts.clear();

  for (const PBVHNode &node : Span<PBVHNode>(pbvh->nodes, pbvh->totnode)) {
    BMVert *v;

    if (!(node.flag & PBVH_Leaf) || !(node.flag & PBVH_UpdateTopology)) {
      continue;
    }

    TGSET_ITER (v, node.bm_unique_verts) {
      if (dyntopo_test_flag(pbvh, v, SCULPTFLAG_NEED_VALENCE)) {
        BKE_pbvh_bmesh_update_valence(pbvh, {(intptr_t)v});
      }

      bool ok = BM_ELEM_CD_GET_INT(v, pbvh->cd_valence) < 5;
      ok = ok && eq_ctx->brush_tester->vert_in_range(v);

      if (ok) {
        edge_queue_insert_val34_vert(eq_ctx, v);
      }
    }
    TGSET_ITER_END;
  }

  BM_log_entry_add_ex(pbvh->header.bm, pbvh->bm_log, true);

  pbvh_bmesh_check_nodes(pbvh);

  modified |= cleanup_valence_3_4(eq_ctx, pbvh);
  pbvh_bmesh_check_nodes(pbvh);

  return modified;
}

float mask_cb_nop(PBVHVertRef /*vertex*/, void * /*userdata*/)
{
  return 1.0f;
}

EdgeQueueContext::EdgeQueueContext(BrushTester *brush_tester_,
                                   struct SculptSession *ss_,
                                   PBVH *pbvh_,
                                   PBVHTopologyUpdateMode mode_,
                                   bool use_frontface_,
                                   float3 view_normal_,
                                   bool updatePBVH_,
                                   DyntopoMaskCB mask_cb_,
                                   void *mask_cb_data_,
                                   int edge_limit_multiply)
{
  ss = ss_;
  pbvh = pbvh_;
  brush_tester = brush_tester_;
  use_view_normal = use_frontface_;
  view_normal = view_normal_;

  pool = nullptr;
  bm = pbvh->header.bm;
  mask_cb = mask_cb_;
  mask_cb_data = mask_cb_data_;
  view_normal = view_normal_;

  updatePBVH = updatePBVH_;
  cd_vert_mask_offset = pbvh->cd_vert_mask_offset;
  cd_vert_node_offset = pbvh->cd_vert_node_offset;
  cd_face_node_offset = pbvh->cd_face_node_offset;
  local_mode = false;
  mode = mode_;
  reproject_cdata = CustomData_has_layer(&pbvh->header.bm->ldata, CD_PROP_FLOAT2) &&
                    !ss->ignore_uvs;

  max_heap_mm = (DYNTOPO_MAX_ITER * edge_limit_multiply) << 8;
  limit_len_min = pbvh->bm_min_edge_len;
  limit_len_max = pbvh->bm_max_edge_len;
  limit_len_min_sqr = limit_len_min * limit_len_min;
  limit_len_max_sqr = limit_len_max * limit_len_max;
  limit_mid = limit_len_max * 0.5f + limit_len_min * 0.5f;

  surface_smooth_fac = DYNTOPO_SAFE_SMOOTH_FAC;

  if (mode & PBVH_LocalSubdivide) {
    mode |= PBVH_Subdivide;
  }
  if (mode & PBVH_LocalSubdivide) {
    mode |= PBVH_Collapse;
  }

#ifdef DYNTOPO_REPORT
  report();
#endif

#if DYNTOPO_DISABLE_FLAG & DYNTOPO_DISABLE_COLLAPSE
  mode &= ~PBVH_Collapse;
#endif
#if DYNTOPO_DISABLE_FLAG & DYNTOPO_DISABLE_SPLIT_EDGES
  mode &= ~PBVH_Subdivide;
#endif

  if (mode & (PBVH_Subdivide | PBVH_Collapse)) {
    unified_edge_queue_create(this, pbvh, mode & (PBVH_LocalSubdivide | PBVH_LocalCollapse));
  }

  if ((mode & PBVH_Subdivide) && (mode & PBVH_Collapse)) {
    ops[0] = PBVH_Subdivide;
    ops[1] = PBVH_Collapse;
    totop = 2;

    steps[1] = DYNTOPO_MAX_ITER_COLLAPSE;
    steps[0] = DYNTOPO_MAX_ITER_SUBD;
  }
  else if (mode & PBVH_Subdivide) {
    ops[0] = PBVH_Subdivide;
    totop = 1;

    steps[0] = DYNTOPO_MAX_ITER_SUBD;
  }
  else if (mode & PBVH_Collapse) {
    ops[0] = PBVH_Collapse;
    totop = 1;

    steps[0] = DYNTOPO_MAX_ITER_COLLAPSE;
  }

  max_steps = (DYNTOPO_MAX_ITER * edge_limit_multiply) << (totop - 1);
  max_subd = max_steps >> (totop - 1);

  split_edges_size = steps[0];
  split_edges = (BMEdge **)MEM_malloc_arrayN(split_edges_size, sizeof(void *), __func__);
  etot = 0;

  subd_edges.clear();
}

void EdgeQueueContext::flush_subdivision()
{
  if (etot == 0) {
    return;
  }

  modified = true;
  subd_edges.clear();

  pbvh_split_edges(this, pbvh, pbvh->header.bm, split_edges, etot);

  count_subd += etot;
  VALIDATE_LOG(pbvh->bm_log);
  etot = 0;
}

EdgeQueueContext::~EdgeQueueContext()
{
  MEM_SAFE_FREE(split_edges);
}

void EdgeQueueContext::start()
{
  current_i = 0;
}

bool EdgeQueueContext::done()
{
  return !(totop > 0 && !edge_heap.empty() && current_i < max_steps);
}

void EdgeQueueContext::finish()
{
  if (mode & PBVH_Cleanup) {
    modified |= do_cleanup_3_4(this, pbvh);

    VALIDATE_LOG(pbvh->bm_log);
  }

  if (modified) {
    /* Avoid potential infinite loops. */
    const int totnode = pbvh->totnode;

    for (int i = 0; i < totnode; i++) {
      PBVHNode *node = pbvh->nodes + i;

      if ((node->flag & PBVH_Leaf) && (node->flag & PBVH_UpdateTopology) &&
          !(node->flag & PBVH_FullyHidden))
      {

        /* do not clear PBVH_UpdateTopology here in case split messes with it */

        /* Recursively split nodes that have gotten too many
         * elements */
        if (updatePBVH) {
          pbvh_bmesh_node_limit_ensure(pbvh, i);
        }
      }
    }
  }

  /* clear PBVH_UpdateTopology flags */
  for (int i = 0; i < pbvh->totnode; i++) {
    PBVHNode *node = pbvh->nodes + i;

    if (!(node->flag & PBVH_Leaf)) {
      continue;
    }

    node->flag &= ~PBVH_UpdateTopology;
  }

#ifdef USE_VERIFY
  pbvh_bmesh_verify(pbvh);
#endif

  /* Ensure triangulations are all up to date. */
  for (int i = 0; i < pbvh->totnode; i++) {
    PBVHNode *node = pbvh->nodes + i;

    if (node->flag & PBVH_Leaf) {
      pbvh_bmesh_check_other_verts(node);
      BKE_pbvh_bmesh_check_tris(pbvh, node);
    }
  }

  if (modified) {
    BKE_pbvh_update_bounds(pbvh, PBVH_UpdateBB | PBVH_UpdateOriginalBB);
  }

  /* Push a subentry. */
  BM_log_entry_add_ex(pbvh->header.bm, pbvh->bm_log, true);
}

void EdgeQueueContext::step()
{
  if (done()) {
    return;
  }

  BMEdge *e = nullptr;

  if (count >= steps[curop]) {
    if (ops[curop] == PBVH_Subdivide) {  // && count_subd < max_subd) {
      flush_subdivision();
      flushed_ = true;
    }

    curop = (curop + 1) % totop;
    count = 0;
  }

#if 0
  if (curop == 0 && count_subd >= max_subd && totop > 1 && ops[0] == PBVH_Subdivide &&
      ops[1] == PBVH_Collapse)
  {
    if (etot > 0) {
      flush_subdivision();
    }
    curop = 1;
  }
#endif

  switch (ops[curop]) {
    case PBVH_None:
      break;
    case PBVH_Subdivide: {
      if (edge_heap.max_weight() < limit_len_max_sqr) {
        break;
      }

      float w = 0.0f;
      e = edge_heap.pop_max(&w);

      while (!edge_heap.empty() && e &&
             (bm_elem_is_free((BMElem *)e, BM_EDGE) ||
              fabs(calc_weighted_length(this, e->v1, e->v2, SPLIT) - w) > w * 0.1))
      {
        if (e && !bm_elem_is_free((BMElem *)e, BM_EDGE)) {
          e->head.hflag &= ~BM_ELEM_TAG;
          edge_heap.insert(calc_weighted_length(this, e->v1, e->v2, SPLIT), e);
        }

        e = edge_heap.pop_max(&w);
      }

      if (!e || bm_elem_is_free((BMElem *)e, BM_EDGE) || w < limit_len_max_sqr) {
        break;
      }

      e->head.hflag &= ~BM_ELEM_TAG;

#if 0
      if (subd_edges.add(e)) {
        split_edges[etot++] = e;
      }
#else
      /* Add complete faces. */
      BMLoop *l = e->l;
      if (l) {
        do {
          BMLoop *l2 = l;
          do {
            if (etot >= split_edges_size) {
              break;
            }

            if (calc_weighted_length(this, l->e->v1, l->e->v2, SPLIT) < limit_len_max_sqr) {
              continue;
            }

            if (subd_edges.add(l->e)) {
              l->e->head.hflag &= ~BM_ELEM_TAG;
              split_edges[etot++] = l->e;
            }
          } while ((l2 = l2->next) != l);
        } while ((l = l->radial_next) != e->l);
      }
#endif
      break;
    }
    case PBVH_Collapse: {
      if (edge_heap.min_weight() > limit_len_min_sqr) {
        break;
      }

      e = edge_heap.pop_min();
      while (!edge_heap.empty() && e &&
             (bm_elem_is_free((BMElem *)e, BM_EDGE) ||
              calc_weighted_length(this, e->v1, e->v2, COLLAPSE) > limit_len_min_sqr))
      {
        e = edge_heap.pop_min();
      }

      if (!e || bm_elem_is_free((BMElem *)e, BM_EDGE)) {
        break;
      }

      if (bm_elem_is_free((BMElem *)e->v1, BM_VERT) || bm_elem_is_free((BMElem *)e->v2, BM_VERT)) {
        printf("%s: error! operated on freed bmesh elements! e: %p, e->v1: %p, e->v2: %p\n",
               __func__,
               e,
               e->v1,
               e->v2);
        break;
      }

      modified = true;
      pbvh_bmesh_collapse_edge(pbvh, e, e->v1, e->v2, nullptr, nullptr, this);
      flushed_ = true;
      VALIDATE_LOG(pbvh->bm_log);
      break;
    }
    case PBVH_LocalSubdivide:
    case PBVH_LocalCollapse:
    case PBVH_Cleanup:
      BLI_assert_unreachable();
      break;
  }

  if (edge_heap.empty() && etot > 0) {
    /* Flush subdivision, it may add more to queue.*/
    flush_subdivision();
    flushed_ = true;
  }

  count++;
  current_i++;
}

void EdgeQueueContext::report()
{
  BMesh *bm = pbvh->header.bm;

  int vmem = (int)((size_t)bm->totvert * (sizeof(BMVert) + bm->vdata.totsize));
  int emem = (int)((size_t)bm->totedge * (sizeof(BMEdge) + bm->edata.totsize));
  int lmem = (int)((size_t)bm->totloop * (sizeof(BMLoop) + bm->ldata.totsize));
  int fmem = (int)((size_t)bm->totface * (sizeof(BMFace) + bm->pdata.totsize));

  double fvmem = (double)vmem / 1024.0 / 1024.0;
  double femem = (double)emem / 1024.0 / 1024.0;
  double flmem = (double)lmem / 1024.0 / 1024.0;
  double ffmem = (double)fmem / 1024.0 / 1024.0;

  printf("totmem: %.2fmb\n", fvmem + femem + flmem + ffmem);
  printf("v: %.2f e: %.2f l: %.2f f: %.2f\n", fvmem, femem, flmem, ffmem);

  printf("custom attributes only:\n");
  vmem = (int)((size_t)bm->totvert * (bm->vdata.totsize));
  emem = (int)((size_t)bm->totedge * (bm->edata.totsize));
  lmem = (int)((size_t)bm->totloop * (bm->ldata.totsize));
  fmem = (int)((size_t)bm->totface * (bm->pdata.totsize));

  fvmem = (double)vmem / 1024.0 / 1024.0;
  femem = (double)emem / 1024.0 / 1024.0;
  flmem = (double)lmem / 1024.0 / 1024.0;
  ffmem = (double)fmem / 1024.0 / 1024.0;

  printf("v: %.2f e: %.2f l: %.2f f: %.2f\n", fvmem, femem, flmem, ffmem);
}

// EdgeQueueContext
bool remesh_topology(BrushTester *brush_tester,
                     struct SculptSession *ss,
                     PBVH *pbvh,
                     PBVHTopologyUpdateMode mode,
                     bool use_frontface,
                     float3 view_normal,
                     bool updatePBVH,
                     DyntopoMaskCB mask_cb,
                     void *mask_cb_data,
                     int edge_limit_multiply)
{
  EdgeQueueContext eq_ctx(brush_tester,
                          ss,
                          pbvh,
                          mode,
                          use_frontface,
                          view_normal,
                          updatePBVH,
                          mask_cb,
                          mask_cb_data,
                          edge_limit_multiply);
  eq_ctx.start();

  while (!eq_ctx.done()) {
    eq_ctx.step();
  }

  eq_ctx.finish();
  return eq_ctx.modified;
}

#define SPLIT_TAG BM_ELEM_TAG_ALT

/*

#generate shifted and mirrored patterns
# [number of verts, vert_connections... ]
table = [
  [4,     -1,  3, -1, -1],
  [5,     -1,  3, -1,  0, -1],
  [6,     -1,  3, -1,  5, -1, 1]
]

table2 = {}

def getmask(row):
  mask = 0
  for i in range(len(row)):
    if row[i] >= 0:
      mask |= 1 << i
  return mask

ii = 0
for row in table:
  n = row[0]
  row = row[1:]

  mask = getmask(row)
  table2[mask] = [n] + row

  for step in range(2):
    for i in range(n):
      row2 = []
      for j in range(n):
        j2 = row[(j + i) % n]

        if j2 != -1:
          j2 = (j2 - i + n) % n

        row2.append(j2)

      if row2[0] != -1:
        continue

      mask2 = getmask(row2)
      if mask2 not in table2:
        table2[mask2] = [n] + row2

    #reverse row
    for i in range(n):
      if row[i] != -1:
        row[i] = n - row[i]

    row.reverse()

maxk = 0
for k in table2:
  maxk = max(maxk, k)

buf = 'static const int splitmap[%i][16] = {\n' % (maxk+1)
buf += '  //{numverts, vert_connections...}\n'

for k in range(maxk+1):
  line = ""

  if k not in table2:
    line += '  {-1},'
  else:
    line += '  {'
    row = table2[k]
    for j in range(len(row)):
      if j > 0:
        line += ", "
      line += str(row[j])
    line += '},'

  while len(line) < 35:
    line += " "
  line += "//" + str(k) + " "

  if k in table2:
    for i in range(table2[k][0]):
      ch = "1" if k & (1 << i) else "0"
      line += str(ch) + " "

  buf += line + "\n"
buf += '};\n'
print(buf)

*/
static const int splitmap[43][16] = {
    //{numverts, vert_connections...}
    {-1},                      // 0
    {-1},                      // 1
    {4, -1, 3, -1, -1},        // 2 0 1 0 0
    {-1},                      // 3
    {4, -1, -1, 0, -1},        // 4 0 0 1 0
    {-1},                      // 5
    {-1},                      // 6
    {-1},                      // 7
    {4, -1, -1, -1, 1},        // 8 0 0 0 1
    {-1},                      // 9
    {5, -1, 3, -1, 0, -1},     // 10 0 1 0 1 0
    {-1},                      // 11
    {-1},                      // 12
    {-1},                      // 13
    {-1},                      // 14
    {-1},                      // 15
    {-1},                      // 16
    {-1},                      // 17
    {5, -1, 3, -1, -1, 1},     // 18 0 1 0 0 1
    {-1},                      // 19
    {5, -1, -1, 4, -1, 1},     // 20 0 0 1 0 1
    {-1},                      // 21
    {-1},                      // 22
    {-1},                      // 23
    {-1},                      // 24
    {-1},                      // 25
    {-1},                      // 26
    {-1},                      // 27
    {-1},                      // 28
    {-1},                      // 29
    {-1},                      // 30
    {-1},                      // 31
    {-1},                      // 32
    {-1},                      // 33
    {-1},                      // 34
    {-1},                      // 35
    {-1},                      // 36
    {-1},                      // 37
    {-1},                      // 38
    {-1},                      // 39
    {-1},                      // 40
    {-1},                      // 41
    {6, -1, 3, -1, 5, -1, 1},  // 42 0 1 0 1 0 1
};

float dyntopo_params[5] = {5.0f, 1.0f, 4.0f};

static void pbvh_split_edges(
    EdgeQueueContext *eq_ctx, PBVH *pbvh, BMesh *bm, BMEdge **edges1, int totedge)
{
  bm_logstack_push();
  bm_log_message("  == split edges == ");

  /* Try to improve quality by inserting new edge into queue.
   * This is a bit tricky since we don't want to expand outside
   * the brush radius too much, but we can't stay strictly inside
   * either.  We use tri_in_range for this.
   */

  auto test_near_brush = [&](BMEdge *e, float *co, float *r_w = nullptr) {
    float w = calc_weighted_length(eq_ctx, e->v1, e->v2, SPLIT);
    if (r_w) {
      *r_w = w;
    }

    if (w == 0.0f || !e->l) {
      return false;
    }

    if (skinny_bad_edge(e)) {
      return false;
    }

    BMLoop *l = e->l;
    do {
      BMVert *vs[3] = {l->v, l->next->v, l->next->next->v};
      if (eq_ctx->brush_tester->tri_in_range(vs, l->f->no)) {
        return true;
      }
    } while ((l = l->radial_next) != e->l);

    return false;
  };

  BMEdge **edges = edges1;
  Vector<BMFace *> faces;

#define SUBD_ADD_TO_QUEUE

  const int node_updateflag = PBVH_UpdateBB | PBVH_UpdateOriginalBB | PBVH_UpdateNormals |
                              PBVH_UpdateOtherVerts | PBVH_UpdateCurvatureDir |
                              PBVH_UpdateTriAreas | PBVH_UpdateDrawBuffers |
                              PBVH_RebuildDrawBuffers | PBVH_UpdateTris | PBVH_UpdateNormals;

  for (int i = 0; i < totedge; i++) {
    BMEdge *e = edges[i];

    check_vert_fan_are_tris(pbvh, e->v1);
    check_vert_fan_are_tris(pbvh, e->v2);
  }

  for (int i = 0; i < totedge; i++) {
    BMEdge *e = edges[i];
    BMLoop *l = e->l;

    /* Clear tags. */
    e->head.hflag &= ~SPLIT_TAG;
    e->v1->head.hflag &= ~SPLIT_TAG;
    e->v2->head.hflag &= ~SPLIT_TAG;

    if (!l) {
      continue;
    }

    /* Clear tags in wider neighborhood and flag valence/boundary for update. */
    do {
      BMLoop *l2 = l->f->l_first;

      do {
        l2->e->head.hflag &= ~SPLIT_TAG;
        l2->v->head.hflag &= ~SPLIT_TAG;

        pbvh_boundary_update_bmesh(pbvh, l2->v);
        dyntopo_add_flag(pbvh,
                         l2->v,
                         SCULPTFLAG_NEED_VALENCE | SCULPTFLAG_NEED_DISK_SORT |
                             SCULPTFLAG_NEED_TRIANGULATE);
      } while ((l2 = l2->next) != l->f->l_first);

      l->f->head.hflag &= ~SPLIT_TAG;
    } while ((l = l->radial_next) != e->l);
  }

  /* Tag edges and faces to split. */
  for (int i = 0; i < totedge; i++) {
    BMEdge *e = edges[i];
    BMLoop *l = e->l;

    e->head.index = 0;
    e->head.hflag |= SPLIT_TAG;

    if (!l) {
      continue;
    }

    do {
      if (!(l->f->head.hflag & SPLIT_TAG)) {
        BMLoop *l2 = l;
        do {
          l2->v->head.hflag &= ~SPLIT_TAG;
        } while ((l2 = l2->next) != l);
        l->f->head.hflag |= SPLIT_TAG;

        if (l->f->len == 3) {
          l->f->head.index = l->f->len;
          faces.append(l->f);
        }
      }

    } while ((l = l->radial_next) != e->l);
  }

  int totface = faces.size();
  for (int i = 0; i < totface; i++) {
    BMFace *f = faces[i];
    BMLoop *l = f->l_first;

    f->head.hflag |= SPLIT_TAG;
    BM_log_face_removed(pbvh->header.bm, pbvh->bm_log, f);
    BM_idmap_release(pbvh->bm_idmap, (BMElem *)f, true);

    /* Build pattern mask and store in f->head.index. */
    int mask = 0;
    int j = 0;
    do {
      if (l->e->head.hflag & SPLIT_TAG) {
        mask |= 1 << j;
      }

      j++;
    } while ((l = l->next) != f->l_first);

    f->head.index = mask;
  }

  bm_log_message("  == split edges (edge split) == ");

#ifdef SUBD_ADD_TO_QUEUE
  Vector<BMEdge *> new_edges;
#endif

  for (int i = 0; i < totedge; i++) {
    BMEdge *e = edges[i];

    if (!e || !(e->head.hflag & SPLIT_TAG)) {
      continue;
    }

    BMVert *v1 = e->v1;
    BMVert *v2 = e->v2;
    BMEdge *newe = nullptr;

    e->head.hflag &= ~SPLIT_TAG;

    BKE_pbvh_bmesh_check_origdata(eq_ctx->ss, e->v1, pbvh->stroke_id);
    BKE_pbvh_bmesh_check_origdata(eq_ctx->ss, e->v2, pbvh->stroke_id);

    validate_edge(pbvh, pbvh->header.bm, e, true, true);

    BM_idmap_check_assign(pbvh->bm_idmap, (BMElem *)e->v1);
    BM_idmap_check_assign(pbvh->bm_idmap, (BMElem *)e->v2);
    BM_log_edge_removed(pbvh->header.bm, pbvh->bm_log, e);

    BM_idmap_release(pbvh->bm_idmap, (BMElem *)e, true);

    BMVert *newv = BM_edge_split(pbvh->header.bm, e, e->v1, &newe, 0.5f);
    newe->head.hflag &= ~(SPLIT_TAG | BM_ELEM_TAG);
    e->head.hflag &= ~(SPLIT_TAG | BM_ELEM_TAG);

    BM_log_vert_added(bm, pbvh->bm_log, newv);
    BM_log_edge_added(bm, pbvh->bm_log, e);
    BM_log_edge_added(bm, pbvh->bm_log, newe);

    edge_queue_insert_val34_vert(eq_ctx, newv);

#ifdef SUBD_ADD_TO_QUEUE
    new_edges.append(e);
    new_edges.append(newe);
#endif

    PBVH_CHECK_NAN(newv->co);

    validate_edge(pbvh, pbvh->header.bm, e, true, true);
    validate_edge(pbvh, pbvh->header.bm, newe, true, true);
    validate_vert(pbvh, pbvh->header.bm, newv, true, true);

    newv->head.hflag |= SPLIT_TAG;

    pbvh_boundary_update_bmesh(pbvh, newv);
    dyntopo_add_flag(pbvh,
                     newv,
                     SCULPTFLAG_NEED_VALENCE | SCULPTFLAG_NEED_DISK_SORT |
                         SCULPTFLAG_NEED_TRIANGULATE);

    BMVert *otherv = e->v1 != newv ? e->v1 : e->v2;
    pbvh_boundary_update_bmesh(pbvh, e->v1 != newv ? e->v1 : e->v2);
    dyntopo_add_flag(pbvh,
                     otherv,
                     SCULPTFLAG_NEED_DISK_SORT | SCULPTFLAG_NEED_VALENCE |
                         SCULPTFLAG_NEED_TRIANGULATE);

    BM_ELEM_CD_SET_INT(newv, pbvh->cd_vert_node_offset, DYNTOPO_NODE_NONE);

    int ni = BM_ELEM_CD_GET_INT(v1, pbvh->cd_vert_node_offset);

    if (ni == DYNTOPO_NODE_NONE) {
      ni = BM_ELEM_CD_GET_INT(v2, pbvh->cd_vert_node_offset);
    }

    if (ni >= pbvh->totnode || !(pbvh->nodes[ni].flag & PBVH_Leaf)) {
      printf("%s: error\n", __func__);
    }

    /* This should rarely happen. */
    if (ni == DYNTOPO_NODE_NONE) {
      ni = DYNTOPO_NODE_NONE;

      for (int j = 0; j < 2; j++) {
        BMVert *v = nullptr;

        switch (j) {
          case 0:
            v = v1;
            break;
          case 1:
            v = v2;
            break;
        }

        if (!v->e) {
          continue;
        }

        BMEdge *e2 = v->e;
        do {
          if (!e2->l) {
            break;
          }

          BMLoop *l = e2->l;
          do {
            int ni2 = BM_ELEM_CD_GET_INT(l->f, pbvh->cd_face_node_offset);

            if (ni2 >= 0 && ni2 < pbvh->totnode && (pbvh->nodes[ni2].flag & PBVH_Leaf)) {
              ni = ni2;
              goto outerbreak;
            }
          } while ((l = l->radial_next) != e2->l);
        } while ((e2 = BM_DISK_EDGE_NEXT(e2, v)) != v->e);
      }
    outerbreak:;
    }

    if (ni != DYNTOPO_NODE_NONE) {
      PBVHNode *node = pbvh->nodes + ni;

      if (!(node->flag & PBVH_Leaf)) {
        printf("pbvh error in pbvh_split_edges!\n");

        BM_ELEM_CD_SET_INT(newv, pbvh->cd_vert_node_offset, DYNTOPO_NODE_NONE);

        continue;
      }

      node->flag |= (PBVHNodeFlags)node_updateflag;

      BLI_table_gset_add(node->bm_unique_verts, newv);

      BM_ELEM_CD_SET_INT(newv, pbvh->cd_vert_node_offset, ni);
      // BM_ELEM_CD_SET_INT(newv, pbvh->cd_vert_node_offset, -1);
    }
    else {
      BM_ELEM_CD_SET_INT(newv, pbvh->cd_vert_node_offset, DYNTOPO_NODE_NONE);
      printf("%s: error!\n", __func__);
    }
  }

  bm_log_message("  == split edges (triangulate) == ");

  /* Subdivide from template. */

  Vector<BMVert *, 32> vs;
  Vector<BMFace *> newfaces;

  for (int i = 0; i < totface; i++) {
    BMFace *f = faces[i];

    if (!(f->head.hflag & SPLIT_TAG)) {
      continue;
    }

    int ni = BM_ELEM_CD_GET_INT(f, pbvh->cd_face_node_offset);

    if (ni < 0 || ni >= pbvh->totnode || !(pbvh->nodes[ni].flag & PBVH_Leaf)) {
      printf("%s: error!\n", __func__);
      ni = DYNTOPO_NODE_NONE;
    }

    BMLoop *l = f->l_first;
    int totmask = 0, mask = 0;
    int j = 0;

    do {
      if (l->v->head.hflag & SPLIT_TAG) {
        mask |= 1 << j;
        totmask++;
      }
      j++;
    } while ((l = l->next) != f->l_first);

    int flen = j;

    if (mask < 0 || mask >= (int)ARRAY_SIZE(splitmap)) {
      printf("splitmap error! flen: %d totmask: %d mask: %d\n", flen, totmask, mask);
      continue;
    }

    const int *pat = splitmap[mask];
    int n = pat[0];

    if (n < 0) {
      printf("%s: error 1! %d %d\n", __func__, n, flen);
      continue;
    }

    if (n != f->len || n != flen) {
      printf("%s: error 2! %d %d\n", __func__, n, flen);
      continue;
    }

    BMFace *f2 = f;

    vs.resize(n);

    l = f->l_first;
    j = 0;
    do {
      vs[j++] = l->v;
    } while ((l = l->next) != f->l_first);

    if (j != n) {
      printf("%s: error 1!\n", __func__);
      continue;
    }

    newfaces.resize(newfaces.size() + n);

    int count = 0;

    for (j = 0; j < n; j++) {
      if (pat[j + 1] < 0) {
        continue;
      }

      BMVert *v1 = vs[j], *v2 = vs[pat[j + 1]];
      BMLoop *l1 = nullptr, *l2 = nullptr;
      BMLoop *rl = nullptr;

      BMLoop *l3 = f2->l_first;
      do {
        if (l3->v == v1) {
          l1 = l3;
        }
        else if (l3->v == v2) {
          l2 = l3;
        }
      } while ((l3 = l3->next) != f2->l_first);

      if (l1 == l2 || !l1 || !l2) {
        printf("%s: error 2!\n", __func__);
        continue;
      }

      validate_face(pbvh, bm, f2, false, true);

      bool log_edge = true;
      BMFace *newf = nullptr;
      BMEdge *exist_e;

      if ((exist_e = BM_edge_exists(v1, v2))) {
        log_edge = false;

        BMLoop *l1 = exist_e->l;

        if (l1 && l1->f == f2) {
          l1 = l1->radial_next;
        }

        if (l1 && l1->f != f2) {
          // newf = l1->f;
        }
      }
      else {
        newf = BM_face_split(bm, f2, l1, l2, &rl, nullptr, false);
        exist_e = newf ? rl->e : nullptr;
      }

      if (exist_e && exist_e->l) {
        exist_e->head.hflag &= ~BM_ELEM_TAG;

#ifdef SUBD_ADD_TO_QUEUE
        new_edges.append(exist_e);
#endif

        check_face_is_manifold(pbvh, bm, newf);
        check_face_is_manifold(pbvh, bm, f2);
        check_face_is_manifold(pbvh, bm, f);

        validate_face(pbvh, bm, f2, false, true);
        validate_face(pbvh, bm, newf, false, true);

        if (log_edge) {
          BM_log_edge_added(bm, pbvh->bm_log, rl->e);
        }

        bool ok = ni != DYNTOPO_NODE_NONE;
        ok = ok && BM_ELEM_CD_GET_INT(v1, pbvh->cd_vert_node_offset) != DYNTOPO_NODE_NONE;
        ok = ok && BM_ELEM_CD_GET_INT(v2, pbvh->cd_vert_node_offset) != DYNTOPO_NODE_NONE;

        if (ok) {
          PBVHNode *node = pbvh->nodes + ni;

          node->flag |= (PBVHNodeFlags)node_updateflag;

          BLI_table_gset_add(node->bm_faces, newf);
          BM_ELEM_CD_SET_INT(newf, pbvh->cd_face_node_offset, ni);
        }
        else {
          BM_ELEM_CD_SET_INT(newf, pbvh->cd_face_node_offset, DYNTOPO_NODE_NONE);
        }

        if (count < n) {
          newfaces[count++] = newf;
        }
        else {
          printf("%s: error 4!\n", __func__);
        }
        f2 = newf;
      }
      else {
        printf("%s: split error 2!\n", __func__);
        continue;
      }
    }

    for (j = 0; j < count; j++) {
      if (BM_ELEM_CD_GET_INT(newfaces[j], pbvh->cd_face_node_offset) == DYNTOPO_NODE_NONE) {
        BKE_pbvh_bmesh_add_face(pbvh, newfaces[j], false, true);
      }

      if (newfaces[j] != f) {
        BM_log_face_added(bm, pbvh->bm_log, newfaces[j]);
      }
#if 1
      if (newfaces[j]->len != 3) {
        printf("%s: tesselation error!\n", __func__);
      }
#endif
    }

    if (f->len != 3) {
      printf("%s: tesselation error!\n", __func__);
    }

    if (BM_ELEM_CD_GET_INT(f, pbvh->cd_face_node_offset) == DYNTOPO_NODE_NONE) {
      BKE_pbvh_bmesh_add_face(pbvh, f, false, true);
    }

    BM_log_face_added(bm, pbvh->bm_log, f);
  }

#ifdef SUBD_ADD_TO_QUEUE
  for (BMEdge *e : new_edges) {
    float3 co = e->v1->co;
    co = (co + e->v2->co) * 0.5f;

    float w = 0.0f;
    w = calc_weighted_length(eq_ctx, e->v1, e->v2, SPLIT);

    if (test_near_brush(e, co)) {
      if (w > eq_ctx->limit_len_max_sqr) {
        add_split_edge_recursive(eq_ctx, e->l, w, eq_ctx->limit_len_max, 0);
      }
      else if (w < eq_ctx->limit_len_min_sqr) {
        edge_queue_insert_unified(eq_ctx, e, w);
      }
    }
  }
#endif

  bm_logstack_pop();
}
void detail_size_set(PBVH *pbvh, float detail_size, float detail_range)
{
  pbvh->bm_detail_range = max_ff(detail_range, 0.1f);
  pbvh->bm_max_edge_len = detail_size;
  pbvh->bm_min_edge_len = pbvh->bm_max_edge_len * pbvh->bm_detail_range;
}
}  // namespace blender::bke::dyntopo

void BKE_pbvh_bmesh_remove_face(PBVH *pbvh, BMFace *f, bool log_face)
{
  blender::bke::dyntopo::pbvh_bmesh_face_remove(pbvh, f, log_face, true, true);
}

void BKE_pbvh_bmesh_remove_edge(PBVH *pbvh, BMEdge *e, bool log_edge)
{
  if (log_edge) {
    bm_logstack_push();
    BM_log_edge_removed(pbvh->header.bm, pbvh->bm_log, e);
    bm_logstack_pop();
  }
}

void BKE_pbvh_bmesh_remove_vertex(PBVH *pbvh, BMVert *v, bool log_vert)
{
  blender::bke::dyntopo::pbvh_bmesh_vert_remove(pbvh, v);

  if (log_vert) {
    BM_log_vert_removed(pbvh->header.bm, pbvh->bm_log, v);
  }
}

void BKE_pbvh_bmesh_add_face(PBVH *pbvh, struct BMFace *f, bool log_face, bool force_tree_walk)
{
  bm_logstack_push();

  int ni = DYNTOPO_NODE_NONE;

  if (force_tree_walk) {
    bke_pbvh_insert_face(pbvh, f);

    if (log_face) {
      BM_log_face_added(pbvh->header.bm, pbvh->bm_log, f);
    }

    bm_logstack_pop();
    return;
  }

  /* Look for node in srounding geometry. */
  BMLoop *l = f->l_first;
  do {
    ni = BM_ELEM_CD_GET_INT(l->radial_next->f, pbvh->cd_face_node_offset);

    if (ni >= 0 && (!(pbvh->nodes[ni].flag & PBVH_Leaf) || ni >= pbvh->totnode)) {
      printf("%s: error: ni: %d totnode: %d\n", __func__, ni, pbvh->totnode);
      l = l->next;
      continue;
    }

    if (ni >= 0 && (pbvh->nodes[ni].flag & PBVH_Leaf)) {
      break;
    }

    l = l->next;
  } while (l != f->l_first);

  if (ni < 0) {
    bke_pbvh_insert_face(pbvh, f);
  }
  else {
    BM_ELEM_CD_SET_INT(f, pbvh->cd_face_node_offset, ni);
    bke_pbvh_insert_face_finalize(pbvh, f, ni);
  }

  if (log_face) {
    BM_log_face_added(pbvh->header.bm, pbvh->bm_log, f);
  }

  bm_logstack_pop();
}

void BKE_sculpt_reproject_cdata(SculptSession *ss,
                                PBVHVertRef vertex,
                                float startco[3],
                                float startno[3])
{
  BMVert *v = (BMVert *)vertex.i;
  BMEdge *e;

  if (!v->e) {
    return;
  }

  CustomData *ldata = &ss->bm->ldata;

  int totuv = 0;
  CustomDataLayer *uvlayer = NULL;

  /* Optimized substitute for CustomData_number_of_layers. */
  if (ldata->typemap[CD_PROP_FLOAT2] != -1) {
    for (int i = ldata->typemap[CD_PROP_FLOAT2];
         i < ldata->totlayer && ldata->layers[i].type == CD_PROP_FLOAT2;
         i++)
    {
      totuv++;
    }

    uvlayer = ldata->layers + ldata->typemap[CD_PROP_FLOAT2];
  }

  int tag = BM_ELEM_TAG_ALT;

  float origin[3];
  float ray[3];

  copy_v3_v3(origin, v->co);
  copy_v3_v3(ray, v->no);
  negate_v3(ray);

  struct IsectRayPrecalc precalc;
  isect_ray_tri_watertight_v3_precalc(&precalc, ray);

  float *lastuvs = (float *)BLI_array_alloca(lastuvs, totuv * 2);
  bool *snapuvs = (bool *)BLI_array_alloca(snapuvs, totuv);

  e = v->e;

  /* First clear some flags. */
  do {
    e->head.api_flag &= ~tag;

    if (!e->l) {
      continue;
    }

    BMLoop *l = e->l;
    do {
      l->head.hflag &= ~tag;
      l->next->head.hflag &= ~tag;
      l->prev->head.hflag &= ~tag;
    } while ((l = l->radial_next) != e->l);
  } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);

  Vector<BMLoop *, 8> ls;

  bool first = true;
  bool bad = false;

  for (int i = 0; i < totuv; i++) {
    snapuvs[i] = true;
  }

  do {
    BMLoop *l = e->l;

    if (!l) {
      continue;
    }
#if 0
    bool bound = l == l->radial_next;

    // check for faceset boundaries
    bound = bound || (BM_ELEM_CD_GET_INT(l->f,ss->cd_faceset_offset) !=
      BM_ELEM_CD_GET_INT(l->radial_next->f,ss->cd_faceset_offset));

    // check for seam and sharp edges
    bound = bound || (e->head.hflag & BM_ELEM_SEAM) || !(e->head.hflag & BM_ELEM_SMOOTH);

    if (bound) {
      continue;
    }
#endif
    do {
      BMLoop *l2 = l->v != v ? l->next : l;

      if (l2->head.hflag & tag) {
        continue;
      }

      l2->head.hflag |= tag;
      ls.append(l2);

      for (int i = 0; i < totuv; i++) {
        const int cd_uv = uvlayer[i].offset;
        float *luv = BM_ELEM_CD_PTR<float *>(l2, cd_uv);

        /* Check that we are not part of a uv seam. */
        if (!first) {
          const float dx = lastuvs[i * 2] - luv[0];
          const float dy = lastuvs[i * 2 + 1] - luv[1];
          const float eps = 0.00001f;

          if (dx * dx + dy * dy > eps) {
            bad = true;
            snapuvs[i] = false;
          }
        }

        lastuvs[i * 2] = luv[0];
        lastuvs[i * 2 + 1] = luv[1];
      }

      first = false;

      if (bad) {
        break;
      }
    } while ((l = l->radial_next) != e->l);

    if (bad) {
      break;
    }
  } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);

  if (bad || !ls.size()) {
    return;
  }

  int totloop = ls.size();

  /* Original (l->prev, l, l->next) projections for each loop ('l' remains unchanged). */

  char *_blocks = (char *)alloca(ldata->totsize * totloop);
  void **blocks = (void **)BLI_array_alloca(blocks, totloop);

  for (int i = 0; i < totloop; i++, _blocks += ldata->totsize) {
    blocks[i] = (void *)_blocks;
  }

  float vco[3], vno[3];

  copy_v3_v3(vco, v->co);
  copy_v3_v3(vno, v->no);

  BMFace _fakef, *fakef = &_fakef;

#if 0
  BMFace *projf = NULL;
  // find face vertex projects into
  for (int i = 0; i < totloop; i++) {
    BMLoop *l = ls[i];

    copy_v3_v3(ray,l->f->no);
    negate_v3(ray);

    float t,uv[2];

    //*
    bool hit = isect_ray_tri_v3(origin,ray,l->prev->v->co,origco,l->next->v->co,&t,uv);
    if (hit) {
      projf = l->f;
      break;
    }  //*/
  }

  if (!projf) {
    return;
  }
#endif

  /* Build fake face with original coordinates. */
  for (int i = 0; i < totloop; i++) {
    BMLoop *l = ls[i];
    float no[3] = {0.0f, 0.0f, 0.0f};

    BMLoop *fakels = (BMLoop *)BLI_array_alloca(fakels, l->f->len);
    BMVert *fakevs = (BMVert *)BLI_array_alloca(fakevs, l->f->len);
    BMLoop *l2 = l->f->l_first;
    BMLoop *fakel = fakels;
    BMVert *fakev = fakevs;
    int j = 0;

    do {
      *fakel = *l2;
      fakel->next = fakels + ((j + 1) % l->f->len);
      fakel->prev = fakels + ((j + l->f->len - 1) % l->f->len);

      *fakev = *l2->v;
      fakel->v = fakev;

      /* Make sure original coordinate is up to date. */
      blender::bke::paint::get_original_vertex(ss, vertex, nullptr, nullptr, nullptr, nullptr);

      if (l2->v == v) {
        copy_v3_v3(fakev->co, startco);
        copy_v3_v3(fakev->no, startno);
        add_v3_v3(no, startno);
      }
      else {
        add_v3_v3(no, l2->v->no);
      }

      fakel++;
      fakev++;
      j++;
    } while ((l2 = l2->next) != l->f->l_first);

    *fakef = *l->f;
    fakef->l_first = fakels;

    normalize_v3(no);

    if (len_squared_v3(no) > 0.0f) {
      copy_v3_v3(fakef->no, no);
    }
    else if (fakef->len == 4) {
      normal_quad_v3(
          fakef->no, l->v->co, l->next->v->co, l->next->next->v->co, l->next->next->next->v->co);
    }
    else {
      normal_tri_v3(fakef->no, l->v->co, l->next->v->co, l->next->next->v->co);
    }

    /* Interpolate. */
    BMLoop _interpl, *interpl = &_interpl;

    uint8_t *flag = BM_ELEM_CD_PTR<uint8_t *>(v, ss->attrs.flags->bmesh_cd_offset);
    uint8_t *stroke_id = BM_ELEM_CD_PTR<uint8_t *>(v, ss->attrs.stroke_id->bmesh_cd_offset);

    int flag_saved = *flag;
    int stroke_id_saved = *stroke_id;

    *interpl = *l;
    interpl->head.data = blocks[i];

    BM_loop_interp_from_face(ss->bm, interpl, fakef, false, false);

    *stroke_id = stroke_id_saved;
    *flag = flag_saved;

    CustomData_bmesh_copy_data(&ss->bm->ldata, &ss->bm->ldata, interpl->head.data, &l->head.data);
  }

  int *tots = (int *)BLI_array_alloca(tots, totuv);

  for (int i = 0; i < totuv; i++) {
    lastuvs[i * 2] = lastuvs[i * 2 + 1] = 0.0f;
    tots[i] = 0;
  }

  /* Re-snap uvs. */
  v = (BMVert *)vertex.i;

  e = v->e;
  do {
    if (!e->l) {
      continue;
    }

    BMLoop *l_iter = e->l;
    do {
      BMLoop *l = l_iter->v != v ? l_iter->next : l_iter;

      for (int i = 0; i < totuv; i++) {
        const int cd_uv = uvlayer[i].offset;
        float *luv = BM_ELEM_CD_PTR<float *>(l, cd_uv);

        add_v2_v2(lastuvs + i * 2, luv);
        tots[i]++;
      }
    } while ((l_iter = l_iter->radial_next) != e->l);
  } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);

  for (int i = 0; i < totuv; i++) {
    if (tots[i]) {
      mul_v2_fl(lastuvs + i * 2, 1.0f / (float)tots[i]);
    }
  }

  e = v->e;
  do {
    if (!e->l) {
      continue;
    }

    BMLoop *l_iter = e->l;
    do {
      BMLoop *l = l_iter->v != v ? l_iter->next : l_iter;

      for (int i = 0; i < totuv; i++) {
        const int cd_uv = uvlayer[i].offset;
        float *luv = BM_ELEM_CD_PTR<float *>(l, cd_uv);

        if (snapuvs[i]) {
          copy_v2_v2(luv, lastuvs + i * 2);
        }
      }
    } while ((l_iter = l_iter->radial_next) != e->l);
  } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);
}
