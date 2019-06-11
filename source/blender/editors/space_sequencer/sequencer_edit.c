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

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_math.h"
#include "BLI_timecode.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_scene_types.h"
#include "DNA_sound_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_sequencer.h"
#include "BKE_sound.h"
#include "BKE_library.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

/* for menu/popup icons etc etc*/

#include "ED_anim_api.h"
#include "ED_numinput.h"
#include "ED_screen.h"
#include "ED_transform.h"
#include "ED_sequencer.h"
#include "ED_space_api.h"

#include "UI_view2d.h"
#include "UI_interface.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

/* own include */
#include "sequencer_intern.h"

/* XXX */
/* RNA Enums, used in multiple files */
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

/* mute operator */

#define SEQ_SIDE_MOUSE -1

EnumPropertyItem prop_side_types[] = {
    {SEQ_SIDE_MOUSE, "MOUSE", 0, "Mouse position", ""},
    {SEQ_SIDE_LEFT, "LEFT", 0, "Left", ""},
    {SEQ_SIDE_RIGHT, "RIGHT", 0, "Right", ""},
    {SEQ_SIDE_BOTH, "BOTH", 0, "Both", ""},
    {0, NULL, 0, NULL, NULL},
};

static const EnumPropertyItem prop_side_lr_types[] = {
    {SEQ_SIDE_LEFT, "LEFT", 0, "Left", ""},
    {SEQ_SIDE_RIGHT, "RIGHT", 0, "Right", ""},
    {0, NULL, 0, NULL, NULL},
};

typedef struct TransSeq {
  int start, machine;
  int startstill, endstill;
  int startdisp, enddisp;
  int startofs, endofs;
  int anim_startofs, anim_endofs;
  /* int final_left, final_right; */ /* UNUSED */
  int len;
} TransSeq;

/* ********************************************************************** */

/* ***************** proxy job manager ********************** */

typedef struct ProxyBuildJob {
  struct Main *main;
  struct Depsgraph *depsgraph;
  Scene *scene;
  ListBase queue;
  int stop;
} ProxyJob;

static void proxy_freejob(void *pjv)
{
  ProxyJob *pj = pjv;

  BLI_freelistN(&pj->queue);

  MEM_freeN(pj);
}

/* only this runs inside thread */
static void proxy_startjob(void *pjv, short *stop, short *do_update, float *progress)
{
  ProxyJob *pj = pjv;
  LinkData *link;

  for (link = pj->queue.first; link; link = link->next) {
    struct SeqIndexBuildContext *context = link->data;

    BKE_sequencer_proxy_rebuild(context, stop, do_update, progress);

    if (*stop) {
      pj->stop = 1;
      fprintf(stderr, "Canceling proxy rebuild on users request...\n");
      break;
    }
  }
}

static void proxy_endjob(void *pjv)
{
  ProxyJob *pj = pjv;
  Editing *ed = BKE_sequencer_editing_get(pj->scene, false);
  LinkData *link;

  for (link = pj->queue.first; link; link = link->next) {
    BKE_sequencer_proxy_rebuild_finish(link->data, pj->stop);
  }

  BKE_sequencer_free_imbuf(pj->scene, &ed->seqbase, false);

  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, pj->scene);
}

static void seq_proxy_build_job(const bContext *C)
{
  wmJob *wm_job;
  ProxyJob *pj;
  struct Depsgraph *depsgraph = CTX_data_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  ScrArea *sa = CTX_wm_area(C);
  Sequence *seq;
  GSet *file_list;

  if (ed == NULL) {
    return;
  }

  wm_job = WM_jobs_get(CTX_wm_manager(C),
                       CTX_wm_window(C),
                       sa,
                       "Building Proxies",
                       WM_JOB_PROGRESS,
                       WM_JOB_TYPE_SEQ_BUILD_PROXY);

  pj = WM_jobs_customdata_get(wm_job);

  if (!pj) {
    pj = MEM_callocN(sizeof(ProxyJob), "proxy rebuild job");

    pj->depsgraph = depsgraph;
    pj->scene = scene;
    pj->main = CTX_data_main(C);

    WM_jobs_customdata_set(wm_job, pj, proxy_freejob);
    WM_jobs_timer(wm_job, 0.1, NC_SCENE | ND_SEQUENCER, NC_SCENE | ND_SEQUENCER);
    WM_jobs_callbacks(wm_job, proxy_startjob, NULL, NULL, proxy_endjob);
  }

  file_list = BLI_gset_new(BLI_ghashutil_strhash_p, BLI_ghashutil_strcmp, "file list");
  SEQP_BEGIN (ed, seq) {
    if ((seq->flag & SELECT)) {
      BKE_sequencer_proxy_rebuild_context(
          pj->main, pj->depsgraph, pj->scene, seq, file_list, &pj->queue);
    }
  }
  SEQ_END;

  BLI_gset_free(file_list, MEM_freeN);

  if (!WM_jobs_is_running(wm_job)) {
    G.is_break = false;
    WM_jobs_start(CTX_wm_manager(C), wm_job);
  }

  ED_area_tag_redraw(sa);
}

/* ********************************************************************** */

void seq_rectf(Sequence *seq, rctf *rect)
{
  rect->xmin = seq->startdisp;
  rect->xmax = seq->enddisp;
  rect->ymin = seq->machine + SEQ_STRIP_OFSBOTTOM;
  rect->ymax = seq->machine + SEQ_STRIP_OFSTOP;
}

void boundbox_seq(Scene *scene, rctf *rect)
{
  Sequence *seq;
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  float min[2], max[2];

  if (ed == NULL) {
    return;
  }

  min[0] = 0.0;
  max[0] = EFRA + 1;
  min[1] = 0.0;
  max[1] = 8.0;

  seq = ed->seqbasep->first;
  while (seq) {

    if (min[0] > seq->startdisp - 1) {
      min[0] = seq->startdisp - 1;
    }
    if (max[0] < seq->enddisp + 1) {
      max[0] = seq->enddisp + 1;
    }
    if (max[1] < seq->machine + 2) {
      max[1] = seq->machine + 2;
    }

    seq = seq->next;
  }

  rect->xmin = min[0];
  rect->xmax = max[0];
  rect->ymin = min[1];
  rect->ymax = max[1];
}

static int mouse_frame_side(View2D *v2d, short mouse_x, int frame)
{
  int mval[2];
  float mouseloc[2];

  mval[0] = mouse_x;
  mval[1] = 0;

  /* choose the side based on which side of the playhead the mouse is on */
  UI_view2d_region_to_view(v2d, mval[0], mval[1], &mouseloc[0], &mouseloc[1]);

  return mouseloc[0] > frame ? SEQ_SIDE_RIGHT : SEQ_SIDE_LEFT;
}

Sequence *find_neighboring_sequence(Scene *scene, Sequence *test, int lr, int sel)
{
  /* sel - 0==unselected, 1==selected, -1==done care*/
  Sequence *seq;
  Editing *ed = BKE_sequencer_editing_get(scene, false);

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

static Sequence *find_next_prev_sequence(Scene *scene, Sequence *test, int lr, int sel)
{
  /* sel - 0==unselected, 1==selected, -1==done care*/
  Sequence *seq, *best_seq = NULL;
  Editing *ed = BKE_sequencer_editing_get(scene, false);

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
      else if (dist < best_dist) {
        best_dist = dist;
        best_seq = seq;
      }
    }
    seq = seq->next;
  }
  return best_seq; /* can be null */
}

