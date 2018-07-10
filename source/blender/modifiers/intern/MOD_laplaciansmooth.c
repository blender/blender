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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 by the Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Alexander Pinzon
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/modifiers/intern/MOD_laplaciansmooth.c
 *  \ingroup modifiers
 */


#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include "BKE_deform.h"
#include "BKE_editmesh.h"
#include "BKE_library.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"

#include "MOD_util.h"

#include "eigen_capi.h"

#if 0
#define MOD_LAPLACIANSMOOTH_MAX_EDGE_PERCENTAGE 1.8f
#define MOD_LAPLACIANSMOOTH_MIN_EDGE_PERCENTAGE 0.02f
#endif

struct BLaplacianSystem {
	float *eweights;        /* Length weights per Edge */
	float (*fweights)[3];   /* Cotangent weights per face */
	float *ring_areas;      /* Total area per ring*/
	float *vlengths;        /* Total sum of lengths(edges) per vertice*/
	float *vweights;        /* Total sum of weights per vertice*/
	int numEdges;           /* Number of edges*/
	int numLoops;           /* Number of edges*/
	int numPolys;           /* Number of faces*/
	int numVerts;           /* Number of verts*/
	short *numNeFa;         /* Number of neighboors faces around vertice*/
	short *numNeEd;         /* Number of neighboors Edges around vertice*/
	short *zerola;          /* Is zero area or length*/

	/* Pointers to data*/
	float (*vertexCos)[3];
	const MPoly *mpoly;
	const MLoop *mloop;
	const MEdge *medges;
	LinearSolver *context;

	/*Data*/
	float min_area;
	float vert_centroid[3];
};
typedef struct BLaplacianSystem LaplacianSystem;

static CustomDataMask required_data_mask(Object *ob, ModifierData *md);
static bool is_disabled(const struct Scene *UNUSED(scene), ModifierData *md, bool useRenderParams);
static float compute_volume(const float center[3], float (*vertexCos)[3], const MPoly *mpoly, int numPolys, const MLoop *mloop);
static LaplacianSystem *init_laplacian_system(int a_numEdges, int a_numPolys, int a_numLoops, int a_numVerts);
static void delete_laplacian_system(LaplacianSystem *sys);
static void fill_laplacian_matrix(LaplacianSystem *sys);
static void init_data(ModifierData *md);
static void init_laplacian_matrix(LaplacianSystem *sys);
static void memset_laplacian_system(LaplacianSystem *sys, int val);
static void volume_preservation(LaplacianSystem *sys, float vini, float vend, short flag);
static void validate_solution(LaplacianSystem *sys, short flag, float lambda, float lambda_border);

static void delete_laplacian_system(LaplacianSystem *sys)
{
	MEM_SAFE_FREE(sys->eweights);
	MEM_SAFE_FREE(sys->fweights);
	MEM_SAFE_FREE(sys->numNeEd);
	MEM_SAFE_FREE(sys->numNeFa);
	MEM_SAFE_FREE(sys->ring_areas);
	MEM_SAFE_FREE(sys->vlengths);
	MEM_SAFE_FREE(sys->vweights);
	MEM_SAFE_FREE(sys->zerola);

	if (sys->context) {
		EIG_linear_solver_delete(sys->context);
	}
	sys->vertexCos = NULL;
	sys->mpoly = NULL;
	sys->mloop = NULL;
	sys->medges = NULL;
	MEM_freeN(sys);
}

static void memset_laplacian_system(LaplacianSystem *sys, int val)
{
	memset(sys->eweights,     val, sizeof(float) * sys->numEdges);
	memset(sys->fweights,     val, sizeof(float[3]) * sys->numLoops);
	memset(sys->numNeEd,      val, sizeof(short) * sys->numVerts);
	memset(sys->numNeFa,      val, sizeof(short) * sys->numVerts);
	memset(sys->ring_areas,   val, sizeof(float) * sys->numVerts);
	memset(sys->vlengths,     val, sizeof(float) * sys->numVerts);
	memset(sys->vweights,     val, sizeof(float) * sys->numVerts);
	memset(sys->zerola,       val, sizeof(short) * sys->numVerts);
}

