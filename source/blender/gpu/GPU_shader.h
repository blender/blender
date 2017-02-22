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
struct GPUUniformBuffer;

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

int GPU_shader_get_program(GPUShader *shader);
void *GPU_shader_get_interface(GPUShader *shader);
void GPU_shader_set_interface(GPUShader *shader, void *interface);
int GPU_shader_get_uniform(GPUShader *shader, const char *name);
int GPU_shader_get_uniform_block(GPUShader *shader, const char *name);
void GPU_shader_uniform_vector(GPUShader *shader, int location, int length,
	int arraysize, const float *value);
void GPU_shader_uniform_vector_int(GPUShader *shader, int location, int length,
	int arraysize, const int *value);

void GPU_shader_uniform_buffer(GPUShader *shader, int location, struct GPUUniformBuffer *ubo);
void GPU_shader_uniform_texture(GPUShader *shader, int location, struct GPUTexture *tex);
void GPU_shader_uniform_int(GPUShader *shader, int location, int value);
void GPU_shader_geometry_stage_primitive_io(GPUShader *shader, int input, int output, int number);

int GPU_shader_get_attribute(GPUShader *shader, const char *name);

/* Builtin/Non-generated shaders */
typedef enum GPUBuiltinShader {
	GPU_SHADER_VSM_STORE,
	GPU_SHADER_SEP_GAUSSIAN_BLUR,
	GPU_SHADER_SMOKE,
	GPU_SHADER_SMOKE_FIRE,
	GPU_SHADER_SMOKE_COBA,

	/* specialized drawing */
	GPU_SHADER_TEXT,
	GPU_SHADER_EDGES_FRONT_BACK_PERSP,
	GPU_SHADER_EDGES_FRONT_BACK_ORTHO,
	GPU_SHADER_EDGES_OVERLAY_SIMPLE,
	GPU_SHADER_EDGES_OVERLAY,
	GPU_SHADER_KEYFRAME_DIAMOND,
	GPU_SHADER_SIMPLE_LIGHTING,
	/* for simple 2D drawing */
	GPU_SHADER_2D_UNIFORM_COLOR,
	GPU_SHADER_2D_FLAT_COLOR,
	GPU_SHADER_2D_SMOOTH_COLOR,
	GPU_SHADER_2D_IMAGE_COLOR,
	GPU_SHADER_2D_CHECKER,
	GPU_SHADER_2D_DIAG_STRIPES,
	/* for simple 3D drawing */
	GPU_SHADER_3D_UNIFORM_COLOR,
	GPU_SHADER_3D_UNIFORM_COLOR_INSTANCE,
	GPU_SHADER_3D_FLAT_COLOR,
	GPU_SHADER_3D_SMOOTH_COLOR,
	GPU_SHADER_3D_DEPTH_ONLY,
	/* basic image drawing */
	GPU_SHADER_2D_IMAGE_MASK_UNIFORM_COLOR,
	GPU_SHADER_3D_IMAGE_MODULATE_ALPHA,
	GPU_SHADER_3D_IMAGE_RECT_MODULATE_ALPHA,
	GPU_SHADER_3D_IMAGE_DEPTH,
	/* stereo 3d */
	GPU_SHADER_2D_IMAGE_INTERLACE,
	/* points */
	GPU_SHADER_2D_POINT_FIXED_SIZE_UNIFORM_COLOR,
	GPU_SHADER_2D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_SMOOTH,
	GPU_SHADER_2D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_OUTLINE_SMOOTH,
	GPU_SHADER_2D_POINT_UNIFORM_SIZE_VARYING_COLOR_OUTLINE_SMOOTH,
	GPU_SHADER_2D_POINT_VARYING_SIZE_VARYING_COLOR,
	GPU_SHADER_3D_POINT_FIXED_SIZE_UNIFORM_COLOR,
	GPU_SHADER_3D_POINT_FIXED_SIZE_VARYING_COLOR,
	GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_SMOOTH,
	GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_OUTLINE_SMOOTH,
	GPU_SHADER_3D_POINT_VARYING_SIZE_UNIFORM_COLOR,
	GPU_SHADER_3D_POINT_VARYING_SIZE_VARYING_COLOR,
	/* lamp drawing */
	GPU_SHADER_3D_GROUNDPOINT,
	GPU_SHADER_3D_GROUNDLINE,
	GPU_SHADER_3D_SCREENSPACE_VARIYING_COLOR,
	/* bone drawing */
	GPU_SHADER_3D_OBJECTSPACE_VARIYING_COLOR,
	GPU_SHADER_3D_OBJECTSPACE_SIMPLE_LIGHTING_VARIYING_COLOR,
	/* axis name */
	GPU_SHADER_3D_SCREENSPACE_AXIS,
	/* instance */
	GPU_SHADER_INSTANCE_UNIFORM_COLOR,
	GPU_SHADER_INSTANCE_VARIYING_COLOR_VARIYING_SIZE,

	GPU_NUM_BUILTIN_SHADERS /* (not an actual shader) */
} GPUBuiltinShader;

/* Keep these in sync with:
 *  gpu_shader_image_interlace_frag.glsl
 *  gpu_shader_image_rect_interlace_frag.glsl
 **/
typedef enum GPUInterlaceShader {
	GPU_SHADER_INTERLACE_ROW               = 0,
	GPU_SHADER_INTERLACE_COLUMN            = 1,
	GPU_SHADER_INTERLACE_CHECKER           = 2,
} GPUInterlaceShader;

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
