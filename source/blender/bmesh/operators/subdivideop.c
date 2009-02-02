#include "MEM_guardedalloc.h"

#include "BKE_utildefines.h"

#include "BLI_arithb.h"
#include "BLI_rand.h"

#include "DNA_object_types.h"

#include "ED_mesh.h"

#include "bmesh.h"
#include "mesh_intern.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define SUBD_SPLIT	1
#define FACE_NEW	1
#define MAX_FACE	800

/*
note: this is a pattern-based edge subdivider.
it tries to match a pattern to edge selections on faces,
then executes functions to cut them.
*/
typedef struct subdpattern {
	int seledges[20]; //selected edges mask, for splitting

	/*verts starts at the first new vert cut, not the first vert in the
	  face*/
	void (*connectexec)(BMesh *bm, BMFace *face, BMVert **verts, 
		            int numcuts, int beauty, float rad);
	int len; /*total number of verts*/
} subdpattern;

/*generic subdivision rules:
  
  * two selected edges in a face should make a link
    between them.

  * one edge should do, what? make pretty topology, or just
    split the edge only?
*/


/* calculates offset for co, based on fractal, sphere or smooth settings  */
static void alter_co(float *co, BMEdge *edge, float rad, int beauty, float perc)
{
	float vec1[3], fac;
	
	if(beauty & B_SMOOTH) {
		/* we calculate an offset vector vec1[], to be added to *co */
		float len, fac, nor[3], nor1[3], nor2[3];
		
		VecSubf(nor, edge->v1->co, edge->v2->co);
		len= 0.5f*Normalize(nor);
	
		VECCOPY(nor1, edge->v1->no);
		VECCOPY(nor2, edge->v2->no);
	
		/* cosine angle */
		fac= nor[0]*nor1[0] + nor[1]*nor1[1] + nor[2]*nor1[2] ;
		
		vec1[0]= fac*nor1[0];
		vec1[1]= fac*nor1[1];
		vec1[2]= fac*nor1[2];
	
		/* cosine angle */
		fac= -nor[0]*nor2[0] - nor[1]*nor2[1] - nor[2]*nor2[2] ;
		
		vec1[0]+= fac*nor2[0];
		vec1[1]+= fac*nor2[1];
		vec1[2]+= fac*nor2[2];
		
		vec1[0]*= rad*len;
		vec1[1]*= rad*len;
		vec1[2]*= rad*len;
		
		co[0] += vec1[0];
		co[1] += vec1[1];
		co[2] += vec1[2];
	}
	else {
		if(rad > 0.0) {   /* subdivide sphere */
			Normalize(co);
			co[0]*= rad;
			co[1]*= rad;
			co[2]*= rad;
		}
		else if(rad< 0.0) {  /* fractal subdivide */
			fac= rad* VecLenf(edge->v1->co, edge->v2->co);
			vec1[0]= fac*(float)(0.5-BLI_drand());
			vec1[1]= fac*(float)(0.5-BLI_drand());
			vec1[2]= fac*(float)(0.5-BLI_drand());
			VecAddf(co, co, vec1);
		}

	}
}

/* assumes in the edge is the correct interpolated vertices already */
/* percent defines the interpolation, rad and beauty are for special options */
/* results in new vertex with correct coordinate, vertex normal and weight group info */
static BMVert *bm_subdivide_edge_addvert(BMesh *bm, BMEdge *edge, float rad, 
					 int beauty, float percent, BMEdge **out)
{
	BMVert *ev;
//	float co[3];
	
	ev = BM_Split_Edge(bm, edge->v1, edge, out, percent, 1);

	/* offset for smooth or sphere or fractal */
	alter_co(ev->co, edge, rad, beauty, percent);

#if 0 //TODO
	/* clip if needed by mirror modifier */
	if (edge->v1->f2) {
		if ( edge->v1->f2 & edge->v2->f2 & 1) {
			co[0]= 0.0f;
		}
		if ( edge->v1->f2 & edge->v2->f2 & 2) {
			co[1]= 0.0f;
		}
		if ( edge->v1->f2 & edge->v2->f2 & 4) {
			co[2]= 0.0f;
		}
	}
#endif	
	
	return ev;
}

