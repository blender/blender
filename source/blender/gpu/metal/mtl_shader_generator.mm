/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BKE_global.hh"

#include "BLI_string.h"

#include "BLI_string.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <regex>
#include <sstream>
#include <string>

#include <cstring>

#include "GPU_platform.h"
#include "GPU_vertex_format.h"

#include "gpu_shader_dependency_private.h"

#include "mtl_common.hh"
#include "mtl_context.hh"
#include "mtl_debug.hh"
#include "mtl_shader.hh"
#include "mtl_shader_generator.hh"
#include "mtl_shader_interface.hh"
#include "mtl_texture.hh"

extern char datatoc_mtl_shader_defines_msl[];
extern char datatoc_mtl_shader_shared_h[];

using namespace blender;
using namespace blender::gpu;
using namespace blender::gpu::shader;

namespace blender::gpu {

char *MSLGeneratorInterface::msl_patch_default = nullptr;

/* Generator names. */
#define FRAGMENT_OUT_STRUCT_NAME "FragmentOut"
#define FRAGMENT_TILE_IN_STRUCT_NAME "FragmentTileIn"

#define ATOMIC_DEFINE_STR "#define MTL_SUPPORTS_TEXTURE_ATOMICS 1\n"

/* -------------------------------------------------------------------- */
/** \name Shader Translation utility functions.
 * \{ */

static eMTLDataType to_mtl_type(Type type)
{
  switch (type) {
    case Type::FLOAT:
      return MTL_DATATYPE_FLOAT;
    case Type::VEC2:
      return MTL_DATATYPE_FLOAT2;
    case Type::VEC3:
      return MTL_DATATYPE_FLOAT3;
    case Type::VEC4:
      return MTL_DATATYPE_FLOAT4;
    case Type::MAT3:
      return MTL_DATATYPE_FLOAT3x3;
    case Type::MAT4:
      return MTL_DATATYPE_FLOAT4x4;
    case Type::UINT:
      return MTL_DATATYPE_UINT;
    case Type::UVEC2:
      return MTL_DATATYPE_UINT2;
    case Type::UVEC3:
      return MTL_DATATYPE_UINT3;
    case Type::UVEC4:
      return MTL_DATATYPE_UINT4;
    case Type::INT:
      return MTL_DATATYPE_INT;
    case Type::IVEC2:
      return MTL_DATATYPE_INT2;
    case Type::IVEC3:
      return MTL_DATATYPE_INT3;
    case Type::IVEC4:
      return MTL_DATATYPE_INT4;
    case Type::VEC3_101010I2:
      return MTL_DATATYPE_INT1010102_NORM;
    case Type::BOOL:
      return MTL_DATATYPE_BOOL;
    case Type::UCHAR:
      return MTL_DATATYPE_UCHAR;
    case Type::UCHAR2:
      return MTL_DATATYPE_UCHAR2;
    case Type::UCHAR3:
      return MTL_DATATYPE_UCHAR3;
    case Type::UCHAR4:
      return MTL_DATATYPE_UCHAR4;
    case Type::CHAR:
      return MTL_DATATYPE_CHAR;
    case Type::CHAR2:
      return MTL_DATATYPE_CHAR2;
    case Type::CHAR3:
      return MTL_DATATYPE_CHAR3;
    case Type::CHAR4:
      return MTL_DATATYPE_CHAR4;
    case Type::USHORT:
      return MTL_DATATYPE_USHORT;
    case Type::USHORT2:
      return MTL_DATATYPE_USHORT2;
    case Type::USHORT3:
      return MTL_DATATYPE_USHORT3;
    case Type::USHORT4:
      return MTL_DATATYPE_USHORT4;
    case Type::SHORT:
      return MTL_DATATYPE_SHORT;
    case Type::SHORT2:
      return MTL_DATATYPE_SHORT2;
    case Type::SHORT3:
      return MTL_DATATYPE_SHORT3;
    case Type::SHORT4:
      return MTL_DATATYPE_SHORT4;
    default: {
      BLI_assert_msg(false, "Unexpected data type");
    }
  }
  return MTL_DATATYPE_FLOAT;
}

static std::regex remove_non_numeric_characters("[^0-9]");

static void remove_multiline_comments_func(std::string &str)
{
  char *current_str_begin = &*str.begin();
  char *current_str_end = &*str.end();

  bool is_inside_comment = false;
  for (char *c = current_str_begin; c < current_str_end; c++) {
    if (is_inside_comment) {
      if ((*c == '*') && (c < current_str_end - 1) && (*(c + 1) == '/')) {
        is_inside_comment = false;
        *c = ' ';
        *(c + 1) = ' ';
      }
      else {
        *c = ' ';
      }
    }
    else {
      if ((*c == '/') && (c < current_str_end - 1) && (*(c + 1) == '*')) {
        is_inside_comment = true;
        *c = ' ';
      }
    }
  }
}

static void remove_singleline_comments_func(std::string &str)
{
  char *current_str_begin = &*str.begin();
  char *current_str_end = &*str.end();

  bool is_inside_comment = false;
  for (char *c = current_str_begin; c < current_str_end; c++) {
    if (is_inside_comment) {
      if (*c == '\n') {
        is_inside_comment = false;
      }
      else {
        *c = ' ';
      }
    }
    else {
      if ((*c == '/') && (c < current_str_end - 1) && (*(c + 1) == '/')) {
        is_inside_comment = true;
        *c = ' ';
      }
    }
  }
}

static bool is_program_word(const char *chr, int *len)
{
  int numchars = 0;
  for (const char *c = chr; *c != '\0'; c++) {
    char ch = *c;
    /* Note: Hash (`#`) is not valid in var names, but is used by Closure macro patterns. */
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
        (numchars > 0 && ch >= '0' && ch <= '9') || ch == '_' || ch == '#')
    {
      numchars++;
    }
    else {
      *len = numchars;
      return (numchars > 0);
    }
  }
  *len = numchars;
  return true;
}

static int backwards_program_word_scan(const char *array_loc, const char *min)
{
  const char *start;
  char last_char = ' ';
  int numchars = 0;
  for (start = array_loc - 1; (start >= min) && (*start != '\0'); start--) {
    char ch = *start;
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
        ch == '_' || ch == '#')
    {
      numchars++;
      last_char = ch;
    }
    else {
      break;
    }
  }

  if (numchars > 0) {
    /* cannot start with numbers, so we need to invalidate the word. */
    if ((last_char >= '0' && last_char <= '9')) {
      numchars = 0;
    }
  }
  return numchars;
}

/* Extract clipping distance usage indices, and replace syntax with metal-compatible.
 * We need to replace syntax gl_ClipDistance[N] with gl_ClipDistance_N such that it is compatible
 * with the Metal shaders Vertex shader output struct. */
static void extract_and_replace_clipping_distances(std::string &vertex_source,
                                                   MSLGeneratorInterface &msl_iface)
{
  char *current_str_begin = &*vertex_source.begin();
  char *current_str_end = &*vertex_source.end();

  for (char *c = current_str_begin + 2; c < current_str_end - 18; c++) {
    char *base_search = strstr(c, "gl_ClipDistance[");
    if (base_search == nullptr) {
      /* No clip distances found. */
      return;
    }
    c = base_search + 16;

    /* Ensure closing brace. */
    if (*(c + 1) != ']') {
      continue;
    }

    /* Extract ID between zero and 9. */
    if ((*c >= '0') && (*c <= '9')) {
      char clip_distance_id = ((*c) - '0');
      auto found = std::find(
          msl_iface.clip_distances.begin(), msl_iface.clip_distances.end(), clip_distance_id);
      if (found == msl_iface.clip_distances.end()) {
        msl_iface.clip_distances.append(clip_distance_id);
      }

      /* Replace syntax (array brace removal, and replacement with underscore). */
      *(base_search + 15) = '_';
      *(base_search + 17) = ' ';
    }
  }
}

/**
 * Replace function parameter patterns containing:
 * `out vec3 somevar` with `THD vec3&somevar`.
 * which enables pass by reference via resolved macro:
 * `thread vec3& somevar`.
 */
static void replace_outvars(std::string &str)
{
  char *current_str_begin = &*str.begin();
  char *current_str_end = &*str.end();

  for (char *c = current_str_begin + 2; c < current_str_end - 6; c++) {
    char *start = strstr(c, "out ");
    if (start == nullptr) {
      return;
    }
    else {
      c = start;
      if (strncmp(c - 2, "in", 2) == 0) {
        start = c - 2;
      }

      /* Check that the following are words. */
      int len1, len2;
      char *word_base1 = c + 4;
      char *word_base2 = word_base1;

      if (is_program_word(word_base1, &len1) && (*(word_base1 + len1) == ' ')) {
        word_base2 = word_base1 + len1 + 1;
        if (is_program_word(word_base2, &len2)) {
          /* Match found. */
          bool is_array = (*(word_base2 + len2) == '[');
          if (is_array) {
            /* Generate out-variable pattern for arrays, of form
             * `OUT(vec2,samples,CRYPTOMATTE_LEVELS_MAX)`
             * replacing original `out vec2 samples[SAMPLE_LEN]`
             * using 'OUT' macro declared in `mtl_shader_defines.msl`. */
            char *array_end = strchr(word_base2 + len2, ']');
            if (array_end != nullptr) {
              *start = 'O';
              *(start + 1) = 'U';
              *(start + 2) = 'T';
              *(start + 3) = '(';
              for (char *clear = start + 4; clear < c + 4; clear++) {
                *clear = ' ';
              }
              *(word_base2 - 1) = ',';
              *(word_base2 + len2) = ',';
              *array_end = ')';
            }
          }
          else {
            /* Generate out-variable pattern of form `THD type&var` from original `out vec4 var`.
             */
            *start = 'T';
            *(start + 1) = 'H';
            *(start + 2) = 'D';
            for (char *clear = start + 3; clear < c + 4; clear++) {
              *clear = ' ';
            }
            *(word_base2 - 1) = '&';
          }
        }
      }
    }
  }
}

static void replace_matrix_constructors(std::string &str)
{

  /* Replace matrix constructors with GLSL-compatible constructors for Metal.
   * Base matrix constructors e.g. mat3x3 do not have as many overload variants as GLSL.
   * To add compatibility, we declare custom constructors e.g. MAT3x3 in `mtl_shader_defines.msl`.
   * If the GLSL syntax matches, we map mat3x3(..) -> MAT3x3(..) and implement a custom
   * constructor. This supports both mat3(..) and mat3x3(..) style syntax. */
  char *current_str_begin = &*str.begin();
  char *current_str_end = &*str.end();

  for (char *c = current_str_begin; c < current_str_end - 10; c++) {
    char *base_scan = strstr(c, "mat");
    if (base_scan == nullptr) {
      break;
    }
    /* Track end of constructor. */
    char *constructor_end = nullptr;

    /* check if next character is matrix dim. */
    c = base_scan + 3;
    if (!(*c == '2' || *c == '3' || *c == '4')) {
      /* Not constructor, skip. */
      continue;
    }

    /* Possible multiple dimensional matrix constructor. Verify if next char is a dim. */
    c++;
    if (*c == 'x') {
      c++;
      if (*c == '2' || *c == '3' || *c == '4') {
        c++;
      }
      else {
        /* Not matrix constructor, continue. */
        continue;
      }
    }

    /* Check for constructor opening brace. */
    if (*c == '(') {
      constructor_end = c;
    }
    else {
      /* Not matrix constructor, continue. */
      continue;
    }

    /* If is constructor, replace with MATN(..) syntax. */
    if (constructor_end != nullptr) {
      ARRAY_SET_ITEMS(base_scan, 'M', 'A', 'T');
      continue;
    }
  }
}

static void replace_array_initializers_func(std::string &str)
{
  char *current_str_begin = &*str.begin();
  char *current_str_end = &*str.end();

  for (char *c = current_str_begin; c < current_str_end - 6; c++) {

    int typelen = 0;

    /* first find next array brace, then work backwards to find start of program word to check if
     * valid array syntax. */
    char *array_scan = strchr(c, '[');
    if (array_scan == nullptr) {
      return;
    }
    typelen = backwards_program_word_scan(array_scan - 1, current_str_begin);
    char *base_type_name = array_scan - 1 - typelen;

    if (typelen > 0) {
      // if (is_program_word(c, &typelen) && *(c + typelen) == '[') {

      c = array_scan;
      char *closing_square_brace = strchr(c, ']');
      if (closing_square_brace != nullptr) {
        c = closing_square_brace;
        char *first_bracket = c + 1;
        if (*first_bracket == '(') {
          c += 1;
          char *semi_colon = strchr(c, ';');
          if (semi_colon != nullptr && *(semi_colon - 1) == ')') {
            char *closing_bracket = semi_colon - 1;

            /* Resolve to MSL-compatible array formatting. */
            *first_bracket = '{';
            *closing_bracket = '}';
            for (char *clear = base_type_name; clear <= closing_square_brace; clear++) {
              *clear = ' ';
            }
          }
        }
      }
      else {
        return;
      }
    }
    else {
      /* Not an array initializer, continue scanning. */
      c = array_scan + 1;
      continue;
    }
  }
}

#ifndef NDEBUG

static bool balanced_braces(char *current_str_begin, char *current_str_end)
{
  int nested_bracket_depth = 0;
  for (char *c = current_str_begin; c < current_str_end; c++) {
    /* Track whether we are in global scope. */
    if (*c == '{' || *c == '[' || *c == '(') {
      nested_bracket_depth++;
      continue;
    }
    if (*c == '}' || *c == ']' || *c == ')') {
      nested_bracket_depth--;
      continue;
    }
  }
  return (nested_bracket_depth == 0);
}

/**
 * Certain Constants (such as arrays, or pointer types) declared in Global-scope
 * end up being initialized per shader thread, resulting in high
 * register pressure within the shader.
 * Here we flag occurrences of these constants such that
 * they can be moved to a place where this is not a problem.
 *
 * Constants declared within function-scope do not exhibit this problem.
 */
static void extract_global_scope_constants(std::string &str,
                                           std::stringstream & /*global_scope_out*/)
{
  char *current_str_begin = &*str.begin();
  char *current_str_end = &*str.end();

  int nested_bracket_depth = 0;
  for (char *c = current_str_begin; c < current_str_end - 6; c++) {
    /* Track whether we are in global scope. */
    if (*c == '{' || *c == '[' || *c == '(') {
      nested_bracket_depth++;
      continue;
    }
    if (*c == '}' || *c == ']' || *c == ')') {
      nested_bracket_depth--;
      BLI_assert(nested_bracket_depth >= 0);
      continue;
    }

    /* Check For global const declarations */
    if (nested_bracket_depth == 0 && strncmp(c, "const ", 6) == 0 &&
        strncmp(c, "const constant ", 15) != 0)
    {
      char *c_expr_end = strchr(c, ';');
      if (c_expr_end != nullptr && balanced_braces(c, c_expr_end)) {
        MTL_LOG_INFO(
            "[PERFORMANCE WARNING] Global scope constant expression found - These get allocated "
            "per-thread in METAL - Best to use Macro's or uniforms to avoid overhead: '%.*s'",
            (int)(c_expr_end + 1 - c),
            c);

        /* Jump ptr forward as we know we remain in global scope. */
        c = c_expr_end - 1;
        continue;
      }
    }
  }
}
#endif

static bool extract_ssbo_pragma_info(const MTLShader *shader,
                                     const MSLGeneratorInterface & /*msl_iface*/,
                                     const std::string &in_vertex_src,
                                     MTLPrimitiveType &out_prim_tye,
                                     uint32_t &out_num_output_verts)
{
  /* SSBO Vertex-fetch parameter extraction. */
  static std::regex use_ssbo_fetch_mode_find(
      "#pragma "
      "USE_SSBO_VERTEX_FETCH\\(\\s*(TriangleList|LineList|TriangleStrip|\\w+)\\s*,\\s*([0-9]+)\\s*"
      "\\)");

  /* Perform regex search if pragma string found. */
  std::smatch vertex_shader_ssbo_flags;
  bool uses_ssbo_fetch = false;
  if (in_vertex_src.find("#pragma USE_SSBO_VERTEX_FETCH") != std::string::npos) {
    uses_ssbo_fetch = std::regex_search(
        in_vertex_src, vertex_shader_ssbo_flags, use_ssbo_fetch_mode_find);
  }
  if (uses_ssbo_fetch) {
    /* Extract Expected output primitive type:
     * #pragma USE_SSBO_VERTEX_FETCH(Output Prim Type, num output vertices per input primitive)
     *
     * Supported Primitive Types (Others can be added if needed, but List types for efficiency):
     * - TriangleList
     * - LineList
     * - TriangleStrip (To be used with caution).
     *
     * Output vertex count is determined by calculating the number of input primitives, and
     * multiplying that by the number of output vertices specified. */
    std::string str_output_primitive_type = vertex_shader_ssbo_flags[1].str();
    std::string str_output_prim_count_per_vertex = vertex_shader_ssbo_flags[2].str();

    /* Ensure output primitive type is valid. */
    if (str_output_primitive_type == "TriangleList") {
      out_prim_tye = MTLPrimitiveTypeTriangle;
    }
    else if (str_output_primitive_type == "LineList") {
      out_prim_tye = MTLPrimitiveTypeLine;
    }
    else if (str_output_primitive_type == "TriangleStrip") {
      out_prim_tye = MTLPrimitiveTypeTriangleStrip;
    }
    else {
      MTL_LOG_ERROR("Unsupported output primitive type for SSBO VERTEX FETCH MODE. Shader: %s",
                    shader->name_get());
      return false;
    }

    /* Assign output num vertices per primitive. */
    out_num_output_verts = std::stoi(
        std::regex_replace(str_output_prim_count_per_vertex, remove_non_numeric_characters, ""));
    BLI_assert(out_num_output_verts > 0);
    return true;
  }

  /* SSBO Vertex fetchmode not used. */
  return false;
}

/* Extract shared memory declaration and their parameters.
 * Inserts extracted cases as entries in MSLGeneratorInterface's shared memory block
 * list. These will later be used to generate shared memory declarations within the entry point.
 *
 * TODO(Metal/GPU): Move shared memory declarations to GPUShaderCreateInfo. This is currently a
 * necessary workaround to match GLSL functionality and enable full compute shader support. In the
 * long term, best to avoid needing to perform this operation. */
