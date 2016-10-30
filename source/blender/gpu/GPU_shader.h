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

/** \file GPU_shader.h
 *  \ingroup gpu
 */

#ifndef __GPU_SHADER_H__
#define __GPU_SHADER_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GPUShader GPUShader;
struct GPUTexture;

/* GPU Shader
 * - only for fragment shaders now
 * - must call texture bind before setting a texture as uniform! */

enum {
	GPU_SHADER_FLAGS_NONE = 0,
	GPU_SHADER_FLAGS_SPECIAL_OPENSUBDIV = (1 << 0),
	GPU_SHADER_FLAGS_NEW_SHADING        = (1 << 1),
};

GPUShader *GPU_shader_create(
        const char *vertexcode,
        const char *fragcode,
        const char *geocode,
        const char *libcode,
        const char *defines,
        int input, int output, int number);
GPUShader *GPU_shader_create_ex(
        const char *vertexcode,
        const char *fragcode,
        const char *geocode,
        const char *libcode,
        const char *defines,
        int input, int output, int number,
        const int flags);
void GPU_shader_free(GPUShader *shader);

void GPU_shader_bind(GPUShader *shader);
void GPU_shader_unbind(void);

void *GPU_shader_get_interface(GPUShader *shader);
void GPU_shader_set_interface(GPUShader *shader, void *interface);
int GPU_shader_get_uniform(GPUShader *shader, const char *name);
void GPU_shader_uniform_vector(GPUShader *shader, int location, int length,
	int arraysize, const float *value);
void GPU_shader_uniform_vector_int(GPUShader *shader, int location, int length,
	int arraysize, const int *value);

void GPU_shader_uniform_texture(GPUShader *shader, int location, struct GPUTexture *tex);
void GPU_shader_uniform_int(GPUShader *shader, int location, int value);
void GPU_shader_geometry_stage_primitive_io(GPUShader *shader, int input, int output, int number);

int GPU_shader_get_attribute(GPUShader *shader, const char *name);

/* Builtin/Non-generated shaders */
typedef enum GPUBuiltinShader {
	GPU_SHADER_VSM_STORE         = 0,
	GPU_SHADER_SEP_GAUSSIAN_BLUR = 1,
	GPU_SHADER_SMOKE             = 2,
	GPU_SHADER_SMOKE_FIRE        = 3,
	GPU_SHADER_SMOKE_COBA        = 4,
} GPUBuiltinShader;

GPUShader *GPU_shader_get_builtin_shader(GPUBuiltinShader shader);
GPUShader *GPU_shader_get_builtin_fx_shader(int effects, bool persp);

void GPU_shader_free_builtin_shaders(void);

/* Vertex attributes for shaders */

#define GPU_MAX_ATTRIB 32

typedef struct GPUVertexAttribs {
	struct {
		int type;
		int glindex;
		int glinfoindoex;
		int gltexco;
		int attribid;
		char name[64];	/* MAX_CUSTOMDATA_LAYER_NAME */
	} layer[GPU_MAX_ATTRIB];

	int totlayer;
} GPUVertexAttribs;

#ifdef __cplusplus
}
#endif

#endif  /* __GPU_SHADER_H__ */
