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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup spseq
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_timecode.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_scene_types.h"
#include "DNA_sound_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_sound.h"

#include "SEQ_add.h"
#include "SEQ_clipboard.h"
#include "SEQ_edit.h"
#include "SEQ_effects.h"
#include "SEQ_iterator.h"
#include "SEQ_prefetch.h"
#include "SEQ_relations.h"
#include "SEQ_render.h"
#include "SEQ_select.h"
#include "SEQ_sequencer.h"
#include "SEQ_time.h"
#include "SEQ_transform.h"
#include "SEQ_utils.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_define.h"

/* For menu, popup, icons, etc. */
#include "ED_numinput.h"
#include "ED_outliner.h"
#include "ED_screen.h"
#include "ED_sequencer.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

/* Own include. */
#include "sequencer_intern.h"

/* -------------------------------------------------------------------- */
/** \name Structs & Enums
 * \{ */

typedef struct TransSeq {
  int start, machine;
  int startstill, endstill;
  int startdisp, enddisp;
  int startofs, endofs;
  int anim_startofs, anim_endofs;
  /* int final_left, final_right; */ /* UNUSED */
  int len;
} TransSeq;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public Context Checks
 * \{ */

bool ED_space_sequencer_maskedit_mask_poll(bContext *C)
{
  return ED_space_sequencer_maskedit_poll(C);
}

bool ED_space_sequencer_check_show_maskedit(SpaceSeq *sseq, Scene *scene)
{
  if (sseq && sseq->mainb == SEQ_DRAW_IMG_IMBUF) {
    return (SEQ_active_mask_get(scene) != NULL);
  }

  return false;
}

bool ED_space_sequencer_maskedit_poll(bContext *C)
{
  SpaceSeq *sseq = CTX_wm_space_seq(C);

  if (sseq) {
    Scene *scene = CTX_data_scene(C);
    return ED_space_sequencer_check_show_maskedit(sseq, scene);
  }

  return false;
}

/* Are we displaying the seq output (not channels or histogram). */
bool ED_space_sequencer_check_show_imbuf(SpaceSeq *sseq)
{
  return (ELEM(sseq->view, SEQ_VIEW_PREVIEW, SEQ_VIEW_SEQUENCE_PREVIEW) &&
          ELEM(sseq->mainb, SEQ_DRAW_SEQUENCE, SEQ_DRAW_IMG_IMBUF));
}

bool ED_space_sequencer_check_show_strip(SpaceSeq *sseq)
{
  return (ELEM(sseq->view, SEQ_VIEW_SEQUENCE, SEQ_VIEW_SEQUENCE_PREVIEW) &&
          ELEM(sseq->mainb, SEQ_DRAW_SEQUENCE, SEQ_DRAW_IMG_IMBUF));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shared Poll Functions
 * \{ */

/* Operator functions. */
bool sequencer_edit_poll(bContext *C)
{
  return (SEQ_editing_get(CTX_data_scene(C), false) != NULL);
}

#if 0 /* UNUSED */
bool sequencer_strip_poll(bContext *C)
{
  Editing *ed;
  return (((ed = SEQ_editing_get(CTX_data_scene(C), false)) != NULL) &&
          (ed->act_seq != NULL));
}
#endif

bool sequencer_strip_has_path_poll(bContext *C)
{
  Editing *ed;
  Sequence *seq;
  return (((ed = SEQ_editing_get(CTX_data_scene(C), false)) != NULL) &&
          ((seq = ed->act_seq) != NULL) && (SEQ_HAS_PATH(seq)));
}

bool sequencer_view_preview_poll(bContext *C)
{
  SpaceSeq *sseq = CTX_wm_space_seq(C);
  Editing *ed = SEQ_editing_get(CTX_data_scene(C), false);
  if (ed && sseq && (sseq->mainb == SEQ_DRAW_IMG_IMBUF)) {
    return 1;
  }

  return 0;
}

bool sequencer_view_strips_poll(bContext *C)
{
  SpaceSeq *sseq = CTX_wm_space_seq(C);
  if (sseq && ED_space_sequencer_check_show_strip(sseq)) {
    return 1;
  }

  return 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Remove Gaps Operator
 * \{ */

static int sequencer_gap_remove_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  const bool do_all = RNA_boolean_get(op->ptr, "all");
  const Editing *ed = SEQ_editing_get(scene, false);

  SEQ_edit_remove_gaps(scene, ed->seqbasep, CFRA, do_all);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_gap_remove(struct wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Remove Gaps";
  ot->idname = "SEQUENCER_OT_gap_remove";
  ot->description =
      "Remove gap at current frame to first strip at the right, independent of selection or "
      "locked state of strips";

  /* Api callbacks. */
  //  ot->invoke = sequencer_snap_invoke;
  ot->exec = sequencer_gap_remove_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "all", 0, "All Gaps", "Do all gaps to right of current frame");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Insert Gaps Operator
 * \{ */

static int sequencer_gap_insert_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  const int frames = RNA_int_get(op->ptr, "frames");
  const Editing *ed = SEQ_editing_get(scene, false);
  SEQ_transform_offset_after_frame(scene, ed->seqbasep, frames, CFRA);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_gap_insert(struct wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Insert Gaps";
  ot->idname = "SEQUENCER_OT_gap_insert";
  ot->description =
      "Insert gap at current frame to first strips at the right, independent of selection or "
      "locked state of strips";

  /* Api callbacks. */
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

static int sequencer_snap_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);

  Editing *ed = SEQ_editing_get(scene, false);
  Sequence *seq;
  int snap_frame;

  snap_frame = RNA_int_get(op->ptr, "frame");

  /* Check metas. */
  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    if (seq->flag & SELECT && !(seq->depth == 0 && seq->flag & SEQ_LOCK) &&
        SEQ_transform_sequence_can_be_translated(seq)) {
      if ((seq->flag & (SEQ_LEFTSEL + SEQ_RIGHTSEL)) == 0) {
        SEQ_transform_translate_sequence(
            scene, seq, (snap_frame - seq->startofs + seq->startstill) - seq->start);
      }
      else {
        if (seq->flag & SEQ_LEFTSEL) {
          SEQ_transform_set_left_handle_frame(seq, snap_frame);
        }
        else { /* SEQ_RIGHTSEL */
          SEQ_transform_set_right_handle_frame(seq, snap_frame);
        }
        SEQ_transform_handle_xlimits(seq, seq->flag & SEQ_LEFTSEL, seq->flag & SEQ_RIGHTSEL);
        SEQ_transform_fix_single_image_seq_offsets(seq);
      }
      SEQ_time_update_sequence(scene, seq);
    }
  }

  /* Test for effects and overlap.
   * Don't use SEQ_CURRENT_BEGIN since that would be recursive. */
  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    if (seq->flag & SELECT && !(seq->depth == 0 && seq->flag & SEQ_LOCK)) {
      seq->flag &= ~SEQ_OVERLAP;
      if (SEQ_transform_test_overlap(ed->seqbasep, seq)) {
        SEQ_transform_seqbase_shuffle(ed->seqbasep, seq, scene);
      }
    }
  }

  /* Recalculate bounds of effect strips, offsetting the keyframes if not snapping any handle. */
  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    if (seq->type & SEQ_TYPE_EFFECT) {
      const bool either_handle_selected = (seq->flag & (SEQ_LEFTSEL | SEQ_RIGHTSEL)) != 0;

      if (seq->seq1 && (seq->seq1->flag & SELECT)) {
        if (!either_handle_selected) {
          SEQ_offset_animdata(scene, seq, (snap_frame - seq->startdisp));
        }
        SEQ_time_update_sequence(scene, seq);
      }
      else if (seq->seq2 && (seq->seq2->flag & SELECT)) {
        if (!either_handle_selected) {
          SEQ_offset_animdata(scene, seq, (snap_frame - seq->startdisp));
        }
        SEQ_time_update_sequence(scene, seq);
      }
      else if (seq->seq3 && (seq->seq3->flag & SELECT)) {
        if (!either_handle_selected) {
          SEQ_offset_animdata(scene, seq, (snap_frame - seq->startdisp));
        }
        SEQ_time_update_sequence(scene, seq);
      }
    }
  }

  SEQ_sort(scene);

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

static int sequencer_snap_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  Scene *scene = CTX_data_scene(C);

  int snap_frame;

  snap_frame = CFRA;

  RNA_int_set(op->ptr, "frame", snap_frame);
  return sequencer_snap_exec(C, op);
}

void SEQUENCER_OT_snap(struct wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Snap Strips to the Current Frame";
  ot->idname = "SEQUENCER_OT_snap";
  ot->description = "Frame where selected strips will be snapped";

  /* Api callbacks. */
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
/** \name Trim Strips Operator
 * \{ */

typedef struct SlipData {
  int init_mouse[2];
  float init_mouseloc[2];
  TransSeq *ts;
  Sequence **seq_array;
  bool *trim;
  int num_seq;
  bool slow;
  int slow_offset; /* Offset at the point where offset was turned on. */
  NumInput num_input;
} SlipData;

static void transseq_backup(TransSeq *ts, Sequence *seq)
{
  ts->start = seq->start;
  ts->machine = seq->machine;
  ts->startstill = seq->startstill;
  ts->endstill = seq->endstill;
  ts->startdisp = seq->startdisp;
  ts->enddisp = seq->enddisp;
  ts->startofs = seq->startofs;
  ts->endofs = seq->endofs;
  ts->anim_startofs = seq->anim_startofs;
  ts->anim_endofs = seq->anim_endofs;
  ts->len = seq->len;
}

static void transseq_restore(TransSeq *ts, Sequence *seq)
{
  seq->start = ts->start;
  seq->machine = ts->machine;
  seq->startstill = ts->startstill;
  seq->endstill = ts->endstill;
  seq->startdisp = ts->startdisp;
  seq->enddisp = ts->enddisp;
  seq->startofs = ts->startofs;
  seq->endofs = ts->endofs;
  seq->anim_startofs = ts->anim_startofs;
  seq->anim_endofs = ts->anim_endofs;
  seq->len = ts->len;
}

static int slip_add_sequences_recursive(
    ListBase *seqbasep, Sequence **seq_array, bool *trim, int offset, bool do_trim)
{
  Sequence *seq;
  int num_items = 0;

  for (seq = seqbasep->first; seq; seq = seq->next) {
    if (!do_trim || (!(seq->type & SEQ_TYPE_EFFECT) && (seq->flag & SELECT))) {
      seq_array[offset + num_items] = seq;
      trim[offset + num_items] = do_trim && ((seq->type & SEQ_TYPE_EFFECT) == 0);
      num_items++;

      if (seq->type == SEQ_TYPE_META) {
        /* Trim the sub-sequences. */
        num_items += slip_add_sequences_recursive(
            &seq->seqbase, seq_array, trim, num_items + offset, false);
      }
    }
  }

  return num_items;
}

static int slip_count_sequences_recursive(ListBase *seqbasep, bool first_level)
{
  Sequence *seq;
  int trimmed_sequences = 0;

  for (seq = seqbasep->first; seq; seq = seq->next) {
    if (!first_level || (!(seq->type & SEQ_TYPE_EFFECT) && (seq->flag & SELECT))) {
      trimmed_sequences++;

      if (seq->type == SEQ_TYPE_META) {
        /* Trim the sub-sequences. */
        trimmed_sequences += slip_count_sequences_recursive(&seq->seqbase, false);
      }
    }
  }

  return trimmed_sequences;
}

static int sequencer_slip_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  SlipData *data;
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene, false);
  float mouseloc[2];
  int num_seq;
  View2D *v2d = UI_view2d_fromcontext(C);

  /* Recursively count the trimmed elements. */
  num_seq = slip_count_sequences_recursive(ed->seqbasep, true);

  if (num_seq == 0) {
    return OPERATOR_CANCELLED;
  }

  data = op->customdata = MEM_mallocN(sizeof(SlipData), "trimdata");
  data->ts = MEM_mallocN(num_seq * sizeof(TransSeq), "trimdata_transform");
  data->seq_array = MEM_mallocN(num_seq * sizeof(Sequence *), "trimdata_sequences");
  data->trim = MEM_mallocN(num_seq * sizeof(bool), "trimdata_trim");
  data->num_seq = num_seq;

  initNumInput(&data->num_input);
  data->num_input.idx_max = 0;
  data->num_input.val_flag[0] |= NUM_NO_FRACTION;
  data->num_input.unit_sys = USER_UNIT_NONE;
  data->num_input.unit_type[0] = 0;

  slip_add_sequences_recursive(ed->seqbasep, data->seq_array, data->trim, 0, true);

  for (int i = 0; i < num_seq; i++) {
    transseq_backup(data->ts + i, data->seq_array[i]);
  }

  UI_view2d_region_to_view(v2d, event->mval[0], event->mval[1], &mouseloc[0], &mouseloc[1]);

  copy_v2_v2_int(data->init_mouse, event->mval);
  copy_v2_v2(data->init_mouseloc, mouseloc);

  data->slow = false;

  WM_event_add_modal_handler(C, op);

  /* Notify so we draw extensions immediately. */
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_RUNNING_MODAL;
}

static bool sequencer_slip_recursively(Scene *scene, SlipData *data, int offset)
{
  /* Only data types supported for now. */
  Editing *ed = SEQ_editing_get(scene, false);
  bool changed = false;

  /* Iterate in reverse so meta-strips are iterated after their children. */
  for (int i = data->num_seq - 1; i >= 0; i--) {
    Sequence *seq = data->seq_array[i];
    int endframe;

    /* Offset seq start. */
    seq->start = data->ts[i].start + offset;

    if (data->trim[i]) {
      /* Find the end-frame. */
      endframe = seq->start + seq->len;

      /* Compute the sequence offsets. */
      if (endframe > seq->enddisp) {
        seq->endstill = 0;
        seq->endofs = endframe - seq->enddisp;
        changed = true;
      }
      else if (endframe <= seq->enddisp) {
        seq->endstill = seq->enddisp - endframe;
        seq->endofs = 0;
        changed = true;
      }

      if (seq->start > seq->startdisp) {
        seq->startstill = seq->start - seq->startdisp;
        seq->startofs = 0;
        changed = true;
      }
      else if (seq->start <= seq->startdisp) {
        seq->startstill = 0;
        seq->startofs = seq->startdisp - seq->start;
        changed = true;
      }
    }
    else {
      /* No transform data (likely effect strip). Only move start and end. */
      seq->startdisp = data->ts[i].startdisp + offset;
      seq->enddisp = data->ts[i].enddisp + offset;
      changed = true;
    }

    /* Effects are only added if we they are in a meta-strip.
     * In this case, dependent strips will just be transformed and
     * we can skip calculating for effects.
     * This way we can avoid an extra loop just for effects. */
    if (!(seq->type & SEQ_TYPE_EFFECT)) {
      SEQ_time_update_sequence(scene, seq);
    }
  }
  if (changed) {
    SEQ_relations_free_imbuf(scene, &ed->seqbase, false);
  }
  return changed;
}

/* Make sure, that each strip contains at least 1 frame of content. */
static void sequencer_slip_apply_limits(SlipData *data, int *offset)
{
  for (int i = 0; i < data->num_seq; i++) {
    if (data->trim[i]) {
      Sequence *seq = data->seq_array[i];
      int seq_content_start = data->ts[i].start + *offset;
      int seq_content_end = seq_content_start + seq->len + seq->anim_startofs + seq->anim_endofs;
      int diff = 0;

      if (seq_content_start >= seq->enddisp) {
        diff = seq->enddisp - seq_content_start - 1;
      }

      if (seq_content_end <= seq->startdisp) {
        diff = seq->startdisp - seq_content_end + 1;
      }
      *offset += diff;
    }
  }
}

static int sequencer_slip_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene, false);
  int offset = RNA_int_get(op->ptr, "offset");
  bool success = false;

  /* Recursively count the trimmed elements. */
  int num_seq = slip_count_sequences_recursive(ed->seqbasep, true);

  if (num_seq == 0) {
    return OPERATOR_CANCELLED;
  }

  SlipData *data = op->customdata = MEM_mallocN(sizeof(SlipData), "trimdata");
  data->ts = MEM_mallocN(num_seq * sizeof(TransSeq), "trimdata_transform");
  data->seq_array = MEM_mallocN(num_seq * sizeof(Sequence *), "trimdata_sequences");
  data->trim = MEM_mallocN(num_seq * sizeof(bool), "trimdata_trim");
  data->num_seq = num_seq;

  slip_add_sequences_recursive(ed->seqbasep, data->seq_array, data->trim, 0, true);

  for (int i = 0; i < num_seq; i++) {
    transseq_backup(data->ts + i, data->seq_array[i]);
  }

  sequencer_slip_apply_limits(data, &offset);
  success = sequencer_slip_recursively(scene, data, offset);

  MEM_freeN(data->seq_array);
  MEM_freeN(data->trim);
  MEM_freeN(data->ts);
  MEM_freeN(data);

  if (success) {
    WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
    DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

static void sequencer_slip_update_header(Scene *scene, ScrArea *area, SlipData *data, int offset)
{
  char msg[UI_MAX_DRAW_STR];

  if (area) {
    if (hasNumInput(&data->num_input)) {
      char num_str[NUM_STR_REP_LEN];
      outputNumInput(&data->num_input, num_str, &scene->unit);
      BLI_snprintf(msg, sizeof(msg), TIP_("Slip offset: %s"), num_str);
    }
    else {
      BLI_snprintf(msg, sizeof(msg), TIP_("Slip offset: %d"), offset);
    }
  }

  ED_area_status_text(area, msg);
}

static int sequencer_slip_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  SlipData *data = (SlipData *)op->customdata;
  ScrArea *area = CTX_wm_area(C);
  const bool has_numInput = hasNumInput(&data->num_input);
  bool handled = true;

  /* Modal numinput active, try to handle numeric inputs. */
  if (event->val == KM_PRESS && has_numInput && handleNumInput(C, &data->num_input, event)) {
    float offset_fl;
    applyNumInput(&data->num_input, &offset_fl);
    int offset = round_fl_to_int(offset_fl);

    sequencer_slip_apply_limits(data, &offset);
    sequencer_slip_update_header(scene, area, data, offset);

    RNA_int_set(op->ptr, "offset", offset);

    if (sequencer_slip_recursively(scene, data, offset)) {
      WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
    }

    return OPERATOR_RUNNING_MODAL;
  }

  switch (event->type) {
    case MOUSEMOVE: {
      if (!has_numInput) {
        float mouseloc[2];
        int offset;
        int mouse_x;
        View2D *v2d = UI_view2d_fromcontext(C);

        if (data->slow) {
          mouse_x = event->mval[0] - data->slow_offset;
          mouse_x *= 0.1f;
          mouse_x += data->slow_offset;
        }
        else {
          mouse_x = event->mval[0];
        }

        /* Choose the side based on which side of the current frame the mouse is. */
        UI_view2d_region_to_view(v2d, mouse_x, 0, &mouseloc[0], &mouseloc[1]);
        offset = mouseloc[0] - data->init_mouseloc[0];

        sequencer_slip_apply_limits(data, &offset);
        sequencer_slip_update_header(scene, area, data, offset);

        RNA_int_set(op->ptr, "offset", offset);

        if (sequencer_slip_recursively(scene, data, offset)) {
          WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
        }
      }
      break;
    }

    case LEFTMOUSE:
    case EVT_RETKEY:
    case EVT_SPACEKEY: {
      MEM_freeN(data->seq_array);
      MEM_freeN(data->trim);
      MEM_freeN(data->ts);
      MEM_freeN(data);
      op->customdata = NULL;
      if (area) {
        ED_area_status_text(area, NULL);
      }
      DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
      WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
      return OPERATOR_FINISHED;
    }

    case EVT_ESCKEY:
    case RIGHTMOUSE: {
      Editing *ed = SEQ_editing_get(scene, false);

      for (int i = 0; i < data->num_seq; i++) {
        transseq_restore(data->ts + i, data->seq_array[i]);
      }

      for (int i = 0; i < data->num_seq; i++) {
        Sequence *seq = data->seq_array[i];
        SEQ_add_reload_new_file(bmain, scene, seq, false);
        SEQ_time_update_sequence(scene, seq);
      }

      MEM_freeN(data->seq_array);
      MEM_freeN(data->ts);
      MEM_freeN(data->trim);
      MEM_freeN(data);
      op->customdata = NULL;

      WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

      SEQ_relations_free_imbuf(scene, &ed->seqbase, false);

      if (area) {
        ED_area_status_text(area, NULL);
      }

      return OPERATOR_CANCELLED;
    }

    case EVT_RIGHTSHIFTKEY:
    case EVT_LEFTSHIFTKEY:
      if (!has_numInput) {
        if (event->val == KM_PRESS) {
          data->slow = true;
          data->slow_offset = event->mval[0];
        }
        else if (event->val == KM_RELEASE) {
          data->slow = false;
        }
      }
      break;

    default:
      handled = false;
      break;
  }

  /* Modal numinput inactive, try to handle numeric inputs. */
  if (!handled && event->val == KM_PRESS && handleNumInput(C, &data->num_input, event)) {
    float offset_fl;
    applyNumInput(&data->num_input, &offset_fl);
    int offset = round_fl_to_int(offset_fl);

    sequencer_slip_apply_limits(data, &offset);
    sequencer_slip_update_header(scene, area, data, offset);

    RNA_int_set(op->ptr, "offset", offset);

    if (sequencer_slip_recursively(scene, data, offset)) {
      WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

void SEQUENCER_OT_slip(struct wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Trim Strips";
  ot->idname = "SEQUENCER_OT_slip";
  ot->description = "Trim the contents of the active strip";

  /* Api callbacks. */
  ot->invoke = sequencer_slip_invoke;
  ot->modal = sequencer_slip_modal;
  ot->exec = sequencer_slip_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_int(ot->srna,
              "offset",
              0,
              INT32_MIN,
              INT32_MAX,
              "Offset",
              "Offset to the data of the strip",
              INT32_MIN,
              INT32_MAX);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mute Strips Operator
 * \{ */

static int sequencer_mute_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene, false);
  Sequence *seq;
  bool selected;

  selected = !RNA_boolean_get(op->ptr, "unselected");

  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    if ((seq->flag & SEQ_LOCK) == 0) {
      if (selected) {
        if (seq->flag & SELECT) {
          seq->flag |= SEQ_MUTE;
          SEQ_relations_invalidate_dependent(scene, seq);
        }
      }
      else {
        if ((seq->flag & SELECT) == 0) {
          seq->flag |= SEQ_MUTE;
          SEQ_relations_invalidate_dependent(scene, seq);
        }
      }
    }
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_mute(struct wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Mute Strips";
  ot->idname = "SEQUENCER_OT_mute";
  ot->description = "Mute (un)selected strips";

  /* Api callbacks. */
  ot->exec = sequencer_mute_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(
      ot->srna, "unselected", 0, "Unselected", "Mute unselected rather than selected strips");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Unmute Strips Operator
 * \{ */

static int sequencer_unmute_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene, false);
  Sequence *seq;
  bool selected;

  selected = !RNA_boolean_get(op->ptr, "unselected");

  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    if ((seq->flag & SEQ_LOCK) == 0) {
      if (selected) {
        if (seq->flag & SELECT) {
          seq->flag &= ~SEQ_MUTE;
          SEQ_relations_invalidate_dependent(scene, seq);
        }
      }
      else {
        if ((seq->flag & SELECT) == 0) {
          seq->flag &= ~SEQ_MUTE;
          SEQ_relations_invalidate_dependent(scene, seq);
        }
      }
    }
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_unmute(struct wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Unmute Strips";
  ot->idname = "SEQUENCER_OT_unmute";
  ot->description = "Unmute (un)selected strips";

  /* Api callbacks. */
  ot->exec = sequencer_unmute_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(
      ot->srna, "unselected", 0, "Unselected", "Unmute unselected rather than selected strips");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lock Strips Operator
 * \{ */

static int sequencer_lock_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene, false);
  Sequence *seq;

  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    if (seq->flag & SELECT) {
      seq->flag |= SEQ_LOCK;
    }
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_lock(struct wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Lock Strips";
  ot->idname = "SEQUENCER_OT_lock";
  ot->description = "Lock strips so they can't be transformed";

  /* Api callbacks. */
  ot->exec = sequencer_lock_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Unlock Strips Operator
 * \{ */

static int sequencer_unlock_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene, false);
  Sequence *seq;

  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    if (seq->flag & SELECT) {
      seq->flag &= ~SEQ_LOCK;
    }
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_unlock(struct wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Unlock Strips";
  ot->idname = "SEQUENCER_OT_unlock";
  ot->description = "Unlock strips so they can be transformed";

  /* Api callbacks. */
  ot->exec = sequencer_unlock_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Reload Strips Operator
 * \{ */

static int sequencer_reload_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene, false);
  Sequence *seq;
  const bool adjust_length = RNA_boolean_get(op->ptr, "adjust_length");

  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    if (seq->flag & SELECT) {
      SEQ_relations_update_changed_seq_and_deps(scene, seq, 0, 1);
      SEQ_add_reload_new_file(bmain, scene, seq, !adjust_length);

      if (adjust_length) {
        if (SEQ_transform_test_overlap(ed->seqbasep, seq)) {
          SEQ_transform_seqbase_shuffle(ed->seqbasep, seq, scene);
        }
      }
    }
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_reload(struct wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers. */
  ot->name = "Reload Strips";
  ot->idname = "SEQUENCER_OT_reload";
  ot->description = "Reload strips in the sequencer";

  /* Api callbacks. */
  ot->exec = sequencer_reload_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER; /* No undo, the data changed is stored outside 'main'. */

  prop = RNA_def_boolean(ot->srna,
                         "adjust_length",
                         0,
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
    return 0;
  }
  return sequencer_edit_poll(C);
}

static int sequencer_refresh_all_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene, false);

  SEQ_relations_free_imbuf(scene, &ed->seqbase, false);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_refresh_all(struct wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Refresh Sequencer";
  ot->idname = "SEQUENCER_OT_refresh_all";
  ot->description = "Refresh the sequencer editor";

  /* Api callbacks. */
  ot->exec = sequencer_refresh_all_exec;
  ot->poll = sequencer_refresh_all_poll;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Reassign Inputs Operator
 * \{ */

int seq_effect_find_selected(Scene *scene,
                             Sequence *activeseq,
                             int type,
                             Sequence **r_selseq1,
                             Sequence **r_selseq2,
                             Sequence **r_selseq3,
                             const char **r_error_str)
{
  Editing *ed = SEQ_editing_get(scene, false);
  Sequence *seq1 = NULL, *seq2 = NULL, *seq3 = NULL, *seq;

  *r_error_str = NULL;

  if (!activeseq) {
    seq2 = SEQ_select_active_get(scene);
  }

  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    if (seq->flag & SELECT) {
      if (seq->type == SEQ_TYPE_SOUND_RAM && SEQ_effect_get_num_inputs(type) != 0) {
        *r_error_str = N_("Cannot apply effects to audio sequence strips");
        return 0;
      }
      if (!ELEM(seq, activeseq, seq2)) {
        if (seq2 == NULL) {
          seq2 = seq;
        }
        else if (seq1 == NULL) {
          seq1 = seq;
        }
        else if (seq3 == NULL) {
          seq3 = seq;
        }
        else {
          *r_error_str = N_("Cannot apply effect to more than 3 sequence strips");
          return 0;
        }
      }
    }
  }

  /* Make sequence selection a little bit more intuitive
   * for 3 strips: the last-strip should be seq3. */
  if (seq3 != NULL && seq2 != NULL) {
    Sequence *tmp = seq2;
    seq2 = seq3;
    seq3 = tmp;
  }

  switch (SEQ_effect_get_num_inputs(type)) {
    case 0:
      *r_selseq1 = *r_selseq2 = *r_selseq3 = NULL;
      return 1; /* Success. */
    case 1:
      if (seq2 == NULL) {
        *r_error_str = N_("At least one selected sequence strip is needed");
        return 0;
      }
      if (seq1 == NULL) {
        seq1 = seq2;
      }
      if (seq3 == NULL) {
        seq3 = seq2;
      }
      ATTR_FALLTHROUGH;
    case 2:
      if (seq1 == NULL || seq2 == NULL) {
        *r_error_str = N_("2 selected sequence strips are needed");
        return 0;
      }
      if (seq3 == NULL) {
        seq3 = seq2;
      }
      break;
  }

  if (seq1 == NULL && seq2 == NULL && seq3 == NULL) {
    *r_error_str = N_("TODO: in what cases does this happen?");
    return 0;
  }

  *r_selseq1 = seq1;
  *r_selseq2 = seq2;
  *r_selseq3 = seq3;

  /* TODO(Richard): This function needs some refactoring, this is just quick hack for T73828. */
  if (SEQ_effect_get_num_inputs(type) < 3) {
    *r_selseq3 = NULL;
  }
  if (SEQ_effect_get_num_inputs(type) < 2) {
    *r_selseq2 = NULL;
  }

  return 1;
}

static int sequencer_reassign_inputs_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Sequence *seq1, *seq2, *seq3, *last_seq = SEQ_select_active_get(scene);
  const char *error_msg;

  if (SEQ_effect_get_num_inputs(last_seq->type) == 0) {
    BKE_report(op->reports, RPT_ERROR, "Cannot reassign inputs: strip has no inputs");
    return OPERATOR_CANCELLED;
  }

  if (!seq_effect_find_selected(
          scene, last_seq, last_seq->type, &seq1, &seq2, &seq3, &error_msg) ||
      SEQ_effect_get_num_inputs(last_seq->type) == 0) {
    BKE_report(op->reports, RPT_ERROR, error_msg);
    return OPERATOR_CANCELLED;
  }
  /* Check if reassigning would create recursivity. */
  if (SEQ_relations_render_loop_check(seq1, last_seq) ||
      SEQ_relations_render_loop_check(seq2, last_seq) ||
      SEQ_relations_render_loop_check(seq3, last_seq)) {
    BKE_report(op->reports, RPT_ERROR, "Cannot reassign inputs: recursion detected");
    return OPERATOR_CANCELLED;
  }

  last_seq->seq1 = seq1;
  last_seq->seq2 = seq2;
  last_seq->seq3 = seq3;

  int old_start = last_seq->start;
  SEQ_relations_update_changed_seq_and_deps(scene, last_seq, 1, 1);
  SEQ_offset_animdata(scene, last_seq, (last_seq->start - old_start));

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

static bool sequencer_effect_poll(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene, false);

  if (ed) {
    Sequence *last_seq = SEQ_select_active_get(scene);
    if (last_seq && (last_seq->type & SEQ_TYPE_EFFECT)) {
      return 1;
    }
  }

  return 0;
}

void SEQUENCER_OT_reassign_inputs(struct wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Reassign Inputs";
  ot->idname = "SEQUENCER_OT_reassign_inputs";
  ot->description = "Reassign the inputs for the effect strip";

  /* Api callbacks. */
  ot->exec = sequencer_reassign_inputs_exec;
  ot->poll = sequencer_effect_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Swap Inputs Operator
 * \{ */

static int sequencer_swap_inputs_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Sequence *seq, *last_seq = SEQ_select_active_get(scene);

  if (last_seq->seq1 == NULL || last_seq->seq2 == NULL) {
    BKE_report(op->reports, RPT_ERROR, "No valid inputs to swap");
    return OPERATOR_CANCELLED;
  }

  seq = last_seq->seq1;
  last_seq->seq1 = last_seq->seq2;
  last_seq->seq2 = seq;

  SEQ_relations_update_changed_seq_and_deps(scene, last_seq, 1, 1);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}
void SEQUENCER_OT_swap_inputs(struct wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Swap Inputs";
  ot->idname = "SEQUENCER_OT_swap_inputs";
  ot->description = "Swap the first two inputs for the effect strip";

  /* Api callbacks. */
  ot->exec = sequencer_swap_inputs_exec;
  ot->poll = sequencer_effect_poll;

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

  return mouseloc[0] > frame ? SEQ_SIDE_RIGHT : SEQ_SIDE_LEFT;
}

static const EnumPropertyItem prop_split_types[] = {
    {SEQ_SPLIT_SOFT, "SOFT", 0, "Soft", ""},
    {SEQ_SPLIT_HARD, "HARD", 0, "Hard", ""},
    {0, NULL, 0, NULL, NULL},
};

EnumPropertyItem prop_side_types[] = {
    {SEQ_SIDE_MOUSE, "MOUSE", 0, "Mouse Position", ""},
    {SEQ_SIDE_LEFT, "LEFT", 0, "Left", ""},
    {SEQ_SIDE_RIGHT, "RIGHT", 0, "Right", ""},
    {SEQ_SIDE_BOTH, "BOTH", 0, "Both", ""},
    {SEQ_SIDE_NO_CHANGE, "NO_CHANGE", 0, "No Change", ""},
    {0, NULL, 0, NULL, NULL},
};

static int sequencer_split_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene, false);
  bool changed = false;
  bool seq_selected = false;

  const int split_frame = RNA_int_get(op->ptr, "frame");
  const int split_channel = RNA_int_get(op->ptr, "channel");
  const bool use_cursor_position = RNA_boolean_get(op->ptr, "use_cursor_position");
  const eSeqSplitMethod method = RNA_enum_get(op->ptr, "type");
  const int split_side = RNA_enum_get(op->ptr, "side");
  const bool ignore_selection = RNA_boolean_get(op->ptr, "ignore_selection");

  SEQ_prefetch_stop(scene);

  LISTBASE_FOREACH (Sequence *, seq, ed->seqbasep) {
    seq->tmp = NULL;
  }

  LISTBASE_FOREACH_BACKWARD (Sequence *, seq, ed->seqbasep) {
    if (use_cursor_position && seq->machine != split_channel) {
      continue;
    }

    if (ignore_selection || seq->flag & SELECT) {
      if (SEQ_edit_strip_split(bmain, scene, ed->seqbasep, seq, split_frame, method) != NULL) {
        changed = true;
      }
    }
  }

  if (changed) { /* Got new strips? */
    Sequence *seq;
    if (ignore_selection) {
      if (use_cursor_position) {
        SEQ_CURRENT_BEGIN (ed, seq) {
          if (seq->enddisp == split_frame && seq->machine == split_channel) {
            seq_selected = seq->flag & SEQ_ALLSEL;
          }
        }
        SEQ_CURRENT_END;
        if (!seq_selected) {
          SEQ_CURRENT_BEGIN (ed, seq) {
            if (seq->startdisp == split_frame && seq->machine == split_channel) {
              seq->flag &= ~SEQ_ALLSEL;
            }
          }
          SEQ_CURRENT_END;
        }
      }
    }
    else {
      if (split_side != SEQ_SIDE_BOTH) {
        SEQ_CURRENT_BEGIN (ed, seq) {
          if (split_side == SEQ_SIDE_LEFT) {
            if (seq->startdisp >= split_frame) {
              seq->flag &= ~SEQ_ALLSEL;
            }
          }
          else {
            if (seq->enddisp <= split_frame) {
              seq->flag &= ~SEQ_ALLSEL;
            }
          }
        }
        SEQ_CURRENT_END;
      }
    }

    SEQ_sort(scene);
  }
  if (changed) {
    WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
    return OPERATOR_FINISHED;
  }

  /* Passthrough to selection if used as tool. */
  return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
}

static int sequencer_split_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Scene *scene = CTX_data_scene(C);
  View2D *v2d = UI_view2d_fromcontext(C);

  int split_side = RNA_enum_get(op->ptr, "side");
  int split_frame = CFRA;

  if (split_side == SEQ_SIDE_MOUSE) {
    if (ED_operator_sequencer_active(C) && v2d) {
      split_side = mouse_frame_side(v2d, event->mval[0], split_frame);
    }
    else {
      split_side = SEQ_SIDE_BOTH;
    }
  }
  float mouseloc[2];
  UI_view2d_region_to_view(v2d, event->mval[0], event->mval[1], &mouseloc[0], &mouseloc[1]);
  if (RNA_boolean_get(op->ptr, "use_cursor_position")) {
    RNA_int_set(op->ptr, "frame", mouseloc[0]);
  }
  else {
    RNA_int_set(op->ptr, "frame", split_frame);
  }
  RNA_int_set(op->ptr, "channel", mouseloc[1]);
  RNA_enum_set(op->ptr, "side", split_side);
  /*RNA_enum_set(op->ptr, "type", split_hard); */

  return sequencer_split_exec(C, op);
}

static void sequencer_split_ui(bContext *UNUSED(C), wmOperator *op)
{
  uiLayout *layout = op->layout;
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  PointerRNA ptr;
  RNA_pointer_create(NULL, op->type->srna, op->properties, &ptr);

  uiLayout *row = uiLayoutRow(layout, false);
  uiItemR(row, &ptr, "type", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
  uiItemR(layout, &ptr, "frame", 0, NULL, ICON_NONE);
  uiItemR(layout, &ptr, "side", 0, NULL, ICON_NONE);

  uiItemS(layout);

  uiItemR(layout, &ptr, "use_cursor_position", 0, NULL, ICON_NONE);
  if (RNA_boolean_get(&ptr, "use_cursor_position")) {
    uiItemR(layout, &ptr, "channel", 0, NULL, ICON_NONE);
  }
}

void SEQUENCER_OT_split(struct wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Split Strips";
  ot->idname = "SEQUENCER_OT_split";
  ot->description = "Split the selected strips in two";

  /* Api callbacks. */
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
               SEQ_SPLIT_SOFT,
               "Type",
               "The type of split operation to perform on strips");

  RNA_def_boolean(ot->srna,
                  "use_cursor_position",
                  0,
                  "Use Cursor Position",
                  "Split at position of the cursor instead of current frame");

  prop = RNA_def_enum(ot->srna,
                      "side",
                      prop_side_types,
                      SEQ_SIDE_MOUSE,
                      "Side",
                      "The side that remains selected after splitting");

  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(
      ot->srna,
      "ignore_selection",
      false,
      "Ignore Selection",
      "Make cut event if strip is not selected preserving selection state after cut");

  RNA_def_property_flag(prop, PROP_HIDDEN);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Duplicate Strips Operator
 * \{ */

static int apply_unique_name_fn(Sequence *seq, void *arg_pt)
{
  Scene *scene = (Scene *)arg_pt;
  char name[sizeof(seq->name) - 2];

  BLI_strncpy_utf8(name, seq->name + 2, sizeof(name));
  SEQ_sequence_base_unique_name_recursive(&scene->ed->seqbase, seq);
  SEQ_dupe_animdata(scene, name, seq->name + 2);
  return 1;
}

static int sequencer_add_duplicate_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene, false);

  ListBase nseqbase = {NULL, NULL};

  if (ed == NULL) {
    return OPERATOR_CANCELLED;
  }

  SEQ_sequence_base_dupli_recursive(scene, scene, &nseqbase, ed->seqbasep, SEQ_DUPE_CONTEXT, 0);

  if (nseqbase.first) {
    Sequence *seq = nseqbase.first;
    /* Rely on the nseqbase list being added at the end.
     * Their UUIDs has been re-generated by the SEQ_sequence_base_dupli_recursive(), */
    BLI_movelisttolist(ed->seqbasep, &nseqbase);

    for (; seq; seq = seq->next) {
      SEQ_iterator_recursive_apply(seq, apply_unique_name_fn, scene);
    }

    WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

void SEQUENCER_OT_duplicate(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Duplicate Strips";
  ot->idname = "SEQUENCER_OT_duplicate";
  ot->description = "Duplicate the selected strips";

  /* Api callbacks. */
  ot->exec = sequencer_add_duplicate_exec;
  ot->poll = ED_operator_sequencer_active;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Erase Strips Operator
 * \{ */

static int sequencer_delete_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene, false);
  Sequence *seq;

  SEQ_prefetch_stop(scene);

  SEQ_CURRENT_BEGIN (scene->ed, seq) {
    if (seq->flag & SELECT) {
      SEQ_edit_flag_for_removal(scene, ed->seqbasep, seq);
    }
  }
  SEQ_CURRENT_END;
  SEQ_edit_remove_flagged_sequences(scene, ed->seqbasep);

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
  return OPERATOR_FINISHED;
}

static int sequencer_delete_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);

  if (region->regiontype == RGN_TYPE_WINDOW) {
    /* Bounding box of 30 pixels is used for markers shortcuts,
     * prevent conflict with markers shortcuts here.
     */
    if (event->mval[1] <= 30) {
      return OPERATOR_PASS_THROUGH;
    }
  }

  return sequencer_delete_exec(C, op);
}

void SEQUENCER_OT_delete(wmOperatorType *ot)
{

  /* Identifiers. */
  ot->name = "Erase Strips";
  ot->idname = "SEQUENCER_OT_delete";
  ot->description = "Erase selected strips from the sequencer";

  /* Api callbacks. */
  ot->invoke = sequencer_delete_invoke;
  ot->exec = sequencer_delete_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clear Strip Offset Operator
 * \{ */

static int sequencer_offset_clear_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene, false);
  Sequence *seq;

  /* For effects, try to find a replacement input. */
  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    if ((seq->type & SEQ_TYPE_EFFECT) == 0 && (seq->flag & SELECT)) {
      seq->startofs = seq->endofs = seq->startstill = seq->endstill = 0;
    }
  }

  /* Update lengths, etc. */
  seq = ed->seqbasep->first;
  while (seq) {
    SEQ_time_update_sequence(scene, seq);
    seq = seq->next;
  }

  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    if ((seq->type & SEQ_TYPE_EFFECT) == 0 && (seq->flag & SELECT)) {
      if (SEQ_transform_test_overlap(ed->seqbasep, seq)) {
        SEQ_transform_seqbase_shuffle(ed->seqbasep, seq, scene);
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
  ot->description = "Clear strip offsets from the start and end frames";

  /* Api callbacks. */
  ot->exec = sequencer_offset_clear_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Separate Images Operator
 * \{ */

static int sequencer_separate_images_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene, false);

  Sequence *seq, *seq_new;
  Strip *strip_new;
  StripElem *se, *se_new;
  int start_ofs, timeline_frame, frame_end;
  int step = RNA_int_get(op->ptr, "length");

  seq = ed->seqbasep->first; /* Poll checks this is valid. */

  SEQ_prefetch_stop(scene);

  while (seq) {
    if ((seq->flag & SELECT) && (seq->type == SEQ_TYPE_IMAGE) && (seq->len > 1)) {
      Sequence *seq_next;

      /* Remove seq so overlap tests don't conflict,
       * see seq_free_sequence below for the real freeing. */
      BLI_remlink(ed->seqbasep, seq);
      /* if (seq->ipo) id_us_min(&seq->ipo->id); */
      /* XXX, remove fcurve and assign to split image strips */

      start_ofs = timeline_frame = SEQ_transform_get_left_handle_frame(seq, false);
      frame_end = SEQ_transform_get_right_handle_frame(seq, false);

      while (timeline_frame < frame_end) {
        /* New seq. */
        se = SEQ_render_give_stripelem(seq, timeline_frame);

        seq_new = SEQ_sequence_dupli_recursive(
            scene, scene, ed->seqbasep, seq, SEQ_DUPE_UNIQUE_NAME);

        seq_new->start = start_ofs;
        seq_new->type = SEQ_TYPE_IMAGE;
        seq_new->len = 1;
        seq_new->endstill = step - 1;

        /* New strip. */
        strip_new = seq_new->strip;
        strip_new->us = 1;

        /* New stripdata, only one element now. */
        /* Note this assume all elements (images) have the same dimension,
         * since we only copy the name here. */
        se_new = MEM_reallocN(strip_new->stripdata, sizeof(*se_new));
        BLI_strncpy(se_new->name, se->name, sizeof(se_new->name));
        strip_new->stripdata = se_new;

        SEQ_time_update_sequence(scene, seq_new);

        if (step > 1) {
          seq_new->flag &= ~SEQ_OVERLAP;
          if (SEQ_transform_test_overlap(ed->seqbasep, seq_new)) {
            SEQ_transform_seqbase_shuffle(ed->seqbasep, seq_new, scene);
          }
        }

        /* XXX, COPY FCURVES */

        timeline_frame++;
        start_ofs += step;
      }

      seq_next = seq->next;
      SEQ_sequence_free(scene, seq, true);
      seq = seq_next;
    }
    else {
      seq = seq->next;
    }
  }

  SEQ_sort(scene);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_images_separate(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Separate Images";
  ot->idname = "SEQUENCER_OT_images_separate";
  ot->description = "On image sequence strips, it returns a strip for each image";

  /* Api callbacks. */
  ot->exec = sequencer_separate_images_exec;
  ot->invoke = WM_operator_props_popup_confirm;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_int(ot->srna, "length", 1, 1, INT_MAX, "Length", "Length of each frame", 1, 1000);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle Meta Strip Operator
 * \{ */

void recurs_sel_seq(Sequence *seqm)
{
  Sequence *seq;

  seq = seqm->seqbase.first;
  while (seq) {

    if (seqm->flag & (SEQ_LEFTSEL + SEQ_RIGHTSEL)) {
      seq->flag &= ~SEQ_ALLSEL;
    }
    else if (seqm->flag & SELECT) {
      seq->flag |= SELECT;
    }
    else {
      seq->flag &= ~SEQ_ALLSEL;
    }

    if (seq->seqbase.first) {
      recurs_sel_seq(seq);
    }

    seq = seq->next;
  }
}

static int sequencer_meta_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene, false);
  Sequence *last_seq = SEQ_select_active_get(scene);
  MetaStack *ms;

  if (last_seq && last_seq->type == SEQ_TYPE_META && last_seq->flag & SELECT) {
    /* Enter metastrip. */
    ms = MEM_mallocN(sizeof(MetaStack), "metastack");
    BLI_addtail(&ed->metastack, ms);
    ms->parseq = last_seq;
    ms->oldbasep = ed->seqbasep;
    copy_v2_v2_int(ms->disp_range, &ms->parseq->startdisp);

    ed->seqbasep = &last_seq->seqbase;

    SEQ_select_active_set(scene, NULL);
  }
  else {
    /* Exit metastrip if possible. */

    Sequence *seq;

    if (BLI_listbase_is_empty(&ed->metastack)) {
      return OPERATOR_CANCELLED;
    }

    ms = ed->metastack.last;
    BLI_remlink(&ed->metastack, ms);

    ed->seqbasep = ms->oldbasep;

    /* For old files, update from meta. */
    if (ms->disp_range[0] == ms->disp_range[1]) {
      copy_v2_v2_int(ms->disp_range, &ms->parseq->startdisp);
    }

    /* Recalc all: the meta can have effects connected to it. */
    for (seq = ed->seqbasep->first; seq; seq = seq->next) {
      SEQ_time_update_sequence(scene, seq);
    }

    /* 2.73+, keeping endpoints is important!
     * Moving them around means you can't usefully use metas in a complex edit. */
#if 1
    SEQ_transform_set_left_handle_frame(ms->parseq, ms->disp_range[0]);
    SEQ_transform_set_right_handle_frame(ms->parseq, ms->disp_range[1]);
    SEQ_transform_fix_single_image_seq_offsets(ms->parseq);
    SEQ_time_update_sequence(scene, ms->parseq);
#else
    if (SEQ_transform_test_overlap(ed->seqbasep, ms->parseq)) {
      SEQ_transform_seqbase_shuffle(ed->seqbasep, ms->parseq, scene);
    }
#endif

    SEQ_select_active_set(scene, ms->parseq);

    ms->parseq->flag |= SELECT;
    recurs_sel_seq(ms->parseq);

    MEM_freeN(ms);
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
  ot->description = "Toggle a metastrip (to edit enclosed strips)";

  /* Api callbacks. */
  ot->exec = sequencer_meta_toggle_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Make Meta Strip Operator
 * \{ */

static int sequencer_meta_make_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene, false);

  Sequence *seq, *seqm, *next, *last_seq = SEQ_select_active_get(scene);
  int channel_max = 1;

  if (SEQ_transform_seqbase_isolated_sel_check(ed->seqbasep) == false) {
    BKE_report(op->reports, RPT_ERROR, "Please select all related strips");
    return OPERATOR_CANCELLED;
  }

  SEQ_prefetch_stop(scene);

  /* Remove all selected from main list, and put in meta. */

  seqm = SEQ_sequence_alloc(ed->seqbasep, 1, 1, SEQ_TYPE_META); /* Channel number set later. */
  strcpy(seqm->name + 2, "MetaStrip");
  seqm->flag = SELECT;

  seq = ed->seqbasep->first;
  while (seq) {
    next = seq->next;
    if (seq != seqm && (seq->flag & SELECT)) {
      SEQ_relations_invalidate_cache_composite(scene, seq);
      channel_max = max_ii(seq->machine, channel_max);
      /* Sequence is moved within the same edit, no need to re-generate the UUID. */
      BLI_remlink(ed->seqbasep, seq);
      BLI_addtail(&seqm->seqbase, seq);
    }
    seq = next;
  }
  seqm->machine = last_seq ? last_seq->machine : channel_max;
  SEQ_time_update_sequence(scene, seqm);

  SEQ_select_active_set(scene, seqm);

  if (SEQ_transform_test_overlap(ed->seqbasep, seqm)) {
    SEQ_transform_seqbase_shuffle(ed->seqbasep, seqm, scene);
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);

  SEQ_sequence_base_unique_name_recursive(&scene->ed->seqbase, seqm);
  SEQ_relations_invalidate_cache_composite(scene, seqm);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_meta_make(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Make Meta Strip";
  ot->idname = "SEQUENCER_OT_meta_make";
  ot->description = "Group selected strips into a metastrip";

  /* Api callbacks. */
  ot->exec = sequencer_meta_make_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UnMeta Strip Operator
 * \{ */

static int seq_depends_on_meta(Sequence *seq, Sequence *seqm)
{
  if (seq == seqm) {
    return 1;
  }
  if (seq->seq1 && seq_depends_on_meta(seq->seq1, seqm)) {
    return 1;
  }
  if (seq->seq2 && seq_depends_on_meta(seq->seq2, seqm)) {
    return 1;
  }
  if (seq->seq3 && seq_depends_on_meta(seq->seq3, seqm)) {
    return 1;
  }
  return 0;
}

static void recurs_del_seq_flag(Scene *scene, ListBase *lb, short flag, short deleteall)
{
  Sequence *seq, *seqn;
  Sequence *last_seq = SEQ_select_active_get(scene);

  seq = lb->first;
  while (seq) {
    seqn = seq->next;
    if ((seq->flag & flag) || deleteall) {
      BLI_remlink(lb, seq);
      if (seq == last_seq) {
        SEQ_select_active_set(scene, NULL);
      }
      if (seq->type == SEQ_TYPE_META) {
        recurs_del_seq_flag(scene, &seq->seqbase, flag, 1);
      }
      SEQ_sequence_free(scene, seq, true);
    }
    seq = seqn;
  }
}

static int sequencer_meta_separate_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene, false);

  Sequence *seq, *last_seq = SEQ_select_active_get(scene); /* last_seq checks (ed == NULL) */

  if (last_seq == NULL || last_seq->type != SEQ_TYPE_META) {
    return OPERATOR_CANCELLED;
  }

  SEQ_prefetch_stop(scene);

  for (seq = last_seq->seqbase.first; seq != NULL; seq = seq->next) {
    SEQ_relations_invalidate_cache_composite(scene, seq);
  }

  /* This moves strips from meta to parent, sating within same edit and no new strips are
   * allocated. If the UUID was unique already (as it should) it will stay unique.
   * No need to re-generate the UUIDs. */
  BLI_movelisttolist(ed->seqbasep, &last_seq->seqbase);

  BLI_listbase_clear(&last_seq->seqbase);

  BLI_remlink(ed->seqbasep, last_seq);
  SEQ_sequence_free(scene, last_seq, true);

  /* Empty meta strip, delete all effects depending on it. */
  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    if ((seq->type & SEQ_TYPE_EFFECT) && seq_depends_on_meta(seq, last_seq)) {
      seq->flag |= SEQ_FLAG_DELETE;
    }
  }

  recurs_del_seq_flag(scene, ed->seqbasep, SEQ_FLAG_DELETE, 0);

  /* Test for effects and overlap
   * don't use SEQ_CURRENT_BEGIN since that would be recursive. */
  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    if (seq->flag & SELECT) {
      seq->flag &= ~SEQ_OVERLAP;
      if (SEQ_transform_test_overlap(ed->seqbasep, seq)) {
        SEQ_transform_seqbase_shuffle(ed->seqbasep, seq, scene);
      }
    }
  }

  SEQ_sort(scene);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_meta_separate(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "UnMeta Strip";
  ot->idname = "SEQUENCER_OT_meta_separate";
  ot->description = "Put the contents of a metastrip back in the sequencer";

  /* Api callbacks. */
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
  int timeline_frame = CFRA;
  int next_frame = SEQ_time_find_next_prev_edit(
      scene, timeline_frame, side, do_skip_mute, do_center, false);

  if (next_frame != timeline_frame) {
    CFRA = next_frame;
    changed = true;
  }

  return changed;
}

static bool sequencer_strip_jump_poll(bContext *C)
{
  /* Prevent changes during render. */
  if (G.is_rendering) {
    return 0;
  }

  return sequencer_edit_poll(C);
}

static int sequencer_strip_jump_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  const bool next = RNA_boolean_get(op->ptr, "next");
  const bool center = RNA_boolean_get(op->ptr, "center");

  /* Currently do_skip_mute is always true. */
  if (!strip_jump_internal(scene, next ? SEQ_SIDE_RIGHT : SEQ_SIDE_LEFT, true, center)) {
    return OPERATOR_CANCELLED;
  }

  WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_strip_jump(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Jump to Strip";
  ot->idname = "SEQUENCER_OT_strip_jump";
  ot->description = "Move frame to previous edit point";

  /* Api callbacks. */
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
    {SEQ_SIDE_LEFT, "LEFT", 0, "Left", ""},
    {SEQ_SIDE_RIGHT, "RIGHT", 0, "Right", ""},
    {0, NULL, 0, NULL, NULL},
};

static void swap_sequence(Scene *scene, Sequence *seqa, Sequence *seqb)
{
  int gap = seqb->startdisp - seqa->enddisp;
  int seq_a_start;
  int seq_b_start;

  seq_b_start = (seqb->start - seqb->startdisp) + seqa->startdisp;
  SEQ_transform_translate_sequence(scene, seqb, seq_b_start - seqb->start);
  SEQ_time_update_sequence(scene, seqb);

  seq_a_start = (seqa->start - seqa->startdisp) + seqb->enddisp + gap;
  SEQ_transform_translate_sequence(scene, seqa, seq_a_start - seqa->start);
  SEQ_time_update_sequence(scene, seqa);
}

static Sequence *find_next_prev_sequence(Scene *scene, Sequence *test, int lr, int sel)
{
  /* sel: 0==unselected, 1==selected, -1==don't care. */
  Sequence *seq, *best_seq = NULL;
  Editing *ed = SEQ_editing_get(scene, false);

  int dist, best_dist;
  best_dist = MAXFRAME * 2;

  if (ed == NULL) {
    return NULL;
  }

  seq = ed->seqbasep->first;
  while (seq) {
    if ((seq != test) && (test->machine == seq->machine) && (test->depth == seq->depth) &&
        ((sel == -1) || (sel == (seq->flag & SELECT)))) {
      dist = MAXFRAME * 2;

      switch (lr) {
        case SEQ_SIDE_LEFT:
          if (seq->enddisp <= test->startdisp) {
            dist = test->enddisp - seq->startdisp;
          }
          break;
        case SEQ_SIDE_RIGHT:
          if (seq->startdisp >= test->enddisp) {
            dist = seq->startdisp - test->enddisp;
          }
          break;
      }

      if (dist == 0) {
        best_seq = seq;
        break;
      }
      if (dist < best_dist) {
        best_dist = dist;
        best_seq = seq;
      }
    }
    seq = seq->next;
  }
  return best_seq; /* Can be null. */
}

static bool seq_is_parent(Sequence *par, Sequence *seq)
{
  return ((par->seq1 == seq) || (par->seq2 == seq) || (par->seq3 == seq));
}

static int sequencer_swap_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene, false);
  Sequence *active_seq = SEQ_select_active_get(scene);
  Sequence *seq, *iseq;
  int side = RNA_enum_get(op->ptr, "side");

  if (active_seq == NULL) {
    return OPERATOR_CANCELLED;
  }

  seq = find_next_prev_sequence(scene, active_seq, side, -1);

  if (seq) {

    /* Disallow effect strips. */
    if (SEQ_effect_get_num_inputs(seq->type) >= 1 &&
        (seq->effectdata || seq->seq1 || seq->seq2 || seq->seq3)) {
      return OPERATOR_CANCELLED;
    }
    if ((SEQ_effect_get_num_inputs(active_seq->type) >= 1) &&
        (active_seq->effectdata || active_seq->seq1 || active_seq->seq2 || active_seq->seq3)) {
      return OPERATOR_CANCELLED;
    }

    switch (side) {
      case SEQ_SIDE_LEFT:
        swap_sequence(scene, seq, active_seq);
        break;
      case SEQ_SIDE_RIGHT:
        swap_sequence(scene, active_seq, seq);
        break;
    }

    /* XXX - Should be a generic function. */
    for (iseq = scene->ed->seqbasep->first; iseq; iseq = iseq->next) {
      if ((iseq->type & SEQ_TYPE_EFFECT) &&
          (seq_is_parent(iseq, active_seq) || seq_is_parent(iseq, seq))) {
        SEQ_time_update_sequence(scene, iseq);
      }
    }

    /* Do this in a new loop since both effects need to be calculated first. */
    for (iseq = scene->ed->seqbasep->first; iseq; iseq = iseq->next) {
      if ((iseq->type & SEQ_TYPE_EFFECT) &&
          (seq_is_parent(iseq, active_seq) || seq_is_parent(iseq, seq))) {
        /* This may now overlap. */
        if (SEQ_transform_test_overlap(ed->seqbasep, iseq)) {
          SEQ_transform_seqbase_shuffle(ed->seqbasep, iseq, scene);
        }
      }
    }

    SEQ_sort(scene);

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

  /* Api callbacks. */
  ot->exec = sequencer_swap_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties. */
  RNA_def_enum(
      ot->srna, "side", prop_side_lr_types, SEQ_SIDE_RIGHT, "Side", "Side of the strip to swap");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Render Size Operator
 * \{ */

static int sequencer_rendersize_exec(bContext *C, wmOperator *UNUSED(op))
{
  int retval = OPERATOR_CANCELLED;
  Scene *scene = CTX_data_scene(C);
  Sequence *active_seq = SEQ_select_active_get(scene);
  StripElem *se = NULL;

  if (active_seq == NULL) {
    return OPERATOR_CANCELLED;
  }

  if (active_seq->strip) {
    switch (active_seq->type) {
      case SEQ_TYPE_IMAGE:
        se = SEQ_render_give_stripelem(active_seq, scene->r.cfra);
        break;
      case SEQ_TYPE_MOVIE:
        se = active_seq->strip->stripdata;
        break;
      case SEQ_TYPE_SCENE:
      case SEQ_TYPE_META:
      case SEQ_TYPE_SOUND_RAM:
      case SEQ_TYPE_SOUND_HD:
      default:
        break;
    }
  }

  if (se) {
    /* Prevent setting the render size if sequence values aren't initialized. */
    if ((se->orig_width > 0) && (se->orig_height > 0)) {
      scene->r.xsch = se->orig_width;
      scene->r.ysch = se->orig_height;
      WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, scene);
      retval = OPERATOR_FINISHED;
    }
  }

  return retval;
}

void SEQUENCER_OT_rendersize(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Set Render Size";
  ot->idname = "SEQUENCER_OT_rendersize";
  ot->description = "Set render size and aspect from active sequence";

  /* Api callbacks. */
  ot->exec = sequencer_rendersize_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Copy Operator
 * \{ */

static void seq_copy_del_sound(Scene *scene, Sequence *seq)
{
  if (seq->type == SEQ_TYPE_META) {
    Sequence *iseq;
    for (iseq = seq->seqbase.first; iseq; iseq = iseq->next) {
      seq_copy_del_sound(scene, iseq);
    }
  }
  else if (seq->scene_sound) {
    BKE_sound_remove_scene_sound(scene, seq->scene_sound);
    seq->scene_sound = NULL;
  }
}

static int sequencer_copy_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene, false);

  SEQ_clipboard_free();

  if (SEQ_transform_seqbase_isolated_sel_check(ed->seqbasep) == false) {
    BKE_report(op->reports, RPT_ERROR, "Please select all related strips");
    return OPERATOR_CANCELLED;
  }

  /* NOTE: The UUID is re-generated on paste, so we can keep UUID in the clipboard since
   * nobody can reach them anyway.
   * This reduces chance or running out of UUIDs if a cat falls asleep on Ctrl-C. */
  SEQ_sequence_base_dupli_recursive(scene,
                                    scene,
                                    &seqbase_clipboard,
                                    ed->seqbasep,
                                    0,
                                    (LIB_ID_CREATE_NO_USER_REFCOUNT | LIB_ID_FREE_NO_MAIN));

  seqbase_clipboard_frame = scene->r.cfra;

  /* Remove anything that references the current scene. */
  LISTBASE_FOREACH (Sequence *, seq, &seqbase_clipboard) {
    seq_copy_del_sound(scene, seq);
  }

  /* Replace datablock pointers with copies, to keep things working in case
   * data-blocks get deleted or another .blend file is opened. */
  SEQ_clipboard_pointers_store(bmain, &seqbase_clipboard);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_copy(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Copy";
  ot->idname = "SEQUENCER_OT_copy";
  ot->description = "Copy selected strips to clipboard";

  /* Api callbacks. */
  ot->exec = sequencer_copy_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Paste Operator
 * \{ */

void ED_sequencer_deselect_all(Scene *scene)
{
  Sequence *seq;
  Editing *ed = SEQ_editing_get(scene, false);

  if (ed == NULL) {
    return;
  }

  SEQ_CURRENT_BEGIN (ed, seq) {
    seq->flag &= ~SEQ_ALLSEL;
  }
  SEQ_CURRENT_END;
}

static int sequencer_paste_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene, true); /* Create if needed. */
  ListBase nseqbase = {NULL, NULL};
  int ofs;
  Sequence *iseq, *iseq_first;

  if (BLI_listbase_count(&seqbase_clipboard) == 0) {
    BKE_report(op->reports, RPT_INFO, "No strips to paste");
    return OPERATOR_CANCELLED;
  }

  ED_sequencer_deselect_all(scene);
  if (RNA_boolean_get(op->ptr, "keep_offset")) {
    ofs = scene->r.cfra - seqbase_clipboard_frame;
  }
  else {
    int min_seq_startdisp = INT_MAX;
    LISTBASE_FOREACH (Sequence *, seq, &seqbase_clipboard) {
      if (seq->startdisp < min_seq_startdisp) {
        min_seq_startdisp = seq->startdisp;
      }
    }
    /* Paste strips after playhead. */
    ofs = scene->r.cfra - min_seq_startdisp;
  }

  /* Copy strips, temporarily restoring pointers to actual data-blocks. This
   * must happen on the clipboard itself, so that copying does user counting
   * on the actual data-blocks. */
  SEQ_clipboard_pointers_restore(&seqbase_clipboard, bmain);
  SEQ_sequence_base_dupli_recursive(scene, scene, &nseqbase, &seqbase_clipboard, 0, 0);
  SEQ_clipboard_pointers_store(bmain, &seqbase_clipboard);

  iseq_first = nseqbase.first;

  /* NOTE: SEQ_sequence_base_dupli_recursive() takes care of generating new UUIDs for sequences
   * in the new list. */
  BLI_movelisttolist(ed->seqbasep, &nseqbase);

  for (iseq = iseq_first; iseq; iseq = iseq->next) {
    /* Make sure, that pasted strips have unique names. */
    SEQ_iterator_recursive_apply(iseq, apply_unique_name_fn, scene);
    /* Translate after name has been changed, otherwise this will affect animdata of original
     * strip. */
    SEQ_transform_translate_sequence(scene, iseq, ofs);
    /* Ensure, that pasted strips don't overlap. */
    if (SEQ_transform_test_overlap(ed->seqbasep, iseq)) {
      SEQ_transform_seqbase_shuffle(ed->seqbasep, iseq, scene);
    }
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
  ED_outliner_select_sync_from_sequence_tag(C);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_paste(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Paste";
  ot->idname = "SEQUENCER_OT_paste";
  ot->description = "Paste strips from clipboard";

  /* Api callbacks. */
  ot->exec = sequencer_paste_exec;
  ot->poll = ED_operator_sequencer_active;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties. */
  PropertyRNA *prop = RNA_def_boolean(
      ot->srna, "keep_offset", false, "Keep Offset", "Keep strip offset to playhead when pasting");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sequencer Swap Data Operator
 * \{ */

static int sequencer_swap_data_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Sequence *seq_act;
  Sequence *seq_other;
  const char *error_msg;

  if (SEQ_select_active_get_pair(scene, &seq_act, &seq_other) == 0) {
    BKE_report(op->reports, RPT_ERROR, "Please select two strips");
    return OPERATOR_CANCELLED;
  }

  if (SEQ_edit_sequence_swap(seq_act, seq_other, &error_msg) == 0) {
    BKE_report(op->reports, RPT_ERROR, error_msg);
    return OPERATOR_CANCELLED;
  }

  if (seq_act->scene_sound) {
    BKE_sound_remove_scene_sound(scene, seq_act->scene_sound);
  }

  if (seq_other->scene_sound) {
    BKE_sound_remove_scene_sound(scene, seq_other->scene_sound);
  }

  seq_act->scene_sound = NULL;
  seq_other->scene_sound = NULL;

  SEQ_time_update_sequence(scene, seq_act);
  SEQ_time_update_sequence(scene, seq_other);

  if (seq_act->sound) {
    BKE_sound_add_scene_sound_defaults(scene, seq_act);
  }
  if (seq_other->sound) {
    BKE_sound_add_scene_sound_defaults(scene, seq_other);
  }

  SEQ_relations_invalidate_cache_raw(scene, seq_act);
  SEQ_relations_invalidate_cache_raw(scene, seq_other);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_swap_data(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Sequencer Swap Data";
  ot->idname = "SEQUENCER_OT_swap_data";
  ot->description = "Swap 2 sequencer strips";

  /* Api callbacks. */
  ot->exec = sequencer_swap_data_exec;
  ot->poll = ED_operator_sequencer_active;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Change Effect Input Operator
 * \{ */

static const EnumPropertyItem prop_change_effect_input_types[] = {
    {0, "A_B", 0, "A -> B", ""},
    {1, "B_C", 0, "B -> C", ""},
    {2, "A_C", 0, "A -> C", ""},
    {0, NULL, 0, NULL, NULL},
};

static int sequencer_change_effect_input_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene, false);
  Sequence *seq = SEQ_select_active_get(scene);

  Sequence **seq_1, **seq_2;

  switch (RNA_enum_get(op->ptr, "swap")) {
    case 0:
      seq_1 = &seq->seq1;
      seq_2 = &seq->seq2;
      break;
    case 1:
      seq_1 = &seq->seq2;
      seq_2 = &seq->seq3;
      break;
    default: /* 2 */
      seq_1 = &seq->seq1;
      seq_2 = &seq->seq3;
      break;
  }

  if (*seq_1 == NULL || *seq_2 == NULL) {
    BKE_report(op->reports, RPT_ERROR, "One of the effect inputs is unset, cannot swap");
    return OPERATOR_CANCELLED;
  }

  SWAP(Sequence *, *seq_1, *seq_2);

  SEQ_relations_update_changed_seq_and_deps(scene, seq, 0, 1);

  /* Invalidate cache. */
  SEQ_relations_free_imbuf(scene, &ed->seqbase, false);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_change_effect_input(struct wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Change Effect Input";
  ot->idname = "SEQUENCER_OT_change_effect_input";

  /* Api callbacks. */
  ot->exec = sequencer_change_effect_input_exec;
  ot->poll = sequencer_effect_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(
      ot->srna, "swap", prop_change_effect_input_types, 0, "Swap", "The effect inputs to swap");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Change Effect Type Operator
 * \{ */

EnumPropertyItem sequencer_prop_effect_types[] = {
    {SEQ_TYPE_CROSS, "CROSS", 0, "Crossfade", "Crossfade effect strip type"},
    {SEQ_TYPE_ADD, "ADD", 0, "Add", "Add effect strip type"},
    {SEQ_TYPE_SUB, "SUBTRACT", 0, "Subtract", "Subtract effect strip type"},
    {SEQ_TYPE_ALPHAOVER, "ALPHA_OVER", 0, "Alpha Over", "Alpha Over effect strip type"},
    {SEQ_TYPE_ALPHAUNDER, "ALPHA_UNDER", 0, "Alpha Under", "Alpha Under effect strip type"},
    {SEQ_TYPE_GAMCROSS, "GAMMA_CROSS", 0, "Gamma Cross", "Gamma Cross effect strip type"},
    {SEQ_TYPE_MUL, "MULTIPLY", 0, "Multiply", "Multiply effect strip type"},
    {SEQ_TYPE_OVERDROP, "OVER_DROP", 0, "Alpha Over Drop", "Alpha Over Drop effect strip type"},
    {SEQ_TYPE_WIPE, "WIPE", 0, "Wipe", "Wipe effect strip type"},
    {SEQ_TYPE_GLOW, "GLOW", 0, "Glow", "Glow effect strip type"},
    {SEQ_TYPE_TRANSFORM, "TRANSFORM", 0, "Transform", "Transform effect strip type"},
    {SEQ_TYPE_COLOR, "COLOR", 0, "Color", "Color effect strip type"},
    {SEQ_TYPE_SPEED, "SPEED", 0, "Speed", "Color effect strip type"},
    {SEQ_TYPE_MULTICAM, "MULTICAM", 0, "Multicam Selector", ""},
    {SEQ_TYPE_ADJUSTMENT, "ADJUSTMENT", 0, "Adjustment Layer", ""},
    {SEQ_TYPE_GAUSSIAN_BLUR, "GAUSSIAN_BLUR", 0, "Gaussian Blur", ""},
    {SEQ_TYPE_TEXT, "TEXT", 0, "Text", ""},
    {SEQ_TYPE_COLORMIX, "COLORMIX", 0, "Color Mix", ""},
    {0, NULL, 0, NULL, NULL},
};

static int sequencer_change_effect_type_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene, false);
  Sequence *seq = SEQ_select_active_get(scene);
  const int new_type = RNA_enum_get(op->ptr, "type");

  /* Free previous effect and init new effect. */
  struct SeqEffectHandle sh;

  if ((seq->type & SEQ_TYPE_EFFECT) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* Can someone explain the logic behind only allowing to increase this,
   * copied from 2.4x - campbell */
  if (SEQ_effect_get_num_inputs(seq->type) < SEQ_effect_get_num_inputs(new_type)) {
    BKE_report(op->reports, RPT_ERROR, "New effect needs more input strips");
    return OPERATOR_CANCELLED;
  }

  sh = SEQ_effect_handle_get(seq);
  sh.free(seq, true);

  seq->type = new_type;

  sh = SEQ_effect_handle_get(seq);
  sh.init(seq);

  SEQ_relations_update_changed_seq_and_deps(scene, seq, 0, 1);
  /* Invalidate cache. */
  SEQ_relations_free_imbuf(scene, &ed->seqbase, false);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_change_effect_type(struct wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Change Effect Type";
  ot->idname = "SEQUENCER_OT_change_effect_type";

  /* Api callbacks. */
  ot->exec = sequencer_change_effect_type_exec;
  ot->poll = sequencer_effect_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(ot->srna,
                          "type",
                          sequencer_prop_effect_types,
                          SEQ_TYPE_CROSS,
                          "Type",
                          "Sequencer effect type");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Change Data/Files Operator
 * \{ */

static int sequencer_change_path_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene, false);
  Sequence *seq = SEQ_select_active_get(scene);
  const bool is_relative_path = RNA_boolean_get(op->ptr, "relative_path");
  const bool use_placeholders = RNA_boolean_get(op->ptr, "use_placeholders");
  int minext_frameme, numdigits;

  if (seq->type == SEQ_TYPE_IMAGE) {
    char directory[FILE_MAX];
    int len;
    StripElem *se;

    /* Need to find min/max frame for placeholders. */
    if (use_placeholders) {
      len = sequencer_image_seq_get_minmax_frame(op, seq->sfra, &minext_frameme, &numdigits);
    }
    else {
      len = RNA_property_collection_length(op->ptr, RNA_struct_find_property(op->ptr, "files"));
    }
    if (len == 0) {
      return OPERATOR_CANCELLED;
    }

    RNA_string_get(op->ptr, "directory", directory);
    if (is_relative_path) {
      /* TODO, shouldn't this already be relative from the filesel?
       * (as the 'filepath' is) for now just make relative here,
       * but look into changing after 2.60 - campbell */
      BLI_path_rel(directory, BKE_main_blendfile_path(bmain));
    }
    BLI_strncpy(seq->strip->dir, directory, sizeof(seq->strip->dir));

    if (seq->strip->stripdata) {
      MEM_freeN(seq->strip->stripdata);
    }
    seq->strip->stripdata = se = MEM_callocN(len * sizeof(StripElem), "stripelem");

    if (use_placeholders) {
      sequencer_image_seq_reserve_frames(op, se, len, minext_frameme, numdigits);
    }
    else {
      RNA_BEGIN (op->ptr, itemptr, "files") {
        char *filename = RNA_string_get_alloc(&itemptr, "name", NULL, 0);
        BLI_strncpy(se->name, filename, sizeof(se->name));
        MEM_freeN(filename);
        se++;
      }
      RNA_END;
    }

    /* Reset these else we wont see all the images. */
    seq->anim_startofs = seq->anim_endofs = 0;

    /* Correct start/end frames so we don't move.
     * Important not to set seq->len = len; allow the function to handle it. */
    SEQ_add_reload_new_file(bmain, scene, seq, true);

    SEQ_time_update_sequence(scene, seq);

    /* Invalidate cache. */
    SEQ_relations_free_imbuf(scene, &ed->seqbase, false);
  }
  else if (ELEM(seq->type, SEQ_TYPE_SOUND_RAM, SEQ_TYPE_SOUND_HD)) {
    bSound *sound = seq->sound;
    if (sound == NULL) {
      return OPERATOR_CANCELLED;
    }
    char filepath[FILE_MAX];
    RNA_string_get(op->ptr, "filepath", filepath);
    BLI_strncpy(sound->filepath, filepath, sizeof(sound->filepath));
    BKE_sound_load(bmain, sound);
  }
  else {
    /* Lame, set rna filepath. */
    PointerRNA seq_ptr;
    PropertyRNA *prop;
    char filepath[FILE_MAX];

    RNA_pointer_create(&scene->id, &RNA_Sequence, seq, &seq_ptr);

    RNA_string_get(op->ptr, "filepath", filepath);
    prop = RNA_struct_find_property(&seq_ptr, "filepath");
    RNA_property_string_set(&seq_ptr, prop, filepath);
    RNA_property_update(C, &seq_ptr, prop);
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

static int sequencer_change_path_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  Scene *scene = CTX_data_scene(C);
  Sequence *seq = SEQ_select_active_get(scene);
  char filepath[FILE_MAX];

  BLI_join_dirfile(filepath, sizeof(filepath), seq->strip->dir, seq->strip->stripdata->name);

  RNA_string_set(op->ptr, "directory", seq->strip->dir);
  RNA_string_set(op->ptr, "filepath", filepath);

  /* Set default display depending on seq type. */
  if (seq->type == SEQ_TYPE_IMAGE) {
    RNA_boolean_set(op->ptr, "filter_movie", false);
  }
  else {
    RNA_boolean_set(op->ptr, "filter_image", false);
  }

  WM_event_add_fileselect(C, op);

  return OPERATOR_RUNNING_MODAL;
}

void SEQUENCER_OT_change_path(struct wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Change Data/Files";
  ot->idname = "SEQUENCER_OT_change_path";

  /* Api callbacks. */
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
/** \name Export Subtitles Operator
 * \{ */

static int sequencer_export_subtitles_invoke(bContext *C,
                                             wmOperator *op,
                                             const wmEvent *UNUSED(event))
{
  Main *bmain = CTX_data_main(C);
  if (!RNA_struct_property_is_set(op->ptr, "filepath")) {
    char filepath[FILE_MAX];

    if (BKE_main_blendfile_path(bmain)[0] == '\0') {
      BLI_strncpy(filepath, "untitled", sizeof(filepath));
    }
    else {
      BLI_strncpy(filepath, BKE_main_blendfile_path(bmain), sizeof(filepath));
    }

    BLI_path_extension_replace(filepath, sizeof(filepath), ".srt");
    RNA_string_set(op->ptr, "filepath", filepath);
  }

  WM_event_add_fileselect(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static int sequencer_export_subtitles_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Sequence *seq, *seq_next;
  Editing *ed = SEQ_editing_get(scene, false);
  ListBase text_seq = {0};
  int iter = 0;
  FILE *file;
  char filepath[FILE_MAX];

  if (!RNA_struct_property_is_set(op->ptr, "filepath")) {
    BKE_report(op->reports, RPT_ERROR, "No filename given");
    return OPERATOR_CANCELLED;
  }

  RNA_string_get(op->ptr, "filepath", filepath);
  BLI_path_extension_ensure(filepath, sizeof(filepath), ".srt");

  /* Avoid File write exceptions. */
  if (!BLI_exists(filepath)) {
    BLI_make_existing_file(filepath);
    if (!BLI_file_touch(filepath)) {
      BKE_report(op->reports, RPT_ERROR, "Can't create subtitle file");
      return OPERATOR_CANCELLED;
    }
  }
  else if (!BLI_file_is_writable(filepath)) {
    BKE_report(op->reports, RPT_ERROR, "Can't overwrite export file");
    return OPERATOR_CANCELLED;
  }

  SEQ_ALL_BEGIN (ed, seq) {
    if (seq->type == SEQ_TYPE_TEXT) {
      BLI_addtail(&text_seq, MEM_dupallocN(seq));
    }
  }
  SEQ_ALL_END;

  if (BLI_listbase_is_empty(&text_seq)) {
    BKE_report(op->reports, RPT_ERROR, "No subtitles (text strips) to export");
    return OPERATOR_CANCELLED;
  }

  BLI_listbase_sort(&text_seq, SEQ_time_cmp_time_startdisp);

  /* Open and write file. */
  file = BLI_fopen(filepath, "w");

  for (seq = text_seq.first; seq; seq = seq_next) {
    TextVars *data = seq->effectdata;
    char timecode_str_start[32];
    char timecode_str_end[32];

    BLI_timecode_string_from_time(timecode_str_start,
                                  sizeof(timecode_str_start),
                                  -2,
                                  FRA2TIME(seq->startdisp),
                                  FPS,
                                  USER_TIMECODE_SUBRIP);
    BLI_timecode_string_from_time(timecode_str_end,
                                  sizeof(timecode_str_end),
                                  -2,
                                  FRA2TIME(seq->enddisp),
                                  FPS,
                                  USER_TIMECODE_SUBRIP);

    fprintf(
        file, "%d\n%s --> %s\n%s\n\n", iter++, timecode_str_start, timecode_str_end, data->text);

    seq_next = seq->next;
    MEM_freeN(seq);
  }

  fclose(file);

  return OPERATOR_FINISHED;
}

static bool sequencer_strip_is_text_poll(bContext *C)
{
  Editing *ed;
  Sequence *seq;
  return (((ed = SEQ_editing_get(CTX_data_scene(C), false)) != NULL) &&
          ((seq = ed->act_seq) != NULL) && (seq->type == SEQ_TYPE_TEXT));
}

void SEQUENCER_OT_export_subtitles(struct wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Export Subtitles";
  ot->idname = "SEQUENCER_OT_export_subtitles";
  ot->description = "Export .srt file containing text strips";

  /* Api callbacks. */
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

static int sequencer_set_range_to_strips_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene, false);
  Sequence *seq;

  int sfra = MAXFRAME;
  int efra = -MAXFRAME;
  bool selected = false;
  const bool preview = RNA_boolean_get(op->ptr, "preview");

  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    if (seq->flag & SELECT) {
      selected = true;
      sfra = min_ii(sfra, seq->startdisp);
      efra = max_ii(efra, seq->enddisp - 1);
    }
  }

  if (!selected) {
    BKE_report(op->reports, RPT_WARNING, "Select one or more strips");
    return OPERATOR_CANCELLED;
  }
  if (efra < 0) {
    BKE_report(op->reports, RPT_ERROR, "Can't set a negative range");
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

  WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_set_range_to_strips(struct wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers. */
  ot->name = "Set Range to Strips";
  ot->idname = "SEQUENCER_OT_set_range_to_strips";
  ot->description = "Set the frame range to the selected strips start and end";

  /* Api callbacks. */
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
    {0, NULL, 0, NULL, NULL},
};

static int sequencer_strip_transform_clear_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  const Editing *ed = SEQ_editing_get(scene, false);
  Sequence *seq;
  const int property = RNA_enum_get(op->ptr, "property");

  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    if (seq->flag & SELECT && seq->type != SEQ_TYPE_SOUND_RAM) {
      StripTransform *transform = seq->strip->transform;
      switch (property) {
        case STRIP_TRANSFORM_POSITION:
          transform->xofs = 0;
          transform->yofs = 0;
          break;
        case STRIP_TRANSFORM_SCALE:
          transform->scale_x = 1.0f;
          transform->scale_y = 1.0f;
          break;
        case STRIP_TRANSFORM_ROTATION:
          transform->rotation = 0.0f;
          break;
        case STRIP_TRANSFORM_ALL:
          transform->xofs = 0;
          transform->yofs = 0;
          transform->scale_x = 1.0f;
          transform->scale_y = 1.0f;
          transform->rotation = 0.0f;
          break;
      }
      SEQ_relations_invalidate_cache_preprocessed(scene, seq);
    }
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_strip_transform_clear(struct wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Clear Strip Transform";
  ot->idname = "SEQUENCER_OT_strip_transform_clear";
  ot->description = "Reset image transformation to default value";

  /* Api callbacks. */
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

static const EnumPropertyItem scale_fit_methods[] = {
    {SEQ_SCALE_TO_FIT, "FIT", 0, "Scale to Fit", "Scale image so fits in preview"},
    {SEQ_SCALE_TO_FILL, "FILL", 0, "Scale to Fill", "Scale image so it fills preview completely"},
    {SEQ_STRETCH_TO_FILL, "STRETCH", 0, "Stretch to Fill", "Stretch image so it fills preview"},
    {0, NULL, 0, NULL, NULL},
};

static int sequencer_strip_transform_fit_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  const Editing *ed = SEQ_editing_get(scene, false);
  Sequence *seq;
  const eSeqImageFitMethod fit_method = RNA_enum_get(op->ptr, "fit_method");

  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    if (seq->flag & SELECT && seq->type != SEQ_TYPE_SOUND_RAM) {
      const int timeline_frame = CFRA;
      StripElem *strip_elem = SEQ_render_give_stripelem(seq, timeline_frame);

      if (strip_elem == NULL) {
        continue;
      }

      SEQ_set_scale_to_fit(seq,
                           strip_elem->orig_width,
                           strip_elem->orig_height,
                           scene->r.xsch,
                           scene->r.ysch,
                           fit_method);
      SEQ_relations_invalidate_cache_preprocessed(scene, seq);
    }
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_strip_transform_fit(struct wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Strip Transform Set Fit";
  ot->idname = "SEQUENCER_OT_strip_transform_fit";

  /* Api callbacks. */
  ot->exec = sequencer_strip_transform_fit_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(ot->srna,
                          "fit_method",
                          scale_fit_methods,
                          SEQ_SCALE_TO_FIT,
                          "Fit Method",
                          "Scale fit fit_method");
}

/** \} */
