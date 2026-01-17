/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 */

#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_math_base.h"
#include "BLI_set.hh"

#include "DNA_scene_types.h"

#include "BKE_context.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "ED_select_utils.hh"
#include "ED_sequencer.hh"

#include "SEQ_connect.hh"
#include "SEQ_iterator.hh"
#include "SEQ_relations.hh"
#include "SEQ_retiming.hh"
#include "SEQ_select.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_transform.hh"

#include "WM_api.hh"

#include "RNA_define.hh"

#include "UI_view2d.hh"

#include "sequencer_intern.hh"

namespace blender::ed::vse {

/*-------------------------------------------------------------------- */
/** \name Retiming Generic Functions
 * \{ */

bool sequencer_retiming_mode_is_active(const Scene *scene)
{
  if (!scene) {
    return false;
  }
  Editing *ed = seq::editing_get(scene);
  if (!ed) {
    return false;
  }

  const Map retiming_sel = seq::retiming_selection_get(ed);
  if (retiming_sel.is_empty()) {
    return false;
  }

  for (const Strip *strip : retiming_sel.values()) {
    if (seq::retiming_data_is_editable(strip)) {
      return true;
    }
  }

  return false;
}

static bool retiming_poll(bContext *C)
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
  if (strip == nullptr) {
    return false;
  }
  if (!seq::retiming_is_allowed(strip)) {
    CTX_wm_operator_poll_msg_set(C, "This strip type cannot be retimed");
    return false;
  }
  return true;
}

/** \} */

/*-------------------------------------------------------------------- */
/** \name Retiming Data Show
 * \{ */

static void sequencer_retiming_data_show_selection(ListBaseT<Strip> *seqbase)
{
  for (Strip &strip : *seqbase) {
    if ((strip.flag & SEQ_SELECT) == 0) {
      continue;
    }
    if (!seq::retiming_is_allowed(&strip)) {
      continue;
    }
    strip.flag |= SEQ_SHOW_RETIMING;
  }
}

static void sequencer_retiming_data_hide_selection(ListBaseT<Strip> *seqbase)
{
  for (Strip &strip : *seqbase) {
    if ((strip.flag & SEQ_SELECT) == 0) {
      continue;
    }
    if (!seq::retiming_is_allowed(&strip)) {
      continue;
    }
    strip.flag &= ~SEQ_SHOW_RETIMING;
  }
}

static void sequencer_retiming_data_hide_all(ListBaseT<Strip> *seqbase)
{
  for (Strip &strip : *seqbase) {
    strip.flag &= ~SEQ_SHOW_RETIMING;
  }
}

