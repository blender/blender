/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "BKE_camera.h"
#include "BKE_tracking.h"
#include "BLI_math_rotation.h"
#include "DEG_depsgraph_query.hh"
#include "DNA_camera_types.h"
#include "DRW_render.hh"
#include "ED_view3d.hh"

#include "draw_manager_text.hh"
#include "overlay_base.hh"
#include "overlay_empty.hh"

namespace blender::draw::overlay {
struct CameraInstanceData : public ExtraInstanceData {
 public:
  float &volume_start = color_[2];
  float &volume_end = color_[3];
  float &depth = color_[3];
  float &focus = color_[3];
  float4x4 &matrix = object_to_world;
  float &dist_color_id = matrix[0][3];
  float &corner_x = matrix[0][3];
  float &corner_y = matrix[1][3];
  float &center_x = matrix[2][3];
  float &clip_start = matrix[2][3];
  float &mist_start = matrix[2][3];
  float &center_y = matrix[3][3];
  float &clip_end = matrix[3][3];
  float &mist_end = matrix[3][3];

  CameraInstanceData(const CameraInstanceData &data)
      : CameraInstanceData(data.object_to_world, data.color_)
  {
  }

  CameraInstanceData(const float4x4 &p_matrix, const float4 &color)
      : ExtraInstanceData(p_matrix, color, 1.0f) {};
};

/**
 * Camera object display (including stereoscopy).
 * Also camera reconstruction bundles.
 * Also camera reference images (background).
 */
/* TODO(fclem): Split into multiple overlay classes. */
class Cameras : Overlay {
  using CameraInstanceBuf = ShapeInstanceBuf<ExtraInstanceData>;

 private:
  PassSimple ps_ = {"Cameras"};

  /* Camera background images with "Depth" switched to "Back".
   * Shown in camera view behind all objects. */
  PassMain background_ps_ = {"background_ps_"};
  /* Camera background images with "Depth" switched to "Front".
   * Shown in camera view in front of all objects. */
  PassMain foreground_ps_ = {"foreground_ps_"};

  /* Same as `background_ps_` with "View as Render" checked. */
  PassMain background_scene_ps_ = {"background_scene_ps_"};
  /* Same as `foreground_ps_` with "View as Render" checked. */
  PassMain foreground_scene_ps_ = {"foreground_scene_ps_"};

  struct CallBuffers {
    const SelectionType selection_type_;
    CameraInstanceBuf distances_buf = {selection_type_, "camera_distances_buf"};
    CameraInstanceBuf frame_buf = {selection_type_, "camera_frame_buf"};
    CameraInstanceBuf tria_buf = {selection_type_, "camera_tria_buf"};
    CameraInstanceBuf tria_wire_buf = {selection_type_, "camera_tria_wire_buf"};
    CameraInstanceBuf volume_buf = {selection_type_, "camera_volume_buf"};
    CameraInstanceBuf volume_wire_buf = {selection_type_, "camera_volume_wire_buf"};
    CameraInstanceBuf sphere_solid_buf = {selection_type_, "camera_sphere_solid_buf"};
    LinePrimitiveBuf stereo_connect_lines = {selection_type_, "camera_dashed_lines_buf"};
    LinePrimitiveBuf tracking_path = {selection_type_, "camera_tracking_path_buf"};
    Empties::CallBuffers empties{selection_type_};
  } call_buffers_;

  bool images_enabled_ = false;
  bool extras_enabled_ = false;
  bool motion_tracking_enabled_ = false;

  View::OffsetData offset_data_;
  float4x4 depth_bias_winmat_;

 public:
  Cameras(const SelectionType selection_type) : call_buffers_{selection_type} {};

