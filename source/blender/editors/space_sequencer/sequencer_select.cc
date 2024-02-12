/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 */

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_math_geom.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "DNA_scene_types.h"

#include "BKE_context.hh"
#include "BKE_report.hh"

#include "WM_api.hh"
#include "WM_toolsystem.hh"
#include "WM_types.hh"

#include "RNA_define.hh"

#include "SEQ_channels.hh"
#include "SEQ_iterator.hh"
#include "SEQ_relations.hh"
#include "SEQ_retiming.hh"
#include "SEQ_select.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_time.hh"
#include "SEQ_transform.hh"
#include "SEQ_utils.hh"

/* For menu, popup, icons, etc. */

#include "ED_outliner.hh"
#include "ED_screen.hh"
#include "ED_select_utils.hh"
#include "ED_sequencer.hh"

#include "UI_view2d.hh"

/* Own include. */
#include "sequencer_intern.hh"

/* -------------------------------------------------------------------- */
/** \name Selection Utilities
 * \{ */

blender::VectorSet<Sequence *> all_strips_from_context(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene);
  ListBase *seqbase = SEQ_active_seqbase_get(ed);
  ListBase *channels = SEQ_channels_displayed_get(ed);

  const bool is_preview = sequencer_view_has_preview_poll(C);
  if (is_preview) {
    return SEQ_query_rendered_strips(scene, channels, seqbase, scene->r.cfra, 0);
  }

  return SEQ_query_all_strips(seqbase);
}

blender::VectorSet<Sequence *> selected_strips_from_context(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene);
  ListBase *seqbase = SEQ_active_seqbase_get(ed);
  ListBase *channels = SEQ_channels_displayed_get(ed);

  const bool is_preview = sequencer_view_has_preview_poll(C);

  if (is_preview) {
    blender::VectorSet strips = SEQ_query_rendered_strips(
        scene, channels, seqbase, scene->r.cfra, 0);
    strips.remove_if([&](Sequence *seq) { return (seq->flag & SELECT) == 0; });
    return strips;
  }

  return SEQ_query_selected_strips(seqbase);
}

static void select_surrounding_handles(Scene *scene, Sequence *test) /* XXX BRING BACK */
{
  Sequence *neighbor;

  neighbor = find_neighboring_sequence(scene, test, SEQ_SIDE_LEFT, -1);
  if (neighbor) {
    /* Only select neighbor handle if matching handle from test seq is also selected,
     * or if neighbor was not selected at all up till now.
     * Otherwise, we get odd mismatch when shift-alt-rmb selecting neighbor strips... */
    if (!(neighbor->flag & SELECT) || (test->flag & SEQ_LEFTSEL)) {
      neighbor->flag |= SEQ_RIGHTSEL;
    }
    neighbor->flag |= SELECT;
    recurs_sel_seq(neighbor);
  }
  neighbor = find_neighboring_sequence(scene, test, SEQ_SIDE_RIGHT, -1);
  if (neighbor) {
    if (!(neighbor->flag & SELECT) || (test->flag & SEQ_RIGHTSEL)) { /* See comment above. */
      neighbor->flag |= SEQ_LEFTSEL;
    }
    neighbor->flag |= SELECT;
    recurs_sel_seq(neighbor);
  }
}

/* Used for mouse selection in SEQUENCER_OT_select. */
static void select_active_side(
    const Scene *scene, ListBase *seqbase, int sel_side, int channel, int frame)
{

  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    if (channel == seq->machine) {
      switch (sel_side) {
        case SEQ_SIDE_LEFT:
          if (frame > SEQ_time_left_handle_frame_get(scene, seq)) {
            seq->flag &= ~(SEQ_RIGHTSEL | SEQ_LEFTSEL);
            seq->flag |= SELECT;
          }
          break;
        case SEQ_SIDE_RIGHT:
          if (frame < SEQ_time_left_handle_frame_get(scene, seq)) {
            seq->flag &= ~(SEQ_RIGHTSEL | SEQ_LEFTSEL);
            seq->flag |= SELECT;
          }
          break;
        case SEQ_SIDE_BOTH:
          seq->flag &= ~(SEQ_RIGHTSEL | SEQ_LEFTSEL);
          seq->flag |= SELECT;
          break;
      }
    }
  }
}

/* Used for mouse selection in SEQUENCER_OT_select_side. */
static void select_active_side_range(const Scene *scene,
                                     ListBase *seqbase,
                                     const int sel_side,
                                     const int frame_ranges[MAXSEQ],
                                     const int frame_ignore)
{
  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    if (seq->machine < MAXSEQ) {
      const int frame = frame_ranges[seq->machine];
      if (frame == frame_ignore) {
        continue;
      }
      switch (sel_side) {
        case SEQ_SIDE_LEFT:
          if (frame > SEQ_time_left_handle_frame_get(scene, seq)) {
            seq->flag &= ~(SEQ_RIGHTSEL | SEQ_LEFTSEL);
            seq->flag |= SELECT;
          }
          break;
        case SEQ_SIDE_RIGHT:
          if (frame < SEQ_time_left_handle_frame_get(scene, seq)) {
            seq->flag &= ~(SEQ_RIGHTSEL | SEQ_LEFTSEL);
            seq->flag |= SELECT;
          }
          break;
        case SEQ_SIDE_BOTH:
          seq->flag &= ~(SEQ_RIGHTSEL | SEQ_LEFTSEL);
          seq->flag |= SELECT;
          break;
      }
    }
  }
}

/* Used for mouse selection in SEQUENCER_OT_select */
static void select_linked_time(const Scene *scene, ListBase *seqbase, Sequence *seq_link)
{
  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    if (seq_link->machine != seq->machine) {
      int left_match = (SEQ_time_left_handle_frame_get(scene, seq) == seq_link->startdisp) ? 1 : 0;
      int right_match = (SEQ_time_right_handle_frame_get(scene, seq) == seq_link->enddisp) ? 1 : 0;

      if (left_match && right_match) {
        /* Direct match, copy the selection settings. */
        seq->flag &= ~(SELECT | SEQ_LEFTSEL | SEQ_RIGHTSEL);
        seq->flag |= seq_link->flag & (SELECT | SEQ_LEFTSEL | SEQ_RIGHTSEL);

        recurs_sel_seq(seq);
      }
      else if (seq_link->flag & SELECT && (left_match || right_match)) {

        /* Clear for reselection. */
        seq->flag &= ~(SEQ_LEFTSEL | SEQ_RIGHTSEL);

        if (left_match && seq_link->flag & SEQ_LEFTSEL) {
          seq->flag |= SELECT | SEQ_LEFTSEL;
        }

        if (right_match && seq_link->flag & SEQ_RIGHTSEL) {
          seq->flag |= SELECT | SEQ_RIGHTSEL;
        }

        recurs_sel_seq(seq);
      }
    }
  }
}

#if 0 /* BRING BACK */
void select_surround_from_last(Scene *scene)
{
  Sequence *seq = get_last_seq(scene);

  if (seq == nullptr) {
    return;
  }

  select_surrounding_handles(scene, seq);
}
#endif

void ED_sequencer_select_sequence_single(Scene *scene, Sequence *seq, bool deselect_all)
{
  Editing *ed = SEQ_editing_get(scene);

  if (deselect_all) {
    ED_sequencer_deselect_all(scene);
  }

  SEQ_select_active_set(scene, seq);

  if (ELEM(seq->type, SEQ_TYPE_IMAGE, SEQ_TYPE_MOVIE)) {
    if (seq->strip) {
      BLI_strncpy(ed->act_imagedir, seq->strip->dirpath, FILE_MAXDIR);
    }
  }
  else if (seq->type == SEQ_TYPE_SOUND_RAM) {
    if (seq->strip) {
      BLI_strncpy(ed->act_sounddir, seq->strip->dirpath, FILE_MAXDIR);
    }
  }
  seq->flag |= SELECT;
  recurs_sel_seq(seq);
}

void seq_rectf(const Scene *scene, Sequence *seq, rctf *rect)
{
  rect->xmin = SEQ_time_left_handle_frame_get(scene, seq);
  rect->xmax = SEQ_time_right_handle_frame_get(scene, seq);
  rect->ymin = seq->machine + SEQ_STRIP_OFSBOTTOM;
  rect->ymax = seq->machine + SEQ_STRIP_OFSTOP;
}

