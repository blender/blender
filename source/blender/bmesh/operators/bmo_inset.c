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

typedef struct SplitEdgeInfo {
	float   no[3];
	float   length;
	BMEdge *e_old;
	BMEdge *e_new;
	BMLoop *l;
} SplitEdgeInfo;

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
 * return the tag loop where there is...
 * - only 1 tagged face attached to this edge.
 * - 1 or more untagged faces.
 *
 * \note this function looks to be expensive
 * but in most cases it will only do 2 iterations.
 */
static BMLoop *bm_edge_is_mixed_face_tag(BMLoop *l)
{
	if (LIKELY(l != NULL)) {
		int tot_tag = 0;
		int tot_untag = 0;
		BMLoop *l_iter;
		BMLoop *l_tag = NULL;
		l_iter = l;
		do {
			if (BM_elem_flag_test(l_iter->f, BM_ELEM_TAG)) {
				/* more then one tagged face - bail out early! */
				if (tot_tag == 1) {
					return NULL;
				}
				l_tag = l_iter;
				tot_tag++;
			}
			else {
				tot_untag++;
			}

		} while ((l_iter = l_iter->radial_next) != l);

		return ((tot_tag == 1) && (tot_untag >= 1)) ? l_tag : NULL;
	}
	else {
		return NULL;
	}
}


/**
 * implementation is as follows...
 *
 * - set all faces as tagged/untagged based on selection.
 * - find all edges that have 1 tagged, 1 untagged face.
 * - separate these edges and tag vertices, set their index to point to the original edge.
 * - build faces between old/new edges.
 * - inset the new edges into their faces.
 */

