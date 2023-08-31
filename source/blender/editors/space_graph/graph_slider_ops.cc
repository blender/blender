/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spgraph
 *
 * Graph Slider Operators
 *
 * This file contains a collection of operators to modify keyframes in the graph editor.
 * All operators are modal and use a slider that allows the user to define a percentage
 * to modify the operator.
 */

#include <cfloat>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "BLT_translation.h"

#include "BKE_context.h"

#include "UI_interface.hh"

#include "ED_anim_api.hh"
#include "ED_keyframes_edit.hh"
#include "ED_numinput.hh"
#include "ED_screen.hh"
#include "ED_util.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "graph_intern.h"

/* -------------------------------------------------------------------- */
/** \name Internal Struct & Defines
 * \{ */

/* Used to obtain a list of animation channels for the operators to work on. */
#define OPERATOR_DATA_FILTER \
  (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FCURVESONLY | \
   ANIMFILTER_FOREDIT | ANIMFILTER_SEL | ANIMFILTER_NODUPLIS)

/* This data type is only used for modal operation. */
struct tGraphSliderOp {
  bAnimContext ac;
  Scene *scene;
  ScrArea *area;
  ARegion *region;

  /** A 0-1 value for determining how much we should decimate. */
  PropertyRNA *factor_prop;

  /** The original bezt curve data (used for restoring fcurves). */
  ListBase bezt_arr_list;

  tSlider *slider;

  /* Each operator has a specific update function. */
  void (*modal_update)(bContext *, wmOperator *);

  /* If an operator stores custom data, it also needs to provide the function to clean it up. */
  void *operator_data;
  void (*free_operator_data)(void *operator_data);

  NumInput num;
};

