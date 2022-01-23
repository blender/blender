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
 * \ingroup edtransform
 */

#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "ED_markers.h"

#include "SEQ_animation.h"
#include "SEQ_edit.h"
#include "SEQ_effects.h"
#include "SEQ_iterator.h"
#include "SEQ_relations.h"
#include "SEQ_sequencer.h"
#include "SEQ_time.h"
#include "SEQ_transform.h"
#include "SEQ_utils.h"

#include "UI_view2d.h"

#include "transform.h"
#include "transform_convert.h"

/** Used for sequencer transform. */
typedef struct TransDataSeq {
  struct Sequence *seq;
  /** A copy of #Sequence.flag that may be modified for nested strips. */
  int flag;
  /** Use this so we can have transform data at the strips start,
   * but apply correctly to the start frame. */
  int start_offset;
  /** one of #SELECT, #SEQ_LEFTSEL and #SEQ_RIGHTSEL. */
  short sel_flag;

} TransDataSeq;

/**
 * Sequencer transform customdata (stored in #TransCustomDataContainer).
 */
typedef struct TransSeq {
  TransDataSeq *tdseq;
  int selection_channel_range_min;
  int selection_channel_range_max;
} TransSeq;

/* -------------------------------------------------------------------- */
/** \name Sequencer Transform Creation
 * \{ */

/* This function applies the rules for transforming a strip so duplicate
 * checks don't need to be added in multiple places.
 *
 * count and flag MUST be set.
 */
static void SeqTransInfo(TransInfo *t, Sequence *seq, int *r_count, int *r_flag)
{
  /* for extend we need to do some tricks */
  if (t->mode == TFM_TIME_EXTEND) {

    /* *** Extend Transform *** */

    Scene *scene = t->scene;
    int cfra = CFRA;
    int left = SEQ_transform_get_left_handle_frame(seq);
    int right = SEQ_transform_get_right_handle_frame(seq);

    if (((seq->flag & SELECT) == 0 || (seq->flag & SEQ_LOCK))) {
      *r_count = 0;
      *r_flag = 0;
    }
    else {
      *r_count = 1; /* unless its set to 0, extend will never set 2 handles at once */
      *r_flag = (seq->flag | SELECT) & ~(SEQ_LEFTSEL | SEQ_RIGHTSEL);

      if (t->frame_side == 'R') {
        if (right <= cfra) {
          *r_count = *r_flag = 0;
        } /* ignore */
        else if (left > cfra) {
        } /* keep the selection */
        else {
          *r_flag |= SEQ_RIGHTSEL;
        }
      }
      else {
        if (left >= cfra) {
          *r_count = *r_flag = 0;
        } /* ignore */
        else if (right < cfra) {
        } /* keep the selection */
        else {
          *r_flag |= SEQ_LEFTSEL;
        }
      }
    }
  }
  else {

    t->frame_side = 'B';

    /* *** Normal Transform *** */

    /* Count */

    /* Non nested strips (resect selection and handles) */
    if ((seq->flag & SELECT) == 0 || (seq->flag & SEQ_LOCK)) {
      *r_count = 0;
      *r_flag = 0;
    }
    else {
      if ((seq->flag & (SEQ_LEFTSEL | SEQ_RIGHTSEL)) == (SEQ_LEFTSEL | SEQ_RIGHTSEL)) {
        *r_flag = seq->flag;
        *r_count = 2; /* we need 2 transdata's */
      }
      else {
        *r_flag = seq->flag;
        *r_count = 1; /* selected or with a handle selected */
      }
    }
  }
}

static int SeqTransCount(TransInfo *t, ListBase *seqbase)
{
  Sequence *seq;
  int tot = 0, count, flag;

  for (seq = seqbase->first; seq; seq = seq->next) {
    SeqTransInfo(t, seq, &count, &flag); /* ignore the flag */
    tot += count;
  }

  return tot;
}

