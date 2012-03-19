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

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */

#define ELE_NEW		1

static void edge_loop_tangent(BMEdge *e, BMLoop *e_loop, float r_no[3])
{
	float tvec[3];
	BMVert *v1, *v2;
	BM_edge_ordered_verts_ex(e, &v1, &v2, e_loop);

	sub_v3_v3v3(tvec, v1->co, v2->co); /* use for temp storage */
	cross_v3_v3v3(r_no, tvec, e_loop->f->no);
	normalize_v3(r_no);
}

/**
 * functionality is as follows
 *
 * - set all faces as tagged/untagged based on selection.
 * - find all edges that have 1 tagged, 1 untagged face.
 * - separate these edges and tag vertices, set their index to point to the original edge.
 * - build faces between old/new edges.
 * - inset the new edges into their faces.
 *
 * TODO
 * - close tares when 2 corners touch.
 * - 'use_relative_offset' comparing edge length _while_ moving verts about is incorrect.
 */

void bmo_inset_exec(BMesh *bm, BMOperator *op)
{
	const int use_even_offset     = BMO_slot_bool_get(op, "use_even_offset");
	const int use_even_boundry    = use_even_offset; /* could make own option */
	const int use_relative_offset = BMO_slot_bool_get(op, "use_relative_offset");
	const float thickness = BMO_slot_float_get(op, "thickness");

	const int bm_totedge_orig = bm->totedge;

	BMIter iter;
	BMEdge **edge_arr;
	float (*edge_nor)[3];

	BMVert *v;
	BMEdge *e, *e_new;
	BMFace *f;
	int i, j, k;

	BM_mesh_elem_flag_disable_all(bm, BM_FACE, BM_ELEM_TAG);
	BMO_slot_buffer_hflag_enable(bm, op, "faces", BM_ELEM_TAG, BM_FACE, FALSE);


	/* fill in array and initialize tagging */
	edge_arr = MEM_mallocN(bm_totedge_orig * sizeof(BMEdge), __func__);
	BM_ITER_INDEX(e, &iter, bm, BM_EDGES_OF_MESH, NULL, i) {
		edge_arr[i] = e;
		BM_elem_index_set(e, i); /* set_inline */

		/* tag */
		BM_elem_flag_disable(e->v1, BM_ELEM_TAG);
		BM_elem_flag_disable(e->v2, BM_ELEM_TAG);
		BM_elem_flag_disable(e, BM_ELEM_TAG);
	}
	bm->elem_index_dirty &= ~BM_EDGE;


	/* XXX be smarter!, we could allocate an array only for the tagged edges instead,
	 * since this array will have mostly unused items */
	edge_nor = MEM_mallocN(bm_totedge_orig * sizeof(*edge_nor), __func__);

	/* split off all boundary edges */
	for (i = 0; i < bm_totedge_orig; i++) {
		BMLoop *la;
		BMLoop *lb;

		e = edge_arr[i];

		if (BM_edge_loop_pair(e, &la, &lb)) {
			const int tag_a = BM_elem_flag_test(la->f, BM_ELEM_TAG);
			const int tag_b = BM_elem_flag_test(lb->f, BM_ELEM_TAG);

			if (tag_a != tag_b) {
				BMLoop *l = tag_a ? la : lb;

				edge_loop_tangent(e, l, edge_nor[i]); /* could call after splitting too */
				bmesh_edge_separate(bm, e, l);
				e_new = l->e;

				/* store index back to original in 'edge_arr' */
				BM_elem_index_set(e_new, i);
				BM_elem_flag_enable(e_new, BM_ELEM_TAG);

				BM_elem_flag_enable(e_new->v1, BM_ELEM_TAG);
				BM_elem_flag_enable(e_new->v2, BM_ELEM_TAG);
			}
		}
	}

	/* show edge normals for debugging */
#if 0
	BM_ITER_INDEX(e_new, &iter, bm, BM_EDGES_OF_MESH, NULL, i) {
		if (BM_elem_flag_test(e_new, BM_ELEM_TAG)) {
			float tvec[3];
			BMVert *v1, *v2;

			e = edge_arr[BM_elem_index_get(e_new)];
			mid_v3_v3v3(tvec, e->v1->co, e->v2->co);

			v1 = BM_vert_create(bm, tvec, NULL);
			v2 = BM_vert_create(bm, tvec, NULL);
			madd_v3_v3fl(v2->co, edge_nor[BM_elem_index_get(e_new)], 0.1f);
			BM_edge_create(bm, v1, v2, NULL, FALSE);
		}
	}
#endif

	/* execute the split and position verts, it would be most obvious to loop over verts
	 * here but don't do this since we will be splitting them off (iterating stuff you modify is bad juju)
	 * instead loop over edges then their verts */
	for (j = 0; j < bm_totedge_orig; j++) {
		for (k = 0; k < 2; k++) {
			v = (k == 0) ? edge_arr[j]->v1 : edge_arr[j]->v2;
			/* end confusinug part - just pretend this is a typical loop on verts */



			/* only split of tagged verts - used by separated edges */

			/* comment the first part because we know this verts in a tagged face */
			if (/* v->e && */BM_elem_flag_test(v, BM_ELEM_TAG)) {
				BMVert **vout;
				int r_vout_len = 0;

				bmesh_vert_separate(bm, v, &vout, &r_vout_len);
				v = NULL; /* don't use again */

				for (i = 0; i < r_vout_len; i++) {
					BMVert *v_split = vout[i]; /* only to avoid vout[i] all over */

					/* need to check if this vertex is from a */
					BMIter itersub;
					int vert_edge_tag_tot = 0;
					int vecpair[2];

					/* find adjacent */
					BM_ITER(e, &itersub, bm, BM_EDGES_OF_VERT, v_split) {
						if (BM_edge_is_boundary(e) && /* this will be true because bmesh_edge_separate() has run */
						    BM_elem_flag_test(e, BM_ELEM_TAG) &&
						    BM_elem_flag_test(e->l->f, BM_ELEM_TAG))
						{
							/* disable touching twice, this _will_ happen if the flags not disabled */
							BM_elem_flag_disable(v_split, BM_ELEM_TAG);

							if (vert_edge_tag_tot < 2) {
								vecpair[vert_edge_tag_tot] = BM_elem_index_get(e);
							}

							// BM_elem_flag_disable(e, BM_ELEM_SMOOTH); // testing only
							vert_edge_tag_tot++;
						}
					}

					if (vert_edge_tag_tot != 0) {
						float tvec[3];

						if (vert_edge_tag_tot >= 2) { /* 2 edge users - common case */
							const float *e_no_a = edge_nor[vecpair[0]];
							const float *e_no_b = edge_nor[vecpair[1]];

							add_v3_v3v3(tvec, e_no_a, e_no_b);
							normalize_v3(tvec);

							/* scale by edge angle */
							if (use_even_offset) {
								mul_v3_fl(tvec, shell_angle_to_dist(angle_normalized_v3v3(e_no_a, e_no_b) / 2.0f));
							}

							/* scale relative to edge lengths */
							if (use_relative_offset) {
								mul_v3_fl(tvec,
								          ((BM_edge_length_calc(edge_arr[vecpair[0]]) +
								            BM_edge_length_calc(edge_arr[vecpair[1]])) / 2.0f));
							}
						}
						else if (vert_edge_tag_tot == 1) { /* 1 edge user - boundary vert, not so common */
							const float *e_no_a = edge_nor[vecpair[0]];

							if (use_even_boundry) {

								/* This case where only one edge attached to v_split
								 * is used - ei - the face to inset is on a boundary.
								 *
                                 *                  We want the inset to align flush with the
                                 *                  boundary edge, not the normal of the interior
                                 *             <--- edge which would give an unsligtly bump.
                                 * --+-------------------------+---------------+--
                                 *   |^v_other    ^e_other    /^v_split        |
                                 *   |                       /                 |
                                 *   |                      /                  |
                                 *   |                     / <- tag split edge |
                                 *   |                    /                    |
                                 *   |                   /                     |
                                 *   |                  /                      |
                                 * --+-----------------+-----------------------+--
                                 *   |                                         |
                                 *   |                                         |
                                 *
                                 * note, the fact we are doing location comparisons on verts that are moved about
                                 * doesnt matter becaise the direction will remain the same in this case.
                                 */

								BMEdge *e_other;
								BMVert *v_other;
								/* loop will always be either next of prev */
								BMLoop *l = v_split->e->l;
								if (l->prev->v == v_split) {
									l = l->prev;
								}
								else if (l->next->v == v_split) {
									l = l->next;
								}
								else if (l->v == v_split) {
									/* pass */
								}
								else {
									/* should never happen */
									BLI_assert(0);
								}

								/* find the edge which is _not_ being split here */
								if (!BM_elem_flag_test(l->e, BM_ELEM_TAG)) {
									e_other = l->e;
								}
								else if (!BM_elem_flag_test(l->prev->e, BM_ELEM_TAG)) {
									e_other = l->prev->e;
								}
								else {
									BLI_assert(0);
									e_other = NULL;
								}

								v_other = BM_edge_other_vert(e_other, v_split);
								sub_v3_v3v3(tvec, v_other->co, v_split->co);
								normalize_v3(tvec);

								if (use_even_offset) {
									mul_v3_fl(tvec, shell_angle_to_dist(angle_normalized_v3v3(e_no_a, tvec)));
								}
							}
							else {
								copy_v3_v3(tvec, e_no_a);
							}

							/* use_even_offset - doesn't apply here */

							/* scale relative to edge length */
							if (use_relative_offset) {
								mul_v3_fl(tvec, BM_edge_length_calc(edge_arr[vecpair[0]]));
							}
						}
						else {
							/* should never happen */
							BLI_assert(0);
							zero_v3(tvec);
						}

						madd_v3_v3fl(v_split->co, tvec, thickness);
					}
				}
				MEM_freeN(vout);
			}
		}
	}

	/* create faces */
	BM_ITER_INDEX(e, &iter, bm, BM_EDGES_OF_MESH, NULL, i) {
		if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
			BMVert *v1, *v2, *v3, *v4;
			e_new = edge_arr[BM_elem_index_get(e)];

			/* get the verts in the correct order */
			BM_edge_ordered_verts(e_new, &v1, &v2);
			if (v1 == e_new->v1) {
				v3 = e->v2;
				v4 = e->v1;
			}
			else {
				v3 = e->v1;
				v4 = e->v2;
			}

			/* no need to check doubles, we KNOW there wont be any */
			/* yes - reverse face is correct in fhis case */
			f = BM_face_create_quad_tri(bm, v4, v3, v2, v1, e_new->l->f, FALSE);
			BMO_elem_flag_enable(bm, f, ELE_NEW);
		}
	}

	MEM_freeN(edge_nor);
	MEM_freeN(edge_arr);

	/* we could flag new edges/verts too, is it useful? */
	BMO_slot_buffer_from_flag(bm, op, "faceout", ELE_NEW, BM_FACE);
}
