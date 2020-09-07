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

#include "GPU_extensions.h"

#include "gpu_extensions_private.hh"

#include "gl_backend.hh" /* TODO remove */

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

int GPU_texture_size_with_limit(int res)
{
  int size = GPU_max_texture_size();
  int reslimit = (U.glreslimit != 0) ? min_ii(U.glreslimit, size) : size;
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

bool GPU_arb_texture_cube_map_array_is_supported(void)
{
  /* FIXME bad level call. */
  return GLContext::texture_cube_map_array_support;
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Memory statistics
 * \{ */

bool GPU_mem_stats_supported(void)
{
#ifndef GPU_STANDALONE
  return (GLEW_NVX_gpu_memory_info || GLEW_ATI_meminfo);
#else
  return false;
#endif
}

void GPU_mem_stats_get(int *totalmem, int *freemem)
{
  /* TODO(merwin): use Apple's platform API to get this info */

  if (GLEW_NVX_gpu_memory_info) {
    /* returned value in Kb */
    glGetIntegerv(GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX, totalmem);

    glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX, freemem);
  }
  else if (GLEW_ATI_meminfo) {
    int stats[4];

    glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, stats);
    *freemem = stats[0];
    *totalmem = 0;
  }
  else {
    *totalmem = 0;
    *freemem = 0;
  }
}

/* Return support for the active context + window. */
bool GPU_stereo_quadbuffer_support(void)
{
  GLboolean stereo = GL_FALSE;
  glGetBooleanv(GL_STEREO, &stereo);
  return stereo == GL_TRUE;
}

/** \} */
