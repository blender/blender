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

/** \file blender/bmesh/operators/bmo_extrude.c
 *  \ingroup bmesh
 */

#include "MEM_guardedalloc.h"

#include "DNA_meshdata_types.h"

#include "BLI_math.h"
#include "BLI_array.h"

#include "BKE_customdata.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */

enum {
	EXT_INPUT   = 1,
	EXT_KEEP    = 2,
	EXT_DEL     = 4
};

#define VERT_MARK 1
#define EDGE_MARK 1
#define FACE_MARK 1
#define VERT_NONMAN 2
#define EDGE_NONMAN 2

void bmo_extrude_discrete_faces_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMIter liter, liter2;
	BMFace *f, *f2, *f3;
	BMLoop *l, *l2, *l3, *l4, *l_tmp;
	BMEdge **edges = NULL, *e, *laste;
	BMVert *v, *lastv, *firstv;
	BLI_array_declare(edges);
	int i;

	BMO_ITER (f, &siter, bm, op, "faces", BM_FACE) {
		BLI_array_empty(edges);
		BLI_array_grow_items(edges, f->len);

		i = 0;
		firstv = lastv = NULL;
		BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
			v = BM_vert_create(bm, l->v->co, l->v);

			/* skip on the first iteration */
			if (lastv) {
				e = BM_edge_create(bm, lastv, v, l->e, FALSE);
				edges[i++] = e;
			}

			lastv = v;
			laste = l->e;
			if (!firstv) firstv = v;
		}

		/* this fits in the array because we skip one in the loop above */
		e = BM_edge_create(bm, v, firstv, laste, FALSE);
		edges[i++] = e;

		BMO_elem_flag_enable(bm, f, EXT_DEL);

		f2 = BM_face_create_ngon(bm, firstv, BM_edge_other_vert(edges[0], firstv), edges, f->len, FALSE);
		if (!f2) {
			BMO_error_raise(bm, op, BMERR_MESH_ERROR, "Extrude failed; could not create face");
			BLI_array_free(edges);
			return;
		}
		
		BMO_elem_flag_enable(bm, f2, EXT_KEEP);
		BM_elem_attrs_copy(bm, bm, f, f2);

		l2 = BM_iter_new(&liter2, bm, BM_LOOPS_OF_FACE, f2);
		BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
			BM_elem_attrs_copy(bm, bm, l, l2);

			l3 = l->next;
			l4 = l2->next;

			f3 = BM_face_create_quad_tri(bm, l3->v, l4->v, l2->v, l->v, f, FALSE);

			l_tmp = BM_FACE_FIRST_LOOP(f3);

			BM_elem_attrs_copy(bm, bm, l->next, l_tmp);  l_tmp = l_tmp->next;
			BM_elem_attrs_copy(bm, bm, l->next, l_tmp);  l_tmp = l_tmp->next;
			BM_elem_attrs_copy(bm, bm, l, l_tmp);        l_tmp = l_tmp->next;
			BM_elem_attrs_copy(bm, bm, l, l_tmp);

			l2 = BM_iter_step(&liter2);
		}
	}

	BLI_array_free(edges);

	BMO_op_callf(bm, "delete geom=%ff context=%i", EXT_DEL, DEL_ONLYFACES);
	BMO_slot_buffer_from_enabled_flag(bm, op, "faceout", BM_FACE, EXT_KEEP);
}

/**
 * \brief Copy the loop pair from an adjacent face to both sides of this quad.
 *
 * The face is assumed to be a quad, created by extruding.
 * This function won't crash if its not but won't work right either.
 * \a e_b is the new edge.
 *
 * \note this function could be exposed as an api call if other areas need it,
 * so far only extrude does.
 */
