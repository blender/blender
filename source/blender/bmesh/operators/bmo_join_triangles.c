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

/** \file blender/bmesh/operators/bmo_join_triangles.c
 *  \ingroup bmesh
 *
 * Convert triangle to quads.
 *
 * TODO
 * - convert triangles to any sided faces, not just quads.
 */

#include "MEM_guardedalloc.h"

#include "DNA_meshdata_types.h"

#include "BLI_math.h"
#include "BLI_sort_utils.h"

#include "BKE_customdata.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */

#define FACE_OUT (1 << 0)

/* assumes edges are validated before reaching this poin */
static float measure_facepair(const float v1[3], const float v2[3],
                              const float v3[3], const float v4[3], float limit)
{
	/* gives a 'weight' to a pair of triangles that join an edge to decide how good a join they would make */
	/* Note: this is more complicated than it needs to be and should be cleaned up.. */
	float n1[3], n2[3], measure = 0.0f, angle1, angle2, diff;
	float edgeVec1[3], edgeVec2[3], edgeVec3[3], edgeVec4[3];
	float minarea, maxarea, areaA, areaB;

	/* First Test: Normal difference */
	normal_tri_v3(n1, v1, v2, v3);
	normal_tri_v3(n2, v1, v3, v4);
	angle1 = (compare_v3v3(n1, n2, FLT_EPSILON)) ? 0.0f : angle_normalized_v3v3(n1, n2);

	normal_tri_v3(n1, v2, v3, v4);
	normal_tri_v3(n2, v4, v1, v2);
	angle2 = (compare_v3v3(n1, n2, FLT_EPSILON)) ? 0.0f : angle_normalized_v3v3(n1, n2);

	measure += (angle1 + angle2) * 0.5f;
	if (measure > limit) {
		return measure;
	}

	/* Second test: Colinearity */
	sub_v3_v3v3(edgeVec1, v1, v2);
	sub_v3_v3v3(edgeVec2, v2, v3);
	sub_v3_v3v3(edgeVec3, v3, v4);
	sub_v3_v3v3(edgeVec4, v4, v1);

	normalize_v3(edgeVec1);
	normalize_v3(edgeVec2);
	normalize_v3(edgeVec3);
	normalize_v3(edgeVec4);

	/* a completely skinny face is 'pi' after halving */
	diff = 0.25f * (fabsf(angle_normalized_v3v3(edgeVec1, edgeVec2) - (float)M_PI_2) +
	                fabsf(angle_normalized_v3v3(edgeVec2, edgeVec3) - (float)M_PI_2) +
	                fabsf(angle_normalized_v3v3(edgeVec3, edgeVec4) - (float)M_PI_2) +
	                fabsf(angle_normalized_v3v3(edgeVec4, edgeVec1) - (float)M_PI_2));

	if (!diff) {
		return 0.0;
	}

	measure +=  diff;
	if (measure > limit) {
		return measure;
	}

	/* Third test: Concavity */
	areaA = area_tri_v3(v1, v2, v3) + area_tri_v3(v1, v3, v4);
	areaB = area_tri_v3(v2, v3, v4) + area_tri_v3(v4, v1, v2);

	if (areaA <= areaB) minarea = areaA;
	else minarea = areaB;

	if (areaA >= areaB) maxarea = areaA;
	else maxarea = areaB;

	if (!maxarea) measure += 1;
	else measure += (1 - (minarea / maxarea));

	return measure;
}

#define T2QUV_LIMIT 0.005f
#define T2QCOL_LIMIT 3

