/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <string.h>
#include <stdlib.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"

#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_utildefines.h"

#include "BLI_arithb.h"
#include "BLI_edgehash.h"

#include "BIF_editsima.h"
#include "BIF_space.h"

#include "blendef.h"
#include "mydevice.h"

#include "ONL_opennl.h"
#include "BDR_unwrapper.h"

/* Implementation Least Squares Conformal Maps parameterization, based on
 * chapter 2 of:
 * Bruno Levy, Sylvain Petitjean, Nicolas Ray, Jerome Maillot. Least Squares
 * Conformal Maps for Automatic Texture Atlas Generation. In Siggraph 2002,
 * July 2002.
 */
 
/* Data structure defines */
#define LSCM_SEAM1   1
#define LSCM_SEAM2   2
#define LSCM_INDEXED 4
#define LSCM_PINNED  8

/* LscmVert = One UV */
typedef struct LscmVert {
	int v, v1, v2;            /* vertex indices */
	int index;                /* index in solver */
	short tf_index;           /* index in tface (0, 1, 2 or 3) */
	short flag;               /* see above LSCM constants */
	TFace *tf;                /* original tface */
} LscmVert;

/* QuickSort helper function, sort by vertex id */
static int comp_lscmvert(const void *u1, const void *u2)
{
	LscmVert *v1, *v2;
	
	v1= *((LscmVert**)u1);
	v2= *((LscmVert**)u2);

	if (v1->v > v2->v) return 1;
	else if (v1->v < v2->v) return -1;
	return 0;
}

/* Hashed edge table utility */

static void hash_add_face(EdgeHash *ehash, MFace *mf)
{
	BLI_edgehash_insert(ehash, mf->v1, mf->v2, NULL);
	BLI_edgehash_insert(ehash, mf->v2, mf->v3, NULL);
	if(mf->v4) {
		BLI_edgehash_insert(ehash, mf->v3, mf->v4, NULL);
		BLI_edgehash_insert(ehash, mf->v4, mf->v1, NULL);
	}
	else
		BLI_edgehash_insert(ehash, mf->v3, mf->v1, NULL);
}

/* divide selected faces in groups, based on seams. note that group numbering
   starts at 1 */
static int make_seam_groups(Mesh *me, int **seamgroups)
{
	int a, b, gid;
	TFace *tf, *tface;
	MFace *mf, *mface;
	int *gf, *gface, *groups;
	EdgeHash *ehash;
	int doit, mark;

	if(!me || !me->tface) return 0;

	groups= (int*)MEM_callocN(sizeof(int)*me->totface, "SeamGroups");

	ehash= BLI_edgehash_new();

	mface= (MFace*)me->mface;
	tface= (TFace*)me->tface;
	gface= groups;
	gid= 0;
	for(b=me->totface; b>0; b--, mface++, tface++, gface++) {
		if(!(tface->flag & TF_SELECT) || *gface!=0) continue;

		if(gid != 0)
			BLI_edgehash_clear(ehash, NULL);

		gid++;
		*gface= gid;
		mark= 0;
		doit= 1;


		while(doit) {
			doit= 0;
		
			/* select connected: fill array */
			tf= tface;
			mf= mface;
			gf= gface;
			a= b;
			while(a--) {
				if(tf->flag & TF_HIDE);
				else if(tf->flag & TF_SELECT && *gf==gid) {
					hash_add_face(ehash, mf);
				}
				tf++; mf++; gf++;
			}
		
			/* select the faces using array
			 * consider faces connected when they share one non-seam edge */
			tf= tface;
			mf= mface;
			gf= gface;
			a= b;
			while(a--) {
				if(tf->flag & TF_HIDE);
				else if(tf->flag & TF_SELECT && *gf==0) {
					mark= 0;
	
					if(!(tf->unwrap & TF_SEAM1))
						if(BLI_edgehash_haskey(ehash, mf->v1, mf->v2))
							mark= 1;
					if(!(tf->unwrap & TF_SEAM2))
						if(BLI_edgehash_haskey(ehash, mf->v2, mf->v3))
							mark= 1;
					if(!(tf->unwrap & TF_SEAM3)) {
						if(mf->v4) {
							if(BLI_edgehash_haskey(ehash, mf->v3, mf->v4))
								mark= 1;
						}
						else if(BLI_edgehash_haskey(ehash, mf->v3, mf->v1))
							mark= 1;
					}
					if(mf->v4 && !(tf->unwrap & TF_SEAM4))
						if(BLI_edgehash_haskey(ehash, mf->v4, mf->v1))
							mark= 1;
	
					if(mark) {
						*gf= gid;
						doit= 1;
					}
				}
				tf++; mf++; gf++;
			}
		}
	}

	BLI_edgehash_free(ehash, NULL);
	*seamgroups= groups;

	return gid;
}

