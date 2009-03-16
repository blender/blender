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

void triangulate_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMFace *face, **newfaces = NULL;
	V_DECLARE(newfaces);
	float (*projectverts)[3] = NULL;
	V_DECLARE(projectverts);
	int i, lastlen=0, count = 0;
	
	face = BMO_IterNew(&siter, bm, op, BMOP_TRIANG_FACEIN);
	for (; face; face=BMO_IterStep(&siter)) {
		if (lastlen < face->len) {
			V_RESET(projectverts);
			V_RESET(newfaces);
			for (lastlen=0; lastlen<face->len; lastlen++) {
				V_GROW(projectverts);
				V_GROW(projectverts);
				V_GROW(projectverts);
				V_GROW(newfaces);
			}
		}
		
		BM_Triangulate_Face(bm, face, projectverts, EDGE_NEW, 
		                    FACE_NEW, newfaces);

		BMO_Insert_MapPointer(bm, op, BMOP_TRIANG_FACEMAP, 
	                              face, face);
		for (i=0; newfaces[i]; i++) {
			BMO_Insert_MapPointer(bm, op, BMOP_TRIANG_FACEMAP, 
				              newfaces[i], face);

		}
	}
	
	BMO_Flag_To_Slot(bm, op, BMOP_TRIANG_NEW_EDGES, EDGE_NEW, BM_EDGE);
	BMO_Flag_To_Slot(bm, op, BMOP_TRIANG_NEW_FACES, FACE_NEW, BM_FACE);
	
	V_FREE(projectverts);
}