static bool bm_edge_faces_cmp(BMesh *bm, BMEdge *e, const bool do_uv, const bool do_tf, const bool do_vcol)
{
	/* first get loops */
	BMLoop *l[4];

	l[0] = e->l;
	l[2] = e->l->radial_next;
	
	/* match up loops on each side of an edge corresponding to each vert */
	if (l[0]->v == l[2]->v) {
		l[1] = l[0]->next;
		l[3] = l[1]->next;
	}
	else {
		l[1] = l[0]->next;

		l[3] = l[2];
		l[2] = l[3]->next;
	}

	/* Test UV's */
	if (do_uv) {
		const MLoopUV *luv[4] = {
		    CustomData_bmesh_get(&bm->ldata, l[0]->head.data, CD_MLOOPUV),
		    CustomData_bmesh_get(&bm->ldata, l[1]->head.data, CD_MLOOPUV),
		    CustomData_bmesh_get(&bm->ldata, l[2]->head.data, CD_MLOOPUV),
		    CustomData_bmesh_get(&bm->ldata, l[3]->head.data, CD_MLOOPUV),
		};

		/* do UV */
		if (luv[0] && (!compare_v2v2(luv[0]->uv, luv[2]->uv, T2QUV_LIMIT) ||
		               !compare_v2v2(luv[1]->uv, luv[3]->uv, T2QUV_LIMIT)))
		{
			return false;
		}
	}

	if (do_tf) {
		const MTexPoly *tp[2] = {
		    CustomData_bmesh_get(&bm->pdata, l[0]->f->head.data, CD_MTEXPOLY),
		    CustomData_bmesh_get(&bm->pdata, l[1]->f->head.data, CD_MTEXPOLY),
		};

		if (tp[0] && (tp[0]->tpage != tp[1]->tpage)) {
			return false;
		}
	}

	/* Test Vertex Colors */
	if (do_vcol) {
		const MLoopCol *lcol[4] = {
		    CustomData_bmesh_get(&bm->ldata, l[0]->head.data, CD_MLOOPCOL),
			CustomData_bmesh_get(&bm->ldata, l[1]->head.data, CD_MLOOPCOL),
			CustomData_bmesh_get(&bm->ldata, l[2]->head.data, CD_MLOOPCOL),
			CustomData_bmesh_get(&bm->ldata, l[3]->head.data, CD_MLOOPCOL),
		};

		if (lcol[0]) {
			if (!compare_rgb_uchar((unsigned char *)&lcol[0]->r, (unsigned char *)&lcol[2]->r, T2QCOL_LIMIT) ||
			    !compare_rgb_uchar((unsigned char *)&lcol[1]->r, (unsigned char *)&lcol[3]->r, T2QCOL_LIMIT))
			{
				return false;
			}
		}
	}

	return true;
}


#define EDGE_MARK	1
#define EDGE_CHOSEN	2

#define FACE_MARK	1
#define FACE_INPUT	2



