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

/** \file blender/modifiers/intern/MOD_weightvgedit.c
 *  \ingroup modifiers
 */

#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_rand.h"

#include "DNA_color_types.h"      /* CurveMapping. */
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_colortools.h"       /* CurveMapping. */
#include "BKE_deform.h"
#include "BKE_library.h"
#include "BKE_modifier.h"
#include "BKE_texture.h"          /* Texture masking. */

#include "depsgraph_private.h"
#include "MEM_guardedalloc.h"
#include "MOD_weightvg_util.h"

/**************************************
 * Modifiers functions.               *
 **************************************/
static void initData(ModifierData *md)
{
	WeightVGEditModifierData *wmd = (WeightVGEditModifierData *) md;
	wmd->edit_flags             = 0;
	wmd->falloff_type           = MOD_WVG_MAPPING_NONE;
	wmd->default_weight         = 0.0f;

	wmd->cmap_curve             = curvemapping_add(1, 0.0, 0.0, 1.0, 1.0);
	curvemapping_initialize(wmd->cmap_curve);

	wmd->rem_threshold          = 0.01f;
	wmd->add_threshold          = 0.01f;

	wmd->mask_constant          = 1.0f;
	wmd->mask_tex_use_channel   = MOD_WVG_MASK_TEX_USE_INT; /* Use intensity by default. */
	wmd->mask_tex_mapping       = MOD_DISP_MAP_LOCAL;
}

static void freeData(ModifierData *md)
{
	WeightVGEditModifierData *wmd = (WeightVGEditModifierData *) md;
	curvemapping_free(wmd->cmap_curve);

	if (wmd->mask_texture) {
		id_us_min(&wmd->mask_texture->id);
	}
}

static void copyData(ModifierData *md, ModifierData *target)
{
	WeightVGEditModifierData *wmd  = (WeightVGEditModifierData *) md;
	WeightVGEditModifierData *twmd = (WeightVGEditModifierData *) target;

	modifier_copyData_generic(md, target);

	twmd->cmap_curve = curvemapping_copy(wmd->cmap_curve);

	if (twmd->mask_texture) {
		id_us_plus(&twmd->mask_texture->id);
	}
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	WeightVGEditModifierData *wmd = (WeightVGEditModifierData *) md;
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
	WeightVGEditModifierData *wmd = (WeightVGEditModifierData *) md;

	if (wmd->mask_texture)
		return BKE_texture_dependsOnTime(wmd->mask_texture);
	return false;
}

static void foreachObjectLink(ModifierData *md, Object *ob,
                              void (*walk)(void *userData, Object *ob, Object **obpoin),
                              void *userData)
{
	WeightVGEditModifierData *wmd = (WeightVGEditModifierData *) md;
	walk(userData, ob, &wmd->mask_tex_map_obj);
}

static void foreachIDLink(ModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
	WeightVGEditModifierData *wmd = (WeightVGEditModifierData *) md;

	walk(userData, ob, (ID **)&wmd->mask_texture);

	foreachObjectLink(md, ob, (ObjectWalkFunc)walk, userData);
}

static void foreachTexLink(ModifierData *md, Object *ob, TexWalkFunc walk, void *userData)
{
	walk(userData, ob, md, "mask_texture");
}

static void updateDepgraph(ModifierData *md, DagForest *forest, struct Scene *UNUSED(scene),
                           Object *UNUSED(ob), DagNode *obNode)
{
	WeightVGEditModifierData *wmd = (WeightVGEditModifierData *) md;
	DagNode *curNode;

	if (wmd->mask_tex_map_obj && wmd->mask_tex_mapping == MOD_DISP_MAP_OBJECT) {
		curNode = dag_get_node(forest, wmd->mask_tex_map_obj);

		dag_add_relation(forest, curNode, obNode, DAG_RL_DATA_DATA | DAG_RL_OB_DATA,
		                 "WeightVGEdit Modifier");
	}

	if (wmd->mask_tex_mapping == MOD_DISP_MAP_GLOBAL)
		dag_add_relation(forest, obNode, obNode, DAG_RL_DATA_DATA | DAG_RL_OB_DATA,
		                 "WeightVGEdit Modifier");
}

static bool isDisabled(ModifierData *md, int UNUSED(useRenderParams))
{
	WeightVGEditModifierData *wmd = (WeightVGEditModifierData *) md;
	/* If no vertex group, bypass. */
	return (wmd->defgrp_name[0] == '\0');
}

