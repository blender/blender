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
 * \ingroup edanimation
 */

#include <stdlib.h>
#include <math.h>

#include "BLI_sys_types.h"

#include "BLI_utildefines.h"
#include "BLI_math_base.h"

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"

#include "BKE_context.h"
#include "BKE_sequencer.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_sound.h"
#include "BKE_scene.h"

#include "UI_view2d.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_anim_api.h"
#include "ED_screen.h"
#include "ED_sequencer.h"
#include "ED_util.h"

#include "anim_intern.h"

/* ********************** frame change operator ***************************/

/* Check if the operator can be run from the current context */
static bool change_frame_poll(bContext *C)
{
  ScrArea *sa = CTX_wm_area(C);

  /* XXX temp? prevent changes during render */
  if (G.is_rendering) {
    return false;
  }

  /* although it's only included in keymaps for regions using ED_KEYMAP_ANIMATION,
   * this shouldn't show up in 3D editor (or others without 2D timeline view) via search
   */
  if (sa) {
    if (ELEM(sa->spacetype, SPACE_ACTION, SPACE_NLA, SPACE_SEQ, SPACE_CLIP)) {
      return true;
    }
    else if (sa->spacetype == SPACE_GRAPH) {
      /* NOTE: Graph Editor has special version which does some extra stuff.
       * No need to show the generic error message for that case though!
       */
      return false;
    }
  }

  CTX_wm_operator_poll_msg_set(C, "Expected an animation area to be active");
  return false;
}

/* Set the new frame number */
static void change_frame_apply(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  float frame = RNA_float_get(op->ptr, "frame");
  bool do_snap = RNA_boolean_get(op->ptr, "snap");

  if (do_snap) {
    if (CTX_wm_space_seq(C)) {
      frame = BKE_sequencer_find_next_prev_edit(scene, frame, SEQ_SIDE_BOTH, true, false, false);
    }
    else {
      frame = BKE_scene_frame_snap_by_seconds(scene, 1.0, frame);
    }
  }

  /* set the new frame number */
  if (scene->r.flag & SCER_SHOW_SUBFRAME) {
    CFRA = (int)frame;
    SUBFRA = frame - (int)frame;
  }
  else {
    CFRA = round_fl_to_int(frame);
    SUBFRA = 0.0f;
  }
  FRAMENUMBER_MIN_CLAMP(CFRA);

  /* do updates */
  BKE_sound_seek_scene(bmain, scene);
  WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);
}

/* ---- */

/* Non-modal callback for running operator without user input */
static int change_frame_exec(bContext *C, wmOperator *op)
{
  change_frame_apply(C, op);

  return OPERATOR_FINISHED;
}

/* ---- */

/* Get frame from mouse coordinates */
static float frame_from_event(bContext *C, const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  Scene *scene = CTX_data_scene(C);
  float frame;

  /* convert from region coordinates to View2D 'tot' space */
  frame = UI_view2d_region_to_view_x(&region->v2d, event->mval[0]);

  /* respect preview range restrictions (if only allowed to move around within that range) */
  if (scene->r.flag & SCER_LOCK_FRAME_SELECTION) {
    CLAMP(frame, PSFRA, PEFRA);
  }

  return frame;
}

static void change_frame_seq_preview_begin(bContext *C, const wmEvent *event)
{
  ScrArea *sa = CTX_wm_area(C);
  bScreen *screen = CTX_wm_screen(C);
  if (sa && sa->spacetype == SPACE_SEQ) {
    SpaceSeq *sseq = sa->spacedata.first;
    if (ED_space_sequencer_check_show_strip(sseq)) {
      ED_sequencer_special_preview_set(C, event->mval);
    }
  }
  if (screen) {
    screen->scrubbing = true;
  }
}

static void change_frame_seq_preview_end(bContext *C)
{
  bScreen *screen = CTX_wm_screen(C);
  bool notify = false;

  if (screen->scrubbing) {
    screen->scrubbing = false;
    notify = true;
  }

  if (ED_sequencer_special_preview_get() != NULL) {
    ED_sequencer_special_preview_clear();
    notify = true;
  }

  if (notify) {
    Scene *scene = CTX_data_scene(C);
    WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);
  }
}

/* Modal Operator init */
static int change_frame_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  /* Change to frame that mouse is over before adding modal handler,
   * as user could click on a single frame (jump to frame) as well as
   * click-dragging over a range (modal scrubbing).
   */
  RNA_float_set(op->ptr, "frame", frame_from_event(C, event));

  change_frame_seq_preview_begin(C, event);

  change_frame_apply(C, op);

  /* add temp handler */
  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static void change_frame_cancel(bContext *C, wmOperator *UNUSED(op))
{
  change_frame_seq_preview_end(C);
}

