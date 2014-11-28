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
 * Contributor(s): Daniel Dunbar
 *                 Ton Roosendaal,
 *                 Ben Batt,
 *                 Brecht Van Lommel,
 *                 Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/modifiers/intern/MOD_meshdeform.c
 *  \ingroup modifiers
 */

#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_math.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_global.h"
#include "BKE_modifier.h"
#include "BKE_deform.h"
#include "BKE_editmesh.h"

#include "depsgraph_private.h"

#include "MEM_guardedalloc.h"

#include "MOD_util.h"

#ifdef __SSE2__
#  include <emmintrin.h>
#endif

static void initData(ModifierData *md)
{
	MeshDeformModifierData *mmd = (MeshDeformModifierData *) md;

	mmd->gridsize = 5;
}

static void freeData(ModifierData *md)
{
	MeshDeformModifierData *mmd = (MeshDeformModifierData *) md;

	if (mmd->bindinfluences) MEM_freeN(mmd->bindinfluences);
	if (mmd->bindoffsets) MEM_freeN(mmd->bindoffsets);
	if (mmd->bindcagecos) MEM_freeN(mmd->bindcagecos);
	if (mmd->dyngrid) MEM_freeN(mmd->dyngrid);
	if (mmd->dyninfluences) MEM_freeN(mmd->dyninfluences);
	if (mmd->dynverts) MEM_freeN(mmd->dynverts);
	if (mmd->bindweights) MEM_freeN(mmd->bindweights);  /* deprecated */
	if (mmd->bindcos) MEM_freeN(mmd->bindcos);  /* deprecated */
}

