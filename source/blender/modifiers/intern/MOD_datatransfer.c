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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): None yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/modifiers/intern/MOD_datatransfer.c
 *  \ingroup modifiers
 */

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BKE_customdata.h"
#include "BKE_object_data_transfer.h"
#include "BKE_DerivedMesh.h"
#include "BKE_library.h"
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_remap.h"
#include "BKE_modifier.h"
#include "BKE_report.h"

#include "MEM_guardedalloc.h"
#include "MOD_util.h"

#include "depsgraph_private.h"

/**************************************
 * Modifiers functions.               *
 **************************************/
static void initData(ModifierData *md)
{
	DataTransferModifierData *dtmd = (DataTransferModifierData *) md;
	int i;

	dtmd->ob_source          = NULL;
	dtmd->data_types         = 0;

	dtmd->vmap_mode          = MREMAP_MODE_VERT_NEAREST;
	dtmd->emap_mode          = MREMAP_MODE_EDGE_NEAREST;
	dtmd->lmap_mode          = MREMAP_MODE_LOOP_NEAREST_POLYNOR;
	dtmd->pmap_mode          = MREMAP_MODE_POLY_NEAREST;

	dtmd->map_max_distance   = 1.0f;
	dtmd->map_ray_radius     = 0.0f;

	for (i = 0; i < DT_MULTILAYER_INDEX_MAX; i++) {
		dtmd->layers_select_src[i] = DT_LAYERS_ALL_SRC;
		dtmd->layers_select_dst[i]   = DT_LAYERS_NAME_DST;
	}

	dtmd->mix_mode           = CDT_MIX_TRANSFER;
	dtmd->mix_factor         = 1.0f;
	dtmd->defgrp_name[0]     = '\0';

	dtmd->flags              = MOD_DATATRANSFER_OBSRC_TRANSFORM;
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	DataTransferModifierData *dtmd = (DataTransferModifierData *) md;
	CustomDataMask dataMask = 0;

	if (dtmd->defgrp_name[0]) {
		/* We need vertex groups! */
		dataMask |= CD_MASK_MDEFORMVERT;
	}

	dataMask |= BKE_object_data_transfer_dttypes_to_cdmask(dtmd->data_types);

	return dataMask;
}

static bool dependsOnNormals(ModifierData *md)
{
	DataTransferModifierData *dtmd = (DataTransferModifierData *) md;
	int item_types = BKE_object_data_transfer_get_dttypes_item_types(dtmd->data_types);

	if ((item_types & ME_VERT) && (dtmd->vmap_mode & (MREMAP_USE_NORPROJ | MREMAP_USE_NORMAL))) {
		return true;
	}
	if ((item_types & ME_EDGE) && (dtmd->emap_mode & (MREMAP_USE_NORPROJ | MREMAP_USE_NORMAL))) {
		return true;
	}
	if ((item_types & ME_LOOP) && (dtmd->lmap_mode & (MREMAP_USE_NORPROJ | MREMAP_USE_NORMAL))) {
		return true;
	}
	if ((item_types & ME_POLY) && (dtmd->pmap_mode & (MREMAP_USE_NORPROJ | MREMAP_USE_NORMAL))) {
		return true;
	}

	return false;
}

static void foreachObjectLink(ModifierData *md, Object *ob,
                              void (*walk)(void *userData, Object *ob, Object **obpoin),
                              void *userData)
{
	DataTransferModifierData *dtmd = (DataTransferModifierData *) md;
	walk(userData, ob, &dtmd->ob_source);
}

static void foreachIDLink(ModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
	foreachObjectLink(md, ob, (ObjectWalkFunc)walk, userData);
}

static void updateDepgraph(ModifierData *md, DagForest *forest, struct Scene *UNUSED(scene),
                           Object *UNUSED(ob), DagNode *obNode)
{
	DataTransferModifierData *dtmd = (DataTransferModifierData *) md;
	DagNode *curNode;

	if (dtmd->ob_source) {
		curNode = dag_get_node(forest, dtmd->ob_source);

		dag_add_relation(forest, curNode, obNode, DAG_RL_DATA_DATA | DAG_RL_OB_DATA,
		                 "DataTransfer Modifier");
	}
}

static bool isDisabled(ModifierData *md, int UNUSED(useRenderParams))
{
	DataTransferModifierData *dtmd = (DataTransferModifierData *) md;
	/* If no source object, bypass. */
	return (dtmd->ob_source == NULL);
}

#define HIGH_POLY_WARNING 10000

static DerivedMesh *applyModifier(ModifierData *md, Object *ob, DerivedMesh *derivedData,
                                  ModifierApplyFlag UNUSED(flag))
{
	DataTransferModifierData *dtmd = (DataTransferModifierData *) md;
	DerivedMesh *dm = derivedData;
	ReportList reports;

	const bool invert_vgroup = (dtmd->flags & MOD_DATATRANSFER_INVERT_VGROUP) != 0;

	const float max_dist = (dtmd->flags & MOD_DATATRANSFER_MAP_MAXDIST) ? dtmd->map_max_distance : FLT_MAX;

	SpaceTransform space_transform_data;
	SpaceTransform *space_transform = (dtmd->flags & MOD_DATATRANSFER_OBSRC_TRANSFORM) ? &space_transform_data : NULL;

	if (space_transform) {
		BLI_SPACE_TRANSFORM_SETUP(space_transform, ob, dtmd->ob_source);
	}

	BKE_reports_init(&reports, RPT_STORE);

	/* Note: no islands precision for now here. */
	BKE_object_data_transfer_dm(md->scene, dtmd->ob_source, ob, dm, dtmd->data_types, false,
	                     dtmd->vmap_mode, dtmd->emap_mode, dtmd->lmap_mode, dtmd->pmap_mode,
	                     space_transform, max_dist, dtmd->map_ray_radius, 0.0f,
	                     dtmd->layers_select_src, dtmd->layers_select_dst,
	                     dtmd->mix_mode, dtmd->mix_factor, dtmd->defgrp_name, invert_vgroup, &reports);

	if (BKE_reports_contain(&reports, RPT_ERROR)) {
		modifier_setError(md, "%s", BKE_reports_string(&reports, RPT_ERROR));
	}
	else if (dm->getNumVerts(dm) > HIGH_POLY_WARNING || ((Mesh *)(dtmd->ob_source->data))->totvert > HIGH_POLY_WARNING) {
		modifier_setError(md, "You are using a rather high poly as source or destination, computation might be slow");
	}

	return dm;
}


ModifierTypeInfo modifierType_DataTransfer = {
	/* name */              "DataTransfer",
	/* structName */        "DataTransferModifierData",
	/* structSize */        sizeof(DataTransferModifierData),
	/* type */              eModifierTypeType_NonGeometrical,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_SupportsMapping |
	                        eModifierTypeFlag_SupportsEditmode |
	                        eModifierTypeFlag_UsesPreview,

	/* copyData */          NULL,
	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   NULL,
	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          NULL,
	/* isDisabled */        isDisabled,
	/* updateDepgraph */    updateDepgraph,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */  dependsOnNormals,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     foreachIDLink,
	/* foreachTexLink */    NULL,
};
