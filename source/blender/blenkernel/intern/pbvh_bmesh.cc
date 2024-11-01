/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "BLI_bounds.hh"
#include "BLI_ghash.h"
#include "BLI_heap_simple.h"
#include "BLI_math_geom.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_memarena.h"
#include "BLI_span.hh"
#include "BLI_time.h"
#include "BLI_utildefines.h"

#include "BKE_ccg.hh"
#include "BKE_paint_bvh.hh"

#include "bmesh.hh"
#include "pbvh_intern.hh"

#include "CLG_log.h"

static CLG_LogRef LOG = {"pbvh.bmesh"};

/* Avoid skinny faces */
#define USE_EDGEQUEUE_EVEN_SUBDIV
#ifdef USE_EDGEQUEUE_EVEN_SUBDIV
#  include "BKE_global.hh"
#endif

namespace blender::bke::pbvh {

/* Support for only operating on front-faces. */
#define USE_EDGEQUEUE_FRONTFACE

/* Don't add edges into the queue multiple times. */
#define USE_EDGEQUEUE_TAG
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

#ifdef USE_VERIFY
static void pbvh_bmesh_verify(Tree *pbvh);
#endif

/* TODO: choose leaf limit better. */
constexpr int leaf_limit = 400;

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

static Bounds<float3> negative_bounds()
{
  return {float3(std::numeric_limits<float>::max()), float3(std::numeric_limits<float>::lowest())};
}

static std::array<BMEdge *, 3> bm_edges_from_tri(BMesh &bm, const Span<BMVert *> v_tri)
{
  return {
      BM_edge_create(&bm, v_tri[0], v_tri[1], nullptr, BM_CREATE_NO_DOUBLE),
      BM_edge_create(&bm, v_tri[1], v_tri[2], nullptr, BM_CREATE_NO_DOUBLE),
      BM_edge_create(&bm, v_tri[2], v_tri[0], nullptr, BM_CREATE_NO_DOUBLE),
  };
}

BLI_INLINE std::array<BMVert *, 3> bm_face_as_array(BMFace *f)
{
  BMLoop *l = BM_FACE_FIRST_LOOP(f);

  BLI_assert(f->len == 3);

  std::array<BMVert *, 3> result;
  result[0] = l->v;
  l = l->next;
  result[1] = l->v;
  l = l->next;
  result[2] = l->v;
  return result;
}

/**
 * A version of #BM_face_exists, optimized for triangles
 * when we know the loop and the opposite vertex.
 *
 * Check if any triangle is formed by (l_radial_first->v, l_radial_first->next->v, v_opposite),
 * at either winding (since its a triangle no special checks are needed).
 *
 * <pre>
 * l_radial_first->v & l_radial_first->next->v
 * +---+
 * |  /
 * | /
 * + v_opposite
 * </pre>
 *
 * Its assumed that \a l_radial_first is never forming the target face.
 */
static BMFace *bm_face_exists_tri_from_loop_vert(BMLoop *l_radial_first, BMVert *v_opposite)
{
  BLI_assert(
      !ELEM(v_opposite, l_radial_first->v, l_radial_first->next->v, l_radial_first->prev->v));
  if (l_radial_first->radial_next != l_radial_first) {
    BMLoop *l_radial_iter = l_radial_first->radial_next;
    do {
      BLI_assert(l_radial_iter->f->len == 3);
      if (l_radial_iter->prev->v == v_opposite) {
        return l_radial_iter->f;
      }
    } while ((l_radial_iter = l_radial_iter->radial_next) != l_radial_first);
  }
  return nullptr;
}

/**
 * Uses a map of vertices to lookup the final target.
 * References can't point to previous items (would cause infinite loop).
 */
static BMVert *bm_vert_hash_lookup_chain(GHash *deleted_verts, BMVert *v)
{
  while (true) {
    BMVert **v_next_p = (BMVert **)BLI_ghash_lookup_p(deleted_verts, v);
    if (v_next_p == nullptr) {
      /* Not remapped. */
      return v;
    }
    if (*v_next_p == nullptr) {
      /* Removed and not remapped. */
      return nullptr;
    }

    /* Remapped. */
    v = *v_next_p;
  }
}

/** \} */

/****************************** Building ******************************/

/** Update node data after splitting. */
static void pbvh_bmesh_node_finalize(BMeshNode *n,
                                     const int node_index,
                                     const int cd_vert_node_offset,
                                     const int cd_face_node_offset)
{
  bool has_visible = false;

  n->bounds_ = negative_bounds();

  for (BMFace *f : n->bm_faces_) {
    /* Update ownership of faces. */
    BM_ELEM_CD_SET_INT(f, cd_face_node_offset, node_index);

    /* Update vertices. */
    BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
    BMLoop *l_iter = l_first;

    do {
      BMVert *v = l_iter->v;
      if (!n->bm_unique_verts_.contains(v)) {
        if (BM_ELEM_CD_GET_INT(v, cd_vert_node_offset) != DYNTOPO_NODE_NONE) {
          n->bm_other_verts_.add(v);
        }
        else {
          n->bm_unique_verts_.add(v);
          BM_ELEM_CD_SET_INT(v, cd_vert_node_offset, node_index);
        }
      }
      /* Update node bounding box. */
      math::min_max(float3(v->co), n->bounds_.min, n->bounds_.max);
    } while ((l_iter = l_iter->next) != l_first);

    if (!BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      has_visible = true;
    }
  }

  BLI_assert(n->bounds_.min[0] <= n->bounds_.max[0] && n->bounds_.min[1] <= n->bounds_.max[1] &&
             n->bounds_.min[2] <= n->bounds_.max[2]);

  n->bounds_orig_ = n->bounds_;

  BKE_pbvh_node_fully_hidden_set(*n, !has_visible);
}

/** Recursively split the node if it exceeds the leaf_limit. */
static void pbvh_bmesh_node_split(Vector<BMeshNode> &nodes,
                                  Vector<bool> &node_changed,
                                  const int cd_vert_node_offset,
                                  const int cd_face_node_offset,
                                  const Span<Bounds<float3>> face_bounds,
                                  int node_index)
{
  BMeshNode *n = &nodes[node_index];

  if (n->bm_faces_.size() <= leaf_limit) {
    /* Node limit not exceeded. */
    pbvh_bmesh_node_finalize(n, node_index, cd_vert_node_offset, cd_face_node_offset);
    return;
  }

  /* Calculate bounding box around primitive centroids. */
  Bounds<float3> cb = negative_bounds();
  for (BMFace *f : n->bm_faces_) {
    const int i = BM_elem_index_get(f);
    const float3 center = math::midpoint(face_bounds[i].min, face_bounds[i].max);
    math::min_max(center, cb.min, cb.max);
  }

  /* Find widest axis and its midpoint. */
  const int axis = math::dominant_axis(cb.max - cb.min);
  const float mid = math::midpoint(cb.max[axis], cb.min[axis]);

  /* Add two new child nodes. */
  const int children = nodes.size();
  n->children_offset_ = children;
  nodes.resize(nodes.size() + 2);
  node_changed.resize(node_changed.size() + 2, true);

  /* Array reallocated, update current node pointer. */
  n = &nodes[node_index];

  /* Initialize children */
  BMeshNode *c1 = &nodes[children], *c2 = &nodes[children + 1];
  c1->flag_ |= Node::Leaf;
  c2->flag_ |= Node::Leaf;
  c1->bm_faces_.reserve(n->bm_faces_.size() / 2);
  c2->bm_faces_.reserve(n->bm_faces_.size() / 2);

  /* Partition the parent node's faces between the two children. */
  for (BMFace *f : n->bm_faces_) {
    const int i = BM_elem_index_get(f);
    if (math::midpoint(face_bounds[i].min[axis], face_bounds[i].max[axis]) < mid) {
      c1->bm_faces_.add(f);
    }
    else {
      c2->bm_faces_.add(f);
    }
  }

  /* Enforce at least one primitive in each node */
  Set<BMFace *, 0> *empty = nullptr;
  Set<BMFace *, 0> *other;
  if (c1->bm_faces_.is_empty()) {
    empty = &c1->bm_faces_;
    other = &c2->bm_faces_;
  }
  else if (c2->bm_faces_.is_empty()) {
    empty = &c2->bm_faces_;
    other = &c1->bm_faces_;
  }
  if (empty) {
    for (BMFace *f : *other) {
      empty->add(f);
      other->remove(f);
      break;
    }
  }

  /* Clear this node */

  /* Mark this node's unique verts as unclaimed. */
  for (BMVert *v : n->bm_unique_verts_) {
    BM_ELEM_CD_SET_INT(v, cd_vert_node_offset, DYNTOPO_NODE_NONE);
  }

  /* Unclaim faces. */
  for (BMFace *f : n->bm_faces_) {
    BM_ELEM_CD_SET_INT(f, cd_face_node_offset, DYNTOPO_NODE_NONE);
  }
  n->bm_faces_.clear_and_shrink();

  n->flag_ &= ~Node::Leaf;
  node_changed[node_index] = true;

  /* Recurse. */
  pbvh_bmesh_node_split(
      nodes, node_changed, cd_vert_node_offset, cd_face_node_offset, face_bounds, children);
  pbvh_bmesh_node_split(
      nodes, node_changed, cd_vert_node_offset, cd_face_node_offset, face_bounds, children + 1);

  /* Array maybe reallocated, update current node pointer */
  n = &nodes[node_index];

  /* Update bounding box. */
  n->bounds_ = bounds::merge(nodes[n->children_offset_].bounds_,
                             nodes[n->children_offset_ + 1].bounds_);
  n->bounds_orig_ = n->bounds_;
}

/** Recursively split the node if it exceeds the leaf_limit. */
static bool pbvh_bmesh_node_limit_ensure(BMesh &bm,
                                         Vector<BMeshNode> &nodes,
                                         Vector<bool> &node_changed,
                                         const int cd_vert_node_offset,
                                         const int cd_face_node_offset,
                                         int node_index)
{
  BMeshNode &node = nodes[node_index];
  const int faces_num = node.bm_faces_.size();
  if (faces_num <= leaf_limit) {
    /* Node limit not exceeded */
    return false;
  }

  /* For each BMFace, store the AABB and AABB centroid. */
  Array<Bounds<float3>> face_bounds(faces_num);

  int i = 0;
  for (BMFace *f : node.bm_faces_) {
    face_bounds[i] = negative_bounds();

    BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
    BMLoop *l_iter = l_first;
    do {
      math::min_max(float3(l_iter->v->co), face_bounds[i].min, face_bounds[i].max);
    } while ((l_iter = l_iter->next) != l_first);

    /* So we can do direct lookups on 'face_bounds'. */
    BM_elem_index_set(f, i); /* set_dirty! */
    i++;
  }

  /* Likely this is already dirty. */
  bm.elem_index_dirty |= BM_FACE;

  pbvh_bmesh_node_split(
      nodes, node_changed, cd_vert_node_offset, cd_face_node_offset, face_bounds, node_index);

  return true;
}

/**********************************************************************/

BLI_INLINE int pbvh_bmesh_node_index_from_vert(const int cd_vert_node_offset, const BMVert *key)
{
  const int node_index = BM_ELEM_CD_GET_INT((const BMElem *)key, cd_vert_node_offset);
  BLI_assert(node_index != DYNTOPO_NODE_NONE);
  return node_index;
}

BLI_INLINE int pbvh_bmesh_node_index_from_face(const int cd_face_node_offset, const BMFace *key)
{
  const int node_index = BM_ELEM_CD_GET_INT((const BMElem *)key, cd_face_node_offset);
  BLI_assert(node_index != DYNTOPO_NODE_NONE);
  return node_index;
}

BLI_INLINE BMeshNode *pbvh_bmesh_node_from_vert(MutableSpan<BMeshNode> nodes,
                                                const int cd_vert_node_offset,
                                                const BMVert *key)
{
  return &nodes[pbvh_bmesh_node_index_from_vert(cd_vert_node_offset, key)];
}

BLI_INLINE BMeshNode *pbvh_bmesh_node_from_face(MutableSpan<BMeshNode> nodes,
                                                const int cd_face_node_offset,
                                                const BMFace *key)
{
  return &nodes[pbvh_bmesh_node_index_from_face(cd_face_node_offset, key)];
}

static BMVert *pbvh_bmesh_vert_create(BMesh &bm,
                                      MutableSpan<BMeshNode> nodes,
                                      MutableSpan<bool> node_changed,
                                      BMLog &bm_log,
                                      const BMVert *v1,
                                      const BMVert *v2,
                                      const int node_index,
                                      const float co[3],
                                      const float no[3],
                                      const int cd_vert_node_offset,
                                      const int cd_vert_mask_offset)
{
  BMeshNode *node = &nodes[node_index];

  BLI_assert((nodes.size() == 1 || node_index) && node_index <= nodes.size());

  /* Avoid initializing custom-data because its quite involved. */
  BMVert *v = BM_vert_create(&bm, co, nullptr, BM_CREATE_NOP);

  BM_data_interp_from_verts(&bm, v1, v2, v, 0.5f);

  /* This value is logged below. */
  copy_v3_v3(v->no, no);

  node->bm_unique_verts_.add(v);
  BM_ELEM_CD_SET_INT(v, cd_vert_node_offset, node_index);

  node->flag_ |= Node::TopologyUpdated;
  node_changed[node_index] = true;

  /* Log the new vertex. */
  BM_log_vert_added(&bm_log, v, cd_vert_mask_offset);

  return v;
}

/**
 * \note Callers are responsible for checking if the face exists before adding.
 */
static BMFace *pbvh_bmesh_face_create(BMesh &bm,
                                      MutableSpan<BMeshNode> nodes,
                                      MutableSpan<bool> node_changed,
                                      const int cd_face_node_offset,
                                      BMLog &bm_log,
                                      int node_index,
                                      const Span<BMVert *> v_tri,
                                      const Span<BMEdge *> e_tri,
                                      const BMFace *f_example)
{
  BMeshNode *node = &nodes[node_index];

  /* Ensure we never add existing face. */
  BLI_assert(!BM_face_exists(v_tri.data(), 3));

  BMFace *f = BM_face_create(&bm, v_tri.data(), e_tri.data(), 3, f_example, BM_CREATE_NOP);
  f->head.hflag = f_example->head.hflag;

  node->bm_faces_.add(f);
  BM_ELEM_CD_SET_INT(f, cd_face_node_offset, node_index);

  node->flag_ |= Node::TopologyUpdated;
  node_changed[node_index] = true;
  node->flag_ &= ~Node::FullyHidden;

  /* Log the new face. */
  BM_log_face_added(&bm_log, f);

  return f;
}

#define pbvh_bmesh_node_vert_use_count_is_equal(nodes, cd_face_node_offset, node, v, n) \
  (pbvh_bmesh_node_vert_use_count_at_most(nodes, cd_face_node_offset, node, v, (n) + 1) == n)

static int pbvh_bmesh_node_vert_use_count_at_most(MutableSpan<BMeshNode> nodes,
                                                  const int cd_face_node_offset,
                                                  BMeshNode *node,
                                                  BMVert *v,
                                                  const int count_max)
{
  int count = 0;
  BMFace *f;

  BM_FACES_OF_VERT_ITER_BEGIN (f, v) {
    BMeshNode *f_node = pbvh_bmesh_node_from_face(nodes, cd_face_node_offset, f);
    if (f_node == node) {
      count++;
      if (count == count_max) {
        return count;
      }
    }
  }
  BM_FACES_OF_VERT_ITER_END;

  return count;
}

/** Return a node that uses vertex `v` other than its current owner. */
static std::optional<int> pbvh_bmesh_vert_other_node_find(const int cd_vert_node_offset,
                                                          const int cd_face_node_offset,
                                                          BMVert *v)
{
  const int current_node = pbvh_bmesh_node_index_from_vert(cd_vert_node_offset, v);
  BMFace *f;

  BM_FACES_OF_VERT_ITER_BEGIN (f, v) {
    const int f_node = pbvh_bmesh_node_index_from_face(cd_face_node_offset, f);

    if (f_node != current_node) {
      return f_node;
    }
  }
  BM_FACES_OF_VERT_ITER_END;

  return std::nullopt;
}

static void pbvh_bmesh_vert_ownership_transfer(MutableSpan<BMeshNode> nodes,
                                               MutableSpan<bool> node_changed,
                                               const int cd_vert_node_offset,
                                               const int new_owner_index,
                                               BMVert *v)
{
  const int current_owner_index = pbvh_bmesh_node_index_from_vert(cd_vert_node_offset, v);
  BMeshNode *current_owner = &nodes[current_owner_index];
  current_owner->flag_ |= Node::TopologyUpdated;
  node_changed[new_owner_index] = true;

  BMeshNode *new_owner = &nodes[new_owner_index];

  BLI_assert(current_owner != new_owner);

  /* Remove current ownership. */
  current_owner->bm_unique_verts_.remove(v);

  /* Set new ownership */
  BM_ELEM_CD_SET_INT(v, cd_vert_node_offset, new_owner_index);
  new_owner->bm_unique_verts_.add(v);
  new_owner->bm_other_verts_.remove(v);
  BLI_assert(!new_owner->bm_other_verts_.contains(v));

  new_owner->flag_ |= Node::TopologyUpdated;
  node_changed[new_owner_index] = true;
}

static void pbvh_bmesh_vert_remove(MutableSpan<BMeshNode> nodes,
                                   MutableSpan<bool> node_changed,
                                   const int cd_vert_node_offset,
                                   const int cd_face_node_offset,
                                   BMVert *v)
{
  /* Never match for first time. */
  int f_node_index_prev = DYNTOPO_NODE_NONE;

  BMeshNode *v_node = pbvh_bmesh_node_from_vert(nodes, cd_vert_node_offset, v);
  v_node->bm_unique_verts_.remove(v);
  BM_ELEM_CD_SET_INT(v, cd_vert_node_offset, DYNTOPO_NODE_NONE);

  /* Have to check each neighboring face's node. */
  BMFace *f;
  BM_FACES_OF_VERT_ITER_BEGIN (f, v) {
    const int f_node_index = pbvh_bmesh_node_index_from_face(cd_face_node_offset, f);

    /* Faces often share the same node, quick check to avoid redundant #BLI_gset_remove calls. */
    if (f_node_index_prev != f_node_index) {
      f_node_index_prev = f_node_index;

      BMeshNode *f_node = &nodes[f_node_index];
      f_node->flag_ |= Node::TopologyUpdated;
      node_changed[f_node_index] = true;

      /* Remove current ownership. */
      f_node->bm_other_verts_.remove(v);

      BLI_assert(!f_node->bm_unique_verts_.contains(v));
      BLI_assert(!f_node->bm_other_verts_.contains(v));
    }
  }
  BM_FACES_OF_VERT_ITER_END;
}

static void pbvh_bmesh_face_remove(MutableSpan<BMeshNode> nodes,
                                   MutableSpan<bool> node_changed,
                                   const int cd_vert_node_offset,
                                   const int cd_face_node_offset,
                                   BMLog &bm_log,
                                   BMFace *f)
{
  const int node_index = pbvh_bmesh_node_index_from_face(cd_face_node_offset, f);
  BMeshNode *f_node = &nodes[node_index];

  /* Check if any of this face's vertices need to be removed from the node. */
  BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
  BMLoop *l_iter = l_first;
  do {
    BMVert *v = l_iter->v;
    if (pbvh_bmesh_node_vert_use_count_is_equal(nodes, cd_face_node_offset, f_node, v, 1)) {
      if (f_node->bm_unique_verts_.contains(v)) {
        /* Find a different node that uses 'v'. */

        const std::optional<int> new_node = pbvh_bmesh_vert_other_node_find(
            cd_vert_node_offset, cd_face_node_offset, v);
        BLI_assert(new_node || BM_vert_face_count_is_equal(v, 1));

        if (new_node) {
          pbvh_bmesh_vert_ownership_transfer(
              nodes, node_changed, cd_vert_node_offset, *new_node, v);
        }
      }
      else {
        /* Remove from other verts. */
        f_node->bm_other_verts_.remove(v);
      }
    }
  } while ((l_iter = l_iter->next) != l_first);

  /* Remove face from node and top level. */
  f_node->bm_faces_.remove(f);
  BM_ELEM_CD_SET_INT(f, cd_face_node_offset, DYNTOPO_NODE_NONE);

  /* Log removed face. */
  BM_log_face_removed(&bm_log, f);

  /* Mark node for update. */
  f_node->flag_ |= Node::TopologyUpdated;
  node_changed[node_index] = true;
}

static Array<BMLoop *> pbvh_bmesh_edge_loops(BMEdge *e)
{
  /* Fast-path for most common case where an edge has 2 faces no need to iterate twice. */
  std::array<BMLoop *, 2> manifold_loops;
  if (LIKELY(BM_edge_loop_pair(e, &manifold_loops[0], &manifold_loops[1]))) {
    return Array<BMLoop *>(Span(manifold_loops));
  }
  Array<BMLoop *> loops(BM_edge_face_count(e));
  BM_iter_as_array(
      nullptr, BM_LOOPS_OF_EDGE, e, reinterpret_cast<void **>(loops.data()), loops.size());
  return loops;
}

static void pbvh_bmesh_node_drop_orig(BMeshNode *node)
{
  node->orig_positions_ = {};
  node->orig_tris_ = {};
  node->orig_verts_ = {};
}

/****************************** EdgeQueue *****************************/

struct EdgeQueue {
  HeapSimple *heap;
  const float *center;
  float center_proj[3]; /* For when we use projected coords. */
  float radius_squared;
  float limit_len_squared;
#ifdef USE_EDGEQUEUE_EVEN_SUBDIV
  float limit_len;
#endif

