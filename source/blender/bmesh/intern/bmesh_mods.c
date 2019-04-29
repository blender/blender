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
 * This file contains functions for locally modifying
 * the topology of existing mesh data. (split, join, flip etc).
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_array.h"

#include "BKE_customdata.h"

#include "bmesh.h"
#include "intern/bmesh_private.h"

/**
 * \brief Dissolve Vert
 *
 * Turns the face region surrounding a manifold vertex into a single polygon.
 *
 * \par Example:
 * <pre>
 *              +---------+             +---------+
 *              |  \   /  |             |         |
 *     Before:  |    v    |      After: |         |
 *              |  /   \  |             |         |
 *              +---------+             +---------+
 * </pre>
 *
 * This function can also collapse edges too
 * in cases when it cant merge into faces.
 *
 * \par Example:
 * <pre>
 *     Before:  +----v----+      After: +---------+
 * </pre>
 *
 * \note dissolves vert, in more situations then BM_disk_dissolve
 * (e.g. if the vert is part of a wire edge, etc).
 */
bool BM_vert_dissolve(BMesh *bm, BMVert *v)
{
  /* logic for 3 or more is identical */
  const int len = BM_vert_edge_count_at_most(v, 3);

  if (len == 1) {
    BM_vert_kill(bm, v); /* will kill edges too */
    return true;
  }
  else if (!BM_vert_is_manifold(v)) {
    if (!v->e) {
      BM_vert_kill(bm, v);
      return true;
    }
    else if (!v->e->l) {
      if (len == 2) {
        return (BM_vert_collapse_edge(bm, v->e, v, true, true) != NULL);
      }
      else {
        /* used to kill the vertex here, but it may be connected to faces.
         * so better do nothing */
        return false;
      }
    }
    else {
      return false;
    }
  }
  else if (len == 2 && BM_vert_face_count_is_equal(v, 1)) {
    /* boundary vertex on a face */
    return (BM_vert_collapse_edge(bm, v->e, v, true, true) != NULL);
  }
  else {
    return BM_disk_dissolve(bm, v);
  }
}

/**
 * dissolves all faces around a vert, and removes it.
 */
bool BM_disk_dissolve(BMesh *bm, BMVert *v)
{
  BMEdge *e, *keepedge = NULL, *baseedge = NULL;
  int len = 0;

  if (!BM_vert_is_manifold(v)) {
    return false;
  }

  if (v->e) {
    /* v->e we keep, what else */
    e = v->e;
    do {
      e = bmesh_disk_edge_next(e, v);
      if (!(BM_edge_share_face_check(e, v->e))) {
        keepedge = e;
        baseedge = v->e;
        break;
      }
      len++;
    } while (e != v->e);
  }

  /* this code for handling 2 and 3-valence verts
   * may be totally bad */
  if (keepedge == NULL && len == 3) {
#if 0
    /* handle specific case for three-valence.  solve it by
     * increasing valence to four.  this may be hackish. .  */
    BMLoop *l_a = BM_face_vert_share_loop(e->l->f, v);
    BMLoop *l_b = (e->l->v == v) ? e->l->next : e->l;

    if (!BM_face_split(bm, e->l->f, l_a, l_b, NULL, NULL, false))
      return false;

    if (!BM_disk_dissolve(bm, v)) {
      return false;
    }
#else
    if (UNLIKELY(!BM_faces_join_pair(bm, e->l, e->l->radial_next, true))) {
      return false;
    }
    else if (UNLIKELY(!BM_vert_collapse_faces(bm, v->e, v, 1.0, true, false, true))) {
      return false;
    }
#endif
    return true;
  }
  else if (keepedge == NULL && len == 2) {
    /* collapse the vertex */
    e = BM_vert_collapse_faces(bm, v->e, v, 1.0, true, true, true);

    if (!e) {
      return false;
    }

    /* handle two-valence */
    if (e->l != e->l->radial_next) {
      if (!BM_faces_join_pair(bm, e->l, e->l->radial_next, true)) {
        return false;
      }
    }

    return true;
  }

  if (keepedge) {
    bool done = false;

    while (!done) {
      done = true;
      e = v->e;
      do {
        BMFace *f = NULL;
        if (BM_edge_is_manifold(e) && (e != baseedge) && (e != keepedge)) {
          f = BM_faces_join_pair(bm, e->l, e->l->radial_next, true);
          /* return if couldn't join faces in manifold
           * conditions */
          /* !disabled for testing why bad things happen */
          if (!f) {
            return false;
          }
        }

        if (f) {
          done = false;
          break;
        }
      } while ((e = bmesh_disk_edge_next(e, v)) != v->e);
    }

    /* collapse the vertex */
    /* note, the baseedge can be a boundary of manifold, use this as join_faces arg */
    e = BM_vert_collapse_faces(bm, baseedge, v, 1.0, true, !BM_edge_is_boundary(baseedge), true);

    if (!e) {
      return false;
    }

    if (e->l) {
      /* get remaining two faces */
      if (e->l != e->l->radial_next) {
        /* join two remaining faces */
        if (!BM_faces_join_pair(bm, e->l, e->l->radial_next, true)) {
          return false;
        }
      }
    }
  }

  return true;
}