  void begin_sync(Resources &res, const State &state) final
  {
    enabled_ = state.is_space_v3d();
    extras_enabled_ = enabled_ && state.show_extras();
    motion_tracking_enabled_ = enabled_ && state.v3d->flag2 & V3D_SHOW_RECONSTRUCTION;
    images_enabled_ = enabled_ && !res.is_selection() && !state.is_depth_only_drawing;
    enabled_ = extras_enabled_ || images_enabled_ || motion_tracking_enabled_;

    offset_data_ = state.offset_data_get();

    if (extras_enabled_ || motion_tracking_enabled_) {
      call_buffers_.distances_buf.clear();
      call_buffers_.frame_buf.clear();
      call_buffers_.tria_buf.clear();
      call_buffers_.tria_wire_buf.clear();
      call_buffers_.volume_buf.clear();
      call_buffers_.volume_wire_buf.clear();
      call_buffers_.sphere_solid_buf.clear();
      call_buffers_.stereo_connect_lines.clear();
      call_buffers_.tracking_path.clear();
      Empties::begin_sync(call_buffers_.empties);
    }

    if (images_enabled_) {
      /* Init image passes. */
      auto init_pass = [&](PassMain &pass, DRWState draw_state) {
        pass.init();
        pass.state_set(draw_state, state.clipping_plane_count);
        pass.shader_set(res.shaders->image_plane_depth_bias.get());
        pass.bind_ubo(OVERLAY_GLOBALS_SLOT, &res.globals_buf);
        pass.bind_ubo(DRW_CLIPPING_UBO_SLOT, &res.clip_planes_buf);
        pass.push_constant("depth_bias_winmat", &depth_bias_winmat_);
        res.select_bind(pass);
      };

      DRWState draw_state;
      draw_state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA_PREMUL;
      init_pass(background_ps_, draw_state);

      draw_state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA_UNDER_PREMUL;
      init_pass(background_scene_ps_, draw_state);

      draw_state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA_PREMUL;
      init_pass(foreground_ps_, draw_state);
      init_pass(foreground_scene_ps_, draw_state);
    }
  }

  void object_sync(Manager &manager,
                   const ObjectRef &ob_ref,
                   Resources &res,
                   const State &state) final
  {
    if (!enabled_) {
      return;
    }

    const select::ID select_id = res.select_id(ob_ref);

    object_sync_extras(ob_ref, select_id, state, res);

    object_sync_motion_paths(ob_ref, res, state);

    object_sync_images(ob_ref, select_id, manager, state, res);
  }

  void end_sync(Resources &res, const State &state) final
  {
    if (!extras_enabled_ && !motion_tracking_enabled_) {
      return;
    }

    ps_.init();
    ps_.bind_ubo(OVERLAY_GLOBALS_SLOT, &res.globals_buf);
    ps_.bind_ubo(DRW_CLIPPING_UBO_SLOT, &res.clip_planes_buf);
    res.select_bind(ps_);

    {
      PassSimple::Sub &sub_pass = ps_.sub("volume");
      sub_pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA |
                             DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_CULL_BACK,
                         state.clipping_plane_count);
      sub_pass.shader_set(res.shaders->extra_shape.get());
      call_buffers_.volume_buf.end_sync(sub_pass, res.shapes.camera_volume.get());
    }
    {
      PassSimple::Sub &sub_pass = ps_.sub("volume_wire");
      sub_pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA |
                             DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_CULL_BACK,
                         state.clipping_plane_count);
      sub_pass.shader_set(res.shaders->extra_shape.get());
      call_buffers_.volume_wire_buf.end_sync(sub_pass, res.shapes.camera_volume_wire.get());
    }

