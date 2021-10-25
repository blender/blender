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
 * The Original Code is Copyright (C) 2011 by Bastien Montagne.
 * All rights reserved.
 *
 * Contributor(s): None yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/modifiers/intern/MOD_weightvgmix.c
 *  \ingroup modifiers
 */

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_listbase.h"

#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_deform.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_modifier.h"
#include "BKE_texture.h"          /* Texture masking. */

#include "depsgraph_private.h"
#include "DEG_depsgraph_build.h"

#include "MEM_guardedalloc.h"

#include "MOD_weightvg_util.h"
#include "MOD_modifiertypes.h"


/**
 * This mixes the old weight with the new weight factor.
 */
static float mix_weight(float weight, float weight2, char mix_mode)
{
#if 0
	/*
	 * XXX Don't know why, but the switch version takes many CPU time,
	 *     and produces lag in realtime playback...
	 */
	switch (mix_mode)
	{
		case MOD_WVG_MIX_ADD:
			return (weight + weight2);
		case MOD_WVG_MIX_SUB:
			return (weight - weight2);
		case MOD_WVG_MIX_MUL:
			return (weight * weight2);
		case MOD_WVG_MIX_DIV:
			/* Avoid dividing by zero (or really small values). */
			if (0.0 <= weight2 < MOD_WVG_ZEROFLOOR)
				weight2 = MOD_WVG_ZEROFLOOR;
			else if (-MOD_WVG_ZEROFLOOR < weight2)
				weight2 = -MOD_WVG_ZEROFLOOR;
			return (weight / weight2);
		case MOD_WVG_MIX_DIF:
			return (weight < weight2 ? weight2 - weight : weight - weight2);
		case MOD_WVG_MIX_AVG:
			return (weight + weight2) / 2.0;
		case MOD_WVG_MIX_SET:
		default:
			return weight2;
	}
#endif
	if (mix_mode == MOD_WVG_MIX_SET)
		return weight2;
	else if (mix_mode == MOD_WVG_MIX_ADD)
		return (weight + weight2);
	else if (mix_mode == MOD_WVG_MIX_SUB)
		return (weight - weight2);
	else if (mix_mode == MOD_WVG_MIX_MUL)
		return (weight * weight2);
	else if (mix_mode == MOD_WVG_MIX_DIV) {
		/* Avoid dividing by zero (or really small values). */
		if (weight2 < 0.0f && weight2 > -MOD_WVG_ZEROFLOOR)
			weight2 = -MOD_WVG_ZEROFLOOR;
		else if (weight2 >= 0.0f && weight2 < MOD_WVG_ZEROFLOOR)
			weight2 = MOD_WVG_ZEROFLOOR;
		return (weight / weight2);
	}
	else if (mix_mode == MOD_WVG_MIX_DIF)
		return (weight < weight2 ? weight2 - weight : weight - weight2);
	else if (mix_mode == MOD_WVG_MIX_AVG)
		return (weight + weight2) * 0.5f;
	else return weight2;
}

/**************************************
 * Modifiers functions.               *
 **************************************/
static void initData(ModifierData *md)
{
	WeightVGMixModifierData *wmd = (WeightVGMixModifierData *) md;

	wmd->default_weight_a       = 0.0f;
	wmd->default_weight_b       = 0.0f;
	wmd->mix_mode               = MOD_WVG_MIX_SET;
	wmd->mix_set                = MOD_WVG_SET_AND;

	wmd->mask_constant          = 1.0f;
	wmd->mask_tex_use_channel   = MOD_WVG_MASK_TEX_USE_INT; /* Use intensity by default. */
	wmd->mask_tex_mapping       = MOD_DISP_MAP_LOCAL;
}

static void freeData(ModifierData *md)
{
	WeightVGMixModifierData *wmd = (WeightVGMixModifierData *) md;
	if (wmd->mask_texture) {
		id_us_min(&wmd->mask_texture->id);
	}
}

