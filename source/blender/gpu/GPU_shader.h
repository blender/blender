/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * A #GPUShader is a container for backend specific shader program.
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

/* Hardware limit is 16. Position attribute is always needed so we reduce to 15.
 * This makes sure the GPUVertexFormat name buffer does not overflow. */
#define GPU_MAX_ATTR 15

/* Determined by the maximum uniform buffer size divided by chunk size. */
#define GPU_MAX_UNIFORM_ATTR 8

/* -------------------------------------------------------------------- */
/** \name Creation
 * \{ */

/**
 * Create a shader using the given #GPUShaderCreateInfo.
 * Can return a NULL pointer if compilation fails.
 */
GPUShader *GPU_shader_create_from_info(const GPUShaderCreateInfo *_info);

/**
 * Create a shader using a named #GPUShaderCreateInfo registered at startup.
 * These are declared inside `*_info.hh` files using the `GPU_SHADER_CREATE_INFO()` macro.
 * They are also expected to have been flagged using `do_static_compilation`.
 * Can return a NULL pointer if compilation fails.
 */
GPUShader *GPU_shader_create_from_info_name(const char *info_name);

/**
 * Fetch a named #GPUShaderCreateInfo registered at startup.
 * These are declared inside `*_info.hh` files using the `GPU_SHADER_CREATE_INFO()` macro.
 * Can return a NULL pointer if no match is found.
 */
const GPUShaderCreateInfo *GPU_shader_create_info_get(const char *info_name);

/**
 * Error checking for user created shaders.
 * \return true is create info is valid.
 */