static LaplacianSystem *init_laplacian_system(int a_numEdges, int a_numPolys, int a_numLoops, int a_numVerts)
{
	LaplacianSystem *sys;
	sys = MEM_callocN(sizeof(LaplacianSystem), "ModLaplSmoothSystem");
	sys->numEdges = a_numEdges;
	sys->numPolys = a_numPolys;
	sys->numLoops = a_numLoops;
	sys->numVerts = a_numVerts;

	sys->eweights =  MEM_calloc_arrayN(sys->numEdges, sizeof(float), __func__);
	sys->fweights =  MEM_calloc_arrayN(sys->numLoops, sizeof(float[3]), __func__);
	sys->numNeEd =  MEM_calloc_arrayN(sys->numVerts, sizeof(short), __func__);
	sys->numNeFa =  MEM_calloc_arrayN(sys->numVerts, sizeof(short), __func__);
	sys->ring_areas =  MEM_calloc_arrayN(sys->numVerts, sizeof(float), __func__);
	sys->vlengths =  MEM_calloc_arrayN(sys->numVerts, sizeof(float), __func__);
	sys->vweights =  MEM_calloc_arrayN(sys->numVerts, sizeof(float), __func__);
	sys->zerola =  MEM_calloc_arrayN(sys->numVerts, sizeof(short), __func__);

	return sys;
}

static float compute_volume(
        const float center[3], float (*vertexCos)[3],
        const MPoly *mpoly, int numPolys, const MLoop *mloop)
{
	int i;
	float vol = 0.0f;

	for (i = 0; i < numPolys; i++) {
		const MPoly *mp = &mpoly[i];
		const MLoop *l_first = &mloop[mp->loopstart];
		const MLoop *l_prev = l_first + 1;
		const MLoop *l_curr = l_first + 2;
		const MLoop *l_term = l_first + mp->totloop;


		for (;
		     l_curr != l_term;
		     l_prev = l_curr, l_curr++)
		{
			vol += volume_tetrahedron_signed_v3(
			        center,
			        vertexCos[l_first->v],
			        vertexCos[l_prev->v],
			        vertexCos[l_curr->v]);
		}
	}

	return fabsf(vol);
}

static void volume_preservation(LaplacianSystem *sys, float vini, float vend, short flag)
{
	float beta;
	int i;

	if (vend != 0.0f) {
		beta  = pow(vini / vend, 1.0f / 3.0f);
		for (i = 0; i < sys->numVerts; i++) {
			if (flag & MOD_LAPLACIANSMOOTH_X) {
				sys->vertexCos[i][0] = (sys->vertexCos[i][0] - sys->vert_centroid[0]) * beta + sys->vert_centroid[0];
			}
			if (flag & MOD_LAPLACIANSMOOTH_Y) {
				sys->vertexCos[i][1] = (sys->vertexCos[i][1] - sys->vert_centroid[1]) * beta + sys->vert_centroid[1];
			}
			if (flag & MOD_LAPLACIANSMOOTH_Z) {
				sys->vertexCos[i][2] = (sys->vertexCos[i][2] - sys->vert_centroid[2]) * beta + sys->vert_centroid[2];
			}

		}
	}
}

