/* SPDX-FileCopyrightText: 2009 Blender Authors, Joshua Leung. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spnla
 */

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"

#include "BKE_anim_data.h"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_layer.hh"
#include "BKE_nla.h"
#include "BKE_report.hh"

#include "ED_anim_api.hh"
#include "ED_keyframes_edit.hh"
#include "ED_object.hh"
#include "ED_screen.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "UI_view2d.hh"

#include "nla_intern.hh" /* own include */

/* *********************************************** */
/* Operators for NLA track-list which need to be different
 * from the standard Animation Editor ones */

/* ******************** Mouse-Click Operator *********************** */
/* Depending on the track that was clicked on, the mouse click will activate whichever
 * part of the track is relevant.
 *
 * NOTE: eventually,
 * this should probably be phased out when many of these things are replaced with buttons
 * --> Most tracks are now selection only.
 */

static int mouse_nla_tracks(bContext *C, bAnimContext *ac, int track_index, short selectmode)
{
  ListBase anim_data = {nullptr, nullptr};

  int notifierFlags = 0;

  /* get the track that was clicked on */
  /* filter tracks */
  eAnimFilter_Flags filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE |
                              ANIMFILTER_LIST_CHANNELS | ANIMFILTER_FCURVESONLY);
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, eAnimCont_Types(ac->datatype));

  /* get track from index */
  bAnimListElem *ale = static_cast<bAnimListElem *>(BLI_findlink(&anim_data, track_index));
  if (ale == nullptr) {
    /* track not found */
    if (G.debug & G_DEBUG) {
      printf("Error: animation track (index = %d) not found in mouse_nla_tracks()\n", track_index);
    }

    ANIM_animdata_freelist(&anim_data);
    return 0;
  }

  /* action to take depends on what track we've got */
  /* WARNING: must keep this in sync with the equivalent function in `anim_channels_edit.cc`. */
  switch (ale->type) {
    case ANIMTYPE_SCENE: {
      Scene *sce = static_cast<Scene *>(ale->data);
      AnimData *adt = sce->adt;

      /* set selection status */
      if (selectmode == SELECT_INVERT) {
        /* swap select */
        sce->flag ^= SCE_DS_SELECTED;
        if (adt) {
          adt->flag ^= ADT_UI_SELECTED;
        }
      }
      else {
        sce->flag |= SCE_DS_SELECTED;
        if (adt) {
          adt->flag |= ADT_UI_SELECTED;
        }
      }

      notifierFlags |= (ND_ANIMCHAN | NA_SELECTED);
      break;
    }
    case ANIMTYPE_OBJECT: {
      ViewLayer *view_layer = ac->view_layer;
      Base *base = static_cast<Base *>(ale->data);
      Object *ob = base->object;
      AnimData *adt = ob->adt;

      if (nlaedit_is_tweakmode_on(ac) == 0 && (base->flag & BASE_SELECTABLE)) {
        /* set selection status */
        if (selectmode == SELECT_INVERT) {
          /* swap select */
          ED_object_base_select(base, BA_INVERT);

          if (adt) {
            adt->flag ^= ADT_UI_SELECTED;
          }
        }
        else {
          /* deselect all */
          /* TODO: should this deselect all other types of tracks too? */
          BKE_view_layer_synced_ensure(ac->scene, view_layer);
          LISTBASE_FOREACH (Base *, b, BKE_view_layer_object_bases_get(view_layer)) {
            ED_object_base_select(b, BA_DESELECT);
            if (b->object->adt) {
              b->object->adt->flag &= ~(ADT_UI_SELECTED | ADT_UI_ACTIVE);
            }
          }

          /* select object now */
          ED_object_base_select(base, BA_SELECT);
          if (adt) {
            adt->flag |= ADT_UI_SELECTED;
          }
        }

        /* change active object - regardless of whether it is now selected [#37883] */
        ED_object_base_activate_with_mode_exit_if_needed(C, base); /* adds notifier */

        if ((adt) && (adt->flag & ADT_UI_SELECTED)) {
          adt->flag |= ADT_UI_ACTIVE;
        }

        /* notifiers - track was selected */
        notifierFlags |= (ND_ANIMCHAN | NA_SELECTED);
      }
      break;
    }
    case ANIMTYPE_FILLACTD: /* Action Expander */
    case ANIMTYPE_DSMAT:    /* Datablock AnimData Expanders */
    case ANIMTYPE_DSLAM:
    case ANIMTYPE_DSCAM:
    case ANIMTYPE_DSCACHEFILE:
    case ANIMTYPE_DSCUR:
    case ANIMTYPE_DSSKEY:
    case ANIMTYPE_DSWOR:
    case ANIMTYPE_DSNTREE:
    case ANIMTYPE_DSPART:
    case ANIMTYPE_DSMBALL:
    case ANIMTYPE_DSARM:
    case ANIMTYPE_DSMESH:
    case ANIMTYPE_DSTEX:
    case ANIMTYPE_DSLAT:
    case ANIMTYPE_DSLINESTYLE:
    case ANIMTYPE_DSSPK:
    case ANIMTYPE_DSGPENCIL:
    case ANIMTYPE_PALETTE:
    case ANIMTYPE_DSHAIR:
    case ANIMTYPE_DSPOINTCLOUD:
    case ANIMTYPE_DSVOLUME: {
      /* sanity checking... */
      if (ale->adt) {
        /* select/deselect */
        if (selectmode == SELECT_INVERT) {
          /* inverse selection status of this AnimData block only */
          ale->adt->flag ^= ADT_UI_SELECTED;
        }
        else {
          /* select AnimData block by itself */
          ANIM_anim_channels_select_set(ac, ACHANNEL_SETFLAG_CLEAR);
          ale->adt->flag |= ADT_UI_SELECTED;
        }

        /* set active? */
        if ((ale->adt) && (ale->adt->flag & ADT_UI_SELECTED)) {
          ale->adt->flag |= ADT_UI_ACTIVE;
        }
      }

      notifierFlags |= (ND_ANIMCHAN | NA_SELECTED);
      break;
    }
    case ANIMTYPE_NLATRACK: {
      NlaTrack *nlt = static_cast<NlaTrack *>(ale->data);

      if (nlaedit_is_tweakmode_on(ac) == 0) {
        /* set selection */
        if (selectmode == SELECT_INVERT) {
          /* inverse selection status of this F-Curve only */
          nlt->flag ^= NLATRACK_SELECTED;
        }
        else {
          /* select F-Curve by itself */
          ANIM_anim_channels_select_set(ac, ACHANNEL_SETFLAG_CLEAR);
          nlt->flag |= NLATRACK_SELECTED;
        }

        /* if NLA-Track is selected now,
         * make NLA-Track the 'active' one in the visible list */
        if (nlt->flag & NLATRACK_SELECTED) {
          ANIM_set_active_channel(
              ac, ac->data, eAnimCont_Types(ac->datatype), filter, nlt, ANIMTYPE_NLATRACK);
        }

        /* notifier flags - track was selected */
        notifierFlags |= (ND_ANIMCHAN | NA_SELECTED);
      }
      break;
    }
    case ANIMTYPE_NLAACTION: {
      AnimData *adt = BKE_animdata_from_id(ale->id);

      /* NOTE: rest of NLA-Action name doubles for operating on the AnimData block
       * - this is useful when there's no clear divider, and makes more sense in
       *   the case of users trying to use this to change actions
       * - in tweak-mode, clicking here gets us out of tweak-mode, as changing selection
       *   while in tweak-mode is really evil!
       * - we disable "solo" flags too, to make it easier to work with stashed actions
       *   with less trouble
       */
      if (nlaedit_is_tweakmode_on(ac)) {
        /* Exit tweak-mode immediately. */
        nlaedit_disable_tweakmode(ac, true);

        /* changes to NLA-Action occurred */
        notifierFlags |= ND_NLA_ACTCHANGE;
        ale->update |= ANIM_UPDATE_DEPS;
      }
      else {
        /* select/deselect */
        if (selectmode == SELECT_INVERT) {
          /* inverse selection status of this AnimData block only */
          adt->flag ^= ADT_UI_SELECTED;
        }
        else {
          /* select AnimData block by itself */
          ANIM_anim_channels_select_set(ac, ACHANNEL_SETFLAG_CLEAR);
          adt->flag |= ADT_UI_SELECTED;
        }

        /* set active? */
        if (adt->flag & ADT_UI_SELECTED) {
          adt->flag |= ADT_UI_ACTIVE;
        }

        notifierFlags |= (ND_ANIMCHAN | NA_SELECTED);
      }
      break;
    }
    default:
      if (G.debug & G_DEBUG) {
        printf("Error: Invalid track type in mouse_nla_tracks()\n");
      }
      break;
  }

  /* free tracks */
  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);

  /* return the notifier-flags set */
  return notifierFlags;
}

