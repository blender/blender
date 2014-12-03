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

/** \file blender/gpu/intern/gpu_extensions.c
 *  \ingroup gpu
 *
 * Wrap OpenGL features such as textures, shaders and GLSL
 * with checks for drivers and GPU support.
 */


#include "GPU_glew.h"

#include "DNA_image_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_math_base.h"

#include "BKE_global.h"

#include "GPU_draw.h"
#include "GPU_extensions.h"
#include "GPU_simple_shader.h"

#include "intern/gpu_extensions_private.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

#define MAX_DEFINE_LENGTH 72

/* Extensions support */

/* extensions used:
 * - texture border clamp: 1.3 core
 * - fragment shader: 2.0 core
 * - framebuffer object: ext specification
 * - multitexture 1.3 core
 * - arb non power of two: 2.0 core
 * - pixel buffer objects? 2.1 core
 * - arb draw buffers? 2.0 core
 */

/* Non-generated shaders */
extern char datatoc_gpu_shader_vsm_store_vert_glsl[];
extern char datatoc_gpu_shader_vsm_store_frag_glsl[];
extern char datatoc_gpu_shader_sep_gaussian_blur_vert_glsl[];
extern char datatoc_gpu_shader_sep_gaussian_blur_frag_glsl[];

typedef struct GPUShaders {
	GPUShader *vsm_store;
	GPUShader *sep_gaussian_blur;
} GPUShaders;

static struct GPUGlobal {
	GLint maxtexsize;
	GLint maxtextures;
	GLuint currentfb;
	int glslsupport;
	int extdisabled;
	int colordepth;
	int npotdisabled; /* ATI 3xx-5xx (and more) chipsets support NPoT partially (== not enough) */
	int dlistsdisabled; /* Legacy ATI driver does not support display lists well */
	GPUDeviceType device;
	GPUOSType os;
	GPUDriverType driver;
	GPUShaders shaders;
	GPUTexture *invalid_tex_1D; /* texture used in place of invalid textures (not loaded correctly, missing) */
	GPUTexture *invalid_tex_2D;
	GPUTexture *invalid_tex_3D;
} GG = {1, 0};

/* Number of maximum output slots. We support 4 outputs for now (usually we wouldn't need more to preserve fill rate) */
#define GPU_FB_MAX_SLOTS 4

struct GPUFrameBuffer {
	GLuint object;
	GPUTexture *colortex[GPU_FB_MAX_SLOTS];
	GPUTexture *depthtex;
};


/* GPU Types */

int GPU_type_matches(GPUDeviceType device, GPUOSType os, GPUDriverType driver)
{
	return (GG.device & device) && (GG.os & os) && (GG.driver & driver);
}

/* GPU Extensions */

void GPU_extensions_disable(void)
{
	GG.extdisabled = 1;
}

int GPU_max_texture_size(void)
{
	return GG.maxtexsize;
}

void gpu_extensions_init(void)
{
	GLint r, g, b;
	const char *vendor, *renderer;

	/* glewIsSupported("GL_VERSION_2_0") */

	if (GLEW_ARB_multitexture)
		glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS_ARB, &GG.maxtextures);

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &GG.maxtexsize);

	GG.glslsupport = 1;
	if (!GLEW_ARB_multitexture) GG.glslsupport = 0;
	if (!GLEW_ARB_vertex_shader) GG.glslsupport = 0;
	if (!GLEW_ARB_fragment_shader) GG.glslsupport = 0;

	glGetIntegerv(GL_RED_BITS, &r);
	glGetIntegerv(GL_GREEN_BITS, &g);
	glGetIntegerv(GL_BLUE_BITS, &b);
	GG.colordepth = r+g+b; /* assumes same depth for RGB */

	vendor = (const char *)glGetString(GL_VENDOR);
	renderer = (const char *)glGetString(GL_RENDERER);

	if (strstr(vendor, "ATI")) {
		GG.device = GPU_DEVICE_ATI;
		GG.driver = GPU_DRIVER_OFFICIAL;
	}
	else if (strstr(vendor, "NVIDIA")) {
		GG.device = GPU_DEVICE_NVIDIA;
		GG.driver = GPU_DRIVER_OFFICIAL;
	}
	else if (strstr(vendor, "Intel") ||
	        /* src/mesa/drivers/dri/intel/intel_context.c */
	        strstr(renderer, "Mesa DRI Intel") ||
		strstr(renderer, "Mesa DRI Mobile Intel")) {
		GG.device = GPU_DEVICE_INTEL;
		GG.driver = GPU_DRIVER_OFFICIAL;
	}
	else if (strstr(renderer, "Mesa DRI R") || (strstr(renderer, "Gallium ") && strstr(renderer, " on ATI "))) {
		GG.device = GPU_DEVICE_ATI;
		GG.driver = GPU_DRIVER_OPENSOURCE;
	}
	else if (strstr(renderer, "Nouveau") || strstr(vendor, "nouveau")) {
		GG.device = GPU_DEVICE_NVIDIA;
		GG.driver = GPU_DRIVER_OPENSOURCE;
	}
	else if (strstr(vendor, "Mesa")) {
		GG.device = GPU_DEVICE_SOFTWARE;
		GG.driver = GPU_DRIVER_SOFTWARE;
	}
	else if (strstr(vendor, "Microsoft")) {
		GG.device = GPU_DEVICE_SOFTWARE;
		GG.driver = GPU_DRIVER_SOFTWARE;
	}
	else if (strstr(renderer, "Apple Software Renderer")) {
		GG.device = GPU_DEVICE_SOFTWARE;
		GG.driver = GPU_DRIVER_SOFTWARE;
	}
	else {
		GG.device = GPU_DEVICE_ANY;
		GG.driver = GPU_DRIVER_ANY;
	}

	if (GG.device == GPU_DEVICE_ATI) {
		/* ATI 9500 to X2300 cards support NPoT textures poorly
		 * Incomplete list http://dri.freedesktop.org/wiki/ATIRadeon
		 * New IDs from MESA's src/gallium/drivers/r300/r300_screen.c
		 */
		/* This list is close enough to those using the legacy driver which
		 * has a bug with display lists and glVertexAttrib 
		 */
		if (strstr(renderer, "R3") || strstr(renderer, "RV3") ||
		    strstr(renderer, "R4") || strstr(renderer, "RV4") ||
		    strstr(renderer, "RS4") || strstr(renderer, "RC4") ||
		    strstr(renderer, "R5") || strstr(renderer, "RV5") ||
		    strstr(renderer, "RS600") || strstr(renderer, "RS690") ||
		    strstr(renderer, "RS740") || strstr(renderer, "X1") ||
		    strstr(renderer, "X2") || strstr(renderer, "Radeon 9") ||
		    strstr(renderer, "RADEON 9"))
		{
			GG.npotdisabled = 1;
			GG.dlistsdisabled = 1;
		}
	}

	/* make sure double side isn't used by default and only getting enabled in places where it's
	 * really needed to prevent different unexpected behaviors like with intel gme965 card (sergey) */
	glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);

