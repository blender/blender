/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "GPU_platform.h"

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
  int max_texture_layers = 0;
  int max_textures = 0;
  int max_textures_vert = 0;
  int max_textures_geom = 0;
  int max_textures_frag = 0;
  int max_samplers = 0;
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
  int extensions_len = 0;
  const char *(*extension_get)(int);

  bool mem_stats_support = false;
  bool compute_shader_support = false;
  bool geometry_shader_support = false;
  bool shader_storage_buffer_objects_support = false;
  bool shader_image_load_store_support = false;
  bool shader_draw_parameters_support = false;
  bool transform_feedback_support = false;
  bool hdr_viewport_support = false;

  /* OpenGL related workarounds. */
  bool mip_render_workaround = false;
  bool depth_blitting_workaround = false;
  bool use_main_context_workaround = false;
  bool broken_amd_driver = false;
  bool use_hq_normals_workaround = false;
  bool clear_viewport_workaround = false;
  /* Vulkan related workarounds. */

  /* Metal related workarounds. */
  /* Minimum per-vertex stride in bytes (For a vertex buffer). */
  int minimum_per_vertex_stride = 1;
};

extern GPUCapabilities GCaps;

}  // namespace blender::gpu
