#include "MEM_guardedalloc.h"

#include "BKE_utildefines.h"

#include "bmesh.h"
#include "bmesh_private.h"
#include "BLI_math.h"
#include "BLI_array.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EDGE_NEW	1
#define FACE_NEW	1

void triangulate_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMFace *face, **newfaces = NULL;
	BLI_array_declare(newfaces);
	float (*projectverts)[3] = NULL;
	BLI_array_declare(projectverts);
	int i, lastlen=0, count = 0;
	
	face = BMO_IterNew(&siter, bm, op, "faces", BM_FACE);
	for (; face; face=BMO_IterStep(&siter)) {
		if (lastlen < face->len) {
			BLI_array_empty(projectverts);
			BLI_array_empty(newfaces);
			for (lastlen=0; lastlen<face->len; lastlen++) {
				BLI_array_growone(projectverts);
				BLI_array_growone(projectverts);
				BLI_array_growone(projectverts);
				BLI_array_growone(newfaces);
			}
		}
		
		BM_Triangulate_Face(bm, face, projectverts, EDGE_NEW, 
		                    FACE_NEW, newfaces);

		BMO_Insert_MapPointer(bm, op, "facemap", 
	                              face, face);
		for (i=0; newfaces[i]; i++) {
			BMO_Insert_MapPointer(bm, op, "facemap", 
				              newfaces[i], face);

		}
	}
	
	BMO_Flag_To_Slot(bm, op, "edgeout", EDGE_NEW, BM_EDGE);
	BMO_Flag_To_Slot(bm, op, "faceout", FACE_NEW, BM_FACE);
	
	BLI_array_free(projectverts);
	BLI_array_free(newfaces);
}