  bool (*edge_queue_tri_in_range)(const EdgeQueue *q, BMFace *f);

  const float *view_normal;
#ifdef USE_EDGEQUEUE_FRONTFACE
  uint use_view_normal : 1;
#endif
};

struct EdgeQueueContext {
  EdgeQueue *q;
  BLI_mempool *pool;
  BMesh *bm;
  int cd_vert_mask_offset;
  int cd_vert_node_offset;
  int cd_face_node_offset;
};

/* Only tagged edges are in the queue. */
#ifdef USE_EDGEQUEUE_TAG
#  define EDGE_QUEUE_TEST(e) BM_elem_flag_test((CHECK_TYPE_INLINE(e, BMEdge *), e), BM_ELEM_TAG)
#  define EDGE_QUEUE_ENABLE(e) \
    BM_elem_flag_enable((CHECK_TYPE_INLINE(e, BMEdge *), e), BM_ELEM_TAG)
#  define EDGE_QUEUE_DISABLE(e) \
    BM_elem_flag_disable((CHECK_TYPE_INLINE(e, BMEdge *), e), BM_ELEM_TAG)
#endif

#ifdef USE_EDGEQUEUE_TAG_VERIFY
/* simply check no edges are tagged
 * (it's a requirement that edges enter and leave a clean tag state) */
static void pbvh_bmesh_edge_tag_verify(Tree *pbvh)
{
  for (int n = 0; n < pbvh->totnode; n++) {
    Node *node = &pbvh->nodes_[n];
    if (node->bm_faces_) {
      GSetIterator gs_iter;
      GSET_ITER (gs_iter, node->bm_faces_) {
        BMFace *f = BLI_gsetIterator_getKey(&gs_iter);
        BMEdge *e_tri[3];
        BMLoop *l_iter;

        BLI_assert(f->len == 3);
        l_iter = BM_FACE_FIRST_LOOP(f);
        e_tri[0] = l_iter->e;
        l_iter = l_iter->next;
        e_tri[1] = l_iter->e;
        l_iter = l_iter->next;
        e_tri[2] = l_iter->e;

        BLI_assert((EDGE_QUEUE_TEST(e_tri[0]) == false) && (EDGE_QUEUE_TEST(e_tri[1]) == false) &&
                   (EDGE_QUEUE_TEST(e_tri[2]) == false));
      }
    }
  }
}
#endif

static bool edge_queue_tri_in_sphere(const EdgeQueue *q, BMFace *f)
{
  BMVert *v_tri[3];
  float c[3];

  /* Get closest point in triangle to sphere center. */
  BM_face_as_array_vert_tri(f, v_tri);

  closest_on_tri_to_point_v3(c, q->center, v_tri[0]->co, v_tri[1]->co, v_tri[2]->co);

  /* Check if triangle intersects the sphere. */
  return len_squared_v3v3(q->center, c) <= q->radius_squared;
}

static bool edge_queue_tri_in_circle(const EdgeQueue *q, BMFace *f)
{
  BMVert *v_tri[3];
  float c[3];
  float tri_proj[3][3];

  /* Get closest point in triangle to sphere center. */
  BM_face_as_array_vert_tri(f, v_tri);

  project_plane_normalized_v3_v3v3(tri_proj[0], v_tri[0]->co, q->view_normal);
  project_plane_normalized_v3_v3v3(tri_proj[1], v_tri[1]->co, q->view_normal);
  project_plane_normalized_v3_v3v3(tri_proj[2], v_tri[2]->co, q->view_normal);

  closest_on_tri_to_point_v3(c, q->center_proj, tri_proj[0], tri_proj[1], tri_proj[2]);

  /* Check if triangle intersects the sphere. */
  return len_squared_v3v3(q->center_proj, c) <= q->radius_squared;
}

/** Return true if the vertex mask is less than 1.0, false otherwise. */
static bool check_mask(EdgeQueueContext *eq_ctx, BMVert *v)
{
  return BM_ELEM_CD_GET_FLOAT(v, eq_ctx->cd_vert_mask_offset) < 1.0f;
}

static void edge_queue_insert(EdgeQueueContext *eq_ctx, BMEdge *e, float priority)
{
  /* Don't let topology update affect fully masked vertices. This used to
   * have a 50% mask cutoff, with the reasoning that you can't do a 50%
   * topology update. But this gives an ugly border in the mesh. The mask
   * should already make the brush move the vertices only 50%, which means
   * that topology updates will also happen less frequent, that should be
   * enough. */
  if (((eq_ctx->cd_vert_mask_offset == -1) ||
       (check_mask(eq_ctx, e->v1) || check_mask(eq_ctx, e->v2))) &&
      !(BM_elem_flag_test_bool(e->v1, BM_ELEM_HIDDEN) ||
        BM_elem_flag_test_bool(e->v2, BM_ELEM_HIDDEN)))
  {
    BMVert **pair = static_cast<BMVert **>(BLI_mempool_alloc(eq_ctx->pool));
    pair[0] = e->v1;
    pair[1] = e->v2;
    BLI_heapsimple_insert(eq_ctx->q->heap, priority, pair);
#ifdef USE_EDGEQUEUE_TAG
    BLI_assert(EDGE_QUEUE_TEST(e) == false);
    EDGE_QUEUE_ENABLE(e);
#endif
  }
}

/** Return true if the edge is a boundary edge: both its vertices are on a boundary. */
static bool is_boundary_edge(const BMEdge &edge)
{
  if (edge.head.hflag & BM_ELEM_SEAM) {
    return true;
  }
  if ((edge.head.hflag & BM_ELEM_SMOOTH) == 0) {
    return true;
  }
  if (!BM_edge_is_manifold(&edge)) {
    return true;
  }

  /* TODO(@sergey): Other boundaries? For example, edges between two different face sets. */

  return false;
}

/* Return true if the vertex is adjacent to a boundary edge. */
static bool is_boundary_vert(const BMVert &vertex)
{
  BMEdge *edge = vertex.e;
  BMEdge *first_edge = edge;
  if (first_edge == nullptr) {
    return false;
  }
  do {
    if (is_boundary_edge(*edge)) {
      return true;
    }
  } while ((edge = BM_DISK_EDGE_NEXT(edge, &vertex)) != first_edge);

  return false;
}

/** Return true if at least one of the edge vertices is adjacent to a boundary. */
static bool is_edge_adjacent_to_boundary(const BMEdge &edge)
{
  return is_boundary_vert(*edge.v1) || is_boundary_vert(*edge.v2);
}

/* Notes on edge priority.
 *
 * The priority is used to control the order in which edges are handled for both splitting of long
 * edges and collapsing of short edges. For long edges we start by splitting the longest edge and
 * for collapsing we start with the shortest.
 *
 * A heap-like data structure is used to accelerate such ordering. A bit confusingly, this data
 * structure gives the higher priorities to elements with lower numbers.
 *
 * When edges do not belong to and are not adjacent to boundaries, their length is used as the
 * priority directly. Prefer to handle those edges first. Modifying those edges leads to no
 * distortion to the boundary.
 *
 * Edges adjacent to a boundary with one vertex are handled next, and the vertex which is
 * on the boundary does not change position as part of the edge collapse algorithm.
 *
 * And last, the boundary edges are handled. While subdivision of boundary edges does not change
 * the shape of the boundary, collapsing boundary edges distorts the boundary. Hence they are
 * handled last. */

static float long_edge_queue_priority(const BMEdge &edge)
{
  return -BM_edge_calc_length_squared(&edge);
}

static float short_edge_queue_priority(const BMEdge &edge)
{
  float priority = BM_edge_calc_length_squared(&edge);

  if (is_boundary_edge(edge)) {
    priority *= 1.5f;
  }
  else if (is_edge_adjacent_to_boundary(edge)) {
    priority *= 1.25f;
  }

  return priority;
}

static void long_edge_queue_edge_add(EdgeQueueContext *eq_ctx, BMEdge *e)
{
#ifdef USE_EDGEQUEUE_TAG
  if (EDGE_QUEUE_TEST(e) == false)
#endif
  {
    const float len_sq = BM_edge_calc_length_squared(e);
    if (len_sq > eq_ctx->q->limit_len_squared) {
      edge_queue_insert(eq_ctx, e, long_edge_queue_priority(*e));
    }
  }
}

#ifdef USE_EDGEQUEUE_EVEN_SUBDIV
static void long_edge_queue_edge_add_recursive(
    EdgeQueueContext *eq_ctx, BMLoop *l_edge, BMLoop *l_end, const float len_sq, float limit_len)
{
  BLI_assert(len_sq > square_f(limit_len));

#  ifdef USE_EDGEQUEUE_FRONTFACE
  if (eq_ctx->q->use_view_normal) {
    if (dot_v3v3(l_edge->f->no, eq_ctx->q->view_normal) < 0.0f) {
      return;
    }
  }
#  endif

#  ifdef USE_EDGEQUEUE_TAG
  if (EDGE_QUEUE_TEST(l_edge->e) == false)
#  endif
  {
    edge_queue_insert(eq_ctx, l_edge->e, long_edge_queue_priority(*l_edge->e));
  }

  /* temp support previous behavior! */
  if (UNLIKELY(G.debug_value == 1234)) {
    return;
  }

  if (l_edge->radial_next != l_edge) {
    /* How much longer we need to be to consider for subdividing
     * (avoids subdividing faces which are only *slightly* skinny). */
#  define EVEN_EDGELEN_THRESHOLD 1.2f
    /* How much the limit increases per recursion
     * (avoids performing subdivisions too far away). */
#  define EVEN_GENERATION_SCALE 1.6f

    const float len_sq_cmp = len_sq * EVEN_EDGELEN_THRESHOLD;

    limit_len *= EVEN_GENERATION_SCALE;
    const float limit_len_sq = square_f(limit_len);

    BMLoop *l_iter = l_edge;
    do {
      BMLoop *l_adjacent[2] = {l_iter->next, l_iter->prev};
      for (int i = 0; i < ARRAY_SIZE(l_adjacent); i++) {
        float len_sq_other = BM_edge_calc_length_squared(l_adjacent[i]->e);
        if (len_sq_other > max_ff(len_sq_cmp, limit_len_sq)) {
          // edge_queue_insert(eq_ctx, l_adjacent[i]->e, -len_sq_other);
          long_edge_queue_edge_add_recursive(
              eq_ctx, l_adjacent[i]->radial_next, l_adjacent[i], len_sq_other, limit_len);
        }
      }
    } while ((l_iter = l_iter->radial_next) != l_end);

#  undef EVEN_EDGELEN_THRESHOLD
#  undef EVEN_GENERATION_SCALE
  }
}
#endif /* USE_EDGEQUEUE_EVEN_SUBDIV */

static void short_edge_queue_edge_add(EdgeQueueContext *eq_ctx, BMEdge *e)
{
#ifdef USE_EDGEQUEUE_TAG
  if (EDGE_QUEUE_TEST(e) == false)
#endif
  {
    const float len_sq = BM_edge_calc_length_squared(e);
    if (len_sq < eq_ctx->q->limit_len_squared) {
      edge_queue_insert(eq_ctx, e, short_edge_queue_priority(*e));
    }
  }
}

static void long_edge_queue_face_add(EdgeQueueContext *eq_ctx, BMFace *f)
{
#ifdef USE_EDGEQUEUE_FRONTFACE
  if (eq_ctx->q->use_view_normal) {
    if (dot_v3v3(f->no, eq_ctx->q->view_normal) < 0.0f) {
      return;
    }
  }
#endif

  if (eq_ctx->q->edge_queue_tri_in_range(eq_ctx->q, f)) {
    /* Check each edge of the face. */
    BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
    BMLoop *l_iter = l_first;
    do {
#ifdef USE_EDGEQUEUE_EVEN_SUBDIV
      const float len_sq = BM_edge_calc_length_squared(l_iter->e);
      if (len_sq > eq_ctx->q->limit_len_squared) {
        long_edge_queue_edge_add_recursive(
            eq_ctx, l_iter->radial_next, l_iter, len_sq, eq_ctx->q->limit_len);
      }
#else
      long_edge_queue_edge_add(eq_ctx, l_iter->e);
#endif
    } while ((l_iter = l_iter->next) != l_first);
  }
}

static void short_edge_queue_face_add(EdgeQueueContext *eq_ctx, BMFace *f)
{
#ifdef USE_EDGEQUEUE_FRONTFACE
  if (eq_ctx->q->use_view_normal) {
    if (dot_v3v3(f->no, eq_ctx->q->view_normal) < 0.0f) {
      return;
    }
  }
#endif

  if (eq_ctx->q->edge_queue_tri_in_range(eq_ctx->q, f)) {
    BMLoop *l_iter;
    BMLoop *l_first;

    /* Check each edge of the face. */
    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      short_edge_queue_edge_add(eq_ctx, l_iter->e);
    } while ((l_iter = l_iter->next) != l_first);
  }
}

