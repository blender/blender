/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "BLI_string.h"

#include "DEG_depsgraph_query.hh"

#include "draw_manager_text.hh"

#include "overlay_armature.hh"
#include "overlay_base.hh"

namespace blender::draw::overlay {

/**
 * Display object and armature motion path.
 * Motion paths can be found in (Object > Motion Paths) or (Data > Motion Paths) for armatures.
 */
class MotionPath : Overlay {

 private:
  PassSimple motion_path_ps_ = {"motion_path_ps_"};

  PassSimple::Sub *line_ps_ = nullptr;
  PassSimple::Sub *vert_ps_ = nullptr;

 public:
  void begin_sync(Resources &res, const State &state) final
  {
    enabled_ = state.v3d && state.show_motion_paths() && !res.is_selection();
    if (!enabled_) {
      /* Not used. But release the data. */
      motion_path_ps_.init();
      return;
    }

    {
      PassSimple &pass = motion_path_ps_;
      pass.init();
      pass.bind_ubo(OVERLAY_GLOBALS_SLOT, &res.globals_buf);
      pass.bind_ubo(DRW_CLIPPING_UBO_SLOT, &res.clip_planes_buf);
      pass.state_set(DRW_STATE_WRITE_COLOR, state.clipping_plane_count);
      {
        PassSimple::Sub &sub = pass.sub("Lines");
        sub.shader_set(res.shaders->motion_path_line.get());
        line_ps_ = &sub;
      }
      {
        PassSimple::Sub &sub = pass.sub("Points");
        sub.shader_set(res.shaders->motion_path_vert.get());
        vert_ps_ = &sub;
      }
    }
  }

  void object_sync(Manager & /*manager*/,
                   const ObjectRef &ob_ref,
                   Resources & /*res*/,
                   const State &state) final
  {
    if (!enabled_) {
      return;
    }

    const Object *object = ob_ref.object;

    if (object->type == OB_ARMATURE) {
      if (Armatures::is_pose_mode(object, state)) {
        for (bPoseChannel *pchan : ListBaseWrapper<bPoseChannel>(&object->pose->chanbase)) {
          if (pchan->mpath) {
            motion_path_sync(state, object, pchan, object->pose->avs, pchan->mpath);
          }
        }
      }
    }

    if (object->mpath) {
      motion_path_sync(state, object, nullptr, object->avs, object->mpath);
    }
  }

  void draw_color_only(Framebuffer &framebuffer, Manager &manager, View &view) final
  {
    if (!enabled_) {
      return;
    }
    GPU_framebuffer_bind(framebuffer);
    manager.submit(motion_path_ps_, view);
  }

