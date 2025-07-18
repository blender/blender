/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2003-2009 Blender Authors
 * SPDX-FileCopyrightText: 2005-2006 Peter Schlaile <peter [at] schlaile [dot] de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BLI_listbase.h"

#include "BKE_context.hh"

#include "SEQ_proxy.hh"
#include "SEQ_relations.hh"
#include "SEQ_sequencer.hh"

#include "WM_api.hh"
#include "WM_types.hh"

namespace blender::seq {

static void proxy_freejob(void *pjv)
{
  ProxyJob *pj = static_cast<ProxyJob *>(pjv);

  BLI_freelistN(&pj->queue);

  MEM_freeN(pj);
}

/* Only this runs inside thread. */
static void proxy_startjob(void *pjv, wmJobWorkerStatus *worker_status)
{
  ProxyJob *pj = static_cast<ProxyJob *>(pjv);

  LISTBASE_FOREACH (LinkData *, link, &pj->queue) {
    IndexBuildContext *context = static_cast<IndexBuildContext *>(link->data);

    proxy_rebuild(context, worker_status);

    if (worker_status->stop) {
      pj->stop = true;
      fprintf(stderr, "Canceling proxy rebuild on users request...\n");
      break;
    }
  }
}

static void proxy_endjob(void *pjv)
{
  ProxyJob *pj = static_cast<ProxyJob *>(pjv);
  Editing *ed = editing_get(pj->scene);

  LISTBASE_FOREACH (LinkData *, link, &pj->queue) {
    proxy_rebuild_finish(static_cast<IndexBuildContext *>(link->data), pj->stop);
  }

  relations_free_imbuf(pj->scene, &ed->seqbase, false);

  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, pj->scene);
}

ProxyJob *ED_seq_proxy_job_get(const bContext *C, wmJob *wm_job)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  ProxyJob *pj = static_cast<ProxyJob *>(WM_jobs_customdata_get(wm_job));
  if (!pj) {
    pj = MEM_callocN<ProxyJob>("proxy rebuild job");
    pj->depsgraph = depsgraph;
    pj->scene = scene;
    pj->main = CTX_data_main(C);
    WM_jobs_customdata_set(wm_job, pj, proxy_freejob);
    WM_jobs_timer(wm_job, 0.1, NC_SCENE | ND_SEQUENCER, NC_SCENE | ND_SEQUENCER);
    WM_jobs_callbacks(wm_job, proxy_startjob, nullptr, nullptr, proxy_endjob);
  }
  return pj;
}

wmJob *ED_seq_proxy_wm_job_get(const bContext *C)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  wmJob *wm_job = WM_jobs_get(CTX_wm_manager(C),
                              CTX_wm_window(C),
                              scene,
                              "Building Proxies",
                              WM_JOB_PROGRESS,
                              WM_JOB_TYPE_SEQ_BUILD_PROXY);
  return wm_job;
}

}  // namespace blender::seq
