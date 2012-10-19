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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/intern/bmesh_decimate.c
 *  \ingroup bmesh
 *
 * BMesh decimator.
 */

#include <stddef.h>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"

#include "BLI_math.h"
#include "BLI_quadric.h"
#include "BLI_heap.h"

#include "bmesh.h"
#include "bmesh_structure.h"
#include "bmesh_decimate.h"

/* defines for testing */
#define USE_CUSTOMDATA
#define USE_TRIANGULATE

/* these checks are for rare cases that we can't avoid since they are valid meshes still */
#define USE_SAFETY_CHECKS

#define BOUNDARY_PRESERVE_WEIGHT 100.0f


/* BMesh Helper Functions
 * ********************** */

/**
 * \param vquadrics must be calloc'd
 */
static void bm_decim_build_quadrics(BMesh *bm, Quadric *vquadrics)
{
	BMIter iter;
	BMFace *f;
	BMEdge *e;

	BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
		BMLoop *l_first;
		BMLoop *l_iter;

		const float *co = BM_FACE_FIRST_LOOP(f)->v->co;
		const float *no = f->no;
		const float offset = -dot_v3v3(no, co);
		Quadric q;

		BLI_quadric_from_v3_dist(&q, no, offset);

		l_iter = l_first = BM_FACE_FIRST_LOOP(f);
		do {
			BLI_quadric_add_qu_qu(&vquadrics[BM_elem_index_get(l_iter->v)], &q);
		} while ((l_iter = l_iter->next) != l_first);
	}

	/* boundary edges */
	BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
		if (UNLIKELY(BM_edge_is_boundary(e))) {
			float edge_vector[3];
			float edge_cross[3];
			sub_v3_v3v3(edge_vector, e->v2->co, e->v1->co);
			f = e->l->f;
			cross_v3_v3v3(edge_cross, edge_vector, f->no);

			if (normalize_v3(edge_cross) != 0.0f) {
				Quadric q;
				BLI_quadric_from_v3_dist(&q, edge_vector, -dot_v3v3(edge_cross, e->v1->co));
				BLI_quadric_mul(&q, BOUNDARY_PRESERVE_WEIGHT);

				BLI_quadric_add_qu_qu(&vquadrics[BM_elem_index_get(e->v1)], &q);
				BLI_quadric_add_qu_qu(&vquadrics[BM_elem_index_get(e->v2)], &q);
			}
		}
	}
}


static void bm_decim_calc_target_co(BMEdge *e, float optimize_co[3],
                                    const Quadric *vquadrics)
{
	/* compute an edge contration target for edge ei
	 * this is computed by summing it's vertices quadrics and
	 * optimizing the result. */
	Quadric q;

	BLI_quadric_add_qu_ququ(&q,
	                        &vquadrics[BM_elem_index_get(e->v1)],
	                        &vquadrics[BM_elem_index_get(e->v2)]);


	if (BLI_quadric_optimize(&q, optimize_co)) {
		return;  /* all is good */
	}
	else {
		mid_v3_v3v3(optimize_co, e->v1->co, e->v2->co);
	}
}

static void bm_decim_build_edge_cost_single(BMEdge *e,
                                            const Quadric *vquadrics,
                                            Heap *eheap, HeapNode **eheap_table)
{
	const Quadric *q1, *q2;
	float optimize_co[3];
	float cost;

	if (eheap_table[BM_elem_index_get(e)]) {
		BLI_heap_remove(eheap, eheap_table[BM_elem_index_get(e)]);
	}

	/* check we can collapse, some edges we better not touch */
	if (BM_edge_is_boundary(e)) {
		if (e->l->f->len == 3) {
			/* pass */
		}
		else {
			/* only collapse tri's */
			eheap_table[BM_elem_index_get(e)] = NULL;
			return;
		}
	}
	else if (BM_edge_is_manifold(e)) {
		if ((e->l->f->len == 3) && (e->l->radial_next->f->len == 3)) {
			/* pass */
		}
		else {
			/* only collapse tri's */
			eheap_table[BM_elem_index_get(e)] = NULL;
			return;
		}
	}
	else {
		eheap_table[BM_elem_index_get(e)] = NULL;
		return;
	}
	/* end sanity check */


	bm_decim_calc_target_co(e, optimize_co, vquadrics);

	q1 = &vquadrics[BM_elem_index_get(e->v1)];
	q2 = &vquadrics[BM_elem_index_get(e->v2)];

	cost = (BLI_quadric_evaluate(q1, optimize_co) + BLI_quadric_evaluate(q2, optimize_co));

	eheap_table[BM_elem_index_get(e)] = BLI_heap_insert(eheap, cost, e);
}