    {
      PassSimple::Sub &sub_pass = ps_.sub("camera_shapes");
      sub_pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH |
                             DRW_STATE_DEPTH_LESS_EQUAL,
                         state.clipping_plane_count);
      sub_pass.shader_set(res.shaders->extra_shape.get());
      call_buffers_.distances_buf.end_sync(sub_pass, res.shapes.camera_distances.get());
      call_buffers_.frame_buf.end_sync(sub_pass, res.shapes.camera_frame.get());
      call_buffers_.tria_buf.end_sync(sub_pass, res.shapes.camera_tria.get());
      call_buffers_.tria_wire_buf.end_sync(sub_pass, res.shapes.camera_tria_wire.get());
      call_buffers_.sphere_solid_buf.end_sync(sub_pass, res.shapes.sphere_low_detail.get());
    }

    {
      PassSimple::Sub &sub_pass = ps_.sub("camera_extra_wire");
      sub_pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH |
                             DRW_STATE_DEPTH_LESS_EQUAL,
                         state.clipping_plane_count);
      sub_pass.shader_set(res.shaders->extra_wire.get());
      call_buffers_.stereo_connect_lines.end_sync(sub_pass);
      call_buffers_.tracking_path.end_sync(sub_pass);
    }

    PassSimple::Sub &sub_pass = ps_.sub("empties");
    Empties::end_sync(res, state, sub_pass, call_buffers_.empties);
  }

  void pre_draw(Manager &manager, View &view) final
  {
    if (!images_enabled_) {
      return;
    }

    manager.generate_commands(background_scene_ps_, view);
    manager.generate_commands(foreground_scene_ps_, view);
    manager.generate_commands(background_ps_, view);
    manager.generate_commands(foreground_ps_, view);

    depth_bias_winmat_ = offset_data_.winmat_polygon_offset(view.winmat(), -1.0f);
  }

  void draw_line(Framebuffer &framebuffer, Manager &manager, View &view) final
  {
    if (!extras_enabled_ && !motion_tracking_enabled_) {
      return;
    }

    GPU_framebuffer_bind(framebuffer);
    manager.submit(ps_, view);
  }

  void draw_scene_background_images(gpu::FrameBuffer *framebuffer, Manager &manager, View &view)
  {
    if (!images_enabled_) {
      return;
    }

    GPU_framebuffer_bind(framebuffer);
    manager.submit_only(background_scene_ps_, view);
    manager.submit_only(foreground_scene_ps_, view);
  }

  void draw_background_images(Framebuffer &framebuffer, Manager &manager, View &view)
  {
    if (!images_enabled_) {
      return;
    }

    GPU_framebuffer_bind(framebuffer);
    manager.submit_only(background_ps_, view);
  }

  void draw_in_front(Framebuffer &framebuffer, Manager &manager, View &view)
  {
    if (!images_enabled_) {
      return;
    }

    GPU_framebuffer_bind(framebuffer);
    manager.submit_only(foreground_ps_, view);
  }

 private:
  void object_sync_extras(const ObjectRef &ob_ref,
                          select::ID select_id,
                          const State &state,
                          Resources &res)
  {
    if (!extras_enabled_) {
      return;
    }

    Object *ob = ob_ref.object;
    float4x4 mat = ob->object_to_world();
    /* Normalize matrix scale. */
    mat.view<3, 3>() = math::normalize(mat.view<3, 3>());
    CameraInstanceData data(mat, res.object_wire_color(ob_ref, state));

    const View3D *v3d = state.v3d;
    const Scene *scene = state.scene;
    const RegionView3D *rv3d = state.rv3d;

    const Camera &cam = DRW_object_get_data_for_drawing<Camera>(*ob);
    const Object *camera_object = DEG_get_evaluated(state.depsgraph, v3d->camera);
    const bool is_select = res.is_selection();
    const bool is_active = (ob == camera_object);
    const bool is_camera_view = (is_active && (rv3d->persp == RV3D_CAMOB));

    const bool is_multiview = (scene->r.scemode & R_MULTIVIEW) != 0;
    const bool is_stereo3d_view = (scene->r.views_format == SCE_VIEWS_FORMAT_STEREO_3D);
    const bool is_stereo3d_display_extra = is_active && is_multiview && (!is_camera_view) &&
                                           ((v3d->stereo3d_flag) != 0);
    const bool is_selection_camera_stereo = is_select && is_camera_view && is_multiview &&
                                            is_stereo3d_view;

    float3 scale = math::to_scale(ob->object_to_world());
    /* BKE_camera_multiview_model_matrix already accounts for scale, don't do it here. */
    if (is_selection_camera_stereo) {
      scale = float3(1.0f);
    }
    else if (ELEM(0.0f, scale.x, scale.y, scale.z)) {
      /* Avoid division by 0. */
      return;
    }

    float4x3 vecs;
    float2 aspect_ratio;
    float2 shift;
    float drawsize;
    BKE_camera_view_frame_ex(scene,
                             &cam,
                             cam.drawsize,
                             is_camera_view,
                             1.0f / scale,
                             aspect_ratio,
                             shift,
                             &drawsize,
                             vecs.ptr());

    /* Apply scale to simplify the rest of the drawing. */
    for (int i = 0; i < 4; i++) {
      vecs[i] *= scale;
      /* Project to z=-1 plane. Makes positioning / scaling easier. (see shader) */
      mul_v2_fl(vecs[i], 1.0f / std::abs(vecs[i].z));
    }

    /* Frame coords */
    const float2 center = (vecs[0].xy() + vecs[2].xy()) * 0.5f;
    const float2 corner = vecs[0].xy() - center.xy();
    data.corner_x = corner.x;
    data.corner_y = corner.y;
    data.center_x = center.x;
    data.center_y = center.y;
    data.depth = vecs[0].z;

    if (is_camera_view) {
      if (!state.is_image_render) {
        /* Only draw the frame. */
        if (is_multiview) {
          float4x4 mat;
          const bool is_right = v3d->multiview_eye == STEREO_RIGHT_ID;
          const char *view_name = is_right ? STEREO_RIGHT_NAME : STEREO_LEFT_NAME;
          BKE_camera_multiview_model_matrix(&scene->r, ob, view_name, mat.ptr());
          data.center_x += camera_offaxis_shiftx_get(scene, ob, data.corner_x, is_right);
          for (int i : IndexRange(4)) {
            /* Partial copy to avoid overriding packed data. */
            for (int j : IndexRange(3)) {
              data.matrix[i][j] = mat[i][j];
            }
          }
        }
        data.depth *= -1.0f; /* Hides the back of the camera wires (see shader). */
        call_buffers_.frame_buf.append(data, select_id);
      }
    }
    else {
      /* Stereo cameras, volumes, plane drawing. */
      if (is_stereo3d_display_extra) {
        sync_stereoscopy_extra(data, select_id, scene, v3d, res, ob);
      }
      else {
        call_buffers_.frame_buf.append(data, select_id);
      }
    }

    if (!is_camera_view) {
      /* Triangle. */
      float tria_size = 0.7f * drawsize / fabsf(data.depth);
      float tria_margin = 0.1f * drawsize / fabsf(data.depth);
      data.center_x = center.x;
      data.center_y = center.y + data.corner_y + tria_margin + tria_size;
      data.corner_x = data.corner_y = -tria_size;
      (is_active ? call_buffers_.tria_buf : call_buffers_.tria_wire_buf).append(data, select_id);
    }

    if (cam.flag & CAM_SHOWLIMITS) {
      /* Scale focus point. */
      data.matrix.x_axis() *= cam.drawsize;
      data.matrix.y_axis() *= cam.drawsize;

      data.dist_color_id = (is_active) ? 3 : 2;
      data.focus = -BKE_camera_object_dof_distance(ob);
      data.clip_start = cam.clip_start;
      data.clip_end = cam.clip_end;
      call_buffers_.distances_buf.append(data, select_id);
    }

    if (cam.flag & CAM_SHOWMIST) {
      World *world = scene->world;
      if (world) {
        data.dist_color_id = (is_active) ? 1 : 0;
        data.focus = 1.0f; /* Disable */
        data.mist_start = world->miststa;
        data.mist_end = world->miststa + world->mistdist;
        call_buffers_.distances_buf.append(data, select_id);
      }
    }
  }

  void object_sync_motion_paths(const ObjectRef &ob_ref, Resources &res, const State &state)
  {
    if (!motion_tracking_enabled_) {
      return;
    }

    Object *ob = ob_ref.object;
    const View3D *v3d = state.v3d;
    const Scene *scene = state.scene;

    MovieClip *clip = BKE_object_movieclip_get(const_cast<Scene *>(scene), ob, false);
    if (clip == nullptr) {
      return;
    }

    const float4 &color = res.object_wire_color(ob_ref, state);

    const bool is_selection = res.is_selection();
    const bool is_solid_bundle = (v3d->bundle_drawtype == OB_EMPTY_SPHERE) &&
                                 ((v3d->shading.type != OB_SOLID) || !XRAY_FLAG_ENABLED(v3d));

    MovieTracking *tracking = &clip->tracking;
    /* Index must start in 1, to mimic BKE_tracking_track_get_for_selection_index. */
    int track_index = 1;

    float4 bundle_color_custom;
    float *bundle_color_solid = res.theme.colors.bundle_solid;
    float *bundle_color_unselected = res.theme.colors.wire;
    uchar4 text_color_selected, text_color_unselected;
    /* Color Management: Exception here as texts are drawn in sRGB space directly. */
    UI_GetThemeColor4ubv(TH_SELECT, text_color_selected);
    UI_GetThemeColor4ubv(TH_TEXT, text_color_unselected);

    float4x4 camera_mat;
    BKE_tracking_get_camera_object_matrix(ob, camera_mat.ptr());

    const float4x4 object_to_world{ob->object_to_world().ptr()};

    for (MovieTrackingObject *tracking_object :
         ListBaseWrapper<MovieTrackingObject>(&tracking->objects))
    {
      float4x4 tracking_object_mat;

      if (tracking_object->flag & TRACKING_OBJECT_CAMERA) {
        tracking_object_mat = camera_mat;
      }
      else {
        const int framenr = BKE_movieclip_remap_scene_to_clip_frame(
            clip, DEG_get_ctime(state.depsgraph));

        float4x4 object_mat;
        BKE_tracking_camera_get_reconstructed_interpolate(
            tracking, tracking_object, framenr, object_mat.ptr());

        tracking_object_mat = object_to_world * math::invert(object_mat);
      }

      for (MovieTrackingTrack *track :
           ListBaseWrapper<MovieTrackingTrack>(&tracking_object->tracks))
      {
        if ((track->flag & TRACK_HAS_BUNDLE) == 0) {
          continue;
        }
        bool is_selected = TRACK_SELECTED(track);

        float4x4 bundle_mat = math::translate(tracking_object_mat, float3(track->bundle_pos));

        const float *bundle_color;
        if (track->flag & TRACK_CUSTOMCOLOR) {
          /* Meh, hardcoded srgb transform here. */
          /* TODO: change the actual DNA color to be linear. */
          srgb_to_linearrgb_v3_v3(bundle_color_custom, track->color);
          bundle_color_custom[3] = 1.0;

          bundle_color = bundle_color_custom;
        }
        else if (is_solid_bundle) {
          bundle_color = bundle_color_solid;
        }
        else if (is_selected) {
          bundle_color = color;
        }
        else {
          bundle_color = bundle_color_unselected;
        }

        const select::ID track_select_id = res.select_id(ob_ref, track_index++ << 16);
        if (is_solid_bundle) {
          if (is_selected) {
            Empties::object_sync(track_select_id,
                                 bundle_mat,
                                 v3d->bundle_size,
                                 v3d->bundle_drawtype,
                                 color,
                                 call_buffers_.empties);
          }

          call_buffers_.sphere_solid_buf.append(
              ExtraInstanceData{bundle_mat, float4(float3(bundle_color), 1.0f), v3d->bundle_size},
              track_select_id);
        }
        else {
          Empties::object_sync(track_select_id,
                               bundle_mat,
                               v3d->bundle_size,
                               v3d->bundle_drawtype,
                               bundle_color,
                               call_buffers_.empties);
        }

        if ((v3d->flag2 & V3D_SHOW_BUNDLENAME) && !is_selection) {
          DRW_text_cache_add(state.dt,
                             bundle_mat[3],
                             track->name,
                             strlen(track->name),
                             10,
                             0,
                             DRW_TEXT_CACHE_GLOBALSPACE | DRW_TEXT_CACHE_STRING_PTR,
                             is_selected ? text_color_selected : text_color_unselected);
        }
      }

      if ((v3d->flag2 & V3D_SHOW_CAMERAPATH) && (tracking_object->flag & TRACKING_OBJECT_CAMERA) &&
          !is_selection)
      {
        const MovieTrackingReconstruction *reconstruction = &tracking_object->reconstruction;

        if (reconstruction->camnr) {
          const MovieReconstructedCamera *camera = reconstruction->cameras;
          float3 v0, v1 = float3(0.0f);
          for (int a = 0; a < reconstruction->camnr; a++, camera++) {
            v0 = v1;
            v1 = math::transform_point(camera_mat, float3(camera->mat[3]));
            if (a > 0) {
              /* This one is suboptimal (gl_lines instead of gl_line_strip)
               * but we keep this for simplicity */
              call_buffers_.tracking_path.append(v0, v1, TH_CAMERA_PATH);
            }
          }
        }
      }
    }
  }

  void object_sync_images(const ObjectRef &ob_ref,
                          select::ID select_id,
                          Manager &manager,
                          const State &state,
                          Resources &res)
  {
    Object *ob = ob_ref.object;
    const Camera &cam = DRW_object_get_data_for_drawing<Camera>(*ob_ref.object);
    const Object *camera_object = DEG_get_evaluated(state.depsgraph, state.v3d->camera);

    const bool is_active = ob_ref.object == camera_object;
    const bool is_camera_view = (is_active && (state.rv3d->persp == RV3D_CAMOB));
    const bool show_image = (cam.flag & CAM_SHOW_BG_IMAGE) &&
                            !BLI_listbase_is_empty(&cam.bg_images);
    const bool show_frame = BKE_object_empty_image_frame_is_visible_in_view3d(ob, state.rv3d);

    if (!images_enabled_ || !is_camera_view || !show_image || !show_frame) {
      return;
    }

    const bool stereo_eye = Images::images_stereo_eye(state.scene, state.v3d) == STEREO_LEFT_ID;
    const char *viewname = (stereo_eye == STEREO_LEFT_ID) ? STEREO_RIGHT_NAME : STEREO_LEFT_NAME;
    float4x4 modelmat;
    BKE_camera_multiview_model_matrix(&state.scene->r, ob, viewname, modelmat.ptr());

    for (const CameraBGImage *bgpic : ConstListBaseWrapper<CameraBGImage>(&cam.bg_images)) {
      if (bgpic->flag & CAM_BGIMG_FLAG_DISABLED) {
        continue;
      }

      float aspect = 1.0;
      bool use_alpha_premult;
      bool use_view_transform = false;
      float4x4 mat;

      /* retrieve the image we want to show, continue to next when no image could be found */
      gpu::Texture *tex = image_camera_background_texture_get(
          bgpic, state, res, aspect, use_alpha_premult, use_view_transform);

      if (tex) {
        image_camera_background_matrix_get(&cam, bgpic, state, aspect, mat);

        const bool is_foreground = (bgpic->flag & CAM_BGIMG_FLAG_FOREGROUND) != 0;
        /* Alpha is clamped just below 1.0 to fix background images to interfere with foreground
         * images. Without this a background image with 1.0 will be rendered on top of a
         * transparent foreground image due to the different blending modes they use. */
        const float4 color_premult_alpha{1.0f, 1.0f, 1.0f, std::min(bgpic->alpha, 0.999999f)};

        PassMain &pass = is_foreground ?
                             (use_view_transform ? foreground_scene_ps_ : foreground_ps_) :
                             (use_view_transform ? background_scene_ps_ : background_ps_);
        pass.bind_texture("img_tx", tex);
        pass.push_constant("img_premultiplied", use_alpha_premult);
        pass.push_constant("img_alpha_blend", true);
        pass.push_constant("is_camera_background", true);
        pass.push_constant("depth_set", true);
        pass.push_constant("ucolor", color_premult_alpha);
        ResourceHandleRange res_handle = manager.resource_handle(mat);
        pass.draw(res.shapes.quad_solid.get(), res_handle, select_id.get());
      }
    }
  }

  static void image_camera_background_matrix_get(const Camera *cam,
                                                 const CameraBGImage *bgpic,
                                                 const State &state,
                                                 const float image_aspect,
                                                 float4x4 &rmat)
  {
    float4x4 rotate, scale = float4x4::identity(), translate = float4x4::identity();

    axis_angle_to_mat4_single(rotate.ptr(), 'Z', -bgpic->rotation);

    /* Normalized Object space camera frame corners. */
    float cam_corners[4][3];
    BKE_camera_view_frame(state.scene, cam, cam_corners);
    float cam_width = fabsf(cam_corners[0][0] - cam_corners[3][0]);
    float cam_height = fabsf(cam_corners[0][1] - cam_corners[1][1]);
    float cam_aspect = cam_width / cam_height;

    if (bgpic->flag & CAM_BGIMG_FLAG_CAMERA_CROP) {
      /* Crop. */
      if (image_aspect > cam_aspect) {
        scale[0][0] *= cam_height * image_aspect;
        scale[1][1] *= cam_height;
      }
      else {
        scale[0][0] *= cam_width;
        scale[1][1] *= cam_width / image_aspect;
      }
    }
    else if (bgpic->flag & CAM_BGIMG_FLAG_CAMERA_ASPECT) {
      /* Fit. */
      if (image_aspect > cam_aspect) {
        scale[0][0] *= cam_width;
        scale[1][1] *= cam_width / image_aspect;
      }
      else {
        scale[0][0] *= cam_height * image_aspect;
        scale[1][1] *= cam_height;
      }
    }
    else {
      /* Stretch. */
      scale[0][0] *= cam_width;
      scale[1][1] *= cam_height;
    }

    translate[3][0] = bgpic->offset[0];
    translate[3][1] = bgpic->offset[1];
    translate[3][2] = cam_corners[0][2];
    if (cam->type == CAM_ORTHO) {
      translate[3][0] *= cam->ortho_scale;
      translate[3][1] *= cam->ortho_scale;
    }
    /* These lines are for keeping 2.80 behavior and could be removed to keep 2.79 behavior. */
    translate[3][0] *= min_ff(1.0f, cam_aspect);
    translate[3][1] /= max_ff(1.0f, cam_aspect) * (image_aspect / cam_aspect);
    /* quad is -1..1 so divide by 2. */
    scale[0][0] *= 0.5f * bgpic->scale * ((bgpic->flag & CAM_BGIMG_FLAG_FLIP_X) ? -1.0 : 1.0);
    scale[1][1] *= 0.5f * bgpic->scale * ((bgpic->flag & CAM_BGIMG_FLAG_FLIP_Y) ? -1.0 : 1.0);
    /* Camera shift. (middle of cam_corners) */
    translate[3][0] += (cam_corners[0][0] + cam_corners[2][0]) * 0.5f;
    translate[3][1] += (cam_corners[0][1] + cam_corners[2][1]) * 0.5f;

    rmat = translate * rotate * scale;
  }

  gpu::Texture *image_camera_background_texture_get(const CameraBGImage *bgpic,
                                                    const State &state,
                                                    Resources &res,
                                                    float &r_aspect,
                                                    bool &r_use_alpha_premult,
                                                    bool &r_use_view_transform)
  {
    ::Image *image = bgpic->ima;
    ImageUser *iuser = (ImageUser *)&bgpic->iuser;
    MovieClip *clip = nullptr;
    gpu::Texture *tex = nullptr;
    float aspect_x, aspect_y;
    int width, height;
    int ctime = int(DEG_get_ctime(state.depsgraph));
    r_use_alpha_premult = false;
    r_use_view_transform = false;

    switch (bgpic->source) {
      case CAM_BGIMG_SOURCE_IMAGE: {
        if (image == nullptr) {
          return nullptr;
        }
        r_use_alpha_premult = (image->alpha_mode == IMA_ALPHA_PREMUL);
        r_use_view_transform = (image->flag & IMA_VIEW_AS_RENDER) != 0;

        BKE_image_user_frame_calc(image, iuser, ctime);
        if (image->source == IMA_SRC_SEQUENCE && !(iuser->flag & IMA_USER_FRAME_IN_RANGE)) {
          /* Frame is out of range, don't show. */
          return nullptr;
        }

        Images::stereo_setup(state.scene, state.v3d, image, iuser);

        iuser->scene = (Scene *)state.scene;
        tex = BKE_image_get_gpu_viewer_texture(image, iuser);
        iuser->scene = nullptr;

        if (tex == nullptr) {
          return nullptr;
        }

        width = GPU_texture_original_width(tex);
        height = GPU_texture_original_height(tex);

        aspect_x = bgpic->ima->aspx;
        aspect_y = bgpic->ima->aspy;
        break;
      }

      case CAM_BGIMG_SOURCE_MOVIE: {
        if (bgpic->flag & CAM_BGIMG_FLAG_CAMERACLIP) {
          if (state.scene->camera) {
            clip = BKE_object_movieclip_get((Scene *)state.scene, state.scene->camera, true);
          }
        }
        else {
          clip = bgpic->clip;
        }

        if (clip == nullptr) {
          return nullptr;
        }

        BKE_movieclip_user_set_frame((MovieClipUser *)&bgpic->cuser, ctime);
        tex = BKE_movieclip_get_gpu_texture(clip, (MovieClipUser *)&bgpic->cuser);
        if (tex == nullptr) {
          return nullptr;
        }

        aspect_x = clip->aspx;
        aspect_y = clip->aspy;
        r_use_view_transform = true;

        BKE_movieclip_get_size(clip, &bgpic->cuser, &width, &height);

        /* Save for freeing. */
        res.bg_movie_clips.append(clip);
        break;
      }

      default:
        /* Unsupported type. */
        return nullptr;
    }

    r_aspect = (width * aspect_x) / (height * aspect_y);
    return tex;
  }

  /**
   * Draw the stereo 3d support elements (cameras, plane, volume).
   * They are only visible when not looking through the camera:
   */
  void sync_stereoscopy_extra(const CameraInstanceData &instdata,
                              const select::ID cam_select_id,
                              const Scene *scene,
                              const View3D *v3d,
                              Resources &res,
                              Object *ob)
  {
    CameraInstanceData stereodata = instdata;

    const Camera &cam = DRW_object_get_data_for_drawing<const Camera>(*ob);
    const char *viewnames[2] = {STEREO_LEFT_NAME, STEREO_RIGHT_NAME};

    const bool is_stereo3d_cameras = (v3d->stereo3d_flag & V3D_S3D_DISPCAMERAS) != 0;
    const bool is_stereo3d_plane = (v3d->stereo3d_flag & V3D_S3D_DISPPLANE) != 0;
    const bool is_stereo3d_volume = (v3d->stereo3d_flag & V3D_S3D_DISPVOLUME) != 0;
    const bool is_selection = res.is_selection();

    if (!is_stereo3d_cameras) {
      /* Draw single camera. */
      call_buffers_.frame_buf.append(instdata, cam_select_id);
    }

    for (const int eye : IndexRange(2)) {
      ob = BKE_camera_multiview_render(scene, ob, viewnames[eye]);
      BKE_camera_multiview_model_matrix(&scene->r, ob, viewnames[eye], stereodata.matrix.ptr());

      stereodata.corner_x = instdata.corner_x;
      stereodata.corner_y = instdata.corner_y;
      stereodata.center_x = instdata.center_x;
      stereodata.center_y = instdata.center_y;
      stereodata.depth = instdata.depth;

      stereodata.center_x += camera_offaxis_shiftx_get(scene, ob, instdata.corner_x, eye);

      if (is_stereo3d_cameras) {
        call_buffers_.frame_buf.append(stereodata, cam_select_id);

        /* Connecting line between cameras. */
        call_buffers_.stereo_connect_lines.append(stereodata.matrix.location(),
                                                  instdata.object_to_world.location(),
                                                  res.theme.colors.wire,
                                                  cam_select_id);
      }

      if (is_stereo3d_volume && !is_selection) {
        float r = (eye == 1) ? 2.0f : 1.0f;

        stereodata.volume_start = -cam.clip_start;
        stereodata.volume_end = -cam.clip_end;
        /* Encode eye + intensity and alpha (see shader) */
        stereodata.color_.x = r + 0.15f;
        stereodata.color_.y = 1.0f;
        call_buffers_.volume_wire_buf.append(stereodata, cam_select_id);

        if (v3d->stereo3d_volume_alpha > 0.0f) {
          /* Encode eye + intensity and alpha (see shader) */
          stereodata.color_.x = r + 0.999f;
          stereodata.color_.y = v3d->stereo3d_volume_alpha;
          call_buffers_.volume_buf.append(stereodata, cam_select_id);
        }
        /* restore */
        stereodata.color_.x = instdata.color_.x;
        stereodata.color_.y = instdata.color_.y;
        stereodata.color_.z = instdata.color_.z;
      }
    }

    if (is_stereo3d_plane && !is_selection) {
      if (cam.stereo.convergence_mode == CAM_S3D_TOE) {
        /* There is no real convergence plane but we highlight the center
         * point where the views are pointing at. */
        // stereodata.matrix.x_axis() = float3(0.0f); /* We reconstruct from Z and Y */
        // stereodata.matrix.y_axis() = float3(0.0f); /* Y doesn't change */
        stereodata.matrix.z_axis() = float3(0.0f);
        stereodata.matrix.location() = float3(0.0f);
        for (int i : IndexRange(2)) {
          float4x4 mat;
          /* Need normalized version here. */
          BKE_camera_multiview_model_matrix(&scene->r, ob, viewnames[i], mat.ptr());
          stereodata.matrix.z_axis() += mat.z_axis();
          stereodata.matrix.location() += mat.location() * 0.5f;
        }
        stereodata.matrix.z_axis() = math::normalize(stereodata.matrix.z_axis());
        stereodata.matrix.x_axis() = math::cross(stereodata.matrix.y_axis(),
                                                 stereodata.matrix.z_axis());
      }
      else if (cam.stereo.convergence_mode == CAM_S3D_PARALLEL) {
        /* Show plane at the given distance between the views even if it makes no sense. */
        stereodata.matrix.location() = float3(0.0f);
        for (int i : IndexRange(2)) {
          float4x4 mat;
          BKE_camera_multiview_model_matrix_scaled(&scene->r, ob, viewnames[i], mat.ptr());
          stereodata.matrix.location() += mat.location() * 0.5f;
        }
      }
      else if (cam.stereo.convergence_mode == CAM_S3D_OFFAXIS) {
        /* Nothing to do. Everything is already setup. */
      }
      stereodata.volume_start = -cam.stereo.convergence_distance;
      stereodata.volume_end = -cam.stereo.convergence_distance;
      /* Encode eye + intensity and alpha (see shader) */
      stereodata.color_.x = 0.1f;
      stereodata.color_.y = 1.0f;
      call_buffers_.volume_wire_buf.append(stereodata, cam_select_id);

      if (v3d->stereo3d_convergence_alpha > 0.0f) {
        /* Encode eye + intensity and alpha (see shader) */
        stereodata.color_.x = 0.0f;
        stereodata.color_.y = v3d->stereo3d_convergence_alpha;
        call_buffers_.volume_buf.append(stereodata, cam_select_id);
      }
    }
  }

  static float camera_offaxis_shiftx_get(const Scene *scene,
                                         const Object *ob,
                                         float corner_x,
                                         bool right_eye)
  {
    const Camera &cam = DRW_object_get_data_for_drawing<const Camera>(*ob);
    if (cam.stereo.convergence_mode == CAM_S3D_OFFAXIS) {
      const char *viewnames[2] = {STEREO_LEFT_NAME, STEREO_RIGHT_NAME};
      const float shiftx = BKE_camera_multiview_shift_x(&scene->r, ob, viewnames[right_eye]);
      const float delta_shiftx = shiftx - cam.shiftx;
      const float width = corner_x * 2.0f;
      return delta_shiftx * width;
    }

    return 0.0;
  }
};

}  // namespace blender::draw::overlay
