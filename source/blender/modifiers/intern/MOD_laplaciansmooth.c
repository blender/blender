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


#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_deform.h"
#include "BKE_modifier.h"

#include "MOD_util.h"

#ifdef WITH_OPENNL

#include "ONL_opennl.h"

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
	int numFaces;           /* Number of faces*/
	int numVerts;           /* Number of verts*/
	short *numNeFa;         /* Number of neighboors faces around vertice*/
	short *numNeEd;         /* Number of neighboors Edges around vertice*/
	short *zerola;          /* Is zero area or length*/

	/* Pointers to data*/
	float (*vertexCos)[3];
	MFace *mfaces;
	MEdge *medges;
	NLContext *context;

	/*Data*/
	float min_area;
	float vert_centroid[3];
};
typedef struct BLaplacianSystem LaplacianSystem;

static CustomDataMask required_data_mask(Object *ob, ModifierData *md);
static bool is_disabled(ModifierData *md, int useRenderParams);
static float average_area_quad_v3(float *v1, float *v2, float *v3, float *v4);
static float compute_volume(float (*vertexCos)[3], MFace *mfaces, int numFaces);
static LaplacianSystem *init_laplacian_system(int a_numEdges, int a_numFaces, int a_numVerts);
static void copy_data(ModifierData *md, ModifierData *target);
static void delete_laplacian_system(LaplacianSystem *sys);
static void delete_void_pointer(void *data);
static void fill_laplacian_matrix(LaplacianSystem *sys);
static void init_data(ModifierData *md);
static void init_laplacian_matrix(LaplacianSystem *sys);
static void memset_laplacian_system(LaplacianSystem *sys, int val);
static void volume_preservation(LaplacianSystem *sys, float vini, float vend, short flag);
static void validate_solution(LaplacianSystem *sys, short flag, float lambda, float lambda_border);

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
	delete_void_pointer(sys->numNeEd);
	delete_void_pointer(sys->numNeFa);
	delete_void_pointer(sys->ring_areas);
	delete_void_pointer(sys->vlengths);
	delete_void_pointer(sys->vweights);
	delete_void_pointer(sys->zerola);
	if (sys->context) {
		nlDeleteContext(sys->context);
	}
	sys->vertexCos = NULL;
	sys->mfaces = NULL;
	sys->medges = NULL;
	MEM_freeN(sys);
}