static void copyData(ModifierData *md, ModifierData *target)
{
#if 0
	WeightVGMixModifierData *wmd  = (WeightVGMixModifierData *) md;
#endif
	WeightVGMixModifierData *twmd = (WeightVGMixModifierData *) target;

	modifier_copyData_generic(md, target);

	if (twmd->mask_texture) {
		id_us_plus(&twmd->mask_texture->id);
	}
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	WeightVGMixModifierData *wmd = (WeightVGMixModifierData *) md;
	CustomDataMask dataMask = 0;

	/* We need vertex groups! */
	dataMask |= CD_MASK_MDEFORMVERT;

	/* Ask for UV coordinates if we need them. */
	if (wmd->mask_tex_mapping == MOD_DISP_MAP_UV)
		dataMask |= CD_MASK_MTFACE;

	/* No need to ask for CD_PREVIEW_MLOOPCOL... */

	return dataMask;
}

static bool dependsOnTime(ModifierData *md)
{
	WeightVGMixModifierData *wmd = (WeightVGMixModifierData *) md;

	if (wmd->mask_texture)
		return BKE_texture_dependsOnTime(wmd->mask_texture);
	return false;
}

static void foreachObjectLink(ModifierData *md, Object *ob, ObjectWalkFunc walk, void *userData)
{
	WeightVGMixModifierData *wmd = (WeightVGMixModifierData *) md;
	walk(userData, ob, &wmd->mask_tex_map_obj, IDWALK_CB_NOP);
}

static void foreachIDLink(ModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
	WeightVGMixModifierData *wmd = (WeightVGMixModifierData *) md;

	walk(userData, ob, (ID **)&wmd->mask_texture, IDWALK_CB_USER);

	foreachObjectLink(md, ob, (ObjectWalkFunc)walk, userData);
}

static void foreachTexLink(ModifierData *md, Object *ob, TexWalkFunc walk, void *userData)
{
	walk(userData, ob, md, "mask_texture");
}

static void updateDepgraph(ModifierData *md, DagForest *forest,
                           struct Main *UNUSED(bmain),
                           struct Scene *UNUSED(scene),
                           Object *UNUSED(ob), DagNode *obNode)
{
	WeightVGMixModifierData *wmd = (WeightVGMixModifierData *) md;
	DagNode *curNode;

	if (wmd->mask_tex_map_obj && wmd->mask_tex_mapping == MOD_DISP_MAP_OBJECT) {
		curNode = dag_get_node(forest, wmd->mask_tex_map_obj);

		dag_add_relation(forest, curNode, obNode, DAG_RL_DATA_DATA | DAG_RL_OB_DATA,
		                 "WeightVGMix Modifier");
	}

	if (wmd->mask_tex_mapping == MOD_DISP_MAP_GLOBAL)
		dag_add_relation(forest, obNode, obNode, DAG_RL_DATA_DATA | DAG_RL_OB_DATA,
		                 "WeightVGMix Modifier");
}

static void updateDepsgraph(ModifierData *md,
                            struct Main *UNUSED(bmain),
                            struct Scene *UNUSED(scene),
                            Object *ob,
                            struct DepsNodeHandle *node)
{
	WeightVGMixModifierData *wmd = (WeightVGMixModifierData *) md;
	if (wmd->mask_tex_map_obj != NULL && wmd->mask_tex_mapping == MOD_DISP_MAP_OBJECT) {
		DEG_add_object_relation(node, wmd->mask_tex_map_obj, DEG_OB_COMP_TRANSFORM, "WeightVGMix Modifier");
		DEG_add_object_relation(node, wmd->mask_tex_map_obj, DEG_OB_COMP_GEOMETRY, "WeightVGMix Modifier");
	}
	if (wmd->mask_tex_mapping == MOD_DISP_MAP_GLOBAL) {
		DEG_add_object_relation(node, ob, DEG_OB_COMP_TRANSFORM, "WeightVGMix Modifier");
		DEG_add_object_relation(node, ob, DEG_OB_COMP_GEOMETRY, "WeightVGMix Modifier");
	}
}

static bool isDisabled(ModifierData *md, int UNUSED(useRenderParams))
{
	WeightVGMixModifierData *wmd = (WeightVGMixModifierData *) md;
	/* If no vertex group, bypass. */
	return (wmd->defgrp_name_a[0] == '\0');
}

