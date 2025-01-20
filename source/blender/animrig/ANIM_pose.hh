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
#include "BLI_span.hh"

struct AnimationEvalContext;
struct Object;
struct bAction;

namespace blender::animrig {

/**
 * Evaluate the action and apply it to the pose. Ignore selection state of the bones.
 */
void pose_apply_action_all_bones(Object *ob,
                                 bAction *action,
                                 slot_handle_t slot_handle,
                                 const AnimationEvalContext *anim_eval_context);

/**
 * Evaluate the action and blend the result into the current pose based on `blend_factor`.
 * Only FCurves that relate to selected bones are evaluated.
 */
void pose_apply_action_blend(Object *ob,
                             bAction *action,
                             slot_handle_t slot_handle,
                             const AnimationEvalContext *anim_eval_context,
                             float blend_factor);

/**
 * Like `pose_apply_action_blend` but applies to all bones regardless of selection.
 */
void pose_apply_action_blend_all_bones(Object *ob,
                                       bAction *action,
                                       slot_handle_t slot_handle,
                                       const AnimationEvalContext *anim_eval_context,
                                       float blend_factor);

/**
 * Apply the given Action to all objects of the Span.
 * The slot is chosen automatically, see `get_best_pose_slot_for_id`.
 */
void pose_apply_action(blender::Span<Object *> objects,
                       Action &pose_action,
                       const AnimationEvalContext *anim_eval_context,
                       float blend_factor);
/**
 * Return true if any bone is selected. This is useful to decide if all bones should be affected
 * or not.
 */
bool any_bone_selected(blender::Span<const Object *> objects);

/**
 * Get the best slot to read pose data from for the given ID.
 * Will always return a Slot as it falls back to the first Slot.
 *
 * Assumes that the Action has at least one Slot.
 */
Slot &get_best_pose_slot_for_id(const ID &id, Action &pose_data);
}  // namespace blender::animrig
