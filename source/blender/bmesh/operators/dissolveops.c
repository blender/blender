#include "MEM_guardedalloc.h"

#include "BKE_utildefines.h"

#include "bmesh.h"
#include "bmesh_private.h"
#include "BLI_arithb.h"

#include <stdio.h>

#define FACE_MARK	1
void dissolvefaces_exec(BMesh *bmesh, BMOperator *op)
{
	BMOpSlot *finput;
	BMFace *face;
	float projectverts[400][3];
	void *projverts;
	int i, count = 0;
	
	BMO_Flag_Buffer(bmesh, op, BMOP_DISFACES_FACEIN, FACE_MARK);

	/*TODO: need to discuss with Briggs how best to implement this, seems this would be
	  a great time to use the walker api, get it up to snuff.  perhaps have a walker
	  that goes over inner vertices of a contiguously-flagged region?  then you
	  could just use dissolve disk on them.*/
}

void dissolveverts_exec(BMesh *bmesh, BMOperator *op)
{
	BMOpSlot *vinput;
	BMVert *vert;
	int i;
	
	vinput = BMO_GetSlot(op, BMOP_DISVERTS_VERTIN);

	/*
	BMO_Flag_Buffer(bmesh, op, BMOP_DISVERTS_VERTIN, VERT_MARK);

	for (vert=BMIter_New(&iter, bmesh, BM_VERTS, NULL); vert; vert=BMIter_Step(&iter)) {
		if (BMO_TestFlag(bmesh, vert, VERT_MARK)) {
			BM_Dissolve_Disk(bmesh, vert);
		}
	}
	*/

	for (i=0; i<vinput->len; i++) {
		vert = ((BMVert**)vinput->data.p)[i];
		BM_Dissolve_Disk(bmesh, vert);
	}
}