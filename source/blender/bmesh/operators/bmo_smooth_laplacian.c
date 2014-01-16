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
 * Contributor(s): Alexander Pinzon
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/operators/bmo_smooth_laplacian.c
 *  \ingroup bmesh
 *
 * Advanced smoothing.
 */

#include "MEM_guardedalloc.h"


#include "BLI_math.h"


#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */

#ifdef WITH_OPENNL

#include "ONL_opennl.h"

// #define SMOOTH_LAPLACIAN_AREA_FACTOR 4.0f  /* UNUSED */
// #define SMOOTH_LAPLACIAN_EDGE_FACTOR 2.0f  /* UNUSED */
#define SMOOTH_LAPLACIAN_MAX_EDGE_PERCENTAGE 1.8f
#define SMOOTH_LAPLACIAN_MIN_EDGE_PERCENTAGE 0.15f

struct BLaplacianSystem {
	float *eweights;        /* Length weights per Edge */
	float (*fweights)[3];   /* Cotangent weights per face */
	float *ring_areas;      /* Total area per ring*/
	float *vlengths;        /* Total sum of lengths(edges) per vertice*/
	float *vweights;        /* Total sum of weights per vertice*/
	int numEdges;           /* Number of edges*/
	int numFaces;           /* Number of faces*/
	int numVerts;           /* Number of verts*/
	short *zerola;          /* Is zero area or length*/

	/* Pointers to data*/
	BMesh *bm;
	BMOperator *op;
	NLContext *context;

	/*Data*/
	float min_area;
};
typedef struct BLaplacianSystem LaplacianSystem;

static float cotan_weight(float *v1, float *v2, float *v3);
static int vert_is_boundary(BMVert *v);
static LaplacianSystem *init_laplacian_system(int a_numEdges, int a_numFaces, int a_numVerts);
static void init_laplacian_matrix(LaplacianSystem *sys);
static void delete_laplacian_system(LaplacianSystem *sys);
static void delete_void_pointer(void *data);
static void fill_laplacian_matrix(LaplacianSystem *sys);
static void memset_laplacian_system(LaplacianSystem *sys, int val);
static void validate_solution(LaplacianSystem *sys, int usex, int usey, int usez, int preserve_volume);
static void volume_preservation(BMOperator *op, float vini, float vend, int usex, int usey, int usez);

static void delete_void_pointer(void *data)
{
	if (data) {
		MEM_freeN(data);
	}
}

static void delete_laplacian_system(LaplacianSystem *sys)
{
	delete_void_pointer(sys->eweights);
	delete_void_pointer(sys->fweights);
	delete_void_pointer(sys->ring_areas);
	delete_void_pointer(sys->vlengths);
	delete_void_pointer(sys->vweights);
	delete_void_pointer(sys->zerola);
	if (sys->context) {
		nlDeleteContext(sys->context);
	}
	sys->bm = NULL;
	sys->op = NULL;
	MEM_freeN(sys);
}

static void memset_laplacian_system(LaplacianSystem *sys, int val)
{
	memset(sys->eweights,     val, sizeof(float) * sys->numEdges);
	memset(sys->fweights,     val, sizeof(float) * sys->numFaces * 3);
	memset(sys->ring_areas,   val, sizeof(float) * sys->numVerts);
	memset(sys->vlengths,     val, sizeof(float) * sys->numVerts);
	memset(sys->vweights,     val, sizeof(float) * sys->numVerts);
	memset(sys->zerola,       val, sizeof(short) * sys->numVerts);
}

