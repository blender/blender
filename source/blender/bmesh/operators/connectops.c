#include "MEM_guardedalloc.h"

#include "BKE_utildefines.h"

#include "bmesh.h"
#include "mesh_intern.h"
#include "bmesh_private.h"
#include "BLI_arithb.h"

#include <stdio.h>
#include <string.h>

#define VERT_INPUT	1
#define EDGE_OUT	1
#define FACE_NEW	2

void connectverts_exec(BMesh *bm, BMOperator *op)
{
	BMIter iter, liter;
	BMFace *f, *nf;
	BMLoop **loops = NULL, *lastl = NULL;
	V_DECLARE(loops);
	BMLoop *l, *nl;
	BMVert *v1, *v2, **verts = NULL;
	V_DECLARE(verts);
	int i;
	
	BMO_Flag_Buffer(bm, op, BM_CONVERTS_VERTIN, VERT_INPUT);

	for (f=BMIter_New(&iter, bm, BM_FACES, NULL); f; f=BMIter_Step(&iter)){
		V_RESET(loops);
		V_RESET(verts);
		
		if (BMO_TestFlag(bm, f, FACE_NEW)) continue;

		l = BMIter_New(&liter, bm, BM_LOOPS_OF_FACE, f);
		v1 = v2 = NULL;
		lastl = NULL;
		for (; l; l=BMIter_Step(&liter)) {
			if (BMO_TestFlag(bm, l->v, VERT_INPUT)) {
				if (!lastl) {
					lastl = l;
					continue;
				}

				if (lastl != l->head.prev && lastl != 
				    l->head.next)
				{
					V_GROW(loops);
					loops[V_COUNT(loops)-1] = lastl;

					V_GROW(loops);
					loops[V_COUNT(loops)-1] = l;

					lastl = l;
				}
			}
		}

		if (V_COUNT(loops) == 0) continue;
		
		if (V_COUNT(loops) > 2) {
			V_GROW(loops);
			loops[V_COUNT(loops)-1] = loops[V_COUNT(loops)-2];

			V_GROW(loops);
			loops[V_COUNT(loops)-1] = loops[0];
		}

		BM_LegalSplits(bm, f, loops, V_COUNT(loops)/2);
		
		for (i=0; i<V_COUNT(loops)/2; i++) {
			if (loops[i*2]==NULL) continue;

			V_GROW(verts);
			verts[V_COUNT(verts)-1] = loops[i*2]->v;
		
			V_GROW(verts);
			verts[V_COUNT(verts)-1] = loops[i*2+1]->v;
		}

		for (i=0; i<V_COUNT(verts)/2; i++) {
			nf = BM_Split_Face(bm, f, verts[i*2],
				           verts[i*2+1], &nl, NULL);
			f = nf;
			
			if (!nl || !nf) {
				BMO_RaiseError(bm, op,
					BMERR_CONNECTVERT_FAILED, NULL);
				V_FREE(loops);
				return;;;
			}
			BMO_SetFlag(bm, nf, FACE_NEW);
			BMO_SetFlag(bm, nl->e, EDGE_OUT);
		}
	}

	BMO_Flag_To_Slot(bm, op, BM_CONVERTS_EDGEOUT, EDGE_OUT, BM_EDGE);

	V_FREE(loops);
	V_FREE(verts);
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