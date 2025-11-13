/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "DNA_listBase.h"
#include "DNA_space_enums.h"

#include "BLI_set.hh"

#include "IMB_imbuf_enums.h"

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
                           Set<std::string> *processed_paths,
                           ListBase *queue,
                           bool build_only_on_bad_performance);
void proxy_rebuild(IndexBuildContext *context, wmJobWorkerStatus *worker_status);
void proxy_rebuild_finish(IndexBuildContext *context, bool stop);
void proxy_set(Strip *strip, bool value);
bool can_use_proxy(const RenderData *context, const Strip *strip, IMB_Proxy_Size psize);
IMB_Proxy_Size rendersize_to_proxysize(eSpaceSeq_Proxy_RenderSize render_size);
float rendersize_to_scale_factor(eSpaceSeq_Proxy_RenderSize render_size);

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
