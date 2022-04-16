/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2009 Blender Foundation, Joshua Leung. All rights reserved. */

/** \file
 * \ingroup spnla
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_action.h"
#include "BKE_animsys.h"
#include "BKE_context.h"
#include "BKE_fcurve.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_nla.h"
#include "BKE_report.h"
#include "BKE_screen.h"

#include "ED_anim_api.h"
#include "ED_keyframes_edit.h"
#include "ED_keyframing.h"
#include "ED_markers.h"
#include "ED_screen.h"
#include "ED_transform.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"
#include "RNA_prototypes.h"

#include "WM_api.h"
#include "WM_types.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "nla_intern.h"
#include "nla_private.h"

/* -------------------------------------------------------------------- */
/** \name Public Utilities
 * \{ */

void ED_nla_postop_refresh(bAnimContext *ac)
{
  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  short filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_ANIMDATA | ANIMFILTER_FOREDIT |
                  ANIMFILTER_FCURVESONLY);

  /* get blocks to work on */
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);

  for (ale = anim_data.first; ale; ale = ale->next) {
    /* performing auto-blending, extend-mode validation, etc. */
    BKE_nla_validate_state(ale->data);

    ale->update |= ANIM_UPDATE_DEPS;
  }

  /* free temp memory */
  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

/** \} */

/* 'Special' Editing */

/* 'Tweak mode' allows the action referenced by the active NLA-strip to be edited
 * as if it were the normal Active-Action of its AnimData block.
 */

/* -------------------------------------------------------------------- */
/** \name Enable Tweak-Mode Operator
 * \{ */

static int nlaedit_enable_tweakmode_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;

  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  const bool do_solo = RNA_boolean_get(op->ptr, "isolate_action");
  const bool use_upper_stack_evaluation = RNA_boolean_get(op->ptr, "use_upper_stack_evaluation");
  bool ok = false;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* get a list of the AnimData blocks being shown in the NLA */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_ANIMDATA | ANIMFILTER_FCURVESONLY);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* if no blocks, popup error? */
  if (BLI_listbase_is_empty(&anim_data)) {
    BKE_report(op->reports, RPT_ERROR, "No AnimData blocks to enter tweak mode for");
    return OPERATOR_CANCELLED;
  }

  /* for each AnimData block with NLA-data, try setting it in tweak-mode */
  for (ale = anim_data.first; ale; ale = ale->next) {
    AnimData *adt = ale->data;

    if (use_upper_stack_evaluation) {
      adt->flag |= ADT_NLA_EVAL_UPPER_TRACKS;
    }
    else {
      adt->flag &= ~ADT_NLA_EVAL_UPPER_TRACKS;
    }

    /* Try entering tweak-mode if valid. */
    ok |= BKE_nla_tweakmode_enter(adt);

    /* mark the active track as being "solo"? */
    if (do_solo && adt->actstrip) {
      NlaTrack *nlt = BKE_nlatrack_find_tweaked(adt);

      if (nlt && !(nlt->flag & NLATRACK_SOLO)) {
        BKE_nlatrack_solo_toggle(adt, nlt);
      }
    }

    ale->update |= ANIM_UPDATE_DEPS;
  }

  /* free temp data */
  ANIM_animdata_update(&ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);

  /* If we managed to enter tweak-mode on at least one AnimData block,
   * set the flag for this in the active scene and send notifiers. */
  if (ac.scene && ok) {
    /* set editing flag */
    ac.scene->flag |= SCE_NLA_EDIT_ON;

    /* set notifier that things have changed */
    WM_event_add_notifier(C, NC_ANIMATION | ND_NLA_ACTCHANGE, NULL);
  }
  else {
    BKE_report(op->reports, RPT_ERROR, "No active strip(s) to enter tweak mode on");
    return OPERATOR_CANCELLED;
  }

  /* done */
  return OPERATOR_FINISHED;
}

void NLA_OT_tweakmode_enter(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Enter Tweak Mode";
  ot->idname = "NLA_OT_tweakmode_enter";
  ot->description =
      "Enter tweaking mode for the action referenced by the active strip to edit its keyframes";

  /* api callbacks */
  ot->exec = nlaedit_enable_tweakmode_exec;
  ot->poll = nlaop_poll_tweakmode_off;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_boolean(ot->srna,
                         "isolate_action",
                         0,
                         "Isolate Action",
                         "Enable 'solo' on the NLA Track containing the active strip, "
                         "to edit it without seeing the effects of the NLA stack");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna,
                         "use_upper_stack_evaluation",
                         false,
                         "Evaluate Upper Stack",
                         "In tweak mode, display the effects of the tracks above the tweak strip");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Disable Tweak-Mode Operator
 * \{ */

bool nlaedit_disable_tweakmode(bAnimContext *ac, bool do_solo)
{
  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  /* get a list of the AnimData blocks being shown in the NLA */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_ANIMDATA | ANIMFILTER_FCURVESONLY);
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);

  /* if no blocks, popup error? */
  if (BLI_listbase_is_empty(&anim_data)) {
    BKE_report(ac->reports, RPT_ERROR, "No AnimData blocks in tweak mode to exit from");
    return false;
  }

  /* For each AnimData block with NLA-data, try exiting tweak-mode. */
  for (ale = anim_data.first; ale; ale = ale->next) {
    AnimData *adt = ale->data;

    /* clear solo flags */
    if ((do_solo) & (adt->flag & ADT_NLA_SOLO_TRACK) && (adt->flag & ADT_NLA_EDIT_ON)) {
      BKE_nlatrack_solo_toggle(adt, NULL);
    }

    /* To be sure that we're doing everything right, just exit tweak-mode. */
    BKE_nla_tweakmode_exit(adt);

    ale->update |= ANIM_UPDATE_DEPS;
  }

  /* free temp data */
  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);

  /* Clear the tweak-mode flag in the active scene and send notifiers. */
  if (ac->scene) {
    /* clear editing flag */
    ac->scene->flag &= ~SCE_NLA_EDIT_ON;

    /* set notifier that things have changed */
    WM_main_add_notifier(NC_ANIMATION | ND_NLA_ACTCHANGE, NULL);
  }

  /* done */
  return true;
}

/* Exit tweak-mode operator callback. */
static int nlaedit_disable_tweakmode_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;

  const bool do_solo = RNA_boolean_get(op->ptr, "isolate_action");
  bool ok = false;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* perform operation */
  ok = nlaedit_disable_tweakmode(&ac, do_solo);

  /* success? */
  if (ok) {
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void NLA_OT_tweakmode_exit(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Exit Tweak Mode";
  ot->idname = "NLA_OT_tweakmode_exit";
  ot->description = "Exit tweaking mode for the action referenced by the active strip";

  /* api callbacks */
  ot->exec = nlaedit_disable_tweakmode_exec;
  ot->poll = nlaop_poll_tweakmode_on;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_boolean(ot->srna,
                         "isolate_action",
                         0,
                         "Isolate Action",
                         "Disable 'solo' on any of the NLA Tracks after exiting tweak mode "
                         "to get things back to normal");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* NLA Strips Range Stuff */

/* -------------------------------------------------------------------- */
/** \name Calculate NLA Strip Range
 * \{ */

/* Get the min/max strip extents */
static void get_nlastrip_extents(bAnimContext *ac, float *min, float *max, const bool only_sel)
{
  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;
  bool found_bounds = false;

  /* get data to filter */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_NODUPLIS |
            ANIMFILTER_FCURVESONLY);
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);

  /* set large values to try to override */
  *min = 999999999.0f;
  *max = -999999999.0f;

  /* check if any channels to set range with */
  if (anim_data.first) {
    /* go through channels, finding max extents */
    for (ale = anim_data.first; ale; ale = ale->next) {
      NlaTrack *nlt = (NlaTrack *)ale->data;
      NlaStrip *strip;

      for (strip = nlt->strips.first; strip; strip = strip->next) {
        /* only consider selected strips? */
        if ((only_sel == false) || (strip->flag & NLASTRIP_FLAG_SELECT)) {
          /* extend range if appropriate */
          *min = min_ff(*min, strip->start);
          *max = max_ff(*max, strip->end);

          found_bounds = true;
        }
      }
    }

    /* free memory */
    ANIM_animdata_freelist(&anim_data);
  }

  /* set default range if nothing happened */
  if (found_bounds == false) {
    if (ac->scene) {
      *min = (float)ac->scene->r.sfra;
      *max = (float)ac->scene->r.efra;
    }
    else {
      *min = -5;
      *max = 100;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Automatic Preview-Range Operator
 * \{ */

static int nlaedit_previewrange_exec(bContext *C, wmOperator *UNUSED(op))
{
  bAnimContext ac;
  Scene *scene;
  float min, max;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  if (ac.scene == NULL) {
    return OPERATOR_CANCELLED;
  }

  scene = ac.scene;

  /* set the range directly */
  get_nlastrip_extents(&ac, &min, &max, true);
  scene->r.flag |= SCER_PRV_RANGE;
  scene->r.psfra = round_fl_to_int(min);
  scene->r.pefra = round_fl_to_int(max);

  /* set notifier that things have changed */
  /* XXX err... there's nothing for frame ranges yet, but this should do fine too */
  WM_event_add_notifier(C, NC_SCENE | ND_FRAME, ac.scene);

  return OPERATOR_FINISHED;
}

void NLA_OT_previewrange_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Preview Range to Selected";
  ot->idname = "NLA_OT_previewrange_set";
  ot->description = "Set Preview Range based on extends of selected strips";

  /* api callbacks */
  ot->exec = nlaedit_previewrange_exec;
  ot->poll = ED_operator_nla_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View-All Operator
 * \{ */

/**
 * Find the extents of the active channel
 *
 * \param r_min: Bottom y-extent of channel.
 * \param r_max: Top y-extent of channel.
 * \return Success of finding a selected channel.
 */
static bool nla_channels_get_selected_extents(bAnimContext *ac, float *r_min, float *r_max)
{
  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  SpaceNla *snla = (SpaceNla *)ac->sl;
  /* NOTE: not bool, since we want prioritize individual channels over expanders. */
  short found = 0;

  /* get all items - we need to do it this way */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_LIST_CHANNELS |
            ANIMFILTER_FCURVESONLY);
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);

  /* loop through all channels, finding the first one that's selected */
  float ymax = NLACHANNEL_FIRST_TOP(ac);

  for (ale = anim_data.first; ale; ale = ale->next, ymax -= NLACHANNEL_STEP(snla)) {
    const bAnimChannelType *acf = ANIM_channel_get_typeinfo(ale);

    /* must be selected... */
    if (acf && acf->has_setting(ac, ale, ACHANNEL_SETTING_SELECT) &&
        ANIM_channel_setting_get(ac, ale, ACHANNEL_SETTING_SELECT))
    {
      /* update best estimate */
      *r_min = ymax - NLACHANNEL_HEIGHT(snla);
      *r_max = ymax;

      /* is this high enough priority yet? */
      found = acf->channel_role;

      /* only stop our search when we've found an actual channel
       * - data-block expanders get less priority so that we don't abort prematurely
       */
      if (found == ACHANNEL_ROLE_CHANNEL) {
        break;
      }
    }
  }

  /* free all temp data */
  ANIM_animdata_freelist(&anim_data);

  return (found != 0);
}

static int nlaedit_viewall(bContext *C, const bool only_sel)
{
  bAnimContext ac;
  View2D *v2d;
  float extra;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }
  v2d = &ac.region->v2d;

  /* set the horizontal range, with an extra offset so that the extreme keys will be in view */
  get_nlastrip_extents(&ac, &v2d->cur.xmin, &v2d->cur.xmax, only_sel);

  extra = 0.1f * BLI_rctf_size_x(&v2d->cur);
  v2d->cur.xmin -= extra;
  v2d->cur.xmax += extra;

  /* set vertical range */
  if (only_sel == false) {
    /* view all -> the summary channel is usually the shows everything,
     * and resides right at the top... */
    v2d->cur.ymax = 0.0f;
    v2d->cur.ymin = (float)-BLI_rcti_size_y(&v2d->mask);
  }
  else {
    /* locate first selected channel (or the active one), and frame those */
    float ymin = v2d->cur.ymin;
    float ymax = v2d->cur.ymax;

    if (nla_channels_get_selected_extents(&ac, &ymin, &ymax)) {
      /* recenter the view so that this range is in the middle */
      float ymid = (ymax - ymin) / 2.0f + ymin;
      float x_center;

      UI_view2d_center_get(v2d, &x_center, NULL);
      UI_view2d_center_set(v2d, x_center, ymid);
    }
  }

  /* do View2D syncing */
  UI_view2d_sync(CTX_wm_screen(C), CTX_wm_area(C), v2d, V2D_LOCK_COPY);

  /* just redraw this view */
  ED_area_tag_redraw(CTX_wm_area(C));

  return OPERATOR_FINISHED;
}

/* ......... */

static int nlaedit_viewall_exec(bContext *C, wmOperator *UNUSED(op))
{
  /* whole range */
  return nlaedit_viewall(C, false);
}

static int nlaedit_viewsel_exec(bContext *C, wmOperator *UNUSED(op))
{
  /* only selected */
  return nlaedit_viewall(C, true);
}

void NLA_OT_view_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Frame All";
  ot->idname = "NLA_OT_view_all";
  ot->description = "Reset viewable area to show full strips range";

  /* api callbacks */
  ot->exec = nlaedit_viewall_exec;
  ot->poll = ED_operator_nla_active;

  /* flags */
  ot->flag = 0;
}

void NLA_OT_view_selected(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Frame Selected";
  ot->idname = "NLA_OT_view_selected";
  ot->description = "Reset viewable area to show selected strips range";

  /* api callbacks */
  ot->exec = nlaedit_viewsel_exec;
  ot->poll = ED_operator_nla_active;

  /* flags */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View-Frame Operator
 * \{ */

static int nlaedit_viewframe_exec(bContext *C, wmOperator *op)
{
  const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);
  ANIM_center_frame(C, smooth_viewtx);
  return OPERATOR_FINISHED;
}

void NLA_OT_view_frame(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Go to Current Frame";
  ot->idname = "NLA_OT_view_frame";
  ot->description = "Move the view to the current frame";

  /* api callbacks */
  ot->exec = nlaedit_viewframe_exec;
  ot->poll = ED_operator_nla_active;

  /* flags */
  ot->flag = 0;
}

/** \} */

/* NLA Editing Operations (Constructive/Destructive) */

/* -------------------------------------------------------------------- */
/** \name Add Action-Clip Operator
 *
 * Add a new Action-Clip strip to the active track
 * (or the active block if no space in the track).
 * \{ */

/* Get a list of the editable tracks being shown in the NLA. */
static int nlaedit_get_editable_tracks(bAnimContext *ac, ListBase *anim_data)
{
  const int filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_ACTIVE | ANIMFILTER_FOREDIT |
                      ANIMFILTER_FCURVESONLY);
  return ANIM_animdata_filter(ac, anim_data, filter, ac->data, ac->datatype);
}

static int nlaedit_add_actionclip_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  /* Get editor data. */
  bAnimContext ac;
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  ListBase anim_data = {NULL, NULL};
  const size_t items = nlaedit_get_editable_tracks(&ac, &anim_data);

  if (items == 0) {
    BKE_report(op->reports,
               RPT_ERROR,
               "No active track(s) to add strip to, select an existing track or add one before "
               "trying again");
    return OPERATOR_CANCELLED;
  }

  return WM_enum_search_invoke(C, op, event);
}

