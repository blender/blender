/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * Pattern based geometry reduction which has the result similar to undoing
 * a subdivide operation.
 */

#include "BLI_utildefines.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "intern/bmesh_operators_private.h" /* own include */

void bmo_unsubdivide_exec(BMesh *bm, BMOperator *op)
{
  /* - `BMVert.flag & BM_ELEM_TAG`: Shows we touched this vert.
   * - `BMVert.index == -1`:        Shows we will remove this vert. */
  BMVert *v;
  BMIter iter;

  const int iterations = max_ii(1, BMO_slot_int_get(op->slots_in, "iterations"));

  BMOpSlot *vinput = BMO_slot_get(op->slots_in, "verts");
  BMVert **vinput_arr = (BMVert **)vinput->data.buf;
  int v_index;

  /* tag verts */
  BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
    BM_elem_flag_disable(v, BM_ELEM_TAG);
  }
  for (v_index = 0; v_index < vinput->len; v_index++) {
    v = vinput_arr[v_index];
    BM_elem_flag_enable(v, BM_ELEM_TAG);
  }

  /* do all the real work here */
  BM_mesh_decimate_unsubdivide_ex(bm, iterations, true);
}
