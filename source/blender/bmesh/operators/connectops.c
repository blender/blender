#include "MEM_guardedalloc.h"

#include "BKE_utildefines.h"

#include "bmesh.h"
#include "mesh_intern.h"
#include "bmesh_private.h"
#include "BLI_math.h"
#include "BLI_array.h"

#include <stdio.h>
#include <string.h>

#define VERT_INPUT	1
#define EDGE_OUT	1
#define FACE_NEW	2
#define EDGE_MARK	4
#define EDGE_DONE	8

void connectverts_exec(BMesh *bm, BMOperator *op)
{
	BMIter iter, liter;
	BMFace *f, *nf;
	BMLoop **loops = NULL, *lastl = NULL;
	BLI_array_declare(loops);
	BMLoop *l, *nl;
	BMVert *v1, *v2, **verts = NULL;
	BLI_array_declare(verts);
	int i;
	
	BMO_Flag_Buffer(bm, op, "verts", VERT_INPUT, BM_VERT);

	for (f=BMIter_New(&iter, bm, BM_FACES_OF_MESH, NULL); f; f=BMIter_Step(&iter)){
		BLI_array_empty(loops);
		BLI_array_empty(verts);
		
		if (BMO_TestFlag(bm, f, FACE_NEW)) continue;

		l = BMIter_New(&liter, bm, BM_LOOPS_OF_FACE, f);
		v1 = v2 = NULL;
		lastl = NULL;
		for (; l; l=BMIter_Step(&liter)) {
			if (BMO_TestFlag(bm, l->v, VERT_INPUT)) {
				if (!lastl) {
					lastl = l;
					continue;
				}

				if (lastl != l->prev && lastl != 
				    l->next)
				{
					BLI_array_growone(loops);
					loops[BLI_array_count(loops)-1] = lastl;

					BLI_array_growone(loops);
					loops[BLI_array_count(loops)-1] = l;

				}
				lastl = l;
			}
		}

		if (BLI_array_count(loops) == 0) continue;
		
		if (BLI_array_count(loops) > 2) {
			BLI_array_growone(loops);
			loops[BLI_array_count(loops)-1] = loops[BLI_array_count(loops)-2];

			BLI_array_growone(loops);
			loops[BLI_array_count(loops)-1] = loops[0];
		}

		BM_LegalSplits(bm, f, (BMLoop *(*)[2])loops, BLI_array_count(loops)/2);
		
		for (i=0; i<BLI_array_count(loops)/2; i++) {
			if (loops[i*2]==NULL) continue;

			BLI_array_growone(verts);
			verts[BLI_array_count(verts)-1] = loops[i*2]->v;
		
			BLI_array_growone(verts);
			verts[BLI_array_count(verts)-1] = loops[i*2+1]->v;
		}

		for (i=0; i<BLI_array_count(verts)/2; i++) {
			nf = BM_Split_Face(bm, f, verts[i*2],
				           verts[i*2+1], &nl, NULL);
			f = nf;
			
			if (!nl || !nf) {
				BMO_RaiseError(bm, op,
					BMERR_CONNECTVERT_FAILED, NULL);
				BLI_array_free(loops);
				return;;;
			}
			BMO_SetFlag(bm, nf, FACE_NEW);
			BMO_SetFlag(bm, nl->e, EDGE_OUT);
		}
	}

	BMO_Flag_To_Slot(bm, op, "edgeout", EDGE_OUT, BM_EDGE);

	BLI_array_free(loops);
	BLI_array_free(verts);
}

static BMVert *get_outer_vert(BMesh *bm, BMEdge *e) 
{
	BMIter iter;
	BMEdge *e2;
	int i;
	
	i= 0;
	BM_ITER(e2, &iter, bm, BM_EDGES_OF_VERT, e->v1) {
		if (BMO_TestFlag(bm, e2, EDGE_MARK))
			i++;
	}
	
	if (i==2) 
		return e->v2;
	else
		return e->v1;
}

