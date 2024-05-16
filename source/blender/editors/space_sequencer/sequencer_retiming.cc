/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 */

#include "MEM_guardedalloc.h"

#include "BLI_set.hh"

#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "BKE_context.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "ED_select_utils.hh"
#include "ED_sequencer.hh"

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

using blender::MutableSpan;

bool sequencer_retiming_mode_is_active(const bContext *C)
{
  Editing *ed = SEQ_editing_get(CTX_data_scene(C));
  if (ed == nullptr) {
    return false;
  }
  return SEQ_retiming_selection_get(ed).size() > 0;
}

/*-------------------------------------------------------------------- */
/** \name Retiming Data Show
 * \{ */

static void sequencer_retiming_data_show_selection(ListBase *seqbase)
{
  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    if ((seq->flag & SELECT) == 0) {
      continue;
    }
    seq->flag |= SEQ_SHOW_RETIMING;
  }
}

static void sequencer_retiming_data_hide_selection(ListBase *seqbase)
{
  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    if ((seq->flag & SELECT) == 0) {
      continue;
    }
    seq->flag &= ~SEQ_SHOW_RETIMING;
  }
}

static void sequencer_retiming_data_hide_all(ListBase *seqbase)
{
  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    seq->flag &= ~SEQ_SHOW_RETIMING;
  }
}