static TransData *SeqToTransData(
    TransData *td, TransData2D *td2d, TransDataSeq *tdsq, Sequence *seq, int flag, int sel_flag)
{
  int start_left;

  switch (sel_flag) {
    case SELECT:
      /* Use seq_tx_get_final_left() and an offset here
       * so transform has the left hand location of the strip.
       * tdsq->start_offset is used when flushing the tx data back */
      start_left = SEQ_transform_get_left_handle_frame(seq);
      td2d->loc[0] = start_left;
      tdsq->start_offset = start_left - seq->start; /* use to apply the original location */
      break;
    case SEQ_LEFTSEL:
      start_left = SEQ_transform_get_left_handle_frame(seq);
      td2d->loc[0] = start_left;
      break;
    case SEQ_RIGHTSEL:
      td2d->loc[0] = SEQ_transform_get_right_handle_frame(seq);
      break;
  }

  td2d->loc[1] = seq->machine; /* channel - Y location */
  td2d->loc[2] = 0.0f;
  td2d->loc2d = NULL;

  tdsq->seq = seq;

  /* Use instead of seq->flag for nested strips and other
   * cases where the selection may need to be modified */
  tdsq->flag = flag;
  tdsq->sel_flag = sel_flag;

  td->extra = (void *)tdsq; /* allow us to update the strip from here */

  td->flag = 0;
  td->loc = td2d->loc;
  copy_v3_v3(td->center, td->loc);
  copy_v3_v3(td->iloc, td->loc);

  memset(td->axismtx, 0, sizeof(td->axismtx));
  td->axismtx[2][2] = 1.0f;

  td->ext = NULL;
  td->val = NULL;

  td->flag |= TD_SELECTED;
  td->dist = 0.0;

  unit_m3(td->mtx);
  unit_m3(td->smtx);

  /* Time Transform (extend) */
  td->val = td2d->loc;
  td->ival = td2d->loc[0];

  return td;
}

static int SeqToTransData_build(
    TransInfo *t, ListBase *seqbase, TransData *td, TransData2D *td2d, TransDataSeq *tdsq)
{
  Sequence *seq;
  int count, flag;
  int tot = 0;

  for (seq = seqbase->first; seq; seq = seq->next) {

    SeqTransInfo(t, seq, &count, &flag);

    /* use 'flag' which is derived from seq->flag but modified for special cases */
    if (flag & SELECT) {
      if (flag & (SEQ_LEFTSEL | SEQ_RIGHTSEL)) {
        if (flag & SEQ_LEFTSEL) {
          SeqToTransData(td++, td2d++, tdsq++, seq, flag, SEQ_LEFTSEL);
          tot++;
        }
        if (flag & SEQ_RIGHTSEL) {
          SeqToTransData(td++, td2d++, tdsq++, seq, flag, SEQ_RIGHTSEL);
          tot++;
        }
      }
      else {
        SeqToTransData(td++, td2d++, tdsq++, seq, flag, SELECT);
        tot++;
      }
    }
  }
  return tot;
}

static void free_transform_custom_data(TransCustomData *custom_data)
{
  if ((custom_data->data != NULL) && custom_data->use_free) {
    TransSeq *ts = custom_data->data;
    MEM_freeN(ts->tdseq);
    MEM_freeN(custom_data->data);
    custom_data->data = NULL;
  }
}

/* Canceled, need to update the strips display. */
static void seq_transform_cancel(TransInfo *t, SeqCollection *transformed_strips)
{
  ListBase *seqbase = SEQ_active_seqbase_get(SEQ_editing_get(t->scene));

  Sequence *seq;
  SEQ_ITERATOR_FOREACH (seq, transformed_strips) {
    /* Handle pre-existing overlapping strips even when operator is canceled.
     * This is necessary for SEQUENCER_OT_duplicate_move macro for example. */
    if (SEQ_transform_test_overlap(seqbase, seq)) {
      SEQ_transform_seqbase_shuffle(seqbase, seq, t->scene);
    }

    SEQ_time_update_sequence(t->scene, seqbase, seq);
  }
}

