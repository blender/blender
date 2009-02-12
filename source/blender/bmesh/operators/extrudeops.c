#include "MEM_guardedalloc.h"

#include "BKE_utildefines.h"

#include "BLI_ghash.h"
#include "BLI_memarena.h"
#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "bmesh.h"
#include "bmesh_operators_private.h"

#define EXT_INPUT	1
#define EXT_KEEP	2

void extrude_edge_context_exec(BMesh *bm, BMOperator *op)
{
	BMOperator splitop;
	BMOIter siter;
	BMIter iter, fiter;
	BMEdge *edge, *newedge;
	BMVert *verts[4];
	BMFace *f;
	int totflagged, rlen;

	/*initialize our sub-operators*/
	BMO_Init_Op(&splitop, BMOP_SPLIT);
	
	BMO_Flag_To_Slot(bm, op, BMOP_EXFACE_EDGEFACEIN, EXT_INPUT, BM_ALL);

	/*calculate geometry to keep*/
	for (edge = BMIter_New(&iter, bm, BM_EDGES, NULL); edge; edge=BMIter_Step(&iter)) {
		f = BMIter_New(&fiter, bm, BM_FACES_OF_EDGE, edge);
		rlen = 0;
		for (; f; f=BMIter_Step(&fiter)) {
			if (BMO_TestFlag(bm, f, EXT_INPUT)) rlen++;
		}

		if (rlen < 2) BMO_SetFlag(bm, edge, EXT_KEEP);
	}

	BMO_CopySlot(op, &splitop, BMOP_EXFACE_EDGEFACEIN, BMOP_SPLIT_MULTIN);
	BMO_Flag_To_Slot(bm, &splitop, BMOP_SPLIT_KEEPIN, EXT_KEEP, BM_ALL);
	
	BMO_Exec_Op(bm, &splitop);

	BMO_CopySlot(&splitop, op, BMOP_SPLIT_MULTOUT, BMOP_EXFACE_MULTOUT);
	
	edge = BMO_IterNew(&siter, bm, &splitop, BMOP_SPLIT_BOUNDS_EDGEMAP);
	for (; edge; edge=BMO_IterStep(&siter)) {
		if (BMO_InMap(bm, op, BMOP_EXFACE_EXCLUDEMAP, edge)) continue;

		newedge = *(BMEdge**)BMO_IterMapVal(&siter);
		verts[0] = edge->v1;
		verts[1] = edge->v2;
		verts[2] = newedge->v2;
		verts[3] = newedge->v1;
		
		//not sure what to do about example face, pass NULL for now.
		f = BM_Make_Quadtriangle(bm, verts, NULL, 4, NULL, 0);		
	}
	
	/*cleanup*/
	BMO_Finish_Op(bm, &splitop);
}
