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
 *
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bmesh
 *
 * BM mesh validation function.
 */

/* debug builds only */
#ifdef DEBUG

#  include "BLI_utildefines.h"
#  include "BLI_edgehash.h"

#  include "bmesh.h"

#  include "bmesh_mesh_validate.h"

/* macro which inserts the function name */
#  if defined __GNUC__
#    define ERRMSG(format, args...) \
      { \
        fprintf(stderr, "%s: " format ", " AT "\n", __func__, ##args); \
        errtot++; \
      } \
      (void)0
#  else
#    define ERRMSG(format, ...) \
      { \
        fprintf(stderr, "%s: " format ", " AT "\n", __func__, __VA_ARGS__); \
        errtot++; \
      } \
      (void)0
#  endif

/**
 * Check of this BMesh is valid,
 * this function can be slow since its intended to help with debugging.
 *
 * \return true when the mesh is valid.
 */
bool BM_mesh_validate(BMesh *bm)
{
  EdgeHash *edge_hash = BLI_edgehash_new_ex(__func__, bm->totedge);
  int errtot;

  BMIter iter;
  BMVert *v;
  BMEdge *e;
  BMFace *f;

  int i, j;

  errtot = -1; /* 'ERRMSG' next line will set at zero */
  fprintf(stderr, "\n");
  ERRMSG("This is a debugging function and not intended for general use, running slow test!");

  /* force recalc, even if tagged as valid, since this mesh is suspect! */
  bm->elem_index_dirty |= BM_ALL;
  BM_mesh_elem_index_ensure(bm, BM_ALL);

  BM_ITER_MESH_INDEX (v, &iter, bm, BM_VERTS_OF_MESH, i) {
    if (BM_elem_flag_test(v, BM_ELEM_SELECT | BM_ELEM_HIDDEN) ==
        (BM_ELEM_SELECT | BM_ELEM_HIDDEN)) {
      ERRMSG("vert %d: is hidden and selected", i);
    }

    if (v->e) {
      if (!BM_vert_in_edge(v->e, v)) {
        ERRMSG("vert %d: is not in its referenced edge: %d", i, BM_elem_index_get(v->e));
      }
    }
  }

  /* check edges */
  BM_ITER_MESH_INDEX (e, &iter, bm, BM_EDGES_OF_MESH, i) {
    void **val_p;

    if (e->v1 == e->v2) {
      ERRMSG("edge %d: duplicate index: %d", i, BM_elem_index_get(e->v1));
    }

    /* build edgehash at the same time */
    if (BLI_edgehash_ensure_p(
            edge_hash, BM_elem_index_get(e->v1), BM_elem_index_get(e->v2), &val_p)) {
      BMEdge *e_other = *val_p;
      ERRMSG("edge %d, %d: are duplicates", i, BM_elem_index_get(e_other));
    }
    else {
      *val_p = e;
    }
  }

  /* edge radial structure */
  BM_ITER_MESH_INDEX (e, &iter, bm, BM_EDGES_OF_MESH, i) {
    if (BM_elem_flag_test(e, BM_ELEM_SELECT | BM_ELEM_HIDDEN) ==
        (BM_ELEM_SELECT | BM_ELEM_HIDDEN)) {
      ERRMSG("edge %d: is hidden and selected", i);
    }

    if (e->l) {
      BMLoop *l_iter;
      BMLoop *l_first;

      j = 0;

      l_iter = l_first = e->l;
      /* we could do more checks here, but save for face checks */
      do {
        if (l_iter->e != e) {
          ERRMSG("edge %d: has invalid loop, loop is of face %d", i, BM_elem_index_get(l_iter->f));
        }
        else if (BM_vert_in_edge(e, l_iter->v) == false) {
          ERRMSG("edge %d: has invalid loop with vert not in edge, loop is of face %d",
                 i,
                 BM_elem_index_get(l_iter->f));
        }
        else if (BM_vert_in_edge(e, l_iter->next->v) == false) {
          ERRMSG("edge %d: has invalid loop with next vert not in edge, loop is of face %d",
                 i,
                 BM_elem_index_get(l_iter->f));
        }
      } while ((l_iter = l_iter->radial_next) != l_first);
    }
  }

  /* face structure */
  BM_ITER_MESH_INDEX (f, &iter, bm, BM_FACES_OF_MESH, i) {
    BMLoop *l_iter;
    BMLoop *l_first;

    if (BM_elem_flag_test(f, BM_ELEM_SELECT | BM_ELEM_HIDDEN) ==
        (BM_ELEM_SELECT | BM_ELEM_HIDDEN)) {
      ERRMSG("face %d: is hidden and selected", i);
    }

    l_iter = l_first = BM_FACE_FIRST_LOOP(f);

    do {
      BM_elem_flag_disable(l_iter, BM_ELEM_INTERNAL_TAG);
      BM_elem_flag_disable(l_iter->v, BM_ELEM_INTERNAL_TAG);
      BM_elem_flag_disable(l_iter->e, BM_ELEM_INTERNAL_TAG);
    } while ((l_iter = l_iter->next) != l_first);

    j = 0;

    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      if (BM_elem_flag_test(l_iter, BM_ELEM_INTERNAL_TAG)) {
        ERRMSG("face %d: has duplicate loop at corner: %d", i, j);
      }
      if (BM_elem_flag_test(l_iter->v, BM_ELEM_INTERNAL_TAG)) {
        ERRMSG(
            "face %d: has duplicate vert: %d, at corner: %d", i, BM_elem_index_get(l_iter->v), j);
      }
      if (BM_elem_flag_test(l_iter->e, BM_ELEM_INTERNAL_TAG)) {
        ERRMSG(
            "face %d: has duplicate edge: %d, at corner: %d", i, BM_elem_index_get(l_iter->e), j);
      }

      /* adjacent data checks */
      if (l_iter->f != f) {
        ERRMSG("face %d: has loop that points to face: %d at corner: %d",
               i,
               BM_elem_index_get(l_iter->f),
               j);
      }
      if (l_iter != l_iter->prev->next) {
        ERRMSG("face %d: has invalid 'prev/next' at corner: %d", i, j);
      }
      if (l_iter != l_iter->next->prev) {
        ERRMSG("face %d: has invalid 'next/prev' at corner: %d", i, j);
      }
      if (l_iter != l_iter->radial_prev->radial_next) {
        ERRMSG("face %d: has invalid 'radial_prev/radial_next' at corner: %d", i, j);
      }
      if (l_iter != l_iter->radial_next->radial_prev) {
        ERRMSG("face %d: has invalid 'radial_next/radial_prev' at corner: %d", i, j);
      }

      BM_elem_flag_enable(l_iter, BM_ELEM_INTERNAL_TAG);
      BM_elem_flag_enable(l_iter->v, BM_ELEM_INTERNAL_TAG);
      BM_elem_flag_enable(l_iter->e, BM_ELEM_INTERNAL_TAG);
      j++;
    } while ((l_iter = l_iter->next) != l_first);

    if (j != f->len) {
      ERRMSG("face %d: has length of %d but should be %d", i, f->len, j);
    }

    /* leave elements un-tagged, not essential but nice to avoid unintended dirty tag use later. */
    do {
      BM_elem_flag_disable(l_iter, BM_ELEM_INTERNAL_TAG);
      BM_elem_flag_disable(l_iter->v, BM_ELEM_INTERNAL_TAG);
      BM_elem_flag_disable(l_iter->e, BM_ELEM_INTERNAL_TAG);
    } while ((l_iter = l_iter->next) != l_first);
  }

  BLI_edgehash_free(edge_hash, NULL);

  const bool is_valid = (errtot == 0);
  ERRMSG("Finished - errors %d", errtot);
  return is_valid;
}

#endif