 private:
  void motion_path_sync(const State &state,
                        const Object *ob,
                        const bPoseChannel *pchan,
                        const bAnimVizSettings &avs,
                        bMotionPath *mpath)
  {
    /* Avoid 0 size allocations. Current code to calculate motion paths should
     * sanitize this already [see animviz_verify_motionpaths()], we might however
     * encounter an older file where this was still possible. */
    if (mpath->length == 0) {
      return;
    }

    const bool show_keyframes = (avs.path_viewflag & MOTIONPATH_VIEW_KFRAS);
    const bool show_keyframes_number = (avs.path_viewflag & MOTIONPATH_VIEW_KFNOS);
    const bool show_frame_number = (avs.path_viewflag & MOTIONPATH_VIEW_FNUMS);
    const bool show_lines = (mpath->flag & MOTIONPATH_FLAG_LINES);
    const bool custom_color = (mpath->flag & MOTIONPATH_FLAG_CUSTOM);
    const bool selected = (pchan) ? (pchan->flag & POSE_SELECTED) :
                                    (ob->base_flag & BASE_SELECTED);

    const float3 color_pre = custom_color ? float3(mpath->color) : float3(-1.0f);
    const float3 color_post = custom_color ? float3(mpath->color_post) : float3(-1.0f);

    int stride = max_ii(avs.path_step, 1);
    int current_frame = state.cfra;

    IndexRange frame_range;
    {
      int start, end;
      if (avs.path_type == MOTIONPATH_TYPE_ACFRA) {
        start = current_frame - avs.path_bc;
        end = current_frame + avs.path_ac;
      }
      else {
        start = avs.path_sf;
        end = avs.path_ef;
      }

      if (start > end) {
        std::swap(start, end);
      }
      start = math::clamp(start, mpath->start_frame, mpath->end_frame);
      end = math::clamp(end, mpath->start_frame, mpath->end_frame);

      frame_range = IndexRange::from_begin_end_inclusive(start, end);
    }

    if (frame_range.is_empty()) {
      return;
    }

    int start_index = frame_range.start() - mpath->start_frame;

    Object *camera_eval = nullptr;
    if ((eMotionPath_BakeFlag(avs.path_bakeflag) & MOTIONPATH_BAKE_CAMERA_SPACE) &&
        state.v3d->camera)
    {
      camera_eval = DEG_get_evaluated(state.depsgraph, state.v3d->camera);
    }

    /* Draw curve-line of path. */
    if (show_lines) {
      const int4 motion_path_settings(
          current_frame, int(frame_range.start()), int(frame_range.last()), mpath->start_frame);

      auto &sub = *line_ps_;
      sub.push_constant("mpath_line_settings", motion_path_settings);
      sub.push_constant("line_thickness", mpath->line_thickness);
      sub.push_constant("selected", selected);
      sub.push_constant("custom_color_pre", color_pre);
      sub.push_constant("custom_color_post", color_post);
      sub.push_constant("camera_space_matrix",
                        camera_eval ? camera_eval->object_to_world() : float4x4::identity());

      gpu::Batch *geom = mpath_batch_points_get(mpath);
      /* Only draw the required range. */
      sub.draw_expand(geom, GPU_PRIM_TRIS, 2, 1, frame_range.size() - 1, start_index);
    }

    /* Draw points. */
    {
      int pt_size = max_ii(mpath->line_thickness - 1, 1);
      const int4 motion_path_settings = {pt_size, current_frame, mpath->start_frame, stride};

      auto &sub = *vert_ps_;
      sub.push_constant("mpath_point_settings", motion_path_settings);
      sub.push_constant("show_key_frames", show_keyframes);
      sub.push_constant("custom_color_pre", color_pre);
      sub.push_constant("custom_color_post", color_post);
      sub.push_constant("camera_space_matrix",
                        camera_eval ? camera_eval->object_to_world() : float4x4::identity());

      gpu::Batch *geom = mpath_batch_points_get(mpath);
      /* Only draw the required range. */
      sub.draw(geom, 1, frame_range.size(), start_index);
    }

    /* Draw frame numbers at each frame-step value. */
    if (show_frame_number || (show_keyframes_number && show_keyframes)) {
      uchar4 col, col_kf;
      /* Color Management: Exception here as texts are drawn in sRGB space directly. */
      UI_GetThemeColor3ubv(TH_TEXT_HI, col);
      UI_GetThemeColor3ubv(TH_VERTEX_SELECT, col_kf);
      col.w = col_kf.w = 255;

      auto safe_index = [&](int index) { return math::clamp(index, 0, mpath->length - 1); };

      Span<bMotionPathVert> mpv(mpath->points, mpath->length);
      for (int i = 0; i < frame_range.size(); i += stride) {
        const bMotionPathVert &mpv_curr = mpv[safe_index(start_index + i)];

        int frame = frame_range.start() + i;
        bool is_keyframe = (mpv_curr.flag & MOTIONPATH_VERT_KEY) != 0;

        float3 vert_coordinate(mpv_curr.co);
        if (camera_eval) {
          /* Projecting the point into world space from the camera's POV. */
          vert_coordinate = math::transform_point(camera_eval->object_to_world(), vert_coordinate);
        }

        if ((show_keyframes && show_keyframes_number && is_keyframe) ||
            (show_frame_number && (i == 0)))
        {
          char numstr[32];
          size_t numstr_len = SNPRINTF_RLEN(numstr, " %d", frame);
          DRW_text_cache_add(state.dt,
                             vert_coordinate,
                             numstr,
                             numstr_len,
                             0,
                             0,
                             DRW_TEXT_CACHE_GLOBALSPACE,
                             (is_keyframe) ? col_kf : col);
        }
        else if (show_frame_number) {
          const bMotionPathVert &mpv_prev = mpv[safe_index(start_index + i - stride)];
          const bMotionPathVert &mpv_next = mpv[safe_index(start_index + i + stride)];
          /* Only draw frame number if several consecutive highlighted points
           * don't occur on same point. */
          if (!math::is_equal(float3(mpv_curr.co), float3(mpv_prev.co)) ||
              !math::is_equal(float3(mpv_curr.co), float3(mpv_next.co)))
          {
            char numstr[32];
            size_t numstr_len = SNPRINTF_RLEN(numstr, " %d", frame);
            DRW_text_cache_add(state.dt,
                               vert_coordinate,
                               numstr,
                               numstr_len,
                               0,
                               0,
                               DRW_TEXT_CACHE_GLOBALSPACE,
                               col);
          }
        }
      }
    }
  }

  /* Just convert the CPU cache to GPU cache. */
  /* TODO(fclem) This should go into a draw_cache_impl_motionpath. */
  blender::gpu::VertBuf *mpath_vbo_get(bMotionPath *mpath)
  {
    if (!mpath->points_vbo) {
      GPUVertFormat format = {0};
      /* Match structure of #bMotionPathVert. */
      GPU_vertformat_attr_add(&format, "pos", gpu::VertAttrType::SFLOAT_32_32_32);
      GPU_vertformat_attr_add(&format, "flag", gpu::VertAttrType::SINT_32);
      mpath->points_vbo = GPU_vertbuf_create_with_format(format);
      GPU_vertbuf_data_alloc(*mpath->points_vbo, mpath->length);
      /* meh... a useless `memcpy`. */
      mpath->points_vbo->data<bMotionPathVert>().copy_from({mpath->points, mpath->length});
    }
    return mpath->points_vbo;
  }

  blender::gpu::Batch *mpath_batch_points_get(bMotionPath *mpath)
  {
    if (!mpath->batch_points) {
      mpath->batch_points = GPU_batch_create(GPU_PRIM_POINTS, mpath_vbo_get(mpath), nullptr);
    }
    return mpath->batch_points;
  }
};

}  // namespace blender::draw::overlay