void bmo_inset_exec(BMesh *bm, BMOperator *op)
{
	const int use_outset          = BMO_slot_bool_get(op, "use_outset");
	const int use_boundary        = BMO_slot_bool_get(op, "use_boundary") && (use_outset == FALSE);
	const int use_even_offset     = BMO_slot_bool_get(op, "use_even_offset");
	const int use_even_boundry    = use_even_offset; /* could make own option */
	const int use_relative_offset = BMO_slot_bool_get(op, "use_relative_offset");
	const float thickness = BMO_slot_float_get(op, "thickness");

	int edge_info_len = 0;

	BMIter iter;
	SplitEdgeInfo *edge_info;
	SplitEdgeInfo *es;

	BMVert *v;
	BMEdge *e;
	BMFace *f;
	int i, j, k;

	if (use_outset == FALSE) {
		BM_mesh_elem_flag_disable_all(bm, BM_FACE, BM_ELEM_TAG);
		BMO_slot_buffer_hflag_enable(bm, op, "faces", BM_FACE, BM_ELEM_TAG, FALSE);
	}
	else {
		BM_mesh_elem_flag_enable_all(bm, BM_FACE, BM_ELEM_TAG);
		BMO_slot_buffer_hflag_disable(bm, op, "faces", BM_FACE, BM_ELEM_TAG, FALSE);
	}

	/* first count all inset edges we will split */
	/* fill in array and initialize tagging */
	BM_ITER(e, &iter, bm, BM_EDGES_OF_MESH, NULL) {
		if (
		    /* tag if boundary is enabled */
		    (use_boundary && BM_edge_is_boundary(e) && BM_elem_flag_test(e->l->f, BM_ELEM_TAG)) ||

		    /* tag if edge is an interior edge inbetween a tagged and untagged face */
		    (bm_edge_is_mixed_face_tag(e->l)))
		{
			/* tag */
			BM_elem_flag_enable(e->v1, BM_ELEM_TAG);
			BM_elem_flag_enable(e->v2, BM_ELEM_TAG);
			BM_elem_flag_enable(e, BM_ELEM_TAG);

			BM_elem_index_set(e, edge_info_len); /* set_dirty! */
			edge_info_len++;
		}
		else {
			BM_elem_flag_disable(e->v1, BM_ELEM_TAG);
			BM_elem_flag_disable(e->v2, BM_ELEM_TAG);
			BM_elem_flag_disable(e, BM_ELEM_TAG);

			BM_elem_index_set(e, -1); /* set_dirty! */
		}
	}
	bm->elem_index_dirty |= BM_EDGE;

	edge_info = MEM_mallocN(edge_info_len * sizeof(SplitEdgeInfo), __func__);

	/* fill in array and initialize tagging */
	es = edge_info;
	BM_ITER(e, &iter, bm, BM_EDGES_OF_MESH, NULL) {
		i = BM_elem_index_get(e);
		if (i != -1) {
			/* calc edge-split info */
			es->length = BM_edge_length_calc(e);
			es->e_old = e;
			es++;
			/* initialize no and e_new after */
		}
	}

	for (i = 0, es = edge_info; i < edge_info_len; i++, es++) {
		if ((es->l = bm_edge_is_mixed_face_tag(es->e_old->l))) {
			/* do nothing */
		}
		else {
			es->l = es->e_old->l; /* must be a boundary */
		}


		/* run the separate arg */
		bmesh_edge_separate(bm, es->e_old, es->l);

		/* calc edge-split info */
		es->e_new = es->l->e;
		edge_loop_tangent(es->e_new, es->l, es->no);

		if (es->e_new == es->e_old) { /* happens on boundary edges */
			es->e_old = BM_edge_create(bm, es->e_new->v1, es->e_new->v2, es->e_new, FALSE);
		}

		/* store index back to original in 'edge_info' */
		BM_elem_index_set(es->e_new, i);
		BM_elem_flag_enable(es->e_new, BM_ELEM_TAG);

		/* important to tag again here */
		BM_elem_flag_enable(es->e_new->v1, BM_ELEM_TAG);
		BM_elem_flag_enable(es->e_new->v2, BM_ELEM_TAG);
	}


	/* show edge normals for debugging */
#if 0
	for (i = 0, es = edge_info; i < edge_info_len; i++, es++) {
		float tvec[3];
		BMVert *v1, *v2;

		mid_v3_v3v3(tvec, es->e_new->v1->co, es->e_new->v2->co);

		v1 = BM_vert_create(bm, tvec, NULL);
		v2 = BM_vert_create(bm, tvec, NULL);
		madd_v3_v3fl(v2->co, es->no, 0.1f);
		BM_edge_create(bm, v1, v2, NULL, FALSE);
	}
#endif

	/* execute the split and position verts, it would be most obvious to loop over verts
	 * here but don't do this since we will be splitting them off (iterating stuff you modify is bad juju)
	 * instead loop over edges then their verts */
	for (i = 0, es = edge_info; i < edge_info_len; i++, es++) {
		for (j = 0; j < 2; j++) {
			v = (j == 0) ? es->e_new->v1 : es->e_new->v2;

			/* end confusing part - just pretend this is a typical loop on verts */

			/* only split of tagged verts - used by separated edges */

			/* comment the first part because we know this verts in a tagged face */
			if (/* v->e && */BM_elem_flag_test(v, BM_ELEM_TAG)) {
				BMVert **vout;
				int r_vout_len;
				BMVert *v_glue = NULL;

				/* disable touching twice, this _will_ happen if the flags not disabled */
				BM_elem_flag_disable(v, BM_ELEM_TAG);

				bmesh_vert_separate(bm, v, &vout, &r_vout_len);
				v = NULL; /* don't use again */

				/* in some cases the edge doesnt split off */
				if (r_vout_len == 1) {
					MEM_freeN(vout);
					continue;
				}

				for (k = 0; k < r_vout_len; k++) {
					BMVert *v_split = vout[k]; /* only to avoid vout[k] all over */

					/* need to check if this vertex is from a */
					int vert_edge_tag_tot = 0;
					int vecpair[2];

					/* find adjacent */
					BM_ITER(e, &iter, bm, BM_EDGES_OF_VERT, v_split) {
						if (BM_elem_flag_test(e, BM_ELEM_TAG) &&
						    BM_elem_flag_test(e->l->f, BM_ELEM_TAG))
						{
							if (vert_edge_tag_tot < 2) {
								vecpair[vert_edge_tag_tot] = BM_elem_index_get(e);
								BLI_assert(vecpair[vert_edge_tag_tot] != -1);
							}

							vert_edge_tag_tot++;
						}
					}

					if (vert_edge_tag_tot != 0) {
						float tvec[3];

						if (vert_edge_tag_tot >= 2) { /* 2 edge users - common case */
							const float *e_no_a = edge_info[vecpair[0]].no;
							const float *e_no_b = edge_info[vecpair[1]].no;

							add_v3_v3v3(tvec, e_no_a, e_no_b);
							normalize_v3(tvec);

							/* scale by edge angle */
							if (use_even_offset) {
								mul_v3_fl(tvec, shell_angle_to_dist(angle_normalized_v3v3(e_no_a, e_no_b) / 2.0f));
							}

							/* scale relative to edge lengths */
							if (use_relative_offset) {
								mul_v3_fl(tvec, (edge_info[vecpair[0]].length + edge_info[vecpair[1]].length) / 2.0f);
							}
						}
						else if (vert_edge_tag_tot == 1) { /* 1 edge user - boundary vert, not so common */
							const float *e_no_a = edge_info[vecpair[0]].no;

							if (use_even_boundry) {

								/* This case where only one edge attached to v_split
								 * is used - ei - the face to inset is on a boundary.
								 *
								 *                  We want the inset to align flush with the
								 *                  boundary edge, not the normal of the interior
								 *             <--- edge which would give an unsightly bump.
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
								 * doesn’t matter because the direction will remain the same in this case.
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
								mul_v3_fl(tvec, edge_info[vecpair[0]].length);
							}
						}
						else {
							/* should never happen */
							BLI_assert(0);
							zero_v3(tvec);
						}

						/* apply the offset */
						madd_v3_v3fl(v_split->co, tvec, thickness);
					}

					/* this saves expensive/slow glue check for common cases */
					if (r_vout_len > 2) {
						int ok = TRUE;
						/* last step, NULL this vertex if has a tagged face */
						BM_ITER(f, &iter, bm, BM_FACES_OF_VERT, v_split) {
							if (BM_elem_flag_test(f, BM_ELEM_TAG)) {
								ok = FALSE;
								break;
							}
						}

						if (ok) {
							if (v_glue == NULL) {
								v_glue = v_split;
							}
							else {
								BM_vert_splice(bm, v_split, v_glue);
							}
						}
					}
					/* end glue */

				}
				MEM_freeN(vout);
			}
		}
	}

	/* create faces */
	for (i = 0, es = edge_info; i < edge_info_len; i++, es++) {
		BMVert *varr[4] = {NULL};
		/* get the verts in the correct order */
		BM_edge_ordered_verts_ex(es->e_new, &varr[1], &varr[0], es->l);
#if 0
		if (varr[0] == es->e_new->v1) {
			varr[2] = es->e_old->v2;
			varr[3] = es->e_old->v1;
		}
		else {
			varr[2] = es->e_old->v1;
			varr[3] = es->e_old->v2;
		}
		j = 4;
#else
		/* slightly trickier check - since we can't assume the verts are split */
		j = 2; /* 2 edges are set */
		if (varr[0] == es->e_new->v1) {
			if (es->e_old->v2 != es->e_new->v2) { varr[j++] = es->e_old->v2; }
			if (es->e_old->v1 != es->e_new->v1) { varr[j++] = es->e_old->v1; }
		}
		else {
			if (es->e_old->v1 != es->e_new->v1) { varr[j++] = es->e_old->v1; }
			if (es->e_old->v2 != es->e_new->v2) { varr[j++] = es->e_old->v2; }
		}

		if (j == 2) {
			/* can't make face! */
			continue;
		}
#endif
		/* no need to check doubles, we KNOW there won't be any */
		/* yes - reverse face is correct in this case */
		f = BM_face_create_quad_tri_v(bm, varr, j, es->l->f, FALSE);
		BMO_elem_flag_enable(bm, f, ELE_NEW);
	}

	MEM_freeN(edge_info);

	/* we could flag new edges/verts too, is it useful? */
	BMO_slot_buffer_from_flag(bm, op, "faceout", BM_FACE, ELE_NEW);
}
