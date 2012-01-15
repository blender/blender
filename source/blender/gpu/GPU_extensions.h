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

/** \file GPU_extensions.h
 *  \ingroup gpu
 */

#ifndef GPU_EXTENSIONS_H
#define GPU_EXTENSIONS_H

#ifdef __cplusplus
extern "C" {
#endif

struct Image;
struct ImageUser;

struct GPUTexture;
typedef struct GPUTexture GPUTexture;

struct GPUFrameBuffer;
typedef struct GPUFrameBuffer GPUFrameBuffer;

struct GPUOffScreen;
typedef struct GPUOffScreen GPUOffScreen;

struct GPUShader;
typedef struct GPUShader GPUShader;

/* GPU extensions support */

void GPU_extensions_disable(void);
void GPU_extensions_init(void); /* call this before running any of the functions below */
void GPU_extensions_exit(void);
int GPU_print_error(const char *str);

int GPU_glsl_support(void);
int GPU_non_power_of_two_support(void);
int GPU_color_depth(void);
void GPU_code_generate_glsl_lib(void);
int GPU_bicubic_bump_support(void);

/* GPU Types */

typedef enum GPUDeviceType {
	GPU_DEVICE_NVIDIA =		(1<<0),
	GPU_DEVICE_ATI =		(1<<1),
	GPU_DEVICE_INTEL =		(1<<2),
	GPU_DEVICE_SOFTWARE =	(1<<3),
	GPU_DEVICE_UNKNOWN = 	(1<<4),
	GPU_DEVICE_ANY = 		(0xff)
} GPUDeviceType;

typedef enum GPUOSType {
	GPU_OS_WIN = 			(1<<8),
	GPU_OS_MAC = 			(1<<9),
	GPU_OS_UNIX =			(1<<10),
	GPU_OS_ANY =			(0xff00)
} GPUOSType;

typedef enum GPUDriverType {
	GPU_DRIVER_OFFICIAL =	(1<<16),
	GPU_DRIVER_OPENSOURCE = (1<<17),
	GPU_DRIVER_SOFTWARE =	(1<<18),
	GPU_DRIVER_ANY =		(0xff0000)
} GPUDriverType;

int GPU_type_matches(GPUDeviceType device, GPUOSType os, GPUDriverType driver);

/* GPU Texture
   - always returns unsigned char RGBA textures
   - if texture with non square dimensions is created, depending on the
	 graphics card capabilities the texture may actually be stored in a
	 larger texture with power of two dimensions. the actual dimensions
	 may be queried with GPU_texture_opengl_width/height. GPU_texture_coord_2f
	 calls glTexCoord2f with the coordinates adjusted for this.
   - can use reference counting:
	   - reference counter after GPU_texture_create is 1
	   - GPU_texture_ref increases by one
	   - GPU_texture_free decreases by one, and frees if 0
	- if created with from_blender, will not free the texture
*/

GPUTexture *GPU_texture_create_1D(int w, float *pixels, char err_out[256]);
GPUTexture *GPU_texture_create_2D(int w, int h, float *pixels, char err_out[256]);
GPUTexture *GPU_texture_create_3D(int w, int h, int depth, float *fpixels);
GPUTexture *GPU_texture_create_depth(int w, int h, char err_out[256]);
GPUTexture *GPU_texture_from_blender(struct Image *ima,
	struct ImageUser *iuser, double time, int mipmap);
void GPU_texture_free(GPUTexture *tex);

void GPU_texture_ref(GPUTexture *tex);

void GPU_texture_bind(GPUTexture *tex, int number);
void GPU_texture_unbind(GPUTexture *tex);

GPUFrameBuffer *GPU_texture_framebuffer(GPUTexture *tex);

int GPU_texture_target(GPUTexture *tex);
int GPU_texture_opengl_width(GPUTexture *tex);
int GPU_texture_opengl_height(GPUTexture *tex);
int GPU_texture_opengl_bindcode(GPUTexture *tex);

/* GPU Framebuffer
   - this is a wrapper for an OpenGL framebuffer object (FBO). in practice
	 multiple FBO's may be created, to get around limitations on the number
	 of attached textures and the dimension requirements.
   - after any of the GPU_framebuffer_* functions, GPU_framebuffer_restore must
	 be called before rendering to the window framebuffer again */

GPUFrameBuffer *GPU_framebuffer_create(void);
int GPU_framebuffer_texture_attach(GPUFrameBuffer *fb, GPUTexture *tex, char err_out[256]);
void GPU_framebuffer_texture_detach(GPUFrameBuffer *fb, GPUTexture *tex);
void GPU_framebuffer_texture_bind(GPUFrameBuffer *fb, GPUTexture *tex, int w, int h);
void GPU_framebuffer_texture_unbind(GPUFrameBuffer *fb, GPUTexture *tex);
void GPU_framebuffer_free(GPUFrameBuffer *fb);

void GPU_framebuffer_restore(void);

/* GPU OffScreen
   - wrapper around framebuffer and texture for simple offscreen drawing 
   - changes size if graphics card can't support it */

GPUOffScreen *GPU_offscreen_create(int width, int height, char err_out[256]);
void GPU_offscreen_free(GPUOffScreen *ofs);
void GPU_offscreen_bind(GPUOffScreen *ofs);
void GPU_offscreen_unbind(GPUOffScreen *ofs);
void GPU_offscreen_read_pixels(GPUOffScreen *ofs, int type, void *pixels);

/* GPU Shader
   - only for fragment shaders now
   - must call texture bind before setting a texture as uniform! */

GPUShader *GPU_shader_create(const char *vertexcode, const char *fragcode, const char *libcode); /*GPUShader *lib);*/
/*GPUShader *GPU_shader_create_lib(const char *code);*/
void GPU_shader_free(GPUShader *shader);

void GPU_shader_bind(GPUShader *shader);
void GPU_shader_unbind(GPUShader *shader);

int GPU_shader_get_uniform(GPUShader *shader, const char *name);
void GPU_shader_uniform_vector(GPUShader *shader, int location, int length,
	int arraysize, float *value);
void GPU_shader_uniform_texture(GPUShader *shader, int location, GPUTexture *tex);

int GPU_shader_get_attribute(GPUShader *shader, const char *name);

/* Vertex attributes for shaders */

#define GPU_MAX_ATTRIB		32

typedef struct GPUVertexAttribs {
	struct {
		int type;
		int glindex;
		int gltexco;
		int attribid;
		char name[64];	/* MAX_CUSTOMDATA_LAYER_NAME */
	} layer[GPU_MAX_ATTRIB];

	int totlayer;
} GPUVertexAttribs;

#ifdef __cplusplus
}
#endif

#endif