static LaplacianSystem *init_laplacian_system(int a_numEdges, int a_numFaces, int a_numVerts)
{
	LaplacianSystem *sys;
	sys = MEM_callocN(sizeof(LaplacianSystem), "ModLaplSmoothSystem");
	sys->numEdges = a_numEdges;
	sys->numFaces = a_numFaces;
	sys->numVerts = a_numVerts;

	sys->eweights =  MEM_callocN(sizeof(float) * sys->numEdges, "ModLaplSmoothEWeight");
	if (!sys->eweights) {
		delete_laplacian_system(sys);
		return NULL;
	}

	sys->fweights =  MEM_callocN(sizeof(float) * 3 * sys->numFaces, "ModLaplSmoothFWeight");
	if (!sys->fweights) {
		delete_laplacian_system(sys);
		return NULL;
	}

	sys->ring_areas =  MEM_callocN(sizeof(float) * sys->numVerts, "ModLaplSmoothRingAreas");
	if (!sys->ring_areas) {
		delete_laplacian_system(sys);
		return NULL;
	}

	sys->vlengths =  MEM_callocN(sizeof(float) * sys->numVerts, "ModLaplSmoothVlengths");
	if (!sys->vlengths) {
		delete_laplacian_system(sys);
		return NULL;
	}

	sys->vweights =  MEM_callocN(sizeof(float) * sys->numVerts, "ModLaplSmoothVweights");
	if (!sys->vweights) {
		delete_laplacian_system(sys);
		return NULL;
	}

	sys->zerola =  MEM_callocN(sizeof(short) * sys->numVerts, "ModLaplSmoothZeloa");
	if (!sys->zerola) {
		delete_laplacian_system(sys);
		return NULL;
	}

	return sys;
}

/* Compute weigth between vertice v_i and all your neighbors
 * weight between v_i and v_neighbor
 * Wij = cot(alpha) + cot(beta) / (4.0 * total area of all faces  * sum all weight)
 *        v_i *
 *          / | \
 *         /  |  \
 *  v_beta*   |   * v_alpha
 *         \  |  /
 *          \ | /
 *            * v_neighbor
 */

static void init_laplacian_matrix(LaplacianSystem *sys)
{
	float areaf;
	float *v1, *v2, *v3, *v4;
	float w1, w2, w3, w4;
	int i, j;
	bool has_4_vert;
	unsigned int idv1, idv2, idv3, idv4, idv[4];
	BMEdge *e;
	BMFace *f;
	BMIter eiter;
	BMIter fiter;
	BMIter vi;
	BMVert *vn;
	BMVert *vf[4];

	BM_ITER_MESH_INDEX (e, &eiter, sys->bm, BM_EDGES_OF_MESH, j) {
		if (!BM_elem_flag_test(e, BM_ELEM_SELECT) && BM_edge_is_boundary(e)) {
			v1 = e->v1->co;
			v2 =  e->v2->co;
			idv1 = BM_elem_index_get(e->v1);
			idv2 = BM_elem_index_get(e->v2);

			w1 = len_v3v3(v1, v2);
			if (w1 > sys->min_area) {
				w1 = 1.0f / w1;
				i = BM_elem_index_get(e);
				sys->eweights[i] = w1;
				sys->vlengths[idv1] += w1;
				sys->vlengths[idv2] += w1;
			}
			else {
				sys->zerola[idv1] = 1;
				sys->zerola[idv2] = 1;
			}
		}
	}

	BM_ITER_MESH (f, &fiter, sys->bm, BM_FACES_OF_MESH) {
		if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {

			BM_ITER_ELEM_INDEX (vn, &vi, f, BM_VERTS_OF_FACE, i) {
				vf[i] = vn;
			}
			has_4_vert = (i == 4) ? 1 : 0;
			idv1 = BM_elem_index_get(vf[0]);
			idv2 = BM_elem_index_get(vf[1]);
			idv3 = BM_elem_index_get(vf[2]);
			idv4 = has_4_vert ? BM_elem_index_get(vf[3]) : 0;

			v1 = vf[0]->co;
			v2 = vf[1]->co;
			v3 = vf[2]->co;
			v4 = has_4_vert ? vf[3]->co : NULL;

			if (has_4_vert) {
				areaf = area_quad_v3(v1, v2, v3, v4);
			}
			else {
				areaf = area_tri_v3(v1, v2, v3);
			}

			if (fabsf(areaf) < sys->min_area) {
				sys->zerola[idv1] = 1;
				sys->zerola[idv2] = 1;
				sys->zerola[idv3] = 1;
				if (has_4_vert) sys->zerola[idv4] = 1;
			}

			sys->ring_areas[idv1] += areaf;
			sys->ring_areas[idv2] += areaf;
			sys->ring_areas[idv3] += areaf;
			if (has_4_vert) sys->ring_areas[idv4] += areaf;

			if (has_4_vert) {

				idv[0] = idv1;
				idv[1] = idv2;
				idv[2] = idv3;
				idv[3] = idv4;

				for (j = 0; j < 4; j++) {
					idv1 = idv[j];
					idv2 = idv[(j + 1) % 4];
					idv3 = idv[(j + 2) % 4];
					idv4 = idv[(j + 3) % 4];

					v1 = vf[j]->co;
					v2 = vf[(j + 1) % 4]->co;
					v3 = vf[(j + 2) % 4]->co;
					v4 = vf[(j + 3) % 4]->co;

					w2 = cotan_weight(v4, v1, v2) + cotan_weight(v3, v1, v2);
					w3 = cotan_weight(v2, v3, v1) + cotan_weight(v4, v1, v3);
					w4 = cotan_weight(v2, v4, v1) + cotan_weight(v3, v4, v1);

					sys->vweights[idv1] += (w2 + w3 + w4) / 4.0f;
				}
			}
			else {
				i = BM_elem_index_get(f);

				w1 = cotan_weight(v1, v2, v3);
				w2 = cotan_weight(v2, v3, v1);
				w3 = cotan_weight(v3, v1, v2);

				sys->fweights[i][0] += w1;
				sys->fweights[i][1] += w2;
				sys->fweights[i][2] += w3;

				sys->vweights[idv1] += w2 + w3;
				sys->vweights[idv2] += w1 + w3;
				sys->vweights[idv3] += w1 + w2;
			}
		}
	}
}

