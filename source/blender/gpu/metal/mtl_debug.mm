/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Debug features of OpenGL.
 */

#include "BLI_compiler_attrs.h"
#include "BLI_string.h"
#include "BLI_system.h"
#include "BLI_utildefines.h"

#include "BKE_global.h"

#include "GPU_debug.h"
#include "GPU_platform.h"

#include "mtl_context.hh"
#include "mtl_debug.hh"

#include "CLG_log.h"

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

void MTLContext::debug_group_begin(const char *name, int index)
{
  if (G.debug & G_DEBUG_GPU) {
    this->main_command_buffer.push_debug_group(name, index);
  }
}

void MTLContext::debug_group_end()
{
  if (G.debug & G_DEBUG_GPU) {
    this->main_command_buffer.pop_debug_group();
  }
}

bool MTLContext::debug_capture_begin()
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
  /* Declare opening boundary of scope.
   * When scope is selected for capture, GPU commands between begin/end scope will be captured. */
  [(id<MTLCaptureScope>)scope beginScope];

  MTLCaptureManager *capture_manager = [MTLCaptureManager sharedCaptureManager];
  return [capture_manager isCapturing];
}

void MTLContext::debug_capture_scope_end(void *scope)
{
  [(id<MTLCaptureScope>)scope endScope];
}

/** \} */

}  // namespace blender::gpu
