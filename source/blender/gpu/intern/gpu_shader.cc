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

#include "MEM_guardedalloc.h"

#include "BLI_math_base.h"
#include "BLI_math_vector.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "BKE_appdir.h"
#include "BKE_global.h"

#include "DNA_space_types.h"

#include "GPU_extensions.h"
#include "GPU_matrix.h"
#include "GPU_platform.h"
#include "GPU_shader.h"
#include "GPU_texture.h"
#include "GPU_uniformbuffer.h"

#include "gpu_shader_private.h"

extern "C" char datatoc_gpu_shader_colorspace_lib_glsl[];

/* Adjust these constants as needed. */
#define MAX_DEFINE_LENGTH 256
#define MAX_EXT_DEFINE_LENGTH 512

#ifndef NDEBUG
static uint g_shaderid = 0;
#endif

/* -------------------------------------------------------------------- */
/** \name Convenience functions
 * \{ */

static void shader_print_errors(const char *task, const char *log, const char **code, int totcode)
{
  int line = 1;

  fprintf(stderr, "GPUShader: %s error:\n", task);

  for (int i = 0; i < totcode; i++) {
    const char *c, *pos, *end = code[i] + strlen(code[i]);

    if (G.debug & G_DEBUG) {
      fprintf(stderr, "===== shader string %d ====\n", i + 1);

      c = code[i];
      while ((c < end) && (pos = strchr(c, '\n'))) {
        fprintf(stderr, "%2d  ", line);
        fwrite(c, (pos + 1) - c, 1, stderr);
        c = pos + 1;
        line++;
      }

      fprintf(stderr, "%s", c);
    }
  }

  fprintf(stderr, "%s\n", log);
}

static const char *gpu_shader_version(void)
{
  return "#version 330\n";
}

static void gpu_shader_standard_extensions(char defines[MAX_EXT_DEFINE_LENGTH])
{
  /* enable extensions for features that are not part of our base GLSL version
   * don't use an extension for something already available!
   */

  if (GLEW_ARB_texture_gather) {
    /* There is a bug on older Nvidia GPU where GL_ARB_texture_gather
     * is reported to be supported but yield a compile error (see T55802). */
    if (!GPU_type_matches(GPU_DEVICE_NVIDIA, GPU_OS_ANY, GPU_DRIVER_ANY) || GLEW_VERSION_4_0) {
      strcat(defines, "#extension GL_ARB_texture_gather: enable\n");

      /* Some drivers don't agree on GLEW_ARB_texture_gather and the actual support in the
       * shader so double check the preprocessor define (see T56544). */
      if (!GPU_type_matches(GPU_DEVICE_NVIDIA, GPU_OS_ANY, GPU_DRIVER_ANY) && !GLEW_VERSION_4_0) {
        strcat(defines, "#ifdef GL_ARB_texture_gather\n");
        strcat(defines, "#  define GPU_ARB_texture_gather\n");
        strcat(defines, "#endif\n");
      }
      else {
        strcat(defines, "#define GPU_ARB_texture_gather\n");
      }
    }
  }
  if (GLEW_ARB_texture_query_lod) {
    /* a #version 400 feature, but we use #version 330 maximum so use extension */
    strcat(defines, "#extension GL_ARB_texture_query_lod: enable\n");
  }
  if (GLEW_ARB_shader_draw_parameters) {
    strcat(defines, "#extension GL_ARB_shader_draw_parameters : enable\n");
    strcat(defines, "#define GPU_ARB_shader_draw_parameters\n");
  }
  if (GPU_arb_texture_cube_map_array_is_supported()) {
    strcat(defines, "#extension GL_ARB_texture_cube_map_array : enable\n");
    strcat(defines, "#define GPU_ARB_texture_cube_map_array\n");
  }
}