#ifdef _WIN32
	GG.os = GPU_OS_WIN;
#elif defined(__APPLE__)
	GG.os = GPU_OS_MAC;
#else
	GG.os = GPU_OS_UNIX;
#endif


	GPU_invalid_tex_init();
	GPU_simple_shaders_init();
}

void gpu_extensions_exit(void)
{
	GPU_simple_shaders_exit();
	GPU_invalid_tex_free();
}

int GPU_glsl_support(void)
{
	return !GG.extdisabled && GG.glslsupport;
}

int GPU_non_power_of_two_support(void)
{
	if (GG.npotdisabled)
		return 0;

	return GLEW_ARB_texture_non_power_of_two;
}

int GPU_display_list_support(void)
{
	return !GG.dlistsdisabled;
}

int GPU_color_depth(void)
{
	return GG.colordepth;
}

int GPU_print_error(const char *str)
{
	GLenum errCode;

	if (G.debug & G_DEBUG) {
		if ((errCode = glGetError()) != GL_NO_ERROR) {
			fprintf(stderr, "%s opengl error: %s\n", str, gluErrorString(errCode));
			return 1;
		}
	}

	return 0;
}

static void GPU_print_framebuffer_error(GLenum status, char err_out[256])
{
	const char *err= "unknown";

	switch (status) {
		case GL_FRAMEBUFFER_COMPLETE_EXT:
			break;
		case GL_INVALID_OPERATION:
			err= "Invalid operation";
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT:
			err= "Incomplete attachment";
			break;
		case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
			err= "Unsupported framebuffer format";
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
			err= "Missing attachment";
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
			err= "Attached images must have same dimensions";
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
			err= "Attached images must have same format";
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT:
			err= "Missing draw buffer";
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT:
			err= "Missing read buffer";
			break;
	}

	if (err_out) {
		BLI_snprintf(err_out, 256, "GPUFrameBuffer: framebuffer incomplete error %d '%s'",
			(int)status, err);
	}
	else {
		fprintf(stderr, "GPUFrameBuffer: framebuffer incomplete error %d '%s'\n",
			(int)status, err);
	}
}

/* GPUTexture */

struct GPUTexture {
	int w, h;				/* width/height */
	int number;				/* number for multitexture binding */
	int refcount;			/* reference count */
	GLenum target;			/* GL_TEXTURE_* */
	GLuint bindcode;		/* opengl identifier for texture */
	int fromblender;		/* we got the texture from Blender */

	GPUFrameBuffer *fb;		/* GPUFramebuffer this texture is attached to */
	int fb_attachment;		/* slot the texture is attached to */
	int depth;				/* is a depth texture? */
};

static unsigned char *GPU_texture_convert_pixels(int length, float *fpixels)
{
	unsigned char *pixels, *p;
	const float *fp;
	int a, len;

	len = 4*length;
	fp = fpixels;
	p = pixels = MEM_callocN(sizeof(unsigned char)*len, "GPUTexturePixels");

	for (a=0; a<len; a++, p++, fp++)
		*p = FTOCHAR((*fp));

	return pixels;
}

static void GPU_glTexSubImageEmpty(GLenum target, GLenum format, int x, int y, int w, int h)
{
	void *pixels = MEM_callocN(sizeof(char)*4*w*h, "GPUTextureEmptyPixels");

	if (target == GL_TEXTURE_1D)
		glTexSubImage1D(target, 0, x, w, format, GL_UNSIGNED_BYTE, pixels);
	else
		glTexSubImage2D(target, 0, x, y, w, h, format, GL_UNSIGNED_BYTE, pixels);
	
	MEM_freeN(pixels);
}

