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

/** \file blender/modifiers/intern/MOD_lattice.c
 *  \ingroup modifiers
 */


#include <string.h>

#include "DNA_object_types.h"

#include "BLI_utildefines.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_lattice.h"
#include "BKE_library_query.h"
#include "BKE_modifier.h"

#include "depsgraph_private.h"

#include "MOD_util.h"

static void initData(ModifierData *md)
{
	LatticeModifierData *lmd = (LatticeModifierData *) md;
	lmd->strength = 1.0f;
}

static void copyData(ModifierData *md, ModifierData *target)
{
#if 0
	LatticeModifierData *lmd = (LatticeModifierData *) md;
	LatticeModifierData *tlmd = (LatticeModifierData *) target;
#endif

	modifier_copyData_generic(md, target);
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	LatticeModifierData *lmd = (LatticeModifierData *)md;
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups if we need them */
	if (lmd->name[0]) dataMask |= CD_MASK_MDEFORMVERT;

	return dataMask;
}

static bool isDisabled(ModifierData *md, int UNUSED(userRenderParams))
{
	LatticeModifierData *lmd = (LatticeModifierData *) md;

	return !lmd->object;
}

static void foreachObjectLink(
        ModifierData *md, Object *ob,
        ObjectWalkFunc walk, void *userData)
{
	LatticeModifierData *lmd = (LatticeModifierData *) md;

	walk(userData, ob, &lmd->object, IDWALK_CB_NOP);
}

static void updateDepgraph(ModifierData *md, DagForest *forest,
                           struct Main *UNUSED(bmain),
                           struct Scene *UNUSED(scene),
                           Object *UNUSED(ob),
                           DagNode *obNode)
{
	LatticeModifierData *lmd = (LatticeModifierData *) md;

	if (lmd->object) {
		DagNode *latNode = dag_get_node(forest, lmd->object);

		dag_add_relation(forest, latNode, obNode,
		                 DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Lattice Modifier");
	}
}

static void updateDepsgraph(ModifierData *md,
                            struct Main *UNUSED(bmain),
                            struct Scene *UNUSED(scene),
                            Object *object,
                            struct DepsNodeHandle *node)
{
	LatticeModifierData *lmd = (LatticeModifierData *)md;
	if (lmd->object != NULL) {
		DEG_add_object_relation(node, lmd->object, DEG_OB_COMP_GEOMETRY, "Lattice Modifier");
		DEG_add_object_relation(node, lmd->object, DEG_OB_COMP_TRANSFORM, "Lattice Modifier");
	}
	DEG_add_object_relation(node, object, DEG_OB_COMP_TRANSFORM, "Lattice Modifier");
}

static void deformVerts(ModifierData *md, Object *ob,
                        DerivedMesh *derivedData,
                        float (*vertexCos)[3],
                        int numVerts,
                        ModifierApplyFlag UNUSED(flag))
{
	LatticeModifierData *lmd = (LatticeModifierData *) md;


	modifier_vgroup_cache(md, vertexCos); /* if next modifier needs original vertices */
	
	lattice_deform_verts(lmd->object, ob, derivedData,
	                     vertexCos, numVerts, lmd->name, lmd->strength);
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


ModifierTypeInfo modifierType_Lattice = {
	/* name */              "Lattice",
	/* structName */        "LatticeModifierData",
	/* structSize */        sizeof(LatticeModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsCVs |
	                        eModifierTypeFlag_AcceptsLattice |
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
	/* updateDepsgraph */   updateDepsgraph,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
