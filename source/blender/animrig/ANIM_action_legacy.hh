/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief Functions for backward compatibility with the legacy Action API.
 *
 * It should be possible to remove these functions (and their callers) in
 * Blender 5.0, when we can remove the legacy API altogether.
 */
#pragma once

#include "BLI_vector.hh"

#include "ANIM_action.hh"

namespace blender::animrig::legacy {

constexpr const char *DEFAULT_LEGACY_SLOT_NAME = "Legacy Slot";
constexpr const char *DEFAULT_LEGACY_LAYER_NAME = "Legacy Layer";

/**
 * Ensure that a Slot exists, for legacy Python API shims that need one.
 *
 * \return The first Slot if one already exists, or a newly created "Legacy
 * Slot" otherwise.
 */
Slot &slot_ensure(Action &action);

/**
 * Return the Channelbag for compatibility with the legacy Python API.
 *
 * \return the Channelbag for the first slot, of the first keyframe Strip on the
 * bottom layer, or nullptr if that doesn't exist.
 */
Channelbag *channelbag_get(Action &action);

/**
 * Ensure a Channelbag exists, for compatibility with the legacy Python API.
 *
 * This basically is channelbag_get(action), additionally creating the necessary
 * slot, layer, and keyframe strip if necessary.
 */
Channelbag &channelbag_ensure(Action &action);

/**
 * Return all F-Curves in the Action.
 *
 * This works for both legacy and layered Actions. For the latter, it will
 * return all F-Curves for all slots/layers/strips.
 *
 * The use of this function is an indicator for code that might have to be
 * inspected to see if this is _really_ the desired behavior, or whether the
 * F-Curves for a specific slot/layer/strip should be used instead.
 *
 * \see #blender::animrig::legacy::fcurves_for_action_slot
 */
Vector<const FCurve *> fcurves_all(const bAction *action);
Vector<FCurve *> fcurves_all(bAction *action);

/**
 * Return the F-Curves for the first slot of this Action.
 *
 * This works for both legacy and layered Actions. For the former, it will
 * return all F-Curves in the Action.
 */
Vector<FCurve *> fcurves_first_slot(bAction *action);

/**
 * Return the F-Curves for this specific slot handle.
 *
 * On a legacy Action, this returns all F-Curves, and ignores the slot handle.
 *
 * The use of this function is an indicator for code that can be simplified when the slotted
 * Actions feature is no longer experimental. When that switchover happens, calls to this function
 * can be replaced with the more efficient `blender::animrig::fcurves_for_action_slot()`.
 *
 * \see #blender::animrig::fcurves_for_action_slot
 * \see #blender::animrig::legacy::fcurves_all
 */
Vector<FCurve *> fcurves_for_action_slot(bAction *action, slot_handle_t slot_handle);
Vector<const FCurve *> fcurves_for_action_slot(const bAction *action, slot_handle_t slot_handle);

/**
 * Return the F-Curves for the assigned Action Slot.
 *
 * For legacy Actions, this ignores the slot and just returns all F-Curves of the assigned Action.
 *
 * If `adt` is `nullptr` or there is no Action assigned (i.e. `adt->action == nullptr`), an empty
 * Vector is returned.
 */
Vector<FCurve *> fcurves_for_assigned_action(AnimData *adt);
Vector<const FCurve *> fcurves_for_assigned_action(const AnimData *adt);

/**
 * Return whether the action (+slot), if any, assigned to `adt` has keyframes.
 *
 * This works for both layered and legacy actions. For layered actions this only
 * considers the assigned slot.
 *
 * A null `adt` or a lack of assigned action are both handled, and are
 * considered to mean no key frames (and thus will return false).
 */
bool assigned_action_has_keyframes(AnimData *adt);

/**
 * Return all Channel Groups in the Action.
 *
 * This works for both legacy and layered Actions. For the latter, it will
 * return all channel groups for all slots/layers/strips.
 *
 * \see #blender::animrig::legacy::channel_groups_for_assigned_slot
 */
Vector<bActionGroup *> channel_groups_all(bAction *action);

/**
 * Return all Channel Groups for the assigned Action Slot.
 *
 * This works for both legacy and layered Actions. For the former, this function
 * acts identical to channel_groups_all().
 *
 * \see #blender::animrig::legacy::channel_groups_all
 */
Vector<bActionGroup *> channel_groups_for_assigned_slot(AnimData *adt);

/**
 * Determine whether to treat this Action as a legacy Action or not.
 *
 * - empty Action: returns the value of the 'Slotted Actions' experimental feature.
 * - layered Action: always returns false.
 * - legacy Action: always returns true.
 */
bool action_treat_as_legacy(const bAction &action);

/**
 * Remove all F-Curves whose RNA path starts with the given prefix from an Action Slot.
 *
 * This function works for both legacy and layered Actions. For the former, the
 * slot handle is ignored.
 *
 * \param rna_path_prefix: All F-Curves whose RNA path start with this string will get removed.
 * Note that there is no other semantics here, so `prefix = "rotation"` will remove
 * "rotation_euler" as well. The prefix may not be an empty string.
 *
 * \return true if any were removed, false otherwise.
 */
bool action_fcurves_remove(bAction &action,
                           slot_handle_t slot_handle,
                           StringRefNull rna_path_prefix);

}  // namespace blender::animrig::legacy
