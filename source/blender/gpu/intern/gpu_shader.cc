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
#include "BLI_vector.hh"

#include "BKE_appdir.h"
#include "BKE_global.h"

#include "DNA_space_types.h"

#include "GPU_extensions.h"
#include "GPU_matrix.h"
#include "GPU_platform.h"
#include "GPU_shader.h"
#include "GPU_texture.h"
#include "GPU_uniformbuffer.h"

#include "gpu_backend.hh"
#include "gpu_context_private.hh"
#include "gpu_shader_private.hh"

extern "C" char datatoc_gpu_shader_colorspace_lib_glsl[];

using namespace blender;
using namespace blender::gpu;

/* -------------------------------------------------------------------- */
/** \name Debug functions
 * \{ */

void Shader::print_errors(Span<const char *> sources, char *log)
{
  const bool pretty = true;
  const char line_prefix[] = "      | ";
  char *sources_combined = BLI_string_join_arrayN((const char **)sources.data(), sources.size());

  if (pretty) {
    fprintf(stderr, "\n      \033[1mShader Compilation Log : \033[0m%s\n", this->name);
  }
  else {
    fprintf(stderr, "\n      Shader Compilation Log : %s\n", this->name);
  }

  char *log_line = log, *line_end;
  char *error_line_number_end;
  int error_line, error_char, last_error_line = -2, last_error_char = -1;
  bool found_line_id = false;
  while ((line_end = strchr(log_line, '\n'))) {
    /* Skip empty lines. */
    if (line_end == log_line) {
      log_line++;
      continue;
    }
    /* Skip ERROR: or WARNING:. */
    const char *prefix[] = {"ERROR", "WARNING"};
    for (int i = 0; i < ARRAY_SIZE(prefix); i++) {
      if (STREQLEN(log_line, prefix[i], strlen(prefix[i]))) {
        log_line += strlen(prefix[i]);
        break;
      }
    }
    error_line = error_char = -1;
    if (ELEM(log_line[0], '0', ':') && ELEM(log_line[1], ':', '(', ' ')) {
      error_line = (int)strtol(log_line + 2, &error_line_number_end, 10);
      /* Try to fetch the error caracter (not always available). */
      if (ELEM(error_line_number_end[0], '(', ':')) {
        error_char = (int)strtol(error_line_number_end + 1, NULL, 10);
      }
    }
    if ((error_line == -1)) {
      found_line_id = false;
    }
    const char *src_line = sources_combined;
    /* Some drivers use (source:line) instead of (line:char) */
    if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_UNIX, GPU_DRIVER_OFFICIAL) && (error_line != -1) &&
        (error_char != -1)) {
      int error_source = error_line;
      if (error_source < sources.size()) {
        src_line = sources[error_source];
        error_line = error_char;
        error_char = -1;
      }
    }
    /* Separate from previous block. */
    if (last_error_line != error_line) {
      fprintf(stderr, "\n");
    }
    /* Print line from the source file that is producing the error. */
    if ((error_line != -1) && (error_line != last_error_line || error_char != last_error_char)) {
      const char *src_line_end = src_line;
      found_line_id = false;
      /* error_line is 1 based in this case. */
      int src_line_index = 1;
      while ((src_line_end = strchr(src_line, '\n'))) {
        if (src_line_index == error_line) {
          found_line_id = true;
          break;
        }
        /* Continue to next line. */
        src_line = src_line_end + 1;
        src_line_index++;
      }
      /* Print error source. */
      if (found_line_id) {
        fprintf(stderr, "%5d | ", src_line_index);
        fwrite(src_line, (src_line_end + 1) - src_line, 1, stderr);
        /* Print char offset. */
        fprintf(stderr, line_prefix);
        if (error_char != -1) {
          for (int i = 0; i < error_char; i++) {
            fprintf(stderr, " ");
          }
          fprintf(stderr, "^");
        }
        fprintf(stderr, "\n");
      }
    }
    fprintf(stderr, line_prefix);
    if (found_line_id) {
      /* Skip to message. Avoid redundant info. */
      const char *keywords[] = {"error", "warning"};
      for (int i = 0; i < ARRAY_SIZE(keywords); i++) {
        /* Avoid searching following lines. */
        line_end[0] = '\0';
        if (strstr(log_line, keywords[i])) {
          log_line = strstr(log_line, keywords[i]);
          if (pretty) {
            if (STREQ(keywords[i], "error")) {
              fprintf(stderr, "\033[31;1mError\033[0m ");
            }
            else if (STREQ(keywords[i], "warning")) {
              fprintf(stderr, "\033[33;1mWarning\033[0m ");
            }
            log_line += strlen(keywords[i]);
          }
          break;
        }
      }
      line_end[0] = '\n';
    }
    /* Print the error itself. */
    if (pretty) {
      fprintf(stderr, "\033[2m");
    }
    fwrite(log_line, (line_end + 1) - log_line, 1, stderr);
    if (pretty) {
      fprintf(stderr, "\033[0m");
    }
    /* Continue to next line. */
    log_line = line_end + 1;
    last_error_line = error_line;
    last_error_char = error_char;
  }
  fprintf(stderr, "\n\n");
  MEM_freeN(sources_combined);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Creation / Destruction
 * \{ */

Shader::Shader(const char *sh_name)
{
  BLI_strncpy(this->name, sh_name, sizeof(this->name));
}

Shader::~Shader()
{
  if (this->interface) {
    GPU_shaderinterface_discard(this->interface);
  }
}

static void standard_defines(Vector<const char *> &sources)
{
  BLI_assert(sources.size() == 0);
  /* Version needs to be first. Exact values will be added by implementation. */
  sources.append("version");
  /* some useful defines to detect GPU type */
  if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_ANY, GPU_DRIVER_ANY)) {
    sources.append("#define GPU_ATI\n");
  }
  else if (GPU_type_matches(GPU_DEVICE_NVIDIA, GPU_OS_ANY, GPU_DRIVER_ANY)) {
    sources.append("#define GPU_NVIDIA\n");
  }
  else if (GPU_type_matches(GPU_DEVICE_INTEL, GPU_OS_ANY, GPU_DRIVER_ANY)) {
    sources.append("#define GPU_INTEL\n");
  }
  /* some useful defines to detect OS type */
  if (GPU_type_matches(GPU_DEVICE_ANY, GPU_OS_WIN, GPU_DRIVER_ANY)) {
    sources.append("#define OS_WIN\n");
  }
  else if (GPU_type_matches(GPU_DEVICE_ANY, GPU_OS_MAC, GPU_DRIVER_ANY)) {
    sources.append("#define OS_MAC\n");
  }
  else if (GPU_type_matches(GPU_DEVICE_ANY, GPU_OS_UNIX, GPU_DRIVER_ANY)) {
    sources.append("#define OS_UNIX\n");
  }

  if (GPU_crappy_amd_driver()) {
    sources.append("#define GPU_DEPRECATED_AMD_DRIVER\n");
  }
}

