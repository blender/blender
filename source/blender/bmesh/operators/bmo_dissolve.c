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
 *
 * Removes isolated geometry regions without creating holes in the mesh.
 */

#include "MEM_guardedalloc.h"

#include "BLI_array.h"
#include "BLI_math.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "intern/bmesh_operators_private.h"


/* ***_ISGC: mark for garbage-collection */

#define FACE_MARK   1
#define FACE_ORIG   2
#define FACE_NEW    4

#define EDGE_MARK   1
#define EDGE_TAG    2
#define EDGE_ISGC   8

#define VERT_MARK   1
#define VERT_TAG    2
#define VERT_ISGC   8



static bool UNUSED_FUNCTION(check_hole_in_region) (BMesh *bm, BMFace *f)
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

	for (f2 = BMW_begin(&regwalker, f); f2; f2 = BMW_step(&regwalker)) {
		BM_ITER_ELEM (l2, &liter2, f2, BM_LOOPS_OF_FACE) {
			l3 = l2->radial_next;
			if (BMO_elem_flag_test(bm, l3->f, FACE_MARK) !=
			    BMO_elem_flag_test(bm, l2->f, FACE_MARK))
			{
				if (!BMO_elem_flag_test(bm, l2->e, EDGE_MARK)) {
					return false;
				}
			}
		}
	}
	BMW_end(&regwalker);

	return true;
}

static void bm_face_split(BMesh *bm, const short oflag)
{
	BMIter iter;
	BMVert *v;

	BMIter liter;
	BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
		if (BMO_elem_flag_test(bm, v, oflag)) {
			if (BM_vert_edge_count(v) > 2) {
				BMLoop *l;
				BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
					if (l->f->len > 3) {
						if (BMO_elem_flag_test(bm, l->next->v, oflag) == 0 &&
						    BMO_elem_flag_test(bm, l->prev->v, oflag) == 0)
						{
							BM_face_split(bm, l->f, l->next, l->prev, NULL, NULL, true);
						}
					}
				}
			}
		}
	}
}

void bmo_dissolve_faces_exec(BMesh *bm, BMOperator *op)
{
	BMOIter oiter;
	BMFace *f;
	BMFace ***regions = NULL;
	BMFace **faces = NULL;
	BLI_array_declare(regions);
	BLI_array_declare(faces);
	BMFace *act_face = bm->act_face;
	BMWalker regwalker;
	int i;

	const bool use_verts = BMO_slot_bool_get(op->slots_in, "use_verts");

	if (use_verts) {
		/* tag verts that start out with only 2 edges,
		 * don't remove these later */
		BMIter viter;
		BMVert *v;

		BM_ITER_MESH (v, &viter, bm, BM_VERTS_OF_MESH) {
			BMO_elem_flag_set(bm, v, VERT_MARK, (BM_vert_edge_count(v) != 2));
		}
	}

	BMO_slot_buffer_flag_enable(bm, op->slots_in, "faces", BM_FACE, FACE_MARK);
	
	/* collect region */
	BMO_ITER (f, &oiter, op->slots_in, "faces", BM_FACE) {
		BMFace *f_iter;
		if (!BMO_elem_flag_test(bm, f, FACE_MARK)) {
			continue;
		}

		BLI_array_empty(faces);
		faces = NULL; /* forces different allocatio */

		BMW_init(&regwalker, bm, BMW_ISLAND,
		         BMW_MASK_NOP, BMW_MASK_NOP, FACE_MARK,
		         BMW_FLAG_NOP, /* no need to check BMW_FLAG_TEST_HIDDEN, faces are already marked by the bmo */
		         BMW_NIL_LAY);

		for (f_iter = BMW_begin(&regwalker, f); f_iter; f_iter = BMW_step(&regwalker)) {
			BLI_array_append(faces, f_iter);
		}
		BMW_end(&regwalker);
		
		for (i = 0; i < BLI_array_count(faces); i++) {
			f_iter = faces[i];
			BMO_elem_flag_disable(bm, f_iter, FACE_MARK);
			BMO_elem_flag_enable(bm, f_iter, FACE_ORIG);
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
		BMFace *f_new;
		int tot = 0;
		
		faces = regions[i];
		if (!faces[0]) {
			BMO_error_raise(bm, op, BMERR_DISSOLVEFACES_FAILED,
			                "Could not find boundary of dissolve region");
			goto cleanup;
		}
		
		while (faces[tot])
			tot++;
		
		f_new = BM_faces_join(bm, faces, tot, true);

		if (f_new) {
			/* maintain active face */
			if (act_face && bm->act_face == NULL) {
				bm->act_face = f_new;
			}
		}
		else {
			BMO_error_raise(bm, op, BMERR_DISSOLVEFACES_FAILED,
			                "Could not create merged face");
			goto cleanup;
		}

		/* if making the new face failed (e.g. overlapping test)
		 * unmark the original faces for deletion */
		BMO_elem_flag_disable(bm, f_new, FACE_ORIG);
		BMO_elem_flag_enable(bm, f_new, FACE_NEW);

	}

	BMO_op_callf(bm, op->flag, "delete geom=%ff context=%i", FACE_ORIG, DEL_FACES);


	if (use_verts) {
		BMIter viter;
		BMVert *v;

		BM_ITER_MESH (v, &viter, bm, BM_VERTS_OF_MESH) {
			if (BMO_elem_flag_test(bm, v, VERT_MARK)) {
				if (BM_vert_edge_count(v) == 2) {
					BM_vert_collapse_edge(bm, v->e, v, true);
				}
			}
		}
	}

	if (BMO_error_occurred(bm)) {
		goto cleanup;
	}

	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "region.out", BM_FACE, FACE_NEW);

cleanup:
	/* free/cleanup */
	for (i = 0; i < BLI_array_count(regions); i++) {
		if (regions[i]) MEM_freeN(regions[i]);
	}

	BLI_array_free(regions);
}