static void bm_decim_build_edge_cost(BMesh *bm,
                                     const Quadric *vquadrics,
                                     Heap *eheap, HeapNode **eheap_table)
{
	BMIter iter;
	BMEdge *e;
	unsigned int i;

	BM_ITER_MESH_INDEX (e, &iter, bm, BM_EDGES_OF_MESH, i) {
		eheap_table[i] = NULL;  /* keep sanity check happy */
		bm_decim_build_edge_cost_single(e, vquadrics, eheap, eheap_table);
	}
}

#ifdef USE_TRIANGULATE
/* Temp Triangulation
 * ****************** */

/**
 * To keep things simple we can only collapse edges on triangulated data
 * (limitation with edge collapse and error calculation functions).
 *
 * But to avoid annoying users by only giving triangle results, we can
 * triangulate, keeping a reference between the faces, then join after
 * if the edges don't collapse, this will also allow more choices when
 * collapsing edges so even has some advantage over decimating quads
 * directly.
 *
 * \return TRUE if any faces were triangulated.
 */

static int bm_decim_triangulate_begin(BMesh *bm)
{
#ifdef USE_SAFETY_CHECKS
	const int check_double_edges = TRUE;
#else
	const int check_double_edges = FALSE;
#endif

	BMIter iter;
	BMFace *f;
	// int has_quad;  // could optimize this a little
	int has_cut = FALSE;

	BLI_assert((bm->elem_index_dirty & BM_VERT) == 0);

	/* first clear loop index values */
	BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
		BMLoop *l_iter;
		BMLoop *l_first;

		l_iter = l_first = BM_FACE_FIRST_LOOP(f);
		do {
			BM_elem_index_set(l_iter, -1);
		} while ((l_iter = l_iter->next) != l_first);

		// has_quad |= (f->len == 4)
	}

	/* adding new faces as we loop over faces
	 * is normally best avoided, however in this case its not so bad because any face touched twice
	 * will already be triangulated*/
	BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
		if (f->len == 4) {
			BMLoop *f_l[4];
			BMLoop *l_iter;
			BMLoop *l_a, *l_b;

			l_iter = BM_FACE_FIRST_LOOP(f);

			f_l[0] = l_iter; l_iter = l_iter->next;
			f_l[1] = l_iter; l_iter = l_iter->next;
			f_l[2] = l_iter; l_iter = l_iter->next;
			f_l[3] = l_iter; l_iter = l_iter->next;

			if (len_squared_v3v3(f_l[0]->v->co, f_l[2]->v->co) < len_squared_v3v3(f_l[1]->v->co, f_l[3]->v->co)) {
				l_a = f_l[0];
				l_b = f_l[2];
			}
			else {
				l_a = f_l[1];
				l_b = f_l[3];
			}

			{
				BMFace *f_new;
				BMLoop *l_new;

				/* warning, NO_DOUBLE option here isn't handled as nice as it could be
				 * - if there is a quad that has a free standing edge joining it along
				 * where we want to split the face, there isnt a good way we can handle this.
				 * currently that edge will get removed when joining the tris back into a quad. */
				f_new = BM_face_split(bm, f, l_a->v, l_b->v, &l_new, NULL, check_double_edges);

				if (f_new) {
					/* the value of this doesn't matter, only that the 2 loops match and have unique values */
					const int f_index = BM_elem_index_get(f);

					/* since we just split theres only ever 2 loops */
					BLI_assert(BM_edge_is_manifold(l_new->e));

					BM_elem_index_set(l_new, f_index);
					BM_elem_index_set(l_new->radial_next, f_index);

					has_cut = TRUE;
				}
			}
		}
	}

	BLI_assert((bm->elem_index_dirty & BM_VERT) == 0);

	if (has_cut) {
		/* now triangulation is done we need to correct index values */
		BM_mesh_elem_index_ensure(bm, BM_EDGE | BM_FACE);
	}

	return has_cut;
}

static void bm_decim_triangulate_end(BMesh *bm)
{
	/* decimation finished, now re-join */
	BMIter iter;
	BMEdge *e;

	/* boundary edges */
	BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
		BMLoop *l_a, *l_b;
		if (BM_edge_loop_pair(e, &l_a, &l_b)) {
			const int l_a_index = BM_elem_index_get(l_a);
			if (l_a_index != -1) {
				const int l_b_index = BM_elem_index_get(l_b);
				if (l_a_index == l_b_index) {
					/* highly unlikely to fail, but prevents possible double-ups */
					if (l_a->f->len == 3 && l_b->f->len == 3) {
						BMFace *f[2] = {l_a->f, l_b->f};
						BM_faces_join(bm, f, 2, TRUE);
					}
				}
			}
		}
	}
}