/**
 * \brief Faces Join Pair
 *
 * Joins two adjacent faces together.
 *
 * \note This method calls to #BM_faces_join to do its work.
 * This means connected edges which also share the two faces will be joined.
 *
 * If the windings do not match the winding of the new face will follow
 * \a l_a's winding (i.e. \a l_b will be reversed before the join).
 *
 * \return The combined face or NULL on failure.
 */
BMFace *BM_faces_join_pair(BMesh *bm, BMLoop *l_a, BMLoop *l_b, const bool do_del)
{
  BLI_assert((l_a != l_b) && (l_a->e == l_b->e));

  if (l_a->v == l_b->v) {
    const int cd_loop_mdisp_offset = CustomData_get_offset(&bm->ldata, CD_MDISPS);
    bmesh_kernel_loop_reverse(bm, l_b->f, cd_loop_mdisp_offset, true);
  }

  BMFace *faces[2] = {l_a->f, l_b->f};
  return BM_faces_join(bm, faces, 2, do_del);
}

/**
 * \brief Face Split
 *
 * Split a face along two vertices. returns the newly made face, and sets
 * the \a r_l member to a loop in the newly created edge.
 *
 * \param bm: The bmesh
 * \param f: the original face
 * \param l_a, l_b: Loops of this face, their vertices define
 * the split edge to be created (must be differ and not can't be adjacent in the face).
 * \param r_l: pointer which will receive the BMLoop for the split edge in the new face
 * \param example: Edge used for attributes of splitting edge, if non-NULL
 * \param no_double: Use an existing edge if found
 *
 * \return Pointer to the newly created face representing one side of the split
 * if the split is successful (and the original original face will be the
 * other side). NULL if the split fails.
 */
BMFace *BM_face_split(BMesh *bm,
                      BMFace *f,
                      BMLoop *l_a,
                      BMLoop *l_b,
                      BMLoop **r_l,
                      BMEdge *example,
                      const bool no_double)
{
  const int cd_loop_mdisp_offset = CustomData_get_offset(&bm->ldata, CD_MDISPS);
  BMFace *f_new, *f_tmp;

  BLI_assert(l_a != l_b);
  BLI_assert(f == l_a->f && f == l_b->f);
  BLI_assert(!BM_loop_is_adjacent(l_a, l_b));

  /* could be an assert */
  if (UNLIKELY(BM_loop_is_adjacent(l_a, l_b)) || UNLIKELY((f != l_a->f || f != l_b->f))) {
    if (r_l) {
      *r_l = NULL;
    }
    return NULL;
  }

  /* do we have a multires layer? */
  if (cd_loop_mdisp_offset != -1) {
    f_tmp = BM_face_copy(bm, bm, f, false, false);
  }

#ifdef USE_BMESH_HOLES
  f_new = bmesh_kernel_split_face_make_edge(bm, f, l_a, l_b, r_l, NULL, example, no_double);
#else
  f_new = bmesh_kernel_split_face_make_edge(bm, f, l_a, l_b, r_l, example, no_double);
#endif

  if (f_new) {
    /* handle multires update */
    if (cd_loop_mdisp_offset != -1) {
      float f_dst_center[3];
      float f_src_center[3];

      BM_face_calc_center_median(f_tmp, f_src_center);

      BM_face_calc_center_median(f, f_dst_center);
      BM_face_interp_multires_ex(bm, f, f_tmp, f_dst_center, f_src_center, cd_loop_mdisp_offset);

      BM_face_calc_center_median(f_new, f_dst_center);
      BM_face_interp_multires_ex(
          bm, f_new, f_tmp, f_dst_center, f_src_center, cd_loop_mdisp_offset);

#if 0
      /* BM_face_multires_bounds_smooth doesn't flip displacement correct */
      BM_face_multires_bounds_smooth(bm, f);
      BM_face_multires_bounds_smooth(bm, f_new);
#endif
    }
  }

  if (cd_loop_mdisp_offset != -1) {
    BM_face_kill(bm, f_tmp);
  }

  return f_new;
}

