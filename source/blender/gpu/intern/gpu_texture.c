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

#include "BKE_global.h"

#include "GPU_debug.h"
#include "GPU_draw.h"
#include "GPU_extensions.h"
#include "GPU_framebuffer.h"
#include "GPU_glew.h"
#include "GPU_texture.h"

static struct GPUTextureGlobal {
	GPUTexture *invalid_tex_1D; /* texture used in place of invalid textures (not loaded correctly, missing) */
	GPUTexture *invalid_tex_2D;
	GPUTexture *invalid_tex_3D;
} GG = {NULL, NULL, NULL};

/* GPUTexture */
struct GPUTexture {
	int w, h, d;        /* width/height/depth */
	int number;         /* number for multitexture binding */
	int refcount;       /* reference count */
	GLenum target;      /* GL_TEXTURE_* */
	GLenum target_base; /* same as target, (but no multisample)
	                     * use it for unbinding */
	GLuint bindcode;    /* opengl identifier for texture */
	int fromblender;    /* we got the texture from Blender */

	GPUFrameBuffer *fb; /* GPUFramebuffer this texture is attached to */
	int fb_attachment;  /* slot the texture is attached to */
	bool depth;         /* is a depth texture? */
	bool stencil;       /* is a stencil texture? */
};

static GLenum GPU_texture_get_format(int components, GPUTextureFormat data_type, GLenum *format, GLenum *data_format, bool *is_depth, bool *is_stencil)
{
	if (data_type == GPU_DEPTH_COMPONENT24 ||
	    data_type == GPU_DEPTH_COMPONENT16 ||
	    data_type == GPU_DEPTH_COMPONENT32F)
	{
		*is_depth = true;
		*is_stencil = false;
		*data_format = GL_FLOAT;
		*format = GL_DEPTH_COMPONENT;
	}
	else if (data_type == GPU_DEPTH24_STENCIL8) {
		*is_depth = true;
		*is_stencil = true;
		*data_format = GL_UNSIGNED_INT_24_8;
		*format = GL_DEPTH_STENCIL;
	}
	else {
		*is_depth = false;
		*is_stencil = false;
		*data_format = GL_FLOAT;

		switch (components) {
			case 1: *format = GL_RED; break;
			case 2: *format = GL_RG; break;
			case 3: *format = GL_RGB; break;
			case 4: *format = GL_RGBA; break;
			default: break;
		}
	}

	/* You can add any of the available type to this list
	 * For available types see GPU_texture.h */
	switch (data_type) {
		/* Formats texture & renderbuffer */
		case GPU_RGBA16F: return GL_RGBA16F;
		case GPU_RG32F: return GL_RG32F;
		case GPU_RG16F: return GL_RG16F;
		case GPU_RGBA8: return GL_RGBA8;
		case GPU_R16F: return GL_R16F;
		case GPU_R8: return GL_R8;
		/* Special formats texture & renderbuffer */
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
			fprintf(stderr, "Texture format incorrect or unsupported\n");
			return 0;
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
static bool GPU_texture_try_alloc(
        GPUTexture *tex, GLenum proxy, GLenum internalformat, GLenum format, GLenum data_format,
        int channels, bool try_rescale, const float *fpixels, float **rescaled_fpixels)
{
	int r_width;

	switch (proxy) {
		case GL_PROXY_TEXTURE_1D:
			glTexImage1D(proxy, 0, internalformat, tex->w, 0, format, data_format, NULL);
			break;
		case GL_PROXY_TEXTURE_2D:
			glTexImage2D(proxy, 0, internalformat, tex->w, tex->h, 0, format, data_format, NULL);
			break;
		case GL_PROXY_TEXTURE_3D:
			glTexImage3D(proxy, 0, internalformat, tex->w, tex->h, tex->d, 0, format, data_format, NULL);
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
				glTexImage1D(proxy, 0, internalformat, tex->w, 0, format, data_format, NULL);
			else if (proxy == GL_PROXY_TEXTURE_2D)
				glTexImage2D(proxy, 0, internalformat, tex->w, tex->h, 0, format, data_format, NULL);
			else if (proxy == GL_PROXY_TEXTURE_3D)
				glTexImage3D(proxy, 0, internalformat, tex->w, tex->h, tex->d, 0, format, data_format, NULL);

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
					*rescaled_fpixels = GPU_texture_3D_rescale(tex, w, h, d, channels, fpixels);
					return (bool)*rescaled_fpixels;
			}
		}
	}

	return (r_width > 0);
}

