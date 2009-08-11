#include "MEM_guardedalloc.h"
#include "BKE_customdata.h" 
#include "DNA_listBase.h"
#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include <string.h>
#include "BKE_utildefines.h"
#include "BKE_mesh.h"
#include "BKE_global.h"
#include "BKE_DerivedMesh.h"
#include "BKE_cdderivedmesh.h"

#include "BLI_editVert.h"
#include "mesh_intern.h"
#include "ED_mesh.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_edgehash.h"

#include "bmesh.h"

/*
 * MIRROR.C
 *
 * mirror bmop.
 *
*/

#define ELE_NEW		1

void bmesh_mirror_exec(BMesh *bm, BMOperator *op) {
	BMOperator dupeop, weldop;
	BMOIter siter;
	BMIter iter;
	BMVert *v, *v2, **vmap = NULL;
	V_DECLARE(vmap);
	BMEdge *e, **emap = NULL;
	V_DECLARE(emap);
	float mtx[4][4];
	float imtx[4][4];
	float dist = BMO_Get_Float(op, "mergedist");
	int i, ototvert, ototedge, axis = BMO_Get_Int(op, "axis");
	int mirroru = BMO_Get_Int(op, "mirror_u");
	int mirrorv = BMO_Get_Int(op, "mirror_v");

	ototvert = bm->totvert;
	ototedge = bm->totedge;
	
	BMO_Get_Mat4(op, "mat", mtx);
	Mat4Invert(imtx, mtx);
	
	BMO_InitOpf(bm, &dupeop, "dupe geom=%s", op, "geom");
	BMO_Exec_Op(bm, &dupeop);
	
	BMO_Flag_Buffer(bm, &dupeop, "newout", ELE_NEW, BM_ALL);

	/*create old -> new mapping*/
	i = 0;
	v2 = BMIter_New(&iter, bm, BM_VERTS_OF_MESH, NULL);
	BMO_ITER(v, &siter, bm, &dupeop, "newout", BM_VERT) {
		V_GROW(vmap);
		vmap[i] = v;

		BMINDEX_SET(v2, i);
		v2 = BMIter_Step(&iter);

		i += 1;
	}

	/*feed old data to transform bmop*/
	BMO_CallOpf(bm, "transform verts=%fv mat=%m4", ELE_NEW, mtx);
	
	BMO_Init_Op(&weldop, "weldverts");

	v = BMIter_New(&iter, bm, BM_VERTS_OF_MESH, NULL);
	for (i=0; i<ototvert; i++) {
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
				for (i=0; i<totlayer; i++) {
					luv = CustomData_bmesh_get_n(&bm->ldata, l->head.data, CD_MLOOPUV, i);
					if (mirroru)
						luv->uv[0] = 1.0 - luv->uv[0];
					if (mirrorv)
						luv->uv[1] = 1.0 - luv->uv[1];
				}
			}
		}
	}

	BMO_Exec_Op(bm, &weldop);
	
	BMO_Finish_Op(bm, &weldop);
	BMO_Finish_Op(bm, &dupeop);

	BMO_Flag_To_Slot(bm, op, "newout", ELE_NEW, BM_ALL);

	V_FREE(vmap);
	V_FREE(emap);
}