static void lscm_rotate_vert(int a, LscmVert **sortvert, int v2, int index)
{
	LscmVert **sv, *v;
	int found, b;

	/* starting from edge sortvert->v,v2, rotate around vertex and set
	 * index until a seam or an already marked tri is encountered */
	found = 1;

	while(found) {
		found= 0;
		sv=sortvert;

		for(b=a; b>0 && ((*sv)->v == (*sortvert)->v) && !found; b--, sv++) {
			v= *sv;

			if(v->flag & LSCM_INDEXED);
			else if(v->v1 == v2) {
				v2= v->v2;

				if(v->flag & LSCM_SEAM1) break;

				v->index= index;
				v->flag |= LSCM_INDEXED;
				found= 1;
				break;
			}
			else if(v->v2==v2) {
				v2= v->v1;

				if(v->flag & LSCM_SEAM2) break;

				v->index= index;
				v->flag |= LSCM_INDEXED;
				found= 1;
				break;
			}
		}
	}
}

static int lscm_vertex_set_index(int a, LscmVert **sortvert, int totindex)
{
	LscmVert **sv, *v;
	int index, b;

	/* rotate over 'wheel' of faces around vertex, incrementing the index
	   everytime we meet a seam, or no next connected face is found.
	   repeat this until we have and id for all verts.
	   if mesh is non-manifold, non-manifold edges will be cut randomly */

	index= totindex;
	sv= sortvert;

	for(b=a; b>0 && ((*sv)->v == (*sortvert)->v); b--, sv++) {
		v= *sv;

		if(v->flag & LSCM_INDEXED) continue;

		v->index= index;
		v->flag |= LSCM_INDEXED;

		lscm_rotate_vert(b, sv, v->v1, index);
		lscm_rotate_vert(b, sv, v->v2, index);

		index++;
	}

	return index;
}

static int lscm_set_indices(LscmVert **sortvert, int totvert)
{
	LscmVert *v, **sv;
	int a, lastvert, totindex;

	totindex= 0;
	lastvert= -1;
	sv= sortvert;

	for(a=totvert; a>0; a--, sv++) {
		v= *sv;
		if(v->v != lastvert) {
			totindex= lscm_vertex_set_index(a, sv, totindex);
			lastvert= v->v;
		}
	}

	return totindex;
}

static void lscm_normalize(float *co, float *center, float radius)
{
	/* normalize relative to complete surface */
	VecSubf(co, co, center);
	VecMulf(co, (float)1.0/radius);
}

static void lscm_add_triangle(float *v1, float *v2, float *v3, int vid1, int vid2, int vid3, float *center, float radius)
{
	float x[3], y[3], z[3], sub[3], z1[2], z2[2];
	int id0[2], id1[2], id2[2];

	/* project 3d triangle
	 * edge length is lost, as this algorithm is angle based */
	lscm_normalize(v1, center, radius);
	lscm_normalize(v2, center, radius);
	lscm_normalize(v3, center, radius);

	VecSubf(x, v2, v1);
	Normalise(x);

	VecSubf(sub, v3, v1);
	Crossf(z, x, sub);
	Normalise(z);

	Crossf(y, z, x);

	/* reduce to two 2d vectors */
	VecSubf(sub, v2, v1);
	z1[0]= Normalise(sub);
	z1[1]= 0;

	VecSubf(sub, v3, v1);
	z2[0]= Inpf(sub, x);
	z2[1]= Inpf(sub, y);

	/* split id's up for u and v
	   id = u, id + 1 = v */
	id0[0]= 2*vid1;
	id0[1]= 2*vid1 + 1;
	id1[0]= 2*vid2;
	id1[1]= 2*vid2 + 1;
	id2[0]= 2*vid3;
	id2[1]= 2*vid3 + 1;

	/* The LSCM Equation:
	 * ------------------
	 * (u,v) are the uv coords we are looking for -> complex number u + i*v
	 * (x,y) are the above calculated local coords -> complex number x + i*y
	 * Uk = uk + i*vk
	 * Zk = xk + i*yk (= zk[0] + i*zk[1] in the code)
	 * 
	 * That makes the equation:
	 * (Z1 - Z0)(U2 - U0) = (Z2 - Z0)(U1 - U0)
	 *
	 * x0, y0 and y1 were made zero by projecting the triangle:
	 * (x1 + i*y1)(u2 + i*v2 - u0 - i*v0) = (x2 + i*y2)(u1 + i*v1 - u0 - i*v0)
	 *
	 * this gives the following coefficients:
	 * u0 * ((-x1 + x2) + i*(y2))
	 * v0 * ((-y2) + i*(-x1 + x2))
	 * u1 * ((-x2) + i*(-y2))
	 * v1 * ((y2) + i*(-x2))
	 * u2 * (x1)
	 * v2 * (i*(x1))
	 */

	/* real part */
	nlBegin(NL_ROW);
	nlCoefficient(id0[0], -z1[0] + z2[0]);
	nlCoefficient(id0[1], -z2[1]        );
	nlCoefficient(id1[0], -z2[0]        );
	nlCoefficient(id1[1],  z2[1]        );
	nlCoefficient(id2[0],  z1[0]        );
	nlEnd(NL_ROW);

	/* imaginary  part */
	nlBegin(NL_ROW);
	nlCoefficient(id0[0],  z2[1]        );
	nlCoefficient(id0[1], -z1[0] + z2[0]);
	nlCoefficient(id1[0], -z2[1]        );
	nlCoefficient(id1[1], -z2[0]        );
	nlCoefficient(id2[1],  z1[0]        );
	nlEnd(NL_ROW);
}

