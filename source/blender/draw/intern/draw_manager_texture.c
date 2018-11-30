/*
 * Copyright 2016, Blender Foundation.
 *
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
 * Contributor(s): Blender Institute
 *
 */

/** \file blender/draw/intern/draw_manager_texture.c
 *  \ingroup draw
 */

#include "draw_manager.h"

#ifndef NDEBUG
/* Maybe gpu_texture.c is a better place for this. */
static bool drw_texture_format_supports_framebuffer(GPUTextureFormat format)
{
	/* Some formats do not work with framebuffers. */
	switch (format) {
		/* Only add formats that are COMPATIBLE with FB.
		 * Generally they are multiple of 16bit. */
		case GPU_R16F:
		case GPU_R16I:
		case GPU_R16UI:
		case GPU_R16:
		case GPU_R32F:
		case GPU_R32UI:
		case GPU_RG8:
		case GPU_RG16:
		case GPU_RG16F:
		case GPU_RG16I:
		case GPU_RG32F:
		case GPU_R11F_G11F_B10F:
		case GPU_RGBA8:
		case GPU_RGBA16F:
		case GPU_RGBA32F:
		case GPU_DEPTH_COMPONENT16:
		case GPU_DEPTH_COMPONENT24:
		case GPU_DEPTH24_STENCIL8:
		case GPU_DEPTH_COMPONENT32F:
			return true;
		default:
			return false;
	}
}
#endif

void drw_texture_set_parameters(GPUTexture *tex, DRWTextureFlag flags)
{
	GPU_texture_bind(tex, 0);
	if (flags & DRW_TEX_MIPMAP) {
		GPU_texture_mipmap_mode(tex, true, flags & DRW_TEX_FILTER);
		GPU_texture_generate_mipmap(tex);
	}
	else {
		GPU_texture_filter_mode(tex, flags & DRW_TEX_FILTER);
	}
	GPU_texture_wrap_mode(tex, flags & DRW_TEX_WRAP);
	GPU_texture_compare_mode(tex, flags & DRW_TEX_COMPARE);
	GPU_texture_unbind(tex);
}

GPUTexture *DRW_texture_create_1D(int w, GPUTextureFormat format, DRWTextureFlag flags, const float *fpixels)
{
	GPUTexture *tex = GPU_texture_create_1D(w, format, fpixels, NULL);
	drw_texture_set_parameters(tex, flags);

	return tex;
}

GPUTexture *DRW_texture_create_2D(int w, int h, GPUTextureFormat format, DRWTextureFlag flags, const float *fpixels)
{
	GPUTexture *tex = GPU_texture_create_2D(w, h, format, fpixels, NULL);
	drw_texture_set_parameters(tex, flags);

	return tex;
}

GPUTexture *DRW_texture_create_2D_array(
        int w, int h, int d, GPUTextureFormat format, DRWTextureFlag flags, const float *fpixels)
{
	GPUTexture *tex = GPU_texture_create_2D_array(w, h, d, format, fpixels, NULL);
	drw_texture_set_parameters(tex, flags);

	return tex;
}

GPUTexture *DRW_texture_create_3D(
        int w, int h, int d, GPUTextureFormat format, DRWTextureFlag flags, const float *fpixels)
{
	GPUTexture *tex = GPU_texture_create_3D(w, h, d, format, fpixels, NULL);
	drw_texture_set_parameters(tex, flags);

	return tex;
}

GPUTexture *DRW_texture_create_cube(int w, GPUTextureFormat format, DRWTextureFlag flags, const float *fpixels)
{
	GPUTexture *tex = GPU_texture_create_cube(w, format, fpixels, NULL);
	drw_texture_set_parameters(tex, flags);

	return tex;
}

GPUTexture *DRW_texture_pool_query_2D(int w, int h, GPUTextureFormat format, DrawEngineType *engine_type)
{
	BLI_assert(drw_texture_format_supports_framebuffer(format));
	GPUTexture *tex = GPU_viewport_texture_pool_query(DST.viewport, engine_type, w, h, format);

	return tex;
}

void DRW_texture_ensure_fullscreen_2D(GPUTexture **tex, GPUTextureFormat format, DRWTextureFlag flags)
{
	if (*(tex) == NULL) {
		const float *size = DRW_viewport_size_get();
		*(tex) = DRW_texture_create_2D((int)size[0], (int)size[1], format, flags, NULL);
	}
}

void DRW_texture_ensure_2D(GPUTexture **tex, int w, int h, GPUTextureFormat format, DRWTextureFlag flags)
{
	if (*(tex) == NULL) {
		*(tex) = DRW_texture_create_2D(w, h, format, flags, NULL);
	}
}

void DRW_texture_generate_mipmaps(GPUTexture *tex)
{
	GPU_texture_bind(tex, 0);
	GPU_texture_generate_mipmap(tex);
	GPU_texture_unbind(tex);
}

void DRW_texture_free(GPUTexture *tex)
{
	GPU_texture_free(tex);
}