GPUShader *GPU_shader_create_ex(const char *vertcode,
                                const char *fragcode,
                                const char *geomcode,
                                const char *libcode,
                                const char *defines,
                                const eGPUShaderTFBType tf_type,
                                const char **tf_names,
                                const int tf_count,
                                const char *shname)
{
  /* At least a vertex shader and a fragment shader are required. */
  BLI_assert((fragcode != NULL) && (vertcode != NULL));

  Shader *shader = GPUBackend::get()->shader_alloc(shname);

  if (vertcode) {
    Vector<const char *> sources;
    standard_defines(sources);
    sources.append("#define GPU_VERTEX_SHADER\n");
    sources.append("#define IN_OUT out\n");
    if (geomcode) {
      sources.append("#define USE_GEOMETRY_SHADER\n");
    }
    if (defines) {
      sources.append(defines);
    }
    sources.append(vertcode);

    shader->vertex_shader_from_glsl(sources);
  }

  if (fragcode) {
    Vector<const char *> sources;
    standard_defines(sources);
    sources.append("#define GPU_FRAGMENT_SHADER\n");
    sources.append("#define IN_OUT in\n");
    if (geomcode) {
      sources.append("#define USE_GEOMETRY_SHADER\n");
    }
    if (defines) {
      sources.append(defines);
    }
    if (libcode) {
      sources.append(libcode);
    }
    sources.append(fragcode);

    shader->fragment_shader_from_glsl(sources);
  }

  if (geomcode) {
    Vector<const char *> sources;
    standard_defines(sources);
    sources.append("#define GPU_GEOMETRY_SHADER\n");
    if (defines) {
      sources.append(defines);
    }
    sources.append(geomcode);

    shader->geometry_shader_from_glsl(sources);
  }

  if (tf_names != NULL && tf_count > 0) {
    BLI_assert(tf_type != GPU_SHADER_TFB_NONE);
    shader->transform_feedback_names_set(Span<const char *>(tf_names, tf_count), tf_type);
  }

  if (!shader->finalize()) {
    delete shader;
    return NULL;
  };

  return static_cast<GPUShader *>(shader);
}