static int sequencer_retiming_data_show_exec(bContext *C, wmOperator * /* op */)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene);
  Sequence *seq_act = SEQ_select_active_get(scene);

  if (seq_act == nullptr) {
    return OPERATOR_CANCELLED;
  }

  if (sequencer_retiming_mode_is_active(C)) {
    sequencer_retiming_data_hide_all(ed->seqbasep);
  }
  else if (SEQ_retiming_data_is_editable(seq_act)) {
    sequencer_retiming_data_hide_selection(ed->seqbasep);
  }
  else {
    sequencer_retiming_data_show_selection(ed->seqbasep);
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

  /* api callbacks */
  ot->exec = sequencer_retiming_data_show_exec;
  ot->poll = sequencer_editing_initialized_and_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

static bool retiming_poll(bContext *C)
{
  const Editing *ed = SEQ_editing_get(CTX_data_scene(C));
  if (ed == nullptr) {
    return false;
  }
  Sequence *seq = ed->act_seq;
  if (seq == nullptr) {
    return false;
  }
  if (!SEQ_retiming_is_allowed(seq)) {
    CTX_wm_operator_poll_msg_set(C, "This strip type cannot be retimed");
    return false;
  }
  return true;
}

static void retiming_key_overlap(Scene *scene, Sequence *seq)
{
  ListBase *seqbase = SEQ_active_seqbase_get(SEQ_editing_get(scene));
  blender::VectorSet<Sequence *> strips;
  blender::VectorSet<Sequence *> dependant;
  strips.add(seq);
  dependant.add(seq);
  SEQ_iterator_set_expand(scene, seqbase, dependant, SEQ_query_strip_effect_chain);
  dependant.remove(seq);
  SEQ_transform_handle_overlap(scene, seqbase, strips, dependant, true);
}

/*-------------------------------------------------------------------- */
/** \name Retiming Reset
 * \{ */

static int sequencer_retiming_reset_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  const Editing *ed = SEQ_editing_get(scene);
  Sequence *seq = ed->act_seq;

  SEQ_retiming_data_clear(seq);

  retiming_key_overlap(scene, seq);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_retiming_reset(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reset Retiming";
  ot->description = "Reset strip retiming";
  ot->idname = "SEQUENCER_OT_retiming_reset";

  /* api callbacks */
  ot->exec = sequencer_retiming_reset_exec;
  ot->poll = retiming_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Retiming Add Key
 * \{ */

static bool retiming_key_add_new_for_seq(bContext *C,
                                         wmOperator *op,
                                         Sequence *seq,
                                         const int timeline_frame)
{
  Scene *scene = CTX_data_scene(C);
  const int frame_index = BKE_scene_frame_get(scene) - SEQ_time_start_frame_get(seq);
  const SeqRetimingKey *key = SEQ_retiming_find_segment_start_key(seq, frame_index);

  if (key != nullptr && SEQ_retiming_key_is_transition_start(key)) {
    BKE_report(op->reports, RPT_WARNING, "Cannot create key inside of speed transition");
    return false;
  }

  const float end_frame = seq->start + SEQ_time_strip_length_get(scene, seq);
  if (seq->start > timeline_frame || end_frame < timeline_frame) {
    return false;
  }

  SEQ_retiming_data_ensure(seq);
  SEQ_retiming_add_key(scene, seq, timeline_frame);
  return true;
}

static int retiming_key_add_from_selection(bContext *C,
                                           wmOperator *op,
                                           blender::Span<Sequence *> strips,
                                           const int timeline_frame)
{
  bool inserted = false;

  for (Sequence *seq : strips) {
    inserted |= retiming_key_add_new_for_seq(C, op, seq, timeline_frame);
  }

  return inserted ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static int retiming_key_add_to_editable_strips(bContext *C,
                                               wmOperator *op,
                                               const int timeline_frame)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene);
  bool inserted = false;

  blender::Map selection = SEQ_retiming_selection_get(ed);
  if (selection.is_empty()) {
    return OPERATOR_CANCELLED;
  }

  for (Sequence *seq : selection.values()) {
    inserted |= retiming_key_add_new_for_seq(C, op, seq, timeline_frame);
  }

  return inserted ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static int sequencer_retiming_key_add_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);

  float timeline_frame;
  if (RNA_struct_property_is_set(op->ptr, "timeline_frame")) {
    timeline_frame = RNA_int_get(op->ptr, "timeline_frame");
  }
  else {
    timeline_frame = BKE_scene_frame_get(scene);
  }

  int ret_val;
  blender::VectorSet<Sequence *> strips = ED_sequencer_selected_strips_from_context(C);
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

  /* api callbacks */
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

static bool freeze_frame_add_new_for_seq(const bContext *C,
                                         const wmOperator *op,
                                         Sequence *seq,
                                         const int timeline_frame,
                                         const int duration)
{
  Scene *scene = CTX_data_scene(C);
  SEQ_retiming_data_ensure(seq);
  // ensure L+R key
  SeqRetimingKey *key = SEQ_retiming_add_key(scene, seq, timeline_frame);

  if (key == nullptr) {
    key = SEQ_retiming_key_get_by_timeline_frame(scene, seq, timeline_frame);
  }

  if (SEQ_retiming_key_is_transition_start(key)) {
    BKE_report(op->reports, RPT_WARNING, "Cannot create key inside of speed transition");
    return false;
  }
  if (key == nullptr) {
    BKE_report(op->reports, RPT_WARNING, "Cannot create freeze frame");
    return false;
  }

  SeqRetimingKey *freeze = SEQ_retiming_add_freeze_frame(scene, seq, key, duration);

  if (freeze == nullptr) {
    BKE_report(op->reports, RPT_WARNING, "Cannot create freeze frame");
    return false;
  }

  ED_sequencer_deselect_all(scene);
  SEQ_retiming_selection_append(freeze);

  SEQ_relations_invalidate_cache_raw(scene, seq);
  return true;
}

static bool freeze_frame_add_from_strip_selection(bContext *C,
                                                  const wmOperator *op,
                                                  const int duration)
{
  Scene *scene = CTX_data_scene(C);
  blender::VectorSet<Sequence *> strips = ED_sequencer_selected_strips_from_context(C);
  const int timeline_frame = BKE_scene_frame_get(scene);
  bool success = false;

  for (Sequence *seq : strips) {
    success |= freeze_frame_add_new_for_seq(C, op, seq, timeline_frame, duration);
    SEQ_relations_invalidate_cache_raw(scene, seq);
  }
  return success;
}

static bool freeze_frame_add_from_retiming_selection(const bContext *C,
                                                     const wmOperator *op,
                                                     const int duration)
{
  Scene *scene = CTX_data_scene(C);
  bool success = false;

  for (auto item : SEQ_retiming_selection_get(SEQ_editing_get(scene)).items()) {
    const int timeline_frame = SEQ_retiming_key_timeline_frame_get(scene, item.value, item.key);
    success |= freeze_frame_add_new_for_seq(C, op, item.value, timeline_frame, duration);
    SEQ_relations_invalidate_cache_raw(scene, item.value);
  }
  return success;
}

static int sequencer_retiming_freeze_frame_add_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
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

  /* api callbacks */
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

static bool transition_add_new_for_seq(const bContext *C,
                                       const wmOperator *op,
                                       Sequence *seq,
                                       const int timeline_frame,
                                       const int duration)
{
  Scene *scene = CTX_data_scene(C);

  // ensure L+R key
  SeqRetimingKey *key = SEQ_retiming_add_key(scene, seq, timeline_frame);

  if (key == nullptr) {
    key = SEQ_retiming_key_get_by_timeline_frame(scene, seq, timeline_frame);
  }

  if (SEQ_retiming_is_last_key(seq, key) || key->strip_frame_index == 0) {
    BKE_report(op->reports, RPT_WARNING, "Cannot create transition from first or last key");
    return false;
  }

  SeqRetimingKey *transition = SEQ_retiming_add_transition(scene, seq, key, duration);

  if (transition == nullptr) {
    BKE_report(op->reports, RPT_WARNING, "Cannot create transition");
    return false;
  }

  ED_sequencer_deselect_all(scene);
  SEQ_retiming_selection_append(transition);

  SEQ_relations_invalidate_cache_raw(scene, seq);
  return true;
}

static bool transition_add_from_retiming_selection(const bContext *C,
                                                   const wmOperator *op,
                                                   const int duration)
{
  Scene *scene = CTX_data_scene(C);
  bool success = false;

  for (auto item : SEQ_retiming_selection_get(SEQ_editing_get(scene)).items()) {
    const int timeline_frame = SEQ_retiming_key_timeline_frame_get(scene, item.value, item.key);
    success |= transition_add_new_for_seq(C, op, item.value, timeline_frame, duration);
  }
  return success;
}

static int sequencer_retiming_transition_add_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
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
    return false;
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

  /* api callbacks */
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
/** \name Retiming Set Segment Speed
 * \{ */

static SeqRetimingKey *ensure_left_and_right_keys(const bContext *C, Sequence *seq)
{
  Scene *scene = CTX_data_scene(C);
  SEQ_retiming_data_ensure(seq);
  SEQ_retiming_add_key(scene, seq, left_fake_key_frame_get(C, seq));
  return SEQ_retiming_add_key(scene, seq, right_fake_key_frame_get(C, seq));
}

/* Return speed of existing segment or strip. Assume 1 element is selected. */
static float strip_speed_get(bContext *C, const wmOperator * /* op */)
{
  /* Strip mode. */
  if (!sequencer_retiming_mode_is_active(C)) {
    blender::VectorSet<Sequence *> strips = ED_sequencer_selected_strips_from_context(C);
    if (strips.size() == 1) {
      Sequence *seq = strips[0];
      SeqRetimingKey *key = ensure_left_and_right_keys(C, seq);
      return SEQ_retiming_key_speed_get(seq, key);
    }
  }

  Scene *scene = CTX_data_scene(C);
  blender::Map selection = SEQ_retiming_selection_get(SEQ_editing_get(scene));
  /* Retiming mode. */
  if (selection.size() == 1) {
    for (auto item : selection.items()) {
      return SEQ_retiming_key_speed_get(item.value, item.key);
    }
  }

  return 1.0f;
}

static int strip_speed_set_exec(bContext *C, const wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  blender::VectorSet<Sequence *> strips = ED_sequencer_selected_strips_from_context(C);

  for (Sequence *seq : strips) {
    SeqRetimingKey *key = ensure_left_and_right_keys(C, seq);

    if (key == nullptr) {
      continue;
    }
    /* TODO: it would be nice to multiply speed with complex retiming by a factor. */
    SEQ_retiming_key_speed_set(scene, seq, key, RNA_float_get(op->ptr, "speed"), false);
    SEQ_relations_invalidate_cache_raw(scene, seq);
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
  return OPERATOR_FINISHED;
}

static int segment_speed_set_exec(const bContext *C,
                                  const wmOperator *op,
                                  blender::Map<SeqRetimingKey *, Sequence *> selection)
{
  Scene *scene = CTX_data_scene(C);
  ListBase *seqbase = SEQ_active_seqbase_get(SEQ_editing_get(scene));

  for (auto item : selection.items()) {
    SEQ_retiming_key_speed_set(scene,
                               item.value,
                               item.key,
                               RNA_float_get(op->ptr, "speed"),
                               RNA_boolean_get(op->ptr, "keep_retiming"));

    if (SEQ_transform_test_overlap(scene, seqbase, item.value)) {
      SEQ_transform_seqbase_shuffle(seqbase, item.value, scene);
    }

    SEQ_relations_invalidate_cache_raw(scene, item.value);
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
  return OPERATOR_FINISHED;
}

static int sequencer_retiming_segment_speed_set_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);

  /* Strip mode. */
  if (!sequencer_retiming_mode_is_active(C)) {
    return strip_speed_set_exec(C, op);
  }

  blender::Map selection = SEQ_retiming_selection_get(SEQ_editing_get(scene));

  /* Retiming mode. */
  if (selection.size() > 0) {
    return segment_speed_set_exec(C, op, selection);
  }

  BKE_report(op->reports, RPT_ERROR, "No keys or strips selected");
  return OPERATOR_CANCELLED;
}

static int sequencer_retiming_segment_speed_set_invoke(bContext *C,
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

  /* api callbacks */
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
    changed = SEQ_retiming_selection_clear(ed);
  }

  if (key == nullptr) {
    return changed;
  }

  if (toggle && SEQ_retiming_selection_contains(ed, key)) {
    SEQ_retiming_selection_remove(key);
  }
  else {
    SEQ_retiming_selection_append(key);
  }

  return true;
}

int sequencer_retiming_select_linked_time(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene);
  const int mval[2] = {RNA_int_get(op->ptr, "mouse_x"), RNA_int_get(op->ptr, "mouse_y")};

  Sequence *seq_key_owner = nullptr;
  SeqRetimingKey *key = retiming_mousover_key_get(C, mval, &seq_key_owner);

  if (key == nullptr) {
    return OPERATOR_CANCELLED;
  }
  if (!RNA_boolean_get(op->ptr, "extend")) {
    SEQ_retiming_selection_clear(ed);
  }
  for (; key <= SEQ_retiming_last_key_get(seq_key_owner); key++) {
    select_key(ed, key, false, false);
  }
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
  return OPERATOR_FINISHED;
}