struct tBeztCopyData {
  int tot_vert;
  BezTriple *bezt;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utility Functions
 * \{ */

/**
 * Helper function that iterates over all FCurves and selected segments and applies the given
 * function.
 */
static void apply_fcu_segment_function(bAnimContext *ac,
                                       const float factor,
                                       void (*segment_function)(FCurve *fcu,
                                                                FCurveSegment *segment,
                                                                const float factor))
{
  ListBase anim_data = {nullptr, nullptr};

  ANIM_animdata_filter(
      ac, &anim_data, OPERATOR_DATA_FILTER, ac->data, eAnimCont_Types(ac->datatype));
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    FCurve *fcu = (FCurve *)ale->key_data;
    ListBase segments = find_fcurve_segments(fcu);

    LISTBASE_FOREACH (FCurveSegment *, segment, &segments) {
      segment_function(fcu, segment, factor);
    }

    ale->update |= ANIM_UPDATE_DEFAULT;
    BLI_freelistN(&segments);
  }

  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

static void common_draw_status_header(bContext *C, tGraphSliderOp *gso, const char *operator_name)
{
  char status_str[UI_MAX_DRAW_STR];
  char mode_str[32];
  char slider_string[UI_MAX_DRAW_STR];

  ED_slider_status_string_get(gso->slider, slider_string, UI_MAX_DRAW_STR);

  STRNCPY(mode_str, TIP_(operator_name));

  if (hasNumInput(&gso->num)) {
    char str_ofs[NUM_STR_REP_LEN];

    outputNumInput(&gso->num, str_ofs, &gso->scene->unit);

    SNPRINTF(status_str, "%s: %s", mode_str, str_ofs);
  }
  else {
    SNPRINTF(status_str, "%s: %s", mode_str, slider_string);
  }

  ED_workspace_status_text(C, status_str);
}

/**
 * Construct a list with the original bezt arrays so we can restore them during modal operation.
 * The data is stored on the struct that is passed.
 */
static void store_original_bezt_arrays(tGraphSliderOp *gso)
{
  ListBase anim_data = {nullptr, nullptr};
  bAnimContext *ac = &gso->ac;

  ANIM_animdata_filter(
      ac, &anim_data, OPERATOR_DATA_FILTER, ac->data, eAnimCont_Types(ac->datatype));

  /* Loop through filtered data and copy the curves. */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    FCurve *fcu = (FCurve *)ale->key_data;

    if (fcu->bezt == nullptr) {
      /* This curve is baked, skip it. */
      continue;
    }

    const int arr_size = sizeof(BezTriple) * fcu->totvert;

    tBeztCopyData *copy = static_cast<tBeztCopyData *>(
        MEM_mallocN(sizeof(tBeztCopyData), "bezts_copy"));
    BezTriple *bezts_copy = static_cast<BezTriple *>(MEM_mallocN(arr_size, "bezts_copy_array"));

    copy->tot_vert = fcu->totvert;
    memcpy(bezts_copy, fcu->bezt, arr_size);

    copy->bezt = bezts_copy;

    LinkData *link = nullptr;

    link = static_cast<LinkData *>(MEM_callocN(sizeof(LinkData), "Bezt Link"));
    link->data = copy;

    BLI_addtail(&gso->bezt_arr_list, link);
  }

  ANIM_animdata_freelist(&anim_data);
}

/* Overwrite the current bezts arrays with the original data. */
static void reset_bezts(tGraphSliderOp *gso)
{
  ListBase anim_data = {nullptr, nullptr};
  LinkData *link_bezt;
  bAnimListElem *ale;

  bAnimContext *ac = &gso->ac;

  /* Filter data. */
  ANIM_animdata_filter(
      ac, &anim_data, OPERATOR_DATA_FILTER, ac->data, eAnimCont_Types(ac->datatype));

  /* Loop through filtered data and reset bezts. */
  for (ale = static_cast<bAnimListElem *>(anim_data.first),
      link_bezt = static_cast<LinkData *>(gso->bezt_arr_list.first);
       ale;
       ale = ale->next)
  {
    FCurve *fcu = (FCurve *)ale->key_data;

    if (fcu->bezt == nullptr) {
      /* This curve is baked, skip it. */
      continue;
    }

    tBeztCopyData *data = static_cast<tBeztCopyData *>(link_bezt->data);

    const int arr_size = sizeof(BezTriple) * data->tot_vert;

    MEM_freeN(fcu->bezt);

    fcu->bezt = static_cast<BezTriple *>(MEM_mallocN(arr_size, __func__));
    fcu->totvert = data->tot_vert;

    memcpy(fcu->bezt, data->bezt, arr_size);

    link_bezt = link_bezt->next;
  }

  ANIM_animdata_freelist(&anim_data);
}

/**
 * Get factor value and store it in RNA property.
 * Custom data of #wmOperator needs to contain #tGraphSliderOp.
 */
static float slider_factor_get_and_remember(wmOperator *op)
{
  tGraphSliderOp *gso = static_cast<tGraphSliderOp *>(op->customdata);
  const float factor = ED_slider_factor_get(gso->slider);
  RNA_property_float_set(op->ptr, gso->factor_prop, factor);
  return factor;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Common Modal Functions
 * \{ */

static void graph_slider_exit(bContext *C, wmOperator *op)
{
  tGraphSliderOp *gso = static_cast<tGraphSliderOp *>(op->customdata);
  wmWindow *win = CTX_wm_window(C);

  /* If data exists, clear its data and exit. */
  if (gso == nullptr) {
    return;
  }

  if (gso->free_operator_data != nullptr) {
    gso->free_operator_data(gso->operator_data);
  }

  ScrArea *area = gso->area;
  LinkData *link;

  ED_slider_destroy(C, gso->slider);

  for (link = static_cast<LinkData *>(gso->bezt_arr_list.first); link != nullptr;
       link = link->next) {
    tBeztCopyData *copy = static_cast<tBeztCopyData *>(link->data);
    MEM_freeN(copy->bezt);
    MEM_freeN(link->data);
  }

  BLI_freelistN(&gso->bezt_arr_list);
  MEM_freeN(gso);

  /* Return to normal cursor and header status. */
  WM_cursor_modal_restore(win);
  ED_area_status_text(area, nullptr);

  /* cleanup */
  op->customdata = nullptr;
}

static int graph_slider_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  tGraphSliderOp *gso = static_cast<tGraphSliderOp *>(op->customdata);

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

        WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);

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
        applyNumInput(&gso->num, &value);

        /* Grab percentage from numeric input, and store this new value for redo
         * NOTE: users see ints, while internally we use a 0-1 float. */
        if (ED_slider_mode_get(gso->slider) == SLIDER_MODE_PERCENT) {
          value = value / 100.0f;
        }
        ED_slider_factor_set(gso->slider, value);
        RNA_property_float_set(op->ptr, gso->factor_prop, value);

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
  gso = static_cast<tGraphSliderOp *>(
      op->customdata = MEM_callocN(sizeof(tGraphSliderOp), "tGraphSliderOp"));

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

  if (gso->bezt_arr_list.first == nullptr) {
    WM_report(RPT_ERROR, "Cannot find keys to operate on");
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

enum tDecimModes {
  DECIM_RATIO = 1,
  DECIM_ERROR,
};

static void decimate_graph_keys(bAnimContext *ac, float factor, float error_sq_max)
{
  ListBase anim_data = {nullptr, nullptr};

  /* Filter data. */
  ANIM_animdata_filter(
      ac, &anim_data, OPERATOR_DATA_FILTER, ac->data, eAnimCont_Types(ac->datatype));

  /* Loop through filtered data and clean curves. */
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
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

  STRNCPY(mode_str, TIP_("Decimate Keyframes"));

  if (hasNumInput(&gso->num)) {
    char str_ofs[NUM_STR_REP_LEN];

    outputNumInput(&gso->num, str_ofs, &gso->scene->unit);

    SNPRINTF(status_str, "%s: %s", mode_str, str_ofs);
  }
  else {
    SNPRINTF(status_str, "%s: %s", mode_str, slider_string);
  }

  ED_workspace_status_text(C, status_str);
}

static void decimate_modal_update(bContext *C, wmOperator *op)
{
  /* Perform decimate updates - in response to some user action
   * (e.g. pressing a key or moving the mouse). */
  tGraphSliderOp *gso = static_cast<tGraphSliderOp *>(op->customdata);

  decimate_draw_status(C, gso);

  /* Reset keyframe data (so we get back to the original state). */
  reset_bezts(gso);

  /* Apply... */
  const float factor = slider_factor_get_and_remember(op);
  /* We don't want to limit the decimation to a certain error margin. */
  const float error_sq_max = FLT_MAX;
  decimate_graph_keys(&gso->ac, factor, error_sq_max);
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);
}

static int decimate_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const int invoke_result = graph_slider_invoke(C, op, event);

  if (invoke_result == OPERATOR_CANCELLED) {
    return OPERATOR_CANCELLED;
  }

  tGraphSliderOp *gso = static_cast<tGraphSliderOp *>(op->customdata);
  gso->factor_prop = RNA_struct_find_property(op->ptr, "factor");
  gso->modal_update = decimate_modal_update;
  ED_slider_allow_overshoot_set(gso->slider, false, false);

  return invoke_result;
}

static int decimate_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  tDecimModes mode = tDecimModes(RNA_enum_get(op->ptr, "mode"));
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
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

static bool decimate_poll_property(const bContext * /*C*/, wmOperator *op, const PropertyRNA *prop)
{
  const char *prop_id = RNA_property_identifier(prop);
  const int mode = RNA_enum_get(op->ptr, "mode");

  if (STREQ(prop_id, "factor") && mode != DECIM_RATIO) {
    return false;
  }
  if (STREQ(prop_id, "remove_error_margin") && mode != DECIM_ERROR) {
    return false;
  }

  return true;
}

static std::string decimate_desc(bContext * /*C*/, wmOperatorType * /*op*/, PointerRNA *ptr)
{

  if (RNA_enum_get(ptr, "mode") == DECIM_ERROR) {
    return TIP_(
        "Decimate F-Curves by specifying how much they can deviate from the original curve");
  }

  /* Use default description. */
  return "";
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
    {0, nullptr, 0, nullptr, nullptr},
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
/** \name Blend to Neighbor Operator
 * \{ */

static void blend_to_neighbor_graph_keys(bAnimContext *ac, const float factor)
{
  apply_fcu_segment_function(ac, factor, blend_to_neighbor_fcurve_segment);
}

static void blend_to_neighbor_modal_update(bContext *C, wmOperator *op)
{
  tGraphSliderOp *gso = static_cast<tGraphSliderOp *>(op->customdata);

  common_draw_status_header(C, gso, "Blend to Neighbor");

  /* Reset keyframe data to the state at invoke. */
  reset_bezts(gso);

  const float factor = slider_factor_get_and_remember(op);
  blend_to_neighbor_graph_keys(&gso->ac, factor);

  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);
}

static int blend_to_neighbor_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const int invoke_result = graph_slider_invoke(C, op, event);