static void fill_laplacian_matrix(LaplacianSystem *sys)
{
	float *v1, *v2, *v3, *v4;
	float w2, w3, w4;
	int i, j;
	bool has_4_vert;
	unsigned int idv1, idv2, idv3, idv4, idv[4];

	BMEdge *e;
	BMFace *f;
	BMIter eiter;
	BMIter fiter;
	BMIter vi;
	BMVert *vn;
	BMVert *vf[4];

	BM_ITER_MESH (f, &fiter, sys->bm, BM_FACES_OF_MESH) {
		if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
			BM_ITER_ELEM_INDEX (vn, &vi, f, BM_VERTS_OF_FACE, i) {
				vf[i] = vn;
			}
			has_4_vert = (i == 4) ? 1 : 0;
			if (has_4_vert) {
				idv[0] = BM_elem_index_get(vf[0]);
				idv[1] = BM_elem_index_get(vf[1]);
				idv[2] = BM_elem_index_get(vf[2]);
				idv[3] = BM_elem_index_get(vf[3]);
				for (j = 0; j < 4; j++) {
					idv1 = idv[j];
					idv2 = idv[(j + 1) % 4];
					idv3 = idv[(j + 2) % 4];
					idv4 = idv[(j + 3) % 4];

					v1 = vf[j]->co;
					v2 = vf[(j + 1) % 4]->co;
					v3 = vf[(j + 2) % 4]->co;
					v4 = vf[(j + 3) % 4]->co;

					w2 = cotan_weight(v4, v1, v2) + cotan_weight(v3, v1, v2);
					w3 = cotan_weight(v2, v3, v1) + cotan_weight(v4, v1, v3);
					w4 = cotan_weight(v2, v4, v1) + cotan_weight(v3, v4, v1);

					w2 = w2 / 4.0f;
					w3 = w3 / 4.0f;
					w4 = w4 / 4.0f;

					if (!vert_is_boundary(vf[j]) && sys->zerola[idv1] == 0) {
						nlMatrixAdd(idv1, idv2, w2 * sys->vweights[idv1]);
						nlMatrixAdd(idv1, idv3, w3 * sys->vweights[idv1]);
						nlMatrixAdd(idv1, idv4, w4 * sys->vweights[idv1]);
					}
				}
			}
			else {
				idv1 = BM_elem_index_get(vf[0]);
				idv2 = BM_elem_index_get(vf[1]);
				idv3 = BM_elem_index_get(vf[2]);
				/* Is ring if number of faces == number of edges around vertice*/
				i = BM_elem_index_get(f);
				if (!vert_is_boundary(vf[0]) && sys->zerola[idv1] == 0) {
					nlMatrixAdd(idv1, idv2, sys->fweights[i][2] * sys->vweights[idv1]);
					nlMatrixAdd(idv1, idv3, sys->fweights[i][1] * sys->vweights[idv1]);
				}
				if (!vert_is_boundary(vf[1]) && sys->zerola[idv2] == 0) {
					nlMatrixAdd(idv2, idv1, sys->fweights[i][2] * sys->vweights[idv2]);
					nlMatrixAdd(idv2, idv3, sys->fweights[i][0] * sys->vweights[idv2]);
				}
				if (!vert_is_boundary(vf[2]) && sys->zerola[idv3] == 0) {
					nlMatrixAdd(idv3, idv1, sys->fweights[i][1] * sys->vweights[idv3]);
					nlMatrixAdd(idv3, idv2, sys->fweights[i][0] * sys->vweights[idv3]);
				}
			}
		}
	}
	BM_ITER_MESH (e, &eiter, sys->bm, BM_EDGES_OF_MESH) {
		if (!BM_elem_flag_test(e, BM_ELEM_SELECT) && BM_edge_is_boundary(e)) {
			v1 = e->v1->co;
			v2 =  e->v2->co;
			idv1 = BM_elem_index_get(e->v1);
			idv2 = BM_elem_index_get(e->v2);
			if (sys->zerola[idv1] == 0 && sys->zerola[idv2] == 0) {
				i = BM_elem_index_get(e);
				nlMatrixAdd(idv1, idv2, sys->eweights[i] * sys->vlengths[idv1]);
				nlMatrixAdd(idv2, idv1, sys->eweights[i] * sys->vlengths[idv2]);
			}
		}
	}
}

