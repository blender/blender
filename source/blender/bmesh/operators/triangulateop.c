#include "MEM_guardedalloc.h"

#include "BKE_utildefines.h"

#include "bmesh.h"
#include "bmesh_private.h"
#include "BLI_arithb.h"

#include <stdio.h>

#define EDGE_NEW	1
#define FACE_NEW	1
void triangulate_exec(BMesh *bmesh, BMOperator *op)
{
	BMOpSlot *finput;
	BMFace *face;
	float projectverts[400][3];
	void *projverts;
	int i, count = 0;
	
	finput = BMO_GetSlot(op, BMOP_ESUBDIVIDE_EDGES);

	for (i=0; i<finput->len; i++) {
		face = ((BMFace**)finput->data.p)[i];

		/*HACK! need to discuss with Briggs why the function takes an 
		  externally-allocated array of vert coordinates in the first place.*/
		if (face->len > 400) projverts = MEM_callocN(sizeof(float)*3*face->len, "projverts");
		else projverts = projectverts;
		
		BM_Triangulate_Face(bmesh, face, projectverts, EDGE_NEW, FACE_NEW);
		
		if (projverts != projectverts) MEM_freeN(projverts);
	}
	
	BMO_Flag_To_Slot(bmesh, op, BMOP_NEW_EDGES, EDGE_NEW, BM_EDGE);
	BMO_Flag_To_Slot(bmesh, op, BMOP_NEW_FACES, FACE_NEW, BM_FACE);
}