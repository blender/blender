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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spgraph
 */

#include <stdlib.h>
#include <math.h>

#include "DNA_scene_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_math_base.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_sound.h"

#include "UI_view2d.h"

#include "ED_anim_api.h"
#include "ED_markers.h"
#include "ED_screen.h"
#include "ED_select_utils.h"
#include "ED_transform.h"
#include "ED_object.h"

#include "graph_intern.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

/* ************************** view-based operators **********************************/
// XXX should these really be here?

/* Set Cursor --------------------------------------------------------------------- */
/* The 'cursor' in the Graph Editor consists of two parts:
 * 1) Current Frame Indicator (as per ANIM_OT_change_frame)
 * 2) Value Indicator (stored per Graph Editor instance)
 */

static bool graphview_cursor_poll(bContext *C)
{
  /* prevent changes during render */
  if (G.is_rendering) {
    return 0;
  }

  return ED_operator_graphedit_active(C);
}

/* Set the new frame number */
static void graphview_cursor_apply(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  SpaceGraph *sipo = CTX_wm_space_graph(C);
  /* this isn't technically "frame", but it'll do... */
  float frame = RNA_float_get(op->ptr, "frame");

  /* adjust the frame or the cursor x-value */
  if (sipo->mode == SIPO_MODE_DRIVERS) {
    /* adjust cursor x-value */
    sipo->cursorTime = frame;
  }
  else {
    /* adjust the frame
     * NOTE: sync this part of the code with ANIM_OT_change_frame
     */
    /* 1) frame is rounded to the nearest int, since frames are ints */
    CFRA = round_fl_to_int(frame);

    if (scene->r.flag & SCER_LOCK_FRAME_SELECTION) {
      /* Clip to preview range
       * NOTE: Preview range won't go into negative values,
       *       so only clamping once should be fine.
       */
      CLAMP(CFRA, PSFRA, PEFRA);
    }
    else {
      /* Prevent negative frames */
      FRAMENUMBER_MIN_CLAMP(CFRA);
    }

    SUBFRA = 0.0f;
    BKE_sound_update_and_seek(bmain, CTX_data_depsgraph(C));
  }

  /* set the cursor value */
  sipo->cursorVal = RNA_float_get(op->ptr, "value");

  /* send notifiers - notifiers for frame should force an update for both vars ok... */
  WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);
}

/* ... */

/* Non-modal callback for running operator without user input */
static int graphview_cursor_exec(bContext *C, wmOperator *op)
{
  graphview_cursor_apply(C, op);
  return OPERATOR_FINISHED;
}

/* ... */

/* set the operator properties from the initial event */
static void graphview_cursor_setprops(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *ar = CTX_wm_region(C);
  float viewx, viewy;

  /* abort if not active region (should not really be possible) */
  if (ar == NULL) {
    return;
  }

  /* convert from region coordinates to View2D 'tot' space */
  UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &viewx, &viewy);

  /* store the values in the operator properties */
  /* NOTE: we don't clamp frame here, as it might be used for the drivers cursor */
  RNA_float_set(op->ptr, "frame", viewx);
  RNA_float_set(op->ptr, "value", viewy);
}

/* Modal Operator init */
static int graphview_cursor_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  bScreen *screen = CTX_wm_screen(C);

  /* Change to frame that mouse is over before adding modal handler,
   * as user could click on a single frame (jump to frame) as well as
   * click-dragging over a range (modal scrubbing). Apply this change.
   */
  graphview_cursor_setprops(C, op, event);
  graphview_cursor_apply(C, op);

  /* Signal that a scrubbing operating is starting */
  if (screen) {
    screen->scrubbing = true;
  }

  /* add temp handler */
  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

