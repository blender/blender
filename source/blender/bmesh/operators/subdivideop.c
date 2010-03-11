/**
 * $Id:
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.	
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Joseph Eagar.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "MEM_guardedalloc.h"

#include "BKE_utildefines.h"
#include "BKE_tessmesh.h"

#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_ghash.h"
#include "BLI_array.h"

#include "DNA_object_types.h"
#include "DNA_windowmanager_types.h"

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

	if(params->beauty & B_SMOOTH) {
		/* we calculate an offset vector vec1[], to be added to *co */
		float len, fac, nor[3], nor1[3], nor2[3], smooth=params->smooth;

		sub_v3_v3v3(nor, vsta->co, vend->co);
		len= 0.5f*normalize_v3(nor);

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

		/* falloff for multi subdivide */
		smooth *= sqrt(fabs(1.0f - 2.0f*fabs(perc)));

		vec1[0]*= smooth*len;
		vec1[1]*= smooth*len;
		vec1[2]*= smooth*len;

		co[0] += vec1[0];
		co[1] += vec1[1];
		co[2] += vec1[2];
	}
	else if(params->beauty & B_SPHERE) { /* subdivide sphere */
		normalize_v3(co);
		co[0]*= params->smooth;
		co[1]*= params->smooth;
		co[2]*= params->smooth;
	}

	if(params->beauty & B_FRACTAL) {
		fac= params->fractal*len_v3v3(vsta->co, vend->co);
		vec1[0]= fac*(float)(0.5-BLI_drand());
		vec1[1]= fac*(float)(0.5-BLI_drand());
		vec1[2]= fac*(float)(0.5-BLI_drand());
		add_v3_v3v3(co, co, vec1);
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
		BMO_SetFlag(bm, newe, SUBD_SPLIT);

		BMO_SetFlag(bm, v, ELE_SPLIT);
		BMO_SetFlag(bm, eed, ELE_SPLIT);
		BMO_SetFlag(bm, newe, SUBD_SPLIT);
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
static void quad_1edge_split(BMesh *bm, BMFace *face,
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

subdpattern quad_1edge = {
	{1, 0, 0, 0},
	quad_1edge_split,
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
static void quad_2edge_split_path(BMesh *bm, BMFace *face, BMVert **verts, 
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

subdpattern quad_2edge_path = {
	{1, 1, 0, 0},
	quad_2edge_split_path,
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
static void quad_2edge_split_innervert(BMesh *bm, BMFace *face, BMVert **verts, 
                          subdparams *params)
{
	BMFace *nf;
	BMVert *v, *lastv;
	BMEdge *e, *ne;
	int i, numcuts = params->numcuts;
	
	lastv = verts[numcuts];

	for (i=numcuts-1; i>=0; i--) {
		e = connect_smallest_face(bm, verts[i], verts[numcuts+(numcuts-i)],
			           &nf);
		
		v = BM_Split_Edge(bm, e->v1, e, &ne, 0.5f);
		connect_smallest_face(bm, lastv, v, &nf);
		lastv = v;
	}

	connect_smallest_face(bm, lastv, verts[numcuts*2+2], &nf);	
}

subdpattern quad_2edge_innervert = {
	{1, 1, 0, 0},
	quad_2edge_split_innervert,
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
static void quad_2edge_split_fan(BMesh *bm, BMFace *face, BMVert **verts, 
                          subdparams *params)
{
	BMFace *nf;
	BMVert *v, *lastv;
	BMEdge *e, *ne;
	int i, numcuts = params->numcuts;
	
	lastv = verts[2];

	for (i=0; i<numcuts; i++) {
		connect_smallest_face(bm, verts[i], verts[numcuts*2+2], &nf);
		connect_smallest_face(bm, verts[numcuts+(numcuts-i)], 
			verts[numcuts*2+2], &nf);
	}
}

subdpattern quad_2edge_fan = {
	{1, 1, 0, 0},
	quad_2edge_split_fan,
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
static void quad_3edge_split(BMesh *bm, BMFace *face, BMVert **verts, 
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

subdpattern quad_3edge = {
	{1, 1, 1, 0},
	quad_3edge_split,
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
static void quad_4edge_subdivide(BMesh *bm, BMFace *face, BMVert **verts, 
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
static void tri_1edge_split(BMesh *bm, BMFace *face, BMVert **verts, 
                          subdparams *params)
{
	BMFace *nf;
	int i, numcuts = params->numcuts;
	
	for (i=0; i<numcuts; i++) {
		connect_smallest_face(bm, verts[i], verts[numcuts+1], &nf);
	}
}

subdpattern tri_1edge = {
	{1, 0, 0},
	tri_1edge_split,
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
static void tri_3edge_subdivide(BMesh *bm, BMFace *face, BMVert **verts, 
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

subdpattern tri_3edge = {
	{1, 1, 1},
	tri_3edge_subdivide,
	3,
};


subdpattern quad_4edge = {
	{1, 1, 1, 1},
	quad_4edge_subdivide,
	4,
};

subdpattern *patterns[] = {
	NULL, //quad single edge pattern is inserted here
	NULL, //quad corner vert pattern is inserted here
	NULL, //tri single edge pattern is inserted here
	NULL,
	&quad_3edge,
	NULL,
};

#define PLEN	(sizeof(patterns) / sizeof(void*))

typedef struct subd_facedata {
	BMVert *start; subdpattern *pat;
	int totedgesel; //only used if pat was NULL, e.g. no pattern was found
} subd_facedata;

void esubdivide_exec(BMesh *bmesh, BMOperator *op)
{
	BMOpSlot *einput;
	BMEdge *edge, **edges = NULL;
	BLI_array_declare(edges);
	BMFace *face;
	BMLoop *nl;
	BMVert **verts = NULL;
	BLI_array_declare(verts);
	BMIter fiter, liter;
	subdpattern *pat;
	subdparams params;
	subd_facedata *facedata = NULL;
	BLI_array_declare(facedata);
	BMLoop *l, **splits = NULL, **loops = NULL;
	BLI_array_declare(splits);
	BLI_array_declare(loops);
	float smooth, fractal;
	int beauty, cornertype, singleedge, gridfill;
	int i, j, matched, a, b, numcuts, totesel;
	
	BMO_Flag_Buffer(bmesh, op, "edges", SUBD_SPLIT, BM_EDGE);
	
	numcuts = BMO_Get_Int(op, "numcuts");
	smooth = BMO_Get_Float(op, "smooth");
	fractal = BMO_Get_Float(op, "fractal");
	beauty = BMO_Get_Int(op, "beauty");
	cornertype = BMO_Get_Int(op, "quadcornertype");
	singleedge = BMO_Get_Int(op, "singleedge");
	gridfill = BMO_Get_Int(op, "gridfill");
	
	patterns[1] = NULL;
	//straight cut is patterns[1] == NULL
	switch (cornertype) {
		case SUBD_PATH:
			patterns[1] = &quad_2edge_path;
			break;
		case SUBD_INNERVERT:
			patterns[1] = &quad_2edge_innervert;
			break;
		case SUBD_FAN:
			patterns[1] = &quad_2edge_fan;
			break;
	}
	
	if (singleedge) {
		patterns[0] = &quad_1edge;
		patterns[2] = &tri_1edge;
	} else {
		patterns[0] = NULL;
		patterns[2] = NULL;
	}

	if (gridfill) {
		patterns[3] = &quad_4edge;
		patterns[5] = &tri_3edge;
	} else {
		patterns[3] = NULL;
		patterns[5] = NULL;
	}
	
	/*first go through and tag edges*/
	BMO_Flag_To_Slot(bmesh, op, "edges",
	         SUBD_SPLIT, BM_EDGE);

	params.numcuts = numcuts;
	params.op = op;
	params.smooth = smooth;
	params.fractal = fractal;
	params.beauty = beauty;

	BMO_Mapping_To_Flag(bmesh, op, "custompatterns",
	                    FACE_CUSTOMFILL);

	BMO_Mapping_To_Flag(bmesh, op, "edgepercents",
	                    EDGE_PERCENT);

	for (face=BMIter_New(&fiter, bmesh, BM_FACES_OF_MESH, NULL);
	     face; face=BMIter_Step(&fiter)) {
		BMEdge *e1 = NULL, *e2 = NULL;
		float vec1[3], vec2[3];

		/*figure out which pattern to use*/

		BLI_array_empty(edges);
		BLI_array_empty(verts);
		matched = 0;

		i = 0;
		totesel = 0;
		for (nl=BMIter_New(&liter, bmesh, BM_LOOPS_OF_FACE, face);
		     nl; nl=BMIter_Step(&liter)) {
			BLI_array_growone(edges);
			BLI_array_growone(verts);
			edges[i] = nl->e;
			verts[i] = nl->v;

			if (BMO_TestFlag(bmesh, edges[i], SUBD_SPLIT)) {
				if (!e1) e1 = edges[i];
				else e2 = edges[i];

				totesel++;
			}

			i++;
		}

		/*make sure the two edges have a valid angle to each other*/
		if (totesel == 2 && (e1->v1 == e2->v1 || e1->v1 == e2->v2 
			             || e1->v2 == e2->v1 || e1->v2 == e2->v1)) {
			float angle;

			sub_v3_v3v3(vec1, e1->v2->co, e1->v1->co);
			sub_v3_v3v3(vec2, e2->v2->co, e2->v1->co);
			normalize_v3(vec1);
			normalize_v3(vec2);

			angle = INPR(vec1, vec2);
			angle = ABS(angle);
			if (ABS(angle-1.0) < 0.01)
				totesel = 0;
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
					BLI_array_growone(facedata);
					b = BLI_array_count(facedata)-1;
					facedata[b].pat = pat;
					facedata[b].start = verts[i];
					BMO_SetFlag(bmesh, face, SUBD_SPLIT);
					break;
				}
			}
			if (!matched) {
				/*if no match, append null element to array.*/
				BLI_array_growone(facedata);
			}

			/*obvously don't test for other patterns matching*/
			continue;
		}

		for (i=0; i<PLEN; i++) {
			pat = patterns[i];
			if (!pat) continue;

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
					BLI_array_growone(facedata);
					j = BLI_array_count(facedata) - 1;

					BMO_SetFlag(bmesh, face, SUBD_SPLIT);

					facedata[j].pat = pat;
					facedata[j].start = verts[a];
					break;
				}
			}
		
		}
		
		if (!matched && totesel) {
			BLI_array_growone(facedata);
			j = BLI_array_count(facedata) - 1;
			
			BMO_SetFlag(bmesh, face, SUBD_SPLIT);
			facedata[j].totedgesel = totesel;
		}
	}

	einput = BMO_GetSlot(op, "edges");

	/*go through and split edges*/
	for (i=0; i<einput->len; i++) {
		edge = ((BMEdge**)einput->data.p)[i];
		bm_subdivide_multicut(bmesh, edge, &params, edge->v1, edge->v2);
		//BM_Split_Edge_Multi(bmesh, edge, numcuts);
	}

	//if (facedata) BLI_array_free(facedata);
	//return;

	i = 0;
	for (face=BMIter_New(&fiter, bmesh, BM_FACES_OF_MESH, NULL);
	     face; face=BMIter_Step(&fiter)) {
		/*figure out which pattern to use*/
		BLI_array_empty(verts);
		if (BMO_TestFlag(bmesh, face, SUBD_SPLIT) == 0)
			continue;

		pat = facedata[i].pat;
		if (!pat && facedata[i].totedgesel == 2) { /*ok, no pattern.  we still may be able to do something.*/
			BMFace *nf;
			int vlen;
			
			BLI_array_empty(loops);
			BLI_array_empty(splits);

			/*for case of two edges, connecting them shouldn't be too hard*/
			BM_ITER(l, &liter, bmesh, BM_LOOPS_OF_FACE, face) {
				BLI_array_growone(loops);
				loops[BLI_array_count(loops)-1] = l;
			}
			
			vlen = BLI_array_count(loops);

			/*find the boundary of one of the split edges*/
			for (a=1; a<vlen; a++) {
				if (!BMO_TestFlag(bmesh, loops[a-1]->v, ELE_INNER) 
				    && BMO_TestFlag(bmesh, loops[a]->v, ELE_INNER))
					break;
			}
			
			if (BMO_TestFlag(bmesh, loops[(a+numcuts+1)%vlen]->v, ELE_INNER)) {
				b = (a+numcuts+1)%vlen;
			} else {
				/*find the boundary of the other edge.*/
				for (j=0; j<vlen; j++) {
					b = (j + a + numcuts + 1) % vlen;
					if (!BMO_TestFlag(bmesh, loops[b==0 ? vlen-1 : b-1]->v, ELE_INNER)
					    && BMO_TestFlag(bmesh, loops[b]->v, ELE_INNER))
						break;
				}
			}
			
			b += numcuts - 1;

			for (j=0; j<numcuts; j++) {
				BLI_array_growone(splits);
				splits[BLI_array_count(splits)-1] = loops[a];
				
				BLI_array_growone(splits);
				splits[BLI_array_count(splits)-1] = loops[b];

				b = (b-1) % vlen;
				a = (a+1) % vlen;
			}
			
			//BM_LegalSplits(bmesh, face, splits, BLI_array_count(splits)/2);

			for (j=0; j<BLI_array_count(splits)/2; j++) {
				if (splits[j*2]) {
					BMFace *nf;

					nf = BM_Split_Face(bmesh, face, splits[j*2]->v, splits[j*2+1]->v, &nl, NULL);
				}
			}

			i++;
			continue;
		} else if (!pat) {
			i++;
			continue;
		}

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
			BLI_array_growone(verts);
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

	if (facedata) BLI_array_free(facedata);
	if (edges) BLI_array_free(edges);
	if (verts) BLI_array_free(verts);
	BLI_array_free(splits);
	BLI_array_free(loops);

	BMO_Flag_To_Slot(bmesh, op, "outinner",
		         ELE_INNER, BM_ALL);
	BMO_Flag_To_Slot(bmesh, op, "outsplit",
		         ELE_SPLIT, BM_ALL);
	
	BMO_Flag_To_Slot(bmesh, op, "geomout",
		         ELE_INNER|ELE_SPLIT|SUBD_SPLIT, BM_ALL);
}

/*editmesh-emulating function*/
void BM_esubdivideflag(Object *obedit, BMesh *bm, int flag, float smooth, 
		       float fractal, int beauty, int numcuts, 
		       int seltype, int cornertype, int singleedge, int gridfill)
{
	BMOperator op;
	
	BMO_InitOpf(bm, &op, "esubd edges=%he smooth=%f fractal=%f "
		             "beauty=%d numcuts=%d quadcornertype=%d singleedge=%d "
			     "gridfill=%d",
			     flag, smooth, fractal, beauty, numcuts,
			     cornertype, singleedge, gridfill);
	
	BMO_Exec_Op(bm, &op);
	
	if (seltype == SUBDIV_SELECT_INNER) {
		BMOIter iter;
		BMHeader *ele;
		int i;
		
		ele = BMO_IterNew(&iter, bm, &op, "outinner", BM_EDGE|BM_VERT);
		for (; ele; ele=BMO_IterStep(&iter)) {
			BM_Select(bm, ele, 1);
		}
	} else if (seltype == SUBDIV_SELECT_LOOPCUT) {
		BMOIter iter;
		BMHeader *ele;
		int i;
		
		/*deselect input*/
		BM_clear_flag_all(bm, BM_SELECT);

		ele = BMO_IterNew(&iter, bm, &op, "outinner", BM_EDGE|BM_VERT);
		for (; ele; ele=BMO_IterStep(&iter)) {
			BM_Select(bm, ele, 1);

			if (ele->type == BM_VERT) {
				BMEdge *e;
				BMIter eiter;

				BM_ITER(e, &eiter, bm, BM_EDGES_OF_VERT, ele) {
					if (!BM_TestHFlag(e, BM_SELECT) && BM_TestHFlag(e->v1, BM_SELECT) 
													&& BM_TestHFlag(e->v2, BM_SELECT)) {
						BM_SetHFlag(e, BM_SELECT);
						bm->totedgesel += 1;
					} else if (BM_TestHFlag(e, BM_SELECT) && (!BM_TestHFlag(e->v1, BM_SELECT) 
														  || !BM_TestHFlag(e->v2, BM_SELECT))) {
						BM_ClearHFlag(e, BM_SELECT);
						bm->totedgesel -= 1;		
					}
				}
			}
		}
	}

	BMO_Finish_Op(bm, &op);
}

#if 0
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
#endif

void esplit_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMEdge *e;
	subdparams params;

	params.numcuts = BMO_GetSlot(op, "numcuts")->data.i;
	params.op = op;
	
	/*go through and split edges*/
	BMO_ITER(e, &siter, bm, op, "edges", BM_EDGE) {
		bm_subdivide_multicut(bm, e, &params, e->v1, e->v2);
	}

	BMO_Flag_To_Slot(bm, op, "outsplit",
		         ELE_SPLIT, BM_ALL);
}

