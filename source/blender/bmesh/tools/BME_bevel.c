/**
 * BME_tools.c    jan 2007
 *
 *	Functions for changing the topology of a mesh.
 *
 * $Id: BME_eulers.c,v 1.00 2007/01/17 17:42:01 Briggs Exp $
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
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
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"

#include "BKE_utildefines.h"
#include "BKE_bmesh.h"
#include "BLI_math.h"
#include "BLI_blenlib.h"

#include "MTC_matrixops.h"
#include "MTC_vectorops.h"

#include "blendef.h"

/* ------- Bevel code starts here -------- */

BME_TransData_Head *BME_init_transdata(int bufsize) {
	BME_TransData_Head *td;

	td = MEM_callocN(sizeof(BME_TransData_Head), "BM transdata header");
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

BME_TransData *BME_assign_transdata(BME_TransData_Head *td, BMesh *bm, BMVert *v,
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
		normalize_v3(vtd->vec);
	}
	vtd->loc = loc;

	vtd->factor = factor;
	vtd->weight = weight;
	vtd->maxfactor = maxfactor;
	vtd->max = max;

	return vtd;
}

BME_TransData *BME_get_transdata(BME_TransData_Head *td, BMVert *v) {
	BME_TransData *vtd;
	vtd = BLI_ghash_lookup(td->gh, v);
	return vtd;
}

/* a hack (?) to use the transdata memarena to allocate floats for use with the max limits */
float *BME_new_transdata_float(BME_TransData_Head *td) {
	return BLI_memarena_alloc(td->ma, sizeof(float));
}