/* Modal event handling of cursor changing */
static int graphview_cursor_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  bScreen *screen = CTX_wm_screen(C);
  Scene *scene = CTX_data_scene(C);

  /* execute the events */
  switch (event->type) {
    case ESCKEY:
      if (screen) {
        screen->scrubbing = false;
      }

      WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);
      return OPERATOR_FINISHED;

    case MOUSEMOVE:
      /* set the new values */
      graphview_cursor_setprops(C, op, event);
      graphview_cursor_apply(C, op);
      break;

    case LEFTMOUSE:
    case RIGHTMOUSE:
    case MIDDLEMOUSE:
      /* We check for either mouse-button to end, to work with all user keymaps. */
      if (event->val == KM_RELEASE) {
        if (screen) {
          screen->scrubbing = false;
        }

        WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);
        return OPERATOR_FINISHED;
      }
      break;
  }

  return OPERATOR_RUNNING_MODAL;
}

static void GRAPH_OT_cursor_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Cursor";
  ot->idname = "GRAPH_OT_cursor_set";
  ot->description = "Interactively set the current frame and value cursor";

  /* api callbacks */
  ot->exec = graphview_cursor_exec;
  ot->invoke = graphview_cursor_invoke;
  ot->modal = graphview_cursor_modal;
  ot->poll = graphview_cursor_poll;

  /* flags */
  ot->flag = OPTYPE_BLOCKING | OPTYPE_UNDO;

  /* rna */
  RNA_def_float(ot->srna, "frame", 0, MINAFRAMEF, MAXFRAMEF, "Frame", "", MINAFRAMEF, MAXFRAMEF);
  RNA_def_float(ot->srna, "value", 0, -FLT_MAX, FLT_MAX, "Value", "", -100.0f, 100.0f);
}

/* Hide/Reveal ------------------------------------------------------------ */

static int graphview_curves_hide_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  ListBase anim_data = {NULL, NULL};
  ListBase all_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;
  const bool unselected = RNA_boolean_get(op->ptr, "unselected");

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* get list of all channels that selection may need to be flushed to
   * - hierarchy must not affect what we have access to here...
   */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_CHANNELS | ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(&ac, &all_data, filter, ac.data, ac.datatype);

  /* filter data
   * - of the remaining visible curves, we want to hide the ones that are
   *   selected/unselected (depending on "unselected" prop)
   */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_CURVE_VISIBLE | ANIMFILTER_NODUPLIS);
  if (unselected) {
    filter |= ANIMFILTER_UNSEL;
  }
  else {
    filter |= ANIMFILTER_SEL;
  }

  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  for (ale = anim_data.first; ale; ale = ale->next) {
    /* hack: skip object channels for now, since flushing those will always flush everything,
     * but they are always included */
    /* TODO: find out why this is the case, and fix that */
    if (ale->type == ANIMTYPE_OBJECT) {
      continue;
    }

    /* change the hide setting, and unselect it... */
    ANIM_channel_setting_set(&ac, ale, ACHANNEL_SETTING_VISIBLE, ACHANNEL_SETFLAG_CLEAR);
    ANIM_channel_setting_set(&ac, ale, ACHANNEL_SETTING_SELECT, ACHANNEL_SETFLAG_CLEAR);

    /* now, also flush selection status up/down as appropriate */
    ANIM_flush_setting_anim_channels(
        &ac, &all_data, ale, ACHANNEL_SETTING_VISIBLE, ACHANNEL_SETFLAG_CLEAR);
  }

  /* cleanup */
  ANIM_animdata_freelist(&anim_data);
  BLI_freelistN(&all_data);

  /* unhide selected */
  if (unselected) {
    /* turn off requirement for visible */
    filter = ANIMFILTER_SEL | ANIMFILTER_NODUPLIS | ANIMFILTER_LIST_CHANNELS;

    /* flushing has been done */
    ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

    for (ale = anim_data.first; ale; ale = ale->next) {
      /* hack: skip object channels for now, since flushing those
       * will always flush everything, but they are always included */

      /* TODO: find out why this is the case, and fix that */
      if (ale->type == ANIMTYPE_OBJECT) {
        continue;
      }

      /* change the hide setting, and unselect it... */
      ANIM_channel_setting_set(&ac, ale, ACHANNEL_SETTING_VISIBLE, ACHANNEL_SETFLAG_ADD);
      ANIM_channel_setting_set(&ac, ale, ACHANNEL_SETTING_SELECT, ACHANNEL_SETFLAG_ADD);

      /* now, also flush selection status up/down as appropriate */
      ANIM_flush_setting_anim_channels(
          &ac, &anim_data, ale, ACHANNEL_SETTING_VISIBLE, ACHANNEL_SETFLAG_ADD);
    }
    ANIM_animdata_freelist(&anim_data);
  }

  /* send notifier that things have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

static void GRAPH_OT_hide(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Hide Curves";
  ot->idname = "GRAPH_OT_hide";
  ot->description = "Hide selected curves from Graph Editor view";

  /* api callbacks */
  ot->exec = graphview_curves_hide_exec;
  ot->poll = ED_operator_graphedit_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  RNA_def_boolean(
      ot->srna, "unselected", 0, "Unselected", "Hide unselected rather than selected curves");
}