#endif  /* USE_TRIANGULATE */

/* Edge Collapse Functions
 * *********************** */

/**
 * special, highly limited edge collapse function
 * intended for speed over flexibiliy.
 * can only collapse edges connected to (1, 2) tris.
 *
 * Important - dont add vert/edge/face data on collapsing!
 *
 * \param ke_other let caller know what edges we remove besides \a ke
 */
static int bm_edge_collapse(BMesh *bm, BMEdge *ke, BMVert *kv, int ke_other[2],
#ifdef USE_CUSTOMDATA
                            const float customdata_fac
#else
                            const float UNUSED(customdata_fac)
#endif
                            )
{
	BMVert *v_other = BM_edge_other_vert(ke, kv);

	BLI_assert(v_other != NULL);

	if (BM_edge_is_manifold(ke)) {
		BMLoop *l_a, *l_b;
		BMEdge *e_a_other[2], *e_b_other[2];
		int ok;

		ok = BM_edge_loop_pair(ke, &l_a, &l_b);

		BLI_assert(ok == TRUE);
		BLI_assert(l_a->f->len == 3);
		BLI_assert(l_b->f->len == 3);

		/* keep 'kv' 0th */
		if (BM_vert_in_edge(l_a->prev->e, kv)) {
			e_a_other[0] = l_a->prev->e;
			e_a_other[1] = l_a->next->e;
		}
		else {
			e_a_other[1] = l_a->prev->e;
			e_a_other[0] = l_a->next->e;
		}

		if (BM_vert_in_edge(l_b->prev->e, kv)) {
			e_b_other[0] = l_b->prev->e;
			e_b_other[1] = l_b->next->e;
		}
		else {
			e_b_other[1] = l_b->prev->e;
			e_b_other[0] = l_b->next->e;
		}

		BLI_assert(BM_edge_share_vert(e_a_other[0], e_b_other[0]));
		BLI_assert(BM_edge_share_vert(e_a_other[1], e_b_other[1]));

		/* we could assert this case, but better just bail out */
#if 0
		BLI_assert(e_a_other[0] != e_b_other[0]);
		BLI_assert(e_a_other[0] != e_b_other[1]);
		BLI_assert(e_b_other[0] != e_a_other[0]);
		BLI_assert(e_b_other[0] != e_a_other[1]);
#endif
		/* not totally common but we want to avoid */
		if (ELEM(e_a_other[0], e_b_other[0], e_b_other[1]) ||
		    ELEM(e_a_other[1], e_b_other[0], e_b_other[1]))
		{
			return FALSE;
		}

		ke_other[0] = BM_elem_index_get(e_a_other[0]);
		ke_other[1] = BM_elem_index_get(e_b_other[0]);

#ifdef USE_CUSTOMDATA
		/* TODO, loops */
		// const float w[2] = {customdata_fac, 1.0f - customdata_fac};

		/* before killing, do customdata */
		BM_data_interp_from_verts(bm, v_other, kv, v_other, customdata_fac);
#endif

		BM_edge_kill(bm, ke);

		BM_vert_splice(bm, kv, v_other);

		BM_edge_splice(bm, e_a_other[0], e_a_other[1]);
		BM_edge_splice(bm, e_b_other[0], e_b_other[1]);

		// BM_mesh_validate(bm);

		return TRUE;
	}
	else if (BM_edge_is_boundary(ke)) {
		/* same as above but only one triangle */
		BMLoop *l_a;
		BMEdge *e_a_other[2];

		l_a = ke->l;

		BLI_assert(l_a->f->len == 3);

		/* keep 'kv' 0th */
		if (BM_vert_in_edge(l_a->prev->e, kv)) {
			e_a_other[0] = l_a->prev->e;
			e_a_other[1] = l_a->next->e;
		}
		else {
			e_a_other[1] = l_a->prev->e;
			e_a_other[0] = l_a->next->e;
		}

		ke_other[0] = BM_elem_index_get(e_a_other[0]);
		ke_other[1] = -1;

#ifdef USE_CUSTOMDATA
		/* TODO, loops */
		// const float w[2] = {customdata_fac, 1.0f - customdata_fac};

		/* before killing, do customdata */
		BM_data_interp_from_verts(bm, v_other, kv, v_other, customdata_fac);
#endif

		BM_edge_kill(bm, ke);

		BM_vert_splice(bm, kv, v_other);

		BM_edge_splice(bm, e_a_other[0], e_a_other[1]);

		// BM_mesh_validate(bm);

		return TRUE;
	}
	else {
		return FALSE;
	}
}


