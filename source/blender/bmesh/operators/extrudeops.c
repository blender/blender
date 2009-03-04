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
#define EXT_DEL		4

void extrude_edge_context_exec(BMesh *bm, BMOperator *op)
{
	BMOperator dupeop, delop;
	BMOIter siter;
	BMIter iter, fiter, viter;
	BMEdge *e, *newedge, *e2;
	BMLoop *l, *l2;
	BMVert *verts[4], *v;
	BMFace *f;
	int rlen, found, delorig=0, i;

	/*initialize our sub-operators*/
	BMO_Init_Op(&dupeop, BMOP_DUPE);
	
	BMO_Flag_Buffer(bm, op, BMOP_EXFACE_EDGEFACEIN, EXT_INPUT);

#if 1
	for (e=BMIter_New(&iter, bm, BM_EDGES, NULL);e;e=BMIter_Step(&iter)) {
		if (!BMO_TestFlag(bm, e, EXT_INPUT)) continue;

		found = 0;
		f = BMIter_New(&fiter, bm, BM_FACES_OF_EDGE, e);
		for (rlen=0; f; f=BMIter_Step(&fiter), rlen++) {
			if (!BMO_TestFlag(bm, f, EXT_INPUT)) {
				found = 1;
				delorig = 1;
				break;
			}
		}
		
		if (!found && (rlen > 1)) BMO_SetFlag(bm, e, EXT_DEL);
	}

	/*calculate verts to delete*/
	for (v = BMIter_New(&iter, bm, BM_VERTS, NULL); v; v=BMIter_Step(&iter)) {
		found = 0;

		l = BMIter_New(&viter, bm, BM_LOOPS_OF_VERT, v);
		for (; l; l=BMIter_Step(&viter)) {
			if (!BMO_TestFlag(bm, l->e, EXT_INPUT)) {
				found = 1;
				break;
			}
			if (!BMO_TestFlag(bm, l->f, EXT_INPUT)) {
				found = 1;
				break;
			}

			if (found) break;
		}

		if (!found) {
			BMO_SetFlag(bm, v, EXT_DEL);
		}
	}
	
	for (f=BMIter_New(&fiter, bm, BM_FACES, NULL); f; f=BMIter_Step(&fiter)) {
		if (BMO_TestFlag(bm, f, EXT_INPUT))
			BMO_SetFlag(bm, f, EXT_DEL);
	}
#endif
	//if (delorig) BMO_Flag_To_Slot(bm, &delop, BMOP_DEL_MULTIN, EXT_DEL, BM_ALL);
	//BMO_Set_Int(&delop, BMOP_DEL_CONTEXT, DEL_ONLYTAGGED);
	
	if (delorig) BMO_InitOpf(bm, &delop, "del geom=%fvef context=%d", 
	                         EXT_DEL, DEL_ONLYTAGGED);
	else BMO_InitOpf(bm, &delop, "del context=%d", DEL_ONLYTAGGED);

	BMO_CopySlot(op, &dupeop, BMOP_EXFACE_EDGEFACEIN, BMOP_DUPE_MULTIN);
	
	BMO_Exec_Op(bm, &dupeop);
	if (delorig) BMO_Exec_Op(bm, &delop);

	BMO_CopySlot(&dupeop, op, BMOP_DUPE_NEW, BMOP_EXFACE_MULTOUT);
	e = BMO_IterNew(&siter, bm, &dupeop, BMOP_DUPE_BOUNDS_EDGEMAP);
	for (; e; e=BMO_IterStep(&siter)) {
		if (BMO_InMap(bm, op, BMOP_EXFACE_EXCLUDEMAP, e)) continue;

		newedge = BMO_IterMapVal(&siter);
		if (!newedge) continue;
		newedge = *(BMEdge**)newedge;

		verts[0] = e->v1;
		verts[1] = e->v2;
		verts[2] = newedge->v2;
		verts[3] = newedge->v1;
		
		//not sure what to do about example face, pass NULL for now.
		f = BM_Make_Quadtriangle(bm, verts, NULL, 4, NULL, 0);		

		/*copy attributes*/
		l=BMIter_New(&iter, bm, BM_LOOPS_OF_FACE, f);
		for (; l; l=BMIter_Step(&iter)) {
			l2 = l->radial.next->data;
			
			if (l2 && l2 != l) {
				/*copy data*/
				if (l2->v == l->v) {
					BM_Copy_Attributes(bm, bm, l2, l);
					l2 = (BMLoop*) l2->head.next;
					l = (BMLoop*) l->head.next;
					BM_Copy_Attributes(bm, bm, l2, l);
				} else {
					l2 = (BMLoop*) l2->head.next;
					BM_Copy_Attributes(bm, bm, l2, l);
					l2 = (BMLoop*) l2->head.prev;
					l = (BMLoop*) l->head.next;
					BM_Copy_Attributes(bm, bm, l2, l);
				}
			}
		}
	}

	
	/*cleanup*/
	BMO_Finish_Op(bm, &delop);
	BMO_Finish_Op(bm, &dupeop);
}