/* ........ */

static int graphview_curves_reveal_exec(bContext *C, wmOperator *op)
{
  bAnimContext ac;
  ListBase anim_data = {NULL, NULL};
  ListBase all_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;
  const bool select = RNA_boolean_get(op->ptr, "select");

  /* get editor data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* get list of all channels that selection may need to be flushed to
   * - hierarchy must not affect what we have access to here...
   */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_CHANNELS | ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(&ac, &all_data, filter, ac.data, ac.datatype);

  /* filter data
   * - just go through all visible channels, ensuring that everything is set to be curve-visible
   */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_NODUPLIS);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  for (ale = anim_data.first; ale; ale = ale->next) {
    /* hack: skip object channels for now, since flushing those will always flush everything,
     * but they are always included. */
    /* TODO: find out why this is the case, and fix that */
    if (ale->type == ANIMTYPE_OBJECT) {
      continue;
    }

    /* select if it is not visible */
    if (ANIM_channel_setting_get(&ac, ale, ACHANNEL_SETTING_VISIBLE) == 0) {
      ANIM_channel_setting_set(&ac,
                               ale,
                               ACHANNEL_SETTING_SELECT,
                               select ? ACHANNEL_SETFLAG_ADD : ACHANNEL_SETFLAG_CLEAR);
    }

    /* change the visibility setting */
    ANIM_channel_setting_set(&ac, ale, ACHANNEL_SETTING_VISIBLE, ACHANNEL_SETFLAG_ADD);

    /* now, also flush selection status up/down as appropriate */
    ANIM_flush_setting_anim_channels(&ac, &all_data, ale, ACHANNEL_SETTING_VISIBLE, true);
  }

  /* cleanup */
  ANIM_animdata_freelist(&anim_data);
  BLI_freelistN(&all_data);

  /* send notifier that things have changed */
  WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

static void GRAPH_OT_reveal(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reveal Curves";
  ot->idname = "GRAPH_OT_reveal";
  ot->description = "Make previously hidden curves visible again in Graph Editor view";

  /* api callbacks */
  ot->exec = graphview_curves_reveal_exec;
  ot->poll = ED_operator_graphedit_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "select", true, "Select", "");
}

/* ************************** registration - operator types **********************************/

