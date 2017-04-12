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
	int w, h;           /* width/height */
	int number;         /* number for multitexture binding */
	int refcount;       /* reference count */
	GLenum target;      /* GL_TEXTURE_* */
	GLenum target_base; /* same as target, (but no multisample)
	                     * use it for unbinding */
	GLuint bindcode;    /* opengl identifier for texture */
	int fromblender;    /* we got the texture from Blender */

	GPUFrameBuffer *fb; /* GPUFramebuffer this texture is attached to */
	int fb_attachment;  /* slot the texture is attached to */
	int depth;          /* is a depth texture? if 3D how deep? */
};

static unsigned char *GPU_texture_convert_pixels(int length, const float *fpixels)
{
	unsigned char *pixels, *p;
	const float *fp = fpixels;
	const int len = 4 * length;

	p = pixels = MEM_callocN(sizeof(unsigned char) * len, "GPUTexturePixels");

	for (int a = 0; a < len; a++, p++, fp++)
		*p = FTOCHAR((*fp));

	return pixels;
}

static void gpu_glTexSubImageEmpty(GLenum target, GLenum format, int x, int y, int w, int h)
{
	void *pixels = MEM_callocN(sizeof(char) * 4 * w * h, "GPUTextureEmptyPixels");

	if (target == GL_TEXTURE_1D)
		glTexSubImage1D(target, 0, x, w, format, GL_UNSIGNED_BYTE, pixels);
	else
		glTexSubImage2D(target, 0, x, y, w, h, format, GL_UNSIGNED_BYTE, pixels);
	
	MEM_freeN(pixels);
}

