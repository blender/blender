#include "MEM_guardedalloc.h"

#include "BKE_utildefines.h"

#include "bmesh.h"
#include "mesh_intern.h"
#include "bmesh_private.h"
#include "BLI_arithb.h"
#include "BLI_ghash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BL(ptr) ((BMLoop*)(ptr))

void remdoubles_splitface(BMFace *f, BMesh *bm, BMOperator *op)
{
	BMIter liter;
	BMLoop *l;
	BMVert *v2, *doub;
	int split=0;

	BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f) {
		v2 = BMO_Get_MapPointer(bm, op, "targetmap", l->v);
		/*ok: if v2 is NULL (e.g. not in the map) then it's
		      a target vert, otherwise it's a double*/
		if (v2 && BM_Vert_In_Face(f, v2) && v2 != BL(l->head.prev)->v 
		    && v2 != BL(l->head.next)->v)
		{
			doub = l->v;
			split = 1;
			break;
		}
	}

	if (split && doub != v2) {
		BMLoop *nl;
		BMFace *f2 = BM_Split_Face(bm, f, doub, v2, &nl, NULL);

		remdoubles_splitface(f, bm, op);
		remdoubles_splitface(f2, bm, op);
	}
}

#define ELE_DEL		1
#define EDGE_COL	2
#define FACE_MARK	2

#if 0
int remdoubles_face_overlaps(BMesh *bm, BMVert **varr, 
			     int len, BMFace *exclude, 
			     BMFace **overlapface)
{
	BMIter vertfaces;
	BMFace *f;
	int i, amount;

	if (overlapface) *overlapface = NULL;

	for(i=0; i < len; i++){
		f = BMIter_New(&vertfaces, bm, BM_FACES_OF_VERT, varr[i] );
		while(f){
			amount = BM_Verts_In_Face(bm, f, varr, len);
			if(amount >= len){
				if (overlapface) *overlapface = f;
				return 1;				
			}
			f = BMIter_Step(&vertfaces);
		}
	}
	return 0;
}
#endif

void bmesh_weldverts_exec(BMesh *bm, BMOperator *op)
{
	BMIter iter, liter;
	BMVert *v, *v2;
	BMEdge *e, *e2, **edges = NULL;
	V_DECLARE(edges);
	BMLoop *l, *l2, **loops = NULL;
	V_DECLARE(loops);
	BMFace *f, *f2;
	int a;

	BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
		if (BMO_Get_MapPointer(bm, op, "targetmap", v))
			BMO_SetFlag(bm, v, ELE_DEL);
	}

	BM_ITER(f, &iter, bm, BM_FACES_OF_MESH, NULL) {
		remdoubles_splitface(f, bm, op);
	}
	
	BM_ITER(e, &iter, bm, BM_EDGES_OF_MESH, NULL) {
		v = BMO_Get_MapPointer(bm, op, "targetmap", e->v1);
		v2 = BMO_Get_MapPointer(bm, op, "targetmap", e->v2);
		
		if (!v) v = e->v1;
		if (!v2) v2 = e->v2;

		if ((e->v1 != v) || (e->v2 != v2)) {
			if (v == v2) BMO_SetFlag(bm, e, EDGE_COL);
			else BM_Make_Edge(bm, v, v2, e, 1);

			BMO_SetFlag(bm, e, ELE_DEL);
		}
	}

	BM_ITER(f, &iter, bm, BM_FACES_OF_MESH, NULL) {
		BMINDEX_SET(f, 0);
		BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f) {
			if (BMO_TestFlag(bm, l->v, ELE_DEL))
				BMO_SetFlag(bm, f, FACE_MARK);
			if (BMO_TestFlag(bm, l->e, EDGE_COL)) 
				BMINDEX_SET(f, BMINDEX_GET(f)+1);
		}
	}

	BM_ITER(f, &iter, bm, BM_FACES_OF_MESH, NULL) {
		if (!BMO_TestFlag(bm, f, FACE_MARK)) continue;
		if (f->len - BMINDEX_GET(f) < 3) {
			BMO_SetFlag(bm, f, ELE_DEL);
			continue;
		}

		V_RESET(edges);
		V_RESET(loops);
		a = 0;
		BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f) {
			v = l->v;
			v2 = BL(l->head.next)->v;
			if (BMO_TestFlag(bm, v, ELE_DEL)) 
				v = BMO_Get_MapPointer(bm, op, "targetmap", v);
			if (BMO_TestFlag(bm, v2, ELE_DEL)) 
					v2 = BMO_Get_MapPointer(bm, op, "targetmap", v2);
			
			e2 = v != v2 ? BM_Edge_Exist(v, v2) : NULL;
			if (e2) {
				V_GROW(edges);
				V_GROW(loops);

				edges[a] = e2;
				loops[a] = l;

				a++;
			}
		}
		
		v = loops[0]->v;
		v2 = loops[1]->v;

		if (BMO_TestFlag(bm, v, ELE_DEL)) 
			v = BMO_Get_MapPointer(bm, op, "targetmap", v);
		if (BMO_TestFlag(bm, v2, ELE_DEL)) 
			v2 = BMO_Get_MapPointer(bm, op, "targetmap", v2);
		

		f2 = BM_Make_Ngon(bm, v, v2, edges, a, 0);
		if (f2) {
			BM_Copy_Attributes(bm, bm, f, f2);
			BMO_SetFlag(bm, f, ELE_DEL);

			a = 0;
			BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f2) {
				l2 = loops[a];
				BM_Copy_Attributes(bm, bm, l2, l);

				a++;
			}
		}

		/*need to still copy customdata stuff here, will do later*/
	}

	BMO_CallOpf(bm, "del geom=%fvef context=%i", ELE_DEL, DEL_ONLYTAGGED);

	V_FREE(edges);
}

