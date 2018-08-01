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
#include "BKE_modifier.h"

#include "DNA_modifier_types.h"
#include "DNA_view3d_types.h"

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
	struct DRWPass *bone_outline;
	struct DRWPass *bone_wire;
	struct DRWPass *bone_envelope;
	struct DRWPass *bone_axes;
	struct DRWPass *relationship;
	struct DRWPass *bone_selection;
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
	DRWShadingGroup *bone_selection_shgrp;
	DRWShadingGroup *bone_selection_invert_shgrp;
	float blend_color[4];
	float blend_color_invert[4];
	bool transparent_bones;
} POSE_PrivateData; /* Transient data */

static struct {
	struct GPUShader *bone_selection_sh;
} e_data = {NULL};


/* *********** FUNCTIONS *********** */
static bool POSE_is_bone_selection_overlay_active(void)
{
	const DRWContextState *dcs = DRW_context_state_get();
	const View3D *v3d = dcs->v3d;
	return v3d && (v3d->overlay.flag & V3D_OVERLAY_BONE_SELECT) && OBPOSE_FROM_OBACT(dcs->obact);
}

static void POSE_engine_init(void *UNUSED(vedata))
{
	if (!e_data.bone_selection_sh) {
		e_data.bone_selection_sh = GPU_shader_get_builtin_shader(GPU_SHADER_3D_UNIFORM_COLOR);
	}
}

static void POSE_engine_free(void)
{
}

/* Here init all passes and shading groups
 * Assume that all Passes are NULL */
static void POSE_cache_init(void *vedata)
{
	POSE_PassList *psl = ((POSE_Data *)vedata)->psl;
	POSE_StorageList *stl = ((POSE_Data *)vedata)->stl;
	const DRWContextState *draw_ctx = DRW_context_state_get();
	View3D *v3d = draw_ctx->v3d;

	if (!stl->g_data) {
		/* Alloc transient pointers */
		stl->g_data = MEM_callocN(sizeof(*stl->g_data), __func__);
	}
	POSE_PrivateData *ppd = stl->g_data;
	ppd->transparent_bones = (draw_ctx->v3d->overlay.arm_flag & V3D_OVERLAY_ARM_TRANSP_BONES) != 0;

	{
		/* Solid bones */
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_CULL_BACK;
		psl->bone_solid = DRW_pass_create("Bone Solid Pass", state);
	}

	{
		/* Bones Outline */
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL;
		psl->bone_outline = DRW_pass_create("Bone Outline Pass", state);
	}

	{
		/* Wire bones */
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND;
		psl->bone_wire = DRW_pass_create("Bone Wire Pass", state);
	}

	{
		/* distance outline around envelope bones */
		DRWState state = DRW_STATE_ADDITIVE | DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_CULL_FRONT;
		psl->bone_envelope = DRW_pass_create("Bone Envelope Outline Pass", state);
	}

	{
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WIRE_SMOOTH | DRW_STATE_BLEND;
		psl->bone_axes = DRW_pass_create("Bone Axes Pass", state);
	}

	{
		/* Non Meshes Pass (Camera, empties, lamps ...) */
		DRWState state =
		        DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL |
		        DRW_STATE_BLEND | DRW_STATE_WIRE;
		psl->relationship = DRW_pass_create("Bone Relationship Pass", state);
	}

	{
		if (POSE_is_bone_selection_overlay_active()) {
			copy_v4_fl4(ppd->blend_color, 0.0f, 0.0f, 0.0f, v3d->overlay.bone_select_alpha);
			copy_v4_fl4(ppd->blend_color_invert, 0.0f, 0.0f, 0.0f, pow(v3d->overlay.bone_select_alpha, 4));
			DRWShadingGroup *grp;
			psl->bone_selection = DRW_pass_create(
			        "Bone Selection",
			        DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_EQUAL | DRW_STATE_BLEND);
			grp = DRW_shgroup_create(e_data.bone_selection_sh, psl->bone_selection);
			DRW_shgroup_uniform_vec4(grp, "color", ppd->blend_color, 1);
			stl->g_data->bone_selection_shgrp = grp;
			grp = DRW_shgroup_create(e_data.bone_selection_sh, psl->bone_selection);
			DRW_shgroup_uniform_vec4(grp, "color", ppd->blend_color_invert, 1);
			stl->g_data->bone_selection_invert_shgrp = grp;
		}
	}
}

