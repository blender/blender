/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edanimation
 */

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "BLI_math_base.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "DNA_scene_types.h"

#include "BKE_anim_data.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "UI_view2d.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_anim_api.hh"
#include "ED_screen.hh"
#include "ED_sequencer.hh"
#include "ED_time_scrub_ui.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "SEQ_iterator.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_time.hh"

#include "ANIM_action.hh"
#include "ANIM_animdata.hh"

#include "anim_intern.hh"

/* -------------------------------------------------------------------- */
/** \name Frame Change Operator
 * \{ */

/* Check if the operator can be run from the current context */
static bool change_frame_poll(bContext *C)
{
  ScrArea *area = CTX_wm_area(C);

  /* XXX temp? prevent changes during render */
  if (G.is_rendering) {
    return false;
  }

  /* although it's only included in keymaps for regions using ED_KEYMAP_ANIMATION,
   * this shouldn't show up in 3D editor (or others without 2D timeline view) via search
   */
  if (area) {
    if (ELEM(area->spacetype, SPACE_ACTION, SPACE_NLA, SPACE_CLIP)) {
      return true;
    }
    if (area->spacetype == SPACE_SEQ) {
      /* Check the region type so tools (which are shared between preview/strip view)
       * don't conflict with actions which can have the same key bound (2D cursor for example). */
      const ARegion *region = CTX_wm_region(C);
      if (region && region->regiontype == RGN_TYPE_WINDOW) {
        return true;
      }
    }
    if (area->spacetype == SPACE_GRAPH) {
      const SpaceGraph *sipo = static_cast<const SpaceGraph *>(area->spacedata.first);
      /* Driver Editor's X axis is not time. */
      if (sipo->mode != SIPO_MODE_DRIVERS) {
        return true;
      }
    }
  }

  CTX_wm_operator_poll_msg_set(C, "Expected an animation area to be active");
  return false;
}

static int seq_snap_threshold_get_frame_distance(bContext *C)
{
  const int snap_distance = SEQ_tool_settings_snap_distance_get(CTX_data_scene(C));
  const ARegion *region = CTX_wm_region(C);
  return round_fl_to_int(UI_view2d_region_to_view_x(&region->v2d, snap_distance) -
                         UI_view2d_region_to_view_x(&region->v2d, 0));
}

static void seq_frame_snap_update_best(const int position,
                                       const int timeline_frame,
                                       int *r_best_frame,
                                       int *r_best_distance)
{
  if (abs(position - timeline_frame) < *r_best_distance) {
    *r_best_distance = abs(position - timeline_frame);
    *r_best_frame = position;
  }
}

static int seq_frame_apply_snap(bContext *C, Scene *scene, const int timeline_frame)
{

  ListBase *seqbase = SEQ_active_seqbase_get(SEQ_editing_get(scene));

  int best_frame = 0;
  int best_distance = MAXFRAME;
  for (Strip *strip : SEQ_query_all_strips(seqbase)) {
    seq_frame_snap_update_best(
        SEQ_time_left_handle_frame_get(scene, strip), timeline_frame, &best_frame, &best_distance);
    seq_frame_snap_update_best(SEQ_time_right_handle_frame_get(scene, strip),
                               timeline_frame,
                               &best_frame,
                               &best_distance);
  }

  if (best_distance < seq_snap_threshold_get_frame_distance(C)) {
    return best_frame;
  }

  return timeline_frame;
}

