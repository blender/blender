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

/** \file blender/bmesh/tools/bmesh_decimate_collapse.c
 *  \ingroup bmesh
 *
 * BMesh decimator that uses an edge collapse method.
 */

#include <stddef.h>

#include "MEM_guardedalloc.h"


#include "BLI_math.h"
#include "BLI_quadric.h"
#include "BLI_heap.h"

#include "BKE_customdata.h"

#include "bmesh.h"
#include "bmesh_decimate.h"  /* own include */

#include "../intern/bmesh_structure.h"

/* defines for testing */
#define USE_CUSTOMDATA
#define USE_TRIANGULATE
#define USE_VERT_NORMAL_INTERP  /* has the advantage that flipped faces don't mess up vertex normals */

/* these checks are for rare cases that we can't avoid since they are valid meshes still */
#define USE_SAFETY_CHECKS

#define BOUNDARY_PRESERVE_WEIGHT 100.0f
#define OPTIMIZE_EPS 0.01f  /* FLT_EPSILON is too small, see [#33106] */
#define COST_INVALID FLT_MAX

typedef enum CD_UseFlag {
	CD_DO_VERT = (1 << 0),
	CD_DO_EDGE = (1 << 1),
	CD_DO_LOOP = (1 << 2)
} CD_UseFlag;


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

			if (normalize_v3(edge_cross) > FLT_EPSILON) {
				Quadric q;
				BLI_quadric_from_v3_dist(&q, edge_cross, -dot_v3v3(edge_cross, e->v1->co));
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
	/* compute an edge contraction target for edge 'e'
	 * this is computed by summing it's vertices quadrics and
	 * optimizing the result. */
	Quadric q;

	BLI_quadric_add_qu_ququ(&q,
	                        &vquadrics[BM_elem_index_get(e->v1)],
	                        &vquadrics[BM_elem_index_get(e->v2)]);


	if (BLI_quadric_optimize(&q, optimize_co, OPTIMIZE_EPS)) {
		return;  /* all is good */
	}
	else {
		mid_v3_v3v3(optimize_co, e->v1->co, e->v2->co);
	}
}

static bool bm_edge_collapse_is_degenerate_flip(BMEdge *e, const float optimize_co[3])
{
	BMIter liter;
	BMLoop *l;
	unsigned int i;

	for (i = 0; i < 2; i++) {
		/* loop over both verts */
		BMVert *v = *((&e->v1) + i);

		BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
			if (l->e != e && l->prev->e != e) {
				const float *co_prev = l->prev->v->co;
				const float *co_next = l->next->v->co;
				float cross_exist[3];
				float cross_optim[3];

#if 1
				float vec_other[3];  /* line between the two outer verts, re-use for both cross products */
				float vec_exist[3];  /* before collapse */
				float vec_optim[3];  /* after collapse */

				sub_v3_v3v3(vec_other, co_prev, co_next);
				sub_v3_v3v3(vec_exist, co_prev, v->co);
				sub_v3_v3v3(vec_optim, co_prev, optimize_co);

				cross_v3_v3v3(cross_exist, vec_other, vec_exist);
				cross_v3_v3v3(cross_optim, vec_other, vec_optim);

				/* normalize isn't really needed, but ensures the value at a unit we can compare against */
				normalize_v3(cross_exist);
				normalize_v3(cross_optim);
#else
				normal_tri_v3(cross_exist, v->co,       co_prev, co_next);
				normal_tri_v3(cross_optim, optimize_co, co_prev, co_next);
#endif

				/* use a small value rather then zero so we don't flip a face in multiple steps
				 * (first making it zero area, then flipping again) */
				if (dot_v3v3(cross_exist, cross_optim) <= FLT_EPSILON) {
					//printf("no flip\n");
					return true;
				}
			}
		}
	}

	return false;
}

