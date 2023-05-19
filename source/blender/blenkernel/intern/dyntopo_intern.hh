//#define DYNTOPO_VALIDATE_LOG

#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BLI_asan.h"
#include "BLI_heap_minmax.hh"
#include "bmesh.h"
#include "pbvh_intern.hh"

struct GHash;
struct BLI_Buffer;
struct SculptSession;

#define DYNTOPO_DISABLE_SPLIT_EDGES 1
#define DYNTOPO_DISABLE_FIN_REMOVAL 2
#define DYNTOPO_DISABLE_COLLAPSE 4
#define DYNTOPO_DISABLE_TRIANGULATOR 8

//#define DYNTOPO_DISABLE_FLAG \
//  (DYNTOPO_DISABLE_FIN_REMOVAL | DYNTOPO_DISABLE_COLLAPSE | DYNTOPO_DISABLE_TRIANGULATOR)
#define DYNTOPO_DISABLE_FLAG 0

extern "C" {
void bmesh_disk_edge_append(BMEdge *e, BMVert *v);
void bmesh_radial_loop_append(BMEdge *e, BMLoop *l);
void bm_kill_only_edge(BMesh *bm, BMEdge *e);
void bm_kill_only_loop(BMesh *bm, BMLoop *l);
void bm_kill_only_face(BMesh *bm, BMFace *f);
}

static inline bool dyntopo_test_flag(PBVH *pbvh, BMVert *v, uint8_t flag)
{
  return *BM_ELEM_CD_PTR<uint8_t *>(v, pbvh->cd_flag) & flag;
}

static inline void dyntopo_add_flag(PBVH *pbvh, BMVert *v, uint8_t flag)
{
  *BM_ELEM_CD_PTR<uint8_t *>(v, pbvh->cd_flag) |= flag;
}

