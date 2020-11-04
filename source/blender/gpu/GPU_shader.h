/*
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
 */

/** \file
 * \ingroup gpu
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct GPUTexture;
struct GPUUniformBuf;
struct GPUVertBuf;

/** Opaque type hidding blender::gpu::Shader */
typedef struct GPUShader GPUShader;

typedef enum eGPUShaderTFBType {
  GPU_SHADER_TFB_NONE = 0, /* Transform feedback unsupported. */
  GPU_SHADER_TFB_POINTS = 1,
  GPU_SHADER_TFB_LINES = 2,
  GPU_SHADER_TFB_TRIANGLES = 3,
} eGPUShaderTFBType;

GPUShader *GPU_shader_create(const char *vertcode,
                             const char *fragcode,
                             const char *geomcode,
                             const char *libcode,
                             const char *defines,
                             const char *shname);
GPUShader *GPU_shader_create_from_python(const char *vertcode,
                                         const char *fragcode,
                                         const char *geomcode,
                                         const char *libcode,
                                         const char *defines);
GPUShader *GPU_shader_create_ex(const char *vertcode,
                                const char *fragcode,
                                const char *geomcode,
                                const char *libcode,
                                const char *defines,
                                const eGPUShaderTFBType tf_type,
                                const char **tf_names,
                                const int tf_count,
                                const char *shname);

struct GPU_ShaderCreateFromArray_Params {
  const char **vert, **geom, **frag, **defs;
};
struct GPUShader *GPU_shader_create_from_arrays_impl(
    const struct GPU_ShaderCreateFromArray_Params *params, const char *func, int line);

#define GPU_shader_create_from_arrays(...) \
  GPU_shader_create_from_arrays_impl( \
      &(const struct GPU_ShaderCreateFromArray_Params)__VA_ARGS__, __func__, __LINE__)

#define GPU_shader_create_from_arrays_named(name, ...) \
  GPU_shader_create_from_arrays_impl( \
      &(const struct GPU_ShaderCreateFromArray_Params)__VA_ARGS__, name, 0)

void GPU_shader_free(GPUShader *shader);

void GPU_shader_bind(GPUShader *shader);
void GPU_shader_unbind(void);

/* Returns true if transform feedback was successfully enabled. */
bool GPU_shader_transform_feedback_enable(GPUShader *shader, struct GPUVertBuf *vertbuf);
void GPU_shader_transform_feedback_disable(GPUShader *shader);

int GPU_shader_get_program(GPUShader *shader);

typedef enum {
  GPU_UNIFORM_MODEL = 0,      /* mat4 ModelMatrix */
  GPU_UNIFORM_VIEW,           /* mat4 ViewMatrix */
  GPU_UNIFORM_MODELVIEW,      /* mat4 ModelViewMatrix */
  GPU_UNIFORM_PROJECTION,     /* mat4 ProjectionMatrix */
  GPU_UNIFORM_VIEWPROJECTION, /* mat4 ViewProjectionMatrix */
  GPU_UNIFORM_MVP,            /* mat4 ModelViewProjectionMatrix */

  GPU_UNIFORM_MODEL_INV,          /* mat4 ModelMatrixInverse */
  GPU_UNIFORM_VIEW_INV,           /* mat4 ViewMatrixInverse */
  GPU_UNIFORM_MODELVIEW_INV,      /* mat4 ModelViewMatrixInverse */
  GPU_UNIFORM_PROJECTION_INV,     /* mat4 ProjectionMatrixInverse */
  GPU_UNIFORM_VIEWPROJECTION_INV, /* mat4 ViewProjectionMatrixInverse */

  GPU_UNIFORM_NORMAL,     /* mat3 NormalMatrix */
  GPU_UNIFORM_ORCO,       /* vec4 OrcoTexCoFactors[] */
  GPU_UNIFORM_CLIPPLANES, /* vec4 WorldClipPlanes[] */

  GPU_UNIFORM_COLOR,          /* vec4 color */
  GPU_UNIFORM_BASE_INSTANCE,  /* int baseInstance */
  GPU_UNIFORM_RESOURCE_CHUNK, /* int resourceChunk */
  GPU_UNIFORM_RESOURCE_ID,    /* int resourceId */
  GPU_UNIFORM_SRGB_TRANSFORM, /* bool srgbTarget */

  GPU_NUM_UNIFORMS, /* Special value, denotes number of builtin uniforms. */
} GPUUniformBuiltin;

