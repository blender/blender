/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "BKE_mball.hh"

#include "ED_mball.hh"

#include "overlay_base.hh"

namespace blender::draw::overlay {

/**
 * Draw meta-balls radius overlays.
 */
class Metaballs : Overlay {
  using SphereOutlineInstanceBuf = ShapeInstanceBuf<BoneInstanceData>;

 private:
  const SelectionType selection_type_;

  PassSimple ps_ = {"MetaBalls"};

  SphereOutlineInstanceBuf circle_buf_ = {selection_type_, "metaball_data_buf"};

 public:
  Metaballs(const SelectionType selection_type) : selection_type_(selection_type) {};

  void begin_sync(Resources & /*res*/, const State & /*state*/) final
  {
    circle_buf_.clear();
  }

  void edit_object_sync(Manager & /*manager*/,
                        const ObjectRef &ob_ref,
                        Resources &res,
                        const State & /*state*/) final
  {
    const Object *ob = ob_ref.object;
    const MetaBall &mb = DRW_object_get_data_for_drawing<MetaBall>(*ob);

    const float *color;
    const float *col_radius = res.theme.colors.mball_radius;
    const float *col_radius_select = res.theme.colors.mball_radius_select;
    const float *col_stiffness = res.theme.colors.mball_stiffness;
    const float *col_stiffness_select = res.theme.colors.mball_stiffness_select;

    int elem_num = 0;
    LISTBASE_FOREACH (MetaElem *, ml, mb.editelems) {
      const bool is_selected = (ml->flag & SELECT) != 0;
      const bool is_scale_radius = (ml->flag & MB_SCALE_RAD) != 0;
      blender::float2 radius_stiffness = BKE_mball_element_display_radius_calc_with_stiffness(ml);
      const float3 position = float3(&ml->x);

      const select::ID radius_id = res.select_id(ob_ref, MBALLSEL_RADIUS | elem_num);
      color = (is_selected && is_scale_radius) ? col_radius_select : col_radius;
      circle_buf_.append({ob->object_to_world(), position, radius_stiffness[0], color}, radius_id);

      const select::ID stiff_id = res.select_id(ob_ref, MBALLSEL_STIFF | elem_num);
      color = (is_selected && !is_scale_radius) ? col_stiffness_select : col_stiffness;
      circle_buf_.append({ob->object_to_world(), position, radius_stiffness[1], color}, stiff_id);
      elem_num += 1 << 16;
    }
  }

  void object_sync(Manager & /*manager*/,
                   const ObjectRef &ob_ref,
                   Resources &res,
                   const State &state) final
  {
    const Object *ob = ob_ref.object;
    const MetaBall *mb = &DRW_object_get_data_for_drawing<MetaBall>(*ob);

    const float4 &color = res.object_wire_color(ob_ref, state);
    const select::ID select_id = res.select_id(ob_ref);

    LISTBASE_FOREACH (MetaElem *, ml, &mb->elems) {
      const float3 position = float3(&ml->x);
      /* Draw radius only. */
      const float radius = BKE_mball_element_display_radius_calc(ml);
      circle_buf_.append({ob->object_to_world(), position, radius, color}, select_id);
    }
  }

  void end_sync(Resources &res, const State &state) final
  {
    ps_.init();
    ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS_EQUAL,
                  state.clipping_plane_count);
    /* NOTE: Use armature sphere outline shader to have perspective correct outline instead of
     * just a circle facing the camera. */
    ps_.shader_set(res.shaders->armature_sphere_outline.get());
    ps_.bind_ubo(OVERLAY_GLOBALS_SLOT, &res.globals_buf);
    ps_.bind_ubo(DRW_CLIPPING_UBO_SLOT, &res.clip_planes_buf);
    ps_.push_constant("alpha", 1.0f);
    res.select_bind(ps_);

    circle_buf_.end_sync(ps_, res.shapes.metaball_wire_circle.get());
  }

  void draw_line(Framebuffer &framebuffer, Manager &manager, View &view) final
  {
    GPU_framebuffer_bind(framebuffer);
    manager.submit(ps_, view);
  }
};

}  // namespace blender::draw::overlay