void extract_shared_memory_blocks(MSLGeneratorInterface &msl_iface,
                                  std::string &glsl_compute_source)
{
  msl_iface.shared_memory_blocks.clear();
  char *current_str_begin = &*glsl_compute_source.begin();
  char *current_str_end = &*glsl_compute_source.end();

  for (char *c = current_str_begin; c < current_str_end - 6; c++) {
    /* Find first instance of "shared ". */
    char *c_expr_start = strstr(c, "shared ");
    if (c_expr_start == nullptr) {
      break;
    }
    /* Check if "shared" was part of a previous word. If so, this is not valid. */
    if (next_word_in_range(c_expr_start - 1, c_expr_start) != nullptr) {
      c += 7; /* Jump forward by length of "shared ". */
      continue;
    }

    /* Jump to shared declaration and detect end of statement. */
    c = c_expr_start;
    char *c_expr_end = strstr(c, ";");
    if (c_expr_end == nullptr) {
      break;
    }

    /* Prepare MSLSharedMemoryBlock instance. */
    MSLSharedMemoryBlock new_shared_block;
    char buf[256];

    /* Read type-name. */
    c += 7; /* Jump forward by length of "shared ". */
    c = next_word_in_range(c, c_expr_end);
    if (c == nullptr) {
      c = c_expr_end + 1;
      continue;
    }

    char *c_next_space = next_symbol_in_range(c, c_expr_end, ' ');
    if (c_next_space == nullptr) {
      c = c_expr_end + 1;
      continue;
    }
    int len = c_next_space - c;
    BLI_assert(len < 256);
    BLI_strncpy(buf, c, len + 1);
    new_shared_block.type_name = std::string(buf);

    /* Read var-name.
     * `varname` can either come right before the final semi-colon, or
     * with following array syntax.
     * spaces may exist before closing symbol. */
    c = c_next_space + 1;
    c = next_word_in_range(c, c_expr_end);
    if (c == nullptr) {
      c = c_expr_end + 1;
      continue;
    }

    char *c_array_begin = next_symbol_in_range(c, c_expr_end, '[');
    c_next_space = next_symbol_in_range(c, c_expr_end, ' ');

    char *varname_end = nullptr;
    if (c_array_begin != nullptr) {
      /* Array path. */
      if (c_next_space != nullptr) {
        varname_end = (c_next_space < c_array_begin) ? c_next_space : c_array_begin;
      }
      else {
        varname_end = c_array_begin;
      }
      new_shared_block.is_array = true;
    }
    else {
      /* Ending semi-colon. */
      if (c_next_space != nullptr) {
        varname_end = (c_next_space < c_expr_end) ? c_next_space : c_expr_end;
      }
      else {
        varname_end = c_expr_end;
      }
      new_shared_block.is_array = false;
    }
    len = varname_end - c;
    BLI_assert(len < 256);
    BLI_strncpy(buf, c, len + 1);
    new_shared_block.varname = std::string(buf);

    /* Determine if array. */
    if (new_shared_block.is_array) {
      int len = c_expr_end - c_array_begin;
      BLI_strncpy(buf, c_array_begin, len + 1);
      new_shared_block.array_decl = std::string(buf);
    }

    /* Shared block is valid, add it to the list and replace declaration with class member.
     * reference. This declaration needs to have one of the formats:
     * TG int& varname;
     * TG int (&varname)[len][len]
     *
     * In order to fit in the same space, replace `threadgroup` with `TG` macro.
     */
    for (char *c = c_expr_start; c <= c_expr_end; c++) {
      *c = ' ';
    }
    std::string out_str = "TG ";
    out_str += new_shared_block.type_name;
    out_str += (new_shared_block.is_array) ? "(&" : "&";
    out_str += new_shared_block.varname;
    if (new_shared_block.is_array) {
      out_str += ")" + new_shared_block.array_decl;
    }
    out_str += ";;";
    memcpy(c_expr_start, out_str.c_str(), (out_str.length() - 1) * sizeof(char));

    /* Jump to end of statement. */
    c = c_expr_end + 1;

    msl_iface.shared_memory_blocks.append(new_shared_block);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name MTLShader builtin shader generation utilities.
 * \{ */

static void print_resource(std::ostream &os, const ShaderCreateInfo::Resource &res)
{
  switch (res.bind_type) {
    case ShaderCreateInfo::Resource::BindType::SAMPLER:
      break;
    case ShaderCreateInfo::Resource::BindType::IMAGE:
      break;
    case ShaderCreateInfo::Resource::BindType::UNIFORM_BUFFER: {
      int64_t array_offset = res.uniformbuf.name.find_first_of("[");
      if (array_offset == -1) {
        /* Create local class member as constant pointer reference to bound UBO buffer.
         * Given usage within a shader follows ubo_name.ubo_element syntax, we can
         * dereference the pointer as the compiler will optimize this data fetch.
         * To do this, we also give the UBO name a post-fix of `_local` to avoid
         * macro accessor collisions. */
        os << "constant " << res.uniformbuf.type_name << " *" << res.uniformbuf.name
           << "_local;\n";
        os << "#define " << res.uniformbuf.name << " (*" << res.uniformbuf.name << "_local)\n";
      }
      else {
        /* For arrays, we can directly provide the constant access pointer, as the array
         * syntax will de-reference this at the correct fetch index. */
        StringRef name_no_array = StringRef(res.uniformbuf.name.c_str(), array_offset);
        os << "constant " << res.uniformbuf.type_name << " *" << name_no_array << ";\n";
      }
      break;
    }
    case ShaderCreateInfo::Resource::BindType::STORAGE_BUFFER: {
      int64_t array_offset = res.storagebuf.name.find_first_of("[");
      bool writeable = (res.storagebuf.qualifiers & shader::Qualifier::WRITE) ==
                       shader::Qualifier::WRITE;
      const char *memory_scope = ((writeable) ? "device " : "constant ");
      if (array_offset == -1) {
        /* Create local class member as device pointer reference to bound SSBO.
         * Given usage within a shader follows ssbo_name.ssbo_element syntax, we can
         * dereference the pointer as the compiler will optimize this data fetch.
         * To do this, we also give the UBO name a post-fix of `_local` to avoid
         * macro accessor collisions. */

        os << memory_scope << res.storagebuf.type_name << " *" << res.storagebuf.name
           << "_local;\n";
        os << "#define " << res.storagebuf.name << " (*" << res.storagebuf.name << "_local)\n";
      }
      else {
        /* For arrays, we can directly provide the constant access pointer, as the array
         * syntax will de-reference this at the correct fetch index. */
        StringRef name_no_array = StringRef(res.storagebuf.name.c_str(), array_offset);
        os << memory_scope << res.storagebuf.type_name << " *" << name_no_array << ";\n";
      }
      break;
    }
  }
}

std::string MTLShader::resources_declare(const ShaderCreateInfo &info) const
{
  /* NOTE(Metal): We only use the upfront preparation functions to populate members which
   * would exist in the original non-create-info variant.
   *
   * This function is only used to generate resource structs.
   * Global-scope handles for Uniforms, UBOs, textures and samplers
   * are generated during class-wrapper construction in `generate_msl_from_glsl`. */
  std::stringstream ss;

  /* Generate resource stubs for UBOs and textures. */
  ss << "\n/* Pass Resources. */\n";
  for (const ShaderCreateInfo::Resource &res : info.pass_resources_) {
    print_resource(ss, res);
  }
  ss << "\n/* Batch Resources. */\n";
  for (const ShaderCreateInfo::Resource &res : info.batch_resources_) {
    print_resource(ss, res);
  }
  /* NOTE: Push constant uniform data is generated during `generate_msl_from_glsl`
   * as the generated output is needed for all paths. This includes generation
   * of the push constant data structure (struct PushConstantBlock).
   * As all shader generation paths require creation of this. */
  return ss.str();
}

std::string MTLShader::vertex_interface_declare(const shader::ShaderCreateInfo &info) const
{
  /* NOTE(Metal): We only use the upfront preparation functions to populate members which
   * would exist in the original non-create-info variant.
   *
   * Here we generate the variables within class wrapper scope to allow reading of
   * input attributes by the main code. */
  std::stringstream ss;
  ss << "\n/* Vertex Inputs. */\n";
  for (const ShaderCreateInfo::VertIn &attr : info.vertex_inputs_) {
    ss << to_string(attr.type) << " " << attr.name << ";\n";
  }
  return ss.str();
}

std::string MTLShader::fragment_interface_declare(const shader::ShaderCreateInfo &info) const
{
  /* For shaders generated from MSL, the fragment-output struct is generated as part of the entry
   * stub during glsl->MSL conversion in `generate_msl_from_glsl`.
   * Here, we can instead generate the global-scope variables which will be populated during
   * execution.
   *
   * NOTE: The output declaration for location and blend index are generated in the entry-point
   * struct. This is simply a mirror class member which stores the value during main shader body
   * execution. */
  std::stringstream ss;
  ss << "\n/* Fragment Outputs. */\n";
  for (const ShaderCreateInfo::FragOut &output : info.fragment_outputs_) {
    ss << to_string(output.type) << " " << output.name << ";\n";
  }
  ss << "\n";

  ss << "\n/* Fragment Tile inputs. */\n";
  for (const ShaderCreateInfo::SubpassIn &input : info.subpass_inputs_) {
    ss << to_string(input.type) << " " << input.name << ";\n";
  }
  ss << "\n";

  return ss.str();
}

std::string MTLShader::MTLShader::geometry_interface_declare(
    const shader::ShaderCreateInfo & /*info*/) const
{
  BLI_assert_msg(false, "Geometry shading unsupported by Metal");
  return "";
}

std::string MTLShader::geometry_layout_declare(const shader::ShaderCreateInfo & /*info*/) const
{
  BLI_assert_msg(false, "Geometry shading unsupported by Metal");
  return "";
}

std::string MTLShader::compute_layout_declare(const ShaderCreateInfo & /*info*/) const
{
  /* Metal supports compute shaders. THis function is a pass-through.
   * Compute shader interface population happens during mtl_shader_generator, as part of GLSL
   * conversion. */
  return "";
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shader Translation.
 * \{ */

char *MSLGeneratorInterface::msl_patch_default_get()
{
  if (msl_patch_default != nullptr) {
    return msl_patch_default;
  }

  std::stringstream ss_patch;
  ss_patch << datatoc_mtl_shader_defines_msl << std::endl;
  ss_patch << datatoc_mtl_shader_shared_h << std::endl;
  size_t len = strlen(ss_patch.str().c_str()) + 1;

  msl_patch_default = (char *)malloc(len * sizeof(char));
  memcpy(msl_patch_default, ss_patch.str().c_str(), len * sizeof(char));
  return msl_patch_default;
}

/* Specialization constants will evaluate using a dynamic value if provided at PSO compile time. */
static void generate_specialization_constant_declarations(const shader::ShaderCreateInfo *info,
                                                          std::stringstream &ss)
{
  uint index = MTL_SHADER_SPECIALIZATION_CONSTANT_BASE_ID;
  for (const ShaderCreateInfo::SpecializationConstant &sc : info->specialization_constants_) {
    /* TODO(Metal): Output specialization constant chain. */
    ss << "constant " << sc.type << " " << sc.name << " [[function_constant(" << index << ")]];\n";
    index++;
  }
}

bool MTLShader::generate_msl_from_glsl(const shader::ShaderCreateInfo *info)
{
  /* Verify if create-info is available.
   * NOTE(Metal): For now, only support creation from CreateInfo.
   * If needed, we can perform source translation without this using
   * manual reflection. */
  bool uses_create_info = info != nullptr;
  if (!uses_create_info) {
    MTL_LOG_WARNING("Unable to compile shader %p '%s' as no create-info was provided!",
                    this,
                    this->name_get());
    valid_ = false;
    return false;
  }

  /* Compute shaders use differing compilation path. */
  if (shd_builder_->glsl_compute_source_.size() > 0) {
    return this->generate_msl_from_glsl_compute(info);
  }

  /* #MSLGeneratorInterface is a class populated to describe all parameters, resources, bindings
   * and features used by the source GLSL shader. This information is then used to generate the
   * appropriate Metal entry points and perform any required source translation. */
  MSLGeneratorInterface msl_iface(*this);
  BLI_assert(shd_builder_ != nullptr);

  /* Populate #MSLGeneratorInterface from Create-Info.
   * NOTE: this is a separate path as #MSLGeneratorInterface can also be manually populated
   * from parsing, if support for shaders without create-info is required. */
  msl_iface.prepare_from_createinfo(info);

  /* Verify Source sizes are greater than zero. */
  BLI_assert(shd_builder_->glsl_vertex_source_.size() > 0);
  if (!msl_iface.uses_transform_feedback) {
    BLI_assert(shd_builder_->glsl_fragment_source_.size() > 0);
  }

  if (transform_feedback_type_ != GPU_SHADER_TFB_NONE) {
    /* Ensure #TransformFeedback is configured correctly. */
    BLI_assert(tf_output_name_list_.size() > 0);
    msl_iface.uses_transform_feedback = true;
  }

  /* Concatenate msl_shader_defines to provide functionality mapping
   * from GLSL to MSL. Also include additional GPU defines for
   * optional high-level feature support. */
  std::string msl_defines_string = "#define GPU_ARB_shader_draw_parameters 1\n";

  /* NOTE(Metal): textureGather appears to not function correctly on non-Apple-silicon GPUs.
   * Manifests as selection outlines not showing up (#103412). Disable texture gather if
   * not suitable for use. */
  if (MTLBackend::get_capabilities().supports_texture_gather) {
    msl_defines_string += "#define GPU_ARB_texture_gather 1\n";
  }

  shd_builder_->glsl_vertex_source_ = msl_defines_string + shd_builder_->glsl_vertex_source_;
  if (!msl_iface.uses_transform_feedback) {
    shd_builder_->glsl_fragment_source_ = msl_defines_string + shd_builder_->glsl_fragment_source_;
  }

  /* Extract SSBO usage information from shader pragma:
   *
   * #pragma USE_SSBO_VERTEX_FETCH(Output Prim Type, num output vertices per input primitive)
   *
   * This will determine whether SSBO-vertex-fetch
   * mode is used for this shader. Returns true if used, and populates output reference
   * values with the output prim type and output number of vertices. */
  MTLPrimitiveType vertex_fetch_ssbo_output_prim_type = MTLPrimitiveTypeTriangle;
  uint32_t vertex_fetch_ssbo_num_output_verts = 0;
  msl_iface.uses_ssbo_vertex_fetch_mode = extract_ssbo_pragma_info(
      this,
      msl_iface,
      shd_builder_->glsl_vertex_source_,
      vertex_fetch_ssbo_output_prim_type,
      vertex_fetch_ssbo_num_output_verts);

  if (msl_iface.uses_ssbo_vertex_fetch_mode) {
    shader_debug_printf(
        "[Shader] SSBO VERTEX FETCH Enabled for Shader '%s' With Output primitive type: %s, "
        "vertex count: %u\n",
        this->name_get(),
        output_primitive_type.c_str(),
        vertex_fetch_ssbo_num_output_verts);
  }

  /* Special condition - mat3 and array constructor replacement. */
  replace_matrix_constructors(shd_builder_->glsl_vertex_source_);
  replace_array_initializers_func(shd_builder_->glsl_vertex_source_);

  if (!msl_iface.uses_transform_feedback) {
    replace_matrix_constructors(shd_builder_->glsl_fragment_source_);
    replace_array_initializers_func(shd_builder_->glsl_fragment_source_);
  }

  /**** Extract usage of GL globals. ****/
  /* NOTE(METAL): Currently still performing fallback string scan, as info->builtins_ does
   * not always contain the usage flag. This can be removed once all appropriate create-info's
   * have been updated. In some cases, this may incur a false positive if access is guarded
   * behind a macro. Though in these cases, unused code paths and parameters will be
   * optimized out by the Metal shader compiler. */

  /** Identify usage of vertex-shader builtins. */
  msl_iface.uses_gl_VertexID = bool(info->builtins_ & BuiltinBits::VERTEX_ID) ||
                               shd_builder_->glsl_vertex_source_.find("gl_VertexID") !=
                                   std::string::npos;
  msl_iface.uses_gl_InstanceID = bool(info->builtins_ & BuiltinBits::INSTANCE_ID) ||
                                 shd_builder_->glsl_vertex_source_.find("gl_InstanceID") !=
                                     std::string::npos ||
                                 shd_builder_->glsl_vertex_source_.find("gpu_InstanceIndex") !=
                                     std::string::npos ||
                                 msl_iface.uses_ssbo_vertex_fetch_mode;

  /* instance ID in GL is `[0, instance_count]` in metal it is
   * `[base_instance, base_instance + instance_count]`,
   * so we need to offset instance_ID by base instance in Metal --
   * Thus we expose the `[[base_instance]]` attribute if instance ID is used at all. */
  msl_iface.uses_gl_BaseInstanceARB = msl_iface.uses_gl_InstanceID ||
                                      shd_builder_->glsl_vertex_source_.find(
                                          "gl_BaseInstanceARB") != std::string::npos ||
                                      shd_builder_->glsl_vertex_source_.find("gpu_BaseInstance") !=
                                          std::string::npos;
  msl_iface.uses_gl_Position = shd_builder_->glsl_vertex_source_.find("gl_Position") !=
                               std::string::npos;
  msl_iface.uses_gl_PointSize = shd_builder_->glsl_vertex_source_.find("gl_PointSize") !=
                                std::string::npos;
  msl_iface.uses_gpu_layer = bool(info->builtins_ & BuiltinBits::LAYER);
  msl_iface.uses_gpu_viewport_index = bool(info->builtins_ & BuiltinBits::VIEWPORT_INDEX);

  /** Identify usage of fragment-shader builtins. */
  if (!msl_iface.uses_transform_feedback) {
    std::smatch gl_special_cases;
    msl_iface.uses_gl_PointCoord = bool(info->builtins_ & BuiltinBits::POINT_COORD) ||
                                   shd_builder_->glsl_fragment_source_.find("gl_PointCoord") !=
                                       std::string::npos;
    msl_iface.uses_barycentrics = bool(info->builtins_ & BuiltinBits::BARYCENTRIC_COORD);
    msl_iface.uses_gl_FrontFacing = bool(info->builtins_ & BuiltinBits::FRONT_FACING) ||
                                    shd_builder_->glsl_fragment_source_.find("gl_FrontFacing") !=
                                        std::string::npos;
    msl_iface.uses_gl_PrimitiveID = bool(info->builtins_ & BuiltinBits::PRIMITIVE_ID) ||
                                    shd_builder_->glsl_fragment_source_.find("gl_PrimitiveID") !=
                                        std::string::npos;

    /* NOTE(Metal): If FragColor is not used, then we treat the first fragment output attachment
     * as the primary output. */
    msl_iface.uses_gl_FragColor = shd_builder_->glsl_fragment_source_.find("gl_FragColor") !=
                                  std::string::npos;

    /* NOTE(Metal): FragDepth output mode specified in create-info 'DepthWrite depth_write_'.
     * If parsing without create-info, manual extraction will be required. */
    msl_iface.uses_gl_FragDepth = (info->depth_write_ != DepthWrite::UNCHANGED) &&
                                  shd_builder_->glsl_fragment_source_.find("gl_FragDepth") !=
                                      std::string::npos;

    /* TODO(fclem): Add to create info. */
    msl_iface.uses_gl_FragStencilRefARB = shd_builder_->glsl_fragment_source_.find(
                                              "gl_FragStencilRefARB") != std::string::npos;

    msl_iface.depth_write = info->depth_write_;

    /* Early fragment tests. */
    msl_iface.uses_early_fragment_test = info->early_fragment_test_;
  }

  /* Generate SSBO vertex fetch mode uniform data hooks. */
  if (msl_iface.uses_ssbo_vertex_fetch_mode) {
    msl_iface.prepare_ssbo_vertex_fetch_uniforms();
  }

  /* Extract gl_ClipDistances. */
  extract_and_replace_clipping_distances(shd_builder_->glsl_vertex_source_, msl_iface);

  /* Replace 'out' attribute on function parameters with pass-by-reference. */
  replace_outvars(shd_builder_->glsl_vertex_source_);
  if (!msl_iface.uses_transform_feedback) {
    replace_outvars(shd_builder_->glsl_fragment_source_);
  }

  /**** METAL Shader source generation. ****/
  /* Setup `stringstream` for populating generated MSL shader vertex/frag shaders. */
  std::stringstream ss_vertex;
  std::stringstream ss_fragment;
  ss_vertex << "#line 1 \"msl_wrapper_code\"\n";
  ss_fragment << "#line 1 \"msl_wrapper_code\"\n";

  if (bool(info->builtins_ & BuiltinBits::TEXTURE_ATOMIC) &&
      MTLBackend::get_capabilities().supports_texture_atomics)
  {
    ss_vertex << ATOMIC_DEFINE_STR;
    ss_fragment << ATOMIC_DEFINE_STR;
  }

  /* Generate specialization constants. */
  generate_specialization_constant_declarations(info, ss_vertex);
  generate_specialization_constant_declarations(info, ss_fragment);

  /*** Generate VERTEX Stage ***/
  /* Conditional defines. */
  if (msl_iface.use_argument_buffer_for_samplers()) {
    ss_vertex << "#define USE_ARGUMENT_BUFFER_FOR_SAMPLERS 1" << std::endl;
    ss_vertex << "#define ARGUMENT_BUFFER_NUM_SAMPLERS "
              << msl_iface.max_sampler_index_for_stage(ShaderStage::VERTEX) + 1 << std::endl;
  }
  if (msl_iface.uses_ssbo_vertex_fetch_mode) {
    ss_vertex << "#define MTL_SSBO_VERTEX_FETCH 1" << std::endl;
    for (const MSLVertexInputAttribute &attr : msl_iface.vertex_input_attributes) {
      ss_vertex << "#define SSBO_ATTR_TYPE_" << attr.name << " " << attr.type << std::endl;
    }

    /* Macro's */
    ss_vertex << "#define "
                 "UNIFORM_SSBO_USES_INDEXED_RENDERING_STR " UNIFORM_SSBO_USES_INDEXED_RENDERING_STR
                 "\n"
                 "#define UNIFORM_SSBO_INDEX_MODE_U16_STR " UNIFORM_SSBO_INDEX_MODE_U16_STR
                 "\n"
                 "#define UNIFORM_SSBO_INPUT_PRIM_TYPE_STR " UNIFORM_SSBO_INPUT_PRIM_TYPE_STR
                 "\n"
                 "#define UNIFORM_SSBO_INPUT_VERT_COUNT_STR " UNIFORM_SSBO_INPUT_VERT_COUNT_STR
                 "\n"
                 "#define UNIFORM_SSBO_OFFSET_STR " UNIFORM_SSBO_OFFSET_STR
                 "\n"
                 "#define UNIFORM_SSBO_STRIDE_STR " UNIFORM_SSBO_STRIDE_STR
                 "\n"
                 "#define UNIFORM_SSBO_FETCHMODE_STR " UNIFORM_SSBO_FETCHMODE_STR
                 "\n"
                 "#define UNIFORM_SSBO_VBO_ID_STR " UNIFORM_SSBO_VBO_ID_STR
                 "\n"
                 "#define UNIFORM_SSBO_TYPE_STR " UNIFORM_SSBO_TYPE_STR "\n";
  }

  /* Inject common Metal header. */
  ss_vertex << msl_iface.msl_patch_default_get() << std::endl << std::endl;

#ifndef NDEBUG
  /* Performance warning: Extract global-scope expressions.
   * NOTE: This is dependent on stripping out comments
   * to remove false positives. */
  remove_multiline_comments_func(shd_builder_->glsl_vertex_source_);
  remove_singleline_comments_func(shd_builder_->glsl_vertex_source_);
  extract_global_scope_constants(shd_builder_->glsl_vertex_source_, ss_vertex);
#endif

  /* Generate additional shader interface struct members from create-info. */
  for (const StageInterfaceInfo *iface : info->vertex_out_interfaces_) {

    /* Only generate struct for ones with instance names */
    if (!iface->instance_name.is_empty()) {
      ss_vertex << "struct " << iface->name << " {" << std::endl;
      for (const StageInterfaceInfo::InOut &inout : iface->inouts) {
        ss_vertex << to_string(inout.type) << " " << inout.name << " "
                  << to_string_msl(inout.interp) << ";" << std::endl;
      }
      ss_vertex << "};" << std::endl;
    }
  }

  /* Wrap entire GLSL source inside class to create
   * a scope within the class to enable use of global variables.
   * e.g. global access to attributes, uniforms, UBOs, textures etc; */
  ss_vertex << "class " << get_stage_class_name(ShaderStage::VERTEX) << " {" << std::endl;
  ss_vertex << "public:" << std::endl;

  /* Generate additional shader interface struct members from create-info. */
  for (const StageInterfaceInfo *iface : info->vertex_out_interfaces_) {

    bool is_inside_struct = false;
    if (!iface->instance_name.is_empty()) {
      /* If shader stage interface has an instance name, then it
       * is using a struct format and as such we only need a local
       * class member for the struct, not each element. */
      ss_vertex << iface->name << " " << iface->instance_name << ";" << std::endl;
      is_inside_struct = true;
    }

    /* Generate local variables, populate elems for vertex out struct gen. */
    for (const StageInterfaceInfo::InOut &inout : iface->inouts) {

      /* Only output individual elements if they are not part of an interface struct instance. */
      if (!is_inside_struct) {
        ss_vertex << to_string(inout.type) << " " << inout.name << ";" << std::endl;
      }

      const char *arraystart = strchr(inout.name.c_str(), '[');
      bool is_array = (arraystart != nullptr);
      int array_len = (is_array) ? std::stoi(std::regex_replace(
                                       arraystart, remove_non_numeric_characters, "")) :
                                   0;

      /* Remove array from string name. */
      std::string out_name = inout.name.c_str();
      std::size_t pos = out_name.find('[');
      if (is_array && pos != std::string::npos) {
        out_name.resize(pos);
      }

      /* Add to vertex-output interface. */
      msl_iface.vertex_output_varyings.append(
          {to_string(inout.type),
           out_name.c_str(),
           ((is_inside_struct) ? iface->instance_name.c_str() : ""),
           to_string(inout.interp),
           is_array,
           array_len});

      /* Add to fragment-input interface. */
      msl_iface.fragment_input_varyings.append(
          {to_string(inout.type),
           out_name.c_str(),
           ((is_inside_struct) ? iface->instance_name.c_str() : ""),
           to_string(inout.interp),
           is_array,
           array_len});
    }
  }

  /** Generate structs from MSL Interface. **/
  /* Generate VertexIn struct. */
  if (!msl_iface.uses_ssbo_vertex_fetch_mode) {
    ss_vertex << msl_iface.generate_msl_vertex_in_struct();
  }
  /* Generate Uniform data structs. */
  ss_vertex << msl_iface.generate_msl_uniform_structs(ShaderStage::VERTEX);

  /* Conditionally use global GL variables. */
  if (msl_iface.uses_gl_Position) {
    ss_vertex << "float4 gl_Position;" << std::endl;
  }
  if (msl_iface.uses_gl_PointSize) {
    ss_vertex << "float gl_PointSize = 1.0;" << std::endl;
  }
  if (msl_iface.uses_gl_VertexID) {
    ss_vertex << "int gl_VertexID;" << std::endl;
  }
  if (msl_iface.uses_gl_InstanceID) {
    ss_vertex << "int gl_InstanceID;" << std::endl;
  }
  if (msl_iface.uses_gl_BaseInstanceARB) {
    ss_vertex << "int gl_BaseInstanceARB;" << std::endl;
  }
  for (const int cd : IndexRange(msl_iface.clip_distances.size())) {
    ss_vertex << "float gl_ClipDistance_" << cd << ";" << std::endl;
  }

  /* Render target array index if using multilayered rendering. */
  if (msl_iface.uses_gpu_layer) {
    ss_vertex << "int gpu_Layer = 0;" << std::endl;
  }
  if (msl_iface.uses_gpu_viewport_index) {
    ss_vertex << "int gpu_ViewportIndex = 0;" << std::endl;
  }

  /* Global vertex data pointers when using SSBO vertex fetch mode.
   * Bound vertex buffers passed in via the entry point function
   * are assigned to these pointers to be globally accessible
   * from any function within the GLSL source shader. */
  if (msl_iface.uses_ssbo_vertex_fetch_mode) {
    ss_vertex << "constant uchar** MTL_VERTEX_DATA;" << std::endl;
    ss_vertex << "constant ushort* MTL_INDEX_DATA_U16 = nullptr;" << std::endl;
    ss_vertex << "constant uint32_t* MTL_INDEX_DATA_U32 = nullptr;" << std::endl;
  }

  /* Add Texture members.
   * These members pack both a texture and a sampler into a single
   * struct, as both are needed within texture functions.
   * e.g. `_mtl_combined_image_sampler_2d<float, access::read>`
   * The exact typename is generated inside `get_msl_typestring_wrapper()`. */
  for (const MSLTextureResource &tex : msl_iface.texture_samplers) {
    if (bool(tex.stage & ShaderStage::VERTEX)) {
      ss_vertex << "\tthread " << tex.get_msl_typestring_wrapper(false) << ";" << std::endl;
    }
  }
  ss_vertex << std::endl;

  /* Inject main GLSL source into output stream. */
  ss_vertex << shd_builder_->glsl_vertex_source_ << std::endl;

  /* Generate VertexOut and TransformFeedbackOutput structs. */
  ss_vertex << msl_iface.generate_msl_vertex_out_struct(ShaderStage::VERTEX);
  if (msl_iface.uses_transform_feedback) {
    ss_vertex << msl_iface.generate_msl_vertex_transform_feedback_out_struct(ShaderStage::VERTEX);
  }

  /* Class Closing Bracket to end shader global scope. */
  ss_vertex << "};" << std::endl;

  /* Generate Vertex shader entry-point function containing resource bindings. */
  ss_vertex << msl_iface.generate_msl_vertex_entry_stub();

  /*** Generate FRAGMENT Stage. ***/
  if (!msl_iface.uses_transform_feedback) {

    /* Conditional defines. */
    if (msl_iface.use_argument_buffer_for_samplers()) {
      ss_fragment << "#define USE_ARGUMENT_BUFFER_FOR_SAMPLERS 1" << std::endl;
      ss_fragment << "#define ARGUMENT_BUFFER_NUM_SAMPLERS "
                  << msl_iface.max_sampler_index_for_stage(ShaderStage::FRAGMENT) + 1 << std::endl;
    }

    /* Inject common Metal header. */
    ss_fragment << msl_iface.msl_patch_default_get() << std::endl << std::endl;

#ifndef NDEBUG
    /* Performance warning: Identify global-scope expressions.
     * These cause excessive register pressure due to global arrays being instantiated per-thread.
     * NOTE: This is dependent on stripping out comments to remove false positives. */
    remove_multiline_comments_func(shd_builder_->glsl_fragment_source_);
    remove_singleline_comments_func(shd_builder_->glsl_fragment_source_);
    extract_global_scope_constants(shd_builder_->glsl_fragment_source_, ss_fragment);
#endif

    /* Generate additional shader interface struct members from create-info. */
    for (const StageInterfaceInfo *iface : info->vertex_out_interfaces_) {

      /* Only generate struct for ones with instance names. */
      if (!iface->instance_name.is_empty()) {
        ss_fragment << "struct " << iface->name << " {" << std::endl;
        for (const StageInterfaceInfo::InOut &inout : iface->inouts) {
          ss_fragment << to_string(inout.type) << " " << inout.name << ""
                      << to_string_msl(inout.interp) << ";" << std::endl;
        }
        ss_fragment << "};" << std::endl;
      }
    }

    /* Wrap entire GLSL source inside class to create
     * a scope within the class to enable use of global variables. */
    ss_fragment << "class " << get_stage_class_name(ShaderStage::FRAGMENT) << " {" << std::endl;
    ss_fragment << "public:" << std::endl;

    /* In/out interface values */
    /* Generate additional shader interface struct members from create-info. */
    for (const StageInterfaceInfo *iface : info->vertex_out_interfaces_) {
      bool is_inside_struct = false;
      if (!iface->instance_name.is_empty()) {
        /* Struct local variable. */
        ss_fragment << iface->name << " " << iface->instance_name << ";" << std::endl;
        is_inside_struct = true;
      }

      /* Generate local variables, populate elems for vertex out struct gen. */
      for (const StageInterfaceInfo::InOut &inout : iface->inouts) {
        /* Only output individual elements if they are not part of an interface struct instance.
         */
        if (!is_inside_struct) {
          ss_fragment << to_string(inout.type) << " " << inout.name << ";" << std::endl;
        }
      }
    }

    /* Generate global structs */
    ss_fragment << msl_iface.generate_msl_vertex_out_struct(ShaderStage::FRAGMENT);
    if (msl_iface.fragment_tile_inputs.size() > 0) {
      ss_fragment << msl_iface.generate_msl_fragment_struct(true);
    }
    ss_fragment << msl_iface.generate_msl_fragment_struct(false);
    ss_fragment << msl_iface.generate_msl_uniform_structs(ShaderStage::FRAGMENT);

    /** GL globals. */
    /* gl_FragCoord will always be assigned to the output position from vertex shading. */
    ss_fragment << "float4 gl_FragCoord;" << std::endl;
    if (msl_iface.uses_gl_FragColor) {
      ss_fragment << "float4 gl_FragColor;" << std::endl;
    }
    if (msl_iface.uses_gl_FragDepth) {
      ss_fragment << "float gl_FragDepth;" << std::endl;
    }
    if (msl_iface.uses_gl_FragStencilRefARB) {
      ss_fragment << "int gl_FragStencilRefARB;" << std::endl;
    }
    if (msl_iface.uses_gl_PointCoord) {
      ss_fragment << "float2 gl_PointCoord;" << std::endl;
    }
    if (msl_iface.uses_gl_FrontFacing) {
      ss_fragment << "bool gl_FrontFacing;" << std::endl;
    }
    if (msl_iface.uses_gl_PrimitiveID) {
      ss_fragment << "uint gl_PrimitiveID;" << std::endl;
    }

    /* Global barycentrics. */
    if (msl_iface.uses_barycentrics) {
      ss_fragment << "vec3 gpu_BaryCoord;\n";
    }

    /* Render target array index and viewport array index passed from vertex shader. */
    if (msl_iface.uses_gpu_layer) {
      ss_fragment << "int gpu_Layer = 0;" << std::endl;
    }
    if (msl_iface.uses_gpu_viewport_index) {
      ss_fragment << "int gpu_ViewportIndex = 0;" << std::endl;
    }

    /* Add Texture members. */
    for (const MSLTextureResource &tex : msl_iface.texture_samplers) {
      if (bool(tex.stage & ShaderStage::FRAGMENT)) {
        ss_fragment << "\tthread " << tex.get_msl_typestring_wrapper(false) << ";" << std::endl;
      }
    }

    /* Inject Main GLSL Fragment Source into output stream. */
    ss_fragment << shd_builder_->glsl_fragment_source_ << std::endl;

    /* Class Closing Bracket to end shader global scope. */
    ss_fragment << "};" << std::endl;

    /* Generate Fragment entry-point function. */
    ss_fragment << msl_iface.generate_msl_fragment_entry_stub();
  }

  /* DEBUG: Export source to file for manual verification. */
#if MTL_SHADER_DEBUG_EXPORT_SOURCE
  NSFileManager *sharedFM = [NSFileManager defaultManager];
  NSURL *app_bundle_url = [[NSBundle mainBundle] bundleURL];
  NSURL *shader_dir = [[app_bundle_url URLByDeletingLastPathComponent]
      URLByAppendingPathComponent:@"Shaders/"
                      isDirectory:YES];
  [sharedFM createDirectoryAtURL:shader_dir
      withIntermediateDirectories:YES
                       attributes:nil
                            error:nil];
  const char *path_cstr = [shader_dir fileSystemRepresentation];

  std::ofstream vertex_fs;
  vertex_fs.open(
      (std::string(path_cstr) + "/" + std::string(this->name) + "_GeneratedVertexShader.msl")
          .c_str());
  vertex_fs << ss_vertex.str();
  vertex_fs.close();

  if (!msl_iface.uses_transform_feedback) {
    std::ofstream fragment_fs;
    fragment_fs.open(
        (std::string(path_cstr) + "/" + std::string(this->name) + "_GeneratedFragmentShader.msl")
            .c_str());
    fragment_fs << ss_fragment.str();
    fragment_fs.close();
  }

  shader_debug_printf(
      "Vertex Shader Saved to: %s\n",
      (std::string(path_cstr) + std::string(this->name) + "_GeneratedFragmentShader.msl").c_str());
#endif

  /* Set MSL source NSString's. Required by Metal API. */
  NSString *msl_final_vert = [NSString stringWithUTF8String:ss_vertex.str().c_str()];
  NSString *msl_final_frag = (msl_iface.uses_transform_feedback) ?
                                 (@"") :
                                 ([NSString stringWithUTF8String:ss_fragment.str().c_str()]);

  this->shader_source_from_msl(msl_final_vert, msl_final_frag);

#ifndef NDEBUG
  /* In debug mode, we inject the name of the shader into the entry-point function
   * name, as these are what show up in the Xcode GPU debugger. */
  this->set_vertex_function_name(
      [[NSString stringWithFormat:@"vertex_function_entry_%s", this->name] retain]);
  this->set_fragment_function_name(
      [[NSString stringWithFormat:@"fragment_function_entry_%s", this->name] retain]);
#else
  this->set_vertex_function_name(@"vertex_function_entry");
  this->set_fragment_function_name(@"fragment_function_entry");
#endif

  /* Bake shader interface. */
  this->set_interface(msl_iface.bake_shader_interface(this->name));

  /* Update other shader properties. */
  uses_gpu_layer = msl_iface.uses_gpu_layer;
  uses_gpu_viewport_index = msl_iface.uses_gpu_viewport_index;
  use_ssbo_vertex_fetch_mode_ = msl_iface.uses_ssbo_vertex_fetch_mode;
  if (msl_iface.uses_ssbo_vertex_fetch_mode) {
    ssbo_vertex_fetch_output_prim_type_ = vertex_fetch_ssbo_output_prim_type;
    ssbo_vertex_fetch_output_num_verts_ = vertex_fetch_ssbo_num_output_verts;
    this->prepare_ssbo_vertex_fetch_metadata();
  }

  /* Successfully completed GLSL to MSL translation. */
  return true;
}

bool MTLShader::generate_msl_from_glsl_compute(const shader::ShaderCreateInfo *info)
{
  /* #MSLGeneratorInterface is a class populated to describe all parameters, resources, bindings
   * and features used by the source GLSL shader. This information is then used to generate the
   * appropriate Metal entry points and perform any required source translation. */
  MSLGeneratorInterface msl_iface(*this);
  BLI_assert(shd_builder_ != nullptr);

  /* Populate #MSLGeneratorInterface from Create-Info.
   * NOTE: this is a separate path as #MSLGeneratorInterface can also be manually populated
   * from parsing, if support for shaders without create-info is required. */
  msl_iface.prepare_from_createinfo(info);

  /* Verify Source sizes are greater than zero. */
  BLI_assert(shd_builder_->glsl_compute_source_.size() > 0);

  /*** Source cleanup. ***/
  replace_matrix_constructors(shd_builder_->glsl_compute_source_);
  replace_array_initializers_func(shd_builder_->glsl_compute_source_);

  /**** Extract usage of GL globals. ****/
  /* NOTE(METAL): Currently still performing fallback string scan, as info->builtins_ does
   * not always contain the usage flag. This can be removed once all appropriate create-info's
   * have been updated. In some cases, this may incur a false positive if access is guarded
   * behind a macro. Though in these cases, unused code paths and parameters will be
   * optimized out by the Metal shader compiler. */

  /* gl_GlobalInvocationID. */
  msl_iface.uses_gl_GlobalInvocationID =
      bool(info->builtins_ & BuiltinBits::GLOBAL_INVOCATION_ID) ||
      shd_builder_->glsl_compute_source_.find("gl_GlobalInvocationID") != std::string::npos;
  /* gl_WorkGroupSize. */
  msl_iface.uses_gl_WorkGroupSize = bool(info->builtins_ & BuiltinBits::WORK_GROUP_SIZE) ||
                                    shd_builder_->glsl_compute_source_.find("gl_WorkGroupSize") !=
                                        std::string::npos;
  /* gl_WorkGroupID. */
  msl_iface.uses_gl_WorkGroupID = bool(info->builtins_ & BuiltinBits::WORK_GROUP_ID) ||
                                  shd_builder_->glsl_compute_source_.find("gl_WorkGroupID") !=
                                      std::string::npos;
  /* gl_NumWorkGroups. */
  msl_iface.uses_gl_NumWorkGroups = bool(info->builtins_ & BuiltinBits::NUM_WORK_GROUP) ||
                                    shd_builder_->glsl_compute_source_.find("gl_NumWorkGroups") !=
                                        std::string::npos;
  /* gl_LocalInvocationIndex. */
  msl_iface.uses_gl_LocalInvocationIndex =
      bool(info->builtins_ & BuiltinBits::LOCAL_INVOCATION_INDEX) ||
      shd_builder_->glsl_compute_source_.find("gl_LocalInvocationIndex") != std::string::npos;
  /* gl_LocalInvocationID. */
  msl_iface.uses_gl_LocalInvocationID = bool(info->builtins_ & BuiltinBits::LOCAL_INVOCATION_ID) ||
                                        shd_builder_->glsl_compute_source_.find(
                                            "gl_LocalInvocationID") != std::string::npos;

  /* Performance warning: Extract global-scope expressions.
   * NOTE: This is dependent on stripping out comments
   * to remove false positives. */
  remove_multiline_comments_func(shd_builder_->glsl_compute_source_);
  remove_singleline_comments_func(shd_builder_->glsl_compute_source_);

  /** Extract usage of shared memory.
   * For Metal shaders to compile, shared (threadgroup) memory cannot be declared globally.
   * It must reside within a function scope. Hence, we need to extract these uses and generate
   * shared memory blocks within the entry point function, which can then be passed as references
   * to the remaining shader via the class function scope.
   *
   * The existing block definitions are then replaced with references to threadgroup memory blocks,
   * but kept in-line in case external macros are used to declare the dimensions. */
  extract_shared_memory_blocks(msl_iface, shd_builder_->glsl_compute_source_);

  /* Replace 'out' attribute on function parameters with pass-by-reference. */
  replace_outvars(shd_builder_->glsl_compute_source_);

  /** Generate Compute shader stage. **/
  std::stringstream ss_compute;
  ss_compute << "#line 1 \"msl_wrapper_code\"\n";

  ss_compute << "#define GPU_ARB_shader_draw_parameters 1\n";
  if (bool(info->builtins_ & BuiltinBits::TEXTURE_ATOMIC) &&
      MTLBackend::get_capabilities().supports_texture_atomics)
  {
    ss_compute << ATOMIC_DEFINE_STR;
  }

  generate_specialization_constant_declarations(info, ss_compute);

#ifndef NDEBUG
  extract_global_scope_constants(shd_builder_->glsl_compute_source_, ss_compute);
#endif

  /* Conditional defines. */
  if (msl_iface.use_argument_buffer_for_samplers()) {
    ss_compute << "#define USE_ARGUMENT_BUFFER_FOR_SAMPLERS 1" << std::endl;
    ss_compute << "#define ARGUMENT_BUFFER_NUM_SAMPLERS "
               << msl_iface.max_sampler_index_for_stage(ShaderStage::COMPUTE) + 1 << std::endl;
  }

  /* Inject static workgroup sizes. */
  if (msl_iface.uses_gl_WorkGroupSize) {
  }

  /* Inject constant work group sizes. */
  if (msl_iface.uses_gl_WorkGroupSize) {
    ss_compute << "#define MTL_USE_WORKGROUP_SIZE 1" << std::endl;
    ss_compute << "#define MTL_WORKGROUP_SIZE_X " << info->compute_layout_.local_size_x
               << std::endl;
    ss_compute << "#define MTL_WORKGROUP_SIZE_Y "
               << ((info->compute_layout_.local_size_y != -1) ?
                       info->compute_layout_.local_size_y :
                       1)
               << std::endl;
    ss_compute << "#define MTL_WORKGROUP_SIZE_Z "
               << ((info->compute_layout_.local_size_y != -1) ?
                       info->compute_layout_.local_size_y :
                       1)
               << std::endl;
  }

  /* Inject common Metal header. */
  ss_compute << msl_iface.msl_patch_default_get() << std::endl << std::endl;

  /* Wrap entire GLSL source inside class to create
   * a scope within the class to enable use of global variables.
   * e.g. global access to attributes, uniforms, UBOs, textures etc; */
  ss_compute << "class " << get_stage_class_name(ShaderStage::COMPUTE) << " {" << std::endl;
  ss_compute << "public:" << std::endl;

  /* Generate Uniform data structs. */
  ss_compute << msl_iface.generate_msl_uniform_structs(ShaderStage::VERTEX);

  /* Add Texture members.
   * These members pack both a texture and a sampler into a single
   * struct, as both are needed within texture functions.
   * e.g. `_mtl_combined_image_sampler_2d<float, access::read>`
   * The exact typename is generated inside `get_msl_typestring_wrapper()`. */
  for (const MSLTextureResource &tex : msl_iface.texture_samplers) {
    if (bool(tex.stage & ShaderStage::COMPUTE)) {
      ss_compute << "\tthread " << tex.get_msl_typestring_wrapper(false) << ";" << std::endl;
    }
  }
  ss_compute << std::endl;

  /* Conditionally use global GL variables. */
  if (msl_iface.uses_gl_GlobalInvocationID) {
    ss_compute << "uint3 gl_GlobalInvocationID;" << std::endl;
  }
  if (msl_iface.uses_gl_WorkGroupID) {
    ss_compute << "uint3 gl_WorkGroupID;" << std::endl;
  }
  if (msl_iface.uses_gl_NumWorkGroups) {
    ss_compute << "uint3 gl_NumWorkGroups;" << std::endl;
  }
  if (msl_iface.uses_gl_LocalInvocationIndex) {
    ss_compute << "uint gl_LocalInvocationIndex;" << std::endl;
  }
  if (msl_iface.uses_gl_LocalInvocationID) {
    ss_compute << "uint3 gl_LocalInvocationID;" << std::endl;
  }

  /* Inject main GLSL source into output stream. */
  ss_compute << shd_builder_->glsl_compute_source_ << std::endl;

  /* Compute constructor for Shared memory blocks, as we must pass
   * local references from entry-point function scope into the class
   * instantiation. */
  ss_compute << get_stage_class_name(ShaderStage::COMPUTE) << "(";
  bool first = true;
  if (msl_iface.shared_memory_blocks.size() > 0) {
    for (const MSLSharedMemoryBlock &block : msl_iface.shared_memory_blocks) {
      if (!first) {
        ss_compute << ",";
      }
      if (block.is_array) {
        ss_compute << "TG " << block.type_name << " (&_" << block.varname << ")"
                   << block.array_decl;
      }
      else {
        ss_compute << "TG " << block.type_name << " &_" << block.varname;
      }
      ss_compute << std::endl;
      first = false;
    }
    ss_compute << ") : ";
    first = true;
    for (const MSLSharedMemoryBlock &block : msl_iface.shared_memory_blocks) {
      if (!first) {
        ss_compute << ",";
      }
      ss_compute << block.varname << "(_" << block.varname << ")";
      first = false;
    }
  }
  else {
    ss_compute << ") ";
  }
  ss_compute << "{ }" << std::endl;

  /* Class Closing Bracket to end shader global scope. */
  ss_compute << "};" << std::endl;

  /* Generate Vertex shader entry-point function containing resource bindings. */
  ss_compute << msl_iface.generate_msl_compute_entry_stub();

#ifndef NDEBUG
  /* In debug mode, we inject the name of the shader into the entry-point function
   * name, as these are what show up in the Xcode GPU debugger. */
  this->set_compute_function_name(
      [[NSString stringWithFormat:@"compute_function_entry_%s", this->name] retain]);
#else
  this->set_compute_function_name(@"compute_function_entry");
#endif

  /* DEBUG: Export source to file for manual verification. */
#if MTL_SHADER_DEBUG_EXPORT_SOURCE
  NSFileManager *sharedFM = [NSFileManager defaultManager];
  NSURL *app_bundle_url = [[NSBundle mainBundle] bundleURL];
  NSURL *shader_dir = [[app_bundle_url URLByDeletingLastPathComponent]
      URLByAppendingPathComponent:@"Shaders/"
                      isDirectory:YES];
  [sharedFM createDirectoryAtURL:shader_dir
      withIntermediateDirectories:YES
                       attributes:nil
                            error:nil];
  const char *path_cstr = [shader_dir fileSystemRepresentation];

  std::ofstream compute_fs;
  compute_fs.open(
      (std::string(path_cstr) + "/" + std::string(this->name) + "_GeneratedComputeShader.msl")
          .c_str());
  compute_fs << ss_compute.str();
  compute_fs.close();

  shader_debug_printf(
      "Compute Shader Saved to: %s\n",
      (std::string(path_cstr) + std::string(this->name) + "_GeneratedComputeShader.msl").c_str());
#endif

  NSString *msl_final_compute = [NSString stringWithUTF8String:ss_compute.str().c_str()];
  this->shader_compute_source_from_msl(msl_final_compute);

  /* Bake shader interface. */
  this->set_interface(msl_iface.bake_shader_interface(this->name));

  /* Compute dims. */
  this->compute_pso_common_state_.set_compute_workgroup_size(
      max_ii(info->compute_layout_.local_size_x, 1),
      max_ii(info->compute_layout_.local_size_y, 1),
      max_ii(info->compute_layout_.local_size_z, 1));

  /* Successfully completed GLSL to MSL translation. */
  return true;
}

constexpr size_t const_strlen(const char *str)
{
  return (*str == '\0') ? 0 : const_strlen(str + 1) + 1;
}

void MTLShader::prepare_ssbo_vertex_fetch_metadata()
{
  BLI_assert(use_ssbo_vertex_fetch_mode_);

  /* Cache global SSBO-vertex-fetch uniforms locations. */
  const ShaderInput *inp_prim_type = interface->uniform_get(UNIFORM_SSBO_INPUT_PRIM_TYPE_STR);
  const ShaderInput *inp_vert_count = interface->uniform_get(UNIFORM_SSBO_INPUT_VERT_COUNT_STR);
  const ShaderInput *inp_uses_indexed_rendering = interface->uniform_get(
      UNIFORM_SSBO_USES_INDEXED_RENDERING_STR);
  const ShaderInput *inp_uses_index_mode_u16 = interface->uniform_get(
      UNIFORM_SSBO_INDEX_MODE_U16_STR);

  this->uni_ssbo_input_prim_type_loc = (inp_prim_type != nullptr) ? inp_prim_type->location : -1;
  this->uni_ssbo_input_vert_count_loc = (inp_vert_count != nullptr) ? inp_vert_count->location :
                                                                      -1;
  this->uni_ssbo_uses_indexed_rendering = (inp_uses_indexed_rendering != nullptr) ?
                                              inp_uses_indexed_rendering->location :
                                              -1;
  this->uni_ssbo_uses_index_mode_u16 = (inp_uses_index_mode_u16 != nullptr) ?
                                           inp_uses_index_mode_u16->location :
                                           -1;

  BLI_assert_msg(this->uni_ssbo_input_prim_type_loc != -1,
                 "uni_ssbo_input_prim_type_loc uniform location invalid!");
  BLI_assert_msg(this->uni_ssbo_input_vert_count_loc != -1,
                 "uni_ssbo_input_vert_count_loc uniform location invalid!");
  BLI_assert_msg(this->uni_ssbo_uses_indexed_rendering != -1,
                 "uni_ssbo_uses_indexed_rendering uniform location invalid!");
  BLI_assert_msg(this->uni_ssbo_uses_index_mode_u16 != -1,
                 "uni_ssbo_uses_index_mode_u16 uniform location invalid!");

  /* Prepare SSBO-vertex-fetch attribute uniform location cache. */
  MTLShaderInterface *mtl_interface = this->get_interface();
  for (int i = 0; i < mtl_interface->get_total_attributes(); i++) {
    const MTLShaderInputAttribute &mtl_shader_attribute = mtl_interface->get_attribute(i);
    const char *attr_name = mtl_interface->get_name_at_offset(mtl_shader_attribute.name_offset);

    /* SSBO-vertex-fetch Attribute data is passed via uniforms. here we need to extract the uniform
     * address for each attribute, and we can cache it for later use. */
    ShaderSSBOAttributeBinding &cached_ssbo_attr = cached_ssbo_attribute_bindings_[i];
    cached_ssbo_attr.attribute_index = i;

    constexpr int len_UNIFORM_SSBO_STRIDE_STR = const_strlen(UNIFORM_SSBO_STRIDE_STR);
    constexpr int len_UNIFORM_SSBO_OFFSET_STR = const_strlen(UNIFORM_SSBO_OFFSET_STR);
    constexpr int len_UNIFORM_SSBO_FETCHMODE_STR = const_strlen(UNIFORM_SSBO_FETCHMODE_STR);
    constexpr int len_UNIFORM_SSBO_VBO_ID_STR = const_strlen(UNIFORM_SSBO_VBO_ID_STR);
    constexpr int len_UNIFORM_SSBO_TYPE_STR = const_strlen(UNIFORM_SSBO_TYPE_STR);

    char strattr_buf_stride[GPU_VERT_ATTR_MAX_LEN + len_UNIFORM_SSBO_STRIDE_STR + 1] =
        UNIFORM_SSBO_STRIDE_STR;
    char strattr_buf_offset[GPU_VERT_ATTR_MAX_LEN + len_UNIFORM_SSBO_OFFSET_STR + 1] =
        UNIFORM_SSBO_OFFSET_STR;
    char strattr_buf_fetchmode[GPU_VERT_ATTR_MAX_LEN + len_UNIFORM_SSBO_FETCHMODE_STR + 1] =
        UNIFORM_SSBO_FETCHMODE_STR;
    char strattr_buf_vbo_id[GPU_VERT_ATTR_MAX_LEN + len_UNIFORM_SSBO_VBO_ID_STR + 1] =
        UNIFORM_SSBO_VBO_ID_STR;
    char strattr_buf_type[GPU_VERT_ATTR_MAX_LEN + len_UNIFORM_SSBO_TYPE_STR + 1] =
        UNIFORM_SSBO_TYPE_STR;

    BLI_strncpy(
        &strattr_buf_stride[len_UNIFORM_SSBO_STRIDE_STR], attr_name, GPU_VERT_ATTR_MAX_LEN);
    BLI_strncpy(
        &strattr_buf_offset[len_UNIFORM_SSBO_OFFSET_STR], attr_name, GPU_VERT_ATTR_MAX_LEN);
    BLI_strncpy(
        &strattr_buf_fetchmode[len_UNIFORM_SSBO_FETCHMODE_STR], attr_name, GPU_VERT_ATTR_MAX_LEN);
    BLI_strncpy(
        &strattr_buf_vbo_id[len_UNIFORM_SSBO_VBO_ID_STR], attr_name, GPU_VERT_ATTR_MAX_LEN);
    BLI_strncpy(&strattr_buf_type[len_UNIFORM_SSBO_TYPE_STR], attr_name, GPU_VERT_ATTR_MAX_LEN);

    /* Fetch uniform locations and cache for fast access. */
    const ShaderInput *inp_unf_stride = mtl_interface->uniform_get(strattr_buf_stride);
    const ShaderInput *inp_unf_offset = mtl_interface->uniform_get(strattr_buf_offset);
    const ShaderInput *inp_unf_fetchmode = mtl_interface->uniform_get(strattr_buf_fetchmode);
    const ShaderInput *inp_unf_vbo_id = mtl_interface->uniform_get(strattr_buf_vbo_id);
    const ShaderInput *inp_unf_attr_type = mtl_interface->uniform_get(strattr_buf_type);

    BLI_assert(inp_unf_stride != nullptr);
    BLI_assert(inp_unf_offset != nullptr);
    BLI_assert(inp_unf_fetchmode != nullptr);
    BLI_assert(inp_unf_vbo_id != nullptr);
    BLI_assert(inp_unf_attr_type != nullptr);

    cached_ssbo_attr.uniform_stride = (inp_unf_stride != nullptr) ? inp_unf_stride->location : -1;
    cached_ssbo_attr.uniform_offset = (inp_unf_offset != nullptr) ? inp_unf_offset->location : -1;
    cached_ssbo_attr.uniform_fetchmode = (inp_unf_fetchmode != nullptr) ?
                                             inp_unf_fetchmode->location :
                                             -1;
    cached_ssbo_attr.uniform_vbo_id = (inp_unf_vbo_id != nullptr) ? inp_unf_vbo_id->location : -1;
    cached_ssbo_attr.uniform_attr_type = (inp_unf_attr_type != nullptr) ?
                                             inp_unf_attr_type->location :
                                             -1;

    BLI_assert(cached_ssbo_attr.uniform_offset != -1);
    BLI_assert(cached_ssbo_attr.uniform_stride != -1);
    BLI_assert(cached_ssbo_attr.uniform_fetchmode != -1);
    BLI_assert(cached_ssbo_attr.uniform_vbo_id != -1);
    BLI_assert(cached_ssbo_attr.uniform_attr_type != -1);
  }
}

void MSLGeneratorInterface::prepare_from_createinfo(const shader::ShaderCreateInfo *info)
{
  /** Assign info. */
  create_info_ = info;

  /** Prepare Uniforms. */
  for (const shader::ShaderCreateInfo::PushConst &push_constant : create_info_->push_constants_) {
    MSLUniform uniform(push_constant.type,
                       push_constant.name,
                       bool(push_constant.array_size > 1),
                       push_constant.array_size);
    uniforms.append(uniform);
  }

  /** Prepare Constants. */
  for (const auto &constant : create_info_->specialization_constants_) {
    constants.append(MSLConstant(constant.type, constant.name));
  }

  /* Prepare textures and uniform blocks.
   * Perform across both resource categories and extract both
   * texture samplers and image types. */

  /* NOTE: Metal requires Samplers and images to share slots. We will re-map these.
   * If `auto_resource_location_` is not used, then slot collision could occur and
   * this should be resolved in the original create-info.
   * UBOs and SSBOs also share the same bind table. */
  int texture_slot_id = 0;
  int ubo_buffer_slot_id_ = 0;
  int storage_buffer_slot_id_ = 0;

  uint max_storage_buffer_location = 0;

  /* Determine max sampler slot for image resource offset, when not using auto resource location,
   * as image resources cannot overlap sampler ranges. */
  int max_sampler_slot = 0;
  if (!create_info_->auto_resource_location_) {
    for (int i = 0; i < 2; i++) {
      const Vector<ShaderCreateInfo::Resource> &resources = (i == 0) ? info->pass_resources_ :
                                                                       info->batch_resources_;
      for (const ShaderCreateInfo::Resource &res : resources) {
        if (res.bind_type == shader::ShaderCreateInfo::Resource::BindType::SAMPLER) {
          max_sampler_slot = max_ii(res.slot, max_sampler_slot);
        }
      }
    }
  }

  for (int i = 0; i < 2; i++) {
    const Vector<ShaderCreateInfo::Resource> &resources = (i == 0) ? info->pass_resources_ :
                                                                     info->batch_resources_;
    for (const ShaderCreateInfo::Resource &res : resources) {
      /* TODO(Metal): Consider adding stage flags to textures in create info. */
      /* Handle sampler types. */
      switch (res.bind_type) {
        case shader::ShaderCreateInfo::Resource::BindType::SAMPLER: {

          /* Samplers to have access::sample by default. */
          MSLTextureSamplerAccess access = MSLTextureSamplerAccess::TEXTURE_ACCESS_SAMPLE;
          /* TextureBuffers must have read/write/read-write access pattern. */
          if (res.sampler.type == ImageType::FLOAT_BUFFER ||
              res.sampler.type == ImageType::INT_BUFFER ||
              res.sampler.type == ImageType::UINT_BUFFER)
          {
            access = MSLTextureSamplerAccess::TEXTURE_ACCESS_READ;
          }

          MSLTextureResource msl_tex;
          msl_tex.stage = ShaderStage::ANY;
          msl_tex.type = res.sampler.type;
          msl_tex.name = res.sampler.name;
          msl_tex.access = access;
          msl_tex.slot = texture_slot_id++;
          msl_tex.location = (create_info_->auto_resource_location_) ? msl_tex.slot : res.slot;
          msl_tex.is_texture_sampler = true;
          BLI_assert(msl_tex.slot < MTL_MAX_TEXTURE_SLOTS);

          texture_samplers.append(msl_tex);
          max_tex_bind_index = max_ii(max_tex_bind_index, msl_tex.slot);
        } break;

        case shader::ShaderCreateInfo::Resource::BindType::IMAGE: {
          /* Flatten qualifier flags into final access state. */
          MSLTextureSamplerAccess access;
          if (bool(res.image.qualifiers & Qualifier::READ_WRITE)) {
            access = MSLTextureSamplerAccess::TEXTURE_ACCESS_READWRITE;
          }
          else if (bool(res.image.qualifiers & Qualifier::WRITE)) {
            access = MSLTextureSamplerAccess::TEXTURE_ACCESS_WRITE;
          }
          else {
            access = MSLTextureSamplerAccess::TEXTURE_ACCESS_READ;
          }

          /* Writeable image targets only assigned to Fragment and compute shaders. */
          MSLTextureResource msl_image;
          msl_image.stage = ShaderStage::FRAGMENT | ShaderStage::COMPUTE;
          msl_image.type = res.image.type;
          msl_image.name = res.image.name;
          msl_image.access = access;
          msl_image.slot = texture_slot_id++;
          msl_image.location = (create_info_->auto_resource_location_) ? msl_image.slot : res.slot;
          msl_image.is_texture_sampler = false;
          BLI_assert(msl_image.slot < MTL_MAX_TEXTURE_SLOTS);

          texture_samplers.append(msl_image);
          max_tex_bind_index = max_ii(max_tex_bind_index, msl_image.slot);
        } break;

        case shader::ShaderCreateInfo::Resource::BindType::UNIFORM_BUFFER: {
          MSLBufferBlock ubo;
          BLI_assert(res.uniformbuf.type_name.size() > 0);
          BLI_assert(res.uniformbuf.name.size() > 0);
          int64_t array_offset = res.uniformbuf.name.find_first_of("[");

          /* We maintain two bind indices. "Slot" refers to the storage index buffer(N) in which
           * we will bind the resource. "Location" refers to the explicit bind index specified
           * in ShaderCreateInfo.
           * NOTE: ubo.slot is offset by one, as first UBO slot is reserved for push constant data.
           */
          ubo.slot = 1 + (ubo_buffer_slot_id_++);
          ubo.location = (create_info_->auto_resource_location_) ? ubo.slot : res.slot;

          BLI_assert(ubo.location >= 0 && ubo.location < MTL_MAX_BUFFER_BINDINGS);

          ubo.qualifiers = shader::Qualifier::READ;
          ubo.type_name = res.uniformbuf.type_name;
          ubo.is_texture_buffer = false;
          ubo.is_array = (array_offset > -1);
          if (ubo.is_array) {
            /* If is array UBO, strip out array tag from name. */
            StringRef name_no_array = StringRef(res.uniformbuf.name.c_str(), array_offset);
            ubo.name = name_no_array;
          }
          else {
            ubo.name = res.uniformbuf.name;
          }
          ubo.stage = ShaderStage::ANY;
          uniform_blocks.append(ubo);
        } break;

        case shader::ShaderCreateInfo::Resource::BindType::STORAGE_BUFFER: {
          MSLBufferBlock ssbo;
          BLI_assert(res.storagebuf.type_name.size() > 0);
          BLI_assert(res.storagebuf.name.size() > 0);
          int64_t array_offset = res.storagebuf.name.find_first_of("[");

          /* We maintain two bind indices. "Slot" refers to the storage index buffer(N) in which
           * we will bind the resource. "Location" refers to the explicit bind index specified
           * in ShaderCreateInfo. */
          ssbo.slot = storage_buffer_slot_id_++;
          ssbo.location = (create_info_->auto_resource_location_) ? ssbo.slot : res.slot;

          max_storage_buffer_location = max_uu(max_storage_buffer_location, ssbo.location);

          BLI_assert(ssbo.location >= 0 && ssbo.location < MTL_MAX_BUFFER_BINDINGS);

          ssbo.qualifiers = res.storagebuf.qualifiers;
          ssbo.type_name = res.storagebuf.type_name;
          ssbo.is_texture_buffer = false;
          ssbo.is_array = (array_offset > -1);
          if (ssbo.is_array) {
            /* If is array UBO, strip out array tag from name. */
            StringRef name_no_array = StringRef(res.storagebuf.name.c_str(), array_offset);
            ssbo.name = name_no_array;
          }
          else {
            ssbo.name = res.storagebuf.name;
          }
          ssbo.stage = ShaderStage::ANY;
          storage_blocks.append(ssbo);
        } break;
      }
    }
  }

  /* For texture atomic fallback support, bind texture source buffers and data buffer as storage
   * blocks. */
  if (!MTLBackend::get_capabilities().supports_texture_atomics) {
    uint atomic_fallback_buffer_count = 0;
    for (MSLTextureResource &tex : texture_samplers) {
      if (ELEM(tex.type,
               ImageType::UINT_2D_ATOMIC,
               ImageType::UINT_2D_ARRAY_ATOMIC,
               ImageType::UINT_3D_ATOMIC,
               ImageType::INT_2D_ATOMIC,
               ImageType::INT_2D_ARRAY_ATOMIC,
               ImageType::INT_3D_ATOMIC))
      {
        /* Add storage-buffer bind-point. */
        MSLBufferBlock ssbo;

        /* We maintain two bind indices. "Slot" refers to the storage index buffer(N) in which
         * we will bind the resource. "Location" refers to the explicit bind index specified
         * in ShaderCreateInfo.
         * NOTE: For texture buffers, we will accumulate these after all other storage buffers.
         */
        ssbo.slot = storage_buffer_slot_id_++;
        ssbo.location = max_storage_buffer_location + 1 + atomic_fallback_buffer_count;

        /* Flag atomic fallback buffer id and location.
         * ID is used to determine order for accessing parameters, while
         * location is used to extract the explicit bind point for the buffer. */
        tex.atomic_fallback_buffer_ssbo_id = storage_blocks.size();

        BLI_assert(ssbo.location >= 0 && ssbo.location < MTL_MAX_BUFFER_BINDINGS);

        /* Qualifier should be read write and type is either uint or int. */
        ssbo.qualifiers = Qualifier::READ_WRITE;
        ssbo.type_name = tex.get_msl_return_type_str();
        ssbo.is_array = false;
        ssbo.name = tex.name + "_storagebuf";
        ssbo.stage = ShaderStage::ANY;
        ssbo.is_texture_buffer = true;
        storage_blocks.append(ssbo);

        /* Add uniform for metadata. */
        MSLUniform uniform(shader::Type::IVEC4, tex.name + "_metadata", false, 1);
        uniforms.append(uniform);

        atomic_fallback_buffer_count++;
      }
    }
  }

  /* Assign maximum buffer. */
  max_buffer_slot = storage_buffer_slot_id_ + ubo_buffer_slot_id_ + 1;

  /** Vertex Inputs. */
  bool all_attr_location_assigned = true;
  for (const ShaderCreateInfo::VertIn &attr : info->vertex_inputs_) {

    /* Validate input. */
    BLI_assert(attr.name.size() > 0);

    /* NOTE(Metal): Input attributes may not have a location specified.
     * unset locations are resolved during: `resolve_input_attribute_locations`. */
    MSLVertexInputAttribute msl_attr;
    bool attr_location_assigned = (attr.index >= 0);
    all_attr_location_assigned = all_attr_location_assigned && attr_location_assigned;
    msl_attr.layout_location = attr_location_assigned ? attr.index : -1;
    msl_attr.type = attr.type;
    msl_attr.name = attr.name;
    vertex_input_attributes.append(msl_attr);
  }

  /* Ensure all attributes are assigned a location. */
  if (!all_attr_location_assigned) {
    this->resolve_input_attribute_locations();
  }

  /** Fragment outputs. */
  for (const shader::ShaderCreateInfo::FragOut &frag_out : create_info_->fragment_outputs_) {

    /* Validate input. */
    BLI_assert(frag_out.name.size() > 0);
    BLI_assert(frag_out.index >= 0);

    /* Populate MSLGenerator attribute. */
    MSLFragmentOutputAttribute mtl_frag_out;
    mtl_frag_out.layout_location = frag_out.index;
    mtl_frag_out.layout_index = (frag_out.blend != DualBlend::NONE) ?
                                    ((frag_out.blend == DualBlend::SRC_0) ? 0 : 1) :
                                    -1;
    mtl_frag_out.type = frag_out.type;
    mtl_frag_out.name = frag_out.name;
    mtl_frag_out.raster_order_group = frag_out.raster_order_group;

    fragment_outputs.append(mtl_frag_out);
  }

  /* Fragment tile inputs. */
  for (const shader::ShaderCreateInfo::SubpassIn &frag_tile_in : create_info_->subpass_inputs_) {

    /* Validate input. */
    BLI_assert(frag_tile_in.name.size() > 0);
    BLI_assert(frag_tile_in.index >= 0);

    /* Populate MSLGenerator attribute. */
    MSLFragmentTileInputAttribute mtl_frag_in;
    mtl_frag_in.layout_location = frag_tile_in.index;
    mtl_frag_in.layout_index = (frag_tile_in.blend != DualBlend::NONE) ?
                                   ((frag_tile_in.blend == DualBlend::SRC_0) ? 0 : 1) :
                                   -1;
    mtl_frag_in.type = frag_tile_in.type;
    mtl_frag_in.name = frag_tile_in.name;
    mtl_frag_in.raster_order_group = frag_tile_in.raster_order_group;

    fragment_tile_inputs.append(mtl_frag_in);
  }

  /* Transform feedback. */
  uses_transform_feedback = (create_info_->tf_type_ != GPU_SHADER_TFB_NONE) &&
                            (create_info_->tf_names_.size() > 0);
}

bool MSLGeneratorInterface::use_argument_buffer_for_samplers() const
{
  /* We can only use argument buffers IF highest sampler index exceeds static limit of 16,
   * AND we can support more samplers with an argument buffer. */
  bool use_argument_buffer = (texture_samplers.size() >= 15 || max_tex_bind_index >= 14) &&
                             GPU_max_samplers() > 15;

#ifndef NDEBUG
  /* Due to explicit bind location support, we may be below the sampler limit, but forced to offset
   * bindings due to the range being high. Introduce debug check here to issue warning. In these
   * cases, if explicit bind location support is not required, best to use auto_resource_location
   * to optimize bind point packing. */
  if (use_argument_buffer && texture_samplers.size() < 15) {
    MTL_LOG_WARNING(
        "Compiled Shader '%s' is falling back to bindless via argument buffers due to having a "
        "texture sampler of Index: %u Which exceeds the limit of 15+1. However shader only uses "
        "%d textures. Consider optimising bind points with .auto_resource_location(true).",
        parent_shader_.name_get(),
        max_tex_bind_index,
        (int)texture_samplers.size());
  }
#endif

  return use_argument_buffer;
}

uint32_t MSLGeneratorInterface::num_samplers_for_stage(ShaderStage /*stage*/) const
{
  /* NOTE: Sampler bindings and argument buffer shared across stages,
   * in case stages share texture/sampler bindings. */
  return texture_samplers.size();
}

uint32_t MSLGeneratorInterface::max_sampler_index_for_stage(ShaderStage /*stage*/) const
{
  /* NOTE: Sampler bindings and argument buffer shared across stages,
   * in case stages share texture/sampler bindings. */
  return max_tex_bind_index;
}

uint32_t MSLGeneratorInterface::get_sampler_argument_buffer_bind_index(ShaderStage stage)
{
  /* Note: Shader stage must be a singular index. Compound shader masks are not valid for this
   * function. */
  BLI_assert(stage == ShaderStage::VERTEX || stage == ShaderStage::FRAGMENT ||
             stage == ShaderStage::COMPUTE);
  if (sampler_argument_buffer_bind_index[get_shader_stage_index(stage)] >= 0) {
    return sampler_argument_buffer_bind_index[get_shader_stage_index(stage)];
  }

  /* Sampler argument buffer to follow UBOs and PushConstantBlock. */
  sampler_argument_buffer_bind_index[get_shader_stage_index(stage)] = (max_buffer_slot + 1);
  return sampler_argument_buffer_bind_index[get_shader_stage_index(stage)];
}

void MSLGeneratorInterface::prepare_ssbo_vertex_fetch_uniforms()
{
  BLI_assert(this->uses_ssbo_vertex_fetch_mode);

  /* Add Special Uniforms for SSBO vertex fetch mode. */
  this->uniforms.append(MSLUniform(Type::INT, UNIFORM_SSBO_INPUT_PRIM_TYPE_STR, false));
  this->uniforms.append(MSLUniform(Type::INT, UNIFORM_SSBO_INPUT_VERT_COUNT_STR, false));
  this->uniforms.append(MSLUniform(Type::INT, UNIFORM_SSBO_USES_INDEXED_RENDERING_STR, false));
  this->uniforms.append(MSLUniform(Type::INT, UNIFORM_SSBO_INDEX_MODE_U16_STR, false));

  for (const MSLVertexInputAttribute &attr : this->vertex_input_attributes) {
    const std::string &uname = attr.name;
    this->uniforms.append(MSLUniform(Type::INT, UNIFORM_SSBO_STRIDE_STR + uname, false));
    this->uniforms.append(MSLUniform(Type::INT, UNIFORM_SSBO_OFFSET_STR + uname, false));
    this->uniforms.append(MSLUniform(Type::INT, UNIFORM_SSBO_FETCHMODE_STR + uname, false));
    this->uniforms.append(MSLUniform(Type::INT, UNIFORM_SSBO_VBO_ID_STR + uname, false));
    this->uniforms.append(MSLUniform(Type::INT, UNIFORM_SSBO_TYPE_STR + uname, false));
  }
}

std::string MSLGeneratorInterface::generate_msl_vertex_entry_stub()
{
  static const char *shader_stage_inst_name = get_shader_stage_instance_name(ShaderStage::VERTEX);

  std::stringstream out;
  out << std::endl << "/*** AUTO-GENERATED MSL VERETX SHADER STUB. ***/" << std::endl;

  /* Un-define texture defines from main source - avoid conflict with MSL texture. */
  out << "#undef texture" << std::endl;
  out << "#undef textureLod" << std::endl;

  /* Disable special case for booleans being treated as ints in GLSL. */
  out << "#undef bool" << std::endl;

  /* Un-define uniform mappings to avoid name collisions. */
  out << generate_msl_uniform_undefs(ShaderStage::VERTEX);

  /* Generate function entry point signature w/ resource bindings and inputs. */
  out << "vertex ";
  if (this->uses_transform_feedback) {
    out << "void ";
  }
  else {
    out << get_stage_class_name(ShaderStage::VERTEX) << "::VertexOut ";
  }
#ifndef NDEBUG
  out << "vertex_function_entry_" << parent_shader_.name_get() << "(\n\t";
#else
  out << "vertex_function_entry(\n\t";
#endif

  out << this->generate_msl_vertex_inputs_string();
  out << ") {" << std::endl << std::endl;
  out << "\t" << get_stage_class_name(ShaderStage::VERTEX) << "::VertexOut output;" << std::endl
      << "\t" << get_stage_class_name(ShaderStage::VERTEX) << " " << shader_stage_inst_name << ";"
      << std::endl;

  /* Copy Vertex Globals. */
  if (this->uses_gl_VertexID) {
    out << shader_stage_inst_name << ".gl_VertexID = gl_VertexID;" << std::endl;
  }
  if (this->uses_gl_InstanceID) {
    out << shader_stage_inst_name << ".gl_InstanceID = gl_InstanceID-gl_BaseInstanceARB;"
        << std::endl;
  }
  if (this->uses_gl_BaseInstanceARB) {
    out << shader_stage_inst_name << ".gl_BaseInstanceARB = gl_BaseInstanceARB;" << std::endl;
  }

  /* Copy vertex attributes into local variables. */
  out << this->generate_msl_vertex_attribute_input_population();

  /* Populate Uniforms and uniform blocks. */
  out << this->generate_msl_texture_vars(ShaderStage::VERTEX);
  out << this->generate_msl_global_uniform_population(ShaderStage::VERTEX);
  out << this->generate_msl_uniform_block_population(ShaderStage::VERTEX);

  /* Execute original 'main' function within class scope. */
  out << "\t/* Execute Vertex main function */\t" << std::endl
      << "\t" << shader_stage_inst_name << ".main();" << std::endl
      << std::endl;

  /* Populate Output values. */
  out << this->generate_msl_vertex_output_population();

  /* Final point size,
   * This is only compiled if the `MTL_global_pointsize` is specified
   * as a function specialization in the PSO. This is restricted to
   * point primitive types. */
  out << "if(is_function_constant_defined(MTL_global_pointsize)){ output.pointsize = "
         "(MTL_global_pointsize > 0.0)?MTL_global_pointsize:output.pointsize; }"
      << std::endl;

  /* Populate transform feedback buffer. */
  if (this->uses_transform_feedback) {
    out << this->generate_msl_vertex_output_tf_population();
  }
  else {
    out << "\treturn output;" << std::endl;
  }
  out << "}";
  return out.str();
}

std::string MSLGeneratorInterface::generate_msl_fragment_entry_stub()
{
  static const char *shader_stage_inst_name = get_shader_stage_instance_name(
      ShaderStage::FRAGMENT);
  std::stringstream out;
  out << std::endl << "/*** AUTO-GENERATED MSL FRAGMENT SHADER STUB. ***/" << std::endl;

  /* Undefine texture defines from main source - avoid conflict with MSL texture. */
  out << "#undef texture" << std::endl;
  out << "#undef textureLod" << std::endl;

  /* Disable special case for booleans being treated as integers in GLSL. */
  out << "#undef bool" << std::endl;

  /* Undefine uniform mappings to avoid name collisions. */
  out << generate_msl_uniform_undefs(ShaderStage::FRAGMENT);

  /* Early fragment tests. */
  if (uses_early_fragment_test) {
    out << "[[early_fragment_tests]]" << std::endl;
  }

  /* Generate function entry point signature w/ resource bindings and inputs. */
#ifndef NDEBUG
  out << "fragment " << get_stage_class_name(ShaderStage::FRAGMENT)
      << "::" FRAGMENT_OUT_STRUCT_NAME " fragment_function_entry_" << parent_shader_.name_get()
      << "(\n\t";
#else
  out << "fragment " << get_stage_class_name(ShaderStage::FRAGMENT)
      << "::" FRAGMENT_OUT_STRUCT_NAME " fragment_function_entry(\n\t";
#endif
  out << this->generate_msl_fragment_inputs_string();
  out << ") {" << std::endl << std::endl;
  out << "\t" << get_stage_class_name(ShaderStage::FRAGMENT)
      << "::" FRAGMENT_OUT_STRUCT_NAME " output;" << std::endl
      << "\t" << get_stage_class_name(ShaderStage::FRAGMENT) << " " << shader_stage_inst_name
      << ";" << std::endl;

  /* Copy Fragment Globals. */
  if (this->uses_gl_PointCoord) {
    out << shader_stage_inst_name << ".gl_PointCoord = gl_PointCoord;" << std::endl;
  }
  if (this->uses_gl_FrontFacing) {
    out << shader_stage_inst_name << ".gl_FrontFacing = gl_FrontFacing;" << std::endl;
  }
  if (this->uses_gl_PrimitiveID) {
    out << "fragment_shader_instance.gl_PrimitiveID = gl_PrimitiveID;" << std::endl;
  }

  /* Copy vertex attributes into local variable.s */
  out << this->generate_msl_fragment_input_population();

  /* Barycentrics. */
  if (this->uses_barycentrics) {
    out << shader_stage_inst_name << ".gpu_BaryCoord = mtl_barycentric_coord.xyz;" << std::endl;
  }

  /* Populate Uniforms and uniform blocks. */
  out << this->generate_msl_texture_vars(ShaderStage::FRAGMENT);
  out << this->generate_msl_global_uniform_population(ShaderStage::FRAGMENT);
  out << this->generate_msl_uniform_block_population(ShaderStage::FRAGMENT);

  /* Populate fragment tile-in members. */
  if (this->fragment_tile_inputs.size() > 0) {
    out << this->generate_msl_fragment_tile_input_population();
  }

  /* Execute original 'main' function within class scope. */
  out << "\t/* Execute Fragment main function */\t" << std::endl
      << "\t" << shader_stage_inst_name << ".main();" << std::endl
      << std::endl;

  /* Populate Output values. */
  out << this->generate_msl_fragment_output_population();
  out << "  return output;" << std::endl << "}";

  return out.str();
}

std::string MSLGeneratorInterface::generate_msl_compute_entry_stub()
{
  static const char *shader_stage_inst_name = get_shader_stage_instance_name(ShaderStage::COMPUTE);
  std::stringstream out;
  out << std::endl << "/*** AUTO-GENERATED MSL COMPUTE SHADER STUB. ***/" << std::endl;

  /* Un-define texture defines from main source - avoid conflict with MSL texture. */
  out << "#undef texture" << std::endl;
  out << "#undef textureLod" << std::endl;

  /* Disable special case for booleans being treated as ints in GLSL. */
  out << "#undef bool" << std::endl;

  /* Un-define uniform mappings to avoid name collisions. */
  out << generate_msl_uniform_undefs(ShaderStage::COMPUTE);

  /* Generate function entry point signature w/ resource bindings and inputs. */
  out << "kernel void ";
#ifndef NDEBUG
  out << "compute_function_entry_" << parent_shader_.name_get() << "(\n\t";
#else
  out << "compute_function_entry(\n\t";
#endif

  out << this->generate_msl_compute_inputs_string();
  out << ") {" << std::endl << std::endl;
  /* Generate Compute shader instance constructor. If shared memory blocks are used,
   * these must be declared and then passed into the constructor. */
  std::string stage_instance_constructor = "";
  bool first = true;
  if (shared_memory_blocks.size() > 0) {
    stage_instance_constructor += "(";
    for (const MSLSharedMemoryBlock &block : shared_memory_blocks) {
      if (block.is_array) {
        out << "TG " << block.type_name << " " << block.varname << block.array_decl << ";";
      }
      else {
        out << "TG " << block.type_name << " " << block.varname << ";";
      }
      stage_instance_constructor += ((!first) ? "," : "") + block.varname;
      first = false;

      out << std::endl;
    }
    stage_instance_constructor += ")";
  }
  out << "\t" << get_stage_class_name(ShaderStage::COMPUTE) << " " << shader_stage_inst_name
      << stage_instance_constructor << ";" << std::endl;

  /* Copy global variables. */
  /* Entry point parameters for gl Globals. */
  if (this->uses_gl_GlobalInvocationID) {
    out << shader_stage_inst_name << ".gl_GlobalInvocationID = gl_GlobalInvocationID;"
        << std::endl;
  }
  if (this->uses_gl_WorkGroupID) {
    out << shader_stage_inst_name << ".gl_WorkGroupID = gl_WorkGroupID;" << std::endl;
  }
  if (this->uses_gl_NumWorkGroups) {
    out << shader_stage_inst_name << ".gl_NumWorkGroups = gl_NumWorkGroups;" << std::endl;
  }
  if (this->uses_gl_LocalInvocationIndex) {
    out << shader_stage_inst_name << ".gl_LocalInvocationIndex = gl_LocalInvocationIndex;"
        << std::endl;
  }
  if (this->uses_gl_LocalInvocationID) {
    out << shader_stage_inst_name << ".gl_LocalInvocationID = gl_LocalInvocationID;" << std::endl;
  }

  /* Populate Uniforms and uniform blocks. */
  out << this->generate_msl_texture_vars(ShaderStage::COMPUTE);
  out << this->generate_msl_global_uniform_population(ShaderStage::COMPUTE);
  out << this->generate_msl_uniform_block_population(ShaderStage::COMPUTE);

  /* Execute original 'main' function within class scope. */
  out << "\t/* Execute Compute main function */\t" << std::endl
      << "\t" << shader_stage_inst_name << ".main();" << std::endl
      << std::endl;

  out << "}";
  return out.str();
}

/* If first parameter in function signature, do not print out a comma.
 * Update first parameter flag to false for future invocations. */
static char parameter_delimiter(bool &is_first_parameter)
{
  if (is_first_parameter) {
    is_first_parameter = false;
    return ' ';
  }
  return ',';
}

void MSLGeneratorInterface::generate_msl_textures_input_string(std::stringstream &out,
                                                               ShaderStage stage,
                                                               bool &is_first_parameter)
{
  /* Note: Shader stage must be specified as the singular stage index for which the input
   * is generating. Compound stages are not valid inputs. */
  BLI_assert(stage == ShaderStage::VERTEX || stage == ShaderStage::FRAGMENT ||
             stage == ShaderStage::COMPUTE);
  /* Generate texture signatures for textures used by this stage. */
  BLI_assert(this->texture_samplers.size() <= GPU_max_textures_vert());
  for (const MSLTextureResource &tex : this->texture_samplers) {
    if (bool(tex.stage & stage)) {
      out << parameter_delimiter(is_first_parameter) << "\n\t" << tex.get_msl_typestring(false)
          << " [[texture(" << tex.slot << ")]]";
    }
  }

  /* Generate sampler signatures. */
  /* NOTE: Currently textures and samplers share indices across shading stages, so the limit is
   * shared.
   * If we exceed the hardware-supported limit, then follow a bind-less model using argument
   * buffers. */
  if (this->use_argument_buffer_for_samplers()) {
    out << parameter_delimiter(is_first_parameter)
        << "\n\tconstant SStruct& samplers [[buffer(MTL_uniform_buffer_base_index+"
        << (this->get_sampler_argument_buffer_bind_index(stage)) << ")]]";
  }
  else {
    /* Maximum Limit of samplers defined in the function argument table is
     * `MTL_MAX_DEFAULT_SAMPLERS=16`. */
    BLI_assert(this->texture_samplers.size() <= MTL_MAX_DEFAULT_SAMPLERS);
    for (const MSLTextureResource &tex : this->texture_samplers) {
      if (bool(tex.stage & stage)) {
        out << parameter_delimiter(is_first_parameter) << "\n\tsampler " << tex.name
            << "_sampler [[sampler(" << tex.slot << ")]]";
      }
    }

    /* Fallback. */
    if (this->texture_samplers.size() > 16) {
      shader_debug_printf(
          "[Metal] Warning: Shader exceeds limit of %u samplers on current hardware\n",
          MTL_MAX_DEFAULT_SAMPLERS);
    }
  }
}

void MSLGeneratorInterface::generate_msl_uniforms_input_string(std::stringstream &out,
                                                               ShaderStage stage,
                                                               bool &is_first_parameter)
{
  for (const MSLBufferBlock &ubo : this->uniform_blocks) {
    if (bool(ubo.stage & stage)) {
      /* For literal/existing global types, we do not need the class name-space accessor. */
      out << parameter_delimiter(is_first_parameter) << "\n\tconstant ";
      if (!is_builtin_type(ubo.type_name)) {
        out << get_stage_class_name(stage) << "::";
      }
      /* #UniformBuffer bind indices start at `MTL_uniform_buffer_base_index + 1`, as
       * MTL_uniform_buffer_base_index is reserved for the #PushConstantBlock (push constants).
       * MTL_uniform_buffer_base_index is an offset depending on the number of unique VBOs
       * bound for the current PSO specialization. */
      out << ubo.type_name << "* " << ubo.name << "[[buffer(MTL_uniform_buffer_base_index+"
          << ubo.slot << ")]]";
    }
  }

  /* Storage buffers. */
  for (const MSLBufferBlock &ssbo : this->storage_blocks) {
    if (bool(ssbo.stage & stage)) {
      /* For literal/existing global types, we do not need the class name-space accessor. */
      bool writeable = (ssbo.qualifiers & shader::Qualifier::WRITE) == shader::Qualifier::WRITE;
      const char *memory_scope = ((writeable) ? "device " : "constant ");
      out << parameter_delimiter(is_first_parameter) << "\n\t" << memory_scope;
      if (!is_builtin_type(ssbo.type_name)) {
        out << get_stage_class_name(stage) << "::";
      }
      /* #StorageBuffer bind indices start at `MTL_storage_buffer_base_index`.
       * MTL_storage_buffer_base_index follows immediately after all uniform blocks.
       * such that MTL_storage_buffer_base_index = MTL_uniform_buffer_base_index +
       * uniform_blocks.size() + 1. Where the additional buffer is reserved for the
       * #PushConstantBlock (push constants). */
      out << ssbo.type_name << "* " << ssbo.name << "[[buffer(MTL_storage_buffer_base_index+"
          << (ssbo.slot) << ")]]";
    }
  }
}

std::string MSLGeneratorInterface::generate_msl_vertex_inputs_string()
{
  std::stringstream out;
  bool is_first_parameter = true;

  if (this->uses_ssbo_vertex_fetch_mode) {
    /* Vertex Buffers bound as raw buffers. */
    for (int i = 0; i < MTL_SSBO_VERTEX_FETCH_MAX_VBOS; i++) {
      out << parameter_delimiter(is_first_parameter) << "\tconstant uchar* MTL_VERTEX_DATA_" << i
          << " [[buffer(" << i << ")]]\n";
    }
    out << parameter_delimiter(is_first_parameter)
        << "\tconstant ushort* MTL_INDEX_DATA[[buffer(MTL_SSBO_VERTEX_FETCH_IBO_INDEX)]]";
  }
  else {
    if (this->vertex_input_attributes.size() > 0) {
      /* Vertex Buffers use input assembly. */
      out << get_stage_class_name(ShaderStage::VERTEX) << "::VertexIn v_in [[stage_in]]";
      is_first_parameter = false;
    }
  }

  if (this->uniforms.size() > 0) {
    out << parameter_delimiter(is_first_parameter) << "\n\tconstant "
        << get_stage_class_name(ShaderStage::VERTEX)
        << "::PushConstantBlock* uniforms[[buffer(MTL_uniform_buffer_base_index)]]";
    is_first_parameter = false;
  }

  this->generate_msl_uniforms_input_string(out, ShaderStage::VERTEX, is_first_parameter);

  /* Transform feedback buffer binding. */
  if (this->uses_transform_feedback) {
    out << parameter_delimiter(is_first_parameter) << "\n\tdevice "
        << get_stage_class_name(ShaderStage::VERTEX)
        << "::VertexOut_TF* "
           "transform_feedback_results[[buffer(MTL_transform_feedback_buffer_index)]]";
  }

  /* Generate texture signatures. */
  this->generate_msl_textures_input_string(out, ShaderStage::VERTEX, is_first_parameter);

  /* Entry point parameters for gl Globals. */
  if (this->uses_gl_VertexID) {
    out << parameter_delimiter(is_first_parameter)
        << "\n\tconst uint32_t gl_VertexID [[vertex_id]]";
  }
  if (this->uses_gl_InstanceID) {
    out << parameter_delimiter(is_first_parameter)
        << "\n\tconst uint32_t gl_InstanceID [[instance_id]]";
  }
  if (this->uses_gl_BaseInstanceARB) {
    out << parameter_delimiter(is_first_parameter)
        << "\n\tconst uint32_t gl_BaseInstanceARB [[base_instance]]";
  }
  return out.str();
}

std::string MSLGeneratorInterface::generate_msl_fragment_inputs_string()
{
  bool is_first_parameter = true;
  std::stringstream out;
  out << parameter_delimiter(is_first_parameter) << get_stage_class_name(ShaderStage::FRAGMENT)
      << "::VertexOut v_in [[stage_in]]";

  if (this->uniforms.size() > 0) {
    out << parameter_delimiter(is_first_parameter) << "\n\tconstant "
        << get_stage_class_name(ShaderStage::FRAGMENT)
        << "::PushConstantBlock* uniforms[[buffer(MTL_uniform_buffer_base_index)]]";
  }

  this->generate_msl_uniforms_input_string(out, ShaderStage::FRAGMENT, is_first_parameter);

  /* Generate texture signatures. */
  this->generate_msl_textures_input_string(out, ShaderStage::FRAGMENT, is_first_parameter);

  if (this->uses_gl_PointCoord) {
    out << parameter_delimiter(is_first_parameter)
        << "\n\tconst float2 gl_PointCoord [[point_coord]]";
  }
  if (this->uses_gl_FrontFacing) {
    out << parameter_delimiter(is_first_parameter)
        << "\n\tconst bool gl_FrontFacing [[front_facing]]";
  }
  if (this->uses_gl_PrimitiveID) {
    out << parameter_delimiter(is_first_parameter)
        << "\n\tconst uint gl_PrimitiveID [[primitive_id]]";
  }

  /* Barycentrics. */
  if (this->uses_barycentrics) {
    out << parameter_delimiter(is_first_parameter)
        << "\n\tconst float3 mtl_barycentric_coord [[barycentric_coord]]";
  }

  /* Fragment tile-inputs. */
  if (this->fragment_tile_inputs.size() > 0) {
    out << parameter_delimiter(is_first_parameter) << "\n\t"
        << get_stage_class_name(ShaderStage::FRAGMENT)
        << "::" FRAGMENT_TILE_IN_STRUCT_NAME " fragment_tile_in";
  }
  return out.str();
}

std::string MSLGeneratorInterface::generate_msl_compute_inputs_string()
{
  bool is_first_parameter = true;
  std::stringstream out;
  if (this->uniforms.size() > 0) {
    out << parameter_delimiter(is_first_parameter) << "constant "
        << get_stage_class_name(ShaderStage::COMPUTE)
        << "::PushConstantBlock* uniforms[[buffer(MTL_uniform_buffer_base_index)]]";
  }

  this->generate_msl_uniforms_input_string(out, ShaderStage::COMPUTE, is_first_parameter);

  /* Generate texture signatures. */
  this->generate_msl_textures_input_string(out, ShaderStage::COMPUTE, is_first_parameter);

  /* Entry point parameters for gl Globals. */
  if (this->uses_gl_GlobalInvocationID) {
    out << parameter_delimiter(is_first_parameter)
        << "\n\tconst uint3 gl_GlobalInvocationID [[thread_position_in_grid]]";
  }
  if (this->uses_gl_WorkGroupID) {
    out << parameter_delimiter(is_first_parameter)
        << "\n\tconst uint3 gl_WorkGroupID [[threadgroup_position_in_grid]]";
  }
  if (this->uses_gl_NumWorkGroups) {
    out << parameter_delimiter(is_first_parameter)
        << "\n\tconst uint3 gl_NumWorkGroups [[threadgroups_per_grid]]";
  }
  if (this->uses_gl_LocalInvocationIndex) {
    out << parameter_delimiter(is_first_parameter)
        << "\n\tconst uint gl_LocalInvocationIndex [[thread_index_in_threadgroup]]";
  }
  if (this->uses_gl_LocalInvocationID) {
    out << parameter_delimiter(is_first_parameter)
        << "\n\tconst uint3 gl_LocalInvocationID [[thread_position_in_threadgroup]]";
  }

  return out.str();
}

std::string MSLGeneratorInterface::generate_msl_uniform_structs(ShaderStage shader_stage)
{
  /* Only generate PushConstantBlock if we have uniforms. */
  if (this->uniforms.size() == 0) {
    return "";
  }
  BLI_assert(shader_stage == ShaderStage::VERTEX || shader_stage == ShaderStage::FRAGMENT);
  UNUSED_VARS_NDEBUG(shader_stage);
  std::stringstream out;

  /* Common Uniforms. */
  out << "typedef struct {" << std::endl;

  for (const MSLUniform &uniform : this->uniforms) {
    if (uniform.is_array) {
      out << "\t" << to_string(uniform.type) << " " << uniform.name << "[" << uniform.array_elems
          << "];" << std::endl;
    }
    else {
      out << "\t" << to_string(uniform.type) << " " << uniform.name << ";" << std::endl;
    }
  }
  out << "} PushConstantBlock;\n\n";

  /* Member UBO block reference. */
  out << std::endl << "const constant PushConstantBlock *global_uniforms;" << std::endl;

  /* Macro define chain.
   * To access uniforms, we generate a macro such that the uniform name can
   * be used directly without using the struct's handle. */
  for (const MSLUniform &uniform : this->uniforms) {
    out << "#define " << uniform.name << " global_uniforms->" << uniform.name << std::endl;
  }
  out << std::endl;
  return out.str();
}

/* NOTE: Uniform macro definition vars can conflict with other parameters. */
std::string MSLGeneratorInterface::generate_msl_uniform_undefs(ShaderStage /*shader_stage*/)
{
  std::stringstream out;

  /* Macro undef chain. */
  for (const MSLUniform &uniform : this->uniforms) {
    out << "#undef " << uniform.name << std::endl;
  }
  /* UBO block undef. */
  for (const MSLBufferBlock &ubo : this->uniform_blocks) {
    out << "#undef " << ubo.name << std::endl;
  }
  /* SSBO block undef. */
  for (const MSLBufferBlock &ssbo : this->storage_blocks) {
    out << "#undef " << ssbo.name << std::endl;
  }
  return out.str();
}

std::string MSLGeneratorInterface::generate_msl_vertex_in_struct()
{
  std::stringstream out;

  /* Skip struct if no vert attributes. */
  if (this->vertex_input_attributes.size() == 0) {
    return "";
  }

  /* Output */
  out << "typedef struct {" << std::endl;
  for (const MSLVertexInputAttribute &in_attr : this->vertex_input_attributes) {
    /* Matrix and array attributes are not trivially supported and thus
     * require each element to be passed as an individual attribute.
     * This requires shader source generation of sequential elements.
     * The matrix type is then re-packed into a Mat4 inside the entry function.
     *
     * e.g.
     * float4 __internal_modelmatrix_0 [[attribute(0)]];
     * float4 __internal_modelmatrix_1 [[attribute(1)]];
     * float4 __internal_modelmatrix_2 [[attribute(2)]];
     * float4 __internal_modelmatrix_3 [[attribute(3)]];
     */
    if (is_matrix_type(in_attr.type) && !this->uses_ssbo_vertex_fetch_mode) {
      for (int elem = 0; elem < get_matrix_location_count(in_attr.type); elem++) {
        out << "\t" << get_matrix_subtype(in_attr.type) << " __internal_" << in_attr.name << elem
            << " [[attribute(" << (in_attr.layout_location + elem) << ")]];" << std::endl;
      }
    }
    else {
      out << "\t" << in_attr.type << " " << in_attr.name << " [[attribute("
          << in_attr.layout_location << ")]];" << std::endl;
    }
  }

  out << "} VertexIn;" << std::endl << std::endl;

  return out.str();
}

std::string MSLGeneratorInterface::generate_msl_vertex_out_struct(ShaderStage shader_stage)
{
  BLI_assert(shader_stage == ShaderStage::VERTEX || shader_stage == ShaderStage::FRAGMENT);
  std::stringstream out;

  /* Vertex output struct. */
  out << "typedef struct {" << std::endl;

  /* If we use GL position, our standard output variable will be mapped to '_default_position_'.
   * Otherwise, we use the FIRST element in the output array.
   * If transform feedback is enabled, we do not need to output position, unless it
   * is explicitly specified as a tf output. */
  bool first_attr_is_position = false;
  if (this->uses_gl_Position) {

    /* If invariance is available, utilize this to consistently mitigate depth fighting artifacts
     * by ensuring that vertex position is consistently calculated between subsequent passes
     * with maximum precision. */
    out << "\tfloat4 _default_position_ [[position]]";
    out << " [[invariant]]";
    out << ";" << std::endl;
  }
  else {
    if (!this->uses_transform_feedback) {
      /* Use first output element for position. */
      BLI_assert(this->vertex_output_varyings.size() > 0);
      BLI_assert(this->vertex_output_varyings[0].type == "vec4");

      /* Use invariance if available. See above for detail. */
      out << "\tfloat4 " << this->vertex_output_varyings[0].name << " [[position]];";
      out << " [[invariant]]";
      out << ";" << std::endl;
      first_attr_is_position = true;
    }
  }

  /* Generate other vertex output members. */
  bool skip_first_index = first_attr_is_position;
  for (const MSLVertexOutputAttribute &v_out : this->vertex_output_varyings) {

    /* Skip first index if used for position. */
    if (skip_first_index) {
      skip_first_index = false;
      continue;
    }

    if (v_out.is_array) {
      /* Array types cannot be trivially passed between shading stages.
       * Instead we pass each component individually. E.g. vec4 pos[2]
       * will be converted to: `vec4 pos_0; vec4 pos_1;`
       * The specified interpolation qualifier will be applied per element. */
      /* TODO(Metal): Support array of matrix in-out types if required
       * e.g. Mat4 out_matrices[3]. */
      for (int i = 0; i < v_out.array_elems; i++) {
        out << "\t" << v_out.type << " " << v_out.instance_name << "_" << v_out.name << i
            << v_out.get_mtl_interpolation_qualifier() << ";" << std::endl;
      }
    }
    else {
      /* Matrix types need to be expressed as their vector sub-components. */
      if (is_matrix_type(v_out.type)) {
        BLI_assert(v_out.get_mtl_interpolation_qualifier() == " [[flat]]" &&
                   "Matrix varying types must have [[flat]] interpolation");
        std::string subtype = get_matrix_subtype(v_out.type);
        for (int elem = 0; elem < get_matrix_location_count(v_out.type); elem++) {
          out << "\t" << subtype << v_out.instance_name << " __matrix_" << v_out.name << elem
              << v_out.get_mtl_interpolation_qualifier() << ";" << std::endl;
        }
      }
      else {
        out << "\t" << v_out.type << " " << v_out.instance_name << "_" << v_out.name
            << v_out.get_mtl_interpolation_qualifier() << ";" << std::endl;
      }
    }
  }

  /* Add gl_PointSize if written to. */
  if (shader_stage == ShaderStage::VERTEX) {
    if (this->uses_gl_PointSize) {
      /* If `gl_PointSize` is explicitly written to,
       * we will output the written value directly.
       * This value can still be overridden by the
       * global point-size value. */
      out << "\tfloat pointsize [[point_size]];" << std::endl;
    }
    else {
      /* Otherwise, if point-size is not written to inside the shader,
       * then its usage is controlled by whether the `MTL_global_pointsize`
       * function constant has been specified.
       * This function constant is enabled for all point primitives being rendered. */
      out << "\tfloat pointsize [[point_size, function_constant(MTL_global_pointsize)]];"
          << std::endl;
    }
  }

  /* Add gl_ClipDistance[n]. */
  if (shader_stage == ShaderStage::VERTEX) {
    out << "#if defined(USE_CLIP_PLANES) || defined(USE_WORLD_CLIP_PLANES)" << std::endl;
    if (this->clip_distances.size() > 1) {
      /* Output array of clip distances if specified. */
      out << "\tfloat clipdistance [[clip_distance, "
             "function_constant(MTL_clip_distances_enabled)]] ["
          << this->clip_distances.size() << "];" << std::endl;
    }
    else if (this->clip_distances.size() > 0) {
      out << "\tfloat clipdistance [[clip_distance, "
             "function_constant(MTL_clip_distances_enabled)]];"
          << std::endl;
    }
    out << "#endif" << std::endl;
  }

  /* Add MTL render target array index for multilayered rendering support. */
  if (uses_gpu_layer) {
    out << "\tuint gpu_Layer [[render_target_array_index]];" << std::endl;
  }

  /* Add Viewport Index output */
  if (uses_gpu_viewport_index) {
    out << "\tuint gpu_ViewportIndex [[viewport_array_index]];" << std::endl;
  }

  out << "} VertexOut;" << std::endl << std::endl;

  return out.str();
}

std::string MSLGeneratorInterface::generate_msl_vertex_transform_feedback_out_struct(
    ShaderStage shader_stage)
{
  BLI_assert(shader_stage == ShaderStage::VERTEX || shader_stage == ShaderStage::FRAGMENT);
  UNUSED_VARS_NDEBUG(shader_stage);
  std::stringstream out;
  vertex_output_varyings_tf.clear();

  out << "typedef struct {" << std::endl;

  /* If we use GL position, our standard output variable will be mapped to '_default_position_'.
   * Otherwise, we use the FIRST element in the output array -- If transform feedback is enabled,
   * we do not need to output position */
  bool first_attr_is_position = false;
  if (this->uses_gl_Position) {

    if (parent_shader_.has_transform_feedback_varying("gl_Position")) {
      out << "\tfloat4 pos [[position]];" << std::endl;
      vertex_output_varyings_tf.append({.type = "vec4",
                                        .name = "gl_Position",
                                        .interpolation_qualifier = "",
                                        .is_array = false,
                                        .array_elems = 1});
    }
  }
  else {
    if (!this->uses_transform_feedback) {
      /* Use first output element for position */
      BLI_assert(this->vertex_output_varyings.size() > 0);
      BLI_assert(this->vertex_output_varyings[0].type == "vec4");
      first_attr_is_position = true;
    }
  }

  /* Generate other vertex outputs. */
  bool skip_first_index = first_attr_is_position;
  for (const MSLVertexOutputAttribute &v_out : this->vertex_output_varyings) {

    /* Skip first index if used for position. */
    if (skip_first_index) {
      skip_first_index = false;
      continue;
    }

    if (!parent_shader_.has_transform_feedback_varying(v_out.name)) {
      continue;
    }
    vertex_output_varyings_tf.append(v_out);

    if (v_out.is_array) {
      /* TODO(Metal): Support array of matrix types if required. */
      for (int i = 0; i < v_out.array_elems; i++) {
        out << "\t" << v_out.type << " " << v_out.name << i
            << v_out.get_mtl_interpolation_qualifier() << ";" << std::endl;
      }
    }
    else {
      /* Matrix types need to be expressed as their vector sub-components. */
      if (is_matrix_type(v_out.type)) {
        BLI_assert(v_out.get_mtl_interpolation_qualifier() == " [[flat]]" &&
                   "Matrix varying types must have [[flat]] interpolation");
        std::string subtype = get_matrix_subtype(v_out.type);
        for (int elem = 0; elem < get_matrix_location_count(v_out.type); elem++) {
          out << "\t" << subtype << " __matrix_" << v_out.name << elem
              << v_out.get_mtl_interpolation_qualifier() << ";" << std::endl;
        }
      }
      else {
        out << "\t" << v_out.type << " " << v_out.name << v_out.get_mtl_interpolation_qualifier()
            << ";" << std::endl;
      }
    }
  }

  out << "} VertexOut_TF;" << std::endl << std::endl;

  return out.str();
}

std::string MSLGeneratorInterface::generate_msl_fragment_struct(bool is_input)
{
  std::stringstream out;

  auto &fragment_interface_src = (is_input) ? this->fragment_tile_inputs : this->fragment_outputs;

  /* Output. */
  out << "typedef struct {" << std::endl;
  for (int f_output = 0; f_output < fragment_interface_src.size(); f_output++) {
    out << "\t" << to_string(fragment_interface_src[f_output].type) << " "
        << fragment_interface_src[f_output].name << " [[color("
        << fragment_interface_src[f_output].layout_location << ")";
    if (fragment_interface_src[f_output].layout_index >= 0) {
      out << ", index(" << fragment_interface_src[f_output].layout_index << ")";
    }
    if (fragment_interface_src[f_output].raster_order_group >= 0) {
      out << ", raster_order_group(" << fragment_interface_src[f_output].raster_order_group << ")";
    }
    out << "]]"
        << ";" << std::endl;
  }
  /* Add gl_FragDepth output if used. */
  if (this->uses_gl_FragDepth) {
    std::string out_depth_argument = ((this->depth_write == DepthWrite::GREATER) ?
                                          "greater" :
                                          ((this->depth_write == DepthWrite::LESS) ? "less" :
                                                                                     "any"));
    out << "\tfloat fragdepth [[depth(" << out_depth_argument << ")]];" << std::endl;
  }
  /* Add gl_FragStencilRefARB output if used. */
  if (!is_input && this->uses_gl_FragStencilRefARB) {
    out << "\tuint fragstencil [[stencil]];" << std::endl;
  }
  if (is_input) {
    out << "} " FRAGMENT_TILE_IN_STRUCT_NAME ";" << std::endl;
  }
  else {
    out << "} " FRAGMENT_OUT_STRUCT_NAME ";" << std::endl;
  }
  out << std::endl;
  return out.str();
}

std::string MSLGeneratorInterface::generate_msl_global_uniform_population(ShaderStage stage)
{
  if (this->uniforms.size() == 0) {
    return "";
  }
  /* Populate Global Uniforms. */
  std::stringstream out;

  /* Copy UBO block ref. */
  out << "\t/* Copy Uniform block member reference */" << std::endl;
  out << "\t" << get_shader_stage_instance_name(stage) << "."
      << "global_uniforms = uniforms;" << std::endl;

  return out.str();
}

std::string MSLGeneratorInterface::generate_msl_fragment_tile_input_population()
{
  std::stringstream out;
  for (const MSLFragmentTileInputAttribute &tile_input : this->fragment_tile_inputs) {
    out << "\t" << get_shader_stage_instance_name(ShaderStage::FRAGMENT) << "." << tile_input.name
        << " = "
        << "fragment_tile_in." << tile_input.name << ";" << std::endl;
  }
  return out.str();
}

std::string MSLGeneratorInterface::generate_msl_uniform_block_population(ShaderStage stage)
{
  /* Populate Global Uniforms. */
  std::stringstream out;
  out << "\t/* Copy UBO block references into local class variables */" << std::endl;
  for (const MSLBufferBlock &ubo : this->uniform_blocks) {

    /* Only include blocks which are used within this stage. */
    if (bool(ubo.stage & stage)) {
      /* Generate UBO reference assignment.
       * NOTE(Metal): We append `_local` post-fix onto the class member name
       * for the ubo to avoid name collision with the UBO accessor macro.
       * We only need to add this post-fix for the non-array access variant,
       * as the array is indexed directly, rather than requiring a dereference. */
      out << "\t" << get_shader_stage_instance_name(stage) << "." << ubo.name;
      if (!ubo.is_array) {
        out << "_local";
      }
      out << " = " << ubo.name << ";" << std::endl;
    }
  }

  /* Populate storage buffer references. */
  out << "\t/* Copy SSBO block references into local class variables */" << std::endl;
  for (const MSLBufferBlock &ssbo : this->storage_blocks) {

    /* Only include blocks which are used within this stage. */
    if (bool(ssbo.stage & stage) && !ssbo.is_texture_buffer) {
      /* Generate UBO reference assignment.
       * NOTE(Metal): We append `_local` post-fix onto the class member name
       * for the ubo to avoid name collision with the UBO accessor macro.
       * We only need to add this post-fix for the non-array access variant,
       * as the array is indexed directly, rather than requiring a dereference. */
      out << "\t" << get_shader_stage_instance_name(stage) << "." << ssbo.name;
      if (!ssbo.is_array) {
        out << "_local";
      }
      out << " = " << ssbo.name << ";" << std::endl;
    }
  }

  out << std::endl;
  return out.str();
}

/* Copy input attributes from stage_in into class local variables. */
std::string MSLGeneratorInterface::generate_msl_vertex_attribute_input_population()
{
  static const char *shader_stage_inst_name = get_shader_stage_instance_name(ShaderStage::VERTEX);

  /* SSBO Vertex Fetch mode does not require local attribute population,
   * we only need to pass over the buffer pointer references. */
  if (this->uses_ssbo_vertex_fetch_mode) {
    std::stringstream out;
    out << "const constant uchar* GLOBAL_MTL_VERTEX_DATA[MTL_SSBO_VERTEX_FETCH_MAX_VBOS] = {"
        << std::endl;
    for (int i = 0; i < MTL_SSBO_VERTEX_FETCH_MAX_VBOS; i++) {
      char delimiter = (i < MTL_SSBO_VERTEX_FETCH_MAX_VBOS - 1) ? ',' : ' ';
      out << "\t\tMTL_VERTEX_DATA_" << i << delimiter << std::endl;
    }
    out << "};" << std::endl;
    out << "\t" << shader_stage_inst_name << ".MTL_VERTEX_DATA = GLOBAL_MTL_VERTEX_DATA;"
        << std::endl;
    out << "\t" << shader_stage_inst_name << ".MTL_INDEX_DATA_U16 = MTL_INDEX_DATA;" << std::endl;
    out << "\t" << shader_stage_inst_name
        << ".MTL_INDEX_DATA_U32 = reinterpret_cast<constant "
           "uint32_t*>(MTL_INDEX_DATA);"
        << std::endl;
    return out.str();
  }

  /* Populate local attribute variables. */
  std::stringstream out;
  out << "\t/* Copy Vertex Stage-in attributes into local variables */" << std::endl;
  for (int attribute = 0; attribute < this->vertex_input_attributes.size(); attribute++) {

    if (is_matrix_type(this->vertex_input_attributes[attribute].type)) {
      /* Reading into an internal matrix from split attributes: Should generate the following:
       * vertex_shader_instance.mat_attribute_type =
       * mat4(v_in.__internal_mat_attribute_type0,
       *      v_in.__internal_mat_attribute_type1,
       *      v_in.__internal_mat_attribute_type2,
       *      v_in.__internal_mat_attribute_type3). */
      out << "\t" << shader_stage_inst_name << "." << this->vertex_input_attributes[attribute].name
          << " = " << this->vertex_input_attributes[attribute].type << "(v_in.__internal_"
          << this->vertex_input_attributes[attribute].name << 0;
      for (int elem = 1;
           elem < get_matrix_location_count(this->vertex_input_attributes[attribute].type);
           elem++)
      {
        out << ",\n"
            << "v_in.__internal_" << this->vertex_input_attributes[attribute].name << elem;
      }
      out << ");";
    }
    else {
      /* OpenGL uses the `GPU_FETCH_*` functions which can alter how an attribute value is
       * interpreted. In Metal, we cannot support all implicit conversions within the vertex
       * descriptor/vertex stage-in, so we need to perform value transformation on-read.
       *
       * This is handled by wrapping attribute reads to local shader registers in a
       * suitable conversion function `attribute_conversion_func_name`.
       * This conversion function performs a specific transformation on the source
       * vertex data, depending on the specified GPU_FETCH_* mode for the current
       * vertex format.
       *
       * The fetch_mode is specified per-attribute using specialization constants
       * on the PSO, wherein a unique set of constants is passed in per vertex
       * buffer/format configuration. Efficiently enabling pass-through reads
       * if no special fetch is required. */
      bool do_attribute_conversion_on_read = false;
      std::string attribute_conversion_func_name = get_attribute_conversion_function(
          &do_attribute_conversion_on_read, this->vertex_input_attributes[attribute].type);

      if (do_attribute_conversion_on_read) {
        BLI_assert(this->vertex_input_attributes[attribute].layout_location >= 0);
        out << "\t" << attribute_conversion_func_name << "(MTL_AttributeConvert"
            << this->vertex_input_attributes[attribute].layout_location << ", v_in."
            << this->vertex_input_attributes[attribute].name << ", " << shader_stage_inst_name
            << "." << this->vertex_input_attributes[attribute].name << ");" << std::endl;
      }
      else {
        out << "\t" << shader_stage_inst_name << "."
            << this->vertex_input_attributes[attribute].name << " = v_in."
            << this->vertex_input_attributes[attribute].name << ";" << std::endl;
      }
    }
  }
  out << std::endl;
  return out.str();
}

/* Copy post-main, modified, local class variables into vertex-output struct. */
std::string MSLGeneratorInterface::generate_msl_vertex_output_population()
{
  static const char *shader_stage_inst_name = get_shader_stage_instance_name(ShaderStage::VERTEX);
  std::stringstream out;
  out << "\t/* Copy Vertex Outputs into output struct */" << std::endl;

  /* Output gl_Position with conversion to Metal coordinate-space. */
  if (this->uses_gl_Position) {
    out << "\toutput._default_position_ = " << shader_stage_inst_name << ".gl_Position;"
        << std::endl;

    /* Invert Y and rescale depth range.
     * This is an alternative method to modifying all projection matrices. */
    out << "\toutput._default_position_.y = -output._default_position_.y;" << std::endl;
    out << "\toutput._default_position_.z = "
           "(output._default_position_.z+output._default_position_.w)/2.0;"
        << std::endl;
  }

  /* Output Point-size. */
  if (this->uses_gl_PointSize) {
    out << "\toutput.pointsize = " << shader_stage_inst_name << ".gl_PointSize;" << std::endl;
  }

  /* Output render target array Index. */
  if (uses_gpu_layer) {
    out << "\toutput.gpu_Layer = " << shader_stage_inst_name << ".gpu_Layer;" << std::endl;
  }

  /* Output Viewport Index. */
  if (uses_gpu_viewport_index) {
    out << "\toutput.gpu_ViewportIndex = " << shader_stage_inst_name << ".gpu_ViewportIndex;"
        << std::endl;
  }

  /* Output clip-distances.
   * Clip distances are only written to if both clipping planes are turned on for the shader,
   * and the clipping planes are enabled. Enablement is controlled on a per-plane basis
   * via function constants in the shader pipeline state object (PSO). */
  out << "#if defined(USE_CLIP_PLANES) || defined(USE_WORLD_CLIP_PLANES)" << std::endl
      << "if(MTL_clip_distances_enabled) {" << std::endl;
  if (this->clip_distances.size() > 1) {
    for (int cd = 0; cd < this->clip_distances.size(); cd++) {
      /* Default value when clipping is disabled >= 0.0 to ensure primitive is not clipped. */
      out << "\toutput.clipdistance[" << cd
          << "] = (is_function_constant_defined(MTL_clip_distance_enabled" << cd << "))?"
          << shader_stage_inst_name << ".gl_ClipDistance_" << cd << ":1.0;" << std::endl;
    }
  }
  else if (this->clip_distances.size() > 0) {
    out << "\toutput.clipdistance = " << shader_stage_inst_name << ".gl_ClipDistance_0;"
        << std::endl;
  }
  out << "}" << std::endl << "#endif" << std::endl;

  /* Populate output vertex variables. */
  int output_id = 0;
  for (const MSLVertexOutputAttribute &v_out : this->vertex_output_varyings) {
    if (v_out.is_array) {

      for (int i = 0; i < v_out.array_elems; i++) {
        out << "\toutput." << v_out.instance_name << "_" << v_out.name << i << " = "
            << shader_stage_inst_name << ".";

        if (v_out.instance_name != "") {
          out << v_out.instance_name << ".";
        }

        out << v_out.name << "[" << i << "]"
            << ";" << std::endl;
      }
    }
    else {
      /* Matrix types are split into vectors and need to be reconstructed. */
      if (is_matrix_type(v_out.type)) {
        for (int elem = 0; elem < get_matrix_location_count(v_out.type); elem++) {
          out << "\toutput." << v_out.instance_name << "__matrix_" << v_out.name << elem << " = "
              << shader_stage_inst_name << ".";

          if (v_out.instance_name != "") {
            out << v_out.instance_name << ".";
          }

          out << v_out.name << "[" << elem << "];" << std::endl;
        }
      }
      else {
        /* If we are not using gl_Position, first vertex output is used for position.
         * Ensure it is vec4. If transform feedback is enabled, we do not need position. */
        if (!this->uses_gl_Position && output_id == 0 && !this->uses_transform_feedback) {

          out << "\toutput." << v_out.instance_name << "_" << v_out.name << " = to_vec4("
              << shader_stage_inst_name << "." << v_out.name << ");" << std::endl;

          /* Invert Y */
          out << "\toutput." << v_out.instance_name << "_" << v_out.name << ".y = -output."
              << v_out.name << ".y;" << std::endl;
        }
        else {

          /* Assign vertex output. */
          out << "\toutput." << v_out.instance_name << "_" << v_out.name << " = "
              << shader_stage_inst_name << ".";

          if (v_out.instance_name != "") {
            out << v_out.instance_name << ".";
          }

          out << v_out.name << ";" << std::endl;
        }
      }
    }
    output_id++;
  }
  out << std::endl;
  return out.str();
}

/* Copy desired output varyings into transform feedback structure */
std::string MSLGeneratorInterface::generate_msl_vertex_output_tf_population()
{
  static const char *shader_stage_inst_name = get_shader_stage_instance_name(ShaderStage::VERTEX);
  std::stringstream out;
  out << "\t/* Copy Vertex TF Outputs into transform feedback buffer */" << std::endl;

  /* Populate output vertex variables */
  /* TODO(Metal): Currently do not need to support output matrix types etc; but may need to
   * verify for other configurations if these occur in any cases. */
  for (int v_output = 0; v_output < this->vertex_output_varyings_tf.size(); v_output++) {
    out << "transform_feedback_results[gl_VertexID]."
        << this->vertex_output_varyings_tf[v_output].name << " = " << shader_stage_inst_name << "."
        << this->vertex_output_varyings_tf[v_output].name << ";" << std::endl;
  }
  out << std::endl;
  return out.str();
}

/* Copy fragment stage inputs (Vertex Outputs) into local class variables. */
std::string MSLGeneratorInterface::generate_msl_fragment_input_population()
{
  static const char *shader_stage_inst_name = get_shader_stage_instance_name(
      ShaderStage::FRAGMENT);
  /* Populate local attribute variables. */
  std::stringstream out;
  out << "\t/* Copy Fragment input into local variables. */" << std::endl;

  /* Special common case for gl_FragCoord, assigning to input position. */
  if (this->uses_gl_Position) {
    out << "\t" << shader_stage_inst_name << ".gl_FragCoord = v_in._default_position_;"
        << std::endl;
  }
  else {
    /* When gl_Position is not set, first VertexIn element is used for position. */
    out << "\t" << shader_stage_inst_name << ".gl_FragCoord = v_in."
        << this->vertex_output_varyings[0].name << ";" << std::endl;
  }

  /* Assign default gl_FragDepth.
   * If gl_FragDepth is used, it should default to the original depth value. Resolves #107159 where
   * overlay_wireframe_frag may not write to gl_FragDepth. */
  if (this->uses_gl_FragDepth) {
    out << "\t" << shader_stage_inst_name << ".gl_FragDepth = " << shader_stage_inst_name
        << ".gl_FragCoord.z;" << std::endl;
  }

  /* Input render target array index received from vertex shader. */
  if (uses_gpu_layer) {
    out << "\t" << shader_stage_inst_name << ".gpu_Layer = v_in.gpu_Layer;" << std::endl;
  }

  /* Input viewport array index received from vertex shader. */
  if (uses_gpu_viewport_index) {
    out << "\t" << shader_stage_inst_name << ".gpu_ViewportIndex = v_in.gpu_ViewportIndex;"
        << std::endl;
  }

  /* NOTE: We will only assign to the intersection of the vertex output and fragment input.
   * Fragment input represents varying variables which are declared (but are not necessarily
   * used). The Vertex out defines the set which is passed into the fragment shader, which
   * contains out variables declared in the vertex shader, though these are not necessarily
   * consumed by the fragment shader.
   *
   * In the cases where the fragment shader expects a variable, but it does not exist in the
   * vertex shader, a warning will be provided. */
  for (int f_input = (this->uses_gl_Position) ? 0 : 1;
       f_input < this->fragment_input_varyings.size();
       f_input++)
  {
    bool exists_in_vertex_output = false;
    for (int v_o = 0; v_o < this->vertex_output_varyings.size() && !exists_in_vertex_output; v_o++)
    {
      if (this->fragment_input_varyings[f_input].name == this->vertex_output_varyings[v_o].name) {
        exists_in_vertex_output = true;
      }
    }
    if (!exists_in_vertex_output) {
      shader_debug_printf(
          "[Warning] Fragment shader expects varying input '%s', but this is not passed from "
          "the "
          "vertex shader\n",
          this->fragment_input_varyings[f_input].name.c_str());
      continue;
    }
    if (this->fragment_input_varyings[f_input].is_array) {
      for (int i = 0; i < this->fragment_input_varyings[f_input].array_elems; i++) {
        out << "\t" << shader_stage_inst_name << ".";

        if (this->fragment_input_varyings[f_input].instance_name != "") {
          out << this->fragment_input_varyings[f_input].instance_name << ".";
        }

        out << this->fragment_input_varyings[f_input].name << "[" << i << "] = v_in."
            << this->fragment_input_varyings[f_input].instance_name << "_"
            << this->fragment_input_varyings[f_input].name << i << ";" << std::endl;
      }
    }
    else {
      /* Matrix types are split into components and need to be regrouped into a matrix. */
      if (is_matrix_type(this->fragment_input_varyings[f_input].type)) {
        out << "\t" << shader_stage_inst_name << ".";

        if (this->fragment_input_varyings[f_input].instance_name != "") {
          out << this->fragment_input_varyings[f_input].instance_name << ".";
        }

        out << this->fragment_input_varyings[f_input].name << " = "
            << this->fragment_input_varyings[f_input].type;
        int count = get_matrix_location_count(this->fragment_input_varyings[f_input].type);
        for (int elem = 0; elem < count; elem++) {
          out << ((elem == 0) ? "(" : "") << "v_in."
              << this->fragment_input_varyings[f_input].instance_name << "__matrix_"
              << this->fragment_input_varyings[f_input].name << elem
              << ((elem < count - 1) ? ",\n" : "");
        }
        out << ");" << std::endl;
      }
      else {
        out << "\t" << shader_stage_inst_name << ".";

        if (this->fragment_input_varyings[f_input].instance_name != "") {
          out << this->fragment_input_varyings[f_input].instance_name << ".";
        }

        out << this->fragment_input_varyings[f_input].name << " = v_in."
            << this->fragment_input_varyings[f_input].instance_name << "_"
            << this->fragment_input_varyings[f_input].name << ";" << std::endl;
      }
    }
  }
  out << std::endl;
  return out.str();
}

/* Copy post-main, modified, local class variables into fragment-output struct. */
std::string MSLGeneratorInterface::generate_msl_fragment_output_population()
{
  static const char *shader_stage_inst_name = get_shader_stage_instance_name(
      ShaderStage::FRAGMENT);
  /* Populate output fragment variables. */
  std::stringstream out;
  out << "\t/* Copy Fragment Outputs into output struct. */" << std::endl;

  /* Output gl_FragDepth. */
  if (this->uses_gl_FragDepth) {
    out << "\toutput.fragdepth = " << shader_stage_inst_name << ".gl_FragDepth;" << std::endl;
  }

  /* Output gl_FragStencilRefARB. */
  if (this->uses_gl_FragStencilRefARB) {
    out << "\toutput.fragstencil = uint(" << shader_stage_inst_name << ".gl_FragStencilRefARB);"
        << std::endl;
  }

  /* Output attributes. */
  for (int f_output = 0; f_output < this->fragment_outputs.size(); f_output++) {

    out << "\toutput." << this->fragment_outputs[f_output].name << " = " << shader_stage_inst_name
        << "." << this->fragment_outputs[f_output].name << ";" << std::endl;
  }
  out << std::endl;
  return out.str();
}

std::string MSLGeneratorInterface::generate_msl_texture_vars(ShaderStage shader_stage)
{
  /* NOTE: Shader stage must be a singular stage index. Compound stage is not valid for this
   * function. */
  BLI_assert(shader_stage == ShaderStage::VERTEX || shader_stage == ShaderStage::FRAGMENT ||
             shader_stage == ShaderStage::COMPUTE);

  std::stringstream out;
  out << "\t/* Populate local texture and sampler members */" << std::endl;
  for (int i = 0; i < this->texture_samplers.size(); i++) {
    if (bool(this->texture_samplers[i].stage & shader_stage)) {

      /* Assign texture reference. */
      out << "\t" << get_shader_stage_instance_name(shader_stage) << "."
          << this->texture_samplers[i].name << ".texture = &" << this->texture_samplers[i].name
          << ";" << std::endl;

      /* Assign sampler reference. */
      if (this->use_argument_buffer_for_samplers()) {
        out << "\t" << get_shader_stage_instance_name(shader_stage) << "."
            << this->texture_samplers[i].name << ".samp = &samplers.sampler_args["
            << this->texture_samplers[i].slot << "];" << std::endl;
      }
      else {
        out << "\t" << get_shader_stage_instance_name(shader_stage) << "."
            << this->texture_samplers[i].name << ".samp = &" << this->texture_samplers[i].name
            << "_sampler;" << std::endl;
      }

      /* Assign texture buffer reference and uniform metadata (if used). */
      int tex_buf_id = this->texture_samplers[i].atomic_fallback_buffer_ssbo_id;
      if (tex_buf_id != -1) {
        MSLBufferBlock &ssbo = this->storage_blocks[tex_buf_id];
        out << "\t" << get_shader_stage_instance_name(shader_stage) << "."
            << this->texture_samplers[i].name << ".buffer = " << ssbo.name << ";" << std::endl;
        out << "\t" << get_shader_stage_instance_name(shader_stage) << "."
            << this->texture_samplers[i].name << ".aligned_width = uniforms->"
            << this->texture_samplers[i].name << "_metadata.w;" << std::endl;

        /* Buffer-backed 2D Array and 3D texture types are not natively supported so texture size
         * is passed in as uniform metadata for 3D to 2D coordinate remapping. */
        if (ELEM(this->texture_samplers[i].type,
                 ImageType::UINT_2D_ARRAY_ATOMIC,
                 ImageType::UINT_3D_ATOMIC,
                 ImageType::INT_2D_ARRAY_ATOMIC,
                 ImageType::INT_3D_ATOMIC))
        {
          out << "\t" << get_shader_stage_instance_name(shader_stage) << "."
              << this->texture_samplers[i].name << ".texture_size = ushort3(uniforms->"
              << this->texture_samplers[i].name << "_metadata.xyz);" << std::endl;
        }
      }
    }
  }
  out << std::endl;
  return out.str();
}

void MSLGeneratorInterface::resolve_input_attribute_locations()
{
  /* Determine used-attribute-location mask. */
  uint32_t used_locations = 0;
  for (const MSLVertexInputAttribute &attr : vertex_input_attributes) {
    if (attr.layout_location >= 0) {
      /* Matrix and array types span multiple location slots. */
      uint32_t location_element_count = get_matrix_location_count(attr.type);
      for (uint32_t i = 1; i <= location_element_count; i++) {
        /* Ensure our location hasn't already been used. */
        uint32_t location_mask = (i << attr.layout_location);
        BLI_assert((used_locations & location_mask) == 0);
        used_locations = used_locations | location_mask;
      }
    }
  }

  /* Assign unused location slots to other attributes. */
  for (MSLVertexInputAttribute &attr : vertex_input_attributes) {
    if (attr.layout_location == -1) {
      /* Determine number of locations required. */
      uint32_t required_attr_slot_count = get_matrix_location_count(attr.type);

      /* Determine free location.
       * Starting from 1 is slightly less efficient, however,
       * given multi-sized attributes, an earlier slot may remain free.
       * given GPU_VERT_ATTR_MAX_LEN is small, this wont matter. */
      for (int loc = 0; loc < GPU_VERT_ATTR_MAX_LEN - (required_attr_slot_count - 1); loc++) {

        uint32_t location_mask = (1 << loc);
        /* Generate sliding mask using location and required number of slots,
         * to ensure contiguous slots are free.
         * slot mask will be a number containing N binary 1's, where N is the
         * number of attributes needed.
         * e.g. N=4 -> 1111. */
        uint32_t location_slot_mask = (1 << required_attr_slot_count) - 1;
        uint32_t sliding_location_slot_mask = location_slot_mask << location_mask;
        if ((used_locations & sliding_location_slot_mask) == 0) {
          /* Assign location and update mask. */
          attr.layout_location = loc;
          used_locations = used_locations | location_slot_mask;
          continue;
        }
      }

      /* Error if could not assign attribute. */
      MTL_LOG_ERROR("Could not assign attribute location to attribute %s for shader %s",
                    attr.name.c_str(),
                    this->parent_shader_.name_get());
    }
  }
}

void MSLGeneratorInterface::resolve_fragment_output_locations()
{
  int running_location_ind = 0;

  /* This code works under the assumption that either all layout_locations are set,
   * or none are. */
  for (int i = 0; i < this->fragment_outputs.size(); i++) {
    BLI_assert_msg(
        ((running_location_ind > 0) ? (this->fragment_outputs[i].layout_location == -1) : true),
        "Error: Mismatched input attributes, some with location specified, some without");
    if (this->fragment_outputs[i].layout_location == -1) {
      this->fragment_outputs[i].layout_location = running_location_ind;
      running_location_ind++;
    }
  }
}

/**
 * Add string to name buffer. Utility function to be used in bake_shader_interface.
 * Returns the offset of the inserted name.
 */
static uint32_t name_buffer_copystr(char **name_buffer_ptr,
                                    const char *str_to_copy,
                                    uint32_t &name_buffer_size,
                                    uint32_t &name_buffer_offset)
{
  /* Verify input is valid. */
  BLI_assert(str_to_copy != nullptr);

  /* Determine length of new string, and ensure name buffer is large enough. */
  uint32_t ret_len = strlen(str_to_copy);
  BLI_assert(ret_len > 0);

  /* If required name buffer size is larger, increase by at least 128 bytes. */
  if (name_buffer_offset + ret_len + 1 > name_buffer_size) {
    name_buffer_size = name_buffer_offset + max_ii(128, ret_len + 1);
    *name_buffer_ptr = (char *)MEM_reallocN(*name_buffer_ptr, name_buffer_size);
  }

  /* Copy string into name buffer. */
  uint32_t insert_offset = name_buffer_offset;
  char *current_offset = (*name_buffer_ptr) + insert_offset;
  memcpy(current_offset, str_to_copy, (ret_len + 1) * sizeof(char));

  /* Adjust offset including null terminator. */
  name_buffer_offset += ret_len + 1;

  /* Return offset into name buffer for inserted string. */
  return insert_offset;
}

MTLShaderInterface *MSLGeneratorInterface::bake_shader_interface(const char *name)
{
  MTLShaderInterface *interface = new MTLShaderInterface(name);
  interface->init();

  /* Name buffer. */
  /* Initialize name buffer. */
  uint32_t name_buffer_size = 256;
  uint32_t name_buffer_offset = 0;
  interface->name_buffer_ = (char *)MEM_mallocN(name_buffer_size, "name_buffer");

  /* Prepare Interface Input Attributes. */
  int c_offset = 0;
  for (int attribute = 0; attribute < this->vertex_input_attributes.size(); attribute++) {

    /* We need a special case for handling matrix types, which splits the matrix into its vector
     * components. */
    if (is_matrix_type(this->vertex_input_attributes[attribute].type)) {

      eMTLDataType mtl_type = to_mtl_type(
          get_matrix_subtype(this->vertex_input_attributes[attribute].type));
      int size = mtl_get_data_type_size(mtl_type);
      for (int elem = 0;
           elem < get_matrix_location_count(this->vertex_input_attributes[attribute].type);
           elem++)
      {
        /* First attribute matches the core name -- subsequent attributes tagged with
         * `__internal_<name><index>`. */
        std::string _internal_name = (elem == 0) ?
                                         this->vertex_input_attributes[attribute].name :
                                         "__internal_" +
                                             this->vertex_input_attributes[attribute].name +
                                             std::to_string(elem);

        /* IF Using SSBO vertex Fetch, we do not need to expose other dummy attributes in the
         * shader interface, only the first one for the whole matrix, as we can pass whatever data
         * we want in this mode, and do not need to split attributes. */
        if (elem == 0 || !this->uses_ssbo_vertex_fetch_mode) {
          interface->add_input_attribute(
              name_buffer_copystr(&interface->name_buffer_,
                                  _internal_name.c_str(),
                                  name_buffer_size,
                                  name_buffer_offset),
              this->vertex_input_attributes[attribute].layout_location + elem,
              mtl_datatype_to_vertex_type(mtl_type),
              0,
              size,
              c_offset,
              (elem == 0) ?
                  get_matrix_location_count(this->vertex_input_attributes[attribute].type) :
                  0);
        }
        c_offset += size;
      }
      shader_debug_printf(
          "[Note] Matrix Type '%s' added to shader interface as vertex attribute. (Elem Count: "
          "%d)\n",
          this->vertex_input_attributes[attribute].name.c_str(),
          get_matrix_location_count(this->vertex_input_attributes[attribute].type));
    }
    else {

      /* Normal attribute types. */
      eMTLDataType mtl_type = to_mtl_type(this->vertex_input_attributes[attribute].type);
      int size = mtl_get_data_type_size(mtl_type);
      interface->add_input_attribute(
          name_buffer_copystr(&interface->name_buffer_,
                              this->vertex_input_attributes[attribute].name.c_str(),
                              name_buffer_size,
                              name_buffer_offset),
          this->vertex_input_attributes[attribute].layout_location,
          mtl_datatype_to_vertex_type(mtl_type),
          0,
          size,
          c_offset);
      c_offset += size;
    }

    /* Used in `GPU_shader_get_attribute_info`. */
    interface->attr_types_[this->vertex_input_attributes[attribute].layout_location] = uint8_t(
        this->vertex_input_attributes[attribute].type);
  }

  /* Prepare Interface Default Uniform Block. */
  interface->add_push_constant_block(name_buffer_copystr(
      &interface->name_buffer_, "PushConstantBlock", name_buffer_size, name_buffer_offset));

  for (int uniform = 0; uniform < this->uniforms.size(); uniform++) {
    interface->add_uniform(
        name_buffer_copystr(&interface->name_buffer_,
                            this->uniforms[uniform].name.c_str(),
                            name_buffer_size,
                            name_buffer_offset),
        to_mtl_type(this->uniforms[uniform].type),
        (this->uniforms[uniform].is_array) ? this->uniforms[uniform].array_elems : 1);
  }

  /* Prepare Interface Uniform Blocks. */
  for (int uniform_block = 0; uniform_block < this->uniform_blocks.size(); uniform_block++) {
    interface->add_uniform_block(
        name_buffer_copystr(&interface->name_buffer_,
                            this->uniform_blocks[uniform_block].name.c_str(),
                            name_buffer_size,
                            name_buffer_offset),
        this->uniform_blocks[uniform_block].slot,
        this->uniform_blocks[uniform_block].location,
        0,
        this->uniform_blocks[uniform_block].stage);
  }

  /* Prepare Interface Storage Blocks. */
  for (int storage_block = 0; storage_block < this->storage_blocks.size(); storage_block++) {
    interface->add_storage_block(
        name_buffer_copystr(&interface->name_buffer_,
                            this->storage_blocks[storage_block].name.c_str(),
                            name_buffer_size,
                            name_buffer_offset),
        this->storage_blocks[storage_block].slot,
        this->storage_blocks[storage_block].location,
        0,
        this->storage_blocks[storage_block].stage);
  }

  /* Texture/sampler bindings to interface. */
  for (const MSLTextureResource &input_texture : this->texture_samplers) {
    /* Determine SSBO bind location for buffer-baked texture's data. */
    uint tex_buf_ssbo_location = -1;
    uint tex_buf_ssbo_id = input_texture.atomic_fallback_buffer_ssbo_id;
    if (tex_buf_ssbo_id != -1) {
      tex_buf_ssbo_location = this->storage_blocks[tex_buf_ssbo_id].location;
    }

    interface->add_texture(name_buffer_copystr(&interface->name_buffer_,
                                               input_texture.name.c_str(),
                                               name_buffer_size,
                                               name_buffer_offset),
                           input_texture.slot,
                           input_texture.location,
                           input_texture.get_texture_binding_type(),
                           input_texture.get_sampler_format(),
                           input_texture.is_texture_sampler,
                           input_texture.stage,
                           tex_buf_ssbo_location);
  }

  /* Specialization Constants. */
  for (const MSLConstant &constant : this->constants) {
    interface->add_constant(name_buffer_copystr(
        &interface->name_buffer_, constant.name.c_str(), name_buffer_size, name_buffer_offset));
  }

  /* Sampler Parameters. */
  interface->set_sampler_properties(
      this->use_argument_buffer_for_samplers(),
      this->get_sampler_argument_buffer_bind_index(ShaderStage::VERTEX),
      this->get_sampler_argument_buffer_bind_index(ShaderStage::FRAGMENT),
      this->get_sampler_argument_buffer_bind_index(ShaderStage::COMPUTE));

  /* Map Metal bindings to standardized ShaderInput struct name/binding index. */
  interface->prepare_common_shader_inputs();

  /* Resize name buffer to save some memory. */
  if (name_buffer_offset < name_buffer_size) {
    interface->name_buffer_ = (char *)MEM_reallocN(interface->name_buffer_, name_buffer_offset);
  }

  return interface;
}

std::string MSLTextureResource::get_msl_texture_type_str() const
{
  bool supports_native_atomics = MTLBackend::get_capabilities().supports_texture_atomics;
  /* Add Types as needed. */
  switch (this->type) {
    case ImageType::FLOAT_1D: {
      return "texture1d";
    }
    case ImageType::FLOAT_2D: {
      return "texture2d";
    }
    case ImageType::FLOAT_3D: {
      return "texture3d";
    }
    case ImageType::FLOAT_CUBE: {
      return "texturecube";
    }
    case ImageType::FLOAT_1D_ARRAY: {
      return "texture1d_array";
    }
    case ImageType::FLOAT_2D_ARRAY: {
      return "texture2d_array";
    }
    case ImageType::FLOAT_CUBE_ARRAY: {
      return "texturecube_array";
    }
    case ImageType::FLOAT_BUFFER: {
      return "texture_buffer";
    }
    case ImageType::DEPTH_2D: {
      return "depth2d";
    }
    case ImageType::SHADOW_2D: {
      return "depth2d";
    }
    case ImageType::DEPTH_2D_ARRAY: {
      return "depth2d_array";
    }
    case ImageType::SHADOW_2D_ARRAY: {
      return "depth2d_array";
    }
    case ImageType::DEPTH_CUBE: {
      return "depthcube";
    }
    case ImageType::SHADOW_CUBE: {
      return "depthcube";
    }
    case ImageType::DEPTH_CUBE_ARRAY: {
      return "depthcube_array";
    }
    case ImageType::SHADOW_CUBE_ARRAY: {
      return "depthcube_array";
    }
    case ImageType::INT_1D: {
      return "texture1d";
    }
    case ImageType::INT_2D: {
      return "texture2d";
    }
    case ImageType::INT_3D: {
      return "texture3d";
    }
    case ImageType::INT_CUBE: {
      return "texturecube";
    }
    case ImageType::INT_1D_ARRAY: {
      return "texture1d_array";
    }
    case ImageType::INT_2D_ARRAY: {
      return "texture2d_array";
    }
    case ImageType::INT_CUBE_ARRAY: {
      return "texturecube_array";
    }
    case ImageType::INT_BUFFER: {
      return "texture_buffer";
    }
    case ImageType::UINT_1D: {
      return "texture1d";
    }
    case ImageType::UINT_2D: {
      return "texture2d";
    }
    case ImageType::UINT_3D: {
      return "texture3d";
    }
    case ImageType::UINT_CUBE: {
      return "texturecube";
    }
    case ImageType::UINT_1D_ARRAY: {
      return "texture1d_array";
    }
    case ImageType::UINT_2D_ARRAY: {
      return "texture2d_array";
    }
    case ImageType::UINT_CUBE_ARRAY: {
      return "texturecube_array";
    }
    case ImageType::UINT_BUFFER: {
      return "texture_buffer";
    }
    /* If texture atomics are natively supported, we use the native texture type, otherwise all
     * other formats are implemented via texture2d. */
    case ImageType::INT_2D_ATOMIC:
    case ImageType::UINT_2D_ATOMIC: {
      return "texture2d";
    }
    case ImageType::INT_2D_ARRAY_ATOMIC:
    case ImageType::UINT_2D_ARRAY_ATOMIC: {
      if (supports_native_atomics) {
        return "texture2d_array";
      }
      else {
        return "texture2d";
      }
    }
    case ImageType::INT_3D_ATOMIC:
    case ImageType::UINT_3D_ATOMIC: {
      if (supports_native_atomics) {
        return "texture3d";
      }
      else {
        return "texture2d";
      }
    }

    default: {
      /* Unrecognized type. */
      BLI_assert_unreachable();
      return "ERROR";
    }
  };
}

std::string MSLTextureResource::get_msl_wrapper_type_str() const
{
  bool supports_native_atomics = MTLBackend::get_capabilities().supports_texture_atomics;
  /* Add Types as needed. */
  switch (this->type) {
    case ImageType::FLOAT_1D: {
      return "_mtl_combined_image_sampler_1d";
    }
    case ImageType::FLOAT_2D: {
      return "_mtl_combined_image_sampler_2d";
    }
    case ImageType::FLOAT_3D: {
      return "_mtl_combined_image_sampler_3d";
    }
    case ImageType::FLOAT_CUBE: {
      return "_mtl_combined_image_sampler_cube";
    }
    case ImageType::FLOAT_1D_ARRAY: {
      return "_mtl_combined_image_sampler_1d_array";
    }
    case ImageType::FLOAT_2D_ARRAY: {
      return "_mtl_combined_image_sampler_2d_array";
    }
    case ImageType::FLOAT_CUBE_ARRAY: {
      return "_mtl_combined_image_sampler_cube_array";
    }
    case ImageType::FLOAT_BUFFER: {
      return "_mtl_combined_image_sampler_buffer";
    }
    case ImageType::DEPTH_2D: {
      return "_mtl_combined_image_sampler_depth_2d";
    }
    case ImageType::SHADOW_2D: {
      return "_mtl_combined_image_sampler_depth_2d";
    }
    case ImageType::DEPTH_2D_ARRAY: {
      return "_mtl_combined_image_sampler_depth_2d_array";
    }
    case ImageType::SHADOW_2D_ARRAY: {
      return "_mtl_combined_image_sampler_depth_2d_array";
    }
    case ImageType::DEPTH_CUBE: {
      return "_mtl_combined_image_sampler_depth_cube";
    }
    case ImageType::SHADOW_CUBE: {
      return "_mtl_combined_image_sampler_depth_cube";
    }
    case ImageType::DEPTH_CUBE_ARRAY: {
      return "_mtl_combined_image_sampler_depth_cube_array";
    }
    case ImageType::SHADOW_CUBE_ARRAY: {
      return "_mtl_combined_image_sampler_depth_cube_array";
    }
    case ImageType::INT_1D: {
      return "_mtl_combined_image_sampler_1d";
    }
    case ImageType::INT_2D: {
      return "_mtl_combined_image_sampler_2d";
    }
    case ImageType::INT_3D: {
      return "_mtl_combined_image_sampler_3d";
    }
    case ImageType::INT_CUBE: {
      return "_mtl_combined_image_sampler_cube";
    }
    case ImageType::INT_1D_ARRAY: {
      return "_mtl_combined_image_sampler_1d_array";
    }
    case ImageType::INT_2D_ARRAY: {
      return "_mtl_combined_image_sampler_2d_array";
    }
    case ImageType::INT_CUBE_ARRAY: {
      return "_mtl_combined_image_sampler_cube_array";
    }
    case ImageType::INT_BUFFER: {
      return "_mtl_combined_image_sampler_buffer";
    }
    case ImageType::UINT_1D: {
      return "_mtl_combined_image_sampler_1d";
    }
    case ImageType::UINT_2D: {
      return "_mtl_combined_image_sampler_2d";
    }
    case ImageType::UINT_3D: {
      return "_mtl_combined_image_sampler_3d";
    }
    case ImageType::UINT_CUBE: {
      return "_mtl_combined_image_sampler_cube";
    }
    case ImageType::UINT_1D_ARRAY: {
      return "_mtl_combined_image_sampler_1d_array";
    }
    case ImageType::UINT_2D_ARRAY: {
      return "_mtl_combined_image_sampler_2d_array";
    }
    case ImageType::UINT_CUBE_ARRAY: {
      return "_mtl_combined_image_sampler_cube_array";
    }
    case ImageType::UINT_BUFFER: {
      return "_mtl_combined_image_sampler_buffer";
    }
    /* If native texture atomics are unsupported, map types to fallback atomic structures which
     * contain a buffer pointer and metadata members for size and alignment. */
    case ImageType::INT_2D_ATOMIC:
    case ImageType::UINT_2D_ATOMIC: {
      if (supports_native_atomics) {
        return "_mtl_combined_image_sampler_2d";
      }
      else {
        return "_mtl_combined_image_sampler_2d_atomic_fallback";
      }
    }
    case ImageType::INT_3D_ATOMIC:
    case ImageType::UINT_3D_ATOMIC: {
      if (supports_native_atomics) {
        return "_mtl_combined_image_sampler_3d";
      }
      else {
        return "_mtl_combined_image_sampler_3d_atomic_fallback";
      }
    }
    case ImageType::INT_2D_ARRAY_ATOMIC:
    case ImageType::UINT_2D_ARRAY_ATOMIC: {
      if (supports_native_atomics) {
        return "_mtl_combined_image_sampler_2d_array";
      }
      else {
        return "_mtl_combined_image_sampler_2d_array_atomic_fallback";
      }
    }
    default: {
      /* Unrecognized type. */
      BLI_assert_unreachable();
      return "ERROR";
    }
  };
}

std::string MSLTextureResource::get_msl_return_type_str() const
{
  /* Add Types as needed */
  switch (this->type) {
    /* Floating point return. */
    case ImageType::FLOAT_1D:
    case ImageType::FLOAT_2D:
    case ImageType::FLOAT_3D:
    case ImageType::FLOAT_CUBE:
    case ImageType::FLOAT_1D_ARRAY:
    case ImageType::FLOAT_2D_ARRAY:
    case ImageType::FLOAT_CUBE_ARRAY:
    case ImageType::FLOAT_BUFFER:
    case ImageType::DEPTH_2D:
    case ImageType::SHADOW_2D:
    case ImageType::DEPTH_2D_ARRAY:
    case ImageType::SHADOW_2D_ARRAY:
    case ImageType::DEPTH_CUBE:
    case ImageType::SHADOW_CUBE:
    case ImageType::DEPTH_CUBE_ARRAY:
    case ImageType::SHADOW_CUBE_ARRAY: {
      return "float";
    }
    /* Integer return. */
    case ImageType::INT_1D:
    case ImageType::INT_2D:
    case ImageType::INT_3D:
    case ImageType::INT_CUBE:
    case ImageType::INT_1D_ARRAY:
    case ImageType::INT_2D_ARRAY:
    case ImageType::INT_CUBE_ARRAY:
    case ImageType::INT_BUFFER:
    case ImageType::INT_2D_ATOMIC:
    case ImageType::INT_2D_ARRAY_ATOMIC:
    case ImageType::INT_3D_ATOMIC: {
      return "int";
    }

    /* Unsigned Integer return. */
    case ImageType::UINT_1D:
    case ImageType::UINT_2D:
    case ImageType::UINT_3D:
    case ImageType::UINT_CUBE:
    case ImageType::UINT_1D_ARRAY:
    case ImageType::UINT_2D_ARRAY:
    case ImageType::UINT_CUBE_ARRAY:
    case ImageType::UINT_BUFFER:
    case ImageType::UINT_2D_ATOMIC:
    case ImageType::UINT_2D_ARRAY_ATOMIC:
    case ImageType::UINT_3D_ATOMIC: {
      return "uint32_t";
    }

    default: {
      /* Unrecognized type. */
      BLI_assert_unreachable();
      return "ERROR";
    }
  };
}

eGPUTextureType MSLTextureResource::get_texture_binding_type() const
{
  /* Add Types as needed */
  switch (this->type) {
    case ImageType::FLOAT_1D: {
      return GPU_TEXTURE_1D;
    }
    case ImageType::FLOAT_2D: {
      return GPU_TEXTURE_2D;
    }
    case ImageType::FLOAT_3D: {
      return GPU_TEXTURE_3D;
    }
    case ImageType::FLOAT_CUBE: {
      return GPU_TEXTURE_CUBE;
    }
    case ImageType::FLOAT_1D_ARRAY: {
      return GPU_TEXTURE_1D_ARRAY;
    }
    case ImageType::FLOAT_2D_ARRAY: {
      return GPU_TEXTURE_2D_ARRAY;
    }
    case ImageType::FLOAT_CUBE_ARRAY: {
      return GPU_TEXTURE_CUBE_ARRAY;
    }
    case ImageType::FLOAT_BUFFER: {
      return GPU_TEXTURE_BUFFER;
    }
    case ImageType::DEPTH_2D: {
      return GPU_TEXTURE_2D;
    }
    case ImageType::SHADOW_2D: {
      return GPU_TEXTURE_2D;
    }
    case ImageType::DEPTH_2D_ARRAY: {
      return GPU_TEXTURE_2D_ARRAY;
    }
    case ImageType::SHADOW_2D_ARRAY: {
      return GPU_TEXTURE_2D_ARRAY;
    }
    case ImageType::DEPTH_CUBE: {
      return GPU_TEXTURE_CUBE;
    }
    case ImageType::SHADOW_CUBE: {
      return GPU_TEXTURE_CUBE;
    }
    case ImageType::DEPTH_CUBE_ARRAY: {
      return GPU_TEXTURE_CUBE_ARRAY;
    }
    case ImageType::SHADOW_CUBE_ARRAY: {
      return GPU_TEXTURE_CUBE_ARRAY;
    }
    case ImageType::INT_1D: {
      return GPU_TEXTURE_1D;
    }
    case ImageType::INT_2D: {
      return GPU_TEXTURE_2D;
    }
    case ImageType::INT_3D: {
      return GPU_TEXTURE_3D;
    }
    case ImageType::INT_CUBE: {
      return GPU_TEXTURE_CUBE;
    }
    case ImageType::INT_1D_ARRAY: {
      return GPU_TEXTURE_1D_ARRAY;
    }
    case ImageType::INT_2D_ARRAY: {
      return GPU_TEXTURE_2D_ARRAY;
    }
    case ImageType::INT_CUBE_ARRAY: {
      return GPU_TEXTURE_CUBE_ARRAY;
    }
    case ImageType::INT_BUFFER: {
      return GPU_TEXTURE_BUFFER;
    }
    case ImageType::UINT_1D: {
      return GPU_TEXTURE_1D;
    }
    case ImageType::UINT_2D:
    case ImageType::UINT_2D_ATOMIC:
    case ImageType::INT_2D_ATOMIC: {
      return GPU_TEXTURE_2D;
    }
    case ImageType::UINT_3D:
    case ImageType::UINT_3D_ATOMIC:
    case ImageType::INT_3D_ATOMIC: {
      return GPU_TEXTURE_3D;
    }
    case ImageType::UINT_CUBE: {
      return GPU_TEXTURE_CUBE;
    }
    case ImageType::UINT_1D_ARRAY: {
      return GPU_TEXTURE_1D_ARRAY;
    }
    case ImageType::UINT_2D_ARRAY:
    case ImageType::UINT_2D_ARRAY_ATOMIC:
    case ImageType::INT_2D_ARRAY_ATOMIC: {
      return GPU_TEXTURE_2D_ARRAY;
    }
    case ImageType::UINT_CUBE_ARRAY: {
      return GPU_TEXTURE_CUBE_ARRAY;
    }
    case ImageType::UINT_BUFFER: {
      return GPU_TEXTURE_BUFFER;
    }
    default: {
      BLI_assert_unreachable();
      return GPU_TEXTURE_2D;
    }
  };
}

eGPUSamplerFormat MSLTextureResource::get_sampler_format() const
{
  switch (this->type) {
    case ImageType::FLOAT_BUFFER:
    case ImageType::FLOAT_1D:
    case ImageType::FLOAT_1D_ARRAY:
    case ImageType::FLOAT_2D:
    case ImageType::FLOAT_2D_ARRAY:
    case ImageType::FLOAT_3D:
    case ImageType::FLOAT_CUBE:
    case ImageType::FLOAT_CUBE_ARRAY:
      return GPU_SAMPLER_TYPE_FLOAT;
    case ImageType::INT_BUFFER:
    case ImageType::INT_1D:
    case ImageType::INT_1D_ARRAY:
    case ImageType::INT_2D:
    case ImageType::INT_2D_ARRAY:
    case ImageType::INT_3D:
    case ImageType::INT_CUBE:
    case ImageType::INT_CUBE_ARRAY:
    case ImageType::INT_2D_ATOMIC:
    case ImageType::INT_3D_ATOMIC:
    case ImageType::INT_2D_ARRAY_ATOMIC:
      return GPU_SAMPLER_TYPE_INT;
    case ImageType::UINT_BUFFER:
    case ImageType::UINT_1D:
    case ImageType::UINT_1D_ARRAY:
    case ImageType::UINT_2D:
    case ImageType::UINT_2D_ARRAY:
    case ImageType::UINT_3D:
    case ImageType::UINT_CUBE:
    case ImageType::UINT_CUBE_ARRAY:
    case ImageType::UINT_2D_ATOMIC:
    case ImageType::UINT_3D_ATOMIC:
    case ImageType::UINT_2D_ARRAY_ATOMIC:
      return GPU_SAMPLER_TYPE_UINT;
    case ImageType::SHADOW_2D:
    case ImageType::SHADOW_2D_ARRAY:
    case ImageType::SHADOW_CUBE:
    case ImageType::SHADOW_CUBE_ARRAY:
    case ImageType::DEPTH_2D:
    case ImageType::DEPTH_2D_ARRAY:
    case ImageType::DEPTH_CUBE:
    case ImageType::DEPTH_CUBE_ARRAY:
      return GPU_SAMPLER_TYPE_DEPTH;
    default:
      BLI_assert_unreachable();
  }
  return GPU_SAMPLER_TYPE_FLOAT;
}

/** \} */

}  // namespace blender::gpu
