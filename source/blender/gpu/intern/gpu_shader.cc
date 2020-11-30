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

#include "BLI_dynstr.h"
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

#include "GPU_capabilities.h"
#include "GPU_matrix.h"
#include "GPU_platform.h"
#include "GPU_shader.h"
#include "GPU_texture.h"
#include "GPU_uniform_buffer.h"

#include "gpu_backend.hh"
#include "gpu_context_private.hh"
#include "gpu_shader_private.hh"

#include "CLG_log.h"

extern "C" char datatoc_gpu_shader_colorspace_lib_glsl[];

static CLG_LogRef LOG = {"gpu.shader"};

using namespace blender;
using namespace blender::gpu;

/* -------------------------------------------------------------------- */
/** \name Debug functions
 * \{ */

void Shader::print_log(Span<const char *> sources, char *log, const char *stage, const bool error)
{
  const char line_prefix[] = "      | ";
  char err_col[] = "\033[31;1m";
  char warn_col[] = "\033[33;1m";
  char info_col[] = "\033[0;2m";
  char reset_col[] = "\033[0;0m";
  char *sources_combined = BLI_string_join_arrayN((const char **)sources.data(), sources.size());
  DynStr *dynstr = BLI_dynstr_new();

  if (!CLG_color_support_get(&LOG)) {
    err_col[0] = warn_col[0] = info_col[0] = reset_col[0] = '\0';
  }

  BLI_dynstr_appendf(dynstr, "\n");

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
    /* 0 = error, 1 = warning. */
    int type = -1;
    /* Skip ERROR: or WARNING:. */
    const char *prefix[] = {"ERROR", "WARNING"};
    for (int i = 0; i < ARRAY_SIZE(prefix); i++) {
      if (STREQLEN(log_line, prefix[i], strlen(prefix[i]))) {
        log_line += strlen(prefix[i]);
        type = i;
        break;
      }
    }
    /* Skip whitespaces and separators. */
    while (ELEM(log_line[0], ':', '(', ' ')) {
      log_line++;
    }
    /* Parse error line & char numbers. */
    error_line = error_char = -1;
    if (log_line[0] >= '0' && log_line[0] <= '9') {
      error_line = (int)strtol(log_line, &error_line_number_end, 10);
      /* Try to fetch the error caracter (not always available). */
      if (ELEM(error_line_number_end[0], '(', ':') && error_line_number_end[1] != ' ') {
        error_char = (int)strtol(error_line_number_end + 1, &log_line, 10);
      }
      else {
        log_line = error_line_number_end;
      }
      /* There can be a 3rd number (case of mesa driver). */
      if (ELEM(log_line[0], '(', ':') && log_line[1] >= '0' && log_line[1] <= '9') {
        error_line = error_char;
        error_char = (int)strtol(log_line + 1, &error_line_number_end, 10);
        log_line = error_line_number_end;
      }
    }
    /* Skip whitespaces and separators. */
    while (ELEM(log_line[0], ':', ')', ' ')) {
      log_line++;
    }
    if (error_line == -1) {
      found_line_id = false;
    }
    const char *src_line = sources_combined;
    if ((error_line != -1) && (error_char != -1)) {
      if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_UNIX, GPU_DRIVER_OFFICIAL)) {
        /* source:line */
        int error_source = error_line;
        if (error_source < sources.size()) {
          src_line = sources[error_source];
          error_line = error_char;
          error_char = -1;
        }
      }
      else if (GPU_type_matches(GPU_DEVICE_NVIDIA, GPU_OS_ANY, GPU_DRIVER_OFFICIAL) ||
               GPU_type_matches(GPU_DEVICE_INTEL, GPU_OS_MAC, GPU_DRIVER_OFFICIAL)) {
        /* 0:line */
        error_line = error_char;
        error_char = -1;
      }
      else {
        /* line:char */
      }
    }
    /* Separate from previous block. */
    if (last_error_line != error_line) {
      BLI_dynstr_appendf(dynstr, "%s%s%s\n", info_col, line_prefix, reset_col);
    }
    else if (error_char != last_error_char) {
      BLI_dynstr_appendf(dynstr, "%s\n", line_prefix);
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
        if (error_line != last_error_line) {
          BLI_dynstr_appendf(dynstr, "%5d | ", src_line_index);
        }
        else {
          BLI_dynstr_appendf(dynstr, line_prefix);
        }
        BLI_dynstr_nappend(dynstr, src_line, (src_line_end + 1) - src_line);
        /* Print char offset. */
        BLI_dynstr_appendf(dynstr, line_prefix);
        if (error_char != -1) {
          for (int i = 0; i < error_char; i++) {
            BLI_dynstr_appendf(dynstr, " ");
          }
          BLI_dynstr_appendf(dynstr, "^");
        }
        BLI_dynstr_appendf(dynstr, "\n");
      }
    }
    BLI_dynstr_appendf(dynstr, line_prefix);
    /* Skip to message. Avoid redundant info. */
    const char *keywords[] = {"error", "warning"};
    for (int i = 0; i < ARRAY_SIZE(prefix); i++) {
      if (STREQLEN(log_line, keywords[i], strlen(keywords[i]))) {
        log_line += strlen(keywords[i]);
        type = i;
        break;
      }
    }
    /* Skip and separators. */
    while (ELEM(log_line[0], ':', ')')) {
      log_line++;
    }
    if (type == 0) {
      BLI_dynstr_appendf(dynstr, "%s%s%s: ", err_col, "Error", info_col);
    }
    else if (type == 1) {
      BLI_dynstr_appendf(dynstr, "%s%s%s: ", warn_col, "Warning", info_col);
    }
    /* Print the error itself. */
    BLI_dynstr_append(dynstr, info_col);
    BLI_dynstr_nappend(dynstr, log_line, (line_end + 1) - log_line);
    BLI_dynstr_append(dynstr, reset_col);
    /* Continue to next line. */
    log_line = line_end + 1;
    last_error_line = error_line;
    last_error_char = error_char;
  }
  MEM_freeN(sources_combined);

  CLG_Severity severity = error ? CLG_SEVERITY_ERROR : CLG_SEVERITY_WARN;

  if (((LOG.type->flag & CLG_FLAG_USE) && (LOG.type->level >= 0)) ||
      (severity >= CLG_SEVERITY_WARN)) {
    const char *_str = BLI_dynstr_get_cstring(dynstr);
    CLG_log_str(LOG.type, severity, this->name, stage, _str);
    MEM_freeN((void *)_str);
  }

  BLI_dynstr_free(dynstr);
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
  delete interface;
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
  BLI_assert((fragcode != nullptr) && (vertcode != nullptr));

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

  if (tf_names != nullptr && tf_count > 0) {
    BLI_assert(tf_type != GPU_SHADER_TFB_NONE);
    shader->transform_feedback_names_set(Span<const char *>(tf_names, tf_count), tf_type);
  }

  if (!shader->finalize()) {
    delete shader;
    return nullptr;
  };

  return wrap(shader);
}