static float cotan_weight(float *v1, float *v2, float *v3)
{
	float a[3], b[3], c[3], clen;

	sub_v3_v3v3(a, v2, v1);
	sub_v3_v3v3(b, v3, v1);
	cross_v3_v3v3(c, a, b);

	clen = len_v3(c);

	if (clen == 0.0f)
		return 0.0f;

	return dot_v3v3(a, b) / clen;
}

static int vert_is_boundary(BMVert *v)
{
	BMEdge *ed;
	BMFace *f;
	BMIter ei;
	BMIter fi;
	BM_ITER_ELEM (ed, &ei, v, BM_EDGES_OF_VERT) {
		if (BM_edge_is_boundary(ed)) {
			return 1;
		}
	}
	BM_ITER_ELEM (f, &fi, v, BM_FACES_OF_VERT) {
		if (!BM_elem_flag_test(f, BM_ELEM_SELECT)) {
			return 1;
		}
	}
	return 0;
}

static void volume_preservation(BMOperator *op, float vini, float vend, int usex, int usey, int usez)
{
	float beta;
	BMOIter siter;
	BMVert *v;

	if (vend != 0.0f) {
		beta  = pow(vini / vend, 1.0f / 3.0f);
		BMO_ITER (v, &siter, op->slots_in, "verts", BM_VERT) {
			if (usex) {
				v->co[0] *= beta;
			}
			if (usey) {
				v->co[1] *= beta;
			}
			if (usez) {
				v->co[2] *= beta;
			}

		}
	}
}

static void validate_solution(LaplacianSystem *sys, int usex, int usey, int usez, int preserve_volume)
{
	int m_vertex_id;
	float leni, lene;
	float vini, vend;
	float *vi1, *vi2, ve1[3], ve2[3];
	unsigned int idv1, idv2;
	BMOIter siter;
	BMVert *v;
	BMEdge *e;
	BMIter eiter;

	BM_ITER_MESH  (e, &eiter, sys->bm, BM_EDGES_OF_MESH) {
		idv1 = BM_elem_index_get(e->v1);
		idv2 = BM_elem_index_get(e->v2);
		vi1 = e->v1->co;
		vi2 =  e->v2->co;
		ve1[0] = nlGetVariable(0, idv1);
		ve1[1] = nlGetVariable(1, idv1);
		ve1[2] = nlGetVariable(2, idv1);
		ve2[0] = nlGetVariable(0, idv2);
		ve2[1] = nlGetVariable(1, idv2);
		ve2[2] = nlGetVariable(2, idv2);
		leni = len_v3v3(vi1, vi2);
		lene = len_v3v3(ve1, ve2);
		if (lene > leni * SMOOTH_LAPLACIAN_MAX_EDGE_PERCENTAGE || lene < leni * SMOOTH_LAPLACIAN_MIN_EDGE_PERCENTAGE) {
			sys->zerola[idv1] = 1;
			sys->zerola[idv2] = 1;
		}
	}

	if (preserve_volume) {
		vini = BM_mesh_calc_volume(sys->bm, false);
	}
	BMO_ITER (v, &siter, sys->op->slots_in, "verts", BM_VERT) {
		m_vertex_id = BM_elem_index_get(v);
		if (sys->zerola[m_vertex_id] == 0) {
			if (usex) {
				v->co[0] =  nlGetVariable(0, m_vertex_id);
			}
			if (usey) {
				v->co[1] =  nlGetVariable(1, m_vertex_id);
			}
			if (usez) {
				v->co[2] =  nlGetVariable(2, m_vertex_id);
			}
		}
	}
	if (preserve_volume) {
		vend = BM_mesh_calc_volume(sys->bm, false);
		volume_preservation(sys->op, vini, vend, usex, usey, usez);
	}

}

