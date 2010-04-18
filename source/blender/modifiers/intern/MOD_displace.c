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

#include "DNA_meshdata_types.h"

#include "BLI_math.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_modifier.h"
#include "BKE_texture.h"
#include "BKE_deform.h"

#include "depsgraph_private.h"
#include "MEM_guardedalloc.h"

#include "MOD_util.h"

#include "RE_shader_ext.h"


/* Displace */

static void initData(ModifierData *md)
{
	DisplaceModifierData *dmd = (DisplaceModifierData*) md;

	dmd->texture = NULL;
	dmd->strength = 1;
	dmd->direction = MOD_DISP_DIR_NOR;
	dmd->midlevel = 0.5;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	DisplaceModifierData *dmd = (DisplaceModifierData*) md;
	DisplaceModifierData *tdmd = (DisplaceModifierData*) target;

	tdmd->texture = dmd->texture;
	tdmd->strength = dmd->strength;
	tdmd->direction = dmd->direction;
	strncpy(tdmd->defgrp_name, dmd->defgrp_name, 32);
	tdmd->midlevel = dmd->midlevel;
	tdmd->texmapping = dmd->texmapping;
	tdmd->map_object = dmd->map_object;
	strncpy(tdmd->uvlayer_name, dmd->uvlayer_name, 32);
}

static CustomDataMask requiredDataMask(Object *ob, ModifierData *md)
{
	DisplaceModifierData *dmd = (DisplaceModifierData *)md;
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups if we need them */
	if(dmd->defgrp_name[0]) dataMask |= (1 << CD_MDEFORMVERT);

	/* ask for UV coordinates if we need them */
	if(dmd->texmapping == MOD_DISP_MAP_UV) dataMask |= (1 << CD_MTFACE);

	return dataMask;
}

static int dependsOnTime(ModifierData *md)
{
	DisplaceModifierData *dmd = (DisplaceModifierData *)md;

	if(dmd->texture)
	{
		return BKE_texture_dependsOnTime(dmd->texture);
	}
	else
	{
		return 0;
	}
}

static void foreachObjectLink(ModifierData *md, Object *ob,
						   ObjectWalkFunc walk, void *userData)
{
	DisplaceModifierData *dmd = (DisplaceModifierData*) md;

	walk(userData, ob, &dmd->map_object);
}

static void foreachIDLink(ModifierData *md, Object *ob,
					   IDWalkFunc walk, void *userData)
{
	DisplaceModifierData *dmd = (DisplaceModifierData*) md;

	walk(userData, ob, (ID **)&dmd->texture);

	foreachObjectLink(md, ob, (ObjectWalkFunc)walk, userData);
}

static int isDisabled(ModifierData *md, int useRenderParams)
{
	DisplaceModifierData *dmd = (DisplaceModifierData*) md;

	return !dmd->texture;
}