static wmOperatorStatus sequencer_retiming_data_show_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(scene);
  Strip *strip_act = seq::select_active_get(scene);

  if (strip_act == nullptr) {
    return OPERATOR_CANCELLED;
  }

  if (sequencer_retiming_mode_is_active(scene)) {
    sequencer_retiming_data_hide_all(ed->current_strips());
  }
  else if (seq::retiming_data_is_editable(strip_act)) {
    sequencer_retiming_data_hide_selection(ed->current_strips());
  }
  else {
    sequencer_retiming_data_show_selection(ed->current_strips());
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_retiming_show(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Retime Strips";
  ot->description = "Show retiming keys in selected strips";
  ot->idname = "SEQUENCER_OT_retiming_show";

  /* API callbacks. */
  ot->exec = sequencer_retiming_data_show_exec;
  ot->poll = sequencer_editing_initialized_and_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/*-------------------------------------------------------------------- */
/** \name Retiming Reset
 * \{ */

static wmOperatorStatus sequencer_retiming_reset_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  const Editing *ed = seq::editing_get(scene);

  for (Strip *strip : seq::query_selected_strips(ed->current_strips())) {
    seq::retiming_reset(scene, strip);
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_retiming_reset(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reset Retiming";
  ot->description = "Reset strip retiming";
  ot->idname = "SEQUENCER_OT_retiming_reset";

  /* API callbacks. */
  ot->exec = sequencer_retiming_reset_exec;
  ot->poll = retiming_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Retiming Add Key
 * \{ */

static wmOperatorStatus retiming_key_add_from_selection(const Scene *scene,
                                                        wmOperator *op,
                                                        Span<Strip *> strips,
                                                        const int timeline_frame)
{
  bool inserted = false;

  for (Strip *strip : strips) {
    if (!seq::retiming_is_allowed(strip)) {
      continue;
    }
    if (seq::retiming_key_add_new_for_strip(scene, op->reports, strip, timeline_frame)) {
      inserted = true;
    }
  }

  return inserted ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static wmOperatorStatus retiming_key_add_to_editable_strips(const Scene *scene,
                                                            wmOperator *op,
                                                            const int timeline_frame)
{
  Editing *ed = seq::editing_get(scene);
  bool inserted = false;

  Map selection = seq::retiming_selection_get(ed);
  if (selection.is_empty()) {
    return OPERATOR_CANCELLED;
  }

  for (Strip *strip : selection.values()) {
    if (seq::retiming_key_add_new_for_strip(scene, op->reports, strip, timeline_frame)) {
      inserted = true;
    }
  }

  return inserted ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static wmOperatorStatus sequencer_retiming_key_add_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);

  float timeline_frame;
  if (RNA_struct_property_is_set(op->ptr, "timeline_frame")) {
    timeline_frame = RNA_int_get(op->ptr, "timeline_frame");
  }
  else {
    timeline_frame = BKE_scene_frame_get(scene);
  }

  wmOperatorStatus ret_val;
  VectorSet<Strip *> strips = selected_strips_from_context(C);
  if (!strips.is_empty()) {
    ret_val = retiming_key_add_from_selection(scene, op, strips, timeline_frame);
  }
  else {
    ret_val = retiming_key_add_to_editable_strips(scene, op, timeline_frame);
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
  return ret_val;
}

void SEQUENCER_OT_retiming_key_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Retiming Key";
  ot->description = "Add retiming Key";
  ot->idname = "SEQUENCER_OT_retiming_key_add";

  /* API callbacks. */
  ot->exec = sequencer_retiming_key_add_exec;
  ot->poll = retiming_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_int(ot->srna,
              "timeline_frame",
              0,
              0,
              INT_MAX,
              "Timeline Frame",
              "Frame where key will be added",
              0,
              INT_MAX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Retiming Add Freeze Frame
 * \{ */

static bool freeze_frame_add_new_for_strip(const bContext *C,
                                           const wmOperator *op,
                                           Strip *strip,
                                           const int timeline_frame,
                                           const int duration)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  seq::ensure_left_and_right_keys(scene, strip);

  // ensure L+R key
  SeqRetimingKey *key = seq::retiming_add_key(scene, strip, timeline_frame);

  if (key == nullptr) {
    BKE_report(op->reports, RPT_WARNING, "Cannot create freeze frame");
    return false;
  }

  if (seq::retiming_key_is_transition_start(key)) {
    BKE_report(op->reports, RPT_WARNING, "Cannot create key inside of speed transition");
    return false;
  }

  seq::retiming_selection_remove(key);
  SeqRetimingKey *freeze = seq::retiming_add_freeze_frame(scene, strip, key, duration);

  if (freeze == nullptr) {
    BKE_report(op->reports, RPT_WARNING, "Cannot create freeze frame");
    return false;
  }

  deselect_all_strips(scene);
  sequencer_select_do_updates(C, scene);

  seq::retiming_selection_append(freeze);

  seq::relations_invalidate_cache_raw(scene, strip);
  return true;
}

static bool freeze_frame_add_from_strip_selection(bContext *C,
                                                  const wmOperator *op,
                                                  const int duration)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  VectorSet<Strip *> strips = selected_strips_from_context(C);
  strips.remove_if([&](Strip *strip) { return !seq::retiming_is_allowed(strip); });
  const int timeline_frame = BKE_scene_frame_get(scene);
  bool success = false;

  for (Strip *strip : strips) {
    success |= freeze_frame_add_new_for_strip(C, op, strip, timeline_frame, duration);
    seq::relations_invalidate_cache_raw(scene, strip);
  }
  return success;
}

static bool freeze_frame_add_from_retiming_selection(const bContext *C,
                                                     const wmOperator *op,
                                                     const int duration)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  bool success = false;

  Map selection = seq::retiming_selection_get(seq::editing_get(scene));

  for (auto item : selection.items()) {
    const int timeline_frame = seq::retiming_key_timeline_frame_get(scene, item.value, item.key);
    success |= freeze_frame_add_new_for_strip(C, op, item.value, timeline_frame, duration);
    seq::relations_invalidate_cache_raw(scene, item.value);
  }
  return success;
}

static wmOperatorStatus sequencer_retiming_freeze_frame_add_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  bool success = false;

  int duration = 1;

  if (RNA_property_is_set(op->ptr, RNA_struct_find_property(op->ptr, "duration"))) {
    duration = RNA_int_get(op->ptr, "duration");
  }

  if (sequencer_retiming_mode_is_active(scene)) {
    success = freeze_frame_add_from_retiming_selection(C, op, duration);
  }
  else {
    success = freeze_frame_add_from_strip_selection(C, op, duration);
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return success ? OPERATOR_FINISHED : OPERATOR_PASS_THROUGH;
}

void SEQUENCER_OT_retiming_freeze_frame_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Freeze Frame";
  ot->description = "Add freeze frame";
  ot->idname = "SEQUENCER_OT_retiming_freeze_frame_add";

  /* API callbacks. */
  ot->exec = sequencer_retiming_freeze_frame_add_exec;
  ot->poll = retiming_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_int(ot->srna,
              "duration",
              0,
              0,
              INT_MAX,
              "Duration",
              "Duration of freeze frame segment",
              0,
              INT_MAX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Retiming Add Speed Transition
 * \{ */

static bool transition_add_new_for_strip(const bContext *C,
                                         const wmOperator *op,
                                         Strip *strip,
                                         const int timeline_frame,
                                         const int duration)
{
  Scene *scene = CTX_data_sequencer_scene(C);

  // ensure L+R key
  seq::ensure_left_and_right_keys(scene, strip);
  SeqRetimingKey *key = seq::retiming_add_key(scene, strip, timeline_frame);

  if (key == nullptr) {
    key = seq::retiming_key_get_by_timeline_frame(scene, strip, timeline_frame);
  }

  if (seq::retiming_is_last_key(strip, key) || key->strip_frame_index == 0) {
    BKE_report(op->reports, RPT_WARNING, "Cannot create transition from first or last key");
    return false;
  }

  SeqRetimingKey *transition = seq::retiming_add_transition(scene, strip, key, duration);

  if (transition == nullptr) {
    BKE_report(op->reports, RPT_WARNING, "Cannot create transition");
    return false;
  }

  deselect_all_strips(scene);
  sequencer_select_do_updates(C, scene);

  seq::retiming_selection_append(transition);

  seq::relations_invalidate_cache_raw(scene, strip);
  return true;
}

static bool transition_add_from_retiming_selection(const bContext *C,
                                                   const wmOperator *op,
                                                   const int duration)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  bool success = false;

  Map selection = seq::retiming_selection_get(seq::editing_get(scene));

  for (auto item : selection.items()) {
    const int timeline_frame = seq::retiming_key_timeline_frame_get(scene, item.value, item.key);
    success |= transition_add_new_for_strip(C, op, item.value, timeline_frame, duration);
  }
  return success;
}

static wmOperatorStatus sequencer_retiming_transition_add_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  bool success = false;

  int duration = 1;

  if (RNA_property_is_set(op->ptr, RNA_struct_find_property(op->ptr, "duration"))) {
    duration = RNA_int_get(op->ptr, "duration");
  }

  if (sequencer_retiming_mode_is_active(scene)) {
    success = transition_add_from_retiming_selection(C, op, duration);
  }
  else {
    BKE_report(op->reports, RPT_WARNING, "Retiming key must be selected");
    return OPERATOR_CANCELLED;
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return success ? OPERATOR_FINISHED : OPERATOR_PASS_THROUGH;
}

void SEQUENCER_OT_retiming_transition_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Speed Transition";
  ot->description = "Add smooth transition between 2 retimed segments";
  ot->idname = "SEQUENCER_OT_retiming_transition_add";

  /* API callbacks. */
  ot->exec = sequencer_retiming_transition_add_exec;
  ot->poll = retiming_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_int(ot->srna,
              "duration",
              0,
              0,
              INT_MAX,
              "Duration",
              "Duration of freeze frame segment",
              0,
              INT_MAX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Retiming Delete Key
 * \{ */

static wmOperatorStatus sequencer_retiming_key_delete_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_sequencer_scene(C);

  Map selection = seq::retiming_selection_get(seq::editing_get(scene));
  Vector<Strip *> strips_to_handle;

  if (!sequencer_retiming_mode_is_active(scene) || selection.size() == 0) {
    return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
  }

  for (Strip *strip : selection.values()) {
    strips_to_handle.append_non_duplicates(strip);
  }

  for (Strip *strip : strips_to_handle) {
    Vector<SeqRetimingKey *> keys_to_delete;
    for (auto item : selection.items()) {
      if (item.value != strip) {
        continue;
      }
      keys_to_delete.append(item.key);
    }

    seq::retiming_remove_multiple_keys(strip, keys_to_delete);
  }

  for (Strip *strip : strips_to_handle) {
    seq::relations_invalidate_cache_raw(scene, strip);
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
  return OPERATOR_FINISHED;
}

static wmOperatorStatus sequencer_retiming_key_delete_invoke(bContext *C,
                                                             wmOperator *op,
                                                             const wmEvent *event)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  ListBaseT<TimeMarker> *markers = &scene->markers;

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

  return sequencer_retiming_key_delete_exec(C, op);
}

void SEQUENCER_OT_retiming_key_delete(wmOperatorType *ot)
{

  /* Identifiers. */
  ot->name = "Delete Retiming Keys";
  ot->idname = "SEQUENCER_OT_retiming_key_delete";
  ot->description = "Delete selected retiming keys from the sequencer";

  /* API callbacks. */
  ot->invoke = sequencer_retiming_key_delete_invoke;
  ot->exec = sequencer_retiming_key_delete_exec;
  ot->poll = retiming_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Retiming Set Segment Speed
 * \{ */

/* Return speed of existing segment or active strip. */
static float strip_speed_get(const Scene *scene)
{
  /* Strip mode. */
  if (!sequencer_retiming_mode_is_active(scene)) {
    Strip *strip = seq::editing_get(scene)->act_strip;
    SeqRetimingKey *key = seq::ensure_left_and_right_keys(scene, strip);
    return seq::retiming_key_speed_get(strip, key);
  }

  Map selection = seq::retiming_selection_get(seq::editing_get(scene));
  /* Retiming mode. */
  if (selection.size() == 1) {
    for (auto item : selection.items()) {
      return seq::retiming_key_speed_get(item.value, item.key);
    }
  }

  return 1.0f;
}

static void strip_speed_set(Scene *scene, VectorSet<Strip *> strips, const float speed)
{
  for (Strip *strip : strips) {
    SeqRetimingKey *key = seq::ensure_left_and_right_keys(scene, strip);

    if (key == nullptr) {
      continue;
    }

    /* TODO: it would be nice to multiply speed with complex retiming by a factor. */
    seq::retiming_key_speed_set(scene, strip, key, speed / 100.0f, false);

    ListBaseT<Strip> *seqbase = seq::active_seqbase_get(seq::editing_get(scene));
    if (seq::transform_test_overlap(scene, seqbase, strip)) {
      seq::transform_seqbase_shuffle(seqbase, strip, scene);
    }

    seq::relations_invalidate_cache_raw(scene, strip);
  }
}

static void segment_speed_set(Scene *scene,
                              Map<SeqRetimingKey *, Strip *> selection,
                              const float speed,
                              const bool keep_retiming)
{
  ListBaseT<Strip> *seqbase = seq::active_seqbase_get(seq::editing_get(scene));

  for (auto item : selection.items()) {
    seq::retiming_key_speed_set(scene, item.value, item.key, speed / 100.0f, keep_retiming);

    if (seq::transform_test_overlap(scene, seqbase, item.value)) {
      seq::transform_seqbase_shuffle(seqbase, item.value, scene);
    }

    seq::relations_invalidate_cache_raw(scene, item.value);
  }
}

static wmOperatorStatus sequencer_retiming_segment_speed_set_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  const float speed = RNA_float_get(op->ptr, "speed");
  const bool keep_retiming = RNA_boolean_get(op->ptr, "keep_retiming");

  /* Strip mode. */
  if (!sequencer_retiming_mode_is_active(scene)) {
    VectorSet<Strip *> strips = selected_strips_from_context(C);
    strips.remove_if([&](Strip *strip) { return !seq::retiming_is_allowed(strip); });
    strip_speed_set(scene, strips, speed);
    WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
    return OPERATOR_FINISHED;
  }

  Map selection = seq::retiming_selection_get(seq::editing_get(scene));

  /* Retiming mode. */
  if (selection.size() > 0) {
    segment_speed_set(scene, selection, speed, keep_retiming);
    WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
    return OPERATOR_FINISHED;
  }

  BKE_report(op->reports, RPT_ERROR, "No keys or strips selected");
  return OPERATOR_CANCELLED;
}

static wmOperatorStatus sequencer_retiming_segment_speed_set_invoke(bContext *C,
                                                                    wmOperator *op,
                                                                    const wmEvent *event)
{
  const Scene *scene = CTX_data_sequencer_scene(C);

  if (!RNA_struct_property_is_set(op->ptr, "speed")) {
    RNA_float_set(op->ptr, "speed", strip_speed_get(scene) * 100.0f);
    return WM_operator_props_popup(C, op, event);
  }

  return sequencer_retiming_segment_speed_set_exec(C, op);
}

void SEQUENCER_OT_retiming_segment_speed_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Speed";
  ot->description = "Set speed of retimed segment";
  ot->idname = "SEQUENCER_OT_retiming_segment_speed_set";

  /* API callbacks. */
  ot->invoke = sequencer_retiming_segment_speed_set_invoke;
  ot->exec = sequencer_retiming_segment_speed_set_exec;
  ot->poll = retiming_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_float(ot->srna,
                "speed",
                100.0f,
                0.001f,
                FLT_MAX,
                "Speed",
                "New speed of retimed segment",
                0.1f,
                FLT_MAX);

  RNA_def_boolean(ot->srna,
                  "keep_retiming",
                  true,
                  "Preserve Current Retiming",
                  "Keep speed of other segments unchanged, change strip length instead");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Retiming Select Key
 * \{ */

static bool select_key(const Editing *ed,
                       SeqRetimingKey *key,
                       const bool toggle,
                       const bool deselect_all)
{
  bool changed = false;

  if (deselect_all) {
    changed = seq::retiming_selection_clear(ed);
  }

  if (key == nullptr) {
    return changed;
  }

  if (toggle && seq::retiming_selection_contains(ed, key)) {
    seq::retiming_selection_remove(key);
  }
  else {
    seq::retiming_selection_append(key);
  }

  return true;
}

static bool select_connected_keys(const Scene *scene,
                                  const SeqRetimingKey *source,
                                  const Strip *source_owner)
{
  if (!seq::is_strip_connected(source_owner)) {
    return false;
  }

  const int frame = seq::retiming_key_timeline_frame_get(scene, source_owner, source);
  bool changed = false;
  VectorSet<Strip *> connections = seq::connected_strips_get(source_owner);
  for (Strip *connection : connections) {
    SeqRetimingKey *con_key = seq::retiming_key_get_by_timeline_frame(scene, connection, frame);

    if (con_key) {
      seq::retiming_selection_copy(con_key, source);
      changed = true;
    }
  }
  return changed;
}

static void sequencer_retiming_select_linked_time(const Scene *scene,
                                                  const bool extend,
                                                  SeqRetimingKey *key,
                                                  const Strip *key_owner)
{
  Editing *ed = seq::editing_get(scene);

  if (!extend) {
    seq::retiming_selection_clear(ed);
  }
  for (; key <= seq::retiming_last_key_get(key_owner); key++) {
    select_key(ed, key, false, false);
    select_connected_keys(scene, key, key_owner);
  }
}

wmOperatorStatus sequencer_retiming_key_select_exec(bContext *C,
                                                    wmOperator *op,
                                                    SeqRetimingKey *key,
                                                    const Strip *key_owner)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  const Editing *ed = seq::editing_get(scene);

  const bool deselect_all = RNA_boolean_get(op->ptr, "deselect_all");
  const bool wait_to_deselect_others = RNA_boolean_get(op->ptr, "wait_to_deselect_others");
  const bool toggle = RNA_boolean_get(op->ptr, "toggle");
  const bool extend = RNA_boolean_get(op->ptr, "extend");

  if (RNA_boolean_get(op->ptr, "linked_time")) {
    sequencer_retiming_select_linked_time(scene, extend, key, key_owner);
    WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
    return OPERATOR_FINISHED;
  }

  /* Clicked on an unselected key. */
  if (!seq::retiming_selection_contains(ed, key) && !toggle) {
    select_key(ed, key, false, deselect_all);
    select_connected_keys(scene, key, key_owner);
  }

  /* Clicked on a key that is already selected, waiting to click release. */
  if (wait_to_deselect_others && !toggle) {
    return OPERATOR_RUNNING_MODAL;
  }

  /* The key is already selected, but deselect other selected keys after click is released if no
   * transform or toggle happened. */
  bool changed = select_key(ed, key, toggle, deselect_all);
  if (!toggle) {
    changed |= select_connected_keys(scene, key, key_owner);
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
  return changed ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static void realize_fake_keys_in_rect(const Scene *scene, Strip *strip, const rctf &rectf)
{
  const int content_start = strip->content_start();
  const int left_key_frame = max_ii(content_start, strip->left_handle());
  const int content_end = strip->content_end(scene);
  const int right_key_frame = min_ii(content_end, strip->right_handle(scene));

  /* Realize "fake" keys. */
  if (left_key_frame > rectf.xmin && left_key_frame < rectf.xmax) {
    seq::retiming_add_key(scene, strip, left_key_frame);
  }
  if (right_key_frame > rectf.xmin && right_key_frame < rectf.xmax) {
    seq::retiming_add_key(scene, strip, right_key_frame);
  }
}

wmOperatorStatus sequencer_retiming_box_select_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_sequencer_scene(C);
  const View2D *v2d = ui::view2d_fromcontext(C);
  Editing *ed = seq::editing_get(scene);

  if (ed == nullptr) {
    return OPERATOR_CANCELLED;
  }

  const eSelectOp sel_op = eSelectOp(RNA_enum_get(op->ptr, "mode"));
  bool changed = false;

  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    changed |= seq::retiming_selection_clear(ed);
  }

  rctf rectf;
  WM_operator_properties_border_to_rctf(op, &rectf);
  ui::view2d_region_to_view_rctf(v2d, &rectf, &rectf);

  Set<SeqRetimingKey *> and_keys;

  for (Strip *strip : sequencer_visible_strips_get(C)) {
    if (strip->channel < rectf.ymin || strip->channel > rectf.ymax) {
      continue;
    }
    if (!seq::retiming_data_is_editable(strip)) {
      continue;
    }
    realize_fake_keys_in_rect(scene, strip, rectf);

    for (SeqRetimingKey &key : seq::retiming_keys_get(strip)) {
      const int key_frame = seq::retiming_key_timeline_frame_get(scene, strip, &key);
      const int strip_start = strip->left_handle();
      const int strip_end = strip->right_handle(scene);
      if (key_frame < strip_start || key_frame > strip_end) {
        continue;
      }
      if (key_frame > rectf.xmax || key_frame < rectf.xmin) {
        continue;
      }

      switch (sel_op) {
        case SEL_OP_ADD:
        case SEL_OP_SET: {
          seq::retiming_selection_append(&key);
          break;
        }
        case SEL_OP_SUB: {
          seq::retiming_selection_remove(&key);
          break;
        }
        case SEL_OP_XOR: { /* Toggle */
          if (seq::retiming_selection_contains(ed, &key)) {
            seq::retiming_selection_remove(&key);
          }
          else {
            seq::retiming_selection_append(&key);
          }
          break;
          case SEL_OP_AND: {
            if (seq::retiming_selection_contains(ed, &key)) {
              and_keys.add(&key);
            }
            break;
          }
        }
      }
      changed = true;
    }
  }

  if (and_keys.size() > 0) {
    seq::retiming_selection_clear(ed);
    for (auto *key : and_keys) {
      seq::retiming_selection_append(key);
    }
  }

  return changed ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

wmOperatorStatus sequencer_retiming_select_all_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  int action = RNA_enum_get(op->ptr, "action");

  VectorSet<Strip *> strips = all_strips_from_context(C);

  if (action == SEL_TOGGLE) {
    action = SEL_SELECT;
    for (Strip *strip : strips) {
      if (!seq::retiming_data_is_editable(strip)) {
        continue;
      }
      for (SeqRetimingKey &key : seq::retiming_keys_get(strip)) {
        if (key.flag & SEQ_KEY_SELECTED) {
          action = SEL_DESELECT;
          break;
        }
      }
    }
  }

  if (action == SEL_DESELECT) {
    seq::retiming_selection_clear(seq::editing_get(scene));
  }

  for (Strip *strip : strips) {
    if (!seq::retiming_data_is_editable(strip)) {
      continue;
    }
    for (SeqRetimingKey &key : seq::retiming_keys_get(strip)) {
      switch (action) {
        case SEL_SELECT:
          key.flag |= SEQ_KEY_SELECTED;
          break;
        case SEL_INVERT:
          if (key.flag & SEQ_KEY_SELECTED) {
            key.flag &= ~SEQ_KEY_SELECTED;
          }
          else {
            key.flag |= SEQ_KEY_SELECTED;
          }
          break;
      }
    }
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
  return OPERATOR_FINISHED;
}

/** \} */

}  // namespace blender::ed::vse