static bool seq_transform_check_overlap(SeqCollection *transformed_strips)
{
  Sequence *seq;
  SEQ_ITERATOR_FOREACH (seq, transformed_strips) {
    if (seq->flag & SEQ_OVERLAP) {
      return true;
    }
  }
  return false;
}

static SeqCollection *extract_standalone_strips(SeqCollection *transformed_strips)
{
  SeqCollection *collection = SEQ_collection_create(__func__);
  Sequence *seq;
  SEQ_ITERATOR_FOREACH (seq, transformed_strips) {
    if ((seq->type & SEQ_TYPE_EFFECT) == 0 || seq->seq1 == NULL) {
      SEQ_collection_append_strip(seq, collection);
    }
  }
  return collection;
}

/* Query strips positioned after left edge of transformed strips bound-box. */
static SeqCollection *query_right_side_strips(ListBase *seqbase, SeqCollection *transformed_strips)
{
  int minframe = MAXFRAME;
  {
    Sequence *seq;
    SEQ_ITERATOR_FOREACH (seq, transformed_strips) {
      minframe = min_ii(minframe, seq->startdisp);
    }
  }

  SeqCollection *collection = SEQ_collection_create(__func__);
  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    if ((seq->flag & SELECT) == 0 && seq->startdisp >= minframe) {
      SEQ_collection_append_strip(seq, collection);
    }
  }
  return collection;
}

static void seq_transform_update_effects(TransInfo *t, SeqCollection *collection)
{
  Sequence *seq;
  SEQ_ITERATOR_FOREACH (seq, collection) {
    if ((seq->type & SEQ_TYPE_EFFECT) && (seq->seq1 || seq->seq2 || seq->seq3)) {
      ListBase *seqbase = SEQ_active_seqbase_get(SEQ_editing_get(t->scene));
      SEQ_time_update_sequence(t->scene, seqbase, seq);
    }
  }
}

/* Check if effect strips with input are transformed. */
static bool seq_transform_check_strip_effects(SeqCollection *transformed_strips)
{
  Sequence *seq;
  SEQ_ITERATOR_FOREACH (seq, transformed_strips) {
    if ((seq->type & SEQ_TYPE_EFFECT) && (seq->seq1 || seq->seq2 || seq->seq3)) {
      return true;
    }
  }
  return false;
}

static ListBase *seqbase_active_get(const TransInfo *t)
{
  Editing *ed = SEQ_editing_get(t->scene);
  return SEQ_active_seqbase_get(ed);
}

/* Offset all strips positioned after left edge of transformed strips bound-box by amount equal
 * to overlap of transformed strips. */
static void seq_transform_handle_expand_to_fit(TransInfo *t, SeqCollection *transformed_strips)
{
  ListBase *seqbasep = seqbase_active_get(t);
  ListBase *markers = &t->scene->markers;
  const bool use_sync_markers = (((SpaceSeq *)t->area->spacedata.first)->flag &
                                 SEQ_MARKER_TRANS) != 0;

  SeqCollection *right_side_strips = query_right_side_strips(seqbasep, transformed_strips);

  /* Temporarily move right side strips beyond timeline boundary. */
  Sequence *seq;
  SEQ_ITERATOR_FOREACH (seq, right_side_strips) {
    seq->machine += MAXSEQ * 2;
  }

  /* Shuffle transformed standalone strips. This is because transformed strips can overlap with
   * strips on left side. */
  SeqCollection *standalone_strips = extract_standalone_strips(transformed_strips);
  SEQ_transform_seqbase_shuffle_time(
      standalone_strips, seqbasep, t->scene, markers, use_sync_markers);
  SEQ_collection_free(standalone_strips);

  /* Move temporarily moved strips back to their original place and tag for shuffling. */
  SEQ_ITERATOR_FOREACH (seq, right_side_strips) {
    seq->machine -= MAXSEQ * 2;
  }
  /* Shuffle again to displace strips on right side. Final effect shuffling is done in
   * seq_transform_handle_overlap. */
  SEQ_transform_seqbase_shuffle_time(
      right_side_strips, seqbasep, t->scene, markers, use_sync_markers);
  seq_transform_update_effects(t, right_side_strips);
  SEQ_collection_free(right_side_strips);
}

