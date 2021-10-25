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

/** \file GPU_texture.h
 *  \ingroup gpu
 */

#ifndef __GPU_TEXTURE_H__
#define __GPU_TEXTURE_H__

#ifdef __cplusplus
extern "C" {
#endif

struct Image;
struct ImageUser;
struct PreviewImage;
	
struct GPUFrameBuffer;
typedef struct GPUTexture GPUTexture;

/* GPU Texture
 * - always returns unsigned char RGBA textures
 * - if texture with non square dimensions is created, depending on the
 *   graphics card capabilities the texture may actually be stored in a
 *   larger texture with power of two dimensions.
 * - can use reference counting:
 *     - reference counter after GPU_texture_create is 1
 *     - GPU_texture_ref increases by one
 *     - GPU_texture_free decreases by one, and frees if 0
 *  - if created with from_blender, will not free the texture
 */

typedef enum GPUHDRType {
	GPU_HDR_NONE =       0,
	GPU_HDR_HALF_FLOAT = 1,
	GPU_HDR_FULL_FLOAT = (1 << 1),
} GPUHDRType;

GPUTexture *GPU_texture_create_1D(int w, const float *pixels, char err_out[256]);
GPUTexture *GPU_texture_create_2D(int w, int h, const float *pixels, GPUHDRType hdr, char err_out[256]);
GPUTexture *GPU_texture_create_3D(int w, int h, int depth, int channels, const float *fpixels);
GPUTexture *GPU_texture_create_depth(int w, int h, char err_out[256]);
GPUTexture *GPU_texture_create_vsm_shadow_map(int size, char err_out[256]);
GPUTexture *GPU_texture_create_2D_procedural(int w, int h, const float *pixels, bool repeat, char err_out[256]);
GPUTexture *GPU_texture_create_1D_procedural(int w, const float *pixels, char err_out[256]);
GPUTexture *GPU_texture_create_2D_multisample(
        int w, int h, const float *pixels, GPUHDRType hdr, int samples, char err_out[256]);
GPUTexture *GPU_texture_create_depth_multisample(int w, int h, int samples, char err_out[256]);
GPUTexture *GPU_texture_from_blender(
        struct Image *ima, struct ImageUser *iuser, int textarget, bool is_data, double time, int mipmap);
GPUTexture *GPU_texture_from_preview(struct PreviewImage *prv, int mipmap);
void GPU_invalid_tex_init(void);
void GPU_invalid_tex_bind(int mode);
void GPU_invalid_tex_free(void);

void GPU_texture_free(GPUTexture *tex);

void GPU_texture_ref(GPUTexture *tex);

void GPU_texture_bind(GPUTexture *tex, int number);
void GPU_texture_unbind(GPUTexture *tex);
int GPU_texture_bound_number(GPUTexture *tex);

void GPU_texture_filter_mode(GPUTexture *tex, bool compare, bool use_filter);

struct GPUFrameBuffer *GPU_texture_framebuffer(GPUTexture *tex);
int GPU_texture_framebuffer_attachment(GPUTexture *tex);
void GPU_texture_framebuffer_set(GPUTexture *tex, struct GPUFrameBuffer *fb, int attachment);

int GPU_texture_target(const GPUTexture *tex);
int GPU_texture_width(const GPUTexture *tex);
int GPU_texture_height(const GPUTexture *tex);
int GPU_texture_depth(const GPUTexture *tex);
int GPU_texture_opengl_bindcode(const GPUTexture *tex);

#ifdef __cplusplus
}
#endif

#endif  /* __GPU_TEXTURE_H__ */
