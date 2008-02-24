/**
 * BME_tools.c    jan 2007
 *
 *	Functions for changing the topology of a mesh.
 *
 * $Id: BME_eulers.c,v 1.00 2007/01/17 17:42:01 Briggs Exp $
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
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Geoffrey Bantle and Levi Schooley.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"

#include "BKE_utildefines.h"
#include "BKE_bmesh.h"
#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "blendef.h"

/**
 *			BME_dissolve_edge
 *
 *	Edge Dissolve Function:
 *
 *	Dissolves a 2-manifold edge by joining it's two faces. if
 *	they have opposite windings it first makes them consistent
 *	by calling BME_loop_reverse()
 *
 *	Returns -
*/

/**
 *			BME_inset_edge
 *
 *	Edge Inset Function:
 *
 *	Splits a face in two along an edge and returns the next loop
 *
 *	Returns -
 *	A BME_Poly pointer.
 */

BME_Loop *BME_inset_edge(BME_Mesh *bm, BME_Loop *l, BME_Poly *f){
	BME_Loop *nloop;
	BME_SFME(bm, f, l->v, l->next->v, &nloop);
	return nloop->next;
}

/**
 *			BME_inset_poly
 *
 *	Face Inset Tool:
 *
 *	Insets a single face and returns a pointer to the face at the
 *	center of the newly created region
 *
 *	Returns -
 *	A BME_Poly pointer.
 */

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

BME_Poly *BME_inset_poly(BME_Mesh *bm,BME_Poly *f){

	BME_Vert *v;
	BME_Loop *l,*nextloop, *killoop, *sloop;

	int len,i;
	float max[3],min[3],cent[3]; //center of original face

	/*get bounding box for face*/
	VECCOPY(max,f->loopbase->v->co);
	VECCOPY(min,f->loopbase->v->co);
	len = f->len;
	for(i=0,l=f->loopbase;i<len;i++,l=l->next){
		max[0] = MAX(max[0],l->v->co[0]);
		max[1] = MAX(max[1],l->v->co[1]);
		max[2] = MAX(max[2],l->v->co[2]);

		min[0] = MIN(min[0],l->v->co[0]);
		min[1] = MIN(min[1],l->v->co[1]);
		min[2] = MIN(min[2],l->v->co[2]);
	}

	cent[0] = (min[0] + max[0]) / 2.0f;
	cent[1] = (min[1] + max[1]) / 2.0f;
	cent[2] = (min[2] + max[2]) / 2.0f;

	/*inset each edge in the polygon.*/
	len = f->len;
	for(i=0,l=f->loopbase; i < len; i++){
		nextloop = l->next;
		f = BME_SFME(bm,l->f,l->v,l->next->v,NULL);
		l=nextloop;
	}

	/*for each new edge, call SEMV on it*/
	for(i=0,l=f->loopbase; i < len; i++, l=l->next){
		l->tflag1 = 1; //going to store info that this loops edge still needs split
		f = BME_SFME(bm,l->f,l->v,l->next->v,NULL);
		l->tflag2 = l->v->tflag1 = l->v->tflag2 = 0;
	}

	len = f->len;
	for(i=0,l=f->loopbase; i < len; i++){
		if(l->tflag1){
			l->tflag1 = 0;
			v= BME_SEMV(bm,l->next->v,l->e,NULL);
			VECCOPY(v->co,l->v->co);
			v->tflag1 = 1;
			l = l->next->next;
		}
	}

	len = f->len;
	sloop = NULL;
	for(i=0,l=f->loopbase; i < len; i++,l=l->next){
		if(l->v->tflag1 && l->next->next->v->tflag1){
			sloop = l;
			break;
		}
	}
	if(sloop){
		for(i=0,l=sloop; i < len; i++){
			nextloop = l->next->next;
			f = BME_SFME(bm,f,l->v,l->next->next->v,&killoop);
			i+=1; //i+=2;
			BME_JFKE(bm,l->f,((BME_Loop*)l->radial.next->data)->f,l->e);
			l=nextloop;
		}
	}

	len = f->len;
	for(i=0,l=f->loopbase; i < len; i++,l=l->next){
		l->v->co[0] = (l->v->co[0] + cent[0]) / 2.0f;
		l->v->co[1] = (l->v->co[1] + cent[1]) / 2.0f;
		l->v->co[2] = (l->v->co[2] + cent[2]) / 2.0f;
	}
	return NULL;
}

/* ------- Bevel code starts here -------- */

BME_TransData_Head *BME_init_transdata(int bufsize) {
	BME_TransData_Head *td;

	td = MEM_callocN(sizeof(BME_TransData_Head), "BMesh transdata header");
	td->gh = BLI_ghash_new(BLI_ghashutil_ptrhash,BLI_ghashutil_ptrcmp);
	td->ma = BLI_memarena_new(bufsize);
	BLI_memarena_use_calloc(td->ma);

	return td;
}

void BME_free_transdata(BME_TransData_Head *td) {
	BLI_ghash_free(td->gh,NULL,NULL);
	BLI_memarena_free(td->ma);
	MEM_freeN(td);
}

BME_TransData *BME_assign_transdata(BME_TransData_Head *td, BME_Mesh *bm, BME_Vert *v,
		float *co, float *org, float *vec, float *loc,
		float factor, float weight, float maxfactor, float *max) {
	BME_TransData *vtd;
	int is_new = 0;

	if (v == NULL) return NULL;

	if ((vtd = BLI_ghash_lookup(td->gh, v)) == NULL && bm != NULL) {
		vtd = BLI_memarena_alloc(td->ma, sizeof(*vtd));
		BLI_ghash_insert(td->gh, v, vtd);
		td->len++;
		is_new = 1;
	}

	vtd->bm = bm;
	vtd->v = v;
	if (co != NULL) VECCOPY(vtd->co,co);
	if (org == NULL && is_new) { VECCOPY(vtd->org,v->co); } /* default */
	else if (org != NULL) VECCOPY(vtd->org,org);
	if (vec != NULL) {
		VECCOPY(vtd->vec,vec);
		Normalize(vtd->vec);
	}
	vtd->loc = loc;

	vtd->factor = factor;
	vtd->weight = weight;
	vtd->maxfactor = maxfactor;
	vtd->max = max;

	return vtd;
}

