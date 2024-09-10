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

  AnimData *adt = BKE_animdata_from_id(&animated_id);
  BLI_assert_msg(adt,
                 "animrig::nla::assign_action_slot assumes, since there is a strip, that it is "
                 "owned by the given ID");

  Action &action = strip.act->wrap();

  /* Check that the slot can actually be assigned. */
  if (slot_to_assign) {
    if (!action.slots().as_span().contains(slot_to_assign)) {
      return ActionSlotAssignmentResult::SlotNotFromAction;
    }

    if (!slot_to_assign->is_suitable_for(animated_id)) {
      return ActionSlotAssignmentResult::SlotNotSuitable;
    }
  }

  Slot *slot_to_unassign = action.slot_for_handle(strip.action_slot_handle);

  /* Un-assign the currently-assigned slot first, so the code below doesn't find
   * this reference any more. */
  strip.action_slot_handle = Slot::unassigned;

  /* See if there are any other uses of this action slot on this ID, and if
   * not, remove the ID from the slot's user map. */
  if (slot_to_unassign) {
    const bool is_directly_assigned = (adt->action == strip.act &&
                                       adt->slot_handle == slot_to_unassign->handle);

    if (!is_directly_assigned && !is_nla_referencing_slot(*adt, action, slot_to_unassign->handle))
    {
      slot_to_unassign->users_remove(animated_id);
    }
  }

  if (!slot_to_assign) {
    /* Make sure that the stored Slot name is up to date. The slot name might have
     * changed in a way that wasn't copied into the ADT yet (for example when the
     * Action is linked from another file), so better copy the name to be sure
     * that it can be transparently reassigned later.
     *
     * TODO: Replace this with a BLI_assert() that the name is as expected, and "simply" ensure
     * this name is always correct. */
    STRNCPY_UTF8(strip.action_slot_name, slot_to_unassign->name);
    return ActionSlotAssignmentResult::OK;
  }

  strip.action_slot_handle = slot_to_assign->handle;
  STRNCPY_UTF8(strip.action_slot_name, slot_to_assign->name);
  slot_to_assign->users_add(animated_id);

  return ActionSlotAssignmentResult::OK;
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

bool is_nla_referencing_slot(const AnimData &adt,
                             const Action &action,
                             const slot_handle_t slot_handle)
{

  const bool looped_until_the_end = bke::nla::foreach_strip_adt(adt, [&](NlaStrip *strip) {
    const bool found_the_slot = (strip->act == &action &&
                                 strip->action_slot_handle == slot_handle);
    /* Keep looping until the slot handle is found. */
    return !found_the_slot;
  });

  return !looped_until_the_end;
}

}  // namespace blender::animrig::nla
