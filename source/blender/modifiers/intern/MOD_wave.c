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


#include "BKE_deform.h"
#include "BKE_DerivedMesh.h"
#include "BKE_library.h"
#include "BKE_scene.h"
#include "BKE_texture.h"

#include "depsgraph_private.h"

#include "MEM_guardedalloc.h"
#include "RE_shader_ext.h"

#include "MOD_modifiertypes.h"
#include "MOD_util.h"

static void initData(ModifierData *md)
{
	WaveModifierData *wmd = (WaveModifierData *) md; // whadya know, moved here from Iraq

	wmd->flag |= (MOD_WAVE_X | MOD_WAVE_Y | MOD_WAVE_CYCL |
	              MOD_WAVE_NORM_X | MOD_WAVE_NORM_Y | MOD_WAVE_NORM_Z);

	wmd->objectcenter = NULL;
	wmd->texture = NULL;
	wmd->map_object = NULL;
	wmd->height = 0.5f;
	wmd->width = 1.5f;
	wmd->speed = 0.25f;
	wmd->narrow = 1.5f;
	wmd->lifetime = 0.0f;
	wmd->damp = 10.0f;
	wmd->falloff = 0.0f;
	wmd->texmapping = MOD_DISP_MAP_LOCAL;
	wmd->defgrp_name[0] = 0;
}

static void freeData(ModifierData *md)
{
	WaveModifierData *wmd = (WaveModifierData *) md;
	if (wmd->texture) {
		id_us_min(&wmd->texture->id);
	}
}

static void copyData(ModifierData *md, ModifierData *target)
{
#if 0
	WaveModifierData *wmd = (WaveModifierData *) md;
#endif
	WaveModifierData *twmd = (WaveModifierData *) target;

	modifier_copyData_generic(md, target);

	if (twmd->texture) {
		id_us_plus(&twmd->texture->id);
	}
}

static bool dependsOnTime(ModifierData *UNUSED(md))
{
	return true;
}

static void foreachObjectLink(
        ModifierData *md, Object *ob,
        ObjectWalkFunc walk, void *userData)
{
	WaveModifierData *wmd = (WaveModifierData *) md;

	walk(userData, ob, &wmd->objectcenter);
	walk(userData, ob, &wmd->map_object);
}