  if (invoke_result == OPERATOR_CANCELLED) {
    return invoke_result;
  }

  tGraphSliderOp *gso = static_cast<tGraphSliderOp *>(op->customdata);
  gso->modal_update = blend_to_neighbor_modal_update;
  gso->factor_prop = RNA_struct_find_property(op->ptr, "factor");
  common_draw_status_header(C, gso, "Blend to Neighbor");
  ED_slider_factor_bounds_set(gso->slider, -1, 1);
  ED_slider_factor_set(gso->slider, 0.0f);

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
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GRAPH_OT_blend_to_neighbor(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Blend to Neighbor";
  ot->idname = "GRAPH_OT_blend_to_neighbor";
  ot->description = "Blend selected keyframes to their left or right neighbor";

  /* API callbacks. */
  ot->invoke = blend_to_neighbor_invoke;
  ot->modal = graph_slider_modal;
  ot->exec = blend_to_neighbor_exec;
  ot->poll = graphop_editable_keyframes_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR_X;

  RNA_def_float_factor(ot->srna,
                       "factor",
                       0.0f,
                       -FLT_MAX,
                       FLT_MAX,
                       "Blend",
                       "The blend factor with 0 being the current frame",
                       -1.0f,
                       1.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Breakdown Operator
 * \{ */

static void breakdown_graph_keys(bAnimContext *ac, float factor)
{
  apply_fcu_segment_function(ac, factor, breakdown_fcurve_segment);
}

static void breakdown_modal_update(bContext *C, wmOperator *op)
{
  tGraphSliderOp *gso = static_cast<tGraphSliderOp *>(op->customdata);

  common_draw_status_header(C, gso, "Breakdown");

  /* Reset keyframe data to the state at invoke. */
  reset_bezts(gso);
  const float factor = slider_factor_get_and_remember(op);
  breakdown_graph_keys(&gso->ac, factor);
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);
}

static int breakdown_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const int invoke_result = graph_slider_invoke(C, op, event);

  if (invoke_result == OPERATOR_CANCELLED) {
    return invoke_result;
  }

  tGraphSliderOp *gso = static_cast<tGraphSliderOp *>(op->customdata);
  gso->modal_update = breakdown_modal_update;
  gso->factor_prop = RNA_struct_find_property(op->ptr, "factor");
  common_draw_status_header(C, gso, "Breakdown");
  ED_slider_factor_bounds_set(gso->slider, -1, 1);
  ED_slider_factor_set(gso->slider, 0.0f);

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
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);

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
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR_X;

  RNA_def_float_factor(ot->srna,
                       "factor",
                       0.0f,
                       -FLT_MAX,
                       FLT_MAX,
                       "Factor",
                       "Favor either the left or the right key",
                       -1.0f,
                       1.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Blend to Default Value Operator
 * \{ */

static void blend_to_default_graph_keys(bAnimContext *ac, const float factor)
{
  ListBase anim_data = {nullptr, nullptr};
  ANIM_animdata_filter(
      ac, &anim_data, OPERATOR_DATA_FILTER, ac->data, eAnimCont_Types(ac->datatype));

  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    FCurve *fcu = (FCurve *)ale->key_data;

    /* Check if the curves actually have any points. */
    if (fcu == nullptr || fcu->bezt == nullptr || fcu->totvert == 0) {
      continue;
    }

    PointerRNA id_ptr;
    RNA_id_pointer_create(ale->id, &id_ptr);

    blend_to_default_fcurve(&id_ptr, fcu, factor);
    ale->update |= ANIM_UPDATE_DEFAULT;
  }

  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

static void blend_to_default_modal_update(bContext *C, wmOperator *op)
{
  tGraphSliderOp *gso = static_cast<tGraphSliderOp *>(op->customdata);

  common_draw_status_header(C, gso, "Blend to Default Value");

  /* Set notifier that keyframes have changed. */
  reset_bezts(gso);
  const float factor = ED_slider_factor_get(gso->slider);
  RNA_property_float_set(op->ptr, gso->factor_prop, factor);
  blend_to_default_graph_keys(&gso->ac, factor);
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);
}

static int blend_to_default_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const int invoke_result = graph_slider_invoke(C, op, event);

  if (invoke_result == OPERATOR_CANCELLED) {
    return invoke_result;
  }

  tGraphSliderOp *gso = static_cast<tGraphSliderOp *>(op->customdata);
  gso->modal_update = blend_to_default_modal_update;
  gso->factor_prop = RNA_struct_find_property(op->ptr, "factor");
  common_draw_status_header(C, gso, "Blend to Default Value");
  ED_slider_factor_set(gso->slider, 0.0f);

  return invoke_result;
}

static int blend_to_default_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;

  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  const float factor = RNA_float_get(op->ptr, "factor");

  blend_to_default_graph_keys(&ac, factor);

  /* Set notifier that keyframes have changed. */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GRAPH_OT_blend_to_default(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Blend to Default Value";
  ot->idname = "GRAPH_OT_blend_to_default";
  ot->description = "Blend selected keys to their default value from their current position";

  /* API callbacks. */
  ot->invoke = blend_to_default_invoke;
  ot->modal = graph_slider_modal;
  ot->exec = blend_to_default_exec;
  ot->poll = graphop_editable_keyframes_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR_X;

  RNA_def_float_factor(ot->srna,
                       "factor",
                       0.0f,
                       -FLT_MAX,
                       FLT_MAX,
                       "Factor",
                       "How much to blend to the default value",
                       0.0f,
                       1.0f);
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Ease Operator
 * \{ */

static void ease_graph_keys(bAnimContext *ac, const float factor)
{
  apply_fcu_segment_function(ac, factor, ease_fcurve_segment);
}

static void ease_modal_update(bContext *C, wmOperator *op)
{
  tGraphSliderOp *gso = static_cast<tGraphSliderOp *>(op->customdata);

  common_draw_status_header(C, gso, "Ease Keys");

  /* Reset keyframes to the state at invoke. */
  reset_bezts(gso);
  const float factor = slider_factor_get_and_remember(op);
  ease_graph_keys(&gso->ac, factor);
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);
}

static int ease_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const int invoke_result = graph_slider_invoke(C, op, event);

  if (invoke_result == OPERATOR_CANCELLED) {
    return invoke_result;
  }

  tGraphSliderOp *gso = static_cast<tGraphSliderOp *>(op->customdata);
  gso->modal_update = ease_modal_update;
  gso->factor_prop = RNA_struct_find_property(op->ptr, "factor");
  common_draw_status_header(C, gso, "Ease Keys");
  ED_slider_factor_bounds_set(gso->slider, -1, 1);
  ED_slider_factor_set(gso->slider, 0.0f);

  return invoke_result;
}

static int ease_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  const float factor = RNA_float_get(op->ptr, "factor");

  ease_graph_keys(&ac, factor);

  /* Set notifier that keyframes have changed. */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GRAPH_OT_ease(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Ease Keyframes";
  ot->idname = "GRAPH_OT_ease";
  ot->description = "Align keyframes on a ease-in or ease-out curve";

  /* API callbacks. */
  ot->invoke = ease_invoke;
  ot->modal = graph_slider_modal;
  ot->exec = ease_exec;
  ot->poll = graphop_editable_keyframes_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR_X;

  RNA_def_float_factor(ot->srna,
                       "factor",
                       0.0f,
                       -FLT_MAX,
                       FLT_MAX,
                       "Curve Bend",
                       "Control the bend of the curve",
                       -1.0f,
                       1.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Blend Offset Operator
 * \{ */

static void blend_offset_graph_keys(bAnimContext *ac, const float factor)
{
  apply_fcu_segment_function(ac, factor, blend_offset_fcurve_segment);
}

static void blend_offset_draw_status_header(bContext *C, tGraphSliderOp *gso)
{
  common_draw_status_header(C, gso, "Blend Offset Keys");
}

static void blend_offset_modal_update(bContext *C, wmOperator *op)
{
  tGraphSliderOp *gso = static_cast<tGraphSliderOp *>(op->customdata);

  blend_offset_draw_status_header(C, gso);

  /* Reset keyframes to the state at invoke. */
  reset_bezts(gso);
  const float factor = slider_factor_get_and_remember(op);
  blend_offset_graph_keys(&gso->ac, factor);
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);
}

static int blend_offset_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const int invoke_result = graph_slider_invoke(C, op, event);

  if (invoke_result == OPERATOR_CANCELLED) {
    return invoke_result;
  }

  tGraphSliderOp *gso = static_cast<tGraphSliderOp *>(op->customdata);
  gso->modal_update = blend_offset_modal_update;
  gso->factor_prop = RNA_struct_find_property(op->ptr, "factor");
  blend_offset_draw_status_header(C, gso);
  ED_slider_factor_bounds_set(gso->slider, -1, 1);
  ED_slider_factor_set(gso->slider, 0.0f);

  return invoke_result;
}

static int blend_offset_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  const float factor = RNA_float_get(op->ptr, "factor");

  blend_offset_graph_keys(&ac, factor);

  /* Set notifier that keyframes have changed. */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GRAPH_OT_blend_offset(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Blend Offset Keyframes";
  ot->idname = "GRAPH_OT_blend_offset";
  ot->description = "Shift selected keys to the value of the neighboring keys as a block";

  /* API callbacks. */
  ot->invoke = blend_offset_invoke;
  ot->modal = graph_slider_modal;
  ot->exec = blend_offset_exec;
  ot->poll = graphop_editable_keyframes_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_float_factor(ot->srna,
                       "factor",
                       0.0f,
                       -FLT_MAX,
                       FLT_MAX,
                       "Offset Factor",
                       "Control which key to offset towards and how far",
                       -1.0f,
                       1.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Blend to Ease Operator
 * \{ */

static void blend_to_ease_graph_keys(bAnimContext *ac, const float factor)
{
  apply_fcu_segment_function(ac, factor, blend_to_ease_fcurve_segment);
}

static void blend_to_ease_draw_status_header(bContext *C, tGraphSliderOp *gso)
{
  common_draw_status_header(C, gso, "Blend to Ease Keys");
}

static void blend_to_ease_modal_update(bContext *C, wmOperator *op)
{
  tGraphSliderOp *gso = static_cast<tGraphSliderOp *>(op->customdata);

  blend_to_ease_draw_status_header(C, gso);

  /* Reset keyframes to the state at invoke. */
  reset_bezts(gso);
  const float factor = slider_factor_get_and_remember(op);
  blend_to_ease_graph_keys(&gso->ac, factor);
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);
}

static int blend_to_ease_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const int invoke_result = graph_slider_invoke(C, op, event);

  if (invoke_result == OPERATOR_CANCELLED) {
    return invoke_result;
  }

  tGraphSliderOp *gso = static_cast<tGraphSliderOp *>(op->customdata);
  gso->modal_update = blend_to_ease_modal_update;
  gso->factor_prop = RNA_struct_find_property(op->ptr, "factor");
  blend_to_ease_draw_status_header(C, gso);
  ED_slider_allow_overshoot_set(gso->slider, false, false);
  ED_slider_factor_bounds_set(gso->slider, -1, 1);
  ED_slider_factor_set(gso->slider, 0.0f);

  return invoke_result;
}

static int blend_to_ease_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  const float factor = RNA_float_get(op->ptr, "factor");

  blend_to_ease_graph_keys(&ac, factor);

  /* Set notifier that keyframes have changed. */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GRAPH_OT_blend_to_ease(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Blend to Ease Keyframes";
  ot->idname = "GRAPH_OT_blend_to_ease";
  ot->description = "Blends keyframes from current state to an ease-in or ease-out curve";

  /* API callbacks. */
  ot->invoke = blend_to_ease_invoke;
  ot->modal = graph_slider_modal;
  ot->exec = blend_to_ease_exec;
  ot->poll = graphop_editable_keyframes_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_float_factor(ot->srna,
                       "factor",
                       0.0f,
                       -FLT_MAX,
                       FLT_MAX,
                       "Blend",
                       "Favor either original data or ease curve",
                       -1.0f,
                       1.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Match Slope
 * \{ */

static void match_slope_graph_keys(bAnimContext *ac, const float factor)
{
  ListBase anim_data = {nullptr, nullptr};

  bool all_segments_valid = true;

  ANIM_animdata_filter(
      ac, &anim_data, OPERATOR_DATA_FILTER, ac->data, eAnimCont_Types(ac->datatype));
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    FCurve *fcu = (FCurve *)ale->key_data;
    ListBase segments = find_fcurve_segments(fcu);

    LISTBASE_FOREACH (FCurveSegment *, segment, &segments) {
      all_segments_valid = match_slope_fcurve_segment(fcu, segment, factor);
    }

    ale->update |= ANIM_UPDATE_DEFAULT;
    BLI_freelistN(&segments);
  }

  if (!all_segments_valid) {
    if (factor >= 0) {
      WM_report(RPT_WARNING, "You need at least 2 keys to the right side of the selection");
    }
    else {
      WM_report(RPT_WARNING, "You need at least 2 keys to the left side of the selection");
    }
  }

  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

static void match_slope_draw_status_header(bContext *C, tGraphSliderOp *gso)
{
  common_draw_status_header(C, gso, "Match Slope");
}

static void match_slope_modal_update(bContext *C, wmOperator *op)
{
  tGraphSliderOp *gso = static_cast<tGraphSliderOp *>(op->customdata);

  match_slope_draw_status_header(C, gso);

  /* Reset keyframes to the state at invoke. */
  reset_bezts(gso);
  const float factor = slider_factor_get_and_remember(op);
  match_slope_graph_keys(&gso->ac, factor);
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);
}

static int match_slope_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const int invoke_result = graph_slider_invoke(C, op, event);

  if (invoke_result == OPERATOR_CANCELLED) {
    return invoke_result;
  }

  tGraphSliderOp *gso = static_cast<tGraphSliderOp *>(op->customdata);
  gso->modal_update = match_slope_modal_update;
  gso->factor_prop = RNA_struct_find_property(op->ptr, "factor");
  match_slope_draw_status_header(C, gso);
  ED_slider_allow_overshoot_set(gso->slider, false, false);
  ED_slider_factor_bounds_set(gso->slider, -1, 1);
  ED_slider_factor_set(gso->slider, 0.0f);

  return invoke_result;
}

static int match_slope_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  const float factor = RNA_float_get(op->ptr, "factor");

  match_slope_graph_keys(&ac, factor);

  /* Set notifier that keyframes have changed. */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GRAPH_OT_match_slope(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Match Slope";
  ot->idname = "GRAPH_OT_match_slope";
  ot->description = "Blend selected keys to the slope of neighboring ones";

  /* API callbacks. */
  ot->invoke = match_slope_invoke;
  ot->modal = graph_slider_modal;
  ot->exec = match_slope_exec;
  ot->poll = graphop_editable_keyframes_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_float_factor(ot->srna,
                       "factor",
                       0.0f,
                       -FLT_MAX,
                       FLT_MAX,
                       "Factor",
                       "Defines which keys to use as slope and how much to blend towards them",
                       -1.0f,
                       1.0f);
}

/** \} */
/* -------------------------------------------------------------------- */
/** \name Gauss Smooth Operator
 * \{ */

/* It is necessary to store data for smoothing when running in modal, because the sampling of
 * FCurves shouldn't be done on every update. */
struct tGaussOperatorData {
  double *kernel;
  ListBase segment_links; /* tFCurveSegmentLink */
  ListBase anim_data;     /* bAnimListElem */
};

/* Store data to smooth an FCurve segment. */
struct tFCurveSegmentLink {
  tFCurveSegmentLink *next, *prev;
  FCurve *fcu;
  FCurveSegment *segment;
  float *samples; /* Array of y-values of the FCurve segment. */
  int sample_count;
};

static void gaussian_smooth_allocate_operator_data(tGraphSliderOp *gso,
                                                   const int filter_width,
                                                   const float sigma)
{
  tGaussOperatorData *operator_data = static_cast<tGaussOperatorData *>(
      MEM_callocN(sizeof(tGaussOperatorData), "tGaussOperatorData"));
  const int kernel_size = filter_width + 1;
  double *kernel = static_cast<double *>(
      MEM_callocN(sizeof(double) * kernel_size, "Gauss Kernel"));
  ED_ANIM_get_1d_gauss_kernel(sigma, kernel_size, kernel);
  operator_data->kernel = kernel;

  ListBase anim_data = {nullptr, nullptr};
  ANIM_animdata_filter(
      &gso->ac, &anim_data, OPERATOR_DATA_FILTER, gso->ac.data, eAnimCont_Types(gso->ac.datatype));

  ListBase segment_links = {nullptr, nullptr};
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    FCurve *fcu = (FCurve *)ale->key_data;
    ListBase fcu_segments = find_fcurve_segments(fcu);
    LISTBASE_FOREACH (FCurveSegment *, segment, &fcu_segments) {
      tFCurveSegmentLink *segment_link = static_cast<tFCurveSegmentLink *>(
          MEM_callocN(sizeof(tFCurveSegmentLink), "FCurve Segment Link"));
      segment_link->fcu = fcu;
      segment_link->segment = segment;
      BezTriple left_bezt = fcu->bezt[segment->start_index];
      BezTriple right_bezt = fcu->bezt[segment->start_index + segment->length - 1];
      const int sample_count = int(right_bezt.vec[1][0] - left_bezt.vec[1][0]) +
                               (filter_width * 2 + 1);
      float *samples = static_cast<float *>(
          MEM_callocN(sizeof(float) * sample_count, "Smooth FCurve Op Samples"));
      sample_fcurve_segment(fcu, left_bezt.vec[1][0] - filter_width, 1, samples, sample_count);
      segment_link->samples = samples;
      BLI_addtail(&segment_links, segment_link);
    }
  }

  operator_data->anim_data = anim_data;
  operator_data->segment_links = segment_links;
  gso->operator_data = operator_data;
}

static void gaussian_smooth_free_operator_data(void *operator_data)
{
  tGaussOperatorData *gauss_data = (tGaussOperatorData *)operator_data;
  LISTBASE_FOREACH (tFCurveSegmentLink *, segment_link, &gauss_data->segment_links) {
    MEM_freeN(segment_link->samples);
    MEM_freeN(segment_link->segment);
  }
  MEM_freeN(gauss_data->kernel);
  BLI_freelistN(&gauss_data->segment_links);
  ANIM_animdata_freelist(&gauss_data->anim_data);
  MEM_freeN(gauss_data);
}

static void gaussian_smooth_modal_update(bContext *C, wmOperator *op)
{
  tGraphSliderOp *gso = static_cast<tGraphSliderOp *>(op->customdata);

  bAnimContext ac;

  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return;
  }

  common_draw_status_header(C, gso, "Gaussian Smooth");

  const float factor = slider_factor_get_and_remember(op);
  tGaussOperatorData *operator_data = (tGaussOperatorData *)gso->operator_data;
  const int filter_width = RNA_int_get(op->ptr, "filter_width");

  LISTBASE_FOREACH (tFCurveSegmentLink *, segment, &operator_data->segment_links) {
    smooth_fcurve_segment(segment->fcu,
                          segment->segment,
                          segment->samples,
                          factor,
                          filter_width,
                          operator_data->kernel);
  }

  LISTBASE_FOREACH (bAnimListElem *, ale, &operator_data->anim_data) {
    ale->update |= ANIM_UPDATE_DEFAULT;
  }

  ANIM_animdata_update(&ac, &operator_data->anim_data);
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);
}

static int gaussian_smooth_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const int invoke_result = graph_slider_invoke(C, op, event);

  if (invoke_result == OPERATOR_CANCELLED) {
    return invoke_result;
  }

  tGraphSliderOp *gso = static_cast<tGraphSliderOp *>(op->customdata);
  gso->modal_update = gaussian_smooth_modal_update;
  gso->factor_prop = RNA_struct_find_property(op->ptr, "factor");

  const float sigma = RNA_float_get(op->ptr, "sigma");
  const int filter_width = RNA_int_get(op->ptr, "filter_width");

  gaussian_smooth_allocate_operator_data(gso, filter_width, sigma);
  gso->free_operator_data = gaussian_smooth_free_operator_data;

  ED_slider_allow_overshoot_set(gso->slider, false, false);
  ED_slider_factor_set(gso->slider, 0.0f);
  common_draw_status_header(C, gso, "Gaussian Smooth");

  return invoke_result;
}

static void gaussian_smooth_graph_keys(bAnimContext *ac,
                                       const float factor,
                                       double *kernel,
                                       const int filter_width)
{
  ListBase anim_data = {nullptr, nullptr};
  ANIM_animdata_filter(
      ac, &anim_data, OPERATOR_DATA_FILTER, ac->data, eAnimCont_Types(ac->datatype));

  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    FCurve *fcu = (FCurve *)ale->key_data;
    ListBase segments = find_fcurve_segments(fcu);

    LISTBASE_FOREACH (FCurveSegment *, segment, &segments) {
      BezTriple left_bezt = fcu->bezt[segment->start_index];
      BezTriple right_bezt = fcu->bezt[segment->start_index + segment->length - 1];
      const int sample_count = int(right_bezt.vec[1][0] - left_bezt.vec[1][0]) +
                               (filter_width * 2 + 1);
      float *samples = static_cast<float *>(
          MEM_callocN(sizeof(float) * sample_count, "Smooth FCurve Op Samples"));
      sample_fcurve_segment(fcu, left_bezt.vec[1][0] - filter_width, 1, samples, sample_count);
      smooth_fcurve_segment(fcu, segment, samples, factor, filter_width, kernel);
      MEM_freeN(samples);
    }

    BLI_freelistN(&segments);
    ale->update |= ANIM_UPDATE_DEFAULT;
  }

  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

static int gaussian_smooth_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;

  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }
  const float factor = RNA_float_get(op->ptr, "factor");
  const int filter_width = RNA_int_get(op->ptr, "filter_width");
  const int kernel_size = filter_width + 1;
  double *kernel = static_cast<double *>(
      MEM_callocN(sizeof(double) * kernel_size, "Gauss Kernel"));
  ED_ANIM_get_1d_gauss_kernel(RNA_float_get(op->ptr, "sigma"), kernel_size, kernel);

  gaussian_smooth_graph_keys(&ac, factor, kernel, filter_width);

  MEM_freeN(kernel);

  /* Set notifier that keyframes have changed. */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GRAPH_OT_gaussian_smooth(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Gaussian Smooth";
  ot->idname = "GRAPH_OT_gaussian_smooth";
  ot->description = "Smooth the curve using a Gaussian filter";

  /* API callbacks. */
  ot->invoke = gaussian_smooth_invoke;
  ot->modal = graph_slider_modal;
  ot->exec = gaussian_smooth_exec;
  ot->poll = graphop_editable_keyframes_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_float_factor(ot->srna,
                       "factor",
                       1.0f,
                       0.0f,
                       FLT_MAX,
                       "Factor",
                       "How much to blend to the default value",
                       0.0f,
                       1.0f);

  RNA_def_float(ot->srna,
                "sigma",
                0.33f,
                0.001f,
                FLT_MAX,
                "Sigma",
                "The shape of the gaussian distribution, lower values make it sharper",
                0.001f,
                100.0f);

  RNA_def_int(ot->srna,
              "filter_width",
              6,
              1,
              64,
              "Filter Width",
              "How far to each side the operator will average the key values",
              1,
              32);
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Butterworth Smooth Operator
 * \{ */

struct tBtwOperatorData {
  ButterworthCoefficients *coefficients;
  ListBase segment_links; /* tFCurveSegmentLink */
  ListBase anim_data;     /* bAnimListElem */
};

static int btw_calculate_sample_count(BezTriple *right_bezt,
                                      BezTriple *left_bezt,
                                      const int filter_order,
                                      const int samples_per_frame)
{
  /* Adding a constant 60 frames to combat the issue that the phase delay is shifting data out of
   * the sample count range. This becomes an issue when running the filter backwards. */
  const int sample_count = (int(right_bezt->vec[1][0] - left_bezt->vec[1][0]) + 1 +
                            (filter_order * 2)) *
                               samples_per_frame +
                           60;
  return sample_count;
}

static void btw_smooth_allocate_operator_data(tGraphSliderOp *gso,
                                              const int filter_order,
                                              const int samples_per_frame)
{
  tBtwOperatorData *operator_data = static_cast<tBtwOperatorData *>(
      MEM_callocN(sizeof(tBtwOperatorData), "tBtwOperatorData"));

  operator_data->coefficients = ED_anim_allocate_butterworth_coefficients(filter_order);

  ListBase anim_data = {nullptr, nullptr};
  ANIM_animdata_filter(
      &gso->ac, &anim_data, OPERATOR_DATA_FILTER, gso->ac.data, eAnimCont_Types(gso->ac.datatype));

  ListBase segment_links = {nullptr, nullptr};
  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    FCurve *fcu = (FCurve *)ale->key_data;
    ListBase fcu_segments = find_fcurve_segments(fcu);

    LISTBASE_FOREACH (FCurveSegment *, segment, &fcu_segments) {

      tFCurveSegmentLink *segment_link = static_cast<tFCurveSegmentLink *>(
          MEM_callocN(sizeof(tFCurveSegmentLink), "FCurve Segment Link"));
      segment_link->fcu = fcu;
      segment_link->segment = segment;
      BezTriple left_bezt = fcu->bezt[segment->start_index];
      BezTriple right_bezt = fcu->bezt[segment->start_index + segment->length - 1];
      const int sample_count = btw_calculate_sample_count(
          &right_bezt, &left_bezt, filter_order, samples_per_frame);
      float *samples = static_cast<float *>(
          MEM_callocN(sizeof(float) * sample_count, "Btw Smooth FCurve Op Samples"));
      sample_fcurve_segment(
          fcu, left_bezt.vec[1][0] - filter_order, samples_per_frame, samples, sample_count);
      segment_link->samples = samples;
      segment_link->sample_count = sample_count;
      BLI_addtail(&segment_links, segment_link);
    }
  }

  operator_data->anim_data = anim_data;
  operator_data->segment_links = segment_links;
  gso->operator_data = operator_data;
}

static void btw_smooth_free_operator_data(void *operator_data)
{
  tBtwOperatorData *btw_data = (tBtwOperatorData *)operator_data;
  LISTBASE_FOREACH (tFCurveSegmentLink *, segment_link, &btw_data->segment_links) {
    MEM_freeN(segment_link->samples);
    MEM_freeN(segment_link->segment);
  }
  ED_anim_free_butterworth_coefficients(btw_data->coefficients);
  BLI_freelistN(&btw_data->segment_links);
  ANIM_animdata_freelist(&btw_data->anim_data);
  MEM_freeN(btw_data);
}

static void btw_smooth_modal_update(bContext *C, wmOperator *op)
{
  tGraphSliderOp *gso = static_cast<tGraphSliderOp *>(op->customdata);

  bAnimContext ac;

  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return;
  }

  common_draw_status_header(C, gso, "Butterworth Smooth");

  tBtwOperatorData *operator_data = (tBtwOperatorData *)gso->operator_data;

  const float frame_rate = float(ac.scene->r.frs_sec) / ac.scene->r.frs_sec_base;
  const int samples_per_frame = RNA_int_get(op->ptr, "samples_per_frame");
  const float sampling_frequency = frame_rate * samples_per_frame;

  const float cutoff_frequency = slider_factor_get_and_remember(op);
  const int blend_in_out = RNA_int_get(op->ptr, "blend_in_out");

  ED_anim_calculate_butterworth_coefficients(
      cutoff_frequency, sampling_frequency, operator_data->coefficients);

  LISTBASE_FOREACH (tFCurveSegmentLink *, segment, &operator_data->segment_links) {
    butterworth_smooth_fcurve_segment(segment->fcu,
                                      segment->segment,
                                      segment->samples,
                                      segment->sample_count,
                                      1,
                                      blend_in_out,
                                      samples_per_frame,
                                      operator_data->coefficients);
  }

  LISTBASE_FOREACH (bAnimListElem *, ale, &operator_data->anim_data) {
    ale->update |= ANIM_UPDATE_DEFAULT;
  }

  ANIM_animdata_update(&ac, &operator_data->anim_data);
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);
}

