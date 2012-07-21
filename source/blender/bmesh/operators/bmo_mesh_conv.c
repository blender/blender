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

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_key_types.h"
#include "DNA_modifier_types.h"

#include "BKE_mesh.h"
#include "BLI_listbase.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_main.h"
#include "BKE_customdata.h"

#include "BLI_math.h"
#include "BLI_array.h"

#include "bmesh.h"
#include "intern/bmesh_private.h"

#include "intern/bmesh_operators_private.h" /* own include */

void bmo_mesh_to_bmesh_exec(BMesh *bm, BMOperator *op)
{
	Object *ob = BMO_slot_ptr_get(op, "object");
	Mesh *me = BMO_slot_ptr_get(op, "mesh");
	int set_key = BMO_slot_bool_get(op, "set_shapekey");

	BM_mesh_bm_from_me(bm, me, set_key, ob->shapenr);

	if (me->key && ob->shapenr > me->key->totkey) {
		ob->shapenr = me->key->totkey - 1;
	}
}

void bmo_object_load_bmesh_exec(BMesh *bm, BMOperator *op)
{
	Object *ob = BMO_slot_ptr_get(op, "object");
	/* Scene *scene = BMO_slot_ptr_get(op, "scene"); */
	Mesh *me = ob->data;

	BMO_op_callf(bm, op->flag,
	             "bmesh_to_mesh mesh=%p object=%p notessellation=%b",
	             me, ob, TRUE);
}

void bmo_bmesh_to_mesh_exec(BMesh *bm, BMOperator *op)
{
	Mesh *me = BMO_slot_ptr_get(op, "mesh");
	/* Object *ob = BMO_slot_ptr_get(op, "object"); */
	int dotess = !BMO_slot_bool_get(op, "notessellation");

	BM_mesh_bm_to_me(bm, me, dotess);
}
