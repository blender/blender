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

/** \file blender/draw/modes/pose_mode.c
 *  \ingroup draw
 */

#include "DRW_engine.h"
#include "DRW_render.h"

/* If builtin shaders are needed */
#include "GPU_shader.h"

#include "draw_common.h"

#include "draw_mode_engines.h"

extern GlobalsUboStorage ts;

/* *********** LISTS *********** */
/* All lists are per viewport specific datas.
 * They are all free when viewport changes engines
 * or is free itself. Use POSE_engine_init() to
 * initialize most of them and POSE_cache_init()
 * for POSE_PassList */

typedef struct POSE_PassList {
	struct DRWPass *bone_solid;
	struct DRWPass *bone_wire;
	struct DRWPass *bone_envelope;
	struct DRWPass *relationship;
} POSE_PassList;

typedef struct POSE_StorageList {
	struct POSE_PrivateData *g_data;
} POSE_StorageList;

typedef struct POSE_Data {
	void *engine_type;
	DRWViewportEmptyList *fbl;
	DRWViewportEmptyList *txl;
	POSE_PassList *psl;
	POSE_StorageList *stl;
} POSE_Data;

/* *********** STATIC *********** */

typedef struct POSE_PrivateData {
	DRWShadingGroup *relationship_lines;
} POSE_PrivateData; /* Transient data */

/* *********** FUNCTIONS *********** */

/* Here init all passes and shading groups
 * Assume that all Passes are NULL */
static void POSE_cache_init(void *vedata)
{
	POSE_PassList *psl = ((POSE_Data *)vedata)->psl;
	POSE_StorageList *stl = ((POSE_Data *)vedata)->stl;

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

/* Add geometry to shading groups. Execute for each objects */
static void POSE_cache_populate(void *vedata, Object *ob)
{
	POSE_PassList *psl = ((POSE_Data *)vedata)->psl;
	POSE_StorageList *stl = ((POSE_Data *)vedata)->stl;
	const DRWContextState *draw_ctx = DRW_context_state_get();

	/* In the future this will allow us to implement face manipulators,
	 * and similar functionalities. For now we handle only pose bones. */

	if (ob->type == OB_ARMATURE) {
		if (DRW_pose_mode_armature(ob, draw_ctx->obact)) {
			DRW_shgroup_armature_pose(
			        ob, psl->bone_solid, psl->bone_wire, psl->bone_envelope,
			        stl->g_data->relationship_lines);
		}
	}
}

/**
 * Return true if armature should be handled by the pose mode engine.
 */
bool DRW_pose_mode_armature(Object *ob, Object *active_ob)
{
	const DRWContextState *draw_ctx = DRW_context_state_get();

	/* Pose armature is handled by pose mode engine. */
	if (((ob == active_ob) || (ob->base_flag & BASE_SELECTED)) &&
	    ((draw_ctx->object_mode & OB_MODE_POSE) != 0))
	{
		return true;
	}

	/* Armature parent is also handled by pose mode engine. */
	if ((active_ob != NULL) && ((draw_ctx->object_mode & OB_MODE_WEIGHT_PAINT) != 0)) {
		if (ob == draw_ctx->object_pose) {
			return true;
		}
	}

	return false;
}

/* Draw time ! Control rendering pipeline from here */
static void POSE_draw_scene(void *vedata)
{
	POSE_PassList *psl = ((POSE_Data *)vedata)->psl;

	DRW_draw_pass(psl->bone_envelope);
	DRW_draw_pass(psl->bone_wire);
	DRW_draw_pass(psl->bone_solid);
	DRW_draw_pass(psl->relationship);
}

/* Create collection settings here.
 *
 * Be sure to add this function there :
 * source/blender/draw/DRW_engine.h
 * source/blender/blenkernel/intern/layer.c
 * source/blenderplayer/bad_level_call_stubs/stubs.c
 *
 * And relevant collection settings to :
 * source/blender/makesrna/intern/rna_scene.c
 * source/blender/blenkernel/intern/layer.c
 */
#if 0
void POSE_collection_settings_create(CollectionEngineSettings *ces)
{
	BLI_assert(ces);
	// BKE_collection_engine_property_add_int(ces, "foo", 37);
}
#endif

static const DrawEngineDataSize POSE_data_size = DRW_VIEWPORT_DATA_SIZE(POSE_Data);

DrawEngineType draw_engine_pose_type = {
	NULL, NULL,
	N_("PoseMode"),
	&POSE_data_size,
	NULL,
	NULL,
	&POSE_cache_init,
	&POSE_cache_populate,
	NULL,
	NULL,
	&POSE_draw_scene,
	NULL,
	NULL,
};
