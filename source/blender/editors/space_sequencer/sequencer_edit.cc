/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 */

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_ref.hh"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"
#include "BLI_timecode.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "MEM_guardedalloc.h"

#include "BLT_translation.hh"

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_sound_types.h"

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_idtype.hh"
#include "BKE_layer.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"
#include "BKE_sound.hh"

#include "SEQ_add.hh"
#include "SEQ_animation.hh"
#include "SEQ_channels.hh"
#include "SEQ_connect.hh"
#include "SEQ_edit.hh"
#include "SEQ_effects.hh"
#include "SEQ_iterator.hh"
#include "SEQ_prefetch.hh"
#include "SEQ_relations.hh"
#include "SEQ_render.hh"
#include "SEQ_select.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_thumbnail_cache.hh"
#include "SEQ_time.hh"
#include "SEQ_transform.hh"
#include "SEQ_utils.hh"

#include "ANIM_action_legacy.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

/* For menu, popup, icons, etc. */
#include "ED_fileselect.hh"
#include "ED_numinput.hh"
#include "ED_object.hh"
#include "ED_scene.hh"
#include "ED_screen.hh"
#include "ED_screen_types.hh"
#include "ED_sequencer.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

/* Own include. */
#include "sequencer_intern.hh"
#include <cstddef>
#include <fmt/format.h>

namespace blender::ed::vse {

/* -------------------------------------------------------------------- */
/** \name Public Context Checks
 * \{ */

bool maskedit_mask_poll(bContext *C)
{
  return maskedit_poll(C);
}

bool check_show_maskedit(SpaceSeq *sseq, Scene *scene)
{
  if (sseq && sseq->mainb == SEQ_DRAW_IMG_IMBUF) {
    return (seq::active_mask_get(scene) != nullptr);
  }

  return false;
}

bool maskedit_poll(bContext *C)
{
  SpaceSeq *sseq = CTX_wm_space_seq(C);

  if (sseq) {
    Scene *scene = CTX_data_sequencer_scene(C);
    return check_show_maskedit(sseq, scene);
  }

  return false;
}

bool check_show_imbuf(const SpaceSeq &sseq)
{
  return (sseq.mainb == SEQ_DRAW_IMG_IMBUF) &&
         ELEM(sseq.view, SEQ_VIEW_PREVIEW, SEQ_VIEW_SEQUENCE_PREVIEW);
}

bool check_show_strip(const SpaceSeq &sseq)
{
  return ELEM(sseq.view, SEQ_VIEW_SEQUENCE, SEQ_VIEW_SEQUENCE_PREVIEW);
}

static bool sequencer_fcurves_targets_color_strip(const FCurve *fcurve)
{
  if (!BLI_str_startswith(fcurve->rna_path, "sequence_editor.strips_all[\"")) {
    return false;
  }

  if (!BLI_str_endswith(fcurve->rna_path, "\"].color")) {
    return false;
  }

  return true;
}

bool has_playback_animation(const Scene *scene)
{
  if (!scene->adt) {
    return false;
  }
  if (!scene->adt->action) {
    return false;
  }

  for (FCurve *fcurve : animrig::legacy::fcurves_for_assigned_action(scene->adt)) {
    if (sequencer_fcurves_targets_color_strip(fcurve)) {
      return true;
    }
  }

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shared Poll Functions
 * \{ */

bool sequencer_edit_poll(bContext *C)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  if (!scene) {
    return false;
  }
  return (seq::editing_get(scene) != nullptr);
}

bool sequencer_edit_with_channel_region_poll(bContext *C)
{
  if (!sequencer_edit_poll(C)) {
    return false;
  }
  ARegion *region = CTX_wm_region(C);
  if (!(region && (region->regiontype == RGN_TYPE_CHANNELS))) {
    return false;
  }
  return true;
}

bool sequencer_editing_initialized_and_active(bContext *C)
{
  return ED_operator_sequencer_active(C) && sequencer_edit_poll(C);
}

#if 0 /* UNUSED */
bool sequencer_strip_poll(bContext *C)
{
  Editing *ed;
  return (((ed = seq::editing_get(CTX_data_sequencer_scene(C))) != nullptr) && (ed->act_strip != nullptr));
}
#endif

bool sequencer_strip_editable_poll(bContext *C)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  if (!scene || !ID_IS_EDITABLE(&scene->id)) {
    return false;
  }
  Editing *ed = seq::editing_get(scene);
  return (ed && (ed->act_strip != nullptr));
}

bool sequencer_strip_has_path_poll(bContext *C)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  if (!scene) {
    return false;
  }
  Editing *ed = seq::editing_get(scene);
  if (!ed) {
    return false;
  }
  Strip *strip = ed->act_strip;
  if (!strip) {
    return false;
  }
  return STRIP_HAS_PATH(strip);
}

bool sequencer_view_has_preview_poll(bContext *C)
{
  if (!sequencer_edit_poll(C)) {
    return false;
  }
  SpaceSeq *sseq = CTX_wm_space_seq(C);
  if (sseq == nullptr) {
    return false;
  }
  if (!(ELEM(sseq->view, SEQ_VIEW_PREVIEW, SEQ_VIEW_SEQUENCE_PREVIEW) &&
        (sseq->mainb == SEQ_DRAW_IMG_IMBUF)))
  {
    return false;
  }
  ARegion *region = CTX_wm_region(C);
  if (!(region && region->regiontype == RGN_TYPE_PREVIEW)) {
    return false;
  }

  return true;
}

bool sequencer_view_preview_only_poll(const bContext *C)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  if (!scene) {
    return false;
  }
  Editing *ed = seq::editing_get(scene);
  if (!ed) {
    return false;
  }
  SpaceSeq *sseq = CTX_wm_space_seq(C);
  if (sseq == nullptr) {
    return false;
  }
  if (!(ELEM(sseq->view, SEQ_VIEW_PREVIEW) && (sseq->mainb == SEQ_DRAW_IMG_IMBUF))) {
    return false;
  }
  ARegion *region = CTX_wm_region(C);
  if (!(region && region->regiontype == RGN_TYPE_PREVIEW)) {
    return false;
  }

  return true;
}

bool sequencer_view_strips_poll(bContext *C)
{
  SpaceSeq *sseq = CTX_wm_space_seq(C);
  if (sseq == nullptr) {
    return false;
  }
  if (!check_show_strip(*sseq)) {
    return false;
  }
  ARegion *region = CTX_wm_region(C);
  if (!(region && region->regiontype == RGN_TYPE_WINDOW)) {
    return false;
  }
  return true;
}

static bool sequencer_effect_poll(bContext *C)
{
  if (!sequencer_edit_poll(C)) {
    return false;
  }
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(scene);

  if (ed) {
    Strip *active_strip = seq::select_active_get(scene);
    if (active_strip && active_strip->is_effect()) {
      return true;
    }
  }

  return false;
}

