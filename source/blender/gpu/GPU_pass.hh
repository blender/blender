/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Generate and cache shaders generated from the intermediate node graph.
 */

#pragma once

#include "GPU_material.hh"
#include "GPU_shader.hh"

struct GPUNodeGraph;

struct GPUPass;

enum GPUPassStatus {
  GPU_PASS_FAILED = 0,
  GPU_PASS_QUEUED,
  GPU_PASS_SUCCESS,
};

GPUPass *GPU_generate_pass(GPUMaterial *material,
                           GPUNodeGraph *graph,
                           const char *debug_name,
                           eGPUMaterialEngine engine,
                           bool deferred_compilation,
                           GPUCodegenCallbackFn finalize_source_cb,
                           void *thunk,
                           bool optimize_graph);

GPUPassStatus GPU_pass_status(GPUPass *pass);
bool GPU_pass_should_optimize(GPUPass *pass);
void GPU_pass_ensure_its_ready(GPUPass *pass);
blender::gpu::Shader *GPU_pass_shader_get(GPUPass *pass);
void GPU_pass_acquire(GPUPass *pass);
void GPU_pass_release(GPUPass *pass);

uint64_t GPU_pass_global_compilation_count();
uint64_t GPU_pass_compilation_timestamp(GPUPass *pass);

void GPU_pass_cache_init();
void GPU_pass_cache_update();
void GPU_pass_cache_wait_for_all();
void GPU_pass_cache_free();
