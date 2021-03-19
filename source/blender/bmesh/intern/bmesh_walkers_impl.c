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
 * \ingroup bmesh
 *
 * BMesh Walker Code.
 */

#include <string.h>

#include "BLI_utildefines.h"

#include "BKE_customdata.h"

#include "bmesh.h"
#include "intern/bmesh_walkers_private.h"

/* Pop into stack memory (common operation). */
#define BMW_state_remove_r(walker, owalk) \
  { \
    memcpy(owalk, BMW_current_state(walker), sizeof(*(owalk))); \
    BMW_state_remove(walker); \
  } \
  (void)0

/* -------------------------------------------------------------------- */
/** \name Mask Flag Checks
 * \{ */

static bool bmw_mask_check_vert(BMWalker *walker, BMVert *v)
{
  if ((walker->flag & BMW_FLAG_TEST_HIDDEN) && BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
    return false;
  }
  if (walker->mask_vert && !BMO_vert_flag_test(walker->bm, v, walker->mask_vert)) {
    return false;
  }
  return true;
}

static bool bmw_mask_check_edge(BMWalker *walker, BMEdge *e)
{
  if ((walker->flag & BMW_FLAG_TEST_HIDDEN) && BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
    return false;
  }
  if (walker->mask_edge && !BMO_edge_flag_test(walker->bm, e, walker->mask_edge)) {
    return false;
  }
  return true;
}