static GPUTexture *GPU_texture_create_nD(int w, int h, int n, float *fpixels, int depth, char err_out[256])
{
	GPUTexture *tex;
	GLenum type, format, internalformat;
	void *pixels = NULL;

	if (depth && !GLEW_ARB_depth_texture)
		return NULL;

	tex = MEM_callocN(sizeof(GPUTexture), "GPUTexture");
	tex->w = w;
	tex->h = h;
	tex->number = -1;
	tex->refcount = 1;
	tex->target = (n == 1)? GL_TEXTURE_1D: GL_TEXTURE_2D;
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

	if (!GPU_non_power_of_two_support()) {
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
		type = GL_UNSIGNED_BYTE;
		format = GL_RGBA;
		internalformat = GL_RGBA8;

		if (fpixels)
			pixels = GPU_texture_convert_pixels(w*h, fpixels);
	}

	if (tex->target == GL_TEXTURE_1D) {
		glTexImage1D(tex->target, 0, internalformat, tex->w, 0, format, type, NULL);

		if (fpixels) {
			glTexSubImage1D(tex->target, 0, 0, w, format, type,
				pixels ? pixels : fpixels);

			if (tex->w > w)
				GPU_glTexSubImageEmpty(tex->target, format, w, 0,
					tex->w-w, 1);
		}
	}
	else {
		glTexImage2D(tex->target, 0, internalformat, tex->w, tex->h, 0,
		             format, type, NULL);

		if (fpixels) {
			glTexSubImage2D(tex->target, 0, 0, 0, w, h,
				format, type, pixels ? pixels : fpixels);

			if (tex->w > w)
				GPU_glTexSubImageEmpty(tex->target, format, w, 0, tex->w-w, tex->h);
			if (tex->h > h)
				GPU_glTexSubImageEmpty(tex->target, format, 0, h, w, tex->h-h);
		}
	}

	if (pixels)
		MEM_freeN(pixels);

	if (depth) {
		glTexParameteri(tex->target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(tex->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(tex->target, GL_TEXTURE_COMPARE_MODE_ARB, GL_COMPARE_R_TO_TEXTURE);
		glTexParameteri(tex->target, GL_TEXTURE_COMPARE_FUNC_ARB, GL_LEQUAL);
		glTexParameteri(tex->target, GL_DEPTH_TEXTURE_MODE_ARB, GL_INTENSITY);  
	}
	else {
		glTexParameteri(tex->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(tex->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	if (tex->target != GL_TEXTURE_1D) {
		/* CLAMP_TO_BORDER is an OpenGL 1.3 core feature */
		GLenum wrapmode = (depth || tex->h == 1)? GL_CLAMP_TO_EDGE: GL_CLAMP_TO_BORDER;
		glTexParameteri(tex->target, GL_TEXTURE_WRAP_S, wrapmode);
		glTexParameteri(tex->target, GL_TEXTURE_WRAP_T, wrapmode);

#if 0
		float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor); 
#endif
	}
	else
		glTexParameteri(tex->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);

	return tex;
}


GPUTexture *GPU_texture_create_3D(int w, int h, int depth, int channels, float *fpixels)
{
	GPUTexture *tex;
	GLenum type, format, internalformat;
	void *pixels = NULL;
	float vfBorderColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};

	if (!GLEW_VERSION_1_2)
		return NULL;

	tex = MEM_callocN(sizeof(GPUTexture), "GPUTexture");
	tex->w = w;
	tex->h = h;
	tex->depth = depth;
	tex->number = -1;
	tex->refcount = 1;
	tex->target = GL_TEXTURE_3D;

	glGenTextures(1, &tex->bindcode);

	if (!tex->bindcode) {
		fprintf(stderr, "GPUTexture: texture create failed: %d\n",
			(int)glGetError());
		GPU_texture_free(tex);
		return NULL;
	}

	if (!GPU_non_power_of_two_support()) {
		tex->w = power_of_2_max_i(tex->w);
		tex->h = power_of_2_max_i(tex->h);
		tex->depth = power_of_2_max_i(tex->depth);
	}

	tex->number = 0;
	glBindTexture(tex->target, tex->bindcode);

	GPU_print_error("3D glBindTexture");

	type = GL_FLOAT;
	if (channels == 4) {
		format = GL_RGBA;
		internalformat = GL_RGBA;
	}
	else {
		format = GL_RED;
		internalformat = GL_INTENSITY;
	}

	//if (fpixels)
	//	pixels = GPU_texture_convert_pixels(w*h*depth, fpixels);

	glTexImage3D(tex->target, 0, internalformat, tex->w, tex->h, tex->depth, 0, format, type, NULL);

	GPU_print_error("3D glTexImage3D");

	if (fpixels) {
		if (!GPU_non_power_of_two_support() && (w != tex->w || h != tex->h || depth != tex->depth)) {
			/* clear first to avoid unitialized pixels */
			float *zero= MEM_callocN(sizeof(float)*tex->w*tex->h*tex->depth, "zero");
			glTexSubImage3D(tex->target, 0, 0, 0, 0, tex->w, tex->h, tex->depth, format, type, zero);
			MEM_freeN(zero);
		}

		glTexSubImage3D(tex->target, 0, 0, 0, 0, w, h, depth, format, type, fpixels);
		GPU_print_error("3D glTexSubImage3D");
	}


	glTexParameterfv(GL_TEXTURE_3D, GL_TEXTURE_BORDER_COLOR, vfBorderColor);
	GPU_print_error("3D GL_TEXTURE_BORDER_COLOR");
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	GPU_print_error("3D GL_LINEAR");
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	GPU_print_error("3D GL_CLAMP_TO_BORDER");

	if (pixels)
		MEM_freeN(pixels);

	GPU_texture_unbind(tex);

	return tex;
}

GPUTexture *GPU_texture_from_blender(Image *ima, ImageUser *iuser, bool is_data, double time, int mipmap)
{
	GPUTexture *tex;
	GLint w, h, border, lastbindcode, bindcode;

	glGetIntegerv(GL_TEXTURE_BINDING_2D, &lastbindcode);

	GPU_update_image_time(ima, time);
	/* this binds a texture, so that's why to restore it with lastbindcode */
	bindcode = GPU_verify_image(ima, iuser, 0, 0, mipmap, is_data);

	if (ima->gputexture) {
		ima->gputexture->bindcode = bindcode;
		glBindTexture(GL_TEXTURE_2D, lastbindcode);
		return ima->gputexture;
	}

	tex = MEM_callocN(sizeof(GPUTexture), "GPUTexture");
	tex->bindcode = bindcode;
	tex->number = -1;
	tex->refcount = 1;
	tex->target = GL_TEXTURE_2D;
	tex->fromblender = 1;

	ima->gputexture= tex;

	if (!glIsTexture(tex->bindcode)) {
		GPU_print_error("Blender Texture Not Loaded");
	}
	else {
		glBindTexture(GL_TEXTURE_2D, tex->bindcode);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_BORDER, &border);

		tex->w = w - border;
		tex->h = h - border;
	}

	glBindTexture(GL_TEXTURE_2D, lastbindcode);

	return tex;
}

GPUTexture *GPU_texture_from_preview(PreviewImage *prv, int mipmap)
{
	GPUTexture *tex = prv->gputexture[0];
	GLint w, h, lastbindcode;
	GLuint bindcode = 0;
	
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &lastbindcode);
	
	if (tex)
		bindcode = tex->bindcode;
	
	/* this binds a texture, so that's why to restore it */
	if (bindcode == 0) {
		GPU_create_gl_tex(&bindcode, prv->rect[0], NULL, prv->w[0], prv->h[0], mipmap, 0, NULL);
	}
	if (tex) {
		tex->bindcode = bindcode;
		glBindTexture(GL_TEXTURE_2D, lastbindcode);
		return tex;
	}

	tex = MEM_callocN(sizeof(GPUTexture), "GPUTexture");
	tex->bindcode = bindcode;
	tex->number = -1;
	tex->refcount = 1;
	tex->target = GL_TEXTURE_2D;
	
	prv->gputexture[0]= tex;
	
	if (!glIsTexture(tex->bindcode)) {
		GPU_print_error("Blender Texture Not Loaded");
	}
	else {
		glBindTexture(GL_TEXTURE_2D, tex->bindcode);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);
		
		tex->w = w;
		tex->h = h;
	}
	
	glBindTexture(GL_TEXTURE_2D, lastbindcode);
	
	return tex;

}