/* ------------------- */

/* handle clicking */
static int nlatracks_mouseclick_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  bAnimContext ac;
  ARegion *region;
  View2D *v2d;
  int track_index;
  int notifierFlags = 0;
  short selectmode;
  float x, y;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* get useful pointers from animation context data */
  SpaceNla *snla = reinterpret_cast<SpaceNla *>(ac.sl);
  region = ac.region;
  v2d = &region->v2d;

  /* select mode is either replace (deselect all, then add) or add/extend */
  if (RNA_boolean_get(op->ptr, "extend")) {
    selectmode = SELECT_INVERT;
  }
  else {
    selectmode = SELECT_REPLACE;
  }

  /* Figure out which track user clicked in. */
  UI_view2d_region_to_view(v2d, event->mval[0], event->mval[1], &x, &y);
  UI_view2d_listview_view_to_cell(NLATRACK_NAMEWIDTH,
                                  NLATRACK_STEP(snla),
                                  0,
                                  NLATRACK_FIRST_TOP(&ac),
                                  x,
                                  y,
                                  nullptr,
                                  &track_index);

  /* handle mouse-click in the relevant track then */
  notifierFlags = mouse_nla_tracks(C, &ac, track_index, selectmode);

  /* set notifier that things have changed */
  WM_event_add_notifier(C, NC_ANIMATION | notifierFlags, nullptr);

  return OPERATOR_FINISHED;
}

