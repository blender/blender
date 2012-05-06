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

/** \file blender/modifiers/intern/MOD_displace.c
 *  \ingroup modifiers
 */


#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_string.h"


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
	BLI_strncpy(tdmd->defgrp_name, dmd->defgrp_name, sizeof(tdmd->defgrp_name));
	tdmd->midlevel = dmd->midlevel;
	tdmd->texmapping = dmd->texmapping;
	tdmd->map_object = dmd->map_object;
	BLI_strncpy(tdmd->uvlayer_name, dmd->uvlayer_name, sizeof(tdmd->uvlayer_name));
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	DisplaceModifierData *dmd = (DisplaceModifierData *)md;
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups if we need them */
	if (dmd->defgrp_name[0]) dataMask |= CD_MASK_MDEFORMVERT;

	/* ask for UV coordinates if we need them */
	if (dmd->texmapping == MOD_DISP_MAP_UV) dataMask |= CD_MASK_MTFACE;

	return dataMask;
}

static int dependsOnTime(ModifierData *md)
{
	DisplaceModifierData *dmd = (DisplaceModifierData *)md;

	if (dmd->texture) {
		return BKE_texture_dependsOnTime(dmd->texture);
	}
	else {
		return 0;
	}
}

static int dependsOnNormals(ModifierData *md)
{
	DisplaceModifierData *dmd = (DisplaceModifierData *)md;
	return (dmd->direction == MOD_DISP_DIR_NOR);
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

static void foreachTexLink(ModifierData *md, Object *ob,
					   TexWalkFunc walk, void *userData)
{
	walk(userData, ob, md, "texture");
}

static int isDisabled(ModifierData *md, int UNUSED(useRenderParams))
{
	DisplaceModifierData *dmd = (DisplaceModifierData*) md;

	return (!dmd->texture || dmd->strength == 0.0f);
}

static void updateDepgraph(ModifierData *md, DagForest *forest,
						struct Scene *UNUSED(scene),
						Object *UNUSED(ob),
						DagNode *obNode)
{
	DisplaceModifierData *dmd = (DisplaceModifierData*) md;

	if (dmd->map_object && dmd->texmapping == MOD_DISP_MAP_OBJECT) {
		DagNode *curNode = dag_get_node(forest, dmd->map_object);

		dag_add_relation(forest, curNode, obNode,
		                 DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Displace Modifier");
	}
	

	if (dmd->texmapping == MOD_DISP_MAP_GLOBAL)
		dag_add_relation(forest, obNode, obNode,
		                 DAG_RL_DATA_DATA | DAG_RL_OB_DATA, "Displace Modifier");
	
}

/* dm must be a CDDerivedMesh */
static void displaceModifier_do(
				DisplaceModifierData *dmd, Object *ob,
	DerivedMesh *dm, float (*vertexCos)[3], int numVerts)
{
	int i;
	MVert *mvert;
	MDeformVert *dvert;
	int defgrp_index;
	float (*tex_co)[3];
	float weight= 1.0f; /* init value unused but some compilers may complain */

	if (!dmd->texture) return;
	if (dmd->strength == 0.0f) return;

	mvert = CDDM_get_verts(dm);
	modifier_get_vgroup(ob, dm, dmd->defgrp_name, &dvert, &defgrp_index);

	tex_co = MEM_callocN(sizeof(*tex_co) * numVerts,
				 "displaceModifier_do tex_co");
	get_texture_coords((MappingInfoModifierData *)dmd, ob, dm, vertexCos, tex_co, numVerts);

	modifier_init_texture(dmd->modifier.scene, dmd->texture);

	for (i = 0; i < numVerts; ++i) {
		TexResult texres;
		float delta = 0, strength = dmd->strength;

		if (dvert) {
			weight= defvert_find_weight(dvert + i, defgrp_index);
			if (weight == 0.0f) continue;
		}

		texres.nor = NULL;
		get_texture_value(dmd->texture, tex_co[i], &texres);

		delta = texres.tin - dmd->midlevel;

		if (dvert) strength *= weight;

		delta *= strength;
		CLAMP(delta, -10000, 10000);

		switch (dmd->direction) {
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
				vertexCos[i][0] += delta * (mvert[i].no[0] / 32767.0f);
				vertexCos[i][1] += delta * (mvert[i].no[1] / 32767.0f);
				vertexCos[i][2] += delta * (mvert[i].no[2] / 32767.0f);
				break;
		}
	}

	MEM_freeN(tex_co);
}

static void deformVerts(ModifierData *md, Object *ob,
						DerivedMesh *derivedData,
						float (*vertexCos)[3],
						int numVerts,
						int UNUSED(useRenderParams),
						int UNUSED(isFinalCalc))
{
	DerivedMesh *dm= get_cddm(ob, NULL, derivedData, vertexCos);

	displaceModifier_do((DisplaceModifierData *)md, ob, dm,
	                    vertexCos, numVerts);

	if (dm != derivedData)
		dm->release(dm);
}

static void deformVertsEM(
					   ModifierData *md, Object *ob, struct BMEditMesh *editData,
	DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts)
{
	DerivedMesh *dm= get_cddm(ob, editData, derivedData, vertexCos);

	displaceModifier_do((DisplaceModifierData *)md, ob, dm,
	                    vertexCos, numVerts);

	if (dm != derivedData)
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
	/* dependsOnTime */     dependsOnTime,
	/* dependsOnNormals */	dependsOnNormals,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     foreachIDLink,
	/* foreachTexLink */    foreachTexLink,
};
