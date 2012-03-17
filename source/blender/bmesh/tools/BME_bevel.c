/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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
#include "BKE_tessmesh.h"
#include "BKE_bmesh.h"
#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_memarena.h"

#include "bmesh.h"
#include "intern/bmesh_private.h"

/* BMESH_TODO
 *
 * Date: 2011-11-24 06:25
 * Sender: Andrew Wiggin
 * Status update: I have code changes to actually make basic bevel modifier work. The things that still need to be done:
 * - clean up the changes
 * - get bevel by weight and bevel by angles working
 * - the code uses adaptations of a couple of bmesh APIs,
 * that work a little differently. for example, a join faces that doesn't just create a new face and then delete the
 * original two faces and all associated loops, it extends one of the original faces to cover all the original loops
 * (except for the loop on the join edge which is of course deleted). the bevel code currently requires this because it
 * expects to be able to continue walking loop lists and doesn't like for loops to be deleted out from under it
 * while working...
 * but bmesh APIs don't do it this way because it makes it trickier to manage the interp during these operations,
 * so I need to decide what to do in these cases.
 */

/* ------- Bevel code starts here -------- */

BME_TransData_Head *BME_init_transdata(int bufsize)
{
	BME_TransData_Head *td;

	td = MEM_callocN(sizeof(BME_TransData_Head), "BM transdata header");
	td->gh = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "BME_init_transdata gh");
	td->ma = BLI_memarena_new(bufsize, "BME_TransData arena");
	BLI_memarena_use_calloc(td->ma);

	return td;
}

void BME_free_transdata(BME_TransData_Head *td)
{
	BLI_ghash_free(td->gh, NULL, NULL);
	BLI_memarena_free(td->ma);
	MEM_freeN(td);
}

BME_TransData *BME_assign_transdata(
        BME_TransData_Head *td, BMesh *bm, BMVert *v,
        float *co, float *org, float *vec, float *loc,
        float factor, float weight, float maxfactor, float *max)
{
	BME_TransData *vtd;
	int is_new = 0;

	if (v == NULL) {
		return NULL;
	}

	if ((vtd = BLI_ghash_lookup(td->gh, v)) == NULL && bm != NULL) {
		vtd = BLI_memarena_alloc(td->ma, sizeof(*vtd));
		BLI_ghash_insert(td->gh, v, vtd);
		td->len++;
		is_new = 1;
	}

	vtd->bm = bm;
	vtd->v = v;

	if (co != NULL) {
		copy_v3_v3(vtd->co, co);
	}

	if (org == NULL && is_new) {
		copy_v3_v3(vtd->org, v->co); /* default */
	}
	else if (org != NULL) {
		copy_v3_v3(vtd->org, org);
	}

	if (vec != NULL) {
		copy_v3_v3(vtd->vec, vec);
		normalize_v3(vtd->vec);
	}

	vtd->loc = loc;

	vtd->factor = factor;
	vtd->weight = weight;
	vtd->maxfactor = maxfactor;
	vtd->max = max;

	return vtd;
}

BME_TransData *BME_get_transdata(BME_TransData_Head *td, BMVert *v)
{
	BME_TransData *vtd;
	vtd = BLI_ghash_lookup(td->gh, v);
	return vtd;
}

/* a hack (?) to use the transdata memarena to allocate floats for use with the max limits */
float *BME_new_transdata_float(BME_TransData_Head *td)
{
	return BLI_memarena_alloc(td->ma, sizeof(float));
}

/* BM_disk_dissolve is a real mess, and crashes bevel if called instead of this.
 * The drawback, though, is that this code doesn't merge customdata. */
