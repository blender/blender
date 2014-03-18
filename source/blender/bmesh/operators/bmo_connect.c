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
 * Connect verts across faces (splits faces).
 */

#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_alloca.h"
#include "BLI_linklist_stack.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */

#define VERT_INPUT	1
#define EDGE_OUT	1
#define FACE_TAG	2

static int bm_face_connect_verts(BMesh *bm, BMFace *f)
{
	BMLoop *(*loops_split)[2] = BLI_array_alloca(loops_split, f->len);
	STACK_DECLARE(loops_split);
	BMVert *(*verts_pair)[2] = BLI_array_alloca(verts_pair, f->len);
	STACK_DECLARE(verts_pair);

	BMIter liter;
	BMFace *f_new;
	BMLoop *l;
	BMLoop *l_last;
	unsigned int i;

	STACK_INIT(loops_split);
	STACK_INIT(verts_pair);

	l_last = NULL;
	BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
		if (BMO_elem_flag_test(bm, l->v, VERT_INPUT)) {
			if (!l_last) {
				l_last = l;
				continue;
			}

			if (!BM_loop_is_adjacent(l_last, l)) {
				BMLoop **l_pair = STACK_PUSH_RET(loops_split);
				l_pair[0] = l_last;
				l_pair[1] = l;
			}
			l_last = l;
		}
	}

	if (STACK_SIZE(loops_split) == 0) {
		return 0;
	}

	if (STACK_SIZE(loops_split) > 1) {
		BMLoop **l_pair = STACK_PUSH_RET(loops_split);
		l_pair[0] = loops_split[STACK_SIZE(loops_split) - 2][1];
		l_pair[1] = loops_split[0][0];
	}

	BM_face_legal_splits(f, loops_split, STACK_SIZE(loops_split));

	for (i = 0; i < STACK_SIZE(loops_split); i++) {
		BMVert **v_pair;
		if (loops_split[i][0] == NULL) {
			continue;
		}

		v_pair = STACK_PUSH_RET(verts_pair);
		v_pair[0] = loops_split[i][0]->v;
		v_pair[1] = loops_split[i][1]->v;
	}

	for (i = 0; i < STACK_SIZE(verts_pair); i++) {
		BMLoop *l_new;
		BMLoop *l_a, *l_b;

		if ((l_a = BM_face_vert_share_loop(f, verts_pair[i][0])) &&
		    (l_b = BM_face_vert_share_loop(f, verts_pair[i][1])))
		{
			f_new = BM_face_split(bm, f, l_a, l_b, &l_new, NULL, false);
		}
		else {
			f_new = NULL;
		}

		f = f_new;

		if (!l_new || !f_new) {
			return -1;
		}
		// BMO_elem_flag_enable(bm, f_new, FACE_NEW);
		BMO_elem_flag_enable(bm, l_new->e, EDGE_OUT);
	}

	return 1;
}


void bmo_connect_verts_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMIter iter;
	BMVert *v;
	BMFace *f;
	BLI_LINKSTACK_DECLARE(faces, BMFace *);

	BLI_LINKSTACK_INIT(faces);

	/* add all faces connected to verts */
	BMO_ITER (v, &siter, op->slots_in, "verts", BM_VERT) {
		BMO_elem_flag_enable(bm, v, VERT_INPUT);
		BM_ITER_ELEM (f, &iter, v, BM_FACES_OF_VERT) {
			if (!BMO_elem_flag_test(bm, f, FACE_TAG)) {
				BMO_elem_flag_enable(bm, f, FACE_TAG);
				if (f->len > 3) {
					BLI_LINKSTACK_PUSH(faces, f);
				}
			}
		}
	}

	/* connect faces */
	while ((f = BLI_LINKSTACK_POP(faces))) {
		if (bm_face_connect_verts(bm, f) == -1) {
			BMO_error_raise(bm, op, BMERR_CONNECTVERT_FAILED, NULL);
		}
	}

	BLI_LINKSTACK_FREE(faces);

	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "edges.out", BM_EDGE, EDGE_OUT);
}