static GPUTexture *GPU_texture_create_nD(
        int w, int h, int n, const float *fpixels, int depth,
        GPUHDRType hdr_type, int components, int samples,
        char err_out[256])
{
	GLenum type, format, internalformat;
	void *pixels = NULL;

	if (samples) {
		CLAMP_MAX(samples, GPU_max_color_texture_samples());
	}

	GPUTexture *tex = MEM_callocN(sizeof(GPUTexture), "GPUTexture");
	tex->w = w;
	tex->h = h;
	tex->number = -1;
	tex->refcount = 1;
	tex->target = (n == 1) ? GL_TEXTURE_1D : (samples ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D);
	tex->target_base = (n == 1) ? GL_TEXTURE_1D : GL_TEXTURE_2D;
	tex->depth = depth;
	tex->fb_attachment = -1;

	glGenTextures(1, &tex->bindcode);

	if (!tex->bindcode) {
		if (err_out) {
			BLI_snprintf(err_out, 256, "GPUTexture: texture create failed: %d",
				(int)glGetError());
		}
		else {
			fprintf(stderr, "GPUTexture: texture create failed: %d\n",
				(int)glGetError());
		}
		GPU_texture_free(tex);
		return NULL;
	}

	if (!GPU_full_non_power_of_two_support()) {
		tex->w = power_of_2_max_i(tex->w);
		tex->h = power_of_2_max_i(tex->h);
	}

	tex->number = 0;
	glBindTexture(tex->target, tex->bindcode);

	if (depth) {
		type = GL_UNSIGNED_BYTE;
		format = GL_DEPTH_COMPONENT;
		internalformat = GL_DEPTH_COMPONENT;
	}
	else {
		type = GL_FLOAT;

		if (components == 4) {
			format = GL_RGBA;
			switch (hdr_type) {
				case GPU_HDR_NONE:
					internalformat = GL_RGBA8;
					break;
				/* the following formats rely on ARB_texture_float or OpenGL 3.0 */
				case GPU_HDR_HALF_FLOAT:
					internalformat = GL_RGBA16F_ARB;
					break;
				case GPU_HDR_FULL_FLOAT:
					internalformat = GL_RGBA32F_ARB;
					break;
				default:
					break;
			}
		}
		else if (components == 2) {
			/* these formats rely on ARB_texture_rg or OpenGL 3.0 */
			format = GL_RG;
			switch (hdr_type) {
				case GPU_HDR_NONE:
					internalformat = GL_RG8;
					break;
				case GPU_HDR_HALF_FLOAT:
					internalformat = GL_RG16F;
					break;
				case GPU_HDR_FULL_FLOAT:
					internalformat = GL_RG32F;
					break;
				default:
					break;
			}
		}

		if (fpixels && hdr_type == GPU_HDR_NONE) {
			type = GL_UNSIGNED_BYTE;
			pixels = GPU_texture_convert_pixels(w * h, fpixels);
		}
	}

	if (tex->target == GL_TEXTURE_1D) {
		glTexImage1D(tex->target, 0, internalformat, tex->w, 0, format, type, NULL);

		if (fpixels) {
			glTexSubImage1D(tex->target, 0, 0, w, format, type,
				pixels ? pixels : fpixels);

			if (tex->w > w) {
				gpu_glTexSubImageEmpty(tex->target, format, w, 0, tex->w - w, 1);
			}
		}
	}
	else {
		if (samples) {
			glTexImage2DMultisample(tex->target, samples, internalformat, tex->w, tex->h, true);
		}
		else {
			glTexImage2D(tex->target, 0, internalformat, tex->w, tex->h, 0,
			             format, type, NULL);
		}

		if (fpixels) {
			glTexSubImage2D(tex->target, 0, 0, 0, w, h,
				format, type, pixels ? pixels : fpixels);

			if (tex->w > w) {
				gpu_glTexSubImageEmpty(tex->target, format, w, 0, tex->w - w, tex->h);
			}
			if (tex->h > h) {
				gpu_glTexSubImageEmpty(tex->target, format, 0, h, w, tex->h - h);
			}
		}
	}

	if (pixels)
		MEM_freeN(pixels);

	if (depth) {
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

	if (tex->target_base != GL_TEXTURE_1D) {
		glTexParameteri(tex->target_base, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(tex->target_base, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	else
		glTexParameteri(tex->target_base, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);

	return tex;
}


GPUTexture *GPU_texture_create_3D(int w, int h, int depth, int channels, const float *fpixels)
{
	GLenum type, format, internalformat;
	void *pixels = NULL;

	GPUTexture *tex = MEM_callocN(sizeof(GPUTexture), "GPUTexture");
	tex->w = w;
	tex->h = h;
	tex->depth = depth;
	tex->number = -1;
	tex->refcount = 1;
	tex->target = GL_TEXTURE_3D;
	tex->target_base = GL_TEXTURE_3D;

	glGenTextures(1, &tex->bindcode);

	if (!tex->bindcode) {
		fprintf(stderr, "GPUTexture: texture create failed: %d\n",
			(int)glGetError());
		GPU_texture_free(tex);
		return NULL;
	}

	tex->number = 0;
	glBindTexture(tex->target, tex->bindcode);

	GPU_ASSERT_NO_GL_ERRORS("3D glBindTexture");

	type = GL_FLOAT;
	if (channels == 4) {
		format = GL_RGBA;
		internalformat = GL_RGBA8;
	}
	else {
		format = GL_RED;
		internalformat = GL_INTENSITY8;
	}

	/* 3D textures are quite heavy, test if it's possible to create them first */
	glTexImage3D(GL_PROXY_TEXTURE_3D, 0, internalformat, tex->w, tex->h, tex->depth, 0, format, type, NULL);

	bool rescale = false;
	int r_width;

	glGetTexLevelParameteriv(GL_PROXY_TEXTURE_3D, 0, GL_TEXTURE_WIDTH, &r_width);

	while (r_width == 0) {
		rescale = true;
		tex->w /= 2;
		tex->h /= 2;
		tex->depth /= 2;
		glTexImage3D(GL_PROXY_TEXTURE_3D, 0, internalformat, tex->w, tex->h, tex->depth, 0, format, type, NULL);
		glGetTexLevelParameteriv(GL_PROXY_TEXTURE_3D, 0, GL_TEXTURE_WIDTH, &r_width);
	}

	/* really unlikely to happen but keep this just in case */
	tex->w = max_ii(tex->w, 1);
	tex->h = max_ii(tex->h, 1);
	tex->depth = max_ii(tex->depth, 1);

#if 0
	if (fpixels)
		pixels = GPU_texture_convert_pixels(w*h*depth, fpixels);
#endif

	GPU_ASSERT_NO_GL_ERRORS("3D glTexImage3D");

	/* hardcore stuff, 3D texture rescaling - warning, this is gonna hurt your performance a lot, but we need it
	 * for gooseberry */
	if (rescale && fpixels) {
		/* FIXME: should these be floating point? */
		const unsigned int xf = w / tex->w, yf = h / tex->h, zf = depth / tex->depth;
		float *tex3d = MEM_mallocN(channels * sizeof(float) * tex->w * tex->h * tex->depth, "tex3d");

		GPU_print_error_debug("You need to scale a 3D texture, feel the pain!");

		for (unsigned k = 0; k < tex->depth; k++) {
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
						tex3d[offset * 4] = fpixels[offset_orig * 4];
						tex3d[offset * 4 + 1] = fpixels[offset_orig * 4 + 1];
						tex3d[offset * 4 + 2] = fpixels[offset_orig * 4 + 2];
						tex3d[offset * 4 + 3] = fpixels[offset_orig * 4 + 3];
					}
					else
						tex3d[offset] = fpixels[offset_orig];
				}
			}
		}

		glTexImage3D(tex->target, 0, internalformat, tex->w, tex->h, tex->depth, 0, format, type, tex3d);

		MEM_freeN(tex3d);
	}
	else {
		if (fpixels) {
			glTexImage3D(tex->target, 0, internalformat, tex->w, tex->h, tex->depth, 0, format, type, fpixels);
			GPU_ASSERT_NO_GL_ERRORS("3D glTexSubImage3D");
		}
	}


	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	if (pixels)
		MEM_freeN(pixels);

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
		GPU_ASSERT_NO_GL_ERRORS("Blender Texture Not Loaded");
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
		GPU_ASSERT_NO_GL_ERRORS("Blender Texture Not Loaded");
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

GPUTexture *GPU_texture_create_1D(int w, const float *fpixels, char err_out[256])
{
	GPUTexture *tex = GPU_texture_create_nD(w, 1, 1, fpixels, 0, GPU_HDR_NONE, 4, 0, err_out);

	if (tex)
		GPU_texture_unbind(tex);
	
	return tex;
}

GPUTexture *GPU_texture_create_2D(int w, int h, const float *fpixels, GPUHDRType hdr, char err_out[256])
{
	GPUTexture *tex = GPU_texture_create_nD(w, h, 2, fpixels, 0, hdr, 4, 0, err_out);

	if (tex)
		GPU_texture_unbind(tex);
	
	return tex;
}
GPUTexture *GPU_texture_create_2D_multisample(
        int w, int h, const float *fpixels, GPUHDRType hdr, int samples, char err_out[256])
{
	GPUTexture *tex = GPU_texture_create_nD(w, h, 2, fpixels, 0, hdr, 4, samples, err_out);

	if (tex)
		GPU_texture_unbind(tex);

	return tex;
}

GPUTexture *GPU_texture_create_depth(int w, int h, char err_out[256])
{
	GPUTexture *tex = GPU_texture_create_nD(w, h, 2, NULL, 1, GPU_HDR_NONE, 1, 0, err_out);

	if (tex)
		GPU_texture_unbind(tex);
	
	return tex;
}
GPUTexture *GPU_texture_create_depth_multisample(int w, int h, int samples, char err_out[256])
{
	GPUTexture *tex = GPU_texture_create_nD(w, h, 2, NULL, 1, GPU_HDR_NONE, 1, samples, err_out);

	if (tex)
		GPU_texture_unbind(tex);

	return tex;
}

/**
 * A shadow map for VSM needs two components (depth and depth^2)
 */
GPUTexture *GPU_texture_create_vsm_shadow_map(int size, char err_out[256])
{
	GPUTexture *tex = GPU_texture_create_nD(size, size, 2, NULL, 0, GPU_HDR_FULL_FLOAT, 2, 0, err_out);

	if (tex) {
		/* Now we tweak some of the settings */
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		GPU_texture_unbind(tex);
	}

	return tex;
}

GPUTexture *GPU_texture_create_2D_procedural(int w, int h, const float *pixels, bool repeat, char err_out[256])
{
	GPUTexture *tex = GPU_texture_create_nD(w, h, 2, pixels, 0, GPU_HDR_HALF_FLOAT, 2, 0, err_out);

	if (tex) {
		/* Now we tweak some of the settings */
		if (repeat) {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		}
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

		GPU_texture_unbind(tex);
	}

	return tex;
}

GPUTexture *GPU_texture_create_1D_procedural(int w, const float *pixels, char err_out[256])
{
	GPUTexture *tex = GPU_texture_create_nD(w, 0, 1, pixels, 0, GPU_HDR_HALF_FLOAT, 2, 0, err_out);

	if (tex) {
		/* Now we tweak some of the settings */
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

		GPU_texture_unbind(tex);
	}

	return tex;
}

void GPU_invalid_tex_init(void)
{
	const float color[4] = {1.0f, 0.0f, 1.0f, 1.0f};
	GG.invalid_tex_1D = GPU_texture_create_1D(1, color, NULL);
	GG.invalid_tex_2D = GPU_texture_create_2D(1, 1, color, GPU_HDR_NONE, NULL);
	GG.invalid_tex_3D = GPU_texture_create_3D(1, 1, 1, 4, color);
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

	GPU_ASSERT_NO_GL_ERRORS("Pre Texture Bind");

	GLenum arbnumber = (GLenum)((GLuint)GL_TEXTURE0 + number);
	if (number != 0) glActiveTexture(arbnumber);
	if (tex->bindcode != 0) {
		glBindTexture(tex->target_base, tex->bindcode);
	}
	else
		GPU_invalid_tex_bind(tex->target_base);
	glEnable(tex->target_base);
	if (number != 0) glActiveTexture(GL_TEXTURE0);

	tex->number = number;

	GPU_ASSERT_NO_GL_ERRORS("Post Texture Bind");
}

void GPU_texture_unbind(GPUTexture *tex)
{
	if (tex->number >= GPU_max_textures()) {
		fprintf(stderr, "Not enough texture slots.\n");
		return;
	}

	if (tex->number == -1)
		return;
	
	GPU_ASSERT_NO_GL_ERRORS("Pre Texture Unbind");

	GLenum arbnumber = (GLenum)((GLuint)GL_TEXTURE0 + tex->number);
	if (tex->number != 0) glActiveTexture(arbnumber);
	glBindTexture(tex->target_base, 0);
	glDisable(tex->target_base);
	if (tex->number != 0) glActiveTexture(GL_TEXTURE0);

	tex->number = -1;

	GPU_ASSERT_NO_GL_ERRORS("Post Texture Unbind");
}

int GPU_texture_bound_number(GPUTexture *tex)
{
	return tex->number;
}

void GPU_texture_filter_mode(GPUTexture *tex, bool compare, bool use_filter)
{
	if (tex->number >= GPU_max_textures()) {
		fprintf(stderr, "Not enough texture slots.\n");
		return;
	}

	if (tex->number == -1)
		return;

	GPU_ASSERT_NO_GL_ERRORS("Pre Texture Unbind");

	GLenum arbnumber = (GLenum)((GLuint)GL_TEXTURE0 + tex->number);
	if (tex->number != 0) glActiveTexture(arbnumber);

	if (tex->depth) {
		if (compare)
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);
		else
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
	}

	if (use_filter) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
	else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}
	if (tex->number != 0) glActiveTexture(GL_TEXTURE0);

	GPU_ASSERT_NO_GL_ERRORS("Post Texture Unbind");
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

int GPU_texture_depth(const GPUTexture *tex)
{
	return tex->depth;
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

