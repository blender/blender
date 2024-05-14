/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup animrig
 */

#include "ANIM_action.hh"
#include "ANIM_animdata.hh"

#include "BKE_action.h"
#include "BKE_anim_data.hh"
#include "BKE_fcurve.hh"
#include "BKE_lib_id.hh"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "DNA_anim_types.h"

#include "ED_anim_api.hh"

#include "RNA_access.hh"
#include "RNA_path.hh"

namespace blender::animrig {

/* -------------------------------------------------------------------- */
/** \name Public F-Curves API
 * \{ */

bAction *id_action_ensure(Main *bmain, ID *id)
{
  AnimData *adt;

  /* init animdata if none available yet */
  adt = BKE_animdata_from_id(id);
  if (adt == nullptr) {
    adt = BKE_animdata_ensure_id(id);
  }
  if (adt == nullptr) {
    /* if still none (as not allowed to add, or ID doesn't have animdata for some reason) */
    printf("ERROR: Couldn't add AnimData (ID = %s)\n", (id) ? (id->name) : "<None>");
    return nullptr;
  }

  /* init action if none available yet */
  /* TODO: need some wizardry to handle NLA stuff correct */
  if (adt->action == nullptr) {
    /* init action name from name of ID block */
    char actname[sizeof(id->name) - 2];
    SNPRINTF(actname, "%sAction", id->name + 2);

    /* create action */
    adt->action = BKE_action_add(bmain, actname);

    /* set ID-type from ID-block that this is going to be assigned to
     * so that users can't accidentally break actions by assigning them
     * to the wrong places
     */
    BKE_animdata_action_ensure_idroot(id, adt->action);

    /* Tag depsgraph to be rebuilt to include time dependency. */
    DEG_relations_tag_update(bmain);
  }

  DEG_id_tag_update(&adt->action->id, ID_RECALC_ANIMATION_NO_FLUSH);

  /* return the action */
  return adt->action;
}

void animdata_fcurve_delete(bAnimContext *ac, AnimData *adt, FCurve *fcu)
{
  /* - If no AnimData, we've got nowhere to remove the F-Curve from
   *   (this doesn't guarantee that the F-Curve is in there, but at least we tried
   * - If no F-Curve, there is nothing to remove
   */
  if (ELEM(nullptr, adt, fcu)) {
    return;
  }

  /* Remove from whatever list it came from
   * - Action Group
   * - Action
   * - Drivers
   * - TODO... some others?
   */
  if ((ac) && (ac->datatype == ANIMCONT_DRIVERS)) {
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
      /* TODO: support deleting FCurves from Animation data-blocks. */
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

    if (BLI_listbase_is_empty(&act->curves) && (adt->flag & ADT_NLA_EDIT_ON) == 0) {
      id_us_min(&act->id);
      adt->action = nullptr;
      return true;
    }
  }

  return false;
}

/** \} */

void reevaluate_fcurve_errors(bAnimContext *ac)
{
  /* Need to take off the flag before filtering, else the filter code would skip the FCurves, which
   * have not yet been validated. */
  const bool filtering_enabled = ac->ads->filterflag & ADS_FILTER_ONLY_ERRORS;
  if (filtering_enabled) {
    ac->ads->filterflag &= ~ADS_FILTER_ONLY_ERRORS;
  }
  ListBase anim_data = {nullptr, nullptr};
  const eAnimFilter_Flags filter = ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FCURVESONLY;
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, eAnimCont_Types(ac->datatype));

  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    FCurve *fcu = (FCurve *)ale->key_data;
    PointerRNA ptr;
    PropertyRNA *prop;
    PointerRNA id_ptr = RNA_id_pointer_create(ale->id);
    if (RNA_path_resolve_property(&id_ptr, fcu->rna_path, &ptr, &prop)) {
      fcu->flag &= ~FCURVE_DISABLED;
    }
    else {
      fcu->flag |= FCURVE_DISABLED;
    }
  }

  ANIM_animdata_freelist(&anim_data);
  if (filtering_enabled) {
    ac->ads->filterflag |= ADS_FILTER_ONLY_ERRORS;
  }
}

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

  const Binding *binding = action.binding_for_handle(adt.binding_handle);
  if (!binding) {
    /* No need to inspect anything if this ID does not have an animation Binding. */
    return nullptr;
  }

  /* No check for the binding's ID type. Not only do we not have the actual ID
   * to do this check, but also, since the Action and the binding have been
   * assigned, just trust that it's valid. */

  /* Iterate the layers top-down, as higher-up animation overrides (or at least can override)
   * lower-down animation. */
  for (int layer_idx = action.layer_array_num - 1; layer_idx >= 0; layer_idx--) {
    const Layer *layer = action.layer(layer_idx);

    /* TODO: refactor this into something nicer once we have different strip types. */
    for (const Strip *strip : layer->strips()) {
      switch (strip->type()) {
        case Strip::Type::Keyframe: {
          const KeyframeStrip &key_strip = strip->as<KeyframeStrip>();
          const ChannelBag *channelbag_for_binding = key_strip.channelbag_for_binding(*binding);
          if (!channelbag_for_binding) {
            continue;
          }
          const FCurve *fcu = channelbag_for_binding->fcurve_find(rna_path, array_index);
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
