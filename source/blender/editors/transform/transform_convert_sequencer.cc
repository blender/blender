/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"

#include "BKE_context.hh"

#include "ED_markers.hh"

#include "SEQ_animation.hh"
#include "SEQ_channels.hh"
#include "SEQ_edit.hh"
#include "SEQ_effects.hh"
#include "SEQ_iterator.hh"
#include "SEQ_relations.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_time.hh"
#include "SEQ_transform.hh"

#include "UI_view2d.hh"

#include "transform.hh"
#include "transform_convert.hh"
#include "transform_mode.hh"

namespace blender::ed::transform {

#define STRIP_EDGE_PAN_INSIDE_PAD 3.5
#define STRIP_EDGE_PAN_OUTSIDE_PAD 0 /* Disable clamping for panning, use whole screen. */
#define STRIP_EDGE_PAN_SPEED_RAMP 1
#define STRIP_EDGE_PAN_MAX_SPEED 4 /* In UI units per second, slower than default. */
#define STRIP_EDGE_PAN_DELAY 1.0f
#define STRIP_EDGE_PAN_ZOOM_INFLUENCE 0.5f

namespace {

/** Used for sequencer transform. */
struct TransDataSeq {
  Strip *strip;
  /** A copy of #Strip.flag that may be modified for nested strips. */
  int flag;
  /** Use this so we can have transform data at the strips start,
   * but apply correctly to the start frame. */
  int start_offset;
  /** One of #SELECT, #SEQ_LEFTSEL and #SEQ_RIGHTSEL. */
  short sel_flag;
};

/**
 * Sequencer transform customdata (stored in #TransCustomDataContainer).
 */
struct TransSeq {
  TransDataSeq *tdseq;
  /* Maximum delta allowed along x and y before clamping selected strips/handles. Always active. */
  rcti offset_clamp;
  /* Maximum delta before clamping handles to the bounds of underlying content. May be disabled. */
  int hold_clamp_min, hold_clamp_max;

  /* Initial rect of the view2d, used for computing offset during edge panning. */
  rctf initial_v2d_cur;
  View2DEdgePanData edge_pan;