static void bm_extrude_copy_face_loop_attributes(BMesh *bm, BMFace *f, BMEdge *e_a, BMEdge *e_b)
{
	/* 'a' is the starting edge #e, 'b' is the final edge #newedge */
	BMLoop *l_dst_a = BM_face_edge_share_loop(f, e_a);
	BMLoop *l_dst_b = BM_face_edge_share_loop(f, e_b);
	/* we could only have a face on one-or the other edges,
	 * check if either side of the face has an adjacent face */
	BMLoop *l_src_1;
	BMLoop *l_src_2;

	/* there is no l_src_b */

	/* sanity */
	BLI_assert(l_dst_a->f == l_dst_b->f);

	if (l_dst_a != l_dst_a->radial_next) {
		l_src_1 = l_dst_a->radial_next;
		l_src_2 = l_src_1->next;
	}
	else if (l_dst_b != l_dst_b->radial_next) {
		l_src_2 = l_dst_b->radial_next;
		l_src_1 = l_src_2->next;
	}
	else {
		/* no new faces on either edge, nothing to copy from */
		return;
	}

	BM_elem_attrs_copy(bm, bm, l_src_1->f, l_dst_a->f);
	BM_elem_flag_disable(f, BM_ELEM_HIDDEN); /* possibly we copy from a hidden face */

	/* copy data */
	BM_elem_attrs_copy(bm, bm, l_src_2, l_dst_a);
	BM_elem_attrs_copy(bm, bm, l_src_2, l_dst_b->next);

	BM_elem_attrs_copy(bm, bm, l_src_1, l_dst_a->next);
	BM_elem_attrs_copy(bm, bm, l_src_1, l_dst_b);
}

/* Disable the skin root flag on the input vert, assumes that the vert
   data includes an CD_MVERT_SKIN layer */
static void bm_extrude_disable_skin_root(BMesh *bm, BMVert *v)
{
	MVertSkin *vs;
	
	vs = CustomData_bmesh_get(&bm->vdata, v->head.data, CD_MVERT_SKIN);
	vs->flag &= ~MVERT_SKIN_ROOT;
}

void bmo_extrude_edge_only_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMOperator dupeop;
	BMVert *v1, *v2, *v3, *v4;
	BMEdge *e, *e2;
	BMFace *f;
	
	BMO_ITER (e, &siter, bm, op, "edges", BM_EDGE) {
		BMO_elem_flag_enable(bm, e, EXT_INPUT);
		BMO_elem_flag_enable(bm, e->v1, EXT_INPUT);
		BMO_elem_flag_enable(bm, e->v2, EXT_INPUT);
	}

	BMO_op_initf(bm, &dupeop, "dupe geom=%fve", EXT_INPUT);
	BMO_op_exec(bm, &dupeop);

	/* disable root flag on all new skin nodes */
	if (CustomData_has_layer(&bm->vdata, CD_MVERT_SKIN)) {
		BMO_ITER(v1, &siter, bm, &dupeop, "newout", BM_VERT) {
			bm_extrude_disable_skin_root(bm, v1);
		}
	}

	for (e = BMO_iter_new(&siter, bm, &dupeop, "boundarymap", 0); e; e = BMO_iter_step(&siter)) {
		e2 = BMO_iter_map_value(&siter);
		e2 = *(BMEdge **)e2;

		if (e->l && e->v1 != e->l->v) {
			v1 = e->v1;
			v2 = e->v2;
			v3 = e2->v2;
			v4 = e2->v1;
		}
		else {
			v1 = e2->v1;
			v2 = e2->v2;
			v3 = e->v2;
			v4 = e->v1;
		}
		/* not sure what to do about example face, pass NULL for now */
		f = BM_face_create_quad_tri(bm, v1, v2, v3, v4, NULL, FALSE);
		bm_extrude_copy_face_loop_attributes(bm, f, e, e2);
		
		if (BMO_elem_flag_test(bm, e, EXT_INPUT))
			e = e2;
		
		BMO_elem_flag_enable(bm, f, EXT_KEEP);
		BMO_elem_flag_enable(bm, e, EXT_KEEP);
		BMO_elem_flag_enable(bm, e->v1, EXT_KEEP);
		BMO_elem_flag_enable(bm, e->v2, EXT_KEEP);
		
	}

	BMO_op_finish(bm, &dupeop);

	BMO_slot_buffer_from_enabled_flag(bm, op, "geomout", BM_ALL, EXT_KEEP);
}

