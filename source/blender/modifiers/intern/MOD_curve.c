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

/** \file blender/modifiers/intern/MOD_curve.c
 *  \ingroup modifiers
 */


#include <string.h>

#include "DNA_scene_types.h"
#include "DNA_object_types.h"

#include "BLI_utildefines.h"


#include "BKE_cdderivedmesh.h"
#include "BKE_lattice.h"
#include "BKE_modifier.h"

#include "depsgraph_private.h"

#include "MOD_util.h"

static void initData(ModifierData *md)
{
	CurveModifierData *cmd = (CurveModifierData *) md;

	cmd->defaxis = MOD_CURVE_POSX;
}

static void copyData(ModifierData *md, ModifierData *target)
{
#if 0
	CurveModifierData *cmd = (CurveModifierData *) md;
	CurveModifierData *tcmd = (CurveModifierData *) target;
#endif
	modifier_copyData_generic(md, target);
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	CurveModifierData *cmd = (CurveModifierData *)md;
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups if we need them */
	if (cmd->name[0]) dataMask |= CD_MASK_MDEFORMVERT;

	return dataMask;
}

static bool isDisabled(ModifierData *md, int UNUSED(userRenderParams))
{
	CurveModifierData *cmd = (CurveModifierData *) md;

	return !cmd->object;
}

static void foreachObjectLink(
        ModifierData *md, Object *ob,
        void (*walk)(void *userData, Object *ob, Object **obpoin),
        void *userData)
{
	CurveModifierData *cmd = (CurveModifierData *) md;

	walk(userData, ob, &cmd->object);
}

static void updateDepgraph(ModifierData *md, DagForest *forest,
                           Scene *UNUSED(scene),
                           Object *UNUSED(ob),
                           DagNode *obNode)
{
	CurveModifierData *cmd = (CurveModifierData *) md;

	if (cmd->object) {
		DagNode *curNode = dag_get_node(forest, cmd->object);
		curNode->eval_flags |= DAG_EVAL_NEED_CURVE_PATH;

		dag_add_relation(forest, curNode, obNode,
		                 DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Curve Modifier");
	}
}

static void deformVerts(ModifierData *md, Object *ob,
                        DerivedMesh *derivedData,
                        float (*vertexCos)[3],
                        int numVerts,
                        ModifierApplyFlag UNUSED(flag))
{
	CurveModifierData *cmd = (CurveModifierData *) md;

	/* silly that defaxis and curve_deform_verts are off by 1
	 * but leave for now to save having to call do_versions */
	curve_deform_verts(md->scene, cmd->object, ob, derivedData, vertexCos, numVerts,
	                   cmd->name, cmd->defaxis - 1);
}

static void deformVertsEM(
        ModifierData *md, Object *ob, struct BMEditMesh *em,
        DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts)
{
	DerivedMesh *dm = derivedData;

	if (!derivedData) dm = CDDM_from_editbmesh(em, false, false);

	deformVerts(md, ob, dm, vertexCos, numVerts, 0);

	if (!derivedData) dm->release(dm);
}


ModifierTypeInfo modifierType_Curve = {
	/* name */              "Curve",
	/* structName */        "CurveModifierData",
	/* structSize */        sizeof(CurveModifierData),
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
	/* freeData */          NULL,
	/* isDisabled */        isDisabled,
	/* updateDepgraph */    updateDepgraph,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */  NULL,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
