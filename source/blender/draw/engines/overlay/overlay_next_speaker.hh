/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "overlay_next_private.hh"

#include "DNA_speaker_types.h"

namespace blender::draw::overlay {

class Speakers {
  using SpeakerInstanceBuf = ShapeInstanceBuf<ExtraInstanceData>;

 private:
  const SelectionType selection_type_;

  PassSimple speaker_ps_ = {"Speakers"};
  PassSimple speaker_in_front_ps_ = {"Speakers_In_front"};

  SpeakerInstanceBuf speaker_buf_ = {selection_type_, "speaker_data_buf"};
  SpeakerInstanceBuf speaker_in_front_buf_ = {selection_type_, "speaker_data_buf"};

 public:
  Speakers(const SelectionType selection_type) : selection_type_(selection_type){};

  void begin_sync()
  {
    speaker_buf_.clear();
    speaker_in_front_buf_.clear();
  }

  void object_sync(const ObjectRef &ob_ref, Resources &res, const State &state)
  {
    SpeakerInstanceBuf &speaker_buf = (ob_ref.object->dtx & OB_DRAW_IN_FRONT) != 0 ?
                                          speaker_in_front_buf_ :
                                          speaker_buf_;

    float4 color = res.object_wire_color(ob_ref, state);
    const select::ID select_id = res.select_id(ob_ref);

    speaker_buf.append({ob_ref.object->object_to_world(), color, 1.0f}, select_id);
  }

  void end_sync(Resources &res, ShapeCache &shapes, const State &state)
  {
    auto init_pass = [&](PassSimple &pass, SpeakerInstanceBuf &call_buf) {
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL |
                     state.clipping_state);
      pass.shader_set(res.shaders.extra_shape.get());
      pass.bind_ubo("globalsBlock", &res.globals_buf);
      res.select_bind(pass);

      call_buf.end_sync(pass, shapes.speaker.get());
    };
    init_pass(speaker_ps_, speaker_buf_);
    init_pass(speaker_in_front_ps_, speaker_in_front_buf_);
  }

  void draw(Resources &res, Manager &manager, View &view)
  {
    GPU_framebuffer_bind(res.overlay_line_fb);
    manager.submit(speaker_ps_, view);
  }

  void draw_in_front(Resources &res, Manager &manager, View &view)
  {
    GPU_framebuffer_bind(res.overlay_line_in_front_fb);
    manager.submit(speaker_in_front_ps_, view);
  }
};

}  // namespace blender::draw::overlay
