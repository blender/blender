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
 * Generate data needed for partially updating mesh information.
 * Currently this is used for normals and tessellation.
 *
 * Transform is the obvious use case where there is no need to update normals or tessellation
 * for geometry which has not been modified.
 *
 * In the future this could be integrated into GPU updates too.
 *
 * Potential Improvements
 * ======================
 *
 * Some calculations could be significantly limited in the case of affine transformations
 * (tessellation is an obvious candidate). Where only faces which have a mix
 * of tagged and untagged vertices would need to be recalculated.
 *
 * In general this would work well besides some corner cases such as scaling to zero.
 * Although the exact result may depend on the normal (for N-GONS),
 * so for now update the tessellation of all tagged geometry.
 */

#include "DNA_object_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_bitmap.h"
#include "BLI_math_vector.h"

#include "bmesh.h"

/**
 * Grow by 1.5x (rounding up).
 *
 * \note Use conservative reallocation since the initial sizes reserved
 * may be close to (or exactly) the number of elements needed.
 */
#define GROW(len_alloc) ((len_alloc) + ((len_alloc) - ((len_alloc) / 2)))
#define GROW_ARRAY(mem, len_alloc) \
  { \
    mem = MEM_reallocN(mem, (sizeof(*mem)) * ((len_alloc) = GROW(len_alloc))); \
  } \
  ((void)0)

#define GROW_ARRAY_AS_NEEDED(mem, len_alloc, index) \
  if (UNLIKELY(len_alloc == index)) { \
    GROW_ARRAY(mem, len_alloc); \
  }

BLI_INLINE bool partial_elem_vert_ensure(BMPartialUpdate *bmpinfo,
                                         BLI_bitmap *verts_tag,
                                         BMVert *v)
{
  const int i = BM_elem_index_get(v);
  if (!BLI_BITMAP_TEST(verts_tag, i)) {
    BLI_BITMAP_ENABLE(verts_tag, i);
    GROW_ARRAY_AS_NEEDED(bmpinfo->verts, bmpinfo->verts_len_alloc, bmpinfo->verts_len);
    bmpinfo->verts[bmpinfo->verts_len++] = v;
    return true;
  }
  return false;
}

BLI_INLINE bool partial_elem_edge_ensure(BMPartialUpdate *bmpinfo,
                                         BLI_bitmap *edges_tag,
                                         BMEdge *e)
{
  const int i = BM_elem_index_get(e);
  if (!BLI_BITMAP_TEST(edges_tag, i)) {
    BLI_BITMAP_ENABLE(edges_tag, i);
    GROW_ARRAY_AS_NEEDED(bmpinfo->edges, bmpinfo->edges_len_alloc, bmpinfo->edges_len);
    bmpinfo->edges[bmpinfo->edges_len++] = e;
    return true;
  }
  return false;
}

BLI_INLINE bool partial_elem_face_ensure(BMPartialUpdate *bmpinfo,
                                         BLI_bitmap *faces_tag,
                                         BMFace *f)
{
  const int i = BM_elem_index_get(f);
  if (!BLI_BITMAP_TEST(faces_tag, i)) {
    BLI_BITMAP_ENABLE(faces_tag, i);
    GROW_ARRAY_AS_NEEDED(bmpinfo->faces, bmpinfo->faces_len_alloc, bmpinfo->faces_len);
    bmpinfo->faces[bmpinfo->faces_len++] = f;
    return true;
  }
  return false;
}

