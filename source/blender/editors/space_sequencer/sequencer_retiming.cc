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
#include "SEQ_time.hh"
#include "SEQ_transform.hh"

#include "WM_api.hh"

#include "RNA_define.hh"

#include "UI_view2d.hh"

/* Own include. */
#include "sequencer_intern.hh"

namespace blender::ed::vse {

bool sequencer_retiming_mode_is_active(const bContext *C)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  if (!scene) {
    return false;
  }
  Editing *ed = seq::editing_get(scene);
  if (!ed) {
    return false;
  }

  Map retiming_sel = seq::retiming_selection_get(ed);

  if (seq::retiming_selection_get(ed).size() == 0) {
    return false;
  }

  bool any_strip_has_editable_retiming = false;
  for (const Strip *strip : retiming_sel.values()) {
    any_strip_has_editable_retiming |= seq::retiming_data_is_editable(strip);
  }

  return any_strip_has_editable_retiming;
}

/*-------------------------------------------------------------------- */
/** \name Retiming Data Show
 * \{ */

static void sequencer_retiming_data_show_selection(ListBase *seqbase)
{
  LISTBASE_FOREACH (Strip *, strip, seqbase) {
    if ((strip->flag & SELECT) == 0) {
      continue;
    }
    if (!seq::retiming_is_allowed(strip)) {
      continue;
    }
    strip->flag |= SEQ_SHOW_RETIMING;
  }
}

static void sequencer_retiming_data_hide_selection(ListBase *seqbase)
{
  LISTBASE_FOREACH (Strip *, strip, seqbase) {
    if ((strip->flag & SELECT) == 0) {
      continue;
    }
    if (!seq::retiming_is_allowed(strip)) {
      continue;
    }
    strip->flag &= ~SEQ_SHOW_RETIMING;
  }
}

