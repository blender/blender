#include "MEM_guardedalloc.h"

#include "BKE_utildefines.h"

#include "BLI_arithb.h"
#include "BLI_rand.h"
#include "BLI_ghash.h"

#include "DNA_object_types.h"

#include "ED_mesh.h"

#include "bmesh.h"
#include "mesh_intern.h"
#include "subdivideop.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*flags for all elements share a common bitfield space*/
#define SUBD_SPLIT	1

#define EDGE_PERCENT	2

/*I don't think new faces are flagged, currently, but
  better safe than sorry.*/
#define FACE_NEW	4
#define FACE_CUSTOMFILL	8
#define ELE_INNER	16
#define ELE_SPLIT	32
#define ELE_CONNECT	64

/*stuff for the flag paramter.  note that
  what used to live in "beauty" and
  in "seltype" live here.  still have to
  convert the beauty flags over, which
  is why it starts at 128 (to avoid
  collision).*/
#define SELTYPE_INNER	128

/*
NOTE: beauty has been renamed to flag!
*/

/*generic subdivision rules:
  
  * two selected edges in a face should make a link
    between them.

  * one edge should do, what? make pretty topology, or just
    split the edge only?
*/

/*connects face with smallest len, which I think should always be correct for
  edge subdivision*/
BMEdge *connect_smallest_face(BMesh *bm, BMVert *v1, BMVert *v2, BMFace **nf) {
	BMIter iter, iter2;
	BMVert *v;
	BMLoop *nl;
	BMFace *face, *curf = NULL;

	/*this isn't the best thing in the world.  it doesn't handle cases where there's
	  multiple faces yet.  that might require a convexity test to figure out which
	  face is "best," and who knows what for non-manifold conditions.*/
	for (face = BMIter_New(&iter, bm, BM_FACES_OF_VERT, v1); face; face=BMIter_Step(&iter)) {
		for (v=BMIter_New(&iter2, bm, BM_VERTS_OF_FACE, face); v; v=BMIter_Step(&iter2)) {
			if (v == v2) {
				if (!curf || face->len < curf->len) curf = face;
			}
		}
	}

	if (curf) {
		face = BM_Split_Face(bm, curf, v1, v2, &nl, NULL);
		
		if (nf) *nf = face;
		return nl ? nl->e : NULL;
	}

	return NULL;
}
/* calculates offset for co, based on fractal, sphere or smooth settings  */
static void alter_co(float *co, BMEdge *edge, subdparams *params, float perc,
		     BMVert *vsta, BMVert *vend)
{
	float vec1[3], fac;
	
	if(params->flag & B_SMOOTH) {
		/* we calculate an offset vector vec1[], to be added to *co */
		float len, fac, nor[3], nor1[3], nor2[3];
		
		VecSubf(nor, vsta->co, vend->co);
		len= 0.5f*Normalize(nor);
	
		VECCOPY(nor1, vsta->no);
		VECCOPY(nor2, vend->no);
	
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
		
		vec1[0]*= params->rad*len;
		vec1[1]*= params->rad*len;
		vec1[2]*= params->rad*len;
		
		co[0] += vec1[0];
		co[1] += vec1[1];
		co[2] += vec1[2];
	}
	else {
		if(params->rad > 0.0) {   /* subdivide sphere */
			Normalize(co);
			co[0]*= params->rad;
			co[1]*= params->rad;
			co[2]*= params->rad;
		}
		else if(params->rad< 0.0) {  /* fractal subdivide */
			fac= params->rad* VecLenf(vsta->co, vend->co);
			vec1[0]= fac*(float)(0.5-BLI_drand());
			vec1[1]= fac*(float)(0.5-BLI_drand());
			vec1[2]= fac*(float)(0.5-BLI_drand());
			VecAddf(co, co, vec1);
		}

	}
}