/**
 * \brief Face Split with intermediate points
 *
 * Like BM_face_split, but with an edge split by \a n intermediate points with given coordinates.
 *
 * \param bm: The bmesh.
 * \param f: the original face.
 * \param l_a, l_b: Vertices which define the split edge, must be different.
 * \param cos: Array of coordinates for intermediate points.
 * \param n: Length of \a cos (must be > 0).
 * \param r_l: pointer which will receive the BMLoop.
 * for the first split edge (from \a l_a) in the new face.
 * \param example: Edge used for attributes of splitting edge, if non-NULL.
 *
 * \return Pointer to the newly created face representing one side of the split
 * if the split is successful (and the original original face will be the
 * other side). NULL if the split fails.
 */
BMFace *BM_face_split_n(BMesh *bm,
                        BMFace *f,
                        BMLoop *l_a,
                        BMLoop *l_b,
                        float cos[][3],
                        int n,
                        BMLoop **r_l,
                        BMEdge *example)
{
  BMFace *f_new, *f_tmp;
  BMLoop *l_new;
  BMEdge *e, *e_new;
  BMVert *v_new;
  // BMVert *v_a = l_a->v; /* UNUSED */
  BMVert *v_b = l_b->v;
  int i, j;

  BLI_assert(l_a != l_b);
  BLI_assert(f == l_a->f && f == l_b->f);
  BLI_assert(!((n == 0) && BM_loop_is_adjacent(l_a, l_b)));

  /* could be an assert */
  if (UNLIKELY((n == 0) && BM_loop_is_adjacent(l_a, l_b)) || UNLIKELY(l_a->f != l_b->f)) {
    if (r_l) {
      *r_l = NULL;
    }
    return NULL;
  }

  f_tmp = BM_face_copy(bm, bm, f, true, true);

#ifdef USE_BMESH_HOLES
  f_new = bmesh_kernel_split_face_make_edge(bm, f, l_a, l_b, &l_new, NULL, example, false);
#else
  f_new = bmesh_kernel_split_face_make_edge(bm, f, l_a, l_b, &l_new, example, false);
#endif
  /* bmesh_kernel_split_face_make_edge returns in 'l_new'
   * a Loop for f_new going from 'v_a' to 'v_b'.
   * The radial_next is for 'f' and goes from 'v_b' to 'v_a'  */

  if (f_new) {
    e = l_new->e;
    for (i = 0; i < n; i++) {
      v_new = bmesh_kernel_split_edge_make_vert(bm, v_b, e, &e_new);
      BLI_assert(v_new != NULL);
      /* bmesh_kernel_split_edge_make_vert returns in 'e_new'
       * the edge going from 'v_new' to 'v_b'. */
      copy_v3_v3(v_new->co, cos[i]);

      /* interpolate the loop data for the loops with (v == v_new), using orig face */
      for (j = 0; j < 2; j++) {
        BMEdge *e_iter = (j == 0) ? e : e_new;
        BMLoop *l_iter = e_iter->l;
        do {
          if (l_iter->v == v_new) {
            /* this interpolates both loop and vertex data */
            BM_loop_interp_from_face(bm, l_iter, f_tmp, true, true);
          }
        } while ((l_iter = l_iter->radial_next) != e_iter->l);
      }
      e = e_new;
    }
  }

  BM_face_verts_kill(bm, f_tmp);

  if (r_l) {
    *r_l = l_new;
  }

  return f_new;
}

/**
 * \brief Vert Collapse Faces
 *
 * Collapses vertex \a v_kill that has only two manifold edges
 * onto a vertex it shares an edge with.
 * \a fac defines the amount of interpolation for Custom Data.
 *
 * \note that this is not a general edge collapse function.
 *
 * \note this function is very close to #BM_vert_collapse_edge,
 * both collapse a vertex and return a new edge.
 * Except this takes a factor and merges custom data.
 *
 * \param bm: The bmesh
 * \param e_kill: The edge to collapse
 * \param v_kill: The vertex  to collapse into the edge
 * \param fac: The factor along the edge
 * \param join_faces: When true the faces around the vertex will be joined
 * otherwise collapse the vertex by merging the 2 edges this vert touches into one.
 * \param kill_degenerate_faces: Removes faces with less than 3 verts after collapsing.
 *
 * \returns The New Edge
 */