static void memset_laplacian_system(LaplacianSystem *sys, int val)
{
	memset(sys->eweights,     val, sizeof(float) * sys->numEdges);
	memset(sys->fweights,     val, sizeof(float) * sys->numFaces * 3);
	memset(sys->numNeEd,      val, sizeof(short) * sys->numVerts);
	memset(sys->numNeFa,      val, sizeof(short) * sys->numVerts);
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

	sys->numNeEd =  MEM_callocN(sizeof(short) * sys->numVerts, "ModLaplSmoothNumNeEd");
	if (!sys->numNeEd) {
		delete_laplacian_system(sys);
		return NULL;
	}

	sys->numNeFa =  MEM_callocN(sizeof(short) * sys->numVerts, "ModLaplSmoothNumNeFa");
	if (!sys->numNeFa) {
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

static float average_area_quad_v3(float *v1, float *v2, float *v3, float *v4)
{
	float areaq;
	areaq = area_tri_v3(v1, v2, v3) + area_tri_v3(v1, v2, v4) + area_tri_v3(v1, v3, v4);
	return areaq / 2.0f;
}

static float compute_volume(float (*vertexCos)[3], MFace *mfaces, int numFaces)
{
	float vol = 0.0f;
	float x1, y1, z1, x2, y2, z2, x3, y3, z3, x4, y4, z4;
	int i;
	float *vf[4];
	for (i = 0; i < numFaces; i++) {
		vf[0] = vertexCos[mfaces[i].v1];
		vf[1] = vertexCos[mfaces[i].v2];
		vf[2] = vertexCos[mfaces[i].v3];

		x1 = vf[0][0];
		y1 = vf[0][1];
		z1 = vf[0][2];

		x2 = vf[1][0];
		y2 = vf[1][1];
		z2 = vf[1][2];

		x3 = vf[2][0];
		y3 = vf[2][1];
		z3 = vf[2][2];


		vol +=  (1.0f / 6.0f) * (x2 * y3 * z1 + x3 * y1 * z2 - x1 * y3 * z2 - x2 * y1 * z3 + x1 * y2 * z3 - x3 * y2 * z1);
		if ((&mfaces[i])->v4) {
			vf[3] = vertexCos[mfaces[i].v4];
			x4 = vf[3][0];
			y4 = vf[3][1];
			z4 = vf[3][2];
			vol += (1.0f / 6.0f) * (x1 * y3 * z4 - x1 * y4 * z3 - x3 * y1 * z4 + x3 * z1 * y4 + y1 * x4 * z3 - x4 * y3 * z1);
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
	float *v1, *v2, *v3, *v4;
	float w1, w2, w3, w4;
	float areaf;
	int i, j;
	unsigned int idv1, idv2, idv3, idv4, idv[4];
	bool has_4_vert;
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
	for (i = 0; i < sys->numFaces; i++) {
		has_4_vert = ((&sys->mfaces[i])->v4) ? 1 : 0;

		idv1 = sys->mfaces[i].v1;
		idv2 = sys->mfaces[i].v2;
		idv3 = sys->mfaces[i].v3;
		idv4 = has_4_vert ? sys->mfaces[i].v4 : 0;

		sys->numNeFa[idv1] += 1;
		sys->numNeFa[idv2] += 1;
		sys->numNeFa[idv3] += 1;
		if (has_4_vert) sys->numNeFa[idv4] += 1;

		v1 = sys->vertexCos[idv1];
		v2 = sys->vertexCos[idv2];
		v3 = sys->vertexCos[idv3];
		v4 = has_4_vert ? sys->vertexCos[idv4] : NULL;

		if (has_4_vert) {
			areaf = area_quad_v3(v1, v2, v3, sys->vertexCos[sys->mfaces[i].v4]);
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

		if (has_4_vert) {
			sys->ring_areas[idv1] += average_area_quad_v3(v1, v2, v3, v4);
			sys->ring_areas[idv2] += average_area_quad_v3(v2, v3, v4, v1);
			sys->ring_areas[idv3] += average_area_quad_v3(v3, v4, v1, v2);
			sys->ring_areas[idv4] += average_area_quad_v3(v4, v1, v2, v3);
		}
		else {
			sys->ring_areas[idv1] += areaf;
			sys->ring_areas[idv2] += areaf;
			sys->ring_areas[idv3] += areaf;
		}

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

				v1 = sys->vertexCos[idv1];
				v2 = sys->vertexCos[idv2];
				v3 = sys->vertexCos[idv3];
				v4 = sys->vertexCos[idv4];

				w2 = cotangent_tri_weight_v3(v4, v1, v2) + cotangent_tri_weight_v3(v3, v1, v2);
				w3 = cotangent_tri_weight_v3(v2, v3, v1) + cotangent_tri_weight_v3(v4, v1, v3);
				w4 = cotangent_tri_weight_v3(v2, v4, v1) + cotangent_tri_weight_v3(v3, v4, v1);

				sys->vweights[idv1] += (w2 + w3 + w4) / 4.0f;
			}
		}
		else {
			w1 = cotangent_tri_weight_v3(v1, v2, v3) / 2.0f;
			w2 = cotangent_tri_weight_v3(v2, v3, v1) / 2.0f;
			w3 = cotangent_tri_weight_v3(v3, v1, v2) / 2.0f;

			sys->fweights[i][0] = sys->fweights[i][0] + w1;
			sys->fweights[i][1] = sys->fweights[i][1] + w2;
			sys->fweights[i][2] = sys->fweights[i][2] + w3;

			sys->vweights[idv1] = sys->vweights[idv1] + w2 + w3;
			sys->vweights[idv2] = sys->vweights[idv2] + w1 + w3;
			sys->vweights[idv3] = sys->vweights[idv3] + w1 + w2;
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
	float *v1, *v2, *v3, *v4;
	float w2, w3, w4;
	int i, j;
	bool has_4_vert;
	unsigned int idv1, idv2, idv3, idv4, idv[4];

	for (i = 0; i < sys->numFaces; i++) {
		idv1 = sys->mfaces[i].v1;
		idv2 = sys->mfaces[i].v2;
		idv3 = sys->mfaces[i].v3;
		has_4_vert = ((&sys->mfaces[i])->v4) ? 1 : 0;

		if (has_4_vert) {
			idv[0] = sys->mfaces[i].v1;
			idv[1] = sys->mfaces[i].v2;
			idv[2] = sys->mfaces[i].v3;
			idv[3] = sys->mfaces[i].v4;
			for (j = 0; j < 4; j++) {
				idv1 = idv[j];
				idv2 = idv[(j + 1) % 4];
				idv3 = idv[(j + 2) % 4];
				idv4 = idv[(j + 3) % 4];

				v1 = sys->vertexCos[idv1];
				v2 = sys->vertexCos[idv2];
				v3 = sys->vertexCos[idv3];
				v4 = sys->vertexCos[idv4];

				w2 = cotangent_tri_weight_v3(v4, v1, v2) + cotangent_tri_weight_v3(v3, v1, v2);
				w3 = cotangent_tri_weight_v3(v2, v3, v1) + cotangent_tri_weight_v3(v4, v1, v3);
				w4 = cotangent_tri_weight_v3(v2, v4, v1) + cotangent_tri_weight_v3(v3, v4, v1);

				w2 = w2 / 4.0f;
				w3 = w3 / 4.0f;
				w4 = w4 / 4.0f;

				if (sys->numNeEd[idv1] == sys->numNeFa[idv1] && sys->zerola[idv1] == 0) {
					nlMatrixAdd(idv1, idv2, w2 * sys->vweights[idv1]);
					nlMatrixAdd(idv1, idv3, w3 * sys->vweights[idv1]);
					nlMatrixAdd(idv1, idv4, w4 * sys->vweights[idv1]);
				}
			}
		}
		else {
			/* Is ring if number of faces == number of edges around vertice*/
			if (sys->numNeEd[idv1] == sys->numNeFa[idv1] && sys->zerola[idv1] == 0) {
				nlMatrixAdd(idv1, idv2, sys->fweights[i][2] * sys->vweights[idv1]);
				nlMatrixAdd(idv1, idv3, sys->fweights[i][1] * sys->vweights[idv1]);
			}
			if (sys->numNeEd[idv2] == sys->numNeFa[idv2] && sys->zerola[idv2] == 0) {
				nlMatrixAdd(idv2, idv1, sys->fweights[i][2] * sys->vweights[idv2]);
				nlMatrixAdd(idv2, idv3, sys->fweights[i][0] * sys->vweights[idv2]);
			}
			if (sys->numNeEd[idv3] == sys->numNeFa[idv3] && sys->zerola[idv3] == 0) {
				nlMatrixAdd(idv3, idv1, sys->fweights[i][1] * sys->vweights[idv3]);
				nlMatrixAdd(idv3, idv2, sys->fweights[i][0] * sys->vweights[idv3]);
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
			nlMatrixAdd(idv1, idv2, sys->eweights[i] * sys->vlengths[idv1]);
			nlMatrixAdd(idv2, idv1, sys->eweights[i] * sys->vlengths[idv2]);
		}
	}
}

static void validate_solution(LaplacianSystem *sys, short flag, float lambda, float lambda_border)
{
	int i;
	float lam;
	float vini, vend;

	if (flag & MOD_LAPLACIANSMOOTH_PRESERVE_VOLUME) {
		vini = compute_volume(sys->vertexCos, sys->mfaces, sys->numFaces);
	}
	for (i = 0; i < sys->numVerts; i++) {
		if (sys->zerola[i] == 0) {
			lam = sys->numNeEd[i] == sys->numNeFa[i] ? (lambda >= 0.0f ? 1.0f : -1.0f) : (lambda_border >= 0.0f ? 1.0f : -1.0f);
			if (flag & MOD_LAPLACIANSMOOTH_X) {
				sys->vertexCos[i][0] += lam * (nlGetVariable(0, i) - sys->vertexCos[i][0]);
			}
			if (flag & MOD_LAPLACIANSMOOTH_Y) {
				sys->vertexCos[i][1] += lam * (nlGetVariable(1, i) - sys->vertexCos[i][1]);
			}
			if (flag & MOD_LAPLACIANSMOOTH_Z) {
				sys->vertexCos[i][2] += lam * (nlGetVariable(2, i) - sys->vertexCos[i][2]);
			}
		}
	}
	if (flag & MOD_LAPLACIANSMOOTH_PRESERVE_VOLUME) {
		vend = compute_volume(sys->vertexCos, sys->mfaces, sys->numFaces);
		volume_preservation(sys, vini, vend, flag);
	}
}

static void laplaciansmoothModifier_do(
        LaplacianSmoothModifierData *smd, Object *ob, DerivedMesh *dm,
        float (*vertexCos)[3], int numVerts)
{
	LaplacianSystem *sys;
	MDeformVert *dvert = NULL;
	MDeformVert *dv = NULL;
	float w, wpaint;
	int i, iter;
	int defgrp_index;

	DM_ensure_tessface(dm);

	sys = init_laplacian_system(dm->getNumEdges(dm), dm->getNumTessFaces(dm), numVerts);
	if (!sys) {
		return;
	}

	sys->mfaces = dm->getTessFaceArray(dm);
	sys->medges = dm->getEdgeArray(dm);
	sys->vertexCos = vertexCos;
	sys->min_area = 0.00001f;
	modifier_get_vgroup(ob, dm, smd->defgrp_name, &dvert, &defgrp_index);

	sys->vert_centroid[0] = 0.0f;
	sys->vert_centroid[1] = 0.0f;
	sys->vert_centroid[2] = 0.0f;
	memset_laplacian_system(sys, 0);

#ifdef OPENNL_THREADING_HACK
	modifier_opennl_lock();
#endif

	nlNewContext();
	sys->context = nlGetCurrent();
	nlSolverParameteri(NL_NB_VARIABLES, numVerts);
	nlSolverParameteri(NL_LEAST_SQUARES, NL_TRUE);
	nlSolverParameteri(NL_NB_ROWS, numVerts);
	nlSolverParameteri(NL_NB_RIGHT_HAND_SIDES, 3);

	init_laplacian_matrix(sys);

	for (iter = 0; iter < smd->repeat; iter++) {
		nlBegin(NL_SYSTEM);
		for (i = 0; i < numVerts; i++) {
			nlSetVariable(0, i, vertexCos[i][0]);
			nlSetVariable(1, i, vertexCos[i][1]);
			nlSetVariable(2, i, vertexCos[i][2]);
			if (iter == 0) {
				add_v3_v3(sys->vert_centroid, vertexCos[i]);
			}
		}
		if (iter == 0 && numVerts > 0) {
			mul_v3_fl(sys->vert_centroid, 1.0f / (float)numVerts);
		}

		nlBegin(NL_MATRIX);
		dv = dvert;
		for (i = 0; i < numVerts; i++) {
			nlRightHandSideSet(0, i, vertexCos[i][0]);
			nlRightHandSideSet(1, i, vertexCos[i][1]);
			nlRightHandSideSet(2, i, vertexCos[i][2]);
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
							nlMatrixAdd(i, i,  1.0f + fabsf(smd->lambda) * wpaint);
						}
						else {
							nlMatrixAdd(i, i,  1.0f + fabsf(smd->lambda_border) * wpaint * 2.0f);
						}
					}
					else {
						w = sys->vweights[i] * sys->ring_areas[i];
						sys->vweights[i] = (w == 0.0f) ? 0.0f : -fabsf(smd->lambda) * wpaint / (4.0f * w);
						w = sys->vlengths[i];
						sys->vlengths[i] = (w == 0.0f) ? 0.0f : -fabsf(smd->lambda_border) * wpaint * 2.0f / w;

						if (sys->numNeEd[i] == sys->numNeFa[i]) {
							nlMatrixAdd(i, i,  1.0f + fabsf(smd->lambda) * wpaint / (4.0f * sys->ring_areas[i]));
						}
						else {
							nlMatrixAdd(i, i,  1.0f + fabsf(smd->lambda_border) * wpaint * 2.0f);
						}
					}
				}
				else {
					nlMatrixAdd(i, i, 1.0f);
				}
			}
		}

		if (iter == 0) {
			fill_laplacian_matrix(sys);
		}

		nlEnd(NL_MATRIX);
		nlEnd(NL_SYSTEM);

		if (nlSolveAdvanced(NULL, NL_TRUE)) {
			validate_solution(sys, smd->flag, smd->lambda, smd->lambda_border);
		}
	}
	nlDeleteContext(sys->context);
	sys->context = NULL;

#ifdef OPENNL_THREADING_HACK
	modifier_opennl_unlock();
#endif

	delete_laplacian_system(sys);
}

#else  /* WITH_OPENNL */
static void laplaciansmoothModifier_do(
        LaplacianSmoothModifierData *smd, Object *ob, DerivedMesh *dm,
        float (*vertexCos)[3], int numVerts)
{
	UNUSED_VARS(smd, ob, dm, vertexCos, numVerts);
}
#endif  /* WITH_OPENNL */

static void init_data(ModifierData *md)
{
	LaplacianSmoothModifierData *smd = (LaplacianSmoothModifierData *) md;
	smd->lambda = 0.01f;
	smd->lambda_border = 0.01f;
	smd->repeat = 1;
	smd->flag = MOD_LAPLACIANSMOOTH_X | MOD_LAPLACIANSMOOTH_Y | MOD_LAPLACIANSMOOTH_Z | MOD_LAPLACIANSMOOTH_PRESERVE_VOLUME | MOD_LAPLACIANSMOOTH_NORMALIZED;
	smd->defgrp_name[0] = '\0';
}

static void copy_data(ModifierData *md, ModifierData *target)
{
#if 0
	LaplacianSmoothModifierData *smd = (LaplacianSmoothModifierData *) md;
	LaplacianSmoothModifierData *tsmd = (LaplacianSmoothModifierData *) target;
#endif

	modifier_copyData_generic(md, target);
}

static bool is_disabled(ModifierData *md, int UNUSED(useRenderParams))
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

static void deformVerts(ModifierData *md, Object *ob, DerivedMesh *derivedData,
                        float (*vertexCos)[3], int numVerts, ModifierApplyFlag UNUSED(flag))
{
	DerivedMesh *dm;

	if (numVerts == 0)
		return;

	dm = get_dm(ob, NULL, derivedData, NULL, false, false);

	laplaciansmoothModifier_do((LaplacianSmoothModifierData *)md, ob, dm,
	                           vertexCos, numVerts);

	if (dm != derivedData)
		dm->release(dm);
}

static void deformVertsEM(
        ModifierData *md, Object *ob, struct BMEditMesh *editData,
        DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts)
{
	DerivedMesh *dm;

	if (numVerts == 0)
		return;

	dm = get_dm(ob, editData, derivedData, NULL, false, false);

	laplaciansmoothModifier_do((LaplacianSmoothModifierData *)md, ob, dm,
	                           vertexCos, numVerts);

	if (dm != derivedData)
		dm->release(dm);
}


ModifierTypeInfo modifierType_LaplacianSmooth = {
	/* name */              "Laplacian Smooth",
	/* structName */        "LaplacianSmoothModifierData",
	/* structSize */        sizeof(LaplacianSmoothModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_SupportsEditmode,

	/* copy_data */         copy_data,
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
	/* updateDepgraph */    NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
