/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 */
#include "ANIM_action.hh"
#include "ANIM_animdata.hh"

#include "BKE_action.hh"
#include "BKE_anim_data.hh"
#include "BKE_fcurve.hh"
#include "BKE_key.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_material.hh"
#include "BKE_node.hh"

#include "BLT_translation.hh"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "DNA_anim_types.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_particle_types.h"

#include "RNA_access.hh"

namespace blender::animrig {

/* -------------------------------------------------------------------- */
/** \name Public F-Curves API
 * \{ */

/* Find the users of the given ID within the objects of `bmain` and add non-duplicates to the end
 * of `related_ids`. */
static void add_object_data_users(const Main &bmain, const ID &id, Vector<ID *> &related_ids)
{
  if (ID_REAL_USERS(&id) != 1) {
    /* Only find objects if this ID is only used once. */
    return;
  }

  Object *ob;
  ID *object_id;
  FOREACH_MAIN_LISTBASE_ID_BEGIN (&bmain.objects, object_id) {
    ob = (Object *)object_id;
    if (ob->data != &id) {
      continue;
    }
    related_ids.append_non_duplicates(&ob->id);
  }
  FOREACH_MAIN_LISTBASE_ID_END;
}

Vector<ID *> find_related_ids(Main &bmain, ID &id)
{
  Vector<ID *> related_ids({&id});

  /* `related_ids` can grow during an iteration if the ID of the current iteration has associated
   * code that defines relationships. */
  for (int i = 0; i < related_ids.size(); i++) {
    ID *related_id = related_ids[i];

    if (related_id->flag & ID_FLAG_EMBEDDED_DATA) {
      /* No matter the type of embedded ID, their owner can always be added to the related IDs. */

      /* User counting is irrelevant for the logic here, because embedded IDs cannot be shared.
       * Embedded IDs do exist (sometimes) with a non-zero user count, hence the assertion that the
       * user count is not greater than 1. */
      BLI_assert(ID_REAL_USERS(related_id) <= 1);
      ID *owner_id = BKE_id_owner_get(related_id);
      /* Embedded IDs should always have an owner. */
      BLI_assert(owner_id != nullptr);
      related_ids.append_non_duplicates(owner_id);
    }

    /* No action found on current ID, add related IDs to the ID Vector. */
    switch (GS(related_id->name)) {
      case ID_OB: {
        Object *ob = (Object *)related_id;
        if (!ob->data) {
          break;
        }
        ID *data = (ID *)ob->data;
        if (ID_REAL_USERS(data) == 1) {
          related_ids.append_non_duplicates(data);
        }
        LISTBASE_FOREACH (ParticleSystem *, particle_system, &ob->particlesystem) {
          if (!particle_system->part) {
            continue;
          }
          if (ID_REAL_USERS(&particle_system->part->id) != 1) {
            continue;
          }
          related_ids.append_non_duplicates(&particle_system->part->id);
        }
        break;
      }

      case ID_KE: {
        /* Shape-keys. */
        Key *key = (Key *)related_id;
        /* Shape-keys are not embedded but there is currently no way to reuse them. */
        BLI_assert(ID_REAL_USERS(related_id) == 1);
        related_ids.append_non_duplicates(key->from);
        break;
      }

      case ID_MA: {
        /* Explicitly not relating materials and material users. */
        Material *mat = (Material *)related_id;
        if (mat->nodetree && ID_REAL_USERS(&mat->nodetree->id) == 1) {
          related_ids.append_non_duplicates(&mat->nodetree->id);
        }
        break;
      }

      case ID_PA: {
        if (ID_REAL_USERS(related_id) != 1) {
          continue;
        }
        Object *ob;
        ID *object_id;
        /* Find users of this particle setting. */
        FOREACH_MAIN_LISTBASE_ID_BEGIN (&bmain.objects, object_id) {
          ob = (Object *)object_id;
          bool object_uses_particle_settings = false;
          LISTBASE_FOREACH (ParticleSystem *, particle_system, &ob->particlesystem) {
            if (!particle_system->part) {
              continue;
            }
            if (&particle_system->part->id != related_id) {
              continue;
            }
            object_uses_particle_settings = true;
            break;
          }
          if (object_uses_particle_settings) {
            related_ids.append_non_duplicates(&ob->id);
            break;
          }
        }
        FOREACH_MAIN_LISTBASE_ID_END;

        break;
      }

      default: {
        /* Just check if the ID is used as object data somewhere. */
        add_object_data_users(bmain, *related_id, related_ids);
        bNodeTree *node_tree = bke::node_tree_from_id(related_id);
        if (node_tree && ID_REAL_USERS(&node_tree->id) == 1) {
          related_ids.append_non_duplicates(&node_tree->id);
        }

        Key *key = BKE_key_from_id(related_id);
        if (key) {
          /* No check for multi user because the shape-key cannot be shared. */
          BLI_assert(ID_REAL_USERS(&key->id) == 1);
          related_ids.append_non_duplicates(&key->id);
        }
        break;
      }
    }
  }

  return related_ids;
}

/* Find an action on an ID that is related to the given ID. Related things are e.g. Object<->Data,
 * Mesh<->Material and so on. */
static bAction *find_related_action(Main &bmain, ID &id)
{
  Vector<ID *> related_ids = find_related_ids(bmain, id);

  for (ID *related_id : related_ids) {
    Action *action = get_action(*related_id);
    if (action && action->is_action_layered()) {
      /* Returning the first action found means highest priority has the action closest in the
       * relationship graph. */
      return action;
    }
  }

  return nullptr;
}

bAction *id_action_ensure(Main *bmain, ID *id)
{
  AnimData *adt = BKE_animdata_ensure_id(id);
  if (adt == nullptr) {
    printf("ERROR: data-block type is not animatable (ID = %s)\n", (id) ? (id->name) : "<None>");
    return nullptr;
  }

  /* init action if none available yet */
  /* TODO: need some wizardry to handle NLA stuff correct */
  if (adt->action == nullptr) {
    bAction *action = find_related_action(*bmain, *id);

    if (action == nullptr) {
      /* init action name from name of ID block */
      char actname[sizeof(id->name) - 2];
      if (id->flag & ID_FLAG_EMBEDDED_DATA) {
        /* When the ID is embedded, use the name of the owner ID for clarity. */
        ID *owner_id = BKE_id_owner_get(id);
        /* If the ID is embedded it should have an owner. */
        BLI_assert(owner_id != nullptr);
        SNPRINTF(actname, DATA_("%sAction"), owner_id->name + 2);
      }
      else if (GS(id->name) == ID_KE) {
        Key *key = (Key *)id;
        SNPRINTF(actname, DATA_("%sAction"), key->from->name + 2);
      }
      else {
        SNPRINTF(actname, DATA_("%sAction"), id->name + 2);
      }

      /* create action */
      action = BKE_action_add(bmain, actname);

      /* Decrement the default-1 user count, as assigning it will increase it again. */
      BLI_assert(action->id.us == 1);
      id_us_min(&action->id);
    }

    /* Assigning the Action should always work here. The only reason it wouldn't, is when a legacy
     * Action of the wrong ID type is assigned, but since in this branch of the code we're only
     * dealing with either new or layered Actions, this will never fail. */
    const bool ok = animrig::assign_action(action, {*id, *adt});
    BLI_assert_msg(ok, "Expecting Action assignment to work here");
    UNUSED_VARS_NDEBUG(ok);

    /* Tag depsgraph to be rebuilt to include time dependency. */
    DEG_relations_tag_update(bmain);
  }

  DEG_id_tag_update(&adt->action->id, ID_RECALC_ANIMATION_NO_FLUSH);

  /* return the action */
  return adt->action;
}

void animdata_fcurve_delete(AnimData *adt, FCurve *fcu)
{
  /* - If no AnimData, we've got nowhere to remove the F-Curve from
   *   (this doesn't guarantee that the F-Curve is in there, but at least we tried
   * - If no F-Curve, there is nothing to remove
   */
  if (ELEM(nullptr, adt, fcu)) {
    return;
  }

  const bool is_driver = fcu->driver != nullptr;
  if (is_driver) {
    BLI_remlink(&adt->drivers, fcu);
  }
  else if (adt->action) {
    Action &action = adt->action->wrap();

    if (action.is_action_legacy()) {
      /* Remove from group or action, whichever one "owns" the F-Curve. */
      if (fcu->grp) {
        bActionGroup *agrp = fcu->grp;

        /* Remove F-Curve from group+action. */
        action_groups_remove_channel(&action, fcu);

        /* If group has no more channels, remove it too,
         * otherwise can have many dangling groups #33541.
         */
        if (BLI_listbase_is_empty(&agrp->channels)) {
          BLI_freelinkN(&action.groups, agrp);
        }
      }
      else {
        BLI_remlink(&action.curves, fcu);
      }

      /* If action has no more F-Curves as a result of this, unlink it from
       * AnimData if it did not come from a NLA Strip being tweaked.
       *
       * This is done so that we don't have dangling Object+Action entries in
       * channel list that are empty, and linger around long after the data they
       * are for has disappeared (and probably won't come back).
       */
      animdata_remove_empty_action(adt);
    }
    else {
      action_fcurve_remove(action, *fcu);
      /* Return early to avoid the call to BKE_fcurve_free because the fcu has already been freed
       * by action_fcurve_remove. */
      return;
    }
  }
  else {
    BLI_assert_unreachable();
  }

  BKE_fcurve_free(fcu);
}

bool animdata_remove_empty_action(AnimData *adt)
{
  if (adt->action != nullptr) {
    bAction *act = adt->action;
    DEG_id_tag_update(&act->id, ID_RECALC_ANIMATION_NO_FLUSH);
    if (BLI_listbase_is_empty(&act->curves) && (adt->flag & ADT_NLA_EDIT_ON) == 0) {
      id_us_min(&act->id);
      adt->action = nullptr;
      return true;
    }
  }

  return false;
}

/** \} */

const FCurve *fcurve_find_by_rna_path(const AnimData &adt,
                                      const StringRefNull rna_path,
                                      const int array_index)
{
  BLI_assert(adt.action);
  if (!adt.action) {
    return nullptr;
  }

  const Action &action = adt.action->wrap();
  BLI_assert(action.is_action_layered());

  const Slot *slot = action.slot_for_handle(adt.slot_handle);
  if (!slot) {
    /* No need to inspect anything if this ID does not have an Action Slot. */
    return nullptr;
  }

  /* No check for the slot's ID type. Not only do we not have the actual ID
   * to do this check, but also, since the Action and the slot have been
   * assigned, just trust that it's valid. */

  /* Iterate the layers top-down, as higher-up animation overrides (or at least can override)
   * lower-down animation. */
  for (int layer_idx = action.layer_array_num - 1; layer_idx >= 0; layer_idx--) {
    const Layer *layer = action.layer(layer_idx);

    /* TODO: refactor this into something nicer once we have different strip types. */
    for (const Strip *strip : layer->strips()) {
      switch (strip->type()) {
        case Strip::Type::Keyframe: {
          const StripKeyframeData &strip_data = strip->data<StripKeyframeData>(action);
          const Channelbag *channelbag_for_slot = strip_data.channelbag_for_slot(*slot);
          if (!channelbag_for_slot) {
            continue;
          }
          const FCurve *fcu = channelbag_for_slot->fcurve_find({rna_path, array_index});
          if (!fcu) {
            continue;
          }

          /* This code assumes that there is only one strip, and that it's infinite. When that
           * changes, this code needs to be expanded to check for strip boundaries. */
          return fcu;
        }
      }
      /* Explicit lack of 'default' clause, to get compiler warnings when strip types are added. */
    }
  }

  return nullptr;
}

}  // namespace blender::animrig