Sequence *find_neighboring_sequence(Scene *scene, Sequence *test, int lr, int sel)
{
  /* sel: 0==unselected, 1==selected, -1==don't care. */
  Editing *ed = SEQ_editing_get(scene);

  if (ed == nullptr) {
    return nullptr;
  }

  if (sel > 0) {
    sel = SELECT;
  }
  LISTBASE_FOREACH (Sequence *, seq, ed->seqbasep) {
    if ((seq != test) && (test->machine == seq->machine) &&
        ((sel == -1) || (sel && (seq->flag & SELECT)) || (sel == 0 && (seq->flag & SELECT) == 0)))
    {
      switch (lr) {
        case SEQ_SIDE_LEFT:
          if (SEQ_time_left_handle_frame_get(scene, test) ==
              SEQ_time_right_handle_frame_get(scene, seq))
          {
            return seq;
          }
          break;
        case SEQ_SIDE_RIGHT:
          if (SEQ_time_right_handle_frame_get(scene, test) ==
              SEQ_time_left_handle_frame_get(scene, seq))
          {
            return seq;
          }
          break;
      }
    }
  }
  return nullptr;
}

Sequence *find_nearest_seq(const Scene *scene, const View2D *v2d, const int mval[2], int *r_hand)
{
  Sequence *seq;
  Editing *ed = SEQ_editing_get(scene);
  float x, y;
  float pixelx;
  float handsize;
  float displen;
  *r_hand = SEQ_SIDE_NONE;

  if (ed == nullptr) {
    return nullptr;
  }

  pixelx = BLI_rctf_size_x(&v2d->cur) / BLI_rcti_size_x(&v2d->mask);

  UI_view2d_region_to_view(v2d, mval[0], mval[1], &x, &y);

  seq = static_cast<Sequence *>(ed->seqbasep->first);

  while (seq) {
    if (seq->machine == int(y)) {
      /* Check for both normal strips, and strips that have been flipped horizontally. */
      if (((SEQ_time_left_handle_frame_get(scene, seq) <
            SEQ_time_right_handle_frame_get(scene, seq)) &&
           (SEQ_time_left_handle_frame_get(scene, seq) <= x &&
            SEQ_time_right_handle_frame_get(scene, seq) >= x)) ||
          ((SEQ_time_left_handle_frame_get(scene, seq) >
            SEQ_time_right_handle_frame_get(scene, seq)) &&
           (SEQ_time_left_handle_frame_get(scene, seq) >= x &&
            SEQ_time_right_handle_frame_get(scene, seq) <= x)))
      {
        if (SEQ_transform_sequence_can_be_translated(seq)) {

          /* Clamp handles to defined size in pixel space. */
          handsize = 2.0f * sequence_handle_size_get_clamped(scene, seq, pixelx);
          displen = float(abs(SEQ_time_left_handle_frame_get(scene, seq) -
                              SEQ_time_right_handle_frame_get(scene, seq)));

          /* Don't even try to grab the handles of small strips. */
          if (displen / pixelx > 16) {

            /* Set the max value to handle to 1/3 of the total len when its
             * less than 28. This is important because otherwise selecting
             * handles happens even when you click in the middle. */
            if ((displen / 3) < 30 * pixelx) {
              handsize = displen / 3;
            }
            else {
              CLAMP(handsize, 7 * pixelx, 30 * pixelx);
            }

            if (handsize + SEQ_time_left_handle_frame_get(scene, seq) >= x) {
              *r_hand = SEQ_SIDE_LEFT;
            }
            else if (-handsize + SEQ_time_right_handle_frame_get(scene, seq) <= x) {
              *r_hand = SEQ_SIDE_RIGHT;
            }
          }
        }
        return seq;
      }
    }
    seq = static_cast<Sequence *>(seq->next);
  }
  return nullptr;
}

#if 0
static void select_neighbor_from_last(Scene *scene, int lr)
{
  Sequence *seq = SEQ_select_active_get(scene);
  Sequence *neighbor;
  bool changed = false;
  if (seq) {
    neighbor = find_neighboring_sequence(scene, seq, lr, -1);
    if (neighbor) {
      switch (lr) {
        case SEQ_SIDE_LEFT:
          neighbor->flag |= SELECT;
          recurs_sel_seq(neighbor);
          neighbor->flag |= SEQ_RIGHTSEL;
          seq->flag |= SEQ_LEFTSEL;
          break;
        case SEQ_SIDE_RIGHT:
          neighbor->flag |= SELECT;
          recurs_sel_seq(neighbor);
          neighbor->flag |= SEQ_LEFTSEL;
          seq->flag |= SEQ_RIGHTSEL;
          break;
      }
      seq->flag |= SELECT;
      changed = true;
    }
  }
  if (changed) {
    /* Pass. */
  }
}
#endif

void recurs_sel_seq(Sequence *seq_meta)
{
  Sequence *seq;
  seq = static_cast<Sequence *>(seq_meta->seqbase.first);

  while (seq) {

    if (seq_meta->flag & (SEQ_LEFTSEL + SEQ_RIGHTSEL)) {
      seq->flag &= ~SEQ_ALLSEL;
    }
    else if (seq_meta->flag & SELECT) {
      seq->flag |= SELECT;
    }
    else {
      seq->flag &= ~SEQ_ALLSEL;
    }

    if (seq->seqbase.first) {
      recurs_sel_seq(seq);
    }

    seq = static_cast<Sequence *>(seq->next);
  }
}

static bool seq_point_image_isect(const Scene *scene, const Sequence *seq, float point[2])
{
  float seq_image_quad[4][2];
  SEQ_image_transform_final_quad_get(scene, seq, seq_image_quad);
  return isect_point_quad_v2(
      point, seq_image_quad[0], seq_image_quad[1], seq_image_quad[2], seq_image_quad[3]);
}

