/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * BMesh decimator that uses a grid un-subdivide method.
 */

#include "MEM_guardedalloc.h"

#include "bmesh.hh"
#include "bmesh_decimate.hh" /* own include */

static bool bm_vert_dissolve_fan_test(BMVert *v)
{
  /* check if we should walk over these verts */
  BMIter iter;
  BMEdge *e;

  BMVert *varr[4];

  uint tot_edge = 0;
  uint tot_edge_boundary = 0;
  uint tot_edge_manifold = 0;
  uint tot_edge_wire = 0;

  BM_ITER_ELEM (e, &iter, v, BM_EDGES_OF_VERT) {
    if (BM_edge_is_boundary(e)) {
      tot_edge_boundary++;
    }
    else if (BM_edge_is_manifold(e)) {
      tot_edge_manifold++;
    }
    else if (BM_edge_is_wire(e)) {
      tot_edge_wire++;
    }

    /* bail out early */
    if (tot_edge == 4) {
      return false;
    }

    /* used to check overlapping faces */
    varr[tot_edge] = BM_edge_other_vert(e, v);

    tot_edge++;
  }

  if (((tot_edge == 4) && (tot_edge_boundary == 0) && (tot_edge_manifold == 4)) ||
      ((tot_edge == 3) && (tot_edge_boundary == 0) && (tot_edge_manifold == 3)) ||
      ((tot_edge == 3) && (tot_edge_boundary == 2) && (tot_edge_manifold == 1)))
  {
    if (!BM_face_exists(varr, tot_edge)) {
      return true;
    }
  }
  else if ((tot_edge == 2) && (tot_edge_wire == 2)) {
    return true;
  }
  return false;
}

static bool bm_vert_dissolve_fan(BMesh *bm, BMVert *v)
{
  /* Collapse under 2 conditions:
   * - vert connects to 4 manifold edges (and 4 faces).
   * - vert connects to 1 manifold edge, 2 boundary edges (and 2 faces).
   *
   * This covers boundary verts of a quad grid and center verts.
   * note that surrounding faces don't have to be quads.
   */

  BMIter iter;
  BMEdge *e;

  uint tot_loop = 0;
  uint tot_edge = 0;
  uint tot_edge_boundary = 0;
  uint tot_edge_manifold = 0;
  uint tot_edge_wire = 0;

  BM_ITER_ELEM (e, &iter, v, BM_EDGES_OF_VERT) {
    if (BM_edge_is_boundary(e)) {
      tot_edge_boundary++;
    }
    else if (BM_edge_is_manifold(e)) {
      tot_edge_manifold++;
    }
    else if (BM_edge_is_wire(e)) {
      tot_edge_wire++;
    }
    tot_edge++;
  }

  if (tot_edge == 2) {
    /* check for 2 wire verts only */
    if (tot_edge_wire == 2) {
      return (BM_vert_collapse_edge(bm, v->e, v, true, true, true) != nullptr);
    }
  }
  else if (tot_edge == 4) {
    /* check for 4 faces surrounding */
    if (tot_edge_boundary == 0 && tot_edge_manifold == 4) {
      /* good to go! */
      tot_loop = 4;
    }
  }
  else if (tot_edge == 3) {
    /* check for 2 faces surrounding at a boundary */
    if (tot_edge_boundary == 2 && tot_edge_manifold == 1) {
      /* good to go! */
      tot_loop = 2;
    }
    else if (tot_edge_boundary == 0 && tot_edge_manifold == 3) {
      /* good to go! */
      tot_loop = 3;
    }
  }

  if (tot_loop) {
    BMLoop *f_loop[4];
    uint i;

    /* ensure there are exactly tot_loop loops */
    BLI_assert(BM_iter_at_index(bm, BM_LOOPS_OF_VERT, v, tot_loop) == nullptr);
    BM_iter_as_array(bm, BM_LOOPS_OF_VERT, v, (void **)f_loop, tot_loop);

    for (i = 0; i < tot_loop; i++) {
      BMLoop *l = f_loop[i];
      if (l->f->len > 3) {
        BMLoop *l_new;
        BLI_assert(l->prev->v != l->next->v);
        BM_face_split(bm, l->f, l->prev, l->next, &l_new, nullptr, true);
        BM_elem_flag_merge_into(l_new->e, l->e, l->prev->e);
      }
    }

    return BM_vert_dissolve(bm, v);
  }

  return false;
}

/**
 * Note that #bm_tag_untagged_neighbors requires #VERT_INDEX_DO_COLLAPSE & #VERT_INDEX_IGNORE
 * are equal magnitude, opposite sign.
 */
enum {
  VERT_INDEX_DO_COLLAPSE = -1,
  VERT_INDEX_INIT = 0,
  VERT_INDEX_IGNORE = 1,
};

/**
 * Given a set of starting verts, find all the currently-untagged neighbors of those verts, tag
 * them with the specified value, and return an array specifying all the newly-tagged verts.
 *
 * By using two arrays and two tag values, repeated alternating calls will expand the selection in
 * an alternating tagging pattern. Dissolving one of the two tags will then reduce the density of
 * the mesh, by half, in a regular diamond pattern.
 *
 * \param verts_start: The array of starting verts whose neighbors should be tagged.
 * \param verts_start_num: The number of verts in the verts_start array.
 * \param desired_tag: The value to set as a tag, on any currently-untagged neighbors.
 * \param r_verts_tagged: Returned array of all the verts which were tagged in this call.
 * \param r_verts_tagged_num: Returned number of verts in the r_verts_tagged array.
 */
