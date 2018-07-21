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

typedef enum GPUShaderTFBType {
	GPU_SHADER_TFB_NONE         = 0, /* Transform feedback unsupported. */
	GPU_SHADER_TFB_POINTS       = 1,
	GPU_SHADER_TFB_LINES        = 2,
	GPU_SHADER_TFB_TRIANGLES    = 3,
} GPUShaderTFBType;

GPUShader *GPU_shader_create(
        const char *vertexcode,
        const char *fragcode,
        const char *geocode,
        const char *libcode,
        const char *defines);
GPUShader *GPU_shader_create_ex(
        const char *vertexcode,
        const char *fragcode,
        const char *geocode,
        const char *libcode,
        const char *defines,
        const int flags,
        const GPUShaderTFBType tf_type,
        const char **tf_names,
        const int tf_count);
void GPU_shader_free(GPUShader *shader);

void GPU_shader_bind(GPUShader *shader);
void GPU_shader_unbind(void);

/* Returns true if transform feedback was succesfully enabled. */
bool GPU_shader_transform_feedback_enable(GPUShader *shader, unsigned int vbo_id);
void GPU_shader_transform_feedback_disable(GPUShader *shader);

int GPU_shader_get_program(GPUShader *shader);

void *GPU_shader_get_interface(GPUShader *shader);

int GPU_shader_get_uniform(GPUShader *shader, const char *name);
int GPU_shader_get_builtin_uniform(GPUShader *shader, int builtin);
int GPU_shader_get_uniform_block(GPUShader *shader, const char *name);
void GPU_shader_uniform_vector(
        GPUShader *shader, int location, int length,
        int arraysize, const float *value);