/* Modal event handling of frame changing */
static int change_frame_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  int ret = OPERATOR_RUNNING_MODAL;
  /* execute the events */
  switch (event->type) {
    case ESCKEY:
      ret = OPERATOR_FINISHED;
      break;

    case MOUSEMOVE:
      RNA_float_set(op->ptr, "frame", frame_from_event(C, event));
      change_frame_apply(C, op);
      break;

    case LEFTMOUSE:
    case RIGHTMOUSE:
    case MIDDLEMOUSE:
      /* We check for either mouse-button to end, to work with all user keymaps. */
      if (event->val == KM_RELEASE) {
        ret = OPERATOR_FINISHED;
      }
      break;

    case LEFTCTRLKEY:
    case RIGHTCTRLKEY:
      if (event->val == KM_RELEASE) {
        RNA_boolean_set(op->ptr, "snap", false);
      }
      else if (event->val == KM_PRESS) {
        RNA_boolean_set(op->ptr, "snap", true);
      }
      break;
  }

  if (ret != OPERATOR_RUNNING_MODAL) {
    change_frame_seq_preview_end(C);
  }

  return ret;
}

static void ANIM_OT_change_frame(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Change Frame";
  ot->idname = "ANIM_OT_change_frame";
  ot->description = "Interactively change the current frame number";

  /* api callbacks */
  ot->exec = change_frame_exec;
  ot->invoke = change_frame_invoke;
  ot->cancel = change_frame_cancel;
  ot->modal = change_frame_modal;
  ot->poll = change_frame_poll;

  /* flags */
  ot->flag = OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR | OPTYPE_UNDO_GROUPED;
  ot->undo_group = "Frame Change";

  /* rna */
  ot->prop = RNA_def_float(
      ot->srna, "frame", 0, MINAFRAME, MAXFRAME, "Frame", "", MINAFRAME, MAXFRAME);
  prop = RNA_def_boolean(ot->srna, "snap", false, "Snap", "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/* ****************** Start/End Frame Operators *******************************/

static bool anim_set_end_frames_poll(bContext *C)
{
  ScrArea *sa = CTX_wm_area(C);

  /* XXX temp? prevent changes during render */
  if (G.is_rendering) {
    return false;
  }

  /* although it's only included in keymaps for regions using ED_KEYMAP_ANIMATION,
   * this shouldn't show up in 3D editor (or others without 2D timeline view) via search
   */
  if (sa) {
    if (ELEM(sa->spacetype, SPACE_ACTION, SPACE_GRAPH, SPACE_NLA, SPACE_SEQ, SPACE_CLIP)) {
      return true;
    }
  }

  CTX_wm_operator_poll_msg_set(C, "Expected an animation area to be active");
  return false;
}

static int anim_set_sfra_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  int frame;

  if (scene == NULL) {
    return OPERATOR_CANCELLED;
  }

  frame = CFRA;

  /* if Preview Range is defined, set the 'start' frame for that */
  if (PRVRANGEON) {
    scene->r.psfra = frame;
  }
  else {
    scene->r.sfra = frame;
  }

  if (PEFRA < frame) {
    if (PRVRANGEON) {
      scene->r.pefra = frame;
    }
    else {
      scene->r.efra = frame;
    }
  }

  WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);

  return OPERATOR_FINISHED;
}

static void ANIM_OT_start_frame_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Start Frame";
  ot->idname = "ANIM_OT_start_frame_set";
  ot->description = "Set the current frame as the preview or scene start frame";

  /* api callbacks */
  ot->exec = anim_set_sfra_exec;
  ot->poll = anim_set_end_frames_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int anim_set_efra_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  int frame;

  if (scene == NULL) {
    return OPERATOR_CANCELLED;
  }

  frame = CFRA;

  /* if Preview Range is defined, set the 'end' frame for that */
  if (PRVRANGEON) {
    scene->r.pefra = frame;
  }
  else {
    scene->r.efra = frame;
  }

  if (PSFRA > frame) {
    if (PRVRANGEON) {
      scene->r.psfra = frame;
    }
    else {
      scene->r.sfra = frame;
    }
  }

  WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);

  return OPERATOR_FINISHED;
}

static void ANIM_OT_end_frame_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set End Frame";
  ot->idname = "ANIM_OT_end_frame_set";
  ot->description = "Set the current frame as the preview or scene end frame";

  /* api callbacks */
  ot->exec = anim_set_efra_exec;
  ot->poll = anim_set_end_frames_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ****************** set preview range operator ****************************/

