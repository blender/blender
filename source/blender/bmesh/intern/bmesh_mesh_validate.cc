/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * BM mesh validation function.
 */

/* debug builds only */
#ifndef NDEBUG

#  include "BLI_map.hh"
#  include "BLI_ordered_edge.hh"
#  include "BLI_set.hh"
#  include "BLI_utildefines.h"

#  include "bmesh.hh"

#  include "bmesh_mesh_validate.hh"

/* macro which inserts the function name */
#  if defined __GNUC__
#    define ERRMSG(format, args...) \
      { \
        fprintf(stderr, "%s: " format ", " AT "\n", __func__, ##args); \
        errtot++; \
      } \
      (void)0
#  elif defined(_MSVC_TRADITIONAL) && !_MSVC_TRADITIONAL
#    define ERRMSG(format, ...) \
      { \
        fprintf(stderr, "%s: " format ", " AT "\n", __func__, ##__VA_ARGS__); \
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

template<> struct blender::DefaultHash<blender::Set<const BMVert *>> {
  uint64_t operator()(const blender::Set<const BMVert *> &value) const
  {
    uint64_t hash = 0;
    for (const BMVert *vert : value) {
      hash = get_default_hash(hash, vert);
    }
    return hash;
  }
};

bool BM_mesh_is_valid(BMesh *bm)
{
  blender::Map<blender::OrderedEdge, BMEdge *> edge_hash;
  edge_hash.reserve(bm->totedge);
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
    if (BM_elem_flag_test(v, BM_ELEM_SELECT | BM_ELEM_HIDDEN) == (BM_ELEM_SELECT | BM_ELEM_HIDDEN))
    {
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
    if (e->v1 == e->v2) {
      ERRMSG("edge %d: duplicate index: %d", i, BM_elem_index_get(e->v1));
    }

    /* Build edge-hash at the same time. */
    edge_hash.add_or_modify(
        {BM_elem_index_get(e->v1), BM_elem_index_get(e->v2)},
        [&](BMEdge **value) { *value = e; },
        [&](BMEdge **value) {
          ERRMSG("edge %d, %d: are duplicates", i, BM_elem_index_get(*value));
        });
  }

  /* edge radial structure */
  BM_ITER_MESH_INDEX (e, &iter, bm, BM_EDGES_OF_MESH, i) {
    if (BM_elem_flag_test(e, BM_ELEM_SELECT | BM_ELEM_HIDDEN) == (BM_ELEM_SELECT | BM_ELEM_HIDDEN))
    {
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
  blender::Map<blender::Set<const BMVert *>, int> face_map;
  BM_ITER_MESH_INDEX (f, &iter, bm, BM_FACES_OF_MESH, i) {
    BMLoop *l_iter;
    BMLoop *l_first;

    if (BM_elem_flag_test(f, BM_ELEM_SELECT | BM_ELEM_HIDDEN) == (BM_ELEM_SELECT | BM_ELEM_HIDDEN))
    {
      ERRMSG("face %d: is hidden and selected", i);
    }

    l_iter = l_first = BM_FACE_FIRST_LOOP(f);

    do {
      BM_elem_flag_disable(l_iter, BM_ELEM_INTERNAL_TAG);
      BM_elem_flag_disable(l_iter->v, BM_ELEM_INTERNAL_TAG);
      BM_elem_flag_disable(l_iter->e, BM_ELEM_INTERNAL_TAG);
    } while ((l_iter = l_iter->next) != l_first);

    j = 0;

    blender::Set<const BMVert *> face_verts;

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

      face_verts.add(l_iter->v);

      j++;
    } while ((l_iter = l_iter->next) != l_first);

    face_map.add_or_modify(
        std::move(face_verts),
        [&](int *value) { *value = i; },
        [&](const int *value) { ERRMSG("face %d: duplicate of %d", i, *value); });

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

  const bool is_valid = (errtot == 0);
  ERRMSG("Finished - errors %d", errtot);
  return is_valid;
}

#endif