namespace blender::dyntopo {

static int elem_sizes[] = {-1,
                           (int)sizeof(BMVert),
                           (int)sizeof(BMEdge),
                           0,
                           (int)sizeof(BMLoop),
                           -1,
                           -1,
                           -1,
                           (int)sizeof(BMFace)};

inline bool bm_elem_is_free(BMElem *elem, int htype)
{
  BLI_asan_unpoison(elem, elem_sizes[htype]);

  bool ret = elem->head.htype != htype;

  if (ret) {
    BLI_asan_poison(elem, elem_sizes[htype]);
  }

  return ret;
}

#ifdef DYNTOPO_VALIDATE_LOG
#  define VALIDATE_LOG(log) BM_log_validate_cur(log)
#else
#  define VALIDATE_LOG(log)
#endif

//#define DYNTOPO_REPORT
//#define WITH_ADAPTIVE_CURVATURE
//#define DYNTOPO_NO_THREADING

#define SCULPTVERT_VALENCE_TEMP SCULPTFLAG_SPLIT_TEMP

/* Which boundary types dyntopo should respect. */

/* Smooth boundaries */
#define SCULPTVERT_SMOOTH_BOUNDARY \
  (SCULPT_BOUNDARY_MESH | SCULPT_BOUNDARY_FACE_SET | SCULPT_BOUNDARY_SHARP_MARK | \
   SCULPT_BOUNDARY_SEAM | SCULPT_BOUNDARY_UV | SCULPT_BOUNDARY_SHARP_ANGLE)
#define SCULPTVERT_SMOOTH_CORNER \
  (SCULPT_CORNER_MESH | SCULPT_CORNER_FACE_SET | SCULPT_CORNER_SHARP_MARK | SCULPT_CORNER_SEAM | \
   SCULPT_CORNER_UV | SCULPT_CORNER_SHARP_ANGLE)

/* All boundaries */
#define SCULPTVERT_ALL_BOUNDARY \
  (SCULPT_BOUNDARY_MESH | SCULPT_BOUNDARY_FACE_SET | SCULPT_BOUNDARY_SHARP_MARK | \
   SCULPT_BOUNDARY_SEAM | SCULPT_BOUNDARY_UV | SCULPT_BOUNDARY_SHARP_ANGLE)
#define SCULPTVERT_ALL_CORNER \
  (SCULPT_CORNER_MESH | SCULPT_CORNER_FACE_SET | SCULPT_CORNER_SHARP_MARK | SCULPT_CORNER_SEAM | \
   SCULPT_CORNER_UV | SCULPT_CORNER_SHARP_ANGLE)

#define DYNTOPO_MAX_ITER 1024
#define DYNTOPO_MAX_ITER_SUBD 4096

#define DYNTOPO_USE_HEAP
#define DYNTOPO_USE_MINMAX_HEAP

#ifndef DYNTOPO_USE_HEAP
/* don't add edges into the queue multiple times */
#  define USE_EDGEQUEUE_TAG
#endif

/* Avoid skinny faces */
#define USE_EDGEQUEUE_EVEN_SUBDIV

/* How much longer we need to be to consider for subdividing
 * (avoids subdividing faces which are only *slightly* skinny) */
#define EVEN_EDGELEN_THRESHOLD 1.2f
/* How much the limit increases per recursion
 * (avoids performing subdivisions too far away). */
#define EVEN_GENERATION_SCALE 1.25f

/* recursion depth to start applying front face test */
#define DEPTH_START_LIMIT 5

//#define FANCY_EDGE_WEIGHTS <= too slow
//#define SKINNY_EDGE_FIX

/* slightly relax geometry by this factor along surface tangents
   to improve convergence of remesher */
#define DYNTOPO_SAFE_SMOOTH_FAC 0.05f

#ifdef USE_EDGEQUEUE_EVEN_SUBDIV
#  include "BKE_global.h"
#endif

/* Support for only operating on front-faces */
#define USE_EDGEQUEUE_FRONTFACE

/**
 * Ensure we don't have dirty tags for the edge queue, and that they are left cleared.
 * (slow, even for debug mode, so leave disabled for now).
 */
#if defined(USE_EDGEQUEUE_TAG) && 0
#  if !defined(NDEBUG)
#    define USE_EDGEQUEUE_TAG_VERIFY
#  endif
#endif

// #define USE_VERIFY

#define DYNTOPO_MASK(cd_mask_offset, v) BM_ELEM_CD_GET_FLOAT(v, cd_mask_offset)

#ifdef USE_VERIFY
static void pbvh_bmesh_verify(PBVH *pbvh);
#endif

/* -------------------------------------------------------------------- */
/** \name BMesh Utility API
 *
 * Use some local functions which assume triangles.
 * \{ */

/**
 * Typically using BM_LOOPS_OF_VERT and BM_FACES_OF_VERT iterators are fine,
 * however this is an area where performance matters so do it in-line.
 *
 * Take care since 'break' won't works as expected within these macros!
 */

#define BM_DISK_EDGE(e, v) (&((&(e)->v1_disk_link)[(v) == (e)->v2]))

#define BM_LOOPS_OF_VERT_ITER_BEGIN(l_iter_radial_, v_) \
  { \
    struct { \
      BMVert *v; \
      BMEdge *e_iter, *e_first; \
      BMLoop *l_iter_radial; \
    } _iter; \
    _iter.v = v_; \
    if (_iter.v->e) { \
      _iter.e_iter = _iter.e_first = _iter.v->e; \
      do { \
        if (_iter.e_iter->l) { \
          _iter.l_iter_radial = _iter.e_iter->l; \
          do { \
            if (_iter.l_iter_radial->v == _iter.v) { \
              l_iter_radial_ = _iter.l_iter_radial;

#define BM_LOOPS_OF_VERT_ITER_END \
  } \
  } \
  while ((_iter.l_iter_radial = _iter.l_iter_radial->radial_next) != _iter.e_iter->l) \
    ; \
  } \
  } \
  while ((_iter.e_iter = BM_DISK_EDGE_NEXT(_iter.e_iter, _iter.v)) != _iter.e_first) \
    ; \
  } \
  } \
  ((void)0)

#define BM_FACES_OF_VERT_ITER_BEGIN(f_iter_, v_) \
  { \
    BMLoop *l_iter_radial_; \
    BM_LOOPS_OF_VERT_ITER_BEGIN (l_iter_radial_, v_) { \
      f_iter_ = l_iter_radial_->f;

#define BM_FACES_OF_VERT_ITER_END \
  } \
  BM_LOOPS_OF_VERT_ITER_END; \
  } \
  ((void)0)

struct EdgeQueue;

struct EdgeQueueContext {
  SculptSession *ss;
  BLI_mempool *pool = nullptr;
  BMesh *bm = nullptr;
  DyntopoMaskCB mask_cb = nullptr;
  void *mask_cb_data;
  int cd_vert_mask_offset;
  int cd_vert_node_offset;
  int cd_face_node_offset;
  float avg_elen;
  float max_elen;
  float min_elen;
  float totedge;
  bool local_mode;
  float surface_smooth_fac;

