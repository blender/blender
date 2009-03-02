#include "MEM_guardedalloc.h"

#include "BKE_utildefines.h"

#include "bmesh.h"
#include "mesh_intern.h"
#include "bmesh_private.h"
#include "BLI_arithb.h"

#include <stdio.h>

#define FACE_MARK	1
#define VERT_MARK	1

void dissolvefaces_exec(BMesh *bm, BMOperator *op)
{
	BMOIter iter;
	BMIter liter;
	BMLoop *l;
	BMFace *f, *f2, *nf = NULL;

	//BMO_Flag_Buffer(bm, op, BMOP_DISFACES_FACEIN, FACE_MARK);

	/*TODO: need to discuss with Briggs how best to implement this, seems this would be
	  a great time to use the walker api, get it up to snuff.  perhaps have a walker
	  that goes over inner vertices of a contiguously-flagged region?  then you
	  could just use dissolve disk on them.*/
	if (BMO_GetSlot(op, BMOP_DISFACES_FACEIN)->len != 2) return;

	/*HACK: for debugging purposes, handle cases of two adjacent faces*/
	f = BMO_IterNew(&iter, bm, op, BMOP_DISFACES_FACEIN);
	f2 = BMO_IterStep(&iter);

	for (l=BMIter_New(&liter, bm, BM_LOOPS_OF_FACE, f);l;l=BMIter_Step(&liter)) {
		if (!l->radial.next) continue;
		if (((BMLoop*)l->radial.next->data)->f == f2) {
			nf = BM_Join_Faces(bm, f, f2, l->e, 1, 0);
			break;
		}
	}

	if (nf) {
		BMO_SetFlag(bm, nf, 1);
		BMO_Flag_To_Slot(bm, op, BMOP_DISFACES_REGIONOUT, 1, BM_FACE);
	}

}

/*returns 1 if any faces were dissolved*/
int BM_DissolveFaces(EditMesh *em, int flag) {
	BMesh *bm = editmesh_to_bmesh(em);
	EditMesh *em2;
	BMOperator op;

	BMO_Init_Op(&op, BMOP_DISSOLVE_FACES);
	BMO_HeaderFlag_To_Slot(bm, &op, BMOP_DISFACES_FACEIN, flag, BM_FACE);
	BMO_Exec_Op(bm, &op);
	BMO_Finish_Op(bm, &op);
	
	em2 = bmesh_to_editmesh(bm);
	set_editMesh(em, em2);
	MEM_freeN(em2);

	return BMO_GetSlot(&op, BMOP_DISFACES_REGIONOUT)->len > 0;
}

void dissolveverts_exec(BMesh *bm, BMOperator *op)
{
	BMOpSlot *vinput;
	BMIter iter, liter, fiter;
	BMVert *v;
	BMFace *f, *f2;
	BMEdge *fe;
	BMLoop *l;
	int found, found2, found3, len, oldlen=0;
	
	vinput = BMO_GetSlot(op, BMOP_DISVERTS_VERTIN);

	BMO_Flag_Buffer(bm, op, BMOP_DISVERTS_VERTIN, VERT_MARK);
	
	found = 1;
	while (found) {
		found = 0;
		len = 0;
		for (v=BMIter_New(&iter, bm, BM_VERTS, NULL); v; v=BMIter_Step(&iter)) {
			if (BMO_TestFlag(bm, v, VERT_MARK)) {
				if (!BM_Dissolve_Vert(bm, v)) {
					BMO_RaiseError(bm, op,
					      BMERR_DISSOLVEDISK_FAILED, NULL);
					return;
				}
				found = 1;
				len++;
			}
		}


		/*clean up two-edged faces*/
		/*basic idea is to keep joining 2-edged faces until their
		  gone.  this however relies on joining two 2-edged faces
		  together to work, which doesn't.*/
		found3 = 1;
		while (found3) {
			found3 = 0;
			for (f=BMIter_New(&iter, bm, BM_FACES, NULL); f; f=BMIter_Step(&iter)){
				if (!BM_Validate_Face(bm, f, stderr)) {
					printf("error.\n");
				}

				if (f->len == 2) {
					//this design relies on join faces working
					//with two-edged faces properly.
					//commenting this line disables the
					//outermost loop.
					//found3 = 1;
					found2 = 0;
					l = BMIter_New(&liter, bm, BM_LOOPS_OF_FACE, f);
					fe = l->e;
					for (; l; l=BMIter_Step(&liter)) {
						f2 = BMIter_New(&fiter, bm,
								BM_FACES_OF_EDGE, l->e);
						for (; f2; f2=BMIter_Step(&fiter)) {
							if (f2 != f) {
								BM_Join_Faces(bm, f, f2, l->e, 
									      1, 0);
								found2 = 1;
								break;
							}
						}
						if (found2) break;
					}

					if (!found2) {
						bmesh_kf(bm, f);
						bmesh_ke(bm, fe);
					}
				} /*else if (f->len == 3) {
					BMEdge *ed[3];
					BMVert *vt[3];
					BMLoop *lp[3];
					int i=0;

					//check for duplicate edges
					l = BMIter_New(&liter, bm, BM_LOOPS_OF_FACE, f);
					for (; l; l=BMIter_Step(&liter)) {
						ed[i] = l->e;	
						lp[i] = l;
						vt[i++] = l->v;
					}
					if (vt[0] == vt[1] || vt[0] == vt[2]) {
						i += 1;
					}
				}*/
			}
		}
		if (oldlen == len) break;
		oldlen = len;
	}

}