GPUTexture *GPU_texture_create_1D(int w, float *fpixels, char err_out[256])
{
	GPUTexture *tex = GPU_texture_create_nD(w, 1, 1, fpixels, 0, err_out);

	if (tex)
		GPU_texture_unbind(tex);
	
	return tex;
}

GPUTexture *GPU_texture_create_2D(int w, int h, float *fpixels, char err_out[256])
{
	GPUTexture *tex = GPU_texture_create_nD(w, h, 2, fpixels, 0, err_out);

	if (tex)
		GPU_texture_unbind(tex);
	
	return tex;
}

GPUTexture *GPU_texture_create_depth(int w, int h, char err_out[256])
{
	GPUTexture *tex = GPU_texture_create_nD(w, h, 2, NULL, 1, err_out);

	if (tex)
		GPU_texture_unbind(tex);
	
	return tex;
}

/**
 * A shadow map for VSM needs two components (depth and depth^2)
 */
GPUTexture *GPU_texture_create_vsm_shadow_map(int size, char err_out[256])
{
	GPUTexture *tex = GPU_texture_create_nD(size, size, 2, NULL, 0, err_out);

	if (tex) {
		/* Now we tweak some of the settings */
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, size, size, 0, GL_RG, GL_FLOAT, NULL);

		GPU_texture_unbind(tex);
	}

	return tex;
}

void GPU_invalid_tex_init(void)
{
	float color[4] = {1.0f, 0.0f, 1.0f, 1.0};
	GG.invalid_tex_1D = GPU_texture_create_1D(1, color, NULL);
	GG.invalid_tex_2D = GPU_texture_create_2D(1, 1, color, NULL);
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
	GLenum arbnumber;

	if (number >= GG.maxtextures) {
		GPU_print_error("Not enough texture slots.");
		return;
	}

	if (tex->fb) {
		if (tex->fb->object == GG.currentfb && (G.debug & G_DEBUG)) {
			fprintf(stderr, "Feedback loop warning!: Attempting to bind texture attached to current framebuffer!\n");
		}
	}

	if (number < 0)
		return;

	GPU_print_error("Pre Texture Bind");

	arbnumber = (GLenum)((GLuint)GL_TEXTURE0_ARB + number);
	if (number != 0) glActiveTextureARB(arbnumber);
	if (tex->bindcode != 0) {
		glBindTexture(tex->target, tex->bindcode);
	}
	else
		GPU_invalid_tex_bind(tex->target);
	glEnable(tex->target);
	if (number != 0) glActiveTextureARB(GL_TEXTURE0_ARB);

	tex->number = number;

	GPU_print_error("Post Texture Bind");
}

void GPU_texture_unbind(GPUTexture *tex)
{
	GLenum arbnumber;

	if (tex->number >= GG.maxtextures) {
		GPU_print_error("Not enough texture slots.");
		return;
	}

	if (tex->number == -1)
		return;
	
	GPU_print_error("Pre Texture Unbind");

	arbnumber = (GLenum)((GLuint)GL_TEXTURE0_ARB + tex->number);
	if (tex->number != 0) glActiveTextureARB(arbnumber);
	glBindTexture(tex->target, 0);
	glDisable(tex->target);
	if (tex->number != 0) glActiveTextureARB(GL_TEXTURE0_ARB);

	tex->number = -1;

	GPU_print_error("Post Texture Unbind");
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

int GPU_texture_target(GPUTexture *tex)
{
	return tex->target;
}

int GPU_texture_opengl_width(GPUTexture *tex)
{
	return tex->w;
}

int GPU_texture_opengl_height(GPUTexture *tex)
{
	return tex->h;
}

int GPU_texture_opengl_bindcode(GPUTexture *tex)
{
	return tex->bindcode;
}

GPUFrameBuffer *GPU_texture_framebuffer(GPUTexture *tex)
{
	return tex->fb;
}

/* GPUFrameBuffer */

GPUFrameBuffer *GPU_framebuffer_create(void)
{
	GPUFrameBuffer *fb;

	if (!GLEW_EXT_framebuffer_object)
		return NULL;
	
	fb= MEM_callocN(sizeof(GPUFrameBuffer), "GPUFrameBuffer");
	glGenFramebuffersEXT(1, &fb->object);

	if (!fb->object) {
		fprintf(stderr, "GPUFFrameBuffer: framebuffer gen failed. %d\n",
			(int)glGetError());
		GPU_framebuffer_free(fb);
		return NULL;
	}

	/* make sure no read buffer is enabled, so completeness check will not fail. We set those at binding time */
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fb->object);
	glReadBuffer(GL_NONE);
	glDrawBuffer(GL_NONE);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	
	return fb;
}

int GPU_framebuffer_texture_attach(GPUFrameBuffer *fb, GPUTexture *tex, int slot, char err_out[256])
{
	GLenum attachment;
	GLenum error;

	if (slot >= GPU_FB_MAX_SLOTS) {
		fprintf(stderr, "Attaching to index %d framebuffer slot unsupported in blender use at most %d\n", slot, GPU_FB_MAX_SLOTS);
		return 0;
	}

	if (tex->number != -1 && (G.debug & G_DEBUG)) {
		fprintf(stderr, "Feedback loop warning!: Attempting to attach texture to framebuffer while still bound to texture unit for drawing!");
	}

	if (tex->depth)
		attachment = GL_DEPTH_ATTACHMENT_EXT;
	else
		attachment = GL_COLOR_ATTACHMENT0_EXT + slot;

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fb->object);
	GG.currentfb = fb->object;

	/* Clean glError buffer. */
	while (glGetError() != GL_NO_ERROR) {}

	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, attachment, 
		tex->target, tex->bindcode, 0);

	error = glGetError();

	if (error == GL_INVALID_OPERATION) {
		GPU_framebuffer_restore();
		GPU_print_framebuffer_error(error, err_out);
		return 0;
	}

	if (tex->depth)
		fb->depthtex = tex;
	else
		fb->colortex[slot] = tex;

	tex->fb= fb;
	tex->fb_attachment = slot;

	return 1;
}