static void bm_tag_untagged_neighbors(BMVert *verts_start[],
                                      const uint verts_start_num,
                                      const int desired_tag,
                                      BMVert *r_verts_tagged[],
                                      uint &r_verts_tagged_num)
{

  BMEdge *e;
  BMIter iter;
  r_verts_tagged_num = 0;

  for (int i = 0; i < verts_start_num; i++) {
    BMVert *v = verts_start[i];

    /* Since DO_COLLAPSE and IGNORE are -1 and +1, inverting the sign finds the other. */
    BLI_assert(BM_elem_index_get(v) == -desired_tag);

    BM_ITER_ELEM (e, &iter, v, BM_EDGES_OF_VERT) {
      BMVert *v_other = BM_edge_other_vert(e, v);
      if (BM_elem_index_get(v_other) == VERT_INDEX_INIT) {
        BM_elem_index_set(v_other, desired_tag); /* set_dirty! */
        r_verts_tagged[r_verts_tagged_num++] = v_other;
      }
    }
  }
}

/* - BMVert.flag & BM_ELEM_TAG:  shows we touched this vert
 * - BMVert.index == -1:         shows we will remove this vert
 */

void BM_mesh_decimate_unsubdivide_ex(BMesh *bm, const int iterations, const bool tag_only)
{
  /* NOTE: while #BMWalker seems like a logical choice, it results in uneven geometry. */

  BMVert **verts_collapse = static_cast<BMVert **>(
      MEM_mallocN(sizeof(BMVert *) * bm->totvert, __func__));
  BMVert **verts_ignore = static_cast<BMVert **>(
      MEM_mallocN(sizeof(BMVert *) * bm->totvert, __func__));
  uint verts_collapse_num = 0;
  uint verts_ignore_num = 0;

  BMIter iter;

  int iter_step;

  /* if tag_only is set, we assume the caller knows what verts to tag
   * needed for the operator */
  if (tag_only == false) {
    BMVert *v;
    BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
      BM_elem_flag_enable(v, BM_ELEM_TAG);
    }
  }

  /* Perform the number of iteration steps which the user requested. */
  for (iter_step = 0; iter_step < iterations; iter_step++) {
    BMVert *v, *v_next;
    bool verts_were_marked_for_dissolve = false;

    /* Tag all verts which are eligible to be dissolved on this iteration. */
    BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
      if (BM_elem_flag_test(v, BM_ELEM_TAG) && bm_vert_dissolve_fan_test(v)) {
        BM_elem_index_set(v, VERT_INDEX_INIT); /* set_dirty! */
      }
      else {
        BM_elem_index_set(v, VERT_INDEX_IGNORE); /* set_dirty! */
      }
    }

    /* main loop, keep tagging until we can't tag any more islands */
    BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {

      /* Only process verts which are eligible for dissolve and which have not yet been tagged. */
      if (!(BM_elem_index_get(v) == VERT_INDEX_INIT)) {
        continue;
      }

      /* Set the first #VERT_INDEX_INIT vert as the first starting vert. */
      BM_elem_index_set(v, VERT_INDEX_IGNORE); /* set_dirty! */
      verts_ignore[0] = v;
      verts_ignore_num = 1;

      /* Starting at v, expand outwards, tagging any currently untagged neighbors.
       * verts will be alternately tagged for collapse or ignore.
       * Stop when there are no neighbors left to expand to. */
      while (true) {

        bm_tag_untagged_neighbors(verts_ignore,
                                  verts_ignore_num,
                                  VERT_INDEX_DO_COLLAPSE, /* set_dirty! */
                                  verts_collapse,
                                  verts_collapse_num);
        if (verts_collapse_num == 0) {
          break;
        }
        verts_were_marked_for_dissolve = true;

        bm_tag_untagged_neighbors(verts_collapse,
                                  verts_collapse_num,
                                  VERT_INDEX_IGNORE, /* set_dirty! */
                                  verts_ignore,
                                  verts_ignore_num);
        if (verts_ignore_num == 0) {
          break;
        }
      }
    }

    /* At high iteration levels, later steps can run out of verts that are eligible for dissolve.
     * If this occurs, stop. Future iterations won't find any verts that this iteration didn't. */
    if (!verts_were_marked_for_dissolve) {
      break;
    }

    /* Remove all verts tagged for removal. */
    BM_ITER_MESH_MUTABLE (v, v_next, &iter, bm, BM_VERTS_OF_MESH) {
      if (BM_elem_index_get(v) == VERT_INDEX_DO_COLLAPSE) {
        bm_vert_dissolve_fan(bm, v);
      }
    }
  }

  /* Ensure the vert index values will be recomputed. */
  bm->elem_index_dirty |= BM_VERT;

  MEM_freeN(verts_collapse);
  MEM_freeN(verts_ignore);
}

void BM_mesh_decimate_unsubdivide(BMesh *bm, const int iterations)
{
  BM_mesh_decimate_unsubdivide_ex(bm, iterations, false);
}
