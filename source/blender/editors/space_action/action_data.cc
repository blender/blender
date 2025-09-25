/* SPDX-FileCopyrightText: 2015 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spaction
 */

#include <cfloat>
#include <cstdlib>
#include <cstring>

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_anim_types.h"
#include "DNA_key_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.hh"

#include "BKE_action.hh"
#include "BKE_context.hh"
#include "BKE_key.hh"
#include "BKE_lib_id.hh"
#include "BKE_nla.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "ANIM_action.hh"

#include "ED_anim_api.hh"
#include "ED_screen.hh"

#include "DEG_depsgraph.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "UI_interface_c.hh"

#include "action_intern.hh"

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

AnimData *ED_actedit_animdata_from_context(const bContext *C, ID **r_adt_id_owner)
{
  { /* Support use from the layout.template_action() UI template. */
    PointerRNA ptr = {};
    PropertyRNA *prop = nullptr;
    UI_context_active_but_prop_get_templateID(C, &ptr, &prop);
    /* template_action() sets a RNA_AnimData pointer, whereas other code may set
     * other pointer types. This code here only deals with the former. */
    if (prop && ptr.type == &RNA_AnimData) {
      if (!RNA_property_editable(&ptr, prop)) {
        return nullptr;
      }
      if (r_adt_id_owner) {
        *r_adt_id_owner = ptr.owner_id;
      }
      AnimData *adt = static_cast<AnimData *>(ptr.data);
      return adt;
    }
  }

  SpaceLink *space_data = CTX_wm_space_data(C);
  if (!space_data || space_data->spacetype != SPACE_ACTION) {
    return nullptr;
  }

  SpaceAction *saction = (SpaceAction *)space_data;
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
    action = BKE_action_add(CTX_data_main(C), DATA_("Action"));
  }

  /* when creating new ID blocks, there is already 1 user (as for all new datablocks),
   * but the RNA pointer code will assign all the proper users instead, so we compensate
   * for that here
   */
  BLI_assert(action->id.us == 1);
  id_us_min(&action->id);

  return action;
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
  { /* Support use from the layout.template_action() UI template. */
    PointerRNA ptr = {};
    PropertyRNA *prop = nullptr;
    UI_context_active_but_prop_get_templateID(C, &ptr, &prop);
    if (prop) {
      return RNA_property_editable(&ptr, prop);
    }
  }

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

static wmOperatorStatus action_new_exec(bContext *C, wmOperator * /*op*/)
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
      if (!BKE_nla_action_stash({*adt_id_owner, *adt}, ID_IS_OVERRIDE_LIBRARY(adt_id_owner))) {
#if 0
        printf("WARNING: Failed to stash %s. It may already exist in the NLA stack though\n",
               oldact->id.name);
#endif
      }
    }

    /* create action */
    action = action_create_new(C, oldact);

    if (prop) {
      /* set this new action */
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

  /* API callbacks. */
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
  if (!ED_operator_action_active(C)) {
    return false;
  }

  AnimData *adt = ED_actedit_animdata_from_context(C, nullptr);
  if (!adt || !adt->action) {
    return false;
  }

  /* NOTE: We check this for the AnimData block in question and not the global flag,
   *       as the global flag may be left dirty by some of the browsing ops here.
   */
  return (adt->flag & ADT_NLA_EDIT_ON) == 0;
}

