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
 * Contributor(s): Joseph Eagar.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/operators/bmo_dissolve.c
 *  \ingroup bmesh
 */

#include "MEM_guardedalloc.h"

#include "BLI_array.h"
#include "BLI_math.h"

#include "bmesh.h"
#include "intern/bmesh_private.h"

#include "intern/bmesh_operators_private.h" /* own include */

#define FACE_MARK	1
#define FACE_ORIG	2
#define FACE_NEW	4
#define EDGE_MARK	1

#define VERT_MARK	1

static int UNUSED_FUNCTION(check_hole_in_region)(BMesh *bm, BMFace *f)
{
	BMWalker regwalker;
	BMIter liter2;
	BMLoop *l2, *l3;
	BMFace *f2;

	/* checks if there are any unmarked boundary edges in the face regio */

	BMW_init(&regwalker, bm, BMW_ISLAND,
	         BMW_MASK_NOP, BMW_MASK_NOP, FACE_MARK,
	         BMW_FLAG_NOP,
	         BMW_NIL_LAY);

	f2 = BMW_begin(&regwalker, f);
	for ( ; f2; f2 = BMW_step(&regwalker)) {
		l2 = BM_iter_new(&liter2, bm, BM_LOOPS_OF_FACE, f2);
		for ( ; l2; l2 = BM_iter_step(&liter2)) {
			l3 = l2->radial_next;
			if ( BMO_elem_flag_test(bm, l3->f, FACE_MARK) !=
			     BMO_elem_flag_test(bm, l2->f, FACE_MARK))
			{
				if (!BMO_elem_flag_test(bm, l2->e, EDGE_MARK)) {
					return FALSE;
				}
			}
		}
	}
	BMW_end(&regwalker);

	return TRUE;
}

void bmo_dissolve_faces_exec(BMesh *bm, BMOperator *op)
{
	BMOIter oiter;
	BMFace *f, *f2 /* , *nf = NULL */;
	BLI_array_declare(faces);
	BLI_array_declare(regions);
	BMFace ***regions = NULL;
	BMFace **faces = NULL;
	BMWalker regwalker;
	int i;

	int use_verts = BMO_slot_bool_get(op, "use_verts");

	if (use_verts) {
		/* tag verts that start out with only 2 edges,
		 * don't remove these later */
		BMIter viter;
		BMVert *v;

		BM_ITER_MESH (v, &viter, bm, BM_VERTS_OF_MESH) {
			BMO_elem_flag_set(bm, v, VERT_MARK, (BM_vert_edge_count(v) != 2));
		}
	}

	BMO_slot_buffer_flag_enable(bm, op, "faces", BM_FACE, FACE_MARK);
	
	/* collect region */
	BMO_ITER (f, &oiter, bm, op, "faces", BM_FACE) {

		if (!BMO_elem_flag_test(bm, f, FACE_MARK)) {
			continue;
		}

		BLI_array_empty(faces);
		faces = NULL; /* forces different allocatio */

		BMW_init(&regwalker, bm, BMW_ISLAND,
		         BMW_MASK_NOP, BMW_MASK_NOP, FACE_MARK,
		         BMW_FLAG_NOP, /* no need to check BMW_FLAG_TEST_HIDDEN, faces are already marked by the bmo */
		         BMW_NIL_LAY);

		f2 = BMW_begin(&regwalker, f);
		for ( ; f2; f2 = BMW_step(&regwalker)) {
			BLI_array_append(faces, f2);
		}
		BMW_end(&regwalker);
		
		for (i = 0; i < BLI_array_count(faces); i++) {
			f2 = faces[i];
			BMO_elem_flag_disable(bm, f2, FACE_MARK);
			BMO_elem_flag_enable(bm, f2, FACE_ORIG);
		}

		if (BMO_error_occurred(bm)) {
			BMO_error_clear(bm);
			BMO_error_raise(bm, op, BMERR_DISSOLVEFACES_FAILED, NULL);
			goto cleanup;
		}
		
		BLI_array_append(faces, NULL);
		BLI_array_append(regions, faces);
	}
	
	for (i = 0; i < BLI_array_count(regions); i++) {
		int tot = 0;
		
		faces = regions[i];
		if (!faces[0]) {
			BMO_error_raise(bm, op, BMERR_DISSOLVEFACES_FAILED,
			                "Could not find boundary of dissolve region");
			goto cleanup;
		}
		
		while (faces[tot])
			tot++;
		
		f = BM_faces_join(bm, faces, tot, TRUE);
		if (!f) {
			BMO_error_raise(bm, op, BMERR_DISSOLVEFACES_FAILED,
			                "Could not create merged face");
			goto cleanup;
		}

		/* if making the new face failed (e.g. overlapping test)
		 * unmark the original faces for deletion */
		BMO_elem_flag_disable(bm, f, FACE_ORIG);
		BMO_elem_flag_enable(bm, f, FACE_NEW);

	}

	BMO_op_callf(bm, "del geom=%ff context=%i", FACE_ORIG, DEL_FACES);


	if (use_verts) {
		BMIter viter;
		BMVert *v;

		BM_ITER_MESH (v, &viter, bm, BM_VERTS_OF_MESH) {
			if (BMO_elem_flag_test(bm, v, VERT_MARK)) {
				if (BM_vert_edge_count(v) == 2) {
					BM_vert_collapse_edge(bm, v->e, v, TRUE);
				}
			}
		}
	}

	if (BMO_error_occurred(bm)) {
		goto cleanup;
	}

	BMO_slot_buffer_from_enabled_flag(bm, op, "regionout", BM_FACE, FACE_NEW);

cleanup:
	/* free/cleanup */
	for (i = 0; i < BLI_array_count(regions); i++) {
		if (regions[i]) MEM_freeN(regions[i]);
	}

	BLI_array_free(regions);
}