  blender::MinMaxHeap<BMEdge *> edge_heap;

  int max_heap_mm;
  // TableGSet *used_verts;
  blender::Vector<BMVert *> used_verts;

  float view_normal[3];
  bool use_view_normal;
  float limit_min, limit_max, limit_mid;

  const float *center = nullptr;
  float center_proj[3]; /* for when we use projected coords. */
  float radius_squared;
  float limit_len_min;
  float limit_len_max;
  float limit_len_min_sqr;
  float limit_len_max_sqr;

  bool (*edge_queue_tri_in_range)(const struct EdgeQueueContext *q, BMVert *vs[3], float no[3]);
  bool (*edge_queue_vert_in_range)(const struct EdgeQueueContext *q, BMVert *v);

  PBVHTopologyUpdateMode mode;
  bool reproject_cdata;
};

bool destroy_nonmanifold_fins(PBVH *pbvh, BMEdge *e_root);
bool check_face_is_tri(PBVH *pbvh, BMFace *f);
bool check_vert_fan_are_tris(PBVH *pbvh, BMVert *v);

BMVert *pbvh_bmesh_collapse_edge(PBVH *pbvh,
                                 BMEdge *e,
                                 BMVert *v1,
                                 BMVert *v2,
                                 struct GHash *deleted_verts,
                                 struct BLI_Buffer *deleted_faces,
                                 struct EdgeQueueContext *eq_ctx);

extern "C" void bm_log_message(const char *fmt, ...);
void pbvh_bmesh_vert_remove(PBVH *pbvh, BMVert *v);
inline bool bm_edge_tag_test(BMEdge *e)
{
  /* is the edge or one of its faces tagged? */
  return (BM_elem_flag_test(e->v1, BM_ELEM_TAG) || BM_elem_flag_test(e->v2, BM_ELEM_TAG) ||
          (e->l &&
           (BM_elem_flag_test(e->l->f, BM_ELEM_TAG) ||
            (e->l != e->l->radial_next && BM_elem_flag_test(e->l->radial_next->f, BM_ELEM_TAG)))));
}

inline void bm_edge_tag_disable(BMEdge *e)
{
  BM_elem_flag_disable(e->v1, BM_ELEM_TAG);
  BM_elem_flag_disable(e->v2, BM_ELEM_TAG);
  if (e->l) {
    BM_elem_flag_disable(e->l->f, BM_ELEM_TAG);
    if (e->l != e->l->radial_next) {
      BM_elem_flag_disable(e->l->radial_next->f, BM_ELEM_TAG);
    }
  }
}

/* takes the edges loop */
BLI_INLINE int bm_edge_is_manifold_or_boundary(BMLoop *l)
{
#if 0
  /* less optimized version of check below */
  return (BM_edge_is_manifold(l->e) || BM_edge_is_boundary(l->e);
#else
  /* if the edge is a boundary it points to its self, else this must be a manifold */
  return LIKELY(l) && LIKELY(l->radial_next->radial_next == l);
#endif
}

inline void bm_edge_tag_enable(BMEdge *e)
{
  BM_elem_flag_enable(e->v1, BM_ELEM_TAG);
  BM_elem_flag_enable(e->v2, BM_ELEM_TAG);
  if (e->l) {
    BM_elem_flag_enable(e->l->f, BM_ELEM_TAG);
    if (e->l != e->l->radial_next) {
      BM_elem_flag_enable(e->l->radial_next->f, BM_ELEM_TAG);
    }
  }
}

void pbvh_bmesh_face_remove(
    PBVH *pbvh, BMFace *f, bool log_face, bool check_verts, bool ensure_ownership_transfer);

bool check_for_fins(PBVH *pbvh, BMVert *v);
void pbvh_kill_vert(PBVH *pbvh, BMVert *v, bool log_vert, bool log_edges);
BMFace *pbvh_bmesh_face_create(PBVH *pbvh,
                               int node_index,
                               BMVert *v_tri[3],
                               BMEdge *e_tri[3],
                               const BMFace *f_example,
                               bool ensure_verts,
                               bool log_face);
}  // namespace blender::dyntopo

extern "C" {
void BKE_pbvh_bmesh_remove_face(PBVH *pbvh, BMFace *f, bool log_face);
void BKE_pbvh_bmesh_remove_edge(PBVH *pbvh, BMEdge *e, bool log_edge);
void BKE_pbvh_bmesh_remove_vertex(PBVH *pbvh, BMVert *v, bool log_vert);
void BKE_pbvh_bmesh_add_face(PBVH *pbvh, struct BMFace *f, bool log_face, bool force_tree_walk);
}

/*************************** Topology update **************************/

/**** Debugging Tools ********/

inline void fix_mesh(PBVH *pbvh, BMesh *bm)
{
  BMIter iter;
  BMVert *v;
  BMEdge *e;
  BMFace *f;

  printf("fixing mesh. . .\n");

  BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
    v->e = nullptr;
    dyntopo_add_flag(pbvh,
                     v,
                     SCULPTFLAG_NEED_VALENCE | SCULPTFLAG_NEED_DISK_SORT |
                         SCULPTFLAG_NEED_TRIANGULATE);
  }

  BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
    e->v1_disk_link.next = e->v1_disk_link.prev = nullptr;
    e->v2_disk_link.next = e->v2_disk_link.prev = nullptr;
    e->l = nullptr;

    if (e->v1 == e->v2) {
      bm_kill_only_edge(bm, e);
    }
  }

  // rebuild disk cycles
  BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
    if (BM_edge_exists(e->v1, e->v2)) {
      printf("duplicate edge %p!\n", e);
      bm_kill_only_edge(bm, e);

      continue;
    }

    bmesh_disk_edge_append(e, e->v1);
    bmesh_disk_edge_append(e, e->v2);
  }

  BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
    BMLoop *l = f->l_first;

    do {
      if (f->len < 3) {
        break;
      }

      if (l->next->v == l->v) {
        BMLoop *l_del = l->next;

        l->next = l_del->next;
        l_del->next->prev = l;

        f->len--;

        if (f->l_first == l_del) {
          f->l_first = l;
        }

        bm_kill_only_loop(bm, l_del);

        if (f->len < 3) {
          break;
        }
      }
    } while ((l = l->next) != f->l_first);

    if (f->len < 3) {
      int ni = BM_ELEM_CD_GET_INT(f, pbvh->cd_face_node_offset);

      if (ni >= 0 && ni < pbvh->totnode && (pbvh->nodes[ni].flag & PBVH_Leaf)) {
        BLI_table_gset_remove(pbvh->nodes[ni].bm_faces, f, nullptr);
      }

      bm_kill_only_face(bm, f);
      continue;
    }

    do {
      l->e = BM_edge_exists(l->v, l->next->v);

      if (!l->e) {
        l->e = BM_edge_create(bm, l->v, l->next->v, nullptr, BM_CREATE_NOP);
      }

      bmesh_radial_loop_append(l->e, l);
    } while ((l = l->next) != f->l_first);
  }

  bm->elem_table_dirty |= BM_VERT | BM_EDGE | BM_FACE;
  bm->elem_index_dirty |= BM_VERT | BM_EDGE | BM_FACE;

  printf("done fixing mesh.\n");
}

