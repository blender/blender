/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 */

#include "ANIM_nla.hh"

#include "BKE_anim_data.hh"
#include "BKE_lib_id.hh"
#include "BKE_nla.hh"

#include "BLI_string_utf8.h"

namespace blender::animrig::nla {

bool assign_action(NlaStrip &strip, Action &action, ID &animated_id)
{
  if (strip.act == &action) {
    /* Already assigned, so just leave the slot as-is. */
    return false;
  }

  unassign_action(strip, animated_id);

  /* Assign the Action. */
  strip.act = &action;
  id_us_plus(&action.id);

  /* Find a slot with the previously-used slot name. */
  if (strip.action_slot_name[0]) {
    Slot *slot = action.slot_find_by_name(strip.action_slot_name);
    if (slot && assign_action_slot(strip, slot, animated_id) == ActionSlotAssignmentResult::OK) {
      return true;
    }
  }

  /* As a last resort, search for the ID name. */
  Slot *slot = action.slot_find_by_name(animated_id.name);
  if (slot && assign_action_slot(strip, slot, animated_id) == ActionSlotAssignmentResult::OK) {
    return true;
  }

  return false;
}

void unassign_action(NlaStrip &strip, ID &animated_id)
{
  if (!strip.act) {
    /* No Action was assigned, so nothing to do here. */
    BLI_assert_msg(strip.action_slot_handle == Slot::unassigned,
                   "When there is no Action assigned, the slot handle should also be set to "
                   "'unassigned'");
    return;
  }

  /* Unassign any previously-assigned slot. */
  if (strip.action_slot_handle != Slot::unassigned) {
    assign_action_slot(strip, nullptr, animated_id);
  }

  /* Unassign the Action. */
  id_us_min(&strip.act->id);
  strip.act = nullptr;
}

ActionSlotAssignmentResult assign_action_slot(NlaStrip &strip,
                                              Slot *slot_to_assign,
                                              ID &animated_id)
{
  BLI_assert(strip.act);

  return generic_assign_action_slot(
      slot_to_assign, animated_id, strip.act, strip.action_slot_handle, strip.action_slot_name);
}

ActionSlotAssignmentResult assign_action_slot_handle(NlaStrip &strip,
                                                     const slot_handle_t slot_handle,
                                                     ID &animated_id)
{
  BLI_assert(strip.act);

  Action &action = strip.act->wrap();
  Slot *slot_to_assign = action.slot_for_handle(slot_handle);

  return assign_action_slot(strip, slot_to_assign, animated_id);
}

}  // namespace blender::animrig::nla