Sequence *find_nearest_seq(Scene *scene, View2D *v2d, int *hand, const int mval[2])
{
  Sequence *seq;
  Editing *ed = BKE_sequencer_editing_get(scene, false);
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
      /* check for both normal strips, and strips that have been flipped horizontally */
      if (((seq->startdisp < seq->enddisp) && (seq->startdisp <= x && seq->enddisp >= x)) ||
          ((seq->startdisp > seq->enddisp) && (seq->startdisp >= x && seq->enddisp <= x))) {
        if (BKE_sequence_tx_test(seq)) {

          /* clamp handles to defined size in pixel space */

          handsize = seq->handsize;
          displen = (float)abs(seq->startdisp - seq->enddisp);

          /* don't even try to grab the handles of small strips */
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

static bool seq_is_parent(Sequence *par, Sequence *seq)
{
  return ((par->seq1 == seq) || (par->seq2 == seq) || (par->seq3 == seq));
}

static bool seq_is_predecessor(Sequence *pred, Sequence *seq)
{
  if (!pred) {
    return 0;
  }
  if (pred == seq) {
    return 0;
  }
  else if (seq_is_parent(pred, seq)) {
    return 1;
  }
  else if (pred->seq1 && seq_is_predecessor(pred->seq1, seq)) {
    return 1;
  }
  else if (pred->seq2 && seq_is_predecessor(pred->seq2, seq)) {
    return 1;
  }
  else if (pred->seq3 && seq_is_predecessor(pred->seq3, seq)) {
    return 1;
  }

  return 0;
}

void ED_sequencer_deselect_all(Scene *scene)
{
  Sequence *seq;
  Editing *ed = BKE_sequencer_editing_get(scene, false);

  if (ed == NULL) {
    return;
  }

  SEQP_BEGIN (ed, seq) {
    seq->flag &= ~SEQ_ALLSEL;
  }
  SEQ_END;
}

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

bool ED_space_sequencer_maskedit_mask_poll(bContext *C)
{
  /* in this case both funcs are the same, for clip editor not */
  return ED_space_sequencer_maskedit_poll(C);
}

bool ED_space_sequencer_check_show_maskedit(SpaceSeq *sseq, Scene *scene)
{
  if (sseq && sseq->mainb == SEQ_DRAW_IMG_IMBUF) {
    return (BKE_sequencer_mask_get(scene) != NULL);
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

/* are we displaying the seq output (not channels or histogram)*/
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

int seq_effect_find_selected(Scene *scene,
                             Sequence *activeseq,
                             int type,
                             Sequence **selseq1,
                             Sequence **selseq2,
                             Sequence **selseq3,
                             const char **error_str)
{
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  Sequence *seq1 = NULL, *seq2 = NULL, *seq3 = NULL, *seq;

  *error_str = NULL;

  if (!activeseq) {
    seq2 = BKE_sequencer_active_get(scene);
  }

  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    if (seq->flag & SELECT) {
      if (seq->type == SEQ_TYPE_SOUND_RAM && BKE_sequence_effect_get_num_inputs(type) != 0) {
        *error_str = N_("Cannot apply effects to audio sequence strips");
        return 0;
      }
      if ((seq != activeseq) && (seq != seq2)) {
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
          *error_str = N_("Cannot apply effect to more than 3 sequence strips");
          return 0;
        }
      }
    }
  }

  /* make sequence selection a little bit more intuitive
   * for 3 strips: the last-strip should be sequence3 */
  if (seq3 != NULL && seq2 != NULL) {
    Sequence *tmp = seq2;
    seq2 = seq3;
    seq3 = tmp;
  }

  switch (BKE_sequence_effect_get_num_inputs(type)) {
    case 0:
      *selseq1 = *selseq2 = *selseq3 = NULL;
      return 1; /* success */
    case 1:
      if (seq2 == NULL) {
        *error_str = N_("At least one selected sequence strip is needed");
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
        *error_str = N_("2 selected sequence strips are needed");
        return 0;
      }
      if (seq3 == NULL) {
        seq3 = seq2;
      }
      break;
  }

  if (seq1 == NULL && seq2 == NULL && seq3 == NULL) {
    *error_str = N_("TODO: in what cases does this happen?");
    return 0;
  }

  *selseq1 = seq1;
  *selseq2 = seq2;
  *selseq3 = seq3;

  return 1;
}

static Sequence *del_seq_find_replace_recurs(Scene *scene, Sequence *seq)
{
  Sequence *seq1, *seq2, *seq3;

  /* try to find a replacement input sequence, and flag for later deletion if
   * no replacement can be found */

  if (!seq) {
    return NULL;
  }
  else if (!(seq->type & SEQ_TYPE_EFFECT)) {
    return ((seq->flag & SELECT) ? NULL : seq);
  }
  else if (!(seq->flag & SELECT)) {
    /* try to find replacement for effect inputs */
    seq1 = del_seq_find_replace_recurs(scene, seq->seq1);
    seq2 = del_seq_find_replace_recurs(scene, seq->seq2);
    seq3 = del_seq_find_replace_recurs(scene, seq->seq3);

    if (seq1 == seq->seq1 && seq2 == seq->seq2 && seq3 == seq->seq3) {
      /* pass */
    }
    else if (seq1 || seq2 || seq3) {
      seq->seq1 = (seq1) ? seq1 : (seq2) ? seq2 : seq3;
      seq->seq2 = (seq2) ? seq2 : (seq1) ? seq1 : seq3;
      seq->seq3 = (seq3) ? seq3 : (seq1) ? seq1 : seq2;

      BKE_sequencer_update_changed_seq_and_deps(scene, seq, 1, 1);
    }
    else {
      seq->flag |= SELECT; /* mark for delete */
    }
  }

  if (seq->flag & SELECT) {
    if ((seq1 = del_seq_find_replace_recurs(scene, seq->seq1))) {
      return seq1;
    }
    if ((seq2 = del_seq_find_replace_recurs(scene, seq->seq2))) {
      return seq2;
    }
    if ((seq3 = del_seq_find_replace_recurs(scene, seq->seq3))) {
      return seq3;
    }
    else {
      return NULL;
    }
  }
  else {
    return seq;
  }
}

static void del_seq_clear_modifiers_recurs(Scene *scene, Sequence *deleting_sequence)
{
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  Sequence *current_sequence;

  SEQP_BEGIN (ed, current_sequence) {
    if (!(current_sequence->flag & SELECT) && current_sequence != deleting_sequence) {
      SequenceModifierData *smd;

      for (smd = current_sequence->modifiers.first; smd; smd = smd->next) {
        if (smd->mask_sequence == deleting_sequence) {
          smd->mask_sequence = NULL;
        }
      }
    }
  }
  SEQ_END;
}

static void recurs_del_seq_flag(Scene *scene, ListBase *lb, short flag, short deleteall)
{
  Sequence *seq, *seqn;
  Sequence *last_seq = BKE_sequencer_active_get(scene);

  seq = lb->first;
  while (seq) {
    seqn = seq->next;
    if ((seq->flag & flag) || deleteall) {
      BLI_remlink(lb, seq);
      if (seq == last_seq) {
        BKE_sequencer_active_set(scene, NULL);
      }
      if (seq->type == SEQ_TYPE_META) {
        recurs_del_seq_flag(scene, &seq->seqbase, flag, 1);
      }
      BKE_sequence_free(scene, seq);
    }
    seq = seqn;
  }
}

static Sequence *cut_seq_hard(
    Main *bmain, Scene *scene, Sequence *seq, ListBase *new_seq_list, int cutframe)
{
  TransSeq ts;
  Sequence *seqn = NULL;
  bool skip_dup = false;

  /* Unlike soft-cut, it's important to use the same value for both strips. */
  const bool is_end_exact = ((seq->start + seq->len) == cutframe);

  /* backup values */
  ts.start = seq->start;
  ts.machine = seq->machine;
  ts.startstill = seq->startstill;
  ts.endstill = seq->endstill;
  ts.startdisp = seq->startdisp;
  ts.enddisp = seq->enddisp;
  ts.startofs = seq->startofs;
  ts.endofs = seq->endofs;
  ts.anim_startofs = seq->anim_startofs;
  ts.anim_endofs = seq->anim_endofs;
  ts.len = seq->len;

  /* First Strip! */
  /* strips with extended stillfames before */

  /* Precaution, needed because the length saved on-disk may not match the length saved in the
   * blend file, or our code may have minor differences reading file length between versions.
   * This causes hard-cut to fail, see: T47862 */
  if (seq->type != SEQ_TYPE_META) {
    BKE_sequence_reload_new_file(bmain, scene, seq, true);
    BKE_sequence_calc(scene, seq);
  }

  /* Important to offset the start when 'cutframe == seq->start'
   * because we need at least one frame of content after start/end still have clipped it. */
  if ((seq->startstill) && (cutframe <= seq->start)) {
    /* don't do funny things with METAs ... */
    if (seq->type == SEQ_TYPE_META) {
      skip_dup = true;
      seq->startstill = seq->start - cutframe;
    }
    else {
      seq->start = cutframe - 1;
      seq->startstill = cutframe - seq->startdisp - 1;
      seq->anim_endofs += seq->len - 1;
      seq->endstill = 0;
    }
  }
  /* normal strip */
  else if ((is_end_exact == false) &&
           ((cutframe >= seq->start) && (cutframe <= (seq->start + seq->len)))) {
    seq->endofs = 0;
    seq->endstill = 0;
    seq->anim_endofs += (seq->start + seq->len) - cutframe;
  }
  /* strips with extended stillframes after */
  else if ((is_end_exact == true) || (((seq->start + seq->len) < cutframe) && (seq->endstill))) {
    seq->endstill -= seq->enddisp - cutframe;
    /* don't do funny things with METAs ... */
    if (seq->type == SEQ_TYPE_META) {
      skip_dup = true;
    }
  }

  BKE_sequence_reload_new_file(bmain, scene, seq, false);
  BKE_sequence_calc(scene, seq);

  if (!skip_dup) {
    /* Duplicate AFTER the first change */
    seqn = BKE_sequence_dupli_recursive(
        scene, scene, new_seq_list, seq, SEQ_DUPE_UNIQUE_NAME | SEQ_DUPE_ANIM);
  }

  if (seqn) {
    seqn->flag |= SELECT;

    /* Important not to re-assign this (unlike soft-cut) */
#if 0
    is_end_exact = ((seqn->start + seqn->len) == cutframe);
#endif
    /* Second Strip! */
    /* strips with extended stillframes before */
    if ((seqn->startstill) && (cutframe == seqn->start + 1)) {
      seqn->start = ts.start;
      seqn->startstill = ts.start - cutframe;
      seqn->anim_endofs = ts.anim_endofs;
      seqn->endstill = ts.endstill;
    }

    /* normal strip */
    else if ((is_end_exact == false) &&
             ((cutframe >= seqn->start) && (cutframe <= (seqn->start + seqn->len)))) {
      seqn->start = cutframe;
      seqn->startstill = 0;
      seqn->startofs = 0;
      seqn->endofs = ts.endofs;
      seqn->anim_startofs += cutframe - ts.start;
      seqn->anim_endofs = ts.anim_endofs;
      seqn->endstill = ts.endstill;
    }

    /* strips with extended stillframes after */
    else if ((is_end_exact == true) ||
             (((seqn->start + seqn->len) < cutframe) && (seqn->endstill))) {
      seqn->start = cutframe;
      seqn->startofs = 0;
      seqn->anim_startofs += ts.len - 1;
      seqn->endstill = ts.enddisp - cutframe - 1;
      seqn->startstill = 0;
    }

    BKE_sequence_reload_new_file(bmain, scene, seqn, false);
    BKE_sequence_calc(scene, seqn);
  }
  return seqn;
}

static Sequence *cut_seq_soft(
    Main *UNUSED(bmain), Scene *scene, Sequence *seq, ListBase *new_seq_list, int cutframe)
{
  TransSeq ts;
  Sequence *seqn = NULL;
  bool skip_dup = false;

  bool is_end_exact = ((seq->start + seq->len) == cutframe);

  /* backup values */
  ts.start = seq->start;
  ts.machine = seq->machine;
  ts.startstill = seq->startstill;
  ts.endstill = seq->endstill;
  ts.startdisp = seq->startdisp;
  ts.enddisp = seq->enddisp;
  ts.startofs = seq->startofs;
  ts.endofs = seq->endofs;
  ts.anim_startofs = seq->anim_startofs;
  ts.anim_endofs = seq->anim_endofs;
  ts.len = seq->len;

  /* First Strip! */
  /* strips with extended stillfames before */

  /* Important to offset the start when 'cutframe == seq->start'
   * because we need at least one frame of content after start/end still have clipped it. */
  if ((seq->startstill) && (cutframe <= seq->start)) {
    /* don't do funny things with METAs ... */
    if (seq->type == SEQ_TYPE_META) {
      skip_dup = true;
      seq->startstill = seq->start - cutframe;
    }
    else {
      seq->start = cutframe - 1;
      seq->startstill = cutframe - seq->startdisp - 1;
      seq->endofs = seq->len - 1;
      seq->endstill = 0;
    }
  }
  /* normal strip */
  else if ((is_end_exact == false) && (cutframe >= seq->start) &&
           (cutframe <= (seq->start + seq->len))) {
    seq->endofs = (seq->start + seq->len) - cutframe;
  }
  /* strips with extended stillframes after */
  else if ((is_end_exact == true) || (((seq->start + seq->len) < cutframe) && (seq->endstill))) {
    seq->endstill -= seq->enddisp - cutframe;
    /* don't do funny things with METAs ... */
    if (seq->type == SEQ_TYPE_META) {
      skip_dup = true;
    }
  }

  BKE_sequence_calc(scene, seq);

  if (!skip_dup) {
    /* Duplicate AFTER the first change */
    seqn = BKE_sequence_dupli_recursive(
        scene, scene, new_seq_list, seq, SEQ_DUPE_UNIQUE_NAME | SEQ_DUPE_ANIM);
  }

  if (seqn) {
    seqn->flag |= SELECT;

    is_end_exact = ((seqn->start + seqn->len) == cutframe);

    /* Second Strip! */
    /* strips with extended stillframes before */
    if ((seqn->startstill) && (cutframe == seqn->start + 1)) {
      seqn->start = ts.start;
      seqn->startstill = ts.start - cutframe;
      seqn->endofs = ts.endofs;
      seqn->endstill = ts.endstill;
    }

    /* normal strip */
    else if ((is_end_exact == false) && (cutframe >= seqn->start) &&
             (cutframe <= (seqn->start + seqn->len))) {
      seqn->startstill = 0;
      seqn->startofs = cutframe - ts.start;
      seqn->endofs = ts.endofs;
      seqn->endstill = ts.endstill;
    }

    /* strips with extended stillframes after */
    else if ((is_end_exact == true) ||
             (((seqn->start + seqn->len) < cutframe) && (seqn->endstill))) {
      seqn->start = cutframe - ts.len + 1;
      seqn->startofs = ts.len - 1;
      seqn->endstill = ts.enddisp - cutframe - 1;
      seqn->startstill = 0;
    }

    BKE_sequence_calc(scene, seqn);
  }
  return seqn;
}

/* like duplicate, but only duplicate and cut overlapping strips,
 * strips to the left of the cutframe are ignored and strips to the right
 * are moved to the end of slist
 * we have to work on the same slist (not using a separate list), since
 * otherwise dupli_seq can't check for duplicate names properly and
 * may generate strips with the same name (which will mess up animdata)
 */

static bool cut_seq_list(Main *bmain,
                         Scene *scene,
                         ListBase *slist,
                         int cutframe,
                         Sequence *(*cut_seq)(Main *bmain, Scene *, Sequence *, ListBase *, int))
{
  Sequence *seq, *seq_next_iter;
  Sequence *seq_first_new = NULL;

  seq = slist->first;

  while (seq && seq != seq_first_new) {
    seq_next_iter = seq->next; /* we need this because we may remove seq */
    seq->tmp = NULL;
    if (seq->flag & SELECT) {
      if (cutframe > seq->startdisp && cutframe < seq->enddisp) {
        Sequence *seqn = cut_seq(bmain, scene, seq, slist, cutframe);
        if (seqn) {
          if (seq_first_new == NULL) {
            seq_first_new = seqn;
          }
        }
      }
      else if (seq->enddisp <= cutframe) {
        /* do nothing */
      }
      else if (seq->startdisp >= cutframe) {
        /* move to tail */
        BLI_remlink(slist, seq);
        BLI_addtail(slist, seq);

        if (seq_first_new == NULL) {
          seq_first_new = seq;
        }
      }
    }
    seq = seq_next_iter;
  }

  return (seq_first_new != NULL);
}

static bool sequence_offset_after_frame(Scene *scene, const int delta, const int cfra)
{
  Sequence *seq;
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  bool done = false;
  TimeMarker *marker;

  /* all strips >= cfra are shifted */

  if (ed == NULL) {
    return 0;
  }

  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    if (seq->startdisp >= cfra) {
      BKE_sequence_translate(scene, seq, delta);
      BKE_sequence_calc(scene, seq);
      done = true;
    }
  }

  if (!scene->toolsettings->lock_markers) {
    for (marker = scene->markers.first; marker; marker = marker->next) {
      if (marker->frame >= cfra) {
        marker->frame += delta;
      }
    }
  }

  return done;
}

#if 0
static void set_filter_seq(Scene *scene)
{
  Sequence *seq;
  Editing *ed = BKE_sequencer_editing_get(scene, false);

  if (ed == NULL) {
    return;
  }

  if (okee("Set Deinterlace") == 0) {
    return;
  }

  SEQP_BEGIN (ed, seq) {
    if (seq->flag & SELECT) {
      if (seq->type == SEQ_TYPE_MOVIE) {
        seq->flag |= SEQ_FILTERY;
        BKE_sequence_reload_new_file(bmain, scene, seq, false);
        BKE_sequence_calc(scene, seq);
      }
    }
  }
  SEQ_END;
}
#endif

static void UNUSED_FUNCTION(seq_remap_paths)(Scene *scene)
{
  Sequence *seq, *last_seq = BKE_sequencer_active_get(scene);
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  char from[FILE_MAX], to[FILE_MAX], stripped[FILE_MAX];

  if (last_seq == NULL) {
    return;
  }

  BLI_strncpy(from, last_seq->strip->dir, sizeof(from));
  // XXX  if (0 == sbutton(from, 0, sizeof(from) - 1, "From: "))
  //      return;

  BLI_strncpy(to, from, sizeof(to));
  // XXX  if (0 == sbutton(to, 0, sizeof(to) - 1, "To: "))
  //      return;

  if (STREQ(to, from)) {
    return;
  }

  SEQP_BEGIN (ed, seq) {
    if (seq->flag & SELECT) {
      if (STREQLEN(seq->strip->dir, from, strlen(from))) {
        printf("found %s\n", seq->strip->dir);

        /* strip off the beginning */
        stripped[0] = 0;
        BLI_strncpy(stripped, seq->strip->dir + strlen(from), FILE_MAX);

        /* new path */
        BLI_snprintf(seq->strip->dir, sizeof(seq->strip->dir), "%s%s", to, stripped);
        printf("new %s\n", seq->strip->dir);
      }
    }
  }
  SEQ_END;
}

static int sequencer_gap_remove_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  rctf rectf;
  int cfra, efra, sfra;
  bool first = false, done;
  bool do_all = RNA_boolean_get(op->ptr, "all");

  /* get first and last frame */
  boundbox_seq(scene, &rectf);
  sfra = (int)rectf.xmin;
  efra = (int)rectf.xmax;

  /* first check if the current frame has a gap already */
  for (cfra = CFRA; cfra >= sfra; cfra--) {
    if (BKE_sequencer_evaluate_frame(scene, cfra)) {
      first = true;
      break;
    }
  }

  for (; cfra < efra; cfra++) {
    /* first == 0 means there's still no strip to remove a gap for */
    if (first == false) {
      if (BKE_sequencer_evaluate_frame(scene, cfra)) {
        first = true;
      }
    }
    else if (BKE_sequencer_evaluate_frame(scene, cfra) == 0) {
      done = true;
      while (BKE_sequencer_evaluate_frame(scene, cfra) == 0) {
        done = sequence_offset_after_frame(scene, -1, cfra);
        if (done == false) {
          break;
        }
      }
      if (done == false || do_all == false) {
        break;
      }
    }
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_gap_remove(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Gaps";
  ot->idname = "SEQUENCER_OT_gap_remove";
  ot->description =
      "Remove gap at current frame to first strip at the right, independent of selection or "
      "locked state of strips";

  /* api callbacks */
  //  ot->invoke = sequencer_snap_invoke;
  ot->exec = sequencer_gap_remove_exec;
  ot->poll = sequencer_edit_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "all", 0, "All Gaps", "Do all gaps to right of current frame");
}

static int sequencer_gap_insert_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  int frames = RNA_int_get(op->ptr, "frames");

  sequence_offset_after_frame(scene, frames, CFRA);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_gap_insert(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Insert Gaps";
  ot->idname = "SEQUENCER_OT_gap_insert";
  ot->description =
      "Insert gap at current frame to first strips at the right, independent of selection or "
      "locked state of strips";

  /* api callbacks */
  //  ot->invoke = sequencer_snap_invoke;
  ot->exec = sequencer_gap_insert_exec;
  ot->poll = sequencer_edit_poll;

  /* flags */
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

#if 0
static int seq_get_snaplimit(View2D *v2d)
{
  /* fake mouse coords to get the snap value
   * a bit lazy but its only done once pre transform */
  float xmouse, ymouse, x;
  int mval[2] = {24, 0}; /* 24 screen px snap */

  UI_view2d_region_to_view(v2d, mval[0], mval[1], &xmouse, &ymouse);
  x = xmouse;
  mval[0] = 0;
  UI_view2d_region_to_view(v2d, mval[0], mval[1], &xmouse, &ymouse);
  return (int)(x - xmouse);
}
#endif

/* Operator functions */
bool sequencer_edit_poll(bContext *C)
{
  return (BKE_sequencer_editing_get(CTX_data_scene(C), false) != NULL);
}

#if 0 /* UNUSED */
bool sequencer_strip_poll(bContext *C)
{
  Editing *ed;
  return (((ed = BKE_sequencer_editing_get(CTX_data_scene(C), false)) != NULL) &&
          (ed->act_seq != NULL));
}
#endif

bool sequencer_strip_has_path_poll(bContext *C)
{
  Editing *ed;
  Sequence *seq;
  return (((ed = BKE_sequencer_editing_get(CTX_data_scene(C), false)) != NULL) &&
          ((seq = ed->act_seq) != NULL) && (SEQ_HAS_PATH(seq)));
}

bool sequencer_view_preview_poll(bContext *C)
{
  SpaceSeq *sseq = CTX_wm_space_seq(C);
  Editing *ed = BKE_sequencer_editing_get(CTX_data_scene(C), false);
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

/* snap operator*/
static int sequencer_snap_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);

  Editing *ed = BKE_sequencer_editing_get(scene, false);
  Sequence *seq;
  int snap_frame;

  snap_frame = RNA_int_get(op->ptr, "frame");

  /* also check metas */
  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    if (seq->flag & SELECT && !(seq->depth == 0 && seq->flag & SEQ_LOCK) &&
        BKE_sequence_tx_test(seq)) {
      if ((seq->flag & (SEQ_LEFTSEL + SEQ_RIGHTSEL)) == 0) {
        /* simple but no anim update */
        /* seq->start = snap_frame-seq->startofs+seq->startstill; */

        BKE_sequence_translate(
            scene, seq, (snap_frame - seq->startofs + seq->startstill) - seq->start);
      }
      else {
        if (seq->flag & SEQ_LEFTSEL) {
          BKE_sequence_tx_set_final_left(seq, snap_frame);
        }
        else { /* SEQ_RIGHTSEL */
          BKE_sequence_tx_set_final_right(seq, snap_frame);
        }
        BKE_sequence_tx_handle_xlimits(seq, seq->flag & SEQ_LEFTSEL, seq->flag & SEQ_RIGHTSEL);
        BKE_sequence_single_fix(seq);
      }
      BKE_sequence_calc(scene, seq);
    }
  }

  /* test for effects and overlap
   * don't use SEQP_BEGIN since that would be recursive */
  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    if (seq->flag & SELECT && !(seq->depth == 0 && seq->flag & SEQ_LOCK)) {
      seq->flag &= ~SEQ_OVERLAP;
      if (BKE_sequence_test_overlap(ed->seqbasep, seq)) {
        BKE_sequence_base_shuffle(ed->seqbasep, seq, scene);
      }
    }
    else if (seq->type & SEQ_TYPE_EFFECT) {
      if (seq->seq1 && (seq->seq1->flag & SELECT)) {
        BKE_sequence_calc(scene, seq);
      }
      else if (seq->seq2 && (seq->seq2->flag & SELECT)) {
        BKE_sequence_calc(scene, seq);
      }
      else if (seq->seq3 && (seq->seq3->flag & SELECT)) {
        BKE_sequence_calc(scene, seq);
      }
    }
  }

  /* as last: */
  BKE_sequencer_sort(scene);

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
  /* identifiers */
  ot->name = "Snap Strips to Playhead";
  ot->idname = "SEQUENCER_OT_snap";
  ot->description = "Frame where selected strips will be snapped";

  /* api callbacks */
  ot->invoke = sequencer_snap_invoke;
  ot->exec = sequencer_snap_exec;
  ot->poll = sequencer_edit_poll;

  /* flags */
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

typedef struct SlipData {
  int init_mouse[2];
  float init_mouseloc[2];
  TransSeq *ts;
  Sequence **seq_array;
  bool *trim;
  int num_seq;
  bool slow;
  int slow_offset; /* offset at the point where offset was turned on */
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

static int slip_add_sequences_rec(
    ListBase *seqbasep, Sequence **seq_array, bool *trim, int offset, bool do_trim)
{
  Sequence *seq;
  int num_items = 0;

  for (seq = seqbasep->first; seq; seq = seq->next) {
    if (!do_trim || (!(seq->type & SEQ_TYPE_EFFECT) && (seq->flag & SELECT))) {
      seq_array[offset + num_items] = seq;
      trim[offset + num_items] = do_trim;
      num_items++;

      if (seq->type == SEQ_TYPE_META) {
        /* trim the sub-sequences */
        num_items += slip_add_sequences_rec(
            &seq->seqbase, seq_array, trim, num_items + offset, false);
      }
      else if (seq->type & SEQ_TYPE_EFFECT) {
        trim[offset + num_items] = false;
      }
    }
  }

  return num_items;
}

static int slip_count_sequences_rec(ListBase *seqbasep, bool first_level)
{
  Sequence *seq;
  int trimmed_sequences = 0;

  for (seq = seqbasep->first; seq; seq = seq->next) {
    if (!first_level || (!(seq->type & SEQ_TYPE_EFFECT) && (seq->flag & SELECT))) {
      trimmed_sequences++;

      if (seq->type == SEQ_TYPE_META) {
        /* trim the sub-sequences */
        trimmed_sequences += slip_count_sequences_rec(&seq->seqbase, false);
      }
    }
  }

  return trimmed_sequences;
}

static int sequencer_slip_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  SlipData *data;
  Scene *scene = CTX_data_scene(C);
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  float mouseloc[2];
  int num_seq, i;
  View2D *v2d = UI_view2d_fromcontext(C);

  /* first recursively count the trimmed elements */
  num_seq = slip_count_sequences_rec(ed->seqbasep, true);

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

  slip_add_sequences_rec(ed->seqbasep, data->seq_array, data->trim, 0, true);

  for (i = 0; i < num_seq; i++) {
    transseq_backup(data->ts + i, data->seq_array[i]);
  }

  UI_view2d_region_to_view(v2d, event->mval[0], event->mval[1], &mouseloc[0], &mouseloc[1]);

  copy_v2_v2_int(data->init_mouse, event->mval);
  copy_v2_v2(data->init_mouseloc, mouseloc);

  data->slow = false;

  WM_event_add_modal_handler(C, op);

  /* notify so we draw extensions immediately */
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_RUNNING_MODAL;
}

static bool sequencer_slip_recursively(Scene *scene, SlipData *data, int offset)
{

  /* only data types supported for now */
  if (offset != 0) {
    Editing *ed = BKE_sequencer_editing_get(scene, false);
    int i;

    /* we iterate in reverse so metastrips are iterated after their children */
    for (i = data->num_seq - 1; i >= 0; i--) {
      Sequence *seq = data->seq_array[i];
      int endframe;
      /* we have the offset, do the terrible math */

      /* first, do the offset */
      seq->start = data->ts[i].start + offset;

      if (data->trim[i]) {
        /* find the endframe */
        endframe = seq->start + seq->len;

        /* now compute the terrible offsets */
        if (endframe > seq->enddisp) {
          seq->endstill = 0;
          seq->endofs = endframe - seq->enddisp;
        }
        else if (endframe <= seq->enddisp) {
          seq->endstill = seq->enddisp - endframe;
          seq->endofs = 0;
        }

        if (seq->start > seq->startdisp) {
          seq->startstill = seq->start - seq->startdisp;
          seq->startofs = 0;
        }
        else if (seq->start <= seq->startdisp) {
          seq->startstill = 0;
          seq->startofs = seq->startdisp - seq->start;
        }
      }
      else {
        /* if no real trim, don't change the data, rather transform the strips themselves */
        seq->startdisp = data->ts[i].startdisp + offset;
        seq->enddisp = data->ts[i].enddisp + offset;
      }

      /* effects are only added if we they are in a meta-strip.
       * In this case, dependent strips will just be transformed and
       * we can skip calculating for effects.
       * This way we can avoid an extra loop just for effects*/
      if (!(seq->type & SEQ_TYPE_EFFECT)) {
        BKE_sequence_calc(scene, seq);
      }
    }
    BKE_sequencer_free_imbuf(scene, &ed->seqbase, false);

    return true;
  }

  return false;
}

static int sequencer_slip_exec(bContext *C, wmOperator *op)
{
  SlipData *data;
  Scene *scene = CTX_data_scene(C);
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  int num_seq, i;
  int offset = RNA_int_get(op->ptr, "offset");
  bool success = false;

  /* first recursively count the trimmed elements */
  num_seq = slip_count_sequences_rec(ed->seqbasep, true);

  if (num_seq == 0) {
    return OPERATOR_CANCELLED;
  }

  data = op->customdata = MEM_mallocN(sizeof(SlipData), "trimdata");
  data->ts = MEM_mallocN(num_seq * sizeof(TransSeq), "trimdata_transform");
  data->seq_array = MEM_mallocN(num_seq * sizeof(Sequence *), "trimdata_sequences");
  data->trim = MEM_mallocN(num_seq * sizeof(bool), "trimdata_trim");
  data->num_seq = num_seq;

  slip_add_sequences_rec(ed->seqbasep, data->seq_array, data->trim, 0, true);

  for (i = 0; i < num_seq; i++) {
    transseq_backup(data->ts + i, data->seq_array[i]);
  }

  success = sequencer_slip_recursively(scene, data, offset);

  MEM_freeN(data->seq_array);
  MEM_freeN(data->trim);
  MEM_freeN(data->ts);
  MEM_freeN(data);

  if (success) {
    WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
    return OPERATOR_FINISHED;
  }
  else {
    return OPERATOR_CANCELLED;
  }
}

static void sequencer_slip_update_header(Scene *scene, ScrArea *sa, SlipData *data, int offset)
{
  char msg[UI_MAX_DRAW_STR];

  if (sa) {
    if (hasNumInput(&data->num_input)) {
      char num_str[NUM_STR_REP_LEN];
      outputNumInput(&data->num_input, num_str, &scene->unit);
      BLI_snprintf(msg, sizeof(msg), IFACE_("Trim offset: %s"), num_str);
    }
    else {
      BLI_snprintf(msg, sizeof(msg), IFACE_("Trim offset: %d"), offset);
    }
  }

  ED_area_status_text(sa, msg);
}

static int sequencer_slip_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  SlipData *data = (SlipData *)op->customdata;
  ScrArea *sa = CTX_wm_area(C);
  const bool has_numInput = hasNumInput(&data->num_input);
  bool handled = true;

  /* Modal numinput active, try to handle numeric inputs first... */
  if (event->val == KM_PRESS && has_numInput && handleNumInput(C, &data->num_input, event)) {
    float offset;
    applyNumInput(&data->num_input, &offset);

    sequencer_slip_update_header(scene, sa, data, (int)offset);

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

        /* choose the side based on which side of the playhead the mouse is on */
        UI_view2d_region_to_view(v2d, mouse_x, 0, &mouseloc[0], &mouseloc[1]);
        offset = mouseloc[0] - data->init_mouseloc[0];

        sequencer_slip_update_header(scene, sa, data, offset);

        RNA_int_set(op->ptr, "offset", offset);

        if (sequencer_slip_recursively(scene, data, offset)) {
          WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
        }
      }
      break;
    }

    case LEFTMOUSE:
    case RETKEY:
    case SPACEKEY: {
      MEM_freeN(data->seq_array);
      MEM_freeN(data->trim);
      MEM_freeN(data->ts);
      MEM_freeN(data);
      op->customdata = NULL;
      if (sa) {
        ED_area_status_text(sa, NULL);
      }
      WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
      return OPERATOR_FINISHED;
    }

    case ESCKEY:
    case RIGHTMOUSE: {
      int i;
      Editing *ed = BKE_sequencer_editing_get(scene, false);

      for (i = 0; i < data->num_seq; i++) {
        transseq_restore(data->ts + i, data->seq_array[i]);
      }

      for (i = 0; i < data->num_seq; i++) {
        Sequence *seq = data->seq_array[i];
        BKE_sequence_reload_new_file(bmain, scene, seq, false);
        BKE_sequence_calc(scene, seq);
      }

      MEM_freeN(data->seq_array);
      MEM_freeN(data->ts);
      MEM_freeN(data->trim);
      MEM_freeN(data);
      op->customdata = NULL;

      WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

      BKE_sequencer_free_imbuf(scene, &ed->seqbase, false);

      if (sa) {
        ED_area_status_text(sa, NULL);
      }

      return OPERATOR_CANCELLED;
    }

    case RIGHTSHIFTKEY:
    case LEFTSHIFTKEY:
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

  /* Modal numinput inactive, try to handle numeric inputs last... */
  if (!handled && event->val == KM_PRESS && handleNumInput(C, &data->num_input, event)) {
    float offset;
    applyNumInput(&data->num_input, &offset);

    sequencer_slip_update_header(scene, sa, data, (int)offset);

    RNA_int_set(op->ptr, "offset", offset);

    if (sequencer_slip_recursively(scene, data, offset)) {
      WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

void SEQUENCER_OT_slip(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Trim Strips";
  ot->idname = "SEQUENCER_OT_slip";
  ot->description = "Trim the contents of the active strip";

  /* api callbacks */
  ot->invoke = sequencer_slip_invoke;
  ot->modal = sequencer_slip_modal;
  ot->exec = sequencer_slip_exec;
  ot->poll = sequencer_edit_poll;

  /* flags */
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

/* mute operator */
static int sequencer_mute_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  Sequence *seq;
  bool selected;

  selected = !RNA_boolean_get(op->ptr, "unselected");

  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    if ((seq->flag & SEQ_LOCK) == 0) {
      if (selected) { /* mute unselected */
        if (seq->flag & SELECT) {
          seq->flag |= SEQ_MUTE;
          BKE_sequence_invalidate_dependent(scene, seq);
        }
      }
      else {
        if ((seq->flag & SELECT) == 0) {
          seq->flag |= SEQ_MUTE;
          BKE_sequence_invalidate_dependent(scene, seq);
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
  /* identifiers */
  ot->name = "Mute Strips";
  ot->idname = "SEQUENCER_OT_mute";
  ot->description = "Mute (un)selected strips";

  /* api callbacks */
  ot->exec = sequencer_mute_exec;
  ot->poll = sequencer_edit_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(
      ot->srna, "unselected", 0, "Unselected", "Mute unselected rather than selected strips");
}

/* unmute operator */
static int sequencer_unmute_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  Sequence *seq;
  bool selected;

  selected = !RNA_boolean_get(op->ptr, "unselected");

  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    if ((seq->flag & SEQ_LOCK) == 0) {
      if (selected) { /* unmute unselected */
        if (seq->flag & SELECT) {
          seq->flag &= ~SEQ_MUTE;
          BKE_sequence_invalidate_dependent(scene, seq);
        }
      }
      else {
        if ((seq->flag & SELECT) == 0) {
          seq->flag &= ~SEQ_MUTE;
          BKE_sequence_invalidate_dependent(scene, seq);
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
  /* identifiers */
  ot->name = "Unmute Strips";
  ot->idname = "SEQUENCER_OT_unmute";
  ot->description = "Unmute (un)selected strips";

  /* api callbacks */
  ot->exec = sequencer_unmute_exec;
  ot->poll = sequencer_edit_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(
      ot->srna, "unselected", 0, "Unselected", "Unmute unselected rather than selected strips");
}

/* lock operator */
static int sequencer_lock_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = BKE_sequencer_editing_get(scene, false);
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
  /* identifiers */
  ot->name = "Lock Strips";
  ot->idname = "SEQUENCER_OT_lock";
  ot->description = "Lock strips so they can't be transformed";

  /* api callbacks */
  ot->exec = sequencer_lock_exec;
  ot->poll = sequencer_edit_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* unlock operator */
static int sequencer_unlock_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = BKE_sequencer_editing_get(scene, false);
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
  /* identifiers */
  ot->name = "Unlock Strips";
  ot->idname = "SEQUENCER_OT_unlock";
  ot->description = "Unlock strips so they can be transformed";

  /* api callbacks */
  ot->exec = sequencer_unlock_exec;
  ot->poll = sequencer_edit_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* reload operator */
static int sequencer_reload_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  Sequence *seq;
  const bool adjust_length = RNA_boolean_get(op->ptr, "adjust_length");

  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    if (seq->flag & SELECT) {
      BKE_sequencer_update_changed_seq_and_deps(scene, seq, 0, 1);
      BKE_sequence_reload_new_file(bmain, scene, seq, !adjust_length);

      if (adjust_length) {
        if (BKE_sequence_test_overlap(ed->seqbasep, seq)) {
          BKE_sequence_base_shuffle(ed->seqbasep, seq, scene);
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

  /* identifiers */
  ot->name = "Reload Strips";
  ot->idname = "SEQUENCER_OT_reload";
  ot->description = "Reload strips in the sequencer";

  /* api callbacks */
  ot->exec = sequencer_reload_exec;
  ot->poll = sequencer_edit_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER; /* no undo, the data changed is stored outside 'main' */

  prop = RNA_def_boolean(ot->srna,
                         "adjust_length",
                         0,
                         "Adjust Length",
                         "Adjust length of strips to their data length");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/* reload operator */
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
  Editing *ed = BKE_sequencer_editing_get(scene, false);

  BKE_sequencer_free_imbuf(scene, &ed->seqbase, false);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_refresh_all(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Refresh Sequencer";
  ot->idname = "SEQUENCER_OT_refresh_all";
  ot->description = "Refresh the sequencer editor";

  /* api callbacks */
  ot->exec = sequencer_refresh_all_exec;
  ot->poll = sequencer_refresh_all_poll;
}

static int sequencer_reassign_inputs_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Sequence *seq1, *seq2, *seq3, *last_seq = BKE_sequencer_active_get(scene);
  const char *error_msg;

  if (!seq_effect_find_selected(
          scene, last_seq, last_seq->type, &seq1, &seq2, &seq3, &error_msg)) {
    BKE_report(op->reports, RPT_ERROR, error_msg);
    return OPERATOR_CANCELLED;
  }
  /* see reassigning would create a cycle */
  if (seq_is_predecessor(seq1, last_seq) || seq_is_predecessor(seq2, last_seq) ||
      seq_is_predecessor(seq3, last_seq)) {
    BKE_report(op->reports, RPT_ERROR, "Cannot reassign inputs: no cycles allowed");
    return OPERATOR_CANCELLED;
  }

  last_seq->seq1 = seq1;
  last_seq->seq2 = seq2;
  last_seq->seq3 = seq3;

  BKE_sequencer_update_changed_seq_and_deps(scene, last_seq, 1, 1);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

static bool sequencer_effect_poll(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = BKE_sequencer_editing_get(scene, false);

  if (ed) {
    Sequence *last_seq = BKE_sequencer_active_get(scene);
    if (last_seq && (last_seq->type & SEQ_TYPE_EFFECT)) {
      return 1;
    }
  }

  return 0;
}

void SEQUENCER_OT_reassign_inputs(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reassign Inputs";
  ot->idname = "SEQUENCER_OT_reassign_inputs";
  ot->description = "Reassign the inputs for the effect strip";

  /* api callbacks */
  ot->exec = sequencer_reassign_inputs_exec;
  ot->poll = sequencer_effect_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int sequencer_swap_inputs_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Sequence *seq, *last_seq = BKE_sequencer_active_get(scene);

  if (last_seq->seq1 == NULL || last_seq->seq2 == NULL) {
    BKE_report(op->reports, RPT_ERROR, "No valid inputs to swap");
    return OPERATOR_CANCELLED;
  }

  seq = last_seq->seq1;
  last_seq->seq1 = last_seq->seq2;
  last_seq->seq2 = seq;

  BKE_sequencer_update_changed_seq_and_deps(scene, last_seq, 1, 1);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}
void SEQUENCER_OT_swap_inputs(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Swap Inputs";
  ot->idname = "SEQUENCER_OT_swap_inputs";
  ot->description = "Swap the first two inputs for the effect strip";

  /* api callbacks */
  ot->exec = sequencer_swap_inputs_exec;
  ot->poll = sequencer_effect_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* cut operator */
static const EnumPropertyItem prop_cut_types[] = {
    {SEQ_CUT_SOFT, "SOFT", 0, "Soft", ""},
    {SEQ_CUT_HARD, "HARD", 0, "Hard", ""},
    {0, NULL, 0, NULL, NULL},
};

static int sequencer_cut_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  int cut_side, cut_hard, cut_frame;

  bool changed;

  cut_frame = RNA_int_get(op->ptr, "frame");
  cut_hard = RNA_enum_get(op->ptr, "type");
  cut_side = RNA_enum_get(op->ptr, "side");

  if (cut_hard == SEQ_CUT_HARD) {
    changed = cut_seq_list(bmain, scene, ed->seqbasep, cut_frame, cut_seq_hard);
  }
  else {
    changed = cut_seq_list(bmain, scene, ed->seqbasep, cut_frame, cut_seq_soft);
  }

  if (changed) { /* got new strips ? */
    Sequence *seq;

    if (cut_side != SEQ_SIDE_BOTH) {
      SEQP_BEGIN (ed, seq) {
        if (cut_side == SEQ_SIDE_LEFT) {
          if (seq->startdisp >= cut_frame) {
            seq->flag &= ~SEQ_ALLSEL;
          }
        }
        else {
          if (seq->enddisp <= cut_frame) {
            seq->flag &= ~SEQ_ALLSEL;
          }
        }
      }
      SEQ_END;
    }

    SEQP_BEGIN (ed, seq) {
      if (seq->seq1 || seq->seq2 || seq->seq3) {
        BKE_sequence_calc(scene, seq);
      }
    }
    SEQ_END;

    /* as last: */
    BKE_sequencer_sort(scene);
  }

  if (changed) {
    WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
    return OPERATOR_FINISHED;
  }
  else {
    return OPERATOR_CANCELLED;
  }
}

static int sequencer_cut_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Scene *scene = CTX_data_scene(C);
  View2D *v2d = UI_view2d_fromcontext(C);

  int cut_side = RNA_enum_get(op->ptr, "side");
  int cut_frame = CFRA;

  if (cut_side == SEQ_SIDE_MOUSE) {
    if (ED_operator_sequencer_active(C) && v2d) {
      cut_side = mouse_frame_side(v2d, event->mval[0], cut_frame);
    }
    else {
      cut_side = SEQ_SIDE_BOTH;
    }
  }
  RNA_int_set(op->ptr, "frame", cut_frame);
  RNA_enum_set(op->ptr, "side", cut_side);
  /*RNA_enum_set(op->ptr, "type", cut_hard); */ /*This type is set from the key shortcut */
  return sequencer_cut_exec(C, op);
}

void SEQUENCER_OT_cut(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Cut Strips";
  ot->idname = "SEQUENCER_OT_cut";
  ot->description = "Cut the selected strips";

  /* api callbacks */
  ot->invoke = sequencer_cut_invoke;
  ot->exec = sequencer_cut_exec;
  ot->poll = sequencer_edit_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  PropertyRNA *prop;
  RNA_def_int(ot->srna,
              "frame",
              0,
              INT_MIN,
              INT_MAX,
              "Frame",
              "Frame where selected strips will be cut",
              INT_MIN,
              INT_MAX);
  RNA_def_enum(ot->srna,
               "type",
               prop_cut_types,
               SEQ_CUT_SOFT,
               "Type",
               "The type of cut operation to perform on strips");
  prop = RNA_def_enum(ot->srna,
                      "side",
                      prop_side_types,
                      SEQ_SIDE_MOUSE,
                      "Side",
                      "The side that remains selected after cutting");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

#undef SEQ_SIDE_MOUSE

/* duplicate operator */
static int apply_unique_name_cb(Sequence *seq, void *arg_pt)
{
  Scene *scene = (Scene *)arg_pt;
  char name[sizeof(seq->name) - 2];

  BLI_strncpy_utf8(name, seq->name + 2, sizeof(name));
  BKE_sequence_base_unique_name_recursive(&scene->ed->seqbase, seq);
  BKE_sequencer_dupe_animdata(scene, name, seq->name + 2);
  return 1;
}

static int sequencer_add_duplicate_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = BKE_sequencer_editing_get(scene, false);

  ListBase nseqbase = {NULL, NULL};

  if (ed == NULL) {
    return OPERATOR_CANCELLED;
  }

  BKE_sequence_base_dupli_recursive(scene, scene, &nseqbase, ed->seqbasep, SEQ_DUPE_CONTEXT, 0);

  if (nseqbase.first) {
    Sequence *seq = nseqbase.first;
    /* rely on the nseqbase list being added at the end */
    BLI_movelisttolist(ed->seqbasep, &nseqbase);

    for (; seq; seq = seq->next) {
      BKE_sequencer_recursive_apply(seq, apply_unique_name_cb, scene);
    }

    WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);
    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

void SEQUENCER_OT_duplicate(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Duplicate Strips";
  ot->idname = "SEQUENCER_OT_duplicate";
  ot->description = "Duplicate the selected strips";

  /* api callbacks */
  ot->exec = sequencer_add_duplicate_exec;
  ot->poll = ED_operator_sequencer_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* to give to transform */
  RNA_def_enum(ot->srna, "mode", rna_enum_transform_mode_types, TFM_TRANSLATION, "Mode", "");
}

/* delete operator */
static int sequencer_delete_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  Sequence *seq;
  MetaStack *ms;
  bool nothing_selected = true;

  seq = BKE_sequencer_active_get(scene);
  if (seq && seq->flag & SELECT) { /* avoid a loop since this is likely to be selected */
    nothing_selected = false;
  }
  else {
    for (seq = ed->seqbasep->first; seq; seq = seq->next) {
      if (seq->flag & SELECT) {
        nothing_selected = false;
        break;
      }
    }
  }

  if (nothing_selected) {
    return OPERATOR_FINISHED;
  }

  /* for effects and modifiers, try to find a replacement input */
  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    if (!(seq->flag & SELECT)) {
      if ((seq->type & SEQ_TYPE_EFFECT)) {
        del_seq_find_replace_recurs(scene, seq);
      }
    }
    else {
      del_seq_clear_modifiers_recurs(scene, seq);
    }
  }

  /* delete all selected strips */
  recurs_del_seq_flag(scene, ed->seqbasep, SELECT, 0);

  /* updates lengths etc */
  seq = ed->seqbasep->first;
  while (seq) {
    BKE_sequence_calc(scene, seq);
    seq = seq->next;
  }

  /* free parent metas */
  ms = ed->metastack.last;
  while (ms) {
    BKE_sequence_calc(scene, ms->parseq);
    ms = ms->prev;
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

static int sequencer_delete_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *ar = CTX_wm_region(C);

  if (ar->regiontype == RGN_TYPE_WINDOW) {
    /* bounding box of 30 pixels is used for markers shortcuts,
     * prevent conflict with markers shortcuts here
     */
    if (event->mval[1] <= 30) {
      return OPERATOR_PASS_THROUGH;
    }
  }

  return WM_operator_confirm(C, op, event);
}

void SEQUENCER_OT_delete(wmOperatorType *ot)
{

  /* identifiers */
  ot->name = "Erase Strips";
  ot->idname = "SEQUENCER_OT_delete";
  ot->description = "Erase selected strips from the sequencer";

  /* api callbacks */
  ot->invoke = sequencer_delete_invoke;
  ot->exec = sequencer_delete_exec;
  ot->poll = sequencer_edit_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* offset clear operator */
static int sequencer_offset_clear_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  Sequence *seq;

  /* for effects, try to find a replacement input */
  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    if ((seq->type & SEQ_TYPE_EFFECT) == 0 && (seq->flag & SELECT)) {
      seq->startofs = seq->endofs = seq->startstill = seq->endstill = 0;
    }
  }

  /* updates lengths etc */
  seq = ed->seqbasep->first;
  while (seq) {
    BKE_sequence_calc(scene, seq);
    seq = seq->next;
  }

  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    if ((seq->type & SEQ_TYPE_EFFECT) == 0 && (seq->flag & SELECT)) {
      if (BKE_sequence_test_overlap(ed->seqbasep, seq)) {
        BKE_sequence_base_shuffle(ed->seqbasep, seq, scene);
      }
    }
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_offset_clear(wmOperatorType *ot)
{

  /* identifiers */
  ot->name = "Clear Strip Offset";
  ot->idname = "SEQUENCER_OT_offset_clear";
  ot->description = "Clear strip offsets from the start and end frames";

  /* api callbacks */
  ot->exec = sequencer_offset_clear_exec;
  ot->poll = sequencer_edit_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* separate_images operator */
static int sequencer_separate_images_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = BKE_sequencer_editing_get(scene, false);

  Sequence *seq, *seq_new;
  Strip *strip_new;
  StripElem *se, *se_new;
  int start_ofs, cfra, frame_end;
  int step = RNA_int_get(op->ptr, "length");

  seq = ed->seqbasep->first; /* poll checks this is valid */

  while (seq) {
    if ((seq->flag & SELECT) && (seq->type == SEQ_TYPE_IMAGE) && (seq->len > 1)) {
      Sequence *seq_next;

      /* remove seq so overlap tests don't conflict,
       * see seq_free_sequence below for the real freeing. */
      BLI_remlink(ed->seqbasep, seq);
      /* if (seq->ipo) id_us_min(&seq->ipo->id); */
      /* XXX, remove fcurve and assign to split image strips */

      start_ofs = cfra = BKE_sequence_tx_get_final_left(seq, false);
      frame_end = BKE_sequence_tx_get_final_right(seq, false);

      while (cfra < frame_end) {
        /* new seq */
        se = BKE_sequencer_give_stripelem(seq, cfra);

        seq_new = BKE_sequence_dupli_recursive(
            scene, scene, ed->seqbasep, seq, SEQ_DUPE_UNIQUE_NAME);

        seq_new->start = start_ofs;
        seq_new->type = SEQ_TYPE_IMAGE;
        seq_new->len = 1;
        seq_new->endstill = step - 1;

        /* new strip */
        strip_new = seq_new->strip;
        strip_new->us = 1;

        /* new stripdata (only one element now!) */
        /* Note this assume all elements (images) have the same dimension,
         * since we only copy the name here. */
        se_new = MEM_reallocN(strip_new->stripdata, sizeof(*se_new));
        BLI_strncpy(se_new->name, se->name, sizeof(se_new->name));
        strip_new->stripdata = se_new;

        BKE_sequence_calc(scene, seq_new);

        if (step > 1) {
          seq_new->flag &= ~SEQ_OVERLAP;
          if (BKE_sequence_test_overlap(ed->seqbasep, seq_new)) {
            BKE_sequence_base_shuffle(ed->seqbasep, seq_new, scene);
          }
        }

        /* XXX, COPY FCURVES */

        cfra++;
        start_ofs += step;
      }

      seq_next = seq->next;
      BKE_sequence_free(scene, seq);
      seq = seq_next;
    }
    else {
      seq = seq->next;
    }
  }

  /* as last: */
  BKE_sequencer_sort(scene);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_images_separate(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Separate Images";
  ot->idname = "SEQUENCER_OT_images_separate";
  ot->description = "On image sequence strips, it returns a strip for each image";

  /* api callbacks */
  ot->exec = sequencer_separate_images_exec;
  ot->invoke = WM_operator_props_popup_confirm;
  ot->poll = sequencer_edit_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_int(ot->srna, "length", 1, 1, INT_MAX, "Length", "Length of each frame", 1, 1000);
}

/* META Operators */

/* separate_meta_toggle operator */
static int sequencer_meta_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  Sequence *last_seq = BKE_sequencer_active_get(scene);
  MetaStack *ms;

  if (last_seq && last_seq->type == SEQ_TYPE_META && last_seq->flag & SELECT) {
    /* Enter Metastrip */
    ms = MEM_mallocN(sizeof(MetaStack), "metastack");
    BLI_addtail(&ed->metastack, ms);
    ms->parseq = last_seq;
    ms->oldbasep = ed->seqbasep;
    copy_v2_v2_int(ms->disp_range, &ms->parseq->startdisp);

    ed->seqbasep = &last_seq->seqbase;

    BKE_sequencer_active_set(scene, NULL);
  }
  else {
    /* Exit Metastrip (if possible) */

    Sequence *seq;

    if (BLI_listbase_is_empty(&ed->metastack)) {
      return OPERATOR_CANCELLED;
    }

    ms = ed->metastack.last;
    BLI_remlink(&ed->metastack, ms);

    ed->seqbasep = ms->oldbasep;

    /* for old files, update from meta */
    if (ms->disp_range[0] == ms->disp_range[1]) {
      copy_v2_v2_int(ms->disp_range, &ms->parseq->startdisp);
    }

    /* recalc all: the meta can have effects connected to it */
    for (seq = ed->seqbasep->first; seq; seq = seq->next) {
      BKE_sequence_calc(scene, seq);
    }

    /* 2.73+, keeping endpoints is important!
     * moving them around means you can't usefully use metas in a complex edit. */
#if 1
    BKE_sequence_tx_set_final_left(ms->parseq, ms->disp_range[0]);
    BKE_sequence_tx_set_final_right(ms->parseq, ms->disp_range[1]);
    BKE_sequence_single_fix(ms->parseq);
    BKE_sequence_calc(scene, ms->parseq);
#else
    if (BKE_sequence_test_overlap(ed->seqbasep, ms->parseq)) {
      BKE_sequence_base_shuffle(ed->seqbasep, ms->parseq, scene);
    }
#endif

    BKE_sequencer_active_set(scene, ms->parseq);

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
  /* identifiers */
  ot->name = "Toggle Meta Strip";
  ot->idname = "SEQUENCER_OT_meta_toggle";
  ot->description = "Toggle a metastrip (to edit enclosed strips)";

  /* api callbacks */
  ot->exec = sequencer_meta_toggle_exec;
  ot->poll = sequencer_edit_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* separate_meta_make operator */
static int sequencer_meta_make_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = BKE_sequencer_editing_get(scene, false);

  Sequence *seq, *seqm, *next, *last_seq = BKE_sequencer_active_get(scene);
  int channel_max = 1;

  if (BKE_sequence_base_isolated_sel_check(ed->seqbasep) == false) {
    BKE_report(op->reports, RPT_ERROR, "Please select all related strips");
    return OPERATOR_CANCELLED;
  }

  /* remove all selected from main list, and put in meta */

  seqm = BKE_sequence_alloc(ed->seqbasep, 1, 1, SEQ_TYPE_META); /* channel number set later */
  strcpy(seqm->name + 2, "MetaStrip");
  seqm->flag = SELECT;

  seq = ed->seqbasep->first;
  while (seq) {
    next = seq->next;
    if (seq != seqm && (seq->flag & SELECT)) {
      BKE_sequence_invalidate_cache_composite(scene, seq);
      channel_max = max_ii(seq->machine, channel_max);
      BLI_remlink(ed->seqbasep, seq);
      BLI_addtail(&seqm->seqbase, seq);
    }
    seq = next;
  }
  seqm->machine = last_seq ? last_seq->machine : channel_max;
  BKE_sequence_calc(scene, seqm);

  BKE_sequencer_active_set(scene, seqm);

  if (BKE_sequence_test_overlap(ed->seqbasep, seqm)) {
    BKE_sequence_base_shuffle(ed->seqbasep, seqm, scene);
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);

  BKE_sequence_base_unique_name_recursive(&scene->ed->seqbase, seqm);
  BKE_sequence_invalidate_cache_composite(scene, seqm);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_meta_make(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Make Meta Strip";
  ot->idname = "SEQUENCER_OT_meta_make";
  ot->description = "Group selected strips into a metastrip";

  /* api callbacks */
  ot->exec = sequencer_meta_make_exec;
  ot->poll = sequencer_edit_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int seq_depends_on_meta(Sequence *seq, Sequence *seqm)
{
  if (seq == seqm) {
    return 1;
  }
  else if (seq->seq1 && seq_depends_on_meta(seq->seq1, seqm)) {
    return 1;
  }
  else if (seq->seq2 && seq_depends_on_meta(seq->seq2, seqm)) {
    return 1;
  }
  else if (seq->seq3 && seq_depends_on_meta(seq->seq3, seqm)) {
    return 1;
  }
  else {
    return 0;
  }
}

/* separate_meta_make operator */
static int sequencer_meta_separate_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = BKE_sequencer_editing_get(scene, false);

  Sequence *seq, *last_seq = BKE_sequencer_active_get(scene); /* last_seq checks (ed == NULL) */

  if (last_seq == NULL || last_seq->type != SEQ_TYPE_META) {
    return OPERATOR_CANCELLED;
  }

  for (seq = last_seq->seqbase.first; seq != NULL; seq = seq->next) {
    BKE_sequence_invalidate_cache_composite(scene, seq);
  }

  BLI_movelisttolist(ed->seqbasep, &last_seq->seqbase);

  BLI_listbase_clear(&last_seq->seqbase);

  BLI_remlink(ed->seqbasep, last_seq);
  BKE_sequence_free(scene, last_seq);

  /* empty meta strip, delete all effects depending on it */
  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    if ((seq->type & SEQ_TYPE_EFFECT) && seq_depends_on_meta(seq, last_seq)) {
      seq->flag |= SEQ_FLAG_DELETE;
    }
  }

  recurs_del_seq_flag(scene, ed->seqbasep, SEQ_FLAG_DELETE, 0);

  /* test for effects and overlap
   * don't use SEQP_BEGIN since that would be recursive */
  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    if (seq->flag & SELECT) {
      seq->flag &= ~SEQ_OVERLAP;
      if (BKE_sequence_test_overlap(ed->seqbasep, seq)) {
        BKE_sequence_base_shuffle(ed->seqbasep, seq, scene);
      }
    }
  }

  BKE_sequencer_sort(scene);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_meta_separate(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "UnMeta Strip";
  ot->idname = "SEQUENCER_OT_meta_separate";
  ot->description = "Put the contents of a metastrip back in the sequencer";

  /* api callbacks */
  ot->exec = sequencer_meta_separate_exec;
  ot->poll = sequencer_edit_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* view_all operator */
static int sequencer_view_all_exec(bContext *C, wmOperator *op)
{
  ARegion *ar = CTX_wm_region(C);
  View2D *v2d = UI_view2d_fromcontext(C);
  const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

  UI_view2d_smooth_view(C, ar, &v2d->tot, smooth_viewtx);
  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_view_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "View All";
  ot->idname = "SEQUENCER_OT_view_all";
  ot->description = "View all the strips in the sequencer";

  /* api callbacks */
  ot->exec = sequencer_view_all_exec;
  ot->poll = ED_operator_sequencer_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER;
}

static int sequencer_view_frame_exec(bContext *C, wmOperator *op)
{
  const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);
  ANIM_center_frame(C, smooth_viewtx);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_view_frame(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "View Frame";
  ot->idname = "SEQUENCER_OT_view_frame";
  ot->description = "Reset viewable area to show range around current frame";

  /* api callbacks */
  ot->exec = sequencer_view_frame_exec;
  ot->poll = ED_operator_sequencer_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* view_all operator */
static int sequencer_view_all_preview_exec(bContext *C, wmOperator *UNUSED(op))
{
  bScreen *sc = CTX_wm_screen(C);
  ScrArea *area = CTX_wm_area(C);
#if 0
  ARegion *ar = CTX_wm_region(C);
  SpaceSeq *sseq = area->spacedata.first;
  Scene *scene = CTX_data_scene(C);
#endif
  View2D *v2d = UI_view2d_fromcontext(C);

  v2d->cur = v2d->tot;
  UI_view2d_curRect_validate(v2d);
  UI_view2d_sync(sc, area, v2d, V2D_LOCK_COPY);

#if 0
  /* Like zooming on an image view */
  float zoomX, zoomY;
  int width, height, imgwidth, imgheight;

  width = ar->winx;
  height = ar->winy;

  seq_reset_imageofs(sseq);

  imgwidth = (scene->r.size * scene->r.xsch) / 100;
  imgheight = (scene->r.size * scene->r.ysch) / 100;

  /* Apply aspect, dosnt need to be that accurate */
  imgwidth = (int)(imgwidth * (scene->r.xasp / scene->r.yasp));

  if (((imgwidth >= width) || (imgheight >= height)) && ((width > 0) && (height > 0))) {
    /* Find the zoom value that will fit the image in the image space */
    zoomX = ((float)width) / ((float)imgwidth);
    zoomY = ((float)height) / ((float)imgheight);
    sseq->zoom = (zoomX < zoomY) ? zoomX : zoomY;

    sseq->zoom = 1.0f / power_of_2(1 / min_ff(zoomX, zoomY));
  }
  else {
    sseq->zoom = 1.0f;
  }
#endif

  ED_area_tag_redraw(CTX_wm_area(C));
  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_view_all_preview(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "View All";
  ot->idname = "SEQUENCER_OT_view_all_preview";
  ot->description = "Zoom preview to fit in the area";

  /* api callbacks */
  ot->exec = sequencer_view_all_preview_exec;
  ot->poll = ED_operator_sequencer_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER;
}

static int sequencer_view_zoom_ratio_exec(bContext *C, wmOperator *op)
{
  RenderData *rd = &CTX_data_scene(C)->r;
  View2D *v2d = UI_view2d_fromcontext(C);

  float ratio = RNA_float_get(op->ptr, "ratio");

  float winx = (int)(rd->size * rd->xsch) / 100;
  float winy = (int)(rd->size * rd->ysch) / 100;

  float facx = BLI_rcti_size_x(&v2d->mask) / winx;
  float facy = BLI_rcti_size_y(&v2d->mask) / winy;

  BLI_rctf_resize(&v2d->cur, ceilf(winx * facx / ratio + 0.5f), ceilf(winy * facy / ratio + 0.5f));

  ED_region_tag_redraw(CTX_wm_region(C));

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_view_zoom_ratio(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Sequencer View Zoom Ratio";
  ot->idname = "SEQUENCER_OT_view_zoom_ratio";
  ot->description = "Change zoom ratio of sequencer preview";

  /* api callbacks */
  ot->exec = sequencer_view_zoom_ratio_exec;
  ot->poll = ED_operator_sequencer_active;

  /* properties */
  RNA_def_float(ot->srna,
                "ratio",
                1.0f,
                -FLT_MAX,
                FLT_MAX,
                "Ratio",
                "Zoom ratio, 1.0 is 1:1, higher is zoomed in, lower is zoomed out",
                -FLT_MAX,
                FLT_MAX);
}

#if 0
static const EnumPropertyItem view_type_items[] = {
    {SEQ_VIEW_SEQUENCE, "SEQUENCER", ICON_SEQ_SEQUENCER, "Sequencer", ""},
    {SEQ_VIEW_PREVIEW, "PREVIEW", ICON_SEQ_PREVIEW, "Image Preview", ""},
    {SEQ_VIEW_SEQUENCE_PREVIEW,
     "SEQUENCER_PREVIEW",
     ICON_SEQ_SEQUENCER,
     "Sequencer and Image Preview",
     ""},
    {0, NULL, 0, NULL, NULL},
};
#endif

/* view_all operator */
static int sequencer_view_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceSeq *sseq = (SpaceSeq *)CTX_wm_space_data(C);

  sseq->view++;
  if (sseq->view > SEQ_VIEW_SEQUENCE_PREVIEW) {
    sseq->view = SEQ_VIEW_SEQUENCE;
  }

  ED_area_tag_refresh(CTX_wm_area(C));

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_view_toggle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "View Toggle";
  ot->idname = "SEQUENCER_OT_view_toggle";
  ot->description = "Toggle between sequencer views (sequence, preview, both)";

  /* api callbacks */
  ot->exec = sequencer_view_toggle_exec;
  ot->poll = ED_operator_sequencer_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER;
}

/* view_selected operator */
static int sequencer_view_selected_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  View2D *v2d = UI_view2d_fromcontext(C);
  ARegion *ar = CTX_wm_region(C);
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  Sequence *last_seq = BKE_sequencer_active_get(scene);
  Sequence *seq;
  rctf cur_new = v2d->cur;

  int xmin = MAXFRAME * 2;
  int xmax = -MAXFRAME * 2;
  int ymin = MAXSEQ + 1;
  int ymax = 0;
  int orig_height;
  int ymid;
  int ymargin = 1;
  int xmargin = FPS;

  if (ed == NULL) {
    return OPERATOR_CANCELLED;
  }

  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    if ((seq->flag & SELECT) || (seq == last_seq)) {
      xmin = min_ii(xmin, seq->startdisp);
      xmax = max_ii(xmax, seq->enddisp);

      ymin = min_ii(ymin, seq->machine);
      ymax = max_ii(ymax, seq->machine);
    }
  }

  if (ymax != 0) {
    const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);

    xmax += xmargin;
    xmin -= xmargin;
    ymax += ymargin;
    ymin -= ymargin;

    orig_height = BLI_rctf_size_y(&cur_new);

    cur_new.xmin = xmin;
    cur_new.xmax = xmax;

    cur_new.ymin = ymin;
    cur_new.ymax = ymax;

    /* only zoom out vertically */
    if (orig_height > BLI_rctf_size_y(&cur_new)) {
      ymid = BLI_rctf_cent_y(&cur_new);

      cur_new.ymin = ymid - (orig_height / 2);
      cur_new.ymax = ymid + (orig_height / 2);
    }

    UI_view2d_smooth_view(C, ar, &cur_new, smooth_viewtx);

    return OPERATOR_FINISHED;
  }
  else {
    return OPERATOR_CANCELLED;
  }
}

void SEQUENCER_OT_view_selected(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "View Selected";
  ot->idname = "SEQUENCER_OT_view_selected";
  ot->description = "Zoom the sequencer on the selected strips";

  /* api callbacks */
  ot->exec = sequencer_view_selected_exec;
  ot->poll = ED_operator_sequencer_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER;
}

static bool strip_jump_internal(Scene *scene,
                                const short side,
                                const bool do_skip_mute,
                                const bool do_center)
{
  bool changed = false;
  int cfra = CFRA;
  int nfra = BKE_sequencer_find_next_prev_edit(scene, cfra, side, do_skip_mute, do_center, false);

  if (nfra != cfra) {
    CFRA = nfra;
    changed = true;
  }

  return changed;
}

static bool sequencer_strip_jump_poll(bContext *C)
{
  /* prevent changes during render */
  if (G.is_rendering) {
    return 0;
  }

  return sequencer_edit_poll(C);
}

/* jump frame to edit point operator */
static int sequencer_strip_jump_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  const bool next = RNA_boolean_get(op->ptr, "next");
  const bool center = RNA_boolean_get(op->ptr, "center");

  /* currently do_skip_mute is always true */
  if (!strip_jump_internal(scene, next ? SEQ_SIDE_RIGHT : SEQ_SIDE_LEFT, true, center)) {
    return OPERATOR_CANCELLED;
  }

  WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_strip_jump(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Jump to Strip";
  ot->idname = "SEQUENCER_OT_strip_jump";
  ot->description = "Move frame to previous edit point";

  /* api callbacks */
  ot->exec = sequencer_strip_jump_exec;
  ot->poll = sequencer_strip_jump_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(ot->srna, "next", true, "Next Strip", "");
  RNA_def_boolean(ot->srna, "center", true, "Use strip center", "");
}

static void swap_sequence(Scene *scene, Sequence *seqa, Sequence *seqb)
{
  int gap = seqb->startdisp - seqa->enddisp;
  int seq_a_start;
  int seq_b_start;

  seq_b_start = (seqb->start - seqb->startdisp) + seqa->startdisp;
  BKE_sequence_translate(scene, seqb, seq_b_start - seqb->start);
  BKE_sequence_calc(scene, seqb);

  seq_a_start = (seqa->start - seqa->startdisp) + seqb->enddisp + gap;
  BKE_sequence_translate(scene, seqa, seq_a_start - seqa->start);
  BKE_sequence_calc(scene, seqa);
}

#if 0
static Sequence *sequence_find_parent(Scene *scene, Sequence *child)
{
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  Sequence *parent = NULL;
  Sequence *seq;

  if (ed == NULL) {
    return NULL;
  }

  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    if ((seq != child) && seq_is_parent(seq, child)) {
      parent = seq;
      break;
    }
  }

  return parent;
}
#endif

static int sequencer_swap_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  Sequence *active_seq = BKE_sequencer_active_get(scene);
  Sequence *seq, *iseq;
  int side = RNA_enum_get(op->ptr, "side");

  if (active_seq == NULL) {
    return OPERATOR_CANCELLED;
  }

  seq = find_next_prev_sequence(scene, active_seq, side, -1);

  if (seq) {

    /* disallow effect strips */
    if (BKE_sequence_effect_get_num_inputs(seq->type) >= 1 &&
        (seq->effectdata || seq->seq1 || seq->seq2 || seq->seq3)) {
      return OPERATOR_CANCELLED;
    }
    if ((BKE_sequence_effect_get_num_inputs(active_seq->type) >= 1) &&
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

    // XXX - should be a generic function
    for (iseq = scene->ed->seqbasep->first; iseq; iseq = iseq->next) {
      if ((iseq->type & SEQ_TYPE_EFFECT) &&
          (seq_is_parent(iseq, active_seq) || seq_is_parent(iseq, seq))) {
        BKE_sequence_calc(scene, iseq);
      }
    }

    /* do this in a new loop since both effects need to be calculated first */
    for (iseq = scene->ed->seqbasep->first; iseq; iseq = iseq->next) {
      if ((iseq->type & SEQ_TYPE_EFFECT) &&
          (seq_is_parent(iseq, active_seq) || seq_is_parent(iseq, seq))) {
        /* this may now overlap */
        if (BKE_sequence_test_overlap(ed->seqbasep, iseq)) {
          BKE_sequence_base_shuffle(ed->seqbasep, iseq, scene);
        }
      }
    }

    BKE_sequencer_sort(scene);

    WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

void SEQUENCER_OT_swap(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Swap Strip";
  ot->idname = "SEQUENCER_OT_swap";
  ot->description = "Swap active strip with strip to the right or left";

  /* api callbacks */
  ot->exec = sequencer_swap_exec;
  ot->poll = sequencer_edit_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_enum(
      ot->srna, "side", prop_side_lr_types, SEQ_SIDE_RIGHT, "Side", "Side of the strip to swap");
}

static int sequencer_rendersize_exec(bContext *C, wmOperator *UNUSED(op))
{
  int retval = OPERATOR_CANCELLED;
  Scene *scene = CTX_data_scene(C);
  Sequence *active_seq = BKE_sequencer_active_get(scene);
  StripElem *se = NULL;

  if (active_seq == NULL) {
    return OPERATOR_CANCELLED;
  }

  if (active_seq->strip) {
    switch (active_seq->type) {
      case SEQ_TYPE_IMAGE:
        se = BKE_sequencer_give_stripelem(active_seq, scene->r.cfra);
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
    // prevent setting the render size if sequence values aren't initialized
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
  /* identifiers */
  ot->name = "Set Render Size";
  ot->idname = "SEQUENCER_OT_rendersize";
  ot->description = "Set render size and aspect from active sequence";

  /* api callbacks */
  ot->exec = sequencer_rendersize_exec;
  ot->poll = sequencer_edit_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
}

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
  Editing *ed = BKE_sequencer_editing_get(scene, false);

  ListBase nseqbase = {NULL, NULL};

  BKE_sequencer_free_clipboard();

  if (BKE_sequence_base_isolated_sel_check(ed->seqbasep) == false) {
    BKE_report(op->reports, RPT_ERROR, "Please select all related strips");
    return OPERATOR_CANCELLED;
  }

  BKE_sequence_base_dupli_recursive(
      scene, scene, &nseqbase, ed->seqbasep, SEQ_DUPE_UNIQUE_NAME, LIB_ID_CREATE_NO_USER_REFCOUNT);

  /* To make sure the copied strips have unique names between each other add
   * them temporarily to the end of the original seqbase. (bug 25932)
   */
  if (nseqbase.first) {
    Sequence *seq, *first_seq = nseqbase.first;
    BLI_movelisttolist(ed->seqbasep, &nseqbase);

    for (seq = first_seq; seq; seq = seq->next) {
      BKE_sequencer_recursive_apply(seq, apply_unique_name_cb, scene);
    }

    seqbase_clipboard.first = first_seq;
    seqbase_clipboard.last = ed->seqbasep->last;

    if (first_seq->prev) {
      first_seq->prev->next = NULL;
      ed->seqbasep->last = first_seq->prev;
      first_seq->prev = NULL;
    }
  }

  seqbase_clipboard_frame = scene->r.cfra;

  /* Need to remove anything that references the current scene */
  for (Sequence *seq = seqbase_clipboard.first; seq; seq = seq->next) {
    seq_copy_del_sound(scene, seq);
  }

  /* Replace datablock pointers with copies, to keep things working in case
   * datablocks get deleted or another .blend file is opened. */
  BKE_sequencer_base_clipboard_pointers_store(bmain, &seqbase_clipboard);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_copy(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy";
  ot->idname = "SEQUENCER_OT_copy";

  /* api callbacks */
  ot->exec = sequencer_copy_exec;
  ot->poll = sequencer_edit_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER;

  /* properties */
}

static int sequencer_paste_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Editing *ed = BKE_sequencer_editing_get(scene, true); /* create if needed */
  ListBase nseqbase = {NULL, NULL};
  int ofs;
  Sequence *iseq, *iseq_first;

  ED_sequencer_deselect_all(scene);
  ofs = scene->r.cfra - seqbase_clipboard_frame;

  /* Copy strips, temporarily restoring pointers to actual datablocks. This
   * must happen on the clipboard itself, so that copying does user counting
   * on the actual datablocks. */
  BKE_sequencer_base_clipboard_pointers_restore(&seqbase_clipboard, bmain);
  BKE_sequence_base_dupli_recursive(
      scene, scene, &nseqbase, &seqbase_clipboard, SEQ_DUPE_UNIQUE_NAME, 0);
  BKE_sequencer_base_clipboard_pointers_store(bmain, &seqbase_clipboard);

  /* transform pasted strips before adding */
  if (ofs) {
    for (iseq = nseqbase.first; iseq; iseq = iseq->next) {
      BKE_sequence_translate(scene, iseq, ofs);
    }
  }

  for (iseq = nseqbase.first; iseq; iseq = iseq->next) {
    BKE_sequence_sound_init(scene, iseq);
  }

  iseq_first = nseqbase.first;

  BLI_movelisttolist(ed->seqbasep, &nseqbase);

  /* make sure the pasted strips have unique names between them */
  for (iseq = iseq_first; iseq; iseq = iseq->next) {
    BKE_sequencer_recursive_apply(iseq, apply_unique_name_cb, scene);
  }

  /* ensure pasted strips don't overlap */
  for (iseq = iseq_first; iseq; iseq = iseq->next) {
    if (BKE_sequence_test_overlap(ed->seqbasep, iseq)) {
      BKE_sequence_base_shuffle(ed->seqbasep, iseq, scene);
    }
  }

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_paste(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Paste";
  ot->idname = "SEQUENCER_OT_paste";

  /* api callbacks */
  ot->exec = sequencer_paste_exec;
  ot->poll = ED_operator_sequencer_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
}

static int sequencer_swap_data_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Sequence *seq_act;
  Sequence *seq_other;
  const char *error_msg;

  if (BKE_sequencer_active_get_pair(scene, &seq_act, &seq_other) == 0) {
    BKE_report(op->reports, RPT_ERROR, "Please select two strips");
    return OPERATOR_CANCELLED;
  }

  if (BKE_sequence_swap(seq_act, seq_other, &error_msg) == 0) {
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

  BKE_sequence_calc(scene, seq_act);
  BKE_sequence_calc(scene, seq_other);

  if (seq_act->sound) {
    BKE_sound_add_scene_sound_defaults(scene, seq_act);
  }
  if (seq_other->sound) {
    BKE_sound_add_scene_sound_defaults(scene, seq_other);
  }

  BKE_sequence_invalidate_cache_raw(scene, seq_act);
  BKE_sequence_invalidate_cache_raw(scene, seq_other);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_swap_data(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Sequencer Swap Data";
  ot->idname = "SEQUENCER_OT_swap_data";
  ot->description = "Swap 2 sequencer strips";

  /* api callbacks */
  ot->exec = sequencer_swap_data_exec;
  ot->poll = ED_operator_sequencer_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
}

/* box select operator */
static int view_ghost_border_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  View2D *v2d = UI_view2d_fromcontext(C);

  rctf rect;

  /* convert coordinates of rect to 'tot' rect coordinates */
  WM_operator_properties_border_to_rctf(op, &rect);
  UI_view2d_region_to_view_rctf(v2d, &rect, &rect);

  rect.xmin /= fabsf(BLI_rctf_size_x(&v2d->tot));
  rect.ymin /= fabsf(BLI_rctf_size_y(&v2d->tot));

  rect.xmax /= fabsf(BLI_rctf_size_x(&v2d->tot));
  rect.ymax /= fabsf(BLI_rctf_size_y(&v2d->tot));

  rect.xmin += 0.5f;
  rect.xmax += 0.5f;
  rect.ymin += 0.5f;
  rect.ymax += 0.5f;

  CLAMP(rect.xmin, 0.0f, 1.0f);
  CLAMP(rect.ymin, 0.0f, 1.0f);
  CLAMP(rect.xmax, 0.0f, 1.0f);
  CLAMP(rect.ymax, 0.0f, 1.0f);

  scene->ed->over_border = rect;

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

/* ****** Box Select ****** */
void SEQUENCER_OT_view_ghost_border(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Border Offset View";
  ot->idname = "SEQUENCER_OT_view_ghost_border";
  ot->description = "Set the boundaries of the border used for offset-view";

  /* api callbacks */
  ot->invoke = WM_gesture_box_invoke;
  ot->exec = view_ghost_border_exec;
  ot->modal = WM_gesture_box_modal;
  ot->poll = sequencer_view_preview_poll;
  ot->cancel = WM_gesture_box_cancel;

  /* flags */
  ot->flag = 0;

  /* rna */
  WM_operator_properties_gesture_box(ot);
}

/* rebuild_proxy operator */

static int sequencer_rebuild_proxy_invoke(bContext *C,
                                          wmOperator *UNUSED(op),
                                          const wmEvent *UNUSED(event))
{
  seq_proxy_build_job(C);

  return OPERATOR_FINISHED;
}

static int sequencer_rebuild_proxy_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);
  struct Depsgraph *depsgraph = CTX_data_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  Sequence *seq;
  GSet *file_list;

  if (ed == NULL) {
    return OPERATOR_CANCELLED;
  }

  file_list = BLI_gset_new(BLI_ghashutil_strhash_p, BLI_ghashutil_strcmp, "file list");

  SEQP_BEGIN (ed, seq) {
    if ((seq->flag & SELECT)) {
      ListBase queue = {NULL, NULL};
      LinkData *link;
      short stop = 0, do_update;
      float progress;

      BKE_sequencer_proxy_rebuild_context(bmain, depsgraph, scene, seq, file_list, &queue);

      for (link = queue.first; link; link = link->next) {
        struct SeqIndexBuildContext *context = link->data;
        BKE_sequencer_proxy_rebuild(context, &stop, &do_update, &progress);
        BKE_sequencer_proxy_rebuild_finish(context, 0);
      }
      BKE_sequencer_free_imbuf(scene, &ed->seqbase, false);
    }
  }
  SEQ_END;

  BLI_gset_free(file_list, MEM_freeN);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_rebuild_proxy(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Rebuild Proxy and Timecode Indices";
  ot->idname = "SEQUENCER_OT_rebuild_proxy";
  ot->description = "Rebuild all selected proxies and timecode indices using the job system";

  /* api callbacks */
  ot->invoke = sequencer_rebuild_proxy_invoke;
  ot->exec = sequencer_rebuild_proxy_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER;
}

static int sequencer_enable_proxies_invoke(bContext *C,
                                           wmOperator *op,
                                           const wmEvent *UNUSED(event))
{
  return WM_operator_props_dialog_popup(C, op, 200, 100);
}

static int sequencer_enable_proxies_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  Sequence *seq;
  bool proxy_25 = RNA_boolean_get(op->ptr, "proxy_25");
  bool proxy_50 = RNA_boolean_get(op->ptr, "proxy_50");
  bool proxy_75 = RNA_boolean_get(op->ptr, "proxy_75");
  bool proxy_100 = RNA_boolean_get(op->ptr, "proxy_100");
  bool overwrite = RNA_boolean_get(op->ptr, "overwrite");
  bool turnon = true;

  if (ed == NULL || !(proxy_25 || proxy_50 || proxy_75 || proxy_100)) {
    turnon = false;
  }

  SEQP_BEGIN (ed, seq) {
    if ((seq->flag & SELECT)) {
      if (ELEM(seq->type,
               SEQ_TYPE_MOVIE,
               SEQ_TYPE_IMAGE,
               SEQ_TYPE_META,
               SEQ_TYPE_SCENE,
               SEQ_TYPE_MULTICAM)) {
        BKE_sequencer_proxy_set(seq, turnon);
        if (seq->strip->proxy == NULL) {
          continue;
        }

        if (proxy_25) {
          seq->strip->proxy->build_size_flags |= SEQ_PROXY_IMAGE_SIZE_25;
        }
        else {
          seq->strip->proxy->build_size_flags &= ~SEQ_PROXY_IMAGE_SIZE_25;
        }

        if (proxy_50) {
          seq->strip->proxy->build_size_flags |= SEQ_PROXY_IMAGE_SIZE_50;
        }
        else {
          seq->strip->proxy->build_size_flags &= ~SEQ_PROXY_IMAGE_SIZE_50;
        }

        if (proxy_75) {
          seq->strip->proxy->build_size_flags |= SEQ_PROXY_IMAGE_SIZE_75;
        }
        else {
          seq->strip->proxy->build_size_flags &= ~SEQ_PROXY_IMAGE_SIZE_75;
        }

        if (proxy_100) {
          seq->strip->proxy->build_size_flags |= SEQ_PROXY_IMAGE_SIZE_100;
        }
        else {
          seq->strip->proxy->build_size_flags &= ~SEQ_PROXY_IMAGE_SIZE_100;
        }

        if (!overwrite) {
          seq->strip->proxy->build_flags |= SEQ_PROXY_SKIP_EXISTING;
        }
        else {
          seq->strip->proxy->build_flags &= ~SEQ_PROXY_SKIP_EXISTING;
        }
      }
    }
  }
  SEQ_END;

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_enable_proxies(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Selected Strip Proxies";
  ot->idname = "SEQUENCER_OT_enable_proxies";
  ot->description = "Enable selected proxies on all selected Movie, Image and Meta strips";

  /* api callbacks */
  ot->invoke = sequencer_enable_proxies_invoke;
  ot->exec = sequencer_enable_proxies_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER;

  RNA_def_boolean(ot->srna, "proxy_25", false, "25%", "");
  RNA_def_boolean(ot->srna, "proxy_50", false, "50%", "");
  RNA_def_boolean(ot->srna, "proxy_75", false, "75%", "");
  RNA_def_boolean(ot->srna, "proxy_100", false, "100%", "");
  RNA_def_boolean(ot->srna, "overwrite", false, "Overwrite", "");
}

/* change ops */

static const EnumPropertyItem prop_change_effect_input_types[] = {
    {0, "A_B", 0, "A -> B", ""},
    {1, "B_C", 0, "B -> C", ""},
    {2, "A_C", 0, "A -> C", ""},
    {0, NULL, 0, NULL, NULL},
};

static int sequencer_change_effect_input_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  Sequence *seq = BKE_sequencer_active_get(scene);

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
  else {
    SWAP(Sequence *, *seq_1, *seq_2);
  }

  BKE_sequencer_update_changed_seq_and_deps(scene, seq, 0, 1);

  /* important else we don't get the imbuf cache flushed */
  BKE_sequencer_free_imbuf(scene, &ed->seqbase, false);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_change_effect_input(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Change Effect Input";
  ot->idname = "SEQUENCER_OT_change_effect_input";

  /* api callbacks */
  ot->exec = sequencer_change_effect_input_exec;
  ot->poll = sequencer_effect_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(
      ot->srna, "swap", prop_change_effect_input_types, 0, "Swap", "The effect inputs to swap");
}

static int sequencer_change_effect_type_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  Sequence *seq = BKE_sequencer_active_get(scene);
  const int new_type = RNA_enum_get(op->ptr, "type");

  /* free previous effect and init new effect */
  struct SeqEffectHandle sh;

  if ((seq->type & SEQ_TYPE_EFFECT) == 0) {
    return OPERATOR_CANCELLED;
  }

  /* can someone explain the logic behind only allowing to increase this,
   * copied from 2.4x - campbell */
  if (BKE_sequence_effect_get_num_inputs(seq->type) <
      BKE_sequence_effect_get_num_inputs(new_type)) {
    BKE_report(op->reports, RPT_ERROR, "New effect needs more input strips");
    return OPERATOR_CANCELLED;
  }
  else {
    sh = BKE_sequence_get_effect(seq);
    sh.free(seq, true);

    seq->type = new_type;

    sh = BKE_sequence_get_effect(seq);
    sh.init(seq);
  }

  /* update */
  BKE_sequencer_update_changed_seq_and_deps(scene, seq, 0, 1);

  /* important else we don't get the imbuf cache flushed */
  BKE_sequencer_free_imbuf(scene, &ed->seqbase, false);

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_change_effect_type(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Change Effect Type";
  ot->idname = "SEQUENCER_OT_change_effect_type";

  /* api callbacks */
  ot->exec = sequencer_change_effect_type_exec;
  ot->poll = sequencer_effect_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(ot->srna,
                          "type",
                          sequencer_prop_effect_types,
                          SEQ_TYPE_ALPHAOVER,
                          "Type",
                          "Sequencer effect type");
}

static int sequencer_change_path_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  Sequence *seq = BKE_sequencer_active_get(scene);
  const bool is_relative_path = RNA_boolean_get(op->ptr, "relative_path");
  const bool use_placeholders = RNA_boolean_get(op->ptr, "use_placeholders");
  int minframe, numdigits;

  if (seq->type == SEQ_TYPE_IMAGE) {
    char directory[FILE_MAX];
    int len;
    StripElem *se;

    /* need to find min/max frame for placeholders */
    if (use_placeholders) {
      len = sequencer_image_seq_get_minmax_frame(op, seq->sfra, &minframe, &numdigits);
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
      sequencer_image_seq_reserve_frames(op, se, len, minframe, numdigits);
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

    /* reset these else we wont see all the images */
    seq->anim_startofs = seq->anim_endofs = 0;

    /* correct start/end frames so we don't move
     * important not to set seq->len = len; allow the function to handle it */
    BKE_sequence_reload_new_file(bmain, scene, seq, true);

    BKE_sequence_calc(scene, seq);

    /* important else we don't get the imbuf cache flushed */
    BKE_sequencer_free_imbuf(scene, &ed->seqbase, false);
  }
  else if (ELEM(seq->type, SEQ_TYPE_SOUND_RAM, SEQ_TYPE_SOUND_HD)) {
    bSound *sound = seq->sound;
    if (sound == NULL) {
      return OPERATOR_CANCELLED;
    }
    char filepath[FILE_MAX];
    RNA_string_get(op->ptr, "filepath", filepath);
    BLI_strncpy(sound->name, filepath, sizeof(sound->name));
    BKE_sound_load(bmain, sound);
  }
  else {
    /* lame, set rna filepath */
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
  Sequence *seq = BKE_sequencer_active_get(scene);
  char filepath[FILE_MAX];

  BLI_join_dirfile(filepath, sizeof(filepath), seq->strip->dir, seq->strip->stripdata->name);

  RNA_string_set(op->ptr, "directory", seq->strip->dir);
  RNA_string_set(op->ptr, "filepath", filepath);

  /* set default display depending on seq type */
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
  /* identifiers */
  ot->name = "Change Data/Files";
  ot->idname = "SEQUENCER_OT_change_path";

  /* api callbacks */
  ot->exec = sequencer_change_path_exec;
  ot->invoke = sequencer_change_path_invoke;
  ot->poll = sequencer_strip_has_path_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER,
                                 FILE_SPECIAL,
                                 FILE_OPENFILE,
                                 WM_FILESEL_DIRECTORY | WM_FILESEL_RELPATH | WM_FILESEL_FILEPATH |
                                     WM_FILESEL_FILES,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_ALPHA);
  RNA_def_boolean(ot->srna,
                  "use_placeholders",
                  false,
                  "Use Placeholders",
                  "Use placeholders for missing frames of the strip");
}

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
  Editing *ed = BKE_sequencer_editing_get(scene, false);
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

  /* Avoid File write exceptions */
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

  SEQ_BEGIN (ed, seq) {
    if (seq->type == SEQ_TYPE_TEXT) {
      BLI_addtail(&text_seq, MEM_dupallocN(seq));
    }
  }
  SEQ_END;

  if (BLI_listbase_is_empty(&text_seq)) {
    BKE_report(op->reports, RPT_ERROR, "No subtitles (text strips) to export");
    return OPERATOR_CANCELLED;
  }

  BLI_listbase_sort(&text_seq, BKE_sequencer_cmp_time_startdisp);

  /* time to open and write! */
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
  return (((ed = BKE_sequencer_editing_get(CTX_data_scene(C), false)) != NULL) &&
          ((seq = ed->act_seq) != NULL) && (seq->type == SEQ_TYPE_TEXT));
}

void SEQUENCER_OT_export_subtitles(struct wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Export Subtitles";
  ot->idname = "SEQUENCER_OT_export_subtitles";
  ot->description = "Export .srt file containing text strips";

  /* api callbacks */
  ot->exec = sequencer_export_subtitles_exec;
  ot->invoke = sequencer_export_subtitles_invoke;
  ot->poll = sequencer_strip_is_text_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER,
                                 FILE_BLENDER,
                                 FILE_SAVE,
                                 WM_FILESEL_FILEPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_ALPHA);
}
