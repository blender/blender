/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "overlay_next_base.hh"
#include "overlay_next_image.hh"

namespace blender::draw::overlay {

class Empties : Overlay {
  friend class Cameras;
  using EmptyInstanceBuf = ShapeInstanceBuf<ExtraInstanceData>;

 private:
  /* Images added by Image > Background. Both added in preset view (like Top, Front, ..) and in
   * custom view. Object property "In Front" unchecked. */
  PassSortable images_back_ps_ = {"images_back_ps_"};
  /* All Empty images from cases of `images_ps_`, `images_blend_ps_`, `images_back_ps_`
   * with object property "In Front" checked. */
  PassSortable images_front_ps_ = {"images_front_ps_"};

  /* Images added by Empty > Image and Image > Reference with unchecked image "Opacity".
   * Object property "In Front" unchecked. */
  PassMain images_ps_ = {"images_ps_"};
  /* Images added by Empty > Image and Image > Reference with image "Opacity" checked.
   * Object property "In Front" unchecked. */
  PassSortable images_blend_ps_ = {"images_blend_ps_"};

  PassSimple ps_ = {"Empties"};

  struct CallBuffers {
    const SelectionType selection_type_;
    EmptyInstanceBuf plain_axes_buf = {selection_type_, "plain_axes_buf"};
    EmptyInstanceBuf single_arrow_buf = {selection_type_, "single_arrow_buf"};
    EmptyInstanceBuf cube_buf = {selection_type_, "cube_buf"};
    EmptyInstanceBuf circle_buf = {selection_type_, "circle_buf"};
    EmptyInstanceBuf sphere_buf = {selection_type_, "sphere_buf"};
    EmptyInstanceBuf cone_buf = {selection_type_, "cone_buf"};
    EmptyInstanceBuf arrows_buf = {selection_type_, "arrows_buf"};
    EmptyInstanceBuf image_buf = {selection_type_, "image_buf"};
  } call_buffers_;

  float4x4 depth_bias_winmat_;

 public:
  Empties(const SelectionType selection_type) : call_buffers_{selection_type} {};

  /* TODO(fclem): Remove dependency on view. */
  void begin_sync(Resources &res, const State &state, View &view)
  {
    enabled_ = state.is_space_v3d() && state.show_extras();

    if (!enabled_) {
      return;
    }

    depth_bias_winmat_ = winmat_polygon_offset(
        view.winmat(), state.view_dist_get(view.winmat()), -1.0f);

    auto init_pass = [&](PassMain &pass, DRWState draw_state) {
      pass.init();
      pass.state_set(draw_state, state.clipping_plane_count);
      pass.shader_set(res.shaders.image_plane_depth_bias.get());
      pass.push_constant("depth_bias_winmat", depth_bias_winmat_);
      pass.bind_ubo("globalsBlock", &res.globals_buf);
      res.select_bind(pass);
    };

    auto init_sortable = [&](PassSortable &pass, DRWState draw_state) {
      pass.init();
      PassMain::Sub &sub = pass.sub("ResourceBind", -FLT_MAX);
      sub.state_set(draw_state, state.clipping_plane_count);
      res.select_bind(pass, sub);
    };

    DRWState draw_state;

    draw_state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS;
    init_pass(images_ps_, draw_state);

    draw_state = DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_ALPHA_PREMUL;
    init_sortable(images_back_ps_, draw_state);
    init_sortable(images_blend_ps_, draw_state);

    draw_state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA_PREMUL;
    init_sortable(images_front_ps_, draw_state);

    begin_sync(call_buffers_);
  }

  static void begin_sync(CallBuffers &call_buffers)
  {
    call_buffers.plain_axes_buf.clear();
    call_buffers.single_arrow_buf.clear();
    call_buffers.cube_buf.clear();
    call_buffers.circle_buf.clear();
    call_buffers.sphere_buf.clear();
    call_buffers.cone_buf.clear();
    call_buffers.arrows_buf.clear();
    call_buffers.image_buf.clear();
  }

  /* TODO(fclem): Remove dependency on shapes. Pass it to the constructor. */
  void object_sync(const ObjectRef &ob_ref,
                   ShapeCache &shapes,
                   Manager &manager,
                   Resources &res,
                   const State &state)
  {
    if (!enabled_) {
      return;
    }

    const float4 color = res.object_wire_color(ob_ref, state);
    const select::ID select_id = res.select_id(ob_ref);
    if (ob_ref.object->empty_drawtype == OB_EMPTY_IMAGE) {
      image_sync(ob_ref, select_id, shapes, manager, res, state, call_buffers_.image_buf);
      return;
    }
    object_sync(select_id,
                ob_ref.object->object_to_world(),
                ob_ref.object->empty_drawsize,
                ob_ref.object->empty_drawtype,
                color,
                call_buffers_);
  }

