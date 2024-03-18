/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"

#include "DNA_scene_types.h"

#include "BKE_context.hh"
#include "BKE_global.h"
#include "BKE_main.hh"
#include "BKE_report.h"

#include "SEQ_iterator.hh"
#include "SEQ_proxy.hh"
#include "SEQ_relations.hh"
#include "SEQ_sequencer.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_define.hh"

/* For menu, popup, icons, etc. */
#include "ED_screen.hh"

/* Own include. */
#include "sequencer_intern.hh"

/* -------------------------------------------------------------------- */
/** \name Rebuild Proxy and Timecode Indices Operator
 * \{ */

static void seq_proxy_build_job(const bContext *C, ReportList *reports)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene);
  ScrArea *area = CTX_wm_area(C);

  if (ed == nullptr) {
    return;
  }

  wmJob *wm_job = ED_seq_proxy_wm_job_get(C);
  ProxyJob *pj = ED_seq_proxy_job_get(C, wm_job);

  GSet *file_list = BLI_gset_new(BLI_ghashutil_strhash_p, BLI_ghashutil_strcmp, "file list");
  bool selected = false; /* Check for no selected strips */

  LISTBASE_FOREACH (Sequence *, seq, SEQ_active_seqbase_get(ed)) {
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
        pj->main, pj->depsgraph, pj->scene, seq, file_list, &pj->queue, false);

    if (!success && (seq->strip->proxy->build_flags & SEQ_PROXY_SKIP_EXISTING) != 0) {
      BKE_reportf(reports, RPT_WARNING, "Overwrite is not checked for %s, skipping", seq->name);
    }
  }

  BLI_gset_free(file_list, MEM_freeN);

  if (!selected) {
    BKE_reportf(reports, RPT_WARNING, "Select movie or image strips");
    return;
  }

  if (!WM_jobs_is_running(wm_job)) {
    G.is_break = false;
    WM_jobs_start(CTX_wm_manager(C), wm_job);
  }

  ED_area_tag_redraw(area);
}

static int sequencer_rebuild_proxy_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  seq_proxy_build_job(C, op->reports);

  return OPERATOR_FINISHED;
}

static int sequencer_rebuild_proxy_exec(bContext *C, wmOperator * /*o*/)
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene);
  GSet *file_list;

  if (ed == nullptr) {
    return OPERATOR_CANCELLED;
  }

  file_list = BLI_gset_new(BLI_ghashutil_strhash_p, BLI_ghashutil_strcmp, "file list");

  LISTBASE_FOREACH (Sequence *, seq, SEQ_active_seqbase_get(ed)) {
    if (seq->flag & SELECT) {
      ListBase queue = {nullptr, nullptr};

      SEQ_proxy_rebuild_context(bmain, depsgraph, scene, seq, file_list, &queue, false);

      wmJobWorkerStatus worker_status = {};
      LISTBASE_FOREACH (LinkData *, link, &queue) {
        SeqIndexBuildContext *context = static_cast<SeqIndexBuildContext *>(link->data);
        SEQ_proxy_rebuild(context, &worker_status);
        SEQ_proxy_rebuild_finish(context, false);
      }
      SEQ_relations_free_imbuf(scene, &ed->seqbase, false);
    }
  }

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

static int sequencer_enable_proxies_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  return WM_operator_props_dialog_popup(C, op, 200);
}

static int sequencer_enable_proxies_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene);
  bool proxy_25 = RNA_boolean_get(op->ptr, "proxy_25");
  bool proxy_50 = RNA_boolean_get(op->ptr, "proxy_50");
  bool proxy_75 = RNA_boolean_get(op->ptr, "proxy_75");
  bool proxy_100 = RNA_boolean_get(op->ptr, "proxy_100");
  bool overwrite = RNA_boolean_get(op->ptr, "overwrite");
  bool turnon = true;

  if (ed == nullptr || !(proxy_25 || proxy_50 || proxy_75 || proxy_100)) {
    turnon = false;
  }

  LISTBASE_FOREACH (Sequence *, seq, SEQ_active_seqbase_get(ed)) {
    if (seq->flag & SELECT) {
      if (ELEM(seq->type, SEQ_TYPE_MOVIE, SEQ_TYPE_IMAGE)) {
        SEQ_proxy_set(seq, turnon);
        if (seq->strip->proxy == nullptr) {
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
