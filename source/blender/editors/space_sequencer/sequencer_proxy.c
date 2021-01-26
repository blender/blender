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

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_timecode.h"

#include "DNA_scene_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "SEQ_iterator.h"
#include "SEQ_proxy.h"
#include "SEQ_relations.h"
#include "SEQ_sequencer.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_define.h"

/* For menu, popup, icons, etc. */
#include "ED_screen.h"

/* Own include. */
#include "sequencer_intern.h"

/*--------------------------------------------------------------------*/
/** \name Proxy Job Manager
 * \{ */

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

/* Only this runs inside thread. */
static void proxy_startjob(void *pjv, short *stop, short *do_update, float *progress)
{
  ProxyJob *pj = pjv;
  LinkData *link;

  for (link = pj->queue.first; link; link = link->next) {
    struct SeqIndexBuildContext *context = link->data;

    SEQ_proxy_rebuild(context, stop, do_update, progress);

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
  Editing *ed = SEQ_editing_get(pj->scene, false);
  LinkData *link;

  for (link = pj->queue.first; link; link = link->next) {
    SEQ_proxy_rebuild_finish(link->data, pj->stop);
  }

  SEQ_relations_free_imbuf(pj->scene, &ed->seqbase, false);

  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, pj->scene);
}

static void seq_proxy_build_job(const bContext *C, ReportList *reports)
{
  wmJob *wm_job;
  ProxyJob *pj;
  struct Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene, false);
  ScrArea *area = CTX_wm_area(C);
  Sequence *seq;
  GSet *file_list;

  if (ed == NULL) {
    return;
  }

  wm_job = WM_jobs_get(CTX_wm_manager(C),
                       CTX_wm_window(C),
                       scene,
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
  bool selected = false; /* Check for no selected strips */

  SEQ_CURRENT_BEGIN (ed, seq) {
    if (!ELEM(seq->type, SEQ_TYPE_MOVIE, SEQ_TYPE_IMAGE) || (seq->flag & SELECT) == 0) {
      continue;
    }

    selected = true;
    if (!(seq->flag & SEQ_USE_PROXY)) {
      BKE_reportf(reports, RPT_WARNING, "Proxy is not enabled for %s, skipping", seq->name);
      continue;
    }
    if (seq->strip->proxy->build_size_flags == 0) {
      BKE_reportf(reports, RPT_WARNING, "Resolution is not selected for %s, skipping", seq->name);
      continue;
    }

    bool success = SEQ_proxy_rebuild_context(
        pj->main, pj->depsgraph, pj->scene, seq, file_list, &pj->queue);

    if (!success && (seq->strip->proxy->build_flags & SEQ_PROXY_SKIP_EXISTING) != 0) {
      BKE_reportf(reports, RPT_WARNING, "Overwrite is not checked for %s, skipping", seq->name);
    }
  }
  SEQ_CURRENT_END;

  BLI_gset_free(file_list, MEM_freeN);

  if (!selected) {
    BKE_reportf(reports, RPT_WARNING, "Select movie or image strips");
    return;
  }

  if (selected && !WM_jobs_is_running(wm_job)) {
    G.is_break = false;
    WM_jobs_start(CTX_wm_manager(C), wm_job);
  }

  ED_area_tag_redraw(area);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Rebuild Proxy and Timecode Indices Operator
 * \{ */

static int sequencer_rebuild_proxy_invoke(bContext *C,
                                          wmOperator *op,
                                          const wmEvent *UNUSED(event))
{
  seq_proxy_build_job(C, op->reports);

  return OPERATOR_FINISHED;
}

static int sequencer_rebuild_proxy_exec(bContext *C, wmOperator *UNUSED(op))
{
  Main *bmain = CTX_data_main(C);
  struct Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene, false);
  Sequence *seq;
  GSet *file_list;

  if (ed == NULL) {
    return OPERATOR_CANCELLED;
  }

  file_list = BLI_gset_new(BLI_ghashutil_strhash_p, BLI_ghashutil_strcmp, "file list");

  SEQ_CURRENT_BEGIN (ed, seq) {
    if ((seq->flag & SELECT)) {
      ListBase queue = {NULL, NULL};
      LinkData *link;
      short stop = 0, do_update;
      float progress;

      SEQ_proxy_rebuild_context(bmain, depsgraph, scene, seq, file_list, &queue);

      for (link = queue.first; link; link = link->next) {
        struct SeqIndexBuildContext *context = link->data;
        SEQ_proxy_rebuild(context, &stop, &do_update, &progress);
        SEQ_proxy_rebuild_finish(context, 0);
      }
      SEQ_relations_free_imbuf(scene, &ed->seqbase, false);
    }
  }
  SEQ_CURRENT_END;

  BLI_gset_free(file_list, MEM_freeN);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_rebuild_proxy(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Rebuild Proxy and Timecode Indices";
  ot->idname = "SEQUENCER_OT_rebuild_proxy";
  ot->description = "Rebuild all selected proxies and timecode indices using the job system";

  /* Api callbacks. */
  ot->invoke = sequencer_rebuild_proxy_invoke;
  ot->exec = sequencer_rebuild_proxy_exec;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Set Selected Strip Proxies Operator
 * \{ */

static int sequencer_enable_proxies_invoke(bContext *C,
                                           wmOperator *op,
                                           const wmEvent *UNUSED(event))
{
  return WM_operator_props_dialog_popup(C, op, 200);
}

static int sequencer_enable_proxies_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene, false);
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

  SEQ_CURRENT_BEGIN (ed, seq) {
    if ((seq->flag & SELECT)) {
      if (ELEM(seq->type, SEQ_TYPE_MOVIE, SEQ_TYPE_IMAGE)) {
        SEQ_proxy_set(seq, turnon);
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
  SEQ_CURRENT_END;

  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

void SEQUENCER_OT_enable_proxies(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Set Selected Strip Proxies";
  ot->idname = "SEQUENCER_OT_enable_proxies";
  ot->description = "Enable selected proxies on all selected Movie and Image strips";

  /* Api callbacks. */
  ot->invoke = sequencer_enable_proxies_invoke;
  ot->exec = sequencer_enable_proxies_exec;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER;

  RNA_def_boolean(ot->srna, "proxy_25", false, "25%", "");
  RNA_def_boolean(ot->srna, "proxy_50", false, "50%", "");
  RNA_def_boolean(ot->srna, "proxy_75", false, "75%", "");
  RNA_def_boolean(ot->srna, "proxy_100", false, "100%", "");
  RNA_def_boolean(ot->srna, "overwrite", false, "Overwrite", "");
}

/** \} */