static void updateDepgraph(
						ModifierData *md, DagForest *forest, struct Scene *scene,
	 Object *ob, DagNode *obNode)
{
	DisplaceModifierData *dmd = (DisplaceModifierData*) md;

	if(dmd->map_object) {
		DagNode *curNode = dag_get_node(forest, dmd->map_object);

		dag_add_relation(forest, curNode, obNode,
				 DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Displace Modifier");
	}
}

static void get_texture_coords(DisplaceModifierData *dmd, Object *ob,
				   DerivedMesh *dm,
	  float (*co)[3], float (*texco)[3],
		  int numVerts)
{
	int i;
	int texmapping = dmd->texmapping;
	float mapob_imat[4][4];

	if(texmapping == MOD_DISP_MAP_OBJECT) {
		if(dmd->map_object)
			invert_m4_m4(mapob_imat, dmd->map_object->obmat);
		else /* if there is no map object, default to local */
			texmapping = MOD_DISP_MAP_LOCAL;
	}

	/* UVs need special handling, since they come from faces */
	if(texmapping == MOD_DISP_MAP_UV) {
		if(CustomData_has_layer(&dm->faceData, CD_MTFACE)) {
			MFace *mface = dm->getFaceArray(dm);
			MFace *mf;
			char *done = MEM_callocN(sizeof(*done) * numVerts,
					"get_texture_coords done");
			int numFaces = dm->getNumFaces(dm);
			char uvname[32];
			MTFace *tf;

			validate_layer_name(&dm->faceData, CD_MTFACE, dmd->uvlayer_name, uvname);
			tf = CustomData_get_layer_named(&dm->faceData, CD_MTFACE, uvname);

			/* verts are given the UV from the first face that uses them */
			for(i = 0, mf = mface; i < numFaces; ++i, ++mf, ++tf) {
				if(!done[mf->v1]) {
					texco[mf->v1][0] = tf->uv[0][0];
					texco[mf->v1][1] = tf->uv[0][1];
					texco[mf->v1][2] = 0;
					done[mf->v1] = 1;
				}
				if(!done[mf->v2]) {
					texco[mf->v2][0] = tf->uv[1][0];
					texco[mf->v2][1] = tf->uv[1][1];
					texco[mf->v2][2] = 0;
					done[mf->v2] = 1;
				}
				if(!done[mf->v3]) {
					texco[mf->v3][0] = tf->uv[2][0];
					texco[mf->v3][1] = tf->uv[2][1];
					texco[mf->v3][2] = 0;
					done[mf->v3] = 1;
				}
				if(!done[mf->v4]) {
					texco[mf->v4][0] = tf->uv[3][0];
					texco[mf->v4][1] = tf->uv[3][1];
					texco[mf->v4][2] = 0;
					done[mf->v4] = 1;
				}
			}

			/* remap UVs from [0, 1] to [-1, 1] */
			for(i = 0; i < numVerts; ++i) {
				texco[i][0] = texco[i][0] * 2 - 1;
				texco[i][1] = texco[i][1] * 2 - 1;
			}

			MEM_freeN(done);
			return;
		} else /* if there are no UVs, default to local */
			texmapping = MOD_DISP_MAP_LOCAL;
	}

	for(i = 0; i < numVerts; ++i, ++co, ++texco) {
		switch(texmapping) {
			case MOD_DISP_MAP_LOCAL:
				copy_v3_v3(*texco, *co);
				break;
			case MOD_DISP_MAP_GLOBAL:
				mul_v3_m4v3(*texco, ob->obmat, *co);
				break;
			case MOD_DISP_MAP_OBJECT:
				mul_v3_m4v3(*texco, ob->obmat, *co);
				mul_m4_v3(mapob_imat, *texco);
				break;
		}
	}
}

/* dm must be a CDDerivedMesh */
static void displaceModifier_do(
				DisplaceModifierData *dmd, Object *ob,
	DerivedMesh *dm, float (*vertexCos)[3], int numVerts)
{
	int i;
	MVert *mvert;
	MDeformVert *dvert = NULL;
	int defgrp_index;
	float (*tex_co)[3];

	if(!dmd->texture) return;

	defgrp_index = defgroup_name_index(ob, dmd->defgrp_name);

	mvert = CDDM_get_verts(dm);
	if(defgrp_index >= 0)
		dvert = dm->getVertDataArray(dm, CD_MDEFORMVERT);

	tex_co = MEM_callocN(sizeof(*tex_co) * numVerts,
				 "displaceModifier_do tex_co");
	get_texture_coords(dmd, ob, dm, vertexCos, tex_co, numVerts);

	for(i = 0; i < numVerts; ++i) {
		TexResult texres;
		float delta = 0, strength = dmd->strength;
		MDeformWeight *def_weight = NULL;

		if(dvert) {
			int j;
			for(j = 0; j < dvert[i].totweight; ++j) {
				if(dvert[i].dw[j].def_nr == defgrp_index) {
					def_weight = &dvert[i].dw[j];
					break;
				}
			}
			if(!def_weight) continue;
		}

		texres.nor = NULL;
		get_texture_value(dmd->texture, tex_co[i], &texres);

		delta = texres.tin - dmd->midlevel;

		if(def_weight) strength *= def_weight->weight;

		delta *= strength;

		switch(dmd->direction) {
			case MOD_DISP_DIR_X:
				vertexCos[i][0] += delta;
				break;
			case MOD_DISP_DIR_Y:
				vertexCos[i][1] += delta;
				break;
			case MOD_DISP_DIR_Z:
				vertexCos[i][2] += delta;
				break;
			case MOD_DISP_DIR_RGB_XYZ:
				vertexCos[i][0] += (texres.tr - dmd->midlevel) * strength;
				vertexCos[i][1] += (texres.tg - dmd->midlevel) * strength;
				vertexCos[i][2] += (texres.tb - dmd->midlevel) * strength;
				break;
			case MOD_DISP_DIR_NOR:
				vertexCos[i][0] += delta * mvert[i].no[0] / 32767.0f;
				vertexCos[i][1] += delta * mvert[i].no[1] / 32767.0f;
				vertexCos[i][2] += delta * mvert[i].no[2] / 32767.0f;
				break;
		}
	}

	MEM_freeN(tex_co);
}

static void deformVerts(
					 ModifierData *md, Object *ob, DerivedMesh *derivedData,
	  float (*vertexCos)[3], int numVerts, int useRenderParams, int isFinalCalc)
{
	DerivedMesh *dm= get_cddm(md->scene, ob, NULL, derivedData, vertexCos);

	displaceModifier_do((DisplaceModifierData *)md, ob, dm,
				 vertexCos, numVerts);

	if(dm != derivedData)
		dm->release(dm);
}

static void deformVertsEM(
					   ModifierData *md, Object *ob, struct EditMesh *editData,
	DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts)
{
	DerivedMesh *dm= get_cddm(md->scene, ob, editData, derivedData, vertexCos);

	displaceModifier_do((DisplaceModifierData *)md, ob, dm,
				 vertexCos, numVerts);

	if(dm != derivedData)
		dm->release(dm);
}


ModifierTypeInfo modifierType_Displace = {
	/* name */              "Displace",
	/* structName */        "DisplaceModifierData",
	/* structSize */        sizeof(DisplaceModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsMesh
							| eModifierTypeFlag_SupportsEditmode,

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
	/* dependsOnTime */     dependsOnTime,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     foreachIDLink,
};