static SeqCollection *query_overwrite_targets(const TransInfo *t,
                                              SeqCollection *transformed_strips)
{
  SeqCollection *collection = SEQ_query_unselected_strips(seqbase_active_get(t));

  Sequence *seq, *seq_transformed;
  SEQ_ITERATOR_FOREACH (seq, collection) {
    bool does_overlap = false;

    SEQ_ITERATOR_FOREACH (seq_transformed, transformed_strips) {
      /* Effects of transformed strips can be unselected. These must not be included. */
      if (seq == seq_transformed) {
        SEQ_collection_remove_strip(seq, collection);
      }
      if (SEQ_transform_test_overlap_seq_seq(seq, seq_transformed)) {
        does_overlap = true;
      }
    }

    if (!does_overlap) {
      SEQ_collection_remove_strip(seq, collection);
    }
  }

  return collection;
}

typedef enum eOvelapDescrition {
  /* No overlap. */
  STRIP_OVERLAP_NONE,
  /* Overlapping strip covers overlapped completely. */
  STRIP_OVERLAP_IS_FULL,
  /* Overlapping strip is inside overlapped. */
  STRIP_OVERLAP_IS_INSIDE,
  /* Partial overlap between 2 strips. */
  STRIP_OVERLAP_LEFT_SIDE,
  STRIP_OVERLAP_RIGHT_SIDE,
} eOvelapDescrition;

static eOvelapDescrition overlap_description_get(const Sequence *transformed,
                                                 const Sequence *target)
{
  if (transformed->startdisp <= target->startdisp && transformed->enddisp >= target->enddisp) {
    return STRIP_OVERLAP_IS_FULL;
  }
  if (transformed->startdisp > target->startdisp && transformed->enddisp < target->enddisp) {
    return STRIP_OVERLAP_IS_INSIDE;
  }
  if (transformed->startdisp <= target->startdisp && target->startdisp <= transformed->enddisp) {
    return STRIP_OVERLAP_LEFT_SIDE;
  }
  if (transformed->startdisp <= target->enddisp && target->enddisp <= transformed->enddisp) {
    return STRIP_OVERLAP_RIGHT_SIDE;
  }
  return STRIP_OVERLAP_NONE;
}

/* Split strip in 3 parts, remove middle part and fit transformed inside. */
static void seq_transform_handle_overwrite_split(const TransInfo *t,
                                                 const Sequence *transformed,
                                                 Sequence *target)
{
  Main *bmain = CTX_data_main(t->context);
  Scene *scene = t->scene;
  ListBase *seqbase = seqbase_active_get(t);

  Sequence *split_strip = SEQ_edit_strip_split(
      bmain, scene, seqbase, target, transformed->startdisp, SEQ_SPLIT_SOFT, NULL);
  SEQ_edit_strip_split(
      bmain, scene, seqbase, split_strip, transformed->enddisp, SEQ_SPLIT_SOFT, NULL);
  SEQ_edit_flag_for_removal(scene, seqbase_active_get(t), split_strip);
  SEQ_edit_remove_flagged_sequences(t->scene, seqbase_active_get(t));
}

/* Trim strips by adjusting handle position.
 * This is bit more complicated in case overlap happens on effect. */