static int BME_bevel_is_split_vert(BMLoop *l) {
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
static int BME_bevel_get_vec(float *vec, BMVert *v1, BMVert *v2, BME_TransData_Head *td) {
	BME_TransData *vtd1, *vtd2;

	vtd1 = BME_get_transdata(td,v1);
	vtd2 = BME_get_transdata(td,v2);
	if (!vtd1 || !vtd2) {
		//printf("BME_bevel_get_vec() got called without proper BME_TransData\n");
		return -1;
	}

	/* compare the transform origins to see if we can use the vert co's;
	 * if they belong to different origins, then we will use the origins to determine
	 * the vector */
	if (compare_v3v3(vtd1->org,vtd2->org,0.000001f)) {
		VECSUB(vec,v2->co,v1->co);
		if (len_v3(vec) < 0.000001f) {
			mul_v3_fl(vec,0);
		}
		return 0;
	}
	else {
		VECSUB(vec,vtd2->org,vtd1->org);
		if (len_v3(vec) < 0.000001f) {
			mul_v3_fl(vec,0);
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
static float BME_bevel_project_vec(float *vec1, float *vec2, float *up_vec, int is_forward, BME_TransData_Head *td) {
	float factor, vec3[3], tmp[3],c1,c2;

	cross_v3_v3v3(tmp,vec1,vec2);
	normalize_v3(tmp);
	factor = dot_v3v3(up_vec,tmp);
	if ((factor > 0 && is_forward) || (factor < 0 && !is_forward)) {
		cross_v3_v3v3(vec3,vec2,tmp); /* hmm, maybe up_vec should be used instead of tmp */
	}
	else {
		cross_v3_v3v3(vec3,tmp,vec2); /* hmm, maybe up_vec should be used instead of tmp */
	}
	normalize_v3(vec3);
	c1 = dot_v3v3(vec3,vec1);
	c2 = dot_v3v3(vec1,vec1);
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
static BMVert *BME_bevel_split_edge(BMesh *bm, BMVert *v, BMVert *v1, BMLoop *l, float *up_vec, float value, BME_TransData_Head *td) {
	BME_TransData *vtd, *vtd1, *vtd2;
	BMVert *sv, *v2, *v3, *ov;
	BMLoop *lv1, *lv2;
	BMEdge *ne, *e1, *e2;
	float maxfactor, scale, len, dis, vec1[3], vec2[3], t_up_vec[3];
	int is_edge, forward, is_split_vert;

	if (l == NULL) {
		/* what you call operator overloading in C :)
		 * I wanted to use the same function for both wire edges and poly loops
		 * so... here we walk around edges to find the needed verts */
		forward = 1;
		is_split_vert = 0;
		if (v->edge == NULL) {
			//printf("We can't split a loose vert's edge!\n");
			return NULL;
		}
		e1 = v->edge; /* we just use the first two edges */
		e2 = bmesh_disk_nextedge(v->edge, v);
		if (e1 == e2) {
			//printf("You need at least two edges to use BME_bevel_split_edge()\n");
			return NULL;
		}
		v2 = BM_OtherEdgeVert(e1, v);
		v3 = BM_OtherEdgeVert(e2, v);
		if (v1 != v2 && v1 != v3) {
			//printf("Error: more than 2 edges in v's disk cycle, or v1 does not share an edge with v\n");
			return NULL;
		}
		if (v1 == v2) {
			v2 = v3;
		}
		else {
			e1 = e2;
		}
		ov = BM_OtherEdgeVert(e1,v);
		sv = BM_Split_Edge(bm,v,e1,&ne,0);
		//BME_data_interp_from_verts(bm, v, ov, sv, 0.25); /*this is technically wrong...*/
		//BME_data_interp_from_faceverts(bm, v, ov, sv, 0.25);
		//BME_data_interp_from_faceverts(bm, ov, v, sv, 0.25);
		BME_assign_transdata(td, bm, sv, sv->co, sv->co, NULL, sv->co, 0, -1, -1, NULL); /* quick default */
		sv->tflag1 |= BME_BEVEL_BEVEL;
		ne->tflag1 = BME_BEVEL_ORIG; /* mark edge as original, even though it isn't */
		BME_bevel_get_vec(vec1,v1,v,td);
		BME_bevel_get_vec(vec2,v2,v,td);
		cross_v3_v3v3(t_up_vec,vec1,vec2);
		normalize_v3(t_up_vec);
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
			//printf("ERROR: BME_bevel_split_edge() - v must be adjacent to l\n");
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
			ov = BM_OtherEdgeVert(l->e,v);
			sv = BM_Split_Edge(bm,v,l->e,&ne,0);
			//BME_data_interp_from_verts(bm, v, ov, sv, 0.25); /*this is technically wrong...*/
			//BME_data_interp_from_faceverts(bm, v, ov, sv, 0.25);
			//BME_data_interp_from_faceverts(bm, ov, v, sv, 0.25);
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
	len = len_v3(vec1);
	normalize_v3(vec1);

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
	dis = len_v3(vec1);
	normalize_v3(vec1);
	BME_assign_transdata(td, bm, sv, vtd1->org, vtd1->org, vec1, sv->co, dis, scale, maxfactor, vtd->max);

	return sv;
}

static float BME_bevel_set_max(BMVert *v1, BMVert *v2, float value, BME_TransData_Head *td) {
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
		mul_v3_fl(vec2,vtd1->factor);
		if (dot_v3v3(vec1, vec1)) {
			project_v3_v3v3(vec2,vec2,vec1);
			fac1 = len_v3(vec2)/value;
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
		mul_v3_fl(vec3,vtd2->factor);
		if (dot_v3v3(vec1, vec1)) {
			project_v3_v3v3(vec2,vec3,vec1);
			fac2 = len_v3(vec2)/value;
		}
		else {
			fac2 = 0;
		}
	}

	if (fac1 || fac2) {
		max = len_v3(vec1)/(fac1 + fac2);
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

static BMVert *BME_bevel_wire(BMesh *bm, BMVert *v, float value, int res, int options, BME_TransData_Head *td) {
	BMVert *ov1, *ov2, *v1, *v2;

	ov1 = BM_OtherEdgeVert(v->edge, v);
	ov2 = BM_OtherEdgeVert(bmesh_disk_nextedge(v->edge, v), v);

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
		bmesh_jekv;

		//void BM_Collapse_Vert(BMesh *bm, BMEdge *ke, BMVert *kv, float fac, int calcnorm){
		//hrm, why is there a fac here? it just removes a vert
		BM_Collapse_Vert(bm, v->edge, v, 1.0, 0);
		//bmesh_jekv(bm,v->edge,v);
	}

	return v1;
}

static BMLoop *BME_bevel_edge(BMesh *bm, BMLoop *l, float value, int options, float *up_vec, BME_TransData_Head *td) {
	BMVert *v1, *v2, *kv;
	BMLoop *kl=NULL, *nl;
	BMEdge *e;
	BMFace *f;

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
			BM_Split_Face(bm,kl->f,kl->prev->v,kl->next->v,&nl,kl->prev->e);
			bmesh_jfke(bm,((BMLoop*)kl->prev->radial.next->data)->f,kl->f,kl->prev->e);
			BM_Collapse_Vert(bm, kl->e, kv, 1.0);
			//BME_JEKV(bm,kl->e,kv);
			
		}
		else {
			BM_Split_Face(bm,kl->f,kl->next->next->v,kl->v,&nl,kl->next->e);
			bmesh_jfke(bm,((BMLoop*)kl->next->radial.next->data)->f,kl->f,kl->next->e);
			BM_Collapse_Vert(bm, kl->e, kv, 1.0);
			//BME_JEKV(bm,kl->e,kv);
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
			BM_Split_Face(bm,kl->f,kl->prev->v,kl->next->v,&nl,kl->prev->e);
			bmesh_jfke(bm,((BMLoop*)kl->prev->radial.next->data)->f,kl->f,kl->prev->e);
			BM_Collapse_Vert(bm, kl->e, kv, 1.0);
			//BME_JEKV(bm,kl->e,kv);
		}
		else {
			BM_Split_Face(bm,kl->f,kl->next->next->v,kl->v,&nl,kl->next->e);
			bmesh_jfke(bm,((BMLoop*)kl->next->radial.next->data)->f,kl->f,kl->next->e);
			BM_Collapse_Vert(bm, kl->e, kv, 1.0);
			//BME_JEKV(bm,kl->e,kv);
		}
	}

	if ((v1->tflag1 & BME_BEVEL_NONMAN)==0 || (v2->tflag1 & BME_BEVEL_NONMAN)==0) {
		BM_Split_Face(bm,f,v2,v1,&l,e);
		l->e->tflag1 = BME_BEVEL_BEVEL;
		l = l->radial.next->data;
	}

	if (l->f != f){
		//printf("Whoops! You got something out of order in BME_bevel_edge()!\n");
	}

	return l;
}

static BMLoop *BME_bevel_vert(BMesh *bm, BMLoop *l, float value, int options, float *up_vec, BME_TransData_Head *td) {
	BMVert *v1, *v2;
	BMFace *f;

	/* get/make the first vert to be used in SFME */
	/* may need to split the previous edge */
	v1 = BME_bevel_split_edge(bm,l->v,NULL,l->prev,up_vec,value,td);

	/* get/make the second vert to be used in SFME */
	/* may need to split this edge (so move l) */
	l = l->prev;
	v2 = BME_bevel_split_edge(bm,l->next->v,NULL,l->next,up_vec,value,td);
	l = l->next->next;

	/* "cut off" this corner */
	f = BM_Split_Face(bm,l->f,v2,v1,NULL,l->e);

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
 *  A BMFace pointer to the resulting inner face.
*/
static BMFace *BME_bevel_poly(BMesh *bm, BMFace *f, float value, int options, BME_TransData_Head *td) {
	BMLoop *l, *ol;
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
		cross_v3_v3v3(vec3,vec2,vec1);
		VECADD(up_vec,up_vec,vec3);
		i++;
	}
	mul_v3_fl(up_vec,1.0f/i);
	normalize_v3(up_vec);

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
					mul_v3_fl(vec2,vtd1->factor);
					if (dot_v3v3(vec1, vec1)) {
						project_v3_v3v3(vec2,vec2,vec1);
						fac1 = len_v3(vec2)/value;
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
					mul_v3_fl(vec3,vtd2->factor);
					if (dot_v3v3(vec1, vec1)) {
						project_v3_v3v3(vec2,vec3,vec1);
						fac2 = len_v3(vec2)/value;
					}
					else {
						fac2 = 0;
					}
				}
				if (fac1 || fac2) {
					max = len_v3(vec1)/(fac1 + fac2);
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

static void BME_bevel_add_vweight(BME_TransData_Head *td, BMesh *bm, BMVert *v, float weight, float factor, int options) {
	BME_TransData *vtd;

	if (v->tflag1 & BME_BEVEL_NONMAN) return;
	v->tflag1 |= BME_BEVEL_BEVEL;
	if ( (vtd = BME_get_transdata(td, v)) ) {
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


static bevel_init_verts(BMesh *bm, int options, BME_TransData_Head *td){
	BMVert *v;
	float weight;
	for(v=bm->verts.first; v; v=v->next){
		weight = 0.0;
		if(!(v->tflag1 & BME_BEVEL_NONMAN)){
			if(options & BME_BEVEL_SELECT){
				if(v->flag & SELECT) weight = 1.0;
			}
			else if(options & BME_BEVEL_WEIGHT) weight = v->bweight;
			else weight = 1.0;
			if(weight > 0.0){
				v->tflag1 |= BME_BEVEL_BEVEL;
				BME_assign_transdata(td, bm, v, v->co, v->co, NULL, NULL, 1.0,weight, -1, NULL); 
			}
		}
	}
}
static bevel_init_edges(BMesh *bm, int options, BME_TransData_Head *td){
	BMEdge *e;
	int count;
	float weight;
	for( e = BM_first_element(bm, BME_EDGE); e; e = BM_next_element(bm, BME_EDGE, e)){
		weight = 0.0;
		if(!(e->tflag1 & BME_BEVEL_NONMAN)){
			if(options & BME_BEVEL_SELECT){
				if(e->flag & SELECT) weight = 1.0;
			}
			else if(options & BME_BEVEL_WEIGHT){ 
				weight = e->bweight;
			}
			else{ 
				weight = 1.0;
			}
			if(weight > 0.0){
				e->tflag1 |= BME_BEVEL_BEVEL;
				e->v1->tflag1 |= BME_BEVEL_BEVEL;
				e->v2->tflag1 |= BME_BEVEL_BEVEL;
				BME_bevel_add_vweight(td, bm, e->v1, weight, 1.0, options);
				BME_bevel_add_vweight(td, bm, e->v2, weight, 1.0, options);
			}
		}
	}
	
	/*clean up edges with 2 faces that share more than one edge*/
	for( e = BM_first_element(bm, BME_EDGE); e; e = BM_next_element(bm, BME_EDGE, e)){
		if(e->tflag1 & BME_BEVEL_BEVEL){
			count = BM_face_sharededges(e->loop->f, ((BMLoop*)e->loop->radial.next->data)->f);
			if(count > 1) e->tflag1 &= ~BME_BEVEL_BEVEL;
		}
	}
}
static BMesh *BME_bevel_initialize(BMesh *bm, int options, int defgrp_index, float angle, BME_TransData_Head *td){

	BMVert *v, *v2;
	BMEdge *e, *curedge;
	BMFace *f;
	BMIter iter;
	int wire, len;
	
	for (v = BMIter_New(&iter, bm, BM_VERTS, nm); v; v = BMIter_Step(&iter)) v->tflag1 = 0;
	for (e = BMIter_New(&iter, bm, BM_EDGES, nm); e; e = BMIter_Step(&iter)) e->tflag1 = 0;
	for (f = BMIter_New(&iter, bm, BM_FACES, nm); f; f = BMIter_Step(&iter)) f->tflag1 = 0;

	/*tag non-manifold geometry*/
	for (v = BMIter_New(&iter, bm, BM_VERTS, nm); v; v = BMIter_Step(&iter)) {
		v->tflag1 = BME_BEVEL_ORIG;
		if(v->edge){ 
			BME_assign_transdata(td, bm, v, v->co, v->co, NULL, NULL, 0, -1, -1, NULL); 
			if (BM_Nonmanifold_Vert(bm,v)) v->tflag1 |= BME_BEVEL_NONMAN;
			/*test wire vert*/
			len = bmesh_cycle_length(bmesh_disk_getpointer(v->edge,v));
			if(len == 2 && BME_wirevert(bm, v)) v->tflag1 &= ~BME_BEVEL_NONMAN;	
		}else v->tflag1 |= BME_BEVEL_NONMAN;
	}

	for (e = BMIter_New(&iter, bm, BM_EDGES, nm); e; e = BMIter_Step(&iter)) {
		e->tflag1 = BME_BEVEL_ORIG;
		if (e->loop == NULL || BME_cycle_length(&(e->loop->radial)) > 2){
			e->v1->tflag1 |= BME_BEVEL_NONMAN;
			e->v2->tflag1 |= BME_BEVEL_NONMAN;
			e->tflag1 |= BME_BEVEL_NONMAN;
		}
		if((e->v1->tflag1 & BME_BEVEL_NONMAN) || (e->v2->tflag1 & BME_BEVEL_NONMAN)) e->tflag1 |= BME_BEVEL_NONMAN;
	}

	for (f = BMIter_New(&iter, bm, BM_FACES, nm); f; f = BMIter_Step(&iter))
		f->tflag1 = BME_BEVEL_ORIG;
	if(options & BME_BEVEL_VERT) bevel_init_verts(bm, options, td);
	else bevel_init_edges(bm, options, td);
	return bm;

}
static BMesh *BME_bevel_reinitialize(BMesh *bm)
{
	BMVert *v;
	BMEdge *e;
	BMFace *f;

	for (v = BMIter_New(bm, BM_VERTS, NULL); v; v = BMIter_Step(bm)){
		v->tflag1 |= BME_BEVEL_ORIG;
		v->tflag2 = 0;
	}
	for (e = BMIter_New(bm, BM_EDGES, NULL); e; e = BMIter_Step(bm)){
		e->tflag1 |= BME_BEVEL_ORIG;
	}
	for (f = BMIter_New(bm, BM_FACES, NULL); f; f = BMIter_Step(bm)){
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
 *  A BMesh pointer to the BM passed as a parameter.
*/

static BMMesh *BME_bevel_mesh(BMMesh *bm, float value, int res, int options, int defgrp_index, BME_TransData_Head *td) {
	BMVert *v, *nv;
	BMEdge *e, *curedge, *ne;
	BMLoop *l, *l2;
	BMFace *f, *nf;

	BMeshIter verts;
	BMeshIter edges;
	BMeshIter loops;
	BMeshIter faces; 

	unsigned int i, len;

	/*bevel polys*/
	for(f = BMeshIter_init(faces, BM_FACES, bm, 0); f; f = BMeshIter_step(faces)){
		if(bmesh_test_flag(f, BME_BEVEL_ORIG){
			bevel_poly(bm,f,value,options,td);
		}
	}
	/*get rid of beveled edges*/
	for(e = BMeshIter_init(edges, BM_EDGES, bm, 0); e;){
		ne = BMeshIter_step(edges);
		if( bmesh_test_flag(e, BME_BEVEL_BEVEL) && bmesh_test_flag(e, BME_BEVEL_ORIG) ) bmesh_join_faces(bm, e->loop->f,  ((BMLoop*)e->loop->radial.next->data)->f, e, 1);
		e = ne;
	}
	/*link up corners and clip*/
	for(v = BMeshIter_init(verts, BM_VERTS, bm, 0); v;){
		nv = BMeshIter_step(verts)
		if( bmesh_test_flag(v, BME_BEVEL_ORIG) && bmesh_test_flag(v, BME_BEVEL_BEVEL)){
			curedge = v->edge;
			do{
				l = curedge->loop;
				l2 = l->radial.next->data;
				if(l->v != v) l = l->next;
				if(l2->v != v) l2 = l2->next;
				if(l->f->len > 3) BM_split_face(bm,l->f,l->next->v,l->prev->v,&l,l->e); /* clip this corner off */
					if(l2->f->len > 3) BM_split_face(bm,l2->f,l2->next->v,l2->prev->v,&l,l2->e); /* clip this corner off */
				curedge = BM_edge_of_vert(curedge, v);
			}while(curedge != v->edge);
			BM_dissolve_disk(bm,v);
		}
		v = nv;
	}

	/*Debug print, remove*/
	for(f = BMeshIter_init(faces, BM_FACES, bm, 0); f;){
		if(f->len == 2){
			printf("warning");
		}
	}

	return bm;
}

BMesh *BME_bevel(BMMesh *bm, float value, int res, int options, int defgrp_index, float angle, BME_TransData_Head **rtd) {
	BMVert *v;
	BMEdge *e;
	BMIter *verts;

	BME_TransData_Head *td;
	BME_TransData *vtd;
	int i;
	double fac=1, d;


	td = BME_init_transdata(BLI_MEMARENA_STD_BUFSIZE);
	/* recursion math courtesy of Martin Poirier (theeth) */
	for (i=0; i<res-1; i++) {
		if (i==0) fac += 1.0f/3.0f; else fac += 1.0f/(3 * i * 2.0f);
	}
	d = 1.0f/fac;


	for (i=0; i<res || (res==0 && i==0); i++) {
		BME_bevel_initialize(bm, options, defgrp_index, angle, td);
		//if (i != 0) BME_bevel_reinitialize(bm);
		bmesh_begin_edit(bm);
		BME_bevel_mesh(bm,(float)d,res,options,defgrp_index,td);
		bmesh_end_edit(bm);
		if (i==0) d /= 3; else d /= 2;
	}

	BME_model_begin(bm);
	BME_tesselate(bm);
	BME_model_end(bm);


	/*interactive preview?*/
	if (rtd) {
		*rtd = td;
		return bm;
	}

	/* otherwise apply transforms */
	for( v = BMeshIter_init(verts); v; v = BMeshIter_step(verts)){
		if ( (vtd = BME_get_transdata(td, v)) ) {
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
