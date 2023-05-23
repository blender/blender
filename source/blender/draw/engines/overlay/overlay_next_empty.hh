/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "overlay_next_private.hh"

namespace blender::draw::overlay {

class Empties {
  using EmptyInstanceBuf = ShapeInstanceBuf<ExtraInstanceData>;

 private:
  const SelectionType selection_type_;

  PassSimple empty_ps_ = {"Empties"};
  PassSimple empty_in_front_ps_ = {"Empties_In_front"};

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
  } call_buffers_[2] = {{selection_type_}, {selection_type_}};

 public:
  Empties(const SelectionType selection_type) : selection_type_(selection_type){};

  void begin_sync()
  {
    for (int i = 0; i < 2; i++) {
      call_buffers_[i].plain_axes_buf.clear();
      call_buffers_[i].single_arrow_buf.clear();
      call_buffers_[i].cube_buf.clear();
      call_buffers_[i].circle_buf.clear();
      call_buffers_[i].sphere_buf.clear();
      call_buffers_[i].cone_buf.clear();
      call_buffers_[i].arrows_buf.clear();
      call_buffers_[i].image_buf.clear();
    }
  }

  void object_sync(const ObjectRef &ob_ref, Resources &res, const State &state)
  {
    CallBuffers &call_bufs = call_buffers_[int((ob_ref.object->dtx & OB_DRAW_IN_FRONT) != 0)];

    float4 color = res.object_wire_color(ob_ref, state);
    ExtraInstanceData data(
        float4x4(ob_ref.object->object_to_world), color, ob_ref.object->empty_drawsize);

    const select::ID select_id = res.select_id(ob_ref);

    switch (ob_ref.object->empty_drawtype) {
      case OB_PLAINAXES:
        call_bufs.plain_axes_buf.append(data, select_id);
        break;
      case OB_SINGLE_ARROW:
        call_bufs.single_arrow_buf.append(data, select_id);
        break;
      case OB_CUBE:
        call_bufs.cube_buf.append(data, select_id);
        break;
      case OB_CIRCLE:
        call_bufs.circle_buf.append(data, select_id);
        break;
      case OB_EMPTY_SPHERE:
        call_bufs.sphere_buf.append(data, select_id);
        break;
      case OB_EMPTY_CONE:
        call_bufs.cone_buf.append(data, select_id);
        break;
      case OB_ARROWS:
        call_bufs.arrows_buf.append(data, select_id);
        break;
      case OB_EMPTY_IMAGE:
        /* This only show the frame. See OVERLAY_image_empty_cache_populate() for the image. */
        call_bufs.image_buf.append(data, select_id);
        break;
    }
  }

  void end_sync(Resources &res, ShapeCache &shapes, const State &state)
  {
    auto init_pass = [&](PassSimple &pass, CallBuffers &call_bufs) {
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL |
                     state.clipping_state);
      pass.shader_set(res.shaders.extra_shape.get());
      pass.bind_ubo("globalsBlock", &res.globals_buf);
      res.select_bind(pass);

      call_bufs.plain_axes_buf.end_sync(pass, shapes.plain_axes.get());
      call_bufs.single_arrow_buf.end_sync(pass, shapes.single_arrow.get());
      call_bufs.cube_buf.end_sync(pass, shapes.cube.get());
      call_bufs.circle_buf.end_sync(pass, shapes.circle.get());
      call_bufs.sphere_buf.end_sync(pass, shapes.empty_sphere.get());
      call_bufs.cone_buf.end_sync(pass, shapes.empty_cone.get());
      call_bufs.arrows_buf.end_sync(pass, shapes.arrows.get());
      call_bufs.image_buf.end_sync(pass, shapes.quad_wire.get());
    };

    init_pass(empty_ps_, call_buffers_[0]);
    init_pass(empty_in_front_ps_, call_buffers_[1]);
  }

  void draw(Resources &res, Manager &manager, View &view)
  {
    GPU_framebuffer_bind(res.overlay_line_fb);
    manager.submit(empty_ps_, view);
  }

  void draw_in_front(Resources &res, Manager &manager, View &view)
  {
    GPU_framebuffer_bind(res.overlay_line_in_front_fb);
    manager.submit(empty_in_front_ps_, view);
  }
};

}  // namespace blender::draw::overlay