BMEdge *BM_vert_collapse_faces(BMesh *bm,
                               BMEdge *e_kill,
                               BMVert *v_kill,
                               float fac,
                               const bool do_del,
                               const bool join_faces,
                               const bool kill_degenerate_faces)
{
  BMEdge *e_new = NULL;
  BMVert *tv = BM_edge_other_vert(e_kill, v_kill);

  BMEdge *e2;
  BMVert *tv2;

  /* Only intended to be called for 2-valence vertices */
  BLI_assert(bmesh_disk_count(v_kill) <= 2);

  /* first modify the face loop data */

  if (e_kill->l) {
    BMLoop *l_iter;
    const float w[2] = {1.0f - fac, fac};

    l_iter = e_kill->l;
    do {
      if (l_iter->v == tv && l_iter->next->v == v_kill) {
        const void *src[2];
        BMLoop *tvloop = l_iter;
        BMLoop *kvloop = l_iter->next;

        src[0] = kvloop->head.data;
        src[1] = tvloop->head.data;
        CustomData_bmesh_interp(&bm->ldata, src, w, NULL, 2, kvloop->head.data);
      }
    } while ((l_iter = l_iter->radial_next) != e_kill->l);
  }

  /* now interpolate the vertex data */
  BM_data_interp_from_verts(bm, v_kill, tv, v_kill, fac);

  e2 = bmesh_disk_edge_next(e_kill, v_kill);
  tv2 = BM_edge_other_vert(e2, v_kill);

  if (join_faces) {
    BMIter fiter;
    BMFace **faces = NULL;
    BMFace *f;
    BLI_array_staticdeclare(faces, BM_DEFAULT_ITER_STACK_SIZE);

    BM_ITER_ELEM (f, &fiter, v_kill, BM_FACES_OF_VERT) {
      BLI_array_append(faces, f);
    }

    if (BLI_array_len(faces) >= 2) {
      BMFace *f2 = BM_faces_join(bm, faces, BLI_array_len(faces), true);
      if (f2) {
        BMLoop *l_a, *l_b;

        if ((l_a = BM_face_vert_share_loop(f2, tv)) && (l_b = BM_face_vert_share_loop(f2, tv2))) {
          BMLoop *l_new;

          if (BM_face_split(bm, f2, l_a, l_b, &l_new, NULL, false)) {
            e_new = l_new->e;
          }
        }
      }
    }

    BLI_assert(BLI_array_len(faces) < 8);

    BLI_array_free(faces);
  }
  else {
    /* single face or no faces */
    /* same as BM_vert_collapse_edge() however we already
     * have vars to perform this operation so don't call. */
    e_new = bmesh_kernel_join_edge_kill_vert(
        bm, e_kill, v_kill, do_del, true, kill_degenerate_faces);
    /* e_new = BM_edge_exists(tv, tv2); */ /* same as return above */
  }

  return e_new;
}

/**
 * \brief Vert Collapse Faces
 *
 * Collapses a vertex onto another vertex it shares an edge with.
 *
 * \return The New Edge
 */
BMEdge *BM_vert_collapse_edge(
    BMesh *bm, BMEdge *e_kill, BMVert *v_kill, const bool do_del, const bool kill_degenerate_faces)
{
  /* nice example implementation but we want loops to have their customdata
   * accounted for */
#if 0
  BMEdge *e_new = NULL;

  /* Collapse between 2 edges */

  /* in this case we want to keep all faces and not join them,
   * rather just get rid of the vertex - see bug [#28645] */
  BMVert *tv = BM_edge_other_vert(e_kill, v_kill);
  if (tv) {
    BMEdge *e2 = bmesh_disk_edge_next(e_kill, v_kill);
    if (e2) {
      BMVert *tv2 = BM_edge_other_vert(e2, v_kill);
      if (tv2) {
        /* only action, other calls here only get the edge to return */
        e_new = bmesh_kernel_join_edge_kill_vert(
            bm, e_kill, v_kill, do_del, true, kill_degenerate_faces);
      }
    }
  }

  return e_new;
#else
  /* with these args faces are never joined, same as above
   * but account for loop customdata */
  return BM_vert_collapse_faces(bm, e_kill, v_kill, 1.0f, do_del, false, kill_degenerate_faces);
#endif
}

#undef DO_V_INTERP

/**
 * Collapse and edge into a single vertex.
 */
BMVert *BM_edge_collapse(
    BMesh *bm, BMEdge *e_kill, BMVert *v_kill, const bool do_del, const bool kill_degenerate_faces)
{
  return bmesh_kernel_join_vert_kill_edge(bm, e_kill, v_kill, do_del, true, kill_degenerate_faces);
}