/* almost identical to dissolve edge, except it cleans up vertice */
void bmo_dissolve_edgeloop_exec(BMesh *bm, BMOperator *op)
{
	/* BMOperator fop; */
	BMOIter oiter;
	BMIter iter;
	BMVert *v, **verts = NULL;
	BLI_array_declare(verts);
	BMEdge *e;
	BMFace *fa, *fb;
	int i;


	BMO_ITER (e, &oiter, bm, op, "edges", BM_EDGE) {
		if (BM_edge_face_pair(e, &fa, &fb)) {
			BMO_elem_flag_enable(bm, e->v1, VERT_MARK);
			BMO_elem_flag_enable(bm, e->v2, VERT_MARK);

			/* BMESH_TODO - check on delaying edge removal since we may end up removing more then
			 * one edge, and later referene a removed edge */
			BM_faces_join_pair(bm, fa, fb, e, TRUE);
		}
	}

	BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
		if (BMO_elem_flag_test(bm, v, VERT_MARK) && BM_vert_edge_count(v) == 2) {
			BLI_array_append(verts, v);
		}
	}

	/* clean up extreneous 2-valence vertice */
	for (i = 0; i < BLI_array_count(verts); i++) {
		if (verts[i]->e) {
			BM_vert_collapse_edge(bm, verts[i]->e, verts[i], TRUE);
		}
	}
	
	BLI_array_free(verts);

	//BMO_op_initf(bm, &fop, "dissolve_faces faces=%ff", FACE_MARK);
	//BMO_op_exec(bm, &fop);

	//BMO_slot_copy(op, &fop, "regionout", "regionout");

	//BMO_op_finish(bm, &fop);
}


void bmo_dissolve_edges_exec(BMesh *bm, BMOperator *op)
{
	/* might want to make this an option or mode - campbell */

	/* BMOperator fop; */
	BMOIter eiter;
	BMEdge *e;

	BMIter viter;
	BMVert *v;

	int use_verts = BMO_slot_bool_get(op, "use_verts");

	if (use_verts) {
		BM_ITER_MESH (v, &viter, bm, BM_VERTS_OF_MESH) {
			BMO_elem_flag_set(bm, v, VERT_MARK, (BM_vert_edge_count(v) != 2));
		}
	}

	BMO_ITER (e, &eiter, bm, op, "edges", BM_EDGE) {
		BMFace *fa, *fb;

		if (BM_edge_face_pair(e, &fa, &fb)) {

			/* join faces */

			/* BMESH_TODO - check on delaying edge removal since we may end up removing more then
			 * one edge, and later referene a removed edge */
			BM_faces_join_pair(bm, fa, fb, e, TRUE);
		}
	}

	if (use_verts) {
		BM_ITER_MESH (v, &viter, bm, BM_VERTS_OF_MESH) {
			if (BMO_elem_flag_test(bm, v, VERT_MARK)) {
				if (BM_vert_edge_count(v) == 2) {
					BM_vert_collapse_edge(bm, v->e, v, TRUE);
				}
			}
		}
	}
}