static float lscm_angle_cos(float *v1, float *v2, float *v3)
{
    float vec1[3], vec2[3];

	VecSubf(vec1, v2, v1);
	VecSubf(vec2, v3, v1);
	Normalise(vec1);
	Normalise(vec2);

	return vec1[0]*vec2[0] + vec1[1]*vec2[1] + vec1[2]*vec2[2];
}

static int lscm_build_vertex_data(Mesh *me, int *groups, int gid, LscmVert **lscm_vertices, LscmVert ***sort_vertices)
{
	MVert *mv;
	MFace *mf;
	TFace *tf;
	int *gf, totvert, a;
	LscmVert *lscmvert, **sortvert;
	LscmVert *v1, *v2, *v3, **sv1, **sv2, **sv3;
	float a1, a2;

	/* determine size for malloc */
	totvert= 0;
	mv = me->mvert;
	mf= me->mface;
	tf= me->tface;
	gf= groups;
	a1 = a2 = 0;

	for(a=me->totface; a>0; a--) {
		if(*gf==gid) {

			totvert += 3;
			if(mf->v4) totvert +=3; 
		}
		tf++; mf++; gf++;
	}

	/* a list per face vertices */
	lscmvert= (LscmVert*)MEM_mallocN(sizeof(LscmVert)*totvert,"LscmVerts");
	/* the above list sorted by vertex id */
	sortvert= (LscmVert**)MEM_mallocN(sizeof(LscmVert*)*totvert, "LscmVSort");

	/* actually build the list (including virtual triangulation) */
	mf= me->mface;
	tf= me->tface;
	gf= groups;

	v1= lscmvert;
	v2= lscmvert + 1;
	v3= lscmvert + 2;

	sv1= sortvert;
	sv2= sortvert + 1;
	sv3= sortvert + 2;

	/* warning: ugly code :) */
	for(a=me->totface; a>0; a--) {
		if(*gf==gid) {
			/* determine triangulation direction, to avoid degenerate
			   triangles (small cos = degenerate). */
			if(mf->v4) {
				a1 = lscm_angle_cos((mv+mf->v1)->co, (mv+mf->v2)->co, (mv+mf->v3)->co);
				a1 += lscm_angle_cos((mv+mf->v2)->co, (mv+mf->v1)->co, (mv+mf->v3)->co);
				a1 += lscm_angle_cos((mv+mf->v3)->co, (mv+mf->v1)->co, (mv+mf->v2)->co);

				a2 = lscm_angle_cos((mv+mf->v1)->co, (mv+mf->v2)->co, (mv+mf->v4)->co);
				a2 += lscm_angle_cos((mv+mf->v2)->co, (mv+mf->v1)->co, (mv+mf->v4)->co);
				a2 += lscm_angle_cos((mv+mf->v4)->co, (mv+mf->v1)->co, (mv+mf->v2)->co);
			}

			a1 = 0.0; a2 = 1.0;

			if(!mf->v4 || a1 > a2) {
				v1->v= mf->v1;
				v2->v= mf->v2;
				v3->v= mf->v3;

				v1->tf_index= 0;
				v2->tf_index= 1;
				v3->tf_index= 2;

				v1->flag= v2->flag= v3->flag= 0;

				v1->v1= v2->v;
				v1->v2= v3->v;

				v2->v1= v1->v;
				v2->v2= v3->v;

				v3->v1= v1->v;
				v3->v2= v2->v;

				v1->tf= v2->tf= v3->tf= tf;

				*sv1= v1;
				*sv2= v2;
				*sv3= v3;

				if(tf->unwrap & TF_SEAM1) {
					v1->flag |= LSCM_SEAM1;
					v2->flag |= LSCM_SEAM1;
				}
	
				if(tf->unwrap & TF_SEAM2) {
					v2->flag |= LSCM_SEAM2;
					v3->flag |= LSCM_SEAM2;
				}

				if(!mf->v4 && tf->unwrap & TF_SEAM3) {
					v1->flag |= LSCM_SEAM2;
					v3->flag |= LSCM_SEAM1;
				}

				v1 += 3; v2 += 3; v3 += 3;
				sv1 += 3; sv2 += 3; sv3 += 3;
			}

			if(mf->v4 && a1 > a2) {
				v1->v= mf->v1;
				v2->v= mf->v3;
				v3->v= mf->v4;

				v1->tf_index= 0;
				v2->tf_index= 2;
				v3->tf_index= 3;

				v1->flag= v2->flag= v3->flag= 0;

				v1->v1= v2->v;
				v1->v2= v3->v;

				v2->v1= v3->v;
				v2->v2= v1->v;
	
				v3->v1= v1->v;
				v3->v2= v2->v;

				v1->tf= v2->tf= v3->tf= tf;

				*sv1= v1;
				*sv2= v2;
				*sv3= v3;

				if(tf->unwrap & TF_SEAM3) {
					v2->flag |= LSCM_SEAM1;
					v3->flag |= LSCM_SEAM2;
				}
	
				if(tf->unwrap & TF_SEAM4) {
					v1->flag |= LSCM_SEAM2;
					v3->flag |= LSCM_SEAM1;
				}

				v1 += 3; v2 += 3; v3 += 3;
				sv1 += 3; sv2 += 3; sv3 += 3;
			}

			if(mf->v4 && a1 <= a2) {
				v1->v= mf->v1;
				v2->v= mf->v2;
				v3->v= mf->v4;

				v1->tf_index= 0;
				v2->tf_index= 1;
				v3->tf_index= 3;

				v1->flag= v2->flag= v3->flag= 0;

				v1->v1= v2->v;
				v1->v2= v3->v;

				v2->v1= v1->v;
				v2->v2= v3->v;

				v3->v1= v1->v;
				v3->v2= v2->v;

				v1->tf= v2->tf= v3->tf= tf;

				*sv1= v1;
				*sv2= v2;
				*sv3= v3;

				if(tf->unwrap & TF_SEAM1) {
					v1->flag |= LSCM_SEAM1;
					v2->flag |= LSCM_SEAM1;
				}
	
				if(tf->unwrap & TF_SEAM4) {
					v1->flag |= LSCM_SEAM2;
					v3->flag |= LSCM_SEAM1;
				}

				v1 += 3; v2 += 3; v3 += 3;
				sv1 += 3; sv2 += 3; sv3 += 3;

				/* -- */

				v1->v= mf->v2;
				v2->v= mf->v3;
				v3->v= mf->v4;

				v1->tf_index= 1;
				v2->tf_index= 2;
				v3->tf_index= 3;

				v1->flag= v2->flag= v3->flag= 0;

				v1->v1= v2->v;
				v1->v2= v3->v;

				v2->v1= v1->v;
				v2->v2= v3->v;
	
				v3->v1= v1->v;
				v3->v2= v2->v;

				v1->tf= v2->tf= v3->tf= tf;

				*sv1= v1;
				*sv2= v2;
				*sv3= v3;

				if(tf->unwrap & TF_SEAM2) {
					v1->flag |= LSCM_SEAM1;
					v2->flag |= LSCM_SEAM1;
				}
	
				if(tf->unwrap & TF_SEAM3) {
					v2->flag |= LSCM_SEAM2;
					v3->flag |= LSCM_SEAM2;
				}

				v1 += 3; v2 += 3; v3 += 3;
				sv1 += 3; sv2 += 3; sv3 += 3;
			}

		}
		tf++; mf++; gf++;
	}

	/* sort by vertex id */
	qsort(sortvert, totvert, sizeof(LscmVert*), comp_lscmvert);
	
	*lscm_vertices= lscmvert;
	*sort_vertices= sortvert;
	return totvert;
}

