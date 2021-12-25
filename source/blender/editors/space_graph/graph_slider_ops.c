/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spgraph
 *
 * Graph Slider Operators
 *
 * This file contains a collection of operators to modify keyframes in the graph editor.
 * All operators are modal and use a slider that allows the user to define a percentage
 * to modify the operator.
 */

#include <float.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "BLT_translation.h"

#include "BKE_context.h"

#include "UI_interface.h"

#include "ED_anim_api.h"
#include "ED_keyframes_edit.h"
#include "ED_numinput.h"
#include "ED_screen.h"
#include "ED_util.h"

#include "WM_api.h"
#include "WM_types.h"

#include "graph_intern.h"

/* -------------------------------------------------------------------- */
/** \name Internal Struct & Defines
 * \{ */

/* Used to obtain a list of animation channels for the operators to work on. */
#define OPERATOR_DATA_FILTER \
  (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_SEL | \
   ANIMFILTER_NODUPLIS)

/* This data type is only used for modal operation. */
typedef struct tGraphSliderOp {
  bAnimContext ac;
  Scene *scene;
  ScrArea *area;
  ARegion *region;

  /** A 0-1 value for determining how much we should decimate. */
  PropertyRNA *factor_prop;

  /** The original bezt curve data (used for restoring fcurves). */
  ListBase bezt_arr_list;

  struct tSlider *slider;

  /* Each operator has a specific update function. */
  void (*modal_update)(struct bContext *, struct wmOperator *);

  NumInput num;
} tGraphSliderOp;

