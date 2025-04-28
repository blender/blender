/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "DNA_listBase.h"

#include "BLI_set.hh"

struct Depsgraph;
struct ListBase;
struct Main;
struct Scene;
struct Strip;
struct bContext;
struct wmJob;
struct wmJobWorkerStatus;

namespace blender::seq {

struct IndexBuildContext;
struct RenderData;

bool proxy_rebuild_context(Main *bmain,
                           Depsgraph *depsgraph,
                           Scene *scene,
                           Strip *strip,
                           blender::Set<std::string> *processed_paths,
                           ListBase *queue,
                           bool build_only_on_bad_performance);
void proxy_rebuild(IndexBuildContext *context, wmJobWorkerStatus *worker_status);
void proxy_rebuild_finish(IndexBuildContext *context, bool stop);
void proxy_set(Strip *strip, bool value);
bool can_use_proxy(const RenderData *context, const Strip *strip, int psize);
int rendersize_to_proxysize(int render_size);
double rendersize_to_scale_factor(int render_size);

struct ProxyJob {
  Main *main;
  Depsgraph *depsgraph;
  Scene *scene;
  ListBase queue;
  int stop;
};

wmJob *ED_seq_proxy_wm_job_get(const bContext *C);
ProxyJob *ED_seq_proxy_job_get(const bContext *C, wmJob *wm_job);

}  // namespace blender::seq
