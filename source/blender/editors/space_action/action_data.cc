/* SPDX-FileCopyrightText: 2015 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spaction
 */

#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_anim_types.h"
#include "DNA_key_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.h"

#include "BKE_action.h"
#include "BKE_context.hh"
#include "BKE_key.hh"
#include "BKE_lib_id.hh"
#include "BKE_nla.h"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "ED_anim_api.hh"
#include "ED_screen.hh"

#include "DEG_depsgraph.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "UI_interface.hh"

#include "action_intern.hh"

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

AnimData *ED_actedit_animdata_from_context(const bContext *C, ID **r_adt_id_owner)
{
  SpaceAction *saction = (SpaceAction *)CTX_wm_space_data(C);
  Object *ob = CTX_data_active_object(C);
  AnimData *adt = nullptr;

  /* Get AnimData block to use */
  if (saction->mode == SACTCONT_ACTION) {
    /* Currently, "Action Editor" means object-level only... */
    if (ob) {
      adt = ob->adt;
      if (r_adt_id_owner) {
        *r_adt_id_owner = &ob->id;
      }
    }
  }
  else if (saction->mode == SACTCONT_SHAPEKEY) {
    Key *key = BKE_key_from_object(ob);
    if (key) {
      adt = key->adt;
      if (r_adt_id_owner) {
        *r_adt_id_owner = &key->id;
      }
    }
  }

  return adt;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Create New Action
 * \{ */

static bAction *action_create_new(bContext *C, bAction *oldact)
{
  ScrArea *area = CTX_wm_area(C);
  bAction *action;

  /* create action - the way to do this depends on whether we've got an
   * existing one there already, in which case we make a copy of it
   * (which is useful for "versioning" actions within the same file)
   */
  if (oldact && GS(oldact->id.name) == ID_AC) {
    /* make a copy of the existing action */
    action = (bAction *)BKE_id_copy(CTX_data_main(C), &oldact->id);
  }
  else {
    /* just make a new (empty) action */
    action = BKE_action_add(CTX_data_main(C), "Action");
  }

  /* when creating new ID blocks, there is already 1 user (as for all new datablocks),
   * but the RNA pointer code will assign all the proper users instead, so we compensate
   * for that here
   */
  BLI_assert(action->id.us == 1);
  id_us_min(&action->id);

  /* set ID-Root type */
  if (area->spacetype == SPACE_ACTION) {
    SpaceAction *saction = (SpaceAction *)area->spacedata.first;

    if (saction->mode == SACTCONT_SHAPEKEY) {
      action->idroot = ID_KE;
    }
    else {
      action->idroot = ID_OB;
    }
  }

  return action;
}

/* Change the active action used by the action editor */
static void actedit_change_action(bContext *C, bAction *act)
{
  bScreen *screen = CTX_wm_screen(C);
  SpaceAction *saction = (SpaceAction *)CTX_wm_space_data(C);

  PropertyRNA *prop;

  /* create RNA pointers and get the property */
  PointerRNA ptr = RNA_pointer_create(&screen->id, &RNA_SpaceDopeSheetEditor, saction);
  prop = RNA_struct_find_property(&ptr, "action");

  /* NOTE: act may be nullptr here, so better to just use a cast here */
  PointerRNA idptr = RNA_id_pointer_create((ID *)act);

  /* set the new pointer, and force a refresh */
  RNA_property_pointer_set(&ptr, prop, idptr, nullptr);
  RNA_property_update(C, &ptr, prop);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name New Action Operator
 *
 * Criteria:
 * 1) There must be an dope-sheet/action editor, and it must be in a mode which uses actions...
 *       OR
 *    The NLA Editor is active (i.e. Animation Data panel -> new action)
 * 2) The associated #AnimData block must not be in tweak-mode.
 * \{ */

static bool action_new_poll(bContext *C)
{
  Scene *scene = CTX_data_scene(C);

  /* Check tweak-mode is off (as you don't want to be tampering with the action in that case) */
  /* NOTE: unlike for pushdown,
   * this operator needs to be run when creating an action from nothing... */
  if (ED_operator_action_active(C)) {
    SpaceAction *saction = (SpaceAction *)CTX_wm_space_data(C);
    Object *ob = CTX_data_active_object(C);

    /* For now, actions are only for the active object, and on object and shape-key levels... */
    if (saction->mode == SACTCONT_ACTION) {
      /* XXX: This assumes that actions are assigned to the active object in this mode */
      if (ob) {
        if ((ob->adt == nullptr) || (ob->adt->flag & ADT_NLA_EDIT_ON) == 0) {
          return true;
        }
      }
    }
    else if (saction->mode == SACTCONT_SHAPEKEY) {
      Key *key = BKE_key_from_object(ob);
      if (key) {
        if ((key->adt == nullptr) || (key->adt->flag & ADT_NLA_EDIT_ON) == 0) {
          return true;
        }
      }
    }
  }
  else if (ED_operator_nla_active(C)) {
    if (!(scene->flag & SCE_NLA_EDIT_ON)) {
      return true;
    }
  }

  /* something failed... */
  return false;
}

static int action_new_exec(bContext *C, wmOperator * /*op*/)
{
  PointerRNA ptr;
  PropertyRNA *prop;

  bAction *oldact = nullptr;
  AnimData *adt = nullptr;
  ID *adt_id_owner = nullptr;
  /* hook into UI */
  UI_context_active_but_prop_get_templateID(C, &ptr, &prop);

  if (prop) {
    /* The operator was called from a button. */
    PointerRNA oldptr;

    oldptr = RNA_property_pointer_get(&ptr, prop);
    oldact = (bAction *)oldptr.owner_id;

    /* stash the old action to prevent it from being lost */
    if (ptr.type == &RNA_AnimData) {
      adt = static_cast<AnimData *>(ptr.data);
      adt_id_owner = ptr.owner_id;
    }
    else if (ptr.type == &RNA_SpaceDopeSheetEditor) {
      adt = ED_actedit_animdata_from_context(C, &adt_id_owner);
    }
  }
  else {
    adt = ED_actedit_animdata_from_context(C, &adt_id_owner);
    oldact = adt->action;
  }
  {
    bAction *action = nullptr;

    /* Perform stashing operation - But only if there is an action */
    if (adt && oldact) {
      BLI_assert(adt_id_owner != nullptr);
      /* stash the action */
      if (BKE_nla_action_stash(adt, ID_IS_OVERRIDE_LIBRARY(adt_id_owner))) {
        /* The stash operation will remove the user already
         * (and unlink the action from the AnimData action slot).
         * Hence, we must unset the ref to the action in the
         * action editor too (if this is where we're being called from)
         * first before setting the new action once it is created,
         * or else the user gets decremented twice!
         */
        if (ptr.type == &RNA_SpaceDopeSheetEditor) {
          SpaceAction *saction = static_cast<SpaceAction *>(ptr.data);
          saction->action = nullptr;
        }
      }
      else {
#if 0
        printf("WARNING: Failed to stash %s. It may already exist in the NLA stack though\n",
               oldact->id.name);
#endif
      }
    }

    /* create action */
    action = action_create_new(C, oldact);

    if (prop) {
      /* set this new action
       * NOTE: we can't use actedit_change_action, as this function is also called from the NLA
       */
      PointerRNA idptr = RNA_id_pointer_create(&action->id);
      RNA_property_pointer_set(&ptr, prop, idptr, nullptr);
      RNA_property_update(C, &ptr, prop);
    }
  }

  /* set notifier that keyframes have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_ADDED, nullptr);

  return OPERATOR_FINISHED;
}

void ACTION_OT_new(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "New Action";
  ot->idname = "ACTION_OT_new";
  ot->description = "Create new action";

  /* api callbacks */
  ot->exec = action_new_exec;
  ot->poll = action_new_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Action Push-Down Operator
 *
 * Criteria:
 * 1) There must be an dope-sheet/action editor, and it must be in a mode which uses actions.
 * 2) There must be an action active.
 * 3) The associated #AnimData block must not be in tweak-mode.
 * \{ */

static bool action_pushdown_poll(bContext *C)
{
  if (ED_operator_action_active(C)) {
    SpaceAction *saction = (SpaceAction *)CTX_wm_space_data(C);
    AnimData *adt = ED_actedit_animdata_from_context(C, nullptr);

    /* Check for AnimData, Actions, and that tweak-mode is off. */
    if (adt && saction->action) {
      /* NOTE: We check this for the AnimData block in question and not the global flag,
       *       as the global flag may be left dirty by some of the browsing ops here.
       */
      if (!(adt->flag & ADT_NLA_EDIT_ON)) {
        return true;
      }
    }
  }

  /* something failed... */
  return false;
}

static int action_pushdown_exec(bContext *C, wmOperator *op)
{
  SpaceAction *saction = (SpaceAction *)CTX_wm_space_data(C);
  ID *adt_id_owner = nullptr;
  AnimData *adt = ED_actedit_animdata_from_context(C, &adt_id_owner);

  /* Do the deed... */
  if (adt) {
    /* Perform the push-down operation
     * - This will deal with all the AnimData-side user-counts. */
    if (BKE_action_has_motion(adt->action) == 0) {
      /* action may not be suitable... */
      BKE_report(op->reports, RPT_WARNING, "Action must have at least one keyframe or F-Modifier");
      return OPERATOR_CANCELLED;
    }

    /* action can be safely added */
    BKE_nla_action_pushdown(adt, ID_IS_OVERRIDE_LIBRARY(adt_id_owner));

    Main *bmain = CTX_data_main(C);
    DEG_id_tag_update_ex(bmain, adt_id_owner, ID_RECALC_ANIMATION);

    /* The action needs updating too, as FCurve modifiers are to be reevaluated. They won't extend
     * beyond the NLA strip after pushing down to the NLA. */
    DEG_id_tag_update_ex(bmain, &adt->action->id, ID_RECALC_ANIMATION);

    /* Stop displaying this action in this editor
     * NOTE: The editor itself doesn't set a user...
     */
    saction->action = nullptr;
  }

  /* Send notifiers that stuff has changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA_ACTCHANGE, nullptr);
  return OPERATOR_FINISHED;
}

void ACTION_OT_push_down(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Push Down Action";
  ot->idname = "ACTION_OT_push_down";
  ot->description = "Push action down on to the NLA stack as a new strip";

  /* callbacks */
  ot->exec = action_pushdown_exec;
  ot->poll = action_pushdown_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Action Stash Operator
 * \{ */

static int action_stash_exec(bContext *C, wmOperator *op)
{
  SpaceAction *saction = (SpaceAction *)CTX_wm_space_data(C);
  ID *adt_id_owner = nullptr;
  AnimData *adt = ED_actedit_animdata_from_context(C, &adt_id_owner);

  /* Perform stashing operation */
  if (adt) {
    /* don't do anything if this action is empty... */
    if (BKE_action_has_motion(adt->action) == 0) {
      /* action may not be suitable... */
      BKE_report(op->reports, RPT_WARNING, "Action must have at least one keyframe or F-Modifier");
      return OPERATOR_CANCELLED;
    }

    /* stash the action */
    if (BKE_nla_action_stash(adt, ID_IS_OVERRIDE_LIBRARY(adt_id_owner))) {
      /* The stash operation will remove the user already,
       * so the flushing step later shouldn't double up
       * the user-count fixes. Hence, we must unset this ref
       * first before setting the new action.
       */
      saction->action = nullptr;
    }
    else {
      /* action has already been added - simply warn about this, and clear */
      BKE_report(op->reports, RPT_ERROR, "Action has already been stashed");
    }

    /* clear action refs from editor, and then also the backing data (not necessary) */
    actedit_change_action(C, nullptr);
  }

  /* Send notifiers that stuff has changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA_ACTCHANGE, nullptr);
  return OPERATOR_FINISHED;
}

void ACTION_OT_stash(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Stash Action";
  ot->idname = "ACTION_OT_stash";
  ot->description = "Store this action in the NLA stack as a non-contributing strip for later use";

  /* callbacks */
  ot->exec = action_stash_exec;
  ot->poll = action_pushdown_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_boolean(ot->srna,
                             "create_new",
                             true,
                             "Create New Action",
                             "Create a new action once the existing one has been safely stored");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Action Stash & Create Operator
 *
 * Criteria:
 * 1) There must be an dope-sheet/action editor, and it must be in a mode which uses actions.
 * 2) The associated #AnimData block must not be in tweak-mode.
 * \{ */

static bool action_stash_create_poll(bContext *C)
{
  if (ED_operator_action_active(C)) {
    AnimData *adt = ED_actedit_animdata_from_context(C, nullptr);

    /* Check tweak-mode is off (as you don't want to be tampering with the action in that case) */
    /* NOTE: unlike for pushdown,
     * this operator needs to be run when creating an action from nothing... */
    if (adt) {
      if (!(adt->flag & ADT_NLA_EDIT_ON)) {
        return true;
      }
    }
    else {
      /* There may not be any action/animdata yet, so, just fallback to the global setting
       * (which may not be totally valid yet if the action editor was used and things are
       * now in an inconsistent state)
       */
      SpaceAction *saction = (SpaceAction *)CTX_wm_space_data(C);
      Scene *scene = CTX_data_scene(C);

      if (!(scene->flag & SCE_NLA_EDIT_ON)) {
        /* For now, actions are only for the active object, and on object and shape-key levels...
         */
        return ELEM(saction->mode, SACTCONT_ACTION, SACTCONT_SHAPEKEY);
      }
    }
  }

  /* something failed... */
  return false;
}

static int action_stash_create_exec(bContext *C, wmOperator *op)
{
  SpaceAction *saction = (SpaceAction *)CTX_wm_space_data(C);
  ID *adt_id_owner = nullptr;
  AnimData *adt = ED_actedit_animdata_from_context(C, &adt_id_owner);

  /* Check for no action... */
  if (saction->action == nullptr) {
    /* just create a new action */
    bAction *action = action_create_new(C, nullptr);
    actedit_change_action(C, action);
  }
  else if (adt) {
    /* Perform stashing operation */
    if (BKE_action_has_motion(adt->action) == 0) {
      /* don't do anything if this action is empty... */
      BKE_report(op->reports, RPT_WARNING, "Action must have at least one keyframe or F-Modifier");
      return OPERATOR_CANCELLED;
    }

    /* stash the action */
    if (BKE_nla_action_stash(adt, ID_IS_OVERRIDE_LIBRARY(adt_id_owner))) {
      bAction *new_action = nullptr;

      /* Create new action not based on the old one
       * (since the "new" operator already does that). */
      new_action = action_create_new(C, nullptr);

      /* The stash operation will remove the user already,
       * so the flushing step later shouldn't double up
       * the user-count fixes. Hence, we must unset this ref
       * first before setting the new action.
       */
      saction->action = nullptr;
      actedit_change_action(C, new_action);
    }
    else {
      /* action has already been added - simply warn about this, and clear */
      BKE_report(op->reports, RPT_ERROR, "Action has already been stashed");
      actedit_change_action(C, nullptr);
    }
  }

  /* Send notifiers that stuff has changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA_ACTCHANGE, nullptr);
  return OPERATOR_FINISHED;
}

void ACTION_OT_stash_and_create(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Stash Action";
  ot->idname = "ACTION_OT_stash_and_create";
  ot->description =
      "Store this action in the NLA stack as a non-contributing strip for later use, and create a "
      "new action";

  /* callbacks */
  ot->exec = action_stash_create_exec;
  ot->poll = action_stash_create_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Action Unlink Operator
 *
 * We use a custom unlink operator here, as there are some technicalities which need special care:
 * 1) When in Tweak Mode, it shouldn't be possible to unlink the active action,
 *    or else, everything turns to custard.
 * 2) If the Action doesn't have any other users, the user should at least get
 *    a warning that it is going to get lost.
 * 3) We need a convenient way to exit Tweak Mode from the Action Editor
 * \{ */

void ED_animedit_unlink_action(
    bContext *C, ID *id, AnimData *adt, bAction *act, ReportList *reports, bool force_delete)
{
  ScrArea *area = CTX_wm_area(C);

  /* If the old action only has a single user (that it's about to lose),
   * warn user about it
   *
   * TODO: Maybe we should just save it for them? But then, there's the problem of
   *       trying to get rid of stuff that's actually unwanted!
   */
  if (act->id.us == 1) {
    BKE_reportf(reports,
                RPT_WARNING,
                "Action '%s' will not be saved, create Fake User or Stash in NLA Stack to retain",
                act->id.name + 2);
  }

  /* Clear Fake User and remove action stashing strip (if present) */
  if (force_delete) {
    /* Remove stashed strip binding this action to this datablock */
    /* XXX: we cannot unlink it from *OTHER* datablocks that may also be stashing it,
     * but GE users only seem to use/care about single-object binding for now so this
     * should be fine
     */
    if (adt) {
      NlaTrack *nlt, *nlt_next;
      NlaStrip *strip, *nstrip;

      for (nlt = static_cast<NlaTrack *>(adt->nla_tracks.first); nlt; nlt = nlt_next) {
        nlt_next = nlt->next;

        if (strstr(nlt->name, DATA_("[Action Stash]"))) {
          for (strip = static_cast<NlaStrip *>(nlt->strips.first); strip; strip = nstrip) {
            nstrip = strip->next;

            if (strip->act == act) {
              /* Remove this strip, and the track too if it doesn't have anything else */
              BKE_nlastrip_remove_and_free(&nlt->strips, strip, true);

              if (nlt->strips.first == nullptr) {
                BLI_assert(nstrip == nullptr);
                BKE_nlatrack_remove_and_free(&adt->nla_tracks, nlt, true);
              }
            }
          }
        }
      }
    }

    /* Clear Fake User */
    id_fake_user_clear(&act->id);
  }

  /* If in Tweak Mode, don't unlink. Instead, this becomes a shortcut to exit Tweak Mode. */
  if ((adt) && (adt->flag & ADT_NLA_EDIT_ON)) {
    BKE_nla_tweakmode_exit(adt);

    Scene *scene = CTX_data_scene(C);
    if (scene != nullptr) {
      scene->flag &= ~SCE_NLA_EDIT_ON;
    }
  }
  else {
    /* Unlink normally - Setting it to nullptr should be enough to get the old one unlinked */
    if (area->spacetype == SPACE_ACTION) {
      /* clear action editor -> action */
      actedit_change_action(C, nullptr);
    }
    else {
      /* clear AnimData -> action */
      PropertyRNA *prop;

      /* create AnimData RNA pointers */
      PointerRNA ptr = RNA_pointer_create(id, &RNA_AnimData, adt);
      prop = RNA_struct_find_property(&ptr, "action");

      /* clear... */
      RNA_property_pointer_set(&ptr, prop, PointerRNA_NULL, nullptr);
      RNA_property_update(C, &ptr, prop);
    }
  }
}

/* -------------------------- */

static bool action_unlink_poll(bContext *C)
{
  if (ED_operator_action_active(C)) {
    SpaceAction *saction = (SpaceAction *)CTX_wm_space_data(C);
    AnimData *adt = ED_actedit_animdata_from_context(C, nullptr);

    /* Only when there's an active action, in the right modes... */
    if (saction->action && adt) {
      return true;
    }
  }

  /* something failed... */
  return false;
}

static int action_unlink_exec(bContext *C, wmOperator *op)
{
  AnimData *adt = ED_actedit_animdata_from_context(C, nullptr);
  bool force_delete = RNA_boolean_get(op->ptr, "force_delete");

  if (adt && adt->action) {
    ED_animedit_unlink_action(C, nullptr, adt, adt->action, op->reports, force_delete);
  }

  /* Unlink is also abused to exit NLA tweak mode. */
  WM_main_add_notifier(NC_ANIMATION | ND_NLA_ACTCHANGE, nullptr);

  return OPERATOR_FINISHED;
}

static int action_unlink_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  /* NOTE: this is hardcoded to match the behavior for the unlink button
   * (in `interface_templates.cc`). */
  RNA_boolean_set(op->ptr, "force_delete", event->modifier & KM_SHIFT);
  return action_unlink_exec(C, op);
}

void ACTION_OT_unlink(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Unlink Action";
  ot->idname = "ACTION_OT_unlink";
  ot->description = "Unlink this action from the active action slot (and/or exit Tweak Mode)";

  /* callbacks */
  ot->invoke = action_unlink_invoke;
  ot->exec = action_unlink_exec;
  ot->poll = action_unlink_poll;

  /* properties */
  prop = RNA_def_boolean(ot->srna,
                         "force_delete",
                         false,
                         "Force Delete",
                         "Clear Fake User and remove "
                         "copy stashed in this data-block's NLA stack");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Action Browsing
 * \{ */

/* Try to find NLA Strip to use for action layer up/down tool */
static NlaStrip *action_layer_get_nlastrip(ListBase *strips, float ctime)
{
  LISTBASE_FOREACH (NlaStrip *, strip, strips) {
    /* Can we use this? */
    if (IN_RANGE_INCL(ctime, strip->start, strip->end)) {
      /* in range - use this one */
      return strip;
    }
    if ((ctime < strip->start) && (strip->prev == nullptr)) {
      /* before first - use this one */
      return strip;
    }
    if ((ctime > strip->end) && (strip->next == nullptr)) {
      /* after last - use this one */
      return strip;
    }
  }

  /* nothing suitable found... */
  return nullptr;
}

/* Switch NLA Strips/Actions. */
static void action_layer_switch_strip(
    AnimData *adt, NlaTrack *old_track, NlaStrip *old_strip, NlaTrack *nlt, NlaStrip *strip)
{
  /* Exit tweak-mode on old strip
   * NOTE: We need to manually clear this stuff ourselves, as tweak-mode exit doesn't do it
   */
  BKE_nla_tweakmode_exit(adt);

  if (old_strip) {
    old_strip->flag &= ~(NLASTRIP_FLAG_ACTIVE | NLASTRIP_FLAG_SELECT);
  }
  if (old_track) {
    old_track->flag &= ~(NLATRACK_ACTIVE | NLATRACK_SELECTED);
  }

  /* Make this one the active one instead */
  strip->flag |= (NLASTRIP_FLAG_ACTIVE | NLASTRIP_FLAG_SELECT);
  nlt->flag |= NLATRACK_ACTIVE;

  /* Copy over "solo" flag - This is useful for stashed actions... */
  if (old_track) {
    if (old_track->flag & NLATRACK_SOLO) {
      old_track->flag &= ~NLATRACK_SOLO;
      nlt->flag |= NLATRACK_SOLO;
    }
  }
  else {
    /* NLA muting <==> Solo Tracks */
    if (adt->flag & ADT_NLA_EVAL_OFF) {
      /* disable NLA muting */
      adt->flag &= ~ADT_NLA_EVAL_OFF;

      /* mark this track as being solo */
      adt->flag |= ADT_NLA_SOLO_TRACK;
      nlt->flag |= NLATRACK_SOLO;

      /* TODO: Needs rest-pose flushing (when we get reference track) */
    }
  }

  /* Enter tweak-mode again - hopefully we're now "it" */
  BKE_nla_tweakmode_enter(adt);
  BLI_assert(adt->actstrip == strip);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name One Layer Up Operator
 * \{ */

static bool action_layer_next_poll(bContext *C)
{
  /* Action Editor's action editing modes only */
  if (ED_operator_action_active(C)) {
    AnimData *adt = ED_actedit_animdata_from_context(C, nullptr);
    if (adt) {
      /* only allow if we're in tweak-mode, and there's something above us... */
      if (adt->flag & ADT_NLA_EDIT_ON) {
        /* We need to check if there are any tracks above the active one
         * since the track the action comes from is not stored in AnimData
         */
        if (adt->nla_tracks.last) {
          NlaTrack *nlt = (NlaTrack *)adt->nla_tracks.last;

          if (nlt->flag & NLATRACK_DISABLED) {
            /* A disabled track will either be the track itself,
             * or one of the ones above it.
             *
             * If this is the top-most one, there is the possibility
             * that there is no active action. For now, we let this
             * case return true too, so that there is a natural way
             * to "move to an empty layer", even though this means
             * that we won't actually have an action.
             */
            // return (adt->tmpact != nullptr);
            return true;
          }
        }
      }
    }
  }

  /* something failed... */
  return false;
}

static int action_layer_next_exec(bContext *C, wmOperator *op)
{
  AnimData *adt = ED_actedit_animdata_from_context(C, nullptr);
  NlaTrack *act_track;

  Scene *scene = CTX_data_scene(C);
  float ctime = BKE_scene_ctime_get(scene);

  /* Get active track */
  act_track = BKE_nlatrack_find_tweaked(adt);

  if (act_track == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "Could not find current NLA Track");
    return OPERATOR_CANCELLED;
  }

  /* Find next action, and hook it up */
  if (act_track->next) {
    NlaTrack *nlt;

    /* Find next action to use */
    for (nlt = act_track->next; nlt; nlt = nlt->next) {
      NlaStrip *strip = action_layer_get_nlastrip(&nlt->strips, ctime);

      if (strip) {
        action_layer_switch_strip(adt, act_track, adt->actstrip, nlt, strip);
        break;
      }
    }
  }
  else {
    /* No more actions (strips) - Go back to editing the original active action
     * NOTE: This will mean exiting tweak-mode...
     */
    BKE_nla_tweakmode_exit(adt);

    /* Deal with solo flags...
     * Assume: Solo Track == NLA Muting
     */
    if (adt->flag & ADT_NLA_SOLO_TRACK) {
      /* turn off solo flags on tracks */
      act_track->flag &= ~NLATRACK_SOLO;
      adt->flag &= ~ADT_NLA_SOLO_TRACK;

      /* turn on NLA muting (to keep same effect) */
      adt->flag |= ADT_NLA_EVAL_OFF;

      /* TODO: Needs rest-pose flushing (when we get reference track) */
    }
  }

  /* Update the action that this editor now uses
   * NOTE: The calls above have already handled the user-count/anim-data side of things. */
  actedit_change_action(C, adt->action);
  return OPERATOR_FINISHED;
}

void ACTION_OT_layer_next(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Next Layer";
  ot->idname = "ACTION_OT_layer_next";
  ot->description =
      "Switch to editing action in animation layer above the current action in the NLA Stack";

  /* callbacks */
  ot->exec = action_layer_next_exec;
  ot->poll = action_layer_next_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name One Layer Down Operator
 * \{ */

static bool action_layer_prev_poll(bContext *C)
{
  /* Action Editor's action editing modes only */
  if (ED_operator_action_active(C)) {
    AnimData *adt = ED_actedit_animdata_from_context(C, nullptr);
    if (adt) {
      if (adt->flag & ADT_NLA_EDIT_ON) {
        /* Tweak Mode: We need to check if there are any tracks below the active one
         * that we can move to */
        if (adt->nla_tracks.first) {
          NlaTrack *nlt = (NlaTrack *)adt->nla_tracks.first;

          /* Since the first disabled track is the track being tweaked/edited,
           * we can simplify things by only checking the first track:
           *    - If it is disabled, this is the track being tweaked,
           *      so there can't be anything below it
           *    - Otherwise, there is at least 1 track below the tweaking
           *      track that we can descend to
           */
          if ((nlt->flag & NLATRACK_DISABLED) == 0) {
            /* not disabled = there are actions below the one being tweaked */
            return true;
          }
        }
      }
      else {
        /* Normal Mode: If there are any tracks, we can try moving to those */
        return (adt->nla_tracks.first != nullptr);
      }
    }
  }

  /* something failed... */
  return false;
}

static int action_layer_prev_exec(bContext *C, wmOperator *op)
{
  AnimData *adt = ED_actedit_animdata_from_context(C, nullptr);
  NlaTrack *act_track;
  NlaTrack *nlt;

  Scene *scene = CTX_data_scene(C);
  float ctime = BKE_scene_ctime_get(scene);

  /* Sanity Check */
  if (adt == nullptr) {
    BKE_report(
        op->reports, RPT_ERROR, "Internal Error: Could not find Animation Data/NLA Stack to use");
    return OPERATOR_CANCELLED;
  }

  /* Get active track */
  act_track = BKE_nlatrack_find_tweaked(adt);

  /* If there is no active track, that means we are using the active action... */
  if (act_track) {
    /* Active Track - Start from the one below it */
    nlt = act_track->prev;
  }
  else {
    /* Active Action - Use the top-most track */
    nlt = static_cast<NlaTrack *>(adt->nla_tracks.last);
  }

  /* Find previous action and hook it up */
  for (; nlt; nlt = nlt->prev) {
    NlaStrip *strip = action_layer_get_nlastrip(&nlt->strips, ctime);

    if (strip) {
      action_layer_switch_strip(adt, act_track, adt->actstrip, nlt, strip);
      break;
    }
  }

  /* Update the action that this editor now uses
   * NOTE: The calls above have already handled the user-count/animdata side of things. */
  actedit_change_action(C, adt->action);
  return OPERATOR_FINISHED;
}

void ACTION_OT_layer_prev(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Previous Layer";
  ot->idname = "ACTION_OT_layer_prev";
  ot->description =
      "Switch to editing action in animation layer below the current action in the NLA Stack";

  /* callbacks */
  ot->exec = action_layer_prev_exec;
  ot->poll = action_layer_prev_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */
