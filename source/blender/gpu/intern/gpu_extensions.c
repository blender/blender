/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#include "GL/glew.h"

#include "DNA_listBase.h"
#include "DNA_image_types.h"
#include "DNA_userdef_types.h"

#include "MEM_guardedalloc.h"

#include "BKE_image.h"
#include "BKE_global.h"
#include "BKE_utildefines.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BLI_blenlib.h"

#include "GPU_draw.h"
#include "GPU_extensions.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Extensions support */

/* extensions used:
	- texture border clamp: 1.3 core
	- fragement shader: 2.0 core
	- framebuffer object: ext specification
	- multitexture 1.3 core
	- arb non power of two: 2.0 core
	- pixel buffer objects? 2.1 core
	- arb draw buffers? 2.0 core
*/

static struct GPUGlobal {
	GLint maxtextures;
	GLuint currentfb;
	int glslsupport;
	int extdisabled;
} GG = {1, 0, 0, 0};

void GPU_extensions_disable()
{
	GG.extdisabled = 1;
}

void GPU_extensions_init()
{
	glewInit();

	/* glewIsSupported("GL_VERSION_2_0") */

	if (GLEW_ARB_multitexture)
		glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS_ARB, &GG.maxtextures);

	GG.glslsupport = 1;
	if (!GLEW_ARB_multitexture) GG.glslsupport = 0;
	if (!GLEW_ARB_vertex_shader) GG.glslsupport = 0;
	if (!GLEW_ARB_fragment_shader) GG.glslsupport = 0;
}

int GPU_glsl_support()
{
	return !GG.extdisabled && GG.glslsupport;
}

int GPU_non_power_of_two_support()
{
	/* Exception for buggy ATI/Apple driver in Mac OS X 10.5/10.6,
	 * they claim to support this but can cause system freeze */
#ifdef __APPLE__
	if(strcmp(glGetString(GL_VENDOR), "ATI Technologies Inc.") == 0)
		return 0;
#endif

	return GLEW_ARB_texture_non_power_of_two;
}

int GPU_print_error(char *str)
{
	GLenum errCode;

	if (G.f & G_DEBUG) {
		if ((errCode = glGetError()) != GL_NO_ERROR) {
    	    fprintf(stderr, "%s opengl error: %s\n", str, gluErrorString(errCode));
			return 1;
		}
	}

	return 0;
}