static void lscm_min_max_cent_rad(Mesh *me, LscmVert **sortvert, int totvert, float *min, float *max, float *center, float *radius)
{
	MVert *mv= me->mvert;
	LscmVert *v, **sv;
	int a, lastvert, vertcount;
	float *co, sub[3];
	
	/* find min, max and center */
	center[0]= center[1]= center[2]= 0.0;
	INIT_MINMAX(min, max);

	vertcount= 0;
	lastvert= -1;
	sv= sortvert;

	for(a=totvert; a>0; a--, sv++) {
		v= *sv;
		if(v->v != lastvert) {
			co= (mv+v->v)->co;

			VecAddf(center, center, (mv+v->v)->co);
			DO_MINMAX(co, min, max);

			vertcount++;
			lastvert= v->v;
		}
	}

	VecMulf(center, (float)1.0/(float)vertcount);

	/* find radius */
	VecSubf(sub, center, max);
	*radius= Normalise(sub);

	if(*radius < 1e-20)
		*radius= 1.0;
}

static void lscm_projection_axes(float *min, float *max, float *p1, float *p2)
{
	float dx, dy, dz;

	dx= max[0] - min[0];
	dy= max[1] - min[1];
	dz= max[2] - min[2];

	p1[0]= p1[1]= p1[2]= 0.0;
	p2[0]= p2[1]= p2[2]= 0.0;

	if(dx < dy && dx < dz) {
		if(dy > dz) p1[1]= p2[2]= 1.0;   /* y, z */
		else p1[2]= p2[1]= 1.0;          /* z, y */
	}
	else if(dy < dx && dy < dz) {
		if(dx > dz) p1[0]= p2[2]= 1.0;   /* x, z */
		else p1[2]= p2[0]= 1.0;          /* z, x */
	}
	else {
		if(dx > dy) p1[0]= p2[1]= 1.0;   /* x, y */
		else p1[1]= p2[0]= 1.0;          /* y, x */
	}
}

