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
#include "intern/bmesh_operators_private.h"


#define FACE_MARK   1
#define FACE_ORIG   2
#define FACE_NEW    4
#define EDGE_MARK   1
#define EDGE_TAG    2

#define VERT_MARK   1
#define VERT_TAG    2

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
							BM_face_split(bm, l->f, l->next->v, l->prev->v, NULL, NULL, true);
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
	BLI_array_declare(faces);
	BLI_array_declare(regions);
	BMFace ***regions = NULL;
	BMFace **faces = NULL;
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
	/* might want to make this an option or mode - campbell */

	/* BMOperator fop; */
	BMFace *act_face = bm->act_face;
	BMOIter eiter;
	BMEdge *e;
	BMIter viter;
	BMVert *v;

	const bool use_verts = BMO_slot_bool_get(op->slots_in, "use_verts");
	const bool use_face_split = BMO_slot_bool_get(op->slots_in, "use_face_split");

	if (use_face_split) {
		BMO_slot_buffer_flag_enable(bm, op->slots_in, "edges", BM_EDGE, EDGE_TAG);

		BM_ITER_MESH (v, &viter, bm, BM_VERTS_OF_MESH) {
			BMIter iter;
			int untag_count = 0;
			BM_ITER_ELEM(e, &iter, v, BM_EDGES_OF_VERT) {
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
		BM_ITER_MESH (v, &viter, bm, BM_VERTS_OF_MESH) {
			BMO_elem_flag_set(bm, v, VERT_MARK, (BM_vert_edge_count(v) != 2));
		}
	}

	BMO_ITER (e, &eiter, op->slots_in, "edges", BM_EDGE) {
		BMFace *fa, *fb;

		if (BM_edge_face_pair(e, &fa, &fb)) {
			BMFace *f_new;

			/* join faces */

			/* BMESH_TODO - check on delaying edge removal since we may end up removing more than
			 * one edge, and later reference a removed edge */
			f_new = BM_faces_join_pair(bm, fa, fb, e, true);

			if (f_new) {
				/* maintain active face */
				if (act_face && bm->act_face == NULL) {
					bm->act_face = f_new;
				}
			}
		}
	}

	if (use_verts) {
		BM_ITER_MESH (v, &viter, bm, BM_VERTS_OF_MESH) {
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
	BM_ITER_ELEM(f, &fiter, v, BM_FACES_OF_VERT) {
		BM_ITER_ELEM(l, &liter, f, BM_LOOPS_OF_FACE) {
			if (!BMO_elem_flag_test(bm, l->v, VERT_MARK)) {
				/* if an edge around a vert is a boundary edge,
				 * then dissolve faces won't destroy it.
				 * also if it forms a boundary with one
				 * of the face region */
				bool found = false;
				BM_ITER_ELEM(e, &eiter, l->v, BM_EDGES_OF_VERT) {
					BMFace *f_iter;
					if (BM_edge_is_boundary(e)) {
						found = true;
					}
					else {
						BM_ITER_ELEM(f_iter, &fiter_sub, e, BM_FACES_OF_EDGE) {
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
	BMVert *v;
	BMFace *f;

	const bool use_face_split = BMO_slot_bool_get(op->slots_in, "use_face_split");


	BMO_slot_buffer_flag_enable(bm, op->slots_in, "verts", BM_VERT, VERT_MARK);
	
	if (use_face_split) {
		bm_face_split(bm, VERT_MARK);
	}

	BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
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
	
	/* clean up any remainin */
	BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
		if (BMO_elem_flag_test(bm, v, VERT_MARK)) {
			if (!BM_vert_dissolve(bm, v)) {
				BMO_error_raise(bm, op, BMERR_DISSOLVEVERTS_FAILED, NULL);
				return;
			}
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
	                             (BMEdge **)BMO_SLOT_AS_BUFFER(einput), einput->len);
}
