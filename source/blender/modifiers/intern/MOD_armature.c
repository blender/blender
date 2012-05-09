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

/** \file blender/modifiers/intern/MOD_armature.c
 *  \ingroup modifiers
 */


#include <string.h>

#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"

#include "BLI_utildefines.h"
#include "BLI_string.h"


#include "BKE_cdderivedmesh.h"
#include "BKE_lattice.h"
#include "BKE_modifier.h"

#include "MEM_guardedalloc.h"

#include "depsgraph_private.h"

#include "MOD_util.h"


static void initData(ModifierData *md)
{
	ArmatureModifierData *amd = (ArmatureModifierData *) md;
	
	amd->deformflag = ARM_DEF_ENVELOPE | ARM_DEF_VGROUP;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	ArmatureModifierData *amd = (ArmatureModifierData *) md;
	ArmatureModifierData *tamd = (ArmatureModifierData *) target;

	tamd->object = amd->object;
	tamd->deformflag = amd->deformflag;
	tamd->multi = amd->multi;
	BLI_strncpy(tamd->defgrp_name, amd->defgrp_name, sizeof(tamd->defgrp_name));
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *UNUSED(md))
{
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups */
	dataMask |= CD_MASK_MDEFORMVERT;

	return dataMask;
}

static int isDisabled(ModifierData *md, int UNUSED(useRenderParams))
{
	ArmatureModifierData *amd = (ArmatureModifierData *) md;

	return !amd->object;
}

static void foreachObjectLink(
        ModifierData *md, Object *ob,
        void (*walk)(void *userData, Object *ob, Object **obpoin),
        void *userData)
{
	ArmatureModifierData *amd = (ArmatureModifierData *) md;

	walk(userData, ob, &amd->object);
}

static void updateDepgraph(ModifierData *md, DagForest *forest,
                           struct Scene *UNUSED(scene),
                           Object *UNUSED(ob),
                           DagNode *obNode)
{
	ArmatureModifierData *amd = (ArmatureModifierData *) md;

	if (amd->object) {
		DagNode *curNode = dag_get_node(forest, amd->object);

		dag_add_relation(forest, curNode, obNode,
		                 DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Armature Modifier");
	}
}

static void deformVerts(ModifierData *md, Object *ob,
                        DerivedMesh *derivedData,
                        float (*vertexCos)[3],
                        int numVerts,
                        ModifierApplyFlag UNUSED(flag))
{
	ArmatureModifierData *amd = (ArmatureModifierData *) md;

	modifier_vgroup_cache(md, vertexCos); /* if next modifier needs original vertices */
	
	armature_deform_verts(amd->object, ob, derivedData, vertexCos, NULL,
	                      numVerts, amd->deformflag, (float(*)[3])amd->prevCos, amd->defgrp_name);

	/* free cache */
	if (amd->prevCos) {
		MEM_freeN(amd->prevCos);
		amd->prevCos = NULL;
	}
}

static void deformVertsEM(
        ModifierData *md, Object *ob, struct BMEditMesh *editData,
        DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts)
{
	ArmatureModifierData *amd = (ArmatureModifierData *) md;
	DerivedMesh *dm = derivedData;

	if (!derivedData) dm = CDDM_from_BMEditMesh(editData, ob->data, FALSE, FALSE);

	modifier_vgroup_cache(md, vertexCos); /* if next modifier needs original vertices */

	armature_deform_verts(amd->object, ob, dm, vertexCos, NULL,
	                      numVerts, amd->deformflag, (float(*)[3])amd->prevCos, amd->defgrp_name);

	/* free cache */
	if (amd->prevCos) {
		MEM_freeN(amd->prevCos);
		amd->prevCos = NULL;
	}

	if (!derivedData) dm->release(dm);
}

static void deformMatricesEM(
        ModifierData *md, Object *ob, struct BMEditMesh *editData,
        DerivedMesh *derivedData, float (*vertexCos)[3],
        float (*defMats)[3][3], int numVerts)
{
	ArmatureModifierData *amd = (ArmatureModifierData *) md;
	DerivedMesh *dm = derivedData;

	if (!derivedData) dm = CDDM_from_BMEditMesh(editData, ob->data, FALSE, FALSE);

	armature_deform_verts(amd->object, ob, dm, vertexCos, defMats, numVerts,
	                      amd->deformflag, NULL, amd->defgrp_name);

	if (!derivedData) dm->release(dm);
}

static void deformMatrices(ModifierData *md, Object *ob, DerivedMesh *derivedData,
                           float (*vertexCos)[3], float (*defMats)[3][3], int numVerts)
{
	ArmatureModifierData *amd = (ArmatureModifierData *) md;
	DerivedMesh *dm = derivedData;

	if (!derivedData) dm = CDDM_from_mesh((Mesh *)ob->data, ob);

	armature_deform_verts(amd->object, ob, dm, vertexCos, defMats, numVerts,
	                      amd->deformflag, NULL, amd->defgrp_name);

	if (!derivedData) dm->release(dm);
}

ModifierTypeInfo modifierType_Armature = {
	/* name */              "Armature",
	/* structName */        "ArmatureModifierData",
	/* structSize */        sizeof(ArmatureModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsCVs |
	                        eModifierTypeFlag_SupportsEditmode,

	/* copyData */          copyData,
	/* deformVerts */       deformVerts,
	/* deformMatrices */    deformMatrices,
	/* deformVertsEM */     deformVertsEM,
	/* deformMatricesEM */  deformMatricesEM,
	/* applyModifier */     NULL,
	/* applyModifierEM */   NULL,
	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          NULL,
	/* isDisabled */        isDisabled,
	/* updateDepgraph */    updateDepgraph,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