static BMVert *subdivideedgenum(BMesh *bm, BMEdge *edge, 
				int curpoint, int totpoint, float rad, 
				int beauty, BMEdge **newe)
{
	BMVert *ev;
	float percent;
	 
	if (beauty & (B_PERCENTSUBD) && totpoint == 1)
		/*I guess the idea is vertices store what
		  percent to use?*/
		//percent=(float)(edge->tmp.l)/32768.0f;
		percent= 1.0; //edge->tmp.fp;
	else {
		percent= 1.0f/(float)(totpoint+1-curpoint);

	}
	
	/*{
		float co[3], co2[3];
		VecSubf(co, edge->v2->co, edge->v1->co);
		VecMulf(co, 1.0f/(float)(totpoint+1-curpoint));
		VecAddf(co2, edge->v1->co, co);
*/
		ev= bm_subdivide_edge_addvert(bm, edge, rad, beauty, percent, newe);
/*		VECCOPY(ev->co, co2);
	}
*/	
	return ev;
}

static void bm_subdivide_multicut(BMesh *bm, BMEdge *edge, float rad,
				  int beauty, int numcuts) {
	BMEdge *eed = edge, *newe;
	BMVert *v;
	int i;

	for(i=0;i<numcuts;i++) {
		v = subdivideedgenum(bm, eed, i, numcuts, rad, beauty, &newe);
		BMO_SetFlag(bm, v, SUBD_SPLIT);
		BMO_SetFlag(bm, eed, SUBD_SPLIT);
	}
}

/*note: the patterns are rotated as necassary to
  match the input geometry.  they're based on the
  pre-split state of the  face*/

/*
     
v3---------v2
|          |
|          |
|          |
|          |
v4---v0---v1

*/
static void q_1edge_split(BMesh *bm, BMFace *face, BMVert **vlist, int numcuts,
			  int beauty, float rad) {
	BMFace *nf;
	int i, add;

	/*if it's odd, the middle face is a quad, otherwise it's a triangle*/
	if (numcuts % 2==0) {
		add = 2;
		for (i=0; i<numcuts; i++) {
			if (i == numcuts/2) add -= 1;
			BM_Connect_Verts(bm, vlist[i], vlist[numcuts+add], &nf);
		}
	} else {
		add = 2;
		for (i=0; i<numcuts; i++) {
			BM_Connect_Verts(bm, vlist[i], vlist[numcuts+add], &nf);
			if (i == numcuts/2) {
				add -= 1;
				BM_Connect_Verts(bm, vlist[i], vlist[numcuts+add], &nf);
			}
		}

	}
}

subdpattern q_1edge = {
	{1, 0, 0, 0},
	q_1edge_split,
	4,
};


/*
 
v4---v3---v2
|     s    |
|          |
|          |
|     s    |
v5---v0---v1

*/
static void q_2edge_op_split(BMesh *bm, BMFace *face, BMVert **vlist, 
			     int numcuts, int beauty, float rad) {
	BMFace *nf;
	int i;
	
	for (i=0; i<numcuts; i++) {
		BM_Connect_Verts(bm, vlist[i], vlist[(numcuts-i-1)+numcuts+2], &nf);
	}
}

subdpattern q_2edge_op = {
	{1, 0, 1, 0},
	q_2edge_op_split,
	4,
};

/*
v6--------v5
|          |
|          |v4s
|          |v3s
|   s  s   |
v7-v0--v1-v2

*/
static void q_2edge_split(BMesh *bm, BMFace *face, BMVert **vlist, 
			     int numcuts, int beauty, float rad) {
	BMFace *nf;
	int i;
	
	for (i=0; i<numcuts; i++) {
		BM_Connect_Verts(bm, vlist[i], vlist[numcuts+(numcuts-i)], &nf);
	}
	BM_Connect_Verts(bm, vlist[numcuts*2+3], vlist[numcuts*2+1], &nf);
}

subdpattern q_2edge = {
	{1, 1, 0, 0},
	q_2edge_split,
	4,
};