/**
 * Create a priority queue containing vertex pairs connected by a long
 * edge as defined by Tree.bm_max_edge_len.
 *
 * Only nodes marked for topology update are checked, and in those
 * nodes only edges used by a face intersecting the (center, radius)
 * sphere are checked.
 *
 * The highest priority (lowest number) is given to the longest edge.
 */
static void long_edge_queue_create(EdgeQueueContext *eq_ctx,
                                   const float max_edge_len,
                                   MutableSpan<BMeshNode> nodes,
                                   const float center[3],
                                   const float view_normal[3],
                                   float radius,
                                   const bool use_frontface,
                                   const bool use_projected)
{
  eq_ctx->q->heap = BLI_heapsimple_new();
  eq_ctx->q->center = center;
  eq_ctx->q->radius_squared = radius * radius;
  eq_ctx->q->limit_len_squared = max_edge_len * max_edge_len;
#ifdef USE_EDGEQUEUE_EVEN_SUBDIV
  eq_ctx->q->limit_len = max_edge_len;
#endif

  eq_ctx->q->view_normal = view_normal;

#ifdef USE_EDGEQUEUE_FRONTFACE
  eq_ctx->q->use_view_normal = use_frontface;
#else
  UNUSED_VARS(use_frontface);
#endif

  if (use_projected) {
    eq_ctx->q->edge_queue_tri_in_range = edge_queue_tri_in_circle;
    project_plane_normalized_v3_v3v3(eq_ctx->q->center_proj, center, view_normal);
  }
  else {
    eq_ctx->q->edge_queue_tri_in_range = edge_queue_tri_in_sphere;
  }

#ifdef USE_EDGEQUEUE_TAG_VERIFY
  pbvh_bmesh_edge_tag_verify(pbvh);
#endif

  for (BMeshNode &node : nodes) {
    /* Check leaf nodes marked for topology update. */
    if ((node.flag_ & Node::Leaf) && (node.flag_ & Node::UpdateTopology) &&
        !(node.flag_ & Node::FullyHidden))
    {
      for (BMFace *f : node.bm_faces_) {
        long_edge_queue_face_add(eq_ctx, f);
      }
    }
  }
}

/**
 * Create a priority queue containing vertex pairs connected by a
 * short edge as defined by Tree.bm_min_edge_len.
 *
 * Only nodes marked for topology update are checked, and in those
 * nodes only edges used by a face intersecting the (center, radius)
 * sphere are checked.
 *
 * The highest priority (lowest number) is given to the shortest edge.
 */
