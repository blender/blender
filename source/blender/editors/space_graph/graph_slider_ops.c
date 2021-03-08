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

#include "WM_api.h"
#include "WM_types.h"

#include "graph_intern.h"

/* ******************** GRAPH SLIDER OPERATORS ************************* */
/* This file contains a collection of operators to modify keyframes in the graph editor. All
 * operators are modal and use a slider that allows the user to define a percentage to modify the
 * operator.*/

/* ******************** Decimate Keyframes Operator ************************* */

static void decimate_graph_keys(bAnimContext *ac, float remove_ratio, float error_sq_max)
{
  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  /* Filter data. */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FOREDIT |
            ANIMFILTER_SEL | ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);

  /* Loop through filtered data and clean curves. */
  for (ale = anim_data.first; ale; ale = ale->next) {
    if (!decimate_fcurve(ale, remove_ratio, error_sq_max)) {
      /* The selection contains unsupported keyframe types! */
      WM_report(RPT_WARNING, "Decimate: Skipping non linear/bezier keyframes!");
    }

    ale->update |= ANIM_UPDATE_DEFAULT;
  }

  ANIM_animdata_update(ac, &anim_data);
  ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

/* This data type is only used for modal operation. */
typedef struct tDecimateGraphOp {
  bAnimContext ac;
  Scene *scene;
  ScrArea *area;
  ARegion *region;

  /** A 0-1 value for determining how much we should decimate. */
  PropertyRNA *percentage_prop;

  /** The original bezt curve data (used for restoring fcurves).*/
  ListBase bezt_arr_list;

  NumInput num;
} tDecimateGraphOp;

typedef struct tBeztCopyData {
  int tot_vert;
  BezTriple *bezt;
} tBeztCopyData;

typedef enum tDecimModes {
  DECIM_RATIO = 1,
  DECIM_ERROR,
} tDecimModes;

/* Overwrite the current bezts arrays with the original data. */
static void decimate_reset_bezts(tDecimateGraphOp *dgo)
{
  ListBase anim_data = {NULL, NULL};
  LinkData *link_bezt;
  bAnimListElem *ale;
  int filter;

  bAnimContext *ac = &dgo->ac;

  /* Filter data. */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FOREDIT |
            ANIMFILTER_SEL | ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);

  /* Loop through filtered data and reset bezts. */
  for (ale = anim_data.first, link_bezt = dgo->bezt_arr_list.first; ale; ale = ale->next) {
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

static void decimate_exit(bContext *C, wmOperator *op)
{
  tDecimateGraphOp *dgo = op->customdata;
  wmWindow *win = CTX_wm_window(C);

  /* If data exists, clear its data and exit. */
  if (dgo == NULL) {
    return;
  }

  ScrArea *area = dgo->area;
  LinkData *link;

  for (link = dgo->bezt_arr_list.first; link != NULL; link = link->next) {
    tBeztCopyData *copy = link->data;
    MEM_freeN(copy->bezt);
    MEM_freeN(link->data);
  }

  BLI_freelistN(&dgo->bezt_arr_list);
  MEM_freeN(dgo);

  /* Return to normal cursor and header status. */
  WM_cursor_modal_restore(win);
  ED_area_status_text(area, NULL);

  /* Cleanup. */
  op->customdata = NULL;
}

/* Draw a percentage indicator in header. */
static void decimate_draw_status_header(wmOperator *op, tDecimateGraphOp *dgo)
{
  char status_str[UI_MAX_DRAW_STR];
  char mode_str[32];

  strcpy(mode_str, TIP_("Decimate Keyframes"));

  if (hasNumInput(&dgo->num)) {
    char str_ofs[NUM_STR_REP_LEN];

    outputNumInput(&dgo->num, str_ofs, &dgo->scene->unit);

    BLI_snprintf(status_str, sizeof(status_str), "%s: %s", mode_str, str_ofs);
  }
  else {
    float percentage = RNA_property_float_get(op->ptr, dgo->percentage_prop);
    BLI_snprintf(
        status_str, sizeof(status_str), "%s: %d %%", mode_str, (int)(percentage * 100.0f));
  }

  ED_area_status_text(dgo->area, status_str);
}

/* Calculate percentage based on position of mouse (we only use x-axis for now.
 * Since this is more convenient for users to do), and store new percentage value.
 */
static void decimate_mouse_update_percentage(tDecimateGraphOp *dgo,
                                             wmOperator *op,
                                             const wmEvent *event)
{
  float percentage = (event->x - dgo->region->winrct.xmin) / ((float)dgo->region->winx);
  RNA_property_float_set(op->ptr, dgo->percentage_prop, percentage);
}

static int graphkeys_decimate_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  tDecimateGraphOp *dgo;

  WM_cursor_modal_set(CTX_wm_window(C), WM_CURSOR_EW_SCROLL);

  /* Init slide-op data. */
  dgo = op->customdata = MEM_callocN(sizeof(tDecimateGraphOp), "tDecimateGraphOp");

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &dgo->ac) == 0) {
    decimate_exit(C, op);
    return OPERATOR_CANCELLED;
  }

  dgo->percentage_prop = RNA_struct_find_property(op->ptr, "remove_ratio");

  dgo->scene = CTX_data_scene(C);
  dgo->area = CTX_wm_area(C);
  dgo->region = CTX_wm_region(C);

  /* Initialize percentage so that it will have the correct value before the first mouse move. */
  decimate_mouse_update_percentage(dgo, op, event);

  decimate_draw_status_header(op, dgo);

  /* Construct a list with the original bezt arrays so we can restore them during modal operation.
   */
  {
    ListBase anim_data = {NULL, NULL};
    bAnimContext *ac = &dgo->ac;
    bAnimListElem *ale;

    int filter;

    /* Filter data. */
    filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_FOREDIT |
              ANIMFILTER_SEL | ANIMFILTER_NODUPLIS);
    ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);

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

      BLI_addtail(&dgo->bezt_arr_list, link);
    }

    ANIM_animdata_freelist(&anim_data);
  }

  if (dgo->bezt_arr_list.first == NULL) {
    WM_report(RPT_WARNING,
              "Fcurve Decimate: Can't decimate baked channels. Unbake them and try again.");
    decimate_exit(C, op);
    return OPERATOR_CANCELLED;
  }

  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static void graphkeys_decimate_modal_update(bContext *C, wmOperator *op)
{
  /* Perform decimate updates - in response to some user action
   * (e.g. pressing a key or moving the mouse). */
  tDecimateGraphOp *dgo = op->customdata;

  decimate_draw_status_header(op, dgo);

  /* Reset keyframe data (so we get back to the original state). */
  decimate_reset_bezts(dgo);

  /* Apply... */
  float remove_ratio = RNA_property_float_get(op->ptr, dgo->percentage_prop);
  /* We don't want to limit the decimation to a certain error margin. */
  const float error_sq_max = FLT_MAX;
  decimate_graph_keys(&dgo->ac, remove_ratio, error_sq_max);
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
}

