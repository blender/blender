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
 * Copyright 2020, Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "GPU_platform.h"

namespace blender::gpu {

/**
 * This includes both hardware capabilities & workarounds.
 * Try to limit these to the implementation codebase (i.e.: gpu/opengl/).
 * Only add workarounds here if they are common to all implementation or
 * if you need access to it outside of the GPU module.
 * Same goes for capabilities (i.e.: texture size)
 */
struct GPUCapabilities {
  int max_texture_size = 0;
  int max_texture_layers = 0;
  int max_textures = 0;
  int max_textures_vert = 0;
  int max_textures_geom = 0;
  int max_textures_frag = 0;
  bool mem_stats_support = false;
  bool shader_image_load_store_support = false;
  /* OpenGL related workarounds. */
  bool mip_render_workaround = false;
  bool depth_blitting_workaround = false;
  bool use_main_context_workaround = false;
  bool broken_amd_driver = false;
  bool use_hq_normals_workaround = false;
  /* Vulkan related workarounds. */
};

extern GPUCapabilities GCaps;

}  // namespace blender::gpu