static int vergaverco(const void *e1, const void *e2)
{
	const BMVert *v1 = *(void**)e1, *v2 = *(void**)e2;
	float x1 = v1->co[0] + v1->co[1] + v1->co[2];
	float x2 = v2->co[0] + v2->co[1] + v2->co[2];

	if (x1 > x2) return 1;
	else if (x1 < x2) return -1;
	else return 0;
}

#define VERT_TESTED	1
#define VERT_DOUBLE	2
#define VERT_TARGET	4
#define VERT_KEEP	8

void bmesh_removedoubles_exec(BMesh *bm, BMOperator *op)
{
	BMOperator weldop;
	BMOIter oiter;
	BMVert *v, *v2;
	BMVert **verts=NULL;
	V_DECLARE(verts);
	float dist, distsqr;
	int i, j, len;

	dist = BMO_Get_Float(op, "dist");
	distsqr = dist*dist;

	BMO_Init_Op(&weldop, "weldverts");
	
	i = 0;
	BMO_ITER(v, &oiter, bm, op, "verts", BM_VERT) {
		V_GROW(verts);
		verts[i++] = v;
	}

	/*sort by vertex coordinates added together*/
	qsort(verts, V_COUNT(verts), sizeof(void*), vergaverco);
	
	len = V_COUNT(verts);
	for (i=0; i<len; i++) {
		v = verts[i];
		if (BMO_TestFlag(bm, v, VERT_TESTED)) continue;
		
		BMO_SetFlag(bm, v, VERT_TESTED);
		for (j=i+1; j<len; j++) {
			float vec[3];
			
			v2 = verts[j];
			if ((v2->co[0]+v2->co[1]+v2->co[2]) - (v->co[0]+v->co[1]+v->co[2])
			     > distsqr) break;

			vec[0] = v->co[0] - v2->co[0];
			vec[1] = v->co[1] - v2->co[1];
			vec[2] = v->co[2] - v2->co[2];
			
			if (INPR(vec, vec) < distsqr) {
				BMO_SetFlag(bm, v2, VERT_TESTED);
				BMO_SetFlag(bm, v2, VERT_DOUBLE);
				BMO_SetFlag(bm, v, VERT_TARGET);
			
				BMO_Insert_MapPointer(bm, &weldop, "targetmap", v2, v);
			}
		}
	}

	V_FREE(verts);

	BMO_Exec_Op(bm, &weldop);
	BMO_Finish_Op(bm, &weldop);
}


void bmesh_finddoubles_exec(BMesh *bm, BMOperator *op)
{
	BMOIter oiter;
	BMVert *v, *v2;
	BMVert **verts=NULL;
	V_DECLARE(verts);
	float dist, distsqr;
	int i, j, len;

	dist = BMO_Get_Float(op, "dist");
	distsqr = dist*dist;

	i = 0;
	BMO_ITER(v, &oiter, bm, op, "verts", BM_VERT) {
		V_GROW(verts);
		verts[i++] = v;
	}

	/*sort by vertex coordinates added together*/
	//qsort(verts, V_COUNT(verts), sizeof(void*), vergaverco);
	
	BMO_Flag_Buffer(bm, op, "keepverts", VERT_KEEP);

	len = V_COUNT(verts);
	for (i=0; i<len; i++) {
		v = verts[i];
		if (BMO_TestFlag(bm, v, VERT_DOUBLE)) continue;
		
		//BMO_SetFlag(bm, v, VERT_TESTED);
		for (j=0; j<len; j++) {
			if (j == i) continue;

			//float vec[3];
			if (BMO_TestFlag(bm, v, VERT_KEEP)) continue;

			v2 = verts[j];
			//if ((v2->co[0]+v2->co[1]+v2->co[2]) - (v->co[0]+v->co[1]+v->co[2])
			//     > distsqr) break;

			//vec[0] = v->co[0] - v2->co[0];
			//vec[1] = v->co[1] - v2->co[1];
			//vec[2] = v->co[2] - v2->co[2];
			
			if (VecLenCompare(v->co, v2->co, dist)) {
				BMO_SetFlag(bm, v2, VERT_DOUBLE);
				BMO_SetFlag(bm, v, VERT_TARGET);
			
				BMO_Insert_MapPointer(bm, op, "targetmapout", v2, v);
			}
		}
	}

	V_FREE(verts);
}