static DerivedMesh *applyModifier(ModifierData *md, Object *ob, DerivedMesh *derivedData,
                                  ModifierApplyFlag UNUSED(flag))
{
	WeightVGEditModifierData *wmd = (WeightVGEditModifierData *) md;
	DerivedMesh *dm = derivedData;
	MDeformVert *dvert = NULL;
	MDeformWeight **dw = NULL;
	float *org_w; /* Array original weights. */
	float *new_w; /* Array new weights. */
	int numVerts;
	int defgrp_index;
	int i;
	/* Flags. */
	const bool do_add  = (wmd->edit_flags & MOD_WVG_EDIT_ADD2VG) != 0;
	const bool do_rem  = (wmd->edit_flags & MOD_WVG_EDIT_REMFVG) != 0;
	/* Only do weight-preview in Object, Sculpt and Pose modes! */
#if 0
	const bool do_prev = (wmd->modifier.mode & eModifierMode_DoWeightPreview);
#endif

	/* Get number of verts. */
	numVerts = dm->getNumVerts(dm);

	/* Check if we can just return the original mesh.
	 * Must have verts and therefore verts assigned to vgroups to do anything useful!
	 */
	if ((numVerts == 0) || BLI_listbase_is_empty(&ob->defbase))
		return dm;

	/* Get vgroup idx from its name. */
	defgrp_index = defgroup_name_index(ob, wmd->defgrp_name);
	if (defgrp_index == -1)
		return dm;

	dvert = CustomData_duplicate_referenced_layer(&dm->vertData, CD_MDEFORMVERT, numVerts);
	/* If no vertices were ever added to an object's vgroup, dvert might be NULL. */
	if (!dvert) {
		/* If this modifier is not allowed to add vertices, just return. */
		if (!do_add)
			return dm;
		/* Else, add a valid data layer! */
		dvert = CustomData_add_layer_named(&dm->vertData, CD_MDEFORMVERT, CD_CALLOC,
		                                   NULL, numVerts, wmd->defgrp_name);
		/* Ultimate security check. */
		if (!dvert)
			return dm;
	}

	/* Get org weights, assuming 0.0 for vertices not in given vgroup. */
	org_w = MEM_mallocN(sizeof(float) * numVerts, "WeightVGEdit Modifier, org_w");
	new_w = MEM_mallocN(sizeof(float) * numVerts, "WeightVGEdit Modifier, new_w");
	dw = MEM_mallocN(sizeof(MDeformWeight *) * numVerts, "WeightVGEdit Modifier, dw");
	for (i = 0; i < numVerts; i++) {
		dw[i] = defvert_find_index(&dvert[i], defgrp_index);
		if (dw[i]) {
			org_w[i] = new_w[i] = dw[i]->weight;
		}
		else {
			org_w[i] = new_w[i] = wmd->default_weight;
		}
	}

	/* Do mapping. */
	if (wmd->falloff_type != MOD_WVG_MAPPING_NONE) {
		RNG *rng = NULL;

		if (wmd->falloff_type == MOD_WVG_MAPPING_RANDOM)
			rng = BLI_rng_new_srandom(BLI_ghashutil_strhash(ob->id.name + 2));

		weightvg_do_map(numVerts, new_w, wmd->falloff_type, wmd->cmap_curve, rng);

		if (rng)
			BLI_rng_free(rng);
	}

	/* Do masking. */
	weightvg_do_mask(numVerts, NULL, org_w, new_w, ob, dm, wmd->mask_constant,
	                 wmd->mask_defgrp_name, wmd->modifier.scene, wmd->mask_texture,
	                 wmd->mask_tex_use_channel, wmd->mask_tex_mapping,
	                 wmd->mask_tex_map_obj, wmd->mask_tex_uvlayer_name);

	/* Update/add/remove from vgroup. */
	weightvg_update_vg(dvert, defgrp_index, dw, numVerts, NULL, org_w, do_add, wmd->add_threshold,
	                   do_rem, wmd->rem_threshold);

	/* If weight preview enabled... */
#if 0 /* XXX Currently done in mod stack :/ */
	if (do_prev)
		DM_update_weight_mcol(ob, dm, 0, org_w, 0, NULL);
#endif

	/* Freeing stuff. */
	MEM_freeN(org_w);
	MEM_freeN(new_w);
	MEM_freeN(dw);

	/* Return the vgroup-modified mesh. */
	return dm;
}


ModifierTypeInfo modifierType_WeightVGEdit = {
	/* name */              "VertexWeightEdit",
	/* structName */        "WeightVGEditModifierData",
	/* structSize */        sizeof(WeightVGEditModifierData),
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
	/* dependsOnTime */     dependsOnTime,
	/* dependsOnNormals */  NULL,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     foreachIDLink,
	/* foreachTexLink */    foreachTexLink,
};
