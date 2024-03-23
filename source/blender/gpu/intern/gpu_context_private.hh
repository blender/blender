/* SPDX-FileCopyrightText: 2016 by Mike Erwin. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * This interface allow GPU to manage GL objects for multiple context and threads.
 */

#pragma once

#include "MEM_guardedalloc.h"

#include "GPU_context.hh"

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
  Shader *shader = nullptr;
  FrameBuffer *active_fb = nullptr;
  GPUMatrixState *matrix_state = nullptr;
  StateManager *state_manager = nullptr;
  Immediate *imm = nullptr;

  /**
   * All 4 window frame-buffers.
   * None of them are valid in an off-screen context.
   * Right frame-buffers are only available if using stereo rendering.
   * Front frame-buffers contains (in principle, but not always) the last frame color.
   * Default frame-buffer is back_left.
   */
  FrameBuffer *back_left = nullptr;
  FrameBuffer *front_left = nullptr;
  FrameBuffer *back_right = nullptr;
  FrameBuffer *front_right = nullptr;

  DebugStack debug_stack;
  bool debug_is_capturing = false;

  /* GPUContext counter used to assign a unique ID to each GPUContext.
   * NOTE(Metal): This is required by the Metal Backend, as a bug exists in the global OS shader
   * cache wherein compilation of identical source from two distinct threads can result in an
   * invalid cache collision, result in a broken shader object. Appending the unique context ID
   * onto compiled sources ensures the source hashes are different. */
  static int context_counter;
  int context_id = 0;

 protected:
  /** Thread on which this context is active. */
  pthread_t thread_;
  bool is_active_;
  /** Avoid including GHOST headers. Can be nullptr for off-screen contexts. */
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

  virtual void memory_statistics_get(int *r_total_mem, int *r_free_mem) = 0;

  virtual void debug_group_begin(const char *, int){};
  virtual void debug_group_end(){};

  /* Returns true if capture successfully started. */
  virtual bool debug_capture_begin(const char *title) = 0;
  virtual void debug_capture_end() = 0;
  virtual void *debug_capture_scope_create(const char *name) = 0;
  virtual bool debug_capture_scope_begin(void *scope) = 0;
  virtual void debug_capture_scope_end(void *scope) = 0;

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
