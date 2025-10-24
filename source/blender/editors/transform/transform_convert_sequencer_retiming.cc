/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "MEM_guardedalloc.h"

#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h"

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_rect.h"

#include "BKE_context.hh"

#include "SEQ_iterator.hh"
#include "SEQ_relations.hh"
#include "SEQ_retiming.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_transform.hh"

#include "transform.hh"
#include "transform_convert.hh"

namespace blender::ed::transform {

/** Used for sequencer transform. */
struct TransDataSeq {
  Strip *strip;
  int orig_timeline_frame;
  int key_index; /* Some actions may need to destroy original data, use index to access it. */
};

struct TransSeq {
  TransDataSeq *tdseq;
  /* Maximum delta allowed before clamping selected retiming keys. Always active. */
  rcti offset_clamp;
};

static TransData *SeqToTransData(const Scene *scene,
                                 Strip *strip,
                                 const SeqRetimingKey *key,
                                 TransData *td,
                                 TransData2D *td2d,
                                 TransDataSeq *tdseq)
{

  td2d->loc[0] = seq::retiming_key_timeline_frame_get(scene, strip, key);
  td2d->loc[1] = key->retiming_factor;
  td2d->loc2d = nullptr;
  td->loc = td2d->loc;
  copy_v3_v3(td->iloc, td->loc);
  copy_v3_v3(td->center, td->loc);
  memset(td->axismtx, 0, sizeof(td->axismtx));
  td->axismtx[2][2] = 1.0f;
  unit_m3(td->mtx);
  unit_m3(td->smtx);

  tdseq->strip = strip;
  tdseq->orig_timeline_frame = seq::retiming_key_timeline_frame_get(scene, strip, key);
  tdseq->key_index = seq::retiming_key_index_get(strip, key);

  td->extra = static_cast<void *>(tdseq);
  td->flag |= TD_SELECTED;
  td->dist = 0.0;

  return td;
}

static void freeSeqData(TransInfo *t, TransDataContainer *tc, TransCustomData *custom_data)
{
  const TransData *const td = tc->data;
  Scene *scene = t->scene;
  const Editing *ed = seq::editing_get(t->scene);

  /* Handle overlapping strips. */

  VectorSet<Strip *> transformed_strips;
  for (int i = 0; i < tc->data_len; i++) {
    Strip *strip = ((TransDataSeq *)(td + i)->extra)->strip;
    transformed_strips.add(strip);
  }

  ListBase *seqbasep = seq::active_seqbase_get(ed);
  seq::iterator_set_expand(scene, seqbasep, transformed_strips, seq::query_strip_effect_chain);

  VectorSet<Strip *> dependant;
  dependant.add_multiple(transformed_strips);
  dependant.remove_if([&](Strip *strip) { return seq::transform_strip_can_be_translated(strip); });

  if (seq_transform_check_overlap(transformed_strips)) {
    const bool use_sync_markers = (((SpaceSeq *)t->area->spacedata.first)->flag &
                                   SEQ_MARKER_TRANS) != 0;
    seq::transform_handle_overlap(
        scene, seqbasep, transformed_strips, dependant, use_sync_markers);
  }

  if ((custom_data->data != nullptr) && custom_data->use_free) {
    TransSeq *ts = static_cast<TransSeq *>(custom_data->data);
    MEM_freeN(ts->tdseq);
    MEM_delete(ts);
    custom_data->data = nullptr;
  }
}

static void create_trans_seq_clamp_data(TransInfo *t, const Scene *scene)
{
  TransSeq *ts = (TransSeq *)TRANS_DATA_CONTAINER_FIRST_SINGLE(t)->custom.type.data;
  const Editing *ed = seq::editing_get(scene);

  /* Prevent snaps and change in `values` past `offset_clamp` for all selected retiming keys. */
  BLI_rcti_init(&ts->offset_clamp, -INT_MAX, INT_MAX, 0, 0);

  blender::Map selection = seq::retiming_selection_get(ed);
  for (auto item : selection.items()) {
    SeqRetimingKey *key = item.key;

    /* Transition retiming key. */
    if (seq::retiming_key_is_transition_type(key) &&
        !seq::retiming_selection_has_whole_transition(ed, key))
    {
      SeqRetimingKey *key_start = seq::retiming_transition_start_get(key);
      SeqRetimingKey *key_end = key_start + 1;
      SeqRetimingKey *key_prev = key_start - 1;
      SeqRetimingKey *key_next = key_end + 1;

      const float midpoint = key_start->original_strip_frame_index;

      /* Ensure start transition key cannot pass the previous key, or linked end transition key
       * cannot pass the next key. This transform behavior is symmetrical and limited by the
       * smallest distance between keys. */
      const int max_offset = min_ii(key_start->strip_frame_index - key_prev->strip_frame_index - 1,
                                    key_next->strip_frame_index - key_end->strip_frame_index - 1);

      if (key_start->flag & SEQ_KEY_SELECTED) {
        /* Ensure start transition key cannot pass the midpoint. */
        ts->offset_clamp.xmax = min_ii(midpoint - key_start->strip_frame_index,
                                       ts->offset_clamp.xmax);
        ts->offset_clamp.xmin = max_ii(-max_offset, ts->offset_clamp.xmin);
      }
      else {
        /* Ensure end transition key cannot pass the midpoint. */
        ts->offset_clamp.xmin = max_ii(-(key_end->strip_frame_index - midpoint - 1),
                                       ts->offset_clamp.xmin);
        ts->offset_clamp.xmax = min_ii(max_offset, ts->offset_clamp.xmax);
      }
    }
    /* Non-transition retiming key. */
    else {
      Strip *strip = item.value;
      SeqRetimingKey *key_prev = key - 1, *key_next = key + 1;
      if (!seq::retiming_is_last_key(strip, key)) {
        /* Ensure that this key cannot pass the next key. */
        ts->offset_clamp.xmax = min_ii(key_next->strip_frame_index - key->strip_frame_index - 1,
                                       ts->offset_clamp.xmax);
        /* XXX: There is an off-by-one error for the last "fake" key's `strip_frame_index`, which
         * is 1 less than it should be. This is not an immediate issue but should be fixed. */
      }
      if (key->strip_frame_index != 0) {
        /* Ensure that this key cannot pass the previous key. */
        ts->offset_clamp.xmin = max_ii(-(key->strip_frame_index - key_prev->strip_frame_index - 1),
                                       ts->offset_clamp.xmin);
      }
    }
  }
}

static void createTransSeqRetimingData(bContext * /*C*/, TransInfo *t)
{
  const Editing *ed = seq::editing_get(t->scene);
  if (ed == nullptr) {
    return;
  }

  const Map selection = seq::retiming_selection_get(seq::editing_get(t->scene));

  if (selection.is_empty()) {
    return;
  }

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);
  tc->custom.type.free_cb = freeSeqData;
  tc->data_len = selection.size();