static int btw_smooth_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const int invoke_result = graph_slider_invoke(C, op, event);

  if (invoke_result == OPERATOR_CANCELLED) {
    return invoke_result;
  }

  tGraphSliderOp *gso = static_cast<tGraphSliderOp *>(op->customdata);
  gso->modal_update = btw_smooth_modal_update;
  gso->factor_prop = RNA_struct_find_property(op->ptr, "cutoff_frequency");

  const int filter_order = RNA_int_get(op->ptr, "filter_order");
  const int samples_per_frame = RNA_int_get(op->ptr, "samples_per_frame");

  btw_smooth_allocate_operator_data(gso, filter_order, samples_per_frame);
  gso->free_operator_data = btw_smooth_free_operator_data;

  const float frame_rate = float(gso->scene->r.frs_sec) / gso->scene->r.frs_sec_base;
  const float sampling_frequency = frame_rate * samples_per_frame;
  ED_slider_factor_bounds_set(gso->slider, 0, sampling_frequency / 2);
  ED_slider_factor_set(gso->slider, RNA_float_get(op->ptr, "cutoff_frequency"));
  ED_slider_allow_overshoot_set(gso->slider, false, false);
  ED_slider_mode_set(gso->slider, SLIDER_MODE_FLOAT);
  ED_slider_unit_set(gso->slider, "Hz");
  common_draw_status_header(C, gso, "Butterworth Smooth");

  return invoke_result;
}