void GPU_shader_uniform_vector_int(
        GPUShader *shader, int location, int length,
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
	GPU_SHADER_TEXT_SIMPLE,
	GPU_SHADER_EDGES_FRONT_BACK_PERSP,
	GPU_SHADER_EDGES_FRONT_BACK_ORTHO,
	GPU_SHADER_EDGES_OVERLAY_SIMPLE,
	GPU_SHADER_EDGES_OVERLAY,
	GPU_SHADER_KEYFRAME_DIAMOND,
	GPU_SHADER_SIMPLE_LIGHTING,
	GPU_SHADER_SIMPLE_LIGHTING_FLAT_COLOR,
	GPU_SHADER_SIMPLE_LIGHTING_SMOOTH_COLOR,
	GPU_SHADER_SIMPLE_LIGHTING_SMOOTH_COLOR_ALPHA,

	/* for simple 2D drawing */
	/**
	 * Take a single color for all the vertices and a 2D position for each vertex.
	 *
	 * \param color: uniform vec4
	 * \param pos: in vec2
	 */
	GPU_SHADER_2D_UNIFORM_COLOR,
	/**
	 * Take a 2D position and color for each vertex without color interpolation.
	 *
	 * \param color: in vec4
	 * \param pos: in vec2
	 */
	GPU_SHADER_2D_FLAT_COLOR,
	/**
	 * Take a 2D position and color for each vertex with linear interpolation in window space.
	 *
	 * \param color: in vec4
	 * \param pos: in vec2
	 */
	GPU_SHADER_2D_SMOOTH_COLOR,
	GPU_SHADER_2D_SMOOTH_COLOR_DITHER,
	GPU_SHADER_2D_IMAGE,
	GPU_SHADER_2D_IMAGE_COLOR,
	GPU_SHADER_2D_IMAGE_DESATURATE_COLOR,
	GPU_SHADER_2D_IMAGE_ALPHA_COLOR,
	GPU_SHADER_2D_IMAGE_ALPHA,
	GPU_SHADER_2D_IMAGE_RECT_COLOR,
	GPU_SHADER_2D_IMAGE_MULTI_RECT_COLOR,
	GPU_SHADER_2D_IMAGE_MULTISAMPLE_2,
	GPU_SHADER_2D_IMAGE_MULTISAMPLE_4,
	GPU_SHADER_2D_IMAGE_MULTISAMPLE_8,
	GPU_SHADER_2D_IMAGE_MULTISAMPLE_16,
	GPU_SHADER_2D_CHECKER,
	GPU_SHADER_2D_DIAG_STRIPES,
	/* for simple 3D drawing */
	/**
	 * Take a single color for all the vertices and a 3D position for each vertex.
	 *
	 * \param color: uniform vec4
	 * \param pos: in vec3
	 */
	GPU_SHADER_3D_UNIFORM_COLOR,
	GPU_SHADER_3D_UNIFORM_COLOR_U32,
	GPU_SHADER_3D_UNIFORM_COLOR_INSTANCE,
	/**
	 * Take a 3D position and color for each vertex without color interpolation.
	 *
	 * \param color: in vec4
	 * \param pos: in vec3
	 */
	GPU_SHADER_3D_FLAT_COLOR,
	GPU_SHADER_3D_FLAT_COLOR_U32,  /* use for select-id's */
	/**
	 * Take a 3D position and color for each vertex with perspective correct interpolation.
	 *
	 * \param color: in vec4
	 * \param pos: in vec3
	 */
	GPU_SHADER_3D_SMOOTH_COLOR,
	/**
	 * Take a 3D position for each vertex and output only depth.
	 *
	 * \param pos: in vec3
	 */
	GPU_SHADER_3D_DEPTH_ONLY,
	GPU_SHADER_3D_CLIPPED_UNIFORM_COLOR,
	/* basic image drawing */
	GPU_SHADER_2D_IMAGE_LINEAR_TO_SRGB,
	GPU_SHADER_2D_IMAGE_SHUFFLE_COLOR,
	GPU_SHADER_2D_IMAGE_MASK_UNIFORM_COLOR,
	/**
	 * Draw texture with alpha. Take a 3D positon and a 2D texture coordinate for each vertex.
	 *
	 * \param alpha: uniform float
	 * \param image: uniform sampler2D
	 * \param texCoord: in vec2
	 * \param pos: in vec3
	 */
	GPU_SHADER_3D_IMAGE_MODULATE_ALPHA,
	/**
	 * Draw linearized depth texture relate to near and far distances.
	 * Take a 3D positon and a 2D texture coordinate for each vertex.
	 *
	 * \param znear: uniform float
	 * \param zfar: uniform float
	 * \param image: uniform sampler2D
	 * \param texCoord: in vec2
	 * \param pos: in vec3
	 */
	GPU_SHADER_3D_IMAGE_DEPTH,
	GPU_SHADER_3D_IMAGE_DEPTH_COPY,
	/* stereo 3d */
	GPU_SHADER_2D_IMAGE_INTERLACE,
	/* points */
	/**
	 * Draw round points with a hardcoded size.
	 * Take a single color for all the vertices and a 2D position for each vertex.
	 *
	 * \param color: uniform vec4
	 * \param pos: in vec2
	 */
	GPU_SHADER_2D_POINT_FIXED_SIZE_UNIFORM_COLOR,
	/**
	 * Draw round points with a constant size.
	 * Take a single color for all the vertices and a 2D position for each vertex.
	 *
	 * \param size: uniform float
	 * \param color: uniform vec4
	 * \param pos: in vec2
	 */
	GPU_SHADER_2D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA,
	/**
	 * Draw round points with a constant size and an outline.
	 * Take a single color for all the vertices and a 2D position for each vertex.
	 *
	 * \param size: uniform float
	 * \param outlineWidth: uniform float
	 * \param color: uniform vec4
	 * \param outlineColor: uniform vec4
	 * \param pos: in vec2
	 */
	GPU_SHADER_2D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_OUTLINE_AA,
	/**
	 * Draw round points with a constant size and an outline. Take a 2D position and a color for each vertex.
	 *
	 * \param size: uniform float
	 * \param outlineWidth: uniform float
	 * \param outlineColor: uniform vec4
	 * \param color: in vec4
	 * \param pos: in vec2
	 */
	GPU_SHADER_2D_POINT_UNIFORM_SIZE_VARYING_COLOR_OUTLINE_AA,
	/**
	 * Draw round points with a constant size and an outline. Take a 2D position and a color for each vertex.
	 *
	 * \param size: in float
	 * \param color: in vec4
	 * \param pos: in vec2
	 */
	GPU_SHADER_2D_POINT_VARYING_SIZE_VARYING_COLOR,
	/**
	 * Draw round points with a hardcoded size.
	 * Take a single color for all the vertices and a 3D position for each vertex.
	 *
	 * \param color: uniform vec4
	 * \param pos: in vec3
	 */
	GPU_SHADER_3D_POINT_FIXED_SIZE_UNIFORM_COLOR,
	/**
	 * Draw round points with a hardcoded size.
	 * Take a single color for all the vertices and a 3D position for each vertex.
	 *
	 * \param color: uniform vec4
	 * \param pos: in vec3
	 */
	GPU_SHADER_3D_POINT_FIXED_SIZE_VARYING_COLOR,
	/**
	 * Draw round points with a constant size.
	 * Take a single color for all the vertices and a 3D position for each vertex.
	 *
	 * \param size: uniform float
	 * \param color: uniform vec4
	 * \param pos: in vec3
	 */
	GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA,
	/**
	 * Draw round points with a constant size and an outline.
	 * Take a single color for all the vertices and a 3D position for each vertex.
	 *
	 * \param size: uniform float
	 * \param outlineWidth: uniform float
	 * \param color: uniform vec4
	 * \param outlineColor: uniform vec4
	 * \param pos: in vec3
	 */
	GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_OUTLINE_AA,
	/**
	 * Draw round points with a constant size and an outline.
	 * Take a single color for all the vertices and a 3D position for each vertex.
	 *
	 * \param color: uniform vec4
	 * \param size: in float
	 * \param pos: in vec3
	 */
	GPU_SHADER_3D_POINT_VARYING_SIZE_UNIFORM_COLOR,
	/**
	 * Draw round points with a constant size and an outline. Take a 3D position and a color for each vertex.
	 *
	 * \param size: in float
	 * \param color: in vec4
	 * \param pos: in vec3
	 */
	GPU_SHADER_3D_POINT_VARYING_SIZE_VARYING_COLOR,
	/* lines */
	GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR,
	GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR,
	/* lamp drawing */
	GPU_SHADER_3D_GROUNDPOINT,
	GPU_SHADER_3D_GROUNDLINE,
	GPU_SHADER_3D_SCREENSPACE_VARIYING_COLOR,
	/* bone drawing */
	GPU_SHADER_3D_OBJECTSPACE_VARIYING_COLOR,
	GPU_SHADER_3D_OBJECTSPACE_SIMPLE_LIGHTING_VARIYING_COLOR,
	/* camera drawing */
	GPU_SHADER_CAMERA,
	/* distance in front of objects */
	GPU_SHADER_DISTANCE_LINES,
	/* axis name */
	GPU_SHADER_3D_INSTANCE_SCREEN_ALIGNED_AXIS,
	GPU_SHADER_3D_INSTANCE_SCREEN_ALIGNED,
	/* instance */
	GPU_SHADER_INSTANCE_UNIFORM_COLOR,
	GPU_SHADER_INSTANCE_VARIYING_ID_VARIYING_SIZE, /* Uniformly scaled */
	GPU_SHADER_INSTANCE_VARIYING_COLOR_VARIYING_SIZE, /* Uniformly scaled */
	GPU_SHADER_INSTANCE_VARIYING_COLOR_VARIYING_SCALE,
	GPU_SHADER_INSTANCE_EDGES_VARIYING_COLOR,
	/* specialized for UI drawing */
	GPU_SHADER_2D_WIDGET_BASE,
	GPU_SHADER_2D_WIDGET_BASE_INST,
	GPU_SHADER_2D_WIDGET_SHADOW,
	GPU_SHADER_2D_NODELINK,
	GPU_SHADER_2D_NODELINK_INST,

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
		char name[64];  /* MAX_CUSTOMDATA_LAYER_NAME */
	} layer[GPU_MAX_ATTRIB];

	int totlayer;
} GPUVertexAttribs;

#ifdef __cplusplus
}
#endif

#endif  /* __GPU_SHADER_H__ */