BMPartialUpdate *BM_mesh_partial_create_from_verts(BMesh *bm,
                                                   const BMPartialUpdate_Params *params,
                                                   const int verts_len,
                                                   bool (*filter_fn)(BMVert *, void *user_data),
                                                   void *user_data)
{
  /* The caller is doing something wrong if this isn't the case. */
  BLI_assert(verts_len <= bm->totvert);

  BMPartialUpdate *bmpinfo = MEM_callocN(sizeof(*bmpinfo), __func__);

  /* Reserve more edges than vertices since it's common for a grid topology
   * to use around twice as many edges as vertices. */
  const int default_verts_len_alloc = verts_len;
  const int default_edges_len_alloc = min_ii(bm->totedge, verts_len * 2);
  const int default_faces_len_alloc = min_ii(bm->totface, verts_len);

  /* Allocate tags instead of using #BM_ELEM_TAG because the caller may already be using tags.
   * Further, walking over all geometry to clear the tags isn't so efficient. */
  BLI_bitmap *verts_tag = NULL;
  BLI_bitmap *edges_tag = NULL;
  BLI_bitmap *faces_tag = NULL;

  /* Set vert inline. */
  BM_mesh_elem_index_ensure(bm, (BM_EDGE | BM_FACE));

  if (params->do_normals || params->do_tessellate) {
    /* - Extend to all vertices connected faces:
     *   In the case of tessellation this is enough.
     *
     *   In the case of vertex normal calculation,
     *   All the relevant connectivity data can be accessed from the faces
     *   (there is no advantage in storing connected edges or vertices in this pass).
     *
     * NOTE: In the future it may be useful to differentiate between vertices
     * that are directly marked (by the filter function when looping over all vertices).
     * And vertices marked from indirect connections.
     * This would require an extra tag array, so avoid this unless it's needed.
     */

    /* Faces. */
    if (bmpinfo->faces == NULL) {
      bmpinfo->faces_len_alloc = default_faces_len_alloc;
      bmpinfo->faces = MEM_mallocN((sizeof(BMFace *) * bmpinfo->faces_len_alloc), __func__);
      faces_tag = BLI_BITMAP_NEW((size_t)bm->totface, __func__);
    }

    BMVert *v;
    BMIter iter;
    int i;
    BM_ITER_MESH_INDEX (v, &iter, bm, BM_VERTS_OF_MESH, i) {
      BM_elem_index_set(v, i); /* set_inline */
      if (!filter_fn(v, user_data)) {
        continue;
      }
      BMEdge *e_iter = v->e;
      if (e_iter != NULL) {
        /* Loop over edges. */
        BMEdge *e_first = v->e;
        do {
          BMLoop *l_iter = e_iter->l;
          if (e_iter->l != NULL) {
            BMLoop *l_first = e_iter->l;
            /* Loop over radial loops. */
            do {
              if (l_iter->v == v) {
                partial_elem_face_ensure(bmpinfo, faces_tag, l_iter->f);
              }
            } while ((l_iter = l_iter->radial_next) != l_first);
          }
        } while ((e_iter = BM_DISK_EDGE_NEXT(e_iter, v)) != e_first);
      }
    }
  }

  if (params->do_normals) {
    /* - Extend to all faces vertices:
     *   Any changes to the faces normal needs to update all surrounding vertices.
     *
     * - Extend to all these vertices connected edges:
     *   These and needed to access those vertices edge vectors in normal calculation logic.
     */

    /* Vertices. */
    if (bmpinfo->verts == NULL) {
      bmpinfo->verts_len_alloc = default_verts_len_alloc;
      bmpinfo->verts = MEM_mallocN((sizeof(BMVert *) * bmpinfo->verts_len_alloc), __func__);
      verts_tag = BLI_BITMAP_NEW((size_t)bm->totvert, __func__);
    }

    /* Edges. */
    if (bmpinfo->edges == NULL) {
      bmpinfo->edges_len_alloc = default_edges_len_alloc;
      bmpinfo->edges = MEM_mallocN((sizeof(BMEdge *) * bmpinfo->edges_len_alloc), __func__);
      edges_tag = BLI_BITMAP_NEW((size_t)bm->totedge, __func__);
    }

    for (int i = 0; i < bmpinfo->faces_len; i++) {
      BMFace *f = bmpinfo->faces[i];
      BMLoop *l_iter, *l_first;
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      do {
        if (!partial_elem_vert_ensure(bmpinfo, verts_tag, l_iter->v)) {
          continue;
        }
        BMVert *v = l_iter->v;
        BMEdge *e_first = v->e;
        BMEdge *e_iter = e_first;
        do {
          if (e_iter->l) {
            partial_elem_edge_ensure(bmpinfo, edges_tag, e_iter);
          }
        } while ((e_iter = BM_DISK_EDGE_NEXT(e_iter, v)) != e_first);
      } while ((l_iter = l_iter->next) != l_first);
    }
  }

  if (verts_tag) {
    MEM_freeN(verts_tag);
  }
  if (edges_tag) {
    MEM_freeN(edges_tag);
  }
  if (faces_tag) {
    MEM_freeN(faces_tag);
  }

  bmpinfo->params = *params;

  return bmpinfo;
}

void BM_mesh_partial_destroy(BMPartialUpdate *bmpinfo)
{
  if (bmpinfo->verts) {
    MEM_freeN(bmpinfo->verts);
  }
  if (bmpinfo->edges) {
    MEM_freeN(bmpinfo->edges);
  }
  if (bmpinfo->faces) {
    MEM_freeN(bmpinfo->faces);
  }
  MEM_freeN(bmpinfo);
}