/* Set the new frame number */
static void change_frame_apply(bContext *C, wmOperator *op, const bool always_update)
{
  Scene *scene = CTX_data_scene(C);
  float frame = RNA_float_get(op->ptr, "frame");
  bool do_snap = RNA_boolean_get(op->ptr, "snap");

  const int old_frame = scene->r.cfra;
  const float old_subframe = scene->r.subframe;

  if (do_snap) {
    if (CTX_wm_space_seq(C) && SEQ_editing_get(scene) != nullptr) {
      frame = seq_frame_apply_snap(C, scene, frame);
    }
    else {
      frame = BKE_scene_frame_snap_by_seconds(scene, 1.0, frame);
    }
  }

  /* set the new frame number */
  if (scene->r.flag & SCER_SHOW_SUBFRAME) {
    scene->r.cfra = int(frame);
    scene->r.subframe = frame - int(frame);
  }
  else {
    scene->r.cfra = round_fl_to_int(frame);
    scene->r.subframe = 0.0f;
  }
  FRAMENUMBER_MIN_CLAMP(scene->r.cfra);

  /* do updates */
  const bool frame_changed = (old_frame != scene->r.cfra) || (old_subframe != scene->r.subframe);
  if (frame_changed || always_update) {
    DEG_id_tag_update(&scene->id, ID_RECALC_FRAME_CHANGE);
    WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);
  }
}

/* ---- */

/* Non-modal callback for running operator without user input */
static int change_frame_exec(bContext *C, wmOperator *op)
{
  change_frame_apply(C, op, true);

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

static void change_frame_seq_preview_begin(bContext *C, const wmEvent *event, SpaceSeq *sseq)
{
  BLI_assert(sseq != nullptr);
  ARegion *region = CTX_wm_region(C);
  if (ED_space_sequencer_check_show_strip(sseq) && !ED_time_scrub_event_in_region(region, event)) {
    ED_sequencer_special_preview_set(C, event->mval);
  }
}

static void change_frame_seq_preview_end(SpaceSeq *sseq)
{
  BLI_assert(sseq != nullptr);
  UNUSED_VARS_NDEBUG(sseq);
  if (ED_sequencer_special_preview_get() != nullptr) {
    ED_sequencer_special_preview_clear();
  }
}

static bool use_sequencer_snapping(bContext *C)
{
  if (!CTX_wm_space_seq(C)) {
    return false;
  }

  Scene *scene = CTX_data_scene(C);
  short snap_flag = SEQ_tool_settings_snap_flag_get(scene);
  return (scene->toolsettings->snap_flag_seq & SCE_SNAP) &&
         (snap_flag & SEQ_SNAP_CURRENT_FRAME_TO_STRIPS);
}

static bool sequencer_skip_for_handle_tweak(const bContext *C, const wmEvent *event)
{
  if ((U.sequencer_editor_flag & USER_SEQ_ED_SIMPLE_TWEAKING) == 0) {
    return false;
  }

  const Scene *scene = CTX_data_scene(C);
  if (!SEQ_editing_get(scene)) {
    return false;
  }

  const View2D *v2d = UI_view2d_fromcontext(C);

  float mouse_co[2];
  UI_view2d_region_to_view(v2d, event->mval[0], event->mval[1], &mouse_co[0], &mouse_co[1]);

  StripSelection selection = ED_sequencer_pick_strip_and_handle(scene, v2d, mouse_co);

  return selection.handle != SEQ_HANDLE_NONE;
}

/* Modal Operator init */
static int change_frame_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  bScreen *screen = CTX_wm_screen(C);

  /* This check is done in case scrubbing and strip tweaking in the sequencer are bound to the same
   * event (e.g. RCS keymap where both are activated on left mouse press). Tweaking should take
   * precedence. */
  if (CTX_wm_space_seq(C) && sequencer_skip_for_handle_tweak(C, event)) {
    return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
  }

  /* Change to frame that mouse is over before adding modal handler,
   * as user could click on a single frame (jump to frame) as well as
   * click-dragging over a range (modal scrubbing).
   */
  RNA_float_set(op->ptr, "frame", frame_from_event(C, event));

  if (use_sequencer_snapping(C)) {
    RNA_boolean_set(op->ptr, "snap", true);
  }

  screen->scrubbing = true;
  SpaceSeq *sseq = CTX_wm_space_seq(C);
  if (sseq) {
    change_frame_seq_preview_begin(C, event, sseq);
  }

  change_frame_apply(C, op, true);

  /* add temp handler */
  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static bool need_extra_redraw_after_scrubbing_ends(bContext *C)
{
  if (CTX_wm_space_seq(C)) {
    /* During scrubbing in the sequencer, a preview of the final video might be drawn. After
     * scrubbing, the actual result should be shown again. */
    return true;
  }
  Scene *scene = CTX_data_scene(C);
  if (scene->eevee.taa_samples != 1) {
    return true;
  }
  return false;
}

static void change_frame_cancel(bContext *C, wmOperator * /*op*/)
{
  bScreen *screen = CTX_wm_screen(C);
  screen->scrubbing = false;

  SpaceSeq *sseq = CTX_wm_space_seq(C);
  if (sseq != nullptr) {
    change_frame_seq_preview_end(sseq);
  }

  if (need_extra_redraw_after_scrubbing_ends(C)) {
    Scene *scene = CTX_data_scene(C);
    WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);
  }
}