static bool bmw_mask_check_face(BMWalker *walker, BMFace *f)
{
  if ((walker->flag & BMW_FLAG_TEST_HIDDEN) && BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
    return false;
  }
  if (walker->mask_face && !BMO_face_flag_test(walker->bm, f, walker->mask_face)) {
    return false;
  }
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name BMesh Queries (modified to check walker flags)
 * \{ */

/**
 * Check for a wire edge, taking ignoring hidden.
 */
static bool bmw_edge_is_wire(const BMWalker *walker, const BMEdge *e)
{
  if (walker->flag & BMW_FLAG_TEST_HIDDEN) {
    /* Check if this is a wire edge, ignoring hidden faces. */
    if (BM_edge_is_wire(e)) {
      return true;
    }
    return BM_edge_is_all_face_flag_test(e, BM_ELEM_HIDDEN, false);
  }
  return BM_edge_is_wire(e);
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Shell Walker
 *
 * Starts at a vertex on the mesh and walks over the 'shell' it belongs
 * to via visiting connected edges.
 *
 * takes an edge or vertex as an argument, and spits out edges,
 * restrict flag acts on the edges as well.
 *
 * \todo Add restriction flag/callback for wire edges.
 * \{ */
static void bmw_VertShellWalker_visitEdge(BMWalker *walker, BMEdge *e)
{
  BMwShellWalker *shellWalk = NULL;

  if (BLI_gset_haskey(walker->visit_set, e)) {
    return;
  }

  if (!bmw_mask_check_edge(walker, e)) {
    return;
  }

  shellWalk = BMW_state_add(walker);
  shellWalk->curedge = e;
  BLI_gset_insert(walker->visit_set, e);
}

static void bmw_VertShellWalker_begin(BMWalker *walker, void *data)
{
  BMIter eiter;
  BMHeader *h = data;
  BMEdge *e;
  BMVert *v;

  if (UNLIKELY(h == NULL)) {
    return;
  }

  switch (h->htype) {
    case BM_VERT: {
      /* Starting the walk at a vert, add all the edges to the work-list. */
      v = (BMVert *)h;
      BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
        bmw_VertShellWalker_visitEdge(walker, e);
      }
      break;
    }

    case BM_EDGE: {
      /* Starting the walk at an edge, add the single edge to the work-list. */
      e = (BMEdge *)h;
      bmw_VertShellWalker_visitEdge(walker, e);
      break;
    }
    default:
      BLI_assert(0);
  }
}

static void *bmw_VertShellWalker_yield(BMWalker *walker)
{
  BMwShellWalker *shellWalk = BMW_current_state(walker);
  return shellWalk->curedge;
}

static void *bmw_VertShellWalker_step(BMWalker *walker)
{
  BMwShellWalker *swalk, owalk;
  BMEdge *e, *e2;
  BMVert *v;
  BMIter iter;
  int i;

  BMW_state_remove_r(walker, &owalk);
  swalk = &owalk;

  e = swalk->curedge;

  for (i = 0; i < 2; i++) {
    v = i ? e->v2 : e->v1;
    BM_ITER_ELEM (e2, &iter, v, BM_EDGES_OF_VERT) {
      bmw_VertShellWalker_visitEdge(walker, e2);
    }
  }

  return e;
}

#if 0
static void *bmw_VertShellWalker_step(BMWalker *walker)
{
  BMEdge *curedge, *next = NULL;
  BMVert *v_old = NULL;
  bool restrictpass = true;
  BMwShellWalker shellWalk = *((BMwShellWalker *)BMW_current_state(walker));

  if (!BLI_gset_haskey(walker->visit_set, shellWalk.base)) {
    BLI_gset_insert(walker->visit_set, shellWalk.base);
  }

  BMW_state_remove(walker);

  /* Find the next edge whose other vertex has not been visited. */
  curedge = shellWalk.curedge;
  do {
    if (!BLI_gset_haskey(walker->visit_set, curedge)) {
      if (!walker->restrictflag ||
          (walker->restrictflag &&
           BMO_edge_flag_test(walker->bm, curedge, walker->restrictflag))) {
        BMwShellWalker *newstate;

        v_old = BM_edge_other_vert(curedge, shellWalk.base);

        /* Push a new state onto the stack. */
        newState = BMW_state_add(walker);
        BLI_gset_insert(walker->visit_set, curedge);

        /* Populate the new state. */

        newState->base = v_old;
        newState->curedge = curedge;
      }
    }
  } while ((curedge = bmesh_disk_edge_next(curedge, shellWalk.base)) != shellWalk.curedge);

  return shellWalk.curedge;
}
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name LoopShell Walker
 *
 * Starts at any element on the mesh and walks over the 'shell' it belongs
 * to via visiting connected loops.
 *
 * \note this is mainly useful to loop over a shell delimited by edges.
 * \{ */
static void bmw_LoopShellWalker_visitLoop(BMWalker *walker, BMLoop *l)
{
  BMwLoopShellWalker *shellWalk = NULL;

  if (BLI_gset_haskey(walker->visit_set, l)) {
    return;
  }

  if (!bmw_mask_check_face(walker, l->f)) {
    return;
  }

  shellWalk = BMW_state_add(walker);
  shellWalk->curloop = l;
  BLI_gset_insert(walker->visit_set, l);
}

static void bmw_LoopShellWalker_begin(BMWalker *walker, void *data)
{
  BMIter iter;
  BMHeader *h = data;

  if (UNLIKELY(h == NULL)) {
    return;
  }

  switch (h->htype) {
    case BM_LOOP: {
      /* Starting the walk at a vert, add all the edges to the work-list. */
      BMLoop *l = (BMLoop *)h;
      bmw_LoopShellWalker_visitLoop(walker, l);
      break;
    }

    case BM_VERT: {
      BMVert *v = (BMVert *)h;
      BMLoop *l;
      BM_ITER_ELEM (l, &iter, v, BM_LOOPS_OF_VERT) {
        bmw_LoopShellWalker_visitLoop(walker, l);
      }
      break;
    }
    case BM_EDGE: {
      BMEdge *e = (BMEdge *)h;
      BMLoop *l;
      BM_ITER_ELEM (l, &iter, e, BM_LOOPS_OF_EDGE) {
        bmw_LoopShellWalker_visitLoop(walker, l);
      }
      break;
    }
    case BM_FACE: {
      BMFace *f = (BMFace *)h;
      BMLoop *l = BM_FACE_FIRST_LOOP(f);
      /* Walker will handle other loops within the face. */
      bmw_LoopShellWalker_visitLoop(walker, l);
      break;
    }
    default:
      BLI_assert(0);
  }
}

static void *bmw_LoopShellWalker_yield(BMWalker *walker)
{
  BMwLoopShellWalker *shellWalk = BMW_current_state(walker);
  return shellWalk->curloop;
}

static void bmw_LoopShellWalker_step_impl(BMWalker *walker, BMLoop *l)
{
  BMEdge *e_edj_pair[2];
  int i;

  /* Seems paranoid, but one caller also walks edges. */
  BLI_assert(l->head.htype == BM_LOOP);

  bmw_LoopShellWalker_visitLoop(walker, l->next);
  bmw_LoopShellWalker_visitLoop(walker, l->prev);

  e_edj_pair[0] = l->e;
  e_edj_pair[1] = l->prev->e;

  for (i = 0; i < 2; i++) {
    BMEdge *e = e_edj_pair[i];
    if (bmw_mask_check_edge(walker, e)) {
      BMLoop *l_iter, *l_first;

      l_iter = l_first = e->l;
      do {
        BMLoop *l_radial = (l_iter->v == l->v) ? l_iter : l_iter->next;
        BLI_assert(l_radial->v == l->v);
        if (l != l_radial) {
          bmw_LoopShellWalker_visitLoop(walker, l_radial);
        }
      } while ((l_iter = l_iter->radial_next) != l_first);
    }
  }
}

static void *bmw_LoopShellWalker_step(BMWalker *walker)
{
  BMwLoopShellWalker *swalk, owalk;
  BMLoop *l;

  BMW_state_remove_r(walker, &owalk);
  swalk = &owalk;

  l = swalk->curloop;
  bmw_LoopShellWalker_step_impl(walker, l);

  return l;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name LoopShell & 'Wire' Walker
 *
 * Piggyback on top of #BMwLoopShellWalker, but also walk over wire edges
 * This isn't elegant but users expect it when selecting linked,
 * so we can support delimiters _and_ walking over wire edges.
 *
 * Details:
 * - can yield edges (as well as loops)
 * - only step over wire edges.
 * - verts and edges are stored in `visit_set_alt`.
 * \{ */

static void bmw_LoopShellWalker_visitEdgeWire(BMWalker *walker, BMEdge *e)
{
  BMwLoopShellWireWalker *shellWalk = NULL;

  BLI_assert(bmw_edge_is_wire(walker, e));

  if (BLI_gset_haskey(walker->visit_set_alt, e)) {
    return;
  }

  if (!bmw_mask_check_edge(walker, e)) {
    return;
  }

  shellWalk = BMW_state_add(walker);
  shellWalk->curelem = (BMElem *)e;
  BLI_gset_insert(walker->visit_set_alt, e);
}

static void bmw_LoopShellWireWalker_visitVert(BMWalker *walker, BMVert *v, const BMEdge *e_from)
{
  BMEdge *e;

  BLI_assert(v->head.htype == BM_VERT);

  if (BLI_gset_haskey(walker->visit_set_alt, v)) {
    return;
  }

  if (!bmw_mask_check_vert(walker, v)) {
    return;
  }

  e = v->e;
  do {
    if (bmw_edge_is_wire(walker, e) && (e != e_from)) {
      BMVert *v_other;
      BMIter iter;
      BMLoop *l;

      bmw_LoopShellWalker_visitEdgeWire(walker, e);

      /* Check if we step onto a non-wire vertex. */
      v_other = BM_edge_other_vert(e, v);
      BM_ITER_ELEM (l, &iter, v_other, BM_LOOPS_OF_VERT) {

        bmw_LoopShellWalker_visitLoop(walker, l);
      }
    }
  } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);

  BLI_gset_insert(walker->visit_set_alt, v);
}

static void bmw_LoopShellWireWalker_begin(BMWalker *walker, void *data)
{
  BMHeader *h = data;

  if (UNLIKELY(h == NULL)) {
    return;
  }

  bmw_LoopShellWalker_begin(walker, data);

  switch (h->htype) {
    case BM_LOOP: {
      BMLoop *l = (BMLoop *)h;
      bmw_LoopShellWireWalker_visitVert(walker, l->v, NULL);
      break;
    }

    case BM_VERT: {
      BMVert *v = (BMVert *)h;
      if (v->e) {
        bmw_LoopShellWireWalker_visitVert(walker, v, NULL);
      }
      break;
    }
    case BM_EDGE: {
      BMEdge *e = (BMEdge *)h;
      if (bmw_mask_check_edge(walker, e)) {
        bmw_LoopShellWireWalker_visitVert(walker, e->v1, NULL);
        bmw_LoopShellWireWalker_visitVert(walker, e->v2, NULL);
      }
      else if (e->l) {
        BMLoop *l_iter, *l_first;

        l_iter = l_first = e->l;
        do {
          bmw_LoopShellWalker_visitLoop(walker, l_iter);
          bmw_LoopShellWalker_visitLoop(walker, l_iter->next);
        } while ((l_iter = l_iter->radial_next) != l_first);
      }
      break;
    }
    case BM_FACE: {
      /* Wire verts will be walked over. */
      break;
    }
    default:
      BLI_assert(0);
  }
}

static void *bmw_LoopShellWireWalker_yield(BMWalker *walker)
{
  BMwLoopShellWireWalker *shellWalk = BMW_current_state(walker);
  return shellWalk->curelem;
}

static void *bmw_LoopShellWireWalker_step(BMWalker *walker)
{
  BMwLoopShellWireWalker *swalk, owalk;

  BMW_state_remove_r(walker, &owalk);
  swalk = &owalk;

  if (swalk->curelem->head.htype == BM_LOOP) {
    BMLoop *l = (BMLoop *)swalk->curelem;

    bmw_LoopShellWalker_step_impl(walker, l);

    bmw_LoopShellWireWalker_visitVert(walker, l->v, NULL);

    return l;
  }

  BMEdge *e = (BMEdge *)swalk->curelem;

  BLI_assert(e->head.htype == BM_EDGE);

  bmw_LoopShellWireWalker_visitVert(walker, e->v1, e);
  bmw_LoopShellWireWalker_visitVert(walker, e->v2, e);

  return e;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name FaceShell Walker
 *
 * Starts at an edge on the mesh and walks over the 'shell' it belongs
 * to via visiting connected faces.
 * \{ */
static void bmw_FaceShellWalker_visitEdge(BMWalker *walker, BMEdge *e)
{
  BMwShellWalker *shellWalk = NULL;

  if (BLI_gset_haskey(walker->visit_set, e)) {
    return;
  }

  if (!bmw_mask_check_edge(walker, e)) {
    return;
  }

  shellWalk = BMW_state_add(walker);
  shellWalk->curedge = e;
  BLI_gset_insert(walker->visit_set, e);
}

static void bmw_FaceShellWalker_begin(BMWalker *walker, void *data)
{
  BMEdge *e = data;
  bmw_FaceShellWalker_visitEdge(walker, e);
}

static void *bmw_FaceShellWalker_yield(BMWalker *walker)
{
  BMwShellWalker *shellWalk = BMW_current_state(walker);
  return shellWalk->curedge;
}

static void *bmw_FaceShellWalker_step(BMWalker *walker)
{
  BMwShellWalker *swalk, owalk;
  BMEdge *e, *e2;
  BMIter iter;

  BMW_state_remove_r(walker, &owalk);
  swalk = &owalk;

  e = swalk->curedge;

  if (e->l) {
    BMLoop *l_iter, *l_first;

    l_iter = l_first = e->l;
    do {
      BM_ITER_ELEM (e2, &iter, l_iter->f, BM_EDGES_OF_FACE) {
        if (e2 != e) {
          bmw_FaceShellWalker_visitEdge(walker, e2);
        }
      }
    } while ((l_iter = l_iter->radial_next) != l_first);
  }

  return e;
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Connected Vertex Walker
 *
 * Similar to shell walker, but visits vertices instead of edges.
 *
 * Walk from a vertex to all connected vertices.
 * \{ */
static void bmw_ConnectedVertexWalker_visitVertex(BMWalker *walker, BMVert *v)
{
  BMwConnectedVertexWalker *vwalk;

  if (BLI_gset_haskey(walker->visit_set, v)) {
    /* Already visited. */
    return;
  }

  if (!bmw_mask_check_vert(walker, v)) {
    /* Not flagged for walk. */
    return;
  }

  vwalk = BMW_state_add(walker);
  vwalk->curvert = v;
  BLI_gset_insert(walker->visit_set, v);
}

static void bmw_ConnectedVertexWalker_begin(BMWalker *walker, void *data)
{
  BMVert *v = data;
  bmw_ConnectedVertexWalker_visitVertex(walker, v);
}

static void *bmw_ConnectedVertexWalker_yield(BMWalker *walker)
{
  BMwConnectedVertexWalker *vwalk = BMW_current_state(walker);
  return vwalk->curvert;
}

static void *bmw_ConnectedVertexWalker_step(BMWalker *walker)
{
  BMwConnectedVertexWalker *vwalk, owalk;
  BMVert *v, *v2;
  BMEdge *e;
  BMIter iter;

  BMW_state_remove_r(walker, &owalk);
  vwalk = &owalk;

  v = vwalk->curvert;

  BM_ITER_ELEM (e, &iter, v, BM_EDGES_OF_VERT) {
    v2 = BM_edge_other_vert(e, v);
    if (!BLI_gset_haskey(walker->visit_set, v2)) {
      bmw_ConnectedVertexWalker_visitVertex(walker, v2);
    }
  }

  return v;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Island Boundary Walker
 *
 * Starts at a edge on the mesh and walks over the boundary of an island it belongs to.
 *
 * \note that this doesn't work on non-manifold geometry.
 * it might be better to rewrite this to extract
 * boundary info from the island walker, rather than directly walking
 * over the boundary.  raises an error if it encounters non-manifold geometry.
 *
 * \todo Add restriction flag/callback for wire edges.
 * \{ */
static void bmw_IslandboundWalker_begin(BMWalker *walker, void *data)
{
  BMLoop *l = data;
  BMwIslandboundWalker *iwalk = NULL;

  iwalk = BMW_state_add(walker);

  iwalk->base = iwalk->curloop = l;
  iwalk->lastv = l->v;

  BLI_gset_insert(walker->visit_set, data);
}

static void *bmw_IslandboundWalker_yield(BMWalker *walker)
{
  BMwIslandboundWalker *iwalk = BMW_current_state(walker);

  return iwalk->curloop;
}

static void *bmw_IslandboundWalker_step(BMWalker *walker)
{
  BMwIslandboundWalker *iwalk, owalk;
  BMVert *v;
  BMEdge *e;
  BMFace *f;
  BMLoop *l;

  memcpy(&owalk, BMW_current_state(walker), sizeof(owalk));
  /* Normally we'd remove here, but delay until after error checking. */
  iwalk = &owalk;

  l = iwalk->curloop;
  e = l->e;

  v = BM_edge_other_vert(e, iwalk->lastv);

  /* Pop off current state. */
  BMW_state_remove(walker);

  f = l->f;

  while (1) {
    l = BM_loop_other_edge_loop(l, v);
    if (BM_loop_is_manifold(l)) {
      l = l->radial_next;
      f = l->f;
      e = l->e;

      if (!bmw_mask_check_face(walker, f)) {
        l = l->radial_next;
        break;
      }
    }
    else {
      /* Treat non-manifold edges as boundaries. */
      f = l->f;
      e = l->e;
      break;
    }
  }

  if (l == owalk.curloop) {
    return NULL;
  }
  if (BLI_gset_haskey(walker->visit_set, l)) {
    return owalk.curloop;
  }

  BLI_gset_insert(walker->visit_set, l);
  iwalk = BMW_state_add(walker);
  iwalk->base = owalk.base;

#if 0
  if (!BMO_face_flag_test(walker->bm, l->f, walker->restrictflag)) {
    iwalk->curloop = l->radial_next;
  }
  else {
    iwalk->curloop = l;
  }
#else
  iwalk->curloop = l;
#endif
  iwalk->lastv = v;

  return owalk.curloop;
}

/* -------------------------------------------------------------------- */
/** \name Island Walker
 *
 * Starts at a tool flagged-face and walks over the face region
 *
 * \todo Add restriction flag/callback for wire edges.
 * \{ */
static void bmw_IslandWalker_begin(BMWalker *walker, void *data)
{
  BMwIslandWalker *iwalk = NULL;

  if (!bmw_mask_check_face(walker, data)) {
    return;
  }

  iwalk = BMW_state_add(walker);
  BLI_gset_insert(walker->visit_set, data);

  iwalk->cur = data;
}

static void *bmw_IslandWalker_yield(BMWalker *walker)
{
  BMwIslandWalker *iwalk = BMW_current_state(walker);

  return iwalk->cur;
}

static void *bmw_IslandWalker_step_ex(BMWalker *walker, bool only_manifold)
{
  BMwIslandWalker *iwalk, owalk;
  BMLoop *l_iter, *l_first;

  BMW_state_remove_r(walker, &owalk);
  iwalk = &owalk;

  l_iter = l_first = BM_FACE_FIRST_LOOP(iwalk->cur);
  do {
    /* Could skip loop here too, but don't add unless we need it. */
    if (!bmw_mask_check_edge(walker, l_iter->e)) {
      continue;
    }

    BMLoop *l_radial_iter;

    if (only_manifold && (l_iter->radial_next != l_iter)) {
      int face_count = 1;
      /* Check other faces (not this one), ensure only one other Can be walked onto. */
      l_radial_iter = l_iter->radial_next;
      do {
        if (bmw_mask_check_face(walker, l_radial_iter->f)) {
          face_count++;
          if (face_count == 3) {
            break;
          }
        }
      } while ((l_radial_iter = l_radial_iter->radial_next) != l_iter);

      if (face_count != 2) {
        continue;
      }
    }

    l_radial_iter = l_iter;
    while ((l_radial_iter = l_radial_iter->radial_next) != l_iter) {
      BMFace *f = l_radial_iter->f;

      if (!bmw_mask_check_face(walker, f)) {
        continue;
      }

      /* Saves checking #BLI_gset_haskey below (manifold edges there's a 50% chance). */
      if (f == iwalk->cur) {
        continue;
      }

      if (BLI_gset_haskey(walker->visit_set, f)) {
        continue;
      }

      iwalk = BMW_state_add(walker);
      iwalk->cur = f;
      BLI_gset_insert(walker->visit_set, f);
      break;
    }
  } while ((l_iter = l_iter->next) != l_first);

  return owalk.cur;
}

static void *bmw_IslandWalker_step(BMWalker *walker)
{
  return bmw_IslandWalker_step_ex(walker, false);
}

/**
 * Ignore edges that don't have 2x usable faces.
 */
static void *bmw_IslandManifoldWalker_step(BMWalker *walker)
{
  return bmw_IslandWalker_step_ex(walker, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edge Loop Walker
 *
 * Starts at a tool-flagged edge and walks over the edge loop
 * \{ */

/* utility function to see if an edge is a part of an ngon boundary */
static bool bm_edge_is_single(BMEdge *e)
{
  return ((BM_edge_is_boundary(e)) && (e->l->f->len > 4) &&
          (BM_edge_is_boundary(e->l->next->e) || BM_edge_is_boundary(e->l->prev->e)));
}

static void bmw_EdgeLoopWalker_begin(BMWalker *walker, void *data)
{
  BMwEdgeLoopWalker *lwalk = NULL, owalk, *owalk_pt;
  BMEdge *e = data;
  BMVert *v;
  const int vert_edge_count[2] = {
      BM_vert_edge_count_nonwire(e->v1),
      BM_vert_edge_count_nonwire(e->v2),
  };
  const int vert_face_count[2] = {
      BM_vert_face_count(e->v1),
      BM_vert_face_count(e->v2),
  };

  v = e->v1;

  lwalk = BMW_state_add(walker);
  BLI_gset_insert(walker->visit_set, e);

  lwalk->cur = lwalk->start = e;
  lwalk->lastv = lwalk->startv = v;
  lwalk->is_boundary = BM_edge_is_boundary(e);
  lwalk->is_single = (lwalk->is_boundary && bm_edge_is_single(e));

  /**
   * Detect an NGon (face-hub)
   * =========================
   *
   * The face-hub - #BMwEdgeLoopWalker.f_hub - is set when there is an ngon
   * on one side of the edge and a series of faces on the other,
   * loop around the ngon for as long as it's connected to faces which would form an edge loop
   * in the absence of the ngon (used as the hub).
   *
   * This isn't simply ignoring the ngon though, as the edges looped over must all be
   * connected to the hub.
   *
   * NGon in Grid Example
   * --------------------
   * \code{.txt}
   * +-----+-----+-----+-----+-----+
   * |     |     |     |     |     |
   * +-----va=ea=+==eb=+==ec=vb----+
   * |     |                 |     |
   * +-----+                 +-----+
   * |     |      f_hub      |     |
   * +-----+                 +-----+
   * |     |                 |     |
   * +-----+-----+-----+-----+-----+
   * |     |     |     |     |     |
   * +-----+-----+-----+-----+-----+
   * \endcode
   *
   * In the example above, starting from edges marked `ea/eb/ec`,
   * the will detect `f_hub` and walk along the edge loop between `va -> vb`.
   * The same is true for any of the un-marked sides of the ngon,
   * walking stops for vertices with >= 3 connected faces (in this case they look like corners).
   *
   * Mixed Triangle-Fan & Quad Example
   * ---------------------------------
   * \code{.txt}
   * +-----------------------------------------------+
   * |              f_hub                            |
   * va-ea-vb=eb=+==ec=+=ed==+=ee=vc=ef=vd----------ve
   * |     |\    |     |     |    /     / \          |
   * |     | \    \    |    /    /     /   \         |
   * |     |  \   |    |    |   /     /     \        |
   * |     |   \   \   |   /   /     /       \       |
   * |     |    \  |   |   |  /     /         \      |
   * |     |     \  \  |  /  /     /           \     |
   * |     |      \ |  |  | /     /             \    |
   * |     |       \ \ | / /     /               \   |
   * |     |        \| | |/     /                 \  |
   * |     |         \\|//     /                   \ |
   * |     |          \|/     /                     \|
   * +-----+-----------+-----+-----------------------+
   * \endcode
   *
   * In the example above, starting from edges marked `eb/eb/ed/ed/ef`,
   * the will detect `f_hub` and walk along the edge loop between `vb -> vd`.
   *
   * Notice `vb` and `vd` delimit the loop, since the faces connected to `vb`
   * excluding `f_hub` don't share an edge, which isn't walked over in the case
   * of boundaries either.
   *
   * Notice `vc` doesn't delimit stepping from `ee` onto `ef` as the stepping method used
   * doesn't differentiate between the number of sides of faces opposite `f_hub`,
   * only that each of the connected faces share an edge.
   */
  if ((lwalk->is_boundary == false) &&
      /* Without checking the face count, the 3 edges could be this edge
       * plus two boundary edges (which would not be stepped over), see T84906. */
      ((vert_edge_count[0] == 3 && vert_face_count[0] == 3) ||
       (vert_edge_count[1] == 3 && vert_face_count[1] == 3))) {
    BMIter iter;
    BMFace *f_iter;
    BMFace *f_best = NULL;

    BM_ITER_ELEM (f_iter, &iter, e, BM_FACES_OF_EDGE) {
      if (f_best == NULL || f_best->len < f_iter->len) {
        f_best = f_iter;
      }
    }

    if (f_best) {
      /* Only use hub selection for 5+ sides else this could
       * conflict with normal edge loop selection. */
      lwalk->f_hub = f_best->len > 4 ? f_best : NULL;
    }
    else {
      /* Edge doesn't have any faces connected to it. */
      lwalk->f_hub = NULL;
    }
  }
  else {
    lwalk->f_hub = NULL;
  }

  /* Rewind. */
  while ((owalk_pt = BMW_current_state(walker))) {
    owalk = *((BMwEdgeLoopWalker *)owalk_pt);
    BMW_walk(walker);
  }

  lwalk = BMW_state_add(walker);
  *lwalk = owalk;

  lwalk->lastv = lwalk->startv = BM_edge_other_vert(owalk.cur, lwalk->lastv);

  BLI_gset_clear(walker->visit_set, NULL);
  BLI_gset_insert(walker->visit_set, owalk.cur);
}

static void *bmw_EdgeLoopWalker_yield(BMWalker *walker)
{
  BMwEdgeLoopWalker *lwalk = BMW_current_state(walker);

  return lwalk->cur;
}

static void *bmw_EdgeLoopWalker_step(BMWalker *walker)
{
  BMwEdgeLoopWalker *lwalk, owalk;
  BMEdge *e, *nexte = NULL;
  BMLoop *l;
  BMVert *v;

  BMW_state_remove_r(walker, &owalk);
  lwalk = &owalk;

  e = lwalk->cur;
  l = e->l;

  if (owalk.f_hub) { /* NGON EDGE */
    int vert_edge_tot;

    v = BM_edge_other_vert(e, lwalk->lastv);

    vert_edge_tot = BM_vert_edge_count_nonwire(v);

    if (vert_edge_tot == 3) {
      l = BM_face_other_vert_loop(owalk.f_hub, lwalk->lastv, v);
      nexte = BM_edge_exists(v, l->v);

      if (bmw_mask_check_edge(walker, nexte) && !BLI_gset_haskey(walker->visit_set, nexte) &&
          /* Never step onto a boundary edge, this gives odd-results. */
          (BM_edge_is_boundary(nexte) == false)) {
        lwalk = BMW_state_add(walker);
        lwalk->cur = nexte;
        lwalk->lastv = v;

        lwalk->is_boundary = owalk.is_boundary;
        lwalk->is_single = owalk.is_single;
        lwalk->f_hub = owalk.f_hub;

        BLI_gset_insert(walker->visit_set, nexte);
      }
    }
  }
  else if (l == NULL) { /* WIRE EDGE */
    BMIter eiter;

    /* Match trunk: mark all connected wire edges. */
    for (int i = 0; i < 2; i++) {
      v = i ? e->v2 : e->v1;

      BM_ITER_ELEM (nexte, &eiter, v, BM_EDGES_OF_VERT) {
        if ((nexte->l == NULL) && bmw_mask_check_edge(walker, nexte) &&
            !BLI_gset_haskey(walker->visit_set, nexte)) {
          lwalk = BMW_state_add(walker);
          lwalk->cur = nexte;
          lwalk->lastv = v;

          lwalk->is_boundary = owalk.is_boundary;
          lwalk->is_single = owalk.is_single;
          lwalk->f_hub = owalk.f_hub;

          BLI_gset_insert(walker->visit_set, nexte);
        }
      }
    }
  }
  else if (owalk.is_boundary == false) { /* NORMAL EDGE WITH FACES */
    int vert_edge_tot;

    v = BM_edge_other_vert(e, lwalk->lastv);

    vert_edge_tot = BM_vert_edge_count_nonwire(v);

    /* Typical looping over edges in the middle of a mesh.
     * Why use 2 here at all? - for internal ngon loops it can be useful. */
    if (ELEM(vert_edge_tot, 4, 2)) {
      int i_opposite = vert_edge_tot / 2;
      int i = 0;
      do {
        l = BM_loop_other_edge_loop(l, v);
        if (BM_edge_is_manifold(l->e)) {
          l = l->radial_next;
        }
        else {
          l = NULL;
          break;
        }
      } while ((++i != i_opposite));
    }
    else {
      l = NULL;
    }

    if (l != NULL) {
      if (l != e->l && bmw_mask_check_edge(walker, l->e) &&
          !BLI_gset_haskey(walker->visit_set, l->e)) {
        lwalk = BMW_state_add(walker);
        lwalk->cur = l->e;
        lwalk->lastv = v;

        lwalk->is_boundary = owalk.is_boundary;
        lwalk->is_single = owalk.is_single;
        lwalk->f_hub = owalk.f_hub;

        BLI_gset_insert(walker->visit_set, l->e);
      }
    }
  }
  else if (owalk.is_boundary == true) { /* BOUNDARY EDGE WITH FACES */
    int vert_edge_tot;

    v = BM_edge_other_vert(e, lwalk->lastv);

    vert_edge_tot = BM_vert_edge_count_nonwire(v);

    /* Check if we should step, this is fairly involved. */
    if (
        /* Walk over boundary of faces but stop at corners. */
        (owalk.is_single == false && vert_edge_tot > 2) ||

        /* Initial edge was a boundary, so is this edge and vertex is only a part of this face
         * this lets us walk over the boundary of an ngon which is handy. */
        (owalk.is_single == true && vert_edge_tot == 2 && BM_edge_is_boundary(e))) {
      /* Find next boundary edge in the fan. */
      do {
        l = BM_loop_other_edge_loop(l, v);
        if (BM_edge_is_manifold(l->e)) {
          l = l->radial_next;
        }
        else if (BM_edge_is_boundary(l->e)) {
          break;
        }
        else {
          l = NULL;
          break;
        }
      } while (true);
    }

    if (owalk.is_single == false && l && bm_edge_is_single(l->e)) {
      l = NULL;
    }

    if (l != NULL) {
      if (l != e->l && bmw_mask_check_edge(walker, l->e) &&
          !BLI_gset_haskey(walker->visit_set, l->e)) {
        lwalk = BMW_state_add(walker);
        lwalk->cur = l->e;
        lwalk->lastv = v;

        lwalk->is_boundary = owalk.is_boundary;
        lwalk->is_single = owalk.is_single;
        lwalk->f_hub = owalk.f_hub;

        BLI_gset_insert(walker->visit_set, l->e);
      }
    }
  }

  return owalk.cur;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Face Loop Walker
 *
 * Starts at a tool-flagged face and walks over the face loop
 * Conditions for starting and stepping the face loop have been
 * tuned in an attempt to match the face loops built by edit-mesh
 * \{ */

/**
 * Check whether the face loop should includes the face specified
 * by the given #BMLoop.
 */
static bool bmw_FaceLoopWalker_include_face(BMWalker *walker, BMLoop *l)
{
  /* Face must have degree 4. */
  if (l->f->len != 4) {
    return false;
  }

  if (!bmw_mask_check_face(walker, l->f)) {
    return false;
  }

  /* The face must not have been already visited. */
  if (BLI_gset_haskey(walker->visit_set, l->f) && BLI_gset_haskey(walker->visit_set_alt, l->e)) {
    return false;
  }

  return true;
}

/* Check whether the face loop can start from the given edge. */
static bool bmw_FaceLoopWalker_edge_begins_loop(BMWalker *walker, BMEdge *e)
{
  /* There is no face loop starting from a wire edge. */
  if (BM_edge_is_wire(e)) {
    return false;
  }

  /* Don't start a loop from a boundary edge if it cannot be extended to cover any faces. */
  if (BM_edge_is_boundary(e)) {
    if (!bmw_FaceLoopWalker_include_face(walker, e->l)) {
      return false;
    }
  }

  /* Don't start a face loop from non-manifold edges. */
  if (!BM_edge_is_manifold(e)) {
    return false;
  }

  return true;
}

static void bmw_FaceLoopWalker_begin(BMWalker *walker, void *data)
{
  BMwFaceLoopWalker *lwalk, owalk, *owalk_pt;
  BMEdge *e = data;
  /* BMesh *bm = walker->bm; */             /* UNUSED */
  /* int fcount = BM_edge_face_count(e); */ /* UNUSED */

  if (!bmw_FaceLoopWalker_edge_begins_loop(walker, e)) {
    return;
  }

  lwalk = BMW_state_add(walker);
  lwalk->l = e->l;
  lwalk->no_calc = false;
  BLI_gset_insert(walker->visit_set, lwalk->l->f);

  /* Rewind. */
  while ((owalk_pt = BMW_current_state(walker))) {
    owalk = *((BMwFaceLoopWalker *)owalk_pt);
    BMW_walk(walker);
  }

  lwalk = BMW_state_add(walker);
  *lwalk = owalk;
  lwalk->no_calc = false;

  BLI_gset_clear(walker->visit_set_alt, NULL);
  BLI_gset_insert(walker->visit_set_alt, lwalk->l->e);

  BLI_gset_clear(walker->visit_set, NULL);
  BLI_gset_insert(walker->visit_set, lwalk->l->f);
}

static void *bmw_FaceLoopWalker_yield(BMWalker *walker)
{
  BMwFaceLoopWalker *lwalk = BMW_current_state(walker);

  if (!lwalk) {
    return NULL;
  }

  return lwalk->l->f;
}

static void *bmw_FaceLoopWalker_step(BMWalker *walker)
{
  BMwFaceLoopWalker *lwalk, owalk;
  BMFace *f;
  BMLoop *l;

  BMW_state_remove_r(walker, &owalk);
  lwalk = &owalk;

  f = lwalk->l->f;
  l = lwalk->l->radial_next;

  if (lwalk->no_calc) {
    return f;
  }

  if (!bmw_FaceLoopWalker_include_face(walker, l)) {
    l = lwalk->l;
    l = l->next->next;
    if (!BM_edge_is_manifold(l->e)) {
      l = l->prev->prev;
    }
    l = l->radial_next;
  }

  if (bmw_FaceLoopWalker_include_face(walker, l)) {
    lwalk = BMW_state_add(walker);
    lwalk->l = l;

    if (l->f->len != 4) {
      lwalk->no_calc = true;
      lwalk->l = owalk.l;
    }
    else {
      lwalk->no_calc = false;
    }

    /* Both may already exist. */
    BLI_gset_add(walker->visit_set_alt, l->e);
    BLI_gset_add(walker->visit_set, l->f);
  }

  return f;
}

/** \} */

// #define BMW_EDGERING_NGON

/* -------------------------------------------------------------------- */
/** \name Edge Ring Walker
 *
 * Starts at a tool-flagged edge and walks over the edge ring
 * Conditions for starting and stepping the edge ring have been
 * tuned to match behavior users expect (dating back to v2.4x).
 * \{ */
static void bmw_EdgeringWalker_begin(BMWalker *walker, void *data)
{
  BMwEdgeringWalker *lwalk, owalk, *owalk_pt;
  BMEdge *e = data;

  lwalk = BMW_state_add(walker);
  lwalk->l = e->l;

  if (!lwalk->l) {
    lwalk->wireedge = e;
    return;
  }
  lwalk->wireedge = NULL;

  BLI_gset_insert(walker->visit_set, lwalk->l->e);

  /* Rewind. */
  while ((owalk_pt = BMW_current_state(walker))) {
    owalk = *((BMwEdgeringWalker *)owalk_pt);
    BMW_walk(walker);
  }

  lwalk = BMW_state_add(walker);
  *lwalk = owalk;

#ifdef BMW_EDGERING_NGON
  if (lwalk->l->f->len % 2 != 0)
#else
  if (lwalk->l->f->len != 4)
#endif
  {
    lwalk->l = lwalk->l->radial_next;
  }

  BLI_gset_clear(walker->visit_set, NULL);
  BLI_gset_insert(walker->visit_set, lwalk->l->e);
}

static void *bmw_EdgeringWalker_yield(BMWalker *walker)
{
  BMwEdgeringWalker *lwalk = BMW_current_state(walker);

  if (!lwalk) {
    return NULL;
  }

  if (lwalk->l) {
    return lwalk->l->e;
  }
  return lwalk->wireedge;
}

static void *bmw_EdgeringWalker_step(BMWalker *walker)
{
  BMwEdgeringWalker *lwalk, owalk;
  BMEdge *e;
  BMLoop *l;
#ifdef BMW_EDGERING_NGON
  int i, len;
#endif

#define EDGE_CHECK(e) \
  (bmw_mask_check_edge(walker, e) && (BM_edge_is_boundary(e) || BM_edge_is_manifold(e)))

  BMW_state_remove_r(walker, &owalk);
  lwalk = &owalk;

  l = lwalk->l;
  if (!l) {
    return lwalk->wireedge;
  }

  e = l->e;
  if (!EDGE_CHECK(e)) {
    /* Walker won't traverse to a non-manifold edge, but may
     * be started on one, and should not traverse *away* from
     * a non-manifold edge (non-manifold edges are never in an
     * edge ring with manifold edges. */
    return e;
  }

#ifdef BMW_EDGERING_NGON
  l = l->radial_next;

  i = len = l->f->len;
  while (i > 0) {
    l = l->next;
    i -= 2;
  }

  if ((len <= 0) || (len % 2 != 0) || !EDGE_CHECK(l->e) || !bmw_mask_check_face(walker, l->f)) {
    l = owalk.l;
    i = len;
    while (i > 0) {
      l = l->next;
      i -= 2;
    }
  }
  /* Only walk to manifold edge. */
  if ((l->f->len % 2 == 0) && EDGE_CHECK(l->e) && !BLI_gset_haskey(walker->visit_set, l->e))
#else

  l = l->radial_next;
  l = l->next->next;

  if ((l->f->len != 4) || !EDGE_CHECK(l->e) || !bmw_mask_check_face(walker, l->f)) {
    l = owalk.l->next->next;
  }
  /* Only walk to manifold edge. */
  if ((l->f->len == 4) && EDGE_CHECK(l->e) && !BLI_gset_haskey(walker->visit_set, l->e))
#endif
  {
    lwalk = BMW_state_add(walker);
    lwalk->l = l;
    lwalk->wireedge = NULL;

    BLI_gset_insert(walker->visit_set, l->e);
  }

  return e;

#undef EDGE_CHECK
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Boundary Edge Walker
 * \{ */

static void bmw_EdgeboundaryWalker_begin(BMWalker *walker, void *data)
{
  BMwEdgeboundaryWalker *lwalk;
  BMEdge *e = data;

  BLI_assert(BM_edge_is_boundary(e));

  if (BLI_gset_haskey(walker->visit_set, e)) {
    return;
  }

  lwalk = BMW_state_add(walker);
  lwalk->e = e;
  BLI_gset_insert(walker->visit_set, e);
}

static void *bmw_EdgeboundaryWalker_yield(BMWalker *walker)
{
  BMwEdgeboundaryWalker *lwalk = BMW_current_state(walker);

  if (!lwalk) {
    return NULL;
  }

  return lwalk->e;
}

static void *bmw_EdgeboundaryWalker_step(BMWalker *walker)
{
  BMwEdgeboundaryWalker *lwalk, owalk;
  BMEdge *e, *e_other;
  BMVert *v;
  BMIter eiter;
  BMIter viter;

  BMW_state_remove_r(walker, &owalk);
  lwalk = &owalk;

  e = lwalk->e;

  if (!bmw_mask_check_edge(walker, e)) {
    return e;
  }

  BM_ITER_ELEM (v, &viter, e, BM_VERTS_OF_EDGE) {
    BM_ITER_ELEM (e_other, &eiter, v, BM_EDGES_OF_VERT) {
      if (e != e_other && BM_edge_is_boundary(e_other)) {
        if (BLI_gset_haskey(walker->visit_set, e_other)) {
          continue;
        }

        if (!bmw_mask_check_edge(walker, e_other)) {
          continue;
        }

        lwalk = BMW_state_add(walker);
        BLI_gset_insert(walker->visit_set, e_other);

        lwalk->e = e_other;
      }
    }
  }

  return e;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Edge Walker
 *
 * walk over uv islands; takes a loop as input.  restrict flag
 * restricts the walking to loops whose vert has restrict flag set as a
 * tool flag.
 *
 * The flag parameter to BMW_init maps to a loop customdata layer index.
 * \{ */

static void bmw_UVEdgeWalker_begin(BMWalker *walker, void *data)
{
  BMwUVEdgeWalker *lwalk;
  BMLoop *l = data;

  if (BLI_gset_haskey(walker->visit_set, l)) {
    return;
  }

  lwalk = BMW_state_add(walker);
  lwalk->l = l;
  BLI_gset_insert(walker->visit_set, l);
}

static void *bmw_UVEdgeWalker_yield(BMWalker *walker)
{
  BMwUVEdgeWalker *lwalk = BMW_current_state(walker);

  if (!lwalk) {
    return NULL;
  }

  return lwalk->l;
}

static void *bmw_UVEdgeWalker_step(BMWalker *walker)
{
  const int type = walker->bm->ldata.layers[walker->layer].type;
  const int offset = walker->bm->ldata.layers[walker->layer].offset;

  BMwUVEdgeWalker *lwalk, owalk;
  BMLoop *l;
  int i;

  BMW_state_remove_r(walker, &owalk);
  lwalk = &owalk;

  l = lwalk->l;

  if (!bmw_mask_check_edge(walker, l->e)) {
    return l;
  }

  /* Go over loops around `l->v` and `l->next->v` and see which ones share `l` and `l->next`
   * UV's coordinates. in addition, push on `l->next` if necessary. */
  for (i = 0; i < 2; i++) {
    BMIter liter;
    BMLoop *l_pivot, *l_radial;

    l_pivot = i ? l->next : l;
    BM_ITER_ELEM (l_radial, &liter, l_pivot->v, BM_LOOPS_OF_VERT) {
      BMLoop *l_radial_first = l_radial;
      void *data_pivot = BM_ELEM_CD_GET_VOID_P(l_pivot, offset);

      do {
        BMLoop *l_other;
        void *data_other;

        if (BLI_gset_haskey(walker->visit_set, l_radial)) {
          continue;
        }

        if (l_radial->v != l_pivot->v) {
          if (!bmw_mask_check_edge(walker, l_radial->e)) {
            continue;
          }
        }

        l_other = (l_radial->v != l_pivot->v) ? l_radial->next : l_radial;
        data_other = BM_ELEM_CD_GET_VOID_P(l_other, offset);

        if (!CustomData_data_equals(type, data_pivot, data_other)) {
          continue;
        }

        lwalk = BMW_state_add(walker);
        BLI_gset_insert(walker->visit_set, l_radial);

        lwalk->l = l_radial;

      } while ((l_radial = l_radial->radial_next) != l_radial_first);
    }
  }

  return l;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Non-manifold Edge Walker
 * \{ */

static void bmw_NonManifoldedgeWalker_begin(BMWalker *walker, void *data)
{
  BMwNonManifoldEdgeLoopWalker *lwalk;
  BMEdge *e = data;

  if (BLI_gset_haskey(walker->visit_set, e)) {
    return;
  }

  lwalk = BMW_state_add(walker);
  lwalk->start = e;
  lwalk->cur = e;
  lwalk->startv = e->v1;
  lwalk->lastv = e->v1;
  lwalk->face_count = BM_edge_face_count(e);
  BLI_gset_insert(walker->visit_set, e);
}

static void *bmw_NonManifoldedgeWalker_yield(BMWalker *walker)
{
  BMwNonManifoldEdgeLoopWalker *lwalk = BMW_current_state(walker);

  if (!lwalk) {
    return NULL;
  }
  return lwalk->cur;
}

/**
 * Walk over manifold loops around `v` until loop-edge is found with `face_count` users.
 * or return NULL if not found.
 * */
static BMLoop *bmw_NonManifoldLoop_find_next_around_vertex(BMLoop *l, BMVert *v, int face_count)
{
  BLI_assert(!BM_loop_is_manifold(l));
  do {
    l = BM_loop_other_edge_loop(l, v);
    if (BM_loop_is_manifold(l)) {
      l = l->radial_next;
    }
    else if (BM_edge_face_count_is_equal(l->e, face_count)) {
      return l;
    }
    else {
      break;
    }
  } while (true);
  return NULL;
}

static void *bmw_NonManifoldedgeWalker_step(BMWalker *walker)
{
  BMwNonManifoldEdgeLoopWalker *lwalk, owalk;
  BMW_state_remove_r(walker, &owalk);
  lwalk = &owalk;
  BMLoop *l_cur = NULL;
  const int face_count = lwalk->face_count;

  BMVert *v = NULL;

  /* Use the second pass is unlikely, only needed to walk back in the opposite direction. */
  for (int pass = 0; pass < 2; pass++) {

    BMEdge *e = lwalk->cur;
    v = BM_edge_other_vert(e, lwalk->lastv);

    /* If `lwalk.lastv` can't be walked along, start walking in the opposite direction
     * on the initial edge, do this at most one time during this walk operation.  */
    if (UNLIKELY(pass == 1)) {
      e = lwalk->start;
      v = lwalk->startv;
    }

    BMLoop *l = e->l;
    do {
      BMLoop *l_next = bmw_NonManifoldLoop_find_next_around_vertex(l, v, face_count);
      if ((l_next != NULL) && !BLI_gset_haskey(walker->visit_set, l_next->e)) {
        if (l_cur == NULL) {
          l_cur = l_next;
        }
        else if (l_cur->e != l_next->e) {
          /* If there are more than one possible edge to step onto (unlikely but possible),
           * treat as a junction and stop walking as there is no correct answer in this case. */
          l_cur = NULL;
          break;
        }
      }
    } while ((l = l->radial_next) != e->l);

    if (l_cur != NULL) {
      break;
    }
  }

  if (l_cur != NULL) {
    BLI_assert(!BLI_gset_haskey(walker->visit_set, l_cur->e));
    BLI_assert(BM_edge_face_count(l_cur->e) == face_count);
    lwalk = BMW_state_add(walker);
    lwalk->lastv = v;
    lwalk->cur = l_cur->e;
    lwalk->face_count = face_count;
    BLI_gset_insert(walker->visit_set, l_cur->e);
  }
  return owalk.cur;
}

/** \} */

static BMWalker bmw_VertShellWalker_Type = {
    BM_VERT | BM_EDGE,
    bmw_VertShellWalker_begin,
    bmw_VertShellWalker_step,
    bmw_VertShellWalker_yield,
    sizeof(BMwShellWalker),
    BMW_BREADTH_FIRST,
    BM_EDGE, /* Valid restrict masks. */
};

static BMWalker bmw_LoopShellWalker_Type = {
    BM_FACE | BM_LOOP | BM_EDGE | BM_VERT,
    bmw_LoopShellWalker_begin,
    bmw_LoopShellWalker_step,
    bmw_LoopShellWalker_yield,
    sizeof(BMwLoopShellWalker),
    BMW_BREADTH_FIRST,
    BM_EDGE, /* Valid restrict masks. */
};

static BMWalker bmw_LoopShellWireWalker_Type = {
    BM_FACE | BM_LOOP | BM_EDGE | BM_VERT,
    bmw_LoopShellWireWalker_begin,
    bmw_LoopShellWireWalker_step,
    bmw_LoopShellWireWalker_yield,
    sizeof(BMwLoopShellWireWalker),
    BMW_BREADTH_FIRST,
    BM_EDGE, /* Valid restrict masks. */
};

static BMWalker bmw_FaceShellWalker_Type = {
    BM_EDGE,
    bmw_FaceShellWalker_begin,
    bmw_FaceShellWalker_step,
    bmw_FaceShellWalker_yield,
    sizeof(BMwShellWalker),
    BMW_BREADTH_FIRST,
    BM_EDGE, /* Valid restrict masks. */
};

static BMWalker bmw_IslandboundWalker_Type = {
    BM_LOOP,
    bmw_IslandboundWalker_begin,
    bmw_IslandboundWalker_step,
    bmw_IslandboundWalker_yield,
    sizeof(BMwIslandboundWalker),
    BMW_DEPTH_FIRST,
    BM_FACE, /* Valid restrict masks. */
};

static BMWalker bmw_IslandWalker_Type = {
    BM_FACE,
    bmw_IslandWalker_begin,
    bmw_IslandWalker_step,
    bmw_IslandWalker_yield,
    sizeof(BMwIslandWalker),
    BMW_BREADTH_FIRST,
    BM_EDGE | BM_FACE, /* Valid restrict masks. */
};

static BMWalker bmw_IslandManifoldWalker_Type = {
    BM_FACE,
    bmw_IslandWalker_begin,
    bmw_IslandManifoldWalker_step, /* Only difference with #BMW_ISLAND. */
    bmw_IslandWalker_yield,
    sizeof(BMwIslandWalker),
    BMW_BREADTH_FIRST,
    BM_EDGE | BM_FACE, /* Valid restrict masks. */
};

static BMWalker bmw_EdgeLoopWalker_Type = {
    BM_EDGE,
    bmw_EdgeLoopWalker_begin,
    bmw_EdgeLoopWalker_step,
    bmw_EdgeLoopWalker_yield,
    sizeof(BMwEdgeLoopWalker),
    BMW_DEPTH_FIRST,
    0,
    /* Valid restrict masks. */ /* Could add flags here but so far none are used. */
};

static BMWalker bmw_FaceLoopWalker_Type = {
    BM_EDGE,
    bmw_FaceLoopWalker_begin,
    bmw_FaceLoopWalker_step,
    bmw_FaceLoopWalker_yield,
    sizeof(BMwFaceLoopWalker),
    BMW_DEPTH_FIRST,
    0,
    /* Valid restrict masks. */ /* Could add flags here but so far none are used. */
};

static BMWalker bmw_EdgeringWalker_Type = {
    BM_EDGE,
    bmw_EdgeringWalker_begin,
    bmw_EdgeringWalker_step,
    bmw_EdgeringWalker_yield,
    sizeof(BMwEdgeringWalker),
    BMW_DEPTH_FIRST,
    BM_EDGE, /* Valid restrict masks. */
};

static BMWalker bmw_EdgeboundaryWalker_Type = {
    BM_EDGE,
    bmw_EdgeboundaryWalker_begin,
    bmw_EdgeboundaryWalker_step,
    bmw_EdgeboundaryWalker_yield,
    sizeof(BMwEdgeboundaryWalker),
    BMW_DEPTH_FIRST,
    0,
};

static BMWalker bmw_NonManifoldedgeWalker_type = {
    BM_EDGE,
    bmw_NonManifoldedgeWalker_begin,
    bmw_NonManifoldedgeWalker_step,
    bmw_NonManifoldedgeWalker_yield,
    sizeof(BMwNonManifoldEdgeLoopWalker),
    BMW_DEPTH_FIRST,
    0,
};

static BMWalker bmw_UVEdgeWalker_Type = {
    BM_LOOP,
    bmw_UVEdgeWalker_begin,
    bmw_UVEdgeWalker_step,
    bmw_UVEdgeWalker_yield,
    sizeof(BMwUVEdgeWalker),
    BMW_DEPTH_FIRST,
    BM_EDGE, /* Valid restrict masks. */
};

static BMWalker bmw_ConnectedVertexWalker_Type = {
    BM_VERT,
    bmw_ConnectedVertexWalker_begin,
    bmw_ConnectedVertexWalker_step,
    bmw_ConnectedVertexWalker_yield,
    sizeof(BMwConnectedVertexWalker),
    BMW_BREADTH_FIRST,
    BM_VERT, /* Valid restrict masks. */
};

BMWalker *bm_walker_types[] = {
    &bmw_VertShellWalker_Type,       /* #BMW_VERT_SHELL */
    &bmw_LoopShellWalker_Type,       /* #BMW_LOOP_SHELL */
    &bmw_LoopShellWireWalker_Type,   /* #BMW_LOOP_SHELL_WIRE */
    &bmw_FaceShellWalker_Type,       /* #BMW_FACE_SHELL */
    &bmw_EdgeLoopWalker_Type,        /* #BMW_EDGELOOP */
    &bmw_FaceLoopWalker_Type,        /* #BMW_FACELOOP */
    &bmw_EdgeringWalker_Type,        /* #BMW_EDGERING */
    &bmw_EdgeboundaryWalker_Type,    /* #BMW_EDGEBOUNDARY */
    &bmw_NonManifoldedgeWalker_type, /* #BMW_EDGELOOP_NONMANIFOLD */
    &bmw_UVEdgeWalker_Type,          /* #BMW_LOOPDATA_ISLAND */
    &bmw_IslandboundWalker_Type,     /* #BMW_ISLANDBOUND */
    &bmw_IslandWalker_Type,          /* #BMW_ISLAND */
    &bmw_IslandManifoldWalker_Type,  /* #BMW_ISLAND_MANIFOLD */
    &bmw_ConnectedVertexWalker_Type, /* #BMW_CONNECTED_VERTEX */
};

const int bm_totwalkers = ARRAY_SIZE(bm_walker_types);
