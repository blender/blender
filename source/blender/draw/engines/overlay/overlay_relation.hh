/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "BKE_constraint.h"
#include "DEG_depsgraph_query.hh"
#include "DNA_constraint_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_modifier_types.h"
#include "DNA_rigidbody_types.h"

#include "overlay_base.hh"

namespace blender::draw::overlay {

/**
 * Display object relations as dashed lines.
 * Covers parenting relationships and constraints.
 */
class Relations : Overlay {

 private:
  PassSimple ps_ = {"Relations"};

  LinePrimitiveBuf relations_buf_;
  PointPrimitiveBuf points_buf_;

 public:
  Relations(SelectionType selection_type)
      : relations_buf_(selection_type, "relations_buf_"),
        points_buf_(selection_type, "points_buf_")
  {
  }

  void begin_sync(Resources &res, const State &state) final
  {
    enabled_ = state.is_space_v3d();
    enabled_ &= (state.v3d_flag & V3D_HIDE_HELPLINES) == 0;
    enabled_ &= !res.is_selection();

    points_buf_.clear();
    relations_buf_.clear();
  }

  void object_sync(Manager & /*manager*/,
                   const ObjectRef &ob_ref,
                   Resources &res,
                   const State &state) final
  {
    if (!enabled_) {
      return;
    }

    /* Don't show object extras in set's. */
    if (is_from_dupli_or_set(ob_ref)) {
      return;
    }

    Object *ob = ob_ref.object;
    const float4 &relation_color = res.theme.colors.wire;
    const float4 &constraint_color = res.theme.colors.grid_axis_z; /* ? */

    if (ob->parent && (DRW_object_visibility_in_active_context(ob->parent) & OB_VISIBLE_SELF)) {
      const float3 &parent_pos = ob->runtime->parent_display_origin;
      /* Reverse order to have less stipple overlap. */
      relations_buf_.append(ob->object_to_world().location(), parent_pos, relation_color);
    }

    /* Drawing the hook lines. */
    for (ModifierData *md : ListBaseWrapper<ModifierData>(&ob->modifiers)) {
      if (md->type == eModifierType_Hook) {
        HookModifierData *hmd = reinterpret_cast<HookModifierData *>(md);
        const float3 center = math::transform_point(ob->object_to_world(), float3(hmd->cent));
        if (hmd->object) {
          relations_buf_.append(hmd->object->object_to_world().location(), center, relation_color);
        }
        points_buf_.append(center, relation_color);
      }
    }

    for (GpencilModifierData *md :
         ListBaseWrapper<GpencilModifierData>(ob->greasepencil_modifiers))
    {
      if (md->type == eGpencilModifierType_Hook) {
        HookGpencilModifierData *hmd = reinterpret_cast<HookGpencilModifierData *>(md);
        const float3 center = math::transform_point(ob->object_to_world(), float3(hmd->cent));
        if (hmd->object) {
          relations_buf_.append(hmd->object->object_to_world().location(), center, relation_color);
        }
        points_buf_.append(center, relation_color);
      }
    }

    if (ob->rigidbody_constraint) {
      Object *rbc_ob1 = ob->rigidbody_constraint->ob1;
      Object *rbc_ob2 = ob->rigidbody_constraint->ob2;
      if (rbc_ob1 && (DRW_object_visibility_in_active_context(rbc_ob1) & OB_VISIBLE_SELF)) {

        relations_buf_.append(rbc_ob1->object_to_world().location(),
                              ob->object_to_world().location(),
                              relation_color);
      }
      if (rbc_ob2 && (DRW_object_visibility_in_active_context(rbc_ob2) & OB_VISIBLE_SELF)) {
        relations_buf_.append(rbc_ob2->object_to_world().location(),
                              ob->object_to_world().location(),
                              relation_color);
      }
    }

    /* Drawing the constraint lines */
    if (!BLI_listbase_is_empty(&ob->constraints)) {
      Scene *scene = (Scene *)state.scene;
      bConstraintOb *cob = BKE_constraints_make_evalob(
          state.depsgraph, (Scene *)state.scene, ob, nullptr, CONSTRAINT_OBTYPE_OBJECT);

      for (bConstraint *constraint : ListBaseWrapper<bConstraint>(ob->constraints)) {
        if (ELEM(constraint->type, CONSTRAINT_TYPE_FOLLOWTRACK, CONSTRAINT_TYPE_OBJECTSOLVER)) {
          /* special case for object solver and follow track constraints because they don't fill
           * constraint targets properly (design limitation -- scene is needed for their target
           * but it can't be accessed from get_targets callback) */
          Object *camob = nullptr;

          if (constraint->type == CONSTRAINT_TYPE_FOLLOWTRACK) {
            bFollowTrackConstraint *data = (bFollowTrackConstraint *)constraint->data;
            camob = data->camera ? data->camera : scene->camera;
          }
          else if (constraint->type == CONSTRAINT_TYPE_OBJECTSOLVER) {
            bObjectSolverConstraint *data = (bObjectSolverConstraint *)constraint->data;
            camob = data->camera ? data->camera : scene->camera;
          }

          if (camob) {
            relations_buf_.append(camob->object_to_world().location(),
                                  ob->object_to_world().location(),
                                  constraint_color);
          }
        }
        else {
          const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(constraint);
          ListBase targets = {nullptr, nullptr};

          if ((constraint->ui_expand_flag & (1 << 0)) &&
              BKE_constraint_targets_get(constraint, &targets))
          {
            BKE_constraint_custom_object_space_init(cob, constraint);

            for (bConstraintTarget *target : ListBaseWrapper<bConstraintTarget>(targets)) {
              /* Calculate target's position. */
              float3 target_pos = float3(0.0f);
              bool has_target = false;
              if (target->flag & CONSTRAINT_TAR_CUSTOM_SPACE) {
                target_pos = cob->space_obj_world_matrix[3];
                has_target = true;
              }
              else if (cti->get_target_matrix &&
                       cti->get_target_matrix(state.depsgraph,
                                              constraint,
                                              cob,
                                              target,
                                              DEG_get_ctime(state.depsgraph)))
              {
                has_target = true;
                target_pos = target->matrix[3];
              }

              if (has_target) {
                /* Only draw this relationship line when there is actually a target. Otherwise it
                 * would always draw to the world origin, which is visually rather noisy and not
                 * that useful. */
                relations_buf_.append(
                    target_pos, ob->object_to_world().location(), constraint_color);
              }
            }

            BKE_constraint_targets_flush(constraint, &targets, true);
          }
        }
      }
      /* NOTE: Don't use #BKE_constraints_clear_evalob here as that will reset `ob->constinv`.
       */
      MEM_freeN(cob);
    }
  }

  void end_sync(Resources &res, const State &state) final
  {
    if (!enabled_) {
      return;
    }

    ps_.init();
    ps_.bind_ubo(OVERLAY_GLOBALS_SLOT, &res.globals_buf);
    ps_.bind_ubo(DRW_CLIPPING_UBO_SLOT, &res.clip_planes_buf);
    res.select_bind(ps_);
    {
      PassSimple::Sub &sub_pass = ps_.sub("lines");
      sub_pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH |
                             DRW_STATE_DEPTH_LESS_EQUAL,
                         state.clipping_plane_count);
      sub_pass.shader_set(res.shaders->extra_wire.get());
      relations_buf_.end_sync(sub_pass);
    }
    {
      PassSimple::Sub &sub_pass = ps_.sub("loose_points");
      sub_pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH |
                             DRW_STATE_DEPTH_LESS_EQUAL,
                         state.clipping_plane_count);
      sub_pass.shader_set(res.shaders->extra_loose_points.get());
      points_buf_.end_sync(sub_pass);
    }
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
