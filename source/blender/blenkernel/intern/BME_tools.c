/**
 * BME_tools.c    jan 2007
 *
 *	Functions for changing the topology of a mesh.
 *
 * $Id$
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
#include "DNA_object_types.h"

#include "BKE_utildefines.h"
#include "BKE_bmesh.h"
#include "BLI_arithb.h"
#include "BLI_blenlib.h"

/*split this all into a seperate bevel.c file in src*/

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

static int BME_is_nonmanifold_vert(BME_Mesh *bm, BME_Vert *v) {
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
static BME_Poly *BME_JFKE_safe(BME_Mesh *bm, BME_Poly *f1, BME_Poly *f2, BME_Edge *e) {
	BME_Loop *l1, *l2;

	l1 = e->loop;
	l2 = l1->radial.next->data;
	if (l1->v == l2->v) {
		BME_loop_reverse(bm, f2);
	}

	return BME_JFKE(bm, f1, f2, e);
}

/* a wrapper for BME_SFME that transfers element flags */
static BME_Poly *BME_split_face(BME_Mesh *bm, BME_Poly *f, BME_Vert *v1, BME_Vert *v2, BME_Loop **nl, BME_Edge *example) {
	BME_Poly *nf;
	nf = BME_SFME(bm,f,v1,v2,nl);
	nf->flag = f->flag;
	/* if the edge was selected, select this face, too */
	if (example && (example->flag & SELECT)) f->flag |= ME_FACE_SEL;
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


#if 0
static void BME_data_interp_from_verts(BME_Mesh *bm, BME_Vert *v1, BME_Vert *v2, BME_Vert *v, float fac)
{
	void *src[2];
	float w[2];
	if (v1->data && v2->data) {
		src[0]= v1->data;
		src[1]= v2->data;
		w[0] = 1.0f-fac;
		w[1] = fac;
		CustomData_bmesh_interp(&bm->vdata, src, w, NULL, 2, v->data);
	}
}
#endif


static void BME_data_facevert_edgesplit(BME_Mesh *bm, BME_Vert *v1, BME_Vert *v2, BME_Vert *v, BME_Edge *e1, float fac){
	void *src[2];
	float w[2];
	BME_Loop *l=NULL, *v1loop = NULL, *vloop = NULL, *v2loop = NULL;
	
	w[0] = 1.0f - fac;
	w[1] = fac;

	if(!e1->loop) return;
	l = e1->loop;
	do{
		if(l->v == v1){ 
			v1loop = l;
			vloop = v1loop->next;
			v2loop = vloop->next;
		}else if(l->v == v){
			v1loop = l->next;
			vloop = l;
			v2loop = l->prev;
			
		}

		src[0] = v1loop->data;
		src[1] = v2loop->data;					

		CustomData_bmesh_interp(&bm->ldata, src,w, NULL, 2, vloop->data); 				
		l = l->radial.next->data;
	}while(l!=e1->loop);
}


/* a wrapper for BME_SEMV that transfers element flags */ /*add custom data interpolation in here!*/
static BME_Vert *BME_split_edge(BME_Mesh *bm, BME_Vert *v, BME_Edge *e, BME_Edge **ne, float percent) {
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
	/*v->nv->v2*/
	BME_data_facevert_edgesplit(bm,v2, v, nv, e, 0.75);	
	return nv;
}

static void BME_collapse_vert(BME_Mesh *bm, BME_Edge *ke, BME_Vert *kv, float fac){
	void *src[2];
	float w[2];
	BME_Loop *l=NULL, *kvloop=NULL, *tvloop=NULL;
	BME_Vert *tv = BME_edge_getothervert(ke,kv);

	w[0] = 1.0f - fac;
	w[1] = fac;

	if(ke->loop){
		l = ke->loop;
		do{
			if(l->v == tv && l->next->v == kv){
				tvloop = l;
				kvloop = l->next;

				src[0] = kvloop->data;
				src[1] = tvloop->data;
				CustomData_bmesh_interp(&bm->ldata, src,w, NULL, 2, kvloop->data); 								
			}
			l=l->radial.next->data;
		}while(l!=ke->loop);
	}
	BME_JEKV(bm,ke,kv);
}



static int BME_bevel_is_split_vert(BME_Loop *l) {
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
static int BME_bevel_get_vec(float *vec, BME_Vert *v1, BME_Vert *v2, BME_TransData_Head *td) {
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
static float BME_bevel_project_vec(float *vec1, float *vec2, float *up_vec, int is_forward, BME_TransData_Head *td) {
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
static BME_Vert *BME_bevel_split_edge(BME_Mesh *bm, BME_Vert *v, BME_Vert *v1, BME_Loop *l, float *up_vec, float value, BME_TransData_Head *td) {
	BME_TransData *vtd, *vtd1, *vtd2;
	BME_Vert *sv, *v2, *v3, *ov;
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
			//printf("We can't split a loose vert's edge!\n");
			return NULL;
		}
		e1 = v->edge; /* we just use the first two edges */
		e2 = BME_disk_nextedge(v->edge, v);
		if (e1 == e2) {
			//printf("You need at least two edges to use BME_bevel_split_edge()\n");
			return NULL;
		}
		v2 = BME_edge_getothervert(e1, v);
		v3 = BME_edge_getothervert(e2, v);
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
		ov = BME_edge_getothervert(e1,v);
		sv = BME_split_edge(bm,v,e1,&ne,0);
		//BME_data_interp_from_verts(bm, v, ov, sv, 0.25); /*this is technically wrong...*/
		//BME_data_interp_from_faceverts(bm, v, ov, sv, 0.25);
		//BME_data_interp_from_faceverts(bm, ov, v, sv, 0.25);
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
			ov = BME_edge_getothervert(l->e,v);
			sv = BME_split_edge(bm,v,l->e,&ne,0);
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

static float BME_bevel_set_max(BME_Vert *v1, BME_Vert *v2, float value, BME_TransData_Head *td) {
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

static BME_Vert *BME_bevel_wire(BME_Mesh *bm, BME_Vert *v, float value, int res, int options, BME_TransData_Head *td) {
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

static BME_Loop *BME_bevel_edge(BME_Mesh *bm, BME_Loop *l, float value, int options, float *up_vec, BME_TransData_Head *td) {
	BME_Vert *v1, *v2, *kv;
	BME_Loop *kl=NULL, *nl;
	BME_Edge *e;
	BME_Poly *f;

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
			BME_collapse_vert(bm, kl->e, kv, 1.0);
			//BME_JEKV(bm,kl->e,kv);
			
		}
		else {
			BME_split_face(bm,kl->f,kl->next->next->v,kl->v,&nl,kl->next->e);
			BME_JFKE(bm,((BME_Loop*)kl->next->radial.next->data)->f,kl->f,kl->next->e);
			BME_collapse_vert(bm, kl->e, kv, 1.0);
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
			BME_split_face(bm,kl->f,kl->prev->v,kl->next->v,&nl,kl->prev->e);
			BME_JFKE(bm,((BME_Loop*)kl->prev->radial.next->data)->f,kl->f,kl->prev->e);
			BME_collapse_vert(bm, kl->e, kv, 1.0);
			//BME_JEKV(bm,kl->e,kv);
		}
		else {
			BME_split_face(bm,kl->f,kl->next->next->v,kl->v,&nl,kl->next->e);
			BME_JFKE(bm,((BME_Loop*)kl->next->radial.next->data)->f,kl->f,kl->next->e);
			BME_collapse_vert(bm, kl->e, kv, 1.0);
			//BME_JEKV(bm,kl->e,kv);
		}
	}

	if ((v1->tflag1 & BME_BEVEL_NONMAN)==0 || (v2->tflag1 & BME_BEVEL_NONMAN)==0) {
		BME_split_face(bm,f,v2,v1,&l,e);
		l->e->tflag1 = BME_BEVEL_BEVEL;
		l = l->radial.next->data;
	}

	if (l->f != f){
		//printf("Whoops! You got something out of order in BME_bevel_edge()!\n");
	}

	return l;
}

static BME_Loop *BME_bevel_vert(BME_Mesh *bm, BME_Loop *l, float value, int options, float *up_vec, BME_TransData_Head *td) {
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
static BME_Poly *BME_bevel_poly(BME_Mesh *bm, BME_Poly *f, float value, int options, BME_TransData_Head *td) {
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

static void BME_bevel_add_vweight(BME_TransData_Head *td, BME_Mesh *bm, BME_Vert *v, float weight, float factor, int options) {
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

static float BME_bevel_get_angle(BME_Mesh *bm, BME_Edge *e, BME_Vert *v) {
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
static int BME_face_sharededges(BME_Poly *f1, BME_Poly *f2){
	BME_Loop *l;
	int count = 0;
	
	l = f1->loopbase;
	do{
		if(BME_radial_find_face(l->e,f2)) count++;
		l = l->next;
	}while(l != f1->loopbase);
	
	return count;
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
static BME_Mesh *BME_bevel_initialize(BME_Mesh *bm, int options, int defgrp_index, float angle, BME_TransData_Head *td) {
	BME_Vert *v;
	BME_Edge *e;
	BME_Poly *f;
	BME_TransData *vtd;
	MDeformVert *dvert;
	MDeformWeight *dw;
	int len;
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

	/*clean up edges with 2 faces that share more than one edge*/
	for (e=bm->edges.first; e; e=e->next){
		if(e->tflag1 & BME_BEVEL_BEVEL){
			int count = 0;
			count = BME_face_sharededges(e->loop->f, ((BME_Loop*)e->loop->radial.next->data)->f);
			if(count > 1){
				e->tflag1 &= ~BME_BEVEL_BEVEL;
			}	
		}
	}

	return bm;
}

/* tags all elements as originals */
static BME_Mesh *BME_bevel_reinitialize(BME_Mesh *bm) {
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

static void bmesh_dissolve_disk(BME_Mesh *bm, BME_Vert *v){
	BME_Poly *f;
	BME_Edge *e;
	int done, len;
	
	if(v->edge){
		done = 0;
		while(!done){
			done = 1;
			e = v->edge; /*loop the edge looking for a edge to dissolve*/
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
		BME_collapse_vert(bm, v->edge, v, 1.0);
		//BME_JEKV(bm,v->edge,v);
	}
}
static BME_Mesh *BME_bevel_mesh(BME_Mesh *bm, float value, int res, int options, int defgrp_index, BME_TransData_Head *td) {
	BME_Vert *v, *nv;
	BME_Edge *e, *oe;
	BME_Loop *l, *l2;
	BME_Poly *f;
	unsigned int i, len;

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
			int count = 0;
			/* first, make sure we're not sitting on an edge to be removed */
			oe = v->edge;
			e = BME_disk_nextedge(oe,v);
			while ((e->tflag1 & BME_BEVEL_BEVEL) && (e->tflag1 & BME_BEVEL_ORIG)) {
				e = BME_disk_nextedge(e,v);
				if (e == oe) {
					//printf("Something's wrong! We can't remove every edge here!\n");
					break;
				}
			}
			/* look for original edges, and remove them */
			oe = e;
			while ( (e = BME_disk_next_edgeflag(oe, v, 0, BME_BEVEL_ORIG | BME_BEVEL_BEVEL)) ) {
				count++;
				/* join the faces (we'll split them later) */
				f = BME_JFKE_safe(bm,e->loop->f,((BME_Loop*)e->loop->radial.next->data)->f,e);
				if (!f){
					//printf("Non-manifold geometry not getting tagged right?\n");
				}
			}

			/*need to do double check *before* you bevel to make sure that manifold edges are for two faces that share only *one* edge to make sure it doesnt hang here!*/


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
			bmesh_dissolve_disk(bm, v);
		}
		v = nv;
	}

	return bm;
}

static BME_Mesh *BME_tesselate(BME_Mesh *bm) {
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


/*Main bevel function:
	Should be only one exported

*/

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
		BME_model_begin(bm);
		BME_bevel_mesh(bm,d,res,options,defgrp_index,td);
		BME_model_end(bm);
		if (i==0) d /= 3; else d /= 2;
	}

	BME_tesselate(bm);

	if (rtd) {
		*rtd = td;
		return bm;
	}

	/* transform pass */
	for (v = bm->verts.first; v; v=v->next) {
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