void bmo_dissolve_edges_exec(BMesh *bm, BMOperator *op)
{
	/* BMOperator fop; */
	BMFace *act_face = bm->act_face;
	BMOIter eiter;
	BMIter iter;
	BMEdge *e, *e_next;
	BMVert *v, *v_next;

	const bool use_verts = BMO_slot_bool_get(op->slots_in, "use_verts");
	const bool use_face_split = BMO_slot_bool_get(op->slots_in, "use_face_split");

	if (use_face_split) {
		BMO_slot_buffer_flag_enable(bm, op->slots_in, "edges", BM_EDGE, EDGE_TAG);

		BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
			BMIter itersub;
			int untag_count = 0;
			BM_ITER_ELEM (e, &itersub, v, BM_EDGES_OF_VERT) {
				if (!BMO_elem_flag_test(bm, e, EDGE_TAG)) {
					untag_count++;
				}
			}

			/* check that we have 2 edges remaining after dissolve */
			if (untag_count <= 2) {
				BMO_elem_flag_enable(bm, v, VERT_TAG);
			}
		}

		bm_face_split(bm, VERT_TAG);
	}

	if (use_verts) {
		BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
			BMO_elem_flag_set(bm, v, VERT_MARK, (BM_vert_edge_count(v) != 2));
		}
	}

	/* tag all verts/edges connected to faces */
	BMO_ITER (e, &eiter, op->slots_in, "edges", BM_EDGE) {
		BMFace *f_pair[2];
		if (BM_edge_face_pair(e, &f_pair[0], &f_pair[1])) {
			unsigned int j;
			for (j = 0; j < 2; j++) {
				BMLoop *l_first, *l_iter;
				l_iter = l_first = BM_FACE_FIRST_LOOP(f_pair[j]);
				do {
					BMO_elem_flag_enable(bm, l_iter->v, VERT_ISGC);
					BMO_elem_flag_enable(bm, l_iter->e, EDGE_ISGC);
				} while ((l_iter = l_iter->next) != l_first);
			}
		}
	}

	BMO_ITER (e, &eiter, op->slots_in, "edges", BM_EDGE) {
		BMFace *fa, *fb;
		if (BM_edge_face_pair(e, &fa, &fb)) {
			BMFace *f_new;

			/* join faces */
			f_new = BM_faces_join_pair(bm, fa, fb, e, false);

			if (f_new) {
				/* maintain active face */
				if (act_face && bm->act_face == NULL) {
					bm->act_face = f_new;
				}
			}
		}
	}

	/* Cleanup geometry (#BM_faces_join_pair, but it removes geometry we're looping on)
	 * so do this in a separate pass instead. */
	BM_ITER_MESH_MUTABLE (e, e_next, &iter, bm, BM_EDGES_OF_MESH) {
		if ((e->l == NULL) && BMO_elem_flag_test(bm, e, EDGE_ISGC)) {
			BM_edge_kill(bm, e);
		}
	}
	BM_ITER_MESH_MUTABLE (v, v_next, &iter, bm, BM_VERTS_OF_MESH) {
		if ((v->e == NULL) && BMO_elem_flag_test(bm, v, VERT_ISGC)) {
			BM_vert_kill(bm, v);
		}
	}
	/* done with cleanup */


	if (use_verts) {
		BM_ITER_MESH_MUTABLE (v, v_next, &iter, bm, BM_VERTS_OF_MESH) {
			if (BMO_elem_flag_test(bm, v, VERT_MARK)) {
				if (BM_vert_edge_count(v) == 2) {
					BM_vert_collapse_edge(bm, v->e, v, true);
				}
			}
		}
	}
}

