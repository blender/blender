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

#include "MEM_guardedalloc.h"

#include "DNA_meshdata_types.h"

#include "BLI_math.h"
#include "BLI_array.h"

#include "BKE_customdata.h"

#include "bmesh.h"
#include "bmesh_operators_private.h" /* own include */

#define ELE_NEW		1

void bmo_mirror_exec(BMesh *bm, BMOperator *op)
{
	BMOperator dupeop, weldop;
	BMOIter siter;
	BMIter iter;
	BMVert *v, *v2, **vmap = NULL;
	BLI_array_declare(vmap);
	BMEdge /*  *e, */ **emap = NULL;
	BLI_array_declare(emap);
	float mtx[4][4];
	float imtx[4][4];
	float scale[3] = {1.0f, 1.0f, 1.0f};
	float dist = BMO_slot_float_get(op, "mergedist");
	int i, ototvert, ototedge, axis = BMO_slot_int_get(op, "axis");
	int mirroru = BMO_slot_bool_get(op, "mirror_u");
	int mirrorv = BMO_slot_bool_get(op, "mirror_v");

	ototvert = bm->totvert;
	ototedge = bm->totedge;
	
	BMO_slot_mat4_get(op, "mat", mtx);
	invert_m4_m4(imtx, mtx);
	
	BMO_op_initf(bm, &dupeop, "dupe geom=%s", op, "geom");
	BMO_op_exec(bm, &dupeop);
	
	BMO_slot_buffer_flag_enable(bm, &dupeop, "newout", ELE_NEW, BM_ALL);

	/* create old -> new mappin */
	i = 0;
	v2 = BM_iter_new(&iter, bm, BM_VERTS_OF_MESH, NULL);
	BMO_ITER(v, &siter, bm, &dupeop, "newout", BM_VERT) {
		BLI_array_growone(vmap);
		vmap[i] = v;

		/* BMESH_TODO, double check this is being used, calling following operators will overwrite anyway - campbell */
		BM_elem_index_set(v2, i); /* set_dirty! */
		v2 = BM_iter_step(&iter);

		i++;
	}
	bm->elem_index_dirty |= BM_VERT;

	/* feed old data to transform bmo */
	scale[axis] = -1.0f;
	BMO_op_callf(bm, "transform verts=%fv mat=%m4", ELE_NEW, mtx);
	BMO_op_callf(bm, "scale verts=%fv vec=%v", ELE_NEW, scale);
	BMO_op_callf(bm, "transform verts=%fv mat=%m4", ELE_NEW, imtx);
	
	BMO_op_init(bm, &weldop, "weldverts");

	v = BM_iter_new(&iter, bm, BM_VERTS_OF_MESH, NULL);
	for (i = 0; i < ototvert; i++) {
		if (ABS(v->co[axis]) <= dist) {
			BMO_slot_map_ptr_insert(bm, &weldop, "targetmap", vmap[i], v);
		}
		v = BM_iter_step(&iter);
	}
	
	if (mirroru || mirrorv) {
		BMFace *f;
		BMLoop *l;
		MLoopUV *luv;
		int totlayer;
		BMIter liter;

		BMO_ITER(f, &siter, bm, &dupeop, "newout", BM_FACE) {
			BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f) {
				totlayer = CustomData_number_of_layers(&bm->ldata, CD_MLOOPUV);
				for (i = 0; i < totlayer; i++) {
					luv = CustomData_bmesh_get_n(&bm->ldata, l->head.data, CD_MLOOPUV, i);
					if (mirroru)
						luv->uv[0] = 1.0f - luv->uv[0];
					if (mirrorv)
						luv->uv[1] = 1.0f - luv->uv[1];
				}
			}
		}
	}

	BMO_op_exec(bm, &weldop);
	
	BMO_op_finish(bm, &weldop);
	BMO_op_finish(bm, &dupeop);

	BMO_slot_buffer_from_flag(bm, op, "newout", ELE_NEW, BM_ALL);

	BLI_array_free(vmap);
	BLI_array_free(emap);
}