static void foreachIDLink(ModifierData *md, Object *ob,
                          IDWalkFunc walk, void *userData)
{
	WaveModifierData *wmd = (WaveModifierData *) md;

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
	WaveModifierData *wmd = (WaveModifierData *) md;

	if (wmd->objectcenter) {
		DagNode *curNode = dag_get_node(forest, wmd->objectcenter);

		dag_add_relation(forest, curNode, obNode, DAG_RL_OB_DATA,
		                 "Wave Modifier");
	}

	if (wmd->map_object) {
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
	if (wmd->texture && wmd->texmapping == MOD_DISP_MAP_UV)
		dataMask |= CD_MASK_MTFACE;

	/* ask for vertexgroups if we need them */
	if (wmd->defgrp_name[0])
		dataMask |= CD_MASK_MDEFORMVERT;

	return dataMask;
}

static void waveModifier_do(WaveModifierData *md, 
                            Scene *scene, Object *ob, DerivedMesh *dm,
                            float (*vertexCos)[3], int numVerts)
{
	WaveModifierData *wmd = (WaveModifierData *) md;
	MVert *mvert = NULL;
	MDeformVert *dvert;
	int defgrp_index;
	float ctime = BKE_scene_frame_get(scene);
	float minfac = (float)(1.0 / exp(wmd->width * wmd->narrow * wmd->width * wmd->narrow));
	float lifefac = wmd->height;
	float (*tex_co)[3] = NULL;
	const int wmd_axis = wmd->flag & (MOD_WAVE_X | MOD_WAVE_Y);
	const float falloff = wmd->falloff;
	float falloff_fac = 1.0f; /* when falloff == 0.0f this stays at 1.0f */

	if ((wmd->flag & MOD_WAVE_NORM) && (ob->type == OB_MESH))
		mvert = dm->getVertArray(dm);

	if (wmd->objectcenter) {
		float mat[4][4];
		/* get the control object's location in local coordinates */
		invert_m4_m4(ob->imat, ob->obmat);
		mul_m4_m4m4(mat, ob->imat, wmd->objectcenter->obmat);

		wmd->startx = mat[3][0];
		wmd->starty = mat[3][1];
	}

	/* get the index of the deform group */
	modifier_get_vgroup(ob, dm, wmd->defgrp_name, &dvert, &defgrp_index);

	if (wmd->damp == 0) wmd->damp = 10.0f;

	if (wmd->lifetime != 0.0f) {
		float x = ctime - wmd->timeoffs;

		if (x > wmd->lifetime) {
			lifefac = x - wmd->lifetime;

			if (lifefac > wmd->damp) lifefac = 0.0;
			else lifefac = (float)(wmd->height * (1.0f - sqrtf(lifefac / wmd->damp)));
		}
	}

	if (wmd->texture) {
		tex_co = MEM_mallocN(sizeof(*tex_co) * numVerts,
		                     "waveModifier_do tex_co");
		get_texture_coords((MappingInfoModifierData *)wmd, ob, dm, vertexCos, tex_co, numVerts);

		modifier_init_texture(wmd->modifier.scene, wmd->texture);
	}

	if (lifefac != 0.0f) {
		/* avoid divide by zero checks within the loop */
		float falloff_inv = falloff ? 1.0f / falloff : 1.0f;
		int i;

		for (i = 0; i < numVerts; i++) {
			float *co = vertexCos[i];
			float x = co[0] - wmd->startx;
			float y = co[1] - wmd->starty;
			float amplit = 0.0f;
			float def_weight = 1.0f;

			/* get weights */
			if (dvert) {
				def_weight = defvert_find_weight(&dvert[i], defgrp_index);

				/* if this vert isn't in the vgroup, don't deform it */
				if (def_weight == 0.0f) {
					continue;
				}
			}

			switch (wmd_axis) {
				case MOD_WAVE_X | MOD_WAVE_Y:
					amplit = sqrtf(x * x + y * y);
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

			if (wmd->flag & MOD_WAVE_CYCL) {
				amplit = (float)fmodf(amplit - wmd->width, 2.0f * wmd->width) +
				         wmd->width;
			}

			if (falloff != 0.0f) {
				float dist = 0.0f;

				switch (wmd_axis) {
					case MOD_WAVE_X | MOD_WAVE_Y:
						dist = sqrtf(x * x + y * y);
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
			if ((falloff_fac != 0.0f) && (amplit > -wmd->width) && (amplit < wmd->width)) {
				amplit = amplit * wmd->narrow;
				amplit = (float)(1.0f / expf(amplit * amplit) - minfac);

				/*apply texture*/
				if (wmd->texture) {
					TexResult texres;
					texres.nor = NULL;
					BKE_texture_get_value(wmd->modifier.scene, wmd->texture, tex_co[i], &texres, false);
					amplit *= texres.tin;
				}

				/*apply weight & falloff */
				amplit *= def_weight * falloff_fac;

				if (mvert) {
					/* move along normals */
					if (wmd->flag & MOD_WAVE_NORM_X) {
						co[0] += (lifefac * amplit) * mvert[i].no[0] / 32767.0f;
					}
					if (wmd->flag & MOD_WAVE_NORM_Y) {
						co[1] += (lifefac * amplit) * mvert[i].no[1] / 32767.0f;
					}
					if (wmd->flag & MOD_WAVE_NORM_Z) {
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

	if (wmd->texture) MEM_freeN(tex_co);
}

static void deformVerts(ModifierData *md, Object *ob,
                        DerivedMesh *derivedData,
                        float (*vertexCos)[3],
                        int numVerts,
                        ModifierApplyFlag UNUSED(flag))
{
	DerivedMesh *dm = derivedData;
	WaveModifierData *wmd = (WaveModifierData *)md;

	if (wmd->flag & MOD_WAVE_NORM)
		dm = get_cddm(ob, NULL, dm, vertexCos, false);
	else if (wmd->texture || wmd->defgrp_name[0])
		dm = get_dm(ob, NULL, dm, NULL, false, false);

	waveModifier_do(wmd, md->scene, ob, dm, vertexCos, numVerts);

	if (dm != derivedData)
		dm->release(dm);
}

static void deformVertsEM(
        ModifierData *md, Object *ob, struct BMEditMesh *editData,
        DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts)
{
	DerivedMesh *dm = derivedData;
	WaveModifierData *wmd = (WaveModifierData *)md;

	if (wmd->flag & MOD_WAVE_NORM)
		dm = get_cddm(ob, editData, dm, vertexCos, false);
	else if (wmd->texture || wmd->defgrp_name[0])
		dm = get_dm(ob, editData, dm, NULL, false, false);

	waveModifier_do(wmd, md->scene, ob, dm, vertexCos, numVerts);

	if (dm != derivedData)
		dm->release(dm);
}


ModifierTypeInfo modifierType_Wave = {
	/* name */              "Wave",
	/* structName */        "WaveModifierData",
	/* structSize */        sizeof(WaveModifierData),
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
	/* freeData */          freeData,
	/* isDisabled */        NULL,
	/* updateDepgraph */    updateDepgraph,
	/* dependsOnTime */     dependsOnTime,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     foreachIDLink,
	/* foreachTexLink */    foreachTexLink,
};