/* Modal event handling of frame changing */
static int change_frame_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  int ret = OPERATOR_RUNNING_MODAL;
  /* execute the events */
  switch (event->type) {
    case EVT_ESCKEY:
      ret = OPERATOR_FINISHED;
      break;

    case MOUSEMOVE:
      RNA_float_set(op->ptr, "frame", frame_from_event(C, event));
      change_frame_apply(C, op, false);
      break;

    case LEFTMOUSE:
    case RIGHTMOUSE:
    case MIDDLEMOUSE:
      /* We check for either mouse-button to end, to work with all user keymaps. */
      if (event->val == KM_RELEASE) {
        ret = OPERATOR_FINISHED;
      }
      break;

    case EVT_LEFTCTRLKEY:
    case EVT_RIGHTCTRLKEY:
      /* Use Ctrl key to invert snapping in sequencer. */
      if (use_sequencer_snapping(C)) {
        if (event->val == KM_RELEASE) {
          RNA_boolean_set(op->ptr, "snap", true);
        }
        else if (event->val == KM_PRESS) {
          RNA_boolean_set(op->ptr, "snap", false);
        }
      }
      else {
        if (event->val == KM_RELEASE) {
          RNA_boolean_set(op->ptr, "snap", false);
        }
        else if (event->val == KM_PRESS) {
          RNA_boolean_set(op->ptr, "snap", true);
        }
      }
      break;
  }

  if (ret != OPERATOR_RUNNING_MODAL) {
    bScreen *screen = CTX_wm_screen(C);
    screen->scrubbing = false;

    SpaceSeq *sseq = CTX_wm_space_seq(C);
    if (sseq != nullptr) {
      change_frame_seq_preview_end(sseq);
    }
    if (need_extra_redraw_after_scrubbing_ends(C)) {
      Scene *scene = CTX_data_scene(C);
      WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);
    }
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
  ot->flag = OPTYPE_BLOCKING | OPTYPE_GRAB_CURSOR_X | OPTYPE_UNDO_GROUPED;
  ot->undo_group = "Frame Change";

  /* rna */
  ot->prop = RNA_def_float(
      ot->srna, "frame", 0, MINAFRAME, MAXFRAME, "Frame", "", MINAFRAME, MAXFRAME);
  prop = RNA_def_boolean(ot->srna, "snap", false, "Snap", "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Start/End Frame Operators
 * \{ */

static bool anim_set_end_frames_poll(bContext *C)
{
  ScrArea *area = CTX_wm_area(C);

  /* XXX temp? prevent changes during render */
  if (G.is_rendering) {
    return false;
  }

  /* although it's only included in keymaps for regions using ED_KEYMAP_ANIMATION,
   * this shouldn't show up in 3D editor (or others without 2D timeline view) via search
   */
  if (area) {
    if (ELEM(area->spacetype, SPACE_ACTION, SPACE_GRAPH, SPACE_NLA, SPACE_SEQ, SPACE_CLIP)) {
      return true;
    }
  }

  CTX_wm_operator_poll_msg_set(C, "Expected an animation area to be active");
  return false;
}

static int anim_set_sfra_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  int frame;

  if (scene == nullptr) {
    return OPERATOR_CANCELLED;
  }

  frame = scene->r.cfra;

  /* if Preview Range is defined, set the 'start' frame for that */
  if (PRVRANGEON) {
    scene->r.psfra = frame;
  }
  else {
    /* Clamping should be in sync with 'rna_Scene_start_frame_set()'. */
    int frame_clamped = frame;
    CLAMP(frame_clamped, MINFRAME, MAXFRAME);
    if (frame_clamped != frame) {
      BKE_report(op->reports, RPT_WARNING, "Start frame clamped to valid rendering range");
    }
    frame = frame_clamped;
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

static int anim_set_efra_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  int frame;

  if (scene == nullptr) {
    return OPERATOR_CANCELLED;
  }

  frame = scene->r.cfra;

  /* if Preview Range is defined, set the 'end' frame for that */
  if (PRVRANGEON) {
    scene->r.pefra = frame;
  }
  else {
    /* Clamping should be in sync with 'rna_Scene_end_frame_set()'. */
    int frame_clamped = frame;
    CLAMP(frame_clamped, MINFRAME, MAXFRAME);
    if (frame_clamped != frame) {
      BKE_report(op->reports, RPT_WARNING, "End frame clamped to valid rendering range");
    }
    frame = frame_clamped;
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Preview Range Operator
 * \{ */

static int previewrange_define_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ARegion *region = CTX_wm_region(C);
  float sfra, efra;
  rcti rect;

  /* get min/max values from box select rect (already in region coordinates, not screen) */
  WM_operator_properties_border_to_rcti(op, &rect);

  /* convert min/max values to frames (i.e. region to 'tot' rect) */
  sfra = UI_view2d_region_to_view_x(&region->v2d, rect.xmin);
  efra = UI_view2d_region_to_view_x(&region->v2d, rect.xmax);

  /* set start/end frames for preview-range
   * - must clamp within allowable limits
   * - end must not be before start (though this won't occur most of the time)
   */
  FRAMENUMBER_MIN_CLAMP(sfra);
  FRAMENUMBER_MIN_CLAMP(efra);
  efra = std::max(efra, sfra);

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
   * NOTE: border Y values are not used,
   * but are needed by box_select gesture operator stuff */
  WM_operator_properties_border(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clear Preview Range Operator
 * \{ */

static int previewrange_clear_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  ScrArea *curarea = CTX_wm_area(C);

  /* sanity checks */
  if (ELEM(nullptr, scene, curarea)) {
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
  ot->description = "Clear preview range";

  /* api callbacks */
  ot->exec = previewrange_clear_exec;

  ot->poll = ED_operator_animview_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Debug operator: channel list
 * \{ */

#ifndef NDEBUG
static int debug_channel_list_exec(bContext *C, wmOperator * /*op*/)
{
  bAnimContext ac;
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return OPERATOR_CANCELLED;
  }

  ListBase anim_data = {nullptr, nullptr};
  /* Same filter flags as in action_channel_region_draw() in
   * `source/blender/editors/space_action/space_action.cc`. */
  const eAnimFilter_Flags filter = ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE |
                                   ANIMFILTER_LIST_CHANNELS;
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, eAnimCont_Types(ac.datatype));

  printf("==============================================\n");
  printf("Animation Channel List:\n");
  printf("----------------------------------------------\n");

  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    ANIM_channel_debug_print_info(ale, 1);
  }

  printf("==============================================\n");

  ANIM_animdata_freelist(&anim_data);
  return OPERATOR_FINISHED;
}

static void ANIM_OT_debug_channel_list(wmOperatorType *ot)
{
  ot->name = "Debug Channel List";
  ot->idname = "ANIM_OT_debug_channel_list";
  ot->description =
      "Log the channel list info in the terminal. This operator is only available in debug builds "
      "of Blender";

  ot->exec = debug_channel_list_exec;
  ot->poll = ED_operator_animview_active;

  ot->flag = OPTYPE_REGISTER;
}
#endif /* !NDEBUG */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Frame Scene/Preview Range Operator
 * \{ */

static int scene_range_frame_exec(bContext *C, wmOperator * /*op*/)
{
  ARegion *region = CTX_wm_region(C);
  const Scene *scene = CTX_data_scene(C);
  BLI_assert(region);
  BLI_assert(scene);

  View2D &v2d = region->v2d;
  v2d.cur.xmin = PSFRA;
  v2d.cur.xmax = PEFRA;

  v2d.cur = ANIM_frame_range_view2d_add_xmargin(v2d, v2d.cur);

  UI_view2d_sync(CTX_wm_screen(C), CTX_wm_area(C), &v2d, V2D_LOCK_COPY);
  ED_area_tag_redraw(CTX_wm_area(C));

  return OPERATOR_FINISHED;
}

static void ANIM_OT_scene_range_frame(wmOperatorType *ot)
{
  ot->name = "Frame Scene/Preview Range";
  ot->idname = "ANIM_OT_scene_range_frame";
  ot->description =
      "Reset the horizontal view to the current scene frame range, taking the preview range into "
      "account if it is active";

  ot->exec = scene_range_frame_exec;
  ot->poll = ED_operator_animview_active;

  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Conversion
 * \{ */

static int convert_action_exec(bContext *C, wmOperator * /*op*/)
{
  using namespace blender;

  Object *object = CTX_data_active_object(C);
  AnimData *adt = BKE_animdata_from_id(&object->id);
  BLI_assert(adt != nullptr);
  BLI_assert(adt->action != nullptr);

  animrig::Action &legacy_action = adt->action->wrap();
  Main *bmain = CTX_data_main(C);

  animrig::Action *layered_action = animrig::convert_to_layered_action(*bmain, legacy_action);
  /* We did already check if the action can be converted. */
  BLI_assert(layered_action != nullptr);
  const bool assign_ok = animrig::assign_action(layered_action, object->id);
  BLI_assert_msg(assign_ok, "Expecting assigning a layered Action to always work");
  UNUSED_VARS_NDEBUG(assign_ok);

  BLI_assert(layered_action->slots().size() == 1);
  animrig::Slot *slot = layered_action->slot(0);
  layered_action->slot_identifier_set(*bmain, *slot, object->id.name);

  const animrig::ActionSlotAssignmentResult result = animrig::assign_action_slot(slot, object->id);
  BLI_assert(result == animrig::ActionSlotAssignmentResult::OK);
  UNUSED_VARS_NDEBUG(result);

  ANIM_id_update(bmain, &object->id);
  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_ANIMATION | ND_NLA_ACTCHANGE, nullptr);

  return OPERATOR_FINISHED;
}

static bool convert_action_poll(bContext *C)
{
  Object *object = CTX_data_active_object(C);
  if (!object) {
    return false;
  }

  AnimData *adt = BKE_animdata_from_id(&object->id);
  if (!adt || !adt->action) {
    return false;
  }

  /* This will also convert empty actions to layered by just adding an empty slot. */
  if (!adt->action->wrap().is_action_legacy()) {
    CTX_wm_operator_poll_msg_set(C, "Action is already layered");
    return false;
  }

  return true;
}

static void ANIM_OT_convert_legacy_action(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Convert Legacy Action";
  ot->idname = "ANIM_OT_convert_legacy_action";
  ot->description = "Convert a legacy Action to a layered Action on the active object";

  /* api callbacks */
  ot->exec = convert_action_exec;
  ot->poll = convert_action_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static bool merge_actions_selection_poll(bContext *C)
{
  Object *object = CTX_data_active_object(C);
  if (!object) {
    CTX_wm_operator_poll_msg_set(C, "No active object");
    return false;
  }
  blender::animrig::Action *action = blender::animrig::get_action(object->id);
  if (!action) {
    CTX_wm_operator_poll_msg_set(C, "Active object has no action");
    return false;
  }
  if (!BKE_id_is_editable(CTX_data_main(C), &action->id)) {
    return false;
  }
  return true;
}

static int merge_actions_selection_exec(bContext *C, wmOperator *op)
{
  using namespace blender::animrig;
  Object *active_object = CTX_data_active_object(C);
  /* Those cases are caught by the poll. */
  BLI_assert(active_object != nullptr);
  BLI_assert(active_object->adt->action != nullptr);

  Action &active_action = active_object->adt->action->wrap();

  blender::Vector<PointerRNA> selection;
  if (!CTX_data_selected_objects(C, &selection)) {
    return OPERATOR_CANCELLED;
  }

  Main *bmain = CTX_data_main(C);
  int moved_slots_count = 0;
  for (const PointerRNA &ptr : selection) {
    blender::Vector<ID *> related_ids = find_related_ids(*bmain, *ptr.owner_id);
    for (ID *related_id : related_ids) {
      Action *action = get_action(*related_id);
      if (!action) {
        continue;
      }
      if (action == &active_action) {
        /* Object is already animated by the same action, no point in moving. */
        continue;
      }
      if (action->is_action_legacy()) {
        continue;
      }
      if (!BKE_id_is_editable(bmain, &action->id)) {
        BKE_reportf(op->reports, RPT_WARNING, "The action %s is not editable", action->id.name);
        continue;
      }
      AnimData *id_anim_data = BKE_animdata_ensure_id(related_id);
      /* Since we already get an action from the ID the animdata has to exist. */
      BLI_assert(id_anim_data);
      Slot *slot = action->slot_for_handle(id_anim_data->slot_handle);
      if (!slot) {
        continue;
      }
      blender::animrig::move_slot(*bmain, *slot, *action, active_action);
      moved_slots_count++;
      ANIM_id_update(bmain, related_id);
      DEG_id_tag_update_ex(bmain, &action->id, ID_RECALC_ANIMATION_NO_FLUSH);
    }
  }

  if (moved_slots_count > 0) {
    BKE_reportf(op->reports,
                RPT_INFO,
                "Moved %i slot(s) into the action of the active object",
                moved_slots_count);
  }
  else {
    BKE_reportf(op->reports,
                RPT_ERROR,
                "Failed to merge any animation. Note that NLA strips cannot be merged");
  }

  /* `ID_RECALC_ANIMATION_NO_FLUSH` is used here (and above), as the actual animation values do not
   * change, so there is no need to flush to the animated IDs. The Action itself does need to be
   * re-evaluated to get an up-to-date evaluated copy with the new slots & channelbags. Without
   * this, future animation evaluation will break. */
  DEG_id_tag_update_ex(bmain, &active_action.id, ID_RECALC_ANIMATION_NO_FLUSH);

  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_ANIMATION | ND_NLA_ACTCHANGE, nullptr);

  return OPERATOR_FINISHED;
}

static void ANIM_OT_merge_animation(wmOperatorType *ot)
{
  ot->name = "Merge Animation";
  ot->idname = "ANIM_OT_merge_animation";
  ot->description =
      "Merge the animation of the selected objects into the action of the active object. Actions "
      "are not deleted by this, but might end up with zero users";

  ot->exec = merge_actions_selection_exec;
  ot->poll = merge_actions_selection_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registration
 * \{ */

void ED_operatortypes_anim()
{
  /* Animation Editors only -------------------------- */
  WM_operatortype_append(ANIM_OT_change_frame);

  WM_operatortype_append(ANIM_OT_start_frame_set);
  WM_operatortype_append(ANIM_OT_end_frame_set);

  WM_operatortype_append(ANIM_OT_previewrange_set);
  WM_operatortype_append(ANIM_OT_previewrange_clear);

  WM_operatortype_append(ANIM_OT_scene_range_frame);

#ifndef NDEBUG
  WM_operatortype_append(ANIM_OT_debug_channel_list);
#endif

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

  WM_operatortype_append(ANIM_OT_convert_legacy_action);
  WM_operatortype_append(ANIM_OT_merge_animation);

  WM_operatortype_append(blender::ed::animrig::POSELIB_OT_create_pose_asset);
  WM_operatortype_append(blender::ed::animrig::POSELIB_OT_asset_modify);
  WM_operatortype_append(blender::ed::animrig::POSELIB_OT_asset_delete);
}

void ED_keymap_anim(wmKeyConfig *keyconf)
{
  WM_keymap_ensure(keyconf, "Animation", SPACE_EMPTY, RGN_TYPE_WINDOW);
}

/** \} */
