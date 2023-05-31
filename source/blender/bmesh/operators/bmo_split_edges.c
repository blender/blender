/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * Just a wrapper around #BM_mesh_edgesplit
 */

#include "BLI_utildefines.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "intern/bmesh_operators_private.h" /* own include */

void bmo_split_edges_exec(BMesh *bm, BMOperator *op)
{
  const bool use_verts = BMO_slot_bool_get(op->slots_in, "use_verts");

  BM_mesh_elem_hflag_disable_all(bm, BM_EDGE, BM_ELEM_TAG, false);
  BMO_slot_buffer_hflag_enable(bm, op->slots_in, "edges", BM_EDGE, BM_ELEM_TAG, false);

  if (use_verts) {
    /* this slows down the operation but its ok because the modifier doesn't use */
    BMO_slot_buffer_hflag_enable(bm, op->slots_in, "verts", BM_VERT, BM_ELEM_TAG, false);
  }

  /* this is where everything happens */
  BM_mesh_edgesplit(bm, use_verts, true, false);

  BMO_slot_buffer_from_enabled_hflag(bm, op, op->slots_out, "edges.out", BM_EDGE, BM_ELEM_TAG);
}