/* add the specified action as new strip */
static int nlaedit_add_actionclip_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  bAnimContext ac;
  Scene *scene;

  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;

  bAction *act;

  float cfra;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  scene = ac.scene;
  cfra = (float)scene->r.cfra;

  /* get action to use */
  act = BLI_findlink(&bmain->actions, RNA_enum_get(op->ptr, "action"));

  if (act == NULL) {
    BKE_report(op->reports, RPT_ERROR, "No valid action to add");
    // printf("Add strip - actname = '%s'\n", actname);
    return OPERATOR_CANCELLED;
  }
  if (act->idroot == 0) {
    /* hopefully in this case (i.e. library of userless actions),
     * the user knows what they're doing... */
    BKE_reportf(op->reports,
                RPT_WARNING,
                "Action '%s' does not specify what data-blocks it can be used on "
                "(try setting the 'ID Root Type' setting from the data-blocks editor "
                "for this action to avoid future problems)",
                act->id.name + 2);
  }

  /* add tracks to empty but selected animdata blocks so that strips can be added to those directly
   * without having to manually add tracks first
   */
  nlaedit_add_tracks_empty(&ac);

  nlaedit_get_editable_tracks(&ac, &anim_data);

  /* for every active track,
   * try to add strip to free space in track or to the top of the stack if no space */
  for (ale = anim_data.first; ale; ale = ale->next) {
    NlaTrack *nlt = (NlaTrack *)ale->data;
    AnimData *adt = ale->adt;
    NlaStrip *strip = NULL;
    const bool is_liboverride = ID_IS_OVERRIDE_LIBRARY(ale->id);

    /* Sanity check: only apply actions of the right type for this ID.
     * NOTE: in the case that this hasn't been set,
     * we've already warned the user about this already
     */
    if ((act->idroot) && (act->idroot != GS(ale->id->name))) {
      BKE_reportf(
          op->reports,
          RPT_ERROR,
          "Could not add action '%s' as it cannot be used relative to ID-blocks of type '%s'",
          act->id.name + 2,
          ale->id->name);
      continue;
    }

    /* create a new strip, and offset it to start on the current frame */
    strip = BKE_nlastrip_new(act);

    strip->end += (cfra - strip->start);
    strip->start = cfra;

    /* firstly try adding strip to our current track, but if that fails, add to a new track */
    if (BKE_nlatrack_add_strip(nlt, strip, is_liboverride) == 0) {
      /* trying to add to the current failed (no space),
       * so add a new track to the stack, and add to that...
       */
      nlt = BKE_nlatrack_new_tail(&adt->nla_tracks, is_liboverride);
      BKE_nlatrack_set_active(&adt->nla_tracks, nlt);
      BKE_nlatrack_add_strip(nlt, strip, is_liboverride);
    }

    /* auto-name it */
    BKE_nlastrip_validate_name(adt, strip);
  }

  /* free temp data */
  ANIM_animdata_freelist(&anim_data);

  /* refresh auto strip properties */
  ED_nla_postop_refresh(&ac);

  DEG_relations_tag_update(ac.bmain);

  /* set notifier that things have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_ADDED, NULL);

  /* done */
  return OPERATOR_FINISHED;
}

void NLA_OT_actionclip_add(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Add Action Strip";
  ot->idname = "NLA_OT_actionclip_add";
  ot->description =
      "Add an Action-Clip strip (i.e. an NLA Strip referencing an Action) to the active track";

  /* api callbacks */
  ot->invoke = nlaedit_add_actionclip_invoke;
  ot->exec = nlaedit_add_actionclip_exec;
  ot->poll = nlaop_poll_tweakmode_off;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  /* TODO: this would be nicer as an ID-pointer. */
  prop = RNA_def_enum(ot->srna, "action", DummyRNA_NULL_items, 0, "Action", "");
  RNA_def_enum_funcs(prop, RNA_action_itemf);
  RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
  ot->prop = prop;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Transition Operator
 *
 * Add a new transition strip between selected strips.
 * \{ */

static int nlaedit_add_transition_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;

  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  bool done = false;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* get a list of the editable tracks being shown in the NLA */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT |
            ANIMFILTER_FCURVESONLY);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* for each track, find pairs of strips to add transitions to */
  for (ale = anim_data.first; ale; ale = ale->next) {
    NlaTrack *nlt = (NlaTrack *)ale->data;
    AnimData *adt = ale->adt;
    NlaStrip *s1, *s2;

    /* get initial pair of strips */
    if (ELEM(nlt->strips.first, NULL, nlt->strips.last)) {
      continue;
    }
    s1 = nlt->strips.first;
    s2 = s1->next;

    /* loop over strips */
    for (; s1 && s2; s1 = s2, s2 = s2->next) {
      NlaStrip *strip;

      /* check if both are selected */
      if (ELEM(0, (s1->flag & NLASTRIP_FLAG_SELECT), (s2->flag & NLASTRIP_FLAG_SELECT))) {
        continue;
      }
      /* check if there's space between the two */
      if (IS_EQF(s1->end, s2->start)) {
        continue;
      }
      /* make sure neither one is a transition
       * - although this is impossible to create with the standard tools,
       *   the user may have altered the settings
       */
      if (ELEM(NLASTRIP_TYPE_TRANSITION, s1->type, s2->type)) {
        continue;
      }
      /* also make sure neither one is a soundclip */
      if (ELEM(NLASTRIP_TYPE_SOUND, s1->type, s2->type)) {
        continue;
      }

      /* allocate new strip */
      strip = MEM_callocN(sizeof(NlaStrip), "NlaStrip");
      BLI_insertlinkafter(&nlt->strips, s1, strip);

      /* set the type */
      strip->type = NLASTRIP_TYPE_TRANSITION;

      /* generic settings
       * - selected flag to highlight this to the user
       * - auto-blends to ensure that blend in/out values are automatically
       *   determined by overlaps of strips
       */
      strip->flag = NLASTRIP_FLAG_SELECT | NLASTRIP_FLAG_AUTO_BLENDS;

      /* range is simply defined as the endpoints of the adjacent strips */
      strip->start = s1->end;
      strip->end = s2->start;

      /* scale and repeat aren't of any use, but shouldn't ever be 0 */
      strip->scale = 1.0f;
      strip->repeat = 1.0f;

      /* auto-name it */
      BKE_nlastrip_validate_name(adt, strip);

      /* make note of this */
      done = true;
    }
  }

  /* free temp data */
  ANIM_animdata_freelist(&anim_data);

  /* was anything added? */
  if (done) {
    /* refresh auto strip properties */
    ED_nla_postop_refresh(&ac);

    /* set notifier that things have changed */
    WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_ADDED, NULL);

    /* done */
    return OPERATOR_FINISHED;
  }

  BKE_report(op->reports,
             RPT_ERROR,
             "Needs at least a pair of adjacent selected strips with a gap between them");
  return OPERATOR_CANCELLED;
}