typedef struct tBeztCopyData {
  int tot_vert;
  BezTriple *bezt;
} tBeztCopyData;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utility Functions
 * \{ */

/* Construct a list with the original bezt arrays so we can restore them during modal operation.
 * The data is stored on the struct that is passed.*/
static void store_original_bezt_arrays(tGraphSliderOp *gso)
{
  ListBase anim_data = {NULL, NULL};
  bAnimContext *ac = &gso->ac;
  bAnimListElem *ale;

  ANIM_animdata_filter(ac, &anim_data, OPERATOR_DATA_FILTER, ac->data, ac->datatype);

  /* Loop through filtered data and copy the curves. */
  for (ale = anim_data.first; ale; ale = ale->next) {
    FCurve *fcu = (FCurve *)ale->key_data;

    if (fcu->bezt == NULL) {
      /* This curve is baked, skip it. */
      continue;
    }

    const int arr_size = sizeof(BezTriple) * fcu->totvert;

    tBeztCopyData *copy = MEM_mallocN(sizeof(tBeztCopyData), "bezts_copy");
    BezTriple *bezts_copy = MEM_mallocN(arr_size, "bezts_copy_array");

    copy->tot_vert = fcu->totvert;
    memcpy(bezts_copy, fcu->bezt, arr_size);

    copy->bezt = bezts_copy;

    LinkData *link = NULL;

    link = MEM_callocN(sizeof(LinkData), "Bezt Link");
    link->data = copy;

    BLI_addtail(&gso->bezt_arr_list, link);
  }

  ANIM_animdata_freelist(&anim_data);
}

/* Overwrite the current bezts arrays with the original data. */
static void reset_bezts(tGraphSliderOp *gso)
{
  ListBase anim_data = {NULL, NULL};
  LinkData *link_bezt;
  bAnimListElem *ale;

  bAnimContext *ac = &gso->ac;

  /* Filter data. */
  ANIM_animdata_filter(ac, &anim_data, OPERATOR_DATA_FILTER, ac->data, ac->datatype);

  /* Loop through filtered data and reset bezts. */
  for (ale = anim_data.first, link_bezt = gso->bezt_arr_list.first; ale; ale = ale->next) {
    FCurve *fcu = (FCurve *)ale->key_data;

    if (fcu->bezt == NULL) {
      /* This curve is baked, skip it. */
      continue;
    }

    tBeztCopyData *data = link_bezt->data;

    const int arr_size = sizeof(BezTriple) * data->tot_vert;

    MEM_freeN(fcu->bezt);

    fcu->bezt = MEM_mallocN(arr_size, __func__);
    fcu->totvert = data->tot_vert;

    memcpy(fcu->bezt, data->bezt, arr_size);

    link_bezt = link_bezt->next;
  }

  ANIM_animdata_freelist(&anim_data);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Common Modal Functions
 * \{ */

static void graph_slider_exit(bContext *C, wmOperator *op)
{
  tGraphSliderOp *gso = op->customdata;
  wmWindow *win = CTX_wm_window(C);

  /* If data exists, clear its data and exit. */
  if (gso == NULL) {
    return;
  }

  ScrArea *area = gso->area;
  LinkData *link;

  ED_slider_destroy(C, gso->slider);

  for (link = gso->bezt_arr_list.first; link != NULL; link = link->next) {
    tBeztCopyData *copy = link->data;
    MEM_freeN(copy->bezt);
    MEM_freeN(link->data);
  }

  BLI_freelistN(&gso->bezt_arr_list);
  MEM_freeN(gso);

  /* Return to normal cursor and header status. */
  WM_cursor_modal_restore(win);
  ED_area_status_text(area, NULL);

  /* cleanup */
  op->customdata = NULL;
}

static int graph_slider_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  tGraphSliderOp *gso = op->customdata;

  const bool has_numinput = hasNumInput(&gso->num);

  ED_slider_modal(gso->slider, event);

  switch (event->type) {
    /* Confirm */
    case LEFTMOUSE:
    case EVT_RETKEY:
    case EVT_PADENTER: {
      if (event->val == KM_PRESS) {
        graph_slider_exit(C, op);

        return OPERATOR_FINISHED;
      }
      break;
    }

    /* Cancel */
    case EVT_ESCKEY:
    case RIGHTMOUSE: {
      if (event->val == KM_PRESS) {
        reset_bezts(gso);

        WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);

        graph_slider_exit(C, op);

        return OPERATOR_CANCELLED;
      }
      break;
    }

    /* When the mouse is moved, the percentage and the keyframes update. */
    case MOUSEMOVE: {
      if (has_numinput == false) {
        /* Do the update as specified by the operator. */
        gso->modal_update(C, op);
      }
      break;
    }
    default: {
      if ((event->val == KM_PRESS) && handleNumInput(C, &gso->num, event)) {
        float value;
        float percentage = RNA_property_float_get(op->ptr, gso->factor_prop);

        /* Grab percentage from numeric input, and store this new value for redo
         * NOTE: users see ints, while internally we use a 0-1 float.
         */
        value = percentage * 100.0f;
        applyNumInput(&gso->num, &value);

        percentage = value / 100.0f;
        ED_slider_factor_set(gso->slider, percentage);
        RNA_property_float_set(op->ptr, gso->factor_prop, percentage);

        gso->modal_update(C, op);
        break;
      }

      /* Unhandled event - maybe it was some view manipulation? */
      /* Allow to pass through. */
      return OPERATOR_RUNNING_MODAL | OPERATOR_PASS_THROUGH;
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

/* Allocate tGraphSliderOp and assign to op->customdata. */
static int graph_slider_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  tGraphSliderOp *gso;

  WM_cursor_modal_set(CTX_wm_window(C), WM_CURSOR_EW_SCROLL);

  /* Init slide-op data. */
  gso = op->customdata = MEM_callocN(sizeof(tGraphSliderOp), "tGraphSliderOp");

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &gso->ac) == 0) {
    graph_slider_exit(C, op);
    return OPERATOR_CANCELLED;
  }

  gso->scene = CTX_data_scene(C);
  gso->area = CTX_wm_area(C);
  gso->region = CTX_wm_region(C);

  store_original_bezt_arrays(gso);

  gso->slider = ED_slider_create(C);
  ED_slider_init(gso->slider, event);

  if (gso->bezt_arr_list.first == NULL) {
    WM_report(RPT_ERROR, "Cannot find keys to operate on.");
    graph_slider_exit(C, op);
    return OPERATOR_CANCELLED;
  }

  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Decimate Keyframes Operator
 * \{ */

typedef enum tDecimModes {
  DECIM_RATIO = 1,
  DECIM_ERROR,
} tDecimModes;

static void decimate_graph_keys(bAnimContext *ac, float factor, float error_sq_max)
{
  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;

  /* Filter data. */
  ANIM_animdata_filter(ac, &anim_data, OPERATOR_DATA_FILTER, ac->data, ac->datatype);

  /* Loop through filtered data and clean curves. */
  for (ale = anim_data.first; ale; ale = ale->next) {
    if (!decimate_fcurve(ale, factor, error_sq_max)) {
      /* The selection contains unsupported keyframe types! */
      WM_report(RPT_WARNING, "Decimate: Skipping non linear/bezier keyframes!");
    }

    ale->update |= ANIM_UPDATE_DEFAULT;
  }

  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

/* Draw a percentage indicator in workspace footer. */
static void decimate_draw_status(bContext *C, tGraphSliderOp *gso)
{
  char status_str[UI_MAX_DRAW_STR];
  char mode_str[32];
  char slider_string[UI_MAX_DRAW_STR];

  ED_slider_status_string_get(gso->slider, slider_string, UI_MAX_DRAW_STR);

  strcpy(mode_str, TIP_("Decimate Keyframes"));

  if (hasNumInput(&gso->num)) {
    char str_ofs[NUM_STR_REP_LEN];

    outputNumInput(&gso->num, str_ofs, &gso->scene->unit);

    BLI_snprintf(status_str, sizeof(status_str), "%s: %s", mode_str, str_ofs);
  }
  else {
    BLI_snprintf(status_str, sizeof(status_str), "%s: %s", mode_str, slider_string);
  }

  ED_workspace_status_text(C, status_str);
}

static void decimate_modal_update(bContext *C, wmOperator *op)
{
  /* Perform decimate updates - in response to some user action
   * (e.g. pressing a key or moving the mouse). */
  tGraphSliderOp *gso = op->customdata;

  decimate_draw_status(C, gso);

  /* Reset keyframe data (so we get back to the original state). */
  reset_bezts(gso);

  /* Apply... */
  float factor = ED_slider_factor_get(gso->slider);
  RNA_property_float_set(op->ptr, gso->factor_prop, factor);
  /* We don't want to limit the decimation to a certain error margin. */
  const float error_sq_max = FLT_MAX;
  decimate_graph_keys(&gso->ac, factor, error_sq_max);
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
}

static int decimate_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const int invoke_result = graph_slider_invoke(C, op, event);

  if (invoke_result == OPERATOR_CANCELLED) {
    return OPERATOR_CANCELLED;
  }

  tGraphSliderOp *gso = op->customdata;
  gso->factor_prop = RNA_struct_find_property(op->ptr, "factor");
  gso->modal_update = decimate_modal_update;
  ED_slider_allow_overshoot_set(gso->slider, false);

  return invoke_result;
}

static int decimate_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  tDecimModes mode = RNA_enum_get(op->ptr, "mode");
  /* We want to be able to work on all available keyframes. */
  float factor = 1.0f;
  /* We don't want to limit the decimation to a certain error margin. */
  float error_sq_max = FLT_MAX;

  switch (mode) {
    case DECIM_RATIO:
      factor = RNA_float_get(op->ptr, "factor");
      break;
    case DECIM_ERROR:
      error_sq_max = RNA_float_get(op->ptr, "remove_error_margin");
      /* The decimate algorithm expects the error to be squared. */
      error_sq_max *= error_sq_max;

      break;
  }

  if (factor == 0.0f || error_sq_max == 0.0f) {
    /* Nothing to remove. */
    return OPERATOR_FINISHED;
  }

  decimate_graph_keys(&ac, factor, error_sq_max);

  /* Set notifier that keyframes have changed. */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

static bool decimate_poll_property(const bContext *UNUSED(C),
                                   wmOperator *op,
                                   const PropertyRNA *prop)
{
  const char *prop_id = RNA_property_identifier(prop);

  if (STRPREFIX(prop_id, "remove")) {
    int mode = RNA_enum_get(op->ptr, "mode");

    if (STREQ(prop_id, "factor") && mode != DECIM_RATIO) {
      return false;
    }
    if (STREQ(prop_id, "remove_error_margin") && mode != DECIM_ERROR) {
      return false;
    }
  }

  return true;
}

static char *decimate_desc(bContext *UNUSED(C), wmOperatorType *UNUSED(op), PointerRNA *ptr)
{

  if (RNA_enum_get(ptr, "mode") == DECIM_ERROR) {
    return BLI_strdup(
        "Decimate F-Curves by specifying how much it can deviate from the original curve");
  }

  /* Use default description. */
  return NULL;
}

static const EnumPropertyItem decimate_mode_items[] = {
    {DECIM_RATIO,
     "RATIO",
     0,
     "Ratio",
     "Use a percentage to specify how many keyframes you want to remove"},
    {DECIM_ERROR,
     "ERROR",
     0,
     "Error Margin",
     "Use an error margin to specify how much the curve is allowed to deviate from the original "
     "path"},
    {0, NULL, 0, NULL, NULL},
};

void GRAPH_OT_decimate(wmOperatorType *ot)
{
  /* Identifiers */
  ot->name = "Decimate Keyframes";
  ot->idname = "GRAPH_OT_decimate";
  ot->description =
      "Decimate F-Curves by removing keyframes that influence the curve shape the least";

  /* API callbacks */
  ot->poll_property = decimate_poll_property;
  ot->get_description = decimate_desc;
  ot->invoke = decimate_invoke;
  ot->modal = graph_slider_modal;
  ot->exec = decimate_exec;
  ot->poll = graphop_editable_keyframes_poll;

  /* Flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties */
  RNA_def_enum(ot->srna,
               "mode",
               decimate_mode_items,
               DECIM_RATIO,
               "Mode",
               "Which mode to use for decimation");

  RNA_def_float_factor(ot->srna,
                       "factor",
                       1.0f / 3.0f,
                       0.0f,
                       1.0f,
                       "Remove",
                       "The ratio of remaining keyframes after the operation",
                       0.0f,
                       1.0f);
  RNA_def_float(ot->srna,
                "remove_error_margin",
                0.0f,
                0.0f,
                FLT_MAX,
                "Max Error Margin",
                "How much the new decimated curve is allowed to deviate from the original",
                0.0f,
                10.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Blend To Neighbor Operator
 * \{ */

static void blend_to_neighbor_graph_keys(bAnimContext *ac, float factor)
{
  ListBase anim_data = {NULL, NULL};
  ANIM_animdata_filter(ac, &anim_data, OPERATOR_DATA_FILTER, ac->data, ac->datatype);

  bAnimListElem *ale;

  /* Loop through filtered data and blend keys. */

  for (ale = anim_data.first; ale; ale = ale->next) {
    FCurve *fcu = (FCurve *)ale->key_data;
    ListBase segments = find_fcurve_segments(fcu);
    LISTBASE_FOREACH (FCurveSegment *, segment, &segments) {
      blend_to_neighbor_fcurve_segment(fcu, segment, factor);
    }
    BLI_freelistN(&segments);
    ale->update |= ANIM_UPDATE_DEFAULT;
  }

  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

static void blend_to_neighbor_draw_status_header(bContext *C, tGraphSliderOp *gso)
{
  char status_str[UI_MAX_DRAW_STR];
  char mode_str[32];
  char slider_string[UI_MAX_DRAW_STR];

  ED_slider_status_string_get(gso->slider, slider_string, UI_MAX_DRAW_STR);

  strcpy(mode_str, TIP_("Blend To Neighbor"));

  if (hasNumInput(&gso->num)) {
    char str_ofs[NUM_STR_REP_LEN];

    outputNumInput(&gso->num, str_ofs, &gso->scene->unit);

    BLI_snprintf(status_str, sizeof(status_str), "%s: %s", mode_str, str_ofs);
  }
  else {
    BLI_snprintf(status_str, sizeof(status_str), "%s: %s", mode_str, slider_string);
  }

  ED_workspace_status_text(C, status_str);
}

static void blend_to_neighbor_modal_update(bContext *C, wmOperator *op)
{
  tGraphSliderOp *gso = op->customdata;

  blend_to_neighbor_draw_status_header(C, gso);

  /* Reset keyframe data to the state at invoke. */
  reset_bezts(gso);

  const float factor = ED_slider_factor_get(gso->slider);
  RNA_property_float_set(op->ptr, gso->factor_prop, factor);
  blend_to_neighbor_graph_keys(&gso->ac, factor);

  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
}

static int blend_to_neighbor_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const int invoke_result = graph_slider_invoke(C, op, event);

  if (invoke_result == OPERATOR_CANCELLED) {
    return invoke_result;
  }

  tGraphSliderOp *gso = op->customdata;
  gso->modal_update = blend_to_neighbor_modal_update;
  gso->factor_prop = RNA_struct_find_property(op->ptr, "factor");
  blend_to_neighbor_draw_status_header(C, gso);

  return invoke_result;
}

static int blend_to_neighbor_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;

  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  const float factor = RNA_float_get(op->ptr, "factor");

  blend_to_neighbor_graph_keys(&ac, factor);

  /* Set notifier that keyframes have changed. */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

void GRAPH_OT_blend_to_neighbor(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Blend To Neighbor";
  ot->idname = "GRAPH_OT_blend_to_neighbor";
  ot->description = "Blend selected keyframes to their left or right neighbor";

  /* API callbacks. */
  ot->invoke = blend_to_neighbor_invoke;
  ot->modal = graph_slider_modal;
  ot->exec = blend_to_neighbor_exec;
  ot->poll = graphop_editable_keyframes_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_float_factor(ot->srna,
                       "factor",
                       1.0f / 3.0f,
                       -FLT_MAX,
                       FLT_MAX,
                       "Blend",
                       "The blend factor with 0.5 being the current frame",
                       0.0f,
                       1.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Breakdown Operator
 * \{ */

static void breakdown_graph_keys(bAnimContext *ac, float factor)
{
  ListBase anim_data = {NULL, NULL};
  ANIM_animdata_filter(ac, &anim_data, OPERATOR_DATA_FILTER, ac->data, ac->datatype);

  bAnimListElem *ale;

  for (ale = anim_data.first; ale; ale = ale->next) {
    FCurve *fcu = (FCurve *)ale->key_data;
    ListBase segments = find_fcurve_segments(fcu);
    LISTBASE_FOREACH (FCurveSegment *, segment, &segments) {
      breakdown_fcurve_segment(fcu, segment, factor);
    }
    BLI_freelistN(&segments);
    ale->update |= ANIM_UPDATE_DEFAULT;
  }

  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

static void breakdown_draw_status_header(bContext *C, tGraphSliderOp *gso)
{
  char status_str[UI_MAX_DRAW_STR];
  char mode_str[32];
  char slider_string[UI_MAX_DRAW_STR];

  ED_slider_status_string_get(gso->slider, slider_string, UI_MAX_DRAW_STR);

  strcpy(mode_str, TIP_("Breakdown"));

  if (hasNumInput(&gso->num)) {
    char str_ofs[NUM_STR_REP_LEN];

    outputNumInput(&gso->num, str_ofs, &gso->scene->unit);

    BLI_snprintf(status_str, sizeof(status_str), "%s: %s", mode_str, str_ofs);
  }
  else {
    BLI_snprintf(status_str, sizeof(status_str), "%s: %s", mode_str, slider_string);
  }

  ED_workspace_status_text(C, status_str);
}

static void breakdown_modal_update(bContext *C, wmOperator *op)
{
  tGraphSliderOp *gso = op->customdata;

  breakdown_draw_status_header(C, gso);

  /* Reset keyframe data to the state at invoke. */
  reset_bezts(gso);
  breakdown_graph_keys(&gso->ac, ED_slider_factor_get(gso->slider));
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
}

static int breakdown_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const int invoke_result = graph_slider_invoke(C, op, event);

  if (invoke_result == OPERATOR_CANCELLED) {
    return invoke_result;
  }

  tGraphSliderOp *gso = op->customdata;
  gso->modal_update = breakdown_modal_update;
  breakdown_draw_status_header(C, gso);

  return invoke_result;
}

static int breakdown_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;

  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  const float factor = RNA_float_get(op->ptr, "factor");

  breakdown_graph_keys(&ac, factor);

  /* Set notifier that keyframes have changed. */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

void GRAPH_OT_breakdown(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Breakdown";
  ot->idname = "GRAPH_OT_breakdown";
  ot->description = "Move selected keyframes to an inbetween position relative to adjacent keys";

  /* API callbacks. */
  ot->invoke = breakdown_invoke;
  ot->modal = graph_slider_modal;
  ot->exec = breakdown_exec;
  ot->poll = graphop_editable_keyframes_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_float_factor(ot->srna,
                       "factor",
                       1.0f / 3.0f,
                       -FLT_MAX,
                       FLT_MAX,
                       "Factor",
                       "Favor either the left or the right key",
                       0.0f,
                       1.0f);
}

/** \} */