BME_TransData *BME_get_transdata(BME_TransData_Head *td, BME_Vert *v) {
	BME_TransData *vtd;
	vtd = BLI_ghash_lookup(td->gh, v);
	return vtd;
}

/* a hack (?) to use the transdata memarena to allocate floats for use with the max limits */
float *BME_new_transdata_float(BME_TransData_Head *td) {
	return BLI_memarena_alloc(td->ma, sizeof(float));
}

int BME_is_nonmanifold_vert(BME_Mesh *bm, BME_Vert *v) {
	BME_Edge *e, *oe;
	BME_Loop *l;
	int len, count, flag;

	if (v->edge == NULL) {
		/* loose vert */
		return 1;
	}

	/* count edges while looking for non-manifold edges */
	oe = v->edge;
	for (len=0,e=v->edge; e != oe || (e == oe && len == 0); len++,e=BME_disk_nextedge(e,v)) {
		if (e->loop == NULL) {
			/* loose edge */
			return 1;
		}

		if (BME_cycle_length(&(e->loop->radial)) > 2) {
			/* edge shared by more than two faces */
			return 1;
		}
	}

	count = 1;
	flag = 1;
	e = NULL;
	oe = v->edge;
	l = oe->loop;
	while(e != oe) {
		if (l->v == v) l = l->prev;
		else l = l->next;
		e = l->e;
		count++; /* count the edges */

		if (flag && l->radial.next->data == l) {
			/* we've hit the edge of an open mesh, reset once */
			flag = 0;
			count = 1;
			oe = e;
			e = NULL;
			l = oe->loop;
		}
		else if (l->radial.next->data == l) {
			/* break the loop */
			e = oe;
		}
		else {
			l = l->radial.next->data;
		}
	}

	if (count < len) {
		/* vert shared by multiple regions */
		return 1;
	}

	return 0;
}

/* a wrapper for BME_JFKE that [for now just] checks to
 * make sure loop directions are compatible */
BME_Poly *BME_JFKE_safe(BME_Mesh *bm, BME_Poly *f1, BME_Poly *f2, BME_Edge *e) {
	BME_Loop *l1, *l2;

	l1 = e->loop;
	l2 = l1->radial.next->data;
	if (l1->v == l2->v) {
		BME_loop_reverse(bm, f2);
	}

	return BME_JFKE(bm, f1, f2, e);
}

/* a wrapper for BME_SFME that transfers element flags */
BME_Poly *BME_split_face(BME_Mesh *bm, BME_Poly *f, BME_Vert *v1, BME_Vert *v2, BME_Loop **nl, BME_Edge *example) {
	BME_Poly *nf;
	nf = BME_SFME(bm,f,v1,v2,nl);
	nf->flag = f->flag;
	/* if the edge was selected, select this face, too */
	if (example->flag & SELECT) f->flag |= ME_FACE_SEL;
	nf->h = f->h;
	nf->mat_nr = f->mat_nr;
	if (nl && example) {
		(*nl)->e->flag = example->flag;
		(*nl)->e->h = example->h;
		(*nl)->e->crease = example->crease;
		(*nl)->e->bweight = example->bweight;
	}

	return nf;
}

/* a wrapper for BME_SEMV that transfers element flags */
BME_Vert *BME_split_edge(BME_Mesh *bm, BME_Vert *v, BME_Edge *e, BME_Edge **ne, float percent) {
	BME_Vert *nv, *v2;
	float len;

	v2 = BME_edge_getothervert(e,v);
	nv = BME_SEMV(bm,v,e,ne);
	if (nv == NULL) return NULL;
	VECSUB(nv->co,v2->co,v->co);
	len = VecLength(nv->co);
	VECADDFAC(nv->co,v->co,nv->co,len*percent);
	nv->flag = v->flag;
	nv->bweight = v->bweight;
	if (ne) {
		(*ne)->flag = e->flag;
		(*ne)->h = e->h;
		(*ne)->crease = e->crease;
		(*ne)->bweight = e->bweight;
	}

	return nv;
}

int BME_bevel_is_split_vert(BME_Loop *l) {
	/* look for verts that have already been added to the edge when
	 * beveling other polys; this can be determined by testing the
	 * vert and the edges around it for originality
	 */
	if ((l->v->tflag1 & BME_BEVEL_ORIG)==0
			&& (l->e->tflag1 & BME_BEVEL_ORIG)
			&& (l->prev->e->tflag1 & BME_BEVEL_ORIG))
	{
		return 1;
	}
	return 0;
}

/* get a vector, vec, that points from v1->co to wherever makes sense to
 * the bevel operation as a whole based on the relationship between v1 and v2
 * (won't necessarily be a vec from v1->co to v2->co, though it probably will be);
 * the return value is -1 for failure, 0 if we used vert co's, and 1 if we used transform origins */
int BME_bevel_get_vec(float *vec, BME_Vert *v1, BME_Vert *v2, BME_TransData_Head *td) {
	BME_TransData *vtd1, *vtd2;

	vtd1 = BME_get_transdata(td,v1);
	vtd2 = BME_get_transdata(td,v2);
	if (!vtd1 || !vtd2) {
		printf("BME_bevel_get_vec() got called without proper BME_TransData\n");
		return -1;
	}

	/* compare the transform origins to see if we can use the vert co's;
	 * if they belong to different origins, then we will use the origins to determine
	 * the vector */
	if (VecCompare(vtd1->org,vtd2->org,0.000001f)) {
		VECSUB(vec,v2->co,v1->co);
		if (VecLength(vec) < 0.000001f) {
			VecMulf(vec,0);
		}
		return 0;
	}
	else {
		VECSUB(vec,vtd2->org,vtd1->org);
		if (VecLength(vec) < 0.000001f) {
			VecMulf(vec,0);
		}
		return 1;
	}
}

