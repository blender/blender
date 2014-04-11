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

/** \file blender/bmesh/operators/bmo_edgenet.c
 *  \ingroup bmesh
 *
 * Edge-Net for filling in open edge-loops.
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_array.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "intern/bmesh_operators_private.h" /* own include */

#define EDGE_MARK	1
#define EDGE_VIS	2

#define ELE_NEW		1

void bmo_edgenet_fill_exec(BMesh *bm, BMOperator *op)
{
	BMOperator op_attr;
	BMOIter siter;
	BMFace *f;
	const short mat_nr        = BMO_slot_int_get(op->slots_in,  "mat_nr");
	const bool use_smooth     = BMO_slot_bool_get(op->slots_in, "use_smooth");
//	const int sides           = BMO_slot_int_get(op->slots_in,  "sides");

	if (!bm->totvert || !bm->totedge)
		return;

	BM_mesh_elem_hflag_disable_all(bm, BM_EDGE, BM_ELEM_TAG, false);
	BMO_slot_buffer_hflag_enable(bm, op->slots_in, "edges", BM_EDGE, BM_ELEM_TAG, false);

	BM_mesh_elem_hflag_disable_all(bm, BM_FACE, BM_ELEM_TAG, false);
	BM_mesh_edgenet(bm, true, true); // TODO, sides

	BMO_slot_buffer_from_enabled_hflag(bm, op, op->slots_out, "faces.out", BM_FACE, BM_ELEM_TAG);

	BMO_ITER (f, &siter, op->slots_out, "faces.out", BM_FACE) {
		f->mat_nr = mat_nr;
		if (use_smooth) {
			BM_elem_flag_enable(f, BM_ELEM_SMOOTH);
		}
		/* normals are zero'd */
		BM_face_normal_update(f);
	}

	/* --- Attribute Fill --- */
	/* may as well since we have the faces already in a buffer */
	BMO_op_initf(bm, &op_attr, op->flag,
	             "face_attribute_fill faces=%S use_normals=%b",
	             op, "faces.out", true);

	BMO_op_exec(bm, &op_attr);

	/* check if some faces couldn't be touched */
	if (BMO_slot_buffer_count(op_attr.slots_out, "faces_fail.out")) {
		BMO_op_callf(bm, op->flag, "recalc_face_normals faces=%S", &op_attr, "faces_fail.out");
	}
	BMO_op_finish(bm, &op_attr);

}

static BMEdge *edge_next(BMesh *bm, BMEdge *e)
{
	BMIter iter;
	BMEdge *e2;
	int i;

	for (i = 0; i < 2; i++) {
		BM_ITER_ELEM (e2, &iter, i ? e->v2 : e->v1, BM_EDGES_OF_VERT) {
			if ((BMO_elem_flag_test(bm, e2, EDGE_MARK)) &&
			    (!BMO_elem_flag_test(bm, e2, EDGE_VIS)) &&
			    (e2 != e))
			{
				return e2;
			}
		}
	}

	return NULL;
}

