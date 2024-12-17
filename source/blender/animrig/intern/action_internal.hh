/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief Internal C++ functions to deal with Actions.
 */

#pragma once

#include "BLI_string_ref.hh"

struct ID;

namespace blender::animrig {

class Action;
class Slot;

/**
 * Generic function for finding the slot to auto-assign when the Action is assigned.
 *
 * This is a low-level function, used by generic_assign_action() to pick a slot.
 * It's declared here so that unit tests can reach it.
 *
 * The function is named "generic" as it is independent of whether this is for
 * direct assignment to the ID, or to an NLA strip, or an Action Constraint.
 *
 * \see #generic_assign_action()
 * \see #generic_assign_action_slot()
 * \see #generic_assign_action_slot_handle()
 */
[[nodiscard]] Slot *generic_slot_for_autoassign(const ID &animated_id,
                                                Action &action,
                                                StringRefNull last_slot_identifier);

}  // namespace blender::animrig
