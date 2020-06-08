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
 */

#ifndef __GPU_EXTENSIONS_H__
#define __GPU_EXTENSIONS_H__

#ifdef __cplusplus
extern "C" {
#endif

/* GPU extensions support */

int GPU_max_texture_size(void);
int GPU_max_texture_layers(void);
int GPU_max_textures(void);
int GPU_max_textures_vert(void);
int GPU_max_textures_geom(void);
int GPU_max_textures_frag(void);
float GPU_max_texture_anisotropy(void);
int GPU_max_color_texture_samples(void);
int GPU_max_cube_map_size(void);
int GPU_max_ubo_binds(void);
int GPU_max_ubo_size(void);
float GPU_max_line_width(void);
void GPU_get_dfdy_factors(float fac[2]);
bool GPU_arb_base_instance_is_supported(void);
bool GPU_arb_texture_cube_map_array_is_supported(void);
bool GPU_mip_render_workaround(void);
bool GPU_depth_blitting_workaround(void);
bool GPU_unused_fb_slot_workaround(void);
bool GPU_context_local_shaders_workaround(void);
bool GPU_texture_copy_workaround(void);
bool GPU_crappy_amd_driver(void);

bool GPU_mem_stats_supported(void);
void GPU_mem_stats_get(int *totalmem, int *freemem);

void GPU_code_generate_glsl_lib(void);

#ifdef __cplusplus
}
#endif

#endif /* __GPU_EXTENSIONS_H__ */