  static void object_sync(const select::ID select_id,
                          const float4x4 &matrix,
                          const float draw_size,
                          const char empty_drawtype,
                          const float4 &color,
                          CallBuffers &call_buffers)
  {
    ExtraInstanceData data(matrix, color, draw_size);

    switch (empty_drawtype) {
      case OB_PLAINAXES:
        call_buffers.plain_axes_buf.append(data, select_id);
        break;
      case OB_SINGLE_ARROW:
        call_buffers.single_arrow_buf.append(data, select_id);
        break;
      case OB_CUBE:
        call_buffers.cube_buf.append(data, select_id);
        break;
      case OB_CIRCLE:
        call_buffers.circle_buf.append(data, select_id);
        break;
      case OB_EMPTY_SPHERE:
        call_buffers.sphere_buf.append(data, select_id);
        break;
      case OB_EMPTY_CONE:
        call_buffers.cone_buf.append(data, select_id);
        break;
      case OB_ARROWS:
        call_buffers.arrows_buf.append(data, select_id);
        break;
    }
  }

  void end_sync(Resources &res, const ShapeCache &shapes, const State &state) final
  {
    if (!enabled_) {
      return;
    }

    ps_.init();
    res.select_bind(ps_);
    end_sync(res, shapes, state, ps_, call_buffers_);
  }

  static void end_sync(Resources &res,
                       const ShapeCache &shapes,
                       const State &state,
                       PassSimple::Sub &ps,
                       CallBuffers &call_buffers)
  {
    ps.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL,
                 state.clipping_plane_count);
    ps.shader_set(res.shaders.extra_shape.get());
    ps.bind_ubo("globalsBlock", &res.globals_buf);

    call_buffers.plain_axes_buf.end_sync(ps, shapes.plain_axes.get());
    call_buffers.single_arrow_buf.end_sync(ps, shapes.single_arrow.get());
    call_buffers.cube_buf.end_sync(ps, shapes.cube.get());
    call_buffers.circle_buf.end_sync(ps, shapes.circle.get());
    call_buffers.sphere_buf.end_sync(ps, shapes.empty_sphere.get());
    call_buffers.cone_buf.end_sync(ps, shapes.empty_cone.get());
    call_buffers.arrows_buf.end_sync(ps, shapes.arrows.get());
    call_buffers.image_buf.end_sync(ps, shapes.quad_wire.get());
  }

  void pre_draw(Manager &manager, View &view) final
  {
    if (!enabled_) {
      return;
    }

    manager.generate_commands(images_back_ps_, view);
    manager.generate_commands(images_ps_, view);
    manager.generate_commands(images_blend_ps_, view);
    manager.generate_commands(images_front_ps_, view);
  }

  void draw_line(Framebuffer &framebuffer, Manager &manager, View &view) final
  {
    if (!enabled_) {
      return;
    }

    GPU_framebuffer_bind(framebuffer);
    manager.submit(ps_, view);
  }

  void draw_background_images(Framebuffer &framebuffer, Manager &manager, View &view)
  {
    if (!enabled_) {
      return;
    }

    GPU_framebuffer_bind(framebuffer);
    manager.submit_only(images_back_ps_, view);
  }

  void draw_images(Framebuffer &framebuffer, Manager &manager, View &view)
  {
    if (!enabled_) {
      return;
    }

    GPU_framebuffer_bind(framebuffer);

    manager.submit_only(images_ps_, view);
    manager.submit_only(images_blend_ps_, view);
  }

  void draw_in_front_images(Framebuffer &framebuffer, Manager &manager, View &view)
  {
    if (!enabled_) {
      return;
    }

    GPU_framebuffer_bind(framebuffer);

    manager.submit_only(images_front_ps_, view);
  }