bool GPU_shader_create_info_check_error(const GPUShaderCreateInfo *_info, char r_error[128]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Free
 * \{ */

void GPU_shader_free(GPUShader *shader);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Binding
 * \{ */

/**
 * Set the given shader as active shader for the active GPU context.
 * It replaces any already bound shader.
 * All following draw-calls and dispatches will use this shader.
 * Uniform functions need to have the shader bound in order to work. (TODO: until we use
 * glProgramUniform)
 */
void GPU_shader_bind(GPUShader *shader);

/**
 * Unbind the active shader.
 * \note this is a no-op in release builds. But it make sense to actually do it in user land code
 * to detect incorrect API usage.
 */
void GPU_shader_unbind(void);

/**
 * Return the currently bound shader to the active GPU context.
 * \return NULL pointer if no shader is bound of if no context is active.
 */
GPUShader *GPU_shader_get_bound(void);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Debugging introspection API.
 * \{ */

const char *GPU_shader_get_name(GPUShader *shader);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Uniform API.
 * \{ */

/**
 * Returns binding point location.
 * Binding location are given to be set at shader compile time and immutable.
 */
int GPU_shader_get_ubo_binding(GPUShader *shader, const char *name);
int GPU_shader_get_ssbo_binding(GPUShader *shader, const char *name);
int GPU_shader_get_sampler_binding(GPUShader *shader, const char *name);

/**
 * Returns uniform location.
 * If cached, it is faster than querying the interface for each uniform assignment.
 */
int GPU_shader_get_uniform(GPUShader *shader, const char *name);

/**
 * Returns specialization constant location.
 */
int GPU_shader_get_constant(GPUShader *shader, const char *name);

/**
 * Sets a generic push constant (a.k.a. uniform).
 * \a length and \a array_size should match the create info push_constant declaration.
 */
void GPU_shader_uniform_float_ex(
    GPUShader *shader, int location, int length, int array_size, const float *value);
void GPU_shader_uniform_int_ex(
    GPUShader *shader, int location, int length, int array_size, const int *value);

/**
 * Sets a generic push constant (a.k.a. uniform).
 * \a length and \a array_size should match the create info push_constant declaration.
 * These functions need to have the shader bound in order to work. (TODO: until we use
 * glProgramUniform)
 */
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
void GPU_shader_uniform_1f_array(GPUShader *sh, const char *name, int len, const float *val);
void GPU_shader_uniform_2fv_array(GPUShader *sh, const char *name, int len, const float (*val)[2]);
void GPU_shader_uniform_4fv_array(GPUShader *sh, const char *name, int len, const float (*val)[4]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Attribute API.
 *
 * Used to create #GPUVertexFormat from the shader's vertex input layout.
 * \{ */

unsigned int GPU_shader_get_attribute_len(const GPUShader *shader);
int GPU_shader_get_attribute(const GPUShader *shader, const char *name);
bool GPU_shader_get_attribute_info(const GPUShader *shader,
                                   int attr_location,
                                   char r_name[256],
                                   int *r_type);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Specialization API.
 *
 * Used to allow specialization constants.
 * IMPORTANT: All constants must be specified before binding a shader that needs specialization.
 * Otherwise, it will produce undefined behavior.
 * \{ */

void GPU_shader_constant_int_ex(GPUShader *sh, int location, int value);
void GPU_shader_constant_uint_ex(GPUShader *sh, int location, unsigned int value);
void GPU_shader_constant_float_ex(GPUShader *sh, int location, float value);
void GPU_shader_constant_bool_ex(GPUShader *sh, int location, bool value);

void GPU_shader_constant_int(GPUShader *sh, const char *name, int value);
void GPU_shader_constant_uint(GPUShader *sh, const char *name, unsigned int value);
void GPU_shader_constant_float(GPUShader *sh, const char *name, float value);
void GPU_shader_constant_bool(GPUShader *sh, const char *name, bool value);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Legacy API
 *
 * All of this section is deprecated and should be ported to use the API described above.
 * \{ */

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

/**
 * Returns true if transform feedback was successfully enabled.
 */
bool GPU_shader_transform_feedback_enable(GPUShader *shader, struct GPUVertBuf *vertbuf);
void GPU_shader_transform_feedback_disable(GPUShader *shader);

/**
 * SSBO Vertex-fetch is used as an alternative path to geometry shaders wherein the vertex count is
 * expanded up-front. This function fetches the number of specified output vertices per input
 * primitive.
 */
int GPU_shader_get_ssbo_vertex_fetch_num_verts_per_prim(GPUShader *shader);
bool GPU_shader_uses_ssbo_vertex_fetch(GPUShader *shader);

/**
 * Shader cache warming.
 * For each shader, rendering APIs perform a two-step compilation:
 *
 *  * The first stage is Front-End compilation which only needs to be performed once, and generates
 * a portable intermediate representation. This happens during `gpu::Shader::finalize()`.
 *
 *  * The second is Back-End compilation which compiles a device-specific executable shader
 * program. This compilation requires some contextual pipeline state which is baked into the
 * executable shader source, producing a Pipeline State Object (PSO). In OpenGL, backend
 * compilation happens in the background, within the driver, but can still incur runtime stutters.
 * In Metal/Vulkan, PSOs are compiled explicitly. These are currently resolved within the backend
 * based on the current pipeline state and can incur runtime stalls when they occur.
 *
 * Shader Cache warming uses the specified parent shader set using `GPU_shader_set_parent(..)` as a
 * template reference for pre-compiling Render Pipeline State Objects (PSOs) outside of the main
 * render pipeline.
 *
 * PSOs require descriptors containing information on the render state for a given shader, which
 * includes input vertex data layout and output pixel formats, along with some state such as
 * blend mode and color output masks. As this state information is usually consistent between
 * similar draws, we can assign a parent shader and use this shader's cached pipeline state's to
 * prime compilations.
 *
 * Shaders do not necessarily have to be similar in functionality to be used as a parent, so long
 * as the #GPUVertFormt and #GPUFrameBuffer which they are used with remain the same.
 * Other bindings such as textures, uniforms and UBOs are all assigned independently as dynamic
 * state.
 *
 * This function should be called asynchronously, mitigating the impact of run-time stuttering from
 * dynamic compilation of PSOs during normal rendering.
 *
 * \param: shader: The shader whose cache to warm.
 * \param limit: The maximum number of PSOs to compile within a call. Specifying
 * a limit <= 0 will compile a PSO for all cached PSOs in the parent shader. */
void GPU_shader_warm_cache(GPUShader *shader, int limit);

/* We expect the parent shader to be compiled and already have some cached PSOs when being assigned
 * as a reference. Ensure the parent shader still exists when `GPU_shader_cache_warm(..)` is
 * called. */
void GPU_shader_set_parent(GPUShader *shader, GPUShader *parent);

/** DEPRECATED: Kept only because of BGL API. */
int GPU_shader_get_program(GPUShader *shader);

/**
 * Indexed commonly used uniform name for faster lookup into the uniform cache.
 */
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
} GPUUniformBuiltin;
#define GPU_NUM_UNIFORMS (GPU_UNIFORM_SRGB_TRANSFORM + 1)

/**
 * TODO: To be moved as private API. Not really used outside of gpu_matrix.cc and doesn't really
 * offer a noticeable performance boost.
 */
int GPU_shader_get_builtin_uniform(GPUShader *shader, int builtin);

/**
 * Compile all staticly defined shaders and print a report to the console.
 *
 * This is used for platform support, where bug reports can list all failing shaders.
 */
void GPU_shader_compile_static();

/** DEPRECATED: Use hardcoded buffer location instead. */
typedef enum {
  GPU_UNIFORM_BLOCK_VIEW = 0, /* viewBlock */
  GPU_UNIFORM_BLOCK_MODEL,    /* modelBlock */
  GPU_UNIFORM_BLOCK_INFO,     /* infoBlock */

  GPU_UNIFORM_BLOCK_DRW_VIEW,
  GPU_UNIFORM_BLOCK_DRW_MODEL,
  GPU_UNIFORM_BLOCK_DRW_INFOS,
  GPU_UNIFORM_BLOCK_DRW_CLIPPING,

  GPU_NUM_UNIFORM_BLOCKS, /* Special value, denotes number of builtin uniforms block. */
} GPUUniformBlockBuiltin;

/** DEPRECATED: Use hardcoded buffer location instead. */
int GPU_shader_get_builtin_block(GPUShader *shader, int builtin);

/** DEPRECATED: Kept only because of Python GPU API. */
int GPU_shader_get_uniform_block(GPUShader *shader, const char *name);

/** \} */

#ifdef __cplusplus
}
#endif