  TransSeq *ts = MEM_new<TransSeq>(__func__);
  tc->custom.type.data = ts;
  tc->custom.type.use_free = true;

  TransData *td = MEM_calloc_arrayN<TransData>(tc->data_len, "TransSeq TransData");
  TransData2D *td2d = MEM_calloc_arrayN<TransData2D>(tc->data_len, "TransSeq TransData2D");
  TransDataSeq *tdseq = MEM_calloc_arrayN<TransDataSeq>(tc->data_len, "TransSeq TransDataSeq");
  tc->data = td;
  tc->data_2d = td2d;
  ts->tdseq = tdseq;

  for (auto item : selection.items()) {
    SeqToTransData(t->scene, item.value, item.key, td++, td2d++, tdseq++);
  }

  create_trans_seq_clamp_data(t, t->scene);
}

static void recalcData_sequencer_retiming(TransInfo *t)
{
  const TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);
  const TransData *td = nullptr;
  const TransData2D *td2d = nullptr;
  int i;

  VectorSet<Strip *> transformed_strips;

  for (i = 0, td = tc->data, td2d = tc->data_2d; i < tc->data_len; i++, td++, td2d++) {
    const TransDataSeq *tdseq = static_cast<TransDataSeq *>(td->extra);
    Strip *strip = tdseq->strip;

    if (!seq::retiming_data_is_editable(strip)) {
      continue;
    }

    float offset[2];
    float offset_clamped[2];
    sub_v2_v2v2(offset, td->loc, td->iloc);
    copy_v2_v2(offset_clamped, offset);

    transform_convert_sequencer_clamp(t, offset_clamped);
    const int new_frame = round_fl_to_int(td->iloc[0] + offset_clamped[0]);

    transformed_strips.add(strip);

    /* Calculate translation. */

    const MutableSpan keys = seq::retiming_keys_get(strip);
    SeqRetimingKey *key = &keys[tdseq->key_index];

    if (seq::retiming_key_is_transition_type(key) &&
        !seq::retiming_selection_has_whole_transition(seq::editing_get(t->scene), key))
    {
      seq::retiming_transition_key_frame_set(t->scene, strip, key, round_fl_to_int(new_frame));
    }
    else {
      seq::retiming_key_timeline_frame_set(t->scene, strip, key, new_frame);
    }

    seq::relations_invalidate_cache(t->scene, strip);
  }

  /* Test overlap, displays red outline. */
  Editing *ed = seq::editing_get(t->scene);
  seq::iterator_set_expand(
      t->scene, seq::active_seqbase_get(ed), transformed_strips, seq::query_strip_effect_chain);
  for (Strip *strip : transformed_strips) {
    strip->runtime.flag &= ~STRIP_OVERLAP;
    if (seq::transform_test_overlap(t->scene, seq::active_seqbase_get(ed), strip)) {
      strip->runtime.flag |= STRIP_OVERLAP;
    }
  }
}

TransConvertTypeInfo TransConvertType_SequencerRetiming = {
    /*flags*/ (T_POINTS | T_2D_EDIT),
    /*create_trans_data*/ createTransSeqRetimingData,
    /*recalc_data*/ recalcData_sequencer_retiming,
};

}  // namespace blender::ed::transform
