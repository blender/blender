/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "overlay_next_private.hh"
#include "overlay_shader_shared.h"

#include "ED_mball.hh"

namespace blender::draw::overlay {

class Metaballs {
  using SphereOutlineInstanceBuf = ShapeInstanceBuf<BoneInstanceData>;

 private:
  const SelectionType selection_type_;

  PassSimple metaball_ps_ = {"MetaBalls"};
  PassSimple metaball_in_front_ps_ = {"MetaBalls_In_front"};

  SphereOutlineInstanceBuf circle_buf_ = {selection_type_, "metaball_data_buf"};
  SphereOutlineInstanceBuf circle_in_front_buf_ = {selection_type_, "metaball_data_buf"};

 public:
  Metaballs(const SelectionType selection_type) : selection_type_(selection_type){};

  void begin_sync()
  {
    circle_buf_.clear();
    circle_in_front_buf_.clear();
  }

  void edit_object_sync(const ObjectRef &ob_ref, Resources &res)
  {
    SphereOutlineInstanceBuf &circle_buf = (ob_ref.object->dtx & OB_DRAW_IN_FRONT) != 0 ?
                                               circle_in_front_buf_ :
                                               circle_buf_;
    MetaBall *mb = static_cast<MetaBall *>(ob_ref.object->data);

    const float *color;
    const float *col_radius = res.theme_settings.color_mball_radius;
    const float *col_radius_select = res.theme_settings.color_mball_radius_select;
    const float *col_stiffness = res.theme_settings.color_mball_stiffness;
    const float *col_stiffness_select = res.theme_settings.color_mball_stiffness_select;

    LISTBASE_FOREACH (MetaElem *, ml, mb->editelems) {
      const bool is_selected = (ml->flag & SELECT) != 0;
      const bool is_scale_radius = (ml->flag & MB_SCALE_RAD) != 0;
      float stiffness_radius = ml->rad * atanf(ml->s) / float(M_PI_2);

      const select::ID radius_id = res.select_id(ob_ref, MBALLSEL_RADIUS);
      color = (is_selected && is_scale_radius) ? col_radius_select : col_radius;
      circle_buf.append({ob_ref.object, &ml->x, ml->rad, color}, radius_id);

      const select::ID stiff_id = res.select_id(ob_ref, MBALLSEL_STIFF);
      color = (is_selected && !is_scale_radius) ? col_stiffness_select : col_stiffness;
      circle_buf.append({ob_ref.object, &ml->x, stiffness_radius, color}, stiff_id);
    }
  }

  void object_sync(const ObjectRef &ob_ref, Resources &res, const State &state)
  {
    SphereOutlineInstanceBuf &circle_buf = (ob_ref.object->dtx & OB_DRAW_IN_FRONT) != 0 ?
                                               circle_in_front_buf_ :
                                               circle_buf_;
    MetaBall *mb = static_cast<MetaBall *>(ob_ref.object->data);

    const float4 &color = res.object_wire_color(ob_ref, state);
    const select::ID select_id = res.select_id(ob_ref);

    LISTBASE_FOREACH (MetaElem *, ml, &mb->elems) {
      /* Draw radius only. */
      circle_buf.append({ob_ref.object, &ml->x, ml->rad, color}, select_id);
    }
  }

  void end_sync(Resources &res, ShapeCache &shapes, const State &state)
  {
    auto init_pass = [&](PassSimple &pass, SphereOutlineInstanceBuf &call_buf) {
      pass.init();
      pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL |
                     state.clipping_state);
      /* NOTE: Use armature sphere outline shader to have perspective correct outline instead of
       * just a circle facing the camera. */
      pass.shader_set(res.shaders.armature_sphere_outline.get());
      pass.bind_ubo("globalsBlock", &res.globals_buf);
      res.select_bind(pass);

      call_buf.end_sync(pass, shapes.metaball_wire_circle.get());
    };
    init_pass(metaball_ps_, circle_buf_);
    init_pass(metaball_in_front_ps_, circle_in_front_buf_);
  }

  void draw(Resources &res, Manager &manager, View &view)
  {
    GPU_framebuffer_bind(res.overlay_line_fb);
    manager.submit(metaball_ps_, view);
  }

  void draw_in_front(Resources &res, Manager &manager, View &view)
  {
    GPU_framebuffer_bind(res.overlay_line_in_front_fb);
    manager.submit(metaball_in_front_ps_, view);
  }
};

}  // namespace blender::draw::overlay
