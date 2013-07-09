/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * Contributor(s): Joseph Eagar.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/operators/bmo_mesh_conv.c
 *  \ingroup bmesh
 *
 * This file contains functions
 * for converting a Mesh
 * into a Bmesh, and back again.
 */


#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_key_types.h"

#include "BLI_math.h"

#include "bmesh.h"
#include "intern/bmesh_operators_private.h"


void bmo_mesh_to_bmesh_exec(BMesh *bm, BMOperator *op)
{
	Object *ob   = BMO_slot_ptr_get(op->slots_in,  "object");
	Mesh *me     = BMO_slot_ptr_get(op->slots_in,  "mesh");
	bool set_key = BMO_slot_bool_get(op->slots_in, "use_shapekey");

	BM_mesh_bm_from_me(bm, me, false, set_key, ob->shapenr);

	if (me->key && ob->shapenr > me->key->totkey) {
		ob->shapenr = me->key->totkey - 1;
	}
}

void bmo_object_load_bmesh_exec(BMesh *bm, BMOperator *op)
{
	Object *ob = BMO_slot_ptr_get(op->slots_in, "object");
	/* Scene *scene = BMO_slot_ptr_get(op, "scene"); */
	Mesh *me = ob->data;

	BMO_op_callf(bm, op->flag,
	             "bmesh_to_mesh mesh=%p object=%p skip_tessface=%b",
	             me, ob, true);
}

void bmo_bmesh_to_mesh_exec(BMesh *bm, BMOperator *op)
{
	Mesh *me = BMO_slot_ptr_get(op->slots_in, "mesh");
	/* Object *ob = BMO_slot_ptr_get(op, "object"); */
	const bool dotess = !BMO_slot_bool_get(op->slots_in, "skip_tessface");

	BM_mesh_bm_to_me(bm, me, dotess);
}