/* "Projects" a vector perpendicular to vec2 against vec1, such that
 * the projected vec1 + vec2 has a min distance of 1 from the "edge" defined by vec2.
 * note: the direction, is_forward, is used in conjunction with up_vec to determine
 * whether this is a convex or concave corner. If it is a concave corner, it will
 * be projected "backwards." If vec1 is before vec2, is_forward should be 0 (we are projecting backwards).
 * vec1 is the vector to project onto (expected to be normalized)
 * vec2 is the direction of projection (pointing away from vec1)
 * up_vec is used for orientation (expected to be normalized)
 * returns the length of the projected vector that lies along vec1 */
float BME_bevel_project_vec(float *vec1, float *vec2, float *up_vec, int is_forward, BME_TransData_Head *td) {
	float factor, vec3[3], tmp[3],c1,c2;

	Crossf(tmp,vec1,vec2);
	Normalize(tmp);
	factor = Inpf(up_vec,tmp);
	if ((factor > 0 && is_forward) || (factor < 0 && !is_forward)) {
		Crossf(vec3,vec2,tmp); /* hmm, maybe up_vec should be used instead of tmp */
	}
	else {
		Crossf(vec3,tmp,vec2); /* hmm, maybe up_vec should be used instead of tmp */
	}
	Normalize(vec3);
	c1 = Inpf(vec3,vec1);
	c2 = Inpf(vec1,vec1);
	if (fabs(c1) < 0.000001f || fabs(c2) < 0.000001f) {
		factor = 0.0f;
	}
	else {
		factor = c2/c1;
	}

	return factor;
}

/* BME_bevel_split_edge() is the main math work-house; its responsibilities are:
 * using the vert and the loop passed, get or make the split vert, set its coordinates
 * and transform properties, and set the max limits.
 * Finally, return the split vert. */
BME_Vert *BME_bevel_split_edge(BME_Mesh *bm, BME_Vert *v, BME_Vert *v1, BME_Loop *l, float *up_vec, float value, BME_TransData_Head *td) {
	BME_TransData *vtd, *vtd1, *vtd2;
	BME_Vert *sv, *v2, *v3;
	BME_Loop *lv1, *lv2;
	BME_Edge *ne, *e1, *e2;
	float maxfactor, scale, len, dis, vec1[3], vec2[3], t_up_vec[3];
	int is_edge, forward, is_split_vert;

	if (l == NULL) {
		/* what you call operator overloading in C :)
		 * I wanted to use the same function for both wire edges and poly loops
		 * so... here we walk around edges to find the needed verts */
		forward = 1;
		is_split_vert = 0;
		if (v->edge == NULL) {
			printf("We can't split a loose vert's edge!\n");
			return NULL;
		}
		e1 = v->edge; /* we just use the first two edges */
		e2 = BME_disk_nextedge(v->edge, v);
		if (e1 == e2) {
			printf("You need at least two edges to use BME_bevel_split_edge()\n");
			return NULL;
		}
		v2 = BME_edge_getothervert(e1, v);
		v3 = BME_edge_getothervert(e2, v);
		if (v1 != v2 && v1 != v3) {
			printf("Error: more than 2 edges in v's disk cycle, or v1 does not share an edge with v\n");
			return NULL;
		}
		if (v1 == v2) {
			v2 = v3;
		}
		else {
			e1 = e2;
		}
		sv = BME_split_edge(bm,v,e1,&ne,0);
		BME_assign_transdata(td, bm, sv, sv->co, sv->co, NULL, sv->co, 0, -1, -1, NULL); /* quick default */
		sv->tflag1 |= BME_BEVEL_BEVEL;
		ne->tflag1 = BME_BEVEL_ORIG; /* mark edge as original, even though it isn't */
		BME_bevel_get_vec(vec1,v1,v,td);
		BME_bevel_get_vec(vec2,v2,v,td);
		Crossf(t_up_vec,vec1,vec2);
		Normalize(t_up_vec);
		up_vec = t_up_vec;
	}
	else {
		/* establish loop direction */
		if (l->v == v) {
			forward = 1;
			lv1 = l->next;
			lv2 = l->prev;
			v1 = l->next->v;
			v2 = l->prev->v;
		}
		else if (l->next->v == v) {
			forward = 0;
			lv1 = l;
			lv2 = l->next->next;
			v1 = l->v;
			v2 = l->next->next->v;
		}
		else {
			printf("ERROR: BME_bevel_split_edge() - v must be adjacent to l\n");
			return NULL;
		}

		if (BME_bevel_is_split_vert(lv1)) {
			is_split_vert = 1;
			sv = v1;
			if (forward) v1 = l->next->next->v;
			else v1 = l->prev->v;
		}
		else {
			is_split_vert = 0;
			sv = BME_split_edge(bm,v,l->e,&ne,0);
			BME_assign_transdata(td, bm, sv, sv->co, sv->co, NULL, sv->co, 0, -1, -1, NULL); /* quick default */
			sv->tflag1 |= BME_BEVEL_BEVEL;
			ne->tflag1 = BME_BEVEL_ORIG; /* mark edge as original, even though it isn't */
		}

		if (BME_bevel_is_split_vert(lv2)) {
			if (forward) v2 = lv2->prev->v;
			else v2 = lv2->next->v;
		}
	}

	is_edge = BME_bevel_get_vec(vec1,v,v1,td); /* get the vector we will be projecting onto */
	BME_bevel_get_vec(vec2,v,v2,td); /* get the vector we will be projecting parallel to */
	len = VecLength(vec1);
	Normalize(vec1);

	vtd = BME_get_transdata(td, sv);
	vtd1 = BME_get_transdata(td, v);
	vtd2 = BME_get_transdata(td,v1);

	if (vtd1->loc == NULL) {
		/* this is a vert with data only for calculating initial weights */
		if (vtd1->weight < 0) {
			vtd1->weight = 0;
		}
		scale = vtd1->weight/vtd1->factor;
		if (!vtd1->max) {
			vtd1->max = BME_new_transdata_float(td);
			*vtd1->max = -1;
		}
	}
	else {
		scale = vtd1->weight;
	}
	vtd->max = vtd1->max;

	if (is_edge && vtd1->loc != NULL) {
		maxfactor = vtd1->maxfactor;
	}
	else {
		maxfactor = scale*BME_bevel_project_vec(vec1,vec2,up_vec,forward,td);
		if (vtd->maxfactor > 0 && vtd->maxfactor < maxfactor) {
			maxfactor = vtd->maxfactor;
		}
	}

	dis = (v1->tflag1 & BME_BEVEL_ORIG)? len/3 : len/2;
	if (is_edge || dis > maxfactor*value) {
		dis = maxfactor*value;
	}
	VECADDFAC(sv->co,v->co,vec1,dis);
	VECSUB(vec1,sv->co,vtd1->org);
	dis = VecLength(vec1);
	Normalize(vec1);
	BME_assign_transdata(td, bm, sv, vtd1->org, vtd1->org, vec1, sv->co, dis, scale, maxfactor, vtd->max);

	return sv;
}