static int previewrange_define_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ARegion *ar = CTX_wm_region(C);
  float sfra, efra;
  rcti rect;

  /* get min/max values from box select rect (already in region coordinates, not screen) */
  WM_operator_properties_border_to_rcti(op, &rect);

  /* convert min/max values to frames (i.e. region to 'tot' rect) */
  sfra = UI_view2d_region_to_view_x(&ar->v2d, rect.xmin);
  efra = UI_view2d_region_to_view_x(&ar->v2d, rect.xmax);

  /* set start/end frames for preview-range
   * - must clamp within allowable limits
   * - end must not be before start (though this won't occur most of the time)
   */
  FRAMENUMBER_MIN_CLAMP(sfra);
  FRAMENUMBER_MIN_CLAMP(efra);
  if (efra < sfra) {
    efra = sfra;
  }

  scene->r.flag |= SCER_PRV_RANGE;
  scene->r.psfra = round_fl_to_int(sfra);
  scene->r.pefra = round_fl_to_int(efra);

  /* send notifiers */
  WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);

  return OPERATOR_FINISHED;
}

static void ANIM_OT_previewrange_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Preview Range";
  ot->idname = "ANIM_OT_previewrange_set";
  ot->description = "Interactively define frame range used for playback";

  /* api callbacks */
  ot->invoke = WM_gesture_box_invoke;
  ot->exec = previewrange_define_exec;
  ot->modal = WM_gesture_box_modal;
  ot->cancel = WM_gesture_box_cancel;

  ot->poll = ED_operator_animview_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* rna */
  /* used to define frame range.
   *
   * note: border Y values are not used,
   * but are needed by box_select gesture operator stuff */
  WM_operator_properties_border(ot);
}

/* ****************** clear preview range operator ****************************/

static int previewrange_clear_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  ScrArea *curarea = CTX_wm_area(C);

  /* sanity checks */
  if (ELEM(NULL, scene, curarea)) {
    return OPERATOR_CANCELLED;
  }

  /* simply clear values */
  scene->r.flag &= ~SCER_PRV_RANGE;
  scene->r.psfra = 0;
  scene->r.pefra = 0;

  ED_area_tag_redraw(curarea);

  /* send notifiers */
  WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);

  return OPERATOR_FINISHED;
}

static void ANIM_OT_previewrange_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Preview Range";
  ot->idname = "ANIM_OT_previewrange_clear";
  ot->description = "Clear Preview Range";

  /* api callbacks */
  ot->exec = previewrange_clear_exec;

  ot->poll = ED_operator_animview_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ************************** registration **********************************/

void ED_operatortypes_anim(void)
{
  /* Animation Editors only -------------------------- */
  WM_operatortype_append(ANIM_OT_change_frame);

  WM_operatortype_append(ANIM_OT_start_frame_set);
  WM_operatortype_append(ANIM_OT_end_frame_set);

  WM_operatortype_append(ANIM_OT_previewrange_set);
  WM_operatortype_append(ANIM_OT_previewrange_clear);

  /* Entire UI --------------------------------------- */
  WM_operatortype_append(ANIM_OT_keyframe_insert);
  WM_operatortype_append(ANIM_OT_keyframe_delete);
  WM_operatortype_append(ANIM_OT_keyframe_insert_menu);
  WM_operatortype_append(ANIM_OT_keyframe_delete_v3d);
  WM_operatortype_append(ANIM_OT_keyframe_clear_v3d);
  WM_operatortype_append(ANIM_OT_keyframe_insert_button);
  WM_operatortype_append(ANIM_OT_keyframe_delete_button);
  WM_operatortype_append(ANIM_OT_keyframe_clear_button);
  WM_operatortype_append(ANIM_OT_keyframe_insert_by_name);
  WM_operatortype_append(ANIM_OT_keyframe_delete_by_name);

  WM_operatortype_append(ANIM_OT_driver_button_add);
  WM_operatortype_append(ANIM_OT_driver_button_remove);
  WM_operatortype_append(ANIM_OT_driver_button_edit);
  WM_operatortype_append(ANIM_OT_copy_driver_button);
  WM_operatortype_append(ANIM_OT_paste_driver_button);

  WM_operatortype_append(ANIM_OT_keyingset_button_add);
  WM_operatortype_append(ANIM_OT_keyingset_button_remove);

  WM_operatortype_append(ANIM_OT_keying_set_add);
  WM_operatortype_append(ANIM_OT_keying_set_remove);
  WM_operatortype_append(ANIM_OT_keying_set_path_add);
  WM_operatortype_append(ANIM_OT_keying_set_path_remove);

  WM_operatortype_append(ANIM_OT_keying_set_active_set);
}

void ED_keymap_anim(wmKeyConfig *keyconf)
{
  WM_keymap_ensure(keyconf, "Animation", 0, 0);
}
