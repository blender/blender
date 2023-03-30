/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation */

/** \file
 * \ingroup gpu
 *
 * Generate shader code from the intermediate node graph.
 */

#pragma once

#include "GPU_material.h"
#include "GPU_shader.h"

#ifdef __cplusplus
extern "C" {
#endif

struct GPUNodeGraph;

typedef struct GPUPass GPUPass;

/* Pass */

GPUPass *GPU_generate_pass(GPUMaterial *material,
                           struct GPUNodeGraph *graph,
                           GPUCodegenCallbackFn finalize_source_cb,
                           void *thunk,
                           bool optimize_graph);
GPUShader *GPU_pass_shader_get(GPUPass *pass);
bool GPU_pass_compile(GPUPass *pass, const char *shname);
void GPU_pass_release(GPUPass *pass);
bool GPU_pass_should_optimize(GPUPass *pass);

/* Module */

void gpu_codegen_init(void);
void gpu_codegen_exit(void);

#ifdef __cplusplus
}
#endif