static void gpu_shader_standard_defines(char defines[MAX_DEFINE_LENGTH])
{
  /* some useful defines to detect GPU type */
  if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_ANY, GPU_DRIVER_ANY)) {
    strcat(defines, "#define GPU_ATI\n");
    if (GPU_crappy_amd_driver()) {
      strcat(defines, "#define GPU_DEPRECATED_AMD_DRIVER\n");
    }
  }
  else if (GPU_type_matches(GPU_DEVICE_NVIDIA, GPU_OS_ANY, GPU_DRIVER_ANY)) {
    strcat(defines, "#define GPU_NVIDIA\n");
  }
  else if (GPU_type_matches(GPU_DEVICE_INTEL, GPU_OS_ANY, GPU_DRIVER_ANY)) {
    strcat(defines, "#define GPU_INTEL\n");
  }

  /* some useful defines to detect OS type */
  if (GPU_type_matches(GPU_DEVICE_ANY, GPU_OS_WIN, GPU_DRIVER_ANY)) {
    strcat(defines, "#define OS_WIN\n");
  }
  else if (GPU_type_matches(GPU_DEVICE_ANY, GPU_OS_MAC, GPU_DRIVER_ANY)) {
    strcat(defines, "#define OS_MAC\n");
  }
  else if (GPU_type_matches(GPU_DEVICE_ANY, GPU_OS_UNIX, GPU_DRIVER_ANY)) {
    strcat(defines, "#define OS_UNIX\n");
  }

  float derivatives_factors[2];
  GPU_get_dfdy_factors(derivatives_factors);
  if (derivatives_factors[0] == 1.0f) {
    strcat(defines, "#define DFDX_SIGN 1.0\n");
  }
  else {
    strcat(defines, "#define DFDX_SIGN -1.0\n");
  }

  if (derivatives_factors[1] == 1.0f) {
    strcat(defines, "#define DFDY_SIGN 1.0\n");
  }
  else {
    strcat(defines, "#define DFDY_SIGN -1.0\n");
  }
}

#define DEBUG_SHADER_NONE ""
#define DEBUG_SHADER_VERTEX "vert"
#define DEBUG_SHADER_FRAGMENT "frag"
#define DEBUG_SHADER_GEOMETRY "geom"

/**
 * Dump GLSL shaders to disk
 *
 * This is used for profiling shader performance externally and debug if shader code is correct.
 * If called with no code, it simply bumps the shader index, so different shaders for the same
 * program share the same index.
 */
