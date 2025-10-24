/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_sys_types.h"

namespace blender::gpu {

/**
 * This includes both hardware capabilities & workarounds.
 * Try to limit these to the implementation code-base (i.e.: `gpu/opengl/`).
 * Only add workarounds here if they are common to all implementation or
 * if you need access to it outside of the GPU module.
 * Same goes for capabilities (i.e.: texture size).
 */
struct GPUCapabilities {
  int max_texture_size = 0;
  int max_texture_3d_size = 0;
  uint32_t max_buffer_texture_size = 0;
  int max_texture_layers = 0;
  int max_textures = 0;
  int max_textures_vert = 0;
  int max_textures_geom = 0;
  int max_textures_frag = 0;
  int max_samplers = 0;
  int max_images = 0;
  int max_work_group_count[3] = {0, 0, 0};
  int max_work_group_size[3] = {0, 0, 0};
  int max_uniforms_vert = 0;
  int max_uniforms_frag = 0;
  int max_batch_indices = 0;
  int max_batch_vertices = 0;
  int max_vertex_attribs = 0;
  int max_varying_floats = 0;
  int max_shader_storage_buffer_bindings = 0;
  int max_compute_shader_storage_blocks = 0;
  size_t max_uniform_buffer_size = 0;
  size_t max_storage_buffer_size = 0;
  size_t storage_buffer_alignment = 256;
  int extensions_len = 0;
  const char *(*extension_get)(int);

  bool mem_stats_support = false;
  bool geometry_shader_support = false;
  bool hdr_viewport_support = false;
  bool stencil_export_support = false;

  int max_parallel_compilations = -1;

  /* OpenGL related workarounds. */
  bool depth_blitting_workaround = false;
  bool use_main_context_workaround = false;
  bool use_hq_normals_workaround = false;
  bool stencil_clasify_buffer_workaround = false;

  bool use_subprocess_shader_compilations = false;

  /* Metal related workarounds. */
  /* Minimum per-vertex stride in bytes (For a vertex buffer). */
  int minimum_per_vertex_stride = 1;
};

extern GPUCapabilities GCaps;

}  // namespace blender::gpu