int sequencer_retiming_key_select_exec(bContext *C, wmOperator *op)
{
  if (RNA_boolean_get(op->ptr, "linked_time")) {
    return sequencer_retiming_select_linked_time(C, op);
  }

  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene);
  const int mval[2] = {RNA_int_get(op->ptr, "mouse_x"), RNA_int_get(op->ptr, "mouse_y")};

  eSeqHandle hand;
  Sequence *seq_key_owner = nullptr;
  SeqRetimingKey *key = retiming_mousover_key_get(C, mval, &seq_key_owner);

  /* Try to realize "fake" key, since it is clicked on. */
  if (key == nullptr && seq_key_owner != nullptr) {
    key = try_to_realize_virtual_keys(C, seq_key_owner, mval);
  }

  const bool deselect_all = RNA_boolean_get(op->ptr, "deselect_all");
  const bool wait_to_deselect_others = RNA_boolean_get(op->ptr, "wait_to_deselect_others");
  const bool toggle = RNA_boolean_get(op->ptr, "toggle");

  /* Click on unselected key. */
  if (key != nullptr && !SEQ_retiming_selection_contains(ed, key) && !toggle) {
    select_key(ed, key, false, deselect_all);
  }

  /* Clicked on any key, waiting to click release. */
  if (key != nullptr && wait_to_deselect_others && !toggle) {
    return OPERATOR_RUNNING_MODAL;
  }

  /* Click on strip, do strip selection. */
  const Sequence *seq_click_exact = find_nearest_seq(scene, UI_view2d_fromcontext(C), mval, &hand);
  if (seq_click_exact != nullptr && key == nullptr) {
    SEQ_retiming_selection_clear(ed);
    return sequencer_select_exec(C, op);
  }

  /* Selection after click is released. */
  const bool changed = select_key(ed, key, toggle, deselect_all);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
  return changed ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static void realize_fake_keys_in_rect(bContext *C, Sequence *seq, const rctf &rectf)
{
  const Scene *scene = CTX_data_scene(C);

  const int content_start = SEQ_time_start_frame_get(seq);
  const int left_key_frame = max_ii(content_start, SEQ_time_left_handle_frame_get(scene, seq));
  const int content_end = SEQ_time_content_end_frame_get(scene, seq);
  const int right_key_frame = min_ii(content_end, SEQ_time_right_handle_frame_get(scene, seq));

  /* Realize "fake" keys. */
  if (left_key_frame > rectf.xmin && left_key_frame < rectf.xmax) {
    SEQ_retiming_add_key(scene, seq, left_key_frame);
  }
  if (right_key_frame > rectf.xmin && right_key_frame < rectf.xmax) {
    SEQ_retiming_add_key(scene, seq, right_key_frame);
  }
}