float BME_bevel_set_max(BME_Vert *v1, BME_Vert *v2, float value, BME_TransData_Head *td) {
	BME_TransData *vtd1, *vtd2;
	float max, fac1, fac2, vec1[3], vec2[3], vec3[3];

	BME_bevel_get_vec(vec1,v1,v2,td);
	vtd1 = BME_get_transdata(td,v1);
	vtd2 = BME_get_transdata(td,v2);

	if (vtd1->loc == NULL) {
		fac1 = 0;
	}
	else {
		VECCOPY(vec2,vtd1->vec);
		VecMulf(vec2,vtd1->factor);
		if (Inpf(vec1, vec1)) {
			Projf(vec2,vec2,vec1);
			fac1 = VecLength(vec2)/value;
		}
		else {
			fac1 = 0;
		}
	}

	if (vtd2->loc == NULL) {
		fac2 = 0;
	}
	else {
		VECCOPY(vec3,vtd2->vec);
		VecMulf(vec3,vtd2->factor);
		if (Inpf(vec1, vec1)) {
			Projf(vec2,vec3,vec1);
			fac2 = VecLength(vec2)/value;
		}
		else {
			fac2 = 0;
		}
	}

	if (fac1 || fac2) {
		max = VecLength(vec1)/(fac1 + fac2);
		if (vtd1->max && (*vtd1->max < 0 || max < *vtd1->max)) {
			*vtd1->max = max;
		}
		if (vtd2->max && (*vtd2->max < 0 || max < *vtd2->max)) {
			*vtd2->max = max;
		}
	}
	else {
		max = -1;
	}

	return max;
}

BME_Vert *BME_bevel_wire(BME_Mesh *bm, BME_Vert *v, float value, int res, int options, BME_TransData_Head *td) {
	BME_Vert *ov1, *ov2, *v1, *v2;

	ov1 = BME_edge_getothervert(v->edge, v);
	ov2 = BME_edge_getothervert(BME_disk_nextedge(v->edge, v), v);

	/* split the edges */
	v1 = BME_bevel_split_edge(bm,v,ov1,NULL,NULL,value,td);
	v1->tflag1 |= BME_BEVEL_NONMAN;
	v2 = BME_bevel_split_edge(bm,v,ov2,NULL,NULL,value,td);
	v2->tflag1 |= BME_BEVEL_NONMAN;

	if (value > 0.5) {
		BME_bevel_set_max(v1,ov1,value,td);
		BME_bevel_set_max(v2,ov2,value,td);
	}

	/* remove the original vert */
	if (res) {
		BME_JEKV(bm,v->edge,v);
	}

	return v1;
}

BME_Loop *BME_bevel_edge(BME_Mesh *bm, BME_Loop *l, float value, int options, float *up_vec, BME_TransData_Head *td) {
	BME_Vert *v1, *v2, *kv;
	BME_Loop *kl, *nl;
	BME_Edge *e;
	BME_Poly *f;
	float factor=1;

	f = l->f;
	e = l->e;

	if ((l->e->tflag1 & BME_BEVEL_BEVEL) == 0
		&& ((l->v->tflag1 & BME_BEVEL_BEVEL) || (l->next->v->tflag1 & BME_BEVEL_BEVEL)))
	{ /* sanity check */
		return l;
	}

	/* checks and operations for prev edge */
	/* first, check to see if this edge was inset previously */
	if ((l->prev->e->tflag1 & BME_BEVEL_ORIG) == 0
		&& (l->v->tflag1 & BME_BEVEL_NONMAN) == 0) {
		kl = l->prev->radial.next->data;
		if (kl->v == l->v) kl = kl->prev;
		else kl = kl->next;
		kv = l->v;
	}
	else {
		kv = NULL;
	}
	/* get/make the first vert to be used in SFME */
	if (l->v->tflag1 & BME_BEVEL_NONMAN){
		v1 = l->v;
	}
	else { /* we'll need to split the previous edge */
		v1 = BME_bevel_split_edge(bm,l->v,NULL,l->prev,up_vec,value,td);
	}
	/* if we need to clean up geometry... */
	if (kv) {
		l = l->next;
		if (kl->v == kv) {
			BME_split_face(bm,kl->f,kl->prev->v,kl->next->v,&nl,kl->prev->e);
			BME_JFKE(bm,((BME_Loop*)kl->prev->radial.next->data)->f,kl->f,kl->prev->e);
			BME_JEKV(bm,kl->e,kv);
		}
		else {
			BME_split_face(bm,kl->f,kl->next->next->v,kl->v,&nl,kl->next->e);
			BME_JFKE(bm,((BME_Loop*)kl->next->radial.next->data)->f,kl->f,kl->next->e);
			BME_JEKV(bm,kl->e,kv);
		}
		l = l->prev;
	}

	/* checks and operations for the next edge */
	/* first, check to see if this edge was inset previously  */
	if ((l->next->e->tflag1 & BME_BEVEL_ORIG) == 0
		&& (l->next->v->tflag1 & BME_BEVEL_NONMAN) == 0) {
		kl = l->next->radial.next->data;
		if (kl->v == l->next->v) kl = kl->prev;
		else kl = kl->next;
		kv = l->next->v;
	}
	else {
		kv = NULL;
	}
	/* get/make the second vert to be used in SFME */
	if (l->next->v->tflag1 & BME_BEVEL_NONMAN) {
		v2 = l->next->v;
	}
	else { /* we'll need to split the next edge */
		v2 = BME_bevel_split_edge(bm,l->next->v,NULL,l->next,up_vec,value,td);
	}
	/* if we need to clean up geometry... */
	if (kv) {
		if (kl->v == kv) {
			BME_split_face(bm,kl->f,kl->prev->v,kl->next->v,&nl,kl->prev->e);
			BME_JFKE(bm,((BME_Loop*)kl->prev->radial.next->data)->f,kl->f,kl->prev->e);
			BME_JEKV(bm,kl->e,kv);
		}
		else {
			BME_split_face(bm,kl->f,kl->next->next->v,kl->v,&nl,kl->next->e);
			BME_JFKE(bm,((BME_Loop*)kl->next->radial.next->data)->f,kl->f,kl->next->e);
			BME_JEKV(bm,kl->e,kv);
		}
	}

	if ((v1->tflag1 & BME_BEVEL_NONMAN)==0 || (v2->tflag1 & BME_BEVEL_NONMAN)==0) {
		BME_split_face(bm,f,v2,v1,&l,e);
		l->e->tflag1 = BME_BEVEL_BEVEL;
		l = l->radial.next->data;
	}

	if (l->f != f) printf("Whoops! You got something out of order in BME_bevel_edge()!\n");

	return l;
}