void GPU_framebuffer_texture_detach(GPUTexture *tex)
{
	GLenum attachment;
	GPUFrameBuffer *fb;

	if (!tex->fb)
		return;

	fb = tex->fb;

	if (GG.currentfb != fb->object) {
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fb->object);
		GG.currentfb = tex->fb->object;
	}

	if (tex->depth) {
		fb->depthtex = NULL;
		attachment = GL_DEPTH_ATTACHMENT_EXT;
	}
	else {
		BLI_assert(fb->colortex[tex->fb_attachment] == tex);
		fb->colortex[tex->fb_attachment] = NULL;
		attachment = GL_COLOR_ATTACHMENT0_EXT + tex->fb_attachment;
	}

	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, attachment,
		tex->target, 0, 0);

	tex->fb = NULL;
	tex->fb_attachment = -1;
}

void GPU_texture_bind_as_framebuffer(GPUTexture *tex)
{
	if (!tex->fb) {
		fprintf(stderr, "Error, texture not bound to framebuffer!");
		return;
	}

	/* push attributes */
	glPushAttrib(GL_ENABLE_BIT | GL_VIEWPORT_BIT);
	glDisable(GL_SCISSOR_TEST);

	/* bind framebuffer */
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, tex->fb->object);

	/* last bound prevails here, better allow explicit control here too */
	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT + tex->fb_attachment);
	glReadBuffer(GL_COLOR_ATTACHMENT0_EXT + tex->fb_attachment);

	/* push matrices and set default viewport and matrix */
	glViewport(0, 0, tex->w, tex->h);
	GG.currentfb = tex->fb->object;

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
}

void GPU_framebuffer_slot_bind(GPUFrameBuffer *fb, int slot)
{
	if (!fb->colortex[slot]) {
		fprintf(stderr, "Error, framebuffer slot empty!");
		return;
	}

	/* push attributes */
	glPushAttrib(GL_ENABLE_BIT | GL_VIEWPORT_BIT);
	glDisable(GL_SCISSOR_TEST);

	/* bind framebuffer */
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fb->object);

	/* last bound prevails here, better allow explicit control here too */
	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT + slot);
	glReadBuffer(GL_COLOR_ATTACHMENT0_EXT + slot);

	/* push matrices and set default viewport and matrix */
	glViewport(0, 0, fb->colortex[slot]->w, fb->colortex[slot]->h);
	GG.currentfb = fb->object;

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
}


void GPU_framebuffer_texture_unbind(GPUFrameBuffer *UNUSED(fb), GPUTexture *UNUSED(tex))
{
	/* restore matrix */
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	/* restore attributes */
	glPopAttrib();
}


bool GPU_framebuffer_check_valid(GPUFrameBuffer *fb, char err_out[256])
{
	GLenum status;
	
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fb->object);
	GG.currentfb = fb->object;
	
	/* Clean glError buffer. */
	while (glGetError() != GL_NO_ERROR) {}
	
	status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
	
	if (status != GL_FRAMEBUFFER_COMPLETE_EXT) {
		GPU_framebuffer_restore();
		GPU_print_framebuffer_error(status, err_out);
		return false;
	}
	
	return true;
}


void GPU_framebuffer_free(GPUFrameBuffer *fb)
{
	int i;
	if (fb->depthtex)
		GPU_framebuffer_texture_detach(fb->depthtex);

	for (i = 0; i < GPU_FB_MAX_SLOTS; i++) {
		if (fb->colortex[i]) {
			GPU_framebuffer_texture_detach(fb->colortex[i]);
		}
	}

	if (fb->object) {
		glDeleteFramebuffersEXT(1, &fb->object);

		if (GG.currentfb == fb->object) {
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
			GG.currentfb = 0;
		}
	}

	MEM_freeN(fb);
}

void GPU_framebuffer_restore(void)
{
	if (GG.currentfb != 0) {
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		GG.currentfb = 0;
	}
}

void GPU_framebuffer_blur(GPUFrameBuffer *fb, GPUTexture *tex, GPUFrameBuffer *blurfb, GPUTexture *blurtex)
{
	float scaleh[2] = {1.0f/GPU_texture_opengl_width(blurtex), 0.0f};
	float scalev[2] = {0.0f, 1.0f/GPU_texture_opengl_height(tex)};

	GPUShader *blur_shader = GPU_shader_get_builtin_shader(GPU_SHADER_SEP_GAUSSIAN_BLUR);
	int scale_uniform, texture_source_uniform;

	if (!blur_shader)
		return;

	scale_uniform = GPU_shader_get_uniform(blur_shader, "ScaleU");
	texture_source_uniform = GPU_shader_get_uniform(blur_shader, "textureSource");
		
	/* Blurring horizontally */

	/* We do the bind ourselves rather than using GPU_framebuffer_texture_bind() to avoid
	 * pushing unnecessary matrices onto the OpenGL stack. */
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, blurfb->object);

	GPU_shader_bind(blur_shader);
	GPU_shader_uniform_vector(blur_shader, scale_uniform, 2, 1, (float *)scaleh);
	GPU_shader_uniform_texture(blur_shader, texture_source_uniform, tex);
	glViewport(0, 0, GPU_texture_opengl_width(blurtex), GPU_texture_opengl_height(blurtex));

	/* Peparing to draw quad */
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glDisable(GL_DEPTH_TEST);

	GPU_texture_bind(tex, 0);

	/* Drawing quad */
	glBegin(GL_QUADS);
	glTexCoord2d(0, 0); glVertex2f(1, 1);
	glTexCoord2d(1, 0); glVertex2f(-1, 1);
	glTexCoord2d(1, 1); glVertex2f(-1, -1);
	glTexCoord2d(0, 1); glVertex2f(1, -1);
	glEnd();
		
	/* Blurring vertically */

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fb->object);
	glViewport(0, 0, GPU_texture_opengl_width(tex), GPU_texture_opengl_height(tex));
	GPU_shader_uniform_vector(blur_shader, scale_uniform, 2, 1, (float *)scalev);
	GPU_shader_uniform_texture(blur_shader, texture_source_uniform, blurtex);
	GPU_texture_bind(blurtex, 0);

	glBegin(GL_QUADS);
	glTexCoord2d(0, 0); glVertex2f(1, 1);
	glTexCoord2d(1, 0); glVertex2f(-1, 1);
	glTexCoord2d(1, 1); glVertex2f(-1, -1);
	glTexCoord2d(0, 1); glVertex2f(1, -1);
	glEnd();

	GPU_shader_unbind();
}

