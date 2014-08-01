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

/** \file blender/modifiers/intern/MOD_shrinkwrap.c
 *  \ingroup modifiers
 */


#include <string.h>

#include "DNA_object_types.h"

#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_modifier.h"
#include "BKE_shrinkwrap.h"

#include "depsgraph_private.h"

#include "MOD_util.h"

static bool dependsOnNormals(ModifierData *md);


static void initData(ModifierData *md)
{
	ShrinkwrapModifierData *smd = (ShrinkwrapModifierData *) md;
	smd->shrinkType = MOD_SHRINKWRAP_NEAREST_SURFACE;
	smd->shrinkOpts = MOD_SHRINKWRAP_PROJECT_ALLOW_POS_DIR;
	smd->keepDist   = 0.0f;

	smd->target     = NULL;
	smd->auxTarget  = NULL;
}

static void copyData(ModifierData *md, ModifierData *target)
{
#if 0
	ShrinkwrapModifierData *smd  = (ShrinkwrapModifierData *)md;
	ShrinkwrapModifierData *tsmd = (ShrinkwrapModifierData *)target;
#endif
	modifier_copyData_generic(md, target);
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	ShrinkwrapModifierData *smd = (ShrinkwrapModifierData *)md;
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups if we need them */
	if (smd->vgroup_name[0])
		dataMask |= CD_MASK_MDEFORMVERT;

	if ((smd->shrinkType == MOD_SHRINKWRAP_PROJECT) &&
	    (smd->projAxis == MOD_SHRINKWRAP_PROJECT_OVER_NORMAL))
	{
		dataMask |= CD_MASK_MVERT;
	}

	return dataMask;
}

static bool isDisabled(ModifierData *md, int UNUSED(useRenderParams))
{
	ShrinkwrapModifierData *smd = (ShrinkwrapModifierData *) md;
	return !smd->target;
}


static void foreachObjectLink(ModifierData *md, Object *ob, ObjectWalkFunc walk, void *userData)
{
	ShrinkwrapModifierData *smd = (ShrinkwrapModifierData *) md;

	walk(userData, ob, &smd->target);
	walk(userData, ob, &smd->auxTarget);
}

static void deformVerts(ModifierData *md, Object *ob,
                        DerivedMesh *derivedData,
                        float (*vertexCos)[3],
                        int numVerts,
                        ModifierApplyFlag flag)
{
	DerivedMesh *dm = derivedData;
	CustomDataMask dataMask = requiredDataMask(ob, md);
	bool forRender = (flag & MOD_APPLY_RENDER) != 0;

	/* ensure we get a CDDM with applied vertex coords */
	if (dataMask) {
		dm = get_cddm(ob, NULL, dm, vertexCos, dependsOnNormals(md));
	}

	shrinkwrapModifier_deform((ShrinkwrapModifierData *)md, ob, dm, vertexCos, numVerts, forRender);

	if (dm != derivedData)
		dm->release(dm);
}

static void deformVertsEM(ModifierData *md, Object *ob, struct BMEditMesh *editData, DerivedMesh *derivedData,
                          float (*vertexCos)[3], int numVerts)
{
	DerivedMesh *dm = derivedData;
	CustomDataMask dataMask = requiredDataMask(ob, md);

	/* ensure we get a CDDM with applied vertex coords */
	if (dataMask) {
		dm = get_cddm(ob, editData, dm, vertexCos, dependsOnNormals(md));
	}

	shrinkwrapModifier_deform((ShrinkwrapModifierData *)md, ob, dm, vertexCos, numVerts, false);

	if (dm != derivedData)
		dm->release(dm);
}

static void updateDepgraph(ModifierData *md, DagForest *forest,
                           struct Scene *UNUSED(scene),
                           Object *UNUSED(ob),
                           DagNode *obNode)
{
	ShrinkwrapModifierData *smd = (ShrinkwrapModifierData *) md;

	if (smd->target)
		dag_add_relation(forest, dag_get_node(forest, smd->target), obNode,
		                 DAG_RL_OB_DATA | DAG_RL_DATA_DATA, "Shrinkwrap Modifier");

	if (smd->auxTarget)
		dag_add_relation(forest, dag_get_node(forest, smd->auxTarget), obNode,
		                 DAG_RL_OB_DATA | DAG_RL_DATA_DATA, "Shrinkwrap Modifier");
}

static bool dependsOnNormals(ModifierData *md)
{
	ShrinkwrapModifierData *smd = (ShrinkwrapModifierData *)md;

	if (smd->target && smd->shrinkType == MOD_SHRINKWRAP_PROJECT)
		return (smd->projAxis == MOD_SHRINKWRAP_PROJECT_OVER_NORMAL);
	
	return false;
}

ModifierTypeInfo modifierType_Shrinkwrap = {
	/* name */              "Shrinkwrap",
	/* structName */        "ShrinkwrapModifierData",
	/* structSize */        sizeof(ShrinkwrapModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_AcceptsCVs |
	                        eModifierTypeFlag_SupportsEditmode |
	                        eModifierTypeFlag_EnableInEditmode,

	/* copyData */          copyData,
	/* deformVerts */       deformVerts,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     deformVertsEM,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     NULL,
	/* applyModifierEM */   NULL,
	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          NULL,
	/* isDisabled */        isDisabled,
	/* updateDepgraph */    updateDepgraph,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */  dependsOnNormals,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