static void init_laplacian_matrix(LaplacianSystem *sys)
{
	float *v1, *v2;
	float w1, w2, w3;
	float areaf;
	int i;
	unsigned int idv1, idv2;

	for (i = 0; i < sys->numEdges; i++) {
		idv1 = sys->medges[i].v1;
		idv2 = sys->medges[i].v2;

		v1 = sys->vertexCos[idv1];
		v2 = sys->vertexCos[idv2];

		sys->numNeEd[idv1] = sys->numNeEd[idv1] + 1;
		sys->numNeEd[idv2] = sys->numNeEd[idv2] + 1;
		w1 = len_v3v3(v1, v2);
		if (w1 < sys->min_area) {
			sys->zerola[idv1] = 1;
			sys->zerola[idv2] = 1;
		}
		else {
			w1 = 1.0f / w1;
		}

		sys->eweights[i] = w1;
	}

	for (i = 0; i < sys->numPolys; i++) {
		const MPoly *mp = &sys->mpoly[i];
		const MLoop *l_next = &sys->mloop[mp->loopstart];
		const MLoop *l_term = l_next + mp->totloop;
		const MLoop *l_prev = l_term - 2;
		const MLoop *l_curr = l_term - 1;

		for (;
		     l_next != l_term;
		     l_prev = l_curr, l_curr = l_next, l_next++)
		{
			const float *v_prev = sys->vertexCos[l_prev->v];
			const float *v_curr = sys->vertexCos[l_curr->v];
			const float *v_next = sys->vertexCos[l_next->v];
			const unsigned int l_curr_index = l_curr - sys->mloop;

			sys->numNeFa[l_curr->v] += 1;

			areaf = area_tri_v3(v_prev, v_curr, v_next);

			if (areaf < sys->min_area) {
				sys->zerola[l_curr->v] = 1;
			}

			sys->ring_areas[l_prev->v] += areaf;
			sys->ring_areas[l_curr->v] += areaf;
			sys->ring_areas[l_next->v] += areaf;

			w1 = cotangent_tri_weight_v3(v_curr, v_next, v_prev) / 2.0f;
			w2 = cotangent_tri_weight_v3(v_next, v_prev, v_curr) / 2.0f;
			w3 = cotangent_tri_weight_v3(v_prev, v_curr, v_next) / 2.0f;

			sys->fweights[l_curr_index][0] += w1;
			sys->fweights[l_curr_index][1] += w2;
			sys->fweights[l_curr_index][2] += w3;

			sys->vweights[l_curr->v] += w2 + w3;
			sys->vweights[l_next->v] += w1 + w3;
			sys->vweights[l_prev->v] += w1 + w2;
		}
	}
	for (i = 0; i < sys->numEdges; i++) {
		idv1 = sys->medges[i].v1;
		idv2 = sys->medges[i].v2;
		/* if is boundary, apply scale-dependent umbrella operator only with neighboors in boundary */
		if (sys->numNeEd[idv1] != sys->numNeFa[idv1] && sys->numNeEd[idv2] != sys->numNeFa[idv2]) {
			sys->vlengths[idv1] += sys->eweights[i];
			sys->vlengths[idv2] += sys->eweights[i];
		}
	}

}