/* GPUOffScreen */

struct GPUOffScreen {
	GPUFrameBuffer *fb;
	GPUTexture *color;
	GPUTexture *depth;
};

GPUOffScreen *GPU_offscreen_create(int width, int height, char err_out[256])
{
	GPUOffScreen *ofs;

	ofs= MEM_callocN(sizeof(GPUOffScreen), "GPUOffScreen");

	ofs->fb = GPU_framebuffer_create();
	if (!ofs->fb) {
		GPU_offscreen_free(ofs);
		return NULL;
	}

	ofs->depth = GPU_texture_create_depth(width, height, err_out);
	if (!ofs->depth) {
		GPU_offscreen_free(ofs);
		return NULL;
	}

	if (!GPU_framebuffer_texture_attach(ofs->fb, ofs->depth, 0, err_out)) {
		GPU_offscreen_free(ofs);
		return NULL;
	}

	ofs->color = GPU_texture_create_2D(width, height, NULL, err_out);
	if (!ofs->color) {
		GPU_offscreen_free(ofs);
		return NULL;
	}

	if (!GPU_framebuffer_texture_attach(ofs->fb, ofs->color, 0, err_out)) {
		GPU_offscreen_free(ofs);
		return NULL;
	}
	
	/* check validity at the very end! */
	if (!GPU_framebuffer_check_valid(ofs->fb, err_out)) {
		GPU_offscreen_free(ofs);
		return NULL;		
	}

	GPU_framebuffer_restore();

	return ofs;
}

void GPU_offscreen_free(GPUOffScreen *ofs)
{
	if (ofs->fb)
		GPU_framebuffer_free(ofs->fb);
	if (ofs->color)
		GPU_texture_free(ofs->color);
	if (ofs->depth)
		GPU_texture_free(ofs->depth);
	
	MEM_freeN(ofs);
}

void GPU_offscreen_bind(GPUOffScreen *ofs)
{
	glDisable(GL_SCISSOR_TEST);
	GPU_texture_bind_as_framebuffer(ofs->color);
}

void GPU_offscreen_unbind(GPUOffScreen *ofs)
{
	GPU_framebuffer_texture_unbind(ofs->fb, ofs->color);
	GPU_framebuffer_restore();
	glEnable(GL_SCISSOR_TEST);
}

void GPU_offscreen_read_pixels(GPUOffScreen *ofs, int type, void *pixels)
{
	glReadPixels(0, 0, ofs->color->w, ofs->color->h, GL_RGBA, type, pixels);
}

int GPU_offscreen_width(GPUOffScreen *ofs)
{
	return ofs->color->w;
}

int GPU_offscreen_height(GPUOffScreen *ofs)
{
	return ofs->color->h;
}

/* GPUShader */

struct GPUShader {
	GLhandleARB object;		/* handle for full shader */
	GLhandleARB vertex;		/* handle for vertex shader */
	GLhandleARB fragment;	/* handle for fragment shader */
	GLhandleARB lib;		/* handle for libment shader */
	int totattrib;			/* total number of attributes */
};

static void shader_print_errors(const char *task, char *log, const char **code, int totcode)
{
	int i;

	fprintf(stderr, "GPUShader: %s error:\n", task);

	for (i = 0; i < totcode; i++) {
		const char *c, *pos, *end = code[i] + strlen(code[i]);
		int line = 1;
				
		if (G.debug & G_DEBUG) {
			fprintf(stderr, "===== shader string %d ====\n", i + 1);

			c = code[i];
			while ((c < end) && (pos = strchr(c, '\n'))) {
				fprintf(stderr, "%2d  ", line);
				fwrite(c, (pos+1)-c, 1, stderr);
				c = pos+1;
				line++;
			}
			
			fprintf(stderr, "%s", c);
		}
	}
	
	fprintf(stderr, "%s\n", log);
}

static const char *gpu_shader_version(void)
{
	/* turn on glsl 1.30 for bicubic bump mapping and ATI clipping support */
	if (GLEW_VERSION_3_0 &&
	    (GPU_bicubic_bump_support() || GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_ANY, GPU_DRIVER_ANY)))
	{
		return "#version 130\n";
	}

	return "";
}


static const char *gpu_shader_standard_extensions(void)
{
	/* need this extensions for high quality bump mapping */
	if (GPU_bicubic_bump_support())
		return "#extension GL_ARB_texture_query_lod: enable\n";

	return "";
}

static void gpu_shader_standard_defines(char defines[MAX_DEFINE_LENGTH])
{
	/* some useful defines to detect GPU type */
	if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_ANY, GPU_DRIVER_ANY)) {
		strcat(defines, "#define GPU_ATI\n");
		if (GLEW_VERSION_3_0)
			strcat(defines, "#define CLIP_WORKAROUND\n");
	}
	else if (GPU_type_matches(GPU_DEVICE_NVIDIA, GPU_OS_ANY, GPU_DRIVER_ANY))
		strcat(defines, "#define GPU_NVIDIA\n");
	else if (GPU_type_matches(GPU_DEVICE_INTEL, GPU_OS_ANY, GPU_DRIVER_ANY))
		strcat(defines, "#define GPU_INTEL\n");

	if (GPU_bicubic_bump_support())
		strcat(defines, "#define BUMP_BICUBIC\n");
	return;
}

