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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2009/2018 by the Blender Foundation.
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/draw/intern/draw_anim_viz.c
 *  \ingroup draw
 */


#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "BLI_sys_types.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_object_types.h"

#include "BLI_math.h"
#include "BLI_dlrbTree.h"

#include "BKE_animsys.h"
#include "BKE_action.h"

#include "ED_keyframes_draw.h"

#include "UI_resources.h"

#include "DEG_depsgraph_query.h"

#include "DRW_engine.h"
#include "DRW_render.h"

#include "GPU_shader.h"
#include "GPU_immediate.h"
#include "GPU_matrix.h"

#include "draw_common.h"
#include "draw_manager_text.h"

#include "draw_mode_engines.h"

extern struct GPUUniformBuffer *globals_ubo; /* draw_common.c */

/* ********************************* Lists ************************************** */
/* All lists are per viewport specific datas.
 * They are all free when viewport changes engines
 * or is free itself.
 */

/* XXX: How to show frame numbers, etc.?  Currently only doing the dots and lines */
typedef struct MPATH_PassList {
	struct DRWPass *lines;
	struct DRWPass *points;
} MPATH_PassList;

typedef struct MPATH_StorageList {
	struct MPATH_PrivateData *g_data;
} MPATH_StorageList;

typedef struct MPATH_Data {
	void *engine_type;
	DRWViewportEmptyList *fbl;
	DRWViewportEmptyList *txl;
	MPATH_PassList *psl;
	MPATH_StorageList *stl;
} MPATH_Data;

#if 0
static struct {
	GPUShader *mpath_line_sh;
	GPUShader *mpath_points_sh;
} e_data = {0};
#endif

/* *************************** Path Cache *********************************** */

/* Just convert the CPU cache to GPU cache. */
static GPUVertBuf *mpath_vbo_get(bMotionPath *mpath)
{
	if (!mpath->points_vbo) {
		GPUVertFormat format = {0};
		/* Match structure of bMotionPathVert. */
		uint pos = GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
		GPU_vertformat_attr_add(&format, "flag", GPU_COMP_I32, 1, GPU_FETCH_INT);
		mpath->points_vbo = GPU_vertbuf_create_with_format(&format);
		GPU_vertbuf_data_alloc(mpath->points_vbo, mpath->length);

		/* meh... a useless memcpy. */
		GPUVertBufRaw raw_data;
		GPU_vertbuf_attr_get_raw_data(mpath->points_vbo, pos, &raw_data);
		memcpy(GPU_vertbuf_raw_step(&raw_data), mpath->points, sizeof(bMotionPathVert) * mpath->length);
	}
	return mpath->points_vbo;
}

static GPUBatch *mpath_batch_line_get(bMotionPath *mpath)
{
	if (!mpath->batch_line) {
		mpath->batch_line = GPU_batch_create(GPU_PRIM_LINE_STRIP, mpath_vbo_get(mpath), NULL);
	}
	return mpath->batch_line;
}

static GPUBatch *mpath_batch_points_get(bMotionPath *mpath)
{
	if (!mpath->batch_points) {
		mpath->batch_points = GPU_batch_create(GPU_PRIM_POINTS, mpath_vbo_get(mpath), NULL);
	}
	return mpath->batch_points;
}

/* *************************** Draw Engine Entrypoints ************************** */

static void MPATH_engine_init(void *UNUSED(vedata))
{
}

static void MPATH_engine_free(void)
{
}

/* Here init all passes and shading groups
 * Assume that all Passes are NULL */
static void MPATH_cache_init(void *vedata)
{
	MPATH_PassList *psl = ((MPATH_Data *)vedata)->psl;

	{
		DRWState state = DRW_STATE_WRITE_COLOR;
		psl->lines = DRW_pass_create("Motionpath Line Pass", state);
	}

	{
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_POINT;
		psl->points = DRW_pass_create("Motionpath Point Pass", state);
	}
}

