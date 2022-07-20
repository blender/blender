/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2016 by Mike Erwin. All rights reserved. */

/** \file
 * \ingroup gpu
 *
 * This interface allow GPU to manage GL objects for multiple context and threads.
 */

#pragma once

#include "MEM_guardedalloc.h"

#include "GPU_context.h"

#include "gpu_debug_private.hh"
#include "gpu_framebuffer_private.hh"
#include "gpu_immediate_private.hh"
#include "gpu_shader_private.hh"
#include "gpu_state_private.hh"

#include <pthread.h>

struct GPUMatrixState;

namespace blender::gpu {

class Context {
 public:
  /** State management */
  Shader *shader = NULL;
  FrameBuffer *active_fb = NULL;
  GPUMatrixState *matrix_state = NULL;
  StateManager *state_manager = NULL;
  Immediate *imm = NULL;

  /**
   * All 4 window frame-buffers.
   * None of them are valid in an off-screen context.
   * Right frame-buffers are only available if using stereo rendering.
   * Front frame-buffers contains (in principle, but not always) the last frame color.
   * Default frame-buffer is back_left.
   */
  FrameBuffer *back_left = NULL;
  FrameBuffer *front_left = NULL;
  FrameBuffer *back_right = NULL;
  FrameBuffer *front_right = NULL;

  DebugStack debug_stack;

 protected:
  /** Thread on which this context is active. */
  pthread_t thread_;
  bool is_active_;
  /** Avoid including GHOST headers. Can be NULL for off-screen contexts. */
  void *ghost_window_;

 public:
  Context();
  virtual ~Context();

  static Context *get();

  virtual void activate() = 0;
  virtual void deactivate() = 0;
  virtual void begin_frame() = 0;
  virtual void end_frame() = 0;

  /* Will push all pending commands to the GPU. */
  virtual void flush() = 0;
  /* Will wait until the GPU has finished executing all command. */
  virtual void finish() = 0;

  virtual void memory_statistics_get(int *total_mem, int *free_mem) = 0;

  virtual void debug_group_begin(const char *, int){};
  virtual void debug_group_end(){};

  bool is_active_on_thread();
};

/* Syntactic sugar. */
static inline GPUContext *wrap(Context *ctx)
{
  return reinterpret_cast<GPUContext *>(ctx);
}
static inline Context *unwrap(GPUContext *ctx)
{
  return reinterpret_cast<Context *>(ctx);
}
static inline const Context *unwrap(const GPUContext *ctx)
{
  return reinterpret_cast<const Context *>(ctx);
}

}  // namespace blender::gpu
