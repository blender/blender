/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * Basic mirror, optionally with UVs's.
 */

#include "MEM_guardedalloc.h"

#include "DNA_meshdata_types.h"

#include "BLI_math.h"

#include "BKE_customdata.h"

#include "bmesh.h"
#include "intern/bmesh_operators_private.h" /* own include */

#define ELE_NEW 1

void bmo_mirror_exec(BMesh *bm, BMOperator *op)
{
  BMOperator dupeop, weldop;
  BMOIter siter;
  BMVert *v;
  float scale[3] = {1.0f, 1.0f, 1.0f};
  float dist = BMO_slot_float_get(op->slots_in, "merge_dist");
  int i;
  int axis = BMO_slot_int_get(op->slots_in, "axis");
  bool mirror_u = BMO_slot_bool_get(op->slots_in, "mirror_u");
  bool mirror_v = BMO_slot_bool_get(op->slots_in, "mirror_v");
  bool mirror_udim = BMO_slot_bool_get(op->slots_in, "mirror_udim");
  BMOpSlot *slot_targetmap;
  BMOpSlot *slot_vertmap;

  BMO_op_initf(bm, &dupeop, op->flag, "duplicate geom=%s", op, "geom");
  BMO_op_exec(bm, &dupeop);

  BMO_slot_buffer_flag_enable(bm, dupeop.slots_out, "geom.out", BM_ALL_NOLOOP, ELE_NEW);

  /* feed old data to transform bmo */
  scale[axis] = -1.0f;
  BMO_op_callf(bm,
               op->flag,
               "scale verts=%fv vec=%v space=%s use_shapekey=%s",
               ELE_NEW,
               scale,
               op,
               "matrix",
               op,
               "use_shapekey");

  BMO_op_init(bm, &weldop, op->flag, "weld_verts");

  slot_targetmap = BMO_slot_get(weldop.slots_in, "targetmap");
  slot_vertmap = BMO_slot_get(dupeop.slots_out, "vert_map.out");

  BMO_ITER (v, &siter, op->slots_in, "geom", BM_VERT) {
    if (fabsf(v->co[axis]) <= dist) {
      BMVert *v_new = BMO_slot_map_elem_get(slot_vertmap, v);
      BLI_assert(v_new != NULL);
      BMO_slot_map_elem_insert(&weldop, slot_targetmap, v_new, v);
    }
  }

  if (mirror_u || mirror_v) {
    BMFace *f;
    BMLoop *l;
    float *luv;
    const int totlayer = CustomData_number_of_layers(&bm->ldata, CD_PROP_FLOAT2);
    BMIter liter;

    BMO_ITER (f, &siter, dupeop.slots_out, "geom.out", BM_FACE) {
      BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
        for (i = 0; i < totlayer; i++) {
          luv = CustomData_bmesh_get_n(&bm->ldata, l->head.data, CD_PROP_FLOAT2, i);
          if (mirror_u) {
            float uv_u = luv[0];
            if (mirror_udim) {
              luv[0] = ceilf(uv_u) - fmodf(uv_u, 1.0f);
            }
            else {
              luv[0] = 1.0f - uv_u;
            }
          }
          if (mirror_v) {
            float uv_v = luv[1];
            if (mirror_udim) {
              luv[1] = ceilf(uv_v) - fmodf(uv_v, 1.0f);
            }
            else {
              luv[1] = 1.0f - uv_v;
            }
          }
        }
      }
    }
  }

  BMO_op_exec(bm, &weldop);

  BMO_op_finish(bm, &weldop);
  BMO_op_finish(bm, &dupeop);

  BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "geom.out", BM_ALL_NOLOOP, ELE_NEW);
}