static wmOperatorStatus action_pushdown_exec(bContext *C, wmOperator * /*op*/)
{
  ID *adt_id_owner = nullptr;
  AnimData *adt = ED_actedit_animdata_from_context(C, &adt_id_owner);

  /* Do the deed... */
  if (adt && adt->action) {
    blender::animrig::Action &action = adt->action->wrap();

    /* action can be safely added */
    BKE_nla_action_pushdown({*adt_id_owner, *adt}, ID_IS_OVERRIDE_LIBRARY(adt_id_owner));

    Main *bmain = CTX_data_main(C);
    DEG_id_tag_update_ex(bmain, adt_id_owner, ID_RECALC_ANIMATION);

    /* The action needs updating too, as FCurve modifiers are to be reevaluated. They won't extend
     * beyond the NLA strip after pushing down to the NLA. */
    DEG_id_tag_update_ex(bmain, &action.id, ID_RECALC_ANIMATION);
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

static wmOperatorStatus action_stash_exec(bContext *C, wmOperator *op)
{
  ID *adt_id_owner = nullptr;
  AnimData *adt = ED_actedit_animdata_from_context(C, &adt_id_owner);

  /* Perform stashing operation */
  if (adt) {
    /* stash the action */
    if (!BKE_nla_action_stash({*adt_id_owner, *adt}, ID_IS_OVERRIDE_LIBRARY(adt_id_owner))) {
      /* action has already been added - simply warn about this, and clear */
      BKE_report(op->reports, RPT_ERROR, "Action+Slot has already been stashed");
    }

    if (!blender::animrig::unassign_action({*adt_id_owner, *adt})) {
      BKE_report(op->reports, RPT_ERROR, "Could not unassign the active Action");
    }
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
      /* There may not be any action/animdata yet, so, just fall back to the global setting
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

static wmOperatorStatus action_stash_create_exec(bContext *C, wmOperator *op)
{
  ID *adt_id_owner = nullptr;
  AnimData *adt = ED_actedit_animdata_from_context(C, &adt_id_owner);

  /* Check for no action... */
  if (adt->action == nullptr) {
    /* just create a new action */
    bAction *action = action_create_new(C, nullptr);
    if (!blender::animrig::assign_action(action, {*adt_id_owner, *adt})) {
      BKE_reportf(
          op->reports, RPT_ERROR, "Could not assign a new Action to %s", adt_id_owner->name + 2);
    }
  }
  else if (adt) {
    /* Perform stashing operation */
    if (BKE_nla_action_stash({*adt_id_owner, *adt}, ID_IS_OVERRIDE_LIBRARY(adt_id_owner))) {
      bAction *new_action = nullptr;

      /* Create new action not based on the old one
       * (since the "new" operator already does that). */
      new_action = action_create_new(C, nullptr);
      if (!blender::animrig::assign_action(new_action, {*adt_id_owner, *adt})) {
        BKE_reportf(
            op->reports, RPT_ERROR, "Could not assign a new Action to %s", adt_id_owner->name + 2);
      }
    }
    else {
      /* action has already been added - simply warn about this, and clear */
      BKE_report(op->reports, RPT_ERROR, "Action+Slot has already been stashed");
      if (!blender::animrig::unassign_action({*adt_id_owner, *adt})) {
        BKE_reportf(
            op->reports, RPT_ERROR, "Could not un-assign Action from %s", adt_id_owner->name + 2);
      }
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
  BLI_assert(id);

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
    BKE_nla_tweakmode_exit({*id, *adt});

    Scene *scene = CTX_data_scene(C);
    if (scene != nullptr) {
      scene->flag &= ~SCE_NLA_EDIT_ON;
    }
  }
  else {
    /* Clear AnimData -> action via RNA, so that it triggers message bus updates. */
    PointerRNA ptr = RNA_pointer_create_discrete(id, &RNA_AnimData, adt);
    PropertyRNA *prop = RNA_struct_find_property(&ptr, "action");

    RNA_property_pointer_set(&ptr, prop, PointerRNA_NULL, nullptr);
    RNA_property_update(C, &ptr, prop);
  }
}

/* -------------------------- */

static bool action_unlink_poll(bContext *C)
{
  ID *animated_id = nullptr;
  AnimData *adt = ED_actedit_animdata_from_context(C, &animated_id);
  if (!animated_id) {
    return false;
  }
  if (!BKE_id_is_editable(CTX_data_main(C), animated_id)) {
    return false;
  }
  return adt && adt->action;
}

static wmOperatorStatus action_unlink_exec(bContext *C, wmOperator *op)
{
  ID *animated_id = nullptr;
  AnimData *adt = ED_actedit_animdata_from_context(C, &animated_id);
  bool force_delete = RNA_boolean_get(op->ptr, "force_delete");

  if (adt && adt->action) {
    ED_animedit_unlink_action(C, animated_id, adt, adt->action, op->reports, force_delete);
  }

  /* Unlink is also abused to exit NLA tweak mode. */
  WM_main_add_notifier(NC_ANIMATION | ND_NLA_ACTCHANGE, nullptr);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus action_unlink_invoke(bContext *C, wmOperator *op, const wmEvent *event)
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