static void bm_decim_build_edge_cost_single(BMEdge *e,
                                            const Quadric *vquadrics, const float *vweights,
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

	if (vweights) {
		if ((vweights[BM_elem_index_get(e->v1)] >= BM_MESH_DECIM_WEIGHT_MAX) &&
		    (vweights[BM_elem_index_get(e->v2)] >= BM_MESH_DECIM_WEIGHT_MAX))
		{
			/* skip collapsing this edge */
			eheap_table[BM_elem_index_get(e)] = NULL;
			return;
		}
	}
	/* end sanity check */


	bm_decim_calc_target_co(e, optimize_co, vquadrics);

	q1 = &vquadrics[BM_elem_index_get(e->v1)];
	q2 = &vquadrics[BM_elem_index_get(e->v2)];

	if (vweights == NULL) {
		cost = (BLI_quadric_evaluate(q1, optimize_co) +
		        BLI_quadric_evaluate(q2, optimize_co));
	}
	else {
		/* add 1.0 so planar edges are still weighted against */
		cost = (((BLI_quadric_evaluate(q1, optimize_co) + 1.0f) * vweights[BM_elem_index_get(e->v1)]) +
		        ((BLI_quadric_evaluate(q2, optimize_co) + 1.0f) * vweights[BM_elem_index_get(e->v2)]));
	}
	// print("COST %.12f\n");

	/* note, 'cost' shouldn't be negative but happens sometimes with small values.
	 * this can cause faces that make up a flat surface to over-collapse, see [#37121] */
	cost = fabsf(cost);
	eheap_table[BM_elem_index_get(e)] = BLI_heap_insert(eheap, cost, e);
}


/* use this for degenerate cases - add back to the heap with an invalid cost,
 * this way it may be calculated again if surrounding geometry changes */
static void bm_decim_invalid_edge_cost_single(BMEdge *e,
                                              Heap *eheap, HeapNode **eheap_table)
{
	BLI_assert(eheap_table[BM_elem_index_get(e)] == NULL);
	eheap_table[BM_elem_index_get(e)] = BLI_heap_insert(eheap, COST_INVALID, e);
}

