/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "GPU_shader_builtin.h"

#ifdef __cplusplus
extern "C" {
#endif

struct GPUVertBuf;

/** Opaque type hiding #blender::gpu::shader::ShaderCreateInfo */
typedef struct GPUShaderCreateInfo GPUShaderCreateInfo;
/** Opaque type hiding #blender::gpu::Shader */
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
GPUShader *GPU_shader_create_compute(const char *computecode,
                                     const char *libcode,
                                     const char *defines,
                                     const char *shname);
GPUShader *GPU_shader_create_from_python(const char *vertcode,
                                         const char *fragcode,
                                         const char *geomcode,
                                         const char *libcode,
                                         const char *defines,
                                         const char *name);
GPUShader *GPU_shader_create_ex(const char *vertcode,
                                const char *fragcode,
                                const char *geomcode,
                                const char *computecode,
                                const char *libcode,
                                const char *defines,
                                eGPUShaderTFBType tf_type,
                                const char **tf_names,
                                int tf_count,
                                const char *shname);
GPUShader *GPU_shader_create_from_info(const GPUShaderCreateInfo *_info);
GPUShader *GPU_shader_create_from_info_name(const char *info_name);

const GPUShaderCreateInfo *GPU_shader_create_info_get(const char *info_name);
bool GPU_shader_create_info_check_error(const GPUShaderCreateInfo *_info, char r_error[128]);

struct GPU_ShaderCreateFromArray_Params {
  const char **vert, **geom, **frag, **defs;
};
/**
 * Use via #GPU_shader_create_from_arrays macro (avoids passing in param).
 *
 * Similar to #DRW_shader_create_with_lib with the ability to include libraries for each type of
 * shader.
 *
 * It has the advantage that each item can be conditionally included
 * without having to build the string inline, then free it.
 *
 * \param params: NULL terminated arrays of strings.
 *
 * Example:
 * \code{.c}
 * sh = GPU_shader_create_from_arrays({
 *     .vert = (const char *[]){shader_lib_glsl, shader_vert_glsl, NULL},
 *     .geom = (const char *[]){shader_geom_glsl, NULL},
 *     .frag = (const char *[]){shader_frag_glsl, NULL},
 *     .defs = (const char *[]){"#define DEFINE\n", test ? "#define OTHER_DEFINE\n" : "", NULL},
 * });
 * \endcode
 */
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
GPUShader *GPU_shader_get_bound(void);

const char *GPU_shader_get_name(GPUShader *shader);

/**
 * Returns true if transform feedback was successfully enabled.
 */
bool GPU_shader_transform_feedback_enable(GPUShader *shader, struct GPUVertBuf *vertbuf);
void GPU_shader_transform_feedback_disable(GPUShader *shader);

/** DEPRECATED: Kept only because of BGL API. */
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
  /** Deprecated */
  GPU_UNIFORM_BLOCK_VIEW = 0, /* viewBlock */
  GPU_UNIFORM_BLOCK_MODEL,    /* modelBlock */
  GPU_UNIFORM_BLOCK_INFO,     /* infoBlock */
  /** New ones */
  GPU_UNIFORM_BLOCK_DRW_VIEW,
  GPU_UNIFORM_BLOCK_DRW_MODEL,
  GPU_UNIFORM_BLOCK_DRW_INFOS,
  GPU_UNIFORM_BLOCK_DRW_CLIPPING,

  GPU_NUM_UNIFORM_BLOCKS, /* Special value, denotes number of builtin uniforms block. */
} GPUUniformBlockBuiltin;

typedef enum {
  GPU_STORAGE_BUFFER_DEBUG_VERTS = 0, /* drw_debug_verts_buf */
  GPU_STORAGE_BUFFER_DEBUG_PRINT,     /* drw_debug_print_buf */

  GPU_NUM_STORAGE_BUFFERS, /* Special value, denotes number of builtin buffer blocks. */
} GPUStorageBufferBuiltin;

void GPU_shader_set_srgb_uniform(GPUShader *shader);

int GPU_shader_get_uniform(GPUShader *shader, const char *name);
int GPU_shader_get_builtin_uniform(GPUShader *shader, int builtin);
int GPU_shader_get_builtin_block(GPUShader *shader, int builtin);
int GPU_shader_get_builtin_ssbo(GPUShader *shader, int builtin);
/** DEPRECATED: Kept only because of Python GPU API. */
int GPU_shader_get_uniform_block(GPUShader *shader, const char *name);
int GPU_shader_get_ssbo(GPUShader *shader, const char *name);

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
void GPU_shader_uniform_2iv(GPUShader *sh, const char *name, const int data[2]);
void GPU_shader_uniform_mat4(GPUShader *sh, const char *name, const float data[4][4]);
void GPU_shader_uniform_mat3_as_mat4(GPUShader *sh, const char *name, const float data[3][3]);
void GPU_shader_uniform_2fv_array(GPUShader *sh, const char *name, int len, const float (*val)[2]);
void GPU_shader_uniform_4fv_array(GPUShader *sh, const char *name, int len, const float (*val)[4]);

unsigned int GPU_shader_get_attribute_len(const GPUShader *shader);
int GPU_shader_get_attribute(GPUShader *shader, const char *name);
bool GPU_shader_get_attribute_info(const GPUShader *shader,
                                   int attr_location,
                                   char r_name[256],
                                   int *r_type);

void GPU_shader_set_framebuffer_srgb_target(int use_srgb_to_linear);

/* Vertex attributes for shaders */

/* Hardware limit is 16. Position attribute is always needed so we reduce to 15.
 * This makes sure the GPUVertexFormat name buffer does not overflow. */
#define GPU_MAX_ATTR 15

/* Determined by the maximum uniform buffer size divided by chunk size. */
#define GPU_MAX_UNIFORM_ATTR 8

typedef enum eGPUKeyframeShapes {
  GPU_KEYFRAME_SHAPE_DIAMOND = (1 << 0),
  GPU_KEYFRAME_SHAPE_CIRCLE = (1 << 1),
  GPU_KEYFRAME_SHAPE_CLIPPED_VERTICAL = (1 << 2),
  GPU_KEYFRAME_SHAPE_CLIPPED_HORIZONTAL = (1 << 3),
  GPU_KEYFRAME_SHAPE_INNER_DOT = (1 << 4),
  GPU_KEYFRAME_SHAPE_ARROW_END_MAX = (1 << 8),
  GPU_KEYFRAME_SHAPE_ARROW_END_MIN = (1 << 9),
  GPU_KEYFRAME_SHAPE_ARROW_END_MIXED = (1 << 10),
} eGPUKeyframeShapes;
#define GPU_KEYFRAME_SHAPE_SQUARE \
  (GPU_KEYFRAME_SHAPE_CLIPPED_VERTICAL | GPU_KEYFRAME_SHAPE_CLIPPED_HORIZONTAL)

#ifdef __cplusplus
}
#endif
