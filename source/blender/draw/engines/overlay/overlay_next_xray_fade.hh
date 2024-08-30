/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 * Overlay Xray Fade:
 *
 * Full-screen pass that dim overlays that are behind scene geometry.
 * This allows to have a nice transition between opaque (or 100% xray) and wire-frame only mode.
 * This is only available if Xray mode is enabled or in wire-frame mode.
 */

#pragma once

#include "overlay_next_private.hh"

namespace blender::draw::overlay {

class XrayFade {
 private:
  PassSimple xray_fade_ps_ = {"XrayFade"};

  bool enabled_ = false;

 public:
  void begin_sync(Resources &res, State &state)
  {
    enabled_ = state.xray_enabled && (state.xray_opacity > 0.0f) &&
               (res.selection_type == SelectionType::DISABLED);

    if (!enabled_) {
      return;
    }

    {
      PassSimple &pass = xray_fade_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_MUL);
      pass.framebuffer_set(&res.overlay_color_only_fb);
      pass.shader_set(res.shaders.xray_fade.get());
      /* TODO(fclem): Confusing. The meaning of xray depth texture changed between legacy engine
       * and overlay next. To be renamed after shaders are not shared anymore. */
      pass.bind_texture("depthTex", &res.xray_depth_tx);
      pass.bind_texture("xrayDepthTex", &res.depth_tx);
      pass.push_constant("opacity", 1.0f - state.xray_opacity);
      pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
    }
  }

  void draw(Manager &manager)
  {
    if (!enabled_) {
      return;
    }

    manager.submit(xray_fade_ps_);
  }
};

}  // namespace blender::draw::overlay