static int BME_Bevel_Dissolve_Disk(BMesh *bm, BMVert *v)
{
	BMIter iter;
	BMEdge *e, *elast;
	BMLoop *l1, *l2;

	if (!BM_vert_is_manifold(bm, v)) {
		return 0;
	}

	BM_ITER(e, &iter, bm, BM_EDGES_OF_VERT, v) {
		if (BM_edge_face_count(e) != 2) {
			return 0;
		}
	}

	if (BM_vert_edge_count(v) > 2) {
		while (BM_vert_edge_count(v) > 2) {
			e = v->e;
			l1 = e->l;
			l2 = l1->radial_next;
			if (l1->v == l2->v) {
				/* faces have incompatible directions; need to reverse one */
				if (!bmesh_loop_reverse(bm, l2->f)) {
					BLI_assert(!"bevel dissolve disk cannot reverse loop");
					return 0;
				}
				l2 = l1->radial_next;
			}
			if (!bmesh_jfke(bm, l1->f, l2->f, e)) {
				BLI_assert(!"bevel dissolve disk cannot join faces");
				return 0;
			}
		}

		e = v->e;
		elast = bmesh_disk_edge_next(e, v);

		/* BMESH_TODO, figure out if its possible we had a double edge here and need to splice it,
		 * last bool arg */
		bmesh_jekv(bm, e, v, FALSE);

		l1 = elast->l;
		l2 = l1->radial_next;
		if (l1->v == l2->v) {
			/* faces have incompatible directions */
				if (!bmesh_loop_reverse(bm, l2->f)) {
					BLI_assert(!"bevel dissolve disk cannot reverse loop");
					return 0;
				}
				l2 = l1->radial_next;
		}
		bmesh_jfke(bm, l1->f, l2->f, elast);
	}

	return 1;
}

static int BME_bevel_is_split_vert(BMesh *bm, BMLoop *l)
{
	/* look for verts that have already been added to the edge when
	 * beveling other polys; this can be determined by testing the
	 * vert and the edges around it for originality
	 */
	if (!BMO_elem_flag_test(bm, l->v, BME_BEVEL_ORIG) &&
	     BMO_elem_flag_test(bm, l->e, BME_BEVEL_ORIG) &&
	     BMO_elem_flag_test(bm, l->prev->e, BME_BEVEL_ORIG))
	{
		return 1;
	}
	return 0;
}

/* get a vector, vec, that points from v1->co to wherever makes sense to
 * the bevel operation as a whole based on the relationship between v1 and v2
 * (won't necessarily be a vec from v1->co to v2->co, though it probably will be);
 * the return value is -1 for failure, 0 if we used vert co's, and 1 if we used transform origins */
