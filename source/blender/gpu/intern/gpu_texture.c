/*
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Brecht Van Lommel.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "MEM_guardedalloc.h"

#include "DNA_image_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_math_base.h"
#include "BLI_listbase.h"
#include "BLI_threads.h"

#include "BKE_global.h"

#include "GPU_batch.h"
#include "GPU_context.h"
#include "GPU_debug.h"
#include "GPU_draw.h"
#include "GPU_extensions.h"
#include "GPU_glew.h"
#include "GPU_framebuffer.h"
#include "GPU_texture.h"

#include "gpu_context_private.h"

static struct GPUTextureGlobal {
	GPUTexture *invalid_tex_1D; /* texture used in place of invalid textures (not loaded correctly, missing) */
	GPUTexture *invalid_tex_2D;
	GPUTexture *invalid_tex_3D;
} GG = {NULL, NULL, NULL};

/* Maximum number of FBOs a texture can be attached to. */
#define GPU_TEX_MAX_FBO_ATTACHED 8

typedef enum GPUTextureFormatFlag {
	GPU_FORMAT_DEPTH     = (1 << 0),
	GPU_FORMAT_STENCIL   = (1 << 1),
	GPU_FORMAT_INTEGER   = (1 << 2),
	GPU_FORMAT_FLOAT     = (1 << 3),

	GPU_FORMAT_1D        = (1 << 10),
	GPU_FORMAT_2D        = (1 << 11),
	GPU_FORMAT_3D        = (1 << 12),
	GPU_FORMAT_CUBE      = (1 << 13),
	GPU_FORMAT_ARRAY     = (1 << 14),
} GPUTextureFormatFlag;

/* GPUTexture */
struct GPUTexture {
	int w, h, d;        /* width/height/depth */
	int number;         /* number for multitexture binding */
	int refcount;       /* reference count */
	GLenum target;      /* GL_TEXTURE_* */
	GLenum target_base; /* same as target, (but no multisample)
	                     * use it for unbinding */
	GLuint bindcode;    /* opengl identifier for texture */

	GPUTextureFormat format;
	GPUTextureFormatFlag format_flag;

	unsigned int bytesize; /* number of byte for one pixel */
	int components;     /* number of color/alpha channels */
	int samples;        /* number of samples for multisamples textures. 0 if not multisample target */

	int fb_attachment[GPU_TEX_MAX_FBO_ATTACHED];
	GPUFrameBuffer *fb[GPU_TEX_MAX_FBO_ATTACHED];
};

/* ------ Memory Management ------- */
/* Records every texture allocation / free
 * to estimate the Texture Pool Memory consumption */
static unsigned int memory_usage;

static unsigned int gpu_texture_memory_footprint_compute(GPUTexture *tex)
{
	int samp = max_ii(tex->samples, 1);
	switch (tex->target) {
		case GL_TEXTURE_1D:
			return tex->bytesize * tex->w * samp;
		case GL_TEXTURE_1D_ARRAY:
		case GL_TEXTURE_2D:
			return tex->bytesize * tex->w * tex->h * samp;
		case GL_TEXTURE_2D_ARRAY:
		case GL_TEXTURE_3D:
			return tex->bytesize * tex->w * tex->h * tex->d * samp;
		case GL_TEXTURE_CUBE_MAP:
			return tex->bytesize * 6 * tex->w * tex->h * samp;
		case GL_TEXTURE_CUBE_MAP_ARRAY:
			return tex->bytesize * 6 * tex->w * tex->h * tex->d * samp;
		default:
			return 0;
	}
}

static void gpu_texture_memory_footprint_add(GPUTexture *tex)
{
	memory_usage += gpu_texture_memory_footprint_compute(tex);
}

static void gpu_texture_memory_footprint_remove(GPUTexture *tex)
{
	memory_usage -= gpu_texture_memory_footprint_compute(tex);
}

unsigned int GPU_texture_memory_usage_get(void)
{
	return memory_usage;
}

/* -------------------------------- */

static int gpu_get_component_count(GPUTextureFormat format)
{
	switch (format) {
		case GPU_RGBA8:
		case GPU_RGBA16F:
		case GPU_RGBA16:
		case GPU_RGBA32F:
			return 4;
		case GPU_RGB16F:
		case GPU_R11F_G11F_B10F:
			return 3;
		case GPU_RG8:
		case GPU_RG16:
		case GPU_RG16F:
		case GPU_RG16I:
		case GPU_RG16UI:
		case GPU_RG32F:
			return 2;
		default:
			return 1;
	}
}

/* Definitely not complete, edit according to the gl specification. */
static void gpu_validate_data_format(GPUTextureFormat tex_format, GPUDataFormat data_format)
{
	(void)data_format;

	if (ELEM(tex_format,
	         GPU_DEPTH_COMPONENT24,
	         GPU_DEPTH_COMPONENT16,
	         GPU_DEPTH_COMPONENT32F))
	{
		BLI_assert(data_format == GPU_DATA_FLOAT);
	}
	else if (tex_format == GPU_DEPTH24_STENCIL8) {
		BLI_assert(data_format == GPU_DATA_UNSIGNED_INT_24_8);
	}
	else {
		/* Integer formats */
		if (ELEM(tex_format, GPU_RG16I, GPU_R16I, GPU_RG16UI, GPU_R16UI, GPU_R32UI)) {
			if (ELEM(tex_format, GPU_R16UI, GPU_RG16UI, GPU_R32UI)) {
				BLI_assert(data_format == GPU_DATA_UNSIGNED_INT);
			}
			else {
				BLI_assert(data_format == GPU_DATA_INT);
			}
		}
		/* Byte formats */
		else if (ELEM(tex_format, GPU_R8, GPU_RG8, GPU_RGBA8)) {
			BLI_assert(ELEM(data_format, GPU_DATA_UNSIGNED_BYTE, GPU_DATA_FLOAT));
		}
		/* Special case */
		else if (ELEM(tex_format, GPU_R11F_G11F_B10F)) {
			BLI_assert(ELEM(data_format, GPU_DATA_10_11_11_REV, GPU_DATA_FLOAT));
		}
		/* Float formats */
		else {
			BLI_assert(ELEM(data_format, GPU_DATA_FLOAT));
		}
	}
}