static void GPU_print_framebuffer_error(GLenum status)
{
	fprintf(stderr, "GPUFrameBuffer: framebuffer incomplete error %d\n",
		(int)status);

	switch(status) {
		case GL_FRAMEBUFFER_COMPLETE_EXT:
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT:
			fprintf(stderr, "Incomplete attachment.\n");
			break;
		case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
			fprintf(stderr, "Unsupported framebuffer format.\n");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
			fprintf(stderr, "Missing attachment.\n");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
			fprintf(stderr, "Attached images must have same dimensions.\n");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
			 fprintf(stderr, "Attached images must have same format.\n");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT:
			fprintf(stderr, "Missing draw buffer.\n");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT:
			fprintf(stderr, "Missing read buffer.\n");
			break;
		default:
			fprintf(stderr, "Unknown.\n");
			break;
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
	int depth;				/* is a depth texture? */
};

static unsigned char *GPU_texture_convert_pixels(int length, float *fpixels)
{
	unsigned char *pixels, *p;
	float *fp;
	int a, len;

	len = 4*length;
	fp = fpixels;
	p = pixels = MEM_callocN(sizeof(unsigned char)*len, "GPUTexturePixels");

	for (a=0; a<len; a++, p++, fp++)
		*p = FTOCHAR((*fp));

	return pixels;
}

static int is_pow2(int n)
{
	return ((n)&(n-1))==0;
}

static int larger_pow2(int n)
{
	if (is_pow2(n))
		return n;

	while(!is_pow2(n))
		n= n&(n-1);

	return n*2;
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

static GPUTexture *GPU_texture_create_nD(int w, int h, int n, float *fpixels, int depth)
{
	GPUTexture *tex;
	GLenum type, format, internalformat;
	void *pixels = NULL;

	if(depth && !GLEW_ARB_depth_texture)
		return NULL;

	tex = MEM_callocN(sizeof(GPUTexture), "GPUTexture");
	tex->w = w;
	tex->h = h;
	tex->number = -1;
	tex->refcount = 1;
	tex->target = (n == 1)? GL_TEXTURE_1D: GL_TEXTURE_2D;
	tex->depth = depth;

	glGenTextures(1, &tex->bindcode);

	if (!tex->bindcode) {
		fprintf(stderr, "GPUTexture: texture create failed: %d\n",
			(int)glGetError());
		GPU_texture_free(tex);
		return NULL;
	}

	if (!GPU_non_power_of_two_support()) {
		tex->w = larger_pow2(tex->w);
		tex->h = larger_pow2(tex->h);
	}

	tex->number = 0;
	glBindTexture(tex->target, tex->bindcode);

	if(depth) {
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
		glTexImage1D(tex->target, 0, internalformat, tex->w, 0, format, type, 0);

		if (fpixels) {
			glTexSubImage1D(tex->target, 0, 0, w, format, type,
				pixels? pixels: fpixels);

			if (tex->w > w)
				GPU_glTexSubImageEmpty(tex->target, format, w, 0,
					tex->w-w, 1);
		}
	}
	else {
		glTexImage2D(tex->target, 0, internalformat, tex->w, tex->h, 0,
			format, type, 0);

		if (fpixels) {
			glTexSubImage2D(tex->target, 0, 0, 0, w, h,
				format, type, pixels? pixels: fpixels);

			if (tex->w > w)
				GPU_glTexSubImageEmpty(tex->target, format, w, 0, tex->w-w, tex->h);
			if (tex->h > h)
				GPU_glTexSubImageEmpty(tex->target, format, 0, h, w, tex->h-h);
		}
	}

	if (pixels)
		MEM_freeN(pixels);

	if(depth) {
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
		GLenum wrapmode = (depth)? GL_CLAMP_TO_EDGE: GL_CLAMP_TO_BORDER;
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


GPUTexture *GPU_texture_create_3D(int w, int h, int depth, float *fpixels)
{
	GPUTexture *tex;
	GLenum type, format, internalformat;
	void *pixels = NULL;
	float vfBorderColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};

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
		tex->w = larger_pow2(tex->w);
		tex->h = larger_pow2(tex->h);
		tex->depth = larger_pow2(tex->depth);
	}

	tex->number = 0;
	glBindTexture(tex->target, tex->bindcode);

	GPU_print_error("3D glBindTexture");

	type = GL_FLOAT; // GL_UNSIGNED_BYTE
	format = GL_RED;
	internalformat = GL_INTENSITY;

	//if (fpixels)
	//	pixels = GPU_texture_convert_pixels(w*h*depth, fpixels);

	glTexImage3D(tex->target, 0, internalformat, tex->w, tex->h, tex->depth, 0, format, type, 0);

	GPU_print_error("3D glTexImage3D");

	if (fpixels) {
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

	if (tex)
		GPU_texture_unbind(tex);

	return tex;
}

GPUTexture *GPU_texture_from_blender(Image *ima, ImageUser *iuser, double time, int mipmap)
{
	GPUTexture *tex;
	GLint w, h, border, lastbindcode, bindcode;

	glGetIntegerv(GL_TEXTURE_BINDING_2D, &lastbindcode);

	GPU_update_image_time(ima, time);
	bindcode = GPU_verify_image(ima, 0, 0, 0, mipmap);

	if(ima->gputexture) {
		ima->gputexture->bindcode = bindcode;
		glBindTexture(GL_TEXTURE_2D, lastbindcode);
		return ima->gputexture;
	}

	if(!bindcode) {
		glBindTexture(GL_TEXTURE_2D, lastbindcode);
		return NULL;
	}

	tex = MEM_callocN(sizeof(GPUTexture), "GPUTexture");
	tex->bindcode = bindcode;
	tex->number = -1;
	tex->refcount = 1;
	tex->target = GL_TEXTURE_2D;
	tex->fromblender = 1;

	ima->gputexture= tex;

	if (!glIsTexture(tex->bindcode)) {
		GPU_print_error("Blender Texture");
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

GPUTexture *GPU_texture_create_1D(int w, float *fpixels)
{
	GPUTexture *tex = GPU_texture_create_nD(w, 1, 1, fpixels, 0);

	if (tex)
		GPU_texture_unbind(tex);
	
	return tex;
}

GPUTexture *GPU_texture_create_2D(int w, int h, float *fpixels)
{
	GPUTexture *tex = GPU_texture_create_nD(w, h, 2, fpixels, 0);

	if (tex)
		GPU_texture_unbind(tex);
	
	return tex;
}

GPUTexture *GPU_texture_create_depth(int w, int h)
{
	GPUTexture *tex = GPU_texture_create_nD(w, h, 2, NULL, 1);

	if (tex)
		GPU_texture_unbind(tex);
	
	return tex;
}

void GPU_texture_bind(GPUTexture *tex, int number)
{
	GLenum arbnumber;

	if (number >= GG.maxtextures) {
		GPU_print_error("Not enough texture slots.");
		return;
	}

	if(number == -1)
		return;

	GPU_print_error("Pre Texture Bind");

	arbnumber = (GLenum)((GLuint)GL_TEXTURE0_ARB + number);
	if (number != 0) glActiveTextureARB(arbnumber);
	glBindTexture(tex->target, tex->bindcode);
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

	if(tex->number == -1)
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
			GPU_framebuffer_texture_detach(tex->fb, tex);
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

GPUFrameBuffer *GPU_texture_framebuffer(GPUTexture *tex)
{
	return tex->fb;
}

/* GPUFrameBuffer */

struct GPUFrameBuffer {
	GLuint object;
	GPUTexture *colortex;
	GPUTexture *depthtex;
};

GPUFrameBuffer *GPU_framebuffer_create()
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

	return fb;
}

int GPU_framebuffer_texture_attach(GPUFrameBuffer *fb, GPUTexture *tex)
{
	GLenum status;
	GLenum attachment;

	if(tex->depth)
		attachment = GL_DEPTH_ATTACHMENT_EXT;
	else
		attachment = GL_COLOR_ATTACHMENT0_EXT;

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fb->object);
	GG.currentfb = fb->object;

	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, attachment, 
		tex->target, tex->bindcode, 0);

	if(tex->depth) {
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
	}
	else {
		glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
		glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
	}

	status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);

	if (status != GL_FRAMEBUFFER_COMPLETE_EXT) {
		GPU_framebuffer_restore();
		GPU_print_framebuffer_error(status);
		return 0;
	}

	if(tex->depth)
		fb->depthtex = tex;
	else
		fb->colortex = tex;

	tex->fb= fb;

	return 1;
}

void GPU_framebuffer_texture_detach(GPUFrameBuffer *fb, GPUTexture *tex)
{
	GLenum attachment;

	if(!tex->fb)
		return;

	if(GG.currentfb != tex->fb->object) {
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, tex->fb->object);
		GG.currentfb = tex->fb->object;
	}

	if(tex->depth) {
		fb->depthtex = NULL;
		attachment = GL_DEPTH_ATTACHMENT_EXT;
	}
	else {
		fb->colortex = NULL;
		attachment = GL_COLOR_ATTACHMENT0_EXT;
	}

	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, attachment,
		tex->target, 0, 0);

	tex->fb = NULL;
}

