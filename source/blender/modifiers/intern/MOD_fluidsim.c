/*
* $Id$
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

#include "MOD_modifiertypes.h"
#include "MOD_fluidsim_util.h"


/* Fluidsim */
static void initData(ModifierData *md)
{
	FluidsimModifierData *fluidmd= (FluidsimModifierData*) md;
	
	fluidsim_init(fluidmd);
}
static void freeData(ModifierData *md)
{
	FluidsimModifierData *fluidmd= (FluidsimModifierData*) md;
	
	fluidsim_free(fluidmd);
}

static void copyData(ModifierData *md, ModifierData *target)
{
	FluidsimModifierData *fluidmd= (FluidsimModifierData*) md;
	FluidsimModifierData *tfluidmd= (FluidsimModifierData*) target;
	
	if(tfluidmd->fss)
		MEM_freeN(tfluidmd->fss);
	
	tfluidmd->fss = MEM_dupallocN(fluidmd->fss);
}



static DerivedMesh * applyModifier(
		ModifierData *md, Object *ob, DerivedMesh *derivedData,
  int useRenderParams, int isFinalCalc)
{
	FluidsimModifierData *fluidmd= (FluidsimModifierData*) md;
	DerivedMesh *result = NULL;
	
	/* check for alloc failing */
	if(!fluidmd->fss)
	{
		initData(md);
		
		if(!fluidmd->fss)
			return derivedData;
	}

	result = fluidsimModifier_do(fluidmd, md->scene, ob, derivedData, useRenderParams, isFinalCalc);

	if(result) 
	{ 
		return result; 
	}
	
	return derivedData;
}

static void updateDepgraph(
		ModifierData *md, DagForest *forest, Scene *scene,
	  Object *ob, DagNode *obNode)
{
	FluidsimModifierData *fluidmd= (FluidsimModifierData*) md;
	Base *base;

	if(fluidmd && fluidmd->fss)
	{
		if(fluidmd->fss->type == OB_FLUIDSIM_DOMAIN)
		{
			for(base = scene->base.first; base; base= base->next) 
			{
				Object *ob1= base->object;
				if(ob1 != ob)
				{
					FluidsimModifierData *fluidmdtmp = (FluidsimModifierData *)modifiers_findByType(ob1, eModifierType_Fluidsim);
					
					// only put dependancies from NON-DOMAIN fluids in here
					if(fluidmdtmp && fluidmdtmp->fss && (fluidmdtmp->fss->type!=OB_FLUIDSIM_DOMAIN))
					{
						DagNode *curNode = dag_get_node(forest, ob1);
						dag_add_relation(forest, curNode, obNode, DAG_RL_DATA_DATA|DAG_RL_OB_DATA, "Fluidsim Object");
					}
				}
			}
		}
	}
}

static int dependsOnTime(ModifierData *md) 
{
	return 1;
}


ModifierTypeInfo modifierType_Fluidsim = {
	/* name */              "Fluidsim",
	/* structName */        "FluidsimModifierData",
	/* structSize */        sizeof(FluidsimModifierData),
	/* type */              eModifierTypeType_Nonconstructive,

	/* flags */             eModifierTypeFlag_AcceptsMesh
							| eModifierTypeFlag_RequiresOriginalData
							| eModifierTypeFlag_Single,

	/* copyData */          copyData,
	/* deformVerts */       0,
	/* deformVertsEM */     0,
	/* deformMatricesEM */  0,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   0,
	/* initData */          initData,
	/* requiredDataMask */  0,
	/* freeData */          freeData,
	/* isDisabled */        0,
	/* updateDepgraph */    updateDepgraph,
	/* dependsOnTime */     dependsOnTime,
	/* foreachObjectLink */ 0,
	/* foreachIDLink */     0,
};