/**
 * \brief Edge Split
 *
 * <pre>
 * Before: v
 *         +-----------------------------------+
 *                           e
 *
 * After:  v                 v_new (returned)
 *         +-----------------+-----------------+
 *                 r_e                e
 * </pre>
 *
 * \param e: The edge to split.
 * \param v: One of the vertices in \a e and defines the "from" end of the splitting operation,
 * the new vertex will be \a fac of the way from \a v to the other end.
 * \param r_e: The newly created edge.
 * \return  The new vertex.
 */
BMVert *BM_edge_split(BMesh *bm, BMEdge *e, BMVert *v, BMEdge **r_e, float fac)
{
  BMVert *v_new, *v_other;
  BMEdge *e_new;
  BMFace **oldfaces = NULL;
  BLI_array_staticdeclare(oldfaces, 32);
  const int cd_loop_mdisp_offset = BM_edge_is_wire(e) ?
                                       -1 :
                                       CustomData_get_offset(&bm->ldata, CD_MDISPS);

  BLI_assert(BM_vert_in_edge(e, v) == true);

  /* do we have a multi-res layer? */
  if (cd_loop_mdisp_offset != -1) {
    BMLoop *l;
    int i;

    l = e->l;
    do {
      BLI_array_append(oldfaces, l->f);
      l = l->radial_next;
    } while (l != e->l);

    /* flag existing faces so we can differentiate oldfaces from new faces */
    for (i = 0; i < BLI_array_len(oldfaces); i++) {
      BM_ELEM_API_FLAG_ENABLE(oldfaces[i], _FLAG_OVERLAP);
      oldfaces[i] = BM_face_copy(bm, bm, oldfaces[i], true, true);
      BM_ELEM_API_FLAG_DISABLE(oldfaces[i], _FLAG_OVERLAP);
    }
  }

  v_other = BM_edge_other_vert(e, v);
  v_new = bmesh_kernel_split_edge_make_vert(bm, v, e, &e_new);
  if (r_e != NULL) {
    *r_e = e_new;
  }

  BLI_assert(v_new != NULL);
  BLI_assert(BM_vert_in_edge(e_new, v) && BM_vert_in_edge(e_new, v_new));
  BLI_assert(BM_vert_in_edge(e, v_new) && BM_vert_in_edge(e, v_other));

  sub_v3_v3v3(v_new->co, v_other->co, v->co);
  madd_v3_v3v3fl(v_new->co, v->co, v_new->co, fac);

  e_new->head.hflag = e->head.hflag;
  BM_elem_attrs_copy(bm, bm, e, e_new);

  /* v->v_new->v2 */
  BM_data_interp_face_vert_edge(bm, v_other, v, v_new, e, fac);
  BM_data_interp_from_verts(bm, v, v_other, v_new, fac);

  if (cd_loop_mdisp_offset != -1) {
    int i, j;

    /* interpolate new/changed loop data from copied old faces */
    for (i = 0; i < BLI_array_len(oldfaces); i++) {
      float f_center_old[3];

      BM_face_calc_center_median(oldfaces[i], f_center_old);

      for (j = 0; j < 2; j++) {
        BMEdge *e1 = j ? e_new : e;
        BMLoop *l;

        l = e1->l;

        if (UNLIKELY(!l)) {
          BMESH_ASSERT(0);
          break;
        }

        do {
          /* check this is an old face */
          if (BM_ELEM_API_FLAG_TEST(l->f, _FLAG_OVERLAP)) {
            float f_center[3];

            BM_face_calc_center_median(l->f, f_center);
            BM_face_interp_multires_ex(
                bm, l->f, oldfaces[i], f_center, f_center_old, cd_loop_mdisp_offset);
          }
          l = l->radial_next;
        } while (l != e1->l);
      }
    }

    /* destroy the old faces */
    for (i = 0; i < BLI_array_len(oldfaces); i++) {
      BM_face_verts_kill(bm, oldfaces[i]);
    }

    /* fix boundaries a bit, doesn't work too well quite yet */
#if 0
    for (j = 0; j < 2; j++) {
      BMEdge *e1 = j ? e_new : e;
      BMLoop *l, *l2;

      l = e1->l;
      if (UNLIKELY(!l)) {
        BMESH_ASSERT(0);
        break;
      }

      do {
        BM_face_multires_bounds_smooth(bm, l->f);
        l = l->radial_next;
      } while (l != e1->l);
    }
#endif

    BLI_array_free(oldfaces);
  }

  return v_new;
}

/**
 * \brief Split an edge multiple times evenly
 *
 * \param r_varr: Optional array, verts in between (v1 -> v2)
 */
