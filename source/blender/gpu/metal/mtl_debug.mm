/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Debug features of OpenGL.
 */

#include "BLI_compiler_attrs.h"
#include "BLI_string.h"
#include "BLI_system.h"
#include "BLI_utildefines.h"

#include "BKE_global.hh"

#include "GPU_debug.hh"
#include "GPU_platform.hh"

#include "mtl_context.hh"
#include "mtl_debug.hh"

#include "CLG_log.h"

#include "gpu_profile_report.hh"

#include <utility>

namespace blender::gpu::debug {

CLG_LogRef LOG = {"gpu.debug.metal"};

void mtl_debug_init()
{
  CLOG_ENSURE(&LOG);
}

}  // namespace blender::gpu::debug

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Debug Groups
 *
 * Useful for debugging through XCode GPU Debugger. This ensures all the API calls grouped into
 * "passes".
 * \{ */

void MTLContext::debug_group_begin(const char *name, int /*index*/)
{
  /* Note: Debug groups are pushed JIT to the command encoders. This avoids splitting and nesting
   * the command encoders which would make the debugging experience through Xcode more confusing.
   * See #unfold_pending_debug_groups(). */

  if (!G.profile_gpu) {
    return;
  }

  ScopeTimings timings = {};
  timings.name = name;
  timings.finished = false;
  timings.cpu_start = ScopeTimings::Clock::now();

  scope_timings.append(timings);
}

void MTLContext::debug_group_end()
{
  /* Note: Debug groups are pushed JIT to the command encoders. This avoids splitting and nesting
   * the command encoders which would make the debugging experience through Xcode more confusing.
   * See #unfold_pending_debug_groups(). */

  if (!G.profile_gpu) {
    return;
  }

  for (int i = scope_timings.size() - 1; i >= 0; i--) {
    ScopeTimings &query = scope_timings[i];
    if (!query.finished) {
      query.finished = true;
      query.cpu_end = ScopeTimings::Clock::now();
      break;
    }
    if (i == 0) {
      CLOG_ERROR(&debug::LOG, "Profile GPU error: Extra GPU_debug_group_end() call.");
    }
  }
}

MTLContext::ScopeTimings::TimePoint MTLContext::ScopeTimings::epoch =
    MTLContext::ScopeTimings::Clock::now();

void MTLContext::process_frame_timings()
{
  if (!G.profile_gpu) {
    return;
  }

  Vector<ScopeTimings> &queries = scope_timings;

  bool frame_is_valid = !queries.is_empty();

  for (int i = queries.size() - 1; i >= 0; i--) {
    if (!queries[i].finished) {
      frame_is_valid = false;
      CLOG_ERROR(&debug::LOG, "Profile GPU error: Missing GPU_debug_group_end() call");
    }
    break;
  }

  if (!frame_is_valid) {
    return;
  }

  for (ScopeTimings &query : queries) {
    ScopeTimings::Nanoseconds begin = query.cpu_start - ScopeTimings::epoch;
    ScopeTimings::Nanoseconds end = query.cpu_end - ScopeTimings::epoch;
    ProfileReport::get().add_group_cpu(query.name, begin.count(), end.count());
  }

  queries.clear();
}

bool MTLContext::debug_capture_begin(const char * /*title*/)
{
  MTLCaptureManager *capture_manager = [MTLCaptureManager sharedCaptureManager];
  if (!capture_manager) {
    /* Early exit if frame capture is disabled. */
    return false;
  }
  MTLCaptureDescriptor *capture_descriptor = [[MTLCaptureDescriptor alloc] init];
  capture_descriptor.captureObject = this->device;
  NSError *error;
  if (![capture_manager startCaptureWithDescriptor:capture_descriptor error:&error]) {
    NSLog(@"Failed to start Metal frame capture, error %@", error);
    return false;
  }
  return true;
}

void MTLContext::debug_capture_end()
{
  MTLCaptureManager *capture_manager = [MTLCaptureManager sharedCaptureManager];
  if (!capture_manager) {
    /* Early exit if frame capture is disabled. */
    return;
  }
  [capture_manager stopCapture];
}

void *MTLContext::debug_capture_scope_create(const char *name)
{
  /* Create a capture scope visible to xCode Metal Frame capture utility. */
  MTLCaptureManager *capture_manager = [MTLCaptureManager sharedCaptureManager];
  if (!capture_manager) {
    /* Early exit if frame capture is disabled. */
    return nullptr;
  }
  id<MTLCaptureScope> capture_scope = [capture_manager newCaptureScopeWithDevice:this->device];
  capture_scope.label = [NSString stringWithUTF8String:name];
  [capture_scope retain];

  return reinterpret_cast<void *>(capture_scope);
}

bool MTLContext::debug_capture_scope_begin(void *scope)
{
  if ([((id<MTLCaptureScope>)scope).label isEqual:@(G.gpu_debug_scope_name)]) {
    debug_capture_begin("Auto Capture");
  }

  /* Declare opening boundary of scope.
   * When scope is selected for capture, GPU commands between begin/end scope will be captured. */
  [(id<MTLCaptureScope>)scope beginScope];

  MTLCaptureManager *capture_manager = [MTLCaptureManager sharedCaptureManager];
  return [capture_manager isCapturing];
}

void MTLContext::debug_capture_scope_end(void *scope)
{
  if ([((id<MTLCaptureScope>)scope).label isEqual:@(G.gpu_debug_scope_name)]) {
    debug_capture_end();
  }

  [(id<MTLCaptureScope>)scope endScope];
}

/** \} */

}  // namespace blender::gpu