static void seq_transform_handle_overwrite_trim(const TransInfo *t,
                                                const Sequence *transformed,
                                                Sequence *target,
                                                const eOvelapDescrition overlap)
{
  SeqCollection *targets = SEQ_query_by_reference(
      target, seqbase_active_get(t), SEQ_query_strip_effect_chain);

  /* Expand collection by adding all target's children, effects and their children. */
  if ((target->type & SEQ_TYPE_EFFECT) != 0) {
    SEQ_collection_expand(seqbase_active_get(t), targets, SEQ_query_strip_effect_chain);
  }

  /* Trim all non effects, that have influence on effect length which is overlapping. */
  Sequence *seq;
  SEQ_ITERATOR_FOREACH (seq, targets) {
    if ((seq->type & SEQ_TYPE_EFFECT) != 0 && SEQ_effect_get_num_inputs(seq->type) > 0) {
      continue;
    }
    if (overlap == STRIP_OVERLAP_LEFT_SIDE) {
      SEQ_transform_set_left_handle_frame(seq, transformed->enddisp);
    }
    else {
      BLI_assert(overlap == STRIP_OVERLAP_RIGHT_SIDE);
      SEQ_transform_set_right_handle_frame(seq, transformed->startdisp);
    }

    ListBase *seqbase = SEQ_active_seqbase_get(SEQ_editing_get(t->scene));
    SEQ_time_update_sequence(t->scene, seqbase, seq);
  }
  SEQ_collection_free(targets);
}

static void seq_transform_handle_overwrite(const TransInfo *t, SeqCollection *transformed_strips)
{
  SeqCollection *targets = query_overwrite_targets(t, transformed_strips);
  SeqCollection *strips_to_delete = SEQ_collection_create(__func__);

  Sequence *target;
  Sequence *transformed;
  SEQ_ITERATOR_FOREACH (target, targets) {
    SEQ_ITERATOR_FOREACH (transformed, transformed_strips) {
      if (transformed->machine != target->machine) {
        continue;
      }

      const eOvelapDescrition overlap = overlap_description_get(transformed, target);

      if (overlap == STRIP_OVERLAP_IS_FULL) {
        SEQ_collection_append_strip(target, strips_to_delete);
      }
      else if (overlap == STRIP_OVERLAP_IS_INSIDE) {
        seq_transform_handle_overwrite_split(t, transformed, target);
      }
      else if (ELEM(overlap, STRIP_OVERLAP_LEFT_SIDE, STRIP_OVERLAP_RIGHT_SIDE)) {
        seq_transform_handle_overwrite_trim(t, transformed, target, overlap);
      }
    }
  }

  SEQ_collection_free(targets);

  /* Remove covered strips. This must be done in separate loop, because `SEQ_edit_strip_split()`
   * also uses `SEQ_edit_remove_flagged_sequences()`. See T91096. */
  if (SEQ_collection_len(strips_to_delete) > 0) {
    Sequence *seq;
    SEQ_ITERATOR_FOREACH (seq, strips_to_delete) {
      SEQ_edit_flag_for_removal(t->scene, seqbase_active_get(t), seq);
    }
    SEQ_edit_remove_flagged_sequences(t->scene, seqbase_active_get(t));
  }
  SEQ_collection_free(strips_to_delete);
}

static void seq_transform_handle_overlap_shuffle(const TransInfo *t,
                                                 SeqCollection *transformed_strips)
{
  ListBase *seqbase = seqbase_active_get(t);
  ListBase *markers = &t->scene->markers;
  const bool use_sync_markers = (((SpaceSeq *)t->area->spacedata.first)->flag &
                                 SEQ_MARKER_TRANS) != 0;
  /* Shuffle non strips with no effects attached. */
  SeqCollection *standalone_strips = extract_standalone_strips(transformed_strips);
  SEQ_transform_seqbase_shuffle_time(
      standalone_strips, seqbase, t->scene, markers, use_sync_markers);
  SEQ_collection_free(standalone_strips);
}