static int test_extra_verts(BMesh *bm, BMVert *v)
{
	BMIter iter, liter, iter2, iter3;
	BMFace *f, *f2;
	BMLoop *l;
	BMEdge *e;
	int found;

	/* test faces around verts for verts that would be wrongly killed
	 * by dissolve faces. */
	f = BM_iter_new(&iter, bm, BM_FACES_OF_VERT, v);
	for ( ; f; f = BM_iter_step(&iter)) {
		l = BM_iter_new(&liter, bm, BM_LOOPS_OF_FACE, f);
		for ( ; l; l = BM_iter_step(&liter)) {
			if (!BMO_elem_flag_test(bm, l->v, VERT_MARK)) {
				/* if an edge around a vert is a boundary edge,
				 * then dissolve faces won't destroy it.
				 * also if it forms a boundary with one
				 * of the face region */
				found = FALSE;
				e = BM_iter_new(&iter2, bm, BM_EDGES_OF_VERT, l->v);
				for ( ; e; e = BM_iter_step(&iter2)) {
					if (BM_edge_is_boundary(e)) {
						found = TRUE;
					}
					f2 = BM_iter_new(&iter3, bm, BM_FACES_OF_EDGE, e);
					for ( ; f2; f2 = BM_iter_step(&iter3)) {
						if (!BMO_elem_flag_test(bm, f2, FACE_MARK)) {
							found = TRUE;
							break;
						}
					}
					if (found == TRUE) {
						break;
					}
				}
				if (found == FALSE) {
					return FALSE;
				}
			}
		}
	}

	return TRUE;
}
void bmo_dissolve_verts_exec(BMesh *bm, BMOperator *op)
{
	BMIter iter, fiter;
	BMVert *v;
	BMFace *f;
	/* int i; */

	BMO_slot_buffer_flag_enable(bm, op, "verts", BM_VERT, VERT_MARK);
	
	for (v = BM_iter_new(&iter, bm, BM_VERTS_OF_MESH, NULL); v; v = BM_iter_step(&iter)) {
		if (BMO_elem_flag_test(bm, v, VERT_MARK)) {
			/* check if it's a two-valence ver */
			if (BM_vert_edge_count(v) == 2) {

				/* collapse the ver */
				/* previously the faces were joined, but collapsing between 2 edges
				 * gives some advantage/difference in using vertex-dissolve over edge-dissolve */
#if 0
				BM_vert_collapse_faces(bm, v->e, v, 1.0f, TRUE, TRUE);
#else
				BM_vert_collapse_edge(bm, v->e, v, TRUE);
#endif

				continue;
			}

			f = BM_iter_new(&fiter, bm, BM_FACES_OF_VERT, v);
			for ( ; f; f = BM_iter_step(&fiter)) {
				BMO_elem_flag_enable(bm, f, FACE_ORIG);
				BMO_elem_flag_enable(bm, f, FACE_MARK);
			}
			
			/* check if our additions to the input to face dissolve
			 * will destroy nonmarked vertices. */
			if (!test_extra_verts(bm, v)) {
				f = BM_iter_new(&fiter, bm, BM_FACES_OF_VERT, v);
				for ( ; f; f = BM_iter_step(&fiter)) {
					if (BMO_elem_flag_test(bm, f, FACE_ORIG)) {
						BMO_elem_flag_disable(bm, f, FACE_MARK);
						BMO_elem_flag_disable(bm, f, FACE_ORIG);
					}
				}
			}
			else {
				f = BM_iter_new(&fiter, bm, BM_FACES_OF_VERT, v);
				for ( ; f; f = BM_iter_step(&fiter)) {
					BMO_elem_flag_disable(bm, f, FACE_ORIG);
				}
			}
		}
	}

	BMO_op_callf(bm, "dissolve_faces faces=%ff", FACE_MARK);
	if (BMO_error_occurred(bm)) {
		const char *msg;

		BMO_error_get(bm, &msg, NULL);
		BMO_error_clear(bm);
		BMO_error_raise(bm, op, BMERR_DISSOLVEVERTS_FAILED, msg);
	}
	
	/* clean up any remainin */
	for (v = BM_iter_new(&iter, bm, BM_VERTS_OF_MESH, NULL); v; v = BM_iter_step(&iter)) {
		if (BMO_elem_flag_test(bm, v, VERT_MARK)) {
			if (!BM_vert_dissolve(bm, v)) {
				BMO_error_raise(bm, op, BMERR_DISSOLVEVERTS_FAILED, NULL);
				return;
			}
		}
	}

}

