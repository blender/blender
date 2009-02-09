#include "MEM_guardedalloc.h"

#include "BKE_utildefines.h"

#include "bmesh.h"
#include "bmesh_private.h"
#include "BLI_arithb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EDGE_NEW	1
#define FACE_NEW	1

void triangulate_exec(BMesh *bmesh, BMOperator *op)
{
	BMOpSlot *finput;
	BMFace *face;
	float (*projectverts)[3] = NULL;
	V_DECLARE(projectverts);
	int i, lastlen=0, count = 0;
	
	finput = BMO_GetSlot(op, BMOP_ESUBDIVIDE_EDGES);

	for (i=0; i<finput->len; i++) {
		face = ((BMFace**)finput->data.p)[i];

		if (lastlen < face->len) {
			V_RESET(projectverts);
			for (lastlen=0; lastlen<face->len; lastlen++) {
				V_GROW(projectverts);
				V_GROW(projectverts);
				V_GROW(projectverts);
			}
		}
		
		BM_Triangulate_Face(bmesh, face, projectverts, EDGE_NEW, FACE_NEW);
	}
	
	BMO_Flag_To_Slot(bmesh, op, BMOP_TRIANG_NEW_EDGES, EDGE_NEW, BM_EDGE);
	BMO_Flag_To_Slot(bmesh, op, BMOP_TRIANG_NEW_FACES, FACE_NEW, BM_FACE);
}