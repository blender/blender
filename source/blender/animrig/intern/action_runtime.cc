/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 *
 * \brief Internal C++ functions to deal with Actions, Bindings, and their runtime data.
 */

#include "BKE_anim_data.hh"
#include "BKE_global.hh"
#include "BKE_main.hh"

#include "BLI_set.hh"

#include "ANIM_action.hh"

#include "action_runtime.hh"

namespace blender::animrig::internal {

/**
 * Rebuild the binding user cache for a specific bmain.
 */
void rebuild_binding_user_cache(Main &bmain)
{
  /* Loop over all Actions and clear their bindings' user cache. */
  LISTBASE_FOREACH (bAction *, dna_action, &bmain.actions) {
    Action &action = dna_action->wrap();
    for (Binding *binding : action.bindings()) {
      BLI_assert_msg(binding->runtime, "Binding::runtime should always be allocated");
      binding->runtime->users.clear();
    }
  }

  /* Mark all Bindings as clear. This is a bit of a lie, because the code below still has to run.
   * However, this is a necessity to make the `binding.users_add(*id)` call work without triggering
   * an infinite recursion.
   *
   * The alternative would be to go around the `binding.users_add()` function and access the
   * runtime directly, but this is IMO a bit cleaner. */
  bmain.is_action_binding_to_id_map_dirty = false;

  /* Loop over all IDs to cache their binding usage. */
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

      std::optional<std::pair<Action *, Binding *>> action_binding = get_action_binding_pair(*id);
      if (!action_binding) {
        continue;
      }

      Binding &binding = *action_binding->second;
      binding.users_add(*id);
    }
    FOREACH_MAIN_LISTBASE_ID_END;
  }
  FOREACH_MAIN_LISTBASE_END;
}

}  // namespace blender::animrig::internal