void graphedit_operatortypes(void)
{
  /* view */
  WM_operatortype_append(GRAPH_OT_cursor_set);

  WM_operatortype_append(GRAPH_OT_previewrange_set);
  WM_operatortype_append(GRAPH_OT_view_all);
  WM_operatortype_append(GRAPH_OT_view_selected);
  WM_operatortype_append(GRAPH_OT_view_frame);

  WM_operatortype_append(GRAPH_OT_ghost_curves_create);
  WM_operatortype_append(GRAPH_OT_ghost_curves_clear);

  WM_operatortype_append(GRAPH_OT_hide);
  WM_operatortype_append(GRAPH_OT_reveal);

  /* keyframes */
  /* selection */
  WM_operatortype_append(GRAPH_OT_clickselect);
  WM_operatortype_append(GRAPH_OT_select_all);
  WM_operatortype_append(GRAPH_OT_select_box);
  WM_operatortype_append(GRAPH_OT_select_lasso);
  WM_operatortype_append(GRAPH_OT_select_circle);
  WM_operatortype_append(GRAPH_OT_select_column);
  WM_operatortype_append(GRAPH_OT_select_linked);
  WM_operatortype_append(GRAPH_OT_select_more);
  WM_operatortype_append(GRAPH_OT_select_less);
  WM_operatortype_append(GRAPH_OT_select_leftright);

  /* editing */
  WM_operatortype_append(GRAPH_OT_snap);
  WM_operatortype_append(GRAPH_OT_mirror);
  WM_operatortype_append(GRAPH_OT_frame_jump);
  WM_operatortype_append(GRAPH_OT_handle_type);
  WM_operatortype_append(GRAPH_OT_interpolation_type);
  WM_operatortype_append(GRAPH_OT_extrapolation_type);
  WM_operatortype_append(GRAPH_OT_easing_type);
  WM_operatortype_append(GRAPH_OT_sample);
  WM_operatortype_append(GRAPH_OT_bake);
  WM_operatortype_append(GRAPH_OT_sound_bake);
  WM_operatortype_append(GRAPH_OT_smooth);
  WM_operatortype_append(GRAPH_OT_clean);
  WM_operatortype_append(GRAPH_OT_euler_filter);
  WM_operatortype_append(GRAPH_OT_delete);
  WM_operatortype_append(GRAPH_OT_duplicate);

  WM_operatortype_append(GRAPH_OT_copy);
  WM_operatortype_append(GRAPH_OT_paste);

  WM_operatortype_append(GRAPH_OT_keyframe_insert);
  WM_operatortype_append(GRAPH_OT_click_insert);

  /* F-Curve Modifiers */
  WM_operatortype_append(GRAPH_OT_fmodifier_add);
  WM_operatortype_append(GRAPH_OT_fmodifier_copy);
  WM_operatortype_append(GRAPH_OT_fmodifier_paste);

  /* Drivers */
  WM_operatortype_append(GRAPH_OT_driver_variables_copy);
  WM_operatortype_append(GRAPH_OT_driver_variables_paste);
  WM_operatortype_append(GRAPH_OT_driver_delete_invalid);
}

void ED_operatormacros_graph(void)
{
  wmOperatorType *ot;
  wmOperatorTypeMacro *otmacro;

  ot = WM_operatortype_append_macro("GRAPH_OT_duplicate_move",
                                    "Duplicate",
                                    "Make a copy of all selected keyframes and move them",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  WM_operatortype_macro_define(ot, "GRAPH_OT_duplicate");
  otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_transform");
  RNA_enum_set(otmacro->ptr, "mode", TFM_TIME_DUPLICATE);
  RNA_boolean_set(otmacro->ptr, "use_proportional_edit", false);
}

/* ************************** registration - keymaps **********************************/

void graphedit_keymap(wmKeyConfig *keyconf)
{
  /* keymap for all regions */
  WM_keymap_ensure(keyconf, "Graph Editor Generic", SPACE_GRAPH, 0);

  /* channels */
  /* Channels are not directly handled by the Graph Editor module,
   * but are inherited from the Animation module.
   * All the relevant operations, keymaps, drawing, etc.
   * can therefore all be found in that module instead,
   * as these are all used for the Graph Editor too.
   */

  /* keyframes */
  WM_keymap_ensure(keyconf, "Graph Editor", SPACE_GRAPH, 0);
}
