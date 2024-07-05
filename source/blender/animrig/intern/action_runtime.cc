/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief Internal C++ functions to deal with Actions, Slots, and their runtime data.
 */

#include "BKE_anim_data.hh"
#include "BKE_global.hh"
#include "BKE_main.hh"

#include "BLI_set.hh"

#include "ANIM_action.hh"

#include "action_runtime.hh"

namespace blender::animrig::internal {

/**
 * Rebuild the slot user cache for a specific bmain.
 */
void rebuild_slot_user_cache(Main &bmain)
{
  /* Loop over all Actions and clear their slots' user cache. */
  LISTBASE_FOREACH (bAction *, dna_action, &bmain.actions) {
    Action &action = dna_action->wrap();
    for (Slot *slot : action.slots()) {
      BLI_assert_msg(slot->runtime, "Slot::runtime should always be allocated");
      slot->runtime->users.clear();
    }
  }

  /* Mark all Slots as clear. This is a bit of a lie, because the code below still has to run.
   * However, this is a necessity to make the `slot.users_add(*id)` call work without triggering
   * an infinite recursion.
   *
   * The alternative would be to go around the `slot.users_add()` function and access the
   * runtime directly, but this is IMO a bit cleaner. */
  bmain.is_action_slot_to_id_map_dirty = false;

  /* Loop over all IDs to cache their slot usage. */
  ListBase *ids_of_idtype;
  ID *id;
  FOREACH_MAIN_LISTBASE_BEGIN (&bmain, ids_of_idtype) {
    /* Check whether this ID type can be animated. If not, just skip all IDs of this type. */
    id = static_cast<ID *>(ids_of_idtype->first);
    if (!id || !id_type_can_have_animdata(GS(id->name))) {
      continue;
    }

    FOREACH_MAIN_LISTBASE_ID_BEGIN (ids_of_idtype, id) {
      BLI_assert(id_can_have_animdata(id));

      std::optional<std::pair<Action *, Slot *>> action_slot = get_action_slot_pair(*id);
      if (!action_slot) {
        continue;
      }

      Slot &slot = *action_slot->second;
      slot.users_add(*id);
    }
    FOREACH_MAIN_LISTBASE_ID_END;
  }
  FOREACH_MAIN_LISTBASE_END;
}

}  // namespace blender::animrig::internal
