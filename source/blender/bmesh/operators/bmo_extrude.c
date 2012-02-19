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

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_array.h"

#include "bmesh.h"

#include "bmesh_operators_private.h" /* own include */

#define EXT_INPUT 1
#define EXT_KEEP  2
#define EXT_DEL   4

#define VERT_MARK 1
#define EDGE_MARK 1
#define FACE_MARK 1
#define VERT_NONMAN 2
#define EDGE_NONMAN 2

void bmesh_extrude_face_indiv_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMIter liter, liter2;
	BMFace *f, *f2, *f3;
	BMLoop *l, *l2, *l3, *l4, *l_tmp;
	BMEdge **edges = NULL, *e, *laste;
	BMVert *v, *lastv, *firstv;
	BLI_array_declare(edges);
	int i;

	BMO_ITER(f, &siter, bm, op, "faces", BM_FACE) {
		BLI_array_empty(edges);
		i = 0;
		firstv = lastv = NULL;
		BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f) {
			BLI_array_growone(edges);

			v = BM_vert_create(bm, l->v->co, l->v);

			if (lastv) {
				e = BM_edge_create(bm, lastv, v, l->e, FALSE);
				edges[i++] = e;
			}

			lastv = v;
			laste = l->e;
			if (!firstv) firstv = v;
		}

		BLI_array_growone(edges);
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
		BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f) {
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

	BMO_op_callf(bm, "del geom=%ff context=%d", EXT_DEL, DEL_ONLYFACES);
	BMO_slot_from_flag(bm, op, "faceout", EXT_KEEP, BM_FACE);
}

void bmesh_extrude_onlyedge_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMOperator dupeop;
	BMVert *v1, *v2, *v3, *v4;
	BMEdge *e, *e2;
	BMFace *f;
	
	BMO_ITER(e, &siter, bm, op, "edges", BM_EDGE) {
		BMO_elem_flag_enable(bm, e, EXT_INPUT);
		BMO_elem_flag_enable(bm, e->v1, EXT_INPUT);
		BMO_elem_flag_enable(bm, e->v2, EXT_INPUT);
	}

	BMO_op_initf(bm, &dupeop, "dupe geom=%fve", EXT_INPUT);
	BMO_op_exec(bm, &dupeop);

	e = BMO_iter_new(&siter, bm, &dupeop, "boundarymap", 0);
	for ( ; e; e = BMO_iter_step(&siter)) {
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
		/* not sure what to do about example face, pass	 NULL for now */
		f = BM_face_create_quad_tri(bm, v1, v2, v3, v4, NULL, FALSE);
		
		if (BMO_elem_flag_test(bm, e, EXT_INPUT))
			e = e2;
		
		BMO_elem_flag_enable(bm, f, EXT_KEEP);
		BMO_elem_flag_enable(bm, e, EXT_KEEP);
		BMO_elem_flag_enable(bm, e->v1, EXT_KEEP);
		BMO_elem_flag_enable(bm, e->v2, EXT_KEEP);
		
	}

	BMO_op_finish(bm, &dupeop);

	BMO_slot_from_flag(bm, op, "geomout", EXT_KEEP, BM_ALL);
}

void extrude_vert_indiv_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMVert *v, *dupev;
	BMEdge *e;

	v = BMO_iter_new(&siter, bm, op, "verts", BM_VERT);
	for ( ; v; v = BMO_iter_step(&siter)) {
		dupev = BM_vert_create(bm, v->co, v);

		e = BM_edge_create(bm, v, dupev, NULL, FALSE);

		BMO_elem_flag_enable(bm, e, EXT_KEEP);
		BMO_elem_flag_enable(bm, dupev, EXT_KEEP);
	}

	BMO_slot_from_flag(bm, op, "vertout", EXT_KEEP, BM_VERT);
	BMO_slot_from_flag(bm, op, "edgeout", EXT_KEEP, BM_EDGE);
}

