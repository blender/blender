/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * Makes the mesh symmetrical by splitting along an axis and duplicating the geometry.
 */

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "bmesh.h"
#include "intern/bmesh_operators_private.h"

#define ELE_OUT 1

void bmo_symmetrize_exec(BMesh *bm, BMOperator *op)
{
  const float dist = BMO_slot_float_get(op->slots_in, "dist");
  const int direction = BMO_slot_int_get(op->slots_in, "direction");
  const int axis = direction % 3;

  BMOperator op_bisect;
  BMOperator op_dupe;
  BMOperator op_weld;

  BMOpSlot *slot_vertmap;
  BMOpSlot *slot_targetmap;

  float plane_no[3];
  float scale[3];

  BMOIter siter;
  BMVert *v;

  copy_v3_fl(plane_no, 0.0f);
  copy_v3_fl(scale, 1.0f);

  plane_no[axis] = direction > 2 ? -1.0f : 1.0f;
  scale[axis] *= -1.0f;

  /* Cut in half */
  BMO_op_initf(bm,
               &op_bisect,
               op->flag,
               "bisect_plane geom=%s plane_no=%v dist=%f clear_outer=%b use_snap_center=%b",
               op,
               "input",
               plane_no,
               dist,
               true,
               true);

  BMO_op_exec(bm, &op_bisect);

  /* Duplicate */
  BMO_op_initf(bm, &op_dupe, op->flag, "duplicate geom=%S", &op_bisect, "geom.out");

  BMO_op_exec(bm, &op_dupe);

  /* Flag for output (some will be merged) */
  BMO_slot_buffer_flag_enable(bm, op_bisect.slots_out, "geom.out", BM_ALL_NOLOOP, ELE_OUT);
  BMO_slot_buffer_flag_enable(bm, op_dupe.slots_out, "geom.out", BM_ALL_NOLOOP, ELE_OUT);

  BMO_op_callf(bm,
               op->flag,
               "scale verts=%S vec=%v use_shapekey=%s",
               &op_dupe,
               "geom.out",
               scale,
               op,
               "use_shapekey");

  /* important 'flip_multires' is disabled,
   * otherwise multi-res data will be reversed, see: #47788 */
  BMO_op_callf(bm, op->flag, "reverse_faces faces=%S", &op_dupe, "geom.out");

  /* Weld verts */
  BMO_op_init(bm, &op_weld, op->flag, "weld_verts");

  slot_vertmap = BMO_slot_get(op_dupe.slots_out, "vert_map.out");
  slot_targetmap = BMO_slot_get(op_weld.slots_in, "targetmap");

  BMO_ITER (v, &siter, op_bisect.slots_out, "geom_cut.out", BM_VERT) {
    BMVert *v_dupe = BMO_slot_map_elem_get(slot_vertmap, v);
    BMO_slot_map_elem_insert(&op_weld, slot_targetmap, v_dupe, v);
  }

  BMO_op_exec(bm, &op_weld);

  /* Cleanup */
  BMO_op_finish(bm, &op_weld);

  BMO_op_finish(bm, &op_dupe);
  BMO_op_finish(bm, &op_bisect);

  /* Create output */
  BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "geom.out", BM_ALL_NOLOOP, ELE_OUT);
}