static void short_edge_queue_create(EdgeQueueContext *eq_ctx,
                                    const float min_edge_len,
                                    MutableSpan<BMeshNode> nodes,
                                    const float center[3],
                                    const float view_normal[3],
                                    float radius,
                                    const bool use_frontface,
                                    const bool use_projected)
{
  eq_ctx->q->heap = BLI_heapsimple_new();
  eq_ctx->q->center = center;
  eq_ctx->q->radius_squared = radius * radius;
  eq_ctx->q->limit_len_squared = min_edge_len * min_edge_len;
#ifdef USE_EDGEQUEUE_EVEN_SUBDIV
  eq_ctx->q->limit_len = min_edge_len;
#endif

  eq_ctx->q->view_normal = view_normal;

#ifdef USE_EDGEQUEUE_FRONTFACE
  eq_ctx->q->use_view_normal = use_frontface;
#else
  UNUSED_VARS(use_frontface);
#endif

  if (use_projected) {
    eq_ctx->q->edge_queue_tri_in_range = edge_queue_tri_in_circle;
    project_plane_normalized_v3_v3v3(eq_ctx->q->center_proj, center, view_normal);
  }
  else {
    eq_ctx->q->edge_queue_tri_in_range = edge_queue_tri_in_sphere;
  }

  for (BMeshNode &node : nodes) {
    /* Check leaf nodes marked for topology update */
    if ((node.flag_ & Node::Leaf) && (node.flag_ & Node::UpdateTopology) &&
        !(node.flag_ & Node::FullyHidden))
    {
      for (BMFace *f : node.bm_faces_) {
        short_edge_queue_face_add(eq_ctx, f);
      }
    }
  }
}

/*************************** Topology update **************************/

/**
 * Copy custom data from src to dst edge.
 *
 * \note The BM_ELEM_TAG is used to tell whether an edge is in the queue for collapse/split,
 * so we do not copy this flag as we do not want the new edge to appear in the queue.
 */
static void copy_edge_data(BMesh &bm, BMEdge &dst, /*const*/ BMEdge &src)
{
  dst.head.hflag = src.head.hflag & ~BM_ELEM_TAG;
  CustomData_bmesh_copy_block(bm.edata, src.head.data, &dst.head.data);
}

/* Merge edge custom data from src to dst. */
static void merge_edge_data(BMesh &bm, BMEdge &dst, const BMEdge &src)
{
  dst.head.hflag |= (src.head.hflag & ~(BM_ELEM_TAG | BM_ELEM_SMOOTH));

  /* If either of the src or dst is sharp the result is sharp. */
  if ((src.head.hflag & BM_ELEM_SMOOTH) == 0) {
    dst.head.hflag &= ~BM_ELEM_SMOOTH;
  }

  BM_data_interp_from_edges(&bm, &src, &dst, &dst, 0.5f);
}

static void pbvh_bmesh_split_edge(EdgeQueueContext *eq_ctx,
                                  BMesh &bm,
                                  MutableSpan<BMeshNode> nodes,
                                  MutableSpan<bool> node_changed,
                                  const int cd_vert_node_offset,
                                  const int cd_face_node_offset,
                                  BMLog &bm_log,
                                  BMEdge *e)
{
  float co_mid[3], no_mid[3];

  /* Get all faces adjacent to the edge. */
  Array<BMLoop *> edge_loops = pbvh_bmesh_edge_loops(e);

  /* Create a new vertex in current node at the edge's midpoint. */
  mid_v3_v3v3(co_mid, e->v1->co, e->v2->co);
  mid_v3_v3v3(no_mid, e->v1->no, e->v2->no);
  normalize_v3(no_mid);

  int node_index = BM_ELEM_CD_GET_INT(e->v1, eq_ctx->cd_vert_node_offset);
  BMVert *v_new = pbvh_bmesh_vert_create(bm,
                                         nodes,
                                         node_changed,
                                         bm_log,
                                         e->v1,
                                         e->v2,
                                         node_index,
                                         co_mid,
                                         no_mid,
                                         cd_vert_node_offset,
                                         eq_ctx->cd_vert_mask_offset);

  /* For each face, add two new triangles and delete the original. */
  for (const int i : edge_loops.index_range()) {
    BMLoop *l_adj = edge_loops[i];
    BMFace *f_adj = l_adj->f;

    BLI_assert(f_adj->len == 3);
    int ni = BM_ELEM_CD_GET_INT(f_adj, eq_ctx->cd_face_node_offset);

    /* Find the vertex not in the edge. */
    BMVert *v_opp = l_adj->prev->v;

    /* Get e->v1 and e->v2 in the order they appear in the existing face so that the new faces'
     * winding orders match. */
    BMVert *v1 = l_adj->v;
    BMVert *v2 = l_adj->next->v;

    if (ni != node_index && i == 0) {
      pbvh_bmesh_vert_ownership_transfer(nodes, node_changed, cd_vert_node_offset, ni, v_new);
    }

    /* The 2 new faces created and assigned to `f_new` have their
     * verts & edges shuffled around.
     *
     * - faces wind anticlockwise in this example.
     * - original edge is `(v1, v2)`
     * - original face is `(v1, v2, v3)`
     *
     * <pre>
     *         + v_opp
     *        /|\
     *       / | \
     *      /  |  \
     *   e4/   |   \ e3
     *    /    |e5  \
     *   /     |     \
     *  /  e1  |  e2  \
     * +-------+-------+
     * v1      v_new     v2
     *  (first) (second)
     * </pre>
     *
     * - f_new (first):  `v_tri=(v1, v_new, v_opp), e_tri=(e1, e5, e4)`
     * - f_new (second): `v_tri=(v_new, v2, v_opp), e_tri=(e2, e3, e5)`
     */

    /* Create first face (v1, v_new, v_opp). */
    const std::array<BMVert *, 3> first_tri({v1, v_new, v_opp});
    const std::array<BMEdge *, 3> first_edges = bm_edges_from_tri(bm, first_tri);
    copy_edge_data(bm, *first_edges[0], *e);

    BMFace *f_new_first = pbvh_bmesh_face_create(
        bm, nodes, node_changed, cd_face_node_offset, bm_log, ni, first_tri, first_edges, f_adj);
    long_edge_queue_face_add(eq_ctx, f_new_first);

    /* Create second face (v_new, v2, v_opp). */
    const std::array<BMVert *, 3> second_tri({v_new, v2, v_opp});
    const std::array<BMEdge *, 3> second_edges{
        BM_edge_create(&bm, second_tri[0], second_tri[1], nullptr, BM_CREATE_NO_DOUBLE),
        BM_edge_create(&bm, second_tri[1], second_tri[2], nullptr, BM_CREATE_NO_DOUBLE),
        first_edges[1],
    };
    copy_edge_data(bm, *second_edges[0], *e);

    BMFace *f_new_second = pbvh_bmesh_face_create(
        bm, nodes, node_changed, cd_face_node_offset, bm_log, ni, second_tri, second_edges, f_adj);
    long_edge_queue_face_add(eq_ctx, f_new_second);

    /* Delete original */
    pbvh_bmesh_face_remove(
        nodes, node_changed, cd_vert_node_offset, cd_face_node_offset, bm_log, f_adj);
    BM_face_kill(&bm, f_adj);

    /* Ensure new vertex is in the node */
    if (!nodes[ni].bm_unique_verts_.contains(v_new)) {
      nodes[ni].bm_other_verts_.add(v_new);
    }

    if (BM_vert_edge_count_is_over(v_opp, 8)) {
      BMIter bm_iter;
      BMEdge *e2;
      BM_ITER_ELEM (e2, &bm_iter, v_opp, BM_EDGES_OF_VERT) {
        long_edge_queue_edge_add(eq_ctx, e2);
      }
    }
  }

  BM_edge_kill(&bm, e);
}

static bool pbvh_bmesh_subdivide_long_edges(EdgeQueueContext *eq_ctx,
                                            BMesh &bm,
                                            MutableSpan<BMeshNode> nodes,
                                            MutableSpan<bool> node_changed,
                                            const int cd_vert_node_offset,
                                            const int cd_face_node_offset,
                                            BMLog &bm_log)
{
  const double start_time = BLI_time_now_seconds();

  bool any_subdivided = false;

  while (!BLI_heapsimple_is_empty(eq_ctx->q->heap)) {
    BMVert **pair = static_cast<BMVert **>(BLI_heapsimple_pop_min(eq_ctx->q->heap));
    BMVert *v1 = pair[0];
    BMVert *v2 = pair[1];

    BLI_mempool_free(eq_ctx->pool, pair);
    pair = nullptr;

    /* Check that the edge still exists */
    BMEdge *e;
    if (!(e = BM_edge_exists(v1, v2))) {
      continue;
    }
#ifdef USE_EDGEQUEUE_TAG
    EDGE_QUEUE_DISABLE(e);
#endif

    BLI_assert(len_squared_v3v3(v1->co, v2->co) > eq_ctx->q->limit_len_squared);

    /* Check that the edge's vertices are still in the Tree. It's
     * possible that an edge collapse has deleted adjacent faces
     * and the node has been split, thus leaving wire edges and
     * associated vertices. */
    if ((BM_ELEM_CD_GET_INT(e->v1, eq_ctx->cd_vert_node_offset) == DYNTOPO_NODE_NONE) ||
        (BM_ELEM_CD_GET_INT(e->v2, eq_ctx->cd_vert_node_offset) == DYNTOPO_NODE_NONE))
    {
      continue;
    }

    any_subdivided = true;

    pbvh_bmesh_split_edge(
        eq_ctx, bm, nodes, node_changed, cd_vert_node_offset, cd_face_node_offset, bm_log, e);
  }

#ifdef USE_EDGEQUEUE_TAG_VERIFY
  pbvh_bmesh_edge_tag_verify(pbvh);
#endif

  CLOG_INFO(
      &LOG, 2, "Long edge subdivision took %f seconds.", BLI_time_now_seconds() - start_time);

  return any_subdivided;
}

/** Check whether the \a vert is adjacent to any face which are adjacent to the #edge. */
static bool vert_in_face_adjacent_to_edge(BMVert &vert, BMEdge &edge)
{
  BMIter bm_iter;
  BMFace *face;
  BM_ITER_ELEM (face, &bm_iter, &edge, BM_FACES_OF_EDGE) {
    if (BM_vert_in_face(&vert, face)) {
      return true;
    }
  }
  return false;
}

/**
 * Merge attributes of a flap face into an edge which will remain after the edge collapse in
 * #pbvh_bmesh_collapse_edge.
 *
 * This function is to be called before faces adjacent to \a e are deleted.
 * This function only handles edge attributes and does not handle face deletion.
 *
 * \param del_face: Face which is adjacent to \a v_del and will form a flap when merging \a v_del
 * to \a v_conn.
 * \param flap_face: Face which is adjacent to \a v_conn and will form a flap when merging \a v_del
 * to \a v_conn.
 * \param e: An edge which is being collapsed. It connects \a v_del and \a v_conn.
 * \param v_del: A vertex which will be removed after the edge collapse.
 * \param l_del: A loop of del_face which is adjacent to v_del.
 * \param v_conn: A vertex which into which geometry is reconnected to after the edge collapse.
 */
static void merge_flap_edge_data(BMesh &bm,
                                 BMFace *del_face,
                                 BMFace *flap_face,
                                 BMEdge *e,
                                 BMVert *v_del,
                                 BMLoop *l_del,
                                 BMVert *v_conn)
{
  /*
   *                                       v_del
   *                                      +
   *          del_face        .        /  |
   *                .               /     |
   *        .                    /        |
   * v1 +---------------------+ v2        |
   *        .                    \        |
   *                .               \     |
   *                          .        \  |
   *          flap_face                   +
   *                                       v_conn
   *
   *
   */

  UNUSED_VARS_NDEBUG(del_face, flap_face);

  /* Faces around `e` (which connects `v_del` to `v_conn`) are to the handled separately from this
   * function. Help troubleshooting cases where these faces are mistakenly considered flaps. */
  BLI_assert(!BM_edge_in_face(e, del_face));
  BLI_assert(!BM_edge_in_face(e, flap_face));

  /* The `l_del->next->v` and `l_del->prev->v` are v1 and v2, but in an unknown order. */
  BMEdge *edge_v1_v2 = BM_edge_exists(l_del->next->v, l_del->prev->v);
  if (!edge_v1_v2) {
    CLOG_WARN(&LOG, "Unable to find edge shared between deleting and flap faces");
    return;
  }

  BLI_assert(BM_edge_in_face(edge_v1_v2, del_face));
  BLI_assert(BM_edge_in_face(edge_v1_v2, flap_face));

  /* Disambiguate v1 from v2: the v2 is adjacent to a face around #e. */
  BMVert *v2 = vert_in_face_adjacent_to_edge(*edge_v1_v2->v1, *e) ? edge_v1_v2->v1 :
                                                                    edge_v1_v2->v2;
  BMVert *v1 = BM_edge_other_vert(edge_v1_v2, v2);

  /* Merge attributes into an edge (v1, v_conn). */
  BMEdge *dst_edge = BM_edge_exists(v1, v_conn);

  const std::array<const BMEdge *, 4> source_edges{
      /* Edges of the `flap_face`.
       * The face will be deleted, effectively being "collapsed" into an edge. */
      edge_v1_v2,
      BM_edge_exists(v2, v_conn),

      /* Edges of the `del_face`.
       * These edges are implicitly merged with the ones from the `flap_face` upon collapsing edge
       * `e`. */
      BM_edge_exists(v1, v_del),
      BM_edge_exists(v2, v_del),
  };

  for (const BMEdge *src_edge : source_edges) {
    if (!src_edge) {
      CLOG_WARN(&LOG, "Unable to find source edge for flap attributes merge");
      continue;
    }

    merge_edge_data(bm, *dst_edge, *src_edge);
  }
}