void NLA_OT_channels_click(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Mouse Click on NLA Tracks";
  ot->idname = "NLA_OT_channels_click";
  ot->description = "Handle clicks to select NLA tracks";

  /* api callbacks */
  ot->invoke = nlatracks_mouseclick_invoke;
  ot->poll = ED_operator_nla_active;

  /* flags */
  ot->flag = OPTYPE_UNDO;

  /* props */
  prop = RNA_def_boolean(ot->srna, "extend", false, "Extend Select", ""); /* SHIFTKEY */
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/* *********************************************** */
/* Special Operators */

/* ******************** Action Push Down ******************************** */

static int nlatracks_pushdown_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  ID *id = nullptr;
  AnimData *adt = nullptr;
  int track_index = RNA_int_get(op->ptr, "track_index");

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* get anim-channel to use (or more specifically, the animdata block behind it) */
  if (track_index == -1) {
    PointerRNA adt_ptr = {nullptr};

    /* active animdata block */
    if (nla_panel_context(C, &adt_ptr, nullptr, nullptr) == 0 || (adt_ptr.data == nullptr)) {
      BKE_report(op->reports,
                 RPT_ERROR,
                 "No active AnimData block to use "
                 "(select a data-block expander first or set the appropriate flags on an AnimData "
                 "block)");
      return OPERATOR_CANCELLED;
    }

    id = adt_ptr.owner_id;
    adt = static_cast<AnimData *>(adt_ptr.data);
  }
  else {
    /* indexed track */
    ListBase anim_data = {nullptr, nullptr};

    /* filter tracks */
    eAnimFilter_Flags filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE |
                                ANIMFILTER_LIST_CHANNELS | ANIMFILTER_FCURVESONLY);
    ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, eAnimCont_Types(ac.datatype));

    /* get track from index */
    bAnimListElem *ale = static_cast<bAnimListElem *>(BLI_findlink(&anim_data, track_index));
    if (ale == nullptr) {
      BKE_reportf(op->reports, RPT_ERROR, "No animation track found at index %d", track_index);
      ANIM_animdata_freelist(&anim_data);
      return OPERATOR_CANCELLED;
    }
    if (ale->type != ANIMTYPE_NLAACTION) {
      BKE_reportf(op->reports,
                  RPT_ERROR,
                  "Animation track at index %d is not a NLA 'Active Action' track",
                  track_index);
      ANIM_animdata_freelist(&anim_data);
      return OPERATOR_CANCELLED;
    }

    /* grab AnimData from the track */
    adt = ale->adt;
    id = ale->id;

    /* we don't need anything here anymore, so free it all */
    ANIM_animdata_freelist(&anim_data);
  }

  /* double-check that we are free to push down here... */
  if (adt == nullptr) {
    BKE_report(op->reports, RPT_WARNING, "Internal Error - AnimData block is not valid");
    return OPERATOR_CANCELLED;
  }
  if (nlaedit_is_tweakmode_on(&ac)) {
    BKE_report(op->reports,
               RPT_WARNING,
               "Cannot push down actions while tweaking a strip's action, exit tweak mode first");
    return OPERATOR_CANCELLED;
  }
  if (adt->action == nullptr) {
    BKE_report(op->reports, RPT_WARNING, "No active action to push down");
    return OPERATOR_CANCELLED;
  }

  /* 'push-down' action - only usable when not in Tweak-mode. */
  BKE_nla_action_pushdown(adt, ID_IS_OVERRIDE_LIBRARY(id));

  Main *bmain = CTX_data_main(C);
  DEG_id_tag_update_ex(bmain, id, ID_RECALC_ANIMATION);

  /* The action needs updating too, as FCurve modifiers are to be reevaluated. They won't extend
   * beyond the NLA strip after pushing down to the NLA. */
  DEG_id_tag_update_ex(bmain, &adt->action->id, ID_RECALC_ANIMATION);

  /* set notifier that things have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA_ACTCHANGE, nullptr);
  return OPERATOR_FINISHED;
}

void NLA_OT_action_pushdown(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Push Down Action";
  ot->idname = "NLA_OT_action_pushdown";
  ot->description = "Push action down onto the top of the NLA stack as a new strip";

  /* callbacks */
  ot->exec = nlatracks_pushdown_exec;
  ot->poll = nlaop_poll_tweakmode_off;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_int(ot->srna,
                         "track_index",
                         -1,
                         -1,
                         INT_MAX,
                         "Track Index",
                         "Index of NLA action track to perform pushdown operation on",
                         0,
                         INT_MAX);
  RNA_def_property_flag(ot->prop, PROP_SKIP_SAVE | PROP_HIDDEN);
}