void GPU_shader_free(GPUShader *shader)
{
  delete unwrap(shader);
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
      vertcode, fragcode, geomcode, libcode, defines, GPU_SHADER_TFB_NONE, nullptr, 0, shname);
}

GPUShader *GPU_shader_create_from_python(const char *vertcode,
                                         const char *fragcode,
                                         const char *geomcode,
                                         const char *libcode,
                                         const char *defines)
{
  char *libcodecat = nullptr;

  if (libcode == nullptr) {
    libcode = datatoc_gpu_shader_colorspace_lib_glsl;
  }
  else {
    libcode = libcodecat = BLI_strdupcat(libcode, datatoc_gpu_shader_colorspace_lib_glsl);
  }

  GPUShader *sh = GPU_shader_create_ex(vertcode,
                                       fragcode,
                                       geomcode,
                                       libcode,
                                       defines,
                                       GPU_SHADER_TFB_NONE,
                                       nullptr,
                                       0,
                                       "pyGPUShader");

  MEM_SAFE_FREE(libcodecat);
  return sh;
}

static const char *string_join_array_maybe_alloc(const char **str_arr, bool *r_is_alloc)
{
  bool is_alloc = false;
  if (str_arr == nullptr) {
    *r_is_alloc = false;
    return nullptr;
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
  } str_dst[4] = {{nullptr}};
  const char **str_src[4] = {params->vert, params->frag, params->geom, params->defs};

  for (int i = 0; i < ARRAY_SIZE(str_src); i++) {
    str_dst[i].str = string_join_array_maybe_alloc(str_src[i], &str_dst[i].is_alloc);
  }

  char name[64];
  BLI_snprintf(name, sizeof(name), "%s_%d", func, line);

  GPUShader *sh = GPU_shader_create(
      str_dst[0].str, str_dst[1].str, str_dst[2].str, nullptr, str_dst[3].str, name);

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
  Shader *shader = unwrap(gpu_shader);

  Context *ctx = Context::get();

  if (ctx->shader != shader) {
    ctx->shader = shader;
    shader->bind();
    GPU_matrix_bind(gpu_shader);
    GPU_shader_set_srgb_uniform(gpu_shader);
  }

  if (GPU_matrix_dirty_get()) {
    GPU_matrix_bind(gpu_shader);
  }
}

