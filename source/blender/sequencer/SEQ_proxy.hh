/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "DNA_space_enums.h"

#include "BLI_function_ref.hh"
#include "BLI_set.hh"
#include "BLI_vector.hh"

#include "IMB_imbuf_enums.h"

namespace blender {

struct Main;
struct Scene;
struct Strip;
struct bContext;
struct wmJob;
struct wmJobWorkerStatus;

namespace seq {

struct ProxyBuildContext;
struct RenderData;

/*
 * Initializes proxy (re)build for the given input strip.
 * The actual proxy builders, if needed, are added to the
 * `r_queue` output vector (there can be more than one
 * for multi-view videos/images).
 */
bool proxy_build_start(Main *bmain,
                       Scene *scene,
                       Strip *strip,
                       Set<std::string> *processed_paths,
                       bool build_only_on_bad_performance,
                       Vector<ProxyBuildContext *> &r_queue);

/* Processes a proxy (re)build request in given `context`. */
void proxy_build_process(ProxyBuildContext *context,
                         const bool *should_stop,
                         bool *has_updated,
                         FunctionRef<void(float progress)> set_progress_fn);

/* Cleans up and deallocates the proxy build context. */
void proxy_build_finish(ProxyBuildContext *context);

void proxy_set(Strip *strip, bool value);
bool can_use_proxy(const RenderData *context, const Strip *strip, IMB_Proxy_Size psize);
IMB_Proxy_Size rendersize_to_proxysize(eSpaceSeq_Proxy_RenderSize render_size);
float rendersize_to_scale_factor(eSpaceSeq_Proxy_RenderSize render_size);

struct ProxyJob {
  Main *main = nullptr;
  Scene *scene = nullptr;
  Vector<ProxyBuildContext *> queue;
  int stop = 0;
};

wmJob *ED_seq_proxy_wm_job_get(const bContext *C);
ProxyJob *ED_seq_proxy_job_get(const bContext *C, wmJob *wm_job);

}  // namespace seq
}  // namespace blender
