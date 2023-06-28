//#define DYNTOPO_VALIDATE_LOG

#include "BKE_dyntopo.hh"
#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BKE_sculpt.hh"

#include "BLI_asan.h"
#include "BLI_heap_minmax.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_rand.hh"
#include "BLI_set.hh"
#include "BLI_utildefines.h"

#include "bmesh.h"
#include "pbvh_intern.hh"

using blender::float3;

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

namespace blender::bke::dyntopo {

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

#define DYNTOPO_MAX_ITER 512

/* Very long skinny edges are pathological for remeshing,
 * they lead to degenerate geometry with 1/1 ratio between
 * subdivison and collapse.  Ideally we should detect this
 * case and adjust the ratio dynamically, but for now just
 * use a very high ratio all the time.
 */
#define DYNTOPO_MAX_ITER_COLLAPSE 64
#define DYNTOPO_MAX_ITER_SUBD 1

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
#define EVEN_GENERATION_SCALE 1.15f

/* recursion depth to start applying front face test */
#define DEPTH_START_LIMIT 4

//#define FANCY_EDGE_WEIGHTS <= too slow
//#define SKINNY_EDGE_FIX

/* Slightly relax geometry by this factor along surface tangents
 * to improve convergence of dyntopo remesher. This relaxation is
 * applied stochastically (by skipping verts randomly) to improve
 * performance.
 */
#define DYNTOPO_SAFE_SMOOTH_FAC 0.1f

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

#define DYNTOPO_MASK(cd_mask_offset, v) \
  (cd_mask_offset != -1 ? BM_ELEM_CD_GET_FLOAT(v, cd_mask_offset) : 0.0f)

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
  blender::bke::dyntopo::BrushTester *brush_tester;
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

  blender::Vector<BMVert *> used_verts;

  float3 view_normal;
  bool use_view_normal;

  float radius_squared;
  float limit_len_min;
  float limit_mid;
  float limit_len_max;
  float limit_len_min_sqr;
  float limit_len_max_sqr;

  PBVHTopologyUpdateMode mode;
  bool reproject_cdata;
  bool surface_relax;

  bool updatePBVH = false;
  int steps[2];
  PBVHTopologyUpdateMode ops[2];
  int totop = 0;
  PBVH *pbvh;

  bool ignore_loop_data = false;

  bool modified = false;
  int count = 0;
  int curop = 0;

  EdgeQueueContext(BrushTester *brush_tester,
                   Object *ob,
                   PBVH *pbvh,
                   PBVHTopologyUpdateMode mode,
                   bool use_frontface,
                   float3 view_normal,
                   bool updatePBVH,
                   DyntopoMaskCB mask_cb,
                   void *mask_cb_data);

  void insert_val34_vert(BMVert *v);
  void insert_edge(BMEdge *e, float w);

  void start();
  bool done();
  void step();
  void finish();

  /* Remove 3 and 4 valence vertices surrounded only by triangles. */
  bool cleanup_valence_34();
  void surface_smooth(BMVert *v, float fac);

  void report();

  bool was_flushed()
  {
    if (flushed_) {
      flushed_ = false;
      return true;
    }

    return false;
  }

  BMVert *collapse_edge(PBVH *pbvh, BMEdge *e, BMVert *v1, BMVert *v2);
  void split_edge(BMEdge *e);

  blender::RandomNumberGenerator rand;

 private:
  bool flushed_ = false;
};

bool destroy_nonmanifold_fins(PBVH *pbvh, BMEdge *e_root);
bool check_face_is_tri(PBVH *pbvh, BMFace *f);
bool check_vert_fan_are_tris(PBVH *pbvh, BMVert *v);

BMVert *pbvh_bmesh_collapse_edge(
    PBVH *pbvh, BMEdge *e, BMVert *v1, BMVert *v2, struct EdgeQueueContext *eq_ctx);

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
}  // namespace blender::bke::dyntopo

/*************************** Topology update **************************/

/**** Debugging Tools ********/

/* Note: you will have to uncomment FORCE_BMESH_CHECK in bmesh_core.cc for this
 * to work in RelWithDebInfo builds.
 */