static void lscm_set_initial_solution(Mesh *me, LscmVert **sortvert, int totvert, float *p1, float *p2, int *vertex_min, int *vertex_max)
{
	float umin, umax, *uv, *co;
	int vmin, vmax, a;
	LscmVert **sv, *v;
	MVert *mv= me->mvert;

	umin= 1.0e30;
	umax= -1.0e30;

	vmin= 0;
	vmax= 2;

	sv= sortvert;

	for(a=totvert; a>0; a--, sv++) {
		v= *sv;
		co= (mv+v->v)->co;
		uv= v->tf->uv[v->tf_index];

		uv[0]= Inpf(co, p1);
		uv[1]= Inpf(co, p2);
		
		if(uv[0] < umin) {
			vmin= v->index;
			umin= uv[0];
		}
		if(uv[0] > umax) {
			vmax= v->index;
			umax= uv[0];
		}

		nlSetVariable(2*v->index, uv[0]);
		nlSetVariable(2*v->index + 1, uv[1]);
	}

	*vertex_min= vmin;
	*vertex_max= vmax;
}

static void lscm_set_pinned_solution(Mesh *me, LscmVert **sortvert, int totvert, int *pinned)
{
	float min[2], max[2], *uv, *co;
	int a, pin;
	LscmVert **sv, *v;
	MVert *mv= me->mvert;

	INIT_MINMAX2(min, max);
	*pinned= 0;

	sv= sortvert;

	for(a=totvert; a>0; a--, sv++) {
		v= *sv;
		co= (mv+v->v)->co;
		uv= v->tf->uv[v->tf_index];

		pin = ((v->tf->unwrap & TF_PIN1) && (v->tf_index == 0)) ||
		      ((v->tf->unwrap & TF_PIN2) && (v->tf_index == 1)) ||
		      ((v->tf->unwrap & TF_PIN3) && (v->tf_index == 2)) ||
		      ((v->tf->unwrap & TF_PIN4) && (v->tf_index == 3)); 

		nlSetVariable(2*v->index, uv[0]);
		nlSetVariable(2*v->index + 1, uv[1]);

        if(pin){
			DO_MINMAX2(uv, min, max);

			*pinned += 1;

			nlLockVariable(2*v->index);
			nlLockVariable(2*v->index + 1);
		}
	}

	if (*pinned){
		/* abuse umax vmax for caculating euclidian distance */
		max[0] -= min[0];
		max[1] -= min[1];

		/* check for degenerated pinning box */
		if (((max[0]*max[0])+(max[1]*max[1])) < 1e-10)
			*pinned = -1;
	}
}


static void lscm_build_matrix(Mesh *me, LscmVert *lscmvert, int *groups, int gid, float *center, float radius)
{
	MVert *mv= me->mvert;
	MFace *mf;
	TFace *tf;
	int *gf, a, id1, id2, id3;
	LscmVert *v;
	float co1[3], co2[3], co3[3];

	nlBegin(NL_MATRIX);

	mf= me->mface;
	tf= me->tface;
	gf= groups;
	v= lscmvert;

	for(a=me->totface; a>0; a--) {
		if(*gf==gid) {
			VecCopyf(co1, (mv+v->v)->co);
			id1= v->index; v++;
			VecCopyf(co2, (mv+v->v)->co);
			id2= v->index; v++;
			VecCopyf(co3, (mv+v->v)->co);
			id3= v->index; v++;
			lscm_add_triangle(co1, co2, co3, id1, id2, id3, center, radius);

			if(mf->v4) {
				VecCopyf(co1, (mv+v->v)->co);
				id1= v->index; v++;
				VecCopyf(co2, (mv+v->v)->co);
				id2= v->index; v++;
				VecCopyf(co3, (mv+v->v)->co);
				id3= v->index; v++;
				lscm_add_triangle(co1, co2, co3, id1, id2, id3, center, radius);
			} 
		}
		tf++; mf++; gf++;
	}

	nlEnd(NL_MATRIX);
}