static void MPATH_cache_motion_path(MPATH_PassList *psl,
                                    Object *ob, bPoseChannel *pchan,
                                    bAnimVizSettings *avs, bMotionPath *mpath)
{
	const DRWContextState *draw_ctx = DRW_context_state_get();
	struct DRWTextStore *dt = DRW_text_cache_ensure();
	int txt_flag = DRW_TEXT_CACHE_GLOBALSPACE | DRW_TEXT_CACHE_ASCII;
	int stepsize = avs->path_step;
	int sfra, efra, sind, len;
	int cfra = (int)DEG_get_ctime(draw_ctx->depsgraph);
	bool sel = (pchan) ? (pchan->bone->flag & BONE_SELECTED) : (ob->flag & SELECT);
	bool show_keyframes = (avs->path_viewflag & MOTIONPATH_VIEW_KFRAS) != 0;
	bMotionPathVert *mpv, *mpv_start;

	/* get frame ranges */
	if (avs->path_type == MOTIONPATH_TYPE_ACFRA) {
		/* With "Around Current", we only choose frames from around
		 * the current frame to draw.
		 */
		sfra = cfra - avs->path_bc;
		efra = cfra + avs->path_ac + 1;
	}
	else {
		/* Use the current display range */
		sfra = avs->path_sf;
		efra = avs->path_ef;
	}

	/* no matter what, we can only show what is in the cache and no more
	 * - abort if whole range is past ends of path
	 * - otherwise clamp endpoints to extents of path
	 */
	if (sfra < mpath->start_frame) {
		/* start clamp */
		sfra = mpath->start_frame;
	}
	if (efra > mpath->end_frame) {
		/* end clamp */
		efra = mpath->end_frame;
	}

	if ((sfra > mpath->end_frame) || (efra < mpath->start_frame)) {
		/* whole path is out of bounds */
		return;
	}

	len = efra - sfra;

	if ((len <= 0) || (mpath->points == NULL)) {
		return;
	}

	sind = sfra - mpath->start_frame;
	mpv_start = (mpath->points + sind);

	bool use_custom_col = (mpath->flag & MOTIONPATH_FLAG_CUSTOM) != 0;

	/* draw curve-line of path */
	/* Draw lines only if line drawing option is enabled */
	if (mpath->flag & MOTIONPATH_FLAG_LINES) {
		DRWShadingGroup *shgrp = DRW_shgroup_create(mpath_line_shader_get(), psl->lines);
		DRW_shgroup_uniform_int_copy(shgrp, "frameCurrent", cfra);
		DRW_shgroup_uniform_int_copy(shgrp, "frameStart", sfra);
		DRW_shgroup_uniform_int_copy(shgrp, "frameEnd", efra);
		DRW_shgroup_uniform_int_copy(shgrp, "cacheStart", mpath->start_frame);
		DRW_shgroup_uniform_int_copy(shgrp, "lineThickness", mpath->line_thickness);
		DRW_shgroup_uniform_bool_copy(shgrp, "selected", sel);
		DRW_shgroup_uniform_bool_copy(shgrp, "useCustomColor", use_custom_col);
		DRW_shgroup_uniform_vec2(shgrp, "viewportSize", DRW_viewport_size_get(), 1);
		DRW_shgroup_uniform_block(shgrp, "globalsBlock", globals_ubo);
		if (use_custom_col) {
			DRW_shgroup_uniform_vec3(shgrp, "customColor", mpath->color, 1);
		}
		/* Only draw the required range. */
		DRW_shgroup_call_range_add(shgrp, mpath_batch_line_get(mpath), NULL, sind, len);
	}

	/* Draw points. */
	DRWShadingGroup *shgrp = DRW_shgroup_create(mpath_points_shader_get(), psl->points);
	DRW_shgroup_uniform_int_copy(shgrp, "frameCurrent", cfra);
	DRW_shgroup_uniform_int_copy(shgrp, "cacheStart", mpath->start_frame);
	DRW_shgroup_uniform_int_copy(shgrp, "pointSize", mpath->line_thickness);
	DRW_shgroup_uniform_int_copy(shgrp, "stepSize", stepsize);
	DRW_shgroup_uniform_bool_copy(shgrp, "selected", sel);
	DRW_shgroup_uniform_bool_copy(shgrp, "showKeyFrames", show_keyframes);
	DRW_shgroup_uniform_bool_copy(shgrp, "useCustomColor", use_custom_col);
	DRW_shgroup_uniform_block(shgrp, "globalsBlock", globals_ubo);
	if (use_custom_col) {
		DRW_shgroup_uniform_vec3(shgrp, "customColor", mpath->color, 1);
	}
	/* Only draw the required range. */
	DRW_shgroup_call_range_add(shgrp, mpath_batch_points_get(mpath), NULL, sind, len);

	/* Draw frame numbers at each framestep value */
	bool show_kf_no = (avs->path_viewflag & MOTIONPATH_VIEW_KFNOS) != 0;
	if ((avs->path_viewflag & (MOTIONPATH_VIEW_FNUMS)) || (show_kf_no && show_keyframes)) {
		int i;
		uchar col[4], col_kf[4];
		UI_GetThemeColor3ubv(TH_TEXT_HI, col);
		UI_GetThemeColor3ubv(TH_VERTEX_SELECT, col_kf);
		col[3] = col_kf[3] = 255;

		for (i = 0, mpv = mpv_start; i < len; i += stepsize, mpv += stepsize) {
			int frame = sfra + i;
			char numstr[32];
			size_t numstr_len;
			float co[3];
			bool is_keyframe = (mpv->flag & MOTIONPATH_VERT_KEY) != 0;

			if ((show_keyframes && show_kf_no && is_keyframe) ||
			    ((avs->path_viewflag & MOTIONPATH_VIEW_FNUMS) && (i == 0)))
			{
				numstr_len = sprintf(numstr, " %d", frame);
				mul_v3_m4v3(co, ob->imat, mpv->co);
				DRW_text_cache_add(dt, co, numstr, numstr_len, 0, txt_flag, (is_keyframe) ? col_kf : col);
			}
			else if (avs->path_viewflag & MOTIONPATH_VIEW_FNUMS) {
				bMotionPathVert *mpvP = (mpv - stepsize);
				bMotionPathVert *mpvN = (mpv + stepsize);
				/* only draw framenum if several consecutive highlighted points don't occur on same point */
				if ((equals_v3v3(mpv->co, mpvP->co) == 0) || (equals_v3v3(mpv->co, mpvN->co) == 0)) {
					numstr_len = sprintf(numstr, " %d", frame);
					mul_v3_m4v3(co, ob->imat, mpv->co);
					DRW_text_cache_add(dt, co, numstr, numstr_len, 0, txt_flag, col);
				}
			}
		}
	}
}

