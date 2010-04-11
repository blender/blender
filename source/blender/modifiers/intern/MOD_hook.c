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


static void initData(ModifierData *md) 
{
	HookModifierData *hmd = (HookModifierData*) md;

	hmd->force= 1.0;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	HookModifierData *hmd = (HookModifierData*) md;
	HookModifierData *thmd = (HookModifierData*) target;

	VECCOPY(thmd->cent, hmd->cent);
	thmd->falloff = hmd->falloff;
	thmd->force = hmd->force;
	thmd->object = hmd->object;
	thmd->totindex = hmd->totindex;
	thmd->indexar = MEM_dupallocN(hmd->indexar);
	memcpy(thmd->parentinv, hmd->parentinv, sizeof(hmd->parentinv));
	strncpy(thmd->name, hmd->name, 32);
	strncpy(thmd->subtarget, hmd->subtarget, 32);
}

static CustomDataMask requiredDataMask(Object *ob, ModifierData *md)
{
	HookModifierData *hmd = (HookModifierData *)md;
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups if we need them */
	if(!hmd->indexar && hmd->name[0]) dataMask |= (1 << CD_MDEFORMVERT);

	return dataMask;
}

static void freeData(ModifierData *md)
{
	HookModifierData *hmd = (HookModifierData*) md;

	if (hmd->indexar) MEM_freeN(hmd->indexar);
}

static int isDisabled(ModifierData *md, int useRenderParams)
{
	HookModifierData *hmd = (HookModifierData*) md;

	return !hmd->object;
}

static void foreachObjectLink(
					   ModifierData *md, Object *ob,
	void (*walk)(void *userData, Object *ob, Object **obpoin),
		   void *userData)
{
	HookModifierData *hmd = (HookModifierData*) md;

	walk(userData, ob, &hmd->object);
}

static void updateDepgraph(ModifierData *md, DagForest *forest, Scene *scene,
					Object *ob, DagNode *obNode)
{
	HookModifierData *hmd = (HookModifierData*) md;

	if (hmd->object) {
		DagNode *curNode = dag_get_node(forest, hmd->object);
		
		if (hmd->subtarget[0])
			dag_add_relation(forest, curNode, obNode, DAG_RL_OB_DATA|DAG_RL_DATA_DATA, "Hook Modifier");
		else
			dag_add_relation(forest, curNode, obNode, DAG_RL_OB_DATA, "Hook Modifier");
	}
}

static void deformVerts(
					 ModifierData *md, Object *ob, DerivedMesh *derivedData,
	 float (*vertexCos)[3], int numVerts, int useRenderParams, int isFinalCalc)
{
	HookModifierData *hmd = (HookModifierData*) md;
	bPoseChannel *pchan= get_pose_channel(hmd->object->pose, hmd->subtarget);
	float vec[3], mat[4][4], dmat[4][4];
	int i;
	DerivedMesh *dm = derivedData;
	
	/* get world-space matrix of target, corrected for the space the verts are in */
	if (hmd->subtarget[0] && pchan) {
		/* bone target if there's a matching pose-channel */
		mul_m4_m4m4(dmat, pchan->pose_mat, hmd->object->obmat);
	}
	else {
		/* just object target */
		copy_m4_m4(dmat, hmd->object->obmat);
	}
	invert_m4_m4(ob->imat, ob->obmat);
	mul_serie_m4(mat, ob->imat, dmat, hmd->parentinv,
			 NULL, NULL, NULL, NULL, NULL);

	/* vertex indices? */
	if(hmd->indexar) {
		for(i = 0; i < hmd->totindex; i++) {
			int index = hmd->indexar[i];

			/* This should always be true and I don't generally like 
			* "paranoid" style code like this, but old files can have
			* indices that are out of range because old blender did
			* not correct them on exit editmode. - zr
			*/
			if(index < numVerts) {
				float *co = vertexCos[index];
				float fac = hmd->force;

				/* if DerivedMesh is present and has original index data,
				* use it
				*/
				if(dm && dm->getVertDataArray(dm, CD_ORIGINDEX)) {
					int j;
					int orig_index;
					for(j = 0; j < numVerts; ++j) {
						fac = hmd->force;
						orig_index = *(int *)dm->getVertData(dm, j,
								CD_ORIGINDEX);
						if(orig_index == index) {
							co = vertexCos[j];
							if(hmd->falloff != 0.0) {
								float len = len_v3v3(co, hmd->cent);
								if(len > hmd->falloff) fac = 0.0;
								else if(len > 0.0)
									fac *= sqrt(1.0 - len / hmd->falloff);
							}

							if(fac != 0.0) {
								mul_v3_m4v3(vec, mat, co);
								interp_v3_v3v3(co, co, vec, fac);
							}
						}
					}
				} else {
					if(hmd->falloff != 0.0) {
						float len = len_v3v3(co, hmd->cent);
						if(len > hmd->falloff) fac = 0.0;
						else if(len > 0.0)
							fac *= sqrt(1.0 - len / hmd->falloff);
					}

					if(fac != 0.0) {
						mul_v3_m4v3(vec, mat, co);
						interp_v3_v3v3(co, co, vec, fac);
					}
				}
			}
		}
	} 
	else if(hmd->name[0]) {	/* vertex group hook */
		Mesh *me = ob->data;
		int use_dverts = 0;
		int maxVerts = 0;
		int defgrp_index = defgroup_name_index(ob, hmd->name);

		if(dm) {
			if(dm->getVertData(dm, 0, CD_MDEFORMVERT)) {
				maxVerts = dm->getNumVerts(dm);
				use_dverts = 1;
			}
		}
		else if(me->dvert) {
			maxVerts = me->totvert;
			use_dverts = 1;
		}

		if(defgrp_index >= 0 && use_dverts) {
			MDeformVert *dvert = me->dvert;
			int i, j;

			for(i = 0; i < maxVerts; i++, dvert++) {
				if(dm) dvert = dm->getVertData(dm, i, CD_MDEFORMVERT);
				for(j = 0; j < dvert->totweight; j++) {
					if(dvert->dw[j].def_nr == defgrp_index) {
						float fac = hmd->force*dvert->dw[j].weight;
						float *co = vertexCos[i];

						if(hmd->falloff != 0.0) {
							float len = len_v3v3(co, hmd->cent);
							if(len > hmd->falloff) fac = 0.0;
							else if(len > 0.0)
								fac *= sqrt(1.0 - len / hmd->falloff);
						}

						mul_v3_m4v3(vec, mat, co);
						interp_v3_v3v3(co, co, vec, fac);
					}
				}
			}
		}
	}
}

static void deformVertsEM(
					   ModifierData *md, Object *ob, EditMesh *editData,
	   DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts)
{
	DerivedMesh *dm = derivedData;

	if(!derivedData) dm = CDDM_from_editmesh(editData, ob->data);

	deformVerts(md, ob, derivedData, vertexCos, numVerts, 0, 0);

	if(!derivedData) dm->release(dm);
}


ModifierTypeInfo modifierType_Hook = {
	/* name */              "Hook",
	/* structName */        "HookModifierData",
	/* structSize */        sizeof(HookModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsCVs
							| eModifierTypeFlag_SupportsEditmode,
	/* copyData */          copyData,
	/* deformVerts */       deformVerts,
	/* deformVertsEM */     deformVertsEM,
	/* deformMatricesEM */  0,
	/* applyModifier */     0,
	/* applyModifierEM */   0,
	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          freeData,
	/* isDisabled */        isDisabled,
	/* updateDepgraph */    updateDepgraph,
	/* dependsOnTime */     0,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     0,
};