GPUShader *GPU_shader_create(const char *vertexcode, const char *fragcode, const char *libcode, const char *defines)
{
	GLint status;
	GLcharARB log[5000];
	GLsizei length = 0;
	GPUShader *shader;
	char standard_defines[MAX_DEFINE_LENGTH] = "";

	if (!GLEW_ARB_vertex_shader || !GLEW_ARB_fragment_shader)
		return NULL;

	shader = MEM_callocN(sizeof(GPUShader), "GPUShader");

	if (vertexcode)
		shader->vertex = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
	if (fragcode)
		shader->fragment = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
	shader->object = glCreateProgramObjectARB();

	if (!shader->object ||
	    (vertexcode && !shader->vertex) ||
	    (fragcode && !shader->fragment))
	{
		fprintf(stderr, "GPUShader, object creation failed.\n");
		GPU_shader_free(shader);
		return NULL;
	}

	gpu_shader_standard_defines(standard_defines);

	if (vertexcode) {
		const char *source[5];
		/* custom limit, may be too small, beware */
		int num_source = 0;

		source[num_source++] = gpu_shader_version();
		source[num_source++] = gpu_shader_standard_extensions();
		source[num_source++] = standard_defines;

		if (defines) source[num_source++] = defines;
		if (vertexcode) source[num_source++] = vertexcode;

		glAttachObjectARB(shader->object, shader->vertex);
		glShaderSourceARB(shader->vertex, num_source, source, NULL);

		glCompileShaderARB(shader->vertex);
		glGetObjectParameterivARB(shader->vertex, GL_OBJECT_COMPILE_STATUS_ARB, &status);

		if (!status) {
			glGetInfoLogARB(shader->vertex, sizeof(log), &length, log);
			shader_print_errors("compile", log, source, num_source);

			GPU_shader_free(shader);
			return NULL;
		}
	}

	if (fragcode) {
		const char *source[6];
		int num_source = 0;

		source[num_source++] = gpu_shader_version();
		source[num_source++] = gpu_shader_standard_extensions();
		source[num_source++] = standard_defines;

		if (defines) source[num_source++] = defines;
		if (libcode) source[num_source++] = libcode;
		if (fragcode) source[num_source++] = fragcode;

		glAttachObjectARB(shader->object, shader->fragment);
		glShaderSourceARB(shader->fragment, num_source, source, NULL);

		glCompileShaderARB(shader->fragment);
		glGetObjectParameterivARB(shader->fragment, GL_OBJECT_COMPILE_STATUS_ARB, &status);

		if (!status) {
			glGetInfoLogARB(shader->fragment, sizeof(log), &length, log);
			shader_print_errors("compile", log, source, num_source);

			GPU_shader_free(shader);
			return NULL;
		}
	}

#if 0
	if (lib && lib->lib)
		glAttachObjectARB(shader->object, lib->lib);
#endif

	glLinkProgramARB(shader->object);
	glGetObjectParameterivARB(shader->object, GL_OBJECT_LINK_STATUS_ARB, &status);
	if (!status) {
		glGetInfoLogARB(shader->object, sizeof(log), &length, log);
		if (fragcode) shader_print_errors("linking", log, &fragcode, 1);
		else if (vertexcode) shader_print_errors("linking", log, &vertexcode, 1);
		else if (libcode) shader_print_errors("linking", log, &libcode, 1);

		GPU_shader_free(shader);
		return NULL;
	}

	return shader;
}

#if 0
GPUShader *GPU_shader_create_lib(const char *code)
{
	GLint status;
	GLcharARB log[5000];
	GLsizei length = 0;
	GPUShader *shader;

	if (!GLEW_ARB_vertex_shader || !GLEW_ARB_fragment_shader)
		return NULL;

	shader = MEM_callocN(sizeof(GPUShader), "GPUShader");

	shader->lib = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);

	if (!shader->lib) {
		fprintf(stderr, "GPUShader, object creation failed.\n");
		GPU_shader_free(shader);
		return NULL;
	}

	glShaderSourceARB(shader->lib, 1, (const char**)&code, NULL);

	glCompileShaderARB(shader->lib);
	glGetObjectParameterivARB(shader->lib, GL_OBJECT_COMPILE_STATUS_ARB, &status);

	if (!status) {
		glGetInfoLogARB(shader->lib, sizeof(log), &length, log);
		shader_print_errors("compile", log, code);

		GPU_shader_free(shader);
		return NULL;
	}

	return shader;
}
#endif

void GPU_shader_bind(GPUShader *shader)
{
	GPU_print_error("Pre Shader Bind");
	glUseProgramObjectARB(shader->object);
	GPU_print_error("Post Shader Bind");
}

void GPU_shader_unbind(void)
{
	GPU_print_error("Pre Shader Unbind");
	glUseProgramObjectARB(0);
	GPU_print_error("Post Shader Unbind");
}

void GPU_shader_free(GPUShader *shader)
{
	if (shader->lib)
		glDeleteObjectARB(shader->lib);
	if (shader->vertex)
		glDeleteObjectARB(shader->vertex);
	if (shader->fragment)
		glDeleteObjectARB(shader->fragment);
	if (shader->object)
		glDeleteObjectARB(shader->object);
	MEM_freeN(shader);
}

int GPU_shader_get_uniform(GPUShader *shader, const char *name)
{
	return glGetUniformLocationARB(shader->object, name);
}

void GPU_shader_uniform_vector(GPUShader *UNUSED(shader), int location, int length, int arraysize, float *value)
{
	if (location == -1)
		return;

	GPU_print_error("Pre Uniform Vector");

	if (length == 1) glUniform1fvARB(location, arraysize, value);
	else if (length == 2) glUniform2fvARB(location, arraysize, value);
	else if (length == 3) glUniform3fvARB(location, arraysize, value);
	else if (length == 4) glUniform4fvARB(location, arraysize, value);
	else if (length == 9) glUniformMatrix3fvARB(location, arraysize, 0, value);
	else if (length == 16) glUniformMatrix4fvARB(location, arraysize, 0, value);

	GPU_print_error("Post Uniform Vector");
}

void GPU_shader_uniform_int(GPUShader *UNUSED(shader), int location, int value)
{
	if (location == -1)
		return;

	GPU_print_error("Pre Uniform Int");
	glUniform1iARB(location, value);
	GPU_print_error("Post Uniform Int");
}