static void seq_transform_handle_overlap(TransInfo *t, SeqCollection *transformed_strips)
{
  ListBase *seqbasep = seqbase_active_get(t);
  const eSeqOverlapMode overlap_mode = SEQ_tool_settings_overlap_mode_get(t->scene);

  switch (overlap_mode) {
    case SEQ_OVERLAP_EXPAND:
      seq_transform_handle_expand_to_fit(t, transformed_strips);
      break;
    case SEQ_OVERLAP_OVERWRITE:
      seq_transform_handle_overwrite(t, transformed_strips);
      break;
    case SEQ_OVERLAP_SHUFFLE:
      seq_transform_handle_overlap_shuffle(t, transformed_strips);
      break;
  }

  if (seq_transform_check_strip_effects(transformed_strips)) {
    /* Update effect strips based on strips just moved in time. */
    seq_transform_update_effects(t, transformed_strips);
  }

  /* If any effects still overlap, we need to move them up.
   * In some cases other strips can be overlapping still, see T90646. */
  Sequence *seq;
  SEQ_ITERATOR_FOREACH (seq, transformed_strips) {
    if (SEQ_transform_test_overlap(seqbasep, seq)) {
      SEQ_transform_seqbase_shuffle(seqbasep, seq, t->scene);
    }
    seq->flag &= ~SEQ_OVERLAP;
  }
}

static SeqCollection *seq_transform_collection_from_transdata(TransDataContainer *tc)
{
  SeqCollection *collection = SEQ_collection_create(__func__);
  TransData *td = tc->data;
  for (int a = 0; a < tc->data_len; a++, td++) {
    Sequence *seq = ((TransDataSeq *)td->extra)->seq;
    SEQ_collection_append_strip(seq, collection);
  }
  return collection;
}

static void freeSeqData(TransInfo *t, TransDataContainer *tc, TransCustomData *custom_data)
{
  Editing *ed = SEQ_editing_get(t->scene);
  if (ed == NULL) {
    free_transform_custom_data(custom_data);
    return;
  }

  SeqCollection *transformed_strips = seq_transform_collection_from_transdata(tc);
  SEQ_collection_expand(seqbase_active_get(t), transformed_strips, SEQ_query_strip_effect_chain);

  if (t->state == TRANS_CANCEL) {
    seq_transform_cancel(t, transformed_strips);
    SEQ_collection_free(transformed_strips);
    free_transform_custom_data(custom_data);
    return;
  }

  if (seq_transform_check_overlap(transformed_strips)) {
    seq_transform_handle_overlap(t, transformed_strips);
  }

  seq_transform_update_effects(t, transformed_strips);
  SEQ_collection_free(transformed_strips);

  SEQ_sort(ed->seqbasep);
  DEG_id_tag_update(&t->scene->id, ID_RECALC_SEQUENCER_STRIPS);
  free_transform_custom_data(custom_data);
}

