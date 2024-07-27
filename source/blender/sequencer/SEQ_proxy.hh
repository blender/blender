/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

struct Depsgraph;
struct GSet;
struct ListBase;
struct ListBase;
struct Main;
struct ProxyJob;
struct Scene;
struct SeqIndexBuildContext;
struct SeqRenderData;
struct Sequence;
struct wmJob;
struct wmJobWorkerStatus;

bool SEQ_proxy_rebuild_context(Main *bmain,
                               Depsgraph *depsgraph,
                               Scene *scene,
                               Sequence *seq,
                               GSet *file_list,
                               ListBase *queue,
                               bool build_only_on_bad_performance);
void SEQ_proxy_rebuild(SeqIndexBuildContext *context, wmJobWorkerStatus *worker_status);
void SEQ_proxy_rebuild_finish(SeqIndexBuildContext *context, bool stop);
void SEQ_proxy_set(Sequence *seq, bool value);
bool SEQ_can_use_proxy(const SeqRenderData *context, Sequence *seq, int psize);
int SEQ_rendersize_to_proxysize(int render_size);
double SEQ_rendersize_to_scale_factor(int render_size);

struct ProxyJob {
  Main *main;
  Depsgraph *depsgraph;
  Scene *scene;
  ListBase queue;
  int stop;
};

wmJob *ED_seq_proxy_wm_job_get(const bContext *C);
ProxyJob *ED_seq_proxy_job_get(const bContext *C, wmJob *wm_job);
