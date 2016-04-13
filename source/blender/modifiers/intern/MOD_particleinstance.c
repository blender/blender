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

/** \file blender/modifiers/intern/MOD_particleinstance.c
 *  \ingroup modifiers
 */


#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_lattice.h"
#include "BKE_library_query.h"
#include "BKE_modifier.h"
#include "BKE_pointcache.h"

#include "depsgraph_private.h"
#include "DEG_depsgraph_build.h"

static void initData(ModifierData *md)
{
	ParticleInstanceModifierData *pimd = (ParticleInstanceModifierData *) md;

	pimd->flag = eParticleInstanceFlag_Parents | eParticleInstanceFlag_Unborn |
	             eParticleInstanceFlag_Alive | eParticleInstanceFlag_Dead;
	pimd->psys = 1;
	pimd->position = 1.0f;
	pimd->axis = 2;

}
static void copyData(ModifierData *md, ModifierData *target)
{
#if 0
	ParticleInstanceModifierData *pimd = (ParticleInstanceModifierData *) md;
	ParticleInstanceModifierData *tpimd = (ParticleInstanceModifierData *) target;
#endif
	modifier_copyData_generic(md, target);
}

static bool isDisabled(ModifierData *UNUSED(md), int UNUSED(useRenderParams))
{
	return true;
}


static void updateDepgraph(ModifierData *md, DagForest *forest,
                           struct Main *UNUSED(bmain),
                           struct Scene *UNUSED(scene),
                           Object *UNUSED(ob),
                           DagNode *obNode)
{
	ParticleInstanceModifierData *pimd = (ParticleInstanceModifierData *) md;

	if (pimd->ob) {
		DagNode *curNode = dag_get_node(forest, pimd->ob);

		dag_add_relation(forest, curNode, obNode,
		                 DAG_RL_DATA_DATA | DAG_RL_OB_DATA,
		                 "Particle Instance Modifier");
	}
}

static void updateDepsgraph(ModifierData *md,
                            struct Main *UNUSED(bmain),
                            struct Scene *UNUSED(scene),
                            Object *UNUSED(ob),
                            struct DepsNodeHandle *node)
{
	ParticleInstanceModifierData *pimd = (ParticleInstanceModifierData *) md;
	if (pimd->ob != NULL) {
		DEG_add_object_relation(node, pimd->ob, DEG_OB_COMP_TRANSFORM, "Particle Instance Modifier");
	}
}

static void foreachObjectLink(ModifierData *md, Object *ob,
                              ObjectWalkFunc walk, void *userData)
{
	ParticleInstanceModifierData *pimd = (ParticleInstanceModifierData *) md;

	walk(userData, ob, &pimd->ob, IDWALK_NOP);
}

static DerivedMesh *applyModifier(ModifierData *UNUSED(md), Object *UNUSED(ob),
                                  DerivedMesh *derivedData,
                                  ModifierApplyFlag UNUSED(flag))
{
	return derivedData;
}

ModifierTypeInfo modifierType_ParticleInstance = {
	/* name */              "ParticleInstance",
	/* structName */        "ParticleInstanceModifierData",
	/* structSize */        sizeof(ParticleInstanceModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_SupportsMapping |
	                        eModifierTypeFlag_SupportsEditmode |
	                        eModifierTypeFlag_EnableInEditmode,

	/* copyData */          copyData,
	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   NULL,
	/* initData */          initData,
	/* requiredDataMask */  NULL,
	/* freeData */          NULL,
	/* isDisabled */        isDisabled,
	/* updateDepgraph */    updateDepgraph,
	/* updateDepsgraph */   updateDepsgraph,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */  NULL,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
