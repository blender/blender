/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2016 by Mike Erwin. All rights reserved. */

/** \file
 * \ingroup gpu
 *
 * This interface allow GPU to manage VAOs for multiple context and threads.
 */

#pragma once

#include "GPU_batch.h"
#include "GPU_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum eGPUBackendType {
  GPU_BACKEND_NONE = 0,
  GPU_BACKEND_OPENGL,
} eGPUBackendType;

void GPU_backend_init(eGPUBackendType backend);
void GPU_backend_exit(void);

eGPUBackendType GPU_backend_get_type(void);

/** Opaque type hiding blender::gpu::Context. */
typedef struct GPUContext GPUContext;

GPUContext *GPU_context_create(void *ghost_window);
/**
 * To be called after #GPU_context_active_set(ctx_to_destroy).
 */
void GPU_context_discard(GPUContext *);

/**
 * Ctx can be NULL.
 */
void GPU_context_active_set(GPUContext *);
GPUContext *GPU_context_active_get(void);

/* Legacy GPU (Intel HD4000 series) do not support sharing GPU objects between GPU
 * contexts. EEVEE/Workbench can create different contexts for image/preview rendering, baking or
 * compiling. When a legacy GPU is detected (`GPU_use_main_context_workaround()`) any worker
 * threads should use the draw manager opengl context and make sure that they are the only one
 * using it by locking the main context using these two functions. */
void GPU_context_main_lock(void);
void GPU_context_main_unlock(void);

#ifdef __cplusplus
}
#endif
