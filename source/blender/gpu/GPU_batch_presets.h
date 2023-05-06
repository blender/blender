/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2016 Blender Foundation */

/** \file
 * \ingroup gpu
 *
 * Batched geometry rendering is powered by the GPU library.
 * This file contains any additions or modifications specific to Blender.
 */

#pragma once

#include "BLI_compiler_attrs.h"
#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* gpu_batch_presets.c */

/* Replacement for #gluSphere */

struct GPUBatch *GPU_batch_preset_sphere(int lod) ATTR_WARN_UNUSED_RESULT;
struct GPUBatch *GPU_batch_preset_sphere_wire(int lod) ATTR_WARN_UNUSED_RESULT;
struct GPUBatch *GPU_batch_preset_panel_drag_widget(float pixelsize,
                                                    const float col_high[4],
                                                    const float col_dark[4],
                                                    float width) ATTR_WARN_UNUSED_RESULT;

/**
 * To be used with procedural placement inside shader.
 */
struct GPUBatch *GPU_batch_preset_quad(void);

void gpu_batch_presets_init(void);
void gpu_batch_presets_register(struct GPUBatch *preset_batch);
bool gpu_batch_presets_unregister(struct GPUBatch *preset_batch);
void gpu_batch_presets_exit(void);

#ifdef __cplusplus
}
#endif