/* assumes in the edge is the correct interpolated vertices already */
/* percent defines the interpolation, rad and flag are for special options */
/* results in new vertex with correct coordinate, vertex normal and weight group info */
static BMVert *bm_subdivide_edge_addvert(BMesh *bm, BMEdge *edge,BMEdge *oedge,
					subdparams *params, float percent,
					float percent2,
					BMEdge **out,BMVert *vsta,BMVert *vend)
{
	BMVert *ev;
//	float co[3];
	
	ev = BM_Split_Edge(bm, edge->v1, edge, out, percent);
	BM_Vert_UpdateNormal(bm, ev);

	BMO_SetFlag(bm, ev, ELE_INNER);

	/* offset for smooth or sphere or fractal */
	alter_co(ev->co, oedge, params, percent2, vsta, vend);

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

static BMVert *subdivideedgenum(BMesh *bm, BMEdge *edge, BMEdge *oedge,
				int curpoint, int totpoint, subdparams *params,
				BMEdge **newe, BMVert *vsta, BMVert *vend)
{
	BMVert *ev;
	float percent, percent2 = 0.0f;
	 
	if (BMO_TestFlag(bm, edge, EDGE_PERCENT) && totpoint == 1)
		percent = BMO_Get_MapFloat(bm, params->op, 
			                "edgepercents", edge);
	else {
		percent= 1.0f/(float)(totpoint+1-curpoint);
		percent2 = (float)curpoint / (float)(totpoint + 1);

	}
	
	ev= bm_subdivide_edge_addvert(bm, edge, oedge, params, percent,
	                              percent2, newe, vsta, vend);
	return ev;
}

static void bm_subdivide_multicut(BMesh *bm, BMEdge *edge, subdparams *params, 
				  BMVert *vsta, BMVert *vend) {
	BMEdge *eed = edge, *newe, temp = *edge;
	BMVert *v;
	int i, numcuts = params->numcuts;

	for(i=0;i<numcuts;i++) {
		v = subdivideedgenum(bm, eed, &temp, i, params->numcuts, params, 
		                     &newe, vsta, vend);
		BMO_SetFlag(bm, v, SUBD_SPLIT);
		BMO_SetFlag(bm, eed, SUBD_SPLIT);

		BMO_SetFlag(bm, v, ELE_SPLIT);
		BMO_SetFlag(bm, eed, ELE_SPLIT);
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
static void q_1edge_split(BMesh *bm, BMFace *face,
			  BMVert **verts, subdparams *params) {
	BMFace *nf;
	int i, add, numcuts = params->numcuts;

	/*if it's odd, the middle face is a quad, otherwise it's a triangle*/
	if (numcuts % 2==0) {
		add = 2;
		for (i=0; i<numcuts; i++) {
			if (i == numcuts/2) add -= 1;
			connect_smallest_face(bm, verts[i], verts[numcuts+add], 
				           &nf);
		}
	} else {
		add = 2;
		for (i=0; i<numcuts; i++) {
			connect_smallest_face(bm, verts[i], verts[numcuts+add], 
				           &nf);
			if (i == numcuts/2) {
				add -= 1;
				connect_smallest_face(bm, verts[i], 
					           verts[numcuts+add],
						   &nf);
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

static void q_2edge_op_split(BMesh *bm, BMFace *face, BMVert **verts, 
                             subdparams *params)
{
	BMFace *nf;
	int i, numcuts = params->numcuts;
	
	for (i=0; i<numcuts; i++) {
		connect_smallest_face(bm, verts[i],verts[(numcuts-i-1)+numcuts+2],
			           &nf);
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
static void q_2edge_split(BMesh *bm, BMFace *face, BMVert **verts, 
                          subdparams *params)
{
	BMFace *nf;
	int i, numcuts = params->numcuts;
	
	for (i=0; i<numcuts; i++) {
		connect_smallest_face(bm, verts[i], verts[numcuts+(numcuts-i)],
			           &nf);
	}
	connect_smallest_face(bm, verts[numcuts*2+3], verts[numcuts*2+1], &nf);
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
static void q_3edge_split(BMesh *bm, BMFace *face, BMVert **verts, 
                          subdparams *params)
{
	BMFace *nf;
	int i, add=0, numcuts = params->numcuts;
	
	for (i=0; i<numcuts; i++) {
		if (i == numcuts/2) {
			if (numcuts % 2 != 0) {
				connect_smallest_face(bm, verts[numcuts-i-1+add], 
					         verts[i+numcuts+1], &nf);
			}
			add = numcuts*2+2;
		}
		connect_smallest_face(bm, verts[numcuts-i-1+add], 
			             verts[i+numcuts+1], &nf);
	}

	for (i=0; i<numcuts/2+1; i++) {
		connect_smallest_face(bm, verts[i],verts[(numcuts-i)+numcuts*2+1],
			           &nf);
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
static void q_4edge_split(BMesh *bm, BMFace *face, BMVert **verts, 
                          subdparams *params)
{
	BMFace *nf;
	BMVert *v, *v1, *v2;
	BMEdge *e, *ne, temp;
	BMVert **lines;
	int numcuts = params->numcuts;
	int i, j, a, b, s=numcuts+2, totv=numcuts*4+4;

	lines = MEM_callocN(sizeof(BMVert*)*(numcuts+2)*(numcuts+2),
		                     "q_4edge_split");
	/*build a 2-dimensional array of verts,
	  containing every vert (and all new ones)
	  in the face.*/

	/*first line*/
	for (i=0; i<numcuts+2; i++) {
		lines[i] = verts[numcuts*3+2+(numcuts-i+1)];
	}

	/*last line*/
	for (i=0; i<numcuts+2; i++) {
		lines[(s-1)*s+i] = verts[numcuts+i];
	}
	
	/*first and last members of middle lines*/
	for (i=0; i<numcuts; i++) {
		a = i;
		b = numcuts + 1 + numcuts + 1 + (numcuts - i - 1);
		
		e = connect_smallest_face(bm, verts[a], verts[b], &nf);
		if (!e) continue;

		BMO_SetFlag(bm, e, ELE_INNER);
		BMO_SetFlag(bm, nf, ELE_INNER);

		
		v1 = lines[(i+1)*s] = verts[a];
		v2 = lines[(i+1)*s + s-1] = verts[b];
		
		temp = *e;
		for (a=0; a<numcuts; a++) {
			v = subdivideedgenum(bm, e, &temp, a, numcuts, params, &ne,
			                     v1, v2);
			BMO_SetFlag(bm, ne, ELE_INNER);
			lines[(i+1)*s+a+1] = v;
		}
	}

	for (i=1; i<numcuts+2; i++) {
		for (j=1; j<numcuts+1; j++) {
			a = i*s + j;
			b = (i-1)*s + j;
			e = connect_smallest_face(bm, lines[a], lines[b], &nf);
			if (!e) continue;

			BMO_SetFlag(bm, e, ELE_INNER);
			BMO_SetFlag(bm, nf, ELE_INNER);
		}
	}

	MEM_freeN(lines);
}

/*    v3
     / \
    /   \
   /     \
  /       \
 /         \
v4--v0--v1--v2
    s    s
*/
static void t_1edge_split(BMesh *bm, BMFace *face, BMVert **verts, 
                          subdparams *params)
{
	BMFace *nf;
	int i, numcuts = params->numcuts;
	
	for (i=0; i<numcuts; i++) {
		connect_smallest_face(bm, verts[i], verts[numcuts+1], &nf);
	}
}

subdpattern t_1edge = {
	{1, 0, 0},
	t_1edge_split,
	3,
};

/*    v5
     / \
    /   \ v4 s
   /     \
  /       \ v3 s
 /         \
v6--v0--v1--v2
    s   s
*/
static void t_2edge_split(BMesh *bm, BMFace *face, BMVert **verts, 
                          subdparams *params)
{
	BMFace *nf;
	int i, numcuts = params->numcuts;
	
	for (i=0; i<numcuts; i++) {
		connect_smallest_face(bm, verts[i], verts[numcuts+numcuts-i], &nf);
	}
}

subdpattern t_2edge = {
	{1, 1, 0},
	t_2edge_split,
	3,
};


/*     v5
      / \
 s v6/---\ v4 s
    / \ / \
sv7/---v---\ v3 s
  /  \/  \/ \
 v8--v0--v1--v2
    s    s
*/
static void t_3edge_split(BMesh *bm, BMFace *face, BMVert **verts, 
                          subdparams *params)
{
	BMFace *nf;
	BMEdge *e, *ne, temp;
	BMVert ***lines, *v;
	void *stackarr[1];
	int i, j, a, b, numcuts = params->numcuts;
	
	/*number of verts in each line*/
	lines = MEM_callocN(sizeof(void*)*(numcuts+2), "triangle vert table");
	
	lines[0] = (BMVert**) stackarr;
	lines[0][0] = verts[numcuts*2+1];
	
	lines[1+numcuts] = MEM_callocN(sizeof(void*)*(numcuts+2), 
		                       "triangle vert table 2");
	for (i=0; i<numcuts; i++) {
		lines[1+numcuts][1+i] = verts[i];
	}
	lines[1+numcuts][0] = verts[numcuts*3+2];
	lines[1+numcuts][1+numcuts] = verts[numcuts];

	for (i=0; i<numcuts; i++) {
		lines[i+1] = MEM_callocN(sizeof(void*)*(2+i), 
			               "triangle vert table row");
		a = numcuts*2 + 2 + i;
		b = numcuts + numcuts - i;
		e = connect_smallest_face(bm, verts[a], verts[b], &nf);
		if (!e) goto cleanup;

		BMO_SetFlag(bm, e, ELE_INNER);
		BMO_SetFlag(bm, nf, ELE_INNER);

		lines[i+1][0] = verts[a];
		lines[i+1][1+i] = verts[b];
		
		temp = *e;
		for (j=0; j<i; j++) {
			v = subdivideedgenum(bm, e, &temp, j, i, params, &ne,
			                     verts[a], verts[b]);
			lines[i+1][j+1] = v;

			BMO_SetFlag(bm, ne, ELE_INNER);
		}
	}
	

/*     v5
      / \
 s v6/---\ v4 s
    / \ / \
sv7/---v---\ v3 s
  /  \/  \/ \
 v8--v0--v1--v2
    s    s
*/
	for (i=1; i<numcuts+1; i++) {
		for (j=0; j<i; j++) {
			e= connect_smallest_face(bm, lines[i][j], lines[i+1][j+1],
				           &nf);

			BMO_SetFlag(bm, e, ELE_INNER);
			BMO_SetFlag(bm, nf, ELE_INNER);

			e= connect_smallest_face(bm,lines[i][j+1],lines[i+1][j+1],
				           &nf);

			BMO_SetFlag(bm, e, ELE_INNER);
			BMO_SetFlag(bm, nf, ELE_INNER);
		}
	}

cleanup:
	for (i=1; i<numcuts+2; i++) {
		if (lines[i]) MEM_freeN(lines[i]);
	}

	MEM_freeN(lines);
}

subdpattern t_3edge = {
	{1, 1, 1},
	t_3edge_split,
	3,
};


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
	&t_1edge,
	&t_2edge,
	&t_3edge,
};

#define PLEN	(sizeof(patterns) / sizeof(void*))

typedef struct subd_facedata {
	BMVert *start; subdpattern *pat;
} subd_facedata;

void esubdivide_exec(BMesh *bmesh, BMOperator *op)
{
	BMOpSlot *einput;
	BMEdge *edge, **edges = NULL;
	V_DECLARE(edges);
	BMFace *face;
	BMLoop *nl;
	BMVert **verts = NULL;
	V_DECLARE(verts);
	BMIter fiter, liter;
	subdpattern *pat;
	subdparams params;
	subd_facedata *facedata = NULL;
	V_DECLARE(facedata);
	float rad;
	int i, j, matched, a, b, numcuts, flag;
	
	BMO_Flag_Buffer(bmesh, op, "edges", SUBD_SPLIT);
	
	numcuts = BMO_GetSlot(op, "numcuts")->data.i;
	flag = BMO_GetSlot(op, "flag")->data.i;
	rad = BMO_GetSlot(op, "radius")->data.f;

	einput = BMO_GetSlot(op, "edges");
	
	/*first go through and tag edges*/
	BMO_Flag_To_Slot(bmesh, op, "edges",
	         SUBD_SPLIT, BM_EDGE);

	params.flag = flag;
	params.numcuts = numcuts;
	params.op = op;
	params.rad = rad;

	BMO_Mapping_To_Flag(bmesh, op, "custompatterns",
	                    FACE_CUSTOMFILL);

	BMO_Mapping_To_Flag(bmesh, op, "edgepercents",
	                    EDGE_PERCENT);

	for (face=BMIter_New(&fiter, bmesh, BM_FACES_OF_MESH, NULL);
	     face; face=BMIter_Step(&fiter)) {
		/*figure out which pattern to use*/

		V_RESET(edges);
		V_RESET(verts);
		i = 0;
		for (nl=BMIter_New(&liter, bmesh, BM_LOOPS_OF_FACE, face);
		     nl; nl=BMIter_Step(&liter)) {
			V_GROW(edges);
			V_GROW(verts);
			edges[i] = nl->e;
			verts[i] = nl->v;
			i++;
		}

		if (BMO_TestFlag(bmesh, face, FACE_CUSTOMFILL)) {
			pat = BMO_Get_MapData(bmesh, op, 
				    "custompatterns", face);
			for (i=0; i<pat->len; i++) {
				matched = 1;
				for (j=0; j<pat->len; j++) {
					a = (j + i) % pat->len;
					if ((!!BMO_TestFlag(bmesh, edges[a], SUBD_SPLIT))
						!= (!!pat->seledges[j])) {
							matched = 0;
							break;
					}
				}
				if (matched) {
					V_GROW(facedata);
					b = V_COUNT(facedata)-1;
					facedata[b].pat = pat;
					facedata[b].start = verts[i];
					BMO_SetFlag(bmesh, face, SUBD_SPLIT);
					break;
				}
			}
			if (!matched) {
				/*if no match, append null element to array.*/
				V_GROW(facedata);
			}

			/*obvously don't test for other patterns matching*/
			continue;
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

	einput = BMO_GetSlot(op, "edges");

	/*go through and split edges*/
	for (i=0; i<einput->len; i++) {
		edge = ((BMEdge**)einput->data.p)[i];
		bm_subdivide_multicut(bmesh, edge,&params, edge->v1, edge->v2);
		//BM_Split_Edge_Multi(bmesh, edge, numcuts);
	}

	//if (facedata) V_FREE(facedata);
	//return;

	i = 0;
	for (face=BMIter_New(&fiter, bmesh, BM_FACES_OF_MESH, NULL);
	     face; face=BMIter_Step(&fiter)) {
		/*figure out which pattern to use*/
		V_RESET(verts);
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

		for (j=0; j<face->len; j++) {
			V_GROW(verts);
		}
		
		j = 0;
		for (nl=BMIter_New(&liter, bmesh, BM_LOOPS_OF_FACE, face);
		     nl; nl=BMIter_Step(&liter)) {
			b = (j-a+face->len) % face->len;
			verts[b] = nl->v;
			j += 1;
		}
		
		pat->connectexec(bmesh, face, verts, &params);
		i++;
	}

	if (facedata) V_FREE(facedata);
	if (edges) V_FREE(edges);
	if (verts) V_FREE(verts);

	BMO_Flag_To_Slot(bmesh, op, "outinner",
		         ELE_INNER, BM_ALL);
	BMO_Flag_To_Slot(bmesh, op, "outsplit",
		         ELE_SPLIT, BM_ALL);
}

/*editmesh-emulating function*/
void BM_esubdivideflag(Object *obedit, BMesh *bm, int selflag, float rad, 
		       int flag, int numcuts, int seltype) {
	BMOperator op;
	
	BMO_InitOpf(bm, &op, "esubd edges=%he flag=%d radius=%f numcuts=%d",
	            selflag, flag, rad, numcuts);

	BMO_Exec_Op(bm, &op);

	if (seltype == SUBDIV_SELECT_INNER) {
		BMOIter iter;
		BMHeader *ele;
		int i;
		
		ele = BMO_IterNew(&iter,bm,&op, "outinner", BM_EDGE|BM_VERT);
		for (; ele; ele=BMO_IterStep(&iter)) {
			BM_Select(bm, ele, 1);
		}
	}
	BMO_Finish_Op(bm, &op);
}

void BM_esubdivideflag_conv(Object *obedit,EditMesh *em,int selflag, float rad, 
		       int flag, int numcuts, int seltype) {
	BMesh *bm = editmesh_to_bmesh(em);
	EditMesh *em2;

	BM_esubdivideflag(obedit, bm, selflag, rad, flag, numcuts, seltype);
	em2 = bmesh_to_editmesh(bm);
	
	free_editMesh(em);
	*em = *em2;
	MEM_freeN(em2);
	BM_Free_Mesh(bm);
}