void GPU_shader_free(GPUShader *shader)
{
  delete static_cast<Shader *>(shader);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Creation utils
 * \{ */

GPUShader *GPU_shader_create(const char *vertcode,
                             const char *fragcode,
                             const char *geomcode,
                             const char *libcode,
                             const char *defines,
                             const char *shname)
{
  return GPU_shader_create_ex(
      vertcode, fragcode, geomcode, libcode, defines, GPU_SHADER_TFB_NONE, NULL, 0, shname);
}

GPUShader *GPU_shader_create_from_python(const char *vertcode,
                                         const char *fragcode,
                                         const char *geomcode,
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
      vertcode, fragcode, geomcode, libcode, defines, GPU_SHADER_TFB_NONE, NULL, 0, NULL);

  MEM_SAFE_FREE(libcodecat);
  return sh;
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
    const struct GPU_ShaderCreateFromArray_Params *params, const char *func, int line)
{
  struct {
    const char *str;
    bool is_alloc;
  } str_dst[4] = {{0}};
  const char **str_src[4] = {params->vert, params->frag, params->geom, params->defs};

  for (int i = 0; i < ARRAY_SIZE(str_src); i++) {
    str_dst[i].str = string_join_array_maybe_alloc(str_src[i], &str_dst[i].is_alloc);
  }

  char name[64];
  BLI_snprintf(name, sizeof(name), "%s_%d", func, line);

  GPUShader *sh = GPU_shader_create(
      str_dst[0].str, str_dst[1].str, str_dst[2].str, NULL, str_dst[3].str, name);

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

void GPU_shader_bind(GPUShader *gpu_shader)
{
  Shader *shader = static_cast<Shader *>(gpu_shader);

  GPUContext *ctx = GPU_context_active_get();

  if (ctx->shader != shader) {
    ctx->shader = shader;
    shader->bind();
    GPU_matrix_bind(shader);
    GPU_shader_set_srgb_uniform(shader);
  }

  if (GPU_matrix_dirty_get()) {
    GPU_matrix_bind(shader);
  }
}

void GPU_shader_unbind(void)
{
#ifndef NDEBUG
  GPUContext *ctx = GPU_context_active_get();
  if (ctx->shader) {
    static_cast<Shader *>(ctx->shader)->unbind();
  }
  ctx->shader = NULL;
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform feedback
 *
 * TODO(fclem) Should be replaced by compute shaders.
 * \{ */

bool GPU_shader_transform_feedback_enable(GPUShader *shader, GPUVertBuf *vertbuf)
{
  return static_cast<Shader *>(shader)->transform_feedback_enable(vertbuf);
}

void GPU_shader_transform_feedback_disable(GPUShader *shader)
{
  static_cast<Shader *>(shader)->transform_feedback_disable();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Uniforms / Resource location
 * \{ */

int GPU_shader_get_uniform(GPUShader *shader, const char *name)
{
  const GPUShaderInput *uniform = GPU_shaderinterface_uniform(shader->interface, name);
  return uniform ? uniform->location : -1;
}

int GPU_shader_get_builtin_uniform(GPUShader *shader, int builtin)
{
  return GPU_shaderinterface_uniform_builtin(shader->interface,
                                             static_cast<GPUUniformBuiltin>(builtin));
}

int GPU_shader_get_builtin_block(GPUShader *shader, int builtin)
{
  return GPU_shaderinterface_block_builtin(shader->interface,
                                           static_cast<GPUUniformBlockBuiltin>(builtin));
}

int GPU_shader_get_uniform_block(GPUShader *shader, const char *name)
{
  const GPUShaderInput *ubo = GPU_shaderinterface_ubo(shader->interface, name);
  return ubo ? ubo->location : -1;
}

int GPU_shader_get_uniform_block_binding(GPUShader *shader, const char *name)
{
  const GPUShaderInput *ubo = GPU_shaderinterface_ubo(shader->interface, name);
  return ubo ? ubo->binding : -1;
}

int GPU_shader_get_texture_binding(GPUShader *shader, const char *name)
{
  const GPUShaderInput *tex = GPU_shaderinterface_uniform(shader->interface, name);
  return tex ? tex->binding : -1;
}

int GPU_shader_get_attribute(GPUShader *shader, const char *name)
{
  const GPUShaderInput *attr = GPU_shaderinterface_attr(shader->interface, name);
  return attr ? attr->location : -1;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Getters
 * \{ */

/* Clement : Temp */
int GPU_shader_get_program(GPUShader *UNUSED(shader))
{
  /* TODO fixme */
  return (int)0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Uniforms setters
 * \{ */

void GPU_shader_uniform_vector(
    GPUShader *shader, int loc, int len, int arraysize, const float *value)
{
  static_cast<Shader *>(shader)->uniform_float(loc, len, arraysize, value);
}

void GPU_shader_uniform_vector_int(
    GPUShader *shader, int loc, int len, int arraysize, const int *value)
{
  static_cast<Shader *>(shader)->uniform_int(loc, len, arraysize, value);
}

void GPU_shader_uniform_int(GPUShader *shader, int location, int value)
{
  GPU_shader_uniform_vector_int(shader, location, 1, 1, &value);
}

void GPU_shader_uniform_float(GPUShader *shader, int location, float value)
{
  GPU_shader_uniform_vector(shader, location, 1, 1, &value);
}

#define GET_UNIFORM \
  const GPUShaderInput *uniform = GPU_shaderinterface_uniform(sh->interface, name); \
  BLI_assert(uniform);

void GPU_shader_uniform_1i(GPUShader *sh, const char *name, int value)
{
  GET_UNIFORM
  GPU_shader_uniform_int(sh, uniform->location, value);
}

void GPU_shader_uniform_1b(GPUShader *sh, const char *name, bool value)
{
  GPU_shader_uniform_1i(sh, name, value ? 1 : 0);
}

void GPU_shader_uniform_2f(GPUShader *sh, const char *name, float x, float y)
{
  const float data[2] = {x, y};
  GPU_shader_uniform_2fv(sh, name, data);
}

void GPU_shader_uniform_3f(GPUShader *sh, const char *name, float x, float y, float z)
{
  const float data[3] = {x, y, z};
  GPU_shader_uniform_3fv(sh, name, data);
}

void GPU_shader_uniform_4f(GPUShader *sh, const char *name, float x, float y, float z, float w)
{
  const float data[4] = {x, y, z, w};
  GPU_shader_uniform_4fv(sh, name, data);
}

void GPU_shader_uniform_1f(GPUShader *sh, const char *name, float x)
{
  GET_UNIFORM
  GPU_shader_uniform_float(sh, uniform->location, x);
}

void GPU_shader_uniform_2fv(GPUShader *sh, const char *name, const float data[2])
{
  GET_UNIFORM
  GPU_shader_uniform_vector(sh, uniform->location, 2, 1, data);
}

void GPU_shader_uniform_3fv(GPUShader *sh, const char *name, const float data[3])
{
  GET_UNIFORM
  GPU_shader_uniform_vector(sh, uniform->location, 3, 1, data);
}

void GPU_shader_uniform_4fv(GPUShader *sh, const char *name, const float data[4])
{
  GET_UNIFORM
  GPU_shader_uniform_vector(sh, uniform->location, 4, 1, data);
}

void GPU_shader_uniform_mat4(GPUShader *sh, const char *name, const float data[4][4])
{
  GET_UNIFORM
  GPU_shader_uniform_vector(sh, uniform->location, 16, 1, (const float *)data);
}

void GPU_shader_uniform_2fv_array(GPUShader *sh, const char *name, int len, const float (*val)[2])
{
  GET_UNIFORM
  GPU_shader_uniform_vector(sh, uniform->location, 2, len, (const float *)val);
}

void GPU_shader_uniform_4fv_array(GPUShader *sh, const char *name, int len, const float (*val)[4])
{
  GET_UNIFORM
  GPU_shader_uniform_vector(sh, uniform->location, 4, len, (const float *)val);
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

void GPU_shader_set_srgb_uniform(GPUShader *shader)
{
  int32_t loc = GPU_shaderinterface_uniform_builtin(shader->interface, GPU_UNIFORM_SRGB_TRANSFORM);
  if (loc != -1) {
    GPU_shader_uniform_vector_int(shader, loc, 1, 1, &g_shader_builtin_srgb_transform);
  }
}

void GPU_shader_set_framebuffer_srgb_target(int use_srgb_to_linear)
{
  g_shader_builtin_srgb_transform = use_srgb_to_linear;
}

/** \} */