void bmo_smooth_laplacian_vert_exec(BMesh *bm, BMOperator *op)
{
	int i;
	int m_vertex_id;
	bool usex, usey, usez, preserve_volume;
	float lambda_factor, lambda_border;
	float w;
	BMOIter siter;
	BMVert *v;
	LaplacianSystem *sys;

	if (bm->totface == 0) return;
	sys = init_laplacian_system(bm->totedge, bm->totface, bm->totvert);
	if (!sys) return;
	sys->bm = bm;
	sys->op = op;

	memset_laplacian_system(sys, 0);

	BM_mesh_elem_index_ensure(bm, BM_VERT);
	lambda_factor = BMO_slot_float_get(op->slots_in, "lambda_factor");
	lambda_border = BMO_slot_float_get(op->slots_in, "lambda_border");
	sys->min_area = 0.00001f;
	usex = BMO_slot_bool_get(op->slots_in, "use_x");
	usey = BMO_slot_bool_get(op->slots_in, "use_y");
	usez = BMO_slot_bool_get(op->slots_in, "use_z");
	preserve_volume = BMO_slot_bool_get(op->slots_in, "preserve_volume");


	nlNewContext();
	sys->context = nlGetCurrent();

	nlSolverParameteri(NL_NB_VARIABLES, bm->totvert);
	nlSolverParameteri(NL_LEAST_SQUARES, NL_TRUE);
	nlSolverParameteri(NL_NB_ROWS, bm->totvert);
	nlSolverParameteri(NL_NB_RIGHT_HAND_SIDES, 3);

	nlBegin(NL_SYSTEM);
	for (i = 0; i < bm->totvert; i++) {
		nlLockVariable(i);
	}
	BMO_ITER (v, &siter, op->slots_in, "verts", BM_VERT) {
		m_vertex_id = BM_elem_index_get(v);
		nlUnlockVariable(m_vertex_id);
		nlSetVariable(0, m_vertex_id, v->co[0]);
		nlSetVariable(1, m_vertex_id, v->co[1]);
		nlSetVariable(2, m_vertex_id, v->co[2]);
	}

	nlBegin(NL_MATRIX);
	init_laplacian_matrix(sys);
	BMO_ITER (v, &siter, op->slots_in, "verts", BM_VERT) {
		m_vertex_id = BM_elem_index_get(v);
		nlRightHandSideAdd(0, m_vertex_id, v->co[0]);
		nlRightHandSideAdd(1, m_vertex_id, v->co[1]);
		nlRightHandSideAdd(2, m_vertex_id, v->co[2]);
		i = m_vertex_id;
		if (sys->zerola[i] == 0) {
			w = sys->vweights[i] * sys->ring_areas[i];
			sys->vweights[i] = (w == 0.0f) ? 0.0f : -lambda_factor  / (4.0f * w);
			w = sys->vlengths[i];
			sys->vlengths[i] = (w == 0.0f) ? 0.0f : -lambda_border  * 2.0f / w;

			if (!vert_is_boundary(v)) {
				nlMatrixAdd(i, i,  1.0f + lambda_factor / (4.0f * sys->ring_areas[i]));
			}
			else {
				nlMatrixAdd(i, i,  1.0f + lambda_border * 2.0f);
			}
		}
		else {
			nlMatrixAdd(i, i, 1.0f);
		}
	}
	fill_laplacian_matrix(sys);

	nlEnd(NL_MATRIX);
	nlEnd(NL_SYSTEM);

	if (nlSolveAdvanced(NULL, NL_TRUE) ) {
		validate_solution(sys, usex, usey, usez, preserve_volume);
	}

	delete_laplacian_system(sys);
}

#else  /* WITH_OPENNL */

#ifdef __GNUC__
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

void bmo_smooth_laplacian_vert_exec(BMesh *bm, BMOperator *op) {}

#endif  /* WITH_OPENNL */
