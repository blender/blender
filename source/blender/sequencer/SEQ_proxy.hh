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
struct Scene;
struct SeqIndexBuildContext;
struct SeqRenderData;
struct Sequence;
struct wmJobWorkerStatus;

bool SEQ_proxy_rebuild_context(struct Main *bmain,
                               struct Depsgraph *depsgraph,
                               struct Scene *scene,
                               struct Sequence *seq,
                               struct GSet *file_list,
                               struct ListBase *queue,
                               bool build_only_on_bad_performance);
void SEQ_proxy_rebuild(struct SeqIndexBuildContext *context, wmJobWorkerStatus *worker_status);
void SEQ_proxy_rebuild_finish(struct SeqIndexBuildContext *context, bool stop);
void SEQ_proxy_set(struct Sequence *seq, bool value);
bool SEQ_can_use_proxy(const struct SeqRenderData *context, struct Sequence *seq, int psize);
int SEQ_rendersize_to_proxysize(int render_size);
double SEQ_rendersize_to_scale_factor(int size);

typedef struct ProxyBuildJob {
  struct Main *main;
  struct Depsgraph *depsgraph;
  struct Scene *scene;
  struct ListBase queue;
  int stop;
} ProxyJob;

struct wmJob *ED_seq_proxy_wm_job_get(const struct bContext *C);
ProxyJob *ED_seq_proxy_job_get(const struct bContext *C, struct wmJob *wm_job);