/**
 * Find vertex which can be an outer for the flap face: the vertex will become loose when the face
 * and its edges are removed.
 * If there are multiple of such vertices, return null.
 */
static BMVert *find_outer_flap_vert(BMFace &face)
{
  BMVert *flap_vert = nullptr;

  BMIter bm_iter;
  BMVert *vert;
  BM_ITER_ELEM (vert, &bm_iter, &face, BM_VERTS_OF_FACE) {
    if (BM_vert_face_count_at_most(vert, 2) == 1) {
      if (flap_vert) {
        /* There are multiple vertices which become loose on removing the face and its edges. */
        return nullptr;
      }
      flap_vert = vert;
    }
  }

  return flap_vert;
}

/**
 * If the `del_face` is a flap, merge edge data from edges adjacent to "corner" vertex into the
 * other edge. The "corner" as it is an "outer", or a vertex which will become loose when the
 * `del_face` and its edges are removed.
 *
 * If the face is not a flap then this function does nothing.
 */
static void try_merge_flap_edge_data_before_dissolve(BMesh &bm, BMFace &face)
{
  /*
   *           v1                  v2
   * ... ------ + ----------------- + ------ ...
   *             \                 /
   *               \             /
   *                 \         /
   *                   \     /
   *                     \ /
   *                      + v_flap
   */

  BMVert *v_flap = find_outer_flap_vert(face);
  if (!v_flap) {
    return;
  }

  BMLoop *l_flap = BM_vert_find_first_loop(v_flap);
  BLI_assert(l_flap->v == v_flap);

  /* Edges which are adjacent ot the v_flap. */
  BMEdge *edge_1 = l_flap->prev->e;
  BMEdge *edge_2 = l_flap->e;

  BLI_assert(BM_edge_face_count(edge_1) == 1);
  BLI_assert(BM_edge_face_count(edge_2) == 1);

  BMEdge *edge_v1_v2 = l_flap->next->e;

  merge_edge_data(bm, *edge_v1_v2, *edge_1);
  merge_edge_data(bm, *edge_v1_v2, *edge_2);
}

/**
 * Merge attributes of edges from \a v_del to \a f
 *
 * This function is to be called before faces adjacent to \a e are deleted.
 * This function only handles edge attributes. and does not handle face deletion.
 *
 * \param del_face: Face which is adjacent to \a v_del and will be deleted as part of merging
 * \a v_del to \a v_conn.
 * \param new_face: A new face which is created from \a del_face by replacing \a v_del with
 * \a v_conn.
 * \param v_del: A vertex which will be removed after the edge collapse.
 * \param l_del: A loop of del_face which is adjacent to v_del.
 * \param v_conn: A vertex which into which geometry is reconnected to after the edge collapse.
 */
static void merge_face_edge_data(BMesh &bm,
                                 BMFace * /*del_face*/,
                                 BMFace *new_face,
                                 BMVert *v_del,
                                 BMLoop *l_del,
                                 BMVert *v_conn)
{
  /* When collapsing an edge (v_conn, v_del) a face (v_conn, v2, v_del) is to be deleted and the
   * v_del reference in the face (v_del, v2, v1) is to be replaced with v_conn. Doing vertex
   * reference replacement in BMesh is not trivial. so for the simplicity the
   * #pbvh_bmesh_collapse_edge deletes both original faces and creates new one (c_conn, v2, v1).
   *
   * When doing such re-creating attributes from old edges are to be merged into the new ones:
   *   - Attributes of (v_del, v1) needs to be merged into (v_conn, v1),
   *   - Attributes of (v_del, v2) needs to be merged into (v_conn, v2),
   *
   * <pre>
   *
   *            v2
   *             +
   *            /|\
   *           / | \
   *          /  |  \
   *         /   |   \
   *        /    |    \
   *       /     |     \
   *      /      |      \
   *     +-------+-------+
   *  v_conn   v_del      v1
   *
   * </pre>
   */

  /* The l_del->next->v and l_del->prev->v are v1 and v2, but in an unknown order. */
  BMEdge *edge_v1_v2 = BM_edge_exists(l_del->next->v, l_del->prev->v);
  if (!edge_v1_v2) {
    CLOG_WARN(&LOG, "Unable to find edge shared between old and new faces");
    return;
  }

  BMIter bm_iter;
  BMEdge *dst_edge;
  BM_ITER_ELEM (dst_edge, &bm_iter, new_face, BM_EDGES_OF_FACE) {
    if (dst_edge == edge_v1_v2) {
      continue;
    }

    BLI_assert(BM_vert_in_edge(dst_edge, v_conn));

    /* Depending on an edge v_other will be v1 or v2. */
    BMVert *v_other = BM_edge_other_vert(dst_edge, v_conn);

    BMEdge *src_edge = BM_edge_exists(v_del, v_other);
    BLI_assert(src_edge);

    if (src_edge) {
      merge_edge_data(bm, *dst_edge, *src_edge);
    }
    else {
      CLOG_WARN(&LOG, "Unable to find edge to merge attributes from");
    }
  }
}

static void pbvh_bmesh_collapse_edge(BMesh &bm,
                                     MutableSpan<BMeshNode> nodes,
                                     MutableSpan<bool> node_changed,
                                     const int cd_vert_node_offset,
                                     const int cd_face_node_offset,
                                     BMLog &bm_log,
                                     BMEdge *e,
                                     BMVert *v1,
                                     BMVert *v2,
                                     GHash *deleted_verts,
                                     EdgeQueueContext *eq_ctx)
{
  const bool v1_on_boundary = is_boundary_vert(*v1);
  const bool v2_on_boundary = is_boundary_vert(*v2);

  BMVert *v_del;
  BMVert *v_conn;
  if (v1_on_boundary || v2_on_boundary) {
    /* Boundary edges can be collapsed with minimal distortion. For those it does not
     * matter too much which vertex to keep and which one to remove.
     *
     * For edges which are adjacent to boundaries, keep the vertex which is on boundary and
     * dissolve the other one. */
    if (v1_on_boundary) {
      v_del = v2;
      v_conn = v1;
    }
    else {
      v_del = v1;
      v_conn = v2;
    }
  }
  else if (BM_ELEM_CD_GET_FLOAT(v1, eq_ctx->cd_vert_mask_offset) <
           BM_ELEM_CD_GET_FLOAT(v2, eq_ctx->cd_vert_mask_offset))
  {
    /* Prefer deleting the vertex that is less masked. */
    v_del = v1;
    v_conn = v2;
  }
  else {
    v_del = v2;
    v_conn = v1;
  }

  /* Remove the merge vertex from the Tree. */
  pbvh_bmesh_vert_remove(nodes, node_changed, cd_vert_node_offset, cd_face_node_offset, v_del);

  /* For all remaining faces of v_del, create a new face that is the
   * same except it uses v_conn instead of v_del */
  /* NOTE: this could be done with BM_vert_splice(), but that requires handling other issues like
   * duplicate edges, so it wouldn't really buy anything. */
  Vector<BMFace *, 16> deleted_faces;

  BMLoop *l;
  BM_LOOPS_OF_VERT_ITER_BEGIN (l, v_del) {
    BMFace *f_del = l->f;

    /* Ignore faces around `e`: they will be deleted explicitly later on.
     * Without ignoring these faces the #bm_face_exists_tri_from_loop_vert() triggers an assert. */
    if (BM_edge_in_face(e, f_del)) {
      continue;
    }

    /* Schedule the faces adjacent to the v_del for deletion first.
     * This way we know that it will be #existing_face which is deleted last when deleting faces
     * which forms a flap. */
    deleted_faces.append(f_del);

    /* Check if a face using these vertices already exists. If so, skip adding this face and mark
     * the existing one for deletion as well. Prevents extraneous "flaps" from being created.
     * Check is similar to #BM_face_exists. */
    if (BMFace *existing_face = bm_face_exists_tri_from_loop_vert(l->next, v_conn)) {
      merge_flap_edge_data(bm, f_del, existing_face, e, v_del, l, v_conn);

      deleted_faces.append(existing_face);
    }
  }
  BM_LOOPS_OF_VERT_ITER_END;

  /* Remove all faces adjacent to the edge. */
  BMLoop *l_adj;
  while ((l_adj = e->l)) {
    BMFace *f_adj = l_adj->f;

    pbvh_bmesh_face_remove(
        nodes, node_changed, cd_vert_node_offset, cd_face_node_offset, bm_log, f_adj);
    BM_face_kill(&bm, f_adj);
  }

  /* Kill the edge. */
  BLI_assert(BM_edge_is_wire(e));
  BM_edge_kill(&bm, e);

  BM_LOOPS_OF_VERT_ITER_BEGIN (l, v_del) {
    /* Get vertices, replace use of v_del with v_conn */
    BMFace *f = l->f;

    if (bm_face_exists_tri_from_loop_vert(l->next, v_conn)) {
      /* This case is handled above. */
    }
    else {
      const std::array<BMVert *, 3> v_tri{v_conn, l->next->v, l->prev->v};

      BLI_assert(!BM_face_exists(v_tri.data(), 3));
      BMeshNode *n = pbvh_bmesh_node_from_face(nodes, cd_face_node_offset, f);
      int ni = n - nodes.data();
      const std::array<BMEdge *, 3> e_tri = bm_edges_from_tri(bm, v_tri);
      BMFace *new_face = pbvh_bmesh_face_create(
          bm, nodes, node_changed, cd_face_node_offset, bm_log, ni, v_tri, e_tri, f);

      merge_face_edge_data(bm, f, new_face, v_del, l, v_conn);

      /* Ensure that v_conn is in the new face's node */
      if (!n->bm_unique_verts_.contains(v_conn)) {
        n->bm_other_verts_.add(v_conn);
      }
    }
  }
  BM_LOOPS_OF_VERT_ITER_END;

  /* Delete the tagged faces. */
  for (BMFace *f_del : deleted_faces) {
    /* Get vertices and edges of face. */
    BLI_assert(f_del->len == 3);
    BMLoop *l_iter = BM_FACE_FIRST_LOOP(f_del);
    const std::array<BMVert *, 3> v_tri{l_iter->v, l_iter->next->v, l_iter->next->next->v};
    const std::array<BMEdge *, 3> e_tri{l_iter->e, l_iter->next->e, l_iter->next->next->e};

    /* if its sa flap face merge its "outer" edge data into "base", so that boundary is propagated
     * from edges which are about to be deleted to the base of the triangle and will stay attached
     * to the mesh. */
    try_merge_flap_edge_data_before_dissolve(bm, *f_del);

    /* Remove the face */
    pbvh_bmesh_face_remove(
        nodes, node_changed, cd_vert_node_offset, cd_face_node_offset, bm_log, f_del);
    BM_face_kill(&bm, f_del);

    /* Check if any of the face's edges are now unused by any
     * face, if so delete them */
    for (const int j : IndexRange(3)) {
      if (BM_edge_is_wire(e_tri[j])) {
        BM_edge_kill(&bm, e_tri[j]);
      }
    }

    /* Check if any of the face's vertices are now unused, if so
     * remove them from the Tree */
    for (const int j : IndexRange(3)) {
      if ((v_tri[j] != v_del) && (v_tri[j]->e == nullptr)) {
        pbvh_bmesh_vert_remove(
            nodes, node_changed, cd_vert_node_offset, cd_face_node_offset, v_tri[j]);

        BM_log_vert_removed(&bm_log, v_tri[j], eq_ctx->cd_vert_mask_offset);

        if (v_tri[j] == v_conn) {
          v_conn = nullptr;
        }
        BLI_ghash_insert(deleted_verts, v_tri[j], nullptr);
        BM_vert_kill(&bm, v_tri[j]);
      }
    }
  }

  /* If the v_conn was not removed above move it to the midpoint of v_conn and v_del. Doing so
   *  helps avoiding long stretched and degenerated triangles.
   *
   * However, if the vertex is on a boundary, do not move it to preserve the shape of the
   * boundary. */
  if (v_conn != nullptr && !is_boundary_vert(*v_conn)) {
    BM_log_vert_before_modified(&bm_log, v_conn, eq_ctx->cd_vert_mask_offset);
    mid_v3_v3v3(v_conn->co, v_conn->co, v_del->co);
    add_v3_v3(v_conn->no, v_del->no);
    normalize_v3(v_conn->no);
  }

  if (v_conn != nullptr) {
    /* Update bounding boxes attached to the connected vertex.
     * Note that we can often get-away without this but causes #48779. */
    BM_LOOPS_OF_VERT_ITER_BEGIN (l, v_conn) {
      const int f_node = pbvh_bmesh_node_index_from_face(cd_face_node_offset, l->f);
      node_changed[f_node] = true;
    }
    BM_LOOPS_OF_VERT_ITER_END;
  }

  /* Delete v_del */
  BLI_assert(!BM_vert_face_check(v_del));
  BM_log_vert_removed(&bm_log, v_del, eq_ctx->cd_vert_mask_offset);
  /* v_conn == nullptr is OK */
  BLI_ghash_insert(deleted_verts, v_del, v_conn);
  BM_vert_kill(&bm, v_del);
}