/* ******************** Action Unlink ******************************** */

static bool nla_action_unlink_poll(bContext *C)
{
  if (ED_operator_nla_active(C)) {
    PointerRNA adt_ptr;
    return (nla_panel_context(C, &adt_ptr, nullptr, nullptr) && (adt_ptr.data != nullptr));
  }

  /* something failed... */
  return false;
}

static int nla_action_unlink_exec(bContext *C, wmOperator *op)
{
  PointerRNA adt_ptr;

  /* check context and also validity of pointer */
  if (!nla_panel_context(C, &adt_ptr, nullptr, nullptr)) {
    return OPERATOR_CANCELLED;
  }

  /* get animdata */
  AnimData *adt = static_cast<AnimData *>(adt_ptr.data);
  if (adt == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* do unlinking */
  if (adt->action) {
    bool force_delete = RNA_boolean_get(op->ptr, "force_delete");
    ED_animedit_unlink_action(C, adt_ptr.owner_id, adt, adt->action, op->reports, force_delete);
  }

  return OPERATOR_FINISHED;
}

static int nla_action_unlink_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  /* NOTE: this is hardcoded to match the behavior for the unlink button
   * (in `interface_templates.cc`). */
  RNA_boolean_set(op->ptr, "force_delete", event->modifier & KM_SHIFT);
  return nla_action_unlink_exec(C, op);
}

