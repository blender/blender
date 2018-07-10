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
#include "BKE_data_transfer.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_remap.h"
#include "BKE_modifier.h"
#include "BKE_report.h"

#include "DEG_depsgraph_query.h"

#include "MEM_guardedalloc.h"
#include "MOD_util.h"

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

static void foreachObjectLink(
        ModifierData *md, Object *ob,
        ObjectWalkFunc walk, void *userData)
{
	DataTransferModifierData *dtmd = (DataTransferModifierData *) md;
	walk(userData, ob, &dtmd->ob_source, IDWALK_CB_NOP);
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
	DataTransferModifierData *dtmd = (DataTransferModifierData *) md;
	if (dtmd->ob_source != NULL) {
		DEG_add_object_relation(ctx->node, dtmd->ob_source, DEG_OB_COMP_GEOMETRY, "DataTransfer Modifier");
	}
}

static bool isDisabled(const struct Scene *UNUSED(scene), ModifierData *md, bool UNUSED(useRenderParams))
{
	DataTransferModifierData *dtmd = (DataTransferModifierData *) md;
	/* If no source object, bypass. */
	return (dtmd->ob_source == NULL);
}

#define HIGH_POLY_WARNING 10000
#define DT_TYPES_AFFECT_MESH ( \
	DT_TYPE_BWEIGHT_VERT | \
	DT_TYPE_BWEIGHT_EDGE | DT_TYPE_CREASE | DT_TYPE_SHARP_EDGE | \
	DT_TYPE_LNOR | \
	DT_TYPE_SHARP_FACE \
)

static Mesh *applyModifier(ModifierData *md, const ModifierEvalContext *ctx, Mesh *me_mod)
{
	DataTransferModifierData *dtmd = (DataTransferModifierData *) md;
	struct Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
	Mesh *result = me_mod;
	ReportList reports;

	/* Only used to check wehther we are operating on org data or not... */
	Mesh *me = ctx->object->data;

	const bool invert_vgroup = (dtmd->flags & MOD_DATATRANSFER_INVERT_VGROUP) != 0;

	const float max_dist = (dtmd->flags & MOD_DATATRANSFER_MAP_MAXDIST) ? dtmd->map_max_distance : FLT_MAX;

	SpaceTransform space_transform_data;
	SpaceTransform *space_transform = (dtmd->flags & MOD_DATATRANSFER_OBSRC_TRANSFORM) ? &space_transform_data : NULL;

	if (space_transform) {
		BLI_SPACE_TRANSFORM_SETUP(space_transform, ctx->object, dtmd->ob_source);
	}

	if ((result == me_mod || (me->mvert == result->mvert) || (me->medge == result->medge)) &&
	    (dtmd->data_types & DT_TYPES_AFFECT_MESH))
	{
		/* We need to duplicate data here, otherwise setting custom normals, edges' shaprness, etc., could
		 * modify org mesh, see T43671. */
		BKE_id_copy_ex(
		        NULL, &me_mod->id, (ID **)&result,
		        LIB_ID_CREATE_NO_MAIN |
		        LIB_ID_CREATE_NO_USER_REFCOUNT |
		        LIB_ID_CREATE_NO_DEG_TAG |
		        LIB_ID_COPY_NO_PREVIEW,
		        false);
	}

	BKE_reports_init(&reports, RPT_STORE);

	/* Note: no islands precision for now here. */
	BKE_object_data_transfer_ex(ctx->depsgraph, scene, dtmd->ob_source, ctx->object, result, dtmd->data_types, false,
	                     dtmd->vmap_mode, dtmd->emap_mode, dtmd->lmap_mode, dtmd->pmap_mode,
	                     space_transform, false, max_dist, dtmd->map_ray_radius, 0.0f,
	                     dtmd->layers_select_src, dtmd->layers_select_dst,
	                     dtmd->mix_mode, dtmd->mix_factor, dtmd->defgrp_name, invert_vgroup, &reports);

	if (BKE_reports_contain(&reports, RPT_ERROR)) {
		modifier_setError(md, "%s", BKE_reports_string(&reports, RPT_ERROR));
	}
	else if ((dtmd->data_types & DT_TYPE_LNOR) && !(me->flag & ME_AUTOSMOOTH)) {
		modifier_setError((ModifierData *)dtmd, "Enable 'Auto Smooth' option in mesh settings");
	}
	else if (result->totvert > HIGH_POLY_WARNING || ((Mesh *)(dtmd->ob_source->data))->totvert > HIGH_POLY_WARNING) {
		modifier_setError(md, "You are using a rather high poly as source or destination, computation might be slow");
	}

	return result;
}

#undef HIGH_POLY_WARNING
#undef DT_TYPES_AFFECT_MESH

ModifierTypeInfo modifierType_DataTransfer = {
	/* name */              "DataTransfer",
	/* structName */        "DataTransferModifierData",
	/* structSize */        sizeof(DataTransferModifierData),
	/* type */              eModifierTypeType_NonGeometrical,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_SupportsMapping |
	                        eModifierTypeFlag_SupportsEditmode |
	                        eModifierTypeFlag_UsesPreview,

	/* copyData */          modifier_copyData_generic,

	/* deformVerts_DM */    NULL,
	/* deformMatrices_DM */ NULL,
	/* deformVertsEM_DM */  NULL,
	/* deformMatricesEM_DM*/NULL,
	/* applyModifier_DM */  NULL,
	/* applyModifierEM_DM */NULL,

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
	/* updateDepsgraph */   updateDepsgraph,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */  dependsOnNormals,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
