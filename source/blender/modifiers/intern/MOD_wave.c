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

/** \file blender/modifiers/intern/MOD_wave.c
 *  \ingroup modifiers
 */


#include "BLI_math.h"

#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"

#include "BLI_utildefines.h"
#include "BLI_string.h"


#include "BKE_DerivedMesh.h"
#include "BKE_object.h"
#include "BKE_deform.h"
#include "BKE_scene.h"

#include "depsgraph_private.h"

#include "MEM_guardedalloc.h"
#include "RE_shader_ext.h"

#include "MOD_modifiertypes.h"
#include "MOD_util.h"

static void initData(ModifierData *md)
{
	WaveModifierData *wmd = (WaveModifierData*) md; // whadya know, moved here from Iraq

	wmd->flag |= (MOD_WAVE_X | MOD_WAVE_Y | MOD_WAVE_CYCL
			| MOD_WAVE_NORM_X | MOD_WAVE_NORM_Y | MOD_WAVE_NORM_Z);

	wmd->objectcenter = NULL;
	wmd->texture = NULL;
	wmd->map_object = NULL;
	wmd->height= 0.5f;
	wmd->width= 1.5f;
	wmd->speed= 0.25f;
	wmd->narrow= 1.5f;
	wmd->lifetime= 0.0f;
	wmd->damp= 10.0f;
	wmd->falloff= 0.0f;
	wmd->texmapping = MOD_WAV_MAP_LOCAL;
	wmd->defgrp_name[0] = 0;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	WaveModifierData *wmd = (WaveModifierData*) md;
	WaveModifierData *twmd = (WaveModifierData*) target;

	twmd->damp = wmd->damp;
	twmd->flag = wmd->flag;
	twmd->height = wmd->height;
	twmd->lifetime = wmd->lifetime;
	twmd->narrow = wmd->narrow;
	twmd->speed = wmd->speed;
	twmd->startx = wmd->startx;
	twmd->starty = wmd->starty;
	twmd->timeoffs = wmd->timeoffs;
	twmd->width = wmd->width;
	twmd->falloff = wmd->falloff;
	twmd->objectcenter = wmd->objectcenter;
	twmd->texture = wmd->texture;
	twmd->map_object = wmd->map_object;
	twmd->texmapping = wmd->texmapping;
	BLI_strncpy(twmd->defgrp_name, wmd->defgrp_name, 32);
}

static int dependsOnTime(ModifierData *UNUSED(md))
{
	return 1;
}

static void foreachObjectLink(
					   ModifierData *md, Object *ob,
	ObjectWalkFunc walk, void *userData)
{
	WaveModifierData *wmd = (WaveModifierData*) md;

	walk(userData, ob, &wmd->objectcenter);
	walk(userData, ob, &wmd->map_object);
}

static void foreachIDLink(ModifierData *md, Object *ob,
					   IDWalkFunc walk, void *userData)
{
	WaveModifierData *wmd = (WaveModifierData*) md;

	walk(userData, ob, (ID **)&wmd->texture);

	foreachObjectLink(md, ob, (ObjectWalkFunc)walk, userData);
}

static void foreachTexLink(ModifierData *md, Object *ob,
					   TexWalkFunc walk, void *userData)
{
	walk(userData, ob, md, "texture");
}

static void updateDepgraph(ModifierData *md, DagForest *forest,
						Scene *UNUSED(scene),
						Object *UNUSED(ob),
						DagNode *obNode)
{
	WaveModifierData *wmd = (WaveModifierData*) md;

	if(wmd->objectcenter) {
		DagNode *curNode = dag_get_node(forest, wmd->objectcenter);

		dag_add_relation(forest, curNode, obNode, DAG_RL_OB_DATA,
			"Wave Modifier");
	}

	if(wmd->map_object) {
		DagNode *curNode = dag_get_node(forest, wmd->map_object);

		dag_add_relation(forest, curNode, obNode, DAG_RL_OB_DATA,
			"Wave Modifer");
	}
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	WaveModifierData *wmd = (WaveModifierData *)md;
	CustomDataMask dataMask = 0;


	/* ask for UV coordinates if we need them */
	if(wmd->texture && wmd->texmapping == MOD_WAV_MAP_UV)
		dataMask |= CD_MASK_MTFACE;

	/* ask for vertexgroups if we need them */
	if(wmd->defgrp_name[0])
		dataMask |= CD_MASK_MDEFORMVERT;

	return dataMask;
}