void NLA_OT_action_unlink(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Unlink Action";
  ot->idname = "NLA_OT_action_unlink";
  ot->description = "Unlink this action from the active action slot (and/or exit Tweak Mode)";

  /* callbacks */
  ot->invoke = nla_action_unlink_invoke;
  ot->exec = nla_action_unlink_exec;
  ot->poll = nla_action_unlink_poll;

  /* properties */
  prop = RNA_def_boolean(ot->srna,
                         "force_delete",
                         false,
                         "Force Delete",
                         "Clear Fake User and remove copy stashed in this data-block's NLA stack");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/* ******************** Add Tracks Operator ***************************** */
/* Add NLA Tracks to the same AnimData block as a selected track, or above the selected tracks */

bool nlaedit_add_tracks_existing(bAnimContext *ac, bool above_sel)
{
  ListBase anim_data = {nullptr, nullptr};
  AnimData *lastAdt = nullptr;
  bool added = false;

  /* get a list of the (selected) NLA Tracks being shown in the NLA */
  eAnimFilter_Flags filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_SEL |
                              ANIMFILTER_NODUPLIS | ANIMFILTER_FCURVESONLY);
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, eAnimCont_Types(ac->datatype));

  /* add tracks... */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    if (ale->type == ANIMTYPE_NLATRACK) {
      NlaTrack *nlt = static_cast<NlaTrack *>(ale->data);
      AnimData *adt = ale->adt;
      NlaTrack *new_track = nullptr;

      const bool is_liboverride = ID_IS_OVERRIDE_LIBRARY(ale->id);

      /* check if just adding a new track above this one,
       * or whether we're adding a new one to the top of the stack that this one belongs to
       */
      if (above_sel) {
        /* just add a new one above this one */
        new_track = BKE_nlatrack_new_after(&adt->nla_tracks, nlt, is_liboverride);
        BKE_nlatrack_set_active(&adt->nla_tracks, new_track);
        ale->update = ANIM_UPDATE_DEPS;
        added = true;
      }
      else if ((lastAdt == nullptr) || (adt != lastAdt)) {
        /* add one track to the top of the owning AnimData's stack,
         * then don't add anymore to this stack */
        new_track = BKE_nlatrack_new_tail(&adt->nla_tracks, is_liboverride);
        BKE_nlatrack_set_active(&adt->nla_tracks, new_track);
        lastAdt = adt;
        ale->update = ANIM_UPDATE_DEPS;
        added = true;
      }
    }
  }

  /* free temp data */
  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);

  return added;
}

bool nlaedit_add_tracks_empty(bAnimContext *ac)
{
  ListBase anim_data = {nullptr, nullptr};

  bool added = false;

  /* get a list of the selected AnimData blocks in the NLA */
  eAnimFilter_Flags filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE |
                              ANIMFILTER_ANIMDATA | ANIMFILTER_SEL | ANIMFILTER_NODUPLIS |
                              ANIMFILTER_FCURVESONLY);
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, eAnimCont_Types(ac->datatype));

  /* check if selected AnimData blocks are empty, and add tracks if so... */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    AnimData *adt = ale->adt;
    NlaTrack *new_track;

    /* sanity check */
    BLI_assert(adt->flag & ADT_UI_SELECTED);

    /* ensure it is empty */
    if (BLI_listbase_is_empty(&adt->nla_tracks)) {
      /* add new track to this AnimData block then */
      new_track = BKE_nlatrack_new_tail(&adt->nla_tracks, ID_IS_OVERRIDE_LIBRARY(ale->id));
      BKE_nlatrack_set_active(&adt->nla_tracks, new_track);
      ale->update = ANIM_UPDATE_DEPS;
      added = true;
    }
  }

  /* cleanup */
  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);

  return added;
}

/* ----- */

static int nlaedit_add_tracks_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  bool above_sel = RNA_boolean_get(op->ptr, "above_selected");
  bool op_done = false;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* perform adding in two passes - existing first so that we don't double up for empty */
  op_done |= nlaedit_add_tracks_existing(&ac, above_sel);
  op_done |= nlaedit_add_tracks_empty(&ac);

  /* done? */
  if (op_done) {
    DEG_relations_tag_update(CTX_data_main(C));

    /* set notifier that things have changed */
    WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_ADDED, nullptr);

    /* done */
    return OPERATOR_FINISHED;
  }

  /* failed to add any tracks */
  BKE_report(
      op->reports, RPT_WARNING, "Select an existing NLA Track or an empty action line first");

  /* not done */
  return OPERATOR_CANCELLED;
}

