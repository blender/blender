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
 * GPU Capabilities & workarounds
 * This module expose the reported implementation limits & enabled
 * workaround for drivers that needs specific codepaths.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int GPU_max_texture_size(void);
int GPU_max_texture_layers(void);
int GPU_max_textures(void);
int GPU_max_textures_vert(void);
int GPU_max_textures_geom(void);
int GPU_max_textures_frag(void);

int GPU_texture_size_with_limit(int res, bool limit_gl_texture_size);

bool GPU_mip_render_workaround(void);
bool GPU_depth_blitting_workaround(void);
bool GPU_use_main_context_workaround(void);
bool GPU_use_hq_normals_workaround(void);
bool GPU_crappy_amd_driver(void);

bool GPU_shader_image_load_store_support(void);

bool GPU_mem_stats_supported(void);
void GPU_mem_stats_get(int *totalmem, int *freemem);

bool GPU_stereo_quadbuffer_support(void);

#ifdef __cplusplus
}
#endif