//#define CHECKMESH
namespace blender::bmesh {
int bmesh_elem_check_all(void *elem, char htype);
}

enum eValidateVertFlags {
  CHECK_VERT_NONE = 0,
  CHECK_VERT_MANIFOLD = (1 << 0),
  CHECK_VERT_NODE_ASSIGNED = (1 << 1),
  CHECK_VERT_FACES = (1 << 2),
  CHECK_VERT_ALL = (1 << 0) | (1 << 1) /* Don't include CHECK_VERT_FACES */
};
ENUM_OPERATORS(eValidateVertFlags, CHECK_VERT_FACES);

enum eValidateFaceFlags {
  CHECK_FACE_NONE = 0,
  CHECK_FACE_NODE_ASSIGNED = (1 << 1),
  CHECK_FACE_ALL = (1 << 1),
  CHECK_FACE_MANIFOLD = (1 << 2),
};
ENUM_OPERATORS(eValidateFaceFlags, CHECK_FACE_MANIFOLD);

#ifndef CHECKMESH

template<typename T> inline bool validate_elem(PBVH *, T *)
{
  return true;
}
inline bool validate_vert(PBVH *, BMVert *, eValidateVertFlags)
{
  return true;
}
inline bool validate_edge(PBVH *, BMEdge *)
{
  return true;
}
inline bool validate_loop(PBVH *, BMLoop *)
{
  return true;
}
inline bool validate_face(PBVH *, BMFace *, eValidateFaceFlags)
{
  return true;
}
inline bool check_face_is_manifold(PBVH *, BMFace *)
{
  return true;
}
#else
#  include <cstdarg>
#  include <type_traits>

namespace blender::bke::dyntopo::debug {
static void debug_printf(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
}

template<typename T> static char get_type_htype()
{
  if constexpr (std::is_same_v<T, BMVert>) {
    return BM_VERT;
  }
  if constexpr (std::is_same_v<T, BMEdge>) {
    return BM_EDGE;
  }
  if constexpr (std::is_same_v<T, BMLoop>) {
    return BM_LOOP;
  }
  if constexpr (std::is_same_v<T, BMFace>) {
    return BM_FACE;
  }

  return 0;
}

template<typename T> static const char *get_type_name()
{
  if constexpr (std::is_same_v<T, BMVert>) {
    return "vertex";
  }
  if constexpr (std::is_same_v<T, BMEdge>) {
    return "edge";
  }
  if constexpr (std::is_same_v<T, BMLoop>) {
    return "loop";
  }
  if constexpr (std::is_same_v<T, BMFace>) {
    return "face";
  }

  return "(invalid element)";
}
static const char *get_type_name(char htype)
{
  switch (htype) {
    case BM_VERT:
      return "vertex";
    case BM_EDGE:
      return "edge";
    case BM_LOOP:
      return "loop";
    case BM_FACE:
      return "face";
  }

  return "(invalid element)";
}
}  // namespace blender::bke::dyntopo::debug

extern "C" const char *bm_get_error_str(int err);

template<typename T = BMVert> ATTR_NO_OPT static bool validate_elem(PBVH *pbvh, T *elem)
{
  using namespace blender::bke::dyntopo::debug;

  if (!elem) {
    blender::bke::dyntopo::debug::debug_printf("%s was null\n", get_type_name<T>);
    return false;
  }

  if (elem->head.htype != get_type_htype<T>()) {
    blender::bke::dyntopo::debug::debug_printf(
        "%p had wrong type: expected a %s but got %s (type %d).\n",
        elem,
        get_type_name<T>(),
        get_type_name(elem->head.htype));
    return false;
  }

  int ret = blender::bmesh::bmesh_elem_check_all(static_cast<void *>(elem), get_type_htype<T>());

  if (ret) {
    blender::bke::dyntopo::debug::debug_printf("%s (%p) failed integrity checks with code %s\n",
                                               get_type_name<T>(),
                                               elem,
                                               bm_get_error_str(ret));
    return false;
  }

  return true;
}