static bool pbvh_bmesh_collapse_short_edges(EdgeQueueContext *eq_ctx,
                                            const float min_edge_len,
                                            BMesh &bm,
                                            MutableSpan<BMeshNode> nodes,
                                            MutableSpan<bool> node_changed,
                                            const int cd_vert_node_offset,
                                            const int cd_face_node_offset,
                                            BMLog &bm_log)
{
  const double start_time = BLI_time_now_seconds();

  const float min_len_squared = min_edge_len * min_edge_len;
  bool any_collapsed = false;
  /* Deleted verts point to vertices they were merged into, or nullptr when removed. */
  GHash *deleted_verts = BLI_ghash_ptr_new("deleted_verts");

  while (!BLI_heapsimple_is_empty(eq_ctx->q->heap)) {
    BMVert **pair = static_cast<BMVert **>(BLI_heapsimple_pop_min(eq_ctx->q->heap));
    BMVert *v1 = pair[0];
    BMVert *v2 = pair[1];
    BLI_mempool_free(eq_ctx->pool, pair);
    pair = nullptr;

    /* Check the verts still exists. */
    if (!(v1 = bm_vert_hash_lookup_chain(deleted_verts, v1)) ||
        !(v2 = bm_vert_hash_lookup_chain(deleted_verts, v2)) || (v1 == v2))
    {
      continue;
    }

    /* Check that the edge still exists. */
    BMEdge *e;
    if (!(e = BM_edge_exists(v1, v2))) {
      continue;
    }
#ifdef USE_EDGEQUEUE_TAG
    EDGE_QUEUE_DISABLE(e);
#endif

    if (len_squared_v3v3(v1->co, v2->co) >= min_len_squared) {
      continue;
    }

    /* Check that the edge's vertices are still in the Tree. It's possible that
     * an edge collapse has deleted adjacent faces and the node has been split, thus leaving wire
     * edges and associated vertices. */
    if ((BM_ELEM_CD_GET_INT(e->v1, eq_ctx->cd_vert_node_offset) == DYNTOPO_NODE_NONE) ||
        (BM_ELEM_CD_GET_INT(e->v2, eq_ctx->cd_vert_node_offset) == DYNTOPO_NODE_NONE))
    {
      continue;
    }

    any_collapsed = true;

    pbvh_bmesh_collapse_edge(bm,
                             nodes,
                             node_changed,
                             cd_vert_node_offset,
                             cd_face_node_offset,
                             bm_log,
                             e,
                             v1,
                             v2,
                             deleted_verts,
                             eq_ctx);
  }

  BLI_ghash_free(deleted_verts, nullptr, nullptr);

  CLOG_INFO(&LOG, 2, "Short edge collapse took %f seconds.", BLI_time_now_seconds() - start_time);

  return any_collapsed;
}

/************************* Called from pbvh.cc *************************/

bool node_raycast_bmesh(BMeshNode &node,
                        const float3 &ray_start,
                        const float3 &ray_normal,
                        IsectRayPrecalc *isect_precalc,
                        float *depth,
                        bool use_original,
                        BMVert **r_active_vertex,
                        float3 &r_face_normal)
{
  bool hit = false;
  float3 nearest_vertex_co(0.0f);

  use_original = use_original && !node.orig_tris_.is_empty();

  if (use_original) {
    for (const int tri_idx : node.orig_tris_.index_range()) {
      float *cos[3];

      cos[0] = node.orig_positions_[node.orig_tris_[tri_idx][0]];
      cos[1] = node.orig_positions_[node.orig_tris_[tri_idx][1]];
      cos[2] = node.orig_positions_[node.orig_tris_[tri_idx][2]];

      if (ray_face_intersection_tri(ray_start, isect_precalc, cos[0], cos[1], cos[2], depth)) {
        hit = true;

        normal_tri_v3(r_face_normal, cos[0], cos[1], cos[2]);

        if (r_active_vertex) {
          float3 location(0.0f);
          madd_v3_v3v3fl(location, ray_start, ray_normal, *depth);
          for (const int i : IndexRange(3)) {
            if (i == 0 ||
                len_squared_v3v3(location, cos[i]) < len_squared_v3v3(location, nearest_vertex_co))
            {
              copy_v3_v3(nearest_vertex_co, cos[i]);
              *r_active_vertex = node.orig_verts_[node.orig_tris_[tri_idx][i]];
            }
          }
        }
      }
    }
  }
  else {
    for (BMFace *f : node.bm_faces_) {
      BLI_assert(f->len == 3);

      if (!BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
        BMVert *v_tri[3];

        BM_face_as_array_vert_tri(f, v_tri);
        if (ray_face_intersection_tri(
                ray_start, isect_precalc, v_tri[0]->co, v_tri[1]->co, v_tri[2]->co, depth))
        {
          hit = true;

          normal_tri_v3(r_face_normal, v_tri[0]->co, v_tri[1]->co, v_tri[2]->co);

          if (r_active_vertex) {
            float3 location(0.0f);
            madd_v3_v3v3fl(location, ray_start, ray_normal, *depth);
            for (const int i : IndexRange(3)) {
              if (i == 0 || len_squared_v3v3(location, v_tri[i]->co) <
                                len_squared_v3v3(location, nearest_vertex_co))
              {
                copy_v3_v3(nearest_vertex_co, v_tri[i]->co);
                *r_active_vertex = v_tri[i];
              }
            }
          }
        }
      }
    }
  }

  return hit;
}

bool raycast_node_detail_bmesh(BMeshNode &node,
                               const float3 &ray_start,
                               IsectRayPrecalc *isect_precalc,
                               float *depth,
                               float *r_edge_length)
{
  if (node.flag_ & Node::FullyHidden) {
    return false;
  }

  bool hit = false;
  BMFace *f_hit = nullptr;

  for (BMFace *f : node.bm_faces_) {
    BLI_assert(f->len == 3);
    if (!BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      BMVert *v_tri[3];
      bool hit_local;
      BM_face_as_array_vert_tri(f, v_tri);
      hit_local = ray_face_intersection_tri(
          ray_start, isect_precalc, v_tri[0]->co, v_tri[1]->co, v_tri[2]->co, depth);

      if (hit_local) {
        f_hit = f;
        hit = true;
      }
    }
  }

  if (hit) {
    BMVert *v_tri[3];
    BM_face_as_array_vert_tri(f_hit, v_tri);
    float len1 = len_squared_v3v3(v_tri[0]->co, v_tri[1]->co);
    float len2 = len_squared_v3v3(v_tri[1]->co, v_tri[2]->co);
    float len3 = len_squared_v3v3(v_tri[2]->co, v_tri[0]->co);

    /* Detail returned will be set to the maximum allowed size, so take max here. */
    *r_edge_length = sqrtf(max_fff(len1, len2, len3));
  }

  return hit;
}

bool bmesh_node_nearest_to_ray(BMeshNode &node,
                               const float3 &ray_start,
                               const float3 &ray_normal,
                               float *r_depth,
                               float *dist_sq,
                               bool use_original)
{
  bool hit = false;

  if (use_original && !node.orig_tris_.is_empty()) {
    for (const int i : node.orig_tris_.index_range()) {
      const int *t = node.orig_tris_[i];
      hit |= ray_face_nearest_tri(ray_start,
                                  ray_normal,
                                  node.orig_positions_[t[0]],
                                  node.orig_positions_[t[1]],
                                  node.orig_positions_[t[2]],
                                  r_depth,
                                  dist_sq);
    }
  }
  else {
    for (BMFace *f : node.bm_faces_) {
      BLI_assert(f->len == 3);
      if (!BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
        BMVert *v_tri[3];

        BM_face_as_array_vert_tri(f, v_tri);
        hit |= ray_face_nearest_tri(
            ray_start, ray_normal, v_tri[0]->co, v_tri[1]->co, v_tri[2]->co, r_depth, dist_sq);
      }
    }
  }

  return hit;
}

void bmesh_normals_update(Tree &pbvh, const IndexMask &nodes_to_update)
{
  const MutableSpan<BMeshNode> nodes = pbvh.nodes<BMeshNode>();
  nodes_to_update.foreach_index([&](const int i) {
    BMeshNode &node = nodes[i];
    for (BMFace *face : node.bm_faces_) {
      BM_face_normal_update(face);
    }
    for (BMVert *vert : node.bm_unique_verts_) {
      BM_vert_normal_update(vert);
    }
    for (BMVert *vert : node.bm_other_verts_) {
      BM_vert_normal_update(vert);
    }
  });
}

struct FastNodeBuildInfo {
  int totface; /* Number of faces. */
  int start;   /* Start of faces in array. */
  FastNodeBuildInfo *child1;
  FastNodeBuildInfo *child2;
};

/**
 * Recursively split the node if it exceeds the leaf_limit.
 * This function is multi-thread-able since each invocation applies
 * to a sub part of the arrays.
 */
