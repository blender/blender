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

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_math.h"
#include "BLI_array.h"
#include "BLI_utildefines.h"

#include "bmesh.h"
#include "bmesh_operators_private.h" /* own include */

/*
 * MIRROR.C
 *
 * mirror bmop.
 */

#define ELE_NEW		1

void bmesh_mirror_exec(BMesh *bm, BMOperator *op)
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
	float dist = BMO_Get_Float(op, "mergedist");
	int i, ototvert, ototedge, axis = BMO_Get_Int(op, "axis");
	int mirroru = BMO_Get_Int(op, "mirror_u");
	int mirrorv = BMO_Get_Int(op, "mirror_v");

	ototvert = bm->totvert;
	ototedge = bm->totedge;
	
	BMO_Get_Mat4(op, "mat", mtx);
	invert_m4_m4(imtx, mtx);
	
	BMO_InitOpf(bm, &dupeop, "dupe geom=%s", op, "geom");
	BMO_Exec_Op(bm, &dupeop);
	
	BMO_Flag_Buffer(bm, &dupeop, "newout", ELE_NEW, BM_ALL);

	/* create old -> new mappin */
	i = 0;
	v2 = BMIter_New(&iter, bm, BM_VERTS_OF_MESH, NULL);
	BMO_ITER(v, &siter, bm, &dupeop, "newout", BM_VERT) {
		BLI_array_growone(vmap);
		vmap[i] = v;

		/* BMESH_TODO, double check this is being made dirty, 99% sure it is - campbell */
		BM_SetIndex(v2, i); /* set_dirty! */
		v2 = BMIter_Step(&iter);

		i++;
	}
	bm->elem_index_dirty |= BM_VERT;

	/* feed old data to transform bmo */
	scale[axis] = -1.0f;
	BMO_CallOpf(bm, "transform verts=%fv mat=%m4", ELE_NEW, mtx);
	BMO_CallOpf(bm, "scale verts=%fv vec=%v", ELE_NEW, scale);
	BMO_CallOpf(bm, "transform verts=%fv mat=%m4", ELE_NEW, imtx);
	
	BMO_Init_Op(bm, &weldop, "weldverts");

	v = BMIter_New(&iter, bm, BM_VERTS_OF_MESH, NULL);
	for (i = 0; i < ototvert; i++) {
		if (ABS(v->co[axis]) <= dist) {
			BMO_Insert_MapPointer(bm, &weldop, "targetmap", vmap[i], v);
		}
		v = BMIter_Step(&iter);
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

	BMO_Exec_Op(bm, &weldop);
	
	BMO_Finish_Op(bm, &weldop);
	BMO_Finish_Op(bm, &dupeop);

	BMO_Flag_To_Slot(bm, op, "newout", ELE_NEW, BM_ALL);

	BLI_array_free(vmap);
	BLI_array_free(emap);
}