static GPUTexture *GPU_texture_create_nD(
        int w, int h, int d, int n, const float *fpixels,
        GPUTextureFormat data_type, int components, int samples,
        const bool can_rescale, char err_out[256])
{
	GLenum format, internalformat, proxy, data_format;
	float *rescaled_fpixels = NULL;
	const float *pix;
	bool valid;

	if (samples) {
		CLAMP_MAX(samples, GPU_max_color_texture_samples());
	}

	GPUTexture *tex = MEM_callocN(sizeof(GPUTexture), "GPUTexture");
	tex->w = w;
	tex->h = h;
	tex->d = d;
	tex->number = -1;
	tex->refcount = 1;
	tex->fb_attachment = -1;

	if (n == 1) {
		if (h == 0)
			tex->target_base = tex->target = GL_TEXTURE_1D;
		else
			tex->target_base = tex->target = GL_TEXTURE_1D_ARRAY;
	}
	else if (n == 2) {
		if (d == 0)
			tex->target_base = tex->target = GL_TEXTURE_2D;
		else
			tex->target_base = tex->target = GL_TEXTURE_2D_ARRAY;
	}
	else if (n == 3) {
		tex->target_base = tex->target = GL_TEXTURE_3D;
	}

	if (samples && n == 2 && d == 0)
		tex->target = GL_TEXTURE_2D_MULTISAMPLE;

	internalformat = GPU_texture_get_format(components, data_type, &format, &data_format, &tex->depth, &tex->stencil);

	/* Generate Texture object */
	glGenTextures(1, &tex->bindcode);

	if (!tex->bindcode) {
		if (err_out)
			BLI_snprintf(err_out, 256, "GPUTexture: texture create failed");
		else
			fprintf(stderr, "GPUTexture: texture create failed");
		GPU_texture_free(tex);
		return NULL;
	}

	tex->number = 0;
	glBindTexture(tex->target, tex->bindcode);

	/* Check if texture fit in VRAM */
	if (d > 0) {
		proxy = GL_PROXY_TEXTURE_3D;
	}
	else if (h > 0) {
		proxy = GL_PROXY_TEXTURE_2D;
	}
	else {
		proxy = GL_PROXY_TEXTURE_1D;
	}

	valid = GPU_texture_try_alloc(tex, proxy, internalformat, format, data_format, components, can_rescale, fpixels,
	                              &rescaled_fpixels);

	if (!valid) {
		if (err_out)
			BLI_snprintf(err_out, 256, "GPUTexture: texture alloc failed");
		else
			fprintf(stderr, "GPUTexture: texture alloc failed. Not enough Video Memory.");
		GPU_texture_free(tex);
		return NULL;
	}

	/* Upload Texture */
	pix = (rescaled_fpixels) ? rescaled_fpixels : fpixels;

	if (tex->target == GL_TEXTURE_1D) {
		glTexImage1D(tex->target, 0, internalformat, tex->w, 0, format, data_format, pix);
	}
	else if (tex->target == GL_TEXTURE_1D_ARRAY ||
	         tex->target == GL_TEXTURE_2D ||
	         tex->target == GL_TEXTURE_2D_MULTISAMPLE)
	{
		if (samples) {
			glTexImage2DMultisample(tex->target, samples, internalformat, tex->w, tex->h, true);
			if (pix)
				glTexSubImage2D(tex->target, 0, 0, 0, tex->w, tex->h, format, data_format, pix);
		}
		else {
			glTexImage2D(tex->target, 0, internalformat, tex->w, tex->h, 0, format, data_format, pix);
		}
	}
	else {
		glTexImage3D(tex->target, 0, internalformat, tex->w, tex->h, tex->d, 0, format, data_format, pix);
	}

	if (rescaled_fpixels)
		MEM_freeN(rescaled_fpixels);

	/* Texture Parameters */
	if (tex->depth) {
		glTexParameteri(tex->target_base, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(tex->target_base, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(tex->target_base, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
		glTexParameteri(tex->target_base, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
		glTexParameteri(tex->target_base, GL_DEPTH_TEXTURE_MODE, GL_INTENSITY);
	}
	else {
		glTexParameteri(tex->target_base, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(tex->target_base, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	glTexParameteri(tex->target_base, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	if (n > 1)	{
		glTexParameteri(tex->target_base, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	if (n > 2)	{
		glTexParameteri(tex->target_base, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	}

	GPU_texture_unbind(tex);

	return tex;
}

static GPUTexture *GPU_texture_cube_create(
        int w, int d,
        const float *fpixels_px, const float *fpixels_py, const float *fpixels_pz,
        const float *fpixels_nx, const float *fpixels_ny, const float *fpixels_nz,
        GPUTextureFormat data_type, int components,
        char err_out[256])
{
	GLenum format, internalformat, data_format;

	GPUTexture *tex = MEM_callocN(sizeof(GPUTexture), "GPUTexture");
	tex->w = w;
	tex->h = w;
	tex->d = d;
	tex->number = -1;
	tex->refcount = 1;
	tex->fb_attachment = -1;

	if (d == 0) {
		tex->target_base = tex->target = GL_TEXTURE_CUBE_MAP;
	}
	else {
		BLI_assert(false && "Cubemap array Not implemented yet");
		// tex->target_base = tex->target = GL_TEXTURE_CUBE_MAP_ARRAY;
	}

	internalformat = GPU_texture_get_format(components, data_type, &format, &data_format, &tex->depth, &tex->stencil);

	/* Generate Texture object */
	glGenTextures(1, &tex->bindcode);

	if (!tex->bindcode) {
		if (err_out)
			BLI_snprintf(err_out, 256, "GPUTexture: texture create failed");
		else
			fprintf(stderr, "GPUTexture: texture create failed");
		GPU_texture_free(tex);
		return NULL;
	}

	tex->number = 0;
	glBindTexture(tex->target, tex->bindcode);

	/* Upload Texture */
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, internalformat, tex->w, tex->h, 0, format, data_format, fpixels_px);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, internalformat, tex->w, tex->h, 0, format, data_format, fpixels_py);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, internalformat, tex->w, tex->h, 0, format, data_format, fpixels_pz);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, internalformat, tex->w, tex->h, 0, format, data_format, fpixels_nx);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, internalformat, tex->w, tex->h, 0, format, data_format, fpixels_ny);
	glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, internalformat, tex->w, tex->h, 0, format, data_format, fpixels_nz);

	/* Texture Parameters */
	if (tex->depth) {
		glTexParameteri(tex->target_base, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(tex->target_base, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(tex->target_base, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
		glTexParameteri(tex->target_base, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
		glTexParameteri(tex->target_base, GL_DEPTH_TEXTURE_MODE, GL_INTENSITY);
	}
	else {
		glTexParameteri(tex->target_base, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(tex->target_base, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	glTexParameteri(tex->target_base, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(tex->target_base, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(tex->target_base, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	GPU_texture_unbind(tex);

	return tex;
}

GPUTexture *GPU_texture_from_blender(Image *ima, ImageUser *iuser, int textarget, bool is_data, double time, int mipmap)
{
	int gputt;
	/* this binds a texture, so that's why to restore it to 0 */
	GLint bindcode = GPU_verify_image(ima, iuser, textarget, 0, 0, mipmap, is_data);
	GPU_update_image_time(ima, time);

	/* see GPUInput::textarget: it can take two values - GL_TEXTURE_2D and GL_TEXTURE_CUBE_MAP
	 * these values are correct for glDisable, so textarget can be safely used in
	 * GPU_texture_bind/GPU_texture_unbind through tex->target_base */
	if (textarget == GL_TEXTURE_2D)
		gputt = TEXTARGET_TEXTURE_2D;
	else
		gputt = TEXTARGET_TEXTURE_CUBE_MAP;

	if (ima->gputexture[gputt]) {
		ima->gputexture[gputt]->bindcode = bindcode;
		glBindTexture(textarget, 0);
		return ima->gputexture[gputt];
	}

	GPUTexture *tex = MEM_callocN(sizeof(GPUTexture), "GPUTexture");
	tex->bindcode = bindcode;
	tex->number = -1;
	tex->refcount = 1;
	tex->target = textarget;
	tex->target_base = textarget;
	tex->fromblender = 1;

	ima->gputexture[gputt] = tex;

	if (!glIsTexture(tex->bindcode)) {
		GPU_print_error_debug("Blender Texture Not Loaded");
	}
	else {
		GLint w, h, border;

		GLenum gettarget;

		if (textarget == GL_TEXTURE_2D)
			gettarget = GL_TEXTURE_2D;
		else
			gettarget = GL_TEXTURE_CUBE_MAP_POSITIVE_X;

		glBindTexture(textarget, tex->bindcode);
		glGetTexLevelParameteriv(gettarget, 0, GL_TEXTURE_WIDTH, &w);
		glGetTexLevelParameteriv(gettarget, 0, GL_TEXTURE_HEIGHT, &h);
		glGetTexLevelParameteriv(gettarget, 0, GL_TEXTURE_BORDER, &border);

		tex->w = w - border;
		tex->h = h - border;
	}

	glBindTexture(textarget, 0);

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

GPUTexture *GPU_texture_create_1D(int w, const float *pixels, char err_out[256])
{
	return GPU_texture_create_nD(w, 0, 0, 1, pixels, GPU_RGBA8, 4, 0, false, err_out);
}

GPUTexture *GPU_texture_create_1D_custom(
        int w, int channels, GPUTextureFormat data_type, const float *pixels, char err_out[256])
{
	return GPU_texture_create_nD(w, 0, 0, 1, pixels, data_type, channels, 0, false, err_out);
}

GPUTexture *GPU_texture_create_2D(int w, int h, const float *pixels, char err_out[256])
{
	return GPU_texture_create_nD(w, h, 0, 2, pixels, GPU_RGBA8, 4, 0, false, err_out);
}

GPUTexture *GPU_texture_create_2D_custom(
        int w, int h, int channels, GPUTextureFormat data_type, const float *pixels, char err_out[256])
{
	return GPU_texture_create_nD(w, h, 0, 2, pixels, data_type, channels, 0, false, err_out);
}

GPUTexture *GPU_texture_create_2D_multisample(int w, int h, const float *pixels, int samples, char err_out[256])
{
	return GPU_texture_create_nD(w, h, 0, 2, pixels, GPU_RGBA8, 4, samples, false, err_out);
}

GPUTexture *GPU_texture_create_2D_array_custom(int w, int h, int d, int channels, GPUTextureFormat data_type, const float *pixels, char err_out[256])
{
	return GPU_texture_create_nD(w, h, d, 2, pixels, data_type, channels, 0, false, err_out);
}

GPUTexture *GPU_texture_create_3D(int w, int h, int d, const float *pixels, char err_out[256])
{
	return GPU_texture_create_nD(w, h, d, 3, pixels, GPU_RGBA8, 4, 0, true, err_out);
}

GPUTexture *GPU_texture_create_3D_custom(int w, int h, int d, int channels, GPUTextureFormat data_type, const float *pixels, char err_out[256])
{
	return GPU_texture_create_nD(w, h, d, 3, pixels, data_type, channels, 0, true, err_out);
}
GPUTexture *GPU_texture_create_cube_custom(int w, int channels, GPUTextureFormat data_type, const float *fpixels, char err_out[256])
{
	const float *fpixels_px, *fpixels_py, *fpixels_pz, *fpixels_nx, *fpixels_ny, *fpixels_nz;

	if (fpixels) {
		fpixels_px = fpixels + 0 * w * w;
		fpixels_py = fpixels + 1 * w * w;
		fpixels_pz = fpixels + 2 * w * w;
		fpixels_nx = fpixels + 3 * w * w;
		fpixels_ny = fpixels + 4 * w * w;
		fpixels_nz = fpixels + 5 * w * w;
	}
	else {
		fpixels_px = fpixels_py = fpixels_pz = fpixels_nx = fpixels_ny = fpixels_nz = NULL;
	}

	return GPU_texture_cube_create(w, 0, fpixels_px, fpixels_py, fpixels_pz, fpixels_nx, fpixels_ny, fpixels_nz, data_type, channels, err_out);
}

GPUTexture *GPU_texture_create_depth(int w, int h, char err_out[256])
{
	return GPU_texture_create_nD(w, h, 0, 2, NULL, GPU_DEPTH_COMPONENT24, 1, 0, false, err_out);
}

GPUTexture *GPU_texture_create_depth_with_stencil(int w, int h, char err_out[256])
{
	return GPU_texture_create_nD(w, h, 0, 2, NULL, GPU_DEPTH24_STENCIL8, 1, 0, false, err_out);
}

GPUTexture *GPU_texture_create_depth_multisample(int w, int h, int samples, char err_out[256])
{
	return GPU_texture_create_nD(w, h, 0, 2, NULL, GPU_DEPTH_COMPONENT24, 1, samples, false, err_out);
}

void GPU_invalid_tex_init(void)
{
	const float color[4] = {1.0f, 0.0f, 1.0f, 1.0f};
	GG.invalid_tex_1D = GPU_texture_create_1D(1, color, NULL);
	GG.invalid_tex_2D = GPU_texture_create_2D(1, 1, color, NULL);
	GG.invalid_tex_3D = GPU_texture_create_3D(1, 1, 1, color, NULL);
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
	if (number >= GPU_max_textures()) {
		fprintf(stderr, "Not enough texture slots.\n");
		return;
	}

	if ((G.debug & G_DEBUG)) {
		if (tex->fb && GPU_framebuffer_bound(tex->fb)) {
			fprintf(stderr, "Feedback loop warning!: Attempting to bind texture attached to current framebuffer!\n");
		}
	}

	if (number < 0)
		return;

	if (number != 0)
		glActiveTexture(GL_TEXTURE0 + number);

	if (tex->bindcode != 0)
		glBindTexture(tex->target_base, tex->bindcode);
	else
		GPU_invalid_tex_bind(tex->target_base);

	/* TODO: remove this lines once we're using GLSL everywhere */
	GLenum target = tex->target_base;
	if (tex->target_base == GL_TEXTURE_1D_ARRAY)
		target = GL_TEXTURE_2D;
	if (tex->target_base == GL_TEXTURE_2D_ARRAY)
		target = GL_TEXTURE_3D;
	glEnable(target);

	if (number != 0)
		glActiveTexture(GL_TEXTURE0);

	tex->number = number;
}

void GPU_texture_unbind(GPUTexture *tex)
{
	if (tex->number >= GPU_max_textures()) {
		fprintf(stderr, "Not enough texture slots.\n");
		return;
	}

	if (tex->number == -1)
		return;

	if (tex->number != 0)
		glActiveTexture(GL_TEXTURE0 + tex->number);

	glBindTexture(tex->target_base, 0);

	/* TODO: remove this lines */
	GLenum target = tex->target_base;
	if (tex->target_base == GL_TEXTURE_1D_ARRAY)
		target = GL_TEXTURE_2D;
	if (tex->target_base == GL_TEXTURE_2D_ARRAY)
		target = GL_TEXTURE_3D;
	glDisable(target);

	if (tex->number != 0)
		glActiveTexture(GL_TEXTURE0);

	tex->number = -1;
}

int GPU_texture_bound_number(GPUTexture *tex)
{
	return tex->number;
}

void GPU_texture_compare_mode(GPUTexture *tex, bool use_compare)
{
	if (tex->number >= GPU_max_textures()) {
		fprintf(stderr, "Not enough texture slots.\n");
		return;
	}

	if (tex->number == -1)
		return;

	if (tex->number != 0)
		glActiveTexture(GL_TEXTURE0 + tex->number);

	/* TODO viewport: use GL_COMPARE_REF_TO_TEXTURE after we switch to core profile */
	if (tex->depth)
		glTexParameteri(tex->target_base, GL_TEXTURE_COMPARE_MODE, use_compare ? GL_COMPARE_R_TO_TEXTURE : GL_NONE);

	if (tex->number != 0)
		glActiveTexture(GL_TEXTURE0);
}

void GPU_texture_filter_mode(GPUTexture *tex, bool use_filter)
{
	if (tex->number >= GPU_max_textures()) {
		fprintf(stderr, "Not enough texture slots.\n");
		return;
	}

	if (tex->number == -1)
		return;

	if (tex->number != 0)
		glActiveTexture(GL_TEXTURE0 + tex->number);

	GLenum filter = use_filter ? GL_LINEAR : GL_NEAREST;
	glTexParameteri(tex->target_base, GL_TEXTURE_MAG_FILTER, filter);
	glTexParameteri(tex->target_base, GL_TEXTURE_MIN_FILTER, filter);

	if (tex->number != 0)
		glActiveTexture(GL_TEXTURE0);
}

void GPU_texture_wrap_mode(GPUTexture *tex, bool use_repeat)
{
	if (tex->number >= GPU_max_textures()) {
		fprintf(stderr, "Not enough texture slots.\n");
		return;
	}

	if (tex->number == -1)
		return;

	if (tex->number != 0)
		glActiveTexture(GL_TEXTURE0 + tex->number);

	GLenum repeat = use_repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
	glTexParameteri(tex->target_base, GL_TEXTURE_WRAP_S, repeat);
	if (tex->target_base != GL_TEXTURE_1D)
		glTexParameteri(tex->target_base, GL_TEXTURE_WRAP_T, repeat);
	if (tex->target_base == GL_TEXTURE_3D)
		glTexParameteri(tex->target_base, GL_TEXTURE_WRAP_R, repeat);

	if (tex->number != 0)
		glActiveTexture(GL_TEXTURE0);
}

void GPU_texture_free(GPUTexture *tex)
{
	tex->refcount--;

	if (tex->refcount < 0)
		fprintf(stderr, "GPUTexture: negative refcount\n");
	
	if (tex->refcount == 0) {
		if (tex->fb)
			GPU_framebuffer_texture_detach(tex);
		if (tex->bindcode && !tex->fromblender)
			glDeleteTextures(1, &tex->bindcode);

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

bool GPU_texture_depth(const GPUTexture *tex)
{
	return tex->depth;
}

bool GPU_texture_stencil(const GPUTexture *tex)
{
	return tex->stencil;
}

int GPU_texture_opengl_bindcode(const GPUTexture *tex)
{
	return tex->bindcode;
}

GPUFrameBuffer *GPU_texture_framebuffer(GPUTexture *tex)
{
	return tex->fb;
}

int GPU_texture_framebuffer_attachment(GPUTexture *tex)
{
	return tex->fb_attachment;
}

void GPU_texture_framebuffer_set(GPUTexture *tex, GPUFrameBuffer *fb, int attachment)
{
	tex->fb = fb;
	tex->fb_attachment = attachment;
}