BME_Loop *BME_bevel_vert(BME_Mesh *bm, BME_Loop *l, float value, int options, float *up_vec, BME_TransData_Head *td) {
	BME_Vert *v1, *v2;
	BME_Poly *f;

	/* get/make the first vert to be used in SFME */
	/* may need to split the previous edge */
	v1 = BME_bevel_split_edge(bm,l->v,NULL,l->prev,up_vec,value,td);

	/* get/make the second vert to be used in SFME */
	/* may need to split this edge (so move l) */
	l = l->prev;
	v2 = BME_bevel_split_edge(bm,l->next->v,NULL,l->next,up_vec,value,td);
	l = l->next->next;

	/* "cut off" this corner */
	f = BME_split_face(bm,l->f,v2,v1,NULL,l->e);

	return l;
}

/**
 *			BME_bevel_poly
 *
 *	Polygon inset tool:
 *
 *	Insets a polygon/face based on the tflag1's of its vertices
 *	and edges. Used by the bevel tool only, for now.
 *  The parameter "value" is the distance to inset (should be negative).
 *  The parameter "options" is not currently used.
 *
 *	Returns -
 *  A BME_Poly pointer to the resulting inner face.
*/
BME_Poly *BME_bevel_poly(BME_Mesh *bm, BME_Poly *f, float value, int options, BME_TransData_Head *td) {
	BME_Loop *l, *ol;
	BME_TransData *vtd1, *vtd2;
	float up_vec[3], vec1[3], vec2[3], vec3[3], fac1, fac2, max = -1;
	int len, i;

	up_vec[0] = 0.0f;
	up_vec[1] = 0.0f;
	up_vec[2] = 0.0f;
	/* find a good normal for this face (there's better ways, I'm sure) */
	ol = f->loopbase;
	l = ol->next;
	for (i=0,ol=f->loopbase,l=ol->next; l->next!=ol; l=l->next) {
		BME_bevel_get_vec(vec1,l->next->v,ol->v,td);
		BME_bevel_get_vec(vec2,l->v,ol->v,td);
		Crossf(vec3,vec2,vec1);
		VECADD(up_vec,up_vec,vec3);
		i++;
	}
	VecMulf(up_vec,1.0f/i);
	Normalize(up_vec);

	for (i=0,len=f->len; i<len; i++,l=l->next) {
		if ((l->e->tflag1 & BME_BEVEL_BEVEL) && (l->e->tflag1 & BME_BEVEL_ORIG)) {
			max = 1.0f;
			l = BME_bevel_edge(bm, l, value, options, up_vec, td);
		}
		else if ((l->v->tflag1 & BME_BEVEL_BEVEL) && (l->v->tflag1 & BME_BEVEL_ORIG) && (l->prev->e->tflag1 & BME_BEVEL_BEVEL) == 0) {
			max = 1.0f;
			l = BME_bevel_vert(bm, l, value, options, up_vec, td);
		}
	}

	/* max pass */
	if (value > 0.5 && max > 0) {
		max = -1;
		for (i=0,len=f->len; i<len; i++,l=l->next) {
			if ((l->e->tflag1 & BME_BEVEL_BEVEL) || (l->e->tflag1 & BME_BEVEL_ORIG)) {
				BME_bevel_get_vec(vec1,l->v,l->next->v,td);
				vtd1 = BME_get_transdata(td,l->v);
				vtd2 = BME_get_transdata(td,l->next->v);
				if (vtd1->loc == NULL) {
					fac1 = 0;
				}
				else {
					VECCOPY(vec2,vtd1->vec);
					VecMulf(vec2,vtd1->factor);
					if (Inpf(vec1, vec1)) {
						Projf(vec2,vec2,vec1);
						fac1 = VecLength(vec2)/value;
					}
					else {
						fac1 = 0;
					}
				}
				if (vtd2->loc == NULL) {
					fac2 = 0;
				}
				else {
					VECCOPY(vec3,vtd2->vec);
					VecMulf(vec3,vtd2->factor);
					if (Inpf(vec1, vec1)) {
						Projf(vec2,vec3,vec1);
						fac2 = VecLength(vec2)/value;
					}
					else {
						fac2 = 0;
					}
				}
				if (fac1 || fac2) {
					max = VecLength(vec1)/(fac1 + fac2);
					if (vtd1->max && (*vtd1->max < 0 || max < *vtd1->max)) {
						*vtd1->max = max;
					}
					if (vtd2->max && (*vtd2->max < 0 || max < *vtd2->max)) {
						*vtd2->max = max;
					}
				}
			}
		}
	}

	return l->f;
}