static void lscm_load_solution(Mesh *me, LscmVert *lscmvert, int *groups, int gid)
{
	MFace *mf;
	TFace *tf;
	int *gf, a, b;
	LscmVert *v;
	float *uv;

	mf= me->mface;
	tf= me->tface;
	gf= groups;
	v= lscmvert;

	for(a=me->totface; a>0; a--) {
		if(*gf==gid) {

			if(mf->v4) b= 6;
			else b=3;

			/* index= u, index + 1= v */
			while(b > 0) {
				uv= v->tf->uv[v->tf_index];

				uv[0]= nlGetVariable(2*v->index);
				uv[1]= nlGetVariable(2*v->index + 1);

				v++;
				b--;
			}
		}
		tf++; mf++; gf++;
	}
}

static int unwrap_lscm_face_group(Mesh *me, int *groups, int gid)
{
	LscmVert *lscmvert, **sortvert;
	int totindex, totvert, vmin, vmax,pinned;
	float min[3], max[3], center[3], radius, p1[3], p2[3];

	/* build the data structures */
	totvert= lscm_build_vertex_data(me, groups, gid, &lscmvert, &sortvert);

	/* calculate min, max, center and radius */
	lscm_min_max_cent_rad(me, sortvert, totvert, min, max, center, &radius);

	/* index distinct vertices */
	totindex= lscm_set_indices(sortvert, totvert);

	/* create solver */
	nlNewContext();
	nlSolverParameteri(NL_NB_VARIABLES, 2*totindex);
	nlSolverParameteri(NL_LEAST_SQUARES, NL_TRUE);

	nlBegin(NL_SYSTEM);

	/* find axes for projecting initial solutions on */
	lscm_projection_axes(min, max, p1, p2);
    /* see if pinned data is avail and set on fly */
	lscm_set_pinned_solution(me, sortvert, totvert, &pinned);

    if(pinned < 0); /* really small pinned uv's: won't see difference anyway */
	else { 
		/* auto pinning */
		if(pinned < 2) 
		{
			/* set initial solution and locate two extrema vertices to pin */
			lscm_set_initial_solution(me,sortvert,totvert,p1,p2,&vmin,&vmax);

			/* pin 2 uv's */
			nlLockVariable(2*vmin);
			nlLockVariable(2*vmin + 1);
			nlLockVariable(2*vmax);
			nlLockVariable(2*vmax + 1);
		}

		/* add triangles to the solver */
		lscm_build_matrix(me, lscmvert, groups, gid, center, radius);
		
		nlEnd(NL_SYSTEM);
		
		/* LSCM solver magic! */
		nlSolve();
		
		/* load new uv's: will be projected uv's if solving failed  */
		lscm_load_solution(me, lscmvert, groups, gid);
    }

	nlDeleteContext(nlGetCurrent());
	MEM_freeN(lscmvert);
	MEM_freeN(sortvert);
	return (pinned);
}

static void seam_group_bbox(Mesh *me, int *groups, int gid, float *min, float *max)
{
	MFace *mf;
	TFace *tf;
	int *gf, a;

	INIT_MINMAX2(min, max);

	mf= me->mface;
	tf= me->tface;
	gf= groups;

	for(a=me->totface; a>0; a--) {
		if((gid!=0 && *gf==gid) || (gid==0 && *gf)) {
			
			DO_MINMAX2(tf->uv[0], min, max)
			DO_MINMAX2(tf->uv[1], min, max)
			DO_MINMAX2(tf->uv[2], min, max)

			if(mf->v4) { 
				DO_MINMAX2(tf->uv[3], min, max)
			}
		}
		tf++; mf++; gf++;
	}
}

static void seam_group_scale(Mesh *me, int *groups, int gid, float scale)
{
	MFace *mf;
	TFace *tf;
	int *gf, a;

	mf= me->mface;
	tf= me->tface;
	gf= groups;

	for(a=me->totface; a>0; a--) {
		if((gid!=0 && *gf==gid) || (gid==0 && *gf)) {
			
			Vec2Mulf(tf->uv[0], scale);
			Vec2Mulf(tf->uv[1], scale);
			Vec2Mulf(tf->uv[2], scale);
			if(mf->v4) Vec2Mulf(tf->uv[3], scale);
		}
		tf++; mf++; gf++;
	}
}

static void seam_group_move(Mesh *me, int *groups, int gid, float add[2])
{
	MFace *mf;
	TFace *tf;
	int *gf, a;

	mf= me->mface;
	tf= me->tface;
	gf= groups;

	for(a=me->totface; a>0; a--) {
		if((gid!=0 && *gf==gid) || (gid==0 && *gf)) {
			
			Vec2Addf(tf->uv[0], tf->uv[0], add);
			Vec2Addf(tf->uv[1], tf->uv[1], add);
			Vec2Addf(tf->uv[2], tf->uv[2], add);
			if(mf->v4) Vec2Addf(tf->uv[3], tf->uv[3], add);
		}
		tf++; mf++; gf++;
	}
}