void bmo_extrude_vert_indiv_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMVert *v, *dupev;
	BMEdge *e;
	const int has_vskin = CustomData_has_layer(&bm->vdata, CD_MVERT_SKIN);

	for (v = BMO_iter_new(&siter, bm, op, "verts", BM_VERT); v; v = BMO_iter_step(&siter)) {
		dupev = BM_vert_create(bm, v->co, v);
		if (has_vskin)
			bm_extrude_disable_skin_root(bm, v);

		e = BM_edge_create(bm, v, dupev, NULL, FALSE);

		BMO_elem_flag_enable(bm, e, EXT_KEEP);
		BMO_elem_flag_enable(bm, dupev, EXT_KEEP);
	}

	BMO_slot_buffer_from_enabled_flag(bm, op, "vertout", BM_VERT, EXT_KEEP);
	BMO_slot_buffer_from_enabled_flag(bm, op, "edgeout", BM_EDGE, EXT_KEEP);
}

void bmo_extrude_face_region_exec(BMesh *bm, BMOperator *op)
{
	BMOperator dupeop, delop;
	BMOIter siter;
	BMIter iter, fiter, viter;
	BMEdge *e, *newedge;
	BMVert *verts[4], *v, *v2;
	BMFace *f;
	int found, fwd, delorig = FALSE;

	/* initialize our sub-operators */
	BMO_op_init(bm, &dupeop, "duplicate");
	
	BMO_slot_buffer_flag_enable(bm, op, "edgefacein", BM_EDGE | BM_FACE, EXT_INPUT);
	
	/* if one flagged face is bordered by an un-flagged face, then we delete
	 * original geometry unless caller explicitly asked to keep it. */
	if (!BMO_slot_bool_get(op, "alwayskeeporig")) {
		BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {

			int edge_face_tot;

			if (!BMO_elem_flag_test(bm, e, EXT_INPUT)) {
				continue;
			}

			found = FALSE; /* found a face that isn't input? */
			edge_face_tot = 0; /* edge/face count */

			BM_ITER_ELEM (f, &fiter, e, BM_FACES_OF_EDGE) {
				if (!BMO_elem_flag_test(bm, f, EXT_INPUT)) {
					found = TRUE;
					delorig = TRUE;
					break;
				}

				edge_face_tot++;
			}

			if ((edge_face_tot > 1) && (found == FALSE)) {
				/* edge has a face user, that face isn't extrude input */
				BMO_elem_flag_enable(bm, e, EXT_DEL);
			}
		}
	}

	/* calculate verts to delete */
	BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
		found = FALSE;

		BM_ITER_ELEM (e, &viter, v, BM_EDGES_OF_VERT) {
			if (!BMO_elem_flag_test(bm, e, EXT_INPUT) || !BMO_elem_flag_test(bm, e, EXT_DEL)) {
				found = TRUE;
				break;
			}
		}

		/* avoid an extra loop */
		if (found == TRUE) {
			BM_ITER_ELEM (f, &viter, v, BM_FACES_OF_VERT) {
				if (!BMO_elem_flag_test(bm, f, EXT_INPUT)) {
					found = TRUE;
					break;
				}
			}
		}

		if (found == FALSE) {
			BMO_elem_flag_enable(bm, v, EXT_DEL);
		}
	}
	
	BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
		if (BMO_elem_flag_test(bm, f, EXT_INPUT)) {
			BMO_elem_flag_enable(bm, f, EXT_DEL);
		}
	}

	if (delorig == TRUE) {
		BMO_op_initf(bm, &delop, "delete geom=%fvef context=%i",
		             EXT_DEL, DEL_ONLYTAGGED);
	}

	BMO_slot_copy(op, &dupeop, "edgefacein", "geom");
	BMO_op_exec(bm, &dupeop);

	/* disable root flag on all new skin nodes */
	if (CustomData_has_layer(&bm->vdata, CD_MVERT_SKIN)) {
		BMO_ITER(v, &siter, bm, &dupeop, "newout", BM_VERT) {
			bm_extrude_disable_skin_root(bm, v);
		}
	}

	if (bm->act_face && BMO_elem_flag_test(bm, bm->act_face, EXT_INPUT))
		bm->act_face = BMO_slot_map_ptr_get(bm, &dupeop, "facemap", bm->act_face);

	if (delorig) {
		BMO_op_exec(bm, &delop);
	}
	
	/* if not delorig, reverse loops of original face */
	if (!delorig) {
		BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
			if (BMO_elem_flag_test(bm, f, EXT_INPUT)) {
				BM_face_normal_flip(bm, f);
			}
		}
	}
	
	BMO_slot_copy(&dupeop, op, "newout", "geomout");

	for (e = BMO_iter_new(&siter, bm, &dupeop, "boundarymap", 0); e; e = BMO_iter_step(&siter)) {

		/* this should always be wire, so this is mainly a speedup to avoid map lookup */
		if (BM_edge_is_wire(e) && BMO_slot_map_contains(bm, op, "exclude", e)) {
			/* The original edge was excluded,
			 * this would result in a standalone wire edge - see [#30399] */
			BM_edge_kill(bm, e);

			continue;
		}

		newedge = *(BMEdge **)BMO_iter_map_value(&siter);

		if (!newedge) {
			continue;
		}

		/* orient loop to give same normal as a loop of newedge
		 * if it exists (will be an extruded face),
		 * else same normal as a loop of e, if it exists */
		if (!newedge->l)
			fwd = !e->l || !(e->l->v == e->v1);
		else
			fwd = (newedge->l->v == newedge->v1);

		
		if (fwd) {
			verts[0] = e->v1;
			verts[1] = e->v2;
			verts[2] = newedge->v2;
			verts[3] = newedge->v1;
		}
		else {
			verts[3] = e->v1;
			verts[2] = e->v2;
			verts[1] = newedge->v2;
			verts[0] = newedge->v1;
		}

		/* not sure what to do about example face, pass NULL for now */
		f = BM_face_create_quad_tri_v(bm, verts, 4, NULL, FALSE);
		bm_extrude_copy_face_loop_attributes(bm, f, e, newedge);
	}

	/* link isolated vert */
	for (v = BMO_iter_new(&siter, bm, &dupeop, "isovertmap", 0); v; v = BMO_iter_step(&siter)) {
		v2 = *((void **)BMO_iter_map_value(&siter));
		BM_edge_create(bm, v, v2, v->e, TRUE);
	}

	/* cleanup */
	if (delorig) BMO_op_finish(bm, &delop);
	BMO_op_finish(bm, &dupeop);
}