BMVert *BM_edge_split_n(BMesh *bm, BMEdge *e, int numcuts, BMVert **r_varr)
{
  int i;
  float percent;
  BMVert *v_new = NULL;

  for (i = 0; i < numcuts; i++) {
    percent = 1.0f / (float)(numcuts + 1 - i);
    v_new = BM_edge_split(bm, e, e->v2, NULL, percent);
    if (r_varr) {
      /* fill in reverse order (v1 -> v2) */
      r_varr[numcuts - i - 1] = v_new;
    }
  }
  return v_new;
}

/**
 * Swap v1 & v2
 *
 * \note Typically we shouldn't care about this, however it's used when extruding wire edges.
 */
void BM_edge_verts_swap(BMEdge *e)
{
  SWAP(BMVert *, e->v1, e->v2);
  SWAP(BMDiskLink, e->v1_disk_link, e->v2_disk_link);
}

#if 0
/**
 * Checks if a face is valid in the data structure
 */
bool BM_face_validate(BMFace *face, FILE *err)
{
  BMIter iter;
  BLI_array_declare(verts);
  BMVert **verts = NULL;
  BMLoop *l;
  int i, j;
  bool ret = true;

  if (face->len == 2) {
    fprintf(err, "warning: found two-edged face. face ptr: %p\n", face);
    fflush(err);
  }

  BLI_array_grow_items(verts, face->len);
  BM_ITER_ELEM_INDEX (l, &iter, face, BM_LOOPS_OF_FACE, i) {
    verts[i] = l->v;
    if (l->e->v1 == l->e->v2) {
      fprintf(err, "Found bmesh edge with identical verts!\n");
      fprintf(err, "  edge ptr: %p, vert: %p\n", l->e, l->e->v1);
      fflush(err);
      ret = false;
    }
  }

  for (i = 0; i < face->len; i++) {
    for (j = 0; j < face->len; j++) {
      if (j == i) {
        continue;
      }

      if (verts[i] == verts[j]) {
        fprintf(err, "Found duplicate verts in bmesh face!\n");
        fprintf(err, "  face ptr: %p, vert: %p\n", face, verts[i]);
        fflush(err);
        ret = false;
      }
    }
  }

  BLI_array_free(verts);
  return ret;
}
#endif

/**
 * Calculate the 2 loops which _would_ make up the newly rotated Edge
 * but don't actually change anything.
 *
 * Use this to further inspect if the loops to be connected have issues:
 *
 * Examples:
 * - the newly formed edge already exists
 * - the new face would be degenerate (zero area / concave /  bow-tie)
 * - may want to measure if the new edge gives improved results topology.
 *   over the old one, as with beauty fill.
 *
 * \note #BM_edge_rotate_check must have already run.
 */
void BM_edge_calc_rotate(BMEdge *e, const bool ccw, BMLoop **r_l1, BMLoop **r_l2)
{
  BMVert *v1, *v2;
  BMFace *fa, *fb;

  /* this should have already run */
  BLI_assert(BM_edge_rotate_check(e) == true);

  /* we know this will work */
  BM_edge_face_pair(e, &fa, &fb);

  /* so we can use ccw variable correctly,
   * otherwise we could use the edges verts direct */
  BM_edge_ordered_verts(e, &v1, &v2);

  /* we could swap the verts _or_ the faces, swapping faces
   * gives more predictable results since that way the next vert
   * just stitches from face fa / fb */
  if (!ccw) {
    SWAP(BMFace *, fa, fb);
  }

  *r_l1 = BM_face_other_vert_loop(fb, v2, v1);
  *r_l2 = BM_face_other_vert_loop(fa, v1, v2);
}

/**
 * \brief Check if Rotate Edge is OK
 *
 * Quick check to see if we could rotate the edge,
 * use this to avoid calling exceptions on common cases.
 */
bool BM_edge_rotate_check(BMEdge *e)
{
  BMFace *fa, *fb;
  if (BM_edge_face_pair(e, &fa, &fb)) {
    BMLoop *la, *lb;

    la = BM_face_other_vert_loop(fa, e->v2, e->v1);
    lb = BM_face_other_vert_loop(fb, e->v2, e->v1);

    /* check that the next vert in both faces isn't the same
     * (ie - the next edge doesn't share the same faces).
     * since we can't rotate usefully in this case. */
    if (la->v == lb->v) {
      return false;
    }

    /* mirror of the check above but in the opposite direction */
    la = BM_face_other_vert_loop(fa, e->v1, e->v2);
    lb = BM_face_other_vert_loop(fb, e->v1, e->v2);

    if (la->v == lb->v) {
      return false;
    }

    return true;
  }
  else {
    return false;
  }
}