typedef enum {
  GPU_UNIFORM_BLOCK_VIEW = 0, /* viewBlock */
  GPU_UNIFORM_BLOCK_MODEL,    /* modelBlock */
  GPU_UNIFORM_BLOCK_INFO,     /* infoBlock */

  GPU_NUM_UNIFORM_BLOCKS, /* Special value, denotes number of builtin uniforms block. */
} GPUUniformBlockBuiltin;

void GPU_shader_set_srgb_uniform(GPUShader *shader);

int GPU_shader_get_uniform(GPUShader *shader, const char *name);
int GPU_shader_get_builtin_uniform(GPUShader *shader, int builtin);
int GPU_shader_get_builtin_block(GPUShader *shader, int builtin);
int GPU_shader_get_uniform_block(GPUShader *shader, const char *name);

int GPU_shader_get_uniform_block_binding(GPUShader *shader, const char *name);
int GPU_shader_get_texture_binding(GPUShader *shader, const char *name);

void GPU_shader_uniform_vector(
    GPUShader *shader, int location, int length, int arraysize, const float *value);
void GPU_shader_uniform_vector_int(
    GPUShader *shader, int location, int length, int arraysize, const int *value);

void GPU_shader_uniform_float(GPUShader *shader, int location, float value);
void GPU_shader_uniform_int(GPUShader *shader, int location, int value);

void GPU_shader_uniform_1i(GPUShader *sh, const char *name, int value);
void GPU_shader_uniform_1b(GPUShader *sh, const char *name, bool value);
void GPU_shader_uniform_1f(GPUShader *sh, const char *name, float value);
void GPU_shader_uniform_2f(GPUShader *sh, const char *name, float x, float y);
void GPU_shader_uniform_3f(GPUShader *sh, const char *name, float x, float y, float z);
void GPU_shader_uniform_4f(GPUShader *sh, const char *name, float x, float y, float z, float w);
void GPU_shader_uniform_2fv(GPUShader *sh, const char *name, const float data[2]);
void GPU_shader_uniform_3fv(GPUShader *sh, const char *name, const float data[3]);
void GPU_shader_uniform_4fv(GPUShader *sh, const char *name, const float data[4]);
void GPU_shader_uniform_mat4(GPUShader *sh, const char *name, const float data[4][4]);
void GPU_shader_uniform_2fv_array(GPUShader *sh, const char *name, int len, const float (*val)[2]);
void GPU_shader_uniform_4fv_array(GPUShader *sh, const char *name, int len, const float (*val)[4]);

int GPU_shader_get_attribute(GPUShader *shader, const char *name);

void GPU_shader_set_framebuffer_srgb_target(int use_srgb_to_linear);