static void wavemod_get_texture_coords(WaveModifierData *wmd, Object *ob,
					   DerivedMesh *dm,
	   float (*co)[3], float (*texco)[3],
		   int numVerts)
{
	int i;
	int texmapping = wmd->texmapping;

	if(texmapping == MOD_WAV_MAP_OBJECT) {
		if(wmd->map_object)
			invert_m4_m4(wmd->map_object->imat, wmd->map_object->obmat);
		else /* if there is no map object, default to local */
			texmapping = MOD_WAV_MAP_LOCAL;
	}

	/* UVs need special handling, since they come from faces */
	if(texmapping == MOD_WAV_MAP_UV) {
		if(CustomData_has_layer(&dm->faceData, CD_MTFACE)) {
			MFace *mface = dm->getFaceArray(dm);
			MFace *mf;
			char *done = MEM_callocN(sizeof(*done) * numVerts,
					"get_texture_coords done");
			int numFaces = dm->getNumFaces(dm);
			char uvname[32];
			MTFace *tf;

			CustomData_validate_layer_name(&dm->faceData, CD_MTFACE, wmd->uvlayer_name, uvname);
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
			texmapping = MOD_WAV_MAP_LOCAL;
	}

	for(i = 0; i < numVerts; ++i, ++co, ++texco) {
		switch(texmapping) {
			case MOD_WAV_MAP_LOCAL:
				copy_v3_v3(*texco, *co);
				break;
			case MOD_WAV_MAP_GLOBAL:
				mul_v3_m4v3(*texco, ob->obmat, *co);
				break;
			case MOD_WAV_MAP_OBJECT:
				mul_v3_m4v3(*texco, ob->obmat, *co);
				mul_m4_v3(wmd->map_object->imat, *texco);
				break;
		}
	}
}

static void waveModifier_do(WaveModifierData *md, 
		Scene *scene, Object *ob, DerivedMesh *dm,
	   float (*vertexCos)[3], int numVerts)
{
	WaveModifierData *wmd = (WaveModifierData*) md;
	MVert *mvert = NULL;
	MDeformVert *dvert;
	int defgrp_index;
	float ctime = BKE_curframe(scene);
	float minfac =
			(float)(1.0 / exp(wmd->width * wmd->narrow * wmd->width * wmd->narrow));
	float lifefac = wmd->height;
	float (*tex_co)[3] = NULL;
	const int wmd_axis= wmd->flag & (MOD_WAVE_X|MOD_WAVE_Y);
	const float falloff= wmd->falloff;
	float falloff_fac= 1.0f; /* when falloff == 0.0f this stays at 1.0f */

	if(wmd->flag & MOD_WAVE_NORM && ob->type == OB_MESH)
		mvert = dm->getVertArray(dm);

	if(wmd->objectcenter){
		float mat[4][4];
		/* get the control object's location in local coordinates */
		invert_m4_m4(ob->imat, ob->obmat);
		mult_m4_m4m4(mat, ob->imat, wmd->objectcenter->obmat);

		wmd->startx = mat[3][0];
		wmd->starty = mat[3][1];
	}

	/* get the index of the deform group */
	modifier_get_vgroup(ob, dm, wmd->defgrp_name, &dvert, &defgrp_index);

	if(wmd->damp == 0) wmd->damp = 10.0f;

	if(wmd->lifetime != 0.0f) {
		float x = ctime - wmd->timeoffs;

		if(x > wmd->lifetime) {
			lifefac = x - wmd->lifetime;

			if(lifefac > wmd->damp) lifefac = 0.0;
			else lifefac =
				(float)(wmd->height * (1.0f - sqrtf(lifefac / wmd->damp)));
		}
	}

	if(wmd->texture) {
		tex_co = MEM_mallocN(sizeof(*tex_co) * numVerts,
					 "waveModifier_do tex_co");
		wavemod_get_texture_coords(wmd, ob, dm, vertexCos, tex_co, numVerts);
	}

	if(lifefac != 0.0f) {
		/* avoid divide by zero checks within the loop */
		float falloff_inv= falloff ? 1.0f / falloff : 1.0f;
		int i;

		for(i = 0; i < numVerts; i++) {
			float *co = vertexCos[i];
			float x = co[0] - wmd->startx;
			float y = co[1] - wmd->starty;
			float amplit= 0.0f;
			float def_weight= 1.0f;

			/* get weights */
			if(dvert) {
				def_weight= defvert_find_weight(&dvert[i], defgrp_index);

				/* if this vert isn't in the vgroup, don't deform it */
				if(def_weight == 0.0f) {
					continue;
				}
			}

			switch(wmd_axis) {
			case MOD_WAVE_X|MOD_WAVE_Y:
				amplit = sqrtf(x*x + y*y);
				break;
			case MOD_WAVE_X:
				amplit = x;
				break;
			case MOD_WAVE_Y:
				amplit = y;
				break;
			}

			/* this way it makes nice circles */
			amplit -= (ctime - wmd->timeoffs) * wmd->speed;

			if(wmd->flag & MOD_WAVE_CYCL) {
				amplit = (float)fmodf(amplit - wmd->width, 2.0f * wmd->width)
						+ wmd->width;
			}

			if(falloff != 0.0f) {
				float dist = 0.0f;

				switch(wmd_axis) {
				case MOD_WAVE_X|MOD_WAVE_Y:
					dist = sqrtf(x*x + y*y);
					break;
				case MOD_WAVE_X:
					dist = fabsf(x);
					break;
				case MOD_WAVE_Y:
					dist = fabsf(y);
					break;
				}

				falloff_fac = (1.0f - (dist * falloff_inv));
				CLAMP(falloff_fac, 0.0f, 1.0f);
			}

			/* GAUSSIAN */
			if((falloff_fac != 0.0f) && (amplit > -wmd->width) && (amplit < wmd->width)) {
				amplit = amplit * wmd->narrow;
				amplit = (float)(1.0f / expf(amplit * amplit) - minfac);

				/*apply texture*/
				if(wmd->texture) {
					TexResult texres;
					texres.nor = NULL;
					get_texture_value(wmd->texture, tex_co[i], &texres);
					amplit *= texres.tin;
				}

				/*apply weight & falloff */
				amplit *= def_weight * falloff_fac;

				if(mvert) {
					/* move along normals */
					if(wmd->flag & MOD_WAVE_NORM_X) {
						co[0] += (lifefac * amplit) * mvert[i].no[0] / 32767.0f;
					}
					if(wmd->flag & MOD_WAVE_NORM_Y) {
						co[1] += (lifefac * amplit) * mvert[i].no[1] / 32767.0f;
					}
					if(wmd->flag & MOD_WAVE_NORM_Z) {
						co[2] += (lifefac * amplit) * mvert[i].no[2] / 32767.0f;
					}
				}
				else {
					/* move along local z axis */
					co[2] += lifefac * amplit;
				}
			}
		}
	}

	if(wmd->texture) MEM_freeN(tex_co);
}

static void deformVerts(ModifierData *md, Object *ob,
						DerivedMesh *derivedData,
						float (*vertexCos)[3],
						int numVerts,
						int UNUSED(useRenderParams),
						int UNUSED(isFinalCalc))
{
	DerivedMesh *dm= derivedData;
	WaveModifierData *wmd = (WaveModifierData *)md;

	if(wmd->flag & MOD_WAVE_NORM)
		dm= get_cddm(ob, NULL, dm, vertexCos);
	else if(wmd->texture || wmd->defgrp_name[0])
		dm= get_dm(ob, NULL, dm, NULL, 0);

	waveModifier_do(wmd, md->scene, ob, dm, vertexCos, numVerts);

	if(dm != derivedData)
		dm->release(dm);
}

static void deformVertsEM(
					   ModifierData *md, Object *ob, struct EditMesh *editData,
	   DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts)
{
	DerivedMesh *dm= derivedData;
	WaveModifierData *wmd = (WaveModifierData *)md;

	if(wmd->flag & MOD_WAVE_NORM)
		dm= get_cddm(ob, editData, dm, vertexCos);
	else if(wmd->texture || wmd->defgrp_name[0])
		dm= get_dm(ob, editData, dm, NULL, 0);

	waveModifier_do(wmd, md->scene, ob, dm, vertexCos, numVerts);

	if(dm != derivedData)
		dm->release(dm);
}


ModifierTypeInfo modifierType_Wave = {
	/* name */              "Wave",
	/* structName */        "WaveModifierData",
	/* structSize */        sizeof(WaveModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsCVs
							| eModifierTypeFlag_SupportsEditmode,
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
	/* isDisabled */        NULL,
	/* updateDepgraph */    updateDepgraph,
	/* dependsOnTime */     dependsOnTime,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     foreachIDLink,
	/* foreachTexLink */    foreachTexLink,
};