void BME_bevel_add_vweight(BME_TransData_Head *td, BME_Mesh *bm, BME_Vert *v, float weight, float factor, int options) {
	BME_TransData *vtd;

	if (v->tflag1 & BME_BEVEL_NONMAN) return;
	v->tflag1 |= BME_BEVEL_BEVEL;
	if (vtd = BME_get_transdata(td, v)) {
		if (options & BME_BEVEL_EMIN) {
			vtd->factor = 1.0;
			if (vtd->weight < 0 || weight < vtd->weight) {
				vtd->weight = weight;
			}
		}
		else if (options & BME_BEVEL_EMAX) {
			vtd->factor = 1.0;
			if (weight > vtd->weight) {
				vtd->weight = weight;
			}
		}
		else if (vtd->weight < 0) {
			vtd->factor = factor;
			vtd->weight = weight;
		}
		else {
			vtd->factor += factor; /* increment number of edges with weights (will be averaged) */
			vtd->weight += weight; /* accumulate all the weights */
		}
	}
	else {
		/* we'll use vtd->loc == NULL to mark that this vert is not moving */
		vtd = BME_assign_transdata(td, bm, v, v->co, NULL, NULL, NULL, factor, weight, -1, NULL);
	}
}

float BME_bevel_get_angle(BME_Mesh *bm, BME_Edge *e, BME_Vert *v) {
	BME_Vert *v1, *v2;
	BME_Loop *l1, *l2;
	float vec1[3], vec2[3], vec3[3], vec4[3];

	l1 = e->loop;
	l2 = e->loop->radial.next->data;
	if (l1->v == v) {
		v1 = l1->prev->v;
		v2 = l1->next->v;
	}
	else {
		v1 = l1->next->next->v;
		v2 = l1->v;
	}
	VECSUB(vec1,v1->co,v->co);
	VECSUB(vec2,v2->co,v->co);
	Crossf(vec3,vec1,vec2);

	l1 = l2;
	if (l1->v == v) {
		v1 = l1->prev->v;
		v2 = l1->next->v;
	}
	else {
		v1 = l1->next->next->v;
		v2 = l1->v;
	}
	VECSUB(vec1,v1->co,v->co);
	VECSUB(vec2,v2->co,v->co);
	Crossf(vec4,vec2,vec1);

	Normalize(vec3);
	Normalize(vec4);

	return Inpf(vec3,vec4);
}