static void fill_laplacian_matrix(LaplacianSystem *sys)
{
	int i;
	unsigned int idv1, idv2;

	for (i = 0; i < sys->numPolys; i++) {
		const MPoly *mp = &sys->mpoly[i];
		const MLoop *l_next = &sys->mloop[mp->loopstart];
		const MLoop *l_term = l_next + mp->totloop;
		const MLoop *l_prev = l_term - 2;
		const MLoop *l_curr = l_term - 1;

		for (;
		     l_next != l_term;
		     l_prev = l_curr, l_curr = l_next, l_next++)
		{
			const unsigned int l_curr_index = l_curr - sys->mloop;

			/* Is ring if number of faces == number of edges around vertice*/
			if (sys->numNeEd[l_curr->v] == sys->numNeFa[l_curr->v] && sys->zerola[l_curr->v] == 0) {
				EIG_linear_solver_matrix_add(sys->context, l_curr->v, l_next->v, sys->fweights[l_curr_index][2] * sys->vweights[l_curr->v]);
				EIG_linear_solver_matrix_add(sys->context, l_curr->v, l_prev->v, sys->fweights[l_curr_index][1] * sys->vweights[l_curr->v]);
			}
			if (sys->numNeEd[l_next->v] == sys->numNeFa[l_next->v] && sys->zerola[l_next->v] == 0) {
				EIG_linear_solver_matrix_add(sys->context, l_next->v, l_curr->v, sys->fweights[l_curr_index][2] * sys->vweights[l_next->v]);
				EIG_linear_solver_matrix_add(sys->context, l_next->v, l_prev->v, sys->fweights[l_curr_index][0] * sys->vweights[l_next->v]);
			}
			if (sys->numNeEd[l_prev->v] == sys->numNeFa[l_prev->v] && sys->zerola[l_prev->v] == 0) {
				EIG_linear_solver_matrix_add(sys->context, l_prev->v, l_curr->v, sys->fweights[l_curr_index][1] * sys->vweights[l_prev->v]);
				EIG_linear_solver_matrix_add(sys->context, l_prev->v, l_next->v, sys->fweights[l_curr_index][0] * sys->vweights[l_prev->v]);
			}
		}
	}

	for (i = 0; i < sys->numEdges; i++) {
		idv1 = sys->medges[i].v1;
		idv2 = sys->medges[i].v2;
		/* Is boundary */
		if (sys->numNeEd[idv1] != sys->numNeFa[idv1] &&
		    sys->numNeEd[idv2] != sys->numNeFa[idv2] &&
		    sys->zerola[idv1] == 0 &&
		    sys->zerola[idv2] == 0)
		{
			EIG_linear_solver_matrix_add(sys->context, idv1, idv2, sys->eweights[i] * sys->vlengths[idv1]);
			EIG_linear_solver_matrix_add(sys->context, idv2, idv1, sys->eweights[i] * sys->vlengths[idv2]);
		}
	}
}

static void validate_solution(LaplacianSystem *sys, short flag, float lambda, float lambda_border)
{
	int i;
	float lam;
	float vini = 0.0f, vend = 0.0f;

	if (flag & MOD_LAPLACIANSMOOTH_PRESERVE_VOLUME) {
		vini = compute_volume(sys->vert_centroid, sys->vertexCos, sys->mpoly, sys->numPolys, sys->mloop);
	}
	for (i = 0; i < sys->numVerts; i++) {
		if (sys->zerola[i] == 0) {
			lam = sys->numNeEd[i] == sys->numNeFa[i] ? (lambda >= 0.0f ? 1.0f : -1.0f) : (lambda_border >= 0.0f ? 1.0f : -1.0f);
			if (flag & MOD_LAPLACIANSMOOTH_X) {
				sys->vertexCos[i][0] += lam * ((float)EIG_linear_solver_variable_get(sys->context, 0, i) - sys->vertexCos[i][0]);
			}
			if (flag & MOD_LAPLACIANSMOOTH_Y) {
				sys->vertexCos[i][1] += lam * ((float)EIG_linear_solver_variable_get(sys->context, 1, i) - sys->vertexCos[i][1]);
			}
			if (flag & MOD_LAPLACIANSMOOTH_Z) {
				sys->vertexCos[i][2] += lam * ((float)EIG_linear_solver_variable_get(sys->context, 2, i) - sys->vertexCos[i][2]);
			}
		}
	}
	if (flag & MOD_LAPLACIANSMOOTH_PRESERVE_VOLUME) {
		vend = compute_volume(sys->vert_centroid, sys->vertexCos, sys->mpoly, sys->numPolys, sys->mloop);
		volume_preservation(sys, vini, vend, flag);
	}
}

