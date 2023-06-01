/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * BMesh separate, disconnects a set of faces from all others,
 * so they don't share any vertices/edges with other faces.
 */

#include <limits.h>

#include "MEM_guardedalloc.h"

#include "BLI_buffer.h"
#include "BLI_utildefines.h"

#include "bmesh.h"
#include "bmesh_separate.h" /* own include */
#include "intern/bmesh_private.h"

void BM_mesh_separate_faces(BMesh *bm, BMFaceFilterFunc filter_fn, void *user_data)
{
  BMFace **faces_array_all = MEM_mallocN(bm->totface * sizeof(BMFace *), __func__);
  /*
   * - Create an array of faces based on 'filter_fn'.
   *   First part of array for match, for non-match.
   *
   * - Enable all vertex tags, then clear all tagged vertices from 'faces_b'.
   *
   * - Loop over 'faces_a', checking each vertex,
   *   splitting out any which aren't tagged (and therefor shared), disabling tags as we go.
   */

  BMFace *f;
  BMIter iter;

  uint faces_a_len = 0;
  uint faces_b_len = 0;
  {
    int i_a = 0;
    int i_b = bm->totface;
    BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
      faces_array_all[filter_fn(f, user_data) ? i_a++ : --i_b] = f;
    }
    faces_a_len = i_a;
    faces_b_len = bm->totface - i_a;
  }

  BMFace **faces_a = faces_array_all;
  BMFace **faces_b = faces_array_all + faces_a_len;

  /* Enable for all. */
  BM_mesh_elem_hflag_enable_all(bm, BM_VERT, BM_ELEM_TAG, false);

  /* Disable vert tag on faces_b */
  for (uint i = 0; i < faces_b_len; i++) {
    BMLoop *l_iter, *l_first;
    l_iter = l_first = BM_FACE_FIRST_LOOP(faces_b[i]);
    do {
      BM_elem_flag_disable(l_iter->v, BM_ELEM_TAG);
    } while ((l_iter = l_iter->next) != l_first);
  }

  BLI_buffer_declare_static(BMLoop **, loop_split, 0, 128);

  /* Check shared verts ('faces_a' tag and disable) */
  for (uint i = 0; i < faces_a_len; i++) {
    BMLoop *l_iter, *l_first;
    l_iter = l_first = BM_FACE_FIRST_LOOP(faces_a[i]);
    do {
      if (!BM_elem_flag_test(l_iter->v, BM_ELEM_TAG)) {
        BMVert *v = l_iter->v;
        /* Enable, since we may visit this vertex again on other faces */
        BM_elem_flag_enable(v, BM_ELEM_TAG);

        /* We know the vertex is shared, collect all vertices and split them off. */

        /* Fill 'loop_split' */
        {
          BMEdge *e_first, *e_iter;
          e_iter = e_first = l_iter->e;
          do {
            if (e_iter->l != NULL) {
              BMLoop *l_radial_first, *l_radial_iter;
              l_radial_first = l_radial_iter = e_iter->l;
              do {
                if (l_radial_iter->v == v) {
                  if (filter_fn(l_radial_iter->f, user_data)) {
                    BLI_buffer_append(&loop_split, BMLoop *, l_radial_iter);
                  }
                }
              } while ((l_radial_iter = l_radial_iter->radial_next) != l_radial_first);
            }
          } while ((e_iter = bmesh_disk_edge_next(e_iter, v)) != e_first);
        }

        /* Perform the split */
        BM_face_loop_separate_multi(bm, loop_split.data, loop_split.count);

        BLI_buffer_clear(&loop_split);
      }
    } while ((l_iter = l_iter->next) != l_first);
  }

  BLI_buffer_free(&loop_split);

  MEM_freeN(faces_array_all);
}
