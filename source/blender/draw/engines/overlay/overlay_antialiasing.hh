/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 * Overlay anti-aliasing:
 *
 * Most of the overlays are wires which causes a lot of flickering in motions
 * due to aliasing problems.
 *
 * Our goal is to have a technique that works with single sample per pixel
 * to avoid extra cost of managing MSAA or additional texture buffers and jitters.
 *
 * To solve this we use a simple and effective post-process AA. The technique
 * goes like this:
 *
 * - During wireframe rendering, we output the line color, the line direction
 *   and the distance from the line for the pixel center.
 *
 * - Then, in a post process pass, for each pixels we gather all lines in a search area
 *   that could cover (even partially) the center pixel.
 *   We compute the coverage of each line and do a sorted alpha compositing of them.
 *
 * This technique has one major shortcoming compared to MSAA:
 * - It handles (initial) partial visibility poorly (because of single sample). This makes
 *   overlapping / crossing wires a bit too thin at their intersection.
 *   Wireframe meshes overlaid over solid meshes can have half of the edge missing due to
 *   z-fighting (this has workaround).
 *   Another manifestation of this, is flickering of really dense wireframe if using small
 *   line thickness (also has workaround).
 *
 * The pros of this approach are many:
 *  - Works without geometry shader.
 *  - Can inflate line thickness.
 *  - Coverage is very close to perfect and can even be filtered (Blackman-Harris, gaussian).
 *  - Wires can "bleed" / overlap non-line objects since the filter is in screen-space.
 *  - Only uses one additional lightweight full-screen buffer (compared to MSAA/SMAA).
 *  - No convergence time (compared to TAA).
 */

#pragma once

#include "overlay_base.hh"

#include "DNA_userdef_types.h"

namespace blender::draw::overlay {

class AntiAliasing : Overlay {
 private:
  PassSimple anti_aliasing_ps_ = {"AntiAliasing"};

  gpu::FrameBuffer *framebuffer_ref_ = nullptr;

 public:
  void begin_sync(Resources &res, const State & /*state*/) final
  {
    if (res.is_selection()) {
      anti_aliasing_ps_.init();
      return;
    }

    const bool do_smooth_lines = (U.gpu_flag & USER_GPU_FLAG_OVERLAY_SMOOTH_WIRE) != 0;

    {
      PassSimple &pass = anti_aliasing_ps_;
      pass.init();
      pass.framebuffer_set(&framebuffer_ref_);
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA_PREMUL);
      pass.shader_set(res.shaders->anti_aliasing.get());
      pass.bind_ubo(OVERLAY_GLOBALS_SLOT, &res.globals_buf);
      pass.bind_ubo(DRW_CLIPPING_UBO_SLOT, &res.clip_planes_buf);
      pass.bind_texture("depth_tx", &res.depth_tx);
      pass.bind_texture("color_tx", &res.overlay_tx);
      pass.bind_texture("line_tx", &res.line_tx);
      pass.push_constant("do_smooth_lines", do_smooth_lines);
      pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
    }
  }

  void draw_output(Framebuffer &framebuffer, Manager &manager, View & /*view*/) final
  {
    framebuffer_ref_ = framebuffer;
    manager.submit(anti_aliasing_ps_);
  }
};

}  // namespace blender::draw::overlay