static bool test_extra_verts(BMesh *bm, BMVert *v)
{
	BMIter fiter, liter, eiter, fiter_sub;
	BMFace *f;
	BMLoop *l;
	BMEdge *e;

	/* test faces around verts for verts that would be wrongly killed
	 * by dissolve faces. */
	BM_ITER_ELEM (f, &fiter, v, BM_FACES_OF_VERT) {
		BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
			if (!BMO_elem_flag_test(bm, l->v, VERT_MARK)) {
				/* if an edge around a vert is a boundary edge,
				 * then dissolve faces won't destroy it.
				 * also if it forms a boundary with one
				 * of the face region */
				bool found = false;
				BM_ITER_ELEM (e, &eiter, l->v, BM_EDGES_OF_VERT) {
					BMFace *f_iter;
					if (BM_edge_is_boundary(e)) {
						found = true;
					}
					else {
						BM_ITER_ELEM (f_iter, &fiter_sub, e, BM_FACES_OF_EDGE) {
							if (!BMO_elem_flag_test(bm, f_iter, FACE_MARK)) {
								found = true;
								break;
							}
						}
					}
					if (found == true) {
						break;
					}
				}
				if (found == false) {
					return false;
				}
			}
		}
	}

	return true;
}
void bmo_dissolve_verts_exec(BMesh *bm, BMOperator *op)
{
	BMIter iter, fiter;
	BMVert *v, *v_next;
	BMFace *f;

	const bool use_face_split = BMO_slot_bool_get(op->slots_in, "use_face_split");


	BMO_slot_buffer_flag_enable(bm, op->slots_in, "verts", BM_VERT, VERT_MARK);
	
	if (use_face_split) {
		bm_face_split(bm, VERT_MARK);
	}

	BM_ITER_MESH_MUTABLE (v, v_next, &iter, bm, BM_VERTS_OF_MESH) {
		if (BMO_elem_flag_test(bm, v, VERT_MARK)) {
			/* check if it's a two-valence ver */
			if (BM_vert_edge_count(v) == 2) {

				/* collapse the ver */
				/* previously the faces were joined, but collapsing between 2 edges
				 * gives some advantage/difference in using vertex-dissolve over edge-dissolve */
#if 0
				BM_vert_collapse_faces(bm, v->e, v, 1.0f, true, true);
#else
				BM_vert_collapse_edge(bm, v->e, v, true);
#endif

				continue;
			}

			BM_ITER_ELEM (f, &fiter, v, BM_FACES_OF_VERT) {
				BMO_elem_flag_enable(bm, f, FACE_MARK | FACE_ORIG);
			}
			
			/* check if our additions to the input to face dissolve
			 * will destroy nonmarked vertices. */
			if (!test_extra_verts(bm, v)) {
				BM_ITER_ELEM (f, &fiter, v, BM_FACES_OF_VERT) {
					if (BMO_elem_flag_test(bm, f, FACE_ORIG)) {
						BMO_elem_flag_disable(bm, f, FACE_MARK | FACE_ORIG);
					}
				}
			}
			else {
				BM_ITER_ELEM (f, &fiter, v, BM_FACES_OF_VERT) {
					BMO_elem_flag_disable(bm, f, FACE_ORIG);
				}
			}
		}
	}

	BMO_op_callf(bm, op->flag, "dissolve_faces faces=%ff", FACE_MARK);
	if (BMO_error_occurred(bm)) {
		const char *msg;

		BMO_error_get(bm, &msg, NULL);
		BMO_error_clear(bm);
		BMO_error_raise(bm, op, BMERR_DISSOLVEVERTS_FAILED, msg);
	}
	
	/* clean up any remaining */
	/* note: don't use BM_ITER_MESH_MUTABLE here, even though vertices are removed (T37559) */
	BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
		if (BMO_elem_flag_test(bm, v, VERT_MARK)) {
			if (!BM_vert_dissolve(bm, v)) {
				BMO_error_raise(bm, op, BMERR_DISSOLVEVERTS_FAILED, NULL);
				return;
			}
#ifdef DEBUG
			/* workaround debug assert */
			iter.count = bm->totvert;
#endif
		}
	}

}