void GPU_framebuffer_texture_bind(GPUFrameBuffer *fb, GPUTexture *tex)
{
	/* push attributes */
	glPushAttrib(GL_ENABLE_BIT);
	glPushAttrib(GL_VIEWPORT_BIT);
	glDisable(GL_SCISSOR_TEST);

	/* bind framebuffer */
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, tex->fb->object);

	/* push matrices and set default viewport and matrix */
	glViewport(0, 0, tex->w, tex->h);
	GG.currentfb = tex->fb->object;

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
}

void GPU_framebuffer_texture_unbind(GPUFrameBuffer *fb, GPUTexture *tex)
{
	/* restore matrix */
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	/* restore attributes */
	glPopAttrib();
	glPopAttrib();
	glEnable(GL_SCISSOR_TEST);
}

void GPU_framebuffer_free(GPUFrameBuffer *fb)
{
	if(fb->depthtex)
		GPU_framebuffer_texture_detach(fb, fb->depthtex);
	if(fb->colortex)
		GPU_framebuffer_texture_detach(fb, fb->colortex);

	if(fb->object) {
		glDeleteFramebuffersEXT(1, &fb->object);

		if (GG.currentfb == fb->object) {
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
			GG.currentfb = 0;
		}
	}

	MEM_freeN(fb);
}

