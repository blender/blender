#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BKE_utildefines.h"

#include "BLI_scanfill.h"
#include "BLI_math.h"
#include "BLI_array.h"
#include "BLI_editVert.h"
#include "BLI_smallhash.h"

#include "bmesh.h"
#include "bmesh_private.h"

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
			BMVert *v1, *v2, *v3, *v4;
			
			if (BM_Edge_FaceCount(e) != 2 || BMO_TestFlag(bm, e, EDGE_MARK))
				continue;
			if (!BMO_TestFlag(bm, e->l->f, FACE_MARK) || !BMO_TestFlag(bm, e->l->radial_next->f, FACE_MARK))
				continue;
			
			v1 = e->l->prev->v;
			v2 = e->l->v;
			v3 = e->l->radial_next->prev->v;
			v4 = e->l->next->v;
			
			if (is_quad_convex_v3(v1->co, v2->co, v3->co, v4->co)) {
				float len1, len2, len3, len4, len5, len6, opp1, opp2, fac1, fac2;
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

void bmesh_triangle_fill_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMEdge *e;
	BMOperator bmop;
	EditEdge *eed;
	EditVert *eve, *v1, *v2;
	EditFace *efa;
	SmallHash hash;

	BLI_smallhash_init(&hash);
	
	BLI_begin_edgefill();
	
	BMO_ITER(e, &siter, bm, op, "edges", BM_EDGE) {
		BMO_SetFlag(bm, e, EDGE_MARK);
		
		if (!BLI_smallhash_haskey(&hash, (uintptr_t)e->v1)) {
			eve = BLI_addfillvert(e->v1->co);
			eve->tmp.p = e->v1;
			BLI_smallhash_insert(&hash, (uintptr_t)e->v1, eve);
		}
		
		if (!BLI_smallhash_haskey(&hash, (uintptr_t)e->v2)) {
			eve = BLI_addfillvert(e->v2->co);
			eve->tmp.p = e->v2;
			BLI_smallhash_insert(&hash, (uintptr_t)e->v2, eve);
		}
		
		v1 = BLI_smallhash_lookup(&hash, (uintptr_t)e->v1);
		v2 = BLI_smallhash_lookup(&hash, (uintptr_t)e->v2);
		eed = BLI_addfilledge(v1, v2);
		eed->tmp.p = e;
	}
	
	BLI_edgefill(0);
	
	for (efa=fillfacebase.first; efa; efa=efa->next) {
		BMFace *f = BM_Make_QuadTri(bm, efa->v1->tmp.p, efa->v2->tmp.p, efa->v3->tmp.p, NULL, NULL, 1);
		BMLoop *l;
		BMIter liter;
		
		BMO_SetFlag(bm, f, ELE_NEW);
		BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f) {
			if (!BMO_TestFlag(bm, l->e, EDGE_MARK)) {
				BMO_SetFlag(bm, l->e, ELE_NEW);
			}
		}
	}
	
	BLI_end_edgefill();
	BLI_smallhash_release(&hash);
	
	/*clean up fill*/
	BMO_InitOpf(bm, &bmop, "beautify_fill faces=%ff constrain_edges=%fe", ELE_NEW, EDGE_MARK);
	BMO_Exec_Op(bm, &bmop);
	BMO_Flag_Buffer(bm, &bmop, "geomout", ELE_NEW, BM_FACE|BM_EDGE);
	BMO_Finish_Op(bm, &bmop);
	
	BMO_Flag_To_Slot(bm, op, "geomout", ELE_NEW, BM_EDGE|BM_FACE);
}
