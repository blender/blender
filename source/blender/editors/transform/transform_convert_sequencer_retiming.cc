/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "MEM_guardedalloc.h"

#include "DNA_sequence_types.h"
#include "DNA_space_types.h"

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"

#include "BKE_context.hh"
#include "BKE_report.h"

#include "SEQ_relations.hh"
#include "SEQ_retiming.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_time.hh"

#include "ED_keyframing.hh"

#include "UI_view2d.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "transform.hh"
#include "transform_convert.hh"

/** Used for sequencer transform. */
typedef struct TransDataSeq {
  Sequence *seq;
  int orig_timeline_frame;
  int key_index; /* Some actions may need to destroy original data, use index to access it. */
} TransDataSeq;

static TransData *SeqToTransData(const Scene *scene,
                                 Sequence *seq,
                                 const SeqRetimingKey *key,
                                 TransData *td,
                                 TransData2D *td2d,
                                 TransDataSeq *tdseq)
{

  td2d->loc[0] = SEQ_retiming_key_timeline_frame_get(scene, seq, key);
  td2d->loc[1] = key->retiming_factor;
  td2d->loc2d = nullptr;
  td->loc = td2d->loc;
  copy_v3_v3(td->iloc, td->loc);
  copy_v3_v3(td->center, td->loc);
  memset(td->axismtx, 0, sizeof(td->axismtx));
  td->axismtx[2][2] = 1.0f;
  unit_m3(td->mtx);
  unit_m3(td->smtx);

  tdseq->seq = seq;
  tdseq->orig_timeline_frame = SEQ_retiming_key_timeline_frame_get(scene, seq, key);
  tdseq->key_index = SEQ_retiming_key_index_get(seq, key);

  td->extra = static_cast<void *>(tdseq);
  td->ext = nullptr;
  td->flag |= TD_SELECTED;
  td->dist = 0.0;

  return td;
}

static void freeSeqData(TransInfo * /*t*/,
                        TransDataContainer *tc,
                        TransCustomData * /*custom_data*/)
{
  TransData *td = (TransData *)tc->data;
  MEM_freeN(td->extra);
}

static void createTransSeqRetimingData(bContext * /*C*/, TransInfo *t)
{
  const Editing *ed = SEQ_editing_get(t->scene);
  if (ed == nullptr) {
    return;
  }

  const blender::Map selection = SEQ_retiming_selection_get(SEQ_editing_get(t->scene));

  if (selection.is_empty()) {
    return;
  }

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);
  tc->custom.type.free_cb = freeSeqData;

  tc->data_len = selection.size();
  tc->data = MEM_cnew_array<TransData>(tc->data_len, "TransSeq TransData");
  tc->data_2d = MEM_cnew_array<TransData2D>(tc->data_len, "TransSeq TransData2D");
  TransDataSeq *tdseq = MEM_cnew_array<TransDataSeq>(tc->data_len, "TransSeq TransDataSeq");
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

  for (i = 0, td = tc->data, td2d = tc->data_2d; i < tc->data_len; i++, td++, td2d++) {
    const TransDataSeq *tdseq = static_cast<TransDataSeq *>(td->extra);
    Sequence *seq = tdseq->seq;

    /* Calculate translation. */

    const blender::MutableSpan keys = SEQ_retiming_keys_get(seq);
    SeqRetimingKey *key = &keys[tdseq->key_index];

    if (SEQ_retiming_key_is_transition_type(key) &&
        !SEQ_retiming_selection_has_whole_transition(SEQ_editing_get(t->scene), key))
    {
      SEQ_retiming_transition_key_frame_set(t->scene, seq, key, round_fl_to_int(td2d->loc[0]));
    }
    else {
      SEQ_retiming_key_timeline_frame_set(t->scene, seq, key, td2d->loc[0]);
    }

    SEQ_relations_invalidate_cache_preprocessed(t->scene, seq);
  }
}

TransConvertTypeInfo TransConvertType_SequencerRetiming = {
    /*flags*/ (T_POINTS | T_2D_EDIT),
    /*create_trans_data*/ createTransSeqRetimingData,
    /*recalc_data*/ recalcData_sequencer_retiming,
};