void GPU_framebuffer_restore()
{
	if (GG.currentfb != 0) {
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		GG.currentfb = 0;
	}
}

/* GPUOffScreen */

struct GPUOffScreen {
	GPUFrameBuffer *fb;
	GPUTexture *color;
	GPUTexture *depth;
};

GPUOffScreen *GPU_offscreen_create(int width, int height)
{
	GPUOffScreen *ofs;

	ofs= MEM_callocN(sizeof(GPUOffScreen), "GPUOffScreen");

	ofs->fb = GPU_framebuffer_create();
	if(!ofs->fb) {
		GPU_offscreen_free(ofs);
		return NULL;
	}

	ofs->depth = GPU_texture_create_depth(width, height);
	if(!ofs->depth) {
		GPU_offscreen_free(ofs);
		return NULL;
	}

	if(!GPU_framebuffer_texture_attach(ofs->fb, ofs->depth)) {
		GPU_offscreen_free(ofs);
		return NULL;
	}

	ofs->color = GPU_texture_create_2D(width, height, NULL);
	if(!ofs->color) {
		GPU_offscreen_free(ofs);
		return NULL;
	}

	if(!GPU_framebuffer_texture_attach(ofs->fb, ofs->color)) {
		GPU_offscreen_free(ofs);
		return NULL;
	}

	GPU_framebuffer_restore();

	return ofs;
}

void GPU_offscreen_free(GPUOffScreen *ofs)
{
	if(ofs->fb)
		GPU_framebuffer_free(ofs->fb);
	if(ofs->color)
		GPU_texture_free(ofs->color);
	if(ofs->depth)
		GPU_texture_free(ofs->depth);
	
	MEM_freeN(ofs);
}

void GPU_offscreen_bind(GPUOffScreen *ofs)
{
	glDisable(GL_SCISSOR_TEST);
	GPU_framebuffer_texture_bind(ofs->fb, ofs->color);
}

void GPU_offscreen_unbind(GPUOffScreen *ofs)
{
	GPU_framebuffer_texture_unbind(ofs->fb, ofs->color);
	GPU_framebuffer_restore();
	glEnable(GL_SCISSOR_TEST);
}

/* GPUShader */

struct GPUShader {
	GLhandleARB object;		/* handle for full shader */
	GLhandleARB vertex;		/* handle for vertex shader */
	GLhandleARB fragment;	/* handle for fragment shader */
	GLhandleARB lib;		/* handle for libment shader */
	int totattrib;			/* total number of attributes */
};

static void shader_print_errors(char *task, char *log, const char *code)
{
	const char *c, *pos, *end = code + strlen(code);
	int line = 1;

	fprintf(stderr, "GPUShader: %s error:\n", task);

	if(G.f & G_DEBUG) {
		c = code;
		while ((c < end) && (pos = strchr(c, '\n'))) {
			fprintf(stderr, "%2d  ", line);
			fwrite(c, (pos+1)-c, 1, stderr);
			c = pos+1;
			line++;
		}

		fprintf(stderr, "%s", c);
	}

	fprintf(stderr, "%s\n", log);
}

