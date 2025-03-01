/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
 * Kinds of Partial Geometry
 * =========================
 *
 * All Tagged
 * ----------
 * Operate on everything that's tagged as well as connected geometry.
 * see: #BM_mesh_partial_create_from_verts
 *
 * Grouped
 * -------
 * Operate on everything that is connected to both tagged and un-tagged.
 * see: #BM_mesh_partial_create_from_verts_group_single
 *
 * Reduces computations when transforming isolated regions.
 *
 * Optionally support multiple groups since axis-mirror (for example)
 * will transform vertices in different directions, as well as keeping centered vertices.
 * see: #BM_mesh_partial_create_from_verts_group_multi
 *
 * \note Others can be added as needed.
 */

#include "MEM_guardedalloc.h"

#include "BLI_bit_vector.hh"
#include "BLI_math_base.h"

#include "bmesh.hh"

using blender::BitSpan;
using blender::BitVector;
using blender::MutableBitSpan;
using blender::Span;
using blender::Vector;

BLI_INLINE bool partial_elem_vert_ensure(BMPartialUpdate *bmpinfo,
                                         MutableBitSpan verts_tag,
                                         BMVert *v)
{
  const int i = BM_elem_index_get(v);
  if (!verts_tag[i]) {
    verts_tag[i].set();
    bmpinfo->verts.append(v);
    return true;
  }
  return false;
}

BLI_INLINE bool partial_elem_face_ensure(BMPartialUpdate *bmpinfo,
                                         MutableBitSpan faces_tag,
                                         BMFace *f)
{
  const int i = BM_elem_index_get(f);
  if (!faces_tag[i]) {
    faces_tag[i].set();
    bmpinfo->faces.append(f);
    return true;
  }
  return false;
}