static void laplaciansmoothModifier_do(
        LaplacianSmoothModifierData *smd, Object *ob, Mesh *mesh,
        float (*vertexCos)[3], int numVerts)
{
	LaplacianSystem *sys;
	MDeformVert *dvert = NULL;
	MDeformVert *dv = NULL;
	float w, wpaint;
	int i, iter;
	int defgrp_index;

	sys = init_laplacian_system(mesh->totedge, mesh->totpoly, mesh->totloop, numVerts);
	if (!sys) {
		return;
	}

	sys->mpoly = mesh->mpoly;
	sys->mloop = mesh->mloop;
	sys->medges = mesh->medge;
	sys->vertexCos = vertexCos;
	sys->min_area = 0.00001f;
	MOD_get_vgroup(ob, mesh, smd->defgrp_name, &dvert, &defgrp_index);

	sys->vert_centroid[0] = 0.0f;
	sys->vert_centroid[1] = 0.0f;
	sys->vert_centroid[2] = 0.0f;
	memset_laplacian_system(sys, 0);

	sys->context = EIG_linear_least_squares_solver_new(numVerts, numVerts, 3);

	init_laplacian_matrix(sys);

	for (iter = 0; iter < smd->repeat; iter++) {
		for (i = 0; i < numVerts; i++) {
			EIG_linear_solver_variable_set(sys->context, 0, i, vertexCos[i][0]);
			EIG_linear_solver_variable_set(sys->context, 1, i, vertexCos[i][1]);
			EIG_linear_solver_variable_set(sys->context, 2, i, vertexCos[i][2]);
			if (iter == 0) {
				add_v3_v3(sys->vert_centroid, vertexCos[i]);
			}
		}
		if (iter == 0 && numVerts > 0) {
			mul_v3_fl(sys->vert_centroid, 1.0f / (float)numVerts);
		}

		dv = dvert;
		for (i = 0; i < numVerts; i++) {
			EIG_linear_solver_right_hand_side_add(sys->context, 0, i, vertexCos[i][0]);
			EIG_linear_solver_right_hand_side_add(sys->context, 1, i, vertexCos[i][1]);
			EIG_linear_solver_right_hand_side_add(sys->context, 2, i, vertexCos[i][2]);
			if (iter == 0) {
				if (dv) {
					wpaint = defvert_find_weight(dv, defgrp_index);
					dv++;
				}
				else {
					wpaint = 1.0f;
				}

				if (sys->zerola[i] == 0) {
					if (smd->flag & MOD_LAPLACIANSMOOTH_NORMALIZED) {
						w = sys->vweights[i];
						sys->vweights[i] = (w == 0.0f) ? 0.0f : -fabsf(smd->lambda) * wpaint / w;
						w = sys->vlengths[i];
						sys->vlengths[i] = (w == 0.0f) ? 0.0f : -fabsf(smd->lambda_border) * wpaint * 2.0f / w;
						if (sys->numNeEd[i] == sys->numNeFa[i]) {
							EIG_linear_solver_matrix_add(sys->context, i, i,  1.0f + fabsf(smd->lambda) * wpaint);
						}
						else {
							EIG_linear_solver_matrix_add(sys->context, i, i,  1.0f + fabsf(smd->lambda_border) * wpaint * 2.0f);
						}
					}
					else {
						w = sys->vweights[i] * sys->ring_areas[i];
						sys->vweights[i] = (w == 0.0f) ? 0.0f : -fabsf(smd->lambda) * wpaint / (4.0f * w);
						w = sys->vlengths[i];
						sys->vlengths[i] = (w == 0.0f) ? 0.0f : -fabsf(smd->lambda_border) * wpaint * 2.0f / w;

						if (sys->numNeEd[i] == sys->numNeFa[i]) {
							EIG_linear_solver_matrix_add(sys->context, i, i,  1.0f + fabsf(smd->lambda) * wpaint / (4.0f * sys->ring_areas[i]));
						}
						else {
							EIG_linear_solver_matrix_add(sys->context, i, i,  1.0f + fabsf(smd->lambda_border) * wpaint * 2.0f);
						}
					}
				}
				else {
					EIG_linear_solver_matrix_add(sys->context, i, i, 1.0f);
				}
			}
		}

		if (iter == 0) {
			fill_laplacian_matrix(sys);
		}

		if (EIG_linear_solver_solve(sys->context)) {
			validate_solution(sys, smd->flag, smd->lambda, smd->lambda_border);
		}
	}
	EIG_linear_solver_delete(sys->context);
	sys->context = NULL;

	delete_laplacian_system(sys);
}