static void btw_smooth_graph_keys(bAnimContext *ac,
                                  const float factor,
                                  const int blend_in_out,
                                  float cutoff_frequency,
                                  const int filter_order,
                                  const int samples_per_frame)
{
  ListBase anim_data = {nullptr, nullptr};
  ANIM_animdata_filter(
      ac, &anim_data, OPERATOR_DATA_FILTER, ac->data, eAnimCont_Types(ac->datatype));

  ButterworthCoefficients *bw_coeff = ED_anim_allocate_butterworth_coefficients(filter_order);

  const float frame_rate = float(ac->scene->r.frs_sec) / ac->scene->r.frs_sec_base;
  const float sampling_frequency = frame_rate * samples_per_frame;
  /* Clamp cutoff frequency to Nyquist Frequency. */
  cutoff_frequency = min_ff(cutoff_frequency, sampling_frequency / 2);
  ED_anim_calculate_butterworth_coefficients(cutoff_frequency, sampling_frequency, bw_coeff);

  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    FCurve *fcu = (FCurve *)ale->key_data;
    ListBase segments = find_fcurve_segments(fcu);

    LISTBASE_FOREACH (FCurveSegment *, segment, &segments) {
      BezTriple left_bezt = fcu->bezt[segment->start_index];
      BezTriple right_bezt = fcu->bezt[segment->start_index + segment->length - 1];
      const int sample_count = btw_calculate_sample_count(
          &right_bezt, &left_bezt, filter_order, samples_per_frame);
      float *samples = static_cast<float *>(
          MEM_callocN(sizeof(float) * sample_count, "Smooth FCurve Op Samples"));
      sample_fcurve_segment(
          fcu, left_bezt.vec[1][0] - filter_order, samples_per_frame, samples, sample_count);
      butterworth_smooth_fcurve_segment(
          fcu, segment, samples, sample_count, factor, blend_in_out, samples_per_frame, bw_coeff);
      MEM_freeN(samples);
    }

    BLI_freelistN(&segments);
    ale->update |= ANIM_UPDATE_DEFAULT;
  }

  ED_anim_free_butterworth_coefficients(bw_coeff);
  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