void bmo_edgenet_prepare_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMEdge *e;
	BMEdge **edges1 = NULL, **edges2 = NULL, **edges;
	BLI_array_declare(edges1);
	BLI_array_declare(edges2);
	BLI_array_declare(edges);
	bool ok = true;
	int i, count;

	BMO_slot_buffer_flag_enable(bm, op->slots_in, "edges", BM_EDGE, EDGE_MARK);

	/* validate that each edge has at most one other tagged edge in the
	 * disk cycle around each of it's vertices */
	BMO_ITER (e, &siter, op->slots_in, "edges", BM_EDGE) {
		for (i = 0; i < 2; i++) {
			count = BMO_iter_elem_count_flag(bm,  BM_EDGES_OF_VERT, (i ? e->v2 : e->v1), EDGE_MARK, true);
			if (count > 2) {
				ok = 0;
				break;
			}
		}

		if (!ok) {
			break;
		}
	}

	/* we don't have valid edge layouts, retur */
	if (!ok) {
		return;
	}

	/* find connected loops within the input edge */
	count = 0;
	while (1) {
		BMO_ITER (e, &siter, op->slots_in, "edges", BM_EDGE) {
			if (!BMO_elem_flag_test(bm, e, EDGE_VIS)) {
				if (BMO_iter_elem_count_flag(bm, BM_EDGES_OF_VERT, e->v1, EDGE_MARK, true) == 1 ||
				    BMO_iter_elem_count_flag(bm, BM_EDGES_OF_VERT, e->v2, EDGE_MARK, true) == 1)
				{
					break;
				}
			}
		}

		if (!e) {
			break;
		}

		if (!count) {
			edges = edges1;
		}
		else if (count == 1) {
			edges = edges2;
		}
		else {
			break;
		}

		i = 0;
		while (e) {
			BMO_elem_flag_enable(bm, e, EDGE_VIS);
			BLI_array_grow_one(edges);
			edges[i] = e;

			e = edge_next(bm, e);
			i++;
		}

		if (!count) {
			edges1 = edges;
			BLI_array_length_set(edges1, BLI_array_count(edges));
		}
		else {
			edges2 = edges;
			BLI_array_length_set(edges2, BLI_array_count(edges));
		}

		BLI_array_empty(edges);
		count++;
	}

	if (edges1 && BLI_array_count(edges1) > 2 &&
	    BM_edge_share_vert_check(edges1[0], edges1[BLI_array_count(edges1) - 1]))
	{
		if (edges2 && BLI_array_count(edges2) > 2 &&
		    BM_edge_share_vert_check(edges2[0], edges2[BLI_array_count(edges2) - 1]))
		{
			BLI_array_free(edges1);
			BLI_array_free(edges2);
			return;
		}
		else {
			edges1 = edges2;
			edges2 = NULL;
		}
	}

	if (edges2 && BLI_array_count(edges2) > 2 &&
	    BM_edge_share_vert_check(edges2[0], edges2[BLI_array_count(edges2) - 1]))
	{
		edges2 = NULL;
	}

	/* two unconnected loops, connect the */
	if (edges1 && edges2) {
		BMVert *v1, *v2, *v3, *v4;
		float dvec1[3];
		float dvec2[3];

		if (BLI_array_count(edges1) == 1) {
			v1 = edges1[0]->v1;
			v2 = edges1[0]->v2;
		}
		else {
			v1 = BM_vert_in_edge(edges1[1], edges1[0]->v1) ? edges1[0]->v2 : edges1[0]->v1;
			i  = BLI_array_count(edges1) - 1;
			v2 = BM_vert_in_edge(edges1[i - 1], edges1[i]->v1) ? edges1[i]->v2 : edges1[i]->v1;
		}

		if (BLI_array_count(edges2) == 1) {
			v3 = edges2[0]->v1;
			v4 = edges2[0]->v2;
		}
		else {
			v3 = BM_vert_in_edge(edges2[1], edges2[0]->v1) ? edges2[0]->v2 : edges2[0]->v1;
			i  = BLI_array_count(edges2) - 1;
			v4 = BM_vert_in_edge(edges2[i - 1], edges2[i]->v1) ? edges2[i]->v2 : edges2[i]->v1;
		}

		/* if there is ever bow-tie quads between two edges the problem is here! [#30367] */
#if 0
		normal_tri_v3(dvec1, v1->co, v2->co, v4->co);
		normal_tri_v3(dvec2, v1->co, v4->co, v3->co);
#else
		{
			/* save some CPU cycles and skip the sqrt and 1 subtraction */
			float a1[3], a2[3], a3[3];
			sub_v3_v3v3(a1, v1->co, v2->co);
			sub_v3_v3v3(a2, v1->co, v4->co);
			sub_v3_v3v3(a3, v1->co, v3->co);
			cross_v3_v3v3(dvec1, a1, a2);
			cross_v3_v3v3(dvec2, a2, a3);
		}
#endif
		if (dot_v3v3(dvec1, dvec2) < 0.0f) {
			SWAP(BMVert *, v3, v4);
		}

		e = BM_edge_create(bm, v1, v3, NULL, BM_CREATE_NO_DOUBLE);
		BMO_elem_flag_enable(bm, e, ELE_NEW);
		e = BM_edge_create(bm, v2, v4, NULL, BM_CREATE_NO_DOUBLE);
		BMO_elem_flag_enable(bm, e, ELE_NEW);
	}
	else if (edges1) {
		BMVert *v1, *v2;

		if (BLI_array_count(edges1) > 1) {
			v1 = BM_vert_in_edge(edges1[1], edges1[0]->v1) ? edges1[0]->v2 : edges1[0]->v1;
			i  = BLI_array_count(edges1) - 1;
			v2 = BM_vert_in_edge(edges1[i - 1], edges1[i]->v1) ? edges1[i]->v2 : edges1[i]->v1;
			e  = BM_edge_create(bm, v1, v2, NULL, BM_CREATE_NO_DOUBLE);
			BMO_elem_flag_enable(bm, e, ELE_NEW);
		}
	}

	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "edges.out", BM_EDGE, ELE_NEW);

	BLI_array_free(edges1);
	BLI_array_free(edges2);
}