/* put group withing (0,0)->(1,1) boundbox */
static void seam_group_normalize(Mesh *me, int *groups, int gid)
{
	float min[2], max[2], sx, sy, scale, add[2];

	seam_group_bbox(me, groups, gid, min, max);

	sx= (max[0]-min[0]);
	sy= (max[1]-min[1]);

	scale= MAX2(sx, sy);
	scale= (1.0/scale);

	add[0]= -min[0];
	add[1]= -min[1];

	seam_group_move(me, groups, gid, add);
	seam_group_scale(me, groups, gid, scale);
}

/* get scale relative to mesh */
static float seam_group_relative_scale(Mesh *me, int *groups, int gid)
{
	MVert *mv= me->mvert;
	MFace *mf;
	TFace *tf;
	int *gf, a;
	float len_xyz, len_uv;

	len_xyz= 0.0;
	len_uv= 0.0;
	mf= me->mface;
	tf= me->tface;
	gf= groups;

	for(a=me->totface; a>0; a--) {
		if(*gf==gid) {
			
			len_uv += Vec2Lenf(tf->uv[0], tf->uv[1]);
			len_xyz += VecLenf((mv+mf->v1)->co, (mv+mf->v2)->co);

			len_uv += Vec2Lenf(tf->uv[1], tf->uv[2]);
			len_xyz += VecLenf((mv+mf->v2)->co, (mv+mf->v3)->co);

			if(mf->v4) { 

				len_uv += Vec2Lenf(tf->uv[2], tf->uv[3]);
				len_xyz += VecLenf((mv+mf->v3)->co, (mv+mf->v4)->co);
				
				len_uv += Vec2Lenf(tf->uv[3], tf->uv[0]);
				len_xyz += VecLenf((mv+mf->v4)->co, (mv+mf->v1)->co);
			}
			else {
				len_uv += Vec2Lenf(tf->uv[2], tf->uv[0]);
				len_xyz += VecLenf((mv+mf->v3)->co, (mv+mf->v1)->co);
			}
		}
		tf++; mf++; gf++;
	}

	return (len_uv/len_xyz);
}

/* very primitive packing */
static void pack_seam_groups(Mesh *me, int *groups, int totgroup)
{
	float *groupscale, minscale, scale, add[2], groupw;
	float dx, dy, packx, packy, min[2], max[2], rowh;
	int a;

	groupscale = (float*)MEM_mallocN(sizeof(float)*totgroup, "SeamGroupScale");

	minscale= 1e30;

	for(a=0; a<totgroup; a++) {
		groupscale[a]= seam_group_relative_scale(me, groups, a+1);
		minscale= MIN2(groupscale[a], minscale);
	}

	packx= packy= 0.0;
	rowh= 0.0;
	groupw= 1.0/sqrt(totgroup);

	for(a=0; a<totgroup; a++) {

		/* scale so all groups have the same size relative to the mesh */
		scale = minscale/groupscale[a];
		scale *= groupw;

		seam_group_bbox(me, groups, a+1, min, max);
		dx= (max[0]-min[0])*scale;
		dy= (max[1]-min[1])*scale;

		/* for padding */
		dx += 0.01;
		dy += 0.01;

		add[0]= add[1]= 0.0;

		if(dx > 1.0) {
			add[0]= 0.0;
			add[1]= packy;

			packy += dy;
			packx= 0.0;
			rowh= 0.0;
		}
		else if(dx <= (1.0-packx)) {
			add[0]= packx;
			add[1]= packy;

			packx += dx;
			rowh= MAX2(rowh, dy);
		}
		else {
			packy += rowh;
			packx= dx;
			rowh= dy;

			add[0]= 0.0;
			add[1]= packy;
		}

		/* for padding */
		add[0] += 0.005;
		add[1] += 0.005;

		seam_group_scale(me, groups, a+1, scale);
		seam_group_move(me, groups, a+1, add);
	}

	MEM_freeN(groupscale);

	seam_group_normalize(me, groups, 0);
	seam_group_scale(me, groups, 0, 0.98);
	add[0]= add[1]= 0.01;
	seam_group_move(me, groups, 0, add);
}

void unwrap_lscm(void)
{
    int dopack = 1;
	int res;
	Mesh *me;
	int totgroup, *groups=NULL, a;
	
	me= get_mesh(OBACT);
	if(me==0 || me->tface==0) return;
	
	totgroup= make_seam_groups(me, &groups);
	
	if(totgroup==0) return;
	
	for(a=totgroup; a>0; a--) {
		res= unwrap_lscm_face_group(me, groups, a);
		if((res < 3) && (res > -1)) {
			seam_group_normalize(me, groups, a);
		}
		else {
			dopack = 0;
		}
		
	}
	
	if(dopack) pack_seam_groups(me, groups, totgroup);
	
	MEM_freeN(groups);

	BIF_undo_push("UV lscm unwrap");

	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWIMAGE, 0);
}



