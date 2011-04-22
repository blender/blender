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

#define ELE_NEW		1
#define FACE_MARK	2
#define EDGE_MARK	4

void triangulate_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMFace *face, **newfaces = NULL;
	BLI_array_declare(newfaces);
	float (*projectverts)[3] = NULL;
	BLI_array_declare(projectverts);
	int i, lastlen=0 /* , count = 0 */;
	
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

void bmesh_beautify_fill_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMIter iter;
	BMFace *f;
	BMEdge *e;
	int stop=0;
	
	BMO_Flag_Buffer(bm, op, "constrain_edges", EDGE_MARK, BM_EDGE);
	
	BMO_ITER(f, &siter, bm, op, "faces", BM_FACE) {
		if (f->len == 3)
			BMO_SetFlag(bm, f, FACE_MARK);
	}

	while (!stop) {
		stop = 1;
		
		BM_ITER(e, &iter, bm, BM_EDGES_OF_MESH, NULL) {
			BMVert *v1, *v2, *v3, *v4, *d1, *d2;
			float len1, len2, len3, len4, len5, len6, opp1, opp2, fac1, fac2;
			int ok;
			
			if (BM_Edge_FaceCount(e) != 2 || BMO_TestFlag(bm, e, EDGE_MARK))
				continue;
			if (!BMO_TestFlag(bm, e->l->f, FACE_MARK) || !BMO_TestFlag(bm, e->l->radial_next->f, FACE_MARK))
				continue;
			
			v1 = e->l->prev->v;
			v2 = e->l->v;
			v3 = e->l->radial_next->prev->v;
			v4 = e->l->next->v;
			
			if (is_quad_convex_v3(v1->co, v2->co, v3->co, v4->co)) {
				/* testing rule:
				* the area divided by the total edge lengths
				*/
				len1= len_v3v3(v1->co, v2->co);
				len2= len_v3v3(v2->co, v3->co);
				len3= len_v3v3(v3->co, v4->co);
				len4= len_v3v3(v4->co, v1->co);
				len5= len_v3v3(v1->co, v3->co);
				len6= len_v3v3(v2->co, v4->co);
	
				opp1= area_tri_v3(v1->co, v2->co, v3->co);
				opp2= area_tri_v3(v1->co, v3->co, v4->co);
	
				fac1= opp1/(len1+len2+len5) + opp2/(len3+len4+len5);
	
				opp1= area_tri_v3(v2->co, v3->co, v4->co);
				opp2= area_tri_v3(v2->co, v4->co, v1->co);
	
				fac2= opp1/(len2+len3+len6) + opp2/(len4+len1+len6);
				ok = 0;
				
				if (fac1 > fac2) {
					e = BM_Rotate_Edge(bm, e, 0);
					BMO_SetFlag(bm, e, ELE_NEW);
					
					BMO_SetFlag(bm, e->l->f, FACE_MARK|ELE_NEW);
					BMO_SetFlag(bm, e->l->radial_next->f, FACE_MARK|ELE_NEW);
					stop = 0;
				}
			}
		}
	}
	
	BMO_Flag_To_Slot(bm, op, "geomout", ELE_NEW, BM_EDGE|BM_FACE);
}