static bool sequencer_swap_inputs_poll(bContext *C)
{
  if (!sequencer_edit_poll(C)) {
    return false;
  }
  Scene *scene = CTX_data_sequencer_scene(C);
  Strip *active_strip = seq::select_active_get(scene);

  if (sequencer_effect_poll(C) && seq::effect_get_num_inputs(active_strip->type) == 2) {
    return true;
  }

  return false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Scene syncing with current scene strip
 * \{ */

bool is_scene_time_sync_needed(const bContext &C)
{
  WorkSpace *workspace = CTX_wm_workspace(&C);
  if (!workspace || !workspace->sequencer_scene) {
    return false;
  }
  if ((workspace->flags & WORKSPACE_SYNC_SCENE_TIME) == 0) {
    return false;
  }
  SpaceSeq *sseq = CTX_wm_space_seq(&C);
  if (!sseq) {
    /* We only want to start syncing the time when we're in a sequence editor.
     * Changing time in any other editor should just affect the active scene. */
    return false;
  }
  return true;
}

static Scene *get_sequencer_scene_for_time_sync(const bContext &C)
{
  wmWindowManager *wm = CTX_wm_manager(&C);
  bScreen *screen = ED_screen_animation_playing(wm);
  if (screen && screen->animtimer) {
    wmTimer *wt = screen->animtimer;
    ScreenAnimData *sad = static_cast<ScreenAnimData *>(wt->customdata);
    if (sad->do_scene_syncing) {
      return sad->scene;
    }
    /* If we're playing a scene that's not a sequence scene, don't try and sync. */
    return nullptr;
  }
  if (is_scene_time_sync_needed(C)) {
    return CTX_data_sequencer_scene(&C);
  }
  return nullptr;
}

const Strip *get_scene_strip_for_time_sync(const Scene *sequencer_scene)
{
  using namespace blender;
  const Editing *ed = seq::editing_get(sequencer_scene);
  if (!ed) {
    return nullptr;
  }
  ListBase *seqbase = seq::active_seqbase_get(ed);
  const ListBase *channels = seq::channels_displayed_get(ed);
  VectorSet<Strip *> query_strips = seq::query_strips_recursive_at_frame(
      sequencer_scene, seqbase, sequencer_scene->r.cfra);
  /* Ignore effect strips, sound strips and muted strips. */
  query_strips.remove_if([&](const Strip *strip) {
    return strip->is_effect() || strip->type == STRIP_TYPE_SOUND_RAM ||
           seq::render_is_muted(channels, strip);
  });
  Vector<Strip *> strips = query_strips.extract_vector();
  /* Sort strips by channel. */
  std::sort(strips.begin(), strips.end(), [](const Strip *a, const Strip *b) {
    return a->channel > b->channel;
  });
  /* Get the top-most scene strip. */
  for (const Strip *strip : strips) {
    if (strip->type == STRIP_TYPE_SCENE) {
      return strip;
    }
  }
  return nullptr;
}

void sync_active_scene_and_time_with_scene_strip(bContext &C)
{
  using namespace blender;
  Scene *sequencer_scene = get_sequencer_scene_for_time_sync(C);
  if (!sequencer_scene) {
    return;
  }

  wmWindow *win = CTX_wm_window(&C);
  const Strip *scene_strip = get_scene_strip_for_time_sync(sequencer_scene);
  if (!scene_strip || !scene_strip->scene) {
    /* No scene strip with scene found. Switch to pinned scene. */
    Main *bmain = CTX_data_main(&C);
    WM_window_set_active_scene(bmain, &C, win, sequencer_scene);
    return;
  }

  ViewLayer *view_layer = WM_window_get_active_view_layer(win);
  Object *prev_obact = BKE_view_layer_active_object_get(view_layer);

  Scene *active_scene = WM_window_get_active_scene(win);
  if (active_scene != scene_strip->scene) {
    /* Sync active scene in window. */
    Main *bmain = CTX_data_main(&C);
    WM_window_set_active_scene(bmain, &C, win, scene_strip->scene);
    active_scene = scene_strip->scene;
  }
  Object *camera = [&]() -> Object * {
    if (scene_strip->scene_camera) {
      return scene_strip->scene_camera;
    }
    return scene_strip->scene->camera;
  }();
  if (camera) {
    /* Sync camera in any 3D view that uses camera view. */
    PointerRNA camera_ptr = RNA_id_pointer_create(&camera->id);
    bScreen *screen = WM_window_get_active_screen(win);
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
        if (sl->spacetype != SPACE_VIEW3D) {
          continue;
        }
        View3D *view3d = reinterpret_cast<View3D *>(sl);
        if (view3d->camera == camera) {
          continue;
        }
        PointerRNA view3d_ptr = RNA_pointer_create_discrete(&screen->id, &RNA_SpaceView3D, view3d);
        RNA_pointer_set(&view3d_ptr, "camera", camera_ptr);
      }
    }
  }

  /* Compute the scene time based on the scene strip. */
  const float frame_index = seq::give_frame_index(
                                sequencer_scene, scene_strip, sequencer_scene->r.cfra) +
                            active_scene->r.sfra;
  if (active_scene->r.flag & SCER_SHOW_SUBFRAME) {
    active_scene->r.cfra = int(frame_index);
    active_scene->r.subframe = frame_index - int(frame_index);
  }
  else {
    active_scene->r.cfra = round_fl_to_int(frame_index);
    active_scene->r.subframe = 0.0f;
  }
  FRAMENUMBER_MIN_CLAMP(active_scene->r.cfra);

  /* Try to sync the object mode of the active object. */
  if (prev_obact) {
    Object *obact = CTX_data_active_object(&C);
    if (obact && prev_obact->type == obact->type) {
      object::mode_set(&C, eObjectMode(prev_obact->mode));
    }
  }

  DEG_id_tag_update(&active_scene->id, ID_RECALC_FRAME_CHANGE);
  WM_event_add_notifier(&C, NC_WINDOW, nullptr);
  WM_event_add_notifier(&C, NC_SCENE | ND_FRAME, nullptr);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Remove Gaps Operator
 * \{ */

static wmOperatorStatus sequencer_gap_remove_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  const bool do_all = RNA_boolean_get(op->ptr, "all");
  const Editing *ed = seq::editing_get(scene);

  seq::edit_remove_gaps(scene, ed->current_strips(), scene->r.cfra, do_all);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_gap_remove(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Remove Gaps";
  ot->idname = "SEQUENCER_OT_gap_remove";
  ot->description =
      "Remove gap at current frame to first strip at the right, independent of selection or "
      "locked state of strips";

  /* API callbacks. */
  //  ot->invoke = sequencer_snap_invoke;
  ot->exec = sequencer_gap_remove_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "all", false, "All Gaps", "Do all gaps to right of current frame");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Insert Gaps Operator
 * \{ */

static wmOperatorStatus sequencer_gap_insert_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  const int frames = RNA_int_get(op->ptr, "frames");
  const Editing *ed = seq::editing_get(scene);
  seq::transform_offset_after_frame(scene, ed->current_strips(), frames, scene->r.cfra);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_gap_insert(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Insert Gaps";
  ot->idname = "SEQUENCER_OT_gap_insert";
  ot->description =
      "Insert gap at current frame to first strips at the right, independent of selection or "
      "locked state of strips";

  /* API callbacks. */
  //  ot->invoke = sequencer_snap_invoke;
  ot->exec = sequencer_gap_insert_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_int(ot->srna,
              "frames",
              10,
              0,
              INT_MAX,
              "Frames",
              "Frames to insert after current strip",
              0,
              1000);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Snap Strips to the Current Frame Operator
 * \{ */

static wmOperatorStatus sequencer_snap_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);

  Editing *ed = seq::editing_get(scene);
  ListBase *channels = seq::channels_displayed_get(ed);
  int snap_frame;

  snap_frame = RNA_int_get(op->ptr, "frame");

  /* Check meta-strips. */
  LISTBASE_FOREACH (Strip *, strip, ed->current_strips()) {
    if (strip->flag & SELECT && !seq::transform_is_locked(channels, strip) &&
        seq::transform_strip_can_be_translated(strip))
    {
      if ((strip->flag & (SEQ_LEFTSEL + SEQ_RIGHTSEL)) == 0) {
        seq::transform_translate_strip(
            scene, strip, (snap_frame - strip->startofs) - strip->start);
      }
      else {
        if (strip->flag & SEQ_LEFTSEL) {
          seq::time_left_handle_frame_set(scene, strip, snap_frame);
        }
        else { /* SEQ_RIGHTSEL */
          seq::time_right_handle_frame_set(scene, strip, snap_frame);
        }
      }

      seq::relations_invalidate_cache(scene, strip);
    }
  }

  /* Test for effects and overlap. */
  LISTBASE_FOREACH (Strip *, strip, ed->current_strips()) {
    if (strip->flag & SELECT && !seq::transform_is_locked(channels, strip)) {
      strip->runtime.flag &= ~STRIP_OVERLAP;
      if (seq::transform_test_overlap(scene, ed->current_strips(), strip)) {
        seq::transform_seqbase_shuffle(ed->current_strips(), strip, scene);
      }
    }
  }

  /* Recalculate bounds of effect strips, offsetting the keyframes if not snapping any handle. */
  LISTBASE_FOREACH (Strip *, strip, ed->current_strips()) {
    if (strip->is_effect()) {
      const bool either_handle_selected = (strip->flag & (SEQ_LEFTSEL | SEQ_RIGHTSEL)) != 0;

      if (strip->input1 && (strip->input1->flag & SELECT)) {
        if (!either_handle_selected) {
          seq::offset_animdata(
              scene, strip, (snap_frame - seq::time_left_handle_frame_get(scene, strip)));
        }
      }
      else if (strip->input2 && (strip->input2->flag & SELECT)) {
        if (!either_handle_selected) {
          seq::offset_animdata(
              scene, strip, (snap_frame - seq::time_left_handle_frame_get(scene, strip)));
        }
      }
    }
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus sequencer_snap_invoke(bContext *C,
                                              wmOperator *op,
                                              const wmEvent * /*event*/)
{
  Scene *scene = CTX_data_sequencer_scene(C);

  int snap_frame;

  snap_frame = scene->r.cfra;

  RNA_int_set(op->ptr, "frame", snap_frame);
  return sequencer_snap_exec(C, op);
}

void SEQUENCER_OT_snap(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Snap Strips to the Current Frame";
  ot->idname = "SEQUENCER_OT_snap";
  ot->description = "Frame where selected strips will be snapped";

  /* API callbacks. */
  ot->invoke = sequencer_snap_invoke;
  ot->exec = sequencer_snap_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_int(ot->srna,
              "frame",
              0,
              INT_MIN,
              INT_MAX,
              "Frame",
              "Frame where selected strips will be snapped",
              INT_MIN,
              INT_MAX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Slip Strips Operator
 * \{ */

/* Modal Keymap */
enum {
  SLIP_MODAL_CANCEL = 1,
  SLIP_MODAL_CONFIRM,
  SLIP_MODAL_PRECISION_ENABLE,
  SLIP_MODAL_PRECISION_DISABLE,
  SLIP_MODAL_CLAMP_TOGGLE,
};

struct SlipData {
  NumInput num_input;
  VectorSet<Strip *> strips;
  /** Initial mouse position in view-space. */
  float init_mouse_co[2];
  /** Mouse and virtual mouse-cursor x-values in region-space. */
  int prev_mval_x;
  float virtual_mval_x;
  /** Parsed offset (integer when in precision mode, float otherwise). */
  float prev_offset;
  bool precision;
  /** Whether to show sub-frame offset in header. */
  bool show_subframe;

  /** Whether the user is currently clamping. */
  bool clamp;
  /** Whether at least one strip has enough content to clamp. */
  bool can_clamp;
  /** Whether some strips do not have enough content to clamp. */
  bool clamp_warning;
};

void slip_modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {SLIP_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", ""},
      {SLIP_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},

      {SLIP_MODAL_PRECISION_ENABLE, "PRECISION_ENABLE", 0, "Precision Enable", ""},
      {SLIP_MODAL_PRECISION_DISABLE, "PRECISION_DISABLE", 0, "Precision Disable", ""},
      {SLIP_MODAL_CLAMP_TOGGLE, "CLAMP_TOGGLE", 0, "Clamp Toggle", ""},

      {0, nullptr, 0, nullptr, nullptr},
  };

  wmKeyMap *keymap = WM_modalkeymap_find(keyconf, "Slip Modal");

  /* This function is called for each space-type, only needs to add map once. */
  if (keymap && keymap->modal_items) {
    return;
  }

  keymap = WM_modalkeymap_ensure(keyconf, "Slip Modal", modal_items);

  /* Assign map to operators. */
  WM_modalkeymap_assign(keymap, "SEQUENCER_OT_slip");
}

static void slip_draw_status(bContext *C, const wmOperator *op)
{
  SlipData *data = static_cast<SlipData *>(op->customdata);

  WorkspaceStatus status(C);

  status.opmodal(IFACE_("Confirm"), op->type, SLIP_MODAL_CONFIRM);
  status.opmodal(IFACE_("Cancel"), op->type, SLIP_MODAL_CANCEL);

  status.opmodal(IFACE_("Precision"), op->type, SLIP_MODAL_PRECISION_ENABLE, data->precision);

  if (data->can_clamp) {
    status.opmodal(IFACE_("Clamp"), op->type, SLIP_MODAL_CLAMP_TOGGLE, data->clamp);
  }
  if (data->clamp_warning) {
    status.item(TIP_("Not enough content to clamp strip(s)"), ICON_ERROR);
  }
}

static void slip_update_header(const Scene *scene,
                               ScrArea *area,
                               SlipData *data,
                               const float offset)
{
  if (area == nullptr) {
    return;
  }

  char msg[UI_MAX_DRAW_STR];
  if (hasNumInput(&data->num_input)) {
    char num_str[NUM_STR_REP_LEN];
    outputNumInput(&data->num_input, num_str, scene->unit);
    SNPRINTF_UTF8(msg, IFACE_("Slip Offset: Frames: %s"), num_str);
  }
  else {
    int frame_offset = std::trunc(offset);
    if (data->show_subframe) {
      float subframe_offset_sec = (offset - std::trunc(offset)) / scene->frames_per_second();
      SNPRINTF_UTF8(msg,
                    IFACE_("Slip Offset: Frames: %d Sound Offset: %.3f"),
                    frame_offset,
                    subframe_offset_sec);
    }
    else {
      SNPRINTF_UTF8(msg, IFACE_("Slip Offset: Frames: %d"), frame_offset);
    }
  }

  ED_area_status_text(area, msg);
}

static SlipData *slip_data_init(bContext *C, const wmOperator *op, const wmEvent *event)
{
  const Scene *scene = CTX_data_sequencer_scene(C);
  const Editing *ed = seq::editing_get(scene);
  const View2D *v2d = UI_view2d_fromcontext(C);

  SlipData *data = MEM_new<SlipData>("slipdata");

  VectorSet<Strip *> strips;
  if (RNA_boolean_get(op->ptr, "use_cursor_position") && event) {
    Strip *strip = strip_under_mouse_get(scene, v2d, event->mval);
    if (strip) {
      strips.add(strip);
    }
    if (!RNA_boolean_get(op->ptr, "ignore_connections")) {
      VectorSet<Strip *> connections = seq::connected_strips_get(strip);
      for (Strip *connection : connections) {
        strips.add(connection);
      }
    }
  }
  else {
    strips = seq::query_selected_strips(ed->current_strips());
  }

  ListBase *channels = seq::channels_displayed_get(seq::editing_get(scene));
  strips.remove_if([&](Strip *strip) {
    return (seq::transform_single_image_check(strip) || seq::transform_is_locked(channels, strip));
  });
  if (strips.is_empty()) {
    MEM_SAFE_DELETE(data);
    return nullptr;
  }
  data->strips = strips;

  data->show_subframe = false;
  data->clamp_warning = false;
  data->can_clamp = false;

  data->clamp = true;
  for (Strip *strip : strips) {
    strip->runtime.flag |= STRIP_SHOW_OFFSETS;

    /* If any strips start out with hold offsets visible, disable clamping on initialization. */
    if (strip->startofs < 0 || strip->endofs < 0) {
      data->clamp = false;
    }
    /* If any strips do not have enough underlying content to
     * fill their bounds, show a warning. */
    if (strip->len < seq::time_right_handle_frame_get(scene, strip) -
                         seq::time_left_handle_frame_get(scene, strip))
    {
      data->clamp_warning = true;
    }
    /* Strip exists with enough content, we can clamp. */
    else {
      data->can_clamp = true;
    }
    if (strip->type == STRIP_TYPE_SOUND_RAM) {
      data->show_subframe = true;
    }
  }

  return data;
}

static wmOperatorStatus sequencer_slip_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  ScrArea *area = CTX_wm_area(C);
  View2D *v2d = UI_view2d_fromcontext(C);

  SlipData *data = slip_data_init(C, op, event);
  if (data == nullptr) {
    return OPERATOR_CANCELLED;
  }
  op->customdata = data;

  initNumInput(&data->num_input);
  UI_view2d_region_to_view(
      v2d, event->mval[0], event->mval[1], &data->init_mouse_co[0], &data->init_mouse_co[1]);
  data->precision = false;
  data->prev_offset = 0.0f;
  data->prev_mval_x = event->mval[0];
  data->virtual_mval_x = event->mval[0];

  slip_draw_status(C, op);
  slip_update_header(scene, area, data, 0.0f);

  WM_event_add_modal_handler(C, op);

  /* Enable cursor wrapping. */
  op->type->flag |= OPTYPE_GRAB_CURSOR_X;

  /* Notify so we draw extensions immediately. */
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_RUNNING_MODAL;
}

static void slip_strips_delta(
    bContext *C, wmOperator *op, Scene *scene, SlipData *data, const float delta)
{
  float new_offset = data->prev_offset + delta;
  /* Calculate rounded whole frames between offsets, which cannot be determined from `delta` alone.
   * For example, 0.9 -> 1.0 would have a `delta` of 0.1 and a `frame_delta` of 1. */
  int frame_delta = std::trunc(new_offset) - std::trunc(data->prev_offset);

  float subframe_delta = 0.0f;
  /* Only apply subframe delta if the input is not an integer. */
  if (std::trunc(delta) != delta) {
    /* Note that `subframe_delta` has opposite sign to `frame_delta`
     * when `abs(delta)` < `abs(frame_delta)` to undo its effect. */
    subframe_delta = delta - frame_delta;
  }

  bool slip_keyframes = RNA_boolean_get(op->ptr, "slip_keyframes");
  for (Strip *strip : data->strips) {
    seq::time_slip_strip(scene, strip, frame_delta, subframe_delta, slip_keyframes);
    seq::relations_invalidate_cache(scene, strip);

    strip->runtime.flag &= ~(STRIP_CLAMPED_LH | STRIP_CLAMPED_RH);
    /* Reconstruct handle clamp state from first principles. */
    if (data->clamp == true) {
      if (seq::time_left_handle_frame_get(scene, strip) == seq::time_start_frame_get(strip)) {
        strip->runtime.flag |= STRIP_CLAMPED_LH;
      }
      if (seq::time_right_handle_frame_get(scene, strip) ==
          seq::time_content_end_frame_get(scene, strip))
      {
        strip->runtime.flag |= STRIP_CLAMPED_RH;
      }
    }
  }

  sync_active_scene_and_time_with_scene_strip(*C);

  RNA_float_set(op->ptr, "offset", new_offset);
  data->prev_offset = new_offset;
}

static void slip_cleanup(bContext *C, wmOperator *op, Scene *scene)
{
  ScrArea *area = CTX_wm_area(C);
  SlipData *data = static_cast<SlipData *>(op->customdata);

  for (Strip *strip : data->strips) {
    strip->runtime.flag &= ~(STRIP_CLAMPED_LH | STRIP_CLAMPED_RH);
    strip->runtime.flag &= ~STRIP_SHOW_OFFSETS;
  }

  MEM_SAFE_DELETE(data);
  if (area) {
    ED_area_status_text(area, nullptr);
  }
  ED_workspace_status_text(C, nullptr);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
}

/* Returns clamped offset delta relative to current strip positions,
 * which is the sum of the frame delta and the subframe delta. */
static float slip_apply_clamp(const Scene *scene, const SlipData *data, float *r_offset)
{
  float offset_delta = *r_offset - data->prev_offset;

  if (data->can_clamp) {
    for (Strip *strip : data->strips) {
      const float unclamped_start = seq::time_start_frame_get(strip) + strip->sound_offset +
                                    offset_delta;
      const float unclamped_end = seq::time_content_end_frame_get(scene, strip) +
                                  strip->sound_offset + offset_delta;

      const float left_handle = seq::time_left_handle_frame_get(scene, strip);
      const float right_handle = seq::time_right_handle_frame_get(scene, strip);

      float diff = 0;

      /* Clamp hold offsets if the option is currently enabled
       * and if there are enough frames to fill the strip. */
      if (data->clamp && strip->len >= right_handle - left_handle) {
        if (unclamped_start > left_handle) {
          diff = left_handle - unclamped_start;
        }
        else if (unclamped_end < right_handle) {
          diff = right_handle - unclamped_end;
        }
      }
      /* Always make sure each strip contains at least 1 frame of content,
       * even if the user hasn't enabled clamping. */
      else {
        if (unclamped_start > right_handle - 1) {
          diff = right_handle - 1 - unclamped_start;
        }

        if (unclamped_end < left_handle + 1) {
          diff = left_handle + 1 - unclamped_end;
        }
      }

      *r_offset += diff;
      offset_delta += diff;
    }
  }

  return offset_delta;
}

static wmOperatorStatus sequencer_slip_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);

  SlipData *data = slip_data_init(C, op, nullptr);
  if (data == nullptr) {
    return OPERATOR_CANCELLED;
  }
  op->customdata = data;

  float offset = RNA_float_get(op->ptr, "offset");
  slip_apply_clamp(scene, data, &offset);

  slip_strips_delta(C, op, scene, data, offset);

  slip_cleanup(C, op, scene);
  return OPERATOR_FINISHED;
}

static void slip_handle_num_input(
    bContext *C, wmOperator *op, ScrArea *area, SlipData *data, Scene *scene)
{
  float offset;
  applyNumInput(&data->num_input, &offset);

  const float offset_delta = slip_apply_clamp(scene, data, &offset);
  slip_update_header(scene, area, data, offset);
  RNA_float_set(op->ptr, "offset", offset);

  slip_strips_delta(C, op, scene, data, offset_delta);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
}

static wmOperatorStatus sequencer_slip_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  View2D *v2d = UI_view2d_fromcontext(C);
  Scene *scene = CTX_data_sequencer_scene(C);
  SlipData *data = static_cast<SlipData *>(op->customdata);
  ScrArea *area = CTX_wm_area(C);
  const bool has_num_input = hasNumInput(&data->num_input);

  if (event->val == KM_PRESS && handleNumInput(C, &data->num_input, event)) {
    slip_handle_num_input(C, op, area, data, scene);
    return OPERATOR_RUNNING_MODAL;
  }

  if (event->type == EVT_MODAL_MAP) {
    switch (event->val) {
      case SLIP_MODAL_CONFIRM: {
        slip_cleanup(C, op, scene);
        return OPERATOR_FINISHED;
      }
      case SLIP_MODAL_CANCEL: {
        slip_strips_delta(C, op, scene, data, -data->prev_offset);
        slip_cleanup(C, op, scene);
        return OPERATOR_CANCELLED;
      }
      case SLIP_MODAL_PRECISION_ENABLE:
        if (!has_num_input) {
          data->precision = true;
          /* Align virtual mouse pointer with the truncated frame to avoid jumps. */
          float mouse_co[2];
          UI_view2d_region_to_view(v2d, data->virtual_mval_x, 0.0f, &mouse_co[0], &mouse_co[1]);
          float offset = mouse_co[0] - data->init_mouse_co[0];
          float subframe_offset = offset - std::trunc(offset);
          data->virtual_mval_x += -subframe_offset * UI_view2d_scale_get_x(v2d);
        }
        break;
      case SLIP_MODAL_PRECISION_DISABLE:
        if (!has_num_input) {
          data->precision = false;
          /* If we exit precision mode, make sure we undo the fractional adjustments and align the
           * virtual mouse pointer. */
          float to_nearest_frame = -(data->prev_offset - round_fl_to_int(data->prev_offset));
          slip_strips_delta(C, op, scene, data, to_nearest_frame);
          data->virtual_mval_x += to_nearest_frame * UI_view2d_scale_get_x(v2d);
        }
        break;
      case SLIP_MODAL_CLAMP_TOGGLE:
        data->clamp = !data->clamp;
        break;
      default:
        break;
    }
  }

  if (event->type == MOUSEMOVE || event->val == SLIP_MODAL_PRECISION_DISABLE ||
      event->val == SLIP_MODAL_CLAMP_TOGGLE)
  {
    if (!has_num_input) {
      float mouse_x_delta = event->mval[0] - data->prev_mval_x;
      data->prev_mval_x += mouse_x_delta;
      if (data->precision) {
        mouse_x_delta *= 0.1f;
      }
      data->virtual_mval_x += mouse_x_delta;

      float mouse_co[2];
      UI_view2d_region_to_view(v2d, data->virtual_mval_x, 0.0f, &mouse_co[0], &mouse_co[1]);
      float offset = mouse_co[0] - data->init_mouse_co[0];
      if (!data->precision) {
        offset = std::trunc(offset);
      }

      float clamped_offset = offset;
      float clamped_offset_delta = slip_apply_clamp(scene, data, &clamped_offset);
      /* Also adjust virtual mouse pointer after clamp is applied. */
      data->virtual_mval_x += (clamped_offset - offset) * UI_view2d_scale_get_x(v2d);

      slip_strips_delta(C, op, scene, data, clamped_offset_delta);
      slip_update_header(scene, area, data, clamped_offset);

      WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
    }
  }

  slip_draw_status(C, op);

  return OPERATOR_RUNNING_MODAL;
}

void SEQUENCER_OT_slip(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Slip Strips";
  ot->idname = "SEQUENCER_OT_slip";
  ot->description = "Slip the contents of selected strips";

  /* API callbacks. */
  ot->invoke = sequencer_slip_invoke;
  ot->modal = sequencer_slip_modal;
  ot->exec = sequencer_slip_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_BLOCKING | OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties. */
  PropertyRNA *prop;

  prop = RNA_def_float(ot->srna,
                       "offset",
                       0,
                       -FLT_MAX,
                       FLT_MAX,
                       "Offset",
                       "Offset to the data of the strip",
                       -FLT_MAX,
                       FLT_MAX);
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 100, 0);
  RNA_def_boolean(ot->srna,
                  "slip_keyframes",
                  false,
                  "Slip Keyframes",
                  "Move the keyframes alongside the media");
  RNA_def_boolean(ot->srna,
                  "use_cursor_position",
                  false,
                  "Use Cursor Position",
                  "Slip strips under mouse cursor instead of all selected strips");
  RNA_def_boolean(ot->srna,
                  "ignore_connections",
                  false,
                  "Ignore Connections",
                  "Do not slip connected strips if using cursor position");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mute Strips Operator
 * \{ */

static wmOperatorStatus sequencer_mute_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  VectorSet strips = all_strips_from_context(C);

  for (Strip *strip : strips) {
    if (!RNA_boolean_get(op->ptr, "unselected")) {
      if (strip->flag & SELECT) {
        strip->flag |= SEQ_MUTE;
        seq::relations_invalidate_cache(scene, strip);
      }
    }
    else {
      if ((strip->flag & SELECT) == 0) {
        strip->flag |= SEQ_MUTE;
        seq::relations_invalidate_cache(scene, strip);
      }
    }
  }

  sync_active_scene_and_time_with_scene_strip(*C);

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_mute(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Mute Strips";
  ot->idname = "SEQUENCER_OT_mute";
  ot->description = "Mute (un)selected strips";

  /* API callbacks. */
  ot->exec = sequencer_mute_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(
      ot->srna, "unselected", false, "Unselected", "Mute unselected rather than selected strips");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Unmute Strips Operator
 * \{ */

static wmOperatorStatus sequencer_unmute_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(scene);
  ARegion *region = CTX_wm_region(C);
  const bool is_preview = region && (region->regiontype == RGN_TYPE_PREVIEW) &&
                          sequencer_view_preview_only_poll(C);
  LISTBASE_FOREACH (Strip *, strip, ed->current_strips()) {
    if (is_preview) {
      if (seq::time_strip_intersects_frame(scene, strip, scene->r.cfra) &&
          strip->type != STRIP_TYPE_SOUND_RAM)
      {
        strip->flag &= ~SEQ_MUTE;
        seq::relations_invalidate_cache(scene, strip);
      }
    }
    else if (!RNA_boolean_get(op->ptr, "unselected")) {
      if (strip->flag & SELECT) {
        strip->flag &= ~SEQ_MUTE;
        seq::relations_invalidate_cache(scene, strip);
      }
    }
    else {
      if ((strip->flag & SELECT) == 0) {
        strip->flag &= ~SEQ_MUTE;
        seq::relations_invalidate_cache(scene, strip);
      }
    }
  }

  sync_active_scene_and_time_with_scene_strip(*C);

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_unmute(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Unmute Strips";
  ot->idname = "SEQUENCER_OT_unmute";
  ot->description = "Unmute (un)selected strips";

  /* API callbacks. */
  ot->exec = sequencer_unmute_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna,
                  "unselected",
                  false,
                  "Unselected",
                  "Unmute unselected rather than selected strips");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lock Strips Operator
 * \{ */

static wmOperatorStatus sequencer_lock_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(scene);

  LISTBASE_FOREACH (Strip *, strip, ed->current_strips()) {
    if (strip->flag & SELECT) {
      strip->flag |= SEQ_LOCK;
    }
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_lock(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Lock Strips";
  ot->idname = "SEQUENCER_OT_lock";
  ot->description = "Lock strips so they cannot be transformed";

  /* API callbacks. */
  ot->exec = sequencer_lock_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Unlock Strips Operator
 * \{ */

static wmOperatorStatus sequencer_unlock_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(scene);

  LISTBASE_FOREACH (Strip *, strip, ed->current_strips()) {
    if (strip->flag & SELECT) {
      strip->flag &= ~SEQ_LOCK;
    }
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_unlock(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Unlock Strips";
  ot->idname = "SEQUENCER_OT_unlock";
  ot->description = "Unlock strips so they can be transformed";

  /* API callbacks. */
  ot->exec = sequencer_unlock_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Connect Strips Operator
 * \{ */

static wmOperatorStatus sequencer_connect_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(scene);
  ListBase *active_seqbase = seq::active_seqbase_get(ed);

  VectorSet<Strip *> selected = seq::query_selected_strips(active_seqbase);

  if (selected.is_empty()) {
    return OPERATOR_CANCELLED;
  }

  const bool toggle = RNA_boolean_get(op->ptr, "toggle");
  if (toggle && seq::are_strips_connected_together(selected)) {
    seq::disconnect(selected);
  }
  else {
    seq::connect(selected);
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_connect(wmOperatorType *ot)
{
  ot->name = "Connect Strips";
  ot->idname = "SEQUENCER_OT_connect";
  ot->description = "Link selected strips together for simplified group selection";

  ot->exec = sequencer_connect_exec;
  ot->poll = sequencer_edit_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "toggle", true, "Toggle", "Toggle strip connections");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Disconnect Strips Operator
 * \{ */

static wmOperatorStatus sequencer_disconnect_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(scene);
  ListBase *active_seqbase = seq::active_seqbase_get(ed);

  VectorSet<Strip *> selected = seq::query_selected_strips(active_seqbase);

  if (seq::disconnect(selected)) {
    WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

void SEQUENCER_OT_disconnect(wmOperatorType *ot)
{
  ot->name = "Disconnect Strips";
  ot->idname = "SEQUENCER_OT_disconnect";
  ot->description = "Unlink selected strips so that they can be selected individually";

  ot->exec = sequencer_disconnect_exec;
  ot->poll = sequencer_edit_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Reload Strips Operator
 * \{ */

static wmOperatorStatus sequencer_reload_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(scene);
  const bool adjust_length = RNA_boolean_get(op->ptr, "adjust_length");

  LISTBASE_FOREACH (Strip *, strip, ed->current_strips()) {
    if (strip->flag & SELECT) {
      seq::add_reload_new_file(bmain, scene, strip, !adjust_length);
      seq::thumbnail_cache_invalidate_strip(scene, strip);

      if (adjust_length) {
        if (seq::transform_test_overlap(scene, ed->current_strips(), strip)) {
          seq::transform_seqbase_shuffle(ed->current_strips(), strip, scene);
        }
      }
    }
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_reload(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers. */
  ot->name = "Reload Strips";
  ot->idname = "SEQUENCER_OT_reload";
  ot->description = "Reload strips in the sequencer";

  /* API callbacks. */
  ot->exec = sequencer_reload_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER; /* No undo, the data changed is stored outside 'main'. */

  prop = RNA_def_boolean(ot->srna,
                         "adjust_length",
                         false,
                         "Adjust Length",
                         "Adjust length of strips to their data length");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Refresh Sequencer Operator
 * \{ */

static bool sequencer_refresh_all_poll(bContext *C)
{
  if (G.is_rendering) {
    return false;
  }
  return sequencer_edit_poll(C);
}

static wmOperatorStatus sequencer_refresh_all_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(scene);

  seq::relations_free_imbuf(scene, &ed->seqbase, false);
  seq::media_presence_free(scene);
  seq::cache_cleanup(scene, seq::CacheCleanup::All);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_refresh_all(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Refresh Sequencer";
  ot->idname = "SEQUENCER_OT_refresh_all";
  ot->description = "Refresh the sequencer editor";

  /* API callbacks. */
  ot->exec = sequencer_refresh_all_exec;
  ot->poll = sequencer_refresh_all_poll;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Reassign Inputs Operator
 * \{ */

StringRef effect_inputs_validate(const VectorSet<Strip *> &inputs, int num_inputs)
{
  if (inputs.size() > 2) {
    return "Cannot apply effect to more than 2 strips with video content";
  }
  if (num_inputs == 2 && inputs.size() != 2) {
    return "Exactly 2 selected strips with video content are needed";
  }
  if (num_inputs == 1 && inputs.size() != 1) {
    return "Exactly one selected strip with video content is needed";
  }
  return "";
}

VectorSet<Strip *> strip_effect_get_new_inputs(const Scene *scene,
                                               int num_inputs,
                                               bool ignore_active)
{
  if (num_inputs == 0) {
    return {};
  }

  Editing *ed = seq::editing_get(scene);
  VectorSet<Strip *> selected_strips = seq::query_selected_strips(ed->current_strips());
  /* Ignore sound strips for now (avoids unnecessary errors when connected strips are
   * selected together, and the intent to operate on strips with video content is clear). */
  selected_strips.remove_if([&](Strip *strip) { return strip->type == STRIP_TYPE_SOUND_RAM; });

  if (ignore_active) {
    /* If `ignore_active` is true, this function is being called from the reassign inputs
     * operator, meaning the active strip must be the effect strip to reassign. */
    Strip *active_strip = seq::select_active_get(scene);
    selected_strips.remove_if([&](Strip *strip) { return strip == active_strip; });
  }

  if (selected_strips.size() > num_inputs) {
    VectorSet<Strip *> inputs;
    for (int64_t i : IndexRange(num_inputs)) {
      inputs.add(selected_strips[i]);
    }
    return inputs;
  }

  return selected_strips;
}

static wmOperatorStatus sequencer_reassign_inputs_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Strip *active_strip = seq::select_active_get(scene);
  const int num_inputs = seq::effect_get_num_inputs(active_strip->type);

  if (num_inputs == 0) {
    BKE_report(op->reports, RPT_ERROR, "Cannot reassign inputs: strip has no inputs");
    return OPERATOR_CANCELLED;
  }

  VectorSet<Strip *> inputs = strip_effect_get_new_inputs(scene, num_inputs, true);
  StringRef error_msg = effect_inputs_validate(inputs, num_inputs);

  if (!error_msg.is_empty()) {
    BKE_report(op->reports, RPT_ERROR, error_msg.data());
    return OPERATOR_CANCELLED;
  }

  Strip *input1 = inputs[0];
  Strip *input2 = inputs.size() == 2 ? inputs[1] : nullptr;

  /* Check if reassigning would create recursivity. */
  if (seq::relations_render_loop_check(input1, active_strip) ||
      seq::relations_render_loop_check(input2, active_strip))
  {
    BKE_report(op->reports, RPT_ERROR, "Cannot reassign inputs: recursion detected");
    return OPERATOR_CANCELLED;
  }

  active_strip->input1 = input1;
  active_strip->input2 = input2;

  int old_start = active_strip->start;

  /* Force time position update for reassigned effects.
   * TODO(Richard): This is because internally startdisp is still used, due to poor performance
   * of mapping effect range to inputs. This mapping could be cached though. */
  seq::strip_lookup_invalidate(scene->ed);
  seq::time_left_handle_frame_set(scene, input1, seq::time_left_handle_frame_get(scene, input1));

  Editing *ed = seq::editing_get(scene);
  ListBase *active_seqbase = seq::active_seqbase_get(ed);
  if (seq::transform_test_overlap(scene, active_seqbase, active_strip)) {
    seq::transform_seqbase_shuffle(active_seqbase, active_strip, scene);
  }

  seq::relations_invalidate_cache(scene, active_strip);
  seq::offset_animdata(scene, active_strip, (active_strip->start - old_start));

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_reassign_inputs(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Reassign Inputs";
  ot->idname = "SEQUENCER_OT_reassign_inputs";
  ot->description = "Reassign the inputs for the effect strip";

  /* API callbacks. */
  ot->exec = sequencer_reassign_inputs_exec;
  ot->poll = sequencer_effect_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Swap Inputs Operator
 * \{ */

static wmOperatorStatus sequencer_swap_inputs_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Strip *active_strip = seq::select_active_get(scene);

  if (!active_strip->is_effect()) {
    BKE_report(op->reports, RPT_ERROR, "Active strip is not an effect strip");
    return OPERATOR_CANCELLED;
  }

  if (seq::effect_get_num_inputs(active_strip->type) != 2 || active_strip->input1 == nullptr ||
      active_strip->input2 == nullptr)
  {
    BKE_report(op->reports, RPT_ERROR, "Strip needs two inputs to swap");
    return OPERATOR_CANCELLED;
  }

  Strip *strip = active_strip->input1;
  active_strip->input1 = active_strip->input2;
  active_strip->input2 = strip;

  seq::relations_invalidate_cache(scene, active_strip);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}
void SEQUENCER_OT_swap_inputs(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Swap Inputs";
  ot->idname = "SEQUENCER_OT_swap_inputs";
  ot->description = "Swap the two inputs of the effect strip";

  /* API callbacks. */
  ot->exec = sequencer_swap_inputs_exec;
  ot->poll = sequencer_swap_inputs_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Split Strips Operator
 * \{ */

static int mouse_frame_side(View2D *v2d, short mouse_x, int frame)
{
  int mval[2];
  float mouseloc[2];

  mval[0] = mouse_x;
  mval[1] = 0;

  /* Choose the side based on which side of the current frame the mouse is on. */
  UI_view2d_region_to_view(v2d, mval[0], mval[1], &mouseloc[0], &mouseloc[1]);

  return mouseloc[0] > frame ? seq::SIDE_RIGHT : seq::SIDE_LEFT;
}

static const EnumPropertyItem prop_split_types[] = {
    {seq::SPLIT_SOFT, "SOFT", 0, "Soft", ""},
    {seq::SPLIT_HARD, "HARD", 0, "Hard", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem prop_side_types[] = {
    {seq::SIDE_MOUSE, "MOUSE", 0, "Mouse Position", ""},
    {seq::SIDE_LEFT, "LEFT", 0, "Left", ""},
    {seq::SIDE_RIGHT, "RIGHT", 0, "Right", ""},
    {seq::SIDE_BOTH, "BOTH", 0, "Both", ""},
    {seq::SIDE_NO_CHANGE, "NO_CHANGE", 0, "No Change", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

/* Get the splitting side for the Split Strips's operator exec() callback. */
static int sequence_split_side_for_exec_get(wmOperator *op)
{
  const int split_side = RNA_enum_get(op->ptr, "side");

  /* The mouse position can not be resolved from the exec() as the mouse coordinate is not
   * accessible. So fall-back to the RIGHT side instead.
   *
   * The SEQ_SIDE_MOUSE is used by the Strip menu, together with the EXEC_DEFAULT operator
   * context in order to have properly resolved shortcut in the menu. */
  if (split_side == seq::SIDE_MOUSE) {
    return seq::SIDE_RIGHT;
  }

  return split_side;
}

static wmOperatorStatus sequencer_split_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(scene);
  bool changed = false;
  bool strip_selected = false;

  const bool use_cursor_position = RNA_boolean_get(op->ptr, "use_cursor_position");

  const int split_frame = RNA_struct_property_is_set(op->ptr, "frame") ?
                              RNA_int_get(op->ptr, "frame") :
                              scene->r.cfra;
  const int split_channel = RNA_int_get(op->ptr, "channel");

  const seq::eSplitMethod method = seq::eSplitMethod(RNA_enum_get(op->ptr, "type"));
  const int split_side = sequence_split_side_for_exec_get(op);
  const bool ignore_selection = RNA_boolean_get(op->ptr, "ignore_selection");
  const bool ignore_connections = RNA_boolean_get(op->ptr, "ignore_connections");

  seq::prefetch_stop(scene);

  LISTBASE_FOREACH_BACKWARD (Strip *, strip, ed->current_strips()) {
    if (use_cursor_position && strip->channel != split_channel) {
      continue;
    }

    if (ignore_selection || strip->flag & SELECT) {
      const char *error_msg = nullptr;
      if (seq::edit_strip_split(bmain,
                                scene,
                                ed->current_strips(),
                                strip,
                                split_frame,
                                method,
                                ignore_connections,
                                &error_msg) != nullptr)
      {
        changed = true;
      }
      if (error_msg != nullptr) {
        BKE_report(op->reports, RPT_ERROR, error_msg);
      }
    }
  }

  if (changed) { /* Got new strips? */
    if (ignore_selection) {
      if (use_cursor_position) {
        LISTBASE_FOREACH (Strip *, strip, seq::active_seqbase_get(ed)) {
          if (seq::time_right_handle_frame_get(scene, strip) == split_frame &&
              strip->channel == split_channel)
          {
            strip_selected = strip->flag & STRIP_ALLSEL;
          }
        }
        if (!strip_selected) {
          LISTBASE_FOREACH (Strip *, strip, seq::active_seqbase_get(ed)) {
            if (seq::time_left_handle_frame_get(scene, strip) == split_frame &&
                strip->channel == split_channel)
            {
              strip->flag &= ~STRIP_ALLSEL;
            }
          }
        }
      }
    }
    else {
      if (split_side != seq::SIDE_BOTH) {
        LISTBASE_FOREACH (Strip *, strip, seq::active_seqbase_get(ed)) {
          if (split_side == seq::SIDE_LEFT) {
            if (seq::time_left_handle_frame_get(scene, strip) >= split_frame) {
              strip->flag &= ~STRIP_ALLSEL;
            }
          }
          else {
            if (seq::time_right_handle_frame_get(scene, strip) <= split_frame) {
              strip->flag &= ~STRIP_ALLSEL;
            }
          }
        }
      }
    }
  }
  if (changed) {
    WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
    return OPERATOR_FINISHED;
  }

  /* Passthrough to selection if used as tool. */
  return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
}

static wmOperatorStatus sequencer_split_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  View2D *v2d = UI_view2d_fromcontext(C);

  int split_side = RNA_enum_get(op->ptr, "side");
  int split_frame = scene->r.cfra;

  if (split_side == seq::SIDE_MOUSE) {
    if (ED_operator_sequencer_active(C) && v2d) {
      split_side = mouse_frame_side(v2d, event->mval[0], split_frame);
    }
    else {
      split_side = seq::SIDE_BOTH;
    }
  }
  float mouseloc[2];
  if (v2d) {
    UI_view2d_region_to_view(v2d, event->mval[0], event->mval[1], &mouseloc[0], &mouseloc[1]);
    if (RNA_boolean_get(op->ptr, "use_cursor_position")) {
      split_frame = round_fl_to_int(mouseloc[0]);
      Strip *strip = strip_under_mouse_get(scene, v2d, event->mval);
      if (strip == nullptr || split_frame == seq::time_left_handle_frame_get(scene, strip) ||
          split_frame == seq::time_right_handle_frame_get(scene, strip))
      {
        /* Do not pass through to selection. */
        return OPERATOR_CANCELLED;
      }
    }
    RNA_int_set(op->ptr, "channel", mouseloc[1]);
  }
  RNA_int_set(op->ptr, "frame", split_frame);
  RNA_enum_set(op->ptr, "side", split_side);
  // RNA_enum_set(op->ptr, "type", split_hard);

  return sequencer_split_exec(C, op);
}

static void sequencer_split_ui(bContext * /*C*/, wmOperator *op)
{
  uiLayout *layout = op->layout;
  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);

  uiLayout *row = &layout->row(false);
  row->prop(op->ptr, "type", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
  layout->prop(op->ptr, "frame", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(op->ptr, "side", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  layout->separator();

  layout->prop(op->ptr, "use_cursor_position", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  if (RNA_boolean_get(op->ptr, "use_cursor_position")) {
    layout->prop(op->ptr, "channel", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  layout->separator();

  layout->prop(op->ptr, "ignore_connections", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

void SEQUENCER_OT_split(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Split Strips";
  ot->idname = "SEQUENCER_OT_split";
  ot->description = "Split the selected strips in two";

  /* API callbacks. */
  ot->invoke = sequencer_split_invoke;
  ot->exec = sequencer_split_exec;
  ot->poll = sequencer_edit_poll;
  ot->ui = sequencer_split_ui;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  PropertyRNA *prop;
  RNA_def_int(ot->srna,
              "frame",
              0,
              INT_MIN,
              INT_MAX,
              "Frame",
              "Frame where selected strips will be split",
              INT_MIN,
              INT_MAX);
  RNA_def_int(ot->srna,
              "channel",
              0,
              INT_MIN,
              INT_MAX,
              "Channel",
              "Channel in which strip will be cut",
              INT_MIN,
              INT_MAX);
  RNA_def_enum(ot->srna,
               "type",
               prop_split_types,
               seq::SPLIT_SOFT,
               "Type",
               "The type of split operation to perform on strips");

  RNA_def_boolean(ot->srna,
                  "use_cursor_position",
                  false,
                  "Use Cursor Position",
                  "Split at position of the cursor instead of current frame");

  prop = RNA_def_enum(ot->srna,
                      "side",
                      prop_side_types,
                      seq::SIDE_MOUSE,
                      "Side",
                      "The side that remains selected after splitting");

  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(
      ot->srna,
      "ignore_selection",
      false,
      "Ignore Selection",
      "Make cut even if strip is not selected preserving selection state after cut");

  RNA_def_property_flag(prop, PROP_HIDDEN);

  RNA_def_boolean(ot->srna,
                  "ignore_connections",
                  false,
                  "Ignore Connections",
                  "Don't propagate split to connected strips");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Duplicate Strips Operator
 * \{ */

static void sequencer_report_duplicates(wmOperator *op, ListBase *duplicated_strips)
{
  int num_scenes = 0, num_movieclips = 0, num_masks = 0;
  LISTBASE_FOREACH (Strip *, strip, duplicated_strips) {
    switch (strip->type) {
      case STRIP_TYPE_SCENE:
        num_scenes++;
        break;
      case STRIP_TYPE_MOVIECLIP:
        num_movieclips++;
        break;
      case STRIP_TYPE_MASK:
        num_masks++;
        break;
      default:
        break;
    }
  }

  if (num_scenes == 0 && num_movieclips == 0 && num_masks == 0) {
    return;
  }

  std::string report;
  std::string sep;
  if (num_scenes) {
    report += fmt::format("{}{} {}",
                          sep,
                          num_scenes,
                          (num_scenes > 1) ? RPT_(BKE_idtype_idcode_to_name_plural(ID_SCE)) :
                                             RPT_(BKE_idtype_idcode_to_name(ID_SCE)));
    sep = ", ";
  }
  if (num_movieclips) {
    report += fmt::format("{}{} {}",
                          sep,
                          num_movieclips,
                          (num_movieclips > 1) ? RPT_(BKE_idtype_idcode_to_name_plural(ID_MC)) :
                                                 RPT_(BKE_idtype_idcode_to_name(ID_MC)));
    sep = ", ";
  }
  if (num_masks) {
    report += fmt::format("{}{} {}",
                          sep,
                          num_masks,
                          (num_masks > 1) ? RPT_(BKE_idtype_idcode_to_name_plural(ID_MSK)) :
                                            RPT_(BKE_idtype_idcode_to_name(ID_MSK)));
    sep = ", ";
  }

  BKE_reportf(op->reports, RPT_INFO, RPT_("Duplicated %s"), report.c_str());
}

static wmOperatorStatus sequencer_add_duplicate_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(scene);
  ARegion *region = CTX_wm_region(C);

  const bool linked = RNA_boolean_get(op->ptr, "linked");

  if (ed == nullptr) {
    return OPERATOR_CANCELLED;
  }

  Strip *active_strip = seq::select_active_get(scene);
  ListBase duplicated_strips = {nullptr, nullptr};

  /* Special case for duplicating strips in preview: Do not duplicate sound strips,muted
   * strips and strips that do not intersect the current frame */
  if (region->regiontype == RGN_TYPE_PREVIEW && sequencer_view_preview_only_poll(C)) {
    LISTBASE_FOREACH (Strip *, strip, ed->current_strips()) {
      if (strip->type == STRIP_TYPE_SOUND_RAM || strip->flag & SEQ_MUTE ||
          !seq::time_strip_intersects_frame(scene, strip, scene->r.cfra))
      {
        strip->flag &= ~STRIP_ALLSEL;
      }
    }
  }

  const seq::StripDuplicate dupe_flag = linked ? seq::StripDuplicate::Selected :
                                                 (seq::StripDuplicate::Selected |
                                                  seq::StripDuplicate::Data);
  seq::seqbase_duplicate_recursive(
      bmain, scene, scene, &duplicated_strips, ed->current_strips(), dupe_flag, 0);
  deselect_all_strips(scene);

  if (duplicated_strips.first == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* Report all the newly created datablocks in the status bar. */
  if (!linked) {
    sequencer_report_duplicates(op, &duplicated_strips);
  }

  /* Duplicate animation.
   * First backup original curves from scene and duplicate strip curves from backup into scene.
   * This way, when pasted strips are renamed, curves are renamed with them. Finally, restore
   * original curves from backup.
   */
  seq::AnimationBackup animation_backup = {{nullptr}};
  seq::animation_backup_original(scene, &animation_backup);

  ListBase *seqbase = seq::active_seqbase_get(seq::editing_get(scene));
  Strip *strip_last = static_cast<Strip *>(seqbase->last);

  /* Rely on the `duplicated_strips` list being added at the end.
   * Their UIDs has been re-generated by the #SEQ_sequence_base_dupli_recursive(). */
  BLI_movelisttolist(ed->current_strips(), &duplicated_strips);

  /* Handle duplicated strips: set active, select, ensure unique name and duplicate animation
   * data. */
  for (Strip *strip = strip_last->next; strip; strip = strip->next) {
    if (active_strip != nullptr && STREQ(strip->name, active_strip->name)) {
      seq::select_active_set(scene, strip);
    }
    strip->flag &= ~(SEQ_LEFTSEL + SEQ_RIGHTSEL + SEQ_LOCK);
    strip->runtime.flag |= STRIP_IGNORE_CHANNEL_LOCK;

    seq::animation_duplicate_backup_to_scene(scene, strip, &animation_backup);
    seq::ensure_unique_name(strip, scene);
  }

  /* Special case for duplicating strips in preview: handle overlap, because strips won't be
   * translated. */
  if (region->regiontype == RGN_TYPE_PREVIEW && sequencer_view_preview_only_poll(C)) {
    for (Strip *strip = strip_last->next; strip; strip = strip->next) {
      if (seq::transform_test_overlap(scene, ed->current_strips(), strip)) {
        seq::transform_seqbase_shuffle(ed->current_strips(), strip, scene);
      }
      strip->runtime.flag &= ~STRIP_IGNORE_CHANNEL_LOCK;
    }
  }

  seq::animation_restore_original(scene, &animation_backup);

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  DEG_relations_tag_update(CTX_data_main(C));
  sequencer_select_do_updates(C, scene);
  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_duplicate(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers. */
  ot->name = "Duplicate Strips";
  ot->idname = "SEQUENCER_OT_duplicate";
  ot->description = "Duplicate the selected strips";

  /* API callbacks. */
  ot->exec = sequencer_add_duplicate_exec;
  ot->poll = ED_operator_sequencer_active;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  prop = RNA_def_boolean(ot->srna,
                         "linked",
                         false,
                         "Linked",
                         "Duplicate strip but not strip data, linking to the original data");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Erase Strips Operator
 * \{ */

static void sequencer_delete_strip_data(bContext *C, Strip *strip)
{
  if (strip->type != STRIP_TYPE_SCENE) {
    return;
  }

  Main *bmain = CTX_data_main(C);
  if (strip->scene) {
    if (ED_scene_delete(C, bmain, strip->scene)) {
      WM_event_add_notifier(C, NC_SCENE | NA_REMOVED, strip->scene);
    }
  }
}

static wmOperatorStatus sequencer_delete_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_sequencer_scene(C);
  ListBase *seqbasep = seq::active_seqbase_get(seq::editing_get(scene));
  const bool delete_data = RNA_boolean_get(op->ptr, "delete_data");

  if (sequencer_view_has_preview_poll(C) && !sequencer_view_preview_only_poll(C)) {
    return OPERATOR_CANCELLED;
  }

  seq::prefetch_stop(scene);

  for (Strip *strip : selected_strips_from_context(C)) {
    seq::edit_flag_for_removal(scene, seqbasep, strip);
    if (delete_data) {
      sequencer_delete_strip_data(C, strip);
    }
  }
  seq::edit_remove_flagged_strips(scene, seqbasep);

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  if (scene->adt && scene->adt->action) {
    DEG_id_tag_update(&scene->adt->action->id, ID_RECALC_ANIMATION_NO_FLUSH);
  }
  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
  WM_event_add_notifier(C, NC_SCENE | ND_ANIMCHAN, scene);
  return OPERATOR_FINISHED;
}

static wmOperatorStatus sequencer_delete_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  ListBase *markers = &scene->markers;

  if (!BLI_listbase_is_empty(markers)) {
    ARegion *region = CTX_wm_region(C);
    if (region && (region->regiontype == RGN_TYPE_WINDOW)) {
      /* Bounding box of 30 pixels is used for markers shortcuts,
       * prevent conflict with markers shortcuts here. */
      if (event->mval[1] <= 30) {
        return OPERATOR_PASS_THROUGH;
      }
    }
  }

  return sequencer_delete_exec(C, op);
}

void SEQUENCER_OT_delete(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Delete Strips";
  ot->idname = "SEQUENCER_OT_delete";
  ot->description = "Delete selected strips from the sequencer";

  /* API callbacks. */
  ot->invoke = sequencer_delete_invoke;
  ot->exec = sequencer_delete_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties. */
  ot->prop = RNA_def_boolean(ot->srna,
                             "delete_data",
                             false,
                             "Delete Data",
                             "After removing the Strip, delete the associated data also");
  RNA_def_property_flag(ot->prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clear Strip Offset Operator
 * \{ */

static wmOperatorStatus sequencer_offset_clear_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(scene);
  Strip *strip;
  ListBase *channels = seq::channels_displayed_get(seq::editing_get(scene));

  /* For effects, try to find a replacement input. */
  for (strip = static_cast<Strip *>(ed->current_strips()->first); strip;
       strip = static_cast<Strip *>(strip->next))
  {
    if (seq::transform_is_locked(channels, strip)) {
      continue;
    }

    if (!strip->is_effect() && (strip->flag & SELECT)) {
      strip->startofs = strip->endofs = 0;
    }
  }

  /* Update lengths, etc. */
  strip = static_cast<Strip *>(ed->current_strips()->first);
  while (strip) {
    seq::relations_invalidate_cache(scene, strip);
    strip = strip->next;
  }

  for (strip = static_cast<Strip *>(ed->current_strips()->first); strip;
       strip = static_cast<Strip *>(strip->next))
  {
    if (!strip->is_effect() && (strip->flag & SELECT)) {
      if (seq::transform_test_overlap(scene, ed->current_strips(), strip)) {
        seq::transform_seqbase_shuffle(ed->current_strips(), strip, scene);
      }
    }
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_offset_clear(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Clear Strip Offset";
  ot->idname = "SEQUENCER_OT_offset_clear";
  ot->description = "Clear strip in/out offsets from the start and end of content";

  /* API callbacks. */
  ot->exec = sequencer_offset_clear_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Separate Images Operator
 * \{ */

static wmOperatorStatus sequencer_separate_images_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(scene);
  ListBase *seqbase = seq::active_seqbase_get(ed);

  Strip *strip, *strip_new;
  StripData *data_new;
  StripElem *se, *se_new;
  int start_ofs, timeline_frame, frame_end;
  int step = RNA_int_get(op->ptr, "length");

  strip = static_cast<Strip *>(seqbase->first); /* Poll checks this is valid. */

  seq::prefetch_stop(scene);

  while (strip) {
    if ((strip->flag & SELECT) && (strip->type == STRIP_TYPE_IMAGE) && (strip->len > 1)) {
      Strip *strip_next;

      /* TODO: remove f-curve and assign to split image strips.
       * The old animation system would remove the user of `strip->ipo_legacy`. */

      start_ofs = timeline_frame = seq::time_left_handle_frame_get(scene, strip);
      frame_end = seq::time_right_handle_frame_get(scene, strip);

      while (timeline_frame < frame_end) {
        /* New strip. */
        se = seq::render_give_stripelem(scene, strip, timeline_frame);

        strip_new = seq::strip_duplicate_recursive(
            bmain, scene, scene, seqbase, strip, seq::StripDuplicate::UniqueName);

        strip_new->start = start_ofs;
        strip_new->type = STRIP_TYPE_IMAGE;
        strip_new->len = 1;
        strip_new->flag |= SEQ_SINGLE_FRAME_CONTENT;
        strip_new->endofs = 1 - step;

        /* New strip. */
        data_new = strip_new->data;
        data_new->us = 1;

        /* New stripdata, only one element now. */
        /* Note this assume all elements (images) have the same dimension,
         * since we only copy the name here. */
        se_new = static_cast<StripElem *>(MEM_reallocN(data_new->stripdata, sizeof(*se_new)));
        STRNCPY_UTF8(se_new->filename, se->filename);
        data_new->stripdata = se_new;

        if (step > 1) {
          strip_new->runtime.flag &= ~STRIP_OVERLAP;
          if (seq::transform_test_overlap(scene, seqbase, strip_new)) {
            seq::transform_seqbase_shuffle(seqbase, strip_new, scene);
          }
        }

        /* XXX, COPY FCURVES */

        timeline_frame++;
        start_ofs += step;
      }

      strip_next = static_cast<Strip *>(strip->next);
      seq::edit_flag_for_removal(scene, seqbase, strip);
      strip = strip_next;
    }
    else {
      strip = strip->next;
    }
  }

  seq::edit_remove_flagged_strips(scene, seqbase);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus sequencer_separate_images_invoke(bContext *C,
                                                         wmOperator *op,
                                                         const wmEvent *event)
{
  return WM_operator_props_popup_confirm_ex(
      C, op, event, IFACE_("Separate Sequence Images"), IFACE_("Separate"));
}

void SEQUENCER_OT_images_separate(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Separate Images";
  ot->idname = "SEQUENCER_OT_images_separate";
  ot->description = "On image sequence strips, it returns a strip for each image";

  /* API callbacks. */
  ot->exec = sequencer_separate_images_exec;
  ot->invoke = sequencer_separate_images_invoke;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_int(ot->srna, "length", 1, 1, INT_MAX, "Length", "Length of each frame", 1, 1000);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle Meta Strip Operator
 * \{ */

static wmOperatorStatus sequencer_meta_toggle_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(scene);
  Strip *active_strip = seq::select_active_get(scene);

  seq::prefetch_stop(scene);

  if (active_strip && active_strip->type == STRIP_TYPE_META && active_strip->flag & SELECT) {
    /* Deselect active meta strip. */
    seq::select_active_set(scene, nullptr);
    seq::meta_stack_set(scene, active_strip);
    /* Invalidate the cache of the meta strip when going in and out of it.
     * The cache does not consider if we are inside of a meta strip or not,
     * so we have to make sure that we recache so it is not using outdated data.
     */
    seq::relations_invalidate_cache(scene, active_strip);
  }
  else {
    /* Exit meta-strip if possible. */
    if (BLI_listbase_is_empty(&ed->metastack)) {
      return OPERATOR_CANCELLED;
    }

    /* Display parent meta. */
    Strip *meta_parent = seq::meta_stack_pop(ed);
    seq::select_active_set(scene, meta_parent);
    /* Invalidate the cache of the meta strip when going in and out of it.
     * The cache does not consider if we are inside of a meta strip or not,
     * so we have to make sure that we recache so it is not using outdated data.
     */
    seq::relations_invalidate_cache(scene, meta_parent);
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_meta_toggle(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Toggle Meta Strip";
  ot->idname = "SEQUENCER_OT_meta_toggle";
  ot->description = "Toggle a meta-strip (to edit enclosed strips)";

  /* API callbacks. */
  ot->exec = sequencer_meta_toggle_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Make Meta Strip Operator
 * \{ */

static wmOperatorStatus sequencer_meta_make_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(scene);
  Strip *active_strip = seq::select_active_get(scene);
  ListBase *active_seqbase = seq::active_seqbase_get(ed);

  VectorSet<Strip *> selected = seq::query_selected_strips(active_seqbase);

  if (selected.is_empty()) {
    return OPERATOR_CANCELLED;
  }

  seq::prefetch_stop(scene);

  int channel_max = 1, channel_min = std::numeric_limits<int>::max(), meta_start_frame = MAXFRAME,
      meta_end_frame = std::numeric_limits<int>::min();
  Strip *strip_meta = seq::strip_alloc(active_seqbase, 1, 1, STRIP_TYPE_META);

  /* Remove all selected from main list, and put in meta.
   * Strip is moved within the same edit, no need to re-generate the UID. */
  VectorSet<Strip *> strips_to_move;
  strips_to_move.add_multiple(selected);
  seq::iterator_set_expand(
      scene, active_seqbase, strips_to_move, seq::query_strip_connected_and_effect_chain);

  for (Strip *strip : strips_to_move) {
    seq::relations_invalidate_cache(scene, strip);
    BLI_remlink(active_seqbase, strip);
    BLI_addtail(&strip_meta->seqbase, strip);
    channel_max = max_ii(strip->channel, channel_max);
    channel_min = min_ii(strip->channel, channel_min);
    meta_start_frame = min_ii(seq::time_left_handle_frame_get(scene, strip), meta_start_frame);
    meta_end_frame = max_ii(seq::time_right_handle_frame_get(scene, strip), meta_end_frame);
  }

  ListBase *channels_cur = seq::channels_displayed_get(ed);
  ListBase *channels_meta = &strip_meta->channels;
  for (int i = channel_min; i <= channel_max; i++) {
    SeqTimelineChannel *channel_cur = seq::channel_get_by_index(channels_cur, i);
    SeqTimelineChannel *channel_meta = seq::channel_get_by_index(channels_meta, i);
    STRNCPY_UTF8(channel_meta->name, channel_cur->name);
    channel_meta->flag = channel_cur->flag;
  }

  const int channel = active_strip ? active_strip->channel : channel_max;
  seq::strip_channel_set(strip_meta, channel);
  BLI_strncpy_utf8(strip_meta->name + 2, DATA_("MetaStrip"), sizeof(strip_meta->name) - 2);
  seq::strip_unique_name_set(scene, &ed->seqbase, strip_meta);
  strip_meta->start = meta_start_frame;
  strip_meta->len = meta_end_frame - meta_start_frame;
  seq::select_active_set(scene, strip_meta);
  if (seq::transform_test_overlap(scene, active_seqbase, strip_meta)) {
    seq::transform_seqbase_shuffle(active_seqbase, strip_meta, scene);
  }

  seq::strip_lookup_invalidate(ed);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_meta_make(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Make Meta Strip";
  ot->idname = "SEQUENCER_OT_meta_make";
  ot->description = "Group selected strips into a meta-strip";

  /* API callbacks. */
  ot->exec = sequencer_meta_make_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UnMeta Strip Operator
 * \{ */

static wmOperatorStatus sequencer_meta_separate_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(scene);
  Strip *active_strip = seq::select_active_get(scene);

  if (active_strip == nullptr || active_strip->type != STRIP_TYPE_META) {
    return OPERATOR_CANCELLED;
  }

  seq::prefetch_stop(scene);

  LISTBASE_FOREACH (Strip *, strip, &active_strip->seqbase) {
    seq::relations_invalidate_cache(scene, strip);
  }

  /* Remove all selected from meta, and put in main list.
   * Strip is moved within the same edit, no need to re-generate the UID. */
  BLI_movelisttolist(ed->current_strips(), &active_strip->seqbase);
  BLI_listbase_clear(&active_strip->seqbase);

  ListBase *active_seqbase = seq::active_seqbase_get(ed);
  seq::edit_flag_for_removal(scene, active_seqbase, active_strip);
  seq::edit_remove_flagged_strips(scene, active_seqbase);

  /* Test for effects and overlap. */
  LISTBASE_FOREACH (Strip *, strip, active_seqbase) {
    if (strip->flag & SELECT) {
      strip->runtime.flag &= ~STRIP_OVERLAP;
      if (seq::transform_test_overlap(scene, active_seqbase, strip)) {
        seq::transform_seqbase_shuffle(active_seqbase, strip, scene);
      }
    }
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_meta_separate(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "UnMeta Strip";
  ot->idname = "SEQUENCER_OT_meta_separate";
  ot->description = "Put the contents of a meta-strip back in the sequencer";

  /* API callbacks. */
  ot->exec = sequencer_meta_separate_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Jump to Strip Operator
 * \{ */

static bool strip_jump_internal(Scene *scene,
                                const short side,
                                const bool do_skip_mute,
                                const bool do_center)
{
  bool changed = false;
  int timeline_frame = scene->r.cfra;
  int next_frame = seq::time_find_next_prev_edit(
      scene, timeline_frame, side, do_skip_mute, do_center, false);

  if (next_frame != timeline_frame) {
    scene->r.cfra = next_frame;
    changed = true;
  }

  return changed;
}

static bool sequencer_strip_jump_poll(bContext *C)
{
  /* Prevent changes during render. */
  if (G.is_rendering) {
    return false;
  }

  return sequencer_edit_poll(C);
}

static wmOperatorStatus sequencer_strip_jump_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  const bool next = RNA_boolean_get(op->ptr, "next");
  const bool center = RNA_boolean_get(op->ptr, "center");

  /* Currently do_skip_mute is always true. */
  if (!strip_jump_internal(scene, next ? seq::SIDE_RIGHT : seq::SIDE_LEFT, true, center)) {
    return OPERATOR_CANCELLED;
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_FRAME_CHANGE);
  WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_strip_jump(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Jump to Strip";
  ot->idname = "SEQUENCER_OT_strip_jump";
  ot->description = "Move frame to previous edit point";

  /* API callbacks. */
  ot->exec = sequencer_strip_jump_exec;
  ot->poll = sequencer_strip_jump_poll;

  /* Flags. */
  ot->flag = OPTYPE_UNDO;

  /* Properties. */
  RNA_def_boolean(ot->srna, "next", true, "Next Strip", "");
  RNA_def_boolean(ot->srna, "center", true, "Use Strip Center", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Swap Strip Operator
 * \{ */

static const EnumPropertyItem prop_side_lr_types[] = {
    {seq::SIDE_LEFT, "LEFT", 0, "Left", ""},
    {seq::SIDE_RIGHT, "RIGHT", 0, "Right", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static void swap_strips(Scene *scene, Strip *strip_a, Strip *strip_b)
{
  int gap = seq::time_left_handle_frame_get(scene, strip_b) -
            seq::time_right_handle_frame_get(scene, strip_a);
  int strip_a_start;
  int strip_b_start;

  strip_b_start = (strip_b->start - seq::time_left_handle_frame_get(scene, strip_b)) +
                  seq::time_left_handle_frame_get(scene, strip_a);
  seq::transform_translate_strip(scene, strip_b, strip_b_start - strip_b->start);
  seq::relations_invalidate_cache(scene, strip_b);

  strip_a_start = (strip_a->start - seq::time_left_handle_frame_get(scene, strip_a)) +
                  seq::time_right_handle_frame_get(scene, strip_b) + gap;
  seq::transform_translate_strip(scene, strip_a, strip_a_start - strip_a->start);
  seq::relations_invalidate_cache(scene, strip_a);
}

static Strip *find_next_prev_strip(Scene *scene, Strip *test, int lr, int sel)
{
  /* sel: 0==unselected, 1==selected, -1==don't care. */
  Strip *strip, *best_strip = nullptr;
  Editing *ed = seq::editing_get(scene);

  int dist, best_dist;
  best_dist = MAXFRAME * 2;

  if (ed == nullptr) {
    return nullptr;
  }

  strip = static_cast<Strip *>(ed->current_strips()->first);
  while (strip) {
    if ((strip != test) && (test->channel == strip->channel) &&
        ((sel == -1) || (sel == (strip->flag & SELECT))))
    {
      dist = MAXFRAME * 2;

      switch (lr) {
        case seq::SIDE_LEFT:
          if (seq::time_right_handle_frame_get(scene, strip) <=
              seq::time_left_handle_frame_get(scene, test))
          {
            dist = seq::time_right_handle_frame_get(scene, test) -
                   seq::time_left_handle_frame_get(scene, strip);
          }
          break;
        case seq::SIDE_RIGHT:
          if (seq::time_left_handle_frame_get(scene, strip) >=
              seq::time_right_handle_frame_get(scene, test))
          {
            dist = seq::time_left_handle_frame_get(scene, strip) -
                   seq::time_right_handle_frame_get(scene, test);
          }
          break;
      }

      if (dist == 0) {
        best_strip = strip;
        break;
      }
      if (dist < best_dist) {
        best_dist = dist;
        best_strip = strip;
      }
    }
    strip = static_cast<Strip *>(strip->next);
  }
  return best_strip; /* Can be nullptr. */
}

static bool strip_is_parent(const Strip *par, const Strip *strip)
{
  return ((par->input1 == strip) || (par->input2 == strip));
}

static wmOperatorStatus sequencer_swap_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(scene);
  Strip *active_strip = seq::select_active_get(scene);
  ListBase *seqbase = seq::active_seqbase_get(ed);
  Strip *strip;
  int side = RNA_enum_get(op->ptr, "side");

  if (active_strip == nullptr) {
    return OPERATOR_CANCELLED;
  }

  strip = find_next_prev_strip(scene, active_strip, side, -1);

  if (strip) {

    /* Disallow effect strips. */
    if (seq::effect_get_num_inputs(strip->type) >= 1 &&
        (strip->effectdata || strip->input1 || strip->input2))
    {
      return OPERATOR_CANCELLED;
    }
    if ((seq::effect_get_num_inputs(active_strip->type) >= 1) &&
        (active_strip->effectdata || active_strip->input1 || active_strip->input2))
    {
      return OPERATOR_CANCELLED;
    }

    ListBase *channels = seq::channels_displayed_get(seq::editing_get(scene));
    if (seq::transform_is_locked(channels, strip) ||
        seq::transform_is_locked(channels, active_strip))
    {
      return OPERATOR_CANCELLED;
    }

    switch (side) {
      case seq::SIDE_LEFT:
        swap_strips(scene, strip, active_strip);
        break;
      case seq::SIDE_RIGHT:
        swap_strips(scene, active_strip, strip);
        break;
    }

    /* Do this in a new loop since both effects need to be calculated first. */
    LISTBASE_FOREACH (Strip *, istrip, seqbase) {
      if (istrip->is_effect() &&
          (strip_is_parent(istrip, active_strip) || strip_is_parent(istrip, strip)))
      {
        /* This may now overlap. */
        if (seq::transform_test_overlap(scene, seqbase, istrip)) {
          seq::transform_seqbase_shuffle(seqbase, istrip, scene);
        }
      }
    }

    WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

void SEQUENCER_OT_swap(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Swap Strip";
  ot->idname = "SEQUENCER_OT_swap";
  ot->description = "Swap active strip with strip to the right or left";

  /* API callbacks. */
  ot->exec = sequencer_swap_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties. */
  RNA_def_enum(
      ot->srna, "side", prop_side_lr_types, seq::SIDE_RIGHT, "Side", "Side of the strip to swap");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Render Size Operator
 * \{ */

static wmOperatorStatus sequencer_rendersize_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Strip *active_strip = seq::select_active_get(scene);
  StripElem *se = nullptr;

  if (active_strip == nullptr || active_strip->data == nullptr) {
    return OPERATOR_CANCELLED;
  }

  switch (active_strip->type) {
    case STRIP_TYPE_IMAGE:
      se = seq::render_give_stripelem(scene, active_strip, scene->r.cfra);
      break;
    case STRIP_TYPE_MOVIE:
      se = active_strip->data->stripdata;
      break;
    default:
      return OPERATOR_CANCELLED;
  }

  if (se == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* Prevent setting the render size if values aren't initialized. */
  if (se->orig_width <= 0 || se->orig_height <= 0) {
    return OPERATOR_CANCELLED;
  }

  scene->r.xsch = se->orig_width;
  scene->r.ysch = se->orig_height;

  active_strip->data->transform->scale_x = active_strip->data->transform->scale_y = 1.0f;
  active_strip->data->transform->xofs = active_strip->data->transform->yofs = 0.0f;

  seq::relations_invalidate_cache(scene, active_strip);
  WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_rendersize(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Set Render Size";
  ot->idname = "SEQUENCER_OT_rendersize";
  ot->description = "Set render size and aspect from active strip";

  /* API callbacks. */
  ot->exec = sequencer_rendersize_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Copy Operator
 * \{ */

void SEQUENCER_OT_copy(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Copy";
  ot->idname = "SEQUENCER_OT_copy";
  ot->description = "Copy the selected strips to the internal clipboard";

  /* API callbacks. */
  ot->exec = sequencer_clipboard_copy_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Paste Operator
 * \{ */

void SEQUENCER_OT_paste(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Paste";
  ot->idname = "SEQUENCER_OT_paste";
  ot->description = "Paste strips from the internal clipboard";

  /* API callbacks. */
  ot->invoke = sequencer_clipboard_paste_invoke;
  ot->exec = sequencer_clipboard_paste_exec;
  ot->poll = ED_operator_sequencer_active;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties. */
  PropertyRNA *prop = RNA_def_boolean(
      ot->srna,
      "keep_offset",
      false,
      "Keep Offset",
      "Keep strip offset relative to the current frame when pasting");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_int(ot->srna, "x", 0, INT_MIN, INT_MAX, "X", "", INT_MIN, INT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_int(ot->srna, "y", 0, INT_MIN, INT_MAX, "Y", "", INT_MIN, INT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sequencer Swap Data Operator
 * \{ */

static wmOperatorStatus sequencer_swap_data_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Strip *strip_act;
  Strip *strip_other;
  const char *error_msg;

  if (seq::select_active_get_pair(scene, &strip_act, &strip_other) == false) {
    BKE_report(op->reports, RPT_ERROR, "Please select two strips");
    return OPERATOR_CANCELLED;
  }

  if (seq::edit_strip_swap(scene, strip_act, strip_other, &error_msg) == false) {
    BKE_report(op->reports, RPT_ERROR, error_msg);
    return OPERATOR_CANCELLED;
  }

  if (strip_act->scene_sound) {
    BKE_sound_remove_scene_sound(scene, strip_act->scene_sound);
  }

  if (strip_other->scene_sound) {
    BKE_sound_remove_scene_sound(scene, strip_other->scene_sound);
  }

  strip_act->scene_sound = nullptr;
  strip_other->scene_sound = nullptr;

  if (strip_act->sound) {
    BKE_sound_add_scene_sound_defaults(scene, strip_act);
  }
  if (strip_other->sound) {
    BKE_sound_add_scene_sound_defaults(scene, strip_other);
  }

  seq::relations_invalidate_cache_raw(scene, strip_act);
  seq::relations_invalidate_cache_raw(scene, strip_other);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_swap_data(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Sequencer Swap Data";
  ot->idname = "SEQUENCER_OT_swap_data";
  ot->description = "Swap 2 sequencer strips";

  /* API callbacks. */
  ot->exec = sequencer_swap_data_exec;
  ot->poll = ED_operator_sequencer_active;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Change Effect Type Operator
 * \{ */

const EnumPropertyItem sequencer_prop_effect_types[] = {
    {STRIP_TYPE_CROSS, "CROSS", 0, "Crossfade", "Fade out of one video, fading into another"},
    {STRIP_TYPE_ADD, "ADD", 0, "Add", "Add together color channels from two videos"},
    {STRIP_TYPE_SUB, "SUBTRACT", 0, "Subtract", "Subtract one strip's color from another"},
    {STRIP_TYPE_ALPHAOVER, "ALPHA_OVER", 0, "Alpha Over", "Blend alpha on top of another video"},
    {STRIP_TYPE_ALPHAUNDER, "ALPHA_UNDER", 0, "Alpha Under", "Blend alpha below another video"},
    {STRIP_TYPE_GAMCROSS, "GAMMA_CROSS", 0, "Gamma Crossfade", "Crossfade with color correction"},
    {STRIP_TYPE_MUL, "MULTIPLY", 0, "Multiply", "Multiply color channels from two videos"},
    {STRIP_TYPE_WIPE, "WIPE", 0, "Wipe", "Sweep a transition line across the frame"},
    {STRIP_TYPE_GLOW, "GLOW", 0, "Glow", "Add blur and brightness to light areas"},
    {STRIP_TYPE_COLOR, "COLOR", 0, "Color", "Add a simple color strip"},
    {STRIP_TYPE_SPEED, "SPEED", 0, "Speed", "Timewarp video strips, modifying playback speed"},
    {STRIP_TYPE_MULTICAM, "MULTICAM", 0, "Multicam Selector", "Control active camera angles"},
    {STRIP_TYPE_ADJUSTMENT, "ADJUSTMENT", 0, "Adjustment Layer", "Apply nondestructive effects"},
    {STRIP_TYPE_GAUSSIAN_BLUR, "GAUSSIAN_BLUR", 0, "Gaussian Blur", "Soften details along axes"},
    {STRIP_TYPE_TEXT, "TEXT", 0, "Text", "Add a simple text strip"},
    {STRIP_TYPE_COLORMIX, "COLORMIX", 0, "Color Mix", "Combine two strips using blend modes"},
    {0, nullptr, 0, nullptr, nullptr},
};

static wmOperatorStatus sequencer_change_effect_type_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Strip *strip = seq::select_active_get(scene);
  const int old_type = strip->type;
  const int new_type = RNA_enum_get(op->ptr, "type");

  if (!strip->is_effect()) {
    return OPERATOR_CANCELLED;
  }

  if (seq::effect_get_num_inputs(strip->type) != seq::effect_get_num_inputs(new_type)) {
    BKE_report(op->reports, RPT_ERROR, "New effect takes less or more inputs");
    return OPERATOR_CANCELLED;
  }

  /* Free previous effect. */
  seq::effect_free(strip);

  strip->type = new_type;

  /* If the strip's name is the default (equal to the old effect type),
   * rename to the new type to avoid confusion. */
  char name_base[MAX_ID_NAME];
  int name_num;
  BLI_string_split_name_number(strip->name + 2, '.', name_base, &name_num);
  if (STREQ(name_base, seq::get_default_stripname_by_type(old_type))) {
    seq::edit_strip_name_set(scene, strip, seq::strip_give_name(strip));
    seq::ensure_unique_name(strip, scene);
  }

  /* Init new effect. */
  seq::effect_ensure_initialized(strip);

  seq::relations_invalidate_cache(scene, strip);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_change_effect_type(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Change Effect Type";
  ot->idname = "SEQUENCER_OT_change_effect_type";
  ot->description = "Replace effect strip with another that takes the same number of inputs";

  /* API callbacks. */
  ot->exec = sequencer_change_effect_type_exec;
  ot->poll = sequencer_effect_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(ot->srna,
                          "type",
                          sequencer_prop_effect_types,
                          STRIP_TYPE_CROSS,
                          "Type",
                          "Strip effect type");
  RNA_def_property_translation_context(ot->prop, BLT_I18NCONTEXT_ID_SEQUENCE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Change Data/Files Operator
 * \{ */

static wmOperatorStatus sequencer_change_path_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_sequencer_scene(C);
  Strip *strip = seq::select_active_get(scene);
  const bool is_relative_path = RNA_boolean_get(op->ptr, "relative_path");
  const bool use_placeholders = RNA_boolean_get(op->ptr, "use_placeholders");
  int minext_frameme, numdigits;

  if (strip->type == STRIP_TYPE_IMAGE) {
    char directory[FILE_MAX];
    int len;
    StripElem *se;

    /* Need to find min/max frame for placeholders. */
    if (use_placeholders) {
      len = sequencer_image_strip_get_minmax_frame(op, strip->sfra, &minext_frameme, &numdigits);
    }
    else {
      len = RNA_property_collection_length(op->ptr, RNA_struct_find_property(op->ptr, "files"));
    }
    if (len == 0) {
      return OPERATOR_CANCELLED;
    }

    RNA_string_get(op->ptr, "directory", directory);
    if (is_relative_path) {
      /* TODO(@ideasman42): shouldn't this already be relative from the filesel?
       * (as the 'filepath' is) for now just make relative here,
       * but look into changing after 2.60. */
      BLI_path_rel(directory, BKE_main_blendfile_path(bmain));
    }
    STRNCPY(strip->data->dirpath, directory);

    if (strip->data->stripdata) {
      MEM_freeN(strip->data->stripdata);
    }
    strip->data->stripdata = se = MEM_calloc_arrayN<StripElem>(len, "stripelem");

    if (use_placeholders) {
      sequencer_image_strip_reserve_frames(op, se, len, minext_frameme, numdigits);
    }
    else {
      RNA_BEGIN (op->ptr, itemptr, "files") {
        std::string filename = RNA_string_get(&itemptr, "name");
        STRNCPY(se->filename, filename.c_str());
        se++;
      }
      RNA_END;
    }

    if (len == 1) {
      strip->flag |= SEQ_SINGLE_FRAME_CONTENT;
    }
    else {
      strip->flag &= ~SEQ_SINGLE_FRAME_CONTENT;
    }

    /* Reset these else we won't see all the images. */
    strip->anim_startofs = strip->anim_endofs = 0;

    /* Correct start/end frames so we don't move.
     * Important not to set strip->len = len; allow the function to handle it. */
    seq::add_reload_new_file(bmain, scene, strip, true);
  }
  else if (strip->type == STRIP_TYPE_SOUND_RAM) {
    bSound *sound = strip->sound;
    if (sound == nullptr) {
      return OPERATOR_CANCELLED;
    }
    char filepath[FILE_MAX];
    RNA_string_get(op->ptr, "filepath", filepath);
    STRNCPY(sound->filepath, filepath);
    BKE_sound_load(bmain, sound);
  }
  else {
    /* Lame, set rna filepath. */
    PropertyRNA *prop;
    char filepath[FILE_MAX];

    PointerRNA strip_ptr = RNA_pointer_create_discrete(&scene->id, &RNA_Strip, strip);

    RNA_string_get(op->ptr, "filepath", filepath);
    prop = RNA_struct_find_property(&strip_ptr, "filepath");
    RNA_property_string_set(&strip_ptr, prop, filepath);
    RNA_property_update(C, &strip_ptr, prop);
    seq::relations_strip_free_anim(strip);
  }

  seq::relations_invalidate_cache_raw(scene, strip);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus sequencer_change_path_invoke(bContext *C,
                                                     wmOperator *op,
                                                     const wmEvent * /*event*/)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Strip *strip = seq::select_active_get(scene);
  char filepath[FILE_MAX];

  BLI_path_join(
      filepath, sizeof(filepath), strip->data->dirpath, strip->data->stripdata->filename);

  RNA_string_set(op->ptr, "directory", strip->data->dirpath);
  RNA_string_set(op->ptr, "filepath", filepath);

  /* Set default display depending on strip type. */
  if (strip->type == STRIP_TYPE_IMAGE) {
    RNA_boolean_set(op->ptr, "filter_movie", false);
  }
  else {
    RNA_boolean_set(op->ptr, "filter_image", false);
  }

  WM_event_add_fileselect(C, op);

  return OPERATOR_RUNNING_MODAL;
}

void SEQUENCER_OT_change_path(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Change Data/Files";
  ot->idname = "SEQUENCER_OT_change_path";

  /* API callbacks. */
  ot->exec = sequencer_change_path_exec;
  ot->invoke = sequencer_change_path_invoke;
  ot->poll = sequencer_strip_has_path_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER,
                                 FILE_SPECIAL,
                                 FILE_OPENFILE,
                                 WM_FILESEL_DIRECTORY | WM_FILESEL_RELPATH | WM_FILESEL_FILEPATH |
                                     WM_FILESEL_FILES,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);
  RNA_def_boolean(ot->srna,
                  "use_placeholders",
                  false,
                  "Use Placeholders",
                  "Use placeholders for missing frames of the strip");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Change Strip Scene Operator
 * \{ */

static bool sequencer_strip_change_scene_poll(bContext *C)
{
  Editing *ed = seq::editing_get(CTX_data_sequencer_scene(C));
  if (ed == nullptr) {
    return false;
  }
  Strip *strip = ed->act_strip;
  return ((strip != nullptr) && (strip->type == STRIP_TYPE_SCENE));
}
static wmOperatorStatus sequencer_change_scene_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_sequencer_scene(C);
  Scene *scene_seq = static_cast<Scene *>(
      BLI_findlink(&bmain->scenes, RNA_enum_get(op->ptr, "scene")));

  if (scene_seq == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "Scene not found");
    return OPERATOR_CANCELLED;
  }

  /* Assign new scene. */
  Strip *strip = seq::select_active_get(scene);
  if (strip) {
    strip->scene = scene_seq;
    /* Do a refresh of the sequencer data. */
    seq::relations_invalidate_cache_raw(scene, strip);
    DEG_id_tag_update(&scene->id, ID_RECALC_AUDIO | ID_RECALC_SEQUENCER_STRIPS);
    DEG_relations_tag_update(bmain);
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SCENEBROWSE, scene);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus sequencer_change_scene_invoke(bContext *C,
                                                      wmOperator *op,
                                                      const wmEvent *event)
{
  if (!RNA_struct_property_is_set(op->ptr, "scene")) {
    return WM_enum_search_invoke(C, op, event);
  }

  return sequencer_change_scene_exec(C, op);
}

void SEQUENCER_OT_change_scene(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers. */
  ot->name = "Change Scene";
  ot->idname = "SEQUENCER_OT_change_scene";
  ot->description = "Change Scene assigned to Strip";

  /* API callbacks. */
  ot->exec = sequencer_change_scene_exec;
  ot->invoke = sequencer_change_scene_invoke;
  ot->poll = sequencer_strip_change_scene_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties. */
  prop = RNA_def_enum(ot->srna, "scene", rna_enum_dummy_NULL_items, 0, "Scene", "");
  RNA_def_enum_funcs(prop, RNA_scene_without_sequencer_scene_itemf);
  RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
  ot->prop = prop;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Export Subtitles Operator
 * \{ */

/** Comparison function suitable to be used with BLI_listbase_sort(). */
static int strip_cmp_time_startdisp_channel(void *thunk, const void *a, const void *b)
{
  const Scene *scene = static_cast<Scene *>(thunk);
  const Strip *strip_a = static_cast<const Strip *>(a);
  const Strip *strip_b = static_cast<const Strip *>(b);

  int strip_a_start = seq::time_left_handle_frame_get(scene, strip_a);
  int strip_b_start = seq::time_left_handle_frame_get(scene, strip_b);

  /* If strips have the same start frame favor the one with a higher channel. */
  if (strip_a_start == strip_b_start) {
    return strip_a->channel > strip_b->channel;
  }

  return (strip_a_start > strip_b_start);
}

static wmOperatorStatus sequencer_export_subtitles_invoke(bContext *C,
                                                          wmOperator *op,
                                                          const wmEvent * /*event*/)
{
  ED_fileselect_ensure_default_filepath(C, op, ".srt");

  WM_event_add_fileselect(C, op);

  return OPERATOR_RUNNING_MODAL;
}

struct Seq_get_text_cb_data {
  ListBase *text_seq;
  Scene *scene;
};

static bool strip_get_text_strip_cb(Strip *strip, void *user_data)
{
  Seq_get_text_cb_data *cd = (Seq_get_text_cb_data *)user_data;
  Editing *ed = seq::editing_get(cd->scene);
  ListBase *channels = seq::channels_displayed_get(ed);
  /* Only text strips that are not muted and don't end with negative frame. */
  if ((strip->type == STRIP_TYPE_TEXT) && !seq::render_is_muted(channels, strip) &&
      (seq::time_right_handle_frame_get(cd->scene, strip) > cd->scene->r.sfra))
  {
    BLI_addtail(cd->text_seq, MEM_dupallocN(strip));
  }
  return true;
}

static wmOperatorStatus sequencer_export_subtitles_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Strip *strip, *strip_next;
  Editing *ed = seq::editing_get(scene);
  ListBase text_seq = {nullptr};
  int iter = 1; /* Sequence numbers in `.srt` files are 1-indexed. */
  FILE *file;
  char filepath[FILE_MAX];

  if (!RNA_struct_property_is_set_ex(op->ptr, "filepath", false)) {
    BKE_report(op->reports, RPT_ERROR, "No filepath given");
    return OPERATOR_CANCELLED;
  }

  RNA_string_get(op->ptr, "filepath", filepath);
  BLI_path_extension_ensure(filepath, sizeof(filepath), ".srt");

  /* Avoid File write exceptions. */
  if (!BLI_exists(filepath)) {
    BLI_file_ensure_parent_dir_exists(filepath);
    if (!BLI_file_touch(filepath)) {
      BKE_report(op->reports, RPT_ERROR, "Cannot create subtitle file");
      return OPERATOR_CANCELLED;
    }
  }
  else if (!BLI_file_is_writable(filepath)) {
    BKE_report(op->reports, RPT_ERROR, "Cannot overwrite export file");
    return OPERATOR_CANCELLED;
  }

  if (ed != nullptr) {
    Seq_get_text_cb_data cb_data = {&text_seq, scene};
    seq::foreach_strip(&ed->seqbase, strip_get_text_strip_cb, &cb_data);
  }

  if (BLI_listbase_is_empty(&text_seq)) {
    BKE_report(op->reports, RPT_ERROR, "No subtitles (text strips) to export");
    return OPERATOR_CANCELLED;
  }

  BLI_listbase_sort_r(&text_seq, strip_cmp_time_startdisp_channel, scene);

  /* Open and write file. */
  file = BLI_fopen(filepath, "w");

  for (strip = static_cast<Strip *>(text_seq.first); strip; strip = strip_next) {
    TextVars *data = static_cast<TextVars *>(strip->effectdata);
    char timecode_str_start[32];
    char timecode_str_end[32];

    /* Write time-code relative to start frame of scene. Don't allow negative time-codes. */
    BLI_timecode_string_from_time(
        timecode_str_start,
        sizeof(timecode_str_start),
        -2,
        FRA2TIME(max_ii(seq::time_left_handle_frame_get(scene, strip) - scene->r.sfra, 0)),
        scene->frames_per_second(),
        USER_TIMECODE_SUBRIP);
    BLI_timecode_string_from_time(
        timecode_str_end,
        sizeof(timecode_str_end),
        -2,
        FRA2TIME(seq::time_right_handle_frame_get(scene, strip) - scene->r.sfra),
        scene->frames_per_second(),
        USER_TIMECODE_SUBRIP);

    fprintf(file,
            "%d\n%s --> %s\n%s\n\n",
            iter++,
            timecode_str_start,
            timecode_str_end,
            data->text_ptr);

    strip_next = static_cast<Strip *>(strip->next);
    MEM_freeN(strip);
  }

  fclose(file);

  return OPERATOR_FINISHED;
}

static bool sequencer_strip_is_text_poll(bContext *C)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  if (!scene) {
    return false;
  }
  Editing *ed = seq::editing_get(scene);
  if (!ed) {
    return false;
  }
  Strip *strip = ed->act_strip;
  if (!strip) {
    return false;
  }
  return strip->type == STRIP_TYPE_TEXT;
}

void SEQUENCER_OT_export_subtitles(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Export Subtitles";
  ot->idname = "SEQUENCER_OT_export_subtitles";
  ot->description = "Export .srt file containing text strips";

  /* API callbacks. */
  ot->exec = sequencer_export_subtitles_exec;
  ot->invoke = sequencer_export_subtitles_invoke;
  ot->poll = sequencer_strip_is_text_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER,
                                 FILE_BLENDER,
                                 FILE_SAVE,
                                 WM_FILESEL_FILEPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Range to Strips Operator
 * \{ */

static wmOperatorStatus sequencer_set_range_to_strips_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(scene);

  int sfra = MAXFRAME;
  int efra = -MAXFRAME;
  bool selected = false;
  const bool preview = RNA_boolean_get(op->ptr, "preview");

  LISTBASE_FOREACH (Strip *, strip, ed->current_strips()) {
    if (strip->flag & SELECT) {
      selected = true;
      sfra = min_ii(sfra, seq::time_left_handle_frame_get(scene, strip));
      /* Offset of -1 is needed because in the sequencer every frame has width.
       * Range from 1 to 1 is drawn as range 1 to 2, because 1 frame long strip starts at frame 1
       * and ends at frame 2. See #106480. */
      efra = max_ii(efra, seq::time_right_handle_frame_get(scene, strip) - 1);
    }
  }

  if (!selected) {
    BKE_report(op->reports, RPT_WARNING, "Select one or more strips");
    return OPERATOR_CANCELLED;
  }
  if (efra < 0) {
    BKE_report(op->reports, RPT_ERROR, "Cannot set a negative range");
    return OPERATOR_CANCELLED;
  }

  if (preview) {
    scene->r.flag |= SCER_PRV_RANGE;
    scene->r.psfra = max_ii(0, sfra);
    scene->r.pefra = efra;
  }
  else {
    scene->r.flag &= ~SCER_PRV_RANGE;
    scene->r.sfra = max_ii(0, sfra);
    scene->r.efra = efra;
  }

  WM_event_add_notifier(C, NC_SCENE | ND_FRAME_RANGE, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_set_range_to_strips(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers. */
  ot->name = "Set Range to Strips";
  ot->idname = "SEQUENCER_OT_set_range_to_strips";
  ot->description = "Set the frame range to the selected strips start and end";

  /* API callbacks. */
  ot->exec = sequencer_set_range_to_strips_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  prop = RNA_def_boolean(ot->srna, "preview", false, "Preview", "Set the preview range instead");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clear Strip Transform Operator
 * \{ */

enum {
  STRIP_TRANSFORM_POSITION,
  STRIP_TRANSFORM_SCALE,
  STRIP_TRANSFORM_ROTATION,
  STRIP_TRANSFORM_ALL,
};

static const EnumPropertyItem transform_reset_properties[] = {
    {STRIP_TRANSFORM_POSITION, "POSITION", 0, "Position", "Reset strip transform location"},
    {STRIP_TRANSFORM_SCALE, "SCALE", 0, "Scale", "Reset strip transform scale"},
    {STRIP_TRANSFORM_ROTATION, "ROTATION", 0, "Rotation", "Reset strip transform rotation"},
    {STRIP_TRANSFORM_ALL, "ALL", 0, "All", "Reset strip transform location, scale and rotation"},
    {0, nullptr, 0, nullptr, nullptr},
};

static wmOperatorStatus sequencer_strip_transform_clear_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  const Editing *ed = seq::editing_get(scene);
  const int property = RNA_enum_get(op->ptr, "property");

  const bool use_autokeyframe = animrig::is_autokey_on(scene);
  const bool only_when_keyed = animrig::is_keying_flag(scene, AUTOKEY_FLAG_INSERTAVAILABLE);

  LISTBASE_FOREACH (Strip *, strip, ed->current_strips()) {
    if (strip->flag & SELECT && strip->type != STRIP_TYPE_SOUND_RAM) {
      StripTransform *transform = strip->data->transform;
      PropertyRNA *prop;
      PointerRNA ptr = RNA_pointer_create_discrete(&scene->id, &RNA_StripTransform, transform);
      switch (property) {
        case STRIP_TRANSFORM_POSITION:
          transform->xofs = 0;
          transform->yofs = 0;
          if (use_autokeyframe) {
            prop = RNA_struct_find_property(&ptr, "offset_x");
            animrig::autokeyframe_property(
                C, scene, &ptr, prop, -1, scene->r.cfra, only_when_keyed);
            prop = RNA_struct_find_property(&ptr, "offset_y");
            animrig::autokeyframe_property(
                C, scene, &ptr, prop, -1, scene->r.cfra, only_when_keyed);
          }
          break;
        case STRIP_TRANSFORM_SCALE:
          transform->scale_x = 1.0f;
          transform->scale_y = 1.0f;
          if (use_autokeyframe) {
            prop = RNA_struct_find_property(&ptr, "scale_x");
            animrig::autokeyframe_property(
                C, scene, &ptr, prop, -1, scene->r.cfra, only_when_keyed);
            prop = RNA_struct_find_property(&ptr, "scale_y");
            animrig::autokeyframe_property(
                C, scene, &ptr, prop, -1, scene->r.cfra, only_when_keyed);
          }
          break;
        case STRIP_TRANSFORM_ROTATION:
          transform->rotation = 0.0f;
          if (use_autokeyframe) {
            prop = RNA_struct_find_property(&ptr, "rotation");
            animrig::autokeyframe_property(
                C, scene, &ptr, prop, -1, scene->r.cfra, only_when_keyed);
          }
          break;
        case STRIP_TRANSFORM_ALL:
          transform->xofs = 0;
          transform->yofs = 0;
          transform->scale_x = 1.0f;
          transform->scale_y = 1.0f;
          transform->rotation = 0.0f;
          if (use_autokeyframe) {
            prop = RNA_struct_find_property(&ptr, "offset_x");
            animrig::autokeyframe_property(
                C, scene, &ptr, prop, -1, scene->r.cfra, only_when_keyed);
            prop = RNA_struct_find_property(&ptr, "offset_y");
            animrig::autokeyframe_property(
                C, scene, &ptr, prop, -1, scene->r.cfra, only_when_keyed);
            prop = RNA_struct_find_property(&ptr, "scale_x");
            animrig::autokeyframe_property(
                C, scene, &ptr, prop, -1, scene->r.cfra, only_when_keyed);
            prop = RNA_struct_find_property(&ptr, "scale_y");
            animrig::autokeyframe_property(
                C, scene, &ptr, prop, -1, scene->r.cfra, only_when_keyed);
            prop = RNA_struct_find_property(&ptr, "rotation");
            animrig::autokeyframe_property(
                C, scene, &ptr, prop, -1, scene->r.cfra, only_when_keyed);
          }
          break;
      }
      seq::relations_invalidate_cache(scene, strip);
    }
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_strip_transform_clear(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Clear Strip Transform";
  ot->idname = "SEQUENCER_OT_strip_transform_clear";
  ot->description = "Reset image transformation to default value";

  /* API callbacks. */
  ot->exec = sequencer_strip_transform_clear_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(ot->srna,
                          "property",
                          transform_reset_properties,
                          STRIP_TRANSFORM_ALL,
                          "Property",
                          "Strip transform property to be reset");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform Set Fit Operator
 * \{ */

static wmOperatorStatus sequencer_strip_transform_fit_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  const Editing *ed = seq::editing_get(scene);
  const eSeqImageFitMethod fit_method = eSeqImageFitMethod(RNA_enum_get(op->ptr, "fit_method"));

  LISTBASE_FOREACH (Strip *, strip, ed->current_strips()) {
    if (strip->flag & SELECT && strip->type != STRIP_TYPE_SOUND_RAM) {
      const int timeline_frame = scene->r.cfra;
      StripElem *strip_elem = seq::render_give_stripelem(scene, strip, timeline_frame);

      if (strip_elem == nullptr) {
        continue;
      }

      seq::set_scale_to_fit(strip,
                            strip_elem->orig_width,
                            strip_elem->orig_height,
                            scene->r.xsch,
                            scene->r.ysch,
                            fit_method);
      seq::relations_invalidate_cache(scene, strip);
    }
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_strip_transform_fit(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Strip Transform Set Fit";
  ot->idname = "SEQUENCER_OT_strip_transform_fit";

  /* API callbacks. */
  ot->exec = sequencer_strip_transform_fit_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(ot->srna,
                          "fit_method",
                          rna_enum_strip_scale_method_items,
                          SEQ_SCALE_TO_FIT,
                          "Fit Method",
                          "Mode for fitting the image to the canvas");
}

static wmOperatorStatus sequencer_strip_color_tag_set_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  const Editing *ed = seq::editing_get(scene);
  const short color_tag = RNA_enum_get(op->ptr, "color");

  LISTBASE_FOREACH (Strip *, strip, ed->current_strips()) {
    if (strip->flag & SELECT) {
      strip->color_tag = color_tag;
    }
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
  return OPERATOR_FINISHED;
}

static bool sequencer_strip_color_tag_set_poll(bContext *C)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  if (scene == nullptr) {
    return false;
  }

  Editing *ed = seq::editing_get(scene);
  if (ed == nullptr) {
    return false;
  }

  Strip *act_strip = ed->act_strip;
  return act_strip != nullptr;
}

void SEQUENCER_OT_strip_color_tag_set(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Set Color Tag";
  ot->idname = "SEQUENCER_OT_strip_color_tag_set";
  ot->description = "Set a color tag for the selected strips";

  /* API callbacks. */
  ot->exec = sequencer_strip_color_tag_set_exec;
  ot->poll = sequencer_strip_color_tag_set_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(ot->srna, "color", rna_enum_strip_color_items, STRIP_COLOR_NONE, "Color Tag", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set 2D Cursor Operator
 * \{ */

static wmOperatorStatus sequencer_set_2d_cursor_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  SpaceSeq *sseq = CTX_wm_space_seq(C);

  float cursor_pixel[2];
  RNA_float_get_array(op->ptr, "location", cursor_pixel);

  float2 cursor_region = seq::image_preview_unit_from_px(scene, cursor_pixel);
  copy_v2_v2(sseq->cursor, cursor_region);

  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  /* Use pass-through to allow click-drag to transform the cursor. */
  return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
}

static wmOperatorStatus sequencer_set_2d_cursor_invoke(bContext *C,
                                                       wmOperator *op,
                                                       const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  float cursor_pixel[2];
  UI_view2d_region_to_view(
      &region->v2d, event->mval[0], event->mval[1], &cursor_pixel[0], &cursor_pixel[1]);

  RNA_float_set_array(op->ptr, "location", cursor_pixel);

  return sequencer_set_2d_cursor_exec(C, op);
}

void SEQUENCER_OT_cursor_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set 2D Cursor";
  ot->description = "Set 2D cursor location";
  ot->idname = "SEQUENCER_OT_cursor_set";

  /* API callbacks. */
  ot->exec = sequencer_set_2d_cursor_exec;
  ot->invoke = sequencer_set_2d_cursor_invoke;
  ot->poll = sequencer_view_has_preview_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_float_vector(ot->srna,
                       "location",
                       2,
                       nullptr,
                       -FLT_MAX,
                       FLT_MAX,
                       "Location",
                       "Cursor location in normalized preview coordinates",
                       -10.0f,
                       10.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Update scene strip frame range
 * \{ */

static wmOperatorStatus sequencer_scene_frame_range_update_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(scene);
  Strip *strip = ed->act_strip;

  const int old_start = seq::time_left_handle_frame_get(scene, strip);
  const int old_end = seq::time_right_handle_frame_get(scene, strip);

  Scene *target_scene = strip->scene;

  strip->len = target_scene->r.efra - target_scene->r.sfra + 1;
  seq::time_handles_frame_set(scene, strip, old_start, old_end);

  seq::relations_invalidate_cache_raw(scene, strip);
  DEG_id_tag_update(&scene->id, ID_RECALC_AUDIO | ID_RECALC_SEQUENCER_STRIPS);
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);
  return OPERATOR_FINISHED;
}

static bool sequencer_scene_frame_range_update_poll(bContext *C)
{
  Editing *ed = seq::editing_get(CTX_data_sequencer_scene(C));
  return (ed != nullptr && ed->act_strip != nullptr && ed->act_strip->type == STRIP_TYPE_SCENE);
}

void SEQUENCER_OT_scene_frame_range_update(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Update Scene Frame Range";
  ot->description = "Update frame range of scene strip";
  ot->idname = "SEQUENCER_OT_scene_frame_range_update";

  /* API callbacks. */
  ot->exec = sequencer_scene_frame_range_update_exec;
  ot->poll = sequencer_scene_frame_range_update_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

}  // namespace blender::ed::vse