 private:
  void image_sync(const ObjectRef &ob_ref,
                  select::ID select_id,
                  ShapeCache &shapes,
                  Manager &manager,
                  Resources &res,
                  const State &state,
                  EmptyInstanceBuf &empty_image_buf)
  {
    Object *ob = ob_ref.object;
    GPUTexture *tex = nullptr;
    ::Image *ima = static_cast<::Image *>(ob_ref.object->data);
    float4x4 mat;

    const bool show_frame = BKE_object_empty_image_frame_is_visible_in_view3d(ob, state.rv3d);
    const bool show_image = show_frame &&
                            BKE_object_empty_image_data_is_visible_in_view3d(ob, state.rv3d);
    const bool use_alpha_blend = (ob_ref.object->empty_image_flag &
                                  OB_EMPTY_IMAGE_USE_ALPHA_BLEND) != 0;
    const bool use_alpha_premult = ima && (ima->alpha_mode == IMA_ALPHA_PREMUL);

    if (!show_frame) {
      return;
    }

    {
      /* Calling 'BKE_image_get_size' may free the texture. Get the size from 'tex' instead,
       * see: #59347 */
      int2 size = int2(0);
      if (ima != nullptr) {
        ImageUser iuser = *ob->iuser;
        Images::stereo_setup(state.scene, state.v3d, ima, &iuser);
        tex = BKE_image_get_gpu_texture(ima, &iuser);
        if (tex) {
          size = int2(GPU_texture_original_width(tex), GPU_texture_original_height(tex));
        }
      }
      CLAMP_MIN(size.x, 1);
      CLAMP_MIN(size.y, 1);

      float2 image_aspect;
      calc_image_aspect(ima, size, image_aspect);

      mat = ob->object_to_world();
      mat.x_axis() *= image_aspect.x * 0.5f * ob->empty_drawsize;
      mat.y_axis() *= image_aspect.y * 0.5f * ob->empty_drawsize;
      mat[3] += float4(mat.x_axis() * (ob->ima_ofs[0] * 2.0f + 1.0f) +
                       mat.y_axis() * (ob->ima_ofs[1] * 2.0f + 1.0f));
    }

    if (show_frame) {
      const float4 color = res.object_wire_color(ob_ref, state);
      empty_image_buf.append(ExtraInstanceData(mat, color, 1.0f), select_id);
    }

    if (show_image && tex && ((ob->color[3] > 0.0f) || !use_alpha_blend)) {
      /* Use the actual depth if we are doing depth tests to determine the distance to the
       * object. */
      char depth_mode = state.is_depth_only_drawing ? char(OB_EMPTY_IMAGE_DEPTH_DEFAULT) :
                                                      ob->empty_image_depth;
      PassMain::Sub &pass = create_subpass(state, *ob, use_alpha_blend, mat, res);
      pass.bind_texture("imgTexture", tex);
      pass.push_constant("imgPremultiplied", use_alpha_premult);
      pass.push_constant("imgAlphaBlend", use_alpha_blend);
      pass.push_constant("isCameraBackground", false);
      pass.push_constant("depthSet", depth_mode != OB_EMPTY_IMAGE_DEPTH_DEFAULT);
      pass.push_constant("ucolor", float4(ob->color));
      ResourceHandle res_handle = manager.resource_handle(mat);
      pass.draw(shapes.quad_solid.get(), res_handle, select_id.get());
    }
  }

  PassMain::Sub &create_subpass(const State &state,
                                const Object &ob,
                                const bool use_alpha_blend,
                                const float4x4 &mat,
                                Resources &res)
  {
    const bool in_front = state.use_in_front && (ob.dtx & OB_DRAW_IN_FRONT);
    if (in_front) {
      return create_subpass(state, mat, res, images_front_ps_, true);
    }
    const char depth_mode = state.is_depth_only_drawing ? char(OB_EMPTY_IMAGE_DEPTH_DEFAULT) :
                                                          ob.empty_image_depth;
    switch (depth_mode) {
      case OB_EMPTY_IMAGE_DEPTH_BACK:
        return create_subpass(state, mat, res, images_back_ps_, false);
      case OB_EMPTY_IMAGE_DEPTH_FRONT:
        return create_subpass(state, mat, res, images_front_ps_, true);
      case OB_EMPTY_IMAGE_DEPTH_DEFAULT:
      default:
        return use_alpha_blend ? create_subpass(state, mat, res, images_blend_ps_, true) :
                                 images_ps_;
    }
  }

  PassMain::Sub &create_subpass(const State &state,
                                const float4x4 &mat,
                                Resources &res,
                                PassSortable &parent,
                                bool depth_bias)
  {
    const float3 tmp = state.camera_position - mat.location();
    const float z = -math::dot(state.camera_forward, tmp);
    PassMain::Sub &sub = parent.sub("Sub", z);
    if (depth_bias) {
      sub.shader_set(res.shaders.image_plane_depth_bias.get());
      sub.push_constant("depth_bias_winmat", depth_bias_winmat_);
    }
    else {
      sub.shader_set(res.shaders.image_plane.get());
    }
    sub.bind_ubo("globalsBlock", &res.globals_buf);
    return sub;
  };

  static void calc_image_aspect(::Image *ima, const int2 &size, float2 &r_image_aspect)
  {
    /* if no image, make it a 1x1 empty square, honor scale & offset */
    const float2 ima_dim = ima ? float2(size.x, size.y) : float2(1.0f);

    /* Get the image aspect even if the buffer is invalid */
    float2 sca(1.0f);
    if (ima) {
      if (ima->aspx > ima->aspy) {
        sca.y = ima->aspy / ima->aspx;
      }
      else if (ima->aspx < ima->aspy) {
        sca.x = ima->aspx / ima->aspy;
      }
    }

    const float2 scale_inv(ima_dim.x * sca.x, ima_dim.y * sca.y);
    r_image_aspect = (scale_inv.x > scale_inv.y) ? float2(1.0f, scale_inv.y / scale_inv.x) :
                                                   float2(scale_inv.x / scale_inv.y, 1.0f);
  }
};

}  // namespace blender::draw::overlay