void NLA_OT_transition_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Transition";
  ot->idname = "NLA_OT_transition_add";
  ot->description = "Add a transition strip between two adjacent selected strips";

  /* api callbacks */
  ot->exec = nlaedit_add_transition_exec;
  ot->poll = nlaop_poll_tweakmode_off;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Sound Clip Operator
 * \{ */

static int nlaedit_add_sound_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);
  bAnimContext ac;

  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  Scene *scene;
  int cfra;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  scene = ac.scene;
  cfra = scene->r.cfra;

  /* get a list of the editable tracks being shown in the NLA */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_SEL |
            ANIMFILTER_FOREDIT | ANIMFILTER_FCURVESONLY);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* for each track, add sound clips if it belongs to a speaker */
  /* TODO: what happens if there aren't any tracks,
   * well that's a more general problem for later. */
  for (ale = anim_data.first; ale; ale = ale->next) {
    Object *ob = (Object *)ale->id; /* may not be object until we actually check! */

    AnimData *adt = ale->adt;
    NlaTrack *nlt = (NlaTrack *)ale->data;
    NlaStrip *strip;
    const bool is_liboverride = ID_IS_OVERRIDE_LIBRARY(ale->id);

    /* does this belong to speaker - assumed to live on Object level only */
    if ((GS(ale->id->name) != ID_OB) || (ob->type != OB_SPEAKER)) {
      continue;
    }

    /* create a new strip, and offset it to start on the current frame */
    strip = BKE_nla_add_soundstrip(bmain, ac.scene, ob->data);

    strip->start += cfra;
    strip->end += cfra;

    /* firstly try adding strip to our current track, but if that fails, add to a new track */
    if (BKE_nlatrack_add_strip(nlt, strip, is_liboverride) == 0) {
      /* trying to add to the current failed (no space),
       * so add a new track to the stack, and add to that...
       */
      nlt = BKE_nlatrack_new_tail(&adt->nla_tracks, is_liboverride);
      BKE_nlatrack_set_active(&adt->nla_tracks, nlt);
      BKE_nlatrack_add_strip(nlt, strip, is_liboverride);
    }

    /* auto-name it */
    BKE_nlastrip_validate_name(adt, strip);
  }

  /* free temp data */
  ANIM_animdata_freelist(&anim_data);

  /* refresh auto strip properties */
  ED_nla_postop_refresh(&ac);

  /* set notifier that things have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_ADDED, NULL);

  /* done */
  return OPERATOR_FINISHED;
}

void NLA_OT_soundclip_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Sound Clip";
  ot->idname = "NLA_OT_soundclip_add";
  ot->description = "Add a strip for controlling when speaker plays its sound clip";

  /* api callbacks */
  ot->exec = nlaedit_add_sound_exec;
  ot->poll = nlaop_poll_tweakmode_off;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Meta-Strip Operator
 *
 * Add new meta-strips incorporating the selected strips.
 * \{ */

/* add the specified action as new strip */
static int nlaedit_add_meta_exec(bContext *C, wmOperator *UNUSED(op))
{
  bAnimContext ac;

  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* get a list of the editable tracks being shown in the NLA */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT |
            ANIMFILTER_FCURVESONLY);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* for each track, find pairs of strips to add transitions to */
  for (ale = anim_data.first; ale; ale = ale->next) {
    NlaTrack *nlt = (NlaTrack *)ale->data;
    AnimData *adt = ale->adt;
    NlaStrip *strip;

    if (BKE_nlatrack_is_nonlocal_in_liboverride(ale->id, nlt)) {
      /* No making meta-strips in non-local tracks of override data. */
      continue;
    }

    /* create meta-strips from the continuous chains of selected strips */
    BKE_nlastrips_make_metas(&nlt->strips, 0);

    /* name the metas */
    for (strip = nlt->strips.first; strip; strip = strip->next) {
      /* auto-name this strip if selected (that means it is a meta) */
      if (strip->flag & NLASTRIP_FLAG_SELECT) {
        BKE_nlastrip_validate_name(adt, strip);
      }
    }

    ale->update |= ANIM_UPDATE_DEPS;
  }

  /* free temp data */
  ANIM_animdata_update(&ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);

  /* set notifier that things have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_ADDED, NULL);

  /* done */
  return OPERATOR_FINISHED;
}

void NLA_OT_meta_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Meta-Strips";
  ot->idname = "NLA_OT_meta_add";
  ot->description = "Add new meta-strips incorporating the selected strips";

  /* api callbacks */
  ot->exec = nlaedit_add_meta_exec;
  ot->poll = nlaop_poll_tweakmode_off;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Remove Meta-Strip Operator
 *
 * Separate out the strips held by the selected meta-strips.
 * \{ */

static int nlaedit_remove_meta_exec(bContext *C, wmOperator *UNUSED(op))
{
  bAnimContext ac;

  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* get a list of the editable tracks being shown in the NLA */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT |
            ANIMFILTER_FCURVESONLY);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* for each track, find pairs of strips to add transitions to */
  for (ale = anim_data.first; ale; ale = ale->next) {
    NlaTrack *nlt = (NlaTrack *)ale->data;

    if (BKE_nlatrack_is_nonlocal_in_liboverride(ale->id, nlt)) {
      /* No removing meta-strips from non-local tracks of override data. */
      continue;
    }

    /* clear all selected meta-strips, regardless of whether they are temporary or not */
    BKE_nlastrips_clear_metas(&nlt->strips, 1, 0);

    ale->update |= ANIM_UPDATE_DEPS;
  }

  /* free temp data */
  ANIM_animdata_update(&ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);

  /* set notifier that things have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_REMOVED, NULL);

  /* done */
  return OPERATOR_FINISHED;
}

void NLA_OT_meta_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Meta-Strips";
  ot->idname = "NLA_OT_meta_remove";
  ot->description = "Separate out the strips held by the selected meta-strips";

  /* api callbacks */
  ot->exec = nlaedit_remove_meta_exec;
  ot->poll = nlaop_poll_tweakmode_off;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Duplicate Strips Operator
 *
 * Duplicates the selected NLA-Strips,
 * putting them on new tracks above the one the originals were housed in.
 * \{ */

static int nlaedit_duplicate_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;

  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  bool linked = RNA_boolean_get(op->ptr, "linked");
  bool done = false;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* get a list of editable tracks being shown in the NLA */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT |
            ANIMFILTER_FCURVESONLY);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* duplicate strips in tracks starting from the last one so that we're
   * less likely to duplicate strips we just duplicated...
   */
  for (ale = anim_data.last; ale; ale = ale->prev) {
    NlaTrack *nlt = (NlaTrack *)ale->data;
    AnimData *adt = ale->adt;
    NlaStrip *strip, *nstrip, *next;
    NlaTrack *track;

    /* NOTE: We allow this operator in override context because it is almost always (from possible
     * default user interactions) paired with the transform one, which will ensure that the new
     * strip ends up in a valid (local) track. */

    const bool is_liboverride = ID_IS_OVERRIDE_LIBRARY(ale->id);
    for (strip = nlt->strips.first; strip; strip = next) {
      next = strip->next;

      /* if selected, split the strip at its midpoint */
      if (strip->flag & NLASTRIP_FLAG_SELECT) {
        /* make a copy (assume that this is possible) */
        nstrip = BKE_nlastrip_copy(ac.bmain, strip, linked, 0);

        /* in case there's no space in the track above,
         * or we haven't got a reference to it yet, try adding */
        if (BKE_nlatrack_add_strip(nlt->next, nstrip, is_liboverride) == 0) {
          track = BKE_nlatrack_new_after(&adt->nla_tracks, nlt->next, is_liboverride);
          BKE_nlatrack_set_active(&adt->nla_tracks, track);
          BKE_nlatrack_add_strip(track, nstrip, is_liboverride);
        }

        /* deselect the original and the active flag */
        strip->flag &= ~(NLASTRIP_FLAG_SELECT | NLASTRIP_FLAG_ACTIVE);

        /* auto-name newly created strip */
        BKE_nlastrip_validate_name(adt, nstrip);

        done = true;
      }
    }
  }

  /* free temp data */
  ANIM_animdata_freelist(&anim_data);

  if (done) {
    /* refresh auto strip properties */
    ED_nla_postop_refresh(&ac);

    if (!linked) {
      DEG_relations_tag_update(ac.bmain);
    }

    /* set notifier that things have changed */
    WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_ADDED, NULL);

    /* done */
    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

static int nlaedit_duplicate_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  nlaedit_duplicate_exec(C, op);

  return OPERATOR_FINISHED;
}

