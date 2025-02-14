/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 */

#include "ANIM_nla.hh"

namespace blender::animrig::nla {

bool assign_action(NlaStrip &strip, Action &action, ID &animated_id)
{
  if (!generic_assign_action(
          animated_id, &action, strip.act, strip.action_slot_handle, strip.last_slot_identifier))
  {
    return false;
  }

  /* For the NLA, the auto slot selection gets one more fallback option (compared to the generic
   * code). This is to support the following scenario:
   *
   * - Python script creates an Action, and adds some F-Curves via the legacy API.
   * - This creates a slot 'XXSlot'.
   * - The script creates multiple NLA strips for that Action.
   * - The desired result is that these strips get the same Slot assigned as well.
   *
   * The generic code doesn't work for this. The first strip assignment would see the slot
   * `XXSlot`, and because it has never been used, just use it. This would change its name to, for
   * example, `OBSlot`. The second strip assignment would not see a 'virgin' slot, and thus not
   * auto-select `OBSlot`. This behavior makes sense when assigning Actions in the Action editor
   * (it shouldn't automatically pick the first slot of matching ID type), but for the NLA I
   * (Sybren) feel that it could be a bit more 'enthousiastic' in auto-picking a slot.
   */
  if (strip.action_slot_handle == Slot::unassigned && action.slots().size() == 1) {
    Slot *first_slot = action.slot(0);
    if (first_slot->is_suitable_for(animated_id)) {
      const ActionSlotAssignmentResult result = assign_action_slot(strip, first_slot, animated_id);
      BLI_assert_msg(result == ActionSlotAssignmentResult::OK,
                     "Assigning a slot that we know is suitable should work");
      UNUSED_VARS_NDEBUG(result);
    }
  }

  /* Regardless of slot auto-selection, the Action assignment worked just fine. */
  return true;
}

void unassign_action(NlaStrip &strip, ID &animated_id)
{
  const bool ok = generic_assign_action(
      animated_id, nullptr, strip.act, strip.action_slot_handle, strip.last_slot_identifier);
  BLI_assert_msg(ok, "Un-assigning an Action from an NLA strip should always work.");
  UNUSED_VARS_NDEBUG(ok);
}

ActionSlotAssignmentResult assign_action_slot(NlaStrip &strip,
                                              Slot *slot_to_assign,
                                              ID &animated_id)
{
  BLI_assert(strip.act);

  return generic_assign_action_slot(slot_to_assign,
                                    animated_id,
                                    strip.act,
                                    strip.action_slot_handle,
                                    strip.last_slot_identifier);
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