void bmo_join_triangles_exec(BMesh *bm, BMOperator *op)
{
	const bool do_sharp = BMO_slot_bool_get(op->slots_in, "cmp_sharp");
	const bool do_uv    = BMO_slot_bool_get(op->slots_in, "cmp_uvs");
	const bool do_tf    = do_uv;  /* texture face, make make its own option eventually */
	const bool do_vcol  = BMO_slot_bool_get(op->slots_in, "cmp_vcols");
	const bool do_mat   = BMO_slot_bool_get(op->slots_in, "cmp_materials");
	const float limit   = BMO_slot_float_get(op->slots_in, "limit");

	BMIter iter;
	BMOIter siter;
	BMFace *f;
	BMEdge *e, *e_next;
	/* data: edge-to-join, sort_value: error weight */
	struct SortPointerByFloat *jedges;
	unsigned i, totedge;
	unsigned int totedge_tag = 0;

	/* flag all edges of all input face */
	BMO_ITER (f, &siter, op->slots_in, "faces", BM_FACE) {
		if (f->len == 3) {
			BMO_elem_flag_enable(bm, f, FACE_INPUT);
		}
	}

	/* flag edges surrounded by 2 flagged triangles */
	BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
		BMFace *f_a, *f_b;
		if (BM_edge_face_pair(e, &f_a, &f_b) &&
		    (BMO_elem_flag_test(bm, f_a, FACE_INPUT) && BMO_elem_flag_test(bm, f_b, FACE_INPUT)))
		{
			BMO_elem_flag_enable(bm, e, EDGE_MARK);
			totedge_tag++;
		}
	}

	if (totedge_tag == 0) {
		return;
	}

	/* over alloc, some of the edges will be delimited */
	jedges = MEM_mallocN(sizeof(*jedges) * totedge_tag, __func__);

	i = 0;
	BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
		BMVert *v1, *v2, *v3, *v4;
		BMFace *f_a, *f_b;
		float measure;

		if (!BMO_elem_flag_test(bm, e, EDGE_MARK))
			continue;

		f_a = e->l->f;
		f_b = e->l->radial_next->f;

		if (do_sharp && !BM_elem_flag_test(e, BM_ELEM_SMOOTH))
			continue;

		if (do_mat && f_a->mat_nr != f_b->mat_nr)
			continue;

		if ((do_uv || do_tf || do_vcol) && (bm_edge_faces_cmp(bm, e, do_uv, do_tf, do_vcol) == false))
			continue;

		v1 = e->l->v;
		v2 = e->l->prev->v;
		v3 = e->l->next->v;
		v4 = e->l->radial_next->prev->v;

		measure = measure_facepair(v1->co, v2->co, v3->co, v4->co, limit);
		if (measure < limit) {
			jedges[i].data = e;
			jedges[i].sort_value = measure;
			i++;
		}
	}

	totedge = i;
	qsort(jedges, totedge, sizeof(*jedges), BLI_sortutil_cmp_float);

	for (i = 0; i < totedge; i++) {
		BMFace *f_a, *f_b;

		e = jedges[i].data;
		f_a = e->l->f;
		f_b = e->l->radial_next->f;

		/* check if another edge already claimed this face */
		if ((BMO_elem_flag_test(bm, f_a, FACE_MARK) == false) ||
		    (BMO_elem_flag_test(bm, f_b, FACE_MARK) == false))
		{
			BMO_elem_flag_enable(bm, f_a, FACE_MARK);
			BMO_elem_flag_enable(bm, f_b, FACE_MARK);
			BMO_elem_flag_enable(bm, e, EDGE_CHOSEN);
		}
	}

	MEM_freeN(jedges);

	/* join best weighted */
	BM_ITER_MESH_MUTABLE (e, e_next, &iter, bm, BM_EDGES_OF_MESH) {
		BMFace *f_new;
		BMFace *f_a, *f_b;

		if (!BMO_elem_flag_test(bm, e, EDGE_CHOSEN))
			continue;

		BM_edge_face_pair(e, &f_a, &f_b); /* checked above */
		if ((f_a->len == 3 && f_b->len == 3)) {
			f_new = BM_faces_join_pair(bm, f_a, f_b, e, true);
			if (f_new) {
				BMO_elem_flag_enable(bm, f_new, FACE_OUT);
			}
		}
	}

	/* join 2-tri islands */
	BM_ITER_MESH_MUTABLE (e, e_next, &iter, bm, BM_EDGES_OF_MESH) {
		if (BMO_elem_flag_test(bm, e, EDGE_MARK)) {
			BMLoop *l_a, *l_b;
			BMFace *f_a, *f_b;

			/* ok, this edge wasn't merged, check if it's
			 * in a 2-tri-pair island, and if so merge */
			l_a = e->l;
			l_b = e->l->radial_next;

			f_a = l_a->f;
			f_b = l_b->f;
			
			/* check the other 2 edges in both tris are untagged */
			if ((f_a->len == 3 && f_b->len == 3) &&
			    (BMO_elem_flag_test(bm, l_a->next->e, EDGE_MARK) == false) &&
			    (BMO_elem_flag_test(bm, l_a->prev->e, EDGE_MARK) == false) &&
			    (BMO_elem_flag_test(bm, l_b->next->e, EDGE_MARK) == false) &&
			    (BMO_elem_flag_test(bm, l_b->prev->e, EDGE_MARK) == false) &&
			    /* check for faces that use same verts, this is supported but raises an error
			     * and cancels the operation when performed from editmode, since this is only
			     * two triangles we only need to compare a single vertex */
			    (LIKELY(l_a->prev->v != l_b->prev->v)))
			{
				BMFace *f_new;
				f_new = BM_faces_join_pair(bm, f_a, f_b, e, true);
				if (f_new) {
					BMO_elem_flag_enable(bm, f_new, FACE_OUT);
				}
			}
		}
	}

	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "faces.out", BM_FACE, FACE_OUT);
}