void bmesh_bridge_loops_exec(BMesh *bm, BMOperator *op)
{
	BMEdge **ee1 = NULL, **ee2 = NULL;
	BMVert **vv1 = NULL, **vv2 = NULL;
	BLI_array_declare(ee1);
	BLI_array_declare(ee2);
	BLI_array_declare(vv1);
	BLI_array_declare(vv2);
	BMOIter siter;
	BMIter iter;
	BMEdge *e;
	int c=0, cl1=0, cl2=0;
	
	BMO_ITER(e, &siter, bm, op, "edges", BM_EDGE) {
		BMO_SetFlag(bm, e, EDGE_MARK);
	}

	BMO_ITER(e, &siter, bm, op, "edges", BM_EDGE) {
		if (!BMO_TestFlag(bm, e, EDGE_DONE)) {
			BMVert *v, *ov;
			BMEdge *e2, *e3;
			
			if (c > 2) {
				printf("eek! more than two edge loops!\n");
				break;
			}
			
			e2 = e;
			v = e->v1;
			do {
				v = BM_OtherEdgeVert(e2, v);
				BM_ITER(e3, &iter, bm, BM_EDGES_OF_VERT, v) {
					if (e3 != e2 && BMO_TestFlag(bm, e3, EDGE_MARK)) {
						break;
					}
				}
				e2 = e3;
			} while (e2 && e2 != e);
			
			if (!e2)
				e2 = e;
				
			e = e2;
			ov = v;
			do {
				if (c==0) {
					BLI_array_append(ee1, e2);
					BLI_array_append(vv1, v);
				} else {
					BLI_array_append(ee2, e2);
					BLI_array_append(vv2, v);
				}
				
				BMO_SetFlag(bm, e2, EDGE_DONE);
				
				v = BM_OtherEdgeVert(e2, v);
				BM_ITER(e3, &iter, bm, BM_EDGES_OF_VERT, v) {
					if (e3 != e2 && BMO_TestFlag(bm, e3, EDGE_MARK) && !BMO_TestFlag(bm, e3, EDGE_DONE)) {
						break;
					}
				}
				e2 = e3;
			} while (e2 && e2 != e);
			
			if (v && !e2) {			
				if (c==0) {
					if (BLI_array_count(vv1) && v == vv1[BLI_array_count(vv1)-1]) {
						printf("eck!\n");
					}
					BLI_array_append(vv1, v);
				} else {
					BLI_array_append(vv2, v);
				}
			}
				
			if (v == ov) {
				if (c==0)
					cl1 = 1;
				else 
					cl2 = 1;
			}
			
			c++;
		}
	}
	
	if (ee1 && ee2) {
		int i, j;
		BMVert *v1, *v2, *v3, *v4;
		int starti=0, lenv1=BLI_array_count(vv1), lenv2=BLI_array_count(vv1);
		
		/*handle case of two unclosed loops*/
		if (!cl1 && !cl2) {
			v1 = get_outer_vert(bm, ee1[0]);
			v2 = BLI_array_count(ee1) > 1 ? get_outer_vert(bm, ee1[1]) : v1;
			v3 = get_outer_vert(bm, ee2[0]);
			v4 = BLI_array_count(ee2) > 1 ? get_outer_vert(bm, ee2[1]) : v3;

			if (len_v3v3(v1->co, v3->co) > len_v3v3(v1->co, v4->co)) {
				for (i=0; i<BLI_array_count(ee1)/2; i++) {
					SWAP(void*, ee1[i], ee1[BLI_array_count(ee1)-i-1]);
					SWAP(void*, vv1[i], vv1[BLI_array_count(vv1)-i-1]);
				}
			}
		} 
		
		if (cl1) {
			float min = 1e32;
			
			for (i=0; i<BLI_array_count(vv1); i++) {
				if (len_v3v3(vv1[i]->co, vv2[0]->co) < min) {
					min = len_v3v3(vv1[i]->co, vv2[0]->co);
					starti = i;
				}
			}
		}
		
		j = 0;
		if (lenv1 && vv1[0] == vv1[lenv1-1]) {
			lenv1--;
		}
		if (lenv2 && vv2[0] == vv2[lenv2-1]) {
			lenv2--;
		}
		
		for (i=0; i<BLI_array_count(ee1); i++) {
			BMFace *f;
		
			if (j >= BLI_array_count(ee2))
				break;
			
			if (vv1[(i + starti)%lenv1] ==  vv1[(i + 1 + starti)%lenv1]) {
				j++;
				continue;
			}
				
			f = BM_Make_QuadTri(bm, vv1[(i + starti)%lenv1], vv2[i], vv2[(i+1)%lenv2], vv1[(i+1 + starti)%lenv1], NULL, 1);
			if (!f || f->len != 4) {
				printf("eek in bridge!\n");
			}
			
			j++;
		}
	}
}
