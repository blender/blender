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
  td->ext = nullptr;
  td->flag |= TD_SELECTED;
  td->dist = 0.0;

  return td;
}

static void freeSeqData(TransInfo *t, TransDataContainer *tc, TransCustomData * /*custom_data*/)
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
  dependant.remove_if(
      [&](Strip *strip) { return seq::transform_sequence_can_be_translated(strip); });

  if (seq_transform_check_overlap(transformed_strips)) {
    const bool use_sync_markers = (((SpaceSeq *)t->area->spacedata.first)->flag &
                                   SEQ_MARKER_TRANS) != 0;
    seq::transform_handle_overlap(
        scene, seqbasep, transformed_strips, dependant, use_sync_markers);
  }

  MEM_freeN(td->extra);
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
  tc->data = MEM_calloc_arrayN<TransData>(tc->data_len, "TransSeq TransData");
  tc->data_2d = MEM_calloc_arrayN<TransData2D>(tc->data_len, "TransSeq TransData2D");
  TransDataSeq *tdseq = MEM_calloc_arrayN<TransDataSeq>(tc->data_len, "TransSeq TransDataSeq");
  TransData *td = tc->data;
  TransData2D *td2d = tc->data_2d;

  for (auto item : selection.items()) {
    SeqToTransData(t->scene, item.value, item.key, td++, td2d++, tdseq++);
  }
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

    transformed_strips.add(strip);

    /* Calculate translation. */

    const MutableSpan keys = seq::retiming_keys_get(strip);
    SeqRetimingKey *key = &keys[tdseq->key_index];

    if (seq::retiming_key_is_transition_type(key) &&
        !seq::retiming_selection_has_whole_transition(seq::editing_get(t->scene), key))
    {
      seq::retiming_transition_key_frame_set(t->scene, strip, key, round_fl_to_int(td2d->loc[0]));
    }
    else {
      seq::retiming_key_timeline_frame_set(t->scene, strip, key, td2d->loc[0]);
    }

    seq::relations_invalidate_cache_preprocessed(t->scene, strip);
  }

  /* Test overlap, displays red outline. */
  Editing *ed = seq::editing_get(t->scene);
  seq::iterator_set_expand(
      t->scene, seq::active_seqbase_get(ed), transformed_strips, seq::query_strip_effect_chain);
  for (Strip *strip : transformed_strips) {
    strip->flag &= ~SEQ_OVERLAP;
    if (seq::transform_test_overlap(t->scene, seq::active_seqbase_get(ed), strip)) {
      strip->flag |= SEQ_OVERLAP;
    }
  }
}

TransConvertTypeInfo TransConvertType_SequencerRetiming = {
    /*flags*/ (T_POINTS | T_2D_EDIT),
    /*create_trans_data*/ createTransSeqRetimingData,
    /*recalc_data*/ recalcData_sequencer_retiming,
};

}  // namespace blender::ed::transform