static GPUDataFormat gpu_get_data_format_from_tex_format(GPUTextureFormat tex_format)
{
	if (ELEM(tex_format,
	         GPU_DEPTH_COMPONENT24,
	         GPU_DEPTH_COMPONENT16,
	         GPU_DEPTH_COMPONENT32F))
	{
		return GPU_DATA_FLOAT;
	}
	else if (tex_format == GPU_DEPTH24_STENCIL8) {
		return GPU_DATA_UNSIGNED_INT_24_8;
	}
	else {
		/* Integer formats */
		if (ELEM(tex_format, GPU_RG16I, GPU_R16I, GPU_RG16UI, GPU_R16UI, GPU_R32UI)) {
			if (ELEM(tex_format, GPU_R16UI, GPU_RG16UI, GPU_R32UI)) {
				return GPU_DATA_UNSIGNED_INT;
			}
			else {
				return GPU_DATA_INT;
			}
		}
		/* Byte formats */
		else if (ELEM(tex_format, GPU_R8)) {
			return GPU_DATA_UNSIGNED_BYTE;
		}
		/* Special case */
		else if (ELEM(tex_format, GPU_R11F_G11F_B10F)) {
			return GPU_DATA_10_11_11_REV;
		}
		else {
			return GPU_DATA_FLOAT;
		}
	}
}

/* Definitely not complete, edit according to the gl specification. */
static GLenum gpu_get_gl_dataformat(GPUTextureFormat data_type, GPUTextureFormatFlag *format_flag)
{
	if (ELEM(data_type,
	         GPU_DEPTH_COMPONENT24,
	         GPU_DEPTH_COMPONENT16,
	         GPU_DEPTH_COMPONENT32F))
	{
		*format_flag |= GPU_FORMAT_DEPTH;
		return GL_DEPTH_COMPONENT;
	}
	else if (data_type == GPU_DEPTH24_STENCIL8) {
		*format_flag |= GPU_FORMAT_DEPTH | GPU_FORMAT_STENCIL;
		return GL_DEPTH_STENCIL;
	}
	else {
		/* Integer formats */
		if (ELEM(data_type, GPU_RG16I, GPU_R16I, GPU_RG16UI, GPU_R16UI, GPU_R32UI)) {
			*format_flag |= GPU_FORMAT_INTEGER;

			switch (gpu_get_component_count(data_type)) {
				case 1: return GL_RED_INTEGER; break;
				case 2: return GL_RG_INTEGER; break;
				case 3: return GL_RGB_INTEGER; break;
				case 4: return GL_RGBA_INTEGER; break;
			}
		}
		else if (ELEM(data_type, GPU_R8)) {
			*format_flag |= GPU_FORMAT_FLOAT;
			return GL_RED;
		}
		else {
			*format_flag |= GPU_FORMAT_FLOAT;

			switch (gpu_get_component_count(data_type)) {
				case 1: return GL_RED; break;
				case 2: return GL_RG; break;
				case 3: return GL_RGB; break;
				case 4: return GL_RGBA; break;
			}
		}
	}

	BLI_assert(0);
	*format_flag |= GPU_FORMAT_FLOAT;
	return GL_RGBA;
}

static unsigned int gpu_get_bytesize(GPUTextureFormat data_type)
{
	switch (data_type) {
		case GPU_RGBA32F:
			return 32;
		case GPU_RG32F:
		case GPU_RGBA16F:
		case GPU_RGBA16:
			return 16;
		case GPU_RGB16F:
			return 12;
		case GPU_RG16F:
		case GPU_RG16I:
		case GPU_RG16UI:
		case GPU_RG16:
		case GPU_DEPTH24_STENCIL8:
		case GPU_DEPTH_COMPONENT32F:
		case GPU_RGBA8:
		case GPU_R11F_G11F_B10F:
		case GPU_R32F:
		case GPU_R32UI:
		case GPU_R32I:
			return 4;
		case GPU_DEPTH_COMPONENT24:
			return 3;
		case GPU_DEPTH_COMPONENT16:
		case GPU_R16F:
		case GPU_R16UI:
		case GPU_R16I:
		case GPU_RG8:
			return 2;
		case GPU_R8:
			return 1;
		default:
			BLI_assert(!"Texture format incorrect or unsupported\n");
			return 0;
	}
}

static GLenum gpu_get_gl_internalformat(GPUTextureFormat format)
{
	/* You can add any of the available type to this list
	 * For available types see GPU_texture.h */
	switch (format) {
		/* Formats texture & renderbuffer */
		case GPU_RGBA32F: return GL_RGBA32F;
		case GPU_RGBA16F: return GL_RGBA16F;
		case GPU_RGBA16: return GL_RGBA16;
		case GPU_RG32F: return GL_RG32F;
		case GPU_RGB16F: return GL_RGB16F;
		case GPU_RG16F: return GL_RG16F;
		case GPU_RG16I: return GL_RG16I;
		case GPU_RG16: return GL_RG16;
		case GPU_RGBA8: return GL_RGBA8;
		case GPU_R32F: return GL_R32F;
		case GPU_R32UI: return GL_R32UI;
		case GPU_R32I: return GL_R32I;
		case GPU_R16F: return GL_R16F;
		case GPU_R16I: return GL_R16I;
		case GPU_R16UI: return GL_R16UI;
		case GPU_RG8: return GL_RG8;
		case GPU_RG16UI: return GL_RG16UI;
		case GPU_R8: return GL_R8;
		/* Special formats texture & renderbuffer */
		case GPU_R11F_G11F_B10F: return GL_R11F_G11F_B10F;
		case GPU_DEPTH24_STENCIL8: return GL_DEPTH24_STENCIL8;
		/* Texture only format */
		/* ** Add Format here **/
		/* Special formats texture only */
		/* ** Add Format here **/
		/* Depth Formats */
		case GPU_DEPTH_COMPONENT32F: return GL_DEPTH_COMPONENT32F;
		case GPU_DEPTH_COMPONENT24: return GL_DEPTH_COMPONENT24;
		case GPU_DEPTH_COMPONENT16: return GL_DEPTH_COMPONENT16;
		default:
			BLI_assert(!"Texture format incorrect or unsupported\n");
			return 0;
	}
}

static GLenum gpu_get_gl_datatype(GPUDataFormat format)
{
	switch (format) {
		case GPU_DATA_FLOAT:
			return GL_FLOAT;
		case GPU_DATA_INT:
			return GL_INT;
		case GPU_DATA_UNSIGNED_INT:
			return GL_UNSIGNED_INT;
		case GPU_DATA_UNSIGNED_BYTE:
			return GL_UNSIGNED_BYTE;
		case GPU_DATA_UNSIGNED_INT_24_8:
			return GL_UNSIGNED_INT_24_8;
		case GPU_DATA_10_11_11_REV:
			return GL_UNSIGNED_INT_10F_11F_11F_REV;
		default:
			BLI_assert(!"Unhandled data format");
			return GL_FLOAT;
	}
}