/*
 * Compute higher-quality vertex normals used by solidify.
 * Only considers geometry in the marked solidify region.
 * Note that this does not work so well for non-manifold
 * regions.
 */
static void calc_solidify_normals(BMesh *bm)
{
	BMIter viter, eiter, fiter;
	BMVert *v;
	BMEdge *e;
	BMFace *f, *f1, *f2;
	float edge_normal[3];
	int i;

	/* can't use BM_edge_face_count because we need to count only marked faces */
	int *edge_face_count = MEM_callocN(sizeof(int) * bm->totedge, __func__);

	BM_ITER_MESH (v, &viter, bm, BM_VERTS_OF_MESH) {
		BM_elem_flag_enable(v, BM_ELEM_TAG);
	}

	BM_mesh_elem_index_ensure(bm, BM_EDGE);

	BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
		if (!BMO_elem_flag_test(bm, f, FACE_MARK)) {
			continue;
		}

		BM_ITER_ELEM (e, &eiter, f, BM_EDGES_OF_FACE) {

			/* And mark all edges and vertices on the
			 * marked faces */
			BMO_elem_flag_enable(bm, e, EDGE_MARK);
			BMO_elem_flag_enable(bm, e->v1, VERT_MARK);
			BMO_elem_flag_enable(bm, e->v2, VERT_MARK);
			edge_face_count[BM_elem_index_get(e)]++;
		}
	}

	BM_ITER_MESH (e, &eiter, bm, BM_EDGES_OF_MESH) {
		if (!BMO_elem_flag_test(bm, e, EDGE_MARK)) {
			continue;
		}

		i = edge_face_count[BM_elem_index_get(e)]++;

		if (i == 0 || i > 2) {
			/* Edge & vertices are non-manifold even when considering
			 * only marked faces */
			BMO_elem_flag_enable(bm, e, EDGE_NONMAN);
			BMO_elem_flag_enable(bm, e->v1, VERT_NONMAN);
			BMO_elem_flag_enable(bm, e->v2, VERT_NONMAN);
		}
	}
	MEM_freeN(edge_face_count);
	edge_face_count = NULL; /* don't re-use */

	BM_ITER_MESH (v, &viter, bm, BM_VERTS_OF_MESH) {
		if (!BM_vert_is_manifold(v)) {
			BMO_elem_flag_enable(bm, v, VERT_NONMAN);
			continue;
		}

		if (BMO_elem_flag_test(bm, v, VERT_MARK)) {
			zero_v3(v->no);
		}
	}

	BM_ITER_MESH (e, &eiter, bm, BM_EDGES_OF_MESH) {

		/* If the edge is not part of a the solidify region
		 * its normal should not be considered */
		if (!BMO_elem_flag_test(bm, e, EDGE_MARK)) {
			continue;
		}

		/* If the edge joins more than two marked faces high
		 * quality normal computation won't work */
		if (BMO_elem_flag_test(bm, e, EDGE_NONMAN)) {
			continue;
		}

		f1 = f2 = NULL;

		BM_ITER_ELEM (f, &fiter, e, BM_FACES_OF_EDGE) {
			if (BMO_elem_flag_test(bm, f, FACE_MARK)) {
				if (f1 == NULL) {
					f1 = f;
				}
				else {
					BLI_assert(f2 == NULL);
					f2 = f;
				}
			}
		}

		BLI_assert(f1 != NULL);

		if (f2 != NULL) {
			const float angle = angle_normalized_v3v3(f1->no, f2->no);

			if (angle > 0.0f) {
				/* two faces using this edge, calculate the edge normal
				 * using the angle between the faces as a weighting */
				add_v3_v3v3(edge_normal, f1->no, f2->no);
				normalize_v3(edge_normal);
				mul_v3_fl(edge_normal, angle);
			}
			else {
				/* can't do anything useful here!
				 * Set the face index for a vert in case it gets a zero normal */
				BM_elem_flag_disable(e->v1, BM_ELEM_TAG);
				BM_elem_flag_disable(e->v2, BM_ELEM_TAG);
				continue;
			}
		}
		else {
			/* only one face attached to that edge */
			/* an edge without another attached- the weight on this is
			 * undefined, M_PI / 2 is 90d in radians and that seems good enough */
			copy_v3_v3(edge_normal, f1->no);
			mul_v3_fl(edge_normal, M_PI / 2);
		}

		add_v3_v3(e->v1->no, edge_normal);
		add_v3_v3(e->v2->no, edge_normal);
	}

	/* normalize accumulated vertex normal */
	BM_ITER_MESH (v, &viter, bm, BM_VERTS_OF_MESH) {
		if (!BMO_elem_flag_test(bm, v, VERT_MARK)) {
			continue;
		}

		if (BMO_elem_flag_test(bm, v, VERT_NONMAN)) {
			/* use standard normals for vertices connected to non-manifold edges */
			BM_vert_normal_update(v);
		}
		else if (normalize_v3(v->no) == 0.0f && !BM_elem_flag_test(v, BM_ELEM_TAG)) {
			/* exceptional case, totally flat. use the normal
			 * of any marked face around the vertex */
			BM_ITER_ELEM (f, &fiter, v, BM_FACES_OF_VERT) {
				if (BMO_elem_flag_test(bm, f, FACE_MARK)) {
					break;
				}
			}
			copy_v3_v3(v->no, f->no);
		}
	}
}

