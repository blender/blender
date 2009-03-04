#include "MEM_guardedalloc.h"

#include "BKE_utildefines.h"

#include "bmesh.h"
#include "mesh_intern.h"
#include "bmesh_private.h"
#include "BLI_arithb.h"

#include <stdio.h>

#define VERT_INPUT	1
#define EDGE_OUT	1

void connectverts_exec(BMesh *bm, BMOperator *op)
{
	BMIter iter, liter;
	BMFace *f, *nf;
	BMEdge *e;
	BMLoop *l;
	BMVert *v1, *v2;
	int ok;
	
	BMO_Flag_Buffer(bm, op, BM_CONVERTS_VERTIN, VERT_INPUT);

	for (f=BMIter_New(&iter, bm, BM_FACES, NULL); f; f=BMIter_Step(&iter)){
		l = BMIter_New(&liter, bm, BM_LOOPS_OF_FACE, f);
		v1 = v2 = NULL;
		ok = 0;
		for (; l; l=BMIter_Step(&liter)) {
			if (BMO_TestFlag(bm, l->v, VERT_INPUT)) {
				if (v1==NULL) v1 = l->v;
				else if (v2==NULL) {
					if (((BMLoop*)l->head.prev)->v != v1
					   && ((BMLoop*)l->head.next)->v!=v1) {
						v2 = l->v;
						ok = 1;
					}
				} else ok = 0;
			}
		}

		if (ok) {
			e = BM_Connect_Verts(bm, v1, v2, &nf);
			if (!e) {
				BMO_RaiseError(bm, op,
					BMERR_CONNECTVERT_FAILED, NULL);
				return;
			}
			BMO_SetFlag(bm, e, EDGE_OUT);
		}
	}

	BMO_Flag_To_Slot(bm, op, BM_CONVERTS_EDGEOUT, EDGE_OUT, BM_EDGE);
}

int BM_ConnectVerts(EditMesh *em, int flag) 
{
	EditMesh *em2;
	BMesh *bm = editmesh_to_bmesh(em);
	BMOperator op;
	
	BMO_Init_Op(&op, BMOP_CONNECT_VERTS);
	BMO_HeaderFlag_To_Slot(bm, &op, BM_CONVERTS_VERTIN, flag, BM_VERT);
	BMO_Exec_Op(bm, &op);
	BMO_Finish_Op(bm, &op);
	
	if (BMO_GetSlot(&op, BM_CONVERTS_EDGEOUT)->len > 0 && 
	    BMO_GetError(bm, NULL, NULL)==0)
	{
		em2 = bmesh_to_editmesh(bm);
		set_editMesh(em, em2);
		MEM_freeN(em2);

		return 1;
	}

	return 0;
}