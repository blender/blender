/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * Wrap OpenGL features such as textures, shaders and GLSL
 * with checks for drivers and GPU support.
 */

#include "DNA_userdef_types.h"

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

int GPU_max_texture_size(void)
{
  return GCaps.max_texture_size;
}

int GPU_texture_size_with_limit(int res, bool limit_gl_texture_size)
{
  int size = GPU_max_texture_size();
  int reslimit = (limit_gl_texture_size && (U.glreslimit != 0)) ? min_ii(U.glreslimit, size) :
                                                                  size;
  return min_ii(reslimit, res);
}

int GPU_max_texture_layers(void)
{
  return GCaps.max_texture_layers;
}

int GPU_max_textures_vert(void)
{
  return GCaps.max_textures_vert;
}

int GPU_max_textures_geom(void)
{
  return GCaps.max_textures_geom;
}

int GPU_max_textures_frag(void)
{
  return GCaps.max_textures_frag;
}

int GPU_max_textures(void)
{
  return GCaps.max_textures;
}

bool GPU_mip_render_workaround(void)
{
  return GCaps.mip_render_workaround;
}

bool GPU_depth_blitting_workaround(void)
{
  return GCaps.depth_blitting_workaround;
}

bool GPU_use_main_context_workaround(void)
{
  return GCaps.use_main_context_workaround;
}

bool GPU_crappy_amd_driver(void)
{
  /* Currently are the same drivers with the `unused_fb_slot` problem. */
  return GCaps.broken_amd_driver;
}

bool GPU_use_hq_normals_workaround(void)
{
  return GCaps.use_hq_normals_workaround;
}

bool GPU_shader_image_load_store_support(void)
{
  return GCaps.shader_image_load_store_support;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Memory statistics
 * \{ */

bool GPU_mem_stats_supported(void)
{
  return GCaps.mem_stats_support;
}

void GPU_mem_stats_get(int *totalmem, int *freemem)
{
  Context::get()->memory_statistics_get(totalmem, freemem);
}

/* Return support for the active context + window. */
bool GPU_stereo_quadbuffer_support(void)
{
  return Context::get()->front_right != nullptr;
}

/** \} */