static void solidify_add_thickness(BMesh *bm, const float dist)
{
	BMFace *f;
	BMVert *v;
	BMLoop *l;
	BMIter iter, loopIter;
	float *vert_angles = MEM_callocN(sizeof(float) * bm->totvert * 2, "solidify"); /* 2 in 1 */
	float *vert_accum = vert_angles + bm->totvert;
	int i, index;

	/* array for passing verts to angle_poly_v3 */
	float **verts = NULL;
	BLI_array_staticdeclare(verts, BM_NGON_STACK_SIZE);
	/* array for receiving angles from angle_poly_v3 */
	float *face_angles = NULL;
	BLI_array_staticdeclare(face_angles, BM_NGON_STACK_SIZE);

	BM_mesh_elem_index_ensure(bm, BM_VERT);

	BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
		if (!BMO_elem_flag_test(bm, f, FACE_MARK)) {
			continue;
		}

		BLI_array_grow_items(verts, f->len);
		BM_ITER_ELEM_INDEX (l, &loopIter, f, BM_LOOPS_OF_FACE, i) {
			verts[i] = l->v->co;
		}

		BLI_array_grow_items(face_angles, f->len);
		angle_poly_v3(face_angles, (const float **)verts, f->len);

		i = 0;
		BM_ITER_ELEM (l, &loopIter, f, BM_LOOPS_OF_FACE) {
			v = l->v;
			index = BM_elem_index_get(v);
			vert_accum[index] += face_angles[i];
			vert_angles[index] += shell_angle_to_dist(angle_normalized_v3v3(v->no, f->no)) * face_angles[i];
			i++;
		}

		BLI_array_empty(verts);
		BLI_array_empty(face_angles);
	}

	BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
		index = BM_elem_index_get(v);
		if (vert_accum[index]) { /* zero if unselected */
			madd_v3_v3fl(v->co, v->no, dist * (vert_angles[index] / vert_accum[index]));
		}
	}

	MEM_freeN(vert_angles);

	BLI_array_free(verts);
	BLI_array_free(face_angles);
}

void bmo_solidify_face_region_exec(BMesh *bm, BMOperator *op)
{
	BMOperator extrudeop;
	BMOperator reverseop;
	float thickness;

	thickness = BMO_slot_float_get(op, "thickness");

	/* Flip original faces (so the shell is extruded inward) */
	BMO_op_init(bm, &reverseop, "reverse_faces");
	BMO_slot_copy(op, &reverseop, "geom", "faces");
	BMO_op_exec(bm, &reverseop);
	BMO_op_finish(bm, &reverseop);

	/* Extrude the region */
	BMO_op_initf(bm, &extrudeop, "extrude_face_region alwayskeeporig=%b", TRUE);
	BMO_slot_copy(op, &extrudeop, "geom", "edgefacein");
	BMO_op_exec(bm, &extrudeop);

	/* Push the verts of the extruded faces inward to create thickness */
	BMO_slot_buffer_flag_enable(bm, &extrudeop, "geomout", BM_FACE, FACE_MARK);
	calc_solidify_normals(bm);
	solidify_add_thickness(bm, thickness);

	BMO_slot_copy(&extrudeop, op, "geomout", "geomout");

	BMO_op_finish(bm, &extrudeop);
}