static float *GPU_texture_3D_rescale(GPUTexture *tex, int w, int h, int d, int channels, const float *fpixels)
{
	const unsigned int xf = w / tex->w, yf = h / tex->h, zf = d / tex->d;
	float *nfpixels = MEM_mallocN(channels * sizeof(float) * tex->w * tex->h * tex->d, "GPUTexture Rescaled 3Dtex");

	if (nfpixels) {
		GPU_print_error_debug("You need to scale a 3D texture, feel the pain!");

		for (unsigned k = 0; k < tex->d; k++) {
			for (unsigned j = 0; j < tex->h; j++) {
				for (unsigned i = 0; i < tex->w; i++) {
					/* obviously doing nearest filtering here,
					 * it's going to be slow in any case, let's not make it worse */
					float xb = i * xf;
					float yb = j * yf;
					float zb = k * zf;
					unsigned int offset = k * (tex->w * tex->h) + i * tex->h + j;
					unsigned int offset_orig = (zb) * (w * h) + (xb) * h + (yb);

					if (channels == 4) {
						nfpixels[offset * 4] = fpixels[offset_orig * 4];
						nfpixels[offset * 4 + 1] = fpixels[offset_orig * 4 + 1];
						nfpixels[offset * 4 + 2] = fpixels[offset_orig * 4 + 2];
						nfpixels[offset * 4 + 3] = fpixels[offset_orig * 4 + 3];
					}
					else
						nfpixels[offset] = fpixels[offset_orig];
				}
			}
		}
	}

	return nfpixels;
}

/* This tries to allocate video memory for a given texture
 * If alloc fails, lower the resolution until it fits. */
static bool gpu_texture_try_alloc(
        GPUTexture *tex, GLenum proxy, GLenum internalformat, GLenum data_format, GLenum data_type,
        int channels, bool try_rescale, const float *fpixels, float **rescaled_fpixels)
{
	int r_width;

	switch (proxy) {
		case GL_PROXY_TEXTURE_1D:
			glTexImage1D(proxy, 0, internalformat, tex->w, 0, data_format, data_type, NULL);
			break;
		case GL_PROXY_TEXTURE_1D_ARRAY:
		case GL_PROXY_TEXTURE_2D:
			glTexImage2D(proxy, 0, internalformat, tex->w, tex->h, 0, data_format, data_type, NULL);
			break;
		case GL_PROXY_TEXTURE_2D_ARRAY:
			/* HACK: Some driver wrongly check GL_PROXY_TEXTURE_2D_ARRAY as a GL_PROXY_TEXTURE_3D
			 * checking all dimensions against GPU_max_texture_layers (see T55888). */
			return (tex->w > 0) && (tex->w <= GPU_max_texture_size()) &&
			       (tex->h > 0) && (tex->h <= GPU_max_texture_size()) &&
			       (tex->d > 0) && (tex->d <= GPU_max_texture_layers());
		case GL_PROXY_TEXTURE_3D:
			glTexImage3D(proxy, 0, internalformat, tex->w, tex->h, tex->d, 0, data_format, data_type, NULL);
			break;
	}

	glGetTexLevelParameteriv(proxy, 0, GL_TEXTURE_WIDTH, &r_width);

	if (r_width == 0 && try_rescale) {
		const int w = tex->w, h = tex->h, d = tex->d;

		/* Find largest texture possible */
		while (r_width == 0) {
			tex->w /= 2;
			tex->h /= 2;
			tex->d /= 2;

			/* really unlikely to happen but keep this just in case */
			if (tex->w == 0) break;
			if (tex->h == 0 && proxy != GL_PROXY_TEXTURE_1D) break;
			if (tex->d == 0 && proxy == GL_PROXY_TEXTURE_3D) break;

			if (proxy == GL_PROXY_TEXTURE_1D)
				glTexImage1D(proxy, 0, internalformat, tex->w, 0, data_format, data_type, NULL);
			else if (proxy == GL_PROXY_TEXTURE_2D)
				glTexImage2D(proxy, 0, internalformat, tex->w, tex->h, 0, data_format, data_type, NULL);
			else if (proxy == GL_PROXY_TEXTURE_3D)
				glTexImage3D(proxy, 0, internalformat, tex->w, tex->h, tex->d, 0, data_format, data_type, NULL);

			glGetTexLevelParameteriv(GL_PROXY_TEXTURE_3D, 0, GL_TEXTURE_WIDTH, &r_width);
		}

		/* Rescale */
		if (r_width > 0) {
			switch (proxy) {
				case GL_PROXY_TEXTURE_1D:
				case GL_PROXY_TEXTURE_2D:
					/* Do nothing for now */
					return false;
				case GL_PROXY_TEXTURE_3D:
					BLI_assert(data_type == GL_FLOAT);
					*rescaled_fpixels = GPU_texture_3D_rescale(tex, w, h, d, channels, fpixels);
					return (bool)*rescaled_fpixels;
			}
		}
	}

	return (r_width > 0);
}

