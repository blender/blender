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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/mesh/editmesh_preselect.c
 *  \ingroup edmesh
 */

#include "MEM_guardedalloc.h"

#include "BLI_stack.h"
#include "BLI_math.h"

#include "BKE_editmesh.h"

#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "ED_mesh.h"

/* -------------------------------------------------------------------- */
/** \name Mesh Edge Ring Pre-Select
 * Public API:
 *
 * #EDBM_preselect_edgering_create
 * #EDBM_preselect_edgering_destroy
 * #EDBM_preselect_edgering_clear
 * #EDBM_preselect_edgering_draw
 * #EDBM_preselect_edgering_update_from_edge
 *
 * \{ */

static void edgering_vcos_get(BMVert *v[2][2], float r_cos[2][2][3])
{
	/* TODO, get deformed coords. */
#if 0
	if (dm) {
		int j, k;
		for (j = 0; j < 2; j++) {
			for (k = 0; k < 2; k++) {
				dm->getVertCo(dm, BM_elem_index_get(v[j][k]), r_cos[j][k]);
			}
		}
	}
	else
#endif
	{
		int j, k;
		for (j = 0; j < 2; j++) {
			for (k = 0; k < 2; k++) {
				copy_v3_v3(r_cos[j][k], v[j][k]->co);
			}
		}
	}
}

static void edgering_vcos_get_pair(BMVert *v[2], float r_cos[2][3])
{
#if 0
	if (dm) {
		int j;
		for (j = 0; j < 2; j++) {
			dm->getVertCo(dm, BM_elem_index_get(v[j]), r_cos[j]);
		}
	}
	else
#endif
	{
		int j;
		for (j = 0; j < 2; j++) {
			copy_v3_v3(r_cos[j], v[j]->co);
		}
	}
}


/**
 * Given two opposite edges in a face, finds the ordering of their vertices so
 * that cut preview lines won't cross each other.
 */
static void edgering_find_order(
        BMEdge *eed_last, BMEdge *eed,
        BMVert *eve_last, BMVert *v[2][2])
{
	BMLoop *l = eed->l;

	/* find correct order for v[1] */
	if (!(BM_edge_in_face(eed, l->f) && BM_edge_in_face(eed_last, l->f))) {
		BMIter liter;
		BM_ITER_ELEM (l, &liter, l, BM_LOOPS_OF_LOOP) {
			if (BM_edge_in_face(eed, l->f) && BM_edge_in_face(eed_last, l->f))
				break;
		}
	}

	/* this should never happen */
	if (!l) {
		v[0][0] = eed->v1;
		v[0][1] = eed->v2;
		v[1][0] = eed_last->v1;
		v[1][1] = eed_last->v2;
		return;
	}

	BMLoop *l_other = BM_loop_other_edge_loop(l, eed->v1);
	const bool rev = (l_other == l->prev);
	while (l_other->v != eed_last->v1 && l_other->v != eed_last->v2) {
		l_other = rev ? l_other->prev : l_other->next;
	}

	if (l_other->v == eve_last) {
		v[0][0] = eed->v1;
		v[0][1] = eed->v2;
	}
	else {
		v[0][0] = eed->v2;
		v[0][1] = eed->v1;
	}
}

struct EditMesh_PreSelEdgeRing {
	float (*edges)[2][3];
	int     edges_len;

	float (*verts)[3];
	int     verts_len;
};

struct EditMesh_PreSelEdgeRing *EDBM_preselect_edgering_create(void)
{
	struct EditMesh_PreSelEdgeRing *psel = MEM_callocN(sizeof(*psel), __func__);
	return psel;
}

void EDBM_preselect_edgering_destroy(
        struct EditMesh_PreSelEdgeRing *psel)
{
	EDBM_preselect_edgering_clear(psel);
	MEM_freeN(psel);
}

void EDBM_preselect_edgering_clear(
        struct EditMesh_PreSelEdgeRing *psel)
{
	MEM_SAFE_FREE(psel->edges);
	psel->edges_len = 0;

	MEM_SAFE_FREE(psel->verts);
	psel->verts_len = 0;
}

void EDBM_preselect_edgering_draw(
        struct EditMesh_PreSelEdgeRing *psel, const float matrix[4][4])
{
	if ((psel->edges_len == 0) && (psel->verts_len == 0)) {
		return;
	}

	GPU_depth_test(false);

	GPU_matrix_push();
	GPU_matrix_mul(matrix);

	uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

	immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
	immUniformColor3ub(255, 0, 255);

	if (psel->edges_len > 0) {
		immBegin(GPU_PRIM_LINES, psel->edges_len * 2);

		for (int i = 0; i < psel->edges_len; i++) {
			immVertex3fv(pos, psel->edges[i][0]);
			immVertex3fv(pos, psel->edges[i][1]);
		}

		immEnd();
	}

