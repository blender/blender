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

typedef enum eGPUShaderTFBType {
  GPU_SHADER_TFB_NONE = 0, /* Transform feedback unsupported. */
  GPU_SHADER_TFB_POINTS = 1,
  GPU_SHADER_TFB_LINES = 2,
  GPU_SHADER_TFB_TRIANGLES = 3,
} eGPUShaderTFBType;

GPUShader *GPU_shader_create(const char *vertexcode,
                             const char *fragcode,
                             const char *geocode,
                             const char *libcode,
                             const char *defines,
                             const char *shader_name);
GPUShader *GPU_shader_create_ex(const char *vertexcode,
                                const char *fragcode,
                                const char *geocode,
                                const char *libcode,
                                const char *defines,
                                const eGPUShaderTFBType tf_type,
                                const char **tf_names,
                                const int tf_count,
                                const char *shader_name);
struct GPU_ShaderCreateFromArray_Params {
  const char **vert, **geom, **frag, **defs;
};
struct GPUShader *GPU_shader_create_from_arrays_impl(
    const struct GPU_ShaderCreateFromArray_Params *params);
#define GPU_shader_create_from_arrays(...) \
  GPU_shader_create_from_arrays_impl(&(const struct GPU_ShaderCreateFromArray_Params)__VA_ARGS__)

void GPU_shader_free(GPUShader *shader);

void GPU_shader_bind(GPUShader *shader);
void GPU_shader_unbind(void);

/* Returns true if transform feedback was successfully enabled. */
bool GPU_shader_transform_feedback_enable(GPUShader *shader, unsigned int vbo_id);
void GPU_shader_transform_feedback_disable(GPUShader *shader);

int GPU_shader_get_program(GPUShader *shader);

void *GPU_shader_get_interface(GPUShader *shader);

int GPU_shader_get_uniform(GPUShader *shader, const char *name);
int GPU_shader_get_uniform_ensure(GPUShader *shader, const char *name);
int GPU_shader_get_builtin_uniform(GPUShader *shader, int builtin);
int GPU_shader_get_uniform_block(GPUShader *shader, const char *name);
void GPU_shader_uniform_vector(
    GPUShader *shader, int location, int length, int arraysize, const float *value);
void GPU_shader_uniform_vector_int(
    GPUShader *shader, int location, int length, int arraysize, const int *value);

void GPU_shader_uniform_buffer(GPUShader *shader, int location, struct GPUUniformBuffer *ubo);
void GPU_shader_uniform_texture(GPUShader *shader, int location, struct GPUTexture *tex);
void GPU_shader_uniform_float(GPUShader *shader, int location, float value);
void GPU_shader_uniform_int(GPUShader *shader, int location, int value);

int GPU_shader_get_attribute(GPUShader *shader, const char *name);

/* Builtin/Non-generated shaders */
typedef enum eGPUBuiltinShader {
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
  GPU_SHADER_2D_IMAGE_MULTISAMPLE_2_DEPTH_TEST,
  GPU_SHADER_2D_IMAGE_MULTISAMPLE_4_DEPTH_TEST,
  GPU_SHADER_2D_IMAGE_MULTISAMPLE_8_DEPTH_TEST,
  GPU_SHADER_2D_IMAGE_MULTISAMPLE_16_DEPTH_TEST,
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
  /* Sets Z-depth to 1.0 (draw onto background). */
  GPU_SHADER_3D_UNIFORM_COLOR_BACKGROUND,
  GPU_SHADER_3D_UNIFORM_COLOR_INSTANCE,
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
   * Draw texture with alpha. Take a 3D position and a 2D texture coordinate for each vertex.
   *
   * \param alpha: uniform float
   * \param image: uniform sampler2D
   * \param texCoord: in vec2
   * \param pos: in vec3
   */
  GPU_SHADER_3D_IMAGE_MODULATE_ALPHA,
  /**
   * Draw linearized depth texture relate to near and far distances.
   * Take a 3D position and a 2D texture coordinate for each vertex.
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
  /* light drawing */
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
  GPU_SHADER_INSTANCE_VARIYING_ID_VARIYING_SIZE,    /* Uniformly scaled */
  GPU_SHADER_INSTANCE_VARIYING_COLOR_VARIYING_SIZE, /* Uniformly scaled */
  GPU_SHADER_INSTANCE_VARIYING_COLOR_VARIYING_SCALE,
  GPU_SHADER_INSTANCE_EDGES_VARIYING_COLOR,
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
  /* Selection */
  GPU_SHADER_3D_FLAT_SELECT_ID,
  GPU_SHADER_3D_UNIFORM_SELECT_ID,
} eGPUBuiltinShader;
#define GPU_SHADER_BUILTIN_LEN (GPU_SHADER_3D_UNIFORM_SELECT_ID + 1)

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

/** Keep these in sync with:
 * - `gpu_shader_image_interlace_frag.glsl`
 * - `gpu_shader_image_rect_interlace_frag.glsl`
 */
typedef enum eGPUInterlaceShader {
  GPU_SHADER_INTERLACE_ROW = 0,
  GPU_SHADER_INTERLACE_COLUMN = 1,
  GPU_SHADER_INTERLACE_CHECKER = 2,
} eGPUInterlaceShader;

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

#define GPU_MAX_ATTR 32

typedef struct GPUVertAttrLayers {
  struct {
    int type; /* CustomDataType */
    int attr_id;
    char name[64]; /* MAX_CUSTOMDATA_LAYER_NAME */
  } layer[GPU_MAX_ATTR];

  int totlayer;
} GPUVertAttrLayers;

#ifdef __cplusplus
}
#endif

#endif /* __GPU_SHADER_H__ */