static bool POSE_is_driven_by_active_armature(Object *ob)
{
	Object *ob_arm = modifiers_isDeformedByArmature(ob);
	if (ob_arm) {
		const DRWContextState *draw_ctx = DRW_context_state_get();
		bool is_active = DRW_pose_mode_armature(ob_arm, draw_ctx->obact);
		if (!is_active && ob_arm->proxy_from) {
			is_active = DRW_pose_mode_armature(ob_arm->proxy_from, draw_ctx->obact);
		}
		return is_active;
	}
	else {
		Object *ob_mesh_deform = modifiers_isDeformedByMeshDeform(ob);
		if (ob_mesh_deform) {
			return POSE_is_driven_by_active_armature(ob_mesh_deform);
		}
	}
	return false;
}

/* Add geometry to shading groups. Execute for each objects */
static void POSE_cache_populate(void *vedata, Object *ob)
{
	POSE_PassList *psl = ((POSE_Data *)vedata)->psl;
	POSE_PrivateData *ppd = ((POSE_Data *)vedata)->stl->g_data;
	const DRWContextState *draw_ctx = DRW_context_state_get();

	/* In the future this will allow us to implement face gizmos,
	 * and similar functionalities. For now we handle only pose bones. */

	if (ob->type == OB_ARMATURE) {
		if (draw_ctx->v3d->overlay.flag & V3D_OVERLAY_HIDE_BONES) {
			return;
		}
		if (DRW_pose_mode_armature(ob, draw_ctx->obact)) {
			DRWArmaturePasses passes = {
			    .bone_solid = psl->bone_solid,
			    .bone_outline = psl->bone_outline,
			    .bone_wire = psl->bone_wire,
			    .bone_envelope = psl->bone_envelope,
			    .bone_axes = psl->bone_axes,
			    .relationship_lines = psl->relationship,
			};
			DRW_shgroup_armature_pose(ob, passes, ppd->transparent_bones);
		}
	}
	else if (ob->type == OB_MESH &&
	         !DRW_state_is_select() &&
	         POSE_is_bone_selection_overlay_active())
	{
		struct GPUBatch *geom = DRW_cache_object_surface_get(ob);
		if (geom) {
			if (POSE_is_driven_by_active_armature(ob)) {
				DRW_shgroup_call_object_add(ppd->bone_selection_shgrp, geom, ob);
			}
			else {
				DRW_shgroup_call_object_add(ppd->bone_selection_invert_shgrp, geom, ob);
			}
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
	if (((ob == active_ob) || (ob->mode & OB_MODE_POSE)) &&
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
	DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
	DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
	const DRWContextState *draw_ctx = DRW_context_state_get();
	const bool transparent_bones = (draw_ctx->v3d->overlay.arm_flag & V3D_OVERLAY_ARM_TRANSP_BONES) != 0;
	const bool bone_selection_overlay = POSE_is_bone_selection_overlay_active();

	if (DRW_state_is_select()) {
		DRW_draw_pass(psl->bone_solid);
		DRW_draw_pass(psl->bone_wire);
		return;
	}

	if (bone_selection_overlay) {
		GPU_framebuffer_bind(dfbl->default_fb);
		DRW_draw_pass(psl->bone_selection);
		GPU_framebuffer_bind(dfbl->depth_only_fb);
		GPU_framebuffer_clear_depth(dfbl->depth_only_fb, 1.0);
		GPU_framebuffer_bind(dfbl->default_fb);
	}

	DRW_draw_pass(psl->bone_envelope);

	if (transparent_bones) {
		DRW_pass_state_add(psl->bone_solid, DRW_STATE_BLEND);
		DRW_pass_state_remove(psl->bone_solid, DRW_STATE_WRITE_DEPTH);
		DRW_draw_pass(psl->bone_solid);
	}

	MULTISAMPLE_SYNC_ENABLE(dfbl, dtxl)

	if (!transparent_bones) {
		DRW_draw_pass(psl->bone_solid);
	}

	DRW_draw_pass(psl->bone_outline);
	DRW_draw_pass(psl->bone_wire);
	DRW_draw_pass(psl->relationship);

	MULTISAMPLE_SYNC_DISABLE(dfbl, dtxl)

	/* Draw axes with linesmooth and outside of multisample buffer. */
	DRW_draw_pass(psl->bone_axes);
}

static const DrawEngineDataSize POSE_data_size = DRW_VIEWPORT_DATA_SIZE(POSE_Data);

DrawEngineType draw_engine_pose_type = {
	NULL, NULL,
	N_("PoseMode"),
	&POSE_data_size,
	&POSE_engine_init,
	&POSE_engine_free,
	&POSE_cache_init,
	&POSE_cache_populate,
	NULL,
	NULL,
	&POSE_draw_scene,
	NULL,
	NULL,
};