static void bm_decim_build_edge_cost(BMesh *bm,
                                     const Quadric *vquadrics, const float *vweights,
                                     Heap *eheap, HeapNode **eheap_table)
{
	BMIter iter;
	BMEdge *e;
	unsigned int i;

	BM_ITER_MESH_INDEX (e, &iter, bm, BM_EDGES_OF_MESH, i) {
		eheap_table[i] = NULL;  /* keep sanity check happy */
		bm_decim_build_edge_cost_single(e, vquadrics, vweights, eheap, eheap_table);
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
 * \return true if any faces were triangulated.
 */

static bool bm_decim_triangulate_begin(BMesh *bm)
{
	BMIter iter;
	BMFace *f;
	// bool has_quad;  // could optimize this a little
	bool has_cut = false;

	BLI_assert((bm->elem_index_dirty & BM_VERT) == 0);

	/* first clear loop index values */
	BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
		BMLoop *l_iter;
		BMLoop *l_first;

		l_iter = l_first = BM_FACE_FIRST_LOOP(f);
		do {
			BM_elem_index_set(l_iter, -1);  /* set_dirty */
		} while ((l_iter = l_iter->next) != l_first);

		// has_quad |= (f->len == 4)
	}

	bm->elem_index_dirty |= BM_LOOP;

	/* adding new faces as we loop over faces
	 * is normally best avoided, however in this case its not so bad because any face touched twice
	 * will already be triangulated*/
	BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
		if (f->len == 4) {
			BMLoop *f_l[4];
			BMLoop *l_a, *l_b;

			{
				BMLoop *l_iter = BM_FACE_FIRST_LOOP(f);

				f_l[0] = l_iter; l_iter = l_iter->next;
				f_l[1] = l_iter; l_iter = l_iter->next;
				f_l[2] = l_iter; l_iter = l_iter->next;
				f_l[3] = l_iter;
			}

			if (len_squared_v3v3(f_l[0]->v->co, f_l[2]->v->co) <
			    len_squared_v3v3(f_l[1]->v->co, f_l[3]->v->co))
			{
				l_a = f_l[0];
				l_b = f_l[2];
			}
			else {
				l_a = f_l[1];
				l_b = f_l[3];
			}

#ifdef USE_SAFETY_CHECKS
			if (BM_edge_exists(l_a->v, l_b->v) == NULL)
#endif
			{
				BMFace *f_new;
				BMLoop *l_new;

				/* warning, NO_DOUBLE option here isn't handled as nice as it could be
				 * - if there is a quad that has a free standing edge joining it along
				 * where we want to split the face, there isnt a good way we can handle this.
				 * currently that edge will get removed when joining the tris back into a quad. */
				f_new = BM_face_split(bm, f, l_a, l_b, &l_new, NULL, false);

				if (f_new) {
					/* the value of this doesn't matter, only that the 2 loops match and have unique values */
					const int f_index = BM_elem_index_get(f);

					/* since we just split theres only ever 2 loops */
					BLI_assert(BM_edge_is_manifold(l_new->e));

					BM_elem_index_set(l_new, f_index);  /* set_dirty */
					BM_elem_index_set(l_new->radial_next, f_index);  /* set_dirty */

					BM_face_normal_update(f);
					BM_face_normal_update(f_new);

					has_cut = true;
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
	BMEdge *e, *e_next;

	/* boundary edges */
	BM_ITER_MESH_MUTABLE (e, e_next, &iter, bm, BM_EDGES_OF_MESH) {
		BMLoop *l_a, *l_b;
		if (BM_edge_loop_pair(e, &l_a, &l_b)) {
			const int l_a_index = BM_elem_index_get(l_a);
			if (l_a_index != -1) {
				const int l_b_index = BM_elem_index_get(l_b);
				if (l_a_index == l_b_index) {
					if (LIKELY(l_a->f->len == 3 && l_b->f->len == 3)) {
						if (l_a->v != l_b->v) {  /* if this is the case, faces have become flipped */
							/* check we are not making a degenerate quad */
							BMVert *vquad[4] = {
								e->v1,
								BM_vert_in_edge(e, l_a->next->v) ? l_a->prev->v : l_a->next->v,
								e->v2,
								BM_vert_in_edge(e, l_b->next->v) ? l_b->prev->v : l_b->next->v,
							};

							BLI_assert(ELEM(vquad[0], vquad[1], vquad[2], vquad[3]) == false);
							BLI_assert(ELEM(vquad[1], vquad[0], vquad[2], vquad[3]) == false);
							BLI_assert(ELEM(vquad[2], vquad[1], vquad[0], vquad[3]) == false);
							BLI_assert(ELEM(vquad[3], vquad[1], vquad[2], vquad[0]) == false);

							if (is_quad_convex_v3(vquad[0]->co, vquad[1]->co, vquad[2]->co, vquad[3]->co)) {
								/* highly unlikely to fail, but prevents possible double-ups */
								BMFace *f[2] = {l_a->f, l_b->f};
								BM_faces_join(bm, f, 2, true);
							}
						}
					}
				}
			}
		}
	}
}

#endif  /* USE_TRIANGULATE */

/* Edge Collapse Functions
 * *********************** */

#ifdef USE_CUSTOMDATA

/**
 * \param v is the target to merge into.
 */
static void bm_edge_collapse_loop_customdata(BMesh *bm, BMLoop *l, BMVert *v_clear, BMVert *v_other,
                                             const float customdata_fac)
{
	/* disable seam check - the seam check would have to be done per layer, its not really that important */
//#define USE_SEAM
	/* these don't need to be updated, since they will get removed when the edge collapses */
	BMLoop *l_clear, *l_other;
	const bool is_manifold = BM_edge_is_manifold(l->e);
	int side;

	/* l defines the vert to collapse into  */

	/* first find the loop of 'v_other' thats attached to the face of 'l' */
	if (l->v == v_clear) {
		l_clear = l;
		l_other = l->next;
	}
	else {
		l_clear = l->next;
		l_other = l;
	}

	BLI_assert(l_clear->v == v_clear);
	BLI_assert(l_other->v == v_other);
	(void)v_other;  /* quiet warnings for release */

	/* now we have both corners of the face 'l->f' */
	for (side = 0; side < 2; side++) {
#ifdef USE_SEAM
		bool is_seam = false;
#endif
		void *src[2];
		BMFace *f_exit = is_manifold ? l->radial_next->f : NULL;
		BMEdge *e_prev = l->e;
		BMLoop *l_first;
		BMLoop *l_iter;
		float w[2];

		if (side == 0) {
			l_iter = l_first = l_clear;
			src[0] = l_clear->head.data;
			src[1] = l_other->head.data;

			w[0] = customdata_fac;
			w[1] = 1.0f - customdata_fac;
		}
		else {
			l_iter = l_first = l_other;
			src[0] = l_other->head.data;
			src[1] = l_clear->head.data;

			w[0] = 1.0f - customdata_fac;
			w[1] = customdata_fac;
		}

		// print_v2("weights", w);

		/* WATCH IT! - should NOT reference (_clear or _other) vars for this while loop */

		/* walk around the fan using 'e_prev' */
		while (((l_iter = BM_vert_step_fan_loop(l_iter, &e_prev)) != l_first) && (l_iter != NULL)) {
			int i;
			/* quit once we hit the opposite face, if we have one */
			if (f_exit && UNLIKELY(f_exit == l_iter->f)) {
				break;
			}

#ifdef USE_SEAM
			/* break out unless we find a match */
			is_seam = true;
#endif

			/* ok. we have a loop. now be smart with it! */
			for (i = 0; i < bm->ldata.totlayer; i++) {
				if (CustomData_layer_has_math(&bm->ldata, i)) {
					const int offset = bm->ldata.layers[i].offset;
					const int type = bm->ldata.layers[i].type;
					void *cd_src[2] = {(char *)src[0] + offset,
					                   (char *)src[1] + offset};
					void *cd_iter = (char *)l_iter->head.data + offset;

					/* detect seams */
					if (CustomData_data_equals(type, cd_src[0], cd_iter)) {
						CustomData_bmesh_interp_n(&bm->ldata, cd_src, w, NULL, 2, l_iter->head.data, i);
#ifdef USE_SEAM
						is_seam = false;
#endif
					}
				}
			}

#ifdef USE_SEAM
			if (is_seam) {
				break;
			}
#endif
		}
	}

//#undef USE_SEAM

}
#endif  /* USE_CUSTOMDATA */

/**
 * Check if the collapse will result in a degenerate mesh,
 * that is - duplicate edges or faces.
 *
 * This situation could be checked for when calculating collapse cost
 * however its quite slow and a degenerate collapse could eventuate
 * after the cost is calculated, so instead, check just before collapsing.
 */

static void bm_edge_tag_enable(BMEdge *e)
{
	BM_elem_flag_enable(e->v1, BM_ELEM_TAG);
	BM_elem_flag_enable(e->v2, BM_ELEM_TAG);
	if (e->l) {
		BM_elem_flag_enable(e->l->f, BM_ELEM_TAG);
		if (e->l != e->l->radial_next) {
			BM_elem_flag_enable(e->l->radial_next->f, BM_ELEM_TAG);
		}
	}
}

static void bm_edge_tag_disable(BMEdge *e)
{
	BM_elem_flag_disable(e->v1, BM_ELEM_TAG);
	BM_elem_flag_disable(e->v2, BM_ELEM_TAG);
	if (e->l) {
		BM_elem_flag_disable(e->l->f, BM_ELEM_TAG);
		if (e->l != e->l->radial_next) {
			BM_elem_flag_disable(e->l->radial_next->f, BM_ELEM_TAG);
		}
	}
}

static bool bm_edge_tag_test(BMEdge *e)
{
	/* is the edge or one of its faces tagged? */
	return (BM_elem_flag_test(e->v1, BM_ELEM_TAG) ||
	        BM_elem_flag_test(e->v2, BM_ELEM_TAG) ||
	        (e->l && (BM_elem_flag_test(e->l->f, BM_ELEM_TAG) ||
	                  (e->l != e->l->radial_next &&
	                  BM_elem_flag_test(e->l->radial_next->f, BM_ELEM_TAG))))
	        );
}

/* takes the edges loop */
BLI_INLINE int bm_edge_is_manifold_or_boundary(BMLoop *l)
{
#if 0
	/* less optimized version of check below */
	return (BM_edge_is_manifold(l->e) || BM_edge_is_boundary(l->e);
#else
	/* if the edge is a boundary it points to its self, else this must be a manifold */
	return LIKELY(l) && LIKELY(l->radial_next->radial_next == l);
#endif
}

static bool bm_edge_collapse_is_degenerate_topology(BMEdge *e_first)
{
	/* simply check that there is no overlap between faces and edges of each vert,
	 * (excluding the 2 faces attached to 'e' and 'e' its self) */

	BMEdge *e_iter;

	/* clear flags on both disks */
	e_iter = e_first;
	do {
		if (!bm_edge_is_manifold_or_boundary(e_iter->l)) {
			return true;
		}
		bm_edge_tag_disable(e_iter);
	} while ((e_iter = bmesh_disk_edge_next(e_iter, e_first->v1)) != e_first);

	e_iter = e_first;
	do {
		if (!bm_edge_is_manifold_or_boundary(e_iter->l)) {
			return true;
		}
		bm_edge_tag_disable(e_iter);
	} while ((e_iter = bmesh_disk_edge_next(e_iter, e_first->v2)) != e_first);

	/* now enable one side... */
	e_iter = e_first;
	do {
		bm_edge_tag_enable(e_iter);
	} while ((e_iter = bmesh_disk_edge_next(e_iter, e_first->v1)) != e_first);

	/* ... except for the edge we will collapse, we know thats shared,
	 * disable this to avoid false positive. We could be smart and never enable these
	 * face/edge tags in the first place but easier to do this */
	// bm_edge_tag_disable(e_first);
	/* do inline... */
	{
#if 0
		BMIter iter;
		BMIter liter;
		BMLoop *l;
		BMVert *v;
		BM_ITER_ELEM (l, &liter, e_first, BM_LOOPS_OF_EDGE) {
			BM_elem_flag_disable(l->f, BM_ELEM_TAG);
			BM_ITER_ELEM (v, &iter, l->f, BM_VERTS_OF_FACE) {
				BM_elem_flag_disable(v, BM_ELEM_TAG);
			}
		}
#else
		/* we know each face is a triangle, no looping/iterators needed here */

		BMLoop *l_radial;
		BMLoop *l_face;

		l_radial = e_first->l;
		l_face = l_radial;
		BLI_assert(l_face->f->len == 3);
		BM_elem_flag_disable(l_face->f, BM_ELEM_TAG);
		BM_elem_flag_disable((l_face = l_radial)->v,     BM_ELEM_TAG);
		BM_elem_flag_disable((l_face = l_face->next)->v, BM_ELEM_TAG);
		BM_elem_flag_disable((         l_face->next)->v, BM_ELEM_TAG);
		l_face = l_radial->radial_next;
		if (l_radial != l_face) {
			BLI_assert(l_face->f->len == 3);
			BM_elem_flag_disable(l_face->f, BM_ELEM_TAG);
			BM_elem_flag_disable((l_face = l_radial->radial_next)->v, BM_ELEM_TAG);
			BM_elem_flag_disable((l_face = l_face->next)->v,          BM_ELEM_TAG);
			BM_elem_flag_disable((         l_face->next)->v,          BM_ELEM_TAG);
		}
#endif
	}

	/* and check for overlap */
	e_iter = e_first;
	do {
		if (bm_edge_tag_test(e_iter)) {
			return true;
		}
	} while ((e_iter = bmesh_disk_edge_next(e_iter, e_first->v2)) != e_first);

	return false;
}

/**
 * special, highly limited edge collapse function
 * intended for speed over flexibility.
 * can only collapse edges connected to (1, 2) tris.
 *
 * Important - dont add vert/edge/face data on collapsing!
 *
 * \param e_clear_other let caller know what edges we remove besides \a e_clear
 * \param customdata_flag merge factor, scales from 0 - 1 ('v_clear' -> 'v_other')
 */
static bool bm_edge_collapse(BMesh *bm, BMEdge *e_clear, BMVert *v_clear, int r_e_clear_other[2],
#ifdef USE_CUSTOMDATA
                             const CD_UseFlag customdata_flag,
                             const float customdata_fac
#else
                             const CD_UseFlag UNUSED(customdata_flag),
                             const float UNUSED(customdata_fac)
#endif
                            )
{
	BMVert *v_other;

	v_other = BM_edge_other_vert(e_clear, v_clear);
	BLI_assert(v_other != NULL);

	if (BM_edge_is_manifold(e_clear)) {
		BMLoop *l_a, *l_b;
		BMEdge *e_a_other[2], *e_b_other[2];
		bool ok;

		ok = BM_edge_loop_pair(e_clear, &l_a, &l_b);

		BLI_assert(ok == true);
		BLI_assert(l_a->f->len == 3);
		BLI_assert(l_b->f->len == 3);

		/* keep 'v_clear' 0th */
		if (BM_vert_in_edge(l_a->prev->e, v_clear)) {
			e_a_other[0] = l_a->prev->e;
			e_a_other[1] = l_a->next->e;
		}
		else {
			e_a_other[1] = l_a->prev->e;
			e_a_other[0] = l_a->next->e;
		}

		if (BM_vert_in_edge(l_b->prev->e, v_clear)) {
			e_b_other[0] = l_b->prev->e;
			e_b_other[1] = l_b->next->e;
		}
		else {
			e_b_other[1] = l_b->prev->e;
			e_b_other[0] = l_b->next->e;
		}

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
			return false;
		}

		BLI_assert(BM_edge_share_vert(e_a_other[0], e_b_other[0]));
		BLI_assert(BM_edge_share_vert(e_a_other[1], e_b_other[1]));

		r_e_clear_other[0] = BM_elem_index_get(e_a_other[0]);
		r_e_clear_other[1] = BM_elem_index_get(e_b_other[0]);

#ifdef USE_CUSTOMDATA
		/* before killing, do customdata */
		if (customdata_flag & CD_DO_VERT) {
			BM_data_interp_from_verts(bm, v_other, v_clear, v_other, customdata_fac);
		}
		if (customdata_flag & CD_DO_EDGE) {
			BM_data_interp_from_edges(bm, e_a_other[1], e_a_other[0], e_a_other[1], customdata_fac);
			BM_data_interp_from_edges(bm, e_b_other[1], e_b_other[0], e_b_other[1], customdata_fac);
		}
		if (customdata_flag & CD_DO_LOOP) {
			bm_edge_collapse_loop_customdata(bm, e_clear->l,              v_clear, v_other, customdata_fac);
			bm_edge_collapse_loop_customdata(bm, e_clear->l->radial_next, v_clear, v_other, customdata_fac);
		}
#endif

		BM_edge_kill(bm, e_clear);

		v_other->head.hflag |= v_clear->head.hflag;
		BM_vert_splice(bm, v_clear, v_other);

		e_a_other[1]->head.hflag |= e_a_other[0]->head.hflag;
		e_b_other[1]->head.hflag |= e_b_other[0]->head.hflag;
		BM_edge_splice(bm, e_a_other[0], e_a_other[1]);
		BM_edge_splice(bm, e_b_other[0], e_b_other[1]);

		// BM_mesh_validate(bm);

		return true;
	}
	else if (BM_edge_is_boundary(e_clear)) {
		/* same as above but only one triangle */
		BMLoop *l_a;
		BMEdge *e_a_other[2];

		l_a = e_clear->l;

		BLI_assert(l_a->f->len == 3);

		/* keep 'v_clear' 0th */
		if (BM_vert_in_edge(l_a->prev->e, v_clear)) {
			e_a_other[0] = l_a->prev->e;
			e_a_other[1] = l_a->next->e;
		}
		else {
			e_a_other[1] = l_a->prev->e;
			e_a_other[0] = l_a->next->e;
		}

		r_e_clear_other[0] = BM_elem_index_get(e_a_other[0]);
		r_e_clear_other[1] = -1;

#ifdef USE_CUSTOMDATA
		/* before killing, do customdata */
		if (customdata_flag & CD_DO_VERT) {
			BM_data_interp_from_verts(bm, v_other, v_clear, v_other, customdata_fac);
		}
		if (customdata_flag & CD_DO_EDGE) {
			BM_data_interp_from_edges(bm, e_a_other[1], e_a_other[0], e_a_other[1], customdata_fac);
		}
		if (customdata_flag & CD_DO_LOOP) {
			bm_edge_collapse_loop_customdata(bm, e_clear->l, v_clear, v_other, customdata_fac);
		}
#endif

		BM_edge_kill(bm, e_clear);

		v_other->head.hflag |= v_clear->head.hflag;
		BM_vert_splice(bm, v_clear, v_other);

		e_a_other[1]->head.hflag |= e_a_other[0]->head.hflag;
		BM_edge_splice(bm, e_a_other[0], e_a_other[1]);

		// BM_mesh_validate(bm);

		return true;
	}
	else {
		return false;
	}
}


/* collapse e the edge, removing e->v2 */
static void bm_decim_edge_collapse(BMesh *bm, BMEdge *e,
                                   Quadric *vquadrics, float *vweights,
                                   Heap *eheap, HeapNode **eheap_table,
                                   const CD_UseFlag customdata_flag)
{
	int e_clear_other[2];
	BMVert *v_other = e->v1;
	int v_clear_index = BM_elem_index_get(e->v2);  /* the vert is removed so only store the index */
	float optimize_co[3];
	float customdata_fac;

#ifdef USE_VERT_NORMAL_INTERP
	float v_clear_no[3];
	copy_v3_v3(v_clear_no, e->v2->no);
#endif

	/* disallow collapsing which results in degenerate cases */
	if (UNLIKELY(bm_edge_collapse_is_degenerate_topology(e))) {
		bm_decim_invalid_edge_cost_single(e, eheap, eheap_table);  /* add back with a high cost */
		return;
	}

	bm_decim_calc_target_co(e, optimize_co, vquadrics);

	/* check if this would result in an overlapping face */
	if (UNLIKELY(bm_edge_collapse_is_degenerate_flip(e, optimize_co))) {
		bm_decim_invalid_edge_cost_single(e, eheap, eheap_table);  /* add back with a high cost */
		return;
	}

	/* use for customdata merging */
	if (LIKELY(compare_v3v3(e->v1->co, e->v2->co, FLT_EPSILON) == false)) {
		customdata_fac = line_point_factor_v3(optimize_co, e->v1->co, e->v2->co);
#if 0
		/* simple test for stupid collapse */
		if (customdata_fac < 0.0 - FLT_EPSILON || customdata_fac > 1.0f + FLT_EPSILON) {
			return;
		}
#endif
	}
	else {
		/* avoid divide by zero */
		customdata_fac = 0.5f;
	}

	if (bm_edge_collapse(bm, e, e->v2, e_clear_other, customdata_flag, customdata_fac)) {
		/* update collapse info */
		int i;

		if (vweights) {
			vweights[BM_elem_index_get(v_other)] += vweights[v_clear_index];
		}

		e = NULL;  /* paranoid safety check */

		copy_v3_v3(v_other->co, optimize_co);

		/* remove eheap */
		for (i = 0; i < 2; i++) {
			/* highly unlikely 'eheap_table[ke_other[i]]' would be NULL, but do for sanity sake */
			if ((e_clear_other[i] != -1) && (eheap_table[e_clear_other[i]] != NULL)) {
				BLI_heap_remove(eheap, eheap_table[e_clear_other[i]]);
				eheap_table[e_clear_other[i]] = NULL;
			}
		}

		/* update vertex quadric, add kept vertex from killed vertex */
		BLI_quadric_add_qu_qu(&vquadrics[BM_elem_index_get(v_other)], &vquadrics[v_clear_index]);

		/* update connected normals */

		/* in fact face normals are not used for progressive updates, no need to update them */
		// BM_vert_normal_update_all(v);
#ifdef USE_VERT_NORMAL_INTERP
		interp_v3_v3v3(v_other->no, v_other->no, v_clear_no, customdata_fac);
		normalize_v3(v_other->no);
#else
		BM_vert_normal_update(v_other);
#endif


		/* update error costs and the eheap */
		if (LIKELY(v_other->e)) {
			BMEdge *e_iter;
			BMEdge *e_first;
			e_iter = e_first = v_other->e;
			do {
				BLI_assert(BM_edge_find_double(e_iter) == NULL);
				bm_decim_build_edge_cost_single(e_iter, vquadrics, vweights, eheap, eheap_table);
			} while ((e_iter = bmesh_disk_edge_next(e_iter, v_other)) != e_first);
		}

		/* this block used to be disabled,
		 * but enable now since surrounding faces may have been
		 * set to COST_INVALID because of a face overlap that no longer occurs */
#if 1
		/* optional, update edges around the vertex face fan */
		{
			BMIter liter;
			BMLoop *l;
			BM_ITER_ELEM (l, &liter, v_other, BM_LOOPS_OF_VERT) {
				if (l->f->len == 3) {
					BMEdge *e_outer;
					if (BM_vert_in_edge(l->prev->e, l->v))
						e_outer = l->next->e;
					else
						e_outer = l->prev->e;

					BLI_assert(BM_vert_in_edge(e_outer, l->v) == false);

					bm_decim_build_edge_cost_single(e_outer, vquadrics, vweights, eheap, eheap_table);
				}
			}
		}
		/* end optional update */
#endif
	}
	else {
		/* add back with a high cost */
		bm_decim_invalid_edge_cost_single(e, eheap, eheap_table);
	}
}


/* Main Decimate Function
 * ********************** */

/**
 * \brief BM_mesh_decimate
 * \param bm The mesh
 * \param factor face count multiplier [0 - 1]
 * \param vweights Optional array of vertex  aligned weights [0 - 1],
 *        a vertex group is the usual source for this.
 */
void BM_mesh_decimate_collapse(BMesh *bm, const float factor, float *vweights, const bool do_triangulate)
{
	Heap *eheap;             /* edge heap */
	HeapNode **eheap_table;  /* edge index aligned table pointing to the eheap */
	Quadric *vquadrics;      /* vert index aligned quadrics */
	int tot_edge_orig;
	int face_tot_target;
	bool use_triangulate;

	CD_UseFlag customdata_flag = 0;

#ifdef USE_TRIANGULATE
	/* temp convert quads to triangles */
	use_triangulate = bm_decim_triangulate_begin(bm);
#endif


	/* alloc vars */
	vquadrics = MEM_callocN(sizeof(Quadric) * bm->totvert, __func__);
	/* since some edges may be degenerate, we might be over allocing a little here */
	eheap = BLI_heap_new_ex(bm->totedge);
	eheap_table = MEM_mallocN(sizeof(HeapNode *) * bm->totedge, __func__);
	tot_edge_orig = bm->totedge;


	/* build initial edge collapse cost data */
	bm_decim_build_quadrics(bm, vquadrics);

	bm_decim_build_edge_cost(bm, vquadrics, vweights, eheap, eheap_table);

	face_tot_target = bm->totface * factor;
	bm->elem_index_dirty |= BM_ALL;


#ifdef USE_CUSTOMDATA
	/* initialize customdata flag, we only need math for loops */
	if (CustomData_has_interp(&bm->vdata))  customdata_flag |= CD_DO_VERT;
	if (CustomData_has_interp(&bm->edata))  customdata_flag |= CD_DO_EDGE;
	if (CustomData_has_math(&bm->ldata))    customdata_flag |= CD_DO_LOOP;
#endif

	/* iterative edge collapse and maintain the eheap */
	while ((bm->totface > face_tot_target) &&
	       (BLI_heap_is_empty(eheap) == false) &&
	       (BLI_heap_node_value(BLI_heap_top(eheap)) != COST_INVALID))
	{
		// const float value = BLI_heap_node_value(BLI_heap_top(eheap));
		BMEdge *e = BLI_heap_popmin(eheap);
		BLI_assert(BM_elem_index_get(e) < tot_edge_orig);  /* handy to detect corruptions elsewhere */

		// printf("COST %.10f\n", value);

		/* under normal conditions wont be accessed again,
		 * but NULL just incase so we don't use freed node */
		eheap_table[BM_elem_index_get(e)] = NULL;

		bm_decim_edge_collapse(bm, e, vquadrics, vweights, eheap, eheap_table, customdata_flag);
	}


#ifdef USE_TRIANGULATE
	if (do_triangulate == false) {
		/* its possible we only had triangles, skip this step in that case */
		if (LIKELY(use_triangulate)) {
			/* temp convert quads to triangles */
			bm_decim_triangulate_end(bm);
		}
	}
#endif

	/* free vars */
	MEM_freeN(vquadrics);
	MEM_freeN(eheap_table);
	BLI_heap_free(eheap, NULL);

	/* testing only */
	// BM_mesh_validate(bm);

	(void)tot_edge_orig;  /* quiet release build warning */
}
