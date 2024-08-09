/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup overlay
 */

#pragma once

#include "BKE_constraint.h"

#include "DNA_constraint_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_modifier_types.h"

#include "overlay_next_private.hh"

namespace blender::draw::overlay {

class Relations {

 private:
  PassSimple ps_ = {"Relations"};

  LinePrimitiveBuf relations_buf_ = {SelectionType::DISABLED, "relations_buf_"};
  PointPrimitiveBuf points_buf_ = {SelectionType::DISABLED, "points_buf_"};

 public:
  void begin_sync()
  {
    points_buf_.clear();
    relations_buf_.clear();
  }

  void object_sync(const ObjectRef &ob_ref, Resources &res, const State &state)
  {
    Object *ob = ob_ref.object;
    const float4 &relation_color = res.theme_settings.color_wire;
    const float4 &constraint_color = res.theme_settings.color_grid_axis_z; /* ? */
    const select::ID select_id = res.select_id(ob_ref);

    if (ob->parent && (DRW_object_visibility_in_active_context(ob->parent) & OB_VISIBLE_SELF)) {
      const float3 &parent_pos = ob->runtime->parent_display_origin;
      relations_buf_.append(
          parent_pos, ob->object_to_world().location(), relation_color, select_id);
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
              if (target->flag & CONSTRAINT_TAR_CUSTOM_SPACE) {
                target_pos = cob->space_obj_world_matrix[3];
              }
              else if (cti->get_target_matrix) {
                cti->get_target_matrix(
                    state.depsgraph, constraint, cob, target, DEG_get_ctime(state.depsgraph));
                target_pos = target->matrix[3];
              }
              relations_buf_.append(
                  target_pos, ob->object_to_world().location(), constraint_color);
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

  void end_sync(Resources &res, const State &state)
  {
    ps_.init();
    {
      PassSimple::Sub &sub_pass = ps_.sub("lines");
      sub_pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH |
                         DRW_STATE_DEPTH_LESS_EQUAL | state.clipping_state);
      sub_pass.shader_set(res.shaders.extra_wire.get());
      sub_pass.bind_ubo("globalsBlock", &res.globals_buf);
      relations_buf_.end_sync(sub_pass);
    }
    {
      PassSimple::Sub &sub_pass = ps_.sub("loose_points");
      sub_pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH |
                         DRW_STATE_DEPTH_LESS_EQUAL | state.clipping_state);
      sub_pass.shader_set(res.shaders.extra_loose_points.get());
      sub_pass.bind_ubo("globalsBlock", &res.globals_buf);
      points_buf_.end_sync(sub_pass);
    }
  }

  void draw(Framebuffer &framebuffer, Manager &manager, View &view)
  {
    GPU_framebuffer_bind(framebuffer);
    manager.submit(ps_, view);
  }
};

}  // namespace blender::draw::overlay
