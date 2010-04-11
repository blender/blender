/*
* $Id:
*
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

#include "stddef.h"
#include "string.h"
#include "stdarg.h"
#include "math.h"
#include "float.h"

#include "BLI_kdtree.h"
#include "BLI_rand.h"
#include "BLI_uvproject.h"

#include "MEM_guardedalloc.h"

#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_object_fluidsim.h"


#include "BKE_action.h"
#include "BKE_bmesh.h"
#include "BKE_booleanops.h"
#include "BKE_cloth.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_displist.h"
#include "BKE_fluidsim.h"
#include "BKE_global.h"
#include "BKE_multires.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_scene.h"
#include "BKE_smoke.h"
#include "BKE_softbody.h"
#include "BKE_subsurf.h"
#include "BKE_texture.h"

#include "depsgraph_private.h"
#include "BKE_deform.h"
#include "BKE_shrinkwrap.h"

#include "LOD_decimation.h"

#include "CCGSubSurf.h"

#include "RE_shader_ext.h"

#include "MOD_modifiertypes.h"
#include "MOD_util.h"


/* Shrinkwrap */

static void initData(ModifierData *md)
{
	ShrinkwrapModifierData *smd = (ShrinkwrapModifierData*) md;
	smd->shrinkType = MOD_SHRINKWRAP_NEAREST_SURFACE;
	smd->shrinkOpts = MOD_SHRINKWRAP_PROJECT_ALLOW_POS_DIR;
	smd->keepDist	= 0.0f;

	smd->target		= NULL;
	smd->auxTarget	= NULL;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	ShrinkwrapModifierData *smd  = (ShrinkwrapModifierData*)md;
	ShrinkwrapModifierData *tsmd = (ShrinkwrapModifierData*)target;

	tsmd->target	= smd->target;
	tsmd->auxTarget = smd->auxTarget;

	strcpy(tsmd->vgroup_name, smd->vgroup_name);

	tsmd->keepDist	= smd->keepDist;
	tsmd->shrinkType= smd->shrinkType;
	tsmd->shrinkOpts= smd->shrinkOpts;
	tsmd->projAxis = smd->projAxis;
	tsmd->subsurfLevels = smd->subsurfLevels;
}

static CustomDataMask requiredDataMask(Object *ob, ModifierData *md)
{
	ShrinkwrapModifierData *smd = (ShrinkwrapModifierData *)md;
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups if we need them */
	if(smd->vgroup_name[0])
		dataMask |= (1 << CD_MDEFORMVERT);

	if(smd->shrinkType == MOD_SHRINKWRAP_PROJECT
	&& smd->projAxis == MOD_SHRINKWRAP_PROJECT_OVER_NORMAL)
		dataMask |= (1 << CD_MVERT);
		
	return dataMask;
}

static int isDisabled(ModifierData *md, int useRenderParams)
{
	ShrinkwrapModifierData *smd = (ShrinkwrapModifierData*) md;
	return !smd->target;
}


static void foreachObjectLink(ModifierData *md, Object *ob, ObjectWalkFunc walk, void *userData)
{
	ShrinkwrapModifierData *smd = (ShrinkwrapModifierData*) md;

	walk(userData, ob, &smd->target);
	walk(userData, ob, &smd->auxTarget);
}

static void deformVerts(ModifierData *md, Object *ob, DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts, int useRenderParams, int isFinalCalc)
{
	DerivedMesh *dm = derivedData;
	CustomDataMask dataMask = requiredDataMask(ob, md);

	/* ensure we get a CDDM with applied vertex coords */
	if(dataMask)
		dm= get_cddm(md->scene, ob, NULL, dm, vertexCos);

	shrinkwrapModifier_deform((ShrinkwrapModifierData*)md, md->scene, ob, dm, vertexCos, numVerts);

	if(dm != derivedData)
		dm->release(dm);
}

static void deformVertsEM(ModifierData *md, Object *ob, EditMesh *editData, DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts)
{
	DerivedMesh *dm = derivedData;
	CustomDataMask dataMask = requiredDataMask(ob, md);

	/* ensure we get a CDDM with applied vertex coords */
	if(dataMask)
		dm= get_cddm(md->scene, ob, editData, dm, vertexCos);

	shrinkwrapModifier_deform((ShrinkwrapModifierData*)md, md->scene, ob, dm, vertexCos, numVerts);

	if(dm != derivedData)
		dm->release(dm);
}

static void updateDepgraph(ModifierData *md, DagForest *forest, Scene *scene, Object *ob, DagNode *obNode)
{
	ShrinkwrapModifierData *smd = (ShrinkwrapModifierData*) md;

	if (smd->target)
		dag_add_relation(forest, dag_get_node(forest, smd->target),   obNode, DAG_RL_OB_DATA | DAG_RL_DATA_DATA, "Shrinkwrap Modifier");

	if (smd->auxTarget)
		dag_add_relation(forest, dag_get_node(forest, smd->auxTarget), obNode, DAG_RL_OB_DATA | DAG_RL_DATA_DATA, "Shrinkwrap Modifier");
}


ModifierTypeInfo modifierType_Shrinkwrap = {
	/* name */              "Shrinkwrap",
	/* structName */        "ShrinkwrapModifierData",
	/* structSize */        sizeof(ShrinkwrapModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsMesh
							| eModifierTypeFlag_AcceptsCVs
							| eModifierTypeFlag_SupportsEditmode
							| eModifierTypeFlag_EnableInEditmode,

	/* copyData */          copyData,
	/* deformVerts */       deformVerts,
	/* deformVertsEM */     deformVertsEM,
	/* deformMatricesEM */  0,
	/* applyModifier */     0,
	/* applyModifierEM */   0,
	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          0,
	/* isDisabled */        isDisabled,
	/* updateDepgraph */    updateDepgraph,
	/* dependsOnTime */     0,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     0,
};