/**
 * \brief Check if Edge Rotate Gives Degenerate Faces
 *
 * Check 2 cases
 * 1) does the newly forms edge form a flipped face (compare with previous cross product)
 * 2) does the newly formed edge cause a zero area corner (or close enough to be almost zero)
 *
 * \param e: The edge to test rotation.
 * \param l1, l2: are the loops of the proposed verts to rotate too and should
 * be the result of calling #BM_edge_calc_rotate
 */
bool BM_edge_rotate_check_degenerate(BMEdge *e, BMLoop *l1, BMLoop *l2)
{
  /* note: for these vars 'old' just means initial edge state. */

  float ed_dir_old[3];      /* edge vector */
  float ed_dir_new[3];      /* edge vector */
  float ed_dir_new_flip[3]; /* edge vector */

  float ed_dir_v1_old[3];
  float ed_dir_v2_old[3];

  float ed_dir_v1_new[3];
  float ed_dir_v2_new[3];

  float cross_old[3];
  float cross_new[3];

  /* original verts - these will be in the edge 'e' */
  BMVert *v1_old, *v2_old;

  /* verts from the loops passed */

  BMVert *v1, *v2;
  /* these are the opposite verts - the verts that _would_ be used if 'ccw' was inverted*/
  BMVert *v1_alt, *v2_alt;

  /* this should have already run */
  BLI_assert(BM_edge_rotate_check(e) == true);

  BM_edge_ordered_verts(e, &v1_old, &v2_old);

  v1 = l1->v;
  v2 = l2->v;

  /* get the next vert along */
  v1_alt = BM_face_other_vert_loop(l1->f, v1_old, v1)->v;
  v2_alt = BM_face_other_vert_loop(l2->f, v2_old, v2)->v;

  /* normalize all so comparisons are scale independent */

  BLI_assert(BM_edge_exists(v1_old, v1));
  BLI_assert(BM_edge_exists(v1, v1_alt));

  BLI_assert(BM_edge_exists(v2_old, v2));
  BLI_assert(BM_edge_exists(v2, v2_alt));

  /* old and new edge vecs */
  sub_v3_v3v3(ed_dir_old, v1_old->co, v2_old->co);
  sub_v3_v3v3(ed_dir_new, v1->co, v2->co);
  normalize_v3(ed_dir_old);
  normalize_v3(ed_dir_new);

  /* old edge corner vecs */
  sub_v3_v3v3(ed_dir_v1_old, v1_old->co, v1->co);
  sub_v3_v3v3(ed_dir_v2_old, v2_old->co, v2->co);
  normalize_v3(ed_dir_v1_old);
  normalize_v3(ed_dir_v2_old);

  /* old edge corner vecs */
  sub_v3_v3v3(ed_dir_v1_new, v1->co, v1_alt->co);
  sub_v3_v3v3(ed_dir_v2_new, v2->co, v2_alt->co);
  normalize_v3(ed_dir_v1_new);
  normalize_v3(ed_dir_v2_new);

  /* compare */
  cross_v3_v3v3(cross_old, ed_dir_old, ed_dir_v1_old);
  cross_v3_v3v3(cross_new, ed_dir_new, ed_dir_v1_new);
  if (dot_v3v3(cross_old, cross_new) < 0.0f) { /* does this flip? */
    return false;
  }
  cross_v3_v3v3(cross_old, ed_dir_old, ed_dir_v2_old);
  cross_v3_v3v3(cross_new, ed_dir_new, ed_dir_v2_new);
  if (dot_v3v3(cross_old, cross_new) < 0.0f) { /* does this flip? */
    return false;
  }

  negate_v3_v3(ed_dir_new_flip, ed_dir_new);

  /* result is zero area corner */
  if ((dot_v3v3(ed_dir_new, ed_dir_v1_new) > 0.999f) ||
      (dot_v3v3(ed_dir_new_flip, ed_dir_v2_new) > 0.999f)) {
    return false;
  }

  return true;
}

bool BM_edge_rotate_check_beauty(BMEdge *e, BMLoop *l1, BMLoop *l2)
{
  /* Stupid check for now:
   * Could compare angles of surrounding edges
   * before & after, but this is OK.*/
  return (len_squared_v3v3(e->v1->co, e->v2->co) > len_squared_v3v3(l1->v->co, l2->v->co));
}