	if (psel->verts_len > 0) {
		GPU_point_size(3.0f);

		immBegin(GPU_PRIM_POINTS, psel->verts_len);

		for (int i = 0; i < psel->verts_len; i++) {
			immVertex3fv(pos, psel->verts[i]);
		}

		immEnd();
	}

	immUnbindProgram();

	GPU_matrix_pop();

	/* Reset default */
	GPU_depth_test(true);
}

static void view3d_preselect_mesh_edgering_update_verts_from_edge(
        struct EditMesh_PreSelEdgeRing *psel,
        BMesh *UNUSED(bm), BMEdge *eed_start, int previewlines)
{
	float v_cos[2][3];
	float (*verts)[3];
	int i, tot = 0;

	verts = MEM_mallocN(sizeof(*psel->verts) * previewlines, __func__);

	edgering_vcos_get_pair(&eed_start->v1, v_cos);

	for (i = 1; i <= previewlines; i++) {
		const float fac = (i / ((float)previewlines + 1));
		interp_v3_v3v3(verts[tot], v_cos[0], v_cos[1], fac);
		tot++;
	}

	psel->verts = verts;
	psel->verts_len = previewlines;
}

static void view3d_preselect_mesh_edgering_update_edges_from_edge(
        struct EditMesh_PreSelEdgeRing *psel,
        BMesh *bm, BMEdge *eed_start, int previewlines)
{
	BMWalker walker;
	BMEdge *eed, *eed_last;
	BMVert *v[2][2] = {{NULL}}, *eve_last;
	float (*edges)[2][3] = NULL;
	BLI_Stack *edge_stack;

	int i, tot = 0;

	BMW_init(&walker, bm, BMW_EDGERING,
	         BMW_MASK_NOP, BMW_MASK_NOP, BMW_MASK_NOP,
	         BMW_FLAG_TEST_HIDDEN,
	         BMW_NIL_LAY);


	edge_stack = BLI_stack_new(sizeof(BMEdge *), __func__);

	eed_last = NULL;
	for (eed = eed_last = BMW_begin(&walker, eed_start); eed; eed = BMW_step(&walker)) {
		BLI_stack_push(edge_stack, &eed);
	}
	BMW_end(&walker);


	eed_start = *(BMEdge **)BLI_stack_peek(edge_stack);

	edges = MEM_mallocN(
	        (sizeof(*edges) * (BLI_stack_count(edge_stack) + (eed_last != eed_start))) * previewlines, __func__);

	eve_last = NULL;
	eed_last = NULL;

	while (!BLI_stack_is_empty(edge_stack)) {
		BLI_stack_pop(edge_stack, &eed);

		if (eed_last) {
			if (eve_last) {
				v[1][0] = v[0][0];
				v[1][1] = v[0][1];
			}
			else {
				v[1][0] = eed_last->v1;
				v[1][1] = eed_last->v2;
				eve_last  = eed_last->v1;
			}

			edgering_find_order(eed_last, eed, eve_last, v);
			eve_last = v[0][0];

			for (i = 1; i <= previewlines; i++) {
				const float fac = (i / ((float)previewlines + 1));
				float v_cos[2][2][3];

				edgering_vcos_get(v, v_cos);

				interp_v3_v3v3(edges[tot][0], v_cos[0][0], v_cos[0][1], fac);
				interp_v3_v3v3(edges[tot][1], v_cos[1][0], v_cos[1][1], fac);
				tot++;
			}
		}
		eed_last = eed;
	}

	if ((eed_last != eed_start) &&
#ifdef BMW_EDGERING_NGON
	    BM_edge_share_face_check(eed_last, eed_start)
#else
	    BM_edge_share_quad_check(eed_last, eed_start)
#endif
	    )
	{
		v[1][0] = v[0][0];
		v[1][1] = v[0][1];

		edgering_find_order(eed_last, eed_start, eve_last, v);

		for (i = 1; i <= previewlines; i++) {
			const float fac = (i / ((float)previewlines + 1));
			float v_cos[2][2][3];

			if (!v[0][0] || !v[0][1] || !v[1][0] || !v[1][1]) {
				continue;
			}

			edgering_vcos_get(v, v_cos);

			interp_v3_v3v3(edges[tot][0], v_cos[0][0], v_cos[0][1], fac);
			interp_v3_v3v3(edges[tot][1], v_cos[1][0], v_cos[1][1], fac);
			tot++;
		}
	}

	BLI_stack_free(edge_stack);

	psel->edges = edges;
	psel->edges_len = tot;
}

void EDBM_preselect_edgering_update_from_edge(
        struct EditMesh_PreSelEdgeRing *psel,
        BMesh *bm, BMEdge *eed_start, int previewlines)
{
	EDBM_preselect_edgering_clear(psel);
	if (BM_edge_is_wire(eed_start)) {
		view3d_preselect_mesh_edgering_update_verts_from_edge(psel, bm, eed_start, previewlines);
	}
	else {
		view3d_preselect_mesh_edgering_update_edges_from_edge(psel, bm, eed_start, previewlines);
	}

}

/** \} */