static int graphkeys_decimate_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  /* This assumes that we are in "DECIM_RATIO" mode. This is because the error margin is very hard
   * and finicky to control with this modal mouse grab method. Therefore, it is expected that the
   * error margin mode is not adjusted by the modal operator but instead tweaked via the redo
   * panel.*/
  tDecimateGraphOp *dgo = op->customdata;

  const bool has_numinput = hasNumInput(&dgo->num);

  switch (event->type) {
    case LEFTMOUSE: /* Confirm */
    case EVT_RETKEY:
    case EVT_PADENTER: {
      if (event->val == KM_PRESS) {
        decimate_exit(C, op);

        return OPERATOR_FINISHED;
      }
      break;
    }

    case EVT_ESCKEY: /* Cancel */
    case RIGHTMOUSE: {
      if (event->val == KM_PRESS) {
        decimate_reset_bezts(dgo);

        WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);

        decimate_exit(C, op);

        return OPERATOR_CANCELLED;
      }
      break;
    }

    /* Percentage Change... */
    case MOUSEMOVE: /* Calculate new position. */
    {
      if (has_numinput == false) {
        /* Update percentage based on position of mouse. */
        decimate_mouse_update_percentage(dgo, op, event);

        /* Update pose to reflect the new values. */
        graphkeys_decimate_modal_update(C, op);
      }
      break;
    }
    default: {
      if ((event->val == KM_PRESS) && handleNumInput(C, &dgo->num, event)) {
        float value;
        float percentage = RNA_property_float_get(op->ptr, dgo->percentage_prop);

        /* Grab percentage from numeric input, and store this new value for redo
         * NOTE: users see ints, while internally we use a 0-1 float.
         */
        value = percentage * 100.0f;
        applyNumInput(&dgo->num, &value);

        percentage = value / 100.0f;
        RNA_property_float_set(op->ptr, dgo->percentage_prop, percentage);

        /* Update decimate output to reflect the new values. */
        graphkeys_decimate_modal_update(C, op);
        break;
      }

      /* Unhandled event - maybe it was some view manipulation? */
      /* Allow to pass through. */
      return OPERATOR_RUNNING_MODAL | OPERATOR_PASS_THROUGH;
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

static int graphkeys_decimate_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;

  /* Get editor data. */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  tDecimModes mode = RNA_enum_get(op->ptr, "mode");
  /* We want to be able to work on all available keyframes. */
  float remove_ratio = 1.0f;
  /* We don't want to limit the decimation to a certain error margin. */
  float error_sq_max = FLT_MAX;

  switch (mode) {
    case DECIM_RATIO:
      remove_ratio = RNA_float_get(op->ptr, "remove_ratio");
      break;
    case DECIM_ERROR:
      error_sq_max = RNA_float_get(op->ptr, "remove_error_margin");
      /* The decimate algorithm expects the error to be squared. */
      error_sq_max *= error_sq_max;

      break;
  }

  if (remove_ratio == 0.0f || error_sq_max == 0.0f) {
    /* Nothing to remove. */
    return OPERATOR_FINISHED;
  }

  decimate_graph_keys(&ac, remove_ratio, error_sq_max);

  /* Set notifier that keyframes have changed. */
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

static bool graphkeys_decimate_poll_property(const bContext *UNUSED(C),
                                             wmOperator *op,
                                             const PropertyRNA *prop)
{
  const char *prop_id = RNA_property_identifier(prop);

  if (STRPREFIX(prop_id, "remove")) {
    int mode = RNA_enum_get(op->ptr, "mode");

    if (STREQ(prop_id, "remove_ratio") && mode != DECIM_RATIO) {
      return false;
    }
    if (STREQ(prop_id, "remove_error_margin") && mode != DECIM_ERROR) {
      return false;
    }
  }

  return true;
}

static char *graphkeys_decimate_desc(bContext *UNUSED(C),
                                     wmOperatorType *UNUSED(op),
                                     PointerRNA *ptr)
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
  ot->poll_property = graphkeys_decimate_poll_property;
  ot->get_description = graphkeys_decimate_desc;
  ot->invoke = graphkeys_decimate_invoke;
  ot->modal = graphkeys_decimate_modal;
  ot->exec = graphkeys_decimate_exec;
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
                       "remove_ratio",
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