/* Add geometry to shading groups. Execute for each objects */
static void MPATH_cache_populate(void *vedata, Object *ob)
{
	MPATH_PassList *psl = ((MPATH_Data *)vedata)->psl;
	const DRWContextState *draw_ctx = DRW_context_state_get();

	if (draw_ctx->v3d->overlay.flag & V3D_OVERLAY_HIDE_MOTION_PATHS) {
		return;
	}

	if (ob->type == OB_ARMATURE) {
		if (DRW_pose_mode_armature(ob, draw_ctx->obact)) {
			for (bPoseChannel *pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
				if (pchan->mpath) {
					MPATH_cache_motion_path(psl, ob, pchan, &ob->pose->avs, pchan->mpath);
				}
			}
		}
	}
	else {
		if (ob->mpath) {
			MPATH_cache_motion_path(psl, ob, NULL, &ob->avs, ob->mpath);
		}
	}
}

/* Draw time! Control rendering pipeline from here */
static void MPATH_draw_scene(void *vedata)
{
	MPATH_PassList *psl = ((MPATH_Data *)vedata)->psl;
	DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
	DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

	if (DRW_pass_is_empty(psl->lines) &&
	    DRW_pass_is_empty(psl->points))
	{
		/* Nothing to draw. */
		return;
	}

	MULTISAMPLE_SYNC_ENABLE(dfbl, dtxl)

	DRW_draw_pass(psl->lines);
	DRW_draw_pass(psl->points);

	MULTISAMPLE_SYNC_DISABLE_NO_DEPTH(dfbl, dtxl)
}

/* *************************** Draw Engine Defines ****************************** */

static const DrawEngineDataSize MPATH_data_size = DRW_VIEWPORT_DATA_SIZE(MPATH_Data);

DrawEngineType draw_engine_motion_path_type = {
	NULL, NULL,
	N_("MotionPath"),
	&MPATH_data_size,
	&MPATH_engine_init,
	&MPATH_engine_free,
	&MPATH_cache_init,
	&MPATH_cache_populate,
	NULL,
	NULL,
	&MPATH_draw_scene,
	NULL,
	NULL,
};
