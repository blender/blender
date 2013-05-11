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

/** \file blender/bmesh/operators/bmo_connect.c
 *  \ingroup bmesh
 *
 * Connect verts across faces (splits faces) and bridge tool.
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_array.h"
#include "BLI_utildefines.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */

#define VERT_INPUT	1
#define EDGE_OUT	1
#define FACE_NEW	2

void bmo_connect_verts_exec(BMesh *bm, BMOperator *op)
{
	BMIter iter, liter;
	BMFace *f, *f_new;
	BMLoop *(*loops_split)[2] = NULL;
	BLI_array_declare(loops_split);
	BMLoop *l, *l_new;
	BMVert *(*verts_pair)[2] = NULL;
	BLI_array_declare(verts_pair);
	int i;
	
	BMO_slot_buffer_flag_enable(bm, op->slots_in, "verts", BM_VERT, VERT_INPUT);

	/* BMESH_TODO, loop over vert faces:
	 * faster then looping over all faces, then searching each for flagged verts*/
	BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
		BMLoop *l_last;
		BLI_array_empty(loops_split);
		BLI_array_empty(verts_pair);
		
		if (BMO_elem_flag_test(bm, f, FACE_NEW)) {
			continue;
		}

		l_last = NULL;
		BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
			if (BMO_elem_flag_test(bm, l->v, VERT_INPUT)) {
				if (!l_last) {
					l_last = l;
					continue;
				}

				if (l_last != l->prev && l_last != l->next) {
					BLI_array_grow_one(loops_split);
					loops_split[BLI_array_count(loops_split) - 1][0] = l_last;
					loops_split[BLI_array_count(loops_split) - 1][1] = l;

				}
				l_last = l;
			}
		}

		if (BLI_array_count(loops_split) == 0) {
			continue;
		}
		
		if (BLI_array_count(loops_split) > 1) {
			BLI_array_grow_one(loops_split);
			loops_split[BLI_array_count(loops_split) - 1][0] = loops_split[BLI_array_count(loops_split) - 2][1];
			loops_split[BLI_array_count(loops_split) - 1][1] = loops_split[0][0];
		}

		BM_face_legal_splits(bm, f, loops_split, BLI_array_count(loops_split));
		
		for (i = 0; i < BLI_array_count(loops_split); i++) {
			if (loops_split[i][0] == NULL) {
				continue;
			}

			BLI_array_grow_one(verts_pair);
			verts_pair[BLI_array_count(verts_pair) - 1][0] = loops_split[i][0]->v;
			verts_pair[BLI_array_count(verts_pair) - 1][1] = loops_split[i][1]->v;
		}

		for (i = 0; i < BLI_array_count(verts_pair); i++) {
			f_new = BM_face_split(bm, f, verts_pair[i][0], verts_pair[i][1], &l_new, NULL, false);
			f = f_new;
			
			if (!l_new || !f_new) {
				BMO_error_raise(bm, op, BMERR_CONNECTVERT_FAILED, NULL);
				BLI_array_free(loops_split);
				return;
			}
			BMO_elem_flag_enable(bm, f_new, FACE_NEW);
			BMO_elem_flag_enable(bm, l_new->e, EDGE_OUT);
		}
	}

	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "edges.out", BM_EDGE, EDGE_OUT);

	BLI_array_free(loops_split);
	BLI_array_free(verts_pair);
}