GPUTexture *GPU_texture_create_nD(
        int w, int h, int d, int n, const void *pixels,
        GPUTextureFormat tex_format, GPUDataFormat gpu_data_format, int samples,
        const bool can_rescale, char err_out[256])
{
	if (samples) {
		CLAMP_MAX(samples, GPU_max_color_texture_samples());
	}

	GPUTexture *tex = MEM_callocN(sizeof(GPUTexture), "GPUTexture");
	tex->w = w;
	tex->h = h;
	tex->d = d;
	tex->samples = samples;
	tex->number = -1;
	tex->refcount = 1;
	tex->format = tex_format;
	tex->components = gpu_get_component_count(tex_format);
	tex->bytesize = gpu_get_bytesize(tex_format);
	tex->format_flag = 0;

	if (n == 2) {
		if (d == 0)
			tex->target_base = tex->target = GL_TEXTURE_2D;
		else
			tex->target_base = tex->target = GL_TEXTURE_2D_ARRAY;
	}
	else if (n == 1) {
		if (h == 0)
			tex->target_base = tex->target = GL_TEXTURE_1D;
		else
			tex->target_base = tex->target = GL_TEXTURE_1D_ARRAY;
	}
	else if (n == 3) {
		tex->target_base = tex->target = GL_TEXTURE_3D;
	}
	else {
		/* should never happen */
		MEM_freeN(tex);
		return NULL;
	}

	gpu_validate_data_format(tex_format, gpu_data_format);

	if (samples && n == 2 && d == 0)
		tex->target = GL_TEXTURE_2D_MULTISAMPLE;

	GLenum internalformat = gpu_get_gl_internalformat(tex_format);
	GLenum data_format = gpu_get_gl_dataformat(tex_format, &tex->format_flag);
	GLenum data_type = gpu_get_gl_datatype(gpu_data_format);

	gpu_texture_memory_footprint_add(tex);

	/* Generate Texture object */
	tex->bindcode = GPU_tex_alloc();

	if (!tex->bindcode) {
		if (err_out)
			BLI_snprintf(err_out, 256, "GPUTexture: texture create failed\n");
		else
			fprintf(stderr, "GPUTexture: texture create failed\n");
		GPU_texture_free(tex);
		return NULL;
	}

	glBindTexture(tex->target, tex->bindcode);

	/* Check if texture fit in VRAM */
	GLenum proxy = GL_PROXY_TEXTURE_2D;

	if (n == 2) {
		if (d > 0)
			proxy = GL_PROXY_TEXTURE_2D_ARRAY;
	}
	else if (n == 1) {
		if (h == 0)
			proxy = GL_PROXY_TEXTURE_1D;
		else
			proxy = GL_PROXY_TEXTURE_1D_ARRAY;
	}
	else if (n == 3) {
		proxy = GL_PROXY_TEXTURE_3D;
	}

	float *rescaled_pixels = NULL;
	bool valid = gpu_texture_try_alloc(tex, proxy, internalformat, data_format, data_type, tex->components, can_rescale,
	                                   pixels, &rescaled_pixels);
	if (!valid) {
		if (err_out)
			BLI_snprintf(err_out, 256, "GPUTexture: texture alloc failed");
		else
			fprintf(stderr, "GPUTexture: texture alloc failed. Not enough Video Memory.");
		GPU_texture_free(tex);
		return NULL;
	}

	/* Upload Texture */
	const float *pix = (rescaled_pixels) ? rescaled_pixels : pixels;

	if (tex->target == GL_TEXTURE_2D ||
	    tex->target == GL_TEXTURE_2D_MULTISAMPLE ||
	    tex->target == GL_TEXTURE_1D_ARRAY)
	{
		if (samples) {
			glTexImage2DMultisample(tex->target, samples, internalformat, tex->w, tex->h, true);
			if (pix)
				glTexSubImage2D(tex->target, 0, 0, 0, tex->w, tex->h, data_format, data_type, pix);
		}
		else {
			glTexImage2D(tex->target, 0, internalformat, tex->w, tex->h, 0, data_format, data_type, pix);
		}
	}
	else if (tex->target == GL_TEXTURE_1D) {
		glTexImage1D(tex->target, 0, internalformat, tex->w, 0, data_format, data_type, pix);
	}
	else {
		glTexImage3D(tex->target, 0, internalformat, tex->w, tex->h, tex->d, 0, data_format, data_type, pix);
	}

	if (rescaled_pixels)
		MEM_freeN(rescaled_pixels);

	/* Texture Parameters */
	if (GPU_texture_stencil(tex) || /* Does not support filtering */
	    GPU_texture_integer(tex) || /* Does not support filtering */
	    GPU_texture_depth(tex))
	{
		glTexParameteri(tex->target_base, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(tex->target_base, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	else {
		glTexParameteri(tex->target_base, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(tex->target_base, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	if (GPU_texture_depth(tex)) {
		glTexParameteri(tex->target_base, GL_TEXTURE_COMPARE_MODE, GL_NONE);
		glTexParameteri(tex->target_base, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
	}

	glTexParameteri(tex->target_base, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	if (n > 1) {
		glTexParameteri(tex->target_base, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	if (n > 2) {
		glTexParameteri(tex->target_base, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	}

	glBindTexture(tex->target, 0);

	return tex;
}

static GPUTexture *GPU_texture_cube_create(
        int w, int d,
        const float *fpixels_px, const float *fpixels_py, const float *fpixels_pz,
        const float *fpixels_nx, const float *fpixels_ny, const float *fpixels_nz,
        GPUTextureFormat tex_format, GPUDataFormat gpu_data_format,
        char err_out[256])
{
	GPUTexture *tex = MEM_callocN(sizeof(GPUTexture), "GPUTexture");
	tex->w = w;
	tex->h = w;
	tex->d = d;
	tex->samples = 0;
	tex->number = -1;
	tex->refcount = 1;
	tex->format = tex_format;
	tex->components = gpu_get_component_count(tex_format);
	tex->bytesize = gpu_get_bytesize(tex_format);
	tex->format_flag = GPU_FORMAT_CUBE;

	if (d == 0) {
		tex->target_base = tex->target = GL_TEXTURE_CUBE_MAP;
	}
	else {
		BLI_assert(false && "Cubemap array Not implemented yet");
		// tex->target_base = tex->target = GL_TEXTURE_CUBE_MAP_ARRAY;
	}

	GLenum internalformat = gpu_get_gl_internalformat(tex_format);
	GLenum data_format = gpu_get_gl_dataformat(tex_format, &tex->format_flag);
	GLenum data_type = gpu_get_gl_datatype(gpu_data_format);

	gpu_texture_memory_footprint_add(tex);

	/* Generate Texture object */
	tex->bindcode = GPU_tex_alloc();

	if (!tex->bindcode) {
		if (err_out)
			BLI_snprintf(err_out, 256, "GPUTexture: texture create failed\n");
		else
			fprintf(stderr, "GPUTexture: texture create failed\n");
		GPU_texture_free(tex);
		return NULL;
	}

	glBindTexture(tex->target, tex->bindcode);

	/* Upload Texture */
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, internalformat, tex->w, tex->h, 0, data_format, data_type, fpixels_px);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, internalformat, tex->w, tex->h, 0, data_format, data_type, fpixels_py);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, internalformat, tex->w, tex->h, 0, data_format, data_type, fpixels_pz);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, internalformat, tex->w, tex->h, 0, data_format, data_type, fpixels_nx);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, internalformat, tex->w, tex->h, 0, data_format, data_type, fpixels_ny);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, internalformat, tex->w, tex->h, 0, data_format, data_type, fpixels_nz);

	/* Texture Parameters */
	if (GPU_texture_stencil(tex) || /* Does not support filtering */
	    GPU_texture_integer(tex) || /* Does not support filtering */
	    GPU_texture_depth(tex))
	{
		glTexParameteri(tex->target_base, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(tex->target_base, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	else {
		glTexParameteri(tex->target_base, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(tex->target_base, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	if (GPU_texture_depth(tex)) {
		glTexParameteri(tex->target_base, GL_TEXTURE_COMPARE_MODE, GL_NONE);
		glTexParameteri(tex->target_base, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
	}

	glTexParameteri(tex->target_base, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(tex->target_base, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(tex->target_base, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	glBindTexture(tex->target, 0);

	return tex;
}

/* Special buffer textures. tex_format must be compatible with the buffer content. */
GPUTexture *GPU_texture_create_buffer(GPUTextureFormat tex_format, const GLuint buffer)
{
	GPUTexture *tex = MEM_callocN(sizeof(GPUTexture), "GPUTexture");
	tex->number = -1;
	tex->refcount = 1;
	tex->format = tex_format;
	tex->components = gpu_get_component_count(tex_format);
	tex->format_flag = 0;
	tex->target_base = tex->target = GL_TEXTURE_BUFFER;
	tex->bytesize = gpu_get_bytesize(tex_format);

	GLenum internalformat = gpu_get_gl_internalformat(tex_format);

	gpu_get_gl_dataformat(tex_format, &tex->format_flag);

	if (!(ELEM(tex_format, GPU_R8, GPU_R16) ||
	      ELEM(tex_format, GPU_R16F, GPU_R32F) ||
	      ELEM(tex_format, GPU_R8I, GPU_R16I, GPU_R32I) ||
	      ELEM(tex_format, GPU_R8UI, GPU_R16UI, GPU_R32UI) ||
	      ELEM(tex_format, GPU_RG8, GPU_RG16) ||
	      ELEM(tex_format, GPU_RG16F, GPU_RG32F) ||
	      ELEM(tex_format, GPU_RG8I, GPU_RG16I, GPU_RG32I) ||
	      ELEM(tex_format, GPU_RG8UI, GPU_RG16UI, GPU_RG32UI) ||
	      //ELEM(tex_format, GPU_RGB32F, GPU_RGB32I, GPU_RGB32UI) || /* Not available until gl 4.0 */
	      ELEM(tex_format, GPU_RGBA8, GPU_RGBA16) ||
	      ELEM(tex_format, GPU_RGBA16F, GPU_RGBA32F) ||
	      ELEM(tex_format, GPU_RGBA8I, GPU_RGBA16I, GPU_RGBA32I) ||
	      ELEM(tex_format, GPU_RGBA8UI, GPU_RGBA16UI, GPU_RGBA32UI)))
	{
		fprintf(stderr, "GPUTexture: invalid format for texture buffer\n");
		GPU_texture_free(tex);
		return NULL;
	}

	/* Generate Texture object */
	tex->bindcode = GPU_tex_alloc();

	if (!tex->bindcode) {
		fprintf(stderr, "GPUTexture: texture create failed\n");
		GPU_texture_free(tex);
		BLI_assert(0 && "glGenTextures failled: Are you sure a valid OGL context is active on this thread?\n");
		return NULL;
	}

	glBindTexture(tex->target, tex->bindcode);
	glTexBuffer(tex->target, internalformat, buffer);
	glBindTexture(tex->target, 0);

	return tex;
}

GPUTexture *GPU_texture_from_bindcode(int textarget, int bindcode)
{
	/* see GPUInput::textarget: it can take two values - GL_TEXTURE_2D and GL_TEXTURE_CUBE_MAP
	 * these values are correct for glDisable, so textarget can be safely used in
	 * GPU_texture_bind/GPU_texture_unbind through tex->target_base */
	/* (is any of this obsolete now that we don't glEnable/Disable textures?) */
	GPUTexture *tex = MEM_callocN(sizeof(GPUTexture), "GPUTexture");
	tex->bindcode = bindcode;
	tex->number = -1;
	tex->refcount = 1;
	tex->target = textarget;
	tex->target_base = textarget;
	tex->format = -1;
	tex->components = -1;
	tex->samples = 0;

	if (!glIsTexture(tex->bindcode)) {
		GPU_print_error_debug("Blender Texture Not Loaded");
	}
	else {
		GLint w, h;

		GLenum gettarget;

		if (textarget == GL_TEXTURE_2D)
			gettarget = GL_TEXTURE_2D;
		else
			gettarget = GL_TEXTURE_CUBE_MAP_POSITIVE_X;

		glBindTexture(textarget, tex->bindcode);
		glGetTexLevelParameteriv(gettarget, 0, GL_TEXTURE_WIDTH, &w);
		glGetTexLevelParameteriv(gettarget, 0, GL_TEXTURE_HEIGHT, &h);
		tex->w = w;
		tex->h = h;
		glBindTexture(textarget, 0);
	}

	return tex;
}

GPUTexture *GPU_texture_from_preview(PreviewImage *prv, int mipmap)
{
	GPUTexture *tex = prv->gputexture[0];
	GLuint bindcode = 0;

	if (tex)
		bindcode = tex->bindcode;

	/* this binds a texture, so that's why we restore it to 0 */
	if (bindcode == 0) {
		GPU_create_gl_tex(&bindcode, prv->rect[0], NULL, prv->w[0], prv->h[0], GL_TEXTURE_2D, mipmap, 0, NULL);
	}
	if (tex) {
		tex->bindcode = bindcode;
		glBindTexture(GL_TEXTURE_2D, 0);
		return tex;
	}

	tex = MEM_callocN(sizeof(GPUTexture), "GPUTexture");
	tex->bindcode = bindcode;
	tex->number = -1;
	tex->refcount = 1;
	tex->target = GL_TEXTURE_2D;
	tex->target_base = GL_TEXTURE_2D;
	tex->format = -1;
	tex->components = -1;

	prv->gputexture[0] = tex;

	if (!glIsTexture(tex->bindcode)) {
		GPU_print_error_debug("Blender Texture Not Loaded");
	}
	else {
		GLint w, h;

		glBindTexture(GL_TEXTURE_2D, tex->bindcode);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);

		tex->w = w;
		tex->h = h;
	}

	glBindTexture(GL_TEXTURE_2D, 0);

	return tex;

}

GPUTexture *GPU_texture_create_1D(
        int w, GPUTextureFormat tex_format, const float *pixels, char err_out[256])
{
	GPUDataFormat data_format = gpu_get_data_format_from_tex_format(tex_format);
	return GPU_texture_create_nD(w, 0, 0, 1, pixels, tex_format, data_format, 0, false, err_out);
}

GPUTexture *GPU_texture_create_1D_array(
        int w, int h, GPUTextureFormat tex_format, const float *pixels, char err_out[256])
{
	GPUDataFormat data_format = gpu_get_data_format_from_tex_format(tex_format);
	return GPU_texture_create_nD(w, h, 0, 1, pixels, tex_format, data_format, 0, false, err_out);
}

GPUTexture *GPU_texture_create_2D(
        int w, int h, GPUTextureFormat tex_format, const float *pixels, char err_out[256])
{
	GPUDataFormat data_format = gpu_get_data_format_from_tex_format(tex_format);
	return GPU_texture_create_nD(w, h, 0, 2, pixels, tex_format, data_format, 0, false, err_out);
}

GPUTexture *GPU_texture_create_2D_multisample(
        int w, int h, GPUTextureFormat tex_format, const float *pixels, int samples, char err_out[256])
{
	GPUDataFormat data_format = gpu_get_data_format_from_tex_format(tex_format);
	return GPU_texture_create_nD(w, h, 0, 2, pixels, tex_format, data_format, samples, false, err_out);
}

GPUTexture *GPU_texture_create_2D_array(
        int w, int h, int d, GPUTextureFormat tex_format, const float *pixels, char err_out[256])
{
	GPUDataFormat data_format = gpu_get_data_format_from_tex_format(tex_format);
	return GPU_texture_create_nD(w, h, d, 2, pixels, tex_format, data_format, 0, false, err_out);
}

GPUTexture *GPU_texture_create_3D(
        int w, int h, int d, GPUTextureFormat tex_format, const float *pixels, char err_out[256])
{
	GPUDataFormat data_format = gpu_get_data_format_from_tex_format(tex_format);
	return GPU_texture_create_nD(w, h, d, 3, pixels, tex_format, data_format, 0, true, err_out);
}

GPUTexture *GPU_texture_create_cube(
        int w, GPUTextureFormat tex_format, const float *fpixels, char err_out[256])
{
	const float *fpixels_px, *fpixels_py, *fpixels_pz, *fpixels_nx, *fpixels_ny, *fpixels_nz;
	const int channels = gpu_get_component_count(tex_format);

	if (fpixels) {
		fpixels_px = fpixels + 0 * w * w * channels;
		fpixels_nx = fpixels + 1 * w * w * channels;
		fpixels_py = fpixels + 2 * w * w * channels;
		fpixels_ny = fpixels + 3 * w * w * channels;
		fpixels_pz = fpixels + 4 * w * w * channels;
		fpixels_nz = fpixels + 5 * w * w * channels;
	}
	else {
		fpixels_px = fpixels_py = fpixels_pz = fpixels_nx = fpixels_ny = fpixels_nz = NULL;
	}

	return GPU_texture_cube_create(w, 0, fpixels_px, fpixels_py, fpixels_pz, fpixels_nx, fpixels_ny, fpixels_nz,
	                               tex_format, GPU_DATA_FLOAT, err_out);
}

GPUTexture *GPU_texture_create_from_vertbuf(GPUVertBuf *vert)
{
	GPUVertFormat *format = &vert->format;
	GPUVertAttr *attr = &format->attribs[0];

	/* Detect incompatible cases (not supported by texture buffers) */
	BLI_assert(format->attr_len == 1 && vert->vbo_id != 0);
	BLI_assert(attr->comp_len != 3); /* Not until OGL 4.0 */
	BLI_assert(attr->comp_type != GPU_COMP_I10);
	BLI_assert(attr->fetch_mode != GPU_FETCH_INT_TO_FLOAT);

	unsigned int byte_per_comp = attr->sz / attr->comp_len;
	bool is_uint = ELEM(attr->comp_type, GPU_COMP_U8, GPU_COMP_U16, GPU_COMP_U32);

	/* Cannot fetch signed int or 32bit ints as normalized float. */
	if (attr->fetch_mode == GPU_FETCH_INT_TO_FLOAT_UNIT) {
		BLI_assert(is_uint || byte_per_comp <= 2);
	}

	GPUTextureFormat data_type;
	switch (attr->fetch_mode) {
		case GPU_FETCH_FLOAT:
			switch (attr->comp_len) {
				case 1: data_type = GPU_R32F; break;
				case 2: data_type = GPU_RG32F; break;
				// case 3: data_type = GPU_RGB32F; break; /* Not supported */
				default: data_type = GPU_RGBA32F; break;
			}
			break;
		case GPU_FETCH_INT:
			switch (attr->comp_len) {
				case 1:
					switch (byte_per_comp) {
						case 1: data_type = (is_uint) ? GPU_R8UI : GPU_R8I; break;
						case 2: data_type = (is_uint) ? GPU_R16UI : GPU_R16I; break;
						default: data_type = (is_uint) ? GPU_R32UI : GPU_R32I; break;
					}
					break;
				case 2:
					switch (byte_per_comp) {
						case 1: data_type = (is_uint) ? GPU_RG8UI : GPU_RG8I; break;
						case 2: data_type = (is_uint) ? GPU_RG16UI : GPU_RG16I; break;
						default: data_type = (is_uint) ? GPU_RG32UI : GPU_RG32I; break;
					}
					break;
				default:
					switch (byte_per_comp) {
						case 1: data_type = (is_uint) ? GPU_RGBA8UI : GPU_RGBA8I; break;
						case 2: data_type = (is_uint) ? GPU_RGBA16UI : GPU_RGBA16I; break;
						default: data_type = (is_uint) ? GPU_RGBA32UI : GPU_RGBA32I; break;
					}
					break;
			}
			break;
		case GPU_FETCH_INT_TO_FLOAT_UNIT:
			switch (attr->comp_len) {
				case 1: data_type = (byte_per_comp == 1) ? GPU_R8 : GPU_R16; break;
				case 2: data_type = (byte_per_comp == 1) ? GPU_RG8 : GPU_RG16; break;
				default: data_type = (byte_per_comp == 1) ? GPU_RGBA8 : GPU_RGBA16; break;
			}
			break;
		default:
			BLI_assert(0);
			return NULL;
	}

	return GPU_texture_create_buffer(data_type, vert->vbo_id);
}

void GPU_texture_add_mipmap(
        GPUTexture *tex, GPUDataFormat gpu_data_format, int miplvl, const void *pixels)
{
	BLI_assert((int)tex->format > -1);
	BLI_assert(tex->components > -1);

	gpu_validate_data_format(tex->format, gpu_data_format);

	GLenum internalformat = gpu_get_gl_internalformat(tex->format);
	GLenum data_format = gpu_get_gl_dataformat(tex->format, &tex->format_flag);
	GLenum data_type = gpu_get_gl_datatype(gpu_data_format);

	glBindTexture(tex->target, tex->bindcode);

	int size[3];
	GPU_texture_get_mipmap_size(tex, miplvl, size);

	switch (tex->target) {
		case GL_TEXTURE_1D:
			glTexImage1D(tex->target, miplvl, internalformat, size[0], 0, data_format, data_type, pixels);
			break;
		case GL_TEXTURE_2D:
		case GL_TEXTURE_1D_ARRAY:
			glTexImage2D(tex->target, miplvl, internalformat, size[0], size[1], 0, data_format, data_type, pixels);
			break;
		case GL_TEXTURE_3D:
		case GL_TEXTURE_2D_ARRAY:
			glTexImage3D(tex->target, miplvl, internalformat, size[0], size[1], size[2], 0, data_format, data_type, pixels);
			break;
		case GL_TEXTURE_2D_MULTISAMPLE:
			/* Multisample textures cannot have mipmaps. */
		default:
			BLI_assert(!"tex->target mode not supported");
	}

	glTexParameteri(GPU_texture_target(tex), GL_TEXTURE_MAX_LEVEL, miplvl);

	glBindTexture(tex->target, 0);
}

void GPU_texture_update_sub(
        GPUTexture *tex, GPUDataFormat gpu_data_format, const void *pixels,
        int offset_x, int offset_y, int offset_z, int width, int height, int depth)
{
	BLI_assert((int)tex->format > -1);
	BLI_assert(tex->components > -1);

	GLenum data_format = gpu_get_gl_dataformat(tex->format, &tex->format_flag);
	GLenum data_type = gpu_get_gl_datatype(gpu_data_format);
	GLint alignment;

	/* The default pack size for textures is 4, which won't work for byte based textures */
	if (tex->bytesize == 1) {
		glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	}

	glBindTexture(tex->target, tex->bindcode);
	switch (tex->target) {
		case GL_TEXTURE_1D:
			glTexSubImage1D(tex->target, 0, offset_x, width, data_format, data_type, pixels);
			break;
		case GL_TEXTURE_2D:
		case GL_TEXTURE_2D_MULTISAMPLE:
		case GL_TEXTURE_1D_ARRAY:
			glTexSubImage2D(
			        tex->target, 0, offset_x, offset_y,
			        width, height, data_format, data_type, pixels);
			break;
		case GL_TEXTURE_3D:
		case GL_TEXTURE_2D_ARRAY:
			glTexSubImage3D(
			        tex->target, 0, offset_x, offset_y, offset_z,
			        width, height, depth, data_format, data_type, pixels);
			break;
		default:
			BLI_assert(!"tex->target mode not supported");
	}

	if (tex->bytesize == 1) {
		glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
	}

	glBindTexture(tex->target, 0);
}

void *GPU_texture_read(GPUTexture *tex, GPUDataFormat gpu_data_format, int miplvl)
{
	int size[3] = {0, 0, 0};
	GPU_texture_get_mipmap_size(tex, miplvl, size);

	gpu_validate_data_format(tex->format, gpu_data_format);

	size_t buf_size = gpu_texture_memory_footprint_compute(tex);
	size_t samples_count = max_ii(1, tex->samples);

	samples_count *= size[0];
	samples_count *= max_ii(1, size[1]);
	samples_count *= max_ii(1, size[2]);

	switch (gpu_data_format) {
		case GPU_DATA_FLOAT:
			buf_size = sizeof(float) * samples_count * tex->components;
			break;
		case GPU_DATA_INT:
		case GPU_DATA_UNSIGNED_INT:
			buf_size = sizeof(int) * samples_count * tex->components;
			break;
		case GPU_DATA_UNSIGNED_INT_24_8:
		case GPU_DATA_10_11_11_REV:
			buf_size = sizeof(int) * samples_count;
			break;
		case GPU_DATA_UNSIGNED_BYTE:
			break;
	}

	void *buf = MEM_mallocN(buf_size, "GPU_texture_read");

	GLenum data_format = gpu_get_gl_dataformat(tex->format, &tex->format_flag);
	GLenum data_type = gpu_get_gl_datatype(gpu_data_format);

	glBindTexture(tex->target, tex->bindcode);

	glGetTexImage(tex->target, miplvl, data_format, data_type, buf);

	glBindTexture(tex->target, 0);

	return buf;
}

void GPU_texture_update(GPUTexture *tex, GPUDataFormat data_format, const void *pixels)
{
	GPU_texture_update_sub(tex, data_format, pixels, 0, 0, 0, tex->w, tex->h, tex->d);
}

void GPU_invalid_tex_init(void)
{
	memory_usage = 0;
	const float color[4] = {1.0f, 0.0f, 1.0f, 1.0f};
	GG.invalid_tex_1D = GPU_texture_create_1D(1, GPU_RGBA8, color, NULL);
	GG.invalid_tex_2D = GPU_texture_create_2D(1, 1, GPU_RGBA8, color, NULL);
	GG.invalid_tex_3D = GPU_texture_create_3D(1, 1, 1, GPU_RGBA8, color, NULL);
}

void GPU_invalid_tex_bind(int mode)
{
	switch (mode) {
		case GL_TEXTURE_1D:
			glBindTexture(GL_TEXTURE_1D, GG.invalid_tex_1D->bindcode);
			break;
		case GL_TEXTURE_2D:
			glBindTexture(GL_TEXTURE_2D, GG.invalid_tex_2D->bindcode);
			break;
		case GL_TEXTURE_3D:
			glBindTexture(GL_TEXTURE_3D, GG.invalid_tex_3D->bindcode);
			break;
	}
}

void GPU_invalid_tex_free(void)
{
	if (GG.invalid_tex_1D)
		GPU_texture_free(GG.invalid_tex_1D);
	if (GG.invalid_tex_2D)
		GPU_texture_free(GG.invalid_tex_2D);
	if (GG.invalid_tex_3D)
		GPU_texture_free(GG.invalid_tex_3D);
}

void GPU_texture_bind(GPUTexture *tex, int number)
{
	BLI_assert(number >= 0);

	if (number >= GPU_max_textures()) {
		fprintf(stderr, "Not enough texture slots.\n");
		return;
	}

	if ((G.debug & G_DEBUG)) {
		for (int i = 0; i < GPU_TEX_MAX_FBO_ATTACHED; ++i) {
			if (tex->fb[i] && GPU_framebuffer_bound(tex->fb[i])) {
				fprintf(stderr,
				        "Feedback loop warning!: Attempting to bind "
				        "texture attached to current framebuffer!\n");
				BLI_assert(0); /* Should never happen! */
				break;
			}
		}
	}

	glActiveTexture(GL_TEXTURE0 + number);

	if (tex->bindcode != 0)
		glBindTexture(tex->target, tex->bindcode);
	else
		GPU_invalid_tex_bind(tex->target_base);

	tex->number = number;
}

void GPU_texture_unbind(GPUTexture *tex)
{
	if (tex->number == -1)
		return;

	glActiveTexture(GL_TEXTURE0 + tex->number);
	glBindTexture(tex->target, 0);

	tex->number = -1;
}

int GPU_texture_bound_number(GPUTexture *tex)
{
	return tex->number;
}

#define WARN_NOT_BOUND(_tex) do { \
	if (_tex->number == -1) { \
		fprintf(stderr, "Warning : Trying to set parameter on a texture not bound.\n"); \
		BLI_assert(0); \
		return; \
	} \
} while (0);

void GPU_texture_generate_mipmap(GPUTexture *tex)
{
	WARN_NOT_BOUND(tex);

	glActiveTexture(GL_TEXTURE0 + tex->number);
	glGenerateMipmap(tex->target_base);
}

void GPU_texture_compare_mode(GPUTexture *tex, bool use_compare)
{
	WARN_NOT_BOUND(tex);

	/* Could become an assertion ? (fclem) */
	if (!GPU_texture_depth(tex))
		return;

	GLenum mode = (use_compare) ? GL_COMPARE_REF_TO_TEXTURE : GL_NONE;

	glActiveTexture(GL_TEXTURE0 + tex->number);
	glTexParameteri(tex->target_base, GL_TEXTURE_COMPARE_MODE, mode);
}

void GPU_texture_filter_mode(GPUTexture *tex, bool use_filter)
{
	WARN_NOT_BOUND(tex);

	/* Stencil and integer format does not support filtering. */
	BLI_assert(!use_filter || !(GPU_texture_stencil(tex) || GPU_texture_integer(tex)));

	GLenum filter = (use_filter) ? GL_LINEAR : GL_NEAREST;

	glActiveTexture(GL_TEXTURE0 + tex->number);
	glTexParameteri(tex->target_base, GL_TEXTURE_MAG_FILTER, filter);
	glTexParameteri(tex->target_base, GL_TEXTURE_MIN_FILTER, filter);
}

void GPU_texture_mipmap_mode(GPUTexture *tex, bool use_mipmap, bool use_filter)
{
	WARN_NOT_BOUND(tex);

	/* Stencil and integer format does not support filtering. */
	BLI_assert((!use_filter && !use_mipmap) || !(GPU_texture_stencil(tex) || GPU_texture_integer(tex)));

	GLenum filter = (use_filter) ? GL_LINEAR : GL_NEAREST;
	GLenum mipmap = (
	        (use_filter) ?
	        (use_mipmap) ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR :
	        (use_mipmap) ? GL_NEAREST_MIPMAP_LINEAR : GL_NEAREST);

	glActiveTexture(GL_TEXTURE0 + tex->number);
	glTexParameteri(tex->target_base, GL_TEXTURE_MIN_FILTER, mipmap);
	glTexParameteri(tex->target_base, GL_TEXTURE_MAG_FILTER, filter);
}

void GPU_texture_wrap_mode(GPUTexture *tex, bool use_repeat)
{
	WARN_NOT_BOUND(tex);

	GLenum repeat = (use_repeat) ? GL_REPEAT : GL_CLAMP_TO_EDGE;

	glActiveTexture(GL_TEXTURE0 + tex->number);
	glTexParameteri(tex->target_base, GL_TEXTURE_WRAP_S, repeat);
	if (tex->target_base != GL_TEXTURE_1D)
		glTexParameteri(tex->target_base, GL_TEXTURE_WRAP_T, repeat);
	if (tex->target_base == GL_TEXTURE_3D)
		glTexParameteri(tex->target_base, GL_TEXTURE_WRAP_R, repeat);
}

static GLenum gpu_get_gl_filterfunction(GPUFilterFunction filter)
{
	switch (filter) {
		case GPU_NEAREST:
			return GL_NEAREST;
		case GPU_LINEAR:
			return GL_LINEAR;
		default:
			BLI_assert(!"Unhandled filter mode");
			return GL_NEAREST;
	}
}

void GPU_texture_filters(GPUTexture *tex, GPUFilterFunction min_filter, GPUFilterFunction mag_filter)
{
	WARN_NOT_BOUND(tex);

	/* Stencil and integer format does not support filtering. */
	BLI_assert(!(GPU_texture_stencil(tex) || GPU_texture_integer(tex)));
	BLI_assert(mag_filter == GPU_NEAREST || mag_filter == GPU_LINEAR);

	glActiveTexture(GL_TEXTURE0 + tex->number);
	glTexParameteri(tex->target_base, GL_TEXTURE_MIN_FILTER, gpu_get_gl_filterfunction(min_filter));
	glTexParameteri(tex->target_base, GL_TEXTURE_MAG_FILTER, gpu_get_gl_filterfunction(mag_filter));
}

void GPU_texture_free(GPUTexture *tex)
{
	tex->refcount--;

	if (tex->refcount < 0)
		fprintf(stderr, "GPUTexture: negative refcount\n");

	if (tex->refcount == 0) {
		for (int i = 0; i < GPU_TEX_MAX_FBO_ATTACHED; ++i) {
			if (tex->fb[i] != NULL) {
				GPU_framebuffer_texture_detach_slot(tex->fb[i], tex, tex->fb_attachment[i]);
			}
		}

		if (tex->bindcode)
			GPU_tex_free(tex->bindcode);

		gpu_texture_memory_footprint_remove(tex);

		MEM_freeN(tex);
	}
}

void GPU_texture_ref(GPUTexture *tex)
{
	tex->refcount++;
}

int GPU_texture_target(const GPUTexture *tex)
{
	return tex->target;
}

int GPU_texture_width(const GPUTexture *tex)
{
	return tex->w;
}

int GPU_texture_height(const GPUTexture *tex)
{
	return tex->h;
}

int GPU_texture_layers(const GPUTexture *tex)
{
	return tex->d;
}

GPUTextureFormat GPU_texture_format(const GPUTexture *tex)
{
	return tex->format;
}

int GPU_texture_samples(const GPUTexture *tex)
{
	return tex->samples;
}

bool GPU_texture_depth(const GPUTexture *tex)
{
	return (tex->format_flag & GPU_FORMAT_DEPTH) != 0;
}

bool GPU_texture_stencil(const GPUTexture *tex)
{
	return (tex->format_flag & GPU_FORMAT_STENCIL) != 0;
}

bool GPU_texture_integer(const GPUTexture *tex)
{
	return (tex->format_flag & GPU_FORMAT_INTEGER) != 0;
}

bool GPU_texture_cube(const GPUTexture *tex)
{
	return (tex->format_flag & GPU_FORMAT_CUBE) != 0;
}

int GPU_texture_opengl_bindcode(const GPUTexture *tex)
{
	return tex->bindcode;
}

void GPU_texture_attach_framebuffer(GPUTexture *tex, GPUFrameBuffer *fb, int attachment)
{
	for (int i = 0; i < GPU_TEX_MAX_FBO_ATTACHED; ++i) {
		if (tex->fb[i] == NULL) {
			tex->fb[i] = fb;
			tex->fb_attachment[i] = attachment;
			return;
		}
	}

	BLI_assert(!"Error: Texture: Not enough Framebuffer slots");
}

/* Return previous attachment point */
int GPU_texture_detach_framebuffer(GPUTexture *tex, GPUFrameBuffer *fb)
{
	for (int i = 0; i < GPU_TEX_MAX_FBO_ATTACHED; ++i) {
		if (tex->fb[i] == fb) {
			tex->fb[i] = NULL;
			return tex->fb_attachment[i];
		}
	}

	BLI_assert(!"Error: Texture: Framebuffer is not attached");
	return 0;
}

void GPU_texture_get_mipmap_size(GPUTexture *tex, int lvl, int *size)
{
	/* TODO assert if lvl is bellow the limit of 1px in each dimension. */
	int div = 1 << lvl;
	size[0] = max_ii(1, tex->w / div);

	if (tex->target == GL_TEXTURE_1D_ARRAY) {
		size[1] = tex->h;
	}
	else if (tex->h > 0) {
		size[1] = max_ii(1, tex->h / div);
	}

	if (tex->target == GL_TEXTURE_2D_ARRAY) {
		size[2] = tex->d;
	}
	else if (tex->d > 0) {
		size[2] = max_ii(1, tex->d / div);
	}
}
