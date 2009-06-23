#include "MEM_guardedalloc.h"

#include "BKE_utildefines.h"

#include "BLI_ghash.h"
#include "BLI_memarena.h"
#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "bmesh.h"
#include "bmesh_operators_private.h"

#define ELE_NEW		1
#define ELE_OUT		2

void bmesh_edgenet_fill_exec(BMesh *bm, BMOperator *op)
{
	/*unimplemented, need to think on how to do this.  probably are graph
	  theory stuff that could help with this problem.*/
}

/* evaluate if entire quad is a proper convex quad */
static int convex(float *v1, float *v2, float *v3, float *v4)
{
	float nor[3], nor1[3], nor2[3], vec[4][2];
	
	/* define projection, do both trias apart, quad is undefined! */
	CalcNormFloat(v1, v2, v3, nor1);
	CalcNormFloat(v1, v3, v4, nor2);
	nor[0]= ABS(nor1[0]) + ABS(nor2[0]);
	nor[1]= ABS(nor1[1]) + ABS(nor2[1]);
	nor[2]= ABS(nor1[2]) + ABS(nor2[2]);

	if(nor[2] >= nor[0] && nor[2] >= nor[1]) {
		vec[0][0]= v1[0]; vec[0][1]= v1[1];
		vec[1][0]= v2[0]; vec[1][1]= v2[1];
		vec[2][0]= v3[0]; vec[2][1]= v3[1];
		vec[3][0]= v4[0]; vec[3][1]= v4[1];
	}
	else if(nor[1] >= nor[0] && nor[1]>= nor[2]) {
		vec[0][0]= v1[0]; vec[0][1]= v1[2];
		vec[1][0]= v2[0]; vec[1][1]= v2[2];
		vec[2][0]= v3[0]; vec[2][1]= v3[2];
		vec[3][0]= v4[0]; vec[3][1]= v4[2];
	}
	else {
		vec[0][0]= v1[1]; vec[0][1]= v1[2];
		vec[1][0]= v2[1]; vec[1][1]= v2[2];
		vec[2][0]= v3[1]; vec[2][1]= v3[2];
		vec[3][0]= v4[1]; vec[3][1]= v4[2];
	}
	
	/* linetests, the 2 diagonals have to instersect to be convex */
	if( IsectLL2Df(vec[0], vec[2], vec[1], vec[3]) > 0 ) return 1;
	return 0;
}

/*this is essentially new fkey*/
void bmesh_contextual_create_exec(BMesh *bm, BMOperator *op)
{
	BMOperator op2;
	BMOIter oiter;
	BMIter iter, liter;
	BMHeader *h;
	BMVert *v, *verts[4];
	BMEdge *e;
	BMLoop *l;
	BMFace *f;
	int totv=0, tote=0, totf=0, amount;

	/*count number of each element type we were passed*/
	BMO_ITER(h, &oiter, bm, op, "geom", BM_VERT|BM_EDGE|BM_FACE) {
		switch (h->type) {
			case BM_VERT: totv++; break;
			case BM_EDGE: tote++; break;
			case BM_FACE: totf++; break;
		}

		BMO_SetFlag(bm, h, ELE_NEW);
	}
	
	/*first call dissolve faces*/
	BMO_InitOpf(bm, &op2, "dissolvefaces faces=%ff", ELE_NEW);
	BMO_Exec_Op(bm, &op2);
	BMO_ITER(f, &oiter, bm, &op2, "regionout", BM_FACE) {
		BMO_SetFlag(bm, f, ELE_OUT);

		/*unflag verts associated with dissolved faces*/
		BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f) {
			BMO_ClearFlag(bm, l->v, ELE_NEW);
		}
	}
	BMO_Finish_Op(bm, &op2);

	/*then call edgenet create, which may still be unimplemented, heh*/
	BMO_InitOpf(bm, &op2, "edgenet_fill edges=%fe", ELE_NEW);
	BMO_Exec_Op(bm, &op2);
	BMO_ITER(f, &oiter, bm, &op2, "faceout", BM_FACE) {
		BMO_SetFlag(bm, f, ELE_OUT);

		/*unflag verts associated with the output faces*/
		BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f) {
			BMO_ClearFlag(bm, l->v, ELE_NEW);
		}
	}
	BMO_Finish_Op(bm, &op2);
	
	/*now, count how many verts we have*/
	amount = 0;
	BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
		if (BMO_TestFlag(bm, v, ELE_NEW)) {
			verts[amount] = v;
			amount++;

			if (amount > 4) break;
		}
	}

	if (amount == 2) {
		/*create edge*/
		e = BM_Make_Edge(bm, verts[0], verts[1], NULL, 1);
		BMO_SetFlag(bm, e, ELE_OUT);		
	} else if (amount == 3) {
		/*create triangle*/
		BM_Make_QuadTri(bm, verts[0], verts[1], verts[2], NULL, NULL, 1);
	} else if (amount == 4) {
		f = NULL;

		/* the order of vertices can be anything, 6 cases to check */
		if( convex(verts[0]->co, verts[1]->co, verts[2]->co, verts[3]->co) ) {
			f= BM_Make_QuadTri(bm, verts[0], verts[1], verts[2], verts[3], NULL, 1);
		}
		else if( convex(verts[0]->co, verts[2]->co, verts[3]->co, verts[1]->co) ) {
			f= BM_Make_QuadTri(bm, verts[0], verts[2], verts[3], verts[1], NULL, 1);
		}
		else if( convex(verts[0]->co, verts[2]->co, verts[1]->co, verts[3]->co) ) {
			f= BM_Make_QuadTri(bm, verts[0], verts[2], verts[1], verts[3], NULL, 1);
		}
		else if( convex(verts[0]->co, verts[1]->co, verts[3]->co, verts[2]->co) ) {
			f= BM_Make_QuadTri(bm, verts[0], verts[1], verts[3], verts[2], NULL, 1);
		}
		else if( convex(verts[0]->co, verts[3]->co, verts[2]->co, verts[1]->co) ) {
			f= BM_Make_QuadTri(bm, verts[0], verts[3], verts[2], verts[1], NULL, 1);
		}
		else if( convex(verts[0]->co, verts[3]->co, verts[1]->co, verts[2]->co) ) {
			f= BM_Make_QuadTri(bm, verts[0], verts[3], verts[1], verts[2], NULL, 1);
		}
		else {
			printf("cannot find nice quad from concave set of vertices\n");
		}

		if (f) BMO_SetFlag(bm, f, ELE_OUT);
	}

	BMO_Flag_To_Slot(bm, op, "faceout", ELE_OUT, BM_FACE);
}