static DerivedMesh *applyModifier(ModifierData *md, Object *ob, DerivedMesh *derivedData,
                                  ModifierApplyFlag UNUSED(flag))
{
	WeightVGMixModifierData *wmd = (WeightVGMixModifierData *) md;
	DerivedMesh *dm = derivedData;
	MDeformVert *dvert = NULL;
	MDeformWeight **dw1, **tdw1, **dw2, **tdw2;
	int numVerts;
	int defgrp_index, defgrp_index_other = -1;
	float *org_w;
	float *new_w;
	int *tidx, *indices = NULL;
	int numIdx = 0;
	int i;
	/* Flags. */
#if 0
	const bool do_prev = (wmd->modifier.mode & eModifierMode_DoWeightPreview) != 0;
#endif

	/* Get number of verts. */
	numVerts = dm->getNumVerts(dm);

	/* Check if we can just return the original mesh.
	 * Must have verts and therefore verts assigned to vgroups to do anything useful!
	 */
	if ((numVerts == 0) || BLI_listbase_is_empty(&ob->defbase))
		return dm;

	/* Get vgroup idx from its name. */
	defgrp_index = defgroup_name_index(ob, wmd->defgrp_name_a);
	if (defgrp_index == -1)
		return dm;
	/* Get second vgroup idx from its name, if given. */
	if (wmd->defgrp_name_b[0] != (char)0) {
		defgrp_index_other = defgroup_name_index(ob, wmd->defgrp_name_b);
		if (defgrp_index_other == -1)
			return dm;
	}

	dvert = CustomData_duplicate_referenced_layer(&dm->vertData, CD_MDEFORMVERT, numVerts);
	/* If no vertices were ever added to an object's vgroup, dvert might be NULL. */
	if (!dvert) {
		/* If not affecting all vertices, just return. */
		if (wmd->mix_set != MOD_WVG_SET_ALL)
			return dm;
		/* Else, add a valid data layer! */
		dvert = CustomData_add_layer(&dm->vertData, CD_MDEFORMVERT, CD_CALLOC, NULL, numVerts);
		/* Ultimate security check. */
		if (!dvert)
			return dm;
	}
	/* Find out which vertices to work on. */
	tidx = MEM_malloc_arrayN(numVerts, sizeof(int), "WeightVGMix Modifier, tidx");
	tdw1 = MEM_malloc_arrayN(numVerts, sizeof(MDeformWeight *), "WeightVGMix Modifier, tdw1");
	tdw2 = MEM_malloc_arrayN(numVerts, sizeof(MDeformWeight *), "WeightVGMix Modifier, tdw2");
	switch (wmd->mix_set) {
		case MOD_WVG_SET_A:
			/* All vertices in first vgroup. */
			for (i = 0; i < numVerts; i++) {
				MDeformWeight *dw = defvert_find_index(&dvert[i], defgrp_index);
				if (dw) {
					tdw1[numIdx] = dw;
					tdw2[numIdx] = (defgrp_index_other >= 0) ? defvert_find_index(&dvert[i], defgrp_index_other) : NULL;
					tidx[numIdx++] = i;
				}
			}
			break;
		case MOD_WVG_SET_B:
			/* All vertices in second vgroup. */
			for (i = 0; i < numVerts; i++) {
				MDeformWeight *dw = (defgrp_index_other >= 0) ? defvert_find_index(&dvert[i], defgrp_index_other) : NULL;
				if (dw) {
					tdw1[numIdx] = defvert_find_index(&dvert[i], defgrp_index);
					tdw2[numIdx] = dw;
					tidx[numIdx++] = i;
				}
			}
			break;
		case MOD_WVG_SET_OR:
			/* All vertices in one vgroup or the other. */
			for (i = 0; i < numVerts; i++) {
				MDeformWeight *adw = defvert_find_index(&dvert[i], defgrp_index);
				MDeformWeight *bdw = (defgrp_index_other >= 0) ? defvert_find_index(&dvert[i], defgrp_index_other) : NULL;
				if (adw || bdw) {
					tdw1[numIdx] = adw;
					tdw2[numIdx] = bdw;
					tidx[numIdx++] = i;
				}
			}
			break;
		case MOD_WVG_SET_AND:
			/* All vertices in both vgroups. */
			for (i = 0; i < numVerts; i++) {
				MDeformWeight *adw = defvert_find_index(&dvert[i], defgrp_index);
				MDeformWeight *bdw = (defgrp_index_other >= 0) ? defvert_find_index(&dvert[i], defgrp_index_other) : NULL;
				if (adw && bdw) {
					tdw1[numIdx] = adw;
					tdw2[numIdx] = bdw;
					tidx[numIdx++] = i;
				}
			}
			break;
		case MOD_WVG_SET_ALL:
		default:
			/* Use all vertices. */
			for (i = 0; i < numVerts; i++) {
				tdw1[i] = defvert_find_index(&dvert[i], defgrp_index);
				tdw2[i] = (defgrp_index_other >= 0) ? defvert_find_index(&dvert[i], defgrp_index_other) : NULL;
			}
			numIdx = -1;
			break;
	}
	if (numIdx == 0) {
		/* Use no vertices! Hence, return org data. */
		MEM_freeN(tdw1);
		MEM_freeN(tdw2);
		MEM_freeN(tidx);
		return dm;
	}
	if (numIdx != -1) {
		indices = MEM_malloc_arrayN(numIdx, sizeof(int), "WeightVGMix Modifier, indices");
		memcpy(indices, tidx, sizeof(int) * numIdx);
		dw1 = MEM_malloc_arrayN(numIdx, sizeof(MDeformWeight *), "WeightVGMix Modifier, dw1");
		memcpy(dw1, tdw1, sizeof(MDeformWeight *) * numIdx);
		MEM_freeN(tdw1);
		dw2 = MEM_malloc_arrayN(numIdx, sizeof(MDeformWeight *), "WeightVGMix Modifier, dw2");
		memcpy(dw2, tdw2, sizeof(MDeformWeight *) * numIdx);
		MEM_freeN(tdw2);
	}
	else {
		/* Use all vertices. */
		numIdx = numVerts;
		/* Just copy MDeformWeight pointers arrays, they will be freed at the end. */
		dw1 = tdw1;
		dw2 = tdw2;
	}
	MEM_freeN(tidx);

	org_w = MEM_malloc_arrayN(numIdx, sizeof(float), "WeightVGMix Modifier, org_w");
	new_w = MEM_malloc_arrayN(numIdx, sizeof(float), "WeightVGMix Modifier, new_w");

	/* Mix weights. */
	for (i = 0; i < numIdx; i++) {
		float weight2;
		org_w[i] = dw1[i] ? dw1[i]->weight : wmd->default_weight_a;
		weight2  = dw2[i] ? dw2[i]->weight : wmd->default_weight_b;

		new_w[i] = mix_weight(org_w[i], weight2, wmd->mix_mode);
	}

	/* Do masking. */
	weightvg_do_mask(numIdx, indices, org_w, new_w, ob, dm, wmd->mask_constant,
	                 wmd->mask_defgrp_name, wmd->modifier.scene, wmd->mask_texture,
	                 wmd->mask_tex_use_channel, wmd->mask_tex_mapping,
	                 wmd->mask_tex_map_obj, wmd->mask_tex_uvlayer_name);

	/* Update (add to) vgroup.
	 * XXX Depending on the MOD_WVG_SET_xxx option chosen, we might have to add vertices to vgroup.
	 */
	weightvg_update_vg(dvert, defgrp_index, dw1, numIdx, indices, org_w, true, -FLT_MAX, false, 0.0f);

	/* If weight preview enabled... */
#if 0 /* XXX Currently done in mod stack :/ */
	if (do_prev)
		DM_update_weight_mcol(ob, dm, 0, org_w, numIdx, indices);
#endif

	/* Freeing stuff. */
	MEM_freeN(org_w);
	MEM_freeN(new_w);
	MEM_freeN(dw1);
	MEM_freeN(dw2);

	if (indices)
		MEM_freeN(indices);

	/* Return the vgroup-modified mesh. */
	return dm;
}


ModifierTypeInfo modifierType_WeightVGMix = {
	/* name */              "VertexWeightMix",
	/* structName */        "WeightVGMixModifierData",
	/* structSize */        sizeof(WeightVGMixModifierData),
	/* type */              eModifierTypeType_NonGeometrical,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_SupportsMapping |
	                        eModifierTypeFlag_SupportsEditmode |
	                        eModifierTypeFlag_UsesPreview,

	/* copyData */          copyData,
	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   NULL,
	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          freeData,
	/* isDisabled */        isDisabled,
	/* updateDepgraph */    updateDepgraph,
	/* updateDepsgraph */   updateDepsgraph,
	/* dependsOnTime */     dependsOnTime,
	/* dependsOnNormals */  NULL,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     foreachIDLink,
	/* foreachTexLink */    foreachTexLink,
};
