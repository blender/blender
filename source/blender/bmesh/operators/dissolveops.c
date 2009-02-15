#include "MEM_guardedalloc.h"

#include "BKE_utildefines.h"

#include "bmesh.h"
#include "bmesh_private.h"
#include "BLI_arithb.h"

#include <stdio.h>

#define FACE_MARK	1

#define VERT_MARK	1

void dissolvefaces_exec(BMesh *bmesh, BMOperator *op)
{
	BMO_Flag_Buffer(bmesh, op, BMOP_DISFACES_FACEIN, FACE_MARK);

	/*TODO: need to discuss with Briggs how best to implement this, seems this would be
	  a great time to use the walker api, get it up to snuff.  perhaps have a walker
	  that goes over inner vertices of a contiguously-flagged region?  then you
	  could just use dissolve disk on them.*/
}

void dissolveverts_exec(BMesh *bm, BMOperator *op)
{
	BMOpSlot *vinput;
	BMIter iter, liter, fiter;
	BMVert *v;
	BMFace *f, *f2;
	BMEdge *e;
	BMLoop *l;
	int i, found;
	
	vinput = BMO_GetSlot(op, BMOP_DISVERTS_VERTIN);

	BMO_Flag_Buffer(bm, op, BMOP_DISVERTS_VERTIN, VERT_MARK);

	for (v=BMIter_New(&iter, bm, BM_VERTS, NULL); v; v=BMIter_Step(&iter)) {
		if (BMO_TestFlag(bm, v, VERT_MARK)) {
			BM_Dissolve_Disk(bm, v);
		}
	}

	/*clean up two-edged faces*/
	for (f=BMIter_New(&iter, bm, BM_FACES, NULL); f; f=BMIter_Step(&iter)){
		if (f->len == 2) {
			found = 0;
			l = BMIter_New(&liter, bm, BM_LOOPS_OF_FACE, f);
			for (; l; l=BMIter_Step(&liter)) {
				f2 = BMIter_New(&fiter, bm,
					        BM_FACES_OF_EDGE, l->e);
				for (; f2; f2=BMIter_Step(&fiter)) {
					if (f2 != f) {
						BM_Join_Faces(bm, f, f2, l->e, 
							      1, 0);
						found = 1;
						break;
					}
				}
				if (found) break;
			}
		}
	}
}