static int BME_bevel_get_vec(float *vec, BMVert *v1, BMVert *v2, BME_TransData_Head *td)
{
	BME_TransData *vtd1, *vtd2;

	vtd1 = BME_get_transdata(td, v1);
	vtd2 = BME_get_transdata(td, v2);
	if (!vtd1 || !vtd2) {
		//printf("BME_bevel_get_vec() got called without proper BME_TransData\n");
		return -1;
	}

	/* compare the transform origins to see if we can use the vert co's;
	 * if they belong to different origins, then we will use the origins to determine
	 * the vector */
	if (compare_v3v3(vtd1->org, vtd2->org, 0.000001f)) {
		sub_v3_v3v3(vec, v2->co, v1->co);
		if (len_v3(vec) < 0.000001f) {
			zero_v3(vec);
		}
		return 0;
	}
	else {
		sub_v3_v3v3(vec, vtd2->org, vtd1->org);
		if (len_v3(vec) < 0.000001f) {
			zero_v3(vec);
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
static float BME_bevel_project_vec(float *vec1, float *vec2, float *up_vec, int is_forward, BME_TransData_Head *UNUSED(td))
{
	float factor, vec3[3], tmp[3], c1, c2;

	cross_v3_v3v3(tmp, vec1, vec2);
	normalize_v3(tmp);
	factor = dot_v3v3(up_vec, tmp);
	if ((factor > 0 && is_forward) || (factor < 0 && !is_forward)) {
		cross_v3_v3v3(vec3, vec2, tmp); /* hmm, maybe up_vec should be used instead of tmp */
	}
	else {
		cross_v3_v3v3(vec3, tmp, vec2); /* hmm, maybe up_vec should be used instead of tmp */
	}
	normalize_v3(vec3);
	c1 = dot_v3v3(vec3, vec1);
	c2 = dot_v3v3(vec1, vec1);
	if (fabsf(c1) < 0.000001f || fabsf(c2) < 0.000001f) {
		factor = 0.0f;
	}
	else {
		factor = c2 / c1;
	}

	return factor;
}

/* BME_bevel_split_edge() is the main math work-house; its responsibilities are:
 * using the vert and the loop passed, get or make the split vert, set its coordinates
 * and transform properties, and set the max limits.
 * Finally, return the split vert. */
static BMVert *BME_bevel_split_edge(BMesh *bm, BMVert *v, BMVert *v1, BMLoop *l, float *up_vec, float value, BME_TransData_Head *td)
{
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
		if (v->e == NULL) {
			//printf("We can't split a loose vert's edge!\n");
			return NULL;
		}
		e1 = v->e; /* we just use the first two edges */
		e2 = bmesh_disk_edge_next(v->e, v);
		if (e1 == e2) {
			//printf("You need at least two edges to use BME_bevel_split_edge()\n");
			return NULL;
		}
		v2 = BM_edge_other_vert(e1, v);
		v3 = BM_edge_other_vert(e2, v);
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
		ov = BM_edge_other_vert(e1, v);
		sv = BM_edge_split(bm, e1, v, &ne, 0);
		//BME_data_interp_from_verts(bm, v, ov, sv, 0.25); /* this is technically wrong.. */
		//BME_data_interp_from_faceverts(bm, v, ov, sv, 0.25);
		//BME_data_interp_from_faceverts(bm, ov, v, sv, 0.25);
		BME_assign_transdata(td, bm, sv, sv->co, sv->co, NULL, sv->co, 0, -1, -1, NULL); /* quick default */
		BMO_elem_flag_enable(bm, sv, BME_BEVEL_BEVEL);
		BMO_elem_flag_enable(bm, ne, BME_BEVEL_ORIG); /* mark edge as original, even though it isn't */
		BME_bevel_get_vec(vec1, v1, v, td);
		BME_bevel_get_vec(vec2, v2, v, td);
		cross_v3_v3v3(t_up_vec, vec1, vec2);
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

		if (BME_bevel_is_split_vert(bm, lv1)) {
			is_split_vert = 1;
			sv = v1;
			v1 = forward ? l->next->next->v : l->prev->v;
		}
		else {
			is_split_vert = 0;
			ov = BM_edge_other_vert(l->e, v);
			sv = BM_edge_split(bm, l->e, v, &ne, 0);
			//BME_data_interp_from_verts(bm, v, ov, sv, 0.25); /* this is technically wrong.. */
			//BME_data_interp_from_faceverts(bm, v, ov, sv, 0.25);
			//BME_data_interp_from_faceverts(bm, ov, v, sv, 0.25);
			BME_assign_transdata(td, bm, sv, sv->co, sv->co, NULL, sv->co, 0, -1, -1, NULL); /* quick default */
			BMO_elem_flag_enable(bm, sv, BME_BEVEL_BEVEL);
			BMO_elem_flag_enable(bm, ne, BME_BEVEL_ORIG); /* mark edge as original, even though it isn't */
		}

		if (BME_bevel_is_split_vert(bm, lv2)) {
			v2 = forward ? lv2->prev->v : lv2->next->v;
		}
	}

	is_edge = BME_bevel_get_vec(vec1, v, v1, td); /* get the vector we will be projecting onto */
	BME_bevel_get_vec(vec2, v, v2, td); /* get the vector we will be projecting parallel to */
	len = len_v3(vec1);
	normalize_v3(vec1);

	vtd = BME_get_transdata(td, sv);
	vtd1 = BME_get_transdata(td, v);
	vtd2 = BME_get_transdata(td, v1);

	if (vtd1->loc == NULL) {
		/* this is a vert with data only for calculating initial weights */
		if (vtd1->weight < 0) {
			vtd1->weight = 0;
		}
		scale = vtd1->weight / vtd1->factor;
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
		maxfactor = scale * BME_bevel_project_vec(vec1, vec2, up_vec, forward, td);
		if (vtd->maxfactor > 0 && vtd->maxfactor < maxfactor) {
			maxfactor = vtd->maxfactor;
		}
	}

	dis = BMO_elem_flag_test(bm, v1, BME_BEVEL_ORIG) ? len / 3 : len / 2;
	if (is_edge || dis > maxfactor * value) {
		dis = maxfactor * value;
	}
	madd_v3_v3v3fl(sv->co, v->co, vec1, dis);
	sub_v3_v3v3(vec1, sv->co, vtd1->org);
	dis = len_v3(vec1);
	normalize_v3(vec1);
	BME_assign_transdata(td, bm, sv, vtd1->org, vtd1->org, vec1, sv->co, dis, scale, maxfactor, vtd->max);

	return sv;
}

#if 0 /* UNUSED */
static float BME_bevel_set_max(BMVert *v1, BMVert *v2, float value, BME_TransData_Head *td)
{
	BME_TransData *vtd1, *vtd2;
	float max, fac1, fac2, vec1[3], vec2[3], vec3[3];

	BME_bevel_get_vec(vec1, v1, v2, td);
	vtd1 = BME_get_transdata(td, v1);
	vtd2 = BME_get_transdata(td, v2);

	if (vtd1->loc == NULL) {
		fac1 = 0;
	}
	else {
		copy_v3_v3(vec2, vtd1->vec);
		mul_v3_fl(vec2, vtd1->factor);
		if (dot_v3v3(vec1, vec1)) {
			project_v3_v3v3(vec2, vec2, vec1);
			fac1 = len_v3(vec2) / value;
		}
		else {
			fac1 = 0;
		}
	}

	if (vtd2->loc == NULL) {
		fac2 = 0;
	}
	else {
		copy_v3_v3(vec3, vtd2->vec);
		mul_v3_fl(vec3, vtd2->factor);
		if (dot_v3v3(vec1, vec1)) {
			project_v3_v3v3(vec2, vec3, vec1);
			fac2 = len_v3(vec2) / value;
		}
		else {
			fac2 = 0;
		}
	}

	if (fac1 || fac2) {
		max = len_v3(vec1) / (fac1 + fac2);
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
#endif

#if 0 /* UNUSED */
static BMVert *BME_bevel_wire(BMesh *bm, BMVert *v, float value, int res, int UNUSED(options), BME_TransData_Head *td)
{
	BMVert *ov1, *ov2, *v1, *v2;

	ov1 = BM_edge_other_vert(v->e, v);
	ov2 = BM_edge_other_vert(bmesh_disk_edge_next(v->e, v), v);

	/* split the edges */
	v1 = BME_bevel_split_edge(bm, v, ov1, NULL, NULL, value, td);
	BMO_elem_flag_enable(bm, v1, BME_BEVEL_NONMAN);
	v2 = BME_bevel_split_edge(bm, v, ov2, NULL, NULL, value, td);
	BMO_elem_flag_enable(bm, v2, BME_BEVEL_NONMAN);

	if (value > 0.5) {
		BME_bevel_set_max(v1, ov1, value, td);
		BME_bevel_set_max(v2, ov2, value, td);
	}

	/* remove the original vert */
	if (res) {
		/* bmesh_jekv; */

		//void BM_vert_collapse_faces(BMesh *bm, BMEdge *ke, BMVert *kv, float fac, int calcnorm) {
		//hrm, why is there a fac here? it just removes a vert
		BM_vert_collapse_edge(bm, v->e, v);
	}

	return v1;
}
#endif

static BMLoop *BME_bevel_edge(BMesh *bm, BMLoop *l, float value, int UNUSED(options), float *up_vec, BME_TransData_Head *td)
{
	BMVert *v1, *v2, *kv;
	BMLoop *kl = NULL, *nl;
	BMEdge *e, *ke, *se;
	BMFace *f, *jf;

	f = l->f;
	e = l->e;

	/* sanity check */
	if (!BMO_elem_flag_test(bm, l->e, BME_BEVEL_BEVEL) &&
	    (BMO_elem_flag_test(bm, l->v, BME_BEVEL_BEVEL) || BMO_elem_flag_test(bm, l->next->v, BME_BEVEL_BEVEL)))
	{
		return l;
	}

	/* checks and operations for prev edge */
	/* first, check to see if this edge was inset previously */
	if (!BMO_elem_flag_test(bm, l->prev->e, BME_BEVEL_ORIG) &&
	    !BMO_elem_flag_test(bm, l->v, BME_BEVEL_NONMAN))
	{
		kl = l->prev->radial_next;
		kl = (kl->v == l->v) ? kl->prev : kl->next;
		kv = l->v;
	}
	else {
		kv = NULL;
	}
	/* get/make the first vert to be used in SFME */
	if (BMO_elem_flag_test(bm, l->v, BME_BEVEL_NONMAN)) {
		v1 = l->v;
	}
	else { /* we'll need to split the previous edge */
		v1 = BME_bevel_split_edge(bm, l->v, NULL, l->prev, up_vec, value, td);
	}
	/* if we need to clean up geometry... */
	if (kv) {
		se = l->next->e;
		jf = NULL;
		if (kl->v == kv) {
			BM_face_split(bm, kl->f, kl->prev->v, kl->next->v, &nl, kl->prev->e, FALSE);
			ke = kl->e;
			/* BMESH-TODO: jfke doesn't handle customdata */
			jf = bmesh_jfke(bm, kl->prev->radial_next->f, kl->f, kl->prev->e);
			BM_vert_collapse_edge(bm, ke, kv, FALSE);
		}
		else {
			BM_face_split(bm, kl->f, kl->next->next->v, kl->v, &nl, kl->next->e, FALSE);
			ke = kl->e;
			/* BMESH-TODO: jfke doesn't handle customdata */
			jf = bmesh_jfke(bm, kl->next->radial_next->f, kl->f, kl->next->e);
			BM_vert_collapse_edge(bm, ke, kv, FALSE);
		}
		/* find saved loop pointer */
		l = se->l;
		while (l->f != jf) {
			l = l->radial_next;
			BLI_assert(l != se->l);
		}
		l = l->prev;
	}

	/* checks and operations for the next edge */
	/* first, check to see if this edge was inset previously  */
	if (!BMO_elem_flag_test(bm, l->next->e, BME_BEVEL_ORIG) &&
	    !BMO_elem_flag_test(bm, l->next->v, BME_BEVEL_NONMAN))
	{
		kl = l->next->radial_next;
		kl = (kl->v == l->next->v) ? kl->prev : kl->next;
		kv = l->next->v;
	}
	else {
		kv = NULL;
	}
	/* get/make the second vert to be used in SFME */
	if (BMO_elem_flag_test(bm, l->next->v, BME_BEVEL_NONMAN)) {
		v2 = l->next->v;
	}
	else { /* we'll need to split the next edge */
		v2 = BME_bevel_split_edge(bm, l->next->v, NULL, l->next, up_vec, value, td);
	}
	/* if we need to clean up geometry... */
	if (kv) {
		se = l->e;
		jf = NULL;
		if (kl->v == kv) {
			BM_face_split(bm, kl->f, kl->prev->v, kl->next->v, &nl, kl->prev->e, FALSE);
			ke = kl->e;
			/* BMESH-TODO: jfke doesn't handle customdata */
			jf = bmesh_jfke(bm, kl->prev->radial_next->f, kl->f, kl->prev->e);
			BM_vert_collapse_edge(bm, ke, kv, FALSE);
		}
		else {
			BM_face_split(bm, kl->f, kl->next->next->v, kl->v, &nl, kl->next->e, FALSE);
			ke = kl->e;
			/* BMESH-TODO: jfke doesn't handle customdata */
			jf = bmesh_jfke(bm, kl->next->radial_next->f, kl->f, kl->next->e);
			BM_vert_collapse_edge(bm, ke, kv, FALSE);
		}
		/* find saved loop pointer */
		l = se->l;
		while (l->f != jf) {
			l = l->radial_next;
			BLI_assert(l != se->l);
		}
	}

	if (!BMO_elem_flag_test(bm, v1, BME_BEVEL_NONMAN) || !BMO_elem_flag_test(bm, v2, BME_BEVEL_NONMAN)) {
		BM_face_split(bm, f, v2, v1, &l, e, FALSE);
		BMO_elem_flag_enable(bm, l->e, BME_BEVEL_BEVEL);
		l = l->radial_next;
	}

	if (l->f != f) {
		//printf("Whoops! You got something out of order in BME_bevel_edge()!\n");
	}

	return l;
}

static BMLoop *BME_bevel_vert(BMesh *bm, BMLoop *l, float value, int UNUSED(options), float *up_vec, BME_TransData_Head *td)
{
	BMVert *v1, *v2;
	BMFace *f;

	/* get/make the first vert to be used in SFME */
	/* may need to split the previous edge */
	v1 = BME_bevel_split_edge(bm, l->v, NULL, l->prev, up_vec, value, td);

	/* get/make the second vert to be used in SFME */
	/* may need to split this edge (so move l) */
	l = l->prev;
	v2 = BME_bevel_split_edge(bm, l->next->v, NULL, l->next, up_vec, value, td);
	l = l->next->next;

	/* "cut off" this corner */
	f = BM_face_split(bm, l->f, v2, v1, NULL, l->e, FALSE);

	return l;
}

/*
 *			BME_bevel_poly
 *
 *	Polygon inset tool:
 *
 *	Insets a polygon/face based on the flagss of its vertices
 *	and edges. Used by the bevel tool only, for now.
 *  The parameter "value" is the distance to inset (should be negative).
 *  The parameter "options" is not currently used.
 *
 *	Returns -
 *  A BMFace pointer to the resulting inner face.
 */
static BMFace *BME_bevel_poly(BMesh *bm, BMFace *f, float value, int options, BME_TransData_Head *td)
{
	BMLoop *l/*, *o */;
	BME_TransData *vtd1, *vtd2;
	float up_vec[3], vec1[3], vec2[3], vec3[3], fac1, fac2, max = -1;
	int len, i;
	BMIter iter;

	zero_v3(up_vec);

	/* find a good normal for this face (there's better ways, I'm sure) */
	BM_ITER(l, &iter, bm, BM_LOOPS_OF_FACE, f) {
		BME_bevel_get_vec(vec1, l->v, l->next->v, td);
		BME_bevel_get_vec(vec2, l->prev->v, l->v, td);
		cross_v3_v3v3(vec3, vec2, vec1);
		add_v3_v3(up_vec, vec3);
	}
	normalize_v3(up_vec);

	/* Can't use a BM_LOOPS_OF_FACE iterator here, because the loops are being modified
	 * and so the end condition will never hi */
	for (l = BM_FACE_FIRST_LOOP(f)->prev, i = 0, len = f->len; i < len; i++, l = l->next) {
		if (BMO_elem_flag_test(bm, l->e, BME_BEVEL_BEVEL) && BMO_elem_flag_test(bm, l->e, BME_BEVEL_ORIG)) {
			max = 1.0f;
			l = BME_bevel_edge(bm, l, value, options, up_vec, td);
		}
		else if (BMO_elem_flag_test(bm, l->v, BME_BEVEL_BEVEL) &&
		         BMO_elem_flag_test(bm, l->v, BME_BEVEL_ORIG) &&
		        !BMO_elem_flag_test(bm, l->prev->e, BME_BEVEL_BEVEL))
		{
			max = 1.0f;
			l = BME_bevel_vert(bm, l, value, options, up_vec, td);
		}
	}

	f = l->f;

	/* max pass */
	if (value > 0.5f && max > 0) {
		max = -1;
		BM_ITER(l, &iter, bm, BM_LOOPS_OF_FACE, f) {
			if (BMO_elem_flag_test(bm, l->e, BME_BEVEL_BEVEL) || BMO_elem_flag_test(bm, l->e, BME_BEVEL_ORIG)) {
				BME_bevel_get_vec(vec1, l->v, l->next->v, td);
				vtd1 = BME_get_transdata(td, l->v);
				vtd2 = BME_get_transdata(td, l->next->v);
				if (vtd1->loc == NULL) {
					fac1 = 0;
				}
				else {
					copy_v3_v3(vec2, vtd1->vec);
					mul_v3_fl(vec2, vtd1->factor);
					if (dot_v3v3(vec1, vec1)) {
						project_v3_v3v3(vec2, vec2, vec1);
						fac1 = len_v3(vec2) / value;
					}
					else {
						fac1 = 0;
					}
				}
				if (vtd2->loc == NULL) {
					fac2 = 0;
				}
				else {
					copy_v3_v3(vec3, vtd2->vec);
					mul_v3_fl(vec3, vtd2->factor);
					if (dot_v3v3(vec1, vec1)) {
						project_v3_v3v3(vec2, vec3, vec1);
						fac2 = len_v3(vec2) / value;
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

	/* return l->f; */
	return NULL;
}

static void BME_bevel_add_vweight(BME_TransData_Head *td, BMesh *bm, BMVert *v, float weight, float factor, int options)
{
	BME_TransData *vtd;

	if (BMO_elem_flag_test(bm, v, BME_BEVEL_NONMAN)) {
		return;
	}

	BMO_elem_flag_enable(bm, v, BME_BEVEL_BEVEL);
	if ((vtd = BME_get_transdata(td, v))) {
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

static void bevel_init_verts(BMesh *bm, int options, BME_TransData_Head *td)
{
	BMVert *v;
	BMIter iter;
	float weight;
	BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
		weight = 0.0f;
		if (!BMO_elem_flag_test(bm, v, BME_BEVEL_NONMAN)) {
			/* modifiers should not use selection */
			if (options & BME_BEVEL_SELECT) {
				if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
					weight = 1.0f;
				}
			}
			/* bevel weight NYI */
			else if (options & BME_BEVEL_WEIGHT) {
				weight = BM_elem_float_data_get(&bm->vdata, v, CD_BWEIGHT);
			}
			else {
				weight = 1.0f;
			}

			if (weight > 0.0f) {
				BMO_elem_flag_enable(bm, v, BME_BEVEL_BEVEL);
				BME_assign_transdata(td, bm, v, v->co, v->co, NULL, NULL, 1.0, weight, -1, NULL);
			}
		}
	}
}

static void bevel_init_edges(BMesh *bm, int options, BME_TransData_Head *td)
{
	BMEdge *e;
	int count;
	float weight;
	BMIter iter;
	BM_ITER(e, &iter, bm, BM_EDGES_OF_MESH, NULL) {
		weight = 0.0;
		if (!BMO_elem_flag_test(bm, e, BME_BEVEL_NONMAN)) {
			if(options & BME_BEVEL_SELECT) {
				if(BM_elem_flag_test(e, BM_ELEM_SELECT)) weight = 1.0;
			}
			else if(options & BME_BEVEL_WEIGHT) {
				weight = BM_elem_float_data_get(&bm->edata, e, CD_BWEIGHT);
			}
			else {
				weight = 1.0;
			}

			if(weight > 0.0) {
				BMO_elem_flag_enable(bm, e, BME_BEVEL_BEVEL);
				BMO_elem_flag_enable(bm, e->v1, BME_BEVEL_BEVEL);
				BMO_elem_flag_enable(bm, e->v2, BME_BEVEL_BEVEL);
				BME_bevel_add_vweight(td, bm, e->v1, weight, 1.0, options);
				BME_bevel_add_vweight(td, bm, e->v2, weight, 1.0, options);
			}
		}
	}

	/* clean up edges with 2 faces that share more than one edg */
	BM_ITER(e, &iter, bm, BM_EDGES_OF_MESH, NULL) {
		if(BMO_elem_flag_test(bm, e, BME_BEVEL_BEVEL)) {
			count = BM_face_share_edge_count(e->l->f, e->l->radial_next->f);
			if(count > 1) BMO_elem_flag_disable(bm, e, BME_BEVEL_BEVEL);
		}
	}
}

static BMesh *BME_bevel_initialize(BMesh *bm, int options, int UNUSED(defgrp_index), float UNUSED(angle), BME_TransData_Head *td)
{
	BMVert *v/*, *v2 */;
	BMEdge *e/*, *curedg */;
	BMFace *f;
	BMIter iter;
	int /* wire, */ len;

	/* tag non-manifold geometr */
	BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
		BMO_elem_flag_enable(bm, v, BME_BEVEL_ORIG);
		if(v->e) {
			BME_assign_transdata(td, bm, v, v->co, v->co, NULL, NULL, 0, -1, -1, NULL);
			if (!BM_vert_is_manifold(bm, v)) {
				BMO_elem_flag_enable(bm, v, BME_BEVEL_NONMAN);
			}

			/* test wire ver */
			len = BM_vert_edge_count(v);
			if (len == 2 && BM_vert_is_wire(bm, v))
				BMO_elem_flag_disable(bm, v, BME_BEVEL_NONMAN);
		}
		else {
			BMO_elem_flag_enable(bm, v, BME_BEVEL_NONMAN);
		}
	}

	BM_ITER(e, &iter, bm, BM_EDGES_OF_MESH, NULL) {
		BMO_elem_flag_enable(bm, e, BME_BEVEL_ORIG);
		if (!BM_edge_is_manifold(bm, e)) {
			BMO_elem_flag_enable(bm, e->v1, BME_BEVEL_NONMAN);
			BMO_elem_flag_enable(bm, e->v2, BME_BEVEL_NONMAN);
			BMO_elem_flag_enable(bm, e, BME_BEVEL_NONMAN);
		}
		if(BMO_elem_flag_test(bm, e->v1, BME_BEVEL_NONMAN) || BMO_elem_flag_test(bm, e->v2, BME_BEVEL_NONMAN)) {
			BMO_elem_flag_enable(bm, e, BME_BEVEL_NONMAN);
		}
	}

	BM_ITER(f, &iter, bm, BM_FACES_OF_MESH, NULL) {
		BMO_elem_flag_enable(bm, f, BME_BEVEL_ORIG);
	}

	if(options & BME_BEVEL_VERT) {
		bevel_init_verts(bm, options, td);
	}
	else {
		bevel_init_edges(bm, options, td);
	}

	return bm;

}

#if 0

static BMesh *BME_bevel_reinitialize(BMesh *bm)
{
	BMVert *v;
	BMEdge *e;
	BMFace *f;
	BMIter iter;

	BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
		BMO_elem_flag_enable(bm, v, BME_BEVEL_ORIG);
	}
	BM_ITER(e, &iter, bm, BM_EDGES_OF_MESH, NULL) {
		BMO_elem_flag_enable(bm, e, BME_BEVEL_ORIG);
	}
	BM_ITER(f, &iter, bm, BM_FACES_OF_MESH, NULL) {
		BMO_elem_flag_enable(bm, f, BME_BEVEL_ORIG);
	}
	return bm;

}

#endif

/**
 *			BME_bevel_mesh
 *
 *	Mesh beveling tool:
 *
 *	Bevels an entire mesh. It currently uses the flags of
 *	its vertices and edges to track topological changes.
 *  The parameter "value" is the distance to inset (should be negative).
 *  The parameter "options" is not currently used.
 *
 *	Returns -
 *  A BMesh pointer to the BM passed as a parameter.
 */

static BMesh *BME_bevel_mesh(BMesh *bm, float value, int UNUSED(res), int options, int UNUSED(defgrp_index), BME_TransData_Head *td)
{
	BMVert *v;
	BMEdge *e, *curedge;
	BMLoop *l, *l2;
	BMFace *f;
	BMIter iter;

	/* unsigned int i, len; */

	/* bevel poly */
	BM_ITER(f, &iter, bm, BM_FACES_OF_MESH, NULL) {
		if(BMO_elem_flag_test(bm, f, BME_BEVEL_ORIG)) {
			BME_bevel_poly(bm, f, value, options, td);
		}
	}

	/* get rid of beveled edge */
	BM_ITER(e, &iter, bm, BM_EDGES_OF_MESH, NULL) {
		if(BMO_elem_flag_test(bm, e, BME_BEVEL_BEVEL) && BMO_elem_flag_test(bm, e, BME_BEVEL_ORIG)) {
			BM_faces_join_pair(bm, e->l->f, e->l->radial_next->f, e);
		}
	}

	/* link up corners and cli */
	BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
		if(BMO_elem_flag_test(bm, v, BME_BEVEL_ORIG) && BMO_elem_flag_test(bm, v, BME_BEVEL_BEVEL)) {
			curedge = v->e;
			do {
				l = curedge->l;
				l2 = l->radial_next;
				if(l->v != v) l = l->next;
				if(l2->v != v) l2 = l2->next;
				if(l->f->len > 3)
					BM_face_split(bm, l->f, l->next->v, l->prev->v, &l, l->e, FALSE); /* clip this corner off */
				if(l2->f->len > 3)
					BM_face_split(bm, l2->f, l2->next->v, l2->prev->v, &l, l2->e, FALSE); /* clip this corner off */
				curedge = bmesh_disk_edge_next(curedge, v);
			} while(curedge != v->e);
			BME_Bevel_Dissolve_Disk(bm, v);
		}
	}

	/* Debug print, remov */
	BM_ITER(f, &iter, bm, BM_FACES_OF_MESH, NULL) {
		if(f->len == 2) {
			printf("warning");
		}
	}

	return bm;
}

BMesh *BME_bevel(BMEditMesh *em, float value, int res, int options, int defgrp_index, float angle,
                 BME_TransData_Head **rtd, int do_tessface)
{
	BMesh *bm = em->bm;
	BMVert *v;
	BMIter iter;

	BME_TransData_Head *td;
	BME_TransData *vtd;
	int i;
	double fac = 1, d;

	td = BME_init_transdata(BLI_MEMARENA_STD_BUFSIZE);
	/* recursion math courtesy of Martin Poirier (theeth) */
	for (i = 0; i < res - 1; i++) {
		if (i == 0) fac += 1.0f / 3.0f; else fac += 1.0f / (3 * i * 2.0f);
	}
	d = 1.0f / fac;

	for (i = 0; i < res || (res == 0 && i == 0); i++) {
		BMO_push(bm, NULL);
		BME_bevel_initialize(bm, options, defgrp_index, angle, td);
		//if (i != 0) BME_bevel_reinitialize(bm);
		bmesh_edit_begin(bm, 0);
		BME_bevel_mesh(bm, (float)d, res, options, defgrp_index, td);
		bmesh_edit_end(bm, 0);
		d /= (i == 0) ? 3.0 : 2.0;
		BMO_pop(bm);
	}

	/* possibly needed when running as a tool (which is no longer functional)
	 * but keep as an optioin for now */
	if (do_tessface) {
		BMEdit_RecalcTessellation(em);
	}

	/* interactive preview? */
	if (rtd) {
		*rtd = td;
		return bm;
	}

	/* otherwise apply transforms */
	BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
		if ((vtd = BME_get_transdata(td, v))) {
			if (vtd->max && (*vtd->max > 0 && value > *vtd->max)) {
				d = *vtd->max;
			}
			else {
				d = value;
			}
			madd_v3_v3v3fl(v->co, vtd->org, vtd->vec, vtd->factor * d);
		}
	}

	BME_free_transdata(td);
	return bm;
}
