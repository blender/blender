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

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_report.h"
#include "BKE_sequencer.h"

#include "UI_view2d.h"

#include "transform.h"
#include "transform_convert.h"

/**
 * Sequencer transform customdata (stored in #TransCustomDataContainer).
 */
typedef struct TransSeq {
  TransDataSeq *tdseq;
  int min;
  int max;
  bool snap_left;
} TransSeq;

/* -------------------------------------------------------------------- */
/** \name Sequencer Transform Creation
 *
 * \{ */

/* This function applies the rules for transforming a strip so duplicate
 * checks don't need to be added in multiple places.
 *
 * recursive, count and flag MUST be set.
 *
 * seq->depth must be set before running this function so we know if the strips
 * are root level or not
 */
static void SeqTransInfo(TransInfo *t, Sequence *seq, int *r_recursive, int *r_count, int *r_flag)
{
  /* for extend we need to do some tricks */
  if (t->mode == TFM_TIME_EXTEND) {

    /* *** Extend Transform *** */

    Scene *scene = t->scene;
    int cfra = CFRA;
    int left = BKE_sequence_tx_get_final_left(seq, true);
    int right = BKE_sequence_tx_get_final_right(seq, true);

    if (seq->depth == 0 && ((seq->flag & SELECT) == 0 || (seq->flag & SEQ_LOCK))) {
      *r_recursive = false;
      *r_count = 0;
      *r_flag = 0;
    }
    else if (seq->type == SEQ_TYPE_META) {

      /* for meta's we only ever need to extend their children, no matter what depth
       * just check the meta's are in the bounds */
      if (t->frame_side == 'R' && right <= cfra) {
        *r_recursive = false;
      }
      else if (t->frame_side == 'L' && left >= cfra) {
        *r_recursive = false;
      }
      else {
        *r_recursive = true;
      }

      *r_count = 1;
      *r_flag = (seq->flag | SELECT) & ~(SEQ_LEFTSEL | SEQ_RIGHTSEL);
    }
    else {

      *r_recursive = false; /* not a meta, so no thinking here */
      *r_count = 1;         /* unless its set to 0, extend will never set 2 handles at once */
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

    if (seq->depth == 0) {

      /* Count */

      /* Non nested strips (resect selection and handles) */
      if ((seq->flag & SELECT) == 0 || (seq->flag & SEQ_LOCK)) {
        *r_recursive = false;
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

        /* Recursive */

        if ((seq->type == SEQ_TYPE_META) && ((seq->flag & (SEQ_LEFTSEL | SEQ_RIGHTSEL)) == 0)) {
          /* if any handles are selected, don't recurse */
          *r_recursive = true;
        }
        else {
          *r_recursive = false;
        }
      }
    }
    else {
      /* Nested, different rules apply */

#ifdef SEQ_TX_NESTED_METAS
      *r_flag = (seq->flag | SELECT) & ~(SEQ_LEFTSEL | SEQ_RIGHTSEL);
      *r_count = 1; /* ignore the selection for nested */
      *r_recursive = (seq->type == SEQ_TYPE_META);
#else
      if (seq->type == SEQ_TYPE_META) {
        /* Meta's can only directly be moved between channels since they
         * don't have their start and length set directly (children affect that)
         * since this Meta is nested we don't need any of its data in fact.
         * BKE_sequence_calc() will update its settings when run on the top-level meta. */
        *r_flag = 0;
        *r_count = 0;
        *r_recursive = true;
      }
      else {
        *r_flag = (seq->flag | SELECT) & ~(SEQ_LEFTSEL | SEQ_RIGHTSEL);
        *r_count = 1; /* ignore the selection for nested */
        *r_recursive = false;
      }
#endif
    }
  }
}

static int SeqTransCount(TransInfo *t, Sequence *parent, ListBase *seqbase, int depth)
{
  Sequence *seq;
  int tot = 0, recursive, count, flag;

  for (seq = seqbase->first; seq; seq = seq->next) {
    seq->depth = depth;

    /* 'seq->tmp' is used by seq_tx_get_final_{left, right}
     * to check sequence's range and clamp to it if needed.
     * It's first place where digging into sequences tree, so store link to parent here. */
    seq->tmp = parent;

    SeqTransInfo(t, seq, &recursive, &count, &flag); /* ignore the flag */
    tot += count;

    if (recursive) {
      tot += SeqTransCount(t, seq, &seq->seqbase, depth + 1);
    }
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
      start_left = BKE_sequence_tx_get_final_left(seq, false);
      td2d->loc[0] = start_left;
      tdsq->start_offset = start_left - seq->start; /* use to apply the original location */
      break;
    case SEQ_LEFTSEL:
      start_left = BKE_sequence_tx_get_final_left(seq, false);
      td2d->loc[0] = start_left;
      break;
    case SEQ_RIGHTSEL:
      td2d->loc[0] = BKE_sequence_tx_get_final_right(seq, false);
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

static int SeqToTransData_Recursive(
    TransInfo *t, ListBase *seqbase, TransData *td, TransData2D *td2d, TransDataSeq *tdsq)
{
  Sequence *seq;
  int recursive, count, flag;
  int tot = 0;

  for (seq = seqbase->first; seq; seq = seq->next) {

    SeqTransInfo(t, seq, &recursive, &count, &flag);

    /* add children first so recalculating metastrips does nested strips first */
    if (recursive) {
      int tot_children = SeqToTransData_Recursive(t, &seq->seqbase, td, td2d, tdsq);

      td = td + tot_children;
      td2d = td2d + tot_children;
      tdsq = tdsq + tot_children;

      tot += tot_children;
    }

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

static void SeqTransDataBounds(TransInfo *t, ListBase *seqbase, TransSeq *ts)
{
  Sequence *seq;
  int recursive, count, flag;
  int max = INT32_MIN, min = INT32_MAX;

  for (seq = seqbase->first; seq; seq = seq->next) {

    /* just to get the flag since there are corner cases where this isn't totally obvious */
    SeqTransInfo(t, seq, &recursive, &count, &flag);

    /* use 'flag' which is derived from seq->flag but modified for special cases */
    if (flag & SELECT) {
      if (flag & (SEQ_LEFTSEL | SEQ_RIGHTSEL)) {
        if (flag & SEQ_LEFTSEL) {
          min = min_ii(seq->startdisp, min);
          max = max_ii(seq->startdisp, max);
        }
        if (flag & SEQ_RIGHTSEL) {
          min = min_ii(seq->enddisp, min);
          max = max_ii(seq->enddisp, max);
        }
      }
      else {
        min = min_ii(seq->startdisp, min);
        max = max_ii(seq->enddisp, max);
      }
    }
  }

  if (ts) {
    ts->max = max;
    ts->min = min;
  }
}

static void freeSeqData(TransInfo *t, TransDataContainer *tc, TransCustomData *custom_data)
{
  Editing *ed = BKE_sequencer_editing_get(t->scene, false);

  if (ed != NULL) {

    ListBase *seqbasep = ed->seqbasep;
    TransData *td = tc->data;
    int a;

    /* prevent updating the same seq twice
     * if the transdata order is changed this will mess up
     * but so will TransDataSeq */
    Sequence *seq_prev = NULL;
    Sequence *seq;

    if (!(t->state == TRANS_CANCEL)) {

#if 0  // default 2.4 behavior

      /* flush to 2d vector from internally used 3d vector */
      for (a = 0; a < t->total; a++, td++) {
        if ((seq != seq_prev) && (seq->depth == 0) && (seq->flag & SEQ_OVERLAP)) {
          seq = ((TransDataSeq *)td->extra)->seq;
          BKE_sequence_base_shuffle(seqbasep, seq, t->scene);
        }

        seq_prev = seq;
      }

#else  // durian hack
      {
        int overlap = 0;

        for (a = 0, seq_prev = NULL; a < tc->data_len; a++, td++, seq_prev = seq) {
          seq = ((TransDataSeq *)td->extra)->seq;
          if ((seq != seq_prev) && (seq->depth == 0) && (seq->flag & SEQ_OVERLAP)) {
            overlap = 1;
            break;
          }
        }

        if (overlap) {
          const bool use_sync_markers = (((SpaceSeq *)t->area->spacedata.first)->flag &
                                         SEQ_MARKER_TRANS) != 0;
          ListBase *markers = &t->scene->markers;

          bool has_effect_root = false, has_effect_any = false;
          for (seq = seqbasep->first; seq; seq = seq->next) {
            seq->tmp = NULL;
          }

          td = tc->data;
          for (a = 0, seq_prev = NULL; a < tc->data_len; a++, td++, seq_prev = seq) {
            seq = ((TransDataSeq *)td->extra)->seq;
            if ((seq != seq_prev)) {
              /* check effects strips, we cant change their time */
              if ((seq->type & SEQ_TYPE_EFFECT) && seq->seq1) {
                has_effect_any = true;
                if (seq->depth == 0) {
                  has_effect_root = true;
                }
              }
              else {
                /* Tag seq with a non zero value, used by
                 * BKE_sequence_base_shuffle_time to identify the ones to shuffle */
                if (seq->depth == 0) {
                  seq->tmp = (void *)1;
                }
              }
            }
          }

          if (t->flag & T_ALT_TRANSFORM) {
            int minframe = MAXFRAME;
            td = tc->data;
            for (a = 0, seq_prev = NULL; a < tc->data_len; a++, td++, seq_prev = seq) {
              seq = ((TransDataSeq *)td->extra)->seq;
              if ((seq != seq_prev) && (seq->depth == 0)) {
                minframe = min_ii(minframe, seq->startdisp);
              }
            }

            for (seq = seqbasep->first; seq; seq = seq->next) {
              if (!(seq->flag & SELECT)) {
                if (seq->startdisp >= minframe) {
                  seq->machine += MAXSEQ * 2;
                }
              }
            }

            BKE_sequence_base_shuffle_time(seqbasep, t->scene, markers, use_sync_markers);

            for (seq = seqbasep->first; seq; seq = seq->next) {
              if (seq->machine >= MAXSEQ * 2) {
                seq->machine -= MAXSEQ * 2;
                seq->tmp = (void *)1;
              }
              else {
                seq->tmp = NULL;
              }
            }

            BKE_sequence_base_shuffle_time(seqbasep, t->scene, markers, use_sync_markers);
          }
          else {
            BKE_sequence_base_shuffle_time(seqbasep, t->scene, markers, use_sync_markers);
          }

          if (has_effect_any) {
            /* update effects strips based on strips just moved in time */
            td = tc->data;
            for (a = 0, seq_prev = NULL; a < tc->data_len; a++, td++, seq_prev = seq) {
              seq = ((TransDataSeq *)td->extra)->seq;
              if ((seq != seq_prev)) {
                if ((seq->type & SEQ_TYPE_EFFECT) && seq->seq1) {
                  BKE_sequence_calc(t->scene, seq);
                }
              }
            }
          }

          if (has_effect_root) {
            /* now if any effects _still_ overlap, we need to move them up */
            td = tc->data;
            for (a = 0, seq_prev = NULL; a < tc->data_len; a++, td++, seq_prev = seq) {
              seq = ((TransDataSeq *)td->extra)->seq;
              if ((seq != seq_prev) && (seq->depth == 0)) {
                if ((seq->type & SEQ_TYPE_EFFECT) && seq->seq1) {
                  if (BKE_sequence_test_overlap(seqbasep, seq)) {
                    BKE_sequence_base_shuffle(seqbasep, seq, t->scene);
                  }
                }
              }
            }
            /* done with effects */
          }
        }
      }
#endif

      for (seq = seqbasep->first; seq; seq = seq->next) {
        /* We might want to build a list of effects that need to be updated during transform */
        if (seq->type & SEQ_TYPE_EFFECT) {
          if (seq->seq1 && seq->seq1->flag & SELECT) {
            BKE_sequence_calc(t->scene, seq);
          }
          else if (seq->seq2 && seq->seq2->flag & SELECT) {
            BKE_sequence_calc(t->scene, seq);
          }
          else if (seq->seq3 && seq->seq3->flag & SELECT) {
            BKE_sequence_calc(t->scene, seq);
          }
        }
      }

      BKE_sequencer_sort(t->scene);
    }
    else {
      /* Canceled, need to update the strips display */
      for (a = 0; a < tc->data_len; a++, td++) {
        seq = ((TransDataSeq *)td->extra)->seq;
        if ((seq != seq_prev) && (seq->depth == 0)) {
          if (seq->flag & SEQ_OVERLAP) {
            BKE_sequence_base_shuffle(seqbasep, seq, t->scene);
          }

          BKE_sequence_calc_disp(t->scene, seq);
        }
        seq_prev = seq;
      }
    }
  }

  if ((custom_data->data != NULL) && custom_data->use_free) {
    TransSeq *ts = custom_data->data;
    MEM_freeN(ts->tdseq);
    MEM_freeN(custom_data->data);
    custom_data->data = NULL;
  }

  DEG_id_tag_update(&t->scene->id, ID_RECALC_SEQUENCER_STRIPS);
}

void createTransSeqData(TransInfo *t)
{
#define XXX_DURIAN_ANIM_TX_HACK

  Scene *scene = t->scene;
  Editing *ed = BKE_sequencer_editing_get(t->scene, false);
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

  count = SeqTransCount(t, NULL, ed->seqbasep, 0);

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
  SeqToTransData_Recursive(t, ed->seqbasep, td, td2d, tdsq);
  SeqTransDataBounds(t, ed->seqbasep, ts);

  if (t->flag & T_MODAL) {
    /* set the snap mode based on how close the mouse is at the end/start points */
    int xmouse = (int)UI_view2d_region_to_view_x((View2D *)t->view, t->mouse.imval[0]);
    if (abs(xmouse - ts->max) > abs(xmouse - ts->min)) {
      ts->snap_left = true;
    }
  }

#undef XXX_DURIAN_ANIM_TX_HACK
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UVs Transform Flush
 *
 * \{ */

/* commented _only_ because the meta may have animation data which
 * needs moving too [#28158] */

#define SEQ_TX_NESTED_METAS

BLI_INLINE void trans_update_seq(Scene *sce, Sequence *seq, int old_start, int sel_flag)
{
  if (seq->depth == 0) {
    /* Calculate this strip and all nested strips.
     * Children are ALWAYS transformed first so we don't need to do this in another loop.
     */
    BKE_sequence_calc(sce, seq);
  }
  else {
    BKE_sequence_calc_disp(sce, seq);
  }

  if (sel_flag == SELECT) {
    BKE_sequencer_offset_animdata(sce, seq, seq->start - old_start);
  }
}

static void flushTransSeq(TransInfo *t)
{
  /* Editing null check already done */
  ListBase *seqbasep = BKE_sequencer_editing_get(t->scene, false)->seqbasep;

  int a, new_frame;
  TransData *td = NULL;
  TransData2D *td2d = NULL;
  TransDataSeq *tdsq = NULL;
  Sequence *seq;

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  /* prevent updating the same seq twice
   * if the transdata order is changed this will mess up
   * but so will TransDataSeq */
  Sequence *seq_prev = NULL;
  int old_start_prev = 0, sel_flag_prev = 0;

  /* flush to 2d vector from internally used 3d vector */
  for (a = 0, td = tc->data, td2d = tc->data_2d; a < tc->data_len; a++, td++, td2d++) {
    int old_start;
    tdsq = (TransDataSeq *)td->extra;
    seq = tdsq->seq;
    old_start = seq->start;
    new_frame = round_fl_to_int(td2d->loc[0]);

    switch (tdsq->sel_flag) {
      case SELECT:
#ifdef SEQ_TX_NESTED_METAS
        if ((seq->depth != 0 || BKE_sequence_tx_test(seq))) {
          /* for meta's, their children move */
          seq->start = new_frame - tdsq->start_offset;
        }
#else
        if (seq->type != SEQ_TYPE_META && (seq->depth != 0 || seq_tx_test(seq))) {
          /* for meta's, their children move */
          seq->start = new_frame - tdsq->start_offset;
        }
#endif
        if (seq->depth == 0) {
          seq->machine = round_fl_to_int(td2d->loc[1]);
          CLAMP(seq->machine, 1, MAXSEQ);
        }
        break;
      case SEQ_LEFTSEL: /* no vertical transform  */
        BKE_sequence_tx_set_final_left(seq, new_frame);
        BKE_sequence_tx_handle_xlimits(seq, tdsq->flag & SEQ_LEFTSEL, tdsq->flag & SEQ_RIGHTSEL);

        /* todo - move this into aftertrans update? - old seq tx needed it anyway */
        BKE_sequence_single_fix(seq);
        break;
      case SEQ_RIGHTSEL: /* no vertical transform  */
        BKE_sequence_tx_set_final_right(seq, new_frame);
        BKE_sequence_tx_handle_xlimits(seq, tdsq->flag & SEQ_LEFTSEL, tdsq->flag & SEQ_RIGHTSEL);

        /* todo - move this into aftertrans update? - old seq tx needed it anyway */
        BKE_sequence_single_fix(seq);
        break;
    }

    /* Update *previous* seq! Else, we would update a seq after its first transform,
     * and if it has more than one (like e.g. SEQ_LEFTSEL and SEQ_RIGHTSEL),
     * the others are not updated! See T38469.
     */
    if (seq != seq_prev) {
      if (seq_prev) {
        trans_update_seq(t->scene, seq_prev, old_start_prev, sel_flag_prev);
      }

      seq_prev = seq;
      old_start_prev = old_start;
      sel_flag_prev = tdsq->sel_flag;
    }
    else {
      /* We want to accumulate *all* sel_flags for this seq! */
      sel_flag_prev |= tdsq->sel_flag;
    }
  }

  /* Don't forget to update the last seq! */
  if (seq_prev) {
    trans_update_seq(t->scene, seq_prev, old_start_prev, sel_flag_prev);
  }

  /* originally TFM_TIME_EXTEND, transform changes */
  if (ELEM(t->mode, TFM_SEQ_SLIDE, TFM_TIME_TRANSLATE)) {
    /* Special annoying case here, need to calc metas with TFM_TIME_EXTEND only */

    /* calc all meta's then effects [#27953] */
    for (seq = seqbasep->first; seq; seq = seq->next) {
      if (seq->type == SEQ_TYPE_META && seq->flag & SELECT) {
        BKE_sequence_calc(t->scene, seq);
      }
    }
    for (seq = seqbasep->first; seq; seq = seq->next) {
      if (seq->seq1 || seq->seq2 || seq->seq3) {
        BKE_sequence_calc(t->scene, seq);
      }
    }

    /* update effects inside meta's */
    for (a = 0, seq_prev = NULL, td = tc->data, td2d = tc->data_2d; a < tc->data_len;
         a++, td++, td2d++, seq_prev = seq) {
      tdsq = (TransDataSeq *)td->extra;
      seq = tdsq->seq;
      if ((seq != seq_prev) && (seq->depth != 0)) {
        if (seq->seq1 || seq->seq2 || seq->seq3) {
          BKE_sequence_calc(t->scene, seq);
        }
      }
    }
  }

  /* need to do the overlap check in a new loop otherwise adjacent strips
   * will not be updated and we'll get false positives */
  seq_prev = NULL;
  for (a = 0, td = tc->data, td2d = tc->data_2d; a < tc->data_len; a++, td++, td2d++) {

    tdsq = (TransDataSeq *)td->extra;
    seq = tdsq->seq;

    if (seq != seq_prev) {
      if (seq->depth == 0) {
        /* test overlap, displays red outline */
        seq->flag &= ~SEQ_OVERLAP;
        if (BKE_sequence_test_overlap(seqbasep, seq)) {
          seq->flag |= SEQ_OVERLAP;
        }
      }
    }
    seq_prev = seq;
  }
}

/* helper for recalcData() - for sequencer transforms */
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
      BKE_sequence_invalidate_cache_composite(t->scene, seq);
    }

    seq_prev = seq;
  }

  DEG_id_tag_update(&t->scene->id, ID_RECALC_SEQUENCER_STRIPS);

  flushTransSeq(t);
}

int transform_convert_sequencer_get_snap_bound(TransInfo *t)
{
  TransSeq *ts = TRANS_DATA_CONTAINER_FIRST_SINGLE(t)->custom.type.data;
  return ts->snap_left ? ts->min : ts->max;
}

/** \} */
