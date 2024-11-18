/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "BKE_modifier.hh"
#include "BKE_paint.hh"

#include "overlay_next_armature.hh"
#include "overlay_next_private.hh"

namespace blender::draw::overlay {
class Fade {
 private:
  const SelectionType selection_type_;

  PassMain ps_ = {"FadeGeometry"};

  PassMain::Sub *mesh_fade_geometry_ps_;
  /* Passes for Pose Fade Geometry. */
  PassMain::Sub *armature_fade_geometry_active_ps_;
  PassMain::Sub *armature_fade_geometry_other_ps_;

  bool enabled_ = false;

 public:
  Fade(const SelectionType selection_type_) : selection_type_(selection_type_) {}

  void begin_sync(Resources &res, const State &state)
  {
    const bool do_edit_mesh_fade_geom = !state.xray_enabled &&
                                        (state.overlay.flag & V3D_OVERLAY_FADE_INACTIVE);
    enabled_ = state.space_type == SPACE_VIEW3D &&
               (do_edit_mesh_fade_geom || state.do_pose_fade_geom) &&
               (selection_type_ == SelectionType::DISABLED);

    if (!enabled_) {
      /* Not used. But release the data. */
      ps_.init();
      return;
    }

    ps_.init();
    ps_.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS_EQUAL | DRW_STATE_BLEND_ALPHA,
                  state.clipping_plane_count);
    ps_.shader_set(res.shaders.uniform_color.get());
    {
      PassMain::Sub &sub = ps_.sub("edit_mesh.fade");
      float4 color = res.background_color_get(state);
      color[3] = state.overlay.fade_alpha;
      if (state.v3d->shading.background_type == V3D_SHADING_BACKGROUND_THEME) {
        srgb_to_linearrgb_v4(color, color);
      }
      sub.push_constant("ucolor", color);
      mesh_fade_geometry_ps_ = &sub;
    }

    /* Fade Geometry. */
    if (state.do_pose_fade_geom) {
      const float alpha = state.overlay.xray_alpha_bone;
      float4 color = {0.0f, 0.0f, 0.0f, alpha};
      {
        auto &sub = ps_.sub("fade_geometry.active");
        sub.push_constant("ucolor", color);
        armature_fade_geometry_active_ps_ = &sub;
      }
      {
        color[3] = powf(alpha, 4);
        auto &sub = ps_.sub("fade_geometry");
        sub.push_constant("ucolor", color);
        armature_fade_geometry_other_ps_ = &sub;
      }
    }
  }

  void object_sync(Manager &manager, const ObjectRef &ob_ref, const State &state)
  {
    if (!enabled_) {
      return;
    }
    const Object *ob = ob_ref.object;
    const bool renderable = DRW_object_is_renderable(ob);
    const bool draw_surface = (ob->dt >= OB_WIRE) && (renderable || (ob->dt == OB_WIRE));
    const bool draw_fade = draw_surface && overlay_should_fade_object(ob, state.object_active);

    const bool draw_bone_selection = (ob_ref.object->type == OB_MESH) && state.do_pose_fade_geom;

    auto fade_sync =
        [](Manager &manager, const ObjectRef &ob_ref, const State &state, PassMain::Sub &sub) {
          const bool use_sculpt_pbvh = BKE_sculptsession_use_pbvh_draw(ob_ref.object,
                                                                       state.rv3d) &&
                                       !DRW_state_is_image_render();

          if (use_sculpt_pbvh) {
            ResourceHandle handle = manager.resource_handle_for_sculpt(ob_ref);

            for (SculptBatch &batch : sculpt_batches_get(ob_ref.object, SCULPT_BATCH_DEFAULT)) {
              sub.draw(batch.batch, handle);
            }
          }
          else {
            blender::gpu::Batch *geom = DRW_cache_object_surface_get((Object *)ob_ref.object);
            if (geom) {
              sub.draw(geom, manager.unique_handle(ob_ref));
            }
          }
        };

    if (draw_bone_selection) {
      fade_sync(manager,
                ob_ref,
                state,
                is_driven_by_active_armature(ob_ref.object, state) ?
                    *armature_fade_geometry_active_ps_ :
                    *armature_fade_geometry_other_ps_);
    }
    else if (draw_fade) {
      fade_sync(manager, ob_ref, state, *mesh_fade_geometry_ps_);
    }
  }

  void draw(Framebuffer &framebuffer, Manager &manager, View &view)
  {
    if (!enabled_) {
      return;
    }

    GPU_framebuffer_bind(framebuffer);
    manager.submit(ps_, view);
  }

 private:
  static bool overlay_should_fade_object(const Object *ob, const Object *active_object)
  {
    if (!active_object || !ob) {
      return false;
    }

    if (ELEM(active_object->mode, OB_MODE_OBJECT, OB_MODE_POSE)) {
      return false;
    }

    if ((active_object->mode & ob->mode) != 0) {
      return false;
    }

    return true;
  }

  static bool is_driven_by_active_armature(Object *ob, const State &state)
  {
    Object *ob_arm = BKE_modifiers_is_deformed_by_armature(ob);
    if (ob_arm) {
      return Armatures::is_pose_mode(ob_arm, state);
    }

    Object *ob_mesh_deform = BKE_modifiers_is_deformed_by_meshdeform(ob);
    if (ob_mesh_deform) {
      /* Recursive. */
      return is_driven_by_active_armature(ob_mesh_deform, state);
    }

    return false;
  }
};
}  // namespace blender::draw::overlay