/* Limited Dissolve */
void bmo_dissolve_limit_exec(BMesh *bm, BMOperator *op)
{
	BMOpSlot *einput = BMO_slot_get(op->slots_in, "edges");
	BMOpSlot *vinput = BMO_slot_get(op->slots_in, "verts");
	const float angle_max = (float)M_PI / 2.0f;
	const float angle_limit = min_ff(angle_max, BMO_slot_float_get(op->slots_in, "angle_limit"));
	const bool do_dissolve_boundaries = BMO_slot_bool_get(op->slots_in, "use_dissolve_boundaries");
	const BMO_Delimit delimit = BMO_slot_int_get(op->slots_in, "delimit");

	BM_mesh_decimate_dissolve_ex(bm, angle_limit, do_dissolve_boundaries, delimit,
	                             (BMVert **)BMO_SLOT_AS_BUFFER(vinput), vinput->len,
	                             (BMEdge **)BMO_SLOT_AS_BUFFER(einput), einput->len,
	                             FACE_NEW);

	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "region.out", BM_FACE, FACE_NEW);
}


#define EDGE_MARK 1
#define EDGE_COLLAPSE 2

static void bm_mesh_edge_collapse_flagged(BMesh *bm, const int flag, const short oflag)
{
	BMO_op_callf(bm, flag, "collapse edges=%fe", oflag);
}

