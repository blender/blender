/*
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
 * Copyright 2019, Blender Foundation.
 */

/** \file
 * \ingroup draw_engine
 */

#include "DRW_render.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "DNA_armature_types.h"

#include "DEG_depsgraph_query.h"

#include "GPU_batch.h"

#include "UI_resources.h"

#include "draw_manager_text.h"

#include "overlay_private.h"

void OVERLAY_motion_path_cache_init(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  DRWShadingGroup *grp;
  GPUShader *sh;

  DRWState state = DRW_STATE_WRITE_COLOR;
  DRW_PASS_CREATE(psl->motion_paths_ps, state | pd->clipping_state);

  sh = OVERLAY_shader_motion_path_line();
  pd->motion_path_lines_grp = grp = DRW_shgroup_create(sh, psl->motion_paths_ps);
  DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);

  sh = OVERLAY_shader_motion_path_vert();
  pd->motion_path_points_grp = grp = DRW_shgroup_create(sh, psl->motion_paths_ps);
  DRW_shgroup_uniform_block(grp, "globalsBlock", G_draw.block_ubo);
}

/* Just convert the CPU cache to GPU cache. */
/* T0D0(fclem) This should go into a draw_cache_impl_motionpath. */
static GPUVertBuf *mpath_vbo_get(bMotionPath *mpath)
{
  if (!mpath->points_vbo) {
    GPUVertFormat format = {0};
    /* Match structure of bMotionPathVert. */
    GPU_vertformat_attr_add(&format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
    GPU_vertformat_attr_add(&format, "flag", GPU_COMP_I32, 1, GPU_FETCH_INT);
    mpath->points_vbo = GPU_vertbuf_create_with_format(&format);
    GPU_vertbuf_data_alloc(mpath->points_vbo, mpath->length);
    /* meh... a useless memcpy. */
    memcpy(mpath->points_vbo->data, mpath->points, sizeof(bMotionPathVert) * mpath->length);
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

static void motion_path_get_frame_range_to_draw(bAnimVizSettings *avs,
                                                bMotionPath *mpath,
                                                int current_frame,
                                                int *r_start,
                                                int *r_end,
                                                int *r_step)
{
  int start, end;

  if (avs->path_type == MOTIONPATH_TYPE_ACFRA) {
    start = current_frame - avs->path_bc;
    end = current_frame + avs->path_ac + 1;
  }
  else {
    start = avs->path_sf;
    end = avs->path_ef;
  }

  if (start > end) {
    SWAP(int, start, end);
  }

  CLAMP(start, mpath->start_frame, mpath->end_frame);
  CLAMP(end, mpath->start_frame, mpath->end_frame);

  *r_start = start;
  *r_end = end;
  *r_step = max_ii(avs->path_step, 1);
}

static void motion_path_cache(OVERLAY_Data *vedata,
                              Object *ob,
                              bPoseChannel *pchan,
                              bAnimVizSettings *avs,
                              bMotionPath *mpath)
{
  OVERLAY_PrivateData *pd = vedata->stl->pd;
  const DRWContextState *draw_ctx = DRW_context_state_get();
  struct DRWTextStore *dt = DRW_text_cache_ensure();
  int txt_flag = DRW_TEXT_CACHE_GLOBALSPACE | DRW_TEXT_CACHE_ASCII;
  int cfra = (int)DEG_get_ctime(draw_ctx->depsgraph);
  bool selected = (pchan) ? (pchan->bone->flag & BONE_SELECTED) : (ob->base_flag & BASE_SELECTED);
  bool show_keyframes = (avs->path_viewflag & MOTIONPATH_VIEW_KFRAS) != 0;
  bool show_keyframes_no = (avs->path_viewflag & MOTIONPATH_VIEW_KFNOS) != 0;
  bool show_frame_no = (avs->path_viewflag & MOTIONPATH_VIEW_FNUMS) != 0;
  bool show_lines = (mpath->flag & MOTIONPATH_FLAG_LINES) != 0;
  float no_custom_col[3] = {-1.0f, -1.0f, -1.0f};
  float *color = (mpath->flag & MOTIONPATH_FLAG_CUSTOM) ? mpath->color : no_custom_col;

  int sfra, efra, stepsize;
  motion_path_get_frame_range_to_draw(avs, mpath, cfra, &sfra, &efra, &stepsize);

  int len = efra - sfra;
  if (len == 0) {
    return;
  }
  int start_index = sfra - mpath->start_frame;

  /* Draw curve-line of path. */
  if (show_lines) {
    int motion_path_settings[4] = {cfra, sfra, efra, mpath->start_frame};
    DRWShadingGroup *grp = DRW_shgroup_create_sub(pd->motion_path_lines_grp);
    DRW_shgroup_uniform_ivec4_copy(grp, "mpathLineSettings", motion_path_settings);
    DRW_shgroup_uniform_int_copy(grp, "lineThickness", mpath->line_thickness);
    DRW_shgroup_uniform_bool_copy(grp, "selected", selected);
    DRW_shgroup_uniform_vec3_copy(grp, "customColor", color);
    /* Only draw the required range. */
    DRW_shgroup_call_range(grp, NULL, mpath_batch_line_get(mpath), start_index, len);
  }

  /* Draw points. */
  {
    int pt_size = max_ii(mpath->line_thickness - 1, 1);
    int motion_path_settings[4] = {pt_size, cfra, mpath->start_frame, stepsize};
    DRWShadingGroup *grp = DRW_shgroup_create_sub(pd->motion_path_points_grp);
    DRW_shgroup_uniform_ivec4_copy(grp, "mpathPointSettings", motion_path_settings);
    DRW_shgroup_uniform_bool_copy(grp, "showKeyFrames", show_keyframes);
    DRW_shgroup_uniform_vec3_copy(grp, "customColor", color);
    /* Only draw the required range. */
    DRW_shgroup_call_range(grp, NULL, mpath_batch_points_get(mpath), start_index, len);
  }

  /* Draw frame numbers at each frame-step value. */
  if (show_frame_no || (show_keyframes_no && show_keyframes)) {
    int i;
    uchar col[4], col_kf[4];
    /* Color Management: Exception here as texts are drawn in sRGB space directly.  */
    UI_GetThemeColor3ubv(TH_TEXT_HI, col);
    UI_GetThemeColor3ubv(TH_VERTEX_SELECT, col_kf);
    col[3] = col_kf[3] = 255;

    bMotionPathVert *mpv = mpath->points + start_index;
    for (i = 0; i < len; i += stepsize, mpv += stepsize) {
      int frame = sfra + i;
      char numstr[32];
      size_t numstr_len;
      bool is_keyframe = (mpv->flag & MOTIONPATH_VERT_KEY) != 0;

      if ((show_keyframes && show_keyframes_no && is_keyframe) || (show_frame_no && (i == 0))) {
        numstr_len = BLI_snprintf(numstr, sizeof(numstr), " %d", frame);
        DRW_text_cache_add(
            dt, mpv->co, numstr, numstr_len, 0, 0, txt_flag, (is_keyframe) ? col_kf : col);
      }
      else if (show_frame_no) {
        bMotionPathVert *mpvP = (mpv - stepsize);
        bMotionPathVert *mpvN = (mpv + stepsize);
        /* Only draw frame number if several consecutive highlighted points
         * don't occur on same point. */
        if ((equals_v3v3(mpv->co, mpvP->co) == 0) || (equals_v3v3(mpv->co, mpvN->co) == 0)) {
          numstr_len = BLI_snprintf(numstr, sizeof(numstr), " %d", frame);
          DRW_text_cache_add(dt, mpv->co, numstr, numstr_len, 0, 0, txt_flag, col);
        }
      }
    }
  }
}

void OVERLAY_motion_path_cache_populate(OVERLAY_Data *vedata, Object *ob)
{
  const DRWContextState *draw_ctx = DRW_context_state_get();

  if (ob->type == OB_ARMATURE) {
    if (OVERLAY_armature_is_pose_mode(ob, draw_ctx)) {
      LISTBASE_FOREACH (bPoseChannel *, pchan, &ob->pose->chanbase) {
        if (pchan->mpath) {
          motion_path_cache(vedata, ob, pchan, &ob->pose->avs, pchan->mpath);
        }
      }
    }
  }

  if (ob->mpath) {
    motion_path_cache(vedata, ob, NULL, &ob->avs, ob->mpath);
  }
}

void OVERLAY_motion_path_draw(OVERLAY_Data *vedata)
{
  OVERLAY_PassList *psl = vedata->psl;

  DRW_draw_pass(psl->motion_paths_ps);
}