void GPU_shader_unbind(void)
{
#ifndef NDEBUG
  Context *ctx = Context::get();
  if (ctx->shader) {
    ctx->shader->unbind();
  }
  ctx->shader = nullptr;
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform feedback
 *
 * TODO(fclem): Should be replaced by compute shaders.
 * \{ */

bool GPU_shader_transform_feedback_enable(GPUShader *shader, GPUVertBuf *vertbuf)
{
  return unwrap(shader)->transform_feedback_enable(vertbuf);
}

void GPU_shader_transform_feedback_disable(GPUShader *shader)
{
  unwrap(shader)->transform_feedback_disable();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Uniforms / Resource location
 * \{ */

int GPU_shader_get_uniform(GPUShader *shader, const char *name)
{
  ShaderInterface *interface = unwrap(shader)->interface;
  const ShaderInput *uniform = interface->uniform_get(name);
  return uniform ? uniform->location : -1;
}

int GPU_shader_get_builtin_uniform(GPUShader *shader, int builtin)
{
  ShaderInterface *interface = unwrap(shader)->interface;
  return interface->uniform_builtin((GPUUniformBuiltin)builtin);
}

int GPU_shader_get_builtin_block(GPUShader *shader, int builtin)
{
  ShaderInterface *interface = unwrap(shader)->interface;
  return interface->ubo_builtin((GPUUniformBlockBuiltin)builtin);
}

/* DEPRECATED. */
int GPU_shader_get_uniform_block(GPUShader *shader, const char *name)
{
  ShaderInterface *interface = unwrap(shader)->interface;
  const ShaderInput *ubo = interface->ubo_get(name);
  return ubo ? ubo->location : -1;
}

int GPU_shader_get_uniform_block_binding(GPUShader *shader, const char *name)
{
  ShaderInterface *interface = unwrap(shader)->interface;
  const ShaderInput *ubo = interface->ubo_get(name);
  return ubo ? ubo->binding : -1;
}

int GPU_shader_get_texture_binding(GPUShader *shader, const char *name)
{
  ShaderInterface *interface = unwrap(shader)->interface;
  const ShaderInput *tex = interface->uniform_get(name);
  return tex ? tex->binding : -1;
}

int GPU_shader_get_attribute(GPUShader *shader, const char *name)
{
  ShaderInterface *interface = unwrap(shader)->interface;
  const ShaderInput *attr = interface->attr_get(name);
  return attr ? attr->location : -1;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Getters
 * \{ */

/* DEPRECATED: Kept only because of BGL API */
int GPU_shader_get_program(GPUShader *shader)
{
  return unwrap(shader)->program_handle_get();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Uniforms setters
 * \{ */

void GPU_shader_uniform_vector(
    GPUShader *shader, int loc, int len, int arraysize, const float *value)
{
  unwrap(shader)->uniform_float(loc, len, arraysize, value);
}

void GPU_shader_uniform_vector_int(
    GPUShader *shader, int loc, int len, int arraysize, const int *value)
{
  unwrap(shader)->uniform_int(loc, len, arraysize, value);
}

void GPU_shader_uniform_int(GPUShader *shader, int location, int value)
{
  GPU_shader_uniform_vector_int(shader, location, 1, 1, &value);
}

void GPU_shader_uniform_float(GPUShader *shader, int location, float value)
{
  GPU_shader_uniform_vector(shader, location, 1, 1, &value);
}

void GPU_shader_uniform_1i(GPUShader *sh, const char *name, int value)
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_int(sh, loc, value);
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

void GPU_shader_uniform_1f(GPUShader *sh, const char *name, float value)
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_float(sh, loc, value);
}

void GPU_shader_uniform_2fv(GPUShader *sh, const char *name, const float data[2])
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_vector(sh, loc, 2, 1, data);
}

void GPU_shader_uniform_3fv(GPUShader *sh, const char *name, const float data[3])
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_vector(sh, loc, 3, 1, data);
}

void GPU_shader_uniform_4fv(GPUShader *sh, const char *name, const float data[4])
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_vector(sh, loc, 4, 1, data);
}

void GPU_shader_uniform_mat4(GPUShader *sh, const char *name, const float data[4][4])
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_vector(sh, loc, 16, 1, (const float *)data);
}

void GPU_shader_uniform_2fv_array(GPUShader *sh, const char *name, int len, const float (*val)[2])
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_vector(sh, loc, 2, len, (const float *)val);
}

void GPU_shader_uniform_4fv_array(GPUShader *sh, const char *name, int len, const float (*val)[4])
{
  const int loc = GPU_shader_get_uniform(sh, name);
  GPU_shader_uniform_vector(sh, loc, 4, len, (const float *)val);
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
  int32_t loc = GPU_shader_get_builtin_uniform(shader, GPU_UNIFORM_SRGB_TRANSFORM);
  if (loc != -1) {
    GPU_shader_uniform_vector_int(shader, loc, 1, 1, &g_shader_builtin_srgb_transform);
  }
}

void GPU_shader_set_framebuffer_srgb_target(int use_srgb_to_linear)
{
  g_shader_builtin_srgb_transform = use_srgb_to_linear;
}

/** \} */