static int btw_smooth_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;

  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }
  const float blend = RNA_float_get(op->ptr, "blend");
  const float cutoff_frequency = RNA_float_get(op->ptr, "cutoff_frequency");
  const int filter_order = RNA_int_get(op->ptr, "filter_order");
  const int samples_per_frame = RNA_int_get(op->ptr, "samples_per_frame");
  const int blend_in_out = RNA_int_get(op->ptr, "blend_in_out");
  btw_smooth_graph_keys(
      &ac, blend, blend_in_out, cutoff_frequency, filter_order, samples_per_frame);

  /* Set notifier that keyframes have changed. */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void GRAPH_OT_butterworth_smooth(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Butterworth Smooth";
  ot->idname = "GRAPH_OT_butterworth_smooth";
  ot->description = "Smooth an F-Curve while maintaining the general shape of the curve";

  /* API callbacks. */
  ot->invoke = btw_smooth_invoke;
  ot->modal = graph_slider_modal;
  ot->exec = btw_smooth_exec;
  ot->poll = graphop_editable_keyframes_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR_X;

  RNA_def_float(ot->srna,
                "cutoff_frequency",
                3.0f,
                0.0f,
                FLT_MAX,
                "Frequency Cutoff (Hz)",
                "Lower values give a smoother curve",
                0.0f,
                FLT_MAX);

  RNA_def_int(ot->srna,
              "filter_order",
              4,
              1,
              32,
              "Filter Order",
              "Higher values produce a harder frequency cutoff",
              1,
              16);

  RNA_def_int(ot->srna,
              "samples_per_frame",
              1,
              1,
              64,
              "Samples per Frame",
              "How many samples to calculate per frame, helps with subframe data",
              1,
              16);

  RNA_def_float_factor(ot->srna,
                       "blend",
                       1.0f,
                       0,
                       FLT_MAX,
                       "Blend",
                       "How much to blend to the smoothed curve",
                       0.0f,
                       1.0f);

  RNA_def_int(ot->srna,
              "blend_in_out",
              1,
              0,
              INT_MAX,
              "Blend In/Out",
              "Linearly blend the smooth data to the border frames of the selection",
              0,
              128);
}
/** \} */
