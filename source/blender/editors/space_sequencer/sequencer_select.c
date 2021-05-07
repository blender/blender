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

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_scene_types.h"

#include "BKE_context.h"
#include "BKE_report.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_define.h"

#include "SEQ_iterator.h"
#include "SEQ_select.h"
#include "SEQ_sequencer.h"
#include "SEQ_transform.h"

/* For menu, popup, icons, etc. */

#include "ED_outliner.h"
#include "ED_screen.h"
#include "ED_select_utils.h"
#include "ED_sequencer.h"

#include "UI_view2d.h"

/* Own include. */
#include "sequencer_intern.h"

/* -------------------------------------------------------------------- */
/** \name Selection Utilities
 * \{ */

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
static void select_active_side(ListBase *seqbase, int sel_side, int channel, int frame)
{
  Sequence *seq;

  for (seq = seqbase->first; seq; seq = seq->next) {
    if (channel == seq->machine) {
      switch (sel_side) {
        case SEQ_SIDE_LEFT:
          if (frame > (seq->startdisp)) {
            seq->flag &= ~(SEQ_RIGHTSEL | SEQ_LEFTSEL);
            seq->flag |= SELECT;
          }
          break;
        case SEQ_SIDE_RIGHT:
          if (frame < (seq->startdisp)) {
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
static void select_active_side_range(ListBase *seqbase,
                                     const int sel_side,
                                     const int frame_ranges[MAXSEQ],
                                     const int frame_ignore)
{
  Sequence *seq;

  for (seq = seqbase->first; seq; seq = seq->next) {
    if (seq->machine < MAXSEQ) {
      const int frame = frame_ranges[seq->machine];
      if (frame == frame_ignore) {
        continue;
      }
      switch (sel_side) {
        case SEQ_SIDE_LEFT:
          if (frame > (seq->startdisp)) {
            seq->flag &= ~(SEQ_RIGHTSEL | SEQ_LEFTSEL);
            seq->flag |= SELECT;
          }
          break;
        case SEQ_SIDE_RIGHT:
          if (frame < (seq->startdisp)) {
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
static void select_linked_time(ListBase *seqbase, Sequence *seq_link)
{
  Sequence *seq;

  for (seq = seqbase->first; seq; seq = seq->next) {
    if (seq_link->machine != seq->machine) {
      int left_match = (seq->startdisp == seq_link->startdisp) ? 1 : 0;
      int right_match = (seq->enddisp == seq_link->enddisp) ? 1 : 0;

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

  if (seq == NULL) {
    return;
  }

  select_surrounding_handles(scene, seq);
}
#endif

void ED_sequencer_select_sequence_single(Scene *scene, Sequence *seq, bool deselect_all)
{
  Editing *ed = SEQ_editing_get(scene, false);

  if (deselect_all) {
    ED_sequencer_deselect_all(scene);
  }

  SEQ_select_active_set(scene, seq);

  if (ELEM(seq->type, SEQ_TYPE_IMAGE, SEQ_TYPE_MOVIE)) {
    if (seq->strip) {
      BLI_strncpy(ed->act_imagedir, seq->strip->dir, FILE_MAXDIR);
    }
  }
  else if (seq->type == SEQ_TYPE_SOUND_RAM) {
    if (seq->strip) {
      BLI_strncpy(ed->act_sounddir, seq->strip->dir, FILE_MAXDIR);
    }
  }
  seq->flag |= SELECT;
  recurs_sel_seq(seq);
}

void seq_rectf(Sequence *seq, rctf *rect)
{
  rect->xmin = seq->startdisp;
  rect->xmax = seq->enddisp;
  rect->ymin = seq->machine + SEQ_STRIP_OFSBOTTOM;
  rect->ymax = seq->machine + SEQ_STRIP_OFSTOP;
}

Sequence *find_neighboring_sequence(Scene *scene, Sequence *test, int lr, int sel)
{
  /* sel: 0==unselected, 1==selected, -1==don't care. */
  Sequence *seq;
  Editing *ed = SEQ_editing_get(scene, false);

  if (ed == NULL) {
    return NULL;
  }

  if (sel > 0) {
    sel = SELECT;
  }

  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    if ((seq != test) && (test->machine == seq->machine) &&
        ((sel == -1) || (sel && (seq->flag & SELECT)) ||
         (sel == 0 && (seq->flag & SELECT) == 0))) {
      switch (lr) {
        case SEQ_SIDE_LEFT:
          if (test->startdisp == (seq->enddisp)) {
            return seq;
          }
          break;
        case SEQ_SIDE_RIGHT:
          if (test->enddisp == (seq->startdisp)) {
            return seq;
          }
          break;
      }
    }
  }
  return NULL;
}

Sequence *find_nearest_seq(Scene *scene, View2D *v2d, int *hand, const int mval[2])
{
  Sequence *seq;
  Editing *ed = SEQ_editing_get(scene, false);
  float x, y;
  float pixelx;
  float handsize;
  float displen;
  *hand = SEQ_SIDE_NONE;

  if (ed == NULL) {
    return NULL;
  }

  pixelx = BLI_rctf_size_x(&v2d->cur) / BLI_rcti_size_x(&v2d->mask);

  UI_view2d_region_to_view(v2d, mval[0], mval[1], &x, &y);

  seq = ed->seqbasep->first;

  while (seq) {
    if (seq->machine == (int)y) {
      /* Check for both normal strips, and strips that have been flipped horizontally. */
      if (((seq->startdisp < seq->enddisp) && (seq->startdisp <= x && seq->enddisp >= x)) ||
          ((seq->startdisp > seq->enddisp) && (seq->startdisp >= x && seq->enddisp <= x))) {
        if (SEQ_transform_sequence_can_be_translated(seq)) {

          /* Clamp handles to defined size in pixel space. */
          handsize = 2.0f * sequence_handle_size_get_clamped(seq, pixelx);
          displen = (float)abs(seq->startdisp - seq->enddisp);

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

            if (handsize + seq->startdisp >= x) {
              *hand = SEQ_SIDE_LEFT;
            }
            else if (-handsize + seq->enddisp <= x) {
              *hand = SEQ_SIDE_RIGHT;
            }
          }
        }
        return seq;
      }
    }
    seq = seq->next;
  }
  return NULL;
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

  seq = seq_meta->seqbase.first;
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

    seq = seq->next;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name (De)select All Operator
 * \{ */

static int sequencer_de_select_all_exec(bContext *C, wmOperator *op)
{
  int action = RNA_enum_get(op->ptr, "action");

  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene, false);
  Sequence *seq;

  if (action == SEL_TOGGLE) {
    action = SEL_SELECT;
    for (seq = ed->seqbasep->first; seq; seq = seq->next) {
      if (seq->flag & SEQ_ALLSEL) {
        action = SEL_DESELECT;
        break;
      }
    }
  }

  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
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

void SEQUENCER_OT_select_all(struct wmOperatorType *ot)
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

static int sequencer_select_inverse_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene, false);
  Sequence *seq;

  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
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

void SEQUENCER_OT_select_inverse(struct wmOperatorType *ot)
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

static int sequencer_select_exec(bContext *C, wmOperator *op)
{
  View2D *v2d = UI_view2d_fromcontext(C);
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene, false);
  const bool extend = RNA_boolean_get(op->ptr, "extend");
  const bool deselect_all = RNA_boolean_get(op->ptr, "deselect_all");
  const bool linked_handle = RNA_boolean_get(op->ptr, "linked_handle");
  const bool linked_time = RNA_boolean_get(op->ptr, "linked_time");
  bool side_of_frame = RNA_boolean_get(op->ptr, "side_of_frame");
  bool wait_to_deselect_others = RNA_boolean_get(op->ptr, "wait_to_deselect_others");
  int mval[2];
  int ret_value = OPERATOR_CANCELLED;

  mval[0] = RNA_int_get(op->ptr, "mouse_x");
  mval[1] = RNA_int_get(op->ptr, "mouse_y");

  Sequence *seq, *neighbor, *act_orig;
  int hand, sel_side;

  if (ed == NULL) {
    return OPERATOR_CANCELLED;
  }

  if (extend) {
    wait_to_deselect_others = false;
  }

  seq = find_nearest_seq(scene, v2d, &hand, mval);

  /* XXX - not nice, Ctrl+RMB needs to do side_of_frame only when not over a strip */
  if (seq && linked_time) {
    side_of_frame = false;
  }

  /* Select left, right or overlapping the current frame. */
  if (side_of_frame) {
    /* Use different logic for this. */
    if (extend == false) {
      ED_sequencer_deselect_all(scene);
    }

    const float x = UI_view2d_region_to_view_x(v2d, mval[0]);

    LISTBASE_FOREACH (Sequence *, seq_iter, SEQ_active_seqbase_get(ed)) {
      if (((x < CFRA) && (seq_iter->enddisp <= CFRA)) ||
          ((x >= CFRA) && (seq_iter->startdisp >= CFRA))) {
        /* Select left or right. */
        seq_iter->flag |= SELECT;
        recurs_sel_seq(seq_iter);
      }
    }

    {
      SpaceSeq *sseq = CTX_wm_space_seq(C);
      if (sseq && sseq->flag & SEQ_MARKER_TRANS) {
        TimeMarker *tmarker;

        for (tmarker = scene->markers.first; tmarker; tmarker = tmarker->next) {
          if (((x < CFRA) && (tmarker->frame <= CFRA)) ||
              ((x >= CFRA) && (tmarker->frame >= CFRA))) {
            tmarker->flag |= SELECT;
          }
          else {
            tmarker->flag &= ~SELECT;
          }
        }
      }
    }

    ret_value = OPERATOR_FINISHED;
  }
  else {
    act_orig = ed->act_seq;

    if (seq) {
      /* Are we trying to select a handle that's already selected? */
      const bool handle_selected = ((hand == SEQ_SIDE_LEFT) && (seq->flag & SEQ_LEFTSEL)) ||
                                   ((hand == SEQ_SIDE_RIGHT) && (seq->flag & SEQ_RIGHTSEL));

      if (wait_to_deselect_others && (seq->flag & SELECT) &&
          (hand == SEQ_SIDE_NONE || handle_selected)) {
        ret_value = OPERATOR_RUNNING_MODAL;
      }
      else if (!extend && !linked_handle) {
        ED_sequencer_deselect_all(scene);
        ret_value = OPERATOR_FINISHED;
      }
      else {
        ret_value = OPERATOR_FINISHED;
      }

      SEQ_select_active_set(scene, seq);

      if (ELEM(seq->type, SEQ_TYPE_IMAGE, SEQ_TYPE_MOVIE)) {
        if (seq->strip) {
          BLI_strncpy(ed->act_imagedir, seq->strip->dir, FILE_MAXDIR);
        }
      }
      else if (seq->type == SEQ_TYPE_SOUND_RAM) {
        if (seq->strip) {
          BLI_strncpy(ed->act_sounddir, seq->strip->dir, FILE_MAXDIR);
        }
      }

      /* On Alt selection, select the strip and bordering handles. */
      if (linked_handle) {
        if (!ELEM(hand, SEQ_SIDE_LEFT, SEQ_SIDE_RIGHT)) {
          /* First click selects the strip and its adjacent handles (if valid).
           * Second click selects the strip,
           * both of its handles and its adjacent handles (if valid). */
          const bool is_striponly_selected = ((seq->flag & SEQ_ALLSEL) == SELECT);

          if (!extend) {
            ED_sequencer_deselect_all(scene);
          }
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
          sel_side = hand;
          neighbor = find_neighboring_sequence(scene, seq, sel_side, -1);
          if (neighbor) {
            switch (sel_side) {
              case SEQ_SIDE_LEFT:
                if ((seq->flag & SEQ_LEFTSEL) && (neighbor->flag & SEQ_RIGHTSEL)) {
                  if (extend == 0) {
                    ED_sequencer_deselect_all(scene);
                  }
                  seq->flag |= SELECT;

                  select_active_side(ed->seqbasep, SEQ_SIDE_LEFT, seq->machine, seq->startdisp);
                }
                else {
                  if (extend == 0) {
                    ED_sequencer_deselect_all(scene);
                  }
                  seq->flag |= SELECT;

                  neighbor->flag |= SELECT;
                  recurs_sel_seq(neighbor);
                  neighbor->flag |= SEQ_RIGHTSEL;
                  seq->flag |= SEQ_LEFTSEL;
                }
                break;
              case SEQ_SIDE_RIGHT:
                if ((seq->flag & SEQ_RIGHTSEL) && (neighbor->flag & SEQ_LEFTSEL)) {
                  if (extend == 0) {
                    ED_sequencer_deselect_all(scene);
                  }
                  seq->flag |= SELECT;

                  select_active_side(ed->seqbasep, SEQ_SIDE_RIGHT, seq->machine, seq->startdisp);
                }
                else {
                  if (extend == 0) {
                    ED_sequencer_deselect_all(scene);
                  }
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
            if (extend == 0) {
              ED_sequencer_deselect_all(scene);
            }
            select_active_side(ed->seqbasep, sel_side, seq->machine, seq->startdisp);
          }
        }

        ret_value = OPERATOR_FINISHED;
      }
      else {
        if (extend && (seq->flag & SELECT) && ed->act_seq == act_orig) {
          switch (hand) {
            case SEQ_SIDE_NONE:
              if (linked_handle == 0) {
                seq->flag &= ~SEQ_ALLSEL;
              }
              break;
            case SEQ_SIDE_LEFT:
              seq->flag ^= SEQ_LEFTSEL;
              break;
            case SEQ_SIDE_RIGHT:
              seq->flag ^= SEQ_RIGHTSEL;
              break;
          }
          ret_value = OPERATOR_FINISHED;
        }
        else {
          seq->flag |= SELECT;
          if (hand == SEQ_SIDE_LEFT) {
            seq->flag |= SEQ_LEFTSEL;
          }
          if (hand == SEQ_SIDE_RIGHT) {
            seq->flag |= SEQ_RIGHTSEL;
          }
        }
      }

      recurs_sel_seq(seq);

      if (linked_time) {
        select_linked_time(ed->seqbasep, seq);
      }

      BLI_assert((ret_value & OPERATOR_CANCELLED) == 0);
    }
    else if (deselect_all) {
      ED_sequencer_deselect_all(scene);
      ret_value = OPERATOR_FINISHED;
    }
  }

  ED_outliner_select_sync_from_sequence_tag(C);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);

  return ret_value;
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
  ot->invoke = WM_generic_select_invoke;
  ot->modal = WM_generic_select_modal;
  ot->poll = ED_operator_sequencer_active;

  /* Flags. */
  ot->flag = OPTYPE_UNDO;

  /* Properties. */
  WM_operator_properties_generic_select(ot);

  prop = RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend the selection");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna,
                         "deselect_all",
                         false,
                         "Deselect On Nothing",
                         "Deselect all when nothing under the cursor");
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
  Editing *ed = SEQ_editing_get(scene, false);

  if (ed == NULL) {
    return false;
  }

  bool changed = false;

  LISTBASE_FOREACH (Sequence *, seq, SEQ_active_seqbase_get(ed)) {
    if ((seq->flag & SELECT) != 0) {
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
  Editing *ed = SEQ_editing_get(scene, false);

  if (ed == NULL) {
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
    Sequence *neighbor = BLI_gsetIterator_getKey(&gsi);
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

  BLI_gset_free(neighbors, NULL);
  return changed;
}

static int sequencer_select_more_exec(bContext *C, wmOperator *UNUSED(op))
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

static int sequencer_select_less_exec(bContext *C, wmOperator *UNUSED(op))
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
  mouse_seq = find_nearest_seq(scene, v2d, &hand, event->mval);
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
  RNA_def_boolean(ot->srna, "extend", 0, "Extend", "Extend the selection");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Linked Operator
 * \{ */

static int sequencer_select_linked_exec(bContext *C, wmOperator *UNUSED(op))
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
    {0, NULL, 0, NULL, NULL},
};

static int sequencer_select_handles_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene, false);
  Sequence *seq;
  int sel_side = RNA_enum_get(op->ptr, "side");

  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
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
  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
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
  Editing *ed = SEQ_editing_get(scene, false);
  const bool extend = RNA_boolean_get(op->ptr, "extend");
  const int side = RNA_enum_get(op->ptr, "side");

  if (ed == NULL) {
    return OPERATOR_CANCELLED;
  }
  if (extend == false) {
    ED_sequencer_deselect_all(scene);
  }
  const int timeline_frame = CFRA;
  LISTBASE_FOREACH (Sequence *, seq, SEQ_active_seqbase_get(ed)) {
    bool test = false;
    switch (side) {
      case -1:
        test = (timeline_frame >= seq->enddisp);
        break;
      case 1:
        test = (timeline_frame <= seq->startdisp);
        break;
      case 0:
        test = (timeline_frame <= seq->enddisp) && (timeline_frame >= seq->startdisp);
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
      {0, NULL, 0, NULL, NULL},
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
  RNA_def_boolean(ot->srna, "extend", 0, "Extend", "Extend the selection");
  ot->prop = RNA_def_enum(ot->srna, "side", sequencer_select_left_right_types, 0, "Side", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Side Operator
 * \{ */

static int sequencer_select_side_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene, false);

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
        *frame_limit_p = max_ii(*frame_limit_p, seq->startdisp);
      }
      else {
        *frame_limit_p = min_ii(*frame_limit_p, seq->startdisp);
      }
    }
  }

  if (selected == false) {
    return OPERATOR_CANCELLED;
  }

  select_active_side_range(ed->seqbasep, sel_side, frame_ranges, frame_init);

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

static int sequencer_box_select_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  View2D *v2d = UI_view2d_fromcontext(C);
  Editing *ed = SEQ_editing_get(scene, false);

  if (ed == NULL) {
    return OPERATOR_CANCELLED;
  }

  const eSelectOp sel_op = RNA_enum_get(op->ptr, "mode");
  const bool handles = RNA_boolean_get(op->ptr, "include_handles");
  const bool select = (sel_op != SEL_OP_SUB);

  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    ED_sequencer_deselect_all(scene);
  }

  rctf rectf;
  WM_operator_properties_border_to_rctf(op, &rectf);
  UI_view2d_region_to_view_rctf(v2d, &rectf, &rectf);

  LISTBASE_FOREACH (Sequence *, seq, ed->seqbasep) {
    rctf rq;
    seq_rectf(seq, &rq);
    if (BLI_rctf_isect(&rq, &rectf, NULL)) {
      if (handles) {
        /* Get the handles draw size. */
        float pixelx = BLI_rctf_size_x(&v2d->cur) / BLI_rcti_size_x(&v2d->mask);
        float handsize = sequence_handle_size_get_clamped(seq, pixelx);

        /* Right handle. */
        if (rectf.xmax > (seq->enddisp - handsize)) {
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
        }
        /* Left handle. */
        if (rectf.xmin < (seq->startdisp + handsize)) {
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
      }

      /* Regular box selection. */
      else {
        SET_FLAG_FROM_TEST(seq->flag, select, SELECT);
        seq->flag &= ~(SEQ_LEFTSEL | SEQ_RIGHTSEL);
      }
    }
  }

  ED_outliner_select_sync_from_sequence_tag(C);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER | NA_SELECTED, scene);

  return OPERATOR_FINISHED;
}

static int sequencer_box_select_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Scene *scene = CTX_data_scene(C);
  View2D *v2d = &CTX_wm_region(C)->v2d;

  const bool tweak = RNA_boolean_get(op->ptr, "tweak");

  if (tweak) {
    int hand_dummy;
    Sequence *seq = find_nearest_seq(scene, v2d, &hand_dummy, event->mval);
    if (seq != NULL) {
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
      ot->srna, "tweak", 0, "Tweak", "Operator has been activated using a tweak event");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(
      ot->srna, "include_handles", 0, "Select Handles", "Select the strips and their handles");
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
     "Other strips affected by the active one (sharing some time, and below or effect-assigned)"},
    {SEQ_SELECT_GROUP_OVERLAP, "OVERLAP", 0, "Overlap", "Overlapping time"},
    {0, NULL, 0, NULL, NULL},
};

#define SEQ_IS_SOUND(_seq) ((_seq->type & SEQ_TYPE_SOUND_RAM) && !(_seq->type & SEQ_TYPE_EFFECT))

#define SEQ_IS_EFFECT(_seq) ((_seq->type & SEQ_TYPE_EFFECT) != 0)

#define SEQ_USE_DATA(_seq) \
  (ELEM(_seq->type, SEQ_TYPE_SCENE, SEQ_TYPE_MOVIECLIP, SEQ_TYPE_MASK) || SEQ_HAS_PATH(_seq))

#define SEQ_CHANNEL_CHECK(_seq, _chan) (ELEM((_chan), 0, (_seq)->machine))

static bool select_grouped_type(Editing *ed, Sequence *actseq, const int channel)
{
  bool changed = false;

  LISTBASE_FOREACH (Sequence *, seq, SEQ_active_seqbase_get(ed)) {
    if (SEQ_CHANNEL_CHECK(seq, channel) && seq->type == actseq->type) {
      seq->flag |= SELECT;
      changed = true;
    }
  }

  return changed;
}

static bool select_grouped_type_basic(Editing *ed, Sequence *actseq, const int channel)
{
  bool changed = false;
  const bool is_sound = SEQ_IS_SOUND(actseq);

  LISTBASE_FOREACH (Sequence *, seq, SEQ_active_seqbase_get(ed)) {
    if (SEQ_CHANNEL_CHECK(seq, channel) && (is_sound ? SEQ_IS_SOUND(seq) : !SEQ_IS_SOUND(seq))) {
      seq->flag |= SELECT;
      changed = true;
    }
  }

  return changed;
}

static bool select_grouped_type_effect(Editing *ed, Sequence *actseq, const int channel)
{
  bool changed = false;
  const bool is_effect = SEQ_IS_EFFECT(actseq);

  LISTBASE_FOREACH (Sequence *, seq, SEQ_active_seqbase_get(ed)) {
    if (SEQ_CHANNEL_CHECK(seq, channel) &&
        (is_effect ? SEQ_IS_EFFECT(seq) : !SEQ_IS_EFFECT(seq))) {
      seq->flag |= SELECT;
      changed = true;
    }
  }

  return changed;
}

static bool select_grouped_data(Editing *ed, Sequence *actseq, const int channel)
{
  bool changed = false;
  const char *dir = actseq->strip ? actseq->strip->dir : NULL;

  if (!SEQ_USE_DATA(actseq)) {
    return changed;
  }

  if (SEQ_HAS_PATH(actseq) && dir) {
    LISTBASE_FOREACH (Sequence *, seq, SEQ_active_seqbase_get(ed)) {
      if (SEQ_CHANNEL_CHECK(seq, channel) && SEQ_HAS_PATH(seq) && seq->strip &&
          STREQ(seq->strip->dir, dir)) {
        seq->flag |= SELECT;
        changed = true;
      }
    }
  }
  else if (actseq->type == SEQ_TYPE_SCENE) {
    Scene *sce = actseq->scene;
    LISTBASE_FOREACH (Sequence *, seq, SEQ_active_seqbase_get(ed)) {
      if (SEQ_CHANNEL_CHECK(seq, channel) && seq->type == SEQ_TYPE_SCENE && seq->scene == sce) {
        seq->flag |= SELECT;
        changed = true;
      }
    }
  }
  else if (actseq->type == SEQ_TYPE_MOVIECLIP) {
    MovieClip *clip = actseq->clip;
    LISTBASE_FOREACH (Sequence *, seq, SEQ_active_seqbase_get(ed)) {
      if (SEQ_CHANNEL_CHECK(seq, channel) && seq->type == SEQ_TYPE_MOVIECLIP &&
          seq->clip == clip) {
        seq->flag |= SELECT;
        changed = true;
      }
    }
  }
  else if (actseq->type == SEQ_TYPE_MASK) {
    struct Mask *mask = actseq->mask;
    LISTBASE_FOREACH (Sequence *, seq, SEQ_active_seqbase_get(ed)) {
      if (SEQ_CHANNEL_CHECK(seq, channel) && seq->type == SEQ_TYPE_MASK && seq->mask == mask) {
        seq->flag |= SELECT;
        changed = true;
      }
    }
  }

  return changed;
}

static bool select_grouped_effect(Editing *ed, Sequence *actseq, const int channel)
{
  bool changed = false;
  bool effects[SEQ_TYPE_MAX + 1];

  for (int i = 0; i <= SEQ_TYPE_MAX; i++) {
    effects[i] = false;
  }

  LISTBASE_FOREACH (Sequence *, seq, SEQ_active_seqbase_get(ed)) {
    if (SEQ_CHANNEL_CHECK(seq, channel) && (seq->type & SEQ_TYPE_EFFECT) &&
        ELEM(actseq, seq->seq1, seq->seq2, seq->seq3)) {
      effects[seq->type] = true;
    }
  }

  LISTBASE_FOREACH (Sequence *, seq, SEQ_active_seqbase_get(ed)) {
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

static bool select_grouped_time_overlap(Editing *ed, Sequence *actseq)
{
  bool changed = false;

  LISTBASE_FOREACH (Sequence *, seq, SEQ_active_seqbase_get(ed)) {
    if (seq->startdisp < actseq->enddisp && seq->enddisp > actseq->startdisp) {
      seq->flag |= SELECT;
      changed = true;
    }
  }

  return changed;
}

/* Query all effect strips that are directly or indirectly connected to seq_reference. */
static void query_strip_effect_chain(Sequence *seq_reference,
                                     ListBase *seqbase,
                                     SeqCollection *collection)
{
  if (!SEQ_collection_append_strip(seq_reference, collection)) {
    return; /* Strip is already in set, so all effects connected to it are as well. */
  }

  /* Find all strips that seq_reference is connected to. */
  if (seq_reference->type & SEQ_TYPE_EFFECT) {
    if (seq_reference->seq1) {
      query_strip_effect_chain(seq_reference->seq1, seqbase, collection);
    }
    if (seq_reference->seq2) {
      query_strip_effect_chain(seq_reference->seq2, seqbase, collection);
    }
    if (seq_reference->seq3) {
      query_strip_effect_chain(seq_reference->seq3, seqbase, collection);
    }
  }

  /* Find all strips connected to seq_reference. */
  LISTBASE_FOREACH (Sequence *, seq_test, seqbase) {
    if (seq_test->seq1 == seq_reference || seq_test->seq2 == seq_reference ||
        seq_test->seq3 == seq_reference) {
      query_strip_effect_chain(seq_test, seqbase, collection);
    }
  }
}

/* Query strips that are in lower channel and intersect in time with seq_reference. */
static void query_lower_channel_strips(Sequence *seq_reference,
                                       ListBase *seqbase,
                                       SeqCollection *collection)
{
  LISTBASE_FOREACH (Sequence *, seq_test, seqbase) {
    if (seq_test->machine > seq_reference->machine) {
      continue; /* Not lower channel. */
    }
    if (seq_test->enddisp <= seq_reference->startdisp ||
        seq_test->startdisp >= seq_reference->enddisp) {
      continue; /* Not intersecting in time. */
    }
    SEQ_collection_append_strip(seq_test, collection);
  }
}

/* Select all strips within time range and with lower channel of initial selection. Then select
 * effect chains of these strips. */
static bool select_grouped_effect_link(Editing *ed,
                                       Sequence *UNUSED(actseq),
                                       const int UNUSED(channel))
{
  ListBase *seqbase = SEQ_active_seqbase_get(ed);

  /* Get collection of strips. */
  SeqCollection *collection = SEQ_query_selected_strips(seqbase);
  const int selected_strip_count = BLI_gset_len(collection->set);
  SEQ_collection_expand(seqbase, collection, query_lower_channel_strips);
  SEQ_collection_expand(seqbase, collection, query_strip_effect_chain);

  /* Check if other strips will be affected. */
  const bool changed = BLI_gset_len(collection->set) > selected_strip_count;

  /* Actual logic. */
  Sequence *seq;
  SEQ_ITERATOR_FOREACH (seq, collection) {
    seq->flag |= SELECT;
  }

  SEQ_collection_free(collection);

  return changed;
}

#undef SEQ_IS_SOUND
#undef SEQ_IS_EFFECT
#undef SEQ_USE_DATA

static int sequencer_select_grouped_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene, false);
  Sequence *actseq = SEQ_select_active_get(scene);

  if (actseq == NULL) {
    BKE_report(op->reports, RPT_ERROR, "No active sequence!");
    return OPERATOR_CANCELLED;
  }

  const int type = RNA_enum_get(op->ptr, "type");
  const int channel = RNA_boolean_get(op->ptr, "use_active_channel") ? actseq->machine : 0;
  const bool extend = RNA_boolean_get(op->ptr, "extend");

  bool changed = false;

  if (!extend) {
    LISTBASE_FOREACH (Sequence *, seq, SEQ_active_seqbase_get(ed)) {
      seq->flag &= ~SELECT;
      changed = true;
    }
  }

  switch (type) {
    case SEQ_SELECT_GROUP_TYPE:
      changed |= select_grouped_type(ed, actseq, channel);
      break;
    case SEQ_SELECT_GROUP_TYPE_BASIC:
      changed |= select_grouped_type_basic(ed, actseq, channel);
      break;
    case SEQ_SELECT_GROUP_TYPE_EFFECT:
      changed |= select_grouped_type_effect(ed, actseq, channel);
      break;
    case SEQ_SELECT_GROUP_DATA:
      changed |= select_grouped_data(ed, actseq, channel);
      break;
    case SEQ_SELECT_GROUP_EFFECT:
      changed |= select_grouped_effect(ed, actseq, channel);
      break;
    case SEQ_SELECT_GROUP_EFFECT_LINK:
      changed |= select_grouped_effect_link(ed, actseq, channel);
      break;
    case SEQ_SELECT_GROUP_OVERLAP:
      changed |= select_grouped_time_overlap(ed, actseq);
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