static void init_data(ModifierData *md)
{
	LaplacianSmoothModifierData *smd = (LaplacianSmoothModifierData *) md;
	smd->lambda = 0.01f;
	smd->lambda_border = 0.01f;
	smd->repeat = 1;
	smd->flag = MOD_LAPLACIANSMOOTH_X | MOD_LAPLACIANSMOOTH_Y | MOD_LAPLACIANSMOOTH_Z | MOD_LAPLACIANSMOOTH_PRESERVE_VOLUME | MOD_LAPLACIANSMOOTH_NORMALIZED;
	smd->defgrp_name[0] = '\0';
}

static bool is_disabled(const struct Scene *UNUSED(scene), ModifierData *md, bool UNUSED(useRenderParams))
{
	LaplacianSmoothModifierData *smd = (LaplacianSmoothModifierData *) md;
	short flag;

	flag = smd->flag & (MOD_LAPLACIANSMOOTH_X | MOD_LAPLACIANSMOOTH_Y | MOD_LAPLACIANSMOOTH_Z);

	/* disable if modifier is off for X, Y and Z or if factor is 0 */
	if (flag == 0) return 1;

	return 0;
}

static CustomDataMask required_data_mask(Object *UNUSED(ob), ModifierData *md)
{
	LaplacianSmoothModifierData *smd = (LaplacianSmoothModifierData *)md;
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups if we need them */
	if (smd->defgrp_name[0]) dataMask |= CD_MASK_MDEFORMVERT;

	return dataMask;
}

static void deformVerts(
        ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh,
        float (*vertexCos)[3], int numVerts)
{
	Mesh *mesh_src;

	if (numVerts == 0)
		return;

	mesh_src = MOD_get_mesh_eval(ctx->object, NULL, mesh, NULL, false, false);

	laplaciansmoothModifier_do((LaplacianSmoothModifierData *)md, ctx->object, mesh_src,
	                           vertexCos, numVerts);

	if (mesh_src != mesh)
		BKE_id_free(NULL, mesh_src);
}

static void deformVertsEM(
        ModifierData *md, const ModifierEvalContext *ctx, struct BMEditMesh *editData,
        Mesh *mesh, float (*vertexCos)[3], int numVerts)
{
	Mesh *mesh_src;

	if (numVerts == 0)
		return;

	mesh_src = MOD_get_mesh_eval(ctx->object, editData, mesh, NULL, false, false);

	laplaciansmoothModifier_do((LaplacianSmoothModifierData *)md, ctx->object, mesh_src,
	                           vertexCos, numVerts);

	if (mesh_src != mesh)
		BKE_id_free(NULL, mesh_src);
}


ModifierTypeInfo modifierType_LaplacianSmooth = {
	/* name */              "Laplacian Smooth",
	/* structName */        "LaplacianSmoothModifierData",
	/* structSize */        sizeof(LaplacianSmoothModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_SupportsEditmode,

	/* copyData */          modifier_copyData_generic,

	/* deformVerts_DM */    NULL,
	/* deformMatrices_DM */ NULL,
	/* deformVertsEM_DM */  NULL,
	/* deformMatricesEM_DM*/NULL,
	/* applyModifier_DM */  NULL,
	/* applyModifierEM_DM */NULL,

	/* deformVerts */       deformVerts,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     deformVertsEM,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     NULL,
	/* applyModifierEM */   NULL,

	/* initData */          init_data,
	/* requiredDataMask */  required_data_mask,
	/* freeData */          NULL,
	/* isDisabled */        is_disabled,
	/* updateDepsgraph */   NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