/*  s   s
v8--v7--v6-v5
|          |
|          v4 s
|          |
|          v3 s
|   s  s   |
v9-v0--v1-v2

*/
static void q_3edge_split(BMesh *bm, BMFace *face, BMVert **vlist, 
			     int numcuts, int beauty, float rad) {
	BMFace *nf;
	int i, add=0;
	
	for (i=0; i<numcuts; i++) {
		if (i == numcuts/2) {
			if (numcuts % 2 != 0) {
				BM_Connect_Verts(bm, vlist[numcuts-i-1+add], vlist[i+numcuts+1], &nf);
			}
			add = numcuts*2+2;
		}
		BM_Connect_Verts(bm, vlist[numcuts-i-1+add], vlist[i+numcuts+1], &nf);
	}

	for (i=0; i<numcuts/2+1; i++) {
		BM_Connect_Verts(bm, vlist[i], vlist[(numcuts-i)+numcuts*2+1], &nf);
	}
}

subdpattern q_3edge = {
	{1, 1, 1, 0},
	q_3edge_split,
	4,
};

/*
 
           v8--v7-v6--v5
           |     s    |
           |v9 s     s|v4
first line |          |   last line
           |v10s s   s|v3
           v11-v0--v1-v2

	   it goes from bottom up
*/
static void q_4edge_split(BMesh *bm, BMFace *face, BMVert **vlist, int numcuts,
			  int beauty, float rad) {
	BMFace *nf;
	BMVert *v;
	BMEdge *e, *ne;
	BMVert **lines = MEM_callocN(sizeof(BMVert*)*(numcuts+2)*(numcuts+2),
		                     "q_4edge_split");
	int i, j, a, b, s=numcuts+2, totv=numcuts*4+4;

	/*build a 2-dimensional array of verts,
	  containing every vert (and all new ones)
	  in the face.*/

	/*first line*/
	for (i=0; i<numcuts+2; i++) {
		lines[i] = vlist[numcuts*3+2+(numcuts-i+1)];
	}

	/*last line*/
	for (i=0; i<numcuts+2; i++) {
		lines[(s-1)*s+i] = vlist[numcuts+i];
	}
	
	/*first and last members of middle lines*/
	for (i=0; i<numcuts; i++) {
		a = i;
		b = numcuts + 1 + numcuts + 1 + (numcuts - i - 1);
		
		e = BM_Connect_Verts(bm, vlist[a], vlist[b], &nf);
		
		lines[(i+1)*s] = vlist[a];
		lines[(i+1)*s + s-1] = vlist[b];

		for (a=0; a<numcuts; a++) {
			v = subdivideedgenum(bm, e, a, numcuts, rad, beauty, &ne);
			lines[(i+1)*s+a+1] = v;
		}
	}

	for (i=1; i<numcuts+2; i++) {
		for (j=1; j<numcuts+1; j++) {
			a = i*s + j;
			b = (i-1)*s + j;
			BM_Connect_Verts(bm, lines[a], lines[b], &nf);
		}
	}

	/*
	for (i=0; i<numcuts; i++) {
		a = i;
		b = numcuts + 1 + numcuts + 1 + (numcuts - i - 1);
		BM_Connect_Verts(bm, vlist[a], vlist[b], &nf);
	}*/

	MEM_freeN(lines);
}

subdpattern q_4edge = {
	{1, 1, 1, 1},
	q_4edge_split,
	4,
};

subdpattern *patterns[] = {
	&q_1edge,
	&q_2edge_op,
	&q_4edge,
	&q_3edge,
	&q_2edge,
};

#define PLEN	(sizeof(patterns) / sizeof(void*))

typedef struct subd_facedata {
	BMVert *start; subdpattern *pat;
} subd_facedata;