void createTransSeqData(TransInfo *t)
{
#define XXX_DURIAN_ANIM_TX_HACK

  Scene *scene = t->scene;
  Editing *ed = SEQ_editing_get(t->scene);
  TransData *td = NULL;
  TransData2D *td2d = NULL;
  TransDataSeq *tdsq = NULL;
  TransSeq *ts = NULL;

  int count = 0;

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  if (ed == NULL) {
    tc->data_len = 0;
    return;
  }

  tc->custom.type.free_cb = freeSeqData;
  t->frame_side = transform_convert_frame_side_dir_get(t, (float)CFRA);

#ifdef XXX_DURIAN_ANIM_TX_HACK
  {
    Sequence *seq;
    for (seq = ed->seqbasep->first; seq; seq = seq->next) {
      /* hack */
      if ((seq->flag & SELECT) == 0 && seq->type & SEQ_TYPE_EFFECT) {
        Sequence *seq_user;
        int i;
        for (i = 0; i < 3; i++) {
          seq_user = *((&seq->seq1) + i);
          if (seq_user && (seq_user->flag & SELECT) && !(seq_user->flag & SEQ_LOCK) &&
              !(seq_user->flag & (SEQ_LEFTSEL | SEQ_RIGHTSEL))) {
            seq->flag |= SELECT;
          }
        }
      }
    }
  }
#endif

  count = SeqTransCount(t, ed->seqbasep);

  /* allocate memory for data */
  tc->data_len = count;

  /* stop if trying to build list if nothing selected */
  if (count == 0) {
    return;
  }

  tc->custom.type.data = ts = MEM_callocN(sizeof(TransSeq), "transseq");
  tc->custom.type.use_free = true;
  td = tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransSeq TransData");
  td2d = tc->data_2d = MEM_callocN(tc->data_len * sizeof(TransData2D), "TransSeq TransData2D");
  ts->tdseq = tdsq = MEM_callocN(tc->data_len * sizeof(TransDataSeq), "TransSeq TransDataSeq");

  /* loop 2: build transdata array */
  SeqToTransData_build(t, ed->seqbasep, td, td2d, tdsq);

  ts->selection_channel_range_min = MAXSEQ + 1;
  LISTBASE_FOREACH (Sequence *, seq, SEQ_active_seqbase_get(ed)) {
    if ((seq->flag & SELECT) != 0) {
      ts->selection_channel_range_min = min_ii(ts->selection_channel_range_min, seq->machine);
      ts->selection_channel_range_max = max_ii(ts->selection_channel_range_max, seq->machine);
    }
  }

#undef XXX_DURIAN_ANIM_TX_HACK
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UVs Transform Flush
 * \{ */

/* commented _only_ because the meta may have animation data which
 * needs moving too T28158. */

BLI_INLINE void trans_update_seq(Scene *sce, Sequence *seq, int old_start, int sel_flag)
{
  /* Calculate this strip and all nested strips.
   * Children are ALWAYS transformed first so we don't need to do this in another loop.
   */

  ListBase *seqbase = SEQ_active_seqbase_get(SEQ_editing_get(sce));
  SEQ_time_update_sequence(sce, seqbase, seq);

  if (sel_flag == SELECT) {
    SEQ_offset_animdata(sce, seq, seq->start - old_start);
  }
}

static void flushTransSeq(TransInfo *t)
{
  /* Editing null check already done */
  ListBase *seqbasep = seqbase_active_get(t);

  int a, new_frame;
  TransData *td = NULL;
  TransData2D *td2d = NULL;
  TransDataSeq *tdsq = NULL;
  Sequence *seq;

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  /* Flush to 2D vector from internally used 3D vector. */
  for (a = 0, td = tc->data, td2d = tc->data_2d; a < tc->data_len; a++, td++, td2d++) {
    tdsq = (TransDataSeq *)td->extra;
    seq = tdsq->seq;
    new_frame = round_fl_to_int(td2d->loc[0]);

    switch (tdsq->sel_flag) {
      case SELECT:
        if (SEQ_transform_sequence_can_be_translated(seq)) {
          const int offset = new_frame - tdsq->start_offset - seq->start;
          SEQ_transform_translate_sequence(t->scene, seq, offset);
        }
        seq->machine = round_fl_to_int(td2d->loc[1]);
        CLAMP(seq->machine, 1, MAXSEQ);
        break;

      case SEQ_LEFTSEL: /* No vertical transform. */
        SEQ_transform_set_left_handle_frame(seq, new_frame);
        SEQ_transform_handle_xlimits(seq, tdsq->flag & SEQ_LEFTSEL, tdsq->flag & SEQ_RIGHTSEL);
        SEQ_transform_fix_single_image_seq_offsets(seq);
        SEQ_time_update_sequence(t->scene, seqbasep, seq);
        break;
      case SEQ_RIGHTSEL: /* No vertical transform. */
        SEQ_transform_set_right_handle_frame(seq, new_frame);
        SEQ_transform_handle_xlimits(seq, tdsq->flag & SEQ_LEFTSEL, tdsq->flag & SEQ_RIGHTSEL);
        SEQ_transform_fix_single_image_seq_offsets(seq);
        SEQ_time_update_sequence(t->scene, seqbasep, seq);
        break;
    }
  }

  /* Update all effects. */
  if (ELEM(t->mode, TFM_SEQ_SLIDE, TFM_TIME_TRANSLATE)) {
    for (seq = seqbasep->first; seq; seq = seq->next) {
      if (seq->seq1 || seq->seq2 || seq->seq3) {
        SEQ_time_update_sequence(t->scene, seqbasep, seq);
      }
    }
  }

  /* need to do the overlap check in a new loop otherwise adjacent strips
   * will not be updated and we'll get false positives */
  SeqCollection *transformed_strips = seq_transform_collection_from_transdata(tc);
  SEQ_collection_expand(seqbase_active_get(t), transformed_strips, SEQ_query_strip_effect_chain);

  SEQ_ITERATOR_FOREACH (seq, transformed_strips) {
    /* test overlap, displays red outline */
    seq->flag &= ~SEQ_OVERLAP;
    if (SEQ_transform_test_overlap(seqbasep, seq)) {
      seq->flag |= SEQ_OVERLAP;
    }
  }
  SEQ_collection_free(transformed_strips);
}

void recalcData_sequencer(TransInfo *t)
{
  TransData *td;
  int a;
  Sequence *seq_prev = NULL;

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  for (a = 0, td = tc->data; a < tc->data_len; a++, td++) {
    TransDataSeq *tdsq = (TransDataSeq *)td->extra;
    Sequence *seq = tdsq->seq;

    if (seq != seq_prev) {
      SEQ_relations_invalidate_cache_composite(t->scene, seq);
    }

    seq_prev = seq;
  }

  DEG_id_tag_update(&t->scene->id, ID_RECALC_SEQUENCER_STRIPS);

  flushTransSeq(t);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Special After Transform Sequencer
 * \{ */

void special_aftertrans_update__sequencer(bContext *UNUSED(C), TransInfo *t)
{
  if (t->state == TRANS_CANCEL) {
    return;
  }
  /* freeSeqData in transform_conversions.c does this
   * keep here so the else at the end won't run... */

  SpaceSeq *sseq = (SpaceSeq *)t->area->spacedata.first;

  /* Marker transform, not especially nice but we may want to move markers
   * at the same time as strips in the Video Sequencer. */
  if (sseq->flag & SEQ_MARKER_TRANS) {
    /* can't use TFM_TIME_EXTEND
     * for some reason EXTEND is changed into TRANSLATE, so use frame_side instead */

    if (t->mode == TFM_SEQ_SLIDE) {
      if (t->frame_side == 'B') {
        ED_markers_post_apply_transform(
            &t->scene->markers, t->scene, TFM_TIME_TRANSLATE, t->values[0], t->frame_side);
      }
    }
    else if (ELEM(t->frame_side, 'L', 'R')) {
      ED_markers_post_apply_transform(
          &t->scene->markers, t->scene, TFM_TIME_EXTEND, t->values[0], t->frame_side);
    }
  }
}

void transform_convert_sequencer_channel_clamp(TransInfo *t, float r_val[2])
{
  const TransSeq *ts = (TransSeq *)TRANS_DATA_CONTAINER_FIRST_SINGLE(t)->custom.type.data;
  const int channel_offset = round_fl_to_int(r_val[1]);
  const int min_channel_after_transform = ts->selection_channel_range_min + channel_offset;
  const int max_channel_after_transform = ts->selection_channel_range_max + channel_offset;

  if (max_channel_after_transform > MAXSEQ) {
    r_val[1] -= max_channel_after_transform - MAXSEQ;
  }
  if (min_channel_after_transform < 1) {
    r_val[1] -= min_channel_after_transform - 1;
  }
}

/** \} */