/**
 *			BME_bevel_initialize
 *
 *	Prepare the mesh for beveling:
 *
 *	Sets the tflag1's of the mesh elements based on the options passed.
 *
 *	Returns -
 *  A BME_Mesh pointer to the BMesh passed as a parameter.
*/
BME_Mesh *BME_bevel_initialize(BME_Mesh *bm, int options, int defgrp_index, float angle, BME_TransData_Head *td) {
	BME_Vert *v;
	BME_Edge *e;
	BME_Poly *f;
	BME_TransData *vtd;
	MDeformVert *dvert;
	MDeformWeight *dw;
	int i, len;
	float weight, threshold;

	/* vert pass */
	for (v=bm->verts.first; v; v=v->next) {
		dvert = NULL;
		dw = NULL;
		v->tflag1 = BME_BEVEL_ORIG;
		/* originally coded, a vertex gets tagged with BME_BEVEL_BEVEL in this pass if
		 * the vert is manifold (or is shared by only two edges - wire bevel)
		 * BME_BEVEL_SELECT is passed and the vert has v->flag&SELECT or
		 * BME_BEVEL_VWEIGHT is passed, and the vert has a defgrp and weight
		 * BME_BEVEL_ANGLE is not passed
		 * BME_BEVEL_EWEIGHT is not passed
		 */
		/* originally coded, a vertex gets tagged with BME_BEVEL_NONMAN in this pass if
		 * the vert is loose, shared by multiple regions, or is shared by wire edges
		 * note: verts belonging to edges of open meshes are not tagged with BME_BEVEL_NONMAN
		 */
		/* originally coded, a vertex gets a transform weight set in this pass if
		 * BME_BEVEL_VWEIGHT is passed, and the vert has a defgrp and weight
		 */

		/* get disk cycle length */
		if (v->edge == NULL) {
			len = 0;
		}
		else {
			len = BME_cycle_length(BME_disk_getpointer(v->edge,v));
			/* we'll assign a default transform data to every vert (except the loose ones) */
			vtd = BME_assign_transdata(td, bm, v, v->co, v->co, NULL, NULL, 0, -1, -1, NULL);
		}

		/* check for non-manifold vert */
		if (BME_is_nonmanifold_vert(bm,v)) {
			v->tflag1 |= BME_BEVEL_NONMAN;
		}

		/* BME_BEVEL_BEVEL tests */
		if ((v->tflag1 & BME_BEVEL_NONMAN) == 0 || len == 2) { /* either manifold vert, or wire vert */
			if (((options & BME_BEVEL_SELECT) && (v->flag & SELECT))
				|| ((options & BME_BEVEL_WEIGHT) && (options & BME_BEVEL_VERT)) /* use weights for verts */
				|| ((options & BME_BEVEL_ANGLE) == 0
					&& (options & BME_BEVEL_SELECT) == 0
					&& (options & BME_BEVEL_WEIGHT) == 0))
			{
				if (options & BME_BEVEL_WEIGHT) {
					/* do vert weight stuff */
					//~ dvert = CustomData_em_get(&bm->vdata,v->data,CD_MDEFORMVERT);
					//~ if (!dvert) continue;
					//~ for (i = 0; i < dvert->totweight; ++i) {
						//~ if(dvert->dw[i].def_nr == defgrp_index) {
							//~ dw = &dvert->dw[i];
							//~ break;
						//~ }
					//~ }
					//~ if (!dw || dw->weight == 0.0) continue;
					if (v->bweight == 0.0) continue;
					vtd = BME_assign_transdata(td, bm, v, v->co, v->co, NULL, NULL, 1.0, v->bweight, -1, NULL);
					v->tflag1 |= BME_BEVEL_BEVEL;
				}
				else {
					vtd = BME_assign_transdata(td, bm, v, v->co, v->co, NULL, NULL, 1.0, 1.0, -1, NULL);
					v->tflag1 |= BME_BEVEL_BEVEL;
				}
			}
		}
	}

	/* edge pass */
	threshold = (float)cos((angle + 0.00001) * M_PI / 180.0);
	for (e=bm->edges.first; e; e=e->next) {
		e->tflag1 = BME_BEVEL_ORIG;
		weight = 0.0;
		/* originally coded, an edge gets tagged with BME_BEVEL_BEVEL in this pass if
		 * BME_BEVEL_VERT is not set
		 * the edge is manifold (shared by exactly two faces)
		 * BME_BEVEL_SELECT is passed and the edge has e->flag&SELECT or
		 * BME_BEVEL_EWEIGHT is passed, and the edge has the crease set or
		 * BME_BEVEL_ANGLE is passed, and the edge is sharp enough
		 * BME_BEVEL_VWEIGHT is passed, and both verts are set for bevel
		 */
		/* originally coded, a vertex gets tagged with BME_BEVEL_BEVEL in this pass if
		 * the vert belongs to the edge
		 * the vert is not tagged with BME_BEVEL_NONMAN
		 * the edge is eligible for bevel (even if BME_BEVEL_VERT is set, or the edge is shared by less than 2 faces)
		 */
		/* originally coded, a vertex gets a transform weight set in this pass if
		 * the vert belongs to the edge
		 * the edge has a weight
		 */
		/* note: edge weights are cumulative at the verts,
		 * i.e. the vert's weight is the average of the weights of its weighted edges
		 */

		if (e->loop == NULL) {
			len = 0;
			e->v1->tflag1 |= BME_BEVEL_NONMAN;
			e->v2->tflag1 |= BME_BEVEL_NONMAN;
		}
		else {
			len = BME_cycle_length(&(e->loop->radial));
		}

		if (len > 2) {
			/* non-manifold edge of the worst kind */
			continue;
		}

		if ((options & BME_BEVEL_SELECT) && (e->flag & SELECT)) {
			weight = 1.0;
			/* stupid editmode doesn't always flush selections, or something */
			e->v1->flag |= SELECT;
			e->v2->flag |= SELECT;
		}
		else if ((options & BME_BEVEL_WEIGHT) && (options & BME_BEVEL_VERT) == 0) {
			weight = e->bweight;
		}
		else if (options & BME_BEVEL_ANGLE) {
			if ((e->v1->tflag1 & BME_BEVEL_NONMAN) == 0 && BME_bevel_get_angle(bm,e,e->v1) < threshold) {
				e->tflag1 |= BME_BEVEL_BEVEL;
				e->v1->tflag1 |= BME_BEVEL_BEVEL;
				BME_bevel_add_vweight(td, bm, e->v1, 1.0, 1.0, options);
			}
			else {
				BME_bevel_add_vweight(td, bm, e->v1, 0.0, 1.0, options);
			}
			if ((e->v2->tflag1 & BME_BEVEL_NONMAN) == 0 && BME_bevel_get_angle(bm,e,e->v2) < threshold) {
				e->tflag1 |= BME_BEVEL_BEVEL;
				e->v2->tflag1 |= BME_BEVEL_BEVEL;
				BME_bevel_add_vweight(td, bm, e->v2, 1.0, 1.0, options);
			}
			else {
				BME_bevel_add_vweight(td, bm, e->v2, 0.0, 1.0, options);
			}
		}
		//~ else if ((options & BME_BEVEL_VWEIGHT) && (options & BME_BEVEL_VERT) == 0) {
			//~ if ((e->v1->tflag1 & BME_BEVEL_BEVEL) && (e->v2->tflag1 & BME_BEVEL_BEVEL)) {
				//~ e->tflag1 |= BME_BEVEL_BEVEL;
			//~ }
		//~ }
		else if ((options & BME_BEVEL_SELECT) == 0
			&& (options & BME_BEVEL_VERT) == 0)
		{
			weight = 1.0;
		}

		if (weight > 0.0) {
			e->tflag1 |= BME_BEVEL_BEVEL;
			BME_bevel_add_vweight(td, bm, e->v1, weight, 1.0, options);
			BME_bevel_add_vweight(td, bm, e->v2, weight, 1.0, options);
		}

		if (len != 2 || options & BME_BEVEL_VERT) {
			e->tflag1 &= ~BME_BEVEL_BEVEL;
		}
	}

	/* face pass */
	for (f=bm->polys.first; f; f=f->next) f->tflag1 = BME_BEVEL_ORIG;

	return bm;
}

/* tags all elements as originals */
BME_Mesh *BME_bevel_reinitialize(BME_Mesh *bm) {
	BME_Vert *v;
	BME_Edge *e;
	BME_Poly *f;

	for (v = bm->verts.first; v; v=v->next) {
		v->tflag1 |= BME_BEVEL_ORIG;
	}

	for (e=bm->edges.first; e; e=e->next) {
		e->tflag1 |= BME_BEVEL_ORIG;
	}

	for (f=bm->polys.first; f; f=f->next) {
		f->tflag1 |= BME_BEVEL_ORIG;
	}

	return bm;
}