/* this code is for cleaning up two-edged faces, it shall become
 * it's own function one day */
#if 0
void dummy_exec(BMesh *bm, BMOperator *op)
{
	{
		/* clean up two-edged face */
		/* basic idea is to keep joining 2-edged faces until their
		 * gone.  this however relies on joining two 2-edged faces
		 * together to work, which doesn't */
		found3 = 1;
		while (found3) {
			found3 = 0;
			for (f = BM_iter_new(&iter, bm, BM_FACES_OF_MESH, NULL); f; f = BM_iter_step(&iter)) {
				if (!BM_face_validate(bm, f, stderr)) {
					printf("error.\n");
				}

				if (f->len == 2) {
					//this design relies on join faces working
					//with two-edged faces properly.
					//commenting this line disables the
					//outermost loop.
					//found3 = 1;
					found2 = 0;
					l = BM_iter_new(&liter, bm, BM_LOOPS_OF_FACE, f);
					fe = l->e;
					for ( ; l; l = BM_iter_step(&liter)) {
						f2 = BM_iter_new(&fiter, bm,
								BM_FACES_OF_EDGE, l->e);
						for ( ; f2; f2 = BM_iter_step(&fiter)) {
							if (f2 != f) {
								BM_faces_join_pair(bm, f, f2, l->e);
								found2 = 1;
								break;
							}
						}
						if (found2) break;
					}

					if (!found2) {
						BM_face_kill(bm, f);
						BM_edge_kill(bm, fe);
					}
				}
#if 0
				else if (f->len == 3) {
					BMEdge *ed[3];
					BMVert *vt[3];
					BMLoop *lp[3];
					int i = 0;

					//check for duplicate edges
					l = BM_iter_new(&liter, bm, BM_LOOPS_OF_FACE, f);
					for ( ; l; l = BM_iter_step(&liter)) {
						ed[i] = l->e;
						lp[i] = l;
						vt[i++] = l->v;
					}
					if (vt[0] == vt[1] || vt[0] == vt[2]) {
						i += 1;
					}
#endif
			}
		}
		if (oldlen == len) break;
		oldlen = len;
	}
}

#endif

/* Limited Dissolve */

#define UNIT_TO_ANGLE DEG2RADF(90.0f)
#define ANGLE_TO_UNIT (1.0f / UNIT_TO_ANGLE)

/* multiply vertex edge angle by face angle
 * this means we are not left with sharp corners between _almost_ planer faces
 * convert angles [0-PI/2] -> [0-1], multiply together, then convert back to radians. */
float bm_vert_edge_face_angle(BMVert *v)
{
	const float angle = BM_vert_calc_edge_angle(v);
	/* note: could be either edge, it doesn't matter */
	if (v->e && BM_edge_is_manifold(v->e)) {
		return ((angle * ANGLE_TO_UNIT) * (BM_edge_calc_face_angle(v->e) * ANGLE_TO_UNIT)) * UNIT_TO_ANGLE;
	}
	else {
		return angle;
	}
}

#undef UNIT_TO_ANGLE
#undef ANGLE_TO_UNIT

typedef struct DissolveElemWeight {
	BMHeader *ele;
	float weight;
} DissolveElemWeight;

static int dissolve_elem_cmp(const void *a1, const void *a2)
{
	const struct DissolveElemWeight *d1 = a1, *d2 = a2;

	if      (d1->weight > d2->weight) return  1;
	else if (d1->weight < d2->weight) return -1;
	return 0;
}

void bmo_dissolve_limit_exec(BMesh *bm, BMOperator *op)
{
	BMOpSlot *einput = BMO_slot_get(op, "edges");
	BMOpSlot *vinput = BMO_slot_get(op, "verts");
	const float angle_max = (float)M_PI / 2.0f;
	const float angle_limit = minf(angle_max, BMO_slot_float_get(op, "angle_limit"));
	DissolveElemWeight *weight_elems = MEM_mallocN(MAX2(einput->len, vinput->len) *
	                                                 sizeof(DissolveElemWeight), __func__);
	int i, tot_found;

	BMIter iter;
	BMEdge *e_iter;
	BMEdge **earray;

	/* --- first edges --- */

	/* wire -> tag */
	BM_ITER_MESH(e_iter, &iter, bm, BM_EDGES_OF_MESH) {
		BM_elem_flag_set(e_iter, BM_ELEM_TAG, BM_edge_is_wire(e_iter));
	}

	/* go through and split edge */
	for (i = 0, tot_found = 0; i < einput->len; i++) {
		BMEdge *e = ((BMEdge **)einput->data.p)[i];
		const float angle = BM_edge_calc_face_angle(e);

		if (angle < angle_limit) {
			tot_found++;
		}
		weight_elems[i].ele = (BMHeader *)e;
		weight_elems[i].weight = angle;
	}

	if (tot_found != 0) {
		qsort(weight_elems, einput->len, sizeof(DissolveElemWeight), dissolve_elem_cmp);

		for (i = 0; i < tot_found; i++) {
			BMEdge *e = (BMEdge *)weight_elems[i].ele;

			if (/* may have become non-manifold */
			    BM_edge_is_manifold(e) &&
			    /* check twice because cumulative effect could dissolve over angle limit */
			    (BM_edge_calc_face_angle(e) < angle_limit))
			{
				BMFace *nf = BM_faces_join_pair(bm, e->l->f,
				                                e->l->radial_next->f,
				                                e,
				                                FALSE); /* join faces */

				/* there may be some errors, we don't mind, just move on */
				if (nf) {
					BM_face_normal_update(nf);
				}
				else {
					BMO_error_clear(bm);
				}
			}
		}
	}

	/* --- second verts --- */
	for (i = 0, tot_found = 0; i < vinput->len; i++) {
		BMVert *v = ((BMVert **)vinput->data.p)[i];
		const float angle = bm_vert_edge_face_angle(v);

		if (angle < angle_limit) {
			weight_elems[i].ele = (BMHeader *)v;
			weight_elems[i].weight = angle;
			tot_found++;
		}
		else {
			weight_elems[i].ele = NULL;
			weight_elems[i].weight = angle_max;
		}
	}

	if (tot_found != 0) {
		qsort(weight_elems, vinput->len, sizeof(DissolveElemWeight), dissolve_elem_cmp);

		for (i = 0; i < tot_found; i++) {
			BMVert *v = (BMVert *)weight_elems[i].ele;
			if (/* topology changes may cause this to be un-collapsable */
			    (BM_vert_edge_count(v) == 2) &&
			    /* check twice because cumulative effect could dissolve over angle limit */
			    bm_vert_edge_face_angle(v) < angle_limit)
			{
				BMEdge *ne = BM_vert_collapse_edge(bm, v->e, v, TRUE); /* join edges */

				if (ne && ne->l) {
					BM_edge_normals_update(ne);
				}
			}
		}
	}

	MEM_freeN(weight_elems);

	/* --- cleanup --- */
	earray = MEM_mallocN(sizeof(BMEdge *) * bm->totedge, __func__);
	BM_ITER_MESH_INDEX(e_iter, &iter, bm, BM_EDGES_OF_MESH, i) {
		earray[i] = e_iter;
	}
	/* remove all edges/verts left behind from dissolving */
	for (i = bm->totedge - 1; i != -1; i--) {
		e_iter = earray[i];

		if (BM_edge_is_wire(e_iter) && (BM_elem_flag_test(e_iter, BM_ELEM_TAG) == FALSE)) {
			/* edge has become wire */
			BMVert *v1 = e_iter->v1;
			BMVert *v2 = e_iter->v2;
			BM_edge_kill(bm, e_iter);
			if (v1->e == NULL) BM_vert_kill(bm, v1);
			if (v2->e == NULL) BM_vert_kill(bm, v2);
		}
	}

	MEM_freeN(earray);
}
