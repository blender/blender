/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "overlay_next_private.hh"

namespace blender::draw::overlay {

class Background {
 private:
  PassSimple bg_ps_ = {"Background"};

 public:
  void begin_sync(Resources &res, const State &state)
  {
    DRWState pass_state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_BACKGROUND;
    float4 color_override(0.0f, 0.0f, 0.0f, 0.0f);
    int background_type;

    if (DRW_state_is_opengl_render() && !DRW_state_draw_background()) {
      background_type = BG_SOLID;
      color_override[3] = 1.0f;
    }
    else if (state.space_type == SPACE_IMAGE) {
      background_type = BG_SOLID_CHECKER;
    }
    else if (state.space_type == SPACE_NODE) {
      background_type = BG_MASK;
      pass_state = DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_MUL;
    }
    else if (!DRW_state_draw_background()) {
      background_type = BG_CHECKER;
    }
    else if (state.v3d->shading.background_type == V3D_SHADING_BACKGROUND_WORLD &&
             state.scene->world) {
      background_type = BG_SOLID;
      /* TODO(fclem): this is a scene referred linear color. we should convert
       * it to display linear here. */
      color_override = float4(UNPACK3(&state.scene->world->horr), 1.0f);
    }
    else if (state.v3d->shading.background_type == V3D_SHADING_BACKGROUND_VIEWPORT &&
             state.v3d->shading.type <= OB_SOLID)
    {
      background_type = BG_SOLID;
      color_override = float4(UNPACK3(state.v3d->shading.background_color), 1.0f);
    }
    else {
      switch (UI_GetThemeValue(TH_BACKGROUND_TYPE)) {
        case TH_BACKGROUND_GRADIENT_LINEAR:
          background_type = BG_GRADIENT;
          break;
        case TH_BACKGROUND_GRADIENT_RADIAL:
          background_type = BG_RADIAL;
          break;
        default:
        case TH_BACKGROUND_SINGLE_COLOR:
          background_type = BG_SOLID;
          break;
      }
    }

    bg_ps_.init();
    bg_ps_.state_set(pass_state);
    bg_ps_.shader_set(res.shaders.background_fill.get());
    bg_ps_.bind_ubo("globalsBlock", &res.globals_buf);
    bg_ps_.bind_texture("colorBuffer", &res.color_render_tx);
    bg_ps_.bind_texture("depthBuffer", &res.depth_tx);
    bg_ps_.push_constant("colorOverride", color_override);
    bg_ps_.push_constant("bgType", background_type);
    bg_ps_.draw_procedural(GPU_PRIM_TRIS, 1, 3);

    if (state.clipping_state != 0 && state.rv3d != nullptr && state.rv3d->clipbb != nullptr) {
      bg_ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA | DRW_STATE_CULL_BACK);
      bg_ps_.shader_set(res.shaders.background_clip_bound.get());
      bg_ps_.push_constant("ucolor", res.theme_settings.color_clipping_border);
      bg_ps_.push_constant("boundbox", &state.rv3d->clipbb->vec[0][0], 8);
      bg_ps_.draw(DRW_cache_cube_get());
    }
  }

  void draw(Resources &res, Manager &manager)
  {
    GPU_framebuffer_bind(res.overlay_color_only_fb);
    manager.submit(bg_ps_);
  }
};

}  // namespace blender::draw::overlay