void NLA_OT_duplicate(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Duplicate Strips";
  ot->idname = "NLA_OT_duplicate";
  ot->description =
      "Duplicate selected NLA-Strips, adding the new strips in new tracks above the originals";

  /* api callbacks */
  ot->invoke = nlaedit_duplicate_invoke;
  ot->exec = nlaedit_duplicate_exec;
  ot->poll = nlaop_poll_tweakmode_off;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* own properties */
  ot->prop = RNA_def_boolean(ot->srna,
                             "linked",
                             false,
                             "Linked",
                             "When duplicating strips, assign new copies of the actions they use");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Delete Strips Operator
 *
 * Deletes the selected NLA-Strips.
 * \{ */

static int nlaedit_delete_exec(bContext *C, wmOperator *UNUSED(op))
{
  bAnimContext ac;

  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* get a list of the editable tracks being shown in the NLA */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT |
            ANIMFILTER_FCURVESONLY);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* for each NLA-Track, delete all selected strips */
  for (ale = anim_data.first; ale; ale = ale->next) {
    NlaTrack *nlt = (NlaTrack *)ale->data;
    NlaStrip *strip, *nstrip;

    if (BKE_nlatrack_is_nonlocal_in_liboverride(ale->id, nlt)) {
      /* No deletion of strips in non-local tracks of override data. */
      continue;
    }

    for (strip = nlt->strips.first; strip; strip = nstrip) {
      nstrip = strip->next;

      /* if selected, delete */
      if (strip->flag & NLASTRIP_FLAG_SELECT) {
        /* Fix for #109430. Defensively exit tweak mode before deleting
         * the active strip. */
        if (ale->adt && ale->adt->actstrip == strip) {
          BKE_nla_tweakmode_exit(ale->adt);
        }

        /* if a strip either side of this was a transition, delete those too */
        if ((strip->prev) && (strip->prev->type == NLASTRIP_TYPE_TRANSITION)) {
          BKE_nlastrip_remove_and_free(&nlt->strips, strip->prev, true);
        }
        if ((nstrip) && (nstrip->type == NLASTRIP_TYPE_TRANSITION)) {
          nstrip = nstrip->next;
          BKE_nlastrip_remove_and_free(&nlt->strips, strip->next, true);
        }

        /* finally, delete this strip */
        BKE_nlastrip_remove_and_free(&nlt->strips, strip, true);
      }
    }
  }

  /* free temp data */
  ANIM_animdata_freelist(&anim_data);

  /* refresh auto strip properties */
  ED_nla_postop_refresh(&ac);

  DEG_relations_tag_update(ac.bmain);

  /* set notifier that things have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_REMOVED, NULL);

  /* done */
  return OPERATOR_FINISHED;
}

void NLA_OT_delete(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete Strips";
  ot->idname = "NLA_OT_delete";
  ot->description = "Delete selected strips";

  /* api callbacks */
  ot->exec = nlaedit_delete_exec;
  ot->poll = nlaop_poll_tweakmode_off;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Split Strips Operator
 *
 * Splits the selected NLA-Strips into two strips at the midpoint of the strip.
 *
 * TODO's?
 * - multiple splits
 * - variable-length splits?
 * \{ */

/** Split a given Action-Clip strip.
 * \returns newly created strip , which has been added after the original strip.
 */
static NlaStrip *nlaedit_split_strip_actclip(Main *bmain,
                                             AnimData *adt,
                                             NlaTrack *nlt,
                                             NlaStrip *strip,
                                             float cfra,
                                             const bool default_split_middle)
{
  NlaStrip *nstrip;
  float splitframe, splitaframe;

  /* calculate the frames to do the splitting at
   * - use current frame if within extents of strip
   */
  if ((cfra > strip->start) && (cfra < strip->end)) {
    /* use the current frame */
    splitframe = cfra;
    splitaframe = nlastrip_get_frame(strip, cfra, NLATIME_CONVERT_UNMAP);
  }
  else {
    if (!default_split_middle) {
      return NULL;
    }
    /* split in the middle */
    float len;

    /* strip extents */
    len = strip->end - strip->start;
    if (IS_EQF(len, 0.0f)) {
      return NULL;
    }

    splitframe = strip->start + (len / 2.0f);

    /* action range */
    len = strip->actend - strip->actstart;
    if (IS_EQF(len, 0.0f)) {
      splitaframe = strip->actend;
    }
    else {
      splitaframe = strip->actstart + (len / 2.0f);
    }
  }

  /* make a copy (assume that this is possible) and append
   * it immediately after the current strip
   */
  nstrip = BKE_nlastrip_copy(bmain, strip, true, 0);
  BLI_insertlinkafter(&nlt->strips, strip, nstrip);

  /* Set the endpoint of the first strip and the start of the new strip
   * to the split-frame values calculated above.
   */
  strip->end = splitframe;
  nstrip->start = splitframe;

  if ((splitaframe > strip->actstart) && (splitaframe < strip->actend)) {
    /* only do this if we're splitting down the middle... */
    strip->actend = splitaframe;
    nstrip->actstart = splitaframe;
  }

  /* Make sure Sync Length is off. With that setting on, entering and exiting tweak mode would
   * effectively undo the split, because both the old and the new strip will be at the length of
   * the Action again. */
  strip->flag &= ~NLASTRIP_FLAG_SYNC_LENGTH;
  nstrip->flag &= ~(NLASTRIP_FLAG_SYNC_LENGTH | NLASTRIP_FLAG_ACTIVE);

  /* auto-name the new strip */
  BKE_nlastrip_validate_name(adt, nstrip);
  return nstrip;
}

/* split a given Meta strip */
static NlaStrip *nlaedit_split_strip_meta(NlaTrack *nlt, NlaStrip *strip)
{
  /* simply ungroup it for now... */
  BKE_nlastrips_clear_metastrip(&nlt->strips, strip);
  return NULL;
}

static NlaStrip *nlaedit_split_strip(Main *bmain,
                                     AnimData *adt,
                                     NlaTrack *nlt,
                                     NlaStrip *strip,
                                     float cfra,
                                     const bool default_split_middle)
{
  switch (strip->type) {
    case NLASTRIP_TYPE_CLIP:
      return nlaedit_split_strip_actclip(bmain, adt, nlt, strip, cfra, default_split_middle);
      break;
    case NLASTRIP_TYPE_META: /* meta-strips need special handling */
      return nlaedit_split_strip_meta(nlt, strip);
      break;
    case NLASTRIP_TYPE_TRANSITION: /* for things like Transitions, do not split! */
      break;
  }
  return NULL;
}

static void nlaedit_split_strip_twice(Main *bmain,
                                      AnimData *adt,
                                      NlaTrack *nlt,
                                      NlaStrip *strip,
                                      float frame1,
                                      float frame2,
                                      NlaStrip **r_split_by_first,
                                      NlaStrip **r_split_by_second)
{
  *r_split_by_first = NULL;
  *r_split_by_second = NULL;

  if (frame1 > frame2) {
    SWAP(float, frame1, frame2);
  }

  *r_split_by_first = nlaedit_split_strip(bmain, adt, nlt, strip, frame1, false);
  if (*r_split_by_first) {
    *r_split_by_second = nlaedit_split_strip(bmain, adt, nlt, *r_split_by_first, frame2, false);
  }
  else {
    *r_split_by_second = nlaedit_split_strip(bmain, adt, nlt, strip, frame2, false);
  }
}

/* ----- */

static int nlaedit_split_exec(bContext *C, wmOperator *UNUSED(op))
{
  bAnimContext ac;

  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* get a list of editable tracks being shown in the NLA */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT |
            ANIMFILTER_FCURVESONLY);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* for each NLA-Track, split all selected strips into two strips */
  for (ale = anim_data.first; ale; ale = ale->next) {
    NlaTrack *nlt = (NlaTrack *)ale->data;
    AnimData *adt = ale->adt;
    NlaStrip *strip, *next;

    if (BKE_nlatrack_is_nonlocal_in_liboverride(ale->id, nlt)) {
      /* No splitting of strips in non-local tracks of override data. */
      continue;
    }

    for (strip = nlt->strips.first; strip; strip = next) {
      next = strip->next;

      /* if selected, split the strip at its midpoint */
      if (strip->flag & NLASTRIP_FLAG_SELECT) {
        nlaedit_split_strip(ac.bmain, adt, nlt, strip, (float)ac.scene->r.cfra, true);
      }
    }
  }

  /* free temp data */
  ANIM_animdata_freelist(&anim_data);

  /* refresh auto strip properties */
  ED_nla_postop_refresh(&ac);

  /* set notifier that things have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_ADDED, NULL);

  /* done */
  return OPERATOR_FINISHED;
}

void NLA_OT_split(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Split Strips";
  ot->idname = "NLA_OT_split";
  ot->description = "Split selected strips at their midpoints";

  /* api callbacks */
  ot->exec = nlaedit_split_exec;
  ot->poll = nlaop_poll_tweakmode_off;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Resample Strips Operator
 *
 * Resample the selected NLA-Strips into a single strip, preserving the overall NLA stack
 * animation.
 * \{ */

/* There is no proper support for changing blending parameters (blendmode, influence) while
 * also using a restricted frame range. Allowing it would lead to the problem where the dst_strip's
 * action has keys that work properly with one set of blending parms and other keys that work with
 * the old blending parms.
 *
 * To avoid that problem, we constrain the inputs. */
typedef void (*resample_strips_input_constraint)(NlaStrip *dst_strip,
                                                 float *r_start_frame,
                                                 float *r_end_frame,
                                                 short *r_new_blendmode,
                                                 float *r_new_influence);

/*
 * Iterates all visible animation datas and resamples selected strips to the active strip.
 * Each resampled strip is split by the frame range then the resampled section is muted.
 */
static int nlaedit_resample_strips_to_active_exec(
    bContext *C,
    wmOperator *op,
    const bool only_at_existing_keys,
    const float _start_frame,
    const float _end_frame,
    const short _new_blendmode,
    const float _new_influence,
    resample_strips_input_constraint constrain_inputs)
{
  /**GG:TODO: Design Q: check if this works even if resampled-to strip doens't span full frame
   * range and whether strip split-muting works as expected.
   *
   * sol 1: clamp resample range to size of active strip (simplest solution)
   * sol 2: extend strip size to size of reample frame bounds
   * sol 3: neither. Let user resize strip manually afterward.
   */
  BLI_assert(constrain_inputs);

  bAnimContext ac;
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  // TODO:GG: add UI supprot for key selection (replace with inserted, replace with replaced)
  const bool select_inserted_keys = true;
  const bool select_replaced_keys = true;

  /* Get a list of AnimDatas being shown in the NLA. */
  ListBase anim_data = {NULL, NULL};
  int filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_ANIMDATA);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  PointerRNA id_ptr;
  bool any_resample_succeeded = false;
  GSet *selected_strips = BLI_gset_ptr_new(__func__);

  float *sampled_frames;

  /* This tuple list used for muting and splitting after resampling. */
  ListBase selected_track_strip_tuples = {NULL, NULL};
  for (bAnimListElem *ale = anim_data.first; ale; ale = ale->next) {
    AnimData *adt = ale->adt;

    NlaTrack *dst_track = NULL;
    NlaStrip *dst_strip = NULL;

    /* Grab selected non-muted strips and dst data. */
    LISTBASE_FOREACH (NlaTrack *, nlt, &adt->nla_tracks) {
      if (!BKE_nlatrack_is_evaluatable(adt, nlt)) {
        continue;
      }

      if (BKE_nlatrack_is_nonlocal_in_liboverride(ale->id, nlt)) {
        /* No modifying of strips in non-local tracks of override data. */
        continue;
      }

      LISTBASE_FOREACH (NlaStrip *, strip, &nlt->strips) {
        if (strip->flag & NLASTRIP_FLAG_MUTED) {
          continue;
        }
        if ((strip->flag & NLASTRIP_FLAG_SELECT) == 0) {
          continue;
        }
        /** TODO: GG: ? Ensure strip is Action Strip? */
        if (strip->flag & NLASTRIP_FLAG_ACTIVE) {
          dst_track = nlt;
          dst_strip = strip;
          continue;
        }
        BLI_gset_insert(selected_strips, strip);

        BLI_addtail(&selected_track_strip_tuples, BLI_genericNodeN(nlt));
        BLI_addtail(&selected_track_strip_tuples, BLI_genericNodeN(strip));
      }
    }

    if (ELEM(NULL, dst_track, dst_strip, dst_strip->act)) {
      BLI_gset_clear(selected_strips, NULL);
      BLI_freelistN(&selected_track_strip_tuples);
      continue;
    }

    RNA_id_pointer_create(ale->id, &id_ptr);
    /* We always resample to the dst_strip's full strip bounds. If we allowed arbitrary start/end
     * frames and the blendmode or influence are changed, then some keyframes will only evaluate
     * properly with the new blendmode/influence and otherwise only with the old
     * blendmode/influence.
     *
     * The core resampling function will still work fine, but we, as the caller, would have to
     * deal with properly handling this situation, properly splitting the dst_strip, action
     * duplicating, tweakmode handling, etc. And after all that, if the resample is a noop, then
     * we'd have to clean all that up..
     *
     * It's simpler to always resample to the entire dst_strip. If the animator wants to resample
     * to a smaller scene frame range, then they can create whatever they need, a new track and
     * strip, with the proper size and location, etc. */

    float start_frame = _start_frame;
    float end_frame = _end_frame;
    short new_blendmode = _new_blendmode;
    float new_influence = _new_influence;
    /** TODO: GG: Why have this function ptr intead of placing the branch here directly? */
    constrain_inputs(dst_strip, &start_frame, &end_frame, &new_blendmode, &new_influence);

    if (start_frame > end_frame) {
      continue;
    }

    int total_frames = 0;
    if (only_at_existing_keys) {
      const int fcurve_array_len = BLI_listbase_count(&dst_strip->act->curves);
      FCurve **fcurve_array = MEM_mallocN(sizeof(FCurve *) * fcurve_array_len, __func__);
      int fcurve_index;
      LISTBASE_FOREACH_INDEX (FCurve *, fcurve, &dst_strip->act->curves, fcurve_index) {
        fcurve_array[fcurve_index] = fcurve;
      }

      sampled_frames = BKE_fcurves_calc_keyed_frames(
          fcurve_array, fcurve_array_len, &total_frames);

      for (int i = 0; i < total_frames; i++) {
        sampled_frames[i] = nlastrip_get_frame(dst_strip, sampled_frames[i], NLATIME_CONVERT_MAP);
      }

      MEM_freeN(fcurve_array);
    }
    else {
      total_frames = (end_frame - start_frame + 1);
      sampled_frames = MEM_mallocN(sizeof(float) * total_frames, __func__);

      for (int i = 0; i < total_frames; i++) {
        sampled_frames[i] = start_frame + i;
      }
    }

    NlaTrack acttrack_track = {0};
    NlaStrip acttrack_strip = {0};
    BKE_animsys_create_action_track_strip(adt, false, &acttrack_strip);
    BLI_addtail(&acttrack_track.strips, &acttrack_strip);

    /** GG: TODO: XXX:  User does not ahve abilility to specify influence w/o it also being an
     * fcurve key. Thus, this is inconsistent or blender should allow editing influence w/o the
     * need for it to be animated. Also need to support autokeiyng canged influence when fcurve
     * exists.
     *
     * GG: TODO: add support for smart bake (only resample where keys exist).
     * This can be done though keyframe insertion func ptr, to have a version that
     * only replaces keys. This is done at key insertion level instead of resample level
     * since resampling has a frequency ("level of detail?") of bake-per-given frame,
     * not bake-per-channel's key. So the key insertion-replace-only prevents inserting
     * the additional keys.
     *  - this can be a separate patch as its not a core feature of NLA merge.
     */
    const bool resample_succeeded = BKE_nla_resample_strips(
        ac.depsgraph,
        &id_ptr,
        adt,
        &acttrack_track,
        &acttrack_strip,
        sampled_frames,
        total_frames,
        selected_strips,
        new_blendmode,
        new_influence,
        dst_track,
        dst_strip,
        ED_cb_insert_keyframes_slow,  // TODO: use ED_cb_insert_keyframes_fast_subdivide,
        select_inserted_keys,
        select_replaced_keys);

    MEM_freeN(sampled_frames);

    if (!resample_succeeded) {
      BLI_gset_clear(selected_strips, NULL);
      BLI_freelistN(&selected_track_strip_tuples);
      continue;
    }
    any_resample_succeeded = true;

    for (FCurve *fcurve = dst_strip->act->curves.first; fcurve; fcurve = fcurve->next) {
      BKE_fcurve_handles_recalc(fcurve);
    }

    /* Mute strips that have been resampled. If resample bounds intersects strip, then we need to
     * split it instead so the animation outside of the resample remains unmuted. We split at
     * an -1 offset from the start_frame since resampling assumes the resampled strips are not
     * evaluated at all afterward. Strip bounds are inclusive for evaluation. */
    LISTBASE_FOREACH (LinkData *, link, &selected_track_strip_tuples) {
      NlaTrack *nlt = link->data;

      link = link->next;
      NlaStrip *first = link->data;

      NlaStrip *split_by_first = NULL, *split_by_second = NULL;
      nlaedit_split_strip_twice(ac.bmain,
                                adt,
                                nlt,
                                first,
                                start_frame - 1,
                                end_frame,
                                &split_by_first,
                                &split_by_second);
      if (split_by_first) {
        split_by_first->flag |= NLASTRIP_FLAG_MUTED;
      }
      else if (split_by_second) {
        first->flag |= NLASTRIP_FLAG_MUTED;
      }
      else if (BKE_nlastrip_within_bounds(first, start_frame, end_frame)) {
        first->flag |= NLASTRIP_FLAG_MUTED;
      }
    }

    BLI_gset_clear(selected_strips, NULL);
    BLI_freelistN(&selected_track_strip_tuples);

    DEG_id_tag_update(&dst_strip->act->id, ID_RECALC_ANIMATION);
  }

  BLI_gset_free(selected_strips, NULL);

  /* Free temp data. */
  ANIM_animdata_freelist(&anim_data);

  if (!any_resample_succeeded) {
    /* Avoid pushing undo. */
    return OPERATOR_CANCELLED;
  }

  /* Refresh auto strip properties. */
  ED_nla_postop_refresh(&ac);

  /* New f-curves were added, meaning it's possible that it affects dependency graph components
   * which weren't previously animated. */
  DEG_relations_tag_update(ac.bmain);

  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

static void resample_constraint_use_strip_frame_range(NlaStrip *dst_strip,
                                                      float *r_start_frame,
                                                      float *r_end_frame,
                                                      short *r_new_blendmode,
                                                      float *r_new_influence)
{
  *r_start_frame = dst_strip->start;
  *r_end_frame = dst_strip->end;

  if (dst_strip->repeat > 1.0f) {
    /* Clamp r_end_frame to first segment's end, since overwriting repeated sections overwrites all
     * sections.
     *
     * Potential Improvement: Maybe we should let the animator select which segment to use for the
     * bounds. */
    const float first_repeat_amount = clamp_f(dst_strip->repeat, 0, 1);
    const float strip_full_segment_size = (dst_strip->actend - dst_strip->actstart) *
                                          dst_strip->scale;
    *r_end_frame = dst_strip->start + strip_full_segment_size * first_repeat_amount;
  }
}

static int nlaedit_resample_strips_to_active_new_blend_parms_exec(bContext *C, wmOperator *op)
{
  return nlaedit_resample_strips_to_active_exec(C,
                                                op,
                                                RNA_boolean_get(op->ptr, "at_existing_keys"),
                                                0,
                                                0,
                                                RNA_enum_get(op->ptr, "new_blendmode"),
                                                RNA_float_get(op->ptr, "new_influence"),
                                                resample_constraint_use_strip_frame_range);
}

/* Set the defaults based on the active strip. */
static int nlaedit_resample_strips_to_active_new_blend_parms_invoke(bContext *C,
                                                                    wmOperator *op,
                                                                    const wmEvent *event)
{
  bAnimContext ac;
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* Get a list of AnimDatas being shown in the NLA. */
  ListBase anim_data = {NULL, NULL};
  int filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_ANIMDATA);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  NlaStrip *active_strip = NULL;
  for (bAnimListElem *ale = anim_data.first; ale; ale = ale->next) {
    AnimData *adt = ale->adt;

    /* Grab selected non-muted strips*/
    LISTBASE_FOREACH (NlaTrack *, nlt, &adt->nla_tracks) {
      if (!BKE_nlatrack_is_evaluatable(adt, nlt)) {
        continue;
      }

      LISTBASE_FOREACH (NlaStrip *, strip, &nlt->strips) {
        if (strip->flag & NLASTRIP_FLAG_ACTIVE) {
          active_strip = strip;
          goto post_search;
        }
      }
    }
  }

  if (active_strip == NULL) {
    ANIM_animdata_freelist(&anim_data);
    BKE_reportf(op->reports,
                RPT_ERROR,
                "Operator '%s' requires an active strip to resample to",
                op->type->idname);
    return OPERATOR_CANCELLED;
  }

post_search:  // GG: TODO:...instead of goto, why not just make loop a function with return for
              // early out...
  // GG:TODO: and all it does it get the active strip.. im sure thneres an existing function for
  // this..

  ANIM_animdata_freelist(&anim_data);

  RNA_enum_set(op->ptr, "new_blendmode", active_strip->blendmode);
  RNA_float_set(op->ptr, "new_influence", active_strip->influence);

  return WM_operator_props_popup_confirm(C, op, event);
}

void NLA_OT_resample_strips_to_active_new_blend_parms(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Resample Strips To Active (Convert)";
  ot->idname = "NLA_OT_resample_strips_to_active";
  ot->description =
      "Resample selected strips into active strip, with the specified blend mode and influence";

  /* api callbacks */

  ot->invoke = nlaedit_resample_strips_to_active_new_blend_parms_invoke;
  ot->exec = nlaedit_resample_strips_to_active_new_blend_parms_exec;
  ot->poll = ED_operator_nla_active;

  /* own properties */
  PropertyRNA *prop;

  prop = RNA_def_boolean(ot->srna,
                         "at_existing_keys",
                         false,
                         "Only At Existing Keys",
                         "Only resample all fcurves at the summary key times");

  prop = RNA_def_enum(ot->srna,
                      "new_blendmode",
                      rna_enum_nla_mode_blend_items,
                      NLASTRIP_MODE_REPLACE,
                      "New Blend Mode",
                      "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_float_factor(ot->srna, "new_influence", 1, 0, 1, "New Influence", "", 0, 1);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static void resample_constraint_use_strip_blend_parms(NlaStrip *dst_strip,
                                                      float *r_start_frame,
                                                      float *r_end_frame,
                                                      short *r_new_blendmode,
                                                      float *r_new_influence)
{
  *r_new_blendmode = dst_strip->blendmode;
  *r_new_influence = dst_strip->influence;
}

static int nlaedit_resample_strips_to_active_limited_range_exec(bContext *C, wmOperator *op)
{
  return nlaedit_resample_strips_to_active_exec(C,
                                                op,
                                                RNA_boolean_get(op->ptr, "at_existing_keys"),
                                                RNA_int_get(op->ptr, "start_frame"),
                                                RNA_int_get(op->ptr, "end_frame"),
                                                0,
                                                0,
                                                resample_constraint_use_strip_blend_parms);
}

void NLA_OT_resample_strips_to_active_limited_range(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Resample Strips To Active (Range)";
  ot->idname = "NLA_OT_resample_strips_to_active_limited_range";
  ot->description = "Resample selected strips into active strip, limited to a frame range";

  /* api callbacks */
  ot->invoke = WM_operator_props_popup_confirm;
  ot->exec = nlaedit_resample_strips_to_active_limited_range_exec;
  ot->poll = ED_operator_nla_active;

  /* own properties */
  PropertyRNA *prop;

  prop = RNA_def_boolean(ot->srna,
                         "at_existing_keys",
                         false,
                         "Only At Existing Keys",
                         "Only resample all fcurves at the summary key times");

  RNA_def_int(ot->srna,
              "start_frame",
              1,
              MINAFRAME,
              MAXFRAME,
              "Start",
              "First frame to calculate bone paths on",
              MINFRAME,
              MAXFRAME / 2.0);

  RNA_def_int(ot->srna,
              "end_frame",
              250,
              MINAFRAME,
              MAXFRAME,
              "End",
              "Last frame to calculate bone paths on",
              MINFRAME,
              MAXFRAME / 2.0);

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* NLA Editing Operations (Modifying) */

/* -------------------------------------------------------------------- */
/** \name Toggle Muting Operator
 *
 * Toggles whether strips are muted or not.
 * \{ */

static int nlaedit_toggle_mute_exec(bContext *C, wmOperator *UNUSED(op))
{
  bAnimContext ac;

  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* get a list of the editable tracks being shown in the NLA */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT |
            ANIMFILTER_FCURVESONLY);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* go over all selected strips */
  for (ale = anim_data.first; ale; ale = ale->next) {
    NlaTrack *nlt = (NlaTrack *)ale->data;
    NlaStrip *strip;

    /* For every selected strip, toggle muting. */
    for (strip = nlt->strips.first; strip; strip = strip->next) {
      if (strip->flag & NLASTRIP_FLAG_SELECT) {
        /* just flip the mute flag for now */
        /* TODO: have a pre-pass to check if mute all or unmute all? */
        strip->flag ^= NLASTRIP_FLAG_MUTED;

        /* tag AnimData to get recalculated */
        ale->update |= ANIM_UPDATE_DEPS;
      }
    }
  }

  /* cleanup */
  ANIM_animdata_update(&ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);

  /* set notifier that things have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_EDITED, NULL);

  /* done */
  return OPERATOR_FINISHED;
}

void NLA_OT_mute_toggle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Toggle Muting";
  ot->idname = "NLA_OT_mute_toggle";
  ot->description = "Mute or un-mute selected strips";

  /* api callbacks */
  ot->exec = nlaedit_toggle_mute_exec;
  ot->poll = nlaop_poll_tweakmode_off;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Swap Strips Operator
 *
 * Tries to exchange strips within their owner tracks.
 * \{ */

static int nlaedit_swap_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;

  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* get a list of the editable tracks being shown in the NLA */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT |
            ANIMFILTER_FCURVESONLY);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* consider each track in turn */
  for (ale = anim_data.first; ale; ale = ale->next) {
    NlaTrack *nlt = (NlaTrack *)ale->data;

    NlaStrip *strip, *stripN = NULL;
    NlaStrip *area = NULL, *sb = NULL;
    const bool is_liboverride = ID_IS_OVERRIDE_LIBRARY(ale->id);

    if (BKE_nlatrack_is_nonlocal_in_liboverride(ale->id, nlt)) {
      /* No re-ordering of strips within non-local tracks of override data. */
      continue;
    }

    /* Make temporary meta-strips so that entire islands of selections can be moved around. */
    BKE_nlastrips_make_metas(&nlt->strips, 1);

    /* special case: if there is only 1 island
     * (i.e. temp meta BUT NOT unselected/normal/normal-meta strips) left after this,
     * and this island has two strips inside it, then we should be able to just swap these still...
     */
    if (BLI_listbase_is_empty(&nlt->strips) == false) {
      NlaStrip *mstrip = (NlaStrip *)nlt->strips.first;

      if ((mstrip->flag & NLASTRIP_FLAG_TEMP_META) &&
          (BLI_listbase_count_at_most(&mstrip->strips, 3) == 2))
      {
        /* remove this temp meta, so that we can see the strips inside */
        BKE_nlastrips_clear_metas(&nlt->strips, 0, 1);
      }
    }

    /* get two selected strips only (these will be metas due to prev step) to operate on
     * - only allow swapping 2, as with more the context becomes unclear
     */
    for (strip = nlt->strips.first; strip; strip = stripN) {
      stripN = strip->next;

      if (strip->flag & NLASTRIP_FLAG_SELECT) {
        /* first or second strip? */
        if (area == NULL) {
          /* store as first */
          area = strip;
        }
        else if (sb == NULL) {
          /* store as second */
          sb = strip;
        }
        else {
          /* too many selected */
          break;
        }
      }
    }

    if (strip) {
      /* too many selected warning */
      BKE_reportf(
          op->reports,
          RPT_WARNING,
          "Too many clusters of strips selected in NLA Track (%s): needs exactly 2 to be selected",
          nlt->name);
    }
    else if (area == NULL) {
      /* no warning as this is just a common case,
       * and it may get annoying when doing multiple tracks */
    }
    else if (sb == NULL) {
      /* too few selected warning */
      BKE_reportf(
          op->reports,
          RPT_WARNING,
          "Too few clusters of strips selected in NLA Track (%s): needs exactly 2 to be selected",
          nlt->name);
    }
    else {
      float nsa[2], nsb[2];

      /* remove these strips from the track,
       * so that we can test if they can fit in the proposed places */
      BLI_remlink(&nlt->strips, area);
      BLI_remlink(&nlt->strips, sb);

      /* calculate new extents for strips */
      /* a --> b */
      nsa[0] = sb->start;
      nsa[1] = sb->start + (area->end - area->start);
      /* b --> a */
      nsb[0] = area->start;
      nsb[1] = area->start + (sb->end - sb->start);

      /* check if the track has room for the strips to be swapped */
      if (BKE_nlastrips_has_space(&nlt->strips, nsa[0], nsa[1]) &&
          BKE_nlastrips_has_space(&nlt->strips, nsb[0], nsb[1]) && (nsb[1] <= nsa[0]))
      {
        /* set new extents for strips then */
        area->start = nsa[0];
        area->end = nsa[1];
        BKE_nlameta_flush_transforms(area);

        sb->start = nsb[0];
        sb->end = nsb[1];
        BKE_nlameta_flush_transforms(sb);
      }
      else {
        /* not enough room to swap, so show message */
        if (nsb[1] > nsa[0]) {
          BKE_report(op->reports,
                     RPT_WARNING,
                     "Cannot swap selected strips because they will overlap each other in their "
                     "new places");
        }
        else if ((area->flag & NLASTRIP_FLAG_TEMP_META) || (sb->flag & NLASTRIP_FLAG_TEMP_META)) {
          BKE_report(
              op->reports,
              RPT_WARNING,
              "Cannot swap selected strips as they will not be able to fit in their new places");
        }
        else {
          BKE_reportf(op->reports,
                      RPT_WARNING,
                      "Cannot swap '%s' and '%s' as one or both will not be able to fit in their "
                      "new places",
                      area->name,
                      sb->name);
        }
      }

      /* add strips back to track now */
      BKE_nlatrack_add_strip(nlt, area, is_liboverride);
      BKE_nlatrack_add_strip(nlt, sb, is_liboverride);
    }

    /* Clear (temp) meta-strips. */
    BKE_nlastrips_clear_metas(&nlt->strips, 0, 1);
  }

  /* free temp data */
  ANIM_animdata_freelist(&anim_data);

  /* refresh auto strip properties */
  ED_nla_postop_refresh(&ac);

  /* set notifier that things have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_EDITED, NULL);
  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA_ORDER, NULL);

  /* done */
  return OPERATOR_FINISHED;
}

void NLA_OT_swap(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Swap Strips";
  ot->idname = "NLA_OT_swap";
  ot->description = "Swap order of selected strips within tracks";

  /* api callbacks */
  ot->exec = nlaedit_swap_exec;
  ot->poll = nlaop_poll_tweakmode_off;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Move Strips Up Operator
 *
 * Tries to move the selected strips into the track above if possible.
 * \{ */

static int nlaedit_move_up_exec(bContext *C, wmOperator *UNUSED(op))
{
  bAnimContext ac;

  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* get a list of the editable tracks being shown in the NLA */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT |
            ANIMFILTER_FCURVESONLY);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* since we're potentially moving strips from lower tracks to higher tracks, we should
   * loop over the tracks in reverse order to avoid moving earlier strips up multiple tracks
   */
  for (ale = anim_data.last; ale; ale = ale->prev) {
    NlaTrack *nlt = (NlaTrack *)ale->data;
    NlaTrack *nltn = nlt->next;
    NlaStrip *strip, *stripn;

    const bool is_liboverride = ID_IS_OVERRIDE_LIBRARY(ale->id);

    /* if this track has no tracks after it, skip for now... */
    if (nltn == NULL) {
      continue;
    }

    if (BKE_nlatrack_is_nonlocal_in_liboverride(ale->id, nlt) ||
        BKE_nlatrack_is_nonlocal_in_liboverride(ale->id, nltn))
    {
      /* No moving of strips in non-local tracks of override data. */
      continue;
    }

    /* for every selected strip, try to move */
    for (strip = nlt->strips.first; strip; strip = stripn) {
      stripn = strip->next;

      if (strip->flag & NLASTRIP_FLAG_SELECT) {
        /* check if the track above has room for this strip */
        if (BKE_nlatrack_has_space(nltn, strip->start, strip->end)) {
          /* remove from its current track, and add to the one above
           * (it 'should' work, so no need to worry) */
          BKE_nlatrack_remove_strip(nlt, strip);
          BKE_nlatrack_add_strip(nltn, strip, is_liboverride);
        }
      }
    }
  }

  /* free temp data */
  ANIM_animdata_freelist(&anim_data);

  /* refresh auto strip properties */
  ED_nla_postop_refresh(&ac);

  /* set notifier that things have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_EDITED, NULL);
  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA_ORDER, NULL);

  /* done */
  return OPERATOR_FINISHED;
}

void NLA_OT_move_up(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Move Strips Up";
  ot->idname = "NLA_OT_move_up";
  ot->description = "Move selected strips up a track if there's room";

  /* api callbacks */
  ot->exec = nlaedit_move_up_exec;
  ot->poll = nlaop_poll_tweakmode_off;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Move Strips Down Operator
 *
 * Tries to move the selected strips into the track above if possible.
 * \{ */

static int nlaedit_move_down_exec(bContext *C, wmOperator *UNUSED(op))
{
  bAnimContext ac;

  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* get a list of the editable tracks being shown in the NLA */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT |
            ANIMFILTER_FCURVESONLY);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* loop through the tracks in normal order, since we're pushing strips down,
   * strips won't get operated on twice
   */
  for (ale = anim_data.first; ale; ale = ale->next) {
    NlaTrack *nlt = (NlaTrack *)ale->data;
    NlaTrack *nltp = nlt->prev;
    NlaStrip *strip, *stripn;

    const bool is_liboverride = ID_IS_OVERRIDE_LIBRARY(ale->id);

    /* if this track has no tracks before it, skip for now... */
    if (nltp == NULL) {
      continue;
    }

    if (BKE_nlatrack_is_nonlocal_in_liboverride(ale->id, nlt) ||
        BKE_nlatrack_is_nonlocal_in_liboverride(ale->id, nltp))
    {
      /* No moving of strips in non-local tracks of override data. */
      continue;
    }

    /* for every selected strip, try to move */
    for (strip = nlt->strips.first; strip; strip = stripn) {
      stripn = strip->next;

      if (strip->flag & NLASTRIP_FLAG_SELECT) {
        /* check if the track below has room for this strip */
        if (BKE_nlatrack_has_space(nltp, strip->start, strip->end)) {
          /* remove from its current track, and add to the one above
           * (it 'should' work, so no need to worry) */
          BKE_nlatrack_remove_strip(nlt, strip);
          BKE_nlatrack_add_strip(nltp, strip, is_liboverride);
        }
      }
    }
  }

  /* free temp data */
  ANIM_animdata_freelist(&anim_data);

  /* refresh auto strip properties */
  ED_nla_postop_refresh(&ac);

  /* set notifier that things have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_EDITED, NULL);
  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA_ORDER, NULL);

  /* done */
  return OPERATOR_FINISHED;
}

void NLA_OT_move_down(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Move Strips Down";
  ot->idname = "NLA_OT_move_down";
  ot->description = "Move selected strips down a track if there's room";

  /* api callbacks */
  ot->exec = nlaedit_move_down_exec;
  ot->poll = nlaop_poll_tweakmode_off;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sync Action Length Operator
 *
 * Recalculate the extents of the action ranges used for the selected strips.
 * \{ */

static int nlaedit_sync_actlen_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;

  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;
  const bool active_only = RNA_boolean_get(op->ptr, "active");

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* get a list of the editable tracks being shown in the NLA */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT |
            ANIMFILTER_FCURVESONLY);
  if (active_only) {
    filter |= ANIMFILTER_ACTIVE;
  }
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* for each NLA-Track, apply scale of all selected strips */
  for (ale = anim_data.first; ale; ale = ale->next) {
    NlaTrack *nlt = (NlaTrack *)ale->data;
    NlaStrip *strip;

    for (strip = nlt->strips.first; strip; strip = strip->next) {
      /* strip selection/active status check */
      if (active_only) {
        if ((strip->flag & NLASTRIP_FLAG_ACTIVE) == 0) {
          continue;
        }
      }
      else {
        if ((strip->flag & NLASTRIP_FLAG_SELECT) == 0) {
          continue;
        }
      }

      /* must be action-clip only (transitions don't have scale) */
      if (strip->type == NLASTRIP_TYPE_CLIP) {
        if (strip->act == NULL) {
          continue;
        }

        BKE_nlastrip_recalculate_bounds_sync_action(strip);

        ale->update |= ANIM_UPDATE_DEPS;
      }
    }
  }

  /* free temp data */
  ANIM_animdata_update(&ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);

  /* set notifier that things have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_EDITED, NULL);

  /* done */
  return OPERATOR_FINISHED;
}

void NLA_OT_action_sync_length(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Sync Action Length";
  ot->idname = "NLA_OT_action_sync_length";
  ot->description =
      "Synchronize the length of the referenced Action with the length used in the strip";

  /* api callbacks */
  ot->exec = nlaedit_sync_actlen_exec;
  ot->poll = nlaop_poll_tweakmode_off;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_boolean(ot->srna,
                             "active",
                             1,
                             "Active Strip Only",
                             "Only sync the active length for the active strip");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Make Single User
 *
 * Ensure that each strip has its own action.
 * \{ */

static int nlaedit_make_single_user_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);
  bAnimContext ac;

  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;
  bool copied = false;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* get a list of the editable tracks being shown in the NLA */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT |
            ANIMFILTER_FCURVESONLY);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* Ensure that each action used only has a single user
   *   - This is done in reverse order so that the original strips are
   *     likely to still get to keep their action
   */
  for (ale = anim_data.last; ale; ale = ale->prev) {
    NlaTrack *nlt = (NlaTrack *)ale->data;
    NlaStrip *strip;

    for (strip = nlt->strips.last; strip; strip = strip->prev) {
      /* must be action-clip only (as only these have actions) */
      if ((strip->flag & NLASTRIP_FLAG_SELECT) && (strip->type == NLASTRIP_TYPE_CLIP)) {
        if (strip->act == NULL) {
          continue;
        }

        /* multi-user? */
        if (ID_REAL_USERS(strip->act) > 1) {
          /* make a new copy of the action for us to use (it will have 1 user already) */
          bAction *new_action = (bAction *)BKE_id_copy(bmain, &strip->act->id);

          /* decrement user count of our existing action */
          id_us_min(&strip->act->id);

          /* switch to the new copy */
          strip->act = new_action;

          ale->update |= ANIM_UPDATE_DEPS;
          copied = true;
        }
      }
    }
  }

  /* free temp data */
  ANIM_animdata_update(&ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);

  if (copied) {
    DEG_relations_tag_update(ac.bmain);
  }

  /* set notifier that things have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_ADDED, NULL);

  /* done */
  return OPERATOR_FINISHED;
}

void NLA_OT_make_single_user(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Make Single User";
  ot->idname = "NLA_OT_make_single_user";
  ot->description = "Ensure that each action is only used once in the set of strips selected";

  /* api callbacks */
  ot->invoke = WM_operator_confirm_or_exec;
  ot->exec = nlaedit_make_single_user_exec;
  ot->poll = nlaop_poll_tweakmode_off;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
  WM_operator_properties_confirm_or_exec(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Apply Scale Operator
 *
 * Reset the scaling of the selected strips to 1.0f.
 * \{ */

/* apply scaling to keyframe */
static short bezt_apply_nlamapping(KeyframeEditData *ked, BezTriple *bezt)
{
  /* NLA-strip which has this scaling is stored in ked->data */
  NlaStrip *strip = (NlaStrip *)ked->data;

  /* adjust all the times */
  bezt->vec[0][0] = nlastrip_get_frame(strip, bezt->vec[0][0], NLATIME_CONVERT_MAP);
  bezt->vec[1][0] = nlastrip_get_frame(strip, bezt->vec[1][0], NLATIME_CONVERT_MAP);
  bezt->vec[2][0] = nlastrip_get_frame(strip, bezt->vec[2][0], NLATIME_CONVERT_MAP);

  /* nothing to return or else we exit */
  return 0;
}

static int nlaedit_apply_scale_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);
  bAnimContext ac;

  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;
  bool copied = false;

  KeyframeEditData ked = {{NULL}};

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* get a list of the editable tracks being shown in the NLA */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT |
            ANIMFILTER_FCURVESONLY);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* for each NLA-Track, apply scale of all selected strips */
  for (ale = anim_data.first; ale; ale = ale->next) {
    NlaTrack *nlt = (NlaTrack *)ale->data;
    NlaStrip *strip;

    for (strip = nlt->strips.first; strip; strip = strip->next) {
      /* strip must be selected, and must be action-clip only
       * (transitions don't have scale) */
      if ((strip->flag & NLASTRIP_FLAG_SELECT) && (strip->type == NLASTRIP_TYPE_CLIP)) {
        if (strip->act == NULL || ID_IS_OVERRIDE_LIBRARY(strip->act) || ID_IS_LINKED(strip->act)) {
          continue;
        }
        /* if the referenced action is used by other strips,
         * make this strip use its own copy */
        if (strip->act->id.us > 1) {
          /* make a copy of the Action to work on */
          bAction *act = (bAction *)BKE_id_copy(bmain, &strip->act->id);

          /* set this as the new referenced action,
           * decrementing the users of the old one */
          id_us_min(&strip->act->id);
          strip->act = act;

          copied = true;
        }

        /* setup iterator, and iterate over all the keyframes in the action,
         * applying this scaling */
        ked.data = strip;
        ANIM_animchanneldata_keyframes_loop(&ked,
                                            ac.ads,
                                            strip->act,
                                            ALE_ACT,
                                            NULL,
                                            bezt_apply_nlamapping,
                                            BKE_fcurve_handles_recalc);

        /* clear scale of strip now that it has been applied,
         * and recalculate the extents of the action now that it has been scaled
         * but leave everything else alone
         */
        const float start = nlastrip_get_frame(strip, strip->actstart, NLATIME_CONVERT_MAP);
        const float end = nlastrip_get_frame(strip, strip->actend, NLATIME_CONVERT_MAP);

        if (strip->act->flag & ACT_FRAME_RANGE) {
          strip->act->frame_start = nlastrip_get_frame(
              strip, strip->act->frame_start, NLATIME_CONVERT_MAP);
          strip->act->frame_end = nlastrip_get_frame(
              strip, strip->act->frame_end, NLATIME_CONVERT_MAP);
        }

        strip->scale = 1.0f;
        strip->actstart = start;
        strip->actend = end;

        ale->update |= ANIM_UPDATE_DEPS;
      }
    }
  }

  /* free temp data */
  ANIM_animdata_update(&ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);

  if (copied) {
    DEG_relations_tag_update(ac.bmain);
  }

  /* set notifier that things have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_EDITED, NULL);

  /* done */
  return OPERATOR_FINISHED;
}

void NLA_OT_apply_scale(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Apply Scale";
  ot->idname = "NLA_OT_apply_scale";
  ot->description = "Apply scaling of selected strips to their referenced Actions";

  /* api callbacks */
  ot->exec = nlaedit_apply_scale_exec;
  ot->poll = nlaop_poll_tweakmode_off;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clear Scale Operator
 *
 * Reset the scaling of the selected strips to 1.0f.
 * \{ */

static int nlaedit_clear_scale_exec(bContext *C, wmOperator *UNUSED(op))
{
  bAnimContext ac;

  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* get a list of the editable tracks being shown in the NLA */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT |
            ANIMFILTER_FCURVESONLY);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* for each NLA-Track, reset scale of all selected strips */
  for (ale = anim_data.first; ale; ale = ale->next) {
    NlaTrack *nlt = (NlaTrack *)ale->data;
    NlaStrip *strip;

    for (strip = nlt->strips.first; strip; strip = strip->next) {
      /* strip must be selected, and must be action-clip only
       * (transitions don't have scale) */
      if ((strip->flag & NLASTRIP_FLAG_SELECT) && (strip->type == NLASTRIP_TYPE_CLIP)) {
        PointerRNA strip_ptr;

        RNA_pointer_create(NULL, &RNA_NlaStrip, strip, &strip_ptr);
        RNA_float_set(&strip_ptr, "scale", 1.0f);
      }
    }
  }

  /* free temp data */
  ANIM_animdata_freelist(&anim_data);

  /* refresh auto strip properties */
  ED_nla_postop_refresh(&ac);

  /* set notifier that things have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_EDITED, NULL);

  /* done */
  return OPERATOR_FINISHED;
}

void NLA_OT_clear_scale(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Scale";
  ot->idname = "NLA_OT_clear_scale";
  ot->description = "Reset scaling of selected strips";

  /* api callbacks */
  ot->exec = nlaedit_clear_scale_exec;
  ot->poll = nlaop_poll_tweakmode_off;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snap Strips Operator
 *
 * Moves the start-point of the selected strips to the specified places.
 * \{ */

/* defines for snap keyframes tool */
static const EnumPropertyItem prop_nlaedit_snap_types[] = {
    {NLAEDIT_SNAP_CFRA, "CFRA", 0, "Selection to Current Frame", ""},
    /* XXX as single entry? */
    {NLAEDIT_SNAP_NEAREST_FRAME, "NEAREST_FRAME", 0, "Selection to Nearest Frame", ""},
    /* XXX as single entry? */
    {NLAEDIT_SNAP_NEAREST_SECOND, "NEAREST_SECOND", 0, "Selection to Nearest Second", ""},
    {NLAEDIT_SNAP_NEAREST_MARKER, "NEAREST_MARKER", 0, "Selection to Nearest Marker", ""},
    {0, NULL, 0, NULL, NULL},
};

static int nlaedit_snap_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;

  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  Scene *scene;
  int mode = RNA_enum_get(op->ptr, "type");
  float secf;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* get a list of the editable tracks being shown in the NLA */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT |
            ANIMFILTER_FCURVESONLY);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* get some necessary vars */
  scene = ac.scene;
  secf = (float)FPS;

  bool any_added = false;

  /* since we may add tracks, perform this in reverse order */
  for (ale = anim_data.last; ale; ale = ale->prev) {
    ListBase tmp_strips = {NULL, NULL};
    AnimData *adt = ale->adt;
    NlaTrack *nlt = (NlaTrack *)ale->data;
    NlaStrip *strip, *stripn;
    NlaTrack *track;

    const bool is_liboverride = ID_IS_OVERRIDE_LIBRARY(ale->id);

    /* create meta-strips from the continuous chains of selected strips */
    BKE_nlastrips_make_metas(&nlt->strips, 1);

    /* apply the snapping to all the temp meta-strips, then put them in a separate list to be added
     * back to the original only if they still fit
     */
    for (strip = nlt->strips.first; strip; strip = stripn) {
      stripn = strip->next;

      if (strip->flag & NLASTRIP_FLAG_TEMP_META) {
        float start, end;

        /* get the existing end-points */
        start = strip->start;
        end = strip->end;

        /* calculate new start position based on snapping mode */
        switch (mode) {
          case NLAEDIT_SNAP_CFRA: /* to current frame */
            strip->start = (float)scene->r.cfra;
            break;
          case NLAEDIT_SNAP_NEAREST_FRAME: /* to nearest frame */
            strip->start = floorf(start + 0.5f);
            break;
          case NLAEDIT_SNAP_NEAREST_SECOND: /* to nearest second */
            strip->start = floorf(start / secf + 0.5f) * secf;
            break;
          case NLAEDIT_SNAP_NEAREST_MARKER: /* to nearest marker */
            strip->start = (float)ED_markers_find_nearest_marker_time(ac.markers, start);
            break;
          default: /* just in case... no snapping */
            strip->start = start;
            break;
        }

        /* get new endpoint based on start-point (and old length) */
        strip->end = strip->start + (end - start);

        /* apply transforms to meta-strip to its children */
        BKE_nlameta_flush_transforms(strip);

        /* remove strip from track, and add to the temp buffer */
        BLI_remlink(&nlt->strips, strip);
        BLI_addtail(&tmp_strips, strip);
      }
    }

    /* try adding each meta-strip back to the track one at a time, to make sure they'll fit */
    for (strip = tmp_strips.first; strip; strip = stripn) {
      stripn = strip->next;

      /* remove from temp-strips list */
      BLI_remlink(&tmp_strips, strip);

      /* in case there's no space in the current track, try adding */
      if (BKE_nlatrack_add_strip(nlt, strip, is_liboverride) == 0) {
        /* need to add a new track above the current one */
        track = BKE_nlatrack_new_after(&adt->nla_tracks, nlt, is_liboverride);
        BKE_nlatrack_set_active(&adt->nla_tracks, track);
        BKE_nlatrack_add_strip(track, strip, is_liboverride);

        /* clear temp meta-strips on this new track,
         * as we may not be able to get back to it */
        BKE_nlastrips_clear_metas(&track->strips, 0, 1);

        any_added = true;
      }
    }

    /* remove the meta-strips now that we're done */
    BKE_nlastrips_clear_metas(&nlt->strips, 0, 1);

    /* tag for recalculating the animation */
    ale->update |= ANIM_UPDATE_DEPS;
  }

  /* cleanup */
  ANIM_animdata_update(&ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);

  /* refresh auto strip properties */
  ED_nla_postop_refresh(&ac);

  /* set notifier that things have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_EDITED, NULL);
  if (any_added) {
    WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_ADDED, NULL);
  }

  /* done */
  return OPERATOR_FINISHED;
}

void NLA_OT_snap(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Snap Strips";
  ot->idname = "NLA_OT_snap";
  ot->description = "Move start of strips to specified time";

  /* api callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = nlaedit_snap_exec;
  ot->poll = nlaop_poll_tweakmode_off;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", prop_nlaedit_snap_types, 0, "Type", "");
}

/** \} */

/* NLA Modifiers */

/* -------------------------------------------------------------------- */
/** \name Add F-Modifier Operator
 * \{ */

static const EnumPropertyItem *nla_fmodifier_itemf(bContext *C,
                                                   PointerRNA *UNUSED(ptr),
                                                   PropertyRNA *UNUSED(prop),
                                                   bool *r_free)
{
  EnumPropertyItem *item = NULL;
  int totitem = 0;
  int i = 0;

  if (C == NULL) {
    return rna_enum_fmodifier_type_items;
  }

  /* start from 1 to skip the 'Invalid' modifier type */
  for (i = 1; i < FMODIFIER_NUM_TYPES; i++) {
    const FModifierTypeInfo *fmi = get_fmodifier_typeinfo(i);
    int index;

    /* check if modifier is valid for this context */
    if (fmi == NULL) {
      continue;
    }
    if (i == FMODIFIER_TYPE_CYCLES) { /* we already have repeat... */
      continue;
    }

    index = RNA_enum_from_value(rna_enum_fmodifier_type_items, fmi->type);
    if (index != -1) { /* Not all types are implemented yet... */
      RNA_enum_item_add(&item, &totitem, &rna_enum_fmodifier_type_items[index]);
    }
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

static int nla_fmodifier_add_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;

  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  FModifier *fcm;
  int type = RNA_enum_get(op->ptr, "type");
  const bool active_only = RNA_boolean_get(op->ptr, "only_active");

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* get a list of the editable tracks being shown in the NLA */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT |
            ANIMFILTER_FCURVESONLY);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* for each NLA-Track, add the specified modifier to all selected strips */
  for (ale = anim_data.first; ale; ale = ale->next) {
    NlaTrack *nlt = (NlaTrack *)ale->data;
    NlaStrip *strip;

    if (BKE_nlatrack_is_nonlocal_in_liboverride(ale->id, nlt)) {
      /* No adding f-modifiers to strips in non-local tracks of override data. */
      continue;
    }

    for (strip = nlt->strips.first; strip; strip = strip->next) {
      /* can F-Modifier be added to the current strip? */
      if (active_only) {
        /* if not active, cannot add since we're only adding to active strip */
        if ((strip->flag & NLASTRIP_FLAG_ACTIVE) == 0) {
          continue;
        }
      }
      else {
        /* strip must be selected, since we're not just doing active */
        if ((strip->flag & NLASTRIP_FLAG_SELECT) == 0) {
          continue;
        }
      }

      /* sound clips are not affected by FModifiers */
      if (strip->type == NLASTRIP_TYPE_SOUND) {
        continue;
      }

      /* add F-Modifier of specified type to selected, and make it the active one */
      fcm = add_fmodifier(&strip->modifiers, type, NULL);

      if (fcm) {
        set_active_fmodifier(&strip->modifiers, fcm);
        ale->update |= ANIM_UPDATE_DEPS;
      }
      else {
        BKE_reportf(op->reports,
                    RPT_ERROR,
                    "Modifier could not be added to (%s : %s) (see console for details)",
                    nlt->name,
                    strip->name);
      }
    }
  }

  /* free temp data */
  ANIM_animdata_update(&ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);

  /* set notifier that things have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_EDITED, NULL);

  /* done */
  return OPERATOR_FINISHED;
}

void NLA_OT_fmodifier_add(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Add F-Modifier";
  ot->idname = "NLA_OT_fmodifier_add";
  ot->description = "Add F-Modifier to the active/selected NLA-Strips";

  /* api callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = nla_fmodifier_add_exec;
  ot->poll = nlaop_poll_tweakmode_off;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* id-props */
  ot->prop = RNA_def_enum(ot->srna, "type", rna_enum_fmodifier_type_items, 0, "Type", "");
  RNA_def_property_translation_context(ot->prop, BLT_I18NCONTEXT_ID_ACTION);
  RNA_def_enum_funcs(ot->prop, nla_fmodifier_itemf);

  prop = RNA_def_boolean(ot->srna,
                         "only_active",
                         true,
                         "Only Active",
                         "Only add a F-Modifier of the specified type to the active strip");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_ACTION);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Copy F-Modifiers Operator
 * \{ */

static int nla_fmodifier_copy_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;
  bool ok = false;

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* clear buffer first */
  ANIM_fmodifiers_copybuf_free();

  /* get a list of the editable tracks being shown in the NLA */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT |
            ANIMFILTER_FCURVESONLY);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* for each NLA-Track, add the specified modifier to all selected strips */
  for (ale = anim_data.first; ale; ale = ale->next) {
    NlaTrack *nlt = (NlaTrack *)ale->data;
    NlaStrip *strip;

    for (strip = nlt->strips.first; strip; strip = strip->next) {
      /* only add F-Modifier if on active strip? */
      if ((strip->flag & NLASTRIP_FLAG_ACTIVE) == 0) {
        continue;
      }

      /* TODO: when 'active' vs 'all' boolean is added, change last param! */
      ok |= ANIM_fmodifiers_copy_to_buf(&strip->modifiers, 0);
    }
  }

  /* free temp data */
  ANIM_animdata_freelist(&anim_data);

  /* successful or not? */
  if (ok == 0) {
    BKE_report(op->reports, RPT_ERROR, "No F-Modifiers available to be copied");
    return OPERATOR_CANCELLED;
  }

  /* no updates needed - copy is non-destructive operation */
  return OPERATOR_FINISHED;
}

void NLA_OT_fmodifier_copy(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy F-Modifiers";
  ot->idname = "NLA_OT_fmodifier_copy";
  ot->description = "Copy the F-Modifier(s) of the active NLA-Strip";

  /* api callbacks */
  ot->exec = nla_fmodifier_copy_exec;
  ot->poll = nlaop_poll_tweakmode_off;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* id-props */
#if 0
  ot->prop = RNA_def_boolean(ot->srna,
                             "all",
                             1,
                             "All F-Modifiers",
                             "Copy all the F-Modifiers, instead of just the active one");
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Paste F-Modifiers Operator
 * \{ */

static int nla_fmodifier_paste_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter, ok = 0;

  const bool active_only = RNA_boolean_get(op->ptr, "only_active");
  const bool replace = RNA_boolean_get(op->ptr, "replace");

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* get a list of the editable tracks being shown in the NLA */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT |
            ANIMFILTER_NODUPLIS | ANIMFILTER_FCURVESONLY);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* for each NLA-Track, add the specified modifier to all selected strips */
  for (ale = anim_data.first; ale; ale = ale->next) {
    NlaTrack *nlt = (NlaTrack *)ale->data;
    NlaStrip *strip;

    if (BKE_nlatrack_is_nonlocal_in_liboverride(ale->id, nlt)) {
      /* No pasting in non-local tracks of override data. */
      continue;
    }

    for (strip = nlt->strips.first; strip; strip = strip->next) {
      /* can F-Modifier be added to the current strip? */
      if (active_only) {
        /* if not active, cannot add since we're only adding to active strip */
        if ((strip->flag & NLASTRIP_FLAG_ACTIVE) == 0) {
          continue;
        }
      }
      else {
        /* strip must be selected, since we're not just doing active */
        if ((strip->flag & NLASTRIP_FLAG_SELECT) == 0) {
          continue;
        }
      }

      /* paste FModifiers from buffer */
      ok += ANIM_fmodifiers_paste_from_buf(&strip->modifiers, replace, NULL);
      ale->update |= ANIM_UPDATE_DEPS;
    }
  }

  /* clean up */
  ANIM_animdata_update(&ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);

  /* successful or not? */
  if (ok) {
    WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_EDITED, NULL);
    return OPERATOR_FINISHED;
  }

  BKE_report(op->reports, RPT_ERROR, "No F-Modifiers to paste");
  return OPERATOR_CANCELLED;
}

void NLA_OT_fmodifier_paste(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Paste F-Modifiers";
  ot->idname = "NLA_OT_fmodifier_paste";
  ot->description = "Add copied F-Modifiers to the selected NLA-Strips";

  /* api callbacks */
  ot->exec = nla_fmodifier_paste_exec;
  ot->poll = nlaop_poll_tweakmode_off;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_boolean(
      ot->srna, "only_active", true, "Only Active", "Only paste F-Modifiers on active strip");
  RNA_def_property_translation_context(ot->prop, BLT_I18NCONTEXT_ID_ACTION);

  RNA_def_boolean(
      ot->srna,
      "replace",
      false,
      "Replace Existing",
      "Replace existing F-Modifiers, instead of just appending to the end of the existing list");
}

/** \} */
