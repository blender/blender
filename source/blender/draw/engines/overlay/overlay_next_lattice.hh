/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "draw_cache_impl.hh"
#include "draw_common_c.hh"
#include "overlay_next_private.hh"

#include "ED_lattice.hh"

namespace blender::draw::overlay {

class Lattices {
 private:
  PassMain ps_ = {"Lattice"};
  PassMain in_front_ps_ = {"Lattice_in_front"};

  PassMain::Sub *lattice_ps_[2];
  PassMain::Sub *edit_lattice_wire_ps_[2];
  PassMain::Sub *edit_lattice_point_ps_[2];

 public:
  void begin_sync(Resources &res, const State &state)
  {
    const DRWState pass_state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH |
                                DRW_STATE_DEPTH_LESS_EQUAL | state.clipping_state;
    auto init_pass = [&](PassMain &pass, int in_front) {
      auto create_sub_pass = [&](const char *name, GPUShader *shader, bool add_weight_tex) {
        PassMain::Sub &sub_pass = pass.sub(name);
        sub_pass.state_set(pass_state);
        sub_pass.shader_set(shader);
        sub_pass.bind_ubo("globalsBlock", &res.globals_buf);
        if (add_weight_tex) {
          sub_pass.bind_texture("weightTex", &res.weight_ramp_tx);
        }
        return &sub_pass;
      };

      pass.init();
      edit_lattice_wire_ps_[in_front] = create_sub_pass(
          "edit_lattice_wire", res.shaders.lattice_wire.get(), true);
      edit_lattice_point_ps_[in_front] = create_sub_pass(
          "edit_lattice_points", res.shaders.lattice_points.get(), false);
      lattice_ps_[in_front] = create_sub_pass(
          "lattice", res.shaders.extra_wire_object.get(), false);
      res.select_bind(pass);
    };

    init_pass(ps_, 0);
    init_pass(in_front_ps_, 1);
  }

  void edit_object_sync(Manager &manager, const ObjectRef &ob_ref, Resources &res)
  {
    const int in_front = (ob_ref.object->dtx & OB_DRAW_IN_FRONT) != 0 ? 1 : 0;
    {
      gpu::Batch *geom = DRW_cache_lattice_wire_get(ob_ref.object, true);
      if (geom) {
        ResourceHandle res_handle = manager.resource_handle(ob_ref);
        edit_lattice_wire_ps_[in_front]->draw(geom, res_handle, res.select_id(ob_ref).get());
      }
    }
    {
      gpu::Batch *geom = DRW_cache_lattice_vert_overlay_get(ob_ref.object);
      if (geom) {
        ResourceHandle res_handle = manager.resource_handle(ob_ref);
        edit_lattice_point_ps_[in_front]->draw(geom, res_handle, res.select_id(ob_ref).get());
      }
    }
  }

  void object_sync(Manager &manager, const ObjectRef &ob_ref, Resources &res, const State &state)
  {
    gpu::Batch *geom = DRW_cache_lattice_wire_get(ob_ref.object, false);
    if (geom) {
      const float4 &color = res.object_wire_color(ob_ref, state);
      float4x4 draw_mat(ob_ref.object->object_to_world().ptr());
      for (int i : IndexRange(3)) {
        draw_mat[i][3] = color[i];
      }
      draw_mat[3][3] = 0.0f /* No stipples. */;
      ResourceHandle res_handle = manager.resource_handle(ob_ref, &draw_mat, nullptr, nullptr);
      const int in_front = (ob_ref.object->dtx & OB_DRAW_IN_FRONT) != 0 ? 1 : 0;
      lattice_ps_[in_front]->draw(geom, res_handle, res.select_id(ob_ref).get());
    }
  }

  void draw(Resources &res, Manager &manager, View &view)
  {
    GPU_framebuffer_bind(res.overlay_line_fb);
    manager.submit(ps_, view);
  }

  void draw_in_front(Resources &res, Manager &manager, View &view)
  {
    GPU_framebuffer_bind(res.overlay_line_in_front_fb);
    manager.submit(in_front_ps_, view);
  }
};
}  // namespace blender::draw::overlay
