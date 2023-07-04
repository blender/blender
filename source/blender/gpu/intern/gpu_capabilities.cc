/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Wrap OpenGL features such as textures, shaders and GLSL
 * with checks for drivers and GPU support.
 */

#include "DNA_userdef_types.h" /* For `U.glreslimit`. */

#include "GPU_capabilities.h"

#include "gpu_context_private.hh"

#include "gpu_capabilities_private.hh"

namespace blender::gpu {

GPUCapabilities GCaps;

}

using namespace blender::gpu;

/* -------------------------------------------------------------------- */
/** \name Capabilities
 * \{ */

int GPU_max_texture_size()
{
  return GCaps.max_texture_size;
}

int GPU_max_texture_3d_size()
{
  return GCaps.max_texture_3d_size;
}

int GPU_texture_size_with_limit(int res)
{
  int size = GPU_max_texture_size();
  int reslimit = (U.glreslimit != 0) ? min_ii(U.glreslimit, size) : size;
  return min_ii(reslimit, res);
}

int GPU_max_texture_layers()
{
  return GCaps.max_texture_layers;
}

int GPU_max_textures_vert()
{
  return GCaps.max_textures_vert;
}

int GPU_max_textures_geom()
{
  return GCaps.max_textures_geom;
}

int GPU_max_textures_frag()
{
  return GCaps.max_textures_frag;
}

int GPU_max_textures()
{
  return GCaps.max_textures;
}

int GPU_max_work_group_count(int index)
{
  return GCaps.max_work_group_count[index];
}

int GPU_max_work_group_size(int index)
{
  return GCaps.max_work_group_size[index];
}

int GPU_max_uniforms_vert()
{
  return GCaps.max_uniforms_vert;
}

int GPU_max_uniforms_frag()
{
  return GCaps.max_uniforms_frag;
}

int GPU_max_batch_indices()
{
  return GCaps.max_batch_indices;
}

int GPU_max_batch_vertices()
{
  return GCaps.max_batch_vertices;
}

int GPU_max_vertex_attribs()
{
  return GCaps.max_vertex_attribs;
}

int GPU_max_varying_floats()
{
  return GCaps.max_varying_floats;
}

int GPU_extensions_len()
{
  return GCaps.extensions_len;
}

const char *GPU_extension_get(int i)
{
  return GCaps.extension_get ? GCaps.extension_get(i) : "\0";
}

int GPU_max_samplers()
{
  return GCaps.max_samplers;
}

bool GPU_mip_render_workaround()
{
  return GCaps.mip_render_workaround;
}

bool GPU_depth_blitting_workaround()
{
  return GCaps.depth_blitting_workaround;
}

bool GPU_use_main_context_workaround()
{
  return GCaps.use_main_context_workaround;
}

bool GPU_crappy_amd_driver()
{
  /* Currently are the same drivers with the `unused_fb_slot` problem. */
  return GCaps.broken_amd_driver;
}

bool GPU_use_hq_normals_workaround()
{
  return GCaps.use_hq_normals_workaround;
}

bool GPU_clear_viewport_workaround()
{
  return GCaps.clear_viewport_workaround;
}

bool GPU_compute_shader_support()
{
  return GCaps.compute_shader_support;
}

bool GPU_geometry_shader_support()
{
  return GCaps.geometry_shader_support;
}

bool GPU_shader_storage_buffer_objects_support()
{
  return GCaps.shader_storage_buffer_objects_support;
}

bool GPU_shader_image_load_store_support()
{
  return GCaps.shader_image_load_store_support;
}

bool GPU_shader_draw_parameters_support()
{
  return GCaps.shader_draw_parameters_support;
}

int GPU_max_shader_storage_buffer_bindings()
{
  return GCaps.max_shader_storage_buffer_bindings;
}

int GPU_max_compute_shader_storage_blocks()
{
  return GCaps.max_compute_shader_storage_blocks;
}

int GPU_minimum_per_vertex_stride()
{
  return GCaps.minimum_per_vertex_stride;
}

bool GPU_transform_feedback_support()
{
  return GCaps.transform_feedback_support;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Memory statistics
 * \{ */

bool GPU_mem_stats_supported()
{
  return GCaps.mem_stats_support;
}

void GPU_mem_stats_get(int *totalmem, int *freemem)
{
  Context::get()->memory_statistics_get(totalmem, freemem);
}

bool GPU_stereo_quadbuffer_support()
{
  return Context::get()->front_right != nullptr;
}

/** \} */