static void sequencer_retiming_data_hide_all(ListBase *seqbase)
{
  LISTBASE_FOREACH (Strip *, strip, seqbase) {
    strip->flag &= ~SEQ_SHOW_RETIMING;
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

  if (sequencer_retiming_mode_is_active(C)) {
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

static SeqRetimingKey *ensure_left_and_right_keys(const bContext *C, Strip *strip)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  seq::retiming_data_ensure(strip);
  seq::retiming_add_key(scene, strip, left_fake_key_frame_get(C, strip));
  return seq::retiming_add_key(scene, strip, right_fake_key_frame_get(C, strip));
}

/* -------------------------------------------------------------------- */
/** \name Retiming Add Key
 * \{ */

static bool retiming_key_add_new_for_strip(bContext *C,
                                           wmOperator *op,
                                           Strip *strip,
                                           const int timeline_frame)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  const float scene_fps = float(scene->r.frs_sec) / float(scene->r.frs_sec_base);
  const float frame_index = (BKE_scene_frame_get(scene) - seq::time_start_frame_get(strip)) *
                            seq::time_media_playback_rate_factor_get(strip, scene_fps);
  const SeqRetimingKey *key = seq::retiming_find_segment_start_key(strip, frame_index);

  if (key != nullptr && seq::retiming_key_is_transition_start(key)) {
    BKE_report(op->reports, RPT_WARNING, "Cannot create key inside of speed transition");
    return false;
  }

  const float end_frame = strip->start + seq::time_strip_length_get(scene, strip);
  if (strip->start > timeline_frame || end_frame < timeline_frame) {
    return false;
  }

  ensure_left_and_right_keys(C, strip);
  seq::retiming_add_key(scene, strip, timeline_frame);
  return true;
}

static wmOperatorStatus retiming_key_add_from_selection(bContext *C,
                                                        wmOperator *op,
                                                        Span<Strip *> strips,
                                                        const int timeline_frame)
{
  bool inserted = false;

  for (Strip *strip : strips) {
    if (!seq::retiming_is_allowed(strip)) {
      continue;
    }
    inserted |= retiming_key_add_new_for_strip(C, op, strip, timeline_frame);
  }

  return inserted ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static wmOperatorStatus retiming_key_add_to_editable_strips(bContext *C,
                                                            wmOperator *op,
                                                            const int timeline_frame)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(scene);
  bool inserted = false;

  Map selection = seq::retiming_selection_get(ed);
  if (selection.is_empty()) {
    return OPERATOR_CANCELLED;
  }

  for (Strip *strip : selection.values()) {
    inserted |= retiming_key_add_new_for_strip(C, op, strip, timeline_frame);
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
    ret_val = retiming_key_add_from_selection(C, op, strips, timeline_frame);
  }
  else {
    ret_val = retiming_key_add_to_editable_strips(C, op, timeline_frame);
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
  ensure_left_and_right_keys(C, strip);

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

  if (sequencer_retiming_mode_is_active(C)) {
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
  ensure_left_and_right_keys(C, strip);
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

  if (sequencer_retiming_mode_is_active(C)) {
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

  if (!sequencer_retiming_mode_is_active(C) || selection.size() == 0) {
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

/* Return speed of existing segment or strip. Assume 1 element is selected. */
static float strip_speed_get(bContext *C, const wmOperator * /*op*/)
{
  /* Strip mode. */
  if (!sequencer_retiming_mode_is_active(C)) {
    VectorSet<Strip *> strips = selected_strips_from_context(C);
    if (strips.size() == 1) {
      Strip *strip = strips[0];
      SeqRetimingKey *key = ensure_left_and_right_keys(C, strip);
      return seq::retiming_key_speed_get(strip, key);
    }
  }

  Scene *scene = CTX_data_sequencer_scene(C);
  Map selection = seq::retiming_selection_get(seq::editing_get(scene));
  /* Retiming mode. */
  if (selection.size() == 1) {
    for (auto item : selection.items()) {
      return seq::retiming_key_speed_get(item.value, item.key);
    }
  }

  return 1.0f;
}

static wmOperatorStatus strip_speed_set_exec(bContext *C, const wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  VectorSet<Strip *> strips = selected_strips_from_context(C);
  strips.remove_if([&](Strip *strip) { return !seq::retiming_is_allowed(strip); });

  for (Strip *strip : strips) {
    SeqRetimingKey *key = ensure_left_and_right_keys(C, strip);

    if (key == nullptr) {
      continue;
    }
    /* TODO: it would be nice to multiply speed with complex retiming by a factor. */
    seq::retiming_key_speed_set(
        scene, strip, key, RNA_float_get(op->ptr, "speed") / 100.0f, false);

    ListBase *seqbase = seq::active_seqbase_get(seq::editing_get(scene));
    if (seq::transform_test_overlap(scene, seqbase, strip)) {
      seq::transform_seqbase_shuffle(seqbase, strip, scene);
    }

    seq::relations_invalidate_cache_raw(scene, strip);
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
  return OPERATOR_FINISHED;
}

static wmOperatorStatus segment_speed_set_exec(const bContext *C,
                                               const wmOperator *op,
                                               Map<SeqRetimingKey *, Strip *> selection)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  ListBase *seqbase = seq::active_seqbase_get(seq::editing_get(scene));

  for (auto item : selection.items()) {
    seq::retiming_key_speed_set(scene,
                                item.value,
                                item.key,
                                RNA_float_get(op->ptr, "speed") / 100.0f,
                                RNA_boolean_get(op->ptr, "keep_retiming"));

    if (seq::transform_test_overlap(scene, seqbase, item.value)) {
      seq::transform_seqbase_shuffle(seqbase, item.value, scene);
    }

    seq::relations_invalidate_cache_raw(scene, item.value);
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
  return OPERATOR_FINISHED;
}

static wmOperatorStatus sequencer_retiming_segment_speed_set_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_sequencer_scene(C);

  /* Strip mode. */
  if (!sequencer_retiming_mode_is_active(C)) {
    return strip_speed_set_exec(C, op);
  }

  Map selection = seq::retiming_selection_get(seq::editing_get(scene));

  /* Retiming mode. */
  if (selection.size() > 0) {
    return segment_speed_set_exec(C, op, selection);
  }

  BKE_report(op->reports, RPT_ERROR, "No keys or strips selected");
  return OPERATOR_CANCELLED;
}

static wmOperatorStatus sequencer_retiming_segment_speed_set_invoke(bContext *C,
                                                                    wmOperator *op,
                                                                    const wmEvent *event)
{
  if (!RNA_struct_property_is_set(op->ptr, "speed")) {
    RNA_float_set(op->ptr, "speed", strip_speed_get(C, op) * 100.0f);
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

wmOperatorStatus sequencer_retiming_select_linked_time(bContext *C,
                                                       wmOperator *op,
                                                       SeqRetimingKey *key,
                                                       const Strip *key_owner)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(scene);

  if (!RNA_boolean_get(op->ptr, "extend")) {
    seq::retiming_selection_clear(ed);
  }
  for (; key <= seq::retiming_last_key_get(key_owner); key++) {
    select_key(ed, key, false, false);
    select_connected_keys(scene, key, key_owner);
  }
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
  return OPERATOR_FINISHED;
}

wmOperatorStatus sequencer_retiming_key_select_exec(bContext *C,
                                                    wmOperator *op,
                                                    SeqRetimingKey *key,
                                                    const Strip *key_owner)
{
  if (RNA_boolean_get(op->ptr, "linked_time")) {
    return sequencer_retiming_select_linked_time(C, op, key, key_owner);
  }

  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(scene);

  const bool deselect_all = RNA_boolean_get(op->ptr, "deselect_all");
  const bool wait_to_deselect_others = RNA_boolean_get(op->ptr, "wait_to_deselect_others");
  const bool toggle = RNA_boolean_get(op->ptr, "toggle");

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

static void realize_fake_keys_in_rect(bContext *C, Strip *strip, const rctf &rectf)
{
  const Scene *scene = CTX_data_sequencer_scene(C);

  const int content_start = seq::time_start_frame_get(strip);
  const int left_key_frame = max_ii(content_start, seq::time_left_handle_frame_get(scene, strip));
  const int content_end = seq::time_content_end_frame_get(scene, strip);
  const int right_key_frame = min_ii(content_end, seq::time_right_handle_frame_get(scene, strip));

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
  const View2D *v2d = UI_view2d_fromcontext(C);
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
  UI_view2d_region_to_view_rctf(v2d, &rectf, &rectf);

  Set<SeqRetimingKey *> and_keys;

  for (Strip *strip : sequencer_visible_strips_get(C)) {
    if (strip->channel < rectf.ymin || strip->channel > rectf.ymax) {
      continue;
    }
    if (!seq::retiming_data_is_editable(strip)) {
      continue;
    }
    realize_fake_keys_in_rect(C, strip, rectf);

    for (SeqRetimingKey &key : seq::retiming_keys_get(strip)) {
      const int key_frame = seq::retiming_key_timeline_frame_get(scene, strip, &key);
      const int strip_start = seq::time_left_handle_frame_get(scene, strip);
      const int strip_end = seq::time_right_handle_frame_get(scene, strip);
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

}  // namespace blender::ed::vse