static void pbvh_bmesh_node_limit_ensure_fast(const MutableSpan<BMFace *> nodeinfo,
                                              const Span<Bounds<float3>> face_bounds,
                                              FastNodeBuildInfo *node,
                                              MemArena *arena)
{
  FastNodeBuildInfo *child1, *child2;

  if (node->totface <= leaf_limit) {
    return;
  }

  /* Calculate bounding box around primitive centroids. */
  Bounds<float3> cb = negative_bounds();
  for (int i = 0; i < node->totface; i++) {
    BMFace *f = nodeinfo[i + node->start];
    const int face_index = BM_elem_index_get(f);
    const float3 center = math::midpoint(face_bounds[face_index].min, face_bounds[face_index].max);
    math::min_max(center, cb.min, cb.max);
  }

  /* Initialize the children. */

  /* Find widest axis and its midpoint. */
  const int axis = math::dominant_axis(cb.max - cb.min);
  const float mid = math::midpoint(cb.max[axis], cb.min[axis]);

  int num_child1 = 0, num_child2 = 0;

  /* Split vertices along the middle line. */
  const int end = node->start + node->totface;
  for (int i = node->start; i < end - num_child2; i++) {
    BMFace *f = nodeinfo[i];
    const int face_i = BM_elem_index_get(f);

    if (math::midpoint(face_bounds[face_i].min[axis], face_bounds[face_i].max[axis]) > mid) {
      int i_iter = end - num_child2 - 1;
      int candidate = -1;
      /* Found a face that should be part of another node, look for a face to substitute with. */

      for (; i_iter > i; i_iter--) {
        BMFace *f_iter = nodeinfo[i_iter];
        const int face_iter_i = BM_elem_index_get(f_iter);
        if (math::midpoint(face_bounds[face_iter_i].min[axis],
                           face_bounds[face_iter_i].max[axis]) <= mid)
        {
          candidate = i_iter;
          break;
        }

        num_child2++;
      }

      if (candidate != -1) {
        BMFace *tmp = nodeinfo[i];
        nodeinfo[i] = nodeinfo[candidate];
        nodeinfo[candidate] = tmp;
        /* Increase both counts. */
        num_child1++;
        num_child2++;
      }
      else {
        /* Not finding candidate means second half of array part is full of
         * second node parts, just increase the number of child nodes for it. */
        num_child2++;
      }
    }
    else {
      num_child1++;
    }
  }

  /* Ensure at least one child in each node. */
  if (num_child2 == 0) {
    num_child2++;
    num_child1--;
  }
  else if (num_child1 == 0) {
    num_child1++;
    num_child2--;
  }

  /* At this point, faces should have been split along the array range sequentially,
   * each sequential part belonging to one node only. */
  BLI_assert((num_child1 + num_child2) == node->totface);

  node->child1 = child1 = static_cast<FastNodeBuildInfo *>(
      BLI_memarena_alloc(arena, sizeof(FastNodeBuildInfo)));
  node->child2 = child2 = static_cast<FastNodeBuildInfo *>(
      BLI_memarena_alloc(arena, sizeof(FastNodeBuildInfo)));

  child1->totface = num_child1;
  child1->start = node->start;
  child2->totface = num_child2;
  child2->start = node->start + num_child1;
  child1->child1 = child1->child2 = child2->child1 = child2->child2 = nullptr;

  pbvh_bmesh_node_limit_ensure_fast(nodeinfo, face_bounds, child1, arena);
  pbvh_bmesh_node_limit_ensure_fast(nodeinfo, face_bounds, child2, arena);
}

static void pbvh_bmesh_create_nodes_fast_recursive(Vector<BMeshNode> &nodes,
                                                   const int cd_vert_node_offset,
                                                   const int cd_face_node_offset,
                                                   const Span<BMFace *> nodeinfo,
                                                   const Span<Bounds<float3>> face_bounds,
                                                   FastNodeBuildInfo *node,
                                                   int node_index)
{
  BMeshNode *n = &nodes[node_index];
  /* Two cases, node does not have children or does have children. */
  if (node->child1) {
    int children_offset_ = nodes.size();

    n->children_offset_ = children_offset_;
    nodes.resize(nodes.size() + 2);
    pbvh_bmesh_create_nodes_fast_recursive(nodes,
                                           cd_vert_node_offset,
                                           cd_face_node_offset,
                                           nodeinfo,
                                           face_bounds,
                                           node->child1,
                                           children_offset_);
    pbvh_bmesh_create_nodes_fast_recursive(nodes,
                                           cd_vert_node_offset,
                                           cd_face_node_offset,
                                           nodeinfo,
                                           face_bounds,
                                           node->child2,
                                           children_offset_ + 1);
  }
  else {
    /* Node does not have children so it's a leaf node, populate with faces and tag accordingly
     * this is an expensive part but it's not so easily thread-able due to vertex node indices. */

    n->flag_ |= Node::Leaf;
    n->bm_faces_.reserve(node->totface);

    const int end = node->start + node->totface;

    for (int i = node->start; i < end; i++) {
      BMFace *f = nodeinfo[i];

      /* Update ownership of faces. */
      n->bm_faces_.add_new(f);
      BM_ELEM_CD_SET_INT(f, cd_face_node_offset, node_index);

      /* Update vertices. */
      BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
      BMLoop *l_iter = l_first;
      do {
        BMVert *v = l_iter->v;
        if (!n->bm_unique_verts_.contains(v)) {
          if (BM_ELEM_CD_GET_INT(v, cd_vert_node_offset) != DYNTOPO_NODE_NONE) {
            n->bm_other_verts_.add(v);
          }
          else {
            n->bm_unique_verts_.add(v);
            BM_ELEM_CD_SET_INT(v, cd_vert_node_offset, node_index);
          }
        }
      } while ((l_iter = l_iter->next) != l_first);
    }
  }
}

/***************************** Public API *****************************/

Tree Tree::from_bmesh(BMesh &bm)
{
  Tree pbvh(Type::BMesh);
  if (bm.totface == 0) {
    return pbvh;
  }

  const int cd_vert_node_offset = CustomData_get_offset_named(
      &bm.vdata, CD_PROP_INT32, ".sculpt_dyntopo_node_id_vertex");
  const int cd_face_node_offset = CustomData_get_offset_named(
      &bm.pdata, CD_PROP_INT32, ".sculpt_dyntopo_node_id_face");

  /* bounding box array of all faces, no need to recalculate every time. */
  Array<Bounds<float3>> face_bounds(bm.totface);
  Array<BMFace *> nodeinfo(bm.totface);
  MemArena *arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, "fast Tree node storage");

  BMIter iter;
  BMFace *f;
  int i;
  BM_ITER_MESH_INDEX (f, &iter, &bm, BM_FACES_OF_MESH, i) {
    face_bounds[i] = negative_bounds();

    BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
    BMLoop *l_iter = l_first;
    do {
      math::min_max(float3(l_iter->v->co), face_bounds[i].min, face_bounds[i].max);
    } while ((l_iter = l_iter->next) != l_first);

    /* so we can do direct lookups on 'face_bounds' */
    BM_elem_index_set(f, i); /* set_dirty! */
    nodeinfo[i] = f;
    BM_ELEM_CD_SET_INT(f, cd_face_node_offset, DYNTOPO_NODE_NONE);
  }
  /* Likely this is already dirty. */
  bm.elem_index_dirty |= BM_FACE;

  BMVert *v;
  BM_ITER_MESH (v, &iter, &bm, BM_VERTS_OF_MESH) {
    BM_ELEM_CD_SET_INT(v, cd_vert_node_offset, DYNTOPO_NODE_NONE);
  }

  /* Set up root node. */
  FastNodeBuildInfo rootnode = {0};
  rootnode.totface = bm.totface;

  /* Start recursion, assign faces to nodes accordingly. */
  pbvh_bmesh_node_limit_ensure_fast(nodeinfo, face_bounds, &rootnode, arena);

  /* We now have all faces assigned to a node,
   * next we need to assign those to the gsets of the nodes. */

  /* Start with all faces in the root node. */
  /* Take root node and visit and populate children recursively. */
  Vector<BMeshNode> &nodes = std::get<Vector<BMeshNode>>(pbvh.nodes_);
  nodes.resize(1);
  pbvh_bmesh_create_nodes_fast_recursive(
      nodes, cd_vert_node_offset, cd_face_node_offset, nodeinfo, face_bounds, &rootnode, 0);

  pbvh.tag_positions_changed(nodes.index_range());
  update_bounds_bmesh(bm, pbvh);
  store_bounds_orig(pbvh);

  threading::parallel_for(nodes.index_range(), 8, [&](const IndexRange range) {
    for (const int i : range) {
      const Set<BMFace *, 0> &faces = BKE_pbvh_bmesh_node_faces(&nodes[i]);
      if (std::all_of(faces.begin(), faces.end(), [&](const BMFace *face) {
            return BM_elem_flag_test(face, BM_ELEM_HIDDEN);
          }))
      {
        nodes[i].flag_ |= Node::FullyHidden;
      }
    }
  });

  update_mask_bmesh(bm, nodes.index_range(), pbvh);

  BLI_memarena_free(arena);
  return pbvh;
}

bool bmesh_update_topology(BMesh &bm,
                           Tree &pbvh,
                           BMLog &bm_log,
                           PBVHTopologyUpdateMode mode,
                           const float min_edge_len,
                           const float max_edge_len,
                           const float center[3],
                           const float view_normal[3],
                           float radius,
                           const bool use_frontface,
                           const bool use_projected)
{
  const int cd_vert_node_offset = CustomData_get_offset_named(
      &bm.vdata, CD_PROP_INT32, ".sculpt_dyntopo_node_id_vertex");
  const int cd_face_node_offset = CustomData_get_offset_named(
      &bm.pdata, CD_PROP_INT32, ".sculpt_dyntopo_node_id_face");
  const int cd_vert_mask_offset = CustomData_get_offset_named(
      &bm.vdata, CD_PROP_FLOAT, ".sculpt_mask");

  bool modified = false;

  if (view_normal) {
    BLI_assert(len_squared_v3(view_normal) != 0.0f);
  }

  MutableSpan<BMeshNode> nodes = pbvh.nodes<BMeshNode>();
  Array<bool> node_changed(nodes.size(), false);

  if (mode & PBVH_Collapse) {
    EdgeQueue q;
    BLI_mempool *queue_pool = BLI_mempool_create(sizeof(BMVert *) * 2, 0, 128, BLI_MEMPOOL_NOP);
    EdgeQueueContext eq_ctx = {
        &q,
        queue_pool,
        &bm,
        cd_vert_mask_offset,
        cd_vert_node_offset,
        cd_face_node_offset,
    };

    short_edge_queue_create(
        &eq_ctx, min_edge_len, nodes, center, view_normal, radius, use_frontface, use_projected);
    modified |= pbvh_bmesh_collapse_short_edges(&eq_ctx,
                                                min_edge_len,
                                                bm,
                                                nodes,
                                                node_changed,
                                                cd_vert_node_offset,
                                                cd_face_node_offset,
                                                bm_log);
    BLI_heapsimple_free(q.heap, nullptr);
    BLI_mempool_destroy(queue_pool);
  }

  if (mode & PBVH_Subdivide) {
    EdgeQueue q;
    BLI_mempool *queue_pool = BLI_mempool_create(sizeof(BMVert *) * 2, 0, 128, BLI_MEMPOOL_NOP);
    EdgeQueueContext eq_ctx = {
        &q,
        queue_pool,
        &bm,
        cd_vert_mask_offset,
        cd_vert_node_offset,
        cd_face_node_offset,
    };

    long_edge_queue_create(
        &eq_ctx, max_edge_len, nodes, center, view_normal, radius, use_frontface, use_projected);
    modified |= pbvh_bmesh_subdivide_long_edges(
        &eq_ctx, bm, nodes, node_changed, cd_vert_node_offset, cd_face_node_offset, bm_log);
    BLI_heapsimple_free(q.heap, nullptr);
    BLI_mempool_destroy(queue_pool);
  }

  IndexMaskMemory memory;
  const IndexMask node_mask = IndexMask::from_bools(node_changed, memory);
  pbvh.tag_positions_changed(node_mask);
  pbvh.tag_topology_changed(node_mask);

  /* Unmark nodes. */
  for (Node &node : nodes) {
    if (node.flag_ & Node::Leaf && node.flag_ & Node::UpdateTopology) {
      node.flag_ &= ~Node::UpdateTopology;
    }
  }

  /* Go over all changed nodes and check if anything needs to be updated. */
  for (BMeshNode &node : nodes) {
    if (node.flag_ & Node::Leaf && node.flag_ & Node::TopologyUpdated) {
      node.flag_ &= ~Node::TopologyUpdated;

      if (!node.orig_tris_.is_empty()) {
        /* Reallocate original triangle data. */
        pbvh_bmesh_node_drop_orig(&node);
        BKE_pbvh_bmesh_node_save_orig(&bm, &bm_log, &node, true);
      }
    }
  }

#ifdef USE_VERIFY
  pbvh_bmesh_verify(pbvh);
#endif

  return modified;
}

/* Updates a given Tree Node with the original coordinates of the corresponding
 * BMesh vertex. Attempts to retrieve the value from the BMLog, falls back to the vertex's current
 * coordinates if it is either not found in the log or not requested. */
static void copy_original_vert(BMLog *log, BMeshNode *node, BMVert *v, int i, bool use_original)
{
  if (!use_original) {
    node->orig_positions_[i] = v->co;
  }
  else {
    const float *origco = BM_log_find_original_vert_co(log, v);
    if (origco) {
      node->orig_positions_[i] = origco;
    }
    else {
      node->orig_positions_[i] = v->co;
    }
  }

  node->orig_verts_[i] = v;
}

}  // namespace blender::bke::pbvh