/**
 * \brief Rotate Edge
 *
 * Spins an edge topologically,
 * either counter-clockwise or clockwise depending on \a ccw.
 *
 * \return The spun edge, NULL on error
 * (e.g., if the edge isn't surrounded by exactly two faces).
 *
 * \note This works by dissolving the edge then re-creating it,
 * so the returned edge won't have the same pointer address as the original one.
 *
 * \see header definition for \a check_flag enum.
 */
BMEdge *BM_edge_rotate(BMesh *bm, BMEdge *e, const bool ccw, const short check_flag)
{
  BMVert *v1, *v2;
  BMLoop *l1, *l2;
  BMFace *f;
  BMEdge *e_new = NULL;
  char f_active_prev = 0;
  char f_hflag_prev_1;
  char f_hflag_prev_2;

  if (!BM_edge_rotate_check(e)) {
    return NULL;
  }

  BM_edge_calc_rotate(e, ccw, &l1, &l2);

  /* the loops will be freed so assign verts */
  v1 = l1->v;
  v2 = l2->v;

  /* --------------------------------------- */
  /* Checking Code - make sure we can rotate */

  if (check_flag & BM_EDGEROT_CHECK_BEAUTY) {
    if (!BM_edge_rotate_check_beauty(e, l1, l2)) {
      return NULL;
    }
  }

  /* check before applying */
  if (check_flag & BM_EDGEROT_CHECK_EXISTS) {
    if (BM_edge_exists(v1, v2)) {
      return NULL;
    }
  }

  /* slowest, check last */
  if (check_flag & BM_EDGEROT_CHECK_DEGENERATE) {
    if (!BM_edge_rotate_check_degenerate(e, l1, l2)) {
      return NULL;
    }
  }
  /* Done Checking */
  /* ------------- */

  /* --------------- */
  /* Rotate The Edge */

  /* first create the new edge, this is so we can copy the customdata from the old one
   * if splice if disabled, always add in a new edge even if theres one there. */
  e_new = BM_edge_create(
      bm, v1, v2, e, (check_flag & BM_EDGEROT_CHECK_SPLICE) ? BM_CREATE_NO_DOUBLE : BM_CREATE_NOP);

  f_hflag_prev_1 = l1->f->head.hflag;
  f_hflag_prev_2 = l2->f->head.hflag;

  /* maintain active face */
  if (bm->act_face == l1->f) {
    f_active_prev = 1;
  }
  else if (bm->act_face == l2->f) {
    f_active_prev = 2;
  }

  const bool is_flipped = !BM_edge_is_contiguous(e);

  /* don't delete the edge, manually remove the edge after so we can copy its attributes */
  f = BM_faces_join_pair(
      bm, BM_face_edge_share_loop(l1->f, e), BM_face_edge_share_loop(l2->f, e), true);

  if (f == NULL) {
    return NULL;
  }

  /* note, this assumes joining the faces _didnt_ also remove the verts.
   * the #BM_edge_rotate_check will ensure this, but its possibly corrupt state or future edits
   * break this */
  if ((l1 = BM_face_vert_share_loop(f, v1)) && (l2 = BM_face_vert_share_loop(f, v2)) &&
      BM_face_split(bm, f, l1, l2, NULL, NULL, true)) {
    /* we should really be able to know the faces some other way,
     * rather then fetching them back from the edge, but this is predictable
     * where using the return values from face split isn't. - campbell */
    BMFace *fa, *fb;
    if (BM_edge_face_pair(e_new, &fa, &fb)) {
      fa->head.hflag = f_hflag_prev_1;
      fb->head.hflag = f_hflag_prev_2;

      if (f_active_prev == 1) {
        bm->act_face = fa;
      }
      else if (f_active_prev == 2) {
        bm->act_face = fb;
      }

      if (is_flipped) {
        BM_face_normal_flip(bm, fb);

        if (ccw) {
          /* needed otherwise ccw toggles direction */
          e_new->l = e_new->l->radial_next;
        }
      }
    }
  }
  else {
    return NULL;
  }

  return e_new;
}

/**
 * \brief Rip a single face from a vertex fan
 */
BMVert *BM_face_loop_separate(BMesh *bm, BMLoop *l_sep)
{
  return bmesh_kernel_unglue_region_make_vert(bm, l_sep);
}

BMVert *BM_face_loop_separate_multi_isolated(BMesh *bm, BMLoop *l_sep)
{
  return bmesh_kernel_unglue_region_make_vert_multi_isolated(bm, l_sep);
}

BMVert *BM_face_loop_separate_multi(BMesh *bm, BMLoop **larr, int larr_len)
{
  return bmesh_kernel_unglue_region_make_vert_multi(bm, larr, larr_len);
}
