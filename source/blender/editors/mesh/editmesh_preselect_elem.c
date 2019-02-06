/*
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
 */

/** \file \ingroup edmesh
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "BKE_editmesh.h"

#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "DNA_object_types.h"

#include "ED_mesh.h"
#include "ED_view3d.h"

/* -------------------------------------------------------------------- */
/** \name Mesh Element Pre-Select
 * Public API:
 *
 * #EDBM_preselect_elem_create
 * #EDBM_preselect_elem_destroy
 * #EDBM_preselect_elem_clear
 * #EDBM_preselect_elem_draw
 * #EDBM_preselect_elem_update_from_single
 *
 * \{ */

static void vcos_get(BMVert *v, float r_co[3], const float (*coords)[3])
{
	if (coords) {
		copy_v3_v3(r_co, coords[BM_elem_index_get(v)]);
	}
	else {
		copy_v3_v3(r_co, v->co);
	}
}

static void vcos_get_pair(BMVert *v[2], float r_cos[2][3], const float (*coords)[3])
{
	if (coords) {
		for (int j = 0; j < 2; j++) {
			copy_v3_v3(r_cos[j], coords[BM_elem_index_get(v[j])]);
		}
	}
	else {
		for (int j = 0; j < 2; j++) {
			copy_v3_v3(r_cos[j], v[j]->co);
		}
	}
}

struct EditMesh_PreSelElem {
	float (*edges)[2][3];
	int     edges_len;

	float (*verts)[3];
	int     verts_len;
};

struct EditMesh_PreSelElem *EDBM_preselect_elem_create(void)
{
	struct EditMesh_PreSelElem *psel = MEM_callocN(sizeof(*psel), __func__);
	return psel;
}

void EDBM_preselect_elem_destroy(
        struct EditMesh_PreSelElem *psel)
{
	EDBM_preselect_elem_clear(psel);
	MEM_freeN(psel);
}

void EDBM_preselect_elem_clear(
        struct EditMesh_PreSelElem *psel)
{
	MEM_SAFE_FREE(psel->edges);
	psel->edges_len = 0;

	MEM_SAFE_FREE(psel->verts);
	psel->verts_len = 0;
}

void EDBM_preselect_elem_draw(
        struct EditMesh_PreSelElem *psel, const float matrix[4][4])
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

static void view3d_preselect_mesh_elem_update_from_vert(
        struct EditMesh_PreSelElem *psel,
        BMesh *UNUSED(bm), BMVert *eve, const float (*coords)[3])
{
	float (*verts)[3] = MEM_mallocN(sizeof(*psel->verts), __func__);
	vcos_get(eve, verts[0], coords);
	psel->verts = verts;
	psel->verts_len = 1;
}

static void view3d_preselect_mesh_elem_update_from_edge(
        struct EditMesh_PreSelElem *psel,
        BMesh *UNUSED(bm), BMEdge *eed, const float (*coords)[3])
{
	float (*edges)[2][3] = MEM_mallocN(sizeof(*psel->edges), __func__);
	vcos_get_pair(&eed->v1, edges[0], coords);
	psel->edges = edges;
	psel->edges_len = 1;
}

static void view3d_preselect_mesh_elem_update_from_face(
        struct EditMesh_PreSelElem *psel,
        BMesh *UNUSED(bm), BMFace *efa, const float (*coords)[3])
{
	float (*edges)[2][3] = MEM_mallocN(sizeof(*psel->edges) * efa->len, __func__);
	BMLoop *l_iter, *l_first;
	l_iter = l_first = BM_FACE_FIRST_LOOP(efa);
	int i = 0;
	do {
		vcos_get_pair(&l_iter->e->v1, edges[i++], coords);
	} while ((l_iter = l_iter->next) != l_first);
	psel->edges = edges;
	psel->edges_len = efa->len;
}

void EDBM_preselect_elem_update_from_single(
        struct EditMesh_PreSelElem *psel,
        BMesh *bm, BMElem *ele,
        const float (*coords)[3])
{
	EDBM_preselect_elem_clear(psel);

	if (coords) {
		BM_mesh_elem_index_ensure(bm, BM_VERT);
	}

	switch (ele->head.htype) {
		case BM_VERT:
			view3d_preselect_mesh_elem_update_from_vert(psel, bm, (BMVert *)ele, coords);
			break;
		case BM_EDGE:
			view3d_preselect_mesh_elem_update_from_edge(psel, bm, (BMEdge *)ele, coords);
			break;
		case BM_FACE:
			view3d_preselect_mesh_elem_update_from_face(psel, bm, (BMFace *)ele, coords);
			break;
		default:
			BLI_assert(0);
	}
}

/** \} */
