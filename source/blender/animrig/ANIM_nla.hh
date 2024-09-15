/* SPDX-FileCopyrightText: 2024 Blender Authors. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup animrig
 */

#include "ANIM_action.hh"

struct ID;
struct NlaStrip;

namespace blender::animrig::nla {

enum class ActionSlotAssignmentResult : int8_t {
  OK = 0,
  SlotNotFromAction = 1, /* Slot does not belong to the assigned Action. */
  SlotNotSuitable = 2,   /* Slot is not suitable for the given ID type.*/
};

/**
 * Assign the Action to this NLA strip.
 *
 * Similar to animrig::assign_action(), this tries to find a suitable slot.
 *
 * \see blender::animrig::assign_action
 *
 * \returns whether a slot was automatically assigned.
 */
bool assign_action(NlaStrip &strip, Action &action, ID &animated_id);

void unassign_action(NlaStrip &strip, ID &animated_id);

/**
 * Assign a slot to the NLA strip.
 *
 * The strip should already have an Action assigned to it, and the given Slot should belong to that
 * Action.
 *
 * \param slot_to_assign: the slot to assign, or nullptr to un-assign the current slot.
 */
ActionSlotAssignmentResult assign_action_slot(NlaStrip &strip,
                                              Slot *slot_to_assign,
                                              ID &animated_id);

ActionSlotAssignmentResult assign_action_slot_handle(NlaStrip &strip,
                                                     slot_handle_t slot_handle,
                                                     ID &animated_id);

/**
 * Returns whether any NLA strip of this AnimData references the given Action and Slot.
 */
bool is_nla_referencing_slot(const AnimData &adt, const Action &action, slot_handle_t slot_handle);

}  // namespace blender::animrig::nla
