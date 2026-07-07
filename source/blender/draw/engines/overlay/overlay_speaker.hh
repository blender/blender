/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "DNA_speaker_types.h"

#include "overlay_base.hh"

namespace blender::draw::overlay {

/**
 * Display speaker objects.
 */
class Speakers : Overlay {
  using SpeakerInstanceBuf = ShapeInstanceBuf<ExtraInstanceData>;

 private:
  const SelectionType selection_type_;

  PassSimple ps_ = {"Speakers"};

  SpeakerInstanceBuf speaker_buf_ = {selection_type_, "speaker_data_buf"};

 public:
  Speakers(const SelectionType selection_type) : selection_type_(selection_type) {};

  void begin_sync(Resources & /*res*/, const State &state) final
  {
    enabled_ = state.is_space_v3d() && state.show_extras();

    if (!enabled_) {
      return;
    }

    speaker_buf_.clear();
  }

  void object_sync(Manager & /*manager*/,
                   const ObjectRef &ob_ref,
                   Resources &res,
                   const State &state) final
  {
    if (!enabled_) {
      return;
    }

    const float4 color = res.object_wire_color(ob_ref, state);
    const select::ID select_id = res.select_id(ob_ref);

    speaker_buf_.append({ob_ref.object->object_to_world(), color, 1.0f}, select_id);
  }

  void end_sync(Resources &res, const State &state) final
  {
    if (!enabled_) {
      return;
    }

    ps_.init();
    ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL,
                  state.clipping_plane_count);
    ps_.shader_set(res.shaders->extra_shape.get());
    ps_.bind_ubo(OVERLAY_GLOBALS_SLOT, &res.globals_buf);
    ps_.bind_ubo(DRW_CLIPPING_UBO_SLOT, &res.clip_planes_buf);
    res.select_bind(ps_);

    speaker_buf_.end_sync(ps_, res.shapes.speaker.get());
  }

  void draw_line(Framebuffer &framebuffer, Manager &manager, View &view) final
  {
    if (!enabled_) {
      return;
    }

    GPU_framebuffer_bind(framebuffer);
    manager.submit(ps_, view);
  }
};

}  // namespace blender::draw::overlay