static void copyData(ModifierData *md, ModifierData *target)
{
	MeshDeformModifierData *mmd = (MeshDeformModifierData *) md;
	MeshDeformModifierData *tmmd = (MeshDeformModifierData *) target;

	*tmmd = *mmd;

	if (mmd->bindinfluences) tmmd->bindinfluences = MEM_dupallocN(mmd->bindinfluences);
	if (mmd->bindoffsets) tmmd->bindoffsets = MEM_dupallocN(mmd->bindoffsets);
	if (mmd->bindcagecos) tmmd->bindcagecos = MEM_dupallocN(mmd->bindcagecos);
	if (mmd->dyngrid) tmmd->dyngrid = MEM_dupallocN(mmd->dyngrid);
	if (mmd->dyninfluences) tmmd->dyninfluences = MEM_dupallocN(mmd->dyninfluences);
	if (mmd->dynverts) tmmd->dynverts = MEM_dupallocN(mmd->dynverts);
	if (mmd->bindweights) tmmd->dynverts = MEM_dupallocN(mmd->bindweights);  /* deprecated */
	if (mmd->bindcos) tmmd->dynverts = MEM_dupallocN(mmd->bindcos);  /* deprecated */
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{	
	MeshDeformModifierData *mmd = (MeshDeformModifierData *)md;
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups if we need them */
	if (mmd->defgrp_name[0]) dataMask |= CD_MASK_MDEFORMVERT;

	return dataMask;
}

static bool isDisabled(ModifierData *md, int UNUSED(useRenderParams))
{
	MeshDeformModifierData *mmd = (MeshDeformModifierData *) md;

	return !mmd->object;
}

static void foreachObjectLink(
        ModifierData *md, Object *ob,
        void (*walk)(void *userData, Object *ob, Object **obpoin),
        void *userData)
{
	MeshDeformModifierData *mmd = (MeshDeformModifierData *) md;

	walk(userData, ob, &mmd->object);
}

static void updateDepgraph(ModifierData *md, DagForest *forest,
                           struct Scene *UNUSED(scene),
                           Object *UNUSED(ob),
                           DagNode *obNode)
{
	MeshDeformModifierData *mmd = (MeshDeformModifierData *) md;

	if (mmd->object) {
		DagNode *curNode = dag_get_node(forest, mmd->object);

		dag_add_relation(forest, curNode, obNode,
		                 DAG_RL_DATA_DATA | DAG_RL_OB_DATA | DAG_RL_DATA_OB | DAG_RL_OB_OB,
		                 "Mesh Deform Modifier");
	}
}

static float meshdeform_dynamic_bind(MeshDeformModifierData *mmd, float (*dco)[3], float vec[3])
{
	MDefCell *cell;
	MDefInfluence *inf;
	float gridvec[3], dvec[3], ivec[3], wx, wy, wz;
	float weight, cageweight, totweight, *cageco;
	int i, j, a, x, y, z, size;
#ifdef __SSE2__
	__m128 co = _mm_setzero_ps();
#else
	float co[3] = {0.0f, 0.0f, 0.0f};
#endif

	totweight = 0.0f;
	size = mmd->dyngridsize;

	for (i = 0; i < 3; i++) {
		gridvec[i] = (vec[i] - mmd->dyncellmin[i] - mmd->dyncellwidth * 0.5f) / mmd->dyncellwidth;
		ivec[i] = (int)gridvec[i];
		dvec[i] = gridvec[i] - ivec[i];
	}

	for (i = 0; i < 8; i++) {
		if (i & 1) { x = ivec[0] + 1; wx = dvec[0]; }
		else {       x = ivec[0]; wx = 1.0f - dvec[0]; }

		if (i & 2) { y = ivec[1] + 1; wy = dvec[1]; }
		else {       y = ivec[1];     wy = 1.0f - dvec[1]; }

		if (i & 4) { z = ivec[2] + 1; wz = dvec[2]; }
		else {       z = ivec[2];     wz = 1.0f - dvec[2]; }

		CLAMP(x, 0, size - 1);
		CLAMP(y, 0, size - 1);
		CLAMP(z, 0, size - 1);

		a = x + y * size + z * size * size;
		weight = wx * wy * wz;

		cell = &mmd->dyngrid[a];
		inf = mmd->dyninfluences + cell->offset;
		for (j = 0; j < cell->totinfluence; j++, inf++) {
			cageco = dco[inf->vertex];
			cageweight = weight * inf->weight;
#ifdef __SSE2__
			{
				__m128 cageweight_r = _mm_set1_ps(cageweight);
				/* This will load one extra element, this is ok because
				 * we ignore that part of register anyway.
				 */
				__m128 cageco_r = _mm_loadu_ps(cageco);
				co = _mm_add_ps(co,
				                _mm_mul_ps(cageco_r, cageweight_r));
			}
#else
			co[0] += cageweight * cageco[0];
			co[1] += cageweight * cageco[1];
			co[2] += cageweight * cageco[2];
#endif
			totweight += cageweight;
		}
	}

#ifdef __SSE2__
	copy_v3_v3(vec, (float *)&co);
#else
	copy_v3_v3(vec, co);
#endif

	return totweight;
}

typedef struct MeshdeformUserdata {
	/*const*/ MeshDeformModifierData *mmd;
	const MDeformVert *dvert;
	/*const*/ float (*dco)[3];
	int defgrp_index;
	float (*vertexCos)[3];
	float (*cagemat)[4];
	float (*icagemat)[3];
} MeshdeformUserdata;

static void meshdeform_vert_task(void * userdata, int iter)
{
	MeshdeformUserdata *data = userdata;
	/*const*/ MeshDeformModifierData *mmd = data->mmd;
	const MDeformVert *dvert = data->dvert;
	const int defgrp_index = data->defgrp_index;
	const int *offsets = mmd->bindoffsets;
	const MDefInfluence *influences = mmd->bindinfluences;
	/*const*/ float (*dco)[3] = data->dco;
	float (*vertexCos)[3] = data->vertexCos;
	float co[3];
	float weight, totweight, fac = 1.0f;

	if (mmd->flag & MOD_MDEF_DYNAMIC_BIND)
		if (!mmd->dynverts[iter])
			return;

	if (dvert) {
		fac = defvert_find_weight(&dvert[iter], defgrp_index);

		if (mmd->flag & MOD_MDEF_INVERT_VGROUP) {
			fac = 1.0f - fac;
		}

		if (fac <= 0.0f) {
			return;
		}
	}

	if (mmd->flag & MOD_MDEF_DYNAMIC_BIND) {
		/* transform coordinate into cage's local space */
		mul_v3_m4v3(co, data->cagemat, vertexCos[iter]);
		totweight = meshdeform_dynamic_bind(mmd, dco, co);
	}
	else {
		int a;
		totweight = 0.0f;
		zero_v3(co);

		for (a = offsets[iter]; a < offsets[iter + 1]; a++) {
			weight = influences[a].weight;
			madd_v3_v3fl(co, dco[influences[a].vertex], weight);
			totweight += weight;
		}
	}

	if (totweight > 0.0f) {
		mul_v3_fl(co, fac / totweight);
		mul_m3_v3(data->icagemat, co);
		if (G.debug_value != 527)
			add_v3_v3(vertexCos[iter], co);
		else
			copy_v3_v3(vertexCos[iter], co);
	}
}

static void meshdeformModifier_do(
        ModifierData *md, Object *ob, DerivedMesh *dm,
        float (*vertexCos)[3], int numVerts)
{
	MeshDeformModifierData *mmd = (MeshDeformModifierData *) md;
	DerivedMesh *tmpdm, *cagedm;
	MDeformVert *dvert = NULL;
	float imat[4][4], cagemat[4][4], iobmat[4][4], icagemat[3][3], cmat[4][4];
	float co[3], (*dco)[3], (*bindcagecos)[3];
	int a, totvert, totcagevert, defgrp_index;
	float (*cagecos)[3];
	MeshdeformUserdata data;

	if (!mmd->object || (!mmd->bindcagecos && !mmd->bindfunc))
		return;

	/* Get cage derivedmesh.
	 *
	 * Only do this is the target object is in edit mode by itself, meaning
	 * we don't allow linked edit meshes here.
	 * This is because editbmesh_get_derived_cage_and_final() might easily
	 * conflict with the thread which evaluates object which is in the edit
	 * mode for this mesh.
	 *
	 * We'll support this case once granular dependency graph is landed.
	 */
	if (mmd->object == md->scene->obedit) {
		BMEditMesh *em = BKE_editmesh_from_object(mmd->object);
		tmpdm = editbmesh_get_derived_cage_and_final(md->scene, mmd->object, em, &cagedm, 0);
		if (tmpdm)
			tmpdm->release(tmpdm);
	}
	else
		cagedm = mmd->object->derivedFinal;

	/* if we don't have one computed, use derivedmesh from data
	 * without any modifiers */
	if (!cagedm) {
		cagedm = get_dm(mmd->object, NULL, NULL, NULL, false, false);
		if (cagedm)
			cagedm->needsFree = 1;
	}
	
	if (!cagedm) {
		modifier_setError(md, "Cannot get mesh from cage object");
		return;
	}

	/* compute matrices to go in and out of cage object space */
	invert_m4_m4(imat, mmd->object->obmat);
	mul_m4_m4m4(cagemat, imat, ob->obmat);
	mul_m4_m4m4(cmat, mmd->bindmat, cagemat);
	invert_m4_m4(iobmat, cmat);
	copy_m3_m4(icagemat, iobmat);

	/* bind weights if needed */
	if (!mmd->bindcagecos) {
		static int recursive = 0;

		/* progress bar redraw can make this recursive .. */
		if (!recursive) {
			recursive = 1;
			mmd->bindfunc(md->scene, mmd, (float *)vertexCos, numVerts, cagemat);
			recursive = 0;
		}
	}

	/* verify we have compatible weights */
	totvert = numVerts;
	totcagevert = cagedm->getNumVerts(cagedm);

	if (mmd->totvert != totvert) {
		modifier_setError(md, "Verts changed from %d to %d", mmd->totvert, totvert);
		cagedm->release(cagedm);
		return;
	}
	else if (mmd->totcagevert != totcagevert) {
		modifier_setError(md, "Cage verts changed from %d to %d", mmd->totcagevert, totcagevert);
		cagedm->release(cagedm);
		return;
	}
	else if (mmd->bindcagecos == NULL) {
		modifier_setError(md, "Bind data missing");
		cagedm->release(cagedm);
		return;
	}

	cagecos = MEM_mallocN(sizeof(*cagecos) * totcagevert, "meshdeformModifier vertCos");

	/* setup deformation data */
	cagedm->getVertCos(cagedm, cagecos);
	bindcagecos = (float(*)[3])mmd->bindcagecos;

	/* We allocate 1 element extra to make it possible to
	 * load the values to SSE registers, which are float4.
	 */
	dco = MEM_callocN(sizeof(*dco) * (totcagevert + 1), "MDefDco");
	zero_v3(dco[totcagevert]);
	for (a = 0; a < totcagevert; a++) {
		/* get cage vertex in world space with binding transform */
		copy_v3_v3(co, cagecos[a]);

		if (G.debug_value != 527) {
			mul_m4_v3(mmd->bindmat, co);
			/* compute difference with world space bind coord */
			sub_v3_v3v3(dco[a], co, bindcagecos[a]);
		}
		else
			copy_v3_v3(dco[a], co);
	}

	modifier_get_vgroup(ob, dm, mmd->defgrp_name, &dvert, &defgrp_index);

	/* Initialize data to be pass to the for body function. */
	data.mmd = mmd;
	data.dvert = dvert;
	data.dco = dco;
	data.defgrp_index = defgrp_index;
	data.vertexCos = vertexCos;
	data.cagemat = cagemat;
	data.icagemat = icagemat;

	/* Do deformation. */
	BLI_task_parallel_range(0, totvert, &data, meshdeform_vert_task);

	/* release cage derivedmesh */
	MEM_freeN(dco);
	MEM_freeN(cagecos);
	cagedm->release(cagedm);
}

static void deformVerts(ModifierData *md, Object *ob,
                        DerivedMesh *derivedData,
                        float (*vertexCos)[3],
                        int numVerts,
                        ModifierApplyFlag UNUSED(flag))
{
	DerivedMesh *dm = get_dm(ob, NULL, derivedData, NULL, false, false);

	modifier_vgroup_cache(md, vertexCos); /* if next modifier needs original vertices */

	meshdeformModifier_do(md, ob, dm, vertexCos, numVerts);

	if (dm && dm != derivedData)
		dm->release(dm);
}

static void deformVertsEM(ModifierData *md, Object *ob,
                          struct BMEditMesh *UNUSED(editData),
                          DerivedMesh *derivedData,
                          float (*vertexCos)[3],
                          int numVerts)
{
	DerivedMesh *dm = get_dm(ob, NULL, derivedData, NULL, false, false);

	meshdeformModifier_do(md, ob, dm, vertexCos, numVerts);

	if (dm && dm != derivedData)
		dm->release(dm);
}

#define MESHDEFORM_MIN_INFLUENCE 0.00001f

void modifier_mdef_compact_influences(ModifierData *md)
{
	MeshDeformModifierData *mmd = (MeshDeformModifierData *)md;
	float weight, *weights, totweight;
	int totinfluence, totvert, totcagevert, a, b;

	weights = mmd->bindweights;
	if (!weights)
		return;
	
	totvert = mmd->totvert;
	totcagevert = mmd->totcagevert;

	/* count number of influences above threshold */
	for (b = 0; b < totvert; b++) {
		for (a = 0; a < totcagevert; a++) {
			weight = weights[a + b * totcagevert];

			if (weight > MESHDEFORM_MIN_INFLUENCE)
				mmd->totinfluence++;
		}
	}

	/* allocate bind influences */
	mmd->bindinfluences = MEM_callocN(sizeof(MDefInfluence) * mmd->totinfluence, "MDefBindInfluence");
	mmd->bindoffsets = MEM_callocN(sizeof(int) * (totvert + 1), "MDefBindOffset");

	/* write influences */
	totinfluence = 0;

	for (b = 0; b < totvert; b++) {
		mmd->bindoffsets[b] = totinfluence;
		totweight = 0.0f;

		/* sum total weight */
		for (a = 0; a < totcagevert; a++) {
			weight = weights[a + b * totcagevert];

			if (weight > MESHDEFORM_MIN_INFLUENCE)
				totweight += weight;
		}

		/* assign weights normalized */
		for (a = 0; a < totcagevert; a++) {
			weight = weights[a + b * totcagevert];

			if (weight > MESHDEFORM_MIN_INFLUENCE) {
				mmd->bindinfluences[totinfluence].weight = weight / totweight;
				mmd->bindinfluences[totinfluence].vertex = a;
				totinfluence++;
			}
		}
	}

	mmd->bindoffsets[b] = totinfluence;
	
	/* free */
	MEM_freeN(mmd->bindweights);
	mmd->bindweights = NULL;
}

ModifierTypeInfo modifierType_MeshDeform = {
	/* name */              "MeshDeform",
	/* structName */        "MeshDeformModifierData",
	/* structSize */        sizeof(MeshDeformModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsCVs |
	                        eModifierTypeFlag_SupportsEditmode,

	/* copyData */          copyData,
	/* deformVerts */       deformVerts,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     deformVertsEM,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     NULL,
	/* applyModifierEM */   NULL,
	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          freeData,
	/* isDisabled */        isDisabled,
	/* updateDepgraph */    updateDepgraph,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */  NULL,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