static void sequencer_select_do_updates(bContext *C, Scene *scene)
{
  ED_outliner_select_sync_from_sequence_tag(C);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name (De)select All Operator
 * \{ */

static int sequencer_de_select_all_exec(bContext *C, wmOperator *op)
{
  int action = RNA_enum_get(op->ptr, "action");
  Scene *scene = CTX_data_scene(C);

  if (sequencer_view_has_preview_poll(C) && !sequencer_view_preview_only_poll(C)) {
    return OPERATOR_CANCELLED;
  }

  if (sequencer_retiming_mode_is_active(C) && retiming_keys_are_visible(C)) {
    return sequencer_retiming_select_all_exec(C, op);
  }

  blender::VectorSet strips = all_strips_from_context(C);

  if (action == SEL_TOGGLE) {
    action = SEL_SELECT;
    for (Sequence *seq : strips) {
      if (seq->flag & SEQ_ALLSEL) {
        action = SEL_DESELECT;
        break;
      }
    }
  }

  for (Sequence *seq : strips) {
    switch (action) {
      case SEL_SELECT:
        seq->flag &= ~(SEQ_LEFTSEL + SEQ_RIGHTSEL);
        seq->flag |= SELECT;
        break;
      case SEL_DESELECT:
        seq->flag &= ~SEQ_ALLSEL;
        break;
      case SEL_INVERT:
        if (seq->flag & SEQ_ALLSEL) {
          seq->flag &= ~SEQ_ALLSEL;
        }
        else {
          seq->flag &= ~(SEQ_LEFTSEL + SEQ_RIGHTSEL);
          seq->flag |= SELECT;
        }
        break;
    }
  }
  ED_outliner_select_sync_from_sequence_tag(C);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_select_all(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "(De)select All";
  ot->idname = "SEQUENCER_OT_select_all";
  ot->description = "Select or deselect all strips";

  /* Api callbacks. */
  ot->exec = sequencer_de_select_all_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_UNDO;

  WM_operator_properties_select_all(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Inverse Operator
 * \{ */

static int sequencer_select_inverse_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);

  if (sequencer_view_has_preview_poll(C) && !sequencer_view_preview_only_poll(C)) {
    return OPERATOR_CANCELLED;
  }

  blender::VectorSet strips = all_strips_from_context(C);

  for (Sequence *seq : strips) {
    if (seq->flag & SELECT) {
      seq->flag &= ~SEQ_ALLSEL;
    }
    else {
      seq->flag &= ~(SEQ_LEFTSEL + SEQ_RIGHTSEL);
      seq->flag |= SELECT;
    }
  }

  ED_outliner_select_sync_from_sequence_tag(C);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_select_inverse(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Select Inverse";
  ot->idname = "SEQUENCER_OT_select_inverse";
  ot->description = "Select unselected strips";

  /* Api callbacks. */
  ot->exec = sequencer_select_inverse_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Operator
 * \{ */

static void sequencer_select_set_active(Scene *scene, Sequence *seq)
{
  Editing *ed = SEQ_editing_get(scene);

  SEQ_select_active_set(scene, seq);

  if (ELEM(seq->type, SEQ_TYPE_IMAGE, SEQ_TYPE_MOVIE)) {
    if (seq->strip) {
      BLI_strncpy(ed->act_imagedir, seq->strip->dirpath, FILE_MAXDIR);
    }
  }
  else if (seq->type == SEQ_TYPE_SOUND_RAM) {
    if (seq->strip) {
      BLI_strncpy(ed->act_sounddir, seq->strip->dirpath, FILE_MAXDIR);
    }
  }
  recurs_sel_seq(seq);
}

static void sequencer_select_side_of_frame(const bContext *C,
                                           const View2D *v2d,
                                           const int mval[2],
                                           Scene *scene)
{
  Editing *ed = SEQ_editing_get(scene);

  const float x = UI_view2d_region_to_view_x(v2d, mval[0]);
  LISTBASE_FOREACH (Sequence *, seq_iter, SEQ_active_seqbase_get(ed)) {
    if (((x < scene->r.cfra) &&
         (SEQ_time_right_handle_frame_get(scene, seq_iter) <= scene->r.cfra)) ||
        ((x >= scene->r.cfra) &&
         (SEQ_time_left_handle_frame_get(scene, seq_iter) >= scene->r.cfra)))
    {
      /* Select left or right. */
      seq_iter->flag |= SELECT;
      recurs_sel_seq(seq_iter);
    }
  }

  {
    SpaceSeq *sseq = CTX_wm_space_seq(C);
    if (sseq && sseq->flag & SEQ_MARKER_TRANS) {

      LISTBASE_FOREACH (TimeMarker *, tmarker, &scene->markers) {
        if (((x < scene->r.cfra) && (tmarker->frame <= scene->r.cfra)) ||
            ((x >= scene->r.cfra) && (tmarker->frame >= scene->r.cfra)))
        {
          tmarker->flag |= SELECT;
        }
        else {
          tmarker->flag &= ~SELECT;
        }
      }
    }
  }
}

static void sequencer_select_linked_handle(const bContext *C,
                                           Sequence *seq,
                                           const int handle_clicked)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene);
  if (!ELEM(handle_clicked, SEQ_SIDE_LEFT, SEQ_SIDE_RIGHT)) {
    /* First click selects the strip and its adjacent handles (if valid).
     * Second click selects the strip,
     * both of its handles and its adjacent handles (if valid). */
    const bool is_striponly_selected = ((seq->flag & SEQ_ALLSEL) == SELECT);
    seq->flag &= ~SEQ_ALLSEL;
    seq->flag |= is_striponly_selected ? SEQ_ALLSEL : SELECT;
    select_surrounding_handles(scene, seq);
  }
  else {
    /* Always select the strip under the cursor. */
    seq->flag |= SELECT;

    /* First click selects adjacent handles on that side.
     * Second click selects all strips in that direction.
     * If there are no adjacent strips, it just selects all in that direction.
     */
    int sel_side = handle_clicked;
    Sequence *neighbor = find_neighboring_sequence(scene, seq, sel_side, -1);
    if (neighbor) {
      switch (sel_side) {
        case SEQ_SIDE_LEFT:
          if ((seq->flag & SEQ_LEFTSEL) && (neighbor->flag & SEQ_RIGHTSEL)) {
            seq->flag |= SELECT;
            select_active_side(scene,
                               ed->seqbasep,
                               SEQ_SIDE_LEFT,
                               seq->machine,
                               SEQ_time_left_handle_frame_get(scene, seq));
          }
          else {
            seq->flag |= SELECT;
            neighbor->flag |= SELECT;
            recurs_sel_seq(neighbor);
            neighbor->flag |= SEQ_RIGHTSEL;
            seq->flag |= SEQ_LEFTSEL;
          }
          break;
        case SEQ_SIDE_RIGHT:
          if ((seq->flag & SEQ_RIGHTSEL) && (neighbor->flag & SEQ_LEFTSEL)) {
            seq->flag |= SELECT;
            select_active_side(scene,
                               ed->seqbasep,
                               SEQ_SIDE_RIGHT,
                               seq->machine,
                               SEQ_time_left_handle_frame_get(scene, seq));
          }
          else {
            seq->flag |= SELECT;
            neighbor->flag |= SELECT;
            recurs_sel_seq(neighbor);
            neighbor->flag |= SEQ_LEFTSEL;
            seq->flag |= SEQ_RIGHTSEL;
          }
          break;
      }
    }
    else {

      select_active_side(
          scene, ed->seqbasep, sel_side, seq->machine, SEQ_time_left_handle_frame_get(scene, seq));
    }
  }
}

/** Collect sequencer that are candidates for being selected. */
struct SeqSelect_Link {
  SeqSelect_Link *next, *prev;
  Sequence *seq;
  /** Only use for center selection. */
  float center_dist_sq;
};

static int seq_sort_for_depth_select(const void *a, const void *b)
{
  const SeqSelect_Link *slink_a = static_cast<const SeqSelect_Link *>(a);
  const SeqSelect_Link *slink_b = static_cast<const SeqSelect_Link *>(b);

  /* Exactly overlapping strips, sort by machine (so the top-most is first). */
  if (slink_a->seq->machine < slink_b->seq->machine) {
    return 1;
  }
  if (slink_a->seq->machine > slink_b->seq->machine) {
    return -1;
  }
  return 0;
}

static int seq_sort_for_center_select(const void *a, const void *b)
{
  const SeqSelect_Link *slink_a = static_cast<const SeqSelect_Link *>(a);
  const SeqSelect_Link *slink_b = static_cast<const SeqSelect_Link *>(b);
  if (slink_a->center_dist_sq > slink_b->center_dist_sq) {
    return 1;
  }
  if (slink_a->center_dist_sq < slink_b->center_dist_sq) {
    return -1;
  }

  /* Exactly overlapping strips, use depth. */
  return seq_sort_for_depth_select(a, b);
}

/**
 * Check if click happened on image which belongs to strip.
 * If multiple strips are found, loop through them in order
 * (depth (top-most first) or closest to mouse when `center` is true).
 */
static Sequence *seq_select_seq_from_preview(
    const bContext *C, const int mval[2], const bool toggle, const bool extend, const bool center)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene);
  ListBase *seqbase = SEQ_active_seqbase_get(ed);
  ListBase *channels = SEQ_channels_displayed_get(ed);
  SpaceSeq *sseq = CTX_wm_space_seq(C);
  View2D *v2d = UI_view2d_fromcontext(C);

  float mouseco_view[2];
  UI_view2d_region_to_view(v2d, mval[0], mval[1], &mouseco_view[0], &mouseco_view[1]);

  /* Always update the coordinates (check extended after). */
  const bool use_cycle = (!WM_cursor_test_motion_and_update(mval) || extend || toggle);

  /* Allow strips this far from the closest center to be included.
   * This allows cycling over center points which are near enough
   * to overlapping from the users perspective. */
  const float center_dist_sq_max = square_f(75.0f * U.pixelsize);
  const float center_scale_px[2] = {
      UI_view2d_scale_get_x(v2d),
      UI_view2d_scale_get_y(v2d),
  };

  blender::VectorSet strips = SEQ_query_rendered_strips(
      scene, channels, seqbase, scene->r.cfra, sseq->chanshown);

  SeqSelect_Link *slink_active = nullptr;
  Sequence *seq_active = SEQ_select_active_get(scene);
  ListBase strips_ordered = {nullptr};
  for (Sequence *seq : strips) {
    bool isect = false;
    float center_dist_sq_test = 0.0f;
    if (center) {
      /* Detect overlapping center points (scaled by the zoom level). */
      float co[2];
      SEQ_image_transform_origin_offset_pixelspace_get(scene, seq, co);
      sub_v2_v2(co, mouseco_view);
      mul_v2_v2(co, center_scale_px);
      center_dist_sq_test = len_squared_v2(co);
      isect = center_dist_sq_test <= center_dist_sq_max;
      if (isect) {
        /* Use an active strip penalty for "center" selection when cycle is enabled. */
        if (use_cycle && (seq == seq_active) && (seq_active->flag & SELECT)) {
          center_dist_sq_test = square_f(sqrtf(center_dist_sq_test) + (3.0f * U.pixelsize));
        }
      }
    }
    else {
      isect = seq_point_image_isect(scene, seq, mouseco_view);
    }

    if (isect) {
      SeqSelect_Link *slink = MEM_cnew<SeqSelect_Link>(__func__);
      slink->seq = seq;
      slink->center_dist_sq = center_dist_sq_test;
      BLI_addtail(&strips_ordered, slink);

      if (seq == seq_active) {
        slink_active = slink;
      }
    }
  }

  BLI_listbase_sort(&strips_ordered,
                    center ? seq_sort_for_center_select : seq_sort_for_depth_select);

  SeqSelect_Link *slink_select = static_cast<SeqSelect_Link *>(strips_ordered.first);
  Sequence *seq_select = nullptr;
  if (slink_select != nullptr) {
    /* Only use special behavior for the active strip when it's selected. */
    if ((center == false) && slink_active && (seq_active->flag & SELECT)) {
      if (use_cycle) {
        if (slink_active->next) {
          slink_select = slink_active->next;
        }
      }
      else {
        /* Match object selection behavior: keep the current active item unless cycle is enabled.
         * Clicking again in the same location will cycle away from the active object. */
        slink_select = slink_active;
      }
    }
    seq_select = slink_select->seq;
  }

  BLI_freelistN(&strips_ordered);

  return seq_select;
}

static bool element_already_selected(const Sequence *seq, const int handle_clicked)
{
  const bool handle_already_selected = ((handle_clicked == SEQ_SIDE_LEFT) &&
                                        (seq->flag & SEQ_LEFTSEL)) ||
                                       ((handle_clicked == SEQ_SIDE_RIGHT) &&
                                        (seq->flag & SEQ_RIGHTSEL));
  return ((seq->flag & SELECT) && handle_clicked == SEQ_SIDE_NONE) || handle_already_selected;
}

static void sequencer_select_strip_impl(const Editing *ed,
                                        Sequence *seq,
                                        const int handle_clicked,
                                        const bool extend,
                                        const bool deselect,
                                        const bool toggle)
{
  const bool is_active = (ed->act_seq == seq);

  /* Exception for active strip handles. */
  if ((handle_clicked != SEQ_SIDE_NONE) && (seq->flag & SELECT) && is_active && toggle) {
    switch (handle_clicked) {
      case SEQ_SIDE_LEFT:
        seq->flag ^= SEQ_LEFTSEL;
        break;
      case SEQ_SIDE_RIGHT:
        seq->flag ^= SEQ_RIGHTSEL;
        break;
    }
    return;
  }

  /* Select strip. */
  /* Match object selection behavior. */
  int action = -1;
  if (extend) {
    action = 1;
  }
  else if (deselect) {
    action = 0;
  }
  else {
    if (!((seq->flag & SELECT) && is_active)) {
      action = 1;
    }
    else if (toggle) {
      action = 0;
    }
  }

  if (action == 1) {
    seq->flag |= SELECT;
    if (handle_clicked == SEQ_SIDE_LEFT) {
      seq->flag |= SEQ_LEFTSEL;
    }
    if (handle_clicked == SEQ_SIDE_RIGHT) {
      seq->flag |= SEQ_RIGHTSEL;
    }
  }
  else if (action == 0) {
    seq->flag &= ~SEQ_ALLSEL;
  }
}

static bool use_retiming_mode(const bContext *C, const Sequence *seq_key_test)
{
  return seq_key_test && SEQ_retiming_data_is_editable(seq_key_test) &&
         !sequencer_retiming_mode_is_active(C) && retiming_keys_are_visible(C);
}

int sequencer_select_exec(bContext *C, wmOperator *op)
{
  View2D *v2d = UI_view2d_fromcontext(C);
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene);
  ARegion *region = CTX_wm_region(C);

  if (ed == nullptr) {
    return OPERATOR_CANCELLED;
  }

  if (region->regiontype == RGN_TYPE_PREVIEW) {
    if (!sequencer_view_preview_only_poll(C)) {
      return OPERATOR_CANCELLED;
    }
    const SpaceSeq *sseq = CTX_wm_space_seq(C);
    if (sseq->mainb != SEQ_DRAW_IMG_IMBUF) {
      return OPERATOR_CANCELLED;
    }
  }

  if (sequencer_retiming_mode_is_active(C) && retiming_keys_are_visible(C)) {
    return sequencer_retiming_key_select_exec(C, op);
  }

  bool extend = RNA_boolean_get(op->ptr, "extend");
  bool deselect = RNA_boolean_get(op->ptr, "deselect");
  bool deselect_all = RNA_boolean_get(op->ptr, "deselect_all");
  bool toggle = RNA_boolean_get(op->ptr, "toggle");
  bool center = RNA_boolean_get(op->ptr, "center");

  int mval[2];
  mval[0] = RNA_int_get(op->ptr, "mouse_x");
  mval[1] = RNA_int_get(op->ptr, "mouse_y");

  int handle_clicked = SEQ_SIDE_NONE;
  Sequence *seq = nullptr;
  if (region->regiontype == RGN_TYPE_PREVIEW) {
    seq = seq_select_seq_from_preview(C, mval, toggle, extend, center);
  }
  else {
    seq = find_nearest_seq(scene, v2d, mval, &handle_clicked);
  }

  Sequence *seq_key_test = nullptr;
  SeqRetimingKey *key = retiming_mousover_key_get(C, mval, &seq_key_test);

  /* NOTE: `side_of_frame` and `linked_time` functionality is designed to be shared on one keymap,
   * therefore both properties can be true at the same time. */
  if (seq && RNA_boolean_get(op->ptr, "linked_time")) {
    if (use_retiming_mode(C, seq_key_test)) {
      return sequencer_retiming_select_linked_time(C, op);
    }
    else {
      if (!extend && !toggle) {
        ED_sequencer_deselect_all(scene);
      }
      sequencer_select_strip_impl(ed, seq, handle_clicked, extend, deselect, toggle);
      select_linked_time(scene, ed->seqbasep, seq);
      sequencer_select_do_updates(C, scene);
      sequencer_select_set_active(scene, seq);
      return OPERATOR_FINISHED;
    }
  }

  /* Select left, right or overlapping the current frame. */
  if (RNA_boolean_get(op->ptr, "side_of_frame")) {
    if (!extend && !toggle) {
      ED_sequencer_deselect_all(scene);
    }
    sequencer_select_side_of_frame(C, v2d, mval, scene);
    sequencer_select_do_updates(C, scene);
    return OPERATOR_FINISHED;
  }

  /* On Alt selection, select the strip and bordering handles. */
  if (seq && RNA_boolean_get(op->ptr, "linked_handle")) {
    if (!extend && !toggle) {
      ED_sequencer_deselect_all(scene);
    }
    sequencer_select_linked_handle(C, seq, handle_clicked);
    sequencer_select_do_updates(C, scene);
    sequencer_select_set_active(scene, seq);
    return OPERATOR_FINISHED;
  }

  const bool wait_to_deselect_others = RNA_boolean_get(op->ptr, "wait_to_deselect_others");

  /* Clicking on already selected element falls on modal operation.
   * All strips are deselected on mouse button release unless extend mode is used. */
  if (seq && element_already_selected(seq, handle_clicked) && wait_to_deselect_others && !toggle) {
    return OPERATOR_RUNNING_MODAL;
  }

  if (use_retiming_mode(C, seq_key_test)) {

    /* Realize "fake" key, if it is clicked on. */
    if (key == nullptr && seq_key_test != nullptr) {
      key = try_to_realize_virtual_key(C, seq_key_test, mval);
    }

    bool retiming_key_clicked = (key != nullptr);

    if (seq_key_test && retiming_key_clicked) {
      WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
      ED_sequencer_deselect_all(scene);
      SEQ_retiming_selection_clear(ed);
      SEQ_retiming_selection_append(key);
      return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
    }
  }

  bool changed = false;

  /* Deselect everything */
  if (deselect_all || (seq && (extend == false && deselect == false && toggle == false))) {
    changed |= ED_sequencer_deselect_all(scene);
  }

  /* Nothing to select, but strips could be deselected. */
  if (!seq) {
    if (changed) {
      sequencer_select_do_updates(C, scene);
    }
    return changed ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
  }

  /* Do actual selection. */
  sequencer_select_strip_impl(ed, seq, handle_clicked, extend, deselect, toggle);

  sequencer_select_do_updates(C, scene);
  sequencer_select_set_active(scene, seq);
  return OPERATOR_FINISHED;
}

static int sequencer_select_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const int retval = WM_generic_select_invoke(C, op, event);
  ARegion *region = CTX_wm_region(C);
  if (region && (region->regiontype == RGN_TYPE_PREVIEW)) {
    return WM_operator_flag_only_pass_through_on_press(retval, event);
  }
  return retval;
}

void SEQUENCER_OT_select(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers. */
  ot->name = "Select";
  ot->idname = "SEQUENCER_OT_select";
  ot->description = "Select a strip (last selected becomes the \"active strip\")";

  /* Api callbacks. */
  ot->exec = sequencer_select_exec;
  ot->invoke = sequencer_select_invoke;
  ot->modal = WM_generic_select_modal;
  ot->poll = ED_operator_sequencer_active;
  ot->get_name = ED_select_pick_get_name;

  /* Flags. */
  ot->flag = OPTYPE_UNDO;

  /* Properties. */
  WM_operator_properties_generic_select(ot);

  WM_operator_properties_mouse_select(ot);

  prop = RNA_def_boolean(
      ot->srna,
      "center",
      false,
      "Center",
      "Use the object center when selecting, in edit mode used to extend object selection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna,
                         "linked_handle",
                         false,
                         "Linked Handle",
                         "Select handles next to the active strip");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(
      ot->srna, "linked_time", false, "Linked Time", "Select other strips at the same time");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(
      ot->srna,
      "side_of_frame",
      false,
      "Side of Frame",
      "Select all strips on same side of the current frame as the mouse cursor");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select More Operator
 * \{ */

/* Run recursively to select linked. */
static bool select_linked_internal(Scene *scene)
{
  Editing *ed = SEQ_editing_get(scene);

  if (ed == nullptr) {
    return false;
  }

  bool changed = false;

  LISTBASE_FOREACH (Sequence *, seq, SEQ_active_seqbase_get(ed)) {
    if ((seq->flag & SELECT) == 0) {
      continue;
    }
    /* Only get unselected neighbors. */
    Sequence *neighbor = find_neighboring_sequence(scene, seq, SEQ_SIDE_LEFT, 0);
    if (neighbor) {
      neighbor->flag |= SELECT;
      recurs_sel_seq(neighbor);
      changed = true;
    }
    neighbor = find_neighboring_sequence(scene, seq, SEQ_SIDE_RIGHT, 0);
    if (neighbor) {
      neighbor->flag |= SELECT;
      recurs_sel_seq(neighbor);
      changed = true;
    }
  }

  return changed;
}

/* Select only one linked strip on each side. */
static bool select_more_less_seq__internal(Scene *scene, bool select_more)
{
  Editing *ed = SEQ_editing_get(scene);

  if (ed == nullptr) {
    return false;
  }

  GSet *neighbors = BLI_gset_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "Linked strips");
  const int neighbor_selection_filter = select_more ? 0 : SELECT;
  const int selection_filter = select_more ? SELECT : 0;

  LISTBASE_FOREACH (Sequence *, seq, SEQ_active_seqbase_get(ed)) {
    if ((seq->flag & SELECT) != selection_filter) {
      continue;
    }
    Sequence *neighbor = find_neighboring_sequence(
        scene, seq, SEQ_SIDE_LEFT, neighbor_selection_filter);
    if (neighbor) {
      BLI_gset_add(neighbors, neighbor);
    }
    neighbor = find_neighboring_sequence(scene, seq, SEQ_SIDE_RIGHT, neighbor_selection_filter);
    if (neighbor) {
      BLI_gset_add(neighbors, neighbor);
    }
  }

  bool changed = false;
  GSetIterator gsi;
  BLI_gsetIterator_init(&gsi, neighbors);
  while (!BLI_gsetIterator_done(&gsi)) {
    Sequence *neighbor = static_cast<Sequence *>(BLI_gsetIterator_getKey(&gsi));
    if (select_more) {
      neighbor->flag |= SELECT;
      recurs_sel_seq(neighbor);
    }
    else {
      neighbor->flag &= ~SELECT;
    }
    changed = true;
    BLI_gsetIterator_step(&gsi);
  }

  BLI_gset_free(neighbors, nullptr);
  return changed;
}

static int sequencer_select_more_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);

  if (!select_more_less_seq__internal(scene, true)) {
    return OPERATOR_CANCELLED;
  }

  ED_outliner_select_sync_from_sequence_tag(C);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_select_more(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Select More";
  ot->idname = "SEQUENCER_OT_select_more";
  ot->description = "Select more strips adjacent to the current selection";

  /* Api callbacks. */
  ot->exec = sequencer_select_more_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Less Operator
 * \{ */

static int sequencer_select_less_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);

  if (!select_more_less_seq__internal(scene, false)) {
    return OPERATOR_CANCELLED;
  }

  ED_outliner_select_sync_from_sequence_tag(C);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_select_less(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Select Less";
  ot->idname = "SEQUENCER_OT_select_less";
  ot->description = "Shrink the current selection of adjacent selected strips";

  /* Api callbacks. */
  ot->exec = sequencer_select_less_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Pick Linked Operator
 * \{ */

static int sequencer_select_linked_pick_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Scene *scene = CTX_data_scene(C);
  View2D *v2d = UI_view2d_fromcontext(C);

  bool extend = RNA_boolean_get(op->ptr, "extend");

  Sequence *mouse_seq;
  int selected, hand;

  /* This works like UV, not mesh. */
  mouse_seq = find_nearest_seq(scene, v2d, event->mval, &hand);
  if (!mouse_seq) {
    return OPERATOR_FINISHED; /* User error as with mesh?? */
  }

  if (extend == 0) {
    ED_sequencer_deselect_all(scene);
  }

  mouse_seq->flag |= SELECT;
  recurs_sel_seq(mouse_seq);

  selected = 1;
  while (selected) {
    selected = select_linked_internal(scene);
  }

  ED_outliner_select_sync_from_sequence_tag(C);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_select_linked_pick(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Select Pick Linked";
  ot->idname = "SEQUENCER_OT_select_linked_pick";
  ot->description = "Select a chain of linked strips nearest to the mouse pointer";

  /* Api callbacks. */
  ot->invoke = sequencer_select_linked_pick_invoke;
  ot->poll = ED_operator_sequencer_active;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties. */
  PropertyRNA *prop;
  prop = RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend the selection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Linked Operator
 * \{ */

static int sequencer_select_linked_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  bool selected;

  selected = true;
  while (selected) {
    selected = select_linked_internal(scene);
  }

  ED_outliner_select_sync_from_sequence_tag(C);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_select_linked(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Select Linked";
  ot->idname = "SEQUENCER_OT_select_linked";
  ot->description = "Select all strips adjacent to the current selection";

  /* Api callbacks. */
  ot->exec = sequencer_select_linked_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Handles Operator
 * \{ */

enum {
  SEQ_SELECT_HANDLES_SIDE_LEFT,
  SEQ_SELECT_HANDLES_SIDE_RIGHT,
  SEQ_SELECT_HANDLES_SIDE_BOTH,
  SEQ_SELECT_HANDLES_SIDE_LEFT_NEIGHBOR,
  SEQ_SELECT_HANDLES_SIDE_RIGHT_NEIGHBOR,
  SEQ_SELECT_HANDLES_SIDE_BOTH_NEIGHBORS,
};

static const EnumPropertyItem prop_select_handles_side_types[] = {
    {SEQ_SELECT_HANDLES_SIDE_LEFT, "LEFT", 0, "Left", ""},
    {SEQ_SELECT_HANDLES_SIDE_RIGHT, "RIGHT", 0, "Right", ""},
    {SEQ_SELECT_HANDLES_SIDE_BOTH, "BOTH", 0, "Both", ""},
    {SEQ_SELECT_HANDLES_SIDE_LEFT_NEIGHBOR, "LEFT_NEIGHBOR", 0, "Left Neighbor", ""},
    {SEQ_SELECT_HANDLES_SIDE_RIGHT_NEIGHBOR, "RIGHT_NEIGHBOR", 0, "Right Neighbor", ""},
    {SEQ_SELECT_HANDLES_SIDE_BOTH_NEIGHBORS, "BOTH_NEIGHBORS", 0, "Both Neighbors", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static int sequencer_select_handles_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene);
  int sel_side = RNA_enum_get(op->ptr, "side");
  LISTBASE_FOREACH (Sequence *, seq, ed->seqbasep) {
    if (seq->flag & SELECT) {
      Sequence *l_neighbor = find_neighboring_sequence(scene, seq, SEQ_SIDE_LEFT, -1);
      Sequence *r_neighbor = find_neighboring_sequence(scene, seq, SEQ_SIDE_RIGHT, -1);

      switch (sel_side) {
        case SEQ_SELECT_HANDLES_SIDE_LEFT:
          seq->flag &= ~SEQ_RIGHTSEL;
          seq->flag |= SEQ_LEFTSEL;
          break;
        case SEQ_SELECT_HANDLES_SIDE_RIGHT:
          seq->flag &= ~SEQ_LEFTSEL;
          seq->flag |= SEQ_RIGHTSEL;
          break;
        case SEQ_SELECT_HANDLES_SIDE_BOTH:
          seq->flag |= SEQ_LEFTSEL | SEQ_RIGHTSEL;
          break;
        case SEQ_SELECT_HANDLES_SIDE_LEFT_NEIGHBOR:
          if (l_neighbor) {
            if (!(l_neighbor->flag & SELECT)) {
              l_neighbor->flag |= SEQ_RIGHTSEL;
            }
          }
          break;
        case SEQ_SELECT_HANDLES_SIDE_RIGHT_NEIGHBOR:
          if (r_neighbor) {
            if (!(r_neighbor->flag & SELECT)) {
              r_neighbor->flag |= SEQ_LEFTSEL;
            }
          }
          break;
        case SEQ_SELECT_HANDLES_SIDE_BOTH_NEIGHBORS:
          if (l_neighbor) {
            if (!(l_neighbor->flag & SELECT)) {
              l_neighbor->flag |= SEQ_RIGHTSEL;
            }
          }
          if (r_neighbor) {
            if (!(r_neighbor->flag & SELECT)) {
              r_neighbor->flag |= SEQ_LEFTSEL;
            }
            break;
          }
      }
    }
  }
  /*   Select strips */
  LISTBASE_FOREACH (Sequence *, seq, ed->seqbasep) {
    if ((seq->flag & SEQ_LEFTSEL) || (seq->flag & SEQ_RIGHTSEL)) {
      if (!(seq->flag & SELECT)) {
        seq->flag |= SELECT;
        recurs_sel_seq(seq);
      }
    }
  }

  ED_outliner_select_sync_from_sequence_tag(C);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_select_handles(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Select Handles";
  ot->idname = "SEQUENCER_OT_select_handles";
  ot->description = "Select gizmo handles on the sides of the selected strip";

  /* Api callbacks. */
  ot->exec = sequencer_select_handles_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties. */
  RNA_def_enum(ot->srna,
               "side",
               prop_select_handles_side_types,
               SEQ_SELECT_HANDLES_SIDE_BOTH,
               "Side",
               "The side of the handle that is selected");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Side of Frame Operator
 * \{ */

static int sequencer_select_side_of_frame_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene);
  const bool extend = RNA_boolean_get(op->ptr, "extend");
  const int side = RNA_enum_get(op->ptr, "side");

  if (ed == nullptr) {
    return OPERATOR_CANCELLED;
  }
  if (extend == false) {
    ED_sequencer_deselect_all(scene);
  }
  const int timeline_frame = scene->r.cfra;
  LISTBASE_FOREACH (Sequence *, seq, SEQ_active_seqbase_get(ed)) {
    bool test = false;
    switch (side) {
      case -1:
        test = (timeline_frame >= SEQ_time_right_handle_frame_get(scene, seq));
        break;
      case 1:
        test = (timeline_frame <= SEQ_time_left_handle_frame_get(scene, seq));
        break;
      case 2:
        test = SEQ_time_strip_intersects_frame(scene, seq, timeline_frame);
        break;
    }

    if (test) {
      seq->flag |= SELECT;
      recurs_sel_seq(seq);
    }
  }

  ED_outliner_select_sync_from_sequence_tag(C);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_select_side_of_frame(wmOperatorType *ot)
{
  static const EnumPropertyItem sequencer_select_left_right_types[] = {
      {-1, "LEFT", 0, "Left", "Select to the left of the current frame"},
      {1, "RIGHT", 0, "Right", "Select to the right of the current frame"},
      {2, "CURRENT", 0, "Current Frame", "Select intersecting with the current frame"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* Identifiers. */
  ot->name = "Select Side of Frame";
  ot->idname = "SEQUENCER_OT_select_side_of_frame";
  ot->description = "Select strips relative to the current frame";

  /* Api callbacks. */
  ot->exec = sequencer_select_side_of_frame_exec;
  ot->poll = ED_operator_sequencer_active;

  /* Flags. */
  ot->flag = OPTYPE_UNDO;

  /* Properties. */
  PropertyRNA *prop;
  prop = RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend the selection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  ot->prop = RNA_def_enum(ot->srna, "side", sequencer_select_left_right_types, 0, "Side", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Side Operator
 * \{ */

static int sequencer_select_side_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene);

  const int sel_side = RNA_enum_get(op->ptr, "side");
  const int frame_init = sel_side == SEQ_SIDE_LEFT ? INT_MIN : INT_MAX;
  int frame_ranges[MAXSEQ];
  bool selected = false;

  copy_vn_i(frame_ranges, ARRAY_SIZE(frame_ranges), frame_init);

  LISTBASE_FOREACH (Sequence *, seq, ed->seqbasep) {
    if (UNLIKELY(seq->machine >= MAXSEQ)) {
      continue;
    }
    int *frame_limit_p = &frame_ranges[seq->machine];
    if (seq->flag & SELECT) {
      selected = true;
      if (sel_side == SEQ_SIDE_LEFT) {
        *frame_limit_p = max_ii(*frame_limit_p, SEQ_time_left_handle_frame_get(scene, seq));
      }
      else {
        *frame_limit_p = min_ii(*frame_limit_p, SEQ_time_left_handle_frame_get(scene, seq));
      }
    }
  }

  if (selected == false) {
    return OPERATOR_CANCELLED;
  }

  select_active_side_range(scene, ed->seqbasep, sel_side, frame_ranges, frame_init);

  ED_outliner_select_sync_from_sequence_tag(C);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_select_side(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Select Side";
  ot->idname = "SEQUENCER_OT_select_side";
  ot->description = "Select strips on the nominated side of the selected strips";

  /* Api callbacks. */
  ot->exec = sequencer_select_side_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties. */
  RNA_def_enum(ot->srna,
               "side",
               prop_side_types,
               SEQ_SIDE_BOTH,
               "Side",
               "The side to which the selection is applied");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Box Select Operator
 * \{ */

static bool seq_box_select_rect_image_isect(const Scene *scene, const Sequence *seq, rctf *rect)
{
  float seq_image_quad[4][2];
  SEQ_image_transform_final_quad_get(scene, seq, seq_image_quad);
  float rect_quad[4][2] = {{rect->xmax, rect->ymax},
                           {rect->xmax, rect->ymin},
                           {rect->xmin, rect->ymin},
                           {rect->xmin, rect->ymax}};

  return seq_point_image_isect(scene, seq, rect_quad[0]) ||
         seq_point_image_isect(scene, seq, rect_quad[1]) ||
         seq_point_image_isect(scene, seq, rect_quad[2]) ||
         seq_point_image_isect(scene, seq, rect_quad[3]) ||
         isect_point_quad_v2(
             seq_image_quad[0], rect_quad[0], rect_quad[1], rect_quad[2], rect_quad[3]) ||
         isect_point_quad_v2(
             seq_image_quad[1], rect_quad[0], rect_quad[1], rect_quad[2], rect_quad[3]) ||
         isect_point_quad_v2(
             seq_image_quad[2], rect_quad[0], rect_quad[1], rect_quad[2], rect_quad[3]) ||
         isect_point_quad_v2(
             seq_image_quad[3], rect_quad[0], rect_quad[1], rect_quad[2], rect_quad[3]);
}

static void seq_box_select_seq_from_preview(const bContext *C, rctf *rect, const eSelectOp mode)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene);
  ListBase *seqbase = SEQ_active_seqbase_get(ed);
  ListBase *channels = SEQ_channels_displayed_get(ed);
  SpaceSeq *sseq = CTX_wm_space_seq(C);

  blender::VectorSet strips = SEQ_query_rendered_strips(
      scene, channels, seqbase, scene->r.cfra, sseq->chanshown);
  for (Sequence *seq : strips) {
    if (!seq_box_select_rect_image_isect(scene, seq, rect)) {
      continue;
    }

    if (ELEM(mode, SEL_OP_ADD, SEL_OP_SET)) {
      seq->flag |= SELECT;
    }
    else {
      BLI_assert(mode == SEL_OP_SUB);
      seq->flag &= ~SELECT;
    }
  }
}

static int sequencer_box_select_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  View2D *v2d = UI_view2d_fromcontext(C);
  Editing *ed = SEQ_editing_get(scene);

  if (ed == nullptr) {
    return OPERATOR_CANCELLED;
  }

  if (sequencer_retiming_mode_is_active(C) && retiming_keys_are_visible(C)) {
    return sequencer_retiming_box_select_exec(C, op);
  }

  const eSelectOp sel_op = eSelectOp(RNA_enum_get(op->ptr, "mode"));
  const bool handles = RNA_boolean_get(op->ptr, "include_handles");
  const bool select = (sel_op != SEL_OP_SUB);

  bool changed = false;

  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    changed |= ED_sequencer_deselect_all(scene);
  }

  rctf rectf;
  WM_operator_properties_border_to_rctf(op, &rectf);
  UI_view2d_region_to_view_rctf(v2d, &rectf, &rectf);

  ARegion *region = CTX_wm_region(C);
  if (region->regiontype == RGN_TYPE_PREVIEW) {
    if (!sequencer_view_preview_only_poll(C)) {
      return OPERATOR_CANCELLED;
    }
    seq_box_select_seq_from_preview(C, &rectf, sel_op);
    sequencer_select_do_updates(C, scene);
    return OPERATOR_FINISHED;
  }

  LISTBASE_FOREACH (Sequence *, seq, ed->seqbasep) {
    rctf rq;
    seq_rectf(scene, seq, &rq);
    if (BLI_rctf_isect(&rq, &rectf, nullptr)) {
      if (handles) {
        /* Get the handles draw size. */
        float pixelx = BLI_rctf_size_x(&v2d->cur) / BLI_rcti_size_x(&v2d->mask);
        float handsize = sequence_handle_size_get_clamped(scene, seq, pixelx);

        /* Right handle. */
        if (rectf.xmax > (SEQ_time_right_handle_frame_get(scene, seq) - handsize)) {
          if (select) {
            seq->flag |= SELECT | SEQ_RIGHTSEL;
          }
          else {
            /* Deselect the strip if it's left with no handles selected. */
            if ((seq->flag & SEQ_RIGHTSEL) && ((seq->flag & SEQ_LEFTSEL) == 0)) {
              seq->flag &= ~SELECT;
            }
            seq->flag &= ~SEQ_RIGHTSEL;
          }

          changed = true;
        }
        /* Left handle. */
        if (rectf.xmin < (SEQ_time_left_handle_frame_get(scene, seq) + handsize)) {
          if (select) {
            seq->flag |= SELECT | SEQ_LEFTSEL;
          }
          else {
            /* Deselect the strip if it's left with no handles selected. */
            if ((seq->flag & SEQ_LEFTSEL) && ((seq->flag & SEQ_RIGHTSEL) == 0)) {
              seq->flag &= ~SELECT;
            }
            seq->flag &= ~SEQ_LEFTSEL;
          }
        }

        changed = true;
      }

      /* Regular box selection. */
      else {
        SET_FLAG_FROM_TEST(seq->flag, select, SELECT);
        seq->flag &= ~(SEQ_LEFTSEL | SEQ_RIGHTSEL);
        changed = true;
      }
    }
  }

  if (!changed) {
    return OPERATOR_CANCELLED;
  }

  sequencer_select_do_updates(C, scene);

  return OPERATOR_FINISHED;
}

static int sequencer_box_select_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Scene *scene = CTX_data_scene(C);
  View2D *v2d = &CTX_wm_region(C)->v2d;
  ARegion *region = CTX_wm_region(C);

  if (region->regiontype == RGN_TYPE_PREVIEW && !sequencer_view_preview_only_poll(C)) {
    return OPERATOR_CANCELLED;
  }

  const bool tweak = RNA_boolean_get(op->ptr, "tweak");

  if (tweak) {
    int hand_dummy;
    int mval[2];
    WM_event_drag_start_mval(event, region, mval);
    Sequence *seq = find_nearest_seq(scene, v2d, mval, &hand_dummy);
    if (seq != nullptr) {
      return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
    }
  }

  return WM_gesture_box_invoke(C, op, event);
}

void SEQUENCER_OT_select_box(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers. */
  ot->name = "Box Select";
  ot->idname = "SEQUENCER_OT_select_box";
  ot->description = "Select strips using box selection";

  /* Api callbacks. */
  ot->invoke = sequencer_box_select_invoke;
  ot->exec = sequencer_box_select_exec;
  ot->modal = WM_gesture_box_modal;
  ot->cancel = WM_gesture_box_cancel;

  ot->poll = ED_operator_sequencer_active;

  /* Flags. */
  ot->flag = OPTYPE_UNDO;

  /* Properties. */
  WM_operator_properties_gesture_box(ot);
  WM_operator_properties_select_operation_simple(ot);

  prop = RNA_def_boolean(
      ot->srna, "tweak", false, "Tweak", "Operator has been activated using a click-drag event");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(
      ot->srna, "include_handles", false, "Select Handles", "Select the strips and their handles");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Grouped Operator
 * \{ */

enum {
  SEQ_SELECT_GROUP_TYPE,
  SEQ_SELECT_GROUP_TYPE_BASIC,
  SEQ_SELECT_GROUP_TYPE_EFFECT,
  SEQ_SELECT_GROUP_DATA,
  SEQ_SELECT_GROUP_EFFECT,
  SEQ_SELECT_GROUP_EFFECT_LINK,
  SEQ_SELECT_GROUP_OVERLAP,
};

static const EnumPropertyItem sequencer_prop_select_grouped_types[] = {
    {SEQ_SELECT_GROUP_TYPE, "TYPE", 0, "Type", "Shared strip type"},
    {SEQ_SELECT_GROUP_TYPE_BASIC,
     "TYPE_BASIC",
     0,
     "Global Type",
     "All strips of same basic type (graphical or sound)"},
    {SEQ_SELECT_GROUP_TYPE_EFFECT,
     "TYPE_EFFECT",
     0,
     "Effect Type",
     "Shared strip effect type (if active strip is not an effect one, select all non-effect "
     "strips)"},
    {SEQ_SELECT_GROUP_DATA, "DATA", 0, "Data", "Shared data (scene, image, sound, etc.)"},
    {SEQ_SELECT_GROUP_EFFECT, "EFFECT", 0, "Effect", "Shared effects"},
    {SEQ_SELECT_GROUP_EFFECT_LINK,
     "EFFECT_LINK",
     0,
     "Effect/Linked",
     "Other strips affected by the active one (sharing some time, and below or "
     "effect-assigned)"},
    {SEQ_SELECT_GROUP_OVERLAP, "OVERLAP", 0, "Overlap", "Overlapping time"},
    {0, nullptr, 0, nullptr, nullptr},
};

#define SEQ_IS_SOUND(_seq) ((_seq->type & SEQ_TYPE_SOUND_RAM) && !(_seq->type & SEQ_TYPE_EFFECT))

#define SEQ_IS_EFFECT(_seq) ((_seq->type & SEQ_TYPE_EFFECT) != 0)

#define SEQ_USE_DATA(_seq) \
  (ELEM(_seq->type, SEQ_TYPE_SCENE, SEQ_TYPE_MOVIECLIP, SEQ_TYPE_MASK) || SEQ_HAS_PATH(_seq))

#define SEQ_CHANNEL_CHECK(_seq, _chan) ELEM((_chan), 0, (_seq)->machine)

static bool select_grouped_type(blender::Span<Sequence *> strips,
                                ListBase * /*seqbase*/,
                                Sequence *actseq,
                                const int channel)
{
  bool changed = false;

  for (Sequence *seq : strips) {
    if (SEQ_CHANNEL_CHECK(seq, channel) && seq->type == actseq->type) {
      seq->flag |= SELECT;
      changed = true;
    }
  }

  return changed;
}

static bool select_grouped_type_basic(blender::Span<Sequence *> strips,
                                      ListBase * /*seqbase*/,
                                      Sequence *actseq,
                                      const int channel)
{
  bool changed = false;
  const bool is_sound = SEQ_IS_SOUND(actseq);

  for (Sequence *seq : strips) {
    if (SEQ_CHANNEL_CHECK(seq, channel) && (is_sound ? SEQ_IS_SOUND(seq) : !SEQ_IS_SOUND(seq))) {
      seq->flag |= SELECT;
      changed = true;
    }
  }

  return changed;
}

static bool select_grouped_type_effect(blender::Span<Sequence *> strips,
                                       ListBase * /*seqbase*/,
                                       Sequence *actseq,
                                       const int channel)
{
  bool changed = false;
  const bool is_effect = SEQ_IS_EFFECT(actseq);

  for (Sequence *seq : strips) {
    if (SEQ_CHANNEL_CHECK(seq, channel) && (is_effect ? SEQ_IS_EFFECT(seq) : !SEQ_IS_EFFECT(seq)))
    {
      seq->flag |= SELECT;
      changed = true;
    }
  }

  return changed;
}

static bool select_grouped_data(blender::Span<Sequence *> strips,
                                ListBase * /*seqbase*/,
                                Sequence *actseq,
                                const int channel)
{
  bool changed = false;
  const char *dirpath = actseq->strip ? actseq->strip->dirpath : nullptr;

  if (!SEQ_USE_DATA(actseq)) {
    return changed;
  }

  if (SEQ_HAS_PATH(actseq) && dirpath) {
    for (Sequence *seq : strips) {
      if (SEQ_CHANNEL_CHECK(seq, channel) && SEQ_HAS_PATH(seq) && seq->strip &&
          STREQ(seq->strip->dirpath, dirpath))
      {
        seq->flag |= SELECT;
        changed = true;
      }
    }
  }
  else if (actseq->type == SEQ_TYPE_SCENE) {
    Scene *sce = actseq->scene;
    for (Sequence *seq : strips) {
      if (SEQ_CHANNEL_CHECK(seq, channel) && seq->type == SEQ_TYPE_SCENE && seq->scene == sce) {
        seq->flag |= SELECT;
        changed = true;
      }
    }
  }
  else if (actseq->type == SEQ_TYPE_MOVIECLIP) {
    MovieClip *clip = actseq->clip;
    for (Sequence *seq : strips) {
      if (SEQ_CHANNEL_CHECK(seq, channel) && seq->type == SEQ_TYPE_MOVIECLIP && seq->clip == clip)
      {
        seq->flag |= SELECT;
        changed = true;
      }
    }
  }
  else if (actseq->type == SEQ_TYPE_MASK) {
    Mask *mask = actseq->mask;
    for (Sequence *seq : strips) {
      if (SEQ_CHANNEL_CHECK(seq, channel) && seq->type == SEQ_TYPE_MASK && seq->mask == mask) {
        seq->flag |= SELECT;
        changed = true;
      }
    }
  }

  return changed;
}

static bool select_grouped_effect(blender::Span<Sequence *> strips,
                                  ListBase * /*seqbase*/,
                                  Sequence *actseq,
                                  const int channel)
{
  bool changed = false;
  bool effects[SEQ_TYPE_MAX + 1];

  for (int i = 0; i <= SEQ_TYPE_MAX; i++) {
    effects[i] = false;
  }

  for (Sequence *seq : strips) {
    if (SEQ_CHANNEL_CHECK(seq, channel) && (seq->type & SEQ_TYPE_EFFECT) &&
        SEQ_relation_is_effect_of_strip(seq, actseq))
    {
      effects[seq->type] = true;
    }
  }

  for (Sequence *seq : strips) {
    if (SEQ_CHANNEL_CHECK(seq, channel) && effects[seq->type]) {
      if (seq->seq1) {
        seq->seq1->flag |= SELECT;
      }
      if (seq->seq2) {
        seq->seq2->flag |= SELECT;
      }
      if (seq->seq3) {
        seq->seq3->flag |= SELECT;
      }
      changed = true;
    }
  }

  return changed;
}

static bool select_grouped_time_overlap(const Scene *scene,
                                        blender::Span<Sequence *> strips,
                                        ListBase * /*seqbase*/,
                                        Sequence *actseq)
{
  bool changed = false;

  for (Sequence *seq : strips) {
    if (SEQ_time_left_handle_frame_get(scene, seq) <
            SEQ_time_right_handle_frame_get(scene, actseq) &&
        SEQ_time_right_handle_frame_get(scene, seq) >
            SEQ_time_left_handle_frame_get(scene, actseq))
    {
      seq->flag |= SELECT;
      changed = true;
    }
  }

  return changed;
}

/* Query strips that are in lower channel and intersect in time with seq_reference. */
static void query_lower_channel_strips(const Scene *scene,
                                       Sequence *seq_reference,
                                       ListBase *seqbase,
                                       blender::VectorSet<Sequence *> &strips)
{
  LISTBASE_FOREACH (Sequence *, seq_test, seqbase) {
    if (seq_test->machine > seq_reference->machine) {
      continue; /* Not lower channel. */
    }
    if (SEQ_time_right_handle_frame_get(scene, seq_test) <=
            SEQ_time_left_handle_frame_get(scene, seq_reference) ||
        SEQ_time_left_handle_frame_get(scene, seq_test) >=
            SEQ_time_right_handle_frame_get(scene, seq_reference))
    {
      continue; /* Not intersecting in time. */
    }
    strips.add(seq_test);
  }
}

/* Select all strips within time range and with lower channel of initial selection. Then select
 * effect chains of these strips. */
static bool select_grouped_effect_link(const Scene *scene,
                                       blender::VectorSet<Sequence *> strips,
                                       ListBase *seqbase,
                                       Sequence * /*actseq*/,
                                       const int /*channel*/)
{
  /* Get collection of strips. */
  strips.remove_if([&](Sequence *seq) { return (seq->flag & SELECT) == 0; });
  const int selected_strip_count = strips.size();
  /* XXX: this uses scene as arg, so it does not work with iterator :( I had thought about this,
   * but expand function is just so useful... I can just add scene and inject it I guess. */
  SEQ_iterator_set_expand(scene, seqbase, strips, query_lower_channel_strips);
  SEQ_iterator_set_expand(scene, seqbase, strips, SEQ_query_strip_effect_chain);

  /* Check if other strips will be affected. */
  const bool changed = strips.size() > selected_strip_count;

  /* Actual logic. */
  for (Sequence *seq : strips) {
    seq->flag |= SELECT;
  }

  return changed;
}

#undef SEQ_IS_SOUND
#undef SEQ_IS_EFFECT
#undef SEQ_USE_DATA

static int sequencer_select_grouped_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ListBase *seqbase = SEQ_active_seqbase_get(SEQ_editing_get(scene));
  Sequence *actseq = SEQ_select_active_get(scene);

  const bool is_preview = sequencer_view_has_preview_poll(C);
  if (is_preview && !sequencer_view_preview_only_poll(C)) {
    return OPERATOR_CANCELLED;
  }

  blender::VectorSet strips = all_strips_from_context(C);

  if (actseq == nullptr || (is_preview && !strips.contains(actseq))) {
    BKE_report(op->reports, RPT_ERROR, "No active sequence!");
    return OPERATOR_CANCELLED;
  }

  const int type = RNA_enum_get(op->ptr, "type");
  const int channel = RNA_boolean_get(op->ptr, "use_active_channel") ? actseq->machine : 0;
  const bool extend = RNA_boolean_get(op->ptr, "extend");

  bool changed = false;

  if (!extend) {
    LISTBASE_FOREACH (Sequence *, seq, seqbase) {
      seq->flag &= ~SELECT;
      changed = true;
    }
  }

  switch (type) {
    case SEQ_SELECT_GROUP_TYPE:
      changed |= select_grouped_type(strips, seqbase, actseq, channel);
      break;
    case SEQ_SELECT_GROUP_TYPE_BASIC:
      changed |= select_grouped_type_basic(strips, seqbase, actseq, channel);
      break;
    case SEQ_SELECT_GROUP_TYPE_EFFECT:
      changed |= select_grouped_type_effect(strips, seqbase, actseq, channel);
      break;
    case SEQ_SELECT_GROUP_DATA:
      changed |= select_grouped_data(strips, seqbase, actseq, channel);
      break;
    case SEQ_SELECT_GROUP_EFFECT:
      changed |= select_grouped_effect(strips, seqbase, actseq, channel);
      break;
    case SEQ_SELECT_GROUP_EFFECT_LINK:
      changed |= select_grouped_effect_link(scene, strips, seqbase, actseq, channel);
      break;
    case SEQ_SELECT_GROUP_OVERLAP:
      changed |= select_grouped_time_overlap(scene, strips, seqbase, actseq);
      break;
    default:
      BLI_assert(0);
      break;
  }

  if (changed) {
    ED_outliner_select_sync_from_sequence_tag(C);
    WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);
    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

void SEQUENCER_OT_select_grouped(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Select Grouped";
  ot->idname = "SEQUENCER_OT_select_grouped";
  ot->description = "Select all strips grouped by various properties";

  /* Api callbacks. */
  ot->invoke = WM_menu_invoke;
  ot->exec = sequencer_select_grouped_exec;
  ot->poll = sequencer_edit_poll;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Properties. */
  ot->prop = RNA_def_enum(ot->srna, "type", sequencer_prop_select_grouped_types, 0, "Type", "");
  RNA_def_boolean(ot->srna,
                  "extend",
                  false,
                  "Extend",
                  "Extend selection instead of deselecting everything first");
  RNA_def_boolean(ot->srna,
                  "use_active_channel",
                  false,
                  "Same Channel",
                  "Only consider strips on the same channel as the active one");
}

/** \} */