int sequencer_retiming_box_select_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  const View2D *v2d = UI_view2d_fromcontext(C);
  Editing *ed = SEQ_editing_get(scene);

  if (ed == nullptr) {
    return OPERATOR_CANCELLED;
  }

  const eSelectOp sel_op = eSelectOp(RNA_enum_get(op->ptr, "mode"));
  bool changed = false;

  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    changed |= SEQ_retiming_selection_clear(ed);
  }

  rctf rectf;
  WM_operator_properties_border_to_rctf(op, &rectf);
  UI_view2d_region_to_view_rctf(v2d, &rectf, &rectf);

  blender::Set<SeqRetimingKey *> and_keys;

  for (Sequence *seq : sequencer_visible_strips_get(C)) {
    if (seq->machine < rectf.ymin || seq->machine > rectf.ymax) {
      continue;
    }
    if (!SEQ_retiming_data_is_editable(seq)) {
      continue;
    }
    realize_fake_keys_in_rect(C, seq, rectf);

    for (SeqRetimingKey &key : SEQ_retiming_keys_get(seq)) {
      const int key_frame = SEQ_retiming_key_timeline_frame_get(scene, seq, &key);
      const int strip_start = SEQ_time_left_handle_frame_get(scene, seq);
      const int strip_end = SEQ_time_right_handle_frame_get(scene, seq);
      if (key_frame < strip_start || key_frame > strip_end) {
        continue;
      }
      if (key_frame > rectf.xmax || key_frame < rectf.xmin) {
        continue;
      }

      switch (sel_op) {
        case SEL_OP_ADD:
        case SEL_OP_SET: {
          SEQ_retiming_selection_append(&key);
          break;
        }
        case SEL_OP_SUB: {
          SEQ_retiming_selection_remove(&key);
          break;
        }
        case SEL_OP_XOR: { /* Toggle */
          if (SEQ_retiming_selection_contains(ed, &key)) {
            SEQ_retiming_selection_remove(&key);
          }
          else {
            SEQ_retiming_selection_append(&key);
          }
          break;
          case SEL_OP_AND: {
            if (SEQ_retiming_selection_contains(ed, &key)) {
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
    SEQ_retiming_selection_clear(ed);
    for (auto key : and_keys) {
      SEQ_retiming_selection_append(key);
    }
  }

  return changed ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

int sequencer_retiming_select_all_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  int action = RNA_enum_get(op->ptr, "action");

  blender::VectorSet<Sequence *> strips = all_strips_from_context(C);

  if (action == SEL_TOGGLE) {
    action = SEL_SELECT;
    for (Sequence *seq : strips) {
      if (!SEQ_retiming_data_is_editable(seq)) {
        continue;
      }
      for (SeqRetimingKey &key : SEQ_retiming_keys_get(seq)) {
        if (key.flag & SEQ_KEY_SELECTED) {
          action = SEL_DESELECT;
          break;
        }
      }
    }
  }

  if (action == SEL_DESELECT) {
    SEQ_retiming_selection_clear(SEQ_editing_get(scene));
  }

  for (Sequence *seq : strips) {
    if (!SEQ_retiming_data_is_editable(seq)) {
      continue;
    }
    for (SeqRetimingKey &key : SEQ_retiming_keys_get(seq)) {
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

int sequencer_retiming_key_remove_exec(bContext *C, wmOperator * /* op */)
{
  Scene *scene = CTX_data_scene(C);

  blender::Map selection = SEQ_retiming_selection_get(SEQ_editing_get(scene));
  blender::Vector<Sequence *> strips_to_handle;

  for (Sequence *seq : selection.values()) {
    strips_to_handle.append_non_duplicates(seq);
  }

  for (Sequence *seq : strips_to_handle) {
    blender::Vector<SeqRetimingKey *> keys_to_delete;
    for (auto item : selection.items()) {
      if (item.value != seq) {
        continue;
      }
      keys_to_delete.append(item.key);
    }

    SEQ_retiming_remove_multiple_keys(seq, keys_to_delete);
  }

  for (Sequence *seq : strips_to_handle) {
    SEQ_relations_invalidate_cache_raw(scene, seq);
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
  return OPERATOR_FINISHED;
}
