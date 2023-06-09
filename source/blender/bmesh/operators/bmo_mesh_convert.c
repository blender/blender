/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * This file contains functions
 * for converting a Mesh
 * into a Bmesh, and back again.
 */

#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "BLI_math.h"

#include "bmesh.h"
#include "intern/bmesh_operators_private.h"

#include "BKE_global.h"

void bmo_mesh_to_bmesh_exec(BMesh *bm, BMOperator *op)
{
  Object *ob = BMO_slot_ptr_get(op->slots_in, "object");
  Mesh *me = BMO_slot_ptr_get(op->slots_in, "mesh");
  bool set_key = BMO_slot_bool_get(op->slots_in, "use_shapekey");

  BM_mesh_bm_from_me(bm,
                     me,
                     (&(struct BMeshFromMeshParams){
                         .use_shapekey = set_key,
                         .active_shapekey = ob->shapenr,
                     }));

  if (me->key && ob->shapenr > me->key->totkey) {
    ob->shapenr = me->key->totkey - 1;
  }
}

void bmo_object_load_bmesh_exec(BMesh *bm, BMOperator *op)
{
  Object *ob = BMO_slot_ptr_get(op->slots_in, "object");
  /* Scene *scene = BMO_slot_ptr_get(op, "scene"); */
  Mesh *me = ob->data;

  BMO_op_callf(bm, op->flag, "bmesh_to_mesh mesh=%p object=%p", me, ob);
}

void bmo_bmesh_to_mesh_exec(BMesh *bm, BMOperator *op)
{
  Mesh *me = BMO_slot_ptr_get(op->slots_in, "mesh");
  /* Object *ob = BMO_slot_ptr_get(op, "object"); */

  BM_mesh_bm_to_me(G.main,
                   bm,
                   me,
                   (&(struct BMeshToMeshParams){
                       .calc_object_remap = true,
                   }));
}