/* Builtin/Non-generated shaders */
typedef enum eGPUBuiltinShader {
  /* specialized drawing */
  GPU_SHADER_TEXT,
  GPU_SHADER_KEYFRAME_DIAMOND,
  GPU_SHADER_SIMPLE_LIGHTING,
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
  GPU_SHADER_2D_IMAGE,
  GPU_SHADER_2D_IMAGE_COLOR,
  GPU_SHADER_2D_IMAGE_DESATURATE_COLOR,
  GPU_SHADER_2D_IMAGE_RECT_COLOR,
  GPU_SHADER_2D_IMAGE_MULTI_RECT_COLOR,
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
  GPU_SHADER_3D_CLIPPED_UNIFORM_COLOR,
  /**
   * Take a 3D position and color for each vertex without color interpolation.
   *
   * \param color: in vec4
   * \param pos: in vec3
   */
  GPU_SHADER_3D_FLAT_COLOR,
  /**
   * Take a 3D position and color for each vertex with perspective correct interpolation.
   *
   * \param color: in vec4
   * \param pos: in vec3
   */
  GPU_SHADER_3D_SMOOTH_COLOR,
  /**
   * Take a single color for all the vertices and a 3D position for each vertex.
   * Used for drawing wide lines.
   *
   * \param color: uniform vec4
   * \param pos: in vec3
   */
  GPU_SHADER_3D_POLYLINE_UNIFORM_COLOR,
  GPU_SHADER_3D_POLYLINE_CLIPPED_UNIFORM_COLOR,
  /**
   * Take a 3D position and color for each vertex without color interpolation.
   * Used for drawing wide lines.
   *
   * \param color: in vec4
   * \param pos: in vec3
   */
  GPU_SHADER_3D_POLYLINE_FLAT_COLOR,
  /**
   * Take a 3D position and color for each vertex with perspective correct interpolation.
   * Used for drawing wide lines.
   *
   * \param color: in vec4
   * \param pos: in vec3
   */
  GPU_SHADER_3D_POLYLINE_SMOOTH_COLOR,
  /**
   * Take a 3D position for each vertex and output only depth.
   * Used for drawing wide lines.
   *
   * \param pos: in vec3
   */
  GPU_SHADER_3D_DEPTH_ONLY,
  /* basic image drawing */
  GPU_SHADER_2D_IMAGE_OVERLAYS_MERGE,
  GPU_SHADER_2D_IMAGE_OVERLAYS_STEREO_MERGE,
  GPU_SHADER_2D_IMAGE_SHUFFLE_COLOR,
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
   * Draw round points with a constant size and an outline.
   * Take a 2D position and a color for each vertex.
   *
   * \param size: uniform float
   * \param outlineWidth: uniform float
   * \param outlineColor: uniform vec4
   * \param color: in vec4
   * \param pos: in vec2
   */
  GPU_SHADER_2D_POINT_UNIFORM_SIZE_VARYING_COLOR_OUTLINE_AA,
  /**
   * Draw round points with a constant size and an outline.
   * Take a 2D position and a color for each vertex.
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
   * Draw round points with a constant size and an outline.
   * Take a 3D position and a color for each vertex.
   *
   * \param size: in float
   * \param color: in vec4
   * \param pos: in vec3
   */
  GPU_SHADER_3D_POINT_VARYING_SIZE_VARYING_COLOR,
  /* lines */
  GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR,
  GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR,
  /* instance */
  GPU_SHADER_INSTANCE_VARIYING_COLOR_VARIYING_SIZE, /* Uniformly scaled */
  /* grease pencil drawing */
  GPU_SHADER_GPENCIL_STROKE,
  GPU_SHADER_GPENCIL_FILL,
  /* specialized for widget drawing */
  GPU_SHADER_2D_AREA_EDGES,
  GPU_SHADER_2D_WIDGET_BASE,
  GPU_SHADER_2D_WIDGET_BASE_INST,
  GPU_SHADER_2D_WIDGET_SHADOW,
  GPU_SHADER_2D_NODELINK,
  GPU_SHADER_2D_NODELINK_INST,
  /* specialized for edituv drawing */
  GPU_SHADER_2D_UV_UNIFORM_COLOR,
  GPU_SHADER_2D_UV_VERTS,
  GPU_SHADER_2D_UV_FACEDOTS,
  GPU_SHADER_2D_UV_EDGES,
  GPU_SHADER_2D_UV_EDGES_SMOOTH,
  GPU_SHADER_2D_UV_FACES,
  GPU_SHADER_2D_UV_FACES_STRETCH_AREA,
  GPU_SHADER_2D_UV_FACES_STRETCH_ANGLE,
} eGPUBuiltinShader;
#define GPU_SHADER_BUILTIN_LEN (GPU_SHADER_2D_UV_FACES_STRETCH_ANGLE + 1)

/** Support multiple configurations. */
typedef enum eGPUShaderConfig {
  GPU_SHADER_CFG_DEFAULT = 0,
  GPU_SHADER_CFG_CLIPPED = 1,
} eGPUShaderConfig;
#define GPU_SHADER_CFG_LEN (GPU_SHADER_CFG_CLIPPED + 1)

typedef struct GPUShaderConfigData {
  const char *lib;
  const char *def;
} GPUShaderConfigData;
/* gpu_shader.c */
extern const GPUShaderConfigData GPU_shader_cfg_data[GPU_SHADER_CFG_LEN];

GPUShader *GPU_shader_get_builtin_shader_with_config(eGPUBuiltinShader shader,
                                                     eGPUShaderConfig sh_cfg);
GPUShader *GPU_shader_get_builtin_shader(eGPUBuiltinShader shader);

void GPU_shader_get_builtin_shader_code(eGPUBuiltinShader shader,
                                        const char **r_vert,
                                        const char **r_frag,
                                        const char **r_geom,
                                        const char **r_defines);

void GPU_shader_free_builtin_shaders(void);

/* Vertex attributes for shaders */

/* Hardware limit is 16. Position attribute is always needed so we reduce to 15.
 * This makes sure the GPUVertexFormat name buffer does not overflow. */
#define GPU_MAX_ATTR 15

/* Determined by the maximum uniform buffer size divided by chunk size. */
#define GPU_MAX_UNIFORM_ATTR 8

#ifdef __cplusplus
}
#endif
