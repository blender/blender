/*
 * Copyright 2016, Blender Foundation.
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Blender Institute
 *
 */

/** \file blender/draw/modes/edit_armature_mode.c
 *  \ingroup draw
 */

#include "DRW_engine.h"
#include "DRW_render.h"

#include "DNA_armature_types.h"

#include "draw_common.h"

#include "draw_mode_engines.h"

extern GlobalsUboStorage ts;

/* *********** LISTS *********** */
typedef struct EDIT_ARMATURE_PassList {
	struct DRWPass *bone_solid;
	struct DRWPass *bone_wire;
	struct DRWPass *bone_envelope;
	struct DRWPass *relationship;
} EDIT_ARMATURE_PassList;

typedef struct EDIT_ARMATURE_StorageList {
	struct EDIT_ARMATURE_PrivateData *g_data;
} EDIT_ARMATURE_StorageList;

typedef struct EDIT_ARMATURE_Data {
	void *engine_type;
	DRWViewportEmptyList *fbl;
	DRWViewportEmptyList *txl;
	EDIT_ARMATURE_PassList *psl;
	EDIT_ARMATURE_StorageList *stl;
} EDIT_ARMATURE_Data;

/* *********** STATIC *********** */

typedef struct EDIT_ARMATURE_PrivateData {
	DRWShadingGroup *relationship_lines;
} EDIT_ARMATURE_PrivateData; /* Transient data */

/* *********** FUNCTIONS *********** */

static void EDIT_ARMATURE_cache_init(void *vedata)
{
	EDIT_ARMATURE_PassList *psl = ((EDIT_ARMATURE_Data *)vedata)->psl;
	EDIT_ARMATURE_StorageList *stl = ((EDIT_ARMATURE_Data *)vedata)->stl;

	if (!stl->g_data) {
		/* Alloc transient pointers */
		stl->g_data = MEM_mallocN(sizeof(*stl->g_data), __func__);
	}

	{
		/* Solid bones */
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS;
		psl->bone_solid = DRW_pass_create("Bone Solid Pass", state);
	}

	{
		/* Wire bones */
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS | DRW_STATE_BLEND;
		psl->bone_wire = DRW_pass_create("Bone Wire Pass", state);
	}

	{
		/* distance outline around envelope bones */
		DRWState state = DRW_STATE_ADDITIVE | DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS | DRW_STATE_BLEND;
		psl->bone_envelope = DRW_pass_create("Bone Envelope Outline Pass", state);
	}

	{
		/* Non Meshes Pass (Camera, empties, lamps ...) */
		DRWState state =
		        DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS |
		        DRW_STATE_BLEND | DRW_STATE_WIRE;
		psl->relationship = DRW_pass_create("Bone Relationship Pass", state);

		/* Relationship Lines */
		stl->g_data->relationship_lines = shgroup_dynlines_uniform_color(psl->relationship, ts.colorWire);
		DRW_shgroup_state_enable(stl->g_data->relationship_lines, DRW_STATE_STIPPLE_3);
	}
}

static void EDIT_ARMATURE_cache_populate(void *vedata, Object *ob)
{
	bArmature *arm = ob->data;
	EDIT_ARMATURE_PassList *psl = ((EDIT_ARMATURE_Data *)vedata)->psl;
	EDIT_ARMATURE_StorageList *stl = ((EDIT_ARMATURE_Data *)vedata)->stl;

	if (ob->type == OB_ARMATURE) {
		if (arm->edbo) {
			DRW_shgroup_armature_edit(
			            ob, psl->bone_solid, psl->bone_wire, psl->bone_envelope, stl->g_data->relationship_lines);
		}
	}
}

static void EDIT_ARMATURE_draw_scene(void *vedata)
{
	EDIT_ARMATURE_PassList *psl = ((EDIT_ARMATURE_Data *)vedata)->psl;
	DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();

	MULTISAMPLE_SYNC_ENABLE(dfbl)

	DRW_draw_pass(psl->bone_envelope);
	DRW_draw_pass(psl->bone_solid);
	DRW_draw_pass(psl->bone_wire);
	DRW_draw_pass(psl->relationship);

	MULTISAMPLE_SYNC_DISABLE(dfbl)
}

#if 0
void EDIT_ARMATURE_collection_settings_create(CollectionEngineSettings *ces)
{
	BLI_assert(ces);
	//BKE_collection_engine_property_add_int(ces, "show_occlude_wire", false);
}
#endif

static const DrawEngineDataSize EDIT_ARMATURE_data_size = DRW_VIEWPORT_DATA_SIZE(EDIT_ARMATURE_Data);

DrawEngineType draw_engine_edit_armature_type = {
	NULL, NULL,
	N_("EditArmatureMode"),
	&EDIT_ARMATURE_data_size,
	NULL,
	NULL,
	&EDIT_ARMATURE_cache_init,
	&EDIT_ARMATURE_cache_populate,
	NULL,
	NULL,
	&EDIT_ARMATURE_draw_scene,
	NULL,
	NULL,
};