void bmo_dissolve_degenerate_exec(BMesh *bm, BMOperator *op)
{
	const float dist = BMO_slot_float_get(op->slots_in, "dist");
	const float dist_sq = dist * dist;

	bool found;
	BMIter eiter;
	BMEdge *e;


	BMO_slot_buffer_flag_enable(bm, op->slots_in, "edges", BM_EDGE, EDGE_MARK);

	/* collapse zero length edges, this accounts for zero area faces too */
	found = false;
	BM_ITER_MESH (e, &eiter, bm, BM_EDGES_OF_MESH) {
		if (BMO_elem_flag_test(bm, e, EDGE_MARK)) {
			if (BM_edge_calc_length_squared(e) < dist_sq) {
				BMO_elem_flag_enable(bm, e, EDGE_COLLAPSE);
				found = true;
			}
		}

		/* clear all loop tags (checked later) */
		if (e->l) {
			BMLoop *l_iter, *l_first;
			l_iter = l_first = e->l;
			do {
				BM_elem_flag_disable(l_iter, BM_ELEM_TAG);
			} while ((l_iter = l_iter->radial_next) != l_first);
		}
	}

	if (found) {
		bm_mesh_edge_collapse_flagged(bm, op->flag, EDGE_COLLAPSE);
	}


	/* clip degenerate ears from the face */
	found = false;
	BM_ITER_MESH (e, &eiter, bm, BM_EDGES_OF_MESH) {
		if (e->l && BMO_elem_flag_test(bm, e, EDGE_MARK)) {
			BMLoop *l_iter, *l_first;
			l_iter = l_first = e->l;
			do {
				if (
				    /* check the loop hasn't already been tested (and flag not to test again) */
				    !BM_elem_flag_test(l_iter, BM_ELEM_TAG) &&
				    (BM_elem_flag_enable(l_iter, BM_ELEM_TAG),

				     /* check we're marked to tested (radial edge already tested) */
				     BMO_elem_flag_test(bm, l_iter->prev->e, EDGE_MARK) &&

				     /* check edges are not already going to be collapsed */
				     !BMO_elem_flag_test(bm, l_iter->e, EDGE_COLLAPSE) &&
				     !BMO_elem_flag_test(bm, l_iter->prev->e, EDGE_COLLAPSE)))
				{
					/* test if the faces loop (ear) is degenerate */
					float dir_prev[3], len_prev;
					float dir_next[3], len_next;


					sub_v3_v3v3(dir_prev, l_iter->prev->v->co, l_iter->v->co);
					sub_v3_v3v3(dir_next, l_iter->next->v->co, l_iter->v->co);

					len_prev = normalize_v3(dir_prev);
					len_next = normalize_v3(dir_next);

					if ((len_v3v3(dir_prev, dir_next) * min_ff(len_prev, len_next)) <= dist) {
						bool reset = false;

						if (fabsf(len_prev - len_next) <= dist) {
							/* both edges the same length */
							if (l_iter->f->len == 3) {
								/* ideally this would have been discovered with short edge test above */
								BMO_elem_flag_enable(bm, l_iter->next->e, EDGE_COLLAPSE);
								found = true;
							}
							else {
								/* add a joining edge and tag for removal */
								BMLoop *l_split;
								if (BM_face_split(bm, l_iter->f, l_iter->prev, l_iter->next, &l_split, NULL, true)) {
									BMO_elem_flag_enable(bm, l_split->e, EDGE_COLLAPSE);
									found = true;
									reset = true;
								}
							}
						}
						else if (len_prev < len_next) {
							/* split 'l_iter->e', then join the vert with next */
							BMVert *v_new;
							BMEdge *e_new;
							BMLoop *l_split;
							v_new = BM_edge_split(bm, l_iter->e, l_iter->v, &e_new, len_prev / len_next);
							BLI_assert(v_new == l_iter->next->v);
							(void)v_new;
							if (BM_face_split(bm, l_iter->f, l_iter->prev, l_iter->next, &l_split, NULL, true)) {
								BMO_elem_flag_enable(bm, l_split->e, EDGE_COLLAPSE);
								found = true;
							}
							reset = true;
						}
						else if (len_next < len_prev) {
							/* split 'l_iter->prev->e', then join the vert with next */
							BMVert *v_new;
							BMEdge *e_new;
							BMLoop *l_split;
							v_new = BM_edge_split(bm, l_iter->prev->e, l_iter->v, &e_new, len_next / len_prev);
							BLI_assert(v_new == l_iter->prev->v);
							(void)v_new;
							if (BM_face_split(bm, l_iter->f, l_iter->prev, l_iter->next, &l_split, NULL, true)) {
								BMO_elem_flag_enable(bm, l_split->e, EDGE_COLLAPSE);
								found = true;
							}
							reset = true;
						}

						if (reset) {
							/* we can't easily track where we are on the radial edge, reset! */
							l_first = l_iter;
						}
					}
				}
			} while ((l_iter = l_iter->radial_next) != l_first);
		}
	}

	if (found) {
		bm_mesh_edge_collapse_flagged(bm, op->flag, EDGE_COLLAPSE);
	}

}
