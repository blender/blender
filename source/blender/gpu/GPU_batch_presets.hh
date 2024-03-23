/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Batched geometry rendering is powered by the GPU library.
 * This file contains any additions or modifications specific to Blender.
 */

#pragma once

#include "BLI_compiler_attrs.h"
#include "BLI_sys_types.h"

struct GPUBatch;

/* `gpu_batch_presets.cc` */

/* Replacement for #gluSphere */

GPUBatch *GPU_batch_preset_sphere(int lod) ATTR_WARN_UNUSED_RESULT;
GPUBatch *GPU_batch_preset_sphere_wire(int lod) ATTR_WARN_UNUSED_RESULT;
GPUBatch *GPU_batch_preset_panel_drag_widget(float pixelsize,
                                             const float col_high[4],
                                             const float col_dark[4],
                                             float width) ATTR_WARN_UNUSED_RESULT;

/**
 * To be used with procedural placement inside shader.
 */
GPUBatch *GPU_batch_preset_quad();

void gpu_batch_presets_init();
void gpu_batch_presets_register(GPUBatch *preset_batch);
bool gpu_batch_presets_unregister(GPUBatch *preset_batch);
void gpu_batch_presets_exit();
