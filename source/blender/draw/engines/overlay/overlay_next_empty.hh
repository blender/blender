/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "overlay_next_private.hh"

namespace blender::draw::overlay {

class Empties {
  friend class Cameras;
  using EmptyInstanceBuf = ShapeInstanceBuf<ExtraInstanceData>;

 private:
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

 public:
  Empties(const SelectionType selection_type) : call_buffers_{selection_type} {};

  void begin_sync()
  {
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

  void object_sync(const ObjectRef &ob_ref, Resources &res, const State &state)
  {
    const float4 color = res.object_wire_color(ob_ref, state);
    const select::ID select_id = res.select_id(ob_ref);
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
      case OB_EMPTY_IMAGE:
        /* This only show the frame. See OVERLAY_image_empty_cache_populate() for the image. */
        call_buffers.image_buf.append(data, select_id);
        break;
    }
  }

  void end_sync(Resources &res, ShapeCache &shapes, const State &state)
  {
    ps_.init();
    res.select_bind(ps_);
    end_sync(res, shapes, state, ps_, call_buffers_);
  }

  static void end_sync(Resources &res,
                       ShapeCache &shapes,
                       const State &state,
                       PassSimple::Sub &ps,
                       CallBuffers &call_buffers)
  {
    ps.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL |
                 state.clipping_state);
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

  void draw(Framebuffer &framebuffer, Manager &manager, View &view)
  {
    GPU_framebuffer_bind(framebuffer);
    manager.submit(ps_, view);
  }
};

}  // namespace blender::draw::overlay