static void gpu_dump_shaders(const char **code, const int num_shaders, const char *extension)
{
  if ((G.debug & G_DEBUG_GPU_SHADERS) == 0) {
    return;
  }

  /* We use the same shader index for shaders in the same program.
   * So we call this function once before calling for the individual shaders. */
  static int shader_index = 0;
  if (code == NULL) {
    shader_index++;
    BLI_assert(STREQ(DEBUG_SHADER_NONE, extension));
    return;
  }

  /* Determine the full path of the new shader. */
  char shader_path[FILE_MAX];

  char file_name[512] = {'\0'};
  sprintf(file_name, "%04d.%s", shader_index, extension);

  BLI_join_dirfile(shader_path, sizeof(shader_path), BKE_tempdir_session(), file_name);

  /* Write shader to disk. */
  FILE *f = fopen(shader_path, "w");
  if (f == NULL) {
    printf("Error writing to file: %s\n", shader_path);
  }
  for (int j = 0; j < num_shaders; j++) {
    fprintf(f, "%s", code[j]);
  }
  fclose(f);
  printf("Shader file written to disk: %s\n", shader_path);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Creation / Destruction
 * \{ */

GPUShader *GPU_shader_create(const char *vertexcode,
                             const char *fragcode,
                             const char *geocode,
                             const char *libcode,
                             const char *defines,
                             const char *shname)
{
  return GPU_shader_create_ex(
      vertexcode, fragcode, geocode, libcode, defines, GPU_SHADER_TFB_NONE, NULL, 0, shname);
}

GPUShader *GPU_shader_create_from_python(const char *vertexcode,
                                         const char *fragcode,
                                         const char *geocode,
                                         const char *libcode,
                                         const char *defines)
{
  char *libcodecat = NULL;

  if (libcode == NULL) {
    libcode = datatoc_gpu_shader_colorspace_lib_glsl;
  }
  else {
    libcode = libcodecat = BLI_strdupcat(libcode, datatoc_gpu_shader_colorspace_lib_glsl);
  }

  GPUShader *sh = GPU_shader_create_ex(
      vertexcode, fragcode, geocode, libcode, defines, GPU_SHADER_TFB_NONE, NULL, 0, NULL);

  MEM_SAFE_FREE(libcodecat);
  return sh;
}

GPUShader *GPU_shader_load_from_binary(const char *binary,
                                       const int binary_format,
                                       const int binary_len,
                                       const char *shname)
{
  BLI_assert(GL_ARB_get_program_binary);
  int success;
  int program = glCreateProgram();

  glProgramBinary(program, binary_format, binary, binary_len);
  glGetProgramiv(program, GL_LINK_STATUS, &success);

  if (success) {
    glUseProgram(program);

    GPUShader *shader = (GPUShader *)MEM_callocN(sizeof(*shader), __func__);
    shader->interface = GPU_shaderinterface_create(program);
    shader->program = program;

#ifndef NDEBUG
    BLI_snprintf(shader->name, sizeof(shader->name), "%s_%u", shname, g_shaderid++);
#else
    UNUSED_VARS(shname);
#endif

    return shader;
  }

  glDeleteProgram(program);
  return NULL;
}

GPUShader *GPU_shader_create_ex(const char *vertexcode,
                                const char *fragcode,
                                const char *geocode,
                                const char *libcode,
                                const char *defines,
                                const eGPUShaderTFBType tf_type,
                                const char **tf_names,
                                const int tf_count,
                                const char *shname)
{
  GLint status;
  GLchar log[5000];
  GLsizei length = 0;
  GPUShader *shader;
  char standard_defines[MAX_DEFINE_LENGTH] = "";
  char standard_extensions[MAX_EXT_DEFINE_LENGTH] = "";

  shader = (GPUShader *)MEM_callocN(sizeof(GPUShader), "GPUShader");
  gpu_dump_shaders(NULL, 0, DEBUG_SHADER_NONE);

#ifndef NDEBUG
  BLI_snprintf(shader->name, sizeof(shader->name), "%s_%u", shname, g_shaderid++);
#else
  UNUSED_VARS(shname);
#endif

  /* At least a vertex shader and a fragment shader are required. */
  BLI_assert((fragcode != NULL) && (vertexcode != NULL));

  if (vertexcode) {
    shader->vertex = glCreateShader(GL_VERTEX_SHADER);
  }
  if (fragcode) {
    shader->fragment = glCreateShader(GL_FRAGMENT_SHADER);
  }
  if (geocode) {
    shader->geometry = glCreateShader(GL_GEOMETRY_SHADER);
  }

  shader->program = glCreateProgram();

  if (!shader->program || (vertexcode && !shader->vertex) || (fragcode && !shader->fragment) ||
      (geocode && !shader->geometry)) {
    fprintf(stderr, "GPUShader, object creation failed.\n");
    GPU_shader_free(shader);
    return NULL;
  }

  gpu_shader_standard_defines(standard_defines);
  gpu_shader_standard_extensions(standard_extensions);

  if (vertexcode) {
    const char *source[7];
    /* custom limit, may be too small, beware */
    int num_source = 0;

    source[num_source++] = gpu_shader_version();
    source[num_source++] =
        "#define GPU_VERTEX_SHADER\n"
        "#define IN_OUT out\n";
    source[num_source++] = standard_extensions;
    source[num_source++] = standard_defines;

    if (geocode) {
      source[num_source++] = "#define USE_GEOMETRY_SHADER\n";
    }
    if (defines) {
      source[num_source++] = defines;
    }
    source[num_source++] = vertexcode;

    gpu_dump_shaders(source, num_source, DEBUG_SHADER_VERTEX);

    glAttachShader(shader->program, shader->vertex);
    glShaderSource(shader->vertex, num_source, source, NULL);

    glCompileShader(shader->vertex);
    glGetShaderiv(shader->vertex, GL_COMPILE_STATUS, &status);

    if (!status) {
      glGetShaderInfoLog(shader->vertex, sizeof(log), &length, log);
      shader_print_errors("compile", log, source, num_source);

      GPU_shader_free(shader);
      return NULL;
    }
  }

  if (fragcode) {
    const char *source[8];
    int num_source = 0;

    source[num_source++] = gpu_shader_version();
    source[num_source++] =
        "#define GPU_FRAGMENT_SHADER\n"
        "#define IN_OUT in\n";
    source[num_source++] = standard_extensions;
    source[num_source++] = standard_defines;

    if (geocode) {
      source[num_source++] = "#define USE_GEOMETRY_SHADER\n";
    }
    if (defines) {
      source[num_source++] = defines;
    }
    if (libcode) {
      source[num_source++] = libcode;
    }
    source[num_source++] = fragcode;

    gpu_dump_shaders(source, num_source, DEBUG_SHADER_FRAGMENT);

    glAttachShader(shader->program, shader->fragment);
    glShaderSource(shader->fragment, num_source, source, NULL);

    glCompileShader(shader->fragment);
    glGetShaderiv(shader->fragment, GL_COMPILE_STATUS, &status);

    if (!status) {
      glGetShaderInfoLog(shader->fragment, sizeof(log), &length, log);
      shader_print_errors("compile", log, source, num_source);

      GPU_shader_free(shader);
      return NULL;
    }
  }

  if (geocode) {
    const char *source[6];
    int num_source = 0;

    source[num_source++] = gpu_shader_version();
    source[num_source++] = "#define GPU_GEOMETRY_SHADER\n";
    source[num_source++] = standard_extensions;
    source[num_source++] = standard_defines;

    if (defines) {
      source[num_source++] = defines;
    }
    source[num_source++] = geocode;

    gpu_dump_shaders(source, num_source, DEBUG_SHADER_GEOMETRY);

    glAttachShader(shader->program, shader->geometry);
    glShaderSource(shader->geometry, num_source, source, NULL);

    glCompileShader(shader->geometry);
    glGetShaderiv(shader->geometry, GL_COMPILE_STATUS, &status);

    if (!status) {
      glGetShaderInfoLog(shader->geometry, sizeof(log), &length, log);
      shader_print_errors("compile", log, source, num_source);

      GPU_shader_free(shader);
      return NULL;
    }
  }

  if (tf_names != NULL) {
    glTransformFeedbackVaryings(shader->program, tf_count, tf_names, GL_INTERLEAVED_ATTRIBS);
    /* Primitive type must be setup */
    BLI_assert(tf_type != GPU_SHADER_TFB_NONE);
    shader->feedback_transform_type = tf_type;
  }

  glLinkProgram(shader->program);
  glGetProgramiv(shader->program, GL_LINK_STATUS, &status);
  if (!status) {
    glGetProgramInfoLog(shader->program, sizeof(log), &length, log);
    /* print attached shaders in pipeline order */
    if (defines) {
      shader_print_errors("linking", log, &defines, 1);
    }
    if (vertexcode) {
      shader_print_errors("linking", log, &vertexcode, 1);
    }
    if (geocode) {
      shader_print_errors("linking", log, &geocode, 1);
    }
    if (libcode) {
      shader_print_errors("linking", log, &libcode, 1);
    }
    if (fragcode) {
      shader_print_errors("linking", log, &fragcode, 1);
    }

    GPU_shader_free(shader);
    return NULL;
  }

  glUseProgram(shader->program);
  shader->interface = GPU_shaderinterface_create(shader->program);

  return shader;
}

#undef DEBUG_SHADER_GEOMETRY
#undef DEBUG_SHADER_FRAGMENT
#undef DEBUG_SHADER_VERTEX
#undef DEBUG_SHADER_NONE

void GPU_shader_free(GPUShader *shader)
{
#if 0 /* Would be nice to have, but for now the Deferred compilation \
       * does not have a GPUContext. */
  BLI_assert(GPU_context_active_get() != NULL);
#endif
  BLI_assert(shader);

  if (shader->vertex) {
    glDeleteShader(shader->vertex);
  }
  if (shader->geometry) {
    glDeleteShader(shader->geometry);
  }
  if (shader->fragment) {
    glDeleteShader(shader->fragment);
  }
  if (shader->program) {
    glDeleteProgram(shader->program);
  }

  if (shader->interface) {
    GPU_shaderinterface_discard(shader->interface);
  }

  MEM_freeN(shader);
}

static const char *string_join_array_maybe_alloc(const char **str_arr, bool *r_is_alloc)
{
  bool is_alloc = false;
  if (str_arr == NULL) {
    *r_is_alloc = false;
    return NULL;
  }
  /* Skip empty strings (avoid alloc if we can). */
  while (str_arr[0] && str_arr[0][0] == '\0') {
    str_arr++;
  }
  int i;
  for (i = 0; str_arr[i]; i++) {
    if (i != 0 && str_arr[i][0] != '\0') {
      is_alloc = true;
    }
  }
  *r_is_alloc = is_alloc;
  if (is_alloc) {
    return BLI_string_join_arrayN(str_arr, i);
  }

  return str_arr[0];
}

/**
 * Use via #GPU_shader_create_from_arrays macro (avoids passing in param).
 *
 * Similar to #DRW_shader_create_with_lib with the ability to include libs for each type of shader.
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
    const struct GPU_ShaderCreateFromArray_Params *params)
{
  struct {
    const char *str;
    bool is_alloc;
  } str_dst[4] = {{0}};
  const char **str_src[4] = {params->vert, params->frag, params->geom, params->defs};

  for (int i = 0; i < ARRAY_SIZE(str_src); i++) {
    str_dst[i].str = string_join_array_maybe_alloc(str_src[i], &str_dst[i].is_alloc);
  }

  GPUShader *sh = GPU_shader_create(
      str_dst[0].str, str_dst[1].str, str_dst[2].str, NULL, str_dst[3].str, __func__);

  for (int i = 0; i < ARRAY_SIZE(str_dst); i++) {
    if (str_dst[i].is_alloc) {
      MEM_freeN((void *)str_dst[i].str);
    }
  }
  return sh;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Binding
 * \{ */

void GPU_shader_bind(GPUShader *shader)
{
  BLI_assert(shader && shader->program);

  glUseProgram(shader->program);
  GPU_matrix_bind(shader->interface);
  GPU_shader_set_srgb_uniform(shader->interface);
}

void GPU_shader_unbind(void)
{
  glUseProgram(0);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform feedback
 * \{ */

bool GPU_shader_transform_feedback_enable(GPUShader *shader, uint vbo_id)
{
  if (shader->feedback_transform_type == GPU_SHADER_TFB_NONE) {
    return false;
  }

  glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, vbo_id);

  switch (shader->feedback_transform_type) {
    case GPU_SHADER_TFB_POINTS:
      glBeginTransformFeedback(GL_POINTS);
      return true;
    case GPU_SHADER_TFB_LINES:
      glBeginTransformFeedback(GL_LINES);
      return true;
    case GPU_SHADER_TFB_TRIANGLES:
      glBeginTransformFeedback(GL_TRIANGLES);
      return true;
    default:
      return false;
  }
}

void GPU_shader_transform_feedback_disable(GPUShader *UNUSED(shader))
{
  glEndTransformFeedback();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Uniforms / Resource location
 * \{ */

int GPU_shader_get_uniform(GPUShader *shader, const char *name)
{
  BLI_assert(shader && shader->program);
  const GPUShaderInput *uniform = GPU_shaderinterface_uniform(shader->interface, name);
  return uniform ? uniform->location : -1;
}

int GPU_shader_get_builtin_uniform(GPUShader *shader, int builtin)
{
  BLI_assert(shader && shader->program);
  return GPU_shaderinterface_uniform_builtin(shader->interface,
                                             static_cast<GPUUniformBuiltin>(builtin));
}

int GPU_shader_get_builtin_block(GPUShader *shader, int builtin)
{
  BLI_assert(shader && shader->program);
  return GPU_shaderinterface_block_builtin(shader->interface,
                                           static_cast<GPUUniformBlockBuiltin>(builtin));
}

int GPU_shader_get_uniform_block(GPUShader *shader, const char *name)
{
  BLI_assert(shader && shader->program);
  const GPUShaderInput *ubo = GPU_shaderinterface_ubo(shader->interface, name);
  return ubo ? ubo->location : -1;
}

int GPU_shader_get_uniform_block_binding(GPUShader *shader, const char *name)
{
  BLI_assert(shader && shader->program);
  const GPUShaderInput *ubo = GPU_shaderinterface_ubo(shader->interface, name);
  return ubo ? ubo->binding : -1;
}

int GPU_shader_get_texture_binding(GPUShader *shader, const char *name)
{
  BLI_assert(shader && shader->program);
  const GPUShaderInput *tex = GPU_shaderinterface_uniform(shader->interface, name);
  return tex ? tex->binding : -1;
}

int GPU_shader_get_attribute(GPUShader *shader, const char *name)
{
  BLI_assert(shader && shader->program);
  const GPUShaderInput *attr = GPU_shaderinterface_attr(shader->interface, name);
  return attr ? attr->location : -1;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Getters
 * \{ */

/* Clement : Temp */
int GPU_shader_get_program(GPUShader *shader)
{
  return (int)shader->program;
}

char *GPU_shader_get_binary(GPUShader *shader, uint *r_binary_format, int *r_binary_len)
{
  BLI_assert(GLEW_ARB_get_program_binary);
  char *r_binary;
  int binary_len = 0;

  glGetProgramiv(shader->program, GL_PROGRAM_BINARY_LENGTH, &binary_len);
  r_binary = (char *)MEM_mallocN(binary_len, __func__);
  glGetProgramBinary(shader->program, binary_len, NULL, r_binary_format, r_binary);

  if (r_binary_len) {
    *r_binary_len = binary_len;
  }

  return r_binary;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Uniforms setters
 * \{ */

void GPU_shader_uniform_float(GPUShader *UNUSED(shader), int location, float value)
{
  if (location == -1) {
    return;
  }

  glUniform1f(location, value);
}

void GPU_shader_uniform_vector(
    GPUShader *UNUSED(shader), int location, int length, int arraysize, const float *value)
{
  if (location == -1 || value == NULL) {
    return;
  }

  switch (length) {
    case 1:
      glUniform1fv(location, arraysize, value);
      break;
    case 2:
      glUniform2fv(location, arraysize, value);
      break;
    case 3:
      glUniform3fv(location, arraysize, value);
      break;
    case 4:
      glUniform4fv(location, arraysize, value);
      break;
    case 9:
      glUniformMatrix3fv(location, arraysize, 0, value);
      break;
    case 16:
      glUniformMatrix4fv(location, arraysize, 0, value);
      break;
    default:
      BLI_assert(0);
      break;
  }
}

void GPU_shader_uniform_int(GPUShader *UNUSED(shader), int location, int value)
{
  if (location == -1) {
    return;
  }

  glUniform1i(location, value);
}

void GPU_shader_uniform_vector_int(
    GPUShader *UNUSED(shader), int location, int length, int arraysize, const int *value)
{
  if (location == -1) {
    return;
  }

  switch (length) {
    case 1:
      glUniform1iv(location, arraysize, value);
      break;
    case 2:
      glUniform2iv(location, arraysize, value);
      break;
    case 3:
      glUniform3iv(location, arraysize, value);
      break;
    case 4:
      glUniform4iv(location, arraysize, value);
      break;
    default:
      BLI_assert(0);
      break;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name sRGB Rendering Workaround
 *
 * The viewport overlay frame-buffer is sRGB and will expect shaders to output display referred
 * Linear colors. But other frame-buffers (i.e: the area frame-buffers) are not sRGB and require
 * the shader output color to be in sRGB space
 * (assumed display encoded color-space as the time of writing).
 * For this reason we have a uniform to switch the transform on and off depending on the current
 * frame-buffer color-space.
 * \{ */

static int g_shader_builtin_srgb_transform = 0;

void GPU_shader_set_srgb_uniform(const GPUShaderInterface *interface)
{
  int32_t loc = GPU_shaderinterface_uniform_builtin(interface, GPU_UNIFORM_SRGB_TRANSFORM);
  if (loc != -1) {
    glUniform1i(loc, g_shader_builtin_srgb_transform);
  }
}

void GPU_shader_set_framebuffer_srgb_target(int use_srgb_to_linear)
{
  g_shader_builtin_srgb_transform = use_srgb_to_linear;
}

/** \} */
