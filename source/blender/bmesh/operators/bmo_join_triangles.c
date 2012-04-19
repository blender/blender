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
 */

#include "MEM_guardedalloc.h"

#include "DNA_meshdata_types.h"

#include "BKE_customdata.h"

#include "BLI_math.h"
#include "BLI_array.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */

/* Bitflags for edges */
#define T2QDELETE	1
#define T2QCOMPLEX	2
#define T2QJOIN		4

/* assumes edges are validated before reaching this poin */
static float measure_facepair(BMesh *UNUSED(bm), BMVert *v1, BMVert *v2,
                              BMVert *v3, BMVert *v4, float limit)
{
	/* gives a 'weight' to a pair of triangles that join an edge to decide how good a join they would make */
	/* Note: this is more complicated than it needs to be and should be cleaned up.. */
	float n1[3], n2[3], measure = 0.0f, angle1, angle2, diff;
	float edgeVec1[3], edgeVec2[3], edgeVec3[3], edgeVec4[3];
	float minarea, maxarea, areaA, areaB;

	/* First Test: Normal difference */
	normal_tri_v3(n1, v1->co, v2->co, v3->co);
	normal_tri_v3(n2, v1->co, v3->co, v4->co);

	if (n1[0] == n2[0] && n1[1] == n2[1] && n1[2] == n2[2]) angle1 = 0.0f;
	else angle1 = angle_v3v3(n1, n2);

	normal_tri_v3(n1, v2->co, v3->co, v4->co);
	normal_tri_v3(n2, v4->co, v1->co, v2->co);

	if (n1[0] == n2[0] && n1[1] == n2[1] && n1[2] == n2[2]) angle2 = 0.0f;
	else angle2 = angle_v3v3(n1, n2);

	measure += (angle1 + angle2) * 0.5f;
	if (measure > limit) {
		return measure;
	}

	/* Second test: Colinearity */
	sub_v3_v3v3(edgeVec1, v1->co, v2->co);
	sub_v3_v3v3(edgeVec2, v2->co, v3->co);
	sub_v3_v3v3(edgeVec3, v3->co, v4->co);
	sub_v3_v3v3(edgeVec4, v4->co, v1->co);

	/* a completely skinny face is 'pi' after halving */
	diff = 0.25f * (fabsf(angle_v3v3(edgeVec1, edgeVec2) - (float)M_PI_2) +
	                fabsf(angle_v3v3(edgeVec2, edgeVec3) - (float)M_PI_2) +
	                fabsf(angle_v3v3(edgeVec3, edgeVec4) - (float)M_PI_2) +
	                fabsf(angle_v3v3(edgeVec4, edgeVec1) - (float)M_PI_2));

	if (!diff) {
		return 0.0;
	}

	measure +=  diff;
	if (measure > limit) {
		return measure;
	}

	/* Third test: Concavity */
	areaA = area_tri_v3(v1->co, v2->co, v3->co) + area_tri_v3(v1->co, v3->co, v4->co);
	areaB = area_tri_v3(v2->co, v3->co, v4->co) + area_tri_v3(v4->co, v1->co, v2->co);

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

static int compareFaceAttribs(BMesh *bm, BMEdge *e, int douvs, int dovcols)
{
	MTexPoly *tp1, *tp2;
	MLoopCol *lcol1, *lcol2, *lcol3, *lcol4;
	MLoopUV *luv1, *luv2, *luv3, *luv4;
	BMLoop *l1, *l2, *l3, *l4;
	int mergeok_uvs = !douvs, mergeok_vcols = !dovcols;
	
	l1 = e->l;
	l3 = e->l->radial_next;
	
	/* match up loops on each side of an edge corresponding to each ver */
	if (l1->v == l3->v) {
		l2 = l1->next;
		l4 = l2->next;
	}
	else {
		l2 = l1->next;

		l4 = l3;
		l3 = l4->next;
	}

	lcol1 = CustomData_bmesh_get(&bm->ldata, l1->head.data, CD_MLOOPCOL);
	lcol2 = CustomData_bmesh_get(&bm->ldata, l1->head.data, CD_MLOOPCOL);
	lcol3 = CustomData_bmesh_get(&bm->ldata, l1->head.data, CD_MLOOPCOL);
	lcol4 = CustomData_bmesh_get(&bm->ldata, l1->head.data, CD_MLOOPCOL);

	luv1 = CustomData_bmesh_get(&bm->ldata, l1->head.data, CD_MLOOPUV);
	luv2 = CustomData_bmesh_get(&bm->ldata, l1->head.data, CD_MLOOPUV);
	luv3 = CustomData_bmesh_get(&bm->ldata, l1->head.data, CD_MLOOPUV);
	luv4 = CustomData_bmesh_get(&bm->ldata, l1->head.data, CD_MLOOPUV);

	tp1 = CustomData_bmesh_get(&bm->pdata, l1->f->head.data, CD_MTEXPOLY);
	tp2 = CustomData_bmesh_get(&bm->pdata, l2->f->head.data, CD_MTEXPOLY);

	if (!lcol1)
		mergeok_vcols = 1;

	if (!luv1)
		mergeok_uvs = 1;

	/* compare faceedges for each face attribute. Additional per face attributes can be added late */

	/* do VCOL */
	if (lcol1 && dovcols) {
		char *cols[4] = {(char *)lcol1, (char *)lcol2, (char *)lcol3, (char *)lcol4};
		int i;

		for (i = 0; i < 3; i++) {
			if (cols[0][i] + T2QCOL_LIMIT < cols[2][i] - T2QCOL_LIMIT)
				break;
			if (cols[1][i] + T2QCOL_LIMIT < cols[3][i] - T2QCOL_LIMIT)
				break;
		}

		if (i == 3)
			mergeok_vcols = 1;
	}

	/* do UV */
	if (luv1 && douvs) {
		if (tp1->tpage != tp2->tpage) {
			/* do nothing */
		}
		else {
			int i;

			for (i = 0; i < 2; i++) {
				if (luv1->uv[0] + T2QUV_LIMIT > luv3->uv[0] && luv1->uv[0] - T2QUV_LIMIT < luv3->uv[0] &&
				    luv1->uv[1] + T2QUV_LIMIT > luv3->uv[1] && luv1->uv[1] - T2QUV_LIMIT < luv3->uv[1])
				{
					if (luv2->uv[0] + T2QUV_LIMIT > luv4->uv[0] && luv2->uv[0] - T2QUV_LIMIT < luv4->uv[0] &&
					    luv2->uv[1] + T2QUV_LIMIT > luv4->uv[1] && luv2->uv[1] - T2QUV_LIMIT < luv4->uv[1])
					{
						mergeok_uvs = 1;
					}
				}
			}
		}
	}

	if (douvs == mergeok_uvs && dovcols == mergeok_vcols) {
		return TRUE;
	}

	return FALSE;
}

typedef struct JoinEdge {
	float weight;
	BMEdge *e;
} JoinEdge;

#define EDGE_MARK	1
#define EDGE_CHOSEN	2

#define FACE_MARK	1
#define FACE_INPUT	2

static int fplcmp(const void *v1, const void *v2)
{
	const JoinEdge *e1 = (JoinEdge *)v1, *e2 = (JoinEdge *)v2;

	if (e1->weight > e2->weight) return 1;
	else if (e1->weight < e2->weight) return -1;

	return 0;
}

void bmo_join_triangles_exec(BMesh *bm, BMOperator *op)
{
	BMIter iter, liter;
	BMOIter siter;
	BMFace *f1, *f2;
	BMLoop *l;
	BMEdge *e;
	BLI_array_declare(jedges);
	JoinEdge *jedges = NULL;
	int dosharp = BMO_slot_bool_get(op, "cmp_sharp");
	int douvs =   BMO_slot_bool_get(op, "cmp_uvs");
	int dovcols = BMO_slot_bool_get(op, "cmp_vcols");
	int domat =   BMO_slot_bool_get(op, "cmp_materials");
	float limit = BMO_slot_float_get(op, "limit");
	int i, totedge;

	/* flag all edges of all input face */
	BMO_ITER (f1, &siter, bm, op, "faces", BM_FACE) {
		BMO_elem_flag_enable(bm, f1, FACE_INPUT);
		BM_ITER (l, &liter, bm, BM_LOOPS_OF_FACE, f1) {
			BMO_elem_flag_enable(bm, l->e, EDGE_MARK);
		}
	}

	/* unflag edges that are invalid; e.g. aren't surrounded by triangle */
	BM_ITER (e, &iter, bm, BM_EDGES_OF_MESH, NULL) {
		if (!BMO_elem_flag_test(bm, e, EDGE_MARK))
			continue;

		if (!BM_edge_face_pair(e, &f1, &f2)) {
			BMO_elem_flag_disable(bm, e, EDGE_MARK);
			continue;
		}

		if (f1->len != 3 || f2->len != 3) {
			BMO_elem_flag_disable(bm, e, EDGE_MARK);
			continue;
		}

		if (!BMO_elem_flag_test(bm, f1, FACE_INPUT) || !BMO_elem_flag_test(bm, f2, FACE_INPUT)) {
			BMO_elem_flag_disable(bm, e, EDGE_MARK);
			continue;
		}
	}
	
	i = 0;
	BM_ITER (e, &iter, bm, BM_EDGES_OF_MESH, NULL) {
		BMVert *v1, *v2, *v3, *v4;
		BMFace *f1, *f2;
		float measure;

		if (!BMO_elem_flag_test(bm, e, EDGE_MARK))
			continue;

		f1 = e->l->f;
		f2 = e->l->radial_next->f;

		v1 = e->l->v;
		v2 = e->l->prev->v;
		v3 = e->l->next->v;
		v4 = e->l->radial_next->prev->v;

		if (dosharp && !BM_elem_flag_test(e, BM_ELEM_SMOOTH))
			continue;
		
		if ((douvs || dovcols) && compareFaceAttribs(bm, e, douvs, dovcols))
			continue;

		if (domat && f1->mat_nr != f2->mat_nr)
			continue;

		measure = measure_facepair(bm, v1, v2, v3, v4, limit);
		if (measure < limit) {
			BLI_array_growone(jedges);

			jedges[i].e = e;
			jedges[i].weight = measure;

			i++;
		}
	}

	if (!jedges)
		return;

	qsort(jedges, BLI_array_count(jedges), sizeof(JoinEdge), fplcmp);

	totedge = BLI_array_count(jedges);
	for (i = 0; i < totedge; i++) {
		BMFace *f1, *f2;

		e = jedges[i].e;
		f1 = e->l->f;
		f2 = e->l->radial_next->f;

		if (BMO_elem_flag_test(bm, f1, FACE_MARK) || BMO_elem_flag_test(bm, f2, FACE_MARK))
			continue;

		BMO_elem_flag_enable(bm, f1, FACE_MARK);
		BMO_elem_flag_enable(bm, f2, FACE_MARK);
		BMO_elem_flag_enable(bm, e, EDGE_CHOSEN);
	}

	BM_ITER (e, &iter, bm, BM_EDGES_OF_MESH, NULL) {
		if (!BMO_elem_flag_test(bm, e, EDGE_CHOSEN))
			continue;


		BM_edge_face_pair(e, &f1, &f2); /* checked above */
		BM_faces_join_pair(bm, f1, f2, e, TRUE);
	}

	BM_ITER (e, &iter, bm, BM_EDGES_OF_MESH, NULL) {
		if (BMO_elem_flag_test(bm, e, EDGE_MARK)) {
			/* ok, this edge wasn't merged, check if it's
			 * in a 2-tri-pair island, and if so merg */

			f1 = e->l->f;
			f2 = e->l->radial_next->f;
			
			if (f1->len != 3 || f2->len != 3)
				continue;

			for (i = 0; i < 2; i++) {
				BM_ITER (l, &liter, bm, BM_LOOPS_OF_FACE, i ? f2 : f1) {
					if (l->e != e && BMO_elem_flag_test(bm, l->e, EDGE_MARK)) {
						break;
					}
				}
				
				/* if l isn't NULL, we broke out of the loo */
				if (l) {
					break;
				}
			}

			/* if i isn't 2, we broke out of that loo */
			if (i != 2) {
				continue;
			}

			BM_faces_join_pair(bm, f1, f2, e, TRUE);
		}
	}

	BLI_array_free(jedges);
}