/* collapse e the edge, removing e->v2 */
static void bm_decim_edge_collapse(BMesh *bm, BMEdge *e,
                                   Quadric *vquadrics,
                                   Heap *eheap, HeapNode **eheap_table)
{
	int ke_other[2];
	BMVert *v = e->v1;
	int kv_index = BM_elem_index_get(e->v2);  /* the vert is removed so only store the index */
	float optimize_co[3];
	float customdata_fac;

	bm_decim_calc_target_co(e, optimize_co, vquadrics);

	/* use for customdata merging */
	customdata_fac = line_point_factor_v3(optimize_co, e->v1->co, e->v2->co);

	if (bm_edge_collapse(bm, e, e->v2, ke_other, customdata_fac)) {
		/* update collapse info */
		int i;

		e = NULL;  /* paranoid safety check */

		copy_v3_v3(v->co, optimize_co);

		/* remove eheap */
		for (i = 0; i < 2; i++) {
			/* highly unlikely 'eheap_table[ke_other[i]]' would be NULL, but do for sanity sake */
			if ((ke_other[i] != -1) && (eheap_table[ke_other[i]] != NULL)) {
				BLI_heap_remove(eheap, eheap_table[ke_other[i]]);
				eheap_table[ke_other[i]] = NULL;
			}
		}

		/* update vertex quadric, add kept vertex from killed vertex */
		BLI_quadric_add_qu_qu(&vquadrics[BM_elem_index_get(v)], &vquadrics[kv_index]);

		/* update connected normals */
		BM_vert_normal_update_all(v);

		/* update error costs and the eheap */
		if (LIKELY(v->e)) {
			BMEdge *e_iter;
			BMEdge *e_first;
			e_iter = e_first = v->e;
			do {
				bm_decim_build_edge_cost_single(e_iter, vquadrics, eheap, eheap_table);
			} while ((e_iter = bmesh_disk_edge_next(e_iter, v)) != e_first);
		}
	}
}


/* Main Decimate Function
 * ********************** */

void BM_mesh_decimate(BMesh *bm, const float factor)
{
	Heap *eheap;             /* edge heap */
	HeapNode **eheap_table;  /* edge index aligned table pointing to the eheap */
	Quadric *vquadrics;      /* vert index aligned quadrics */
	int tot_edge_orig;
	int face_tot_target;
	int use_triangulate;


#ifdef USE_TRIANGULATE
	/* temp convert quads to triangles */
	use_triangulate = bm_decim_triangulate_begin(bm);
#endif


	/* alloc vars */
	vquadrics = MEM_callocN(sizeof(Quadric) * bm->totvert, __func__);
	eheap = BLI_heap_new_ex(bm->totedge);
	eheap_table = MEM_callocN(sizeof(HeapNode *) * bm->totedge, __func__);
	tot_edge_orig = bm->totedge;


	/* build initial edge collapse cost data */
	bm_decim_build_quadrics(bm, vquadrics);

	bm_decim_build_edge_cost(bm, vquadrics, eheap, eheap_table);

	face_tot_target = bm->totface * factor;
	bm->elem_index_dirty |= BM_FACE | BM_EDGE | BM_VERT;


	/* iterative edge collapse and maintain the eheap */
	while ((bm->totface > face_tot_target) && (BLI_heap_empty(eheap) == FALSE)) {
		BMEdge *e = BLI_heap_popmin(eheap);
		BLI_assert(BM_elem_index_get(e) < tot_edge_orig);  /* handy to detect corruptions elsewhere */

		/* under normal conditions wont be accessed again,
		 * but NULL just incase so we don't use freed node */
		eheap_table[BM_elem_index_get(e)] = NULL;

		bm_decim_edge_collapse(bm, e, vquadrics, eheap, eheap_table);
	}


#ifdef USE_TRIANGULATE
	/* its possible we only had triangles, skip this step in that case */
	if (LIKELY(use_triangulate)) {
		/* temp convert quads to triangles */
		bm_decim_triangulate_end(bm);
	}
#endif


	/* free vars */
	MEM_freeN(vquadrics);
	MEM_freeN(eheap_table);
	BLI_heap_free(eheap, NULL);
}