void extrude_edge_context_exec(BMesh *bm, BMOperator *op)
{
	BMOperator dupeop, delop;
	BMOIter siter;
	BMIter iter, fiter, viter;
	BMEdge *e, *newedge;
	BMLoop *l, *l2;
	BMVert *verts[4], *v, *v2;
	BMFace *f;
	int rlen, found, fwd, delorig = 0;

	/* initialize our sub-operators */
	BMO_op_init(bm, &dupeop, "dupe");
	
	BMO_slot_buffer_flag_enable(bm, op, "edgefacein", EXT_INPUT, BM_EDGE|BM_FACE);
	
	/* if one flagged face is bordered by an unflagged face, then we delete
	 * original geometry unless caller explicitly asked to keep it. */
	if (!BMO_slot_int_get(op, "alwayskeeporig")) {
		BM_ITER(e, &iter, bm, BM_EDGES_OF_MESH, NULL) {
			if (!BMO_elem_flag_test(bm, e, EXT_INPUT)) continue;

			found = 0;
			f = BM_iter_new(&fiter, bm, BM_FACES_OF_EDGE, e);
			for (rlen = 0; f; f = BM_iter_step(&fiter), rlen++) {
				if (!BMO_elem_flag_test(bm, f, EXT_INPUT)) {
					found = 1;
					delorig = 1;
					break;
				}
			}

			if (!found && (rlen > 1)) BMO_elem_flag_enable(bm, e, EXT_DEL);
		}
	}

	/* calculate verts to delet */
	BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
		found = 0;

		BM_ITER(e, &viter, bm, BM_EDGES_OF_VERT, v) {
			if (!BMO_elem_flag_test(bm, e, EXT_INPUT) || !BMO_elem_flag_test(bm, e, EXT_DEL)) {
				found = 1;
				break;
			}
		}
		
		BM_ITER(f, &viter, bm, BM_FACES_OF_VERT, v) {
			if (!BMO_elem_flag_test(bm, f, EXT_INPUT)) {
				found = 1;
				break;
			}
		}

		if (!found) {
			BMO_elem_flag_enable(bm, v, EXT_DEL);
		}
	}
	
	BM_ITER(f, &iter, bm, BM_FACES_OF_MESH, NULL) {
		if (BMO_elem_flag_test(bm, f, EXT_INPUT))
			BMO_elem_flag_enable(bm, f, EXT_DEL);
	}

	if (delorig) {
		BMO_op_initf(bm, &delop, "del geom=%fvef context=%d",
		             EXT_DEL, DEL_ONLYTAGGED);
	}

	BMO_slot_copy(op, &dupeop, "edgefacein", "geom");
	BMO_op_exec(bm, &dupeop);

	if (bm->act_face && BMO_elem_flag_test(bm, bm->act_face, EXT_INPUT))
		bm->act_face = BMO_slot_map_ptr_get(bm, &dupeop, "facemap", bm->act_face);

	if (delorig) BMO_op_exec(bm, &delop);
	
	/* if not delorig, reverse loops of original face */
	if (!delorig) {
		for (f = BM_iter_new(&iter, bm, BM_FACES_OF_MESH, NULL); f; f = BM_iter_step(&iter)) {
			if (BMO_elem_flag_test(bm, f, EXT_INPUT)) {
				BM_face_normal_flip(bm, f);
			}
		}
	}
	
	BMO_slot_copy(&dupeop, op, "newout", "geomout");
	e = BMO_iter_new(&siter, bm, &dupeop, "boundarymap", 0);
	for ( ; e; e = BMO_iter_step(&siter)) {
		if (BMO_slot_map_contains(bm, op, "exclude", e)) continue;

		newedge = BMO_iter_map_value(&siter);
		newedge = *(BMEdge **)newedge;
		if (!newedge) continue;

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

		/* copy attribute */
		l = BM_iter_new(&iter, bm, BM_LOOPS_OF_FACE, f);
		for ( ; l; l = BM_iter_step(&iter)) {
			if (l->e != e && l->e != newedge) continue;
			l2 = l->radial_next;
			
			if (l2 == l) {
				l2 = newedge->l;
				BM_elem_attrs_copy(bm, bm, l2->f, l->f);

				BM_elem_attrs_copy(bm, bm, l2, l);
				l2 = l2->next;
				l = l->next;
				BM_elem_attrs_copy(bm, bm, l2, l);
			}
			else {
				BM_elem_attrs_copy(bm, bm, l2->f, l->f);

				/* copy dat */
				if (l2->v == l->v) {
					BM_elem_attrs_copy(bm, bm, l2, l);
					l2 = l2->next;
					l = l->next;
					BM_elem_attrs_copy(bm, bm, l2, l);
				}
				else {
					l2 = l2->next;
					BM_elem_attrs_copy(bm, bm, l2, l);
					l2 = l2->prev;
					l = l->next;
					BM_elem_attrs_copy(bm, bm, l2, l);
				}
			}
		}
	}

	/* link isolated vert */
	v = BMO_iter_new(&siter, bm, &dupeop, "isovertmap", 0);
	for ( ; v; v = BMO_iter_step(&siter)) {
		v2 = *((void **)BMO_iter_map_value(&siter));
		BM_edge_create(bm, v, v2, v->e, TRUE);
	}

	/* cleanu */
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

	BM_ITER(v, &viter, bm, BM_VERTS_OF_MESH, NULL) {
		BM_elem_flag_enable(v, BM_ELEM_TAG);
	}

	BM_mesh_elem_index_ensure(bm, BM_EDGE);

	BM_ITER(f, &fiter, bm, BM_FACES_OF_MESH, NULL) {
		if (!BMO_elem_flag_test(bm, f, FACE_MARK)) {
			continue;
		}

		BM_ITER(e, &eiter, bm, BM_EDGES_OF_FACE, f) {

			/* And mark all edges and vertices on the
			 * marked faces */
			BMO_elem_flag_enable(bm, e, EDGE_MARK);
			BMO_elem_flag_enable(bm, e->v1, VERT_MARK);
			BMO_elem_flag_enable(bm, e->v2, VERT_MARK);
			edge_face_count[BM_elem_index_get(e)]++;
		}
	}

	BM_ITER(e, &eiter, bm, BM_EDGES_OF_MESH, NULL) {
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
	edge_face_count = NULL; /* dont re-use */

	BM_ITER(v, &viter, bm, BM_VERTS_OF_MESH, NULL) {
		if (!BM_vert_is_manifold(bm, v)) {
			BMO_elem_flag_enable(bm, v, VERT_NONMAN);
			continue;
		}

		if (BMO_elem_flag_test(bm, v, VERT_MARK)) {
			zero_v3(v->no);
		}
	}

	BM_ITER(e, &eiter, bm, BM_EDGES_OF_MESH, NULL) {

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

		BM_ITER(f, &fiter, bm, BM_FACES_OF_EDGE, e) {
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
				 * Set the face index for a vert incase it gets a zero normal */
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
	BM_ITER(v, &viter, bm, BM_VERTS_OF_MESH, NULL) {
		if (!BMO_elem_flag_test(bm, v, VERT_MARK)) {
			continue;
		}

		if (BMO_elem_flag_test(bm, v, VERT_NONMAN)) {
			/* use standard normals for vertices connected to non-manifold edges */
			BM_vert_normal_update(bm, v);
		}
		else if (normalize_v3(v->no) == 0.0f && !BM_elem_flag_test(v, BM_ELEM_TAG)) {
			/* exceptional case, totally flat. use the normal
			 * of any marked face around the vertex */
			BM_ITER(f, &fiter, bm, BM_FACES_OF_VERT, v) {
				if (BMO_elem_flag_test(bm, f, FACE_MARK)) {
					break;
				}
			}
			copy_v3_v3(v->no, f->no);
		}
	}
}

static void solidify_add_thickness(BMesh *bm, float dist)
{
	BMFace *f;
	BMVert *v;
	BMLoop *l;
	BMIter iter, loopIter;
	float *vert_angles = MEM_callocN(sizeof(float) * bm->totvert * 2, "solidify"); /* 2 in 1 */
	float *vert_accum = vert_angles + bm->totvert;
	float angle;
	int i, index;
	float maxdist = dist * sqrtf(3.0f);

	/* array for passing verts to angle_poly_v3 */
	float **verts = NULL;
	BLI_array_staticdeclare(verts, BM_NGON_STACK_SIZE);
	/* array for receiving angles from angle_poly_v3 */
	float *angles = NULL;
	BLI_array_staticdeclare(angles, BM_NGON_STACK_SIZE);

	BM_mesh_elem_index_ensure(bm, BM_VERT);

	BM_ITER(f, &iter, bm, BM_FACES_OF_MESH, NULL) {
		if (!BMO_elem_flag_test(bm, f, FACE_MARK)) {
			continue;
		}

		BM_ITER(l, &loopIter, bm, BM_LOOPS_OF_FACE, f) {
			BLI_array_append(verts, l->v->co);
			BLI_array_growone(angles);
		}

		angle_poly_v3(angles, (const float **)verts, f->len);

		i = 0;
		BM_ITER(l, &loopIter, bm, BM_LOOPS_OF_FACE, f) {
			v = l->v;
			index = BM_elem_index_get(v);
			angle = angles[i];
			vert_accum[index] += angle;
			vert_angles[index] += shell_angle_to_dist(angle_normalized_v3v3(v->no, f->no)) * angle;
			i++;
		}

		BLI_array_empty(verts);
		BLI_array_empty(angles);
	}

	BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
		index = BM_elem_index_get(v);
		if (vert_accum[index]) { /* zero if unselected */
			float vdist = MIN2(maxdist, dist * vert_angles[index] / vert_accum[index]);
			madd_v3_v3fl(v->co, v->no, vdist);
		}
	}

	MEM_freeN(vert_angles);
}

void bmesh_solidify_face_region_exec(BMesh *bm, BMOperator *op)
{
	BMOperator extrudeop;
	BMOperator reverseop;
	float thickness;

	thickness = BMO_slot_float_get(op, "thickness");

	/* Flip original faces (so the shell is extruded inward) */
	BMO_op_init(bm, &reverseop, "reversefaces");
	BMO_slot_copy(op, &reverseop, "geom", "faces");
	BMO_op_exec(bm, &reverseop);
	BMO_op_finish(bm, &reverseop);

	/* Extrude the region */
	BMO_op_initf(bm, &extrudeop, "extrudefaceregion alwayskeeporig=%i", TRUE);
	BMO_slot_copy(op, &extrudeop, "geom", "edgefacein");
	BMO_op_exec(bm, &extrudeop);

	/* Push the verts of the extruded faces inward to create thickness */
	BMO_slot_buffer_flag_enable(bm, &extrudeop, "geomout", FACE_MARK, BM_FACE);
	calc_solidify_normals(bm);
	solidify_add_thickness(bm, thickness);

	BMO_slot_copy(&extrudeop, op, "geomout", "geomout");

	BMO_op_finish(bm, &extrudeop);
}