//#define CHECKMESH
//#define TEST_INVALID_NORMALS

#ifndef CHECKMESH
#  define validate_vert(pbvh, bm, v, autofix, check_manifold) true
#  define validate_edge(pbvh, bm, e, autofix, check_manifold) true
#  define validate_face(pbvh, bm, f, autofix, check_manifold) true
#  define validate_vert_faces(pbvh, bm, v, autofix, check_manifold) true
#  define check_face_is_manifold(pbvh, bm, f) true
#else

#  define CHECKMESH_ATTR ATTR_NO_OPT

CHECKMESH_ATTR static void _debugprint(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
}

CHECKMESH_ATTR static bool check_face_is_manifold(PBVH *pbvh, BMesh *bm, BMFace *f)
{
  BMLoop *l = f->l_first;

  do {
    if (l->radial_next != l && l->radial_next->radial_next != l) {
      //_debugprint("non-manifold edge in loop\n");

      BMVert *v1 = l->e->v1, *v2 = l->e->v2;

      for (int i = 0; i < 2; i++) {
        BMVert *v = i ? v2 : v1;
        BMEdge *e = v->e;

        if (!e) {
          continue;
        }

        int i = 0;

        do {
          if (!e) {
            break;
          }

          bool same = e->v1 == v1 && e->v2 == v2;
          same = same || (e->v1 == v2 && e->v2 == v1);

          if (same && e != l->e) {
            // printf("duplicate edges in face!\n");
          }

          if (i++ > 5000) {
            printf("infinite loop in edge disk cycle! v: %p, e: %p\n", v, e);
            break;
          }
        } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);
      }
      l->e->head.hflag |= BM_ELEM_SELECT;
      l->f->head.hflag |= BM_ELEM_SELECT;
      l->v->head.hflag |= BM_ELEM_SELECT;

      // pbvh->dyntopo_stop = true;

      return false;
    }

#  ifdef TEST_INVALID_NORMALS
    if (l != l->radial_next && l->v == l->radial_next->v) {
      _debugprint("invalid normals\n");
      return false;
    }
#  endif
  } while ((l = l->next) != f->l_first);

  return true;
}

CHECKMESH_ATTR
static bool validate_vert(PBVH *pbvh, BMesh *bm, BMVert *v, bool autofix, bool check_manifold)
{
  if (v->head.htype != BM_VERT) {
    _debugprint("bad vertex\n");
    return false;
  }

  BMEdge *e = v->e;
  int i = 0;

  if (!e) {
    return true;
  }

  do {
    if (e->v1 != v && e->v2 != v) {
      _debugprint("edge does not contain v\n");
      goto error;
    }

    if (e->l) {
      int j = 0;

      BMLoop *l = e->l;
      do {
        if (l->e->v1 != v && l->e->v2 != v) {
          _debugprint("loop's edges doesn't contain v\n");
          goto error;
        }

        if (l->v != v && l->next->v != v) {
          _debugprint("loop and loop->next don't contain v\n");
          goto error;
        }

        j++;
        if (j > 1000) {
          _debugprint("corrupted radial cycle\n");
          goto error;
        }

        if (check_manifold) {
          check_face_is_manifold(pbvh, bm, l->f);
        }
      } while ((l = l->radial_next) != e->l);
    }
    if (i > 10000) {
      _debugprint("corrupted disk cycle\n");
      goto error;
    }

    e = BM_DISK_EDGE_NEXT(e, v);
    i++;
  } while (e != v->e);

  return true;

error:

  if (autofix) {
    fix_mesh(pbvh, bm);
  }

  return false;
}

