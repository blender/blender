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

namespace blender::gpu {
class Batch;
class StorageBuf;
}  // namespace blender::gpu

/* `gpu_batch_presets.cc` */

/* Replacement for #gluSphere */

blender::gpu::Batch *GPU_batch_preset_sphere(int lod) ATTR_WARN_UNUSED_RESULT;
blender::gpu::Batch *GPU_batch_preset_sphere_wire(int lod) ATTR_WARN_UNUSED_RESULT;

/**
 * To be used with procedural placement inside shader.
 */
blender::gpu::Batch *GPU_batch_preset_quad();

void gpu_batch_presets_init();
/* Registers batch to be destroyed at exit time. */
void gpu_batch_presets_register(blender::gpu::Batch *preset_batch);
/* Registers buffer to be destroyed at exit time. */
void gpu_batch_storage_buffer_register(blender::gpu::StorageBuf *preset_buffer);
void gpu_batch_presets_exit();
