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

void drw_texture_get_format(
        DRWTextureFormat format, bool is_framebuffer,
        GPUTextureFormat *r_data_type, int *r_channels, bool *r_is_depth)
{
	/* Some formats do not work with framebuffers. */
	if (is_framebuffer) {
		switch (format) {
			/* Only add formats that are COMPATIBLE with FB.
			 * Generally they are multiple of 16bit. */
			case DRW_TEX_R_16:
			case DRW_TEX_R_16I:
			case DRW_TEX_R_16U:
			case DRW_TEX_R_32:
			case DRW_TEX_R_32U:
			case DRW_TEX_RG_8:
			case DRW_TEX_RG_16:
			case DRW_TEX_RG_16I:
			case DRW_TEX_RG_32:
			case DRW_TEX_RGBA_8:
			case DRW_TEX_RGBA_16:
			case DRW_TEX_RGBA_32:
			case DRW_TEX_DEPTH_16:
			case DRW_TEX_DEPTH_24:
			case DRW_TEX_DEPTH_24_STENCIL_8:
			case DRW_TEX_DEPTH_32:
			case DRW_TEX_RGB_11_11_10:
				break;
			default:
				BLI_assert(false && "Texture format unsupported as render target!");
				*r_channels = 4;
				*r_data_type = GPU_RGBA8;
				*r_is_depth = false;
				return;
		}
	}

	switch (format) {
		case DRW_TEX_RGBA_8: *r_data_type = GPU_RGBA8; break;
		case DRW_TEX_RGBA_16: *r_data_type = GPU_RGBA16F; break;
		case DRW_TEX_RGBA_32: *r_data_type = GPU_RGBA32F; break;
		case DRW_TEX_RGB_16: *r_data_type = GPU_RGB16F; break;
		case DRW_TEX_RGB_11_11_10: *r_data_type = GPU_R11F_G11F_B10F; break;
		case DRW_TEX_RG_8: *r_data_type = GPU_RG8; break;
		case DRW_TEX_RG_16: *r_data_type = GPU_RG16F; break;
		case DRW_TEX_RG_16I: *r_data_type = GPU_RG16I; break;
		case DRW_TEX_RG_32: *r_data_type = GPU_RG32F; break;
		case DRW_TEX_R_8: *r_data_type = GPU_R8; break;
		case DRW_TEX_R_16: *r_data_type = GPU_R16F; break;
		case DRW_TEX_R_16I: *r_data_type = GPU_R16I; break;
		case DRW_TEX_R_16U: *r_data_type = GPU_R16UI; break;
		case DRW_TEX_R_32: *r_data_type = GPU_R32F; break;
		case DRW_TEX_R_32U: *r_data_type = GPU_R32UI; break;
#if 0
		case DRW_TEX_RGB_8: *r_data_type = GPU_RGB8; break;
		case DRW_TEX_RGB_32: *r_data_type = GPU_RGB32F; break;
#endif
		case DRW_TEX_DEPTH_16: *r_data_type = GPU_DEPTH_COMPONENT16; break;
		case DRW_TEX_DEPTH_24: *r_data_type = GPU_DEPTH_COMPONENT24; break;
		case DRW_TEX_DEPTH_24_STENCIL_8: *r_data_type = GPU_DEPTH24_STENCIL8; break;
		case DRW_TEX_DEPTH_32: *r_data_type = GPU_DEPTH_COMPONENT32F; break;
		default :
			/* file type not supported you must uncomment it from above */
			BLI_assert(false);
			break;
	}

	switch (format) {
		case DRW_TEX_RGBA_8:
		case DRW_TEX_RGBA_16:
		case DRW_TEX_RGBA_32:
			*r_channels = 4;
			break;
		case DRW_TEX_RGB_8:
		case DRW_TEX_RGB_16:
		case DRW_TEX_RGB_32:
		case DRW_TEX_RGB_11_11_10:
			*r_channels = 3;
			break;
		case DRW_TEX_RG_8:
		case DRW_TEX_RG_16:
		case DRW_TEX_RG_16I:
		case DRW_TEX_RG_32:
			*r_channels = 2;
			break;
		default:
			*r_channels = 1;
			break;
	}

	if (r_is_depth) {
		*r_is_depth = ELEM(format, DRW_TEX_DEPTH_16, DRW_TEX_DEPTH_24, DRW_TEX_DEPTH_24_STENCIL_8);
	}
}

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

GPUTexture *DRW_texture_create_1D(int w, DRWTextureFormat format, DRWTextureFlag flags, const float *fpixels)
{
	GPUTexture *tex;
	GPUTextureFormat data_type;
	int channels;

	drw_texture_get_format(format, false, &data_type, &channels, NULL);
	tex = GPU_texture_create_1D_custom(w, channels, data_type, fpixels, NULL);
	drw_texture_set_parameters(tex, flags);

	return tex;
}

GPUTexture *DRW_texture_create_2D(int w, int h, DRWTextureFormat format, DRWTextureFlag flags, const float *fpixels)
{
	GPUTexture *tex;
	GPUTextureFormat data_type;
	int channels;

	drw_texture_get_format(format, false, &data_type, &channels, NULL);
	tex = GPU_texture_create_2D_custom(w, h, channels, data_type, fpixels, NULL);
	drw_texture_set_parameters(tex, flags);

	return tex;
}

GPUTexture *DRW_texture_create_2D_array(
        int w, int h, int d, DRWTextureFormat format, DRWTextureFlag flags, const float *fpixels)
{
	GPUTexture *tex;
	GPUTextureFormat data_type;
	int channels;

	drw_texture_get_format(format, false, &data_type, &channels, NULL);
	tex = GPU_texture_create_2D_array_custom(w, h, d, channels, data_type, fpixels, NULL);
	drw_texture_set_parameters(tex, flags);

	return tex;
}

GPUTexture *DRW_texture_create_3D(
        int w, int h, int d, DRWTextureFormat format, DRWTextureFlag flags, const float *fpixels)
{
	GPUTexture *tex;
	GPUTextureFormat data_type;
	int channels;

	drw_texture_get_format(format, false, &data_type, &channels, NULL);
	tex = GPU_texture_create_3D_custom(w, h, d, channels, data_type, fpixels, NULL);
	drw_texture_set_parameters(tex, flags);

	return tex;
}

GPUTexture *DRW_texture_create_cube(int w, DRWTextureFormat format, DRWTextureFlag flags, const float *fpixels)
{
	GPUTexture *tex;
	GPUTextureFormat data_type;
	int channels;

	drw_texture_get_format(format, false, &data_type, &channels, NULL);
	tex = GPU_texture_create_cube_custom(w, channels, data_type, fpixels, NULL);
	drw_texture_set_parameters(tex, flags);

	return tex;
}

GPUTexture *DRW_texture_pool_query_2D(int w, int h, DRWTextureFormat format, DrawEngineType *engine_type)
{
	GPUTexture *tex;
	GPUTextureFormat data_type;
	int channels;

	drw_texture_get_format(format, true, &data_type, &channels, NULL);
	tex = GPU_viewport_texture_pool_query(DST.viewport, engine_type, w, h, channels, data_type);

	return tex;
}

void DRW_texture_ensure_fullscreen_2D(GPUTexture **tex, DRWTextureFormat format, DRWTextureFlag flags)
{
	if (*(tex) == NULL) {
		const float *size = DRW_viewport_size_get();
		*(tex) = DRW_texture_create_2D((int)size[0], (int)size[1], format, flags, NULL);
	}
}

void DRW_texture_ensure_2D(GPUTexture **tex, int w, int h, DRWTextureFormat format, DRWTextureFlag flags)
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