/* Set tface seams based on edge data, uses hash table to find seam edges. */

void set_seamtface()
{
	Mesh *me;
	EdgeHash *ehash;
	int a;
	MFace *mf;
	TFace *tf;
	MEdge *medge;

	me= get_mesh(OBACT);
	if(!me || !me->tface || !(G.f & G_FACESELECT)) return;
	
	ehash= BLI_edgehash_new();

	for(medge=me->medge, a=me->totedge; a>0; a--, medge++)
		if(medge->flag & ME_SEAM)
			BLI_edgehash_insert(ehash, medge->v1, medge->v2, NULL);

	mf= me->mface;
	tf= me->tface;
	for(a=me->totface; a>0; a--, mf++, tf++) {
		tf->unwrap &= ~(TF_SEAM1|TF_SEAM2|TF_SEAM3|TF_SEAM4);

		if(!ehash) continue;

		if(BLI_edgehash_haskey(ehash, mf->v1, mf->v2)) tf->unwrap |= TF_SEAM1;
		if(BLI_edgehash_haskey(ehash, mf->v2, mf->v3)) tf->unwrap |= TF_SEAM2;

		if(mf->v4) {
			if(BLI_edgehash_haskey(ehash, mf->v3, mf->v4)) tf->unwrap |= TF_SEAM3;
			if(BLI_edgehash_haskey(ehash, mf->v4, mf->v1)) tf->unwrap |= TF_SEAM4;
		}
		else if(BLI_edgehash_haskey(ehash, mf->v3, mf->v1)) tf->unwrap |= TF_SEAM3;
	}

	BLI_edgehash_free(ehash, NULL);
}

void select_linked_tfaces_with_seams(int mode, Mesh *me, unsigned int index)
{
	TFace *tf;
	MFace *mf;
	int a, doit=1, mark=0;
	char *linkflag;
	EdgeHash *ehash;

	ehash= BLI_edgehash_new();
	linkflag= MEM_callocN(sizeof(char)*me->totface, "linkflaguv");

	if (mode==0 || mode==1) {
		/* only put face under cursor in array */
		mf= ((MFace*)me->mface) + index;
		hash_add_face(ehash, mf);
		linkflag[index]= 1;
	}
	else {
		/* fill array by selection */
		tf= me->tface;
		mf= me->mface;
		for(a=0; a<me->totface; a++, tf++, mf++) {
			if(tf->flag & TF_HIDE);
			else if(tf->flag & TF_SELECT) {
				hash_add_face(ehash, mf);
				linkflag[a]= 1;
			}
		}
	}
	
	while(doit) {
		doit= 0;
		
		/* expand selection */
		tf= me->tface;
		mf= me->mface;
		for(a=0; a<me->totface; a++, tf++, mf++) {
			if(tf->flag & TF_HIDE);
			else if(!linkflag[a]) {
				mark= 0;

				if(!(tf->unwrap & TF_SEAM1))
					if(BLI_edgehash_haskey(ehash, mf->v1, mf->v2))
						mark= 1;
				if(!(tf->unwrap & TF_SEAM2))
					if(BLI_edgehash_haskey(ehash, mf->v2, mf->v3))
						mark= 1;
				if(!(tf->unwrap & TF_SEAM3)) {
					if(mf->v4) {
						if(BLI_edgehash_haskey(ehash, mf->v3, mf->v4))
							mark= 1;
					}
					else if(BLI_edgehash_haskey(ehash, mf->v3, mf->v1))
						mark= 1;
				}
				if(mf->v4 && !(tf->unwrap & TF_SEAM4))
					if(BLI_edgehash_haskey(ehash, mf->v4, mf->v1))
						mark= 1;

				if(mark) {
					linkflag[a]= 1;
					hash_add_face(ehash, mf);
					doit= 1;
				}
			}
		}
		
	}

	if(mode==0 || mode==2) {
		for(a=0, tf=me->tface; a<me->totface; a++, tf++)
			if(linkflag[a])
				tf->flag |= TF_SELECT;
			else
				tf->flag &= ~TF_SELECT;
	}
	else if(mode==1) {
		for(a=0, tf=me->tface; a<me->totface; a++, tf++)
			if(linkflag[a] && (tf->flag & TF_SELECT))
				break;

		if (a<me->totface) {
			for(a=0, tf=me->tface; a<me->totface; a++, tf++)
				if(linkflag[a])
					tf->flag &= ~TF_SELECT;
		}
		else {
			for(a=0, tf=me->tface; a<me->totface; a++, tf++)
				if(linkflag[a])
					tf->flag |= TF_SELECT;
		}
	}
	
	BLI_edgehash_free(ehash, NULL);
	MEM_freeN(linkflag);
	
	BIF_undo_push("Select linked UV face");
	object_tface_flags_changed(OBACT, 0);
}

