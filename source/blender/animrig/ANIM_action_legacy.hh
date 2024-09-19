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

/**
 * Return the ChannelBag for compatibility with the legacy Python API.
 *
 * \return the ChannelBag for the first slot, of the first keyframe Strip on the
 * bottom layer, or nullptr if that doesn't exist.
 */
ChannelBag *channelbag_get(Action &action);

/**
 * Ensure a ChannelBag exists, for compatibility with the legacy Python API.
 *
 * This basically is channelbag_get(action), additionally creating the necessary
 * slot, layer, and keyframe strip if necessary.
 */
ChannelBag &channelbag_ensure(Action &action);

/**
 * Return all F-Curves in the Action.
 *
 * This works for both legacy and layered Actions. For the latter, it will
 * return all F-Curves for all slots/layers/strips.
 *
 * The use of this function is an indicator for code that might have to be
 * inspected to see if this is _really_ the desired behaviour, or whether the
 * F-Curves for a specific slot/layer/strip should be used instead.
 *
 * \see blender::animrig::legacy::fcurves_for_action_slot
 */
Vector<const FCurve *> fcurves_all(const bAction *action);
Vector<FCurve *> fcurves_all(bAction *action);

/**
 * Return the F-Curves for this specific slot handle.
 *
 * On a legacy Action, this returns all F-Curves, and ignores the slot handle.
 *
 * The use of this function is an indicator for code that can be simplified when the slotted
 * Actions feature is no longer experimental. When that switchover happens, calls to this function
 * can be replaced with the more efficient `blender::animrig::fcurves_for_action_slot()`.
 *
 * \see blender::animrig::fcurves_for_action_slot
 * \see blender::animrig::legacy::fcurves_all
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

}  // namespace blender::animrig::legacy