BMPartialUpdate *BM_mesh_partial_create_from_verts(BMesh &bm,
                                                   const BMPartialUpdate_Params &params,
                                                   const BitSpan verts_mask,
                                                   const int verts_mask_count)
{
  /* The caller is doing something wrong if this isn't the case. */
  BLI_assert(verts_mask_count <= bm.totvert);

  BMPartialUpdate *bmpinfo = MEM_new<BMPartialUpdate>(__func__);

  /* Reserve more edges than vertices since it's common for a grid topology
   * to use around twice as many edges as vertices. */
  const int default_verts_len_alloc = verts_mask_count;
  const int default_faces_len_alloc = min_ii(bm.totface, verts_mask_count);

  /* Allocate tags instead of using #BM_ELEM_TAG because the caller may already be using tags.
   * Further, walking over all geometry to clear the tags isn't so efficient. */
  BitVector<> verts_tag;
  BitVector<> faces_tag;

  /* Set vert inline. */
  BM_mesh_elem_index_ensure(&bm, BM_FACE);

  if (params.do_normals || params.do_tessellate) {
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
    bmpinfo->faces.reserve(default_faces_len_alloc);
    faces_tag.resize(bm.totface);

    BMVert *v;
    BMIter iter;
    int i;
    BM_ITER_MESH_INDEX (v, &iter, &bm, BM_VERTS_OF_MESH, i) {
      BM_elem_index_set(v, i); /* set_inline */
      if (!verts_mask[i]) {
        continue;
      }
      BMEdge *e_iter = v->e;
      if (e_iter != nullptr) {
        /* Loop over edges. */
        BMEdge *e_first = v->e;
        do {
          BMLoop *l_iter = e_iter->l;
          if (e_iter->l != nullptr) {
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

  if (params.do_normals) {
    /* - Extend to all faces vertices:
     *   Any changes to the faces normal needs to update all surrounding vertices.
     *
     * - Extend to all these vertices connected edges:
     *   These and needed to access those vertices edge vectors in normal calculation logic.
     */

    /* Vertices. */
    bmpinfo->verts.reserve(default_verts_len_alloc);
    verts_tag.resize(bm.totvert);

    for (const BMFace *f : bmpinfo->faces) {
      BMLoop *l_iter, *l_first;
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      do {
        partial_elem_vert_ensure(bmpinfo, verts_tag, l_iter->v);
      } while ((l_iter = l_iter->next) != l_first);
    }
  }

  bmpinfo->params = params;

  return bmpinfo;
}

BMPartialUpdate *BM_mesh_partial_create_from_verts_group_single(
    BMesh &bm,
    const BMPartialUpdate_Params &params,
    const BitSpan verts_mask,
    const int verts_mask_count)
{
  BMPartialUpdate *bmpinfo = MEM_new<BMPartialUpdate>(__func__);

  BitVector<> verts_tag;
  BitVector<> faces_tag;

  int face_tag_loop_len = 0;

  if (params.do_normals || params.do_tessellate) {
    faces_tag.resize(bm.totface);

    BMFace *f;
    BMIter iter;
    int i;
    BM_ITER_MESH_INDEX (f, &iter, &bm, BM_FACES_OF_MESH, i) {
      enum Side { SIDE_A = (1 << 0), SIDE_B = (1 << 1) } side_flag = Side(0);
      BM_elem_index_set(f, i); /* set_inline */
      BMLoop *l_iter, *l_first;
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      do {
        const int j = BM_elem_index_get(l_iter->v);
        side_flag = Side(side_flag | (verts_mask[j].test() ? SIDE_A : SIDE_B));
        if (UNLIKELY(side_flag == (SIDE_A | SIDE_B))) {
          partial_elem_face_ensure(bmpinfo, faces_tag, f);
          face_tag_loop_len += f->len;
          break;
        }
      } while ((l_iter = l_iter->next) != l_first);
    }
  }

  if (params.do_normals) {
    /* Extend to all faces vertices:
     * Any changes to the faces normal needs to update all surrounding vertices. */

    /* Over allocate using the total number of face loops. */
    bmpinfo->verts.reserve(min_ii(bm.totvert, max_ii(1, face_tag_loop_len)));
    verts_tag.resize(bm.totvert);

    for (BMFace *f : bmpinfo->faces) {
      BMLoop *l_iter, *l_first;
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      do {
        partial_elem_vert_ensure(bmpinfo, verts_tag, l_iter->v);
      } while ((l_iter = l_iter->next) != l_first);
    }

    /* Loose vertex support, these need special handling as loose normals depend on location. */
    if (bmpinfo->verts.size() < verts_mask_count) {
      BMVert *v;
      BMIter iter;
      int i;
      BM_ITER_MESH_INDEX (v, &iter, &bm, BM_VERTS_OF_MESH, i) {
        if (verts_mask[i] && (BM_vert_find_first_loop(v) == nullptr)) {
          partial_elem_vert_ensure(bmpinfo, verts_tag, v);
        }
      }
    }
  }

  bmpinfo->params = params;

  return bmpinfo;
}

BMPartialUpdate *BM_mesh_partial_create_from_verts_group_multi(
    BMesh &bm,
    const BMPartialUpdate_Params &params,
    const Span<int> verts_group,
    const int verts_group_count)
{
  /* Provide a quick way of visualizing which faces are being manipulated. */
  // #define DEBUG_MATERIAL

  BMPartialUpdate *bmpinfo = MEM_new<BMPartialUpdate>(__func__);

  BitVector<> verts_tag;
  BitVector<> faces_tag;

  int face_tag_loop_len = 0;

  if (params.do_normals || params.do_tessellate) {
    faces_tag.resize(bm.totface);

    BMFace *f;
    BMIter iter;
    int i;
    BM_ITER_MESH_INDEX (f, &iter, &bm, BM_FACES_OF_MESH, i) {
      BM_elem_index_set(f, i); /* set_inline */
      BMLoop *l_iter, *l_first;
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      const int group_test = verts_group[BM_elem_index_get(l_iter->prev->v)];
#ifdef DEBUG_MATERIAL
      f->mat_nr = 0;
#endif
      do {
        const int group_iter = verts_group[BM_elem_index_get(l_iter->v)];
        if (UNLIKELY((group_iter != group_test) || (group_iter == -1))) {
          partial_elem_face_ensure(bmpinfo, faces_tag, f);
          face_tag_loop_len += f->len;
#ifdef DEBUG_MATERIAL
          f->mat_nr = 1;
#endif
          break;
        }
      } while ((l_iter = l_iter->next) != l_first);
    }
  }

  if (params.do_normals) {
    /* Extend to all faces vertices:
     * Any changes to the faces normal needs to update all surrounding vertices. */

    /* Over allocate using the total number of face loops. */
    bmpinfo->verts.reserve(min_ii(bm.totvert, max_ii(1, face_tag_loop_len)));
    verts_tag.resize(bm.totvert);

    for (BMFace *f : bmpinfo->faces) {
      BMLoop *l_iter, *l_first;
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      do {
        partial_elem_vert_ensure(bmpinfo, verts_tag, l_iter->v);
      } while ((l_iter = l_iter->next) != l_first);
    }

    /* Loose vertex support, these need special handling as loose normals depend on location. */
    if (bmpinfo->verts.size() < verts_group_count) {
      BMVert *v;
      BMIter iter;
      int i;
      BM_ITER_MESH_INDEX (v, &iter, &bm, BM_VERTS_OF_MESH, i) {
        if ((verts_group[i] != 0) && (BM_vert_find_first_loop(v) == nullptr)) {
          partial_elem_vert_ensure(bmpinfo, verts_tag, v);
        }
      }
    }
  }

  bmpinfo->params = params;

  return bmpinfo;
}

void BM_mesh_partial_destroy(BMPartialUpdate *bmpinfo)
{
  MEM_delete(bmpinfo);
}