void NLA_OT_tracks_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Tracks";
  ot->idname = "NLA_OT_tracks_add";
  ot->description = "Add NLA-Tracks above/after the selected tracks";

  /* api callbacks */
  ot->exec = nlaedit_add_tracks_exec;
  ot->poll = nlaop_poll_tweakmode_off;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(ot->srna,
                  "above_selected",
                  false,
                  "Above Selected",
                  "Add a new NLA Track above every existing selected one");
}

/* ******************** Delete Tracks Operator ***************************** */
/* Delete selected NLA Tracks */

static int nlaedit_delete_tracks_exec(bContext *C, wmOperator * /*op*/)
{
  bAnimContext ac;

  ListBase anim_data = {nullptr, nullptr};

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* get a list of the AnimData blocks being shown in the NLA */
  eAnimFilter_Flags filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_SEL |
                              ANIMFILTER_NODUPLIS | ANIMFILTER_FCURVESONLY);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, eAnimCont_Types(ac.datatype));

  /* delete tracks */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    if (ale->type == ANIMTYPE_NLATRACK) {
      NlaTrack *nlt = static_cast<NlaTrack *>(ale->data);
      AnimData *adt = ale->adt;

      if (BKE_nlatrack_is_nonlocal_in_liboverride(ale->id, nlt)) {
        /* No deletion of non-local tracks of override data. */
        continue;
      }

      /* if track is currently 'solo', then AnimData should have its
       * 'has solo' flag disabled
       */
      if (nlt->flag & NLATRACK_SOLO) {
        adt->flag &= ~ADT_NLA_SOLO_TRACK;
      }

      /* call delete on this track - deletes all strips too */
      BKE_nlatrack_remove_and_free(&adt->nla_tracks, nlt, true);
      ale->update = ANIM_UPDATE_DEPS;
    }
  }

  /* free temp data */
  ANIM_animdata_update(&ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);

  DEG_relations_tag_update(ac.bmain);

  /* set notifier that things have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_REMOVED, nullptr);

  /* done */
  return OPERATOR_FINISHED;
}

void NLA_OT_tracks_delete(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete Tracks";
  ot->idname = "NLA_OT_tracks_delete";
  ot->description = "Delete selected NLA-Tracks and the strips they contain";

  /* api callbacks */
  ot->exec = nlaedit_delete_tracks_exec;
  ot->poll = nlaop_poll_tweakmode_off;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* *********************************************** */
/* AnimData Related Operators */

/* ******************** Include Objects Operator ***************************** */
/* Include selected objects in NLA Editor, by giving them AnimData blocks
 * NOTE: This doesn't help for non-object AnimData, where we do not have any effective
 *       selection mechanism in place. Unfortunately, this means that non-object AnimData
 *       once again becomes a second-class citizen here. However, at least for the most
 *       common use case, we now have a nice shortcut again.
 */

static int nlaedit_objects_add_exec(bContext *C, wmOperator * /*op*/)
{
  bAnimContext ac;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* ensure that filters are set so that the effect will be immediately visible */
  SpaceNla *snla = reinterpret_cast<SpaceNla *>(ac.sl);
  if (snla && snla->ads) {
    snla->ads->filterflag &= ~ADS_FILTER_NLA_NOACT;
  }

  /* operate on selected objects... */
  CTX_DATA_BEGIN (C, Object *, ob, selected_objects) {
    /* ensure that object has AnimData... that's all */
    BKE_animdata_ensure_id(&ob->id);
  }
  CTX_DATA_END;

  /* set notifier that things have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_EDITED, nullptr);

  /* done */
  return OPERATOR_FINISHED;
}

void NLA_OT_selected_objects_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Include Selected Objects";
  ot->idname = "NLA_OT_selected_objects_add";
  ot->description = "Make selected objects appear in NLA Editor by adding Animation Data";

  /* api callbacks */
  ot->exec = nlaedit_objects_add_exec;
  ot->poll = nlaop_poll_tweakmode_off;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* *********************************************** */