  /* Strips that aren't selected, but their position entirely depends on transformed strips. */
  VectorSet<Strip *> time_dependent_strips;
};

}  // namespace

/* -------------------------------------------------------------------- */
/** \name Sequencer Transform Creation
 * \{ */

/* This function applies the rules for transforming a strip so duplicate
 * checks don't need to be added in multiple places.
 *
 * count and flag MUST be set.
 */
static void SeqTransInfo(TransInfo *t, Strip *strip, int *r_count, int *r_flag)
{
  Scene *scene = CTX_data_sequencer_scene(t->context);
  Editing *ed = seq::editing_get(scene);
  ListBase *channels = seq::channels_displayed_get(ed);

  /* For extend we need to do some tricks. */
  if (t->mode == TFM_TIME_EXTEND) {

    /* *** Extend Transform *** */
    int cfra = scene->r.cfra;
    int left = seq::time_left_handle_frame_get(scene, strip);
    int right = seq::time_right_handle_frame_get(scene, strip);

    if ((strip->flag & SELECT) == 0 || seq::transform_is_locked(channels, strip)) {
      *r_count = 0;
      *r_flag = 0;
    }
    else {
      *r_count = 1; /* Unless its set to 0, extend will never set 2 handles at once. */
      *r_flag = (strip->flag | SELECT) & ~(SEQ_LEFTSEL | SEQ_RIGHTSEL);

      if (t->frame_side == 'R') {
        if (right <= cfra) {
          *r_count = *r_flag = 0;
        } /* Ignore. */
        else if (left > cfra) {
        } /* Keep the selection. */
        else {
          *r_flag |= SEQ_RIGHTSEL;
        }
      }
      else {
        if (left >= cfra) {
          *r_count = *r_flag = 0;
        } /* Ignore. */
        else if (right < cfra) {
        } /* Keep the selection. */
        else {
          *r_flag |= SEQ_LEFTSEL;
        }
      }
    }
  }
  else {

    t->frame_side = 'B';

    /* *** Normal Transform *** */

    /* Count. */

    /* Non nested strips (reset selection and handles). */
    if ((strip->flag & SELECT) == 0 || seq::transform_is_locked(channels, strip)) {
      *r_count = 0;
      *r_flag = 0;
    }
    else {
      if ((strip->flag & (SEQ_LEFTSEL | SEQ_RIGHTSEL)) == (SEQ_LEFTSEL | SEQ_RIGHTSEL)) {
        *r_flag = strip->flag;
        *r_count = 2; /* We need 2 transdata's. */
      }
      else {
        *r_flag = strip->flag;
        *r_count = 1; /* Selected or with a handle selected. */
      }
    }
  }
}

static int SeqTransCount(TransInfo *t, ListBase *seqbase)
{
  int tot = 0, count, flag;

  LISTBASE_FOREACH (Strip *, strip, seqbase) {
    SeqTransInfo(t, strip, &count, &flag); /* Ignore the flag. */
    tot += count;
  }

  return tot;
}

static TransData *SeqToTransData(Scene *scene,
                                 TransData *td,
                                 TransData2D *td2d,
                                 TransDataSeq *tdsq,
                                 Strip *strip,
                                 int flag,
                                 int sel_flag)
{
  int start_left;

  switch (sel_flag) {
    case SELECT:
      /* Use seq_tx_get_final_left() and an offset here
       * so transform has the left hand location of the strip.
       * `tdsq->start_offset` is used when flushing the tx data back. */
      start_left = seq::time_left_handle_frame_get(scene, strip);
      td2d->loc[0] = start_left;
      tdsq->start_offset = start_left - strip->start; /* Use to apply the original location. */
      break;
    case SEQ_LEFTSEL:
      start_left = seq::time_left_handle_frame_get(scene, strip);
      td2d->loc[0] = start_left;
      break;
    case SEQ_RIGHTSEL:
      td2d->loc[0] = seq::time_right_handle_frame_get(scene, strip);
      break;
  }

  td2d->loc[1] = strip->channel; /* Channel - Y location. */
  td2d->loc[2] = 0.0f;
  td2d->loc2d = nullptr;

  tdsq->strip = strip;

  /* Use instead of strip->flag for nested strips and other
   * cases where the selection may need to be modified. */
  tdsq->flag = flag;
  tdsq->sel_flag = sel_flag;

  td->extra = (void *)tdsq; /* Allow us to update the strip from here. */

  td->flag = 0;
  td->loc = td2d->loc;
  copy_v3_v3(td->center, td->loc);
  copy_v3_v3(td->iloc, td->loc);

  memset(td->axismtx, 0, sizeof(td->axismtx));
  td->axismtx[2][2] = 1.0f;

  td->val = nullptr;

  td->flag |= TD_SELECTED;
  td->dist = 0.0;

  unit_m3(td->mtx);
  unit_m3(td->smtx);

  /* Time Transform (extend). */
  td->val = td2d->loc;
  td->ival = td2d->loc[0];

  return td;
}

static int SeqToTransData_build(
    TransInfo *t, ListBase *seqbase, TransData *td, TransData2D *td2d, TransDataSeq *tdsq)
{
  Scene *scene = CTX_data_sequencer_scene(t->context);
  int count, flag;
  int tot = 0;

  LISTBASE_FOREACH (Strip *, strip, seqbase) {

    SeqTransInfo(t, strip, &count, &flag);

    /* Use 'flag' which is derived from strip->flag but modified for special cases. */
    if (flag & SELECT) {
      if (flag & (SEQ_LEFTSEL | SEQ_RIGHTSEL)) {
        if (flag & SEQ_LEFTSEL) {
          SeqToTransData(scene, td++, td2d++, tdsq++, strip, flag, SEQ_LEFTSEL);
          tot++;
        }
        if (flag & SEQ_RIGHTSEL) {
          SeqToTransData(scene, td++, td2d++, tdsq++, strip, flag, SEQ_RIGHTSEL);
          tot++;
        }
      }
      else {
        SeqToTransData(scene, td++, td2d++, tdsq++, strip, flag, SELECT);
        tot++;
      }
    }
  }
  return tot;
}

static void free_transform_custom_data(TransCustomData *custom_data)
{
  if ((custom_data->data != nullptr) && custom_data->use_free) {
    TransSeq *ts = static_cast<TransSeq *>(custom_data->data);
    MEM_freeN(ts->tdseq);
    MEM_delete(ts);
    custom_data->data = nullptr;
  }
}

/* Canceled, need to update the strips display. */
static void seq_transform_cancel(TransInfo *t, Span<Strip *> transformed_strips)
{
  Scene *scene = CTX_data_sequencer_scene(t->context);
  ListBase *seqbase = seq::active_seqbase_get(seq::editing_get(scene));

  if (t->remove_on_cancel) {
    for (Strip *strip : transformed_strips) {
      seq::edit_flag_for_removal(scene, seqbase, strip);
    }
    seq::edit_remove_flagged_strips(scene, seqbase);
    return;
  }

  for (Strip *strip : transformed_strips) {
    /* Handle pre-existing overlapping strips even when operator is canceled.
     * This is necessary for #SEQUENCER_OT_duplicate_move macro for example. */
    if (seq::transform_test_overlap(scene, seqbase, strip)) {
      seq::transform_seqbase_shuffle(seqbase, strip, scene);
    }
  }
}

static ListBase *seqbase_active_get(const TransInfo *t)
{
  Scene *scene = CTX_data_sequencer_scene(t->context);
  Editing *ed = seq::editing_get(scene);
  return seq::active_seqbase_get(ed);
}

bool seq_transform_check_overlap(Span<Strip *> transformed_strips)
{
  for (Strip *strip : transformed_strips) {
    if (strip->runtime.flag & STRIP_OVERLAP) {
      return true;
    }
  }
  return false;
}

static VectorSet<Strip *> seq_transform_collection_from_transdata(TransDataContainer *tc)
{
  VectorSet<Strip *> strips;
  TransData *td = tc->data;
  for (int a = 0; a < tc->data_len; a++, td++) {
    Strip *strip = ((TransDataSeq *)td->extra)->strip;
    strips.add(strip);
  }
  return strips;
}

static void freeSeqData(TransInfo *t, TransDataContainer *tc, TransCustomData *custom_data)
{
  Scene *scene = CTX_data_sequencer_scene(t->context);
  Editing *ed = seq::editing_get(scene);
  if (ed == nullptr) {
    free_transform_custom_data(custom_data);
    return;
  }

  VectorSet transformed_strips = seq_transform_collection_from_transdata(tc);
  seq::iterator_set_expand(
      scene, seqbase_active_get(t), transformed_strips, seq::query_strip_effect_chain);

  for (Strip *strip : transformed_strips) {
    strip->runtime.flag &= ~(STRIP_CLAMPED_LH | STRIP_CLAMPED_RH);
    strip->runtime.flag &= ~STRIP_IGNORE_CHANNEL_LOCK;
  }

  if (t->state == TRANS_CANCEL) {
    seq_transform_cancel(t, transformed_strips);
    free_transform_custom_data(custom_data);
    return;
  }

  TransSeq *ts = static_cast<TransSeq *>(tc->custom.type.data);
  ListBase *seqbasep = seqbase_active_get(t);
  const bool use_sync_markers = (((SpaceSeq *)t->area->spacedata.first)->flag &
                                 SEQ_MARKER_TRANS) != 0;
  if (seq_transform_check_overlap(transformed_strips)) {
    seq::transform_handle_overlap(
        scene, seqbasep, transformed_strips, ts->time_dependent_strips, use_sync_markers);
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  free_transform_custom_data(custom_data);
}

static VectorSet<Strip *> query_selected_strips_no_handles(ListBase *seqbase)
{
  VectorSet<Strip *> strips;
  LISTBASE_FOREACH (Strip *, strip, seqbase) {
    if ((strip->flag & SELECT) != 0 && ((strip->flag & (SEQ_LEFTSEL | SEQ_RIGHTSEL)) == 0)) {
      strips.add(strip);
    }
  }
  return strips;
}

enum SeqInputSide {
  SEQ_INPUT_LEFT = -1,
  SEQ_INPUT_RIGHT = 1,
};

static Strip *effect_input_get(const Scene *scene, Strip *effect, SeqInputSide side)
{
  Strip *input = effect->input1;
  if (effect->input2 && (seq::time_left_handle_frame_get(scene, effect->input2) -
                         seq::time_left_handle_frame_get(scene, effect->input1)) *
                                side >
                            0)
  {
    input = effect->input2;
  }
  return input;
}

static Strip *effect_base_input_get(const Scene *scene, Strip *effect, SeqInputSide side)
{
  Strip *input = effect, *strip_iter = effect;
  while (strip_iter != nullptr) {
    input = strip_iter;
    strip_iter = effect_input_get(scene, strip_iter, side);
  }
  return input;
}

/**
 * Strips that aren't selected, but their position entirely depends on
 * transformed strips. This collection is used to offset animation.
 */
static void query_time_dependent_strips_strips(TransInfo *t,
                                               VectorSet<Strip *> &time_dependent_strips)
{
  Scene *scene = CTX_data_sequencer_scene(t->context);
  ListBase *seqbase = seqbase_active_get(t);

  /* Query dependent strips where used strips do not have handles selected.
   * If all inputs of any effect even indirectly(through another effect) points to selected strip,
   * its position will change. */

  VectorSet<Strip *> strips_no_handles = query_selected_strips_no_handles(seqbase);
  time_dependent_strips.add_multiple(strips_no_handles);

  seq::iterator_set_expand(scene, seqbase, strips_no_handles, seq::query_strip_effect_chain);
  bool strip_added = true;

  while (strip_added) {
    strip_added = false;

    for (Strip *strip : strips_no_handles) {
      if (time_dependent_strips.contains(strip)) {
        continue; /* Strip is already in collection, skip it. */
      }

      /* If both input1 and input2 exist, both must be selected. */
      if (strip->input1 && time_dependent_strips.contains(strip->input1)) {
        if (strip->input2 && !time_dependent_strips.contains(strip->input2)) {
          continue;
        }
        strip_added = true;
        time_dependent_strips.add(strip);
      }
    }
  }

  /* Query dependent strips where used strips do have handles selected.
   * If any 2-input effect changes position because handles were moved, animation should be offset.
   * With single input effect, it is less likely desirable to move animation. */

  VectorSet selected_strips = seq::query_selected_strips(seqbase);
  seq::iterator_set_expand(scene, seqbase, selected_strips, seq::query_strip_effect_chain);
  for (Strip *strip : selected_strips) {
    /* Check only 2 input effects. */
    if (strip->input1 == nullptr || strip->input2 == nullptr) {
      continue;
    }

    /* Find immediate base inputs(left and right side). */
    Strip *input_left = effect_base_input_get(scene, strip, SEQ_INPUT_LEFT);
    Strip *input_right = effect_base_input_get(scene, strip, SEQ_INPUT_RIGHT);

    if ((input_left->flag & SEQ_RIGHTSEL) != 0 && (input_right->flag & SEQ_LEFTSEL) != 0) {
      time_dependent_strips.add(strip);
    }
  }

  /* Remove all non-effects. */
  time_dependent_strips.remove_if(
      [&](Strip *strip) { return seq::transform_strip_can_be_translated(strip); });
}

static void create_trans_seq_clamp_data(TransInfo *t, const Scene *scene)
{
  TransSeq *ts = (TransSeq *)TRANS_DATA_CONTAINER_FIRST_SINGLE(t)->custom.type.data;
  const Editing *ed = seq::editing_get(scene);

  bool only_handles_selected = true;

  /* Prevent snaps and change in `values` past `offset_clamp` for all selected strips. */
  BLI_rcti_init(&ts->offset_clamp, -INT_MAX, INT_MAX, -seq::MAX_CHANNELS, seq::MAX_CHANNELS);

  VectorSet<Strip *> strips = seq::query_selected_strips(seq::active_seqbase_get(ed));
  for (Strip *strip : strips) {
    if (!strip->is_effect() || seq::effect_get_num_inputs(strip->type) == 0) {
      continue;
    }
    /* If there is an effect strip with no inputs selected, prevent any x-direction movement,
     * since these strips are tied to their inputs and can only move up and down. */
    if (!(strip->input1->flag & SELECT) && (!strip->input2 || !(strip->input2->flag & SELECT))) {
      ts->offset_clamp.xmin = 0;
      ts->offset_clamp.xmax = 0;
    }
  }

  /* Try to clamp handles by default. */
  t->modifiers |= MOD_STRIP_CLAMP_HOLDS;
  ts->hold_clamp_min = -INT_MAX;
  ts->hold_clamp_max = INT_MAX;
  for (Strip *strip : strips) {
    if (seq::transform_is_locked(seq::channels_displayed_get(ed), strip)) {
      continue;
    }

    bool left_sel = (strip->flag & SEQ_LEFTSEL);
    bool right_sel = (strip->flag & SEQ_RIGHTSEL);

    /* If any strips start out with hold offsets visible, disable handle clamping on init. */
    if ((strip->startofs < 0 || strip->endofs < 0) && !seq::transform_single_image_check(strip)) {
      t->modifiers &= ~MOD_STRIP_CLAMP_HOLDS;
    }

    /* If both handles are selected, there must be enough underlying content to clamp holds. */
    bool can_clamp_holds = !(left_sel && right_sel) ||
                           (strip->len >= seq::time_right_handle_frame_get(scene, strip) -
                                              seq::time_left_handle_frame_get(scene, strip));
    can_clamp_holds &= !seq::transform_single_image_check(strip);

    /* A handle is selected. Update x-axis clamping data. */
    if (left_sel || right_sel) {
      if (left_sel) {
        /* Ensure that this strip's left handle cannot pass its right handle. */
        if (!(left_sel && right_sel)) {
          int offset = (seq::time_right_handle_frame_get(scene, strip) - 1) -
                       seq::time_left_handle_frame_get(scene, strip);
          ts->offset_clamp.xmax = min_ii(ts->offset_clamp.xmax, offset);
        }

        if (can_clamp_holds) {
          /* Ensure that the left handle's frame is greater than or equal to the content start. */
          ts->hold_clamp_min = max_ii(ts->hold_clamp_min, -strip->startofs);
        }
      }
      if (right_sel) {
        if (!(left_sel && right_sel)) {
          /* Ensure that this strip's right handle cannot pass its left handle. */
          int offset = (seq::time_left_handle_frame_get(scene, strip) + 1) -
                       seq::time_right_handle_frame_get(scene, strip);
          ts->offset_clamp.xmin = max_ii(ts->offset_clamp.xmin, offset);
        }

        if (can_clamp_holds) {
          /* Ensure that the right handle's frame is less than or equal to the content end. */
          ts->hold_clamp_max = min_ii(ts->hold_clamp_max, strip->endofs);
        }
      }
    }
    /* No handles are selected. Update y-axis channel clamping data. */
    else {
      ts->offset_clamp.ymin = max_ii(ts->offset_clamp.ymin, 1 - strip->channel);
      ts->offset_clamp.ymax = min_ii(ts->offset_clamp.ymax, seq::MAX_CHANNELS - strip->channel);
      only_handles_selected = false;
    }
  }

  /* TODO(john): This ensures that y-axis movement is restricted only if all of the selected items
   * are handles, since currently it is possible to select whole strips and handles at the same
   * time. This should be removed for 5.0 when we make this behavior impossible. */
  if (only_handles_selected) {
    ts->offset_clamp.ymin = 0;
    ts->offset_clamp.ymax = 0;
  }
}

static void createTransSeqData(bContext *C, TransInfo *t)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  if (!scene) {
    return;
  }
  Editing *ed = seq::editing_get(scene);
  TransData *td = nullptr;
  TransData2D *td2d = nullptr;
  TransDataSeq *tdsq = nullptr;
  TransSeq *ts = nullptr;

  int count = 0;

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  if (ed == nullptr) {
    tc->data_len = 0;
    return;
  }

  /* Disable cursor wrapping for edge pan. */
  if (t->mode == TFM_TRANSLATION) {
    t->flag |= T_NO_CURSOR_WRAP;
  }

  tc->custom.type.free_cb = freeSeqData;
  t->frame_side = transform_convert_frame_side_dir_get(t, float(scene->r.cfra));

  count = SeqTransCount(t, ed->current_strips());

  /* Allocate memory for data. */
  tc->data_len = count;

  /* Stop if trying to build list if nothing selected. */
  if (count == 0) {
    return;
  }

  tc->custom.type.data = ts = MEM_new<TransSeq>(__func__);
  tc->custom.type.use_free = true;
  td = tc->data = MEM_calloc_arrayN<TransData>(tc->data_len, "TransSeq TransData");
  td2d = tc->data_2d = MEM_calloc_arrayN<TransData2D>(tc->data_len, "TransSeq TransData2D");
  ts->tdseq = tdsq = MEM_calloc_arrayN<TransDataSeq>(tc->data_len, "TransSeq TransDataSeq");

  /* Custom data to enable edge panning during transformation. */
  UI_view2d_edge_pan_init(t->context,
                          &ts->edge_pan,
                          STRIP_EDGE_PAN_INSIDE_PAD,
                          STRIP_EDGE_PAN_OUTSIDE_PAD,
                          STRIP_EDGE_PAN_SPEED_RAMP,
                          STRIP_EDGE_PAN_MAX_SPEED,
                          STRIP_EDGE_PAN_DELAY,
                          STRIP_EDGE_PAN_ZOOM_INFLUENCE);
  UI_view2d_edge_pan_set_limits(&ts->edge_pan, -FLT_MAX, FLT_MAX, 1, seq::MAX_CHANNELS + 1);
  ts->initial_v2d_cur = t->region->v2d.cur;

  /* Loop 2: build transdata array. */
  SeqToTransData_build(t, ed->current_strips(), td, td2d, tdsq);

  create_trans_seq_clamp_data(t, scene);

  query_time_dependent_strips_strips(t, ts->time_dependent_strips);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UVs Transform Flush
 * \{ */

static void view2d_edge_pan_loc_compensate(TransInfo *t, float r_offset[2])
{
  TransSeq *ts = (TransSeq *)TRANS_DATA_CONTAINER_FIRST_SINGLE(t)->custom.type.data;

  const rctf rect_prev = t->region->v2d.cur;

  if (t->options & CTX_VIEW2D_EDGE_PAN) {
    if (t->state == TRANS_CANCEL) {
      UI_view2d_edge_pan_cancel(t->context, &ts->edge_pan);
    }
    else {
      /* Edge panning functions expect window coordinates, mval is relative to region. */
      const int xy[2] = {
          t->region->winrct.xmin + int(t->mval[0]),
          t->region->winrct.ymin + int(t->mval[1]),
      };
      UI_view2d_edge_pan_apply(t->context, &ts->edge_pan, xy);
    }
  }

  if (t->state != TRANS_CANCEL) {
    if (!BLI_rctf_compare(&rect_prev, &t->region->v2d.cur, FLT_EPSILON)) {
      /* Additional offset due to change in view2D rect. */
      BLI_rctf_transform_pt_v(&t->region->v2d.cur, &rect_prev, r_offset, r_offset);
      transformViewUpdate(t);
    }
  }
}

static void flushTransSeq(TransInfo *t)
{
  /* Editing null check already done. */
  ListBase *seqbasep = seqbase_active_get(t);
  Scene *scene = CTX_data_sequencer_scene(t->context);

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);
  TransData *td = tc->data;
  TransData2D *td2d = tc->data_2d;

  /* This is calculated for offsetting animation of effects that change position with inputs.
   * Maximum(positive or negative) value is used, because individual strips can be clamped. This
   * works fairly well in most scenarios, but there can be some edge cases.
   *
   * Better solution would be to store effect position and calculate real offset. However with many
   * (>5) effects in chain, there is visible lag in strip position update, because during
   * recalculation, hierarchy is not taken into account. */
  int max_offset = 0;

  float edge_pan_offset[2] = {0.0f, 0.0f};
  view2d_edge_pan_loc_compensate(t, edge_pan_offset);

  /* Flush to 2D vector from internally used 3D vector. */
  for (int a = 0; a < tc->data_len; a++, td++, td2d++) {
    TransDataSeq *tdsq = (TransDataSeq *)td->extra;
    Strip *strip = tdsq->strip;

    /* Apply extra offset caused by edge panning. */
    add_v2_v2(td->loc, edge_pan_offset);

    float offset[2];
    float offset_clamped[2];
    sub_v2_v2v2(offset, td->loc, td->iloc);
    copy_v2_v2(offset_clamped, offset);

    if (t->state != TRANS_CANCEL) {
      transform_convert_sequencer_clamp(t, offset_clamped);
    }
    const int new_frame = round_fl_to_int(td->iloc[0] + offset_clamped[0]);
    const int new_channel = round_fl_to_int(td->iloc[1] + offset_clamped[1]);

    /* Compute handle clamping state to be drawn. */
    if (tdsq->sel_flag & SEQ_LEFTSEL) {
      strip->runtime.flag &= ~STRIP_CLAMPED_LH;
    }
    if (tdsq->sel_flag & SEQ_RIGHTSEL) {
      strip->runtime.flag &= ~STRIP_CLAMPED_RH;
    }
    if (!seq::transform_single_image_check(strip) && !strip->is_effect()) {
      if (offset_clamped[0] > offset[0] && new_frame == seq::time_start_frame_get(strip)) {
        strip->runtime.flag |= STRIP_CLAMPED_LH;
      }
      else if (offset_clamped[0] < offset[0] &&
               new_frame == seq::time_content_end_frame_get(scene, strip))
      {
        strip->runtime.flag |= STRIP_CLAMPED_RH;
      }
    }

    switch (tdsq->sel_flag) {
      case SELECT: {
        int offset = new_frame - tdsq->start_offset - strip->start;
        if (seq::transform_strip_can_be_translated(strip)) {
          seq::transform_translate_strip(scene, strip, offset);
          if (abs(offset) > abs(max_offset)) {
            max_offset = offset;
          }
        }
        seq::strip_channel_set(strip, new_channel);
        break;
      }
      case SEQ_LEFTSEL: { /* No vertical transform. */
        /* Update right handle first if both handles are selected and the `new_frame` is right of
         * the old one to avoid unexpected left handle clamping when canceling. See #126191. */
        const bool both_handles_selected = (tdsq->flag & (SEQ_LEFTSEL | SEQ_RIGHTSEL)) ==
                                           (SEQ_LEFTSEL | SEQ_RIGHTSEL);
        if (both_handles_selected && new_frame >= seq::time_right_handle_frame_get(scene, strip)) {
          /* For now, move the right handle far enough to avoid the left handle getting clamped.
           * The final, correct position will be calculated later. */
          seq::time_right_handle_frame_set(scene, strip, new_frame + 1);
        }

        int old_startdisp = seq::time_left_handle_frame_get(scene, strip);
        seq::time_left_handle_frame_set(scene, strip, new_frame);

        if (abs(seq::time_left_handle_frame_get(scene, strip) - old_startdisp) > abs(max_offset)) {
          max_offset = seq::time_left_handle_frame_get(scene, strip) - old_startdisp;
        }
        break;
      }
      case SEQ_RIGHTSEL: { /* No vertical transform. */
        int old_enddisp = seq::time_right_handle_frame_get(scene, strip);
        seq::time_right_handle_frame_set(scene, strip, new_frame);

        if (abs(seq::time_right_handle_frame_get(scene, strip) - old_enddisp) > abs(max_offset)) {
          max_offset = seq::time_right_handle_frame_get(scene, strip) - old_enddisp;
        }
        break;
      }
    }
  }

  TransSeq *ts = (TransSeq *)TRANS_DATA_CONTAINER_FIRST_SINGLE(t)->custom.type.data;

  /* Update animation for effects. */
  for (Strip *strip : ts->time_dependent_strips) {
    seq::offset_animdata(scene, strip, max_offset);
  }

  /* Need to do the overlap check in a new loop otherwise adjacent strips
   * will not be updated and we'll get false positives. */
  VectorSet transformed_strips = seq_transform_collection_from_transdata(tc);
  seq::iterator_set_expand(
      scene, seqbase_active_get(t), transformed_strips, seq::query_strip_effect_chain);

  for (Strip *strip : transformed_strips) {
    /* Test overlap, displays red outline. */
    strip->runtime.flag &= ~STRIP_OVERLAP;
    if (seq::transform_test_overlap(scene, seqbasep, strip)) {
      strip->runtime.flag |= STRIP_OVERLAP;
    }
  }
}

static void recalcData_sequencer(TransInfo *t)
{
  TransData *td;
  int a;
  Strip *strip_prev = nullptr;

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);
  Scene *scene = CTX_data_sequencer_scene(t->context);

  for (a = 0, td = tc->data; a < tc->data_len; a++, td++) {
    TransDataSeq *tdsq = (TransDataSeq *)td->extra;
    Strip *strip = tdsq->strip;

    if (strip != strip_prev) {
      seq::relations_invalidate_cache(scene, strip);
    }

    strip_prev = strip;
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);

  flushTransSeq(t);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Special After Transform Sequencer
 * \{ */

static void special_aftertrans_update__sequencer(bContext *C, TransInfo *t)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  SpaceSeq *sseq = (SpaceSeq *)t->area->spacedata.first;
  if ((sseq->flag & SPACE_SEQ_DESELECT_STRIP_HANDLE) != 0 &&
      transform_mode_edge_seq_slide_use_restore_handle_selection(t))
  {
    TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);
    VectorSet<Strip *> strips = seq_transform_collection_from_transdata(tc);
    for (Strip *strip : strips) {
      strip->flag &= ~(SEQ_LEFTSEL | SEQ_RIGHTSEL);
    }
  }

  sseq->flag &= ~SPACE_SEQ_DESELECT_STRIP_HANDLE;

  /* #freeSeqData in `transform_conversions.cc` does this
   * keep here so the `else` at the end won't run. */
  if (t->state == TRANS_CANCEL) {
    return;
  }

  /* Marker transform, not especially nice but we may want to move markers
   * at the same time as strips in the Video Sequencer. */
  if (sseq->flag & SEQ_MARKER_TRANS) {
    /* Can't use #TFM_TIME_EXTEND
     * for some reason EXTEND is changed into TRANSLATE, so use frame_side instead. */

    if (t->mode == TFM_SEQ_SLIDE) {
      if (t->frame_side == 'B') {
        ED_markers_post_apply_transform(
            &scene->markers, scene, TFM_TIME_TRANSLATE, t->values_final[0], t->frame_side);
      }
    }
    else if (ELEM(t->frame_side, 'L', 'R')) {
      ED_markers_post_apply_transform(
          &scene->markers, scene, TFM_TIME_EXTEND, t->values_final[0], t->frame_side);
    }
  }
}

bool transform_convert_sequencer_clamp(const TransInfo *t, float r_val[2])
{
  const TransSeq *ts = (TransSeq *)TRANS_DATA_CONTAINER_FIRST_SINGLE(t)->custom.type.data;
  int val[2] = {round_fl_to_int(r_val[0]), round_fl_to_int(r_val[1])};
  bool clamped = false;

  /* Unconditional channel, retiming key, and handle clamping. Should never be ignored. */
  if (BLI_rcti_clamp_pt_v(&ts->offset_clamp, val)) {
    r_val[0] = float(val[0]);
    r_val[1] = float(val[1]);
    clamped = true;
  }

  /* Optional clamping of handles to underlying holds. Can be disabled by the user. */
  if (t->modifiers & MOD_STRIP_CLAMP_HOLDS) {
    if (val[0] < ts->hold_clamp_min) {
      r_val[0] = float(ts->hold_clamp_min);
      clamped = true;
    }
    else if (val[0] > ts->hold_clamp_max) {
      r_val[0] = float(ts->hold_clamp_max);
      clamped = true;
    }
  }

  return clamped;
}

/** \} */

TransConvertTypeInfo TransConvertType_Sequencer = {
    /*flags*/ (T_POINTS | T_2D_EDIT),
    /*create_trans_data*/ createTransSeqData,
    /*recalc_data*/ recalcData_sequencer,
    /*special_aftertrans_update*/ special_aftertrans_update__sequencer,
};

}  // namespace blender::ed::transform