void esubdivide_exec(BMesh *bmesh, BMOperator *op)
{
	BMOpSlot *einput;
	BMEdge *edge, *edges[MAX_FACE];
	BMFace *face;
	BMLoop *nl;
	BMVert *verts[MAX_FACE];
	BMIter fiter, liter;
	subdpattern *pat;
	float rad;
	int i, j, matched, a, b, numcuts, flag, selaction;
	subd_facedata *facedata = NULL;
	V_DECLARE(facedata);
	
	BMO_Flag_Buffer(bmesh, op, BMOP_ESUBDIVIDE_EDGES, SUBD_SPLIT);
	
	numcuts = BMO_GetSlot(op, BMOP_ESUBDIVIDE_NUMCUTS)->data.i;
	flag = BMO_GetSlot(op, BMOP_ESUBDIVIDE_FLAG)->data.i;
	rad = BMO_GetSlot(op, BMOP_ESUBDIVIDE_RADIUS)->data.f;
	selaction = BMO_GetSlot(op, BMOP_ESUBDIVIDE_SELACTION)->data.i;

	einput = BMO_GetSlot(op, BMOP_ESUBDIVIDE_EDGES);
	
	/*first go through and split edges*/
	for (i=0; i<einput->len; i++) {
		edge = ((BMEdge**)einput->data.p)[i];
		BMO_SetFlag(bmesh, edge, SUBD_SPLIT);
	}
	
	for (face=BMIter_New(&fiter, bmesh, BM_FACES, NULL);
	     face; face=BMIter_Step(&fiter)) {
		/*figure out which pattern to use*/
		if (face->len > MAX_FACE) continue;

		i = 0;
		for (nl=BMIter_New(&liter, bmesh, BM_LOOPS_OF_FACE, face);
		     nl; nl=BMIter_Step(&liter)) {
			edges[i] = nl->e;
			verts[i] = nl->v;
			i++;
		}

		for (i=0; i<PLEN; i++) {
			pat = patterns[i];
 			if (pat->len == face->len) {
				for (a=0; a<pat->len; a++) {
				matched = 1;
				for (b=0; b<pat->len; b++) {
					j = (b + a) % pat->len;
					if ((!!BMO_TestFlag(bmesh, edges[j], SUBD_SPLIT))
						!= (!!pat->seledges[b])) {
							matched = 0;
							break;
					}
				}
				if (matched) break;
				}
				if (matched) {
					V_GROW(facedata);
					BMO_SetFlag(bmesh, face, SUBD_SPLIT);
					j = V_COUNT(facedata) - 1;
					facedata[j].pat = pat;
					facedata[j].start = verts[a];
					break;
				}
			}
		}
	}

	/*go through and split edges*/
	for (i=0; i<einput->len; i++) {
		edge = ((BMEdge**)einput->data.p)[i];
		bm_subdivide_multicut(bmesh, edge, rad, flag, numcuts);
		//BM_Split_Edge_Multi(bmesh, edge, numcuts);
	}

	//if (facedata) V_FREE(facedata);
	//return;

	i = 0;
	for (face=BMIter_New(&fiter, bmesh, BM_FACES, NULL);
	     face; face=BMIter_Step(&fiter)) {
		/*figure out which pattern to use*/
		if (face->len > MAX_FACE) continue;
		if (BMO_TestFlag(bmesh, face, SUBD_SPLIT) == 0) continue;
		
		pat = facedata[i].pat;
		if (!pat) continue;

		j = a = 0;
		for (nl=BMIter_New(&liter, bmesh, BM_LOOPS_OF_FACE, face);
		     nl; nl=BMIter_Step(&liter)) {
			if (nl->v == facedata[i].start) {
				a = j+1;
				break;
			}
			j++;
		}

		j = 0;
		for (nl=BMIter_New(&liter, bmesh, BM_LOOPS_OF_FACE, face);
		     nl; nl=BMIter_Step(&liter)) {
			b = (j-a+face->len) % face->len;
			verts[b] = nl->v;
			j += 1;
		}
		
		pat->connectexec(bmesh, face, verts, numcuts, flag, rad);
		i++;
	}

	if (facedata) V_FREE(facedata);
}

/*editmesh-emulating function*/
void BM_esubdivideflag(Object *obedit, BMesh *bm, int flag, float rad, 
		       int beauty, int numcuts, int seltype) {
	BMOperator op;

	BMO_Init_Op(&op, BMOP_ESUBDIVIDE);
	BMO_Set_Int(&op, BMOP_ESUBDIVIDE_NUMCUTS, numcuts);
	BMO_Set_Int(&op, BMOP_ESUBDIVIDE_FLAG, beauty);
	BMO_Set_Float(&op, BMOP_ESUBDIVIDE_RADIUS, rad);
	BMO_Set_Int(&op, BMOP_ESUBDIVIDE_SELACTION, seltype);
	BMO_HeaderFlag_To_Slot(bm, &op, BMOP_ESUBDIVIDE_EDGES, flag, BM_EDGE);
	
	BMO_Exec_Op(bm, &op);
	BMO_Finish_Op(bm, &op);
}