/**
 *			BME_bevel_mesh
 *
 *	Mesh beveling tool:
 *
 *	Bevels an entire mesh. It currently uses the tflag1's of
 *	its vertices and edges to track topological changes.
 *  The parameter "value" is the distance to inset (should be negative).
 *  The parameter "options" is not currently used.
 *
 *	Returns -
 *  A BME_Mesh pointer to the BMesh passed as a parameter.
*/
BME_Mesh *BME_bevel_mesh(BME_Mesh *bm, float value, int res, int options, int defgrp_index, BME_TransData_Head *td) {
	BME_Vert *v, *nv, *kv;
	BME_Edge *e, *oe, *ne;
	BME_Loop *l, *l2;
	BME_Poly *f, *nf;
	unsigned int i, len, done;

	for (f=bm->polys.first; f; f=f->next) {
		if(f->tflag1 & BME_BEVEL_ORIG) {
			BME_bevel_poly(bm,f,value,options,td);
		}
	}

	/* here we will loop through all the verts to clean up the left over geometry */
	/* crazy idea. when res == 0, don't remove the original geometry */
	for (v = bm->verts.first; v; /* we may kill v, so increment in-loop */) {
		nv = v->next;
		if ((v->tflag1 & BME_BEVEL_NONMAN) && (v->tflag1 & BME_BEVEL_BEVEL) && (v->tflag1 & BME_BEVEL_ORIG)) {
			v = BME_bevel_wire(bm, v, value, res, options, td);
		}
		else if (res && ((v->tflag1 & BME_BEVEL_BEVEL) && (v->tflag1 & BME_BEVEL_ORIG))) {
			/* first, make sure we're not sitting on an edge to be removed */
			oe = v->edge;
			e = BME_disk_nextedge(oe,v);
			while ((e->tflag1 & BME_BEVEL_BEVEL) && (e->tflag1 & BME_BEVEL_ORIG)) {
				e = BME_disk_nextedge(e,v);
				if (e == oe) {
					printf("Something's wrong! We can't remove every edge here!\n");
					break;
				}
			}
			/* look for original edges, and remove them */
			oe = e;
			while (e = BME_disk_next_edgeflag(oe, v, 0, BME_BEVEL_ORIG | BME_BEVEL_BEVEL)) {
				/* join the faces (we'll split them later) */
				f = BME_JFKE_safe(bm,e->loop->f,((BME_Loop*)e->loop->radial.next->data)->f,e);
				if (!f) printf("Non-manifold geometry not getting tagged right?\n");
			}

			/* all original edges marked to be beveled have been removed;
			 * now we need to link up the edges for this "corner" */
			len = BME_cycle_length(BME_disk_getpointer(v->edge, v));
			for (i=0,e=v->edge; i < len; i++,e=BME_disk_nextedge(e,v)) {
				l = e->loop;
				l2 = l->radial.next->data;
				if (l->v != v) l = l->next;
				if (l2->v != v) l2 = l2->next;
				/* look for faces that have had the original edges removed via JFKE */
				if (l->f->len > 3) {
					BME_split_face(bm,l->f,l->next->v,l->prev->v,&l,l->e); /* clip this corner off */
					if (len > 2) {
						l->e->tflag1 |= BME_BEVEL_BEVEL;
					}
				}
				if (l2->f->len > 3) {
					BME_split_face(bm,l2->f,l2->next->v,l2->prev->v,&l,l2->e); /* clip this corner off */
					if (len > 2) {
						l->e->tflag1 |= BME_BEVEL_BEVEL;
					}
				}
			}

			done = 0;
			while(!done){
				done = 1;
				e = v->edge;
				do{
					f = NULL;
					len = BME_cycle_length(&(e->loop->radial));
					if(len == 2){
						f = BME_JFKE_safe(bm,e->loop->f, ((BME_Loop*)(e->loop->radial.next->data))->f, e);
					}
					if(f){ 
						done = 0;
						break;
					}
					e = BME_disk_nextedge(e,v);
				}while(e != v->edge);
			}
			BME_JEKV(bm,e,v);
		}
		v = nv;
	}

	/*clean up two edged faces here*/
		for(f=bm->polys.first; f;){
			nf = f->next;
			if(f->len == 2){
				e = NULL;
				l = f->loopbase;
				do{
					if(BME_cycle_length(&l->radial) == 2){
						e = l->e;
						break;
					}
					l = l->next;
				}while(l!=f->loopbase);

				if(e) BME_JFKE_safe(bm,f, ((BME_Loop*)(e->loop->radial.next->data))->f, e);
				else{
					/*We are still leaving a stray edge? This shouldnt even be possible!*/
					BME_KF(bm, f);
				}
			}
			f = nf;
		}

	return bm;
}

BME_Mesh *BME_tesselate(BME_Mesh *bm) {
	BME_Loop *l, *nextloop;
	BME_Poly *f;

	for (f=bm->polys.first; f; f=f->next) {
		l = f->loopbase;
		while (l->f->len > 4) {
			nextloop = l->next->next->next;
			/* make a quad */
			BME_split_face(bm,l->f,l->v,nextloop->v,NULL,l->e);
			l = nextloop;
		}
	}
	return bm;
}

/* options that can be passed:
 * BME_BEVEL_VWEIGHT	<---- v, Look at vertex weights; use defgrp_index if option is present
 * BME_BEVEL_SELECT		<---- v,e, check selection for verts and edges
 * BME_BEVEL_ANGLE		<---- v,e, don't bevel-tag verts - tag verts per edge
 * BME_BEVEL_VERT		<---- e, don't tag edges
 * BME_BEVEL_EWEIGHT	<---- e, use crease flag for now
 * BME_BEVEL_PERCENT	<---- Will need to think about this one; will probably need to incorporate into actual bevel routine
 * BME_BEVEL_RADIUS		<---- Will need to think about this one; will probably need to incorporate into actual bevel routine
 * All weights/limits are stored per-vertex
 */
BME_Mesh *BME_bevel(BME_Mesh *bm, float value, int res, int options, int defgrp_index, float angle, BME_TransData_Head **rtd) {
	BME_Vert *v;
	BME_TransData_Head *td;
	BME_TransData *vtd;
	int i;
	float fac=1, d;

	td = BME_init_transdata(BLI_MEMARENA_STD_BUFSIZE);

	BME_bevel_initialize(bm, options, defgrp_index, angle, td);

	/* recursion math courtesy of Martin Poirier (theeth) */
	for (i=0; i<res-1; i++) {
		if (i==0) fac += 1.0f/3.0f; else fac += 1.0f/(3 * i * 2.0f);
	}
	d = 1.0f/fac;
	/* crazy idea. if res == 0, don't remove original geometry */
	for (i=0; i<res || (res==0 && i==0); i++) {
		if (i != 0) BME_bevel_reinitialize(bm);
		BME_bevel_mesh(bm,d,res,options,defgrp_index,td);
		if (i==0) d /= 3; else d /= 2;
	}

	BME_tesselate(bm);

	if (rtd) {
		*rtd = td;
		return bm;
	}

	/* transform pass */
	for (v = bm->verts.first; v; v=v->next) {
		if (vtd = BME_get_transdata(td, v)) {
			if (vtd->max && (*vtd->max > 0 && value > *vtd->max)) {
				d = *vtd->max;
			}
			else {
				d = value;
			}
			VECADDFAC(v->co,vtd->org,vtd->vec,vtd->factor*d);
		}
		v->tflag1 = 0;
	}

	BME_free_transdata(td);
	return bm;
}