void BKE_pbvh_bmesh_node_save_orig(BMesh *bm,
                                   BMLog *log,
                                   blender::bke::pbvh::BMeshNode *node,
                                   bool use_original)
{
  using namespace blender;
  /* Skip if original coords/triangles are already saved. */
  if (!node->orig_tris_.is_empty()) {
    return;
  }

  const int totvert = node->bm_unique_verts_.size() + node->bm_other_verts_.size();

  node->orig_positions_.reinitialize(totvert);
  node->orig_verts_.reinitialize(totvert);

  VectorSet<BMVert *> vert_map;
  vert_map.reserve(totvert);

  /* Copy out the vertices and assign a temporary index. */
  int i = 0;
  for (BMVert *v : node->bm_unique_verts_) {
    bke::pbvh::copy_original_vert(log, node, v, i, use_original);
    vert_map.add(v);
    i++;
  }
  for (BMVert *v : node->bm_other_verts_) {
    bke::pbvh::copy_original_vert(log, node, v, i, use_original);
    vert_map.add(v);
    i++;
  }
  /* Likely this is already dirty. */
  bm->elem_index_dirty |= BM_VERT;

  /* Copy the triangles */
  const int tris_num = std::count_if(
      node->bm_faces_.begin(), node->bm_faces_.end(), [&](const BMFace *face) {
        return !BM_elem_flag_test_bool(face, BM_ELEM_HIDDEN);
      });
  node->orig_tris_.reinitialize(tris_num);
  i = 0;
  for (BMFace *f : node->bm_faces_) {
    if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
      continue;
    }
    const std::array<BMVert *, 3> verts = bke::pbvh::bm_face_as_array(f);
    node->orig_tris_[i] = int3(
        vert_map.index_of(verts[0]), vert_map.index_of(verts[1]), vert_map.index_of(verts[2]));
    i++;
  }
}

void BKE_pbvh_bmesh_after_stroke(BMesh &bm, blender::bke::pbvh::Tree &pbvh)
{
  using namespace blender;
  const int cd_vert_node_offset = CustomData_get_offset_named(
      &bm.vdata, CD_PROP_INT32, ".sculpt_dyntopo_node_id_vertex");
  const int cd_face_node_offset = CustomData_get_offset_named(
      &bm.pdata, CD_PROP_INT32, ".sculpt_dyntopo_node_id_face");

  Vector<bke::pbvh::BMeshNode> &nodes = std::get<Vector<bke::pbvh::BMeshNode>>(pbvh.nodes_);
  Vector<bool> node_changed(nodes.size(), false);
  const IndexRange orig_range = nodes.index_range();
  for (const int i : orig_range) {
    bke::pbvh::BMeshNode *n = &nodes[i];
    if (n->flag_ & bke::pbvh::Node::Leaf) {
      /* Free `orco` / `ortri` data. */
      pbvh_bmesh_node_drop_orig(n);

      /* Recursively split nodes that have gotten too many elements. */
      pbvh_bmesh_node_limit_ensure(
          bm, nodes, node_changed, cd_vert_node_offset, cd_face_node_offset, i);
    }
  }

  IndexMaskMemory memory;
  const IndexMask node_mask = IndexMask::from_bools(node_changed, memory);
  pbvh.tag_positions_changed(node_mask);
  pbvh.tag_topology_changed(node_mask);
  update_mask_bmesh(bm, node_mask, pbvh);
}

void BKE_pbvh_node_mark_topology_update(blender::bke::pbvh::Node &node)
{
  node.flag_ |= blender::bke::pbvh::Node::UpdateTopology;
}

const blender::Set<BMVert *, 0> &BKE_pbvh_bmesh_node_unique_verts(
    blender::bke::pbvh::BMeshNode *node)
{
  return node->bm_unique_verts_;
}

const blender::Set<BMVert *, 0> &BKE_pbvh_bmesh_node_other_verts(
    blender::bke::pbvh::BMeshNode *node)
{
  return node->bm_other_verts_;
}

const blender::Set<BMFace *, 0> &BKE_pbvh_bmesh_node_faces(blender::bke::pbvh::BMeshNode *node)
{
  return node->bm_faces_;
}

/****************************** Debugging *****************************/

#if 0

static void pbvh_bmesh_print(Tree *pbvh)
{
  fprintf(stderr, "\npbvh=%p\n", pbvh);
  fprintf(stderr, "bm_face_to_node:\n");

  BMIter iter;
  BMFace *f;
  BM_ITER_MESH (f, &iter, pbvh->header.bm, BM_FACES_OF_MESH) {
    fprintf(
        stderr, "  %d -> %d\n", BM_elem_index_get(f), pbvh_bmesh_node_index_from_face(pbvh, f));
  }

  fprintf(stderr, "bm_vert_to_node:\n");
  BMVert *v;
  BM_ITER_MESH (v, &iter, pbvh->header.bm, BM_FACES_OF_MESH) {
    fprintf(
        stderr, "  %d -> %d\n", BM_elem_index_get(v), pbvh_bmesh_node_index_from_vert(pbvh, v));
  }

  for (int n = 0; n < pbvh->totnode; n++) {
    Node *node = &pbvh->nodes_[n];
    if (!(node->flag & Node::Leaf)) {
      continue;
    }

    GSetIterator gs_iter;
    fprintf(stderr, "node %d\n  faces:\n", n);
    GSET_ITER (gs_iter, node->bm_faces_)
      fprintf(stderr, "    %d\n", BM_elem_index_get((BMFace *)BLI_gsetIterator_getKey(&gs_iter)));
    fprintf(stderr, "  unique verts:\n");
    GSET_ITER (gs_iter, node->bm_unique_verts_)
      fprintf(stderr, "    %d\n", BM_elem_index_get((BMVert *)BLI_gsetIterator_getKey(&gs_iter)));
    fprintf(stderr, "  other verts:\n");
    GSET_ITER (gs_iter, node->bm_other_verts_)
      fprintf(stderr, "    %d\n", BM_elem_index_get((BMVert *)BLI_gsetIterator_getKey(&gs_iter)));
  }
}

static void print_flag_factors(int flag)
{
  printf("flag=0x%x:\n", flag);
  for (int i = 0; i < 32; i++) {
    if (flag & (1 << i)) {
      printf("  %d (1 << %d)\n", 1 << i, i);
    }
  }
}
#endif

#ifdef USE_VERIFY

static void pbvh_bmesh_verify(Tree *pbvh)
{
  /* Build list of faces & verts to lookup. */
  GSet *faces_all = BLI_gset_ptr_new_ex(__func__, pbvh->header.bm->totface);
  BMIter iter;

  {
    BMFace *f;
    BM_ITER_MESH (f, &iter, pbvh->header.bm, BM_FACES_OF_MESH) {
      BLI_assert(BM_ELEM_CD_GET_INT(f, cd_face_node_offset) != DYNTOPO_NODE_NONE);
      BLI_gset_insert(faces_all, f);
    }
  }

  GSet *verts_all = BLI_gset_ptr_new_ex(__func__, pbvh->header.bm->totvert);
  {
    BMVert *v;
    BM_ITER_MESH (v, &iter, pbvh->header.bm, BM_VERTS_OF_MESH) {
      if (BM_ELEM_CD_GET_INT(v, cd_vert_node_offset) != DYNTOPO_NODE_NONE) {
        BLI_gset_insert(verts_all, v);
      }
    }
  }

  /* Check vert/face counts. */
  {
    int totface = 0, totvert = 0;
    for (int i = 0; i < pbvh->totnode; i++) {
      Node *n = &pbvh->nodes_[i];
      totface += n->bm_faces_.is_empty() ? n->bm_faces_.size() : 0;
      totvert += n->bm_unique_verts_ ? n->bm_unique_verts_.size() : 0;
    }

    BLI_assert(totface == BLI_gset_len(faces_all));
    BLI_assert(totvert == BLI_gset_len(verts_all));
  }

  {
    BMFace *f;
    BM_ITER_MESH (f, &iter, pbvh->header.bm, BM_FACES_OF_MESH) {
      BMIter bm_iter;
      BMVert *v;
      Node *n = pbvh_bmesh_node_lookup(pbvh, f);

      /* Check that the face's node is a leaf. */
      BLI_assert(n->flag & Node::Leaf);

      /* Check that the face's node knows it owns the face. */
      BLI_assert(n->bm_faces_.contains(f));

      /* Check the face's vertices... */
      BM_ITER_ELEM (v, &bm_iter, f, BM_VERTS_OF_FACE) {
        Node *nv;

        /* Check that the vertex is in the node. */
        BLI_assert(BLI_gset_haskey(n->bm_unique_verts_, v) ^
                   BLI_gset_haskey(n->bm_other_verts_, v));

        /* Check that the vertex has a node owner. */
        nv = pbvh_bmesh_node_lookup(pbvh, v);

        /* Check that the vertex's node knows it owns the vert. */
        BLI_assert(BLI_gset_haskey(nv->bm_unique_verts_, v));

        /* Check that the vertex isn't duplicated as an 'other' vert. */
        BLI_assert(!BLI_gset_haskey(nv->bm_other_verts_, v));
      }
    }
  }

  /* Check verts */
  {
    BMVert *v;
    BM_ITER_MESH (v, &iter, pbvh->header.bm, BM_VERTS_OF_MESH) {
      /* Vertex isn't tracked. */
      if (BM_ELEM_CD_GET_INT(v, cd_vert_node_offset) == DYNTOPO_NODE_NONE) {
        continue;
      }

      Node *n = pbvh_bmesh_node_lookup(pbvh, v);

      /* Check that the vert's node is a leaf. */
      BLI_assert(n->flag & Node::Leaf);

      /* Check that the vert's node knows it owns the vert. */
      BLI_assert(BLI_gset_haskey(n->bm_unique_verts_, v));

      /* Check that the vertex isn't duplicated as an 'other' vert. */
      BLI_assert(!BLI_gset_haskey(n->bm_other_verts_, v));

      /* Check that the vert's node also contains one of the vert's adjacent faces. */
      bool found = false;
      BMIter bm_iter;
      BMFace *f = nullptr;
      BM_ITER_ELEM (f, &bm_iter, v, BM_FACES_OF_VERT) {
        if (pbvh_bmesh_node_lookup(pbvh, f) == n) {
          found = true;
          break;
        }
      }
      BLI_assert(found || f == nullptr);

#  if 1
      /* total freak stuff, check if node exists somewhere else */
      /* Slow */
      for (int i = 0; i < pbvh->totnode; i++) {
        Node *n_other = &pbvh->nodes_[i];
        if ((n != n_other) && (n_other->bm_unique_verts_)) {
          BLI_assert(!BLI_gset_haskey(n_other->bm_unique_verts_, v));
        }
      }
#  endif
    }
  }

#  if 0
  /* check that every vert belongs somewhere */
  /* Slow */
  BM_ITER_MESH (vi, &iter, pbvh->header.bm, BM_VERTS_OF_MESH) {
    bool has_unique = false;
    for (int i = 0; i < pbvh->totnode; i++) {
      Node *n = &pbvh->nodes_[i];
      if ((n->bm_unique_verts_ != nullptr) && BLI_gset_haskey(n->bm_unique_verts_, vi)) {
        has_unique = true;
      }
    }
    BLI_assert(has_unique);
    vert_count++;
  }

  /* If totvert differs from number of verts inside the hash. hash-totvert is checked above. */
  BLI_assert(vert_count == pbvh->header.bm->totvert);
#  endif

  /* Check that node elements are recorded in the top level */
  for (int i = 0; i < pbvh->totnode; i++) {
    Node *n = &pbvh->nodes_[i];
    if (n->flag & Node::Leaf) {
      GSetIterator gs_iter;

      for (BMFace *f : n->bm_faces_) {
        Node *n_other = pbvh_bmesh_node_lookup(pbvh, f);
        BLI_assert(n == n_other);
        BLI_assert(BLI_gset_haskey(faces_all, f));
      }

      GSET_ITER (gs_iter, n->bm_unique_verts_) {
        BMVert *v = BLI_gsetIterator_getKey(&gs_iter);
        Node *n_other = pbvh_bmesh_node_lookup(pbvh, v);
        BLI_assert(!BLI_gset_haskey(n->bm_other_verts_, v));
        BLI_assert(n == n_other);
        BLI_assert(BLI_gset_haskey(verts_all, v));
      }

      GSET_ITER (gs_iter, n->bm_other_verts_) {
        BMVert *v = BLI_gsetIterator_getKey(&gs_iter);
        /* this happens sometimes and seems harmless */
        // BLI_assert(!BM_vert_face_check(v));
        BLI_assert(BLI_gset_haskey(verts_all, v));
      }
    }
  }

  BLI_gset_free(faces_all, nullptr);
  BLI_gset_free(verts_all, nullptr);
}

#endif
