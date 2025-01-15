/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief Functions to work with animation poses.
 */

#pragma once

#include "ANIM_action.hh"

struct AnimationEvalContext;
struct Object;
struct bAction;

namespace blender::animrig {

/**
 * Evaluate the action and apply it to the pose. If any pose bones are selected, only FCurves that
 * relate to those bones are evaluated.
 */
void pose_apply_action_selected_bones(Object *ob,
                                      bAction *action,
                                      slot_handle_t slot_handle,
                                      const AnimationEvalContext *anim_eval_context);
/**
 * Evaluate the action and apply it to the pose. Ignore selection state of the bones.
 */
void pose_apply_action_all_bones(Object *ob,
                                 bAction *action,
                                 slot_handle_t slot_handle,
                                 const AnimationEvalContext *anim_eval_context);

void pose_apply_action_blend(Object *ob,
                             bAction *action,
                             slot_handle_t slot_handle,
                             const AnimationEvalContext *anim_eval_context,
                             float blend_factor);

/**
 * Get the best slot to read pose data from for the given ID.
 * Will always return a Slot as it falls back to the first Slot.
 *
 * Assumes that the Action has at least one Slot.
 */
Slot &get_best_pose_slot_for_id(const ID &id, Action &pose_data);
}  // namespace blender::animrig