CHECKMESH_ATTR
static bool validate_edge(PBVH *pbvh, BMesh *bm, BMEdge *e, bool autofix, bool check_manifold)
{
  if (e->head.htype != BM_EDGE) {
    _debugprint("corrupted edge!\n");
    return false;
  }

  bool ret = validate_vert(pbvh, bm, e->v1, false, check_manifold) &&
             validate_vert(pbvh, bm, e->v2, false, check_manifold);

  if (!ret && autofix) {
    fix_mesh(pbvh, bm);
  }

  return ret;
}

CHECKMESH_ATTR bool face_verts_are_same(PBVH *pbvh, BMesh *bm, BMFace *f1, BMFace *f2)
{
  BMLoop *l1 = f1->l_first;
  BMLoop *l2 = f2->l_first;

  int count1 = 0;

  do {
    count1++;
  } while ((l1 = l1->next) != f1->l_first);

  do {
    bool ok = false;

    do {
      if (l2->v == l1->v) {
        ok = true;
        break;
      }
    } while ((l2 = l2->next) != f2->l_first);

    if (!ok) {
      return false;
    }
  } while ((l1 = l1->next) != f1->l_first);

  return true;
}

CHECKMESH_ATTR
static bool validate_face(PBVH *pbvh, BMesh *bm, BMFace *f, bool autofix, bool check_manifold)
{
  if (f->head.htype != BM_FACE) {
    _debugprint("corrupted edge!\n");
    return false;
  }

  BMLoop **ls = nullptr;
  BLI_array_staticdeclare(ls, 32);

  BMLoop *l = f->l_first;
  int i = 0;
  do {
    i++;

    if (i > 100000) {
      _debugprint("Very corrupted face!\n");
      goto error;
    }

    if (!validate_edge(pbvh, bm, l->e, false, check_manifold)) {
      goto error;
    }

    BMLoop *l2 = l->radial_next;
    do {
      if (l2->f != f && face_verts_are_same(pbvh, bm, l2->f, f)) {
        _debugprint("Duplicate faces!\n");
        goto error;
      }
    } while ((l2 = l2->radial_next) != l);

    BLI_array_append(ls, l);
  } while ((l = l->next) != f->l_first);

  for (int i = 0; i < BLI_array_len(ls); i++) {
    BMLoop *l1 = ls[i];
    for (int j = 0; j < BLI_array_len(ls); j++) {
      BMLoop *l2 = ls[j];

      if (i != j && l1->v == l2->v) {
        _debugprint("duplicate verts in face!\n");
        goto error;
      }

      if (BM_edge_exists(l->v, l->next->v) != l->e) {
        _debugprint("loop has wrong edge!\n");
        goto error;
      }
    }
  }

  BLI_array_free(ls);
  return true;

error:
  BLI_array_free(ls);

  if (autofix) {
    fix_mesh(pbvh, bm);
  }

  return false;
}

CHECKMESH_ATTR bool validate_vert_faces(
    PBVH *pbvh, BMesh *bm, BMVert *v, int autofix, bool check_manifold)
{
  if (!validate_vert(pbvh, bm, v, autofix, check_manifold)) {
    return false;
  }

  if (!v->e) {
    return true;
  }

  BMEdge *e = v->e;
  do {
    BMLoop *l = e->l;

    if (!l) {
      continue;
    }

    do {
      if (!validate_edge(pbvh, bm, l->e, false, false)) {
        goto error;
      }

      if (!validate_face(pbvh, bm, l->f, false, check_manifold)) {
        goto error;
      }
    } while ((l = l->radial_next) != e->l);
  } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);

  return true;

error:

  if (autofix) {
    fix_mesh(pbvh, bm);
  }

  return false;
}
#endif
