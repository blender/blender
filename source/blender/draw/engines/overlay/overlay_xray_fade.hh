/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#include "overlay_base.hh"

namespace blender::draw::overlay {

/**
 * Fade overlays that are behind scene geometry.
 * This allows to have a nice transition between opaque (or 100% X-ray) and wire-frame only mode.
 * This is only available if X-ray mode is enabled or in wire-frame mode.
 */
class XrayFade : Overlay {
 private:
  PassSimple xray_fade_ps_ = {"XrayFade"};

 public:
  void begin_sync(Resources &res, const State &state) final
  {
    enabled_ = state.xray_enabled && (state.xray_opacity > 0.0f) && !res.is_selection();

    if (!enabled_) {
      return;
    }

    {
      PassSimple &pass = xray_fade_ps_;
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_MUL);
      pass.framebuffer_set(&res.overlay_color_only_fb);
      pass.shader_set(res.shaders->xray_fade.get());
      /* TODO(fclem): Confusing. The meaning of xray depth texture changed between legacy engine
       * and overlay next. To be renamed after shaders are not shared anymore. */
      pass.bind_texture("depth_tx", &res.xray_depth_tx);
      pass.bind_texture("depth_txInfront", &res.xray_depth_in_front_tx);
      pass.bind_texture("xray_depth_tx", &res.depth_tx);
      pass.bind_texture("xray_depth_txInfront", &res.depth_in_front_tx);
      pass.push_constant("opacity", 1.0f - state.xray_opacity);
      pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
    }
  }

  void draw_color_only(Framebuffer &framebuffer, Manager &manager, View & /*view*/) final
  {
    if (!enabled_) {
      return;
    }

    GPU_framebuffer_bind(framebuffer);
    manager.submit(xray_fade_ps_);
  }
};

}  // namespace blender::draw::overlay