void GPU_shader_uniform_texture(GPUShader *UNUSED(shader), int location, GPUTexture *tex)
{
	GLenum arbnumber;

	if (tex->number >= GG.maxtextures) {
		GPU_print_error("Not enough texture slots.");
		return;
	}
		
	if (tex->number == -1)
		return;

	if (location == -1)
		return;

	GPU_print_error("Pre Uniform Texture");

	arbnumber = (GLenum)((GLuint)GL_TEXTURE0_ARB + tex->number);

	if (tex->number != 0) glActiveTextureARB(arbnumber);
	if (tex->bindcode != 0)
		glBindTexture(tex->target, tex->bindcode);
	else
		GPU_invalid_tex_bind(tex->target);
	glUniform1iARB(location, tex->number);
	glEnable(tex->target);
	if (tex->number != 0) glActiveTextureARB(GL_TEXTURE0_ARB);

	GPU_print_error("Post Uniform Texture");
}

int GPU_shader_get_attribute(GPUShader *shader, const char *name)
{
	int index;
	
	GPU_print_error("Pre Get Attribute");

	index = glGetAttribLocationARB(shader->object, name);

	GPU_print_error("Post Get Attribute");

	return index;
}

GPUShader *GPU_shader_get_builtin_shader(GPUBuiltinShader shader)
{
	GPUShader *retval = NULL;

	switch (shader) {
		case GPU_SHADER_VSM_STORE:
			if (!GG.shaders.vsm_store)
				GG.shaders.vsm_store = GPU_shader_create(datatoc_gpu_shader_vsm_store_vert_glsl, datatoc_gpu_shader_vsm_store_frag_glsl, NULL, NULL);
			retval = GG.shaders.vsm_store;
			break;
		case GPU_SHADER_SEP_GAUSSIAN_BLUR:
			if (!GG.shaders.sep_gaussian_blur)
				GG.shaders.sep_gaussian_blur = GPU_shader_create(datatoc_gpu_shader_sep_gaussian_blur_vert_glsl, datatoc_gpu_shader_sep_gaussian_blur_frag_glsl, NULL, NULL);
			retval = GG.shaders.sep_gaussian_blur;
			break;
	}

	if (retval == NULL)
		printf("Unable to create a GPUShader for builtin shader: %d\n", shader);

	return retval;
}

void GPU_shader_free_builtin_shaders(void)
{
	if (GG.shaders.vsm_store) {
		MEM_freeN(GG.shaders.vsm_store);
		GG.shaders.vsm_store = NULL;
	}

	if (GG.shaders.sep_gaussian_blur) {
		MEM_freeN(GG.shaders.sep_gaussian_blur);
		GG.shaders.sep_gaussian_blur = NULL;
	}
}

#if 0
/* GPUPixelBuffer */

typedef struct GPUPixelBuffer {
	GLuint bindcode[2];
	GLuint current;
	int datasize;
	int numbuffers;
	int halffloat;
} GPUPixelBuffer;

void GPU_pixelbuffer_free(GPUPixelBuffer *pb)
{
	if (pb->bindcode[0])
		glDeleteBuffersARB(pb->numbuffers, pb->bindcode);
	MEM_freeN(pb);
}

GPUPixelBuffer *gpu_pixelbuffer_create(int x, int y, int halffloat, int numbuffers)
{
	GPUPixelBuffer *pb;

	if (!GLEW_ARB_multitexture || !GLEW_EXT_pixel_buffer_object)
		return NULL;
	
	pb = MEM_callocN(sizeof(GPUPixelBuffer), "GPUPBO");
	pb->datasize = x*y*4*((halffloat)? 16: 8);
	pb->numbuffers = numbuffers;
	pb->halffloat = halffloat;

	glGenBuffersARB(pb->numbuffers, pb->bindcode);

	if (!pb->bindcode[0]) {
		fprintf(stderr, "GPUPixelBuffer allocation failed\n");
		GPU_pixelbuffer_free(pb);
		return NULL;
	}

	return pb;
}

void GPU_pixelbuffer_texture(GPUTexture *tex, GPUPixelBuffer *pb)
{
	void *pixels;
	int i;

	glBindTexture(GL_TEXTURE_RECTANGLE_EXT, tex->bindcode);

	for (i = 0; i < pb->numbuffers; i++) {
		glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, pb->bindcode[pb->current]);
		glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_EXT, pb->datasize, NULL,
		GL_STREAM_DRAW_ARB);

		pixels = glMapBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, GL_WRITE_ONLY);
		/*memcpy(pixels, _oImage.data(), pb->datasize);*/

		if (!glUnmapBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT)) {
			fprintf(stderr, "Could not unmap opengl PBO\n");
			break;
		}
	}

	glBindTexture(GL_TEXTURE_RECTANGLE_EXT, 0);
}

static int pixelbuffer_map_into_gpu(GLuint bindcode)
{
	void *pixels;

	glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, bindcode);
	pixels = glMapBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, GL_WRITE_ONLY);

	/* do stuff in pixels */

	if (!glUnmapBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT)) {
		fprintf(stderr, "Could not unmap opengl PBO\n");
		return 0;
	}
	
	return 1;
}

static void pixelbuffer_copy_to_texture(GPUTexture *tex, GPUPixelBuffer *pb, GLuint bindcode)
{
	GLenum type = (pb->halffloat)? GL_HALF_FLOAT_NV: GL_UNSIGNED_BYTE;
	glBindTexture(GL_TEXTURE_RECTANGLE_EXT, tex->bindcode);
	glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, bindcode);

	glTexSubImage2D(GL_TEXTURE_RECTANGLE_EXT, 0, 0, 0, tex->w, tex->h,
					GL_RGBA, type, NULL);

	glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, 0);
	glBindTexture(GL_TEXTURE_RECTANGLE_EXT, 0);
}

void GPU_pixelbuffer_async_to_gpu(GPUTexture *tex, GPUPixelBuffer *pb)
{
	int newbuffer;

	if (pb->numbuffers == 1) {
		pixelbuffer_copy_to_texture(tex, pb, pb->bindcode[0]);
		pixelbuffer_map_into_gpu(pb->bindcode[0]);
	}
	else {
		pb->current = (pb->current+1)%pb->numbuffers;
		newbuffer = (pb->current+1)%pb->numbuffers;

		pixelbuffer_map_into_gpu(pb->bindcode[newbuffer]);
		pixelbuffer_copy_to_texture(tex, pb, pb->bindcode[pb->current]);
	}
}
#endif

