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
 * Return all F-Curves in the Action.
 *
 * It will return all F-Curves for all slots/layers/strips.
 *
 * The use of this function is an indicator for code that might have to be
 * inspected to see if this is _really_ the desired behavior, or whether the
 * F-Curves for a specific slot/layer/strip should be used instead.
 *
 * \see #animrig::legacy::fcurves_for_action_slot
 */
Vector<const FCurve *> fcurves_all(const bAction *action);
Vector<FCurve *> fcurves_all(bAction *action);

/**
 * Return whether the action (+slot), if any, assigned to `adt` has keyframes.
 * This only considers the assigned slot.
 *
 * A null `adt` or a lack of assigned action are both handled, and are
 * considered to mean no key frames (and thus will return false).
 */
bool assigned_action_has_keyframes(AnimData *adt);

/**
 * Return all Channel Groups in the Action.
 * This will return all channel groups for all slots/layers/strips.
 *
 * \see #animrig::legacy::channel_groups_for_assigned_slot
 */
Vector<bActionGroup *> channel_groups_all(bAction *action);

/**
 * Return all Channel Groups for the assigned Action Slot.
 *
 * \see #animrig::legacy::channel_groups_all
 */
Vector<bActionGroup *> channel_groups_for_assigned_slot(AnimData *adt);

/**
 * Remove all F-Curves whose RNA path starts with the given prefix from an Action Slot.
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
