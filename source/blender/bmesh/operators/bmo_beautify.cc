/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * Beautify the mesh by rotating edges between triangles
 * to more attractive positions until no more rotations can be made.
 */

#include "BLI_math.h"

#include "MEM_guardedalloc.h"

#include "bmesh.h"
#include "bmesh_tools.h"
#include "intern/bmesh_operators_private.h"

#define ELE_NEW 1
#define FACE_MARK 2

void bmo_beautify_fill_exec(BMesh *bm, BMOperator *op)
{
  BMIter iter;
  BMOIter siter;
  BMFace *f;
  BMEdge *e;
  const bool use_restrict_tag = BMO_slot_bool_get(op->slots_in, "use_restrict_tag");
  const short flag =
      ((use_restrict_tag ? VERT_RESTRICT_TAG : 0) |
       /* Enable to avoid iterative edge rotation to cause the direction of faces to flip. */
       EDGE_RESTRICT_DEGENERATE);
  const short method = (short)BMO_slot_int_get(op->slots_in, "method");

  BMEdge **edge_array;
  int edge_array_len = 0;
  BMO_ITER (f, &siter, op->slots_in, "faces", BM_FACE) {
    if (f->len == 3) {
      BMO_face_flag_enable(bm, f, FACE_MARK);
    }
  }

  BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
    BM_elem_flag_disable(e, BM_ELEM_TAG);
  }

  /* will over alloc if some edges can't be rotated */
  edge_array = static_cast<BMEdge **>(MEM_mallocN(
      sizeof(*edge_array) * (size_t)BMO_slot_buffer_len(op->slots_in, "edges"), __func__));

  BMO_ITER (e, &siter, op->slots_in, "edges", BM_EDGE) {

    /* edge is manifold and can be rotated */
    if (BM_edge_rotate_check(e) &&
        /* faces are tagged */
        BMO_face_flag_test(bm, e->l->f, FACE_MARK) &&
        BMO_face_flag_test(bm, e->l->radial_next->f, FACE_MARK))
    {
      edge_array[edge_array_len] = e;
      edge_array_len++;
    }
  }

  BM_mesh_beautify_fill(
      bm, edge_array, edge_array_len, flag, method, ELE_NEW, FACE_MARK | ELE_NEW);

  MEM_freeN(edge_array);

  BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "geom.out", BM_EDGE | BM_FACE, ELE_NEW);
}