ATTR_NO_OPT static bool check_face_is_manifold(PBVH *pbvh, BMFace *f)
{
  using namespace blender::bke::dyntopo::debug;

  BMLoop *l = f->l_first;
  do {
    int count = 0;
    BMLoop *l2 = l;
    do {
      if (count++ > 10) {
        blender::bke::dyntopo::debug::debug_printf(
            "Face %p has highly non-manifold edge %p\n", f, l->e);
        return false;
      }
    } while ((l2 = l2->radial_next) != l);
  } while ((l = l->next) != f->l_first);

  return true;
}

ATTR_NO_OPT static bool validate_face(PBVH *pbvh, BMFace *f, eValidateFaceFlags flags)
{
  if (!validate_elem<BMFace>(pbvh, f)) {
    return false;
  }

  bool ok = true;

  if (flags & CHECK_FACE_MANIFOLD) {
    ok = ok && check_face_is_manifold(pbvh, f);
  }

  int ni = BM_ELEM_CD_GET_INT(f, pbvh->cd_face_node_offset);
  if (ni < 0 || ni >= pbvh->totnode) {
    if (ni != DYNTOPO_NODE_NONE || flags & CHECK_FACE_NODE_ASSIGNED) {
      // blender::bke::dyntopo::debug::debug_printf("face %p has corrupted node index %d\n", f,
      // ni);
      return false;
    }
    else {
      return ok;
    }
  }

  PBVHNode *node = &pbvh->nodes[ni];
  if (!(node->flag & PBVH_Leaf)) {
    // printf("face %p has corrupted node index.", f);
    return false;
  }

  if (!node->bm_faces->contains(f)) {
    printf("face is not in node->bm_faces.\n");
    return false;
  }

  return ok;
}

ATTR_NO_OPT static bool validate_vert(PBVH *pbvh, BMVert *v, eValidateVertFlags flags)
{
  using namespace blender::bke::dyntopo::debug;

  if (!validate_elem<BMVert>(pbvh, v)) {
    return false;
  }

  if (flags & CHECK_VERT_FACES) {
    BMIter iter;
    BMFace *f;
    BM_ITER_ELEM (f, &iter, v, BM_FACES_OF_VERT) {
      if (!validate_face(pbvh,
                         f,
                         flags & CHECK_VERT_NODE_ASSIGNED ? CHECK_FACE_NODE_ASSIGNED :
                                                            CHECK_FACE_NONE))
      {
        return false;
      }
    }
  }

  bool ok = true;

  if (flags & CHECK_VERT_MANIFOLD) {
    BMIter iter;
    BMFace *f;

    BM_ITER_ELEM (f, &iter, v, BM_FACES_OF_VERT) {
      ok = ok && check_face_is_manifold(pbvh, f);
    }
  }

  int ni = BM_ELEM_CD_GET_INT(v, pbvh->cd_vert_node_offset);

  if (ni < 0 || ni >= pbvh->totnode) {
    if (ni != DYNTOPO_NODE_NONE || flags & CHECK_VERT_NODE_ASSIGNED) {
      // blender::bke::dyntopo::debug::debug_printf("vertex %p has corrupted node index %d\n", v,
      // ni);
      return false;
    }
    else {
      return ok;
    }
  }

  PBVHNode *node = &pbvh->nodes[ni];
  if (!(node->flag & PBVH_Leaf)) {
    // printf("Vertex %p has corrupted node index.", v);
    return false;
  }

  if (!node->bm_unique_verts->contains(v)) {
    printf("Vertex %p is not in node->bm_unique_verts\n");
    return false;
  }
  if (node->bm_other_verts->contains(v)) {
    printf("Vertex %p is inside of node->bm_other_verts\n");
    return false;
  }

  return ok;
}

ATTR_NO_OPT static bool validate_edge(PBVH *pbvh, BMEdge *e)
{
  return validate_elem<BMEdge>(pbvh, e);
}
ATTR_NO_OPT static bool validate_loop(PBVH *pbvh, BMLoop *l)
{
  return validate_elem<BMLoop>(pbvh, l);
}

#endif