GPUShader *GPU_shader_create(const char *vertexcode, const char *fragcode, /*GPUShader *lib,*/ const char *libcode)
{
	GLint status;
	GLcharARB log[5000];
	const char *fragsource[2];
	GLsizei length = 0;
	GLint count;
	GPUShader *shader;

	if (!GLEW_ARB_vertex_shader || !GLEW_ARB_fragment_shader)
		return NULL;

	shader = MEM_callocN(sizeof(GPUShader), "GPUShader");

	if(vertexcode)
		shader->vertex = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
	if(fragcode)
		shader->fragment = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
	shader->object = glCreateProgramObjectARB();

	if (!shader->object ||
		(vertexcode && !shader->vertex) ||
		(fragcode && !shader->fragment)) {
		fprintf(stderr, "GPUShader, object creation failed.\n");
		GPU_shader_free(shader);
		return NULL;
	}

	if(vertexcode) {
		glAttachObjectARB(shader->object, shader->vertex);
		glShaderSourceARB(shader->vertex, 1, (const char**)&vertexcode, NULL);

		glCompileShaderARB(shader->vertex);
		glGetObjectParameterivARB(shader->vertex, GL_OBJECT_COMPILE_STATUS_ARB, &status);

		if (!status) {
			glGetInfoLogARB(shader->vertex, sizeof(log), &length, log);
			shader_print_errors("compile", log, vertexcode);

			GPU_shader_free(shader);
			return NULL;
		}
	}

	if(fragcode) {
		count = 0;
		if(libcode) fragsource[count++] = libcode;
		if(fragcode) fragsource[count++] = fragcode;

		glAttachObjectARB(shader->object, shader->fragment);
		glShaderSourceARB(shader->fragment, count, fragsource, NULL);

		glCompileShaderARB(shader->fragment);
		glGetObjectParameterivARB(shader->fragment, GL_OBJECT_COMPILE_STATUS_ARB, &status);

		if (!status) {
			glGetInfoLogARB(shader->fragment, sizeof(log), &length, log);
			shader_print_errors("compile", log, fragcode);

			GPU_shader_free(shader);
			return NULL;
		}
	}

	/*if(lib && lib->lib)
		glAttachObjectARB(shader->object, lib->lib);*/

	glLinkProgramARB(shader->object);
	glGetObjectParameterivARB(shader->object, GL_OBJECT_LINK_STATUS_ARB, &status);
	if (!status) {
		glGetInfoLogARB(shader->object, sizeof(log), &length, log);
		if (fragcode) shader_print_errors("linking", log, fragcode);
		else if (vertexcode) shader_print_errors("linking", log, vertexcode);
		else if (libcode) shader_print_errors("linking", log, libcode);

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

void GPU_shader_unbind()
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

int GPU_shader_get_uniform(GPUShader *shader, char *name)
{
	return glGetUniformLocationARB(shader->object, name);
}

void GPU_shader_uniform_vector(GPUShader *shader, int location, int length, int arraysize, float *value)
{
	if(location == -1)
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

void GPU_shader_uniform_texture(GPUShader *shader, int location, GPUTexture *tex)
{
	GLenum arbnumber;

	if (tex->number >= GG.maxtextures) {
		GPU_print_error("Not enough texture slots.");
		return;
	}
		
	if(tex->number == -1)
		return;

	if(location == -1)
		return;

	GPU_print_error("Pre Uniform Texture");

	arbnumber = (GLenum)((GLuint)GL_TEXTURE0_ARB + tex->number);

	if (tex->number != 0) glActiveTextureARB(arbnumber);
	glBindTexture(tex->target, tex->bindcode);
	glUniform1iARB(location, tex->number);
	glEnable(tex->target);
	if (tex->number != 0) glActiveTextureARB(GL_TEXTURE0_ARB);

	GPU_print_error("Post Uniform Texture");
}

int GPU_shader_get_attribute(GPUShader *shader, char *name)
{
	int index;
	
	GPU_print_error("Pre Get Attribute");

	index = glGetAttribLocationARB(shader->object, name);

	GPU_print_error("Post Get Attribute");

	return index;
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

