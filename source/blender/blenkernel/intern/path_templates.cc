/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <fmt/format.h>

#include "BLT_translation.hh"

#include "BLI_math_base.hh"
#include "BLI_path_utils.hh"
#include "BLI_span.hh"

#include "BKE_context.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_path_templates.hh"
#include "BKE_scene.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "DNA_ID_enums.h"
#include "DNA_node_types.h"

namespace blender::bke::path_templates {

bool VariableMap::contains(blender::StringRef name) const
{
  if (this->strings_.contains(name)) {
    return true;
  }
  if (this->filepaths_.contains(name)) {
    return true;
  }
  if (this->integers_.contains(name)) {
    return true;
  }
  if (this->floats_.contains(name)) {
    return true;
  }
  return false;
}

bool VariableMap::remove(blender::StringRef name)
{
  if (this->strings_.remove(name)) {
    return true;
  }
  if (this->filepaths_.remove(name)) {
    return true;
  }
  if (this->integers_.remove(name)) {
    return true;
  }
  if (this->floats_.remove(name)) {
    return true;
  }
  return false;
}

bool VariableMap::add_string(blender::StringRef name, blender::StringRef value)
{
  if (this->contains(name)) {
    return false;
  }
  this->strings_.add_new(name, value);
  return true;
}

bool VariableMap::add_filepath(blender::StringRef name, blender::StringRef value)
{
  if (this->contains(name)) {
    return false;
  }
  this->filepaths_.add_new(name, value);
  return true;
}

bool VariableMap::add_integer(blender::StringRef name, const int64_t value)
{
  if (this->contains(name)) {
    return false;
  }
  this->integers_.add_new(name, value);
  return true;
}

bool VariableMap::add_float(blender::StringRef name, const double value)
{
  if (this->contains(name)) {
    return false;
  }
  this->floats_.add_new(name, value);
  return true;
}

std::optional<blender::StringRefNull> VariableMap::get_string(blender::StringRef name) const
{
  const std::string *value = this->strings_.lookup_ptr(name);
  if (value == nullptr) {
    return std::nullopt;
  }
  return blender::StringRefNull(*value);
}

std::optional<blender::StringRefNull> VariableMap::get_filepath(blender::StringRef name) const
{
  const std::string *value = this->filepaths_.lookup_ptr(name);
  if (value == nullptr) {
    return std::nullopt;
  }
  return blender::StringRefNull(*value);
}

std::optional<int64_t> VariableMap::get_integer(blender::StringRef name) const
{
  const int64_t *value = this->integers_.lookup_ptr(name);
  if (value == nullptr) {
    return std::nullopt;
  }
  return *value;
}

std::optional<double> VariableMap::get_float(blender::StringRef name) const
{
  const double *value = this->floats_.lookup_ptr(name);
  if (value == nullptr) {
    return std::nullopt;
  }
  return *value;
}

bool VariableMap::add_filename_only(StringRef var_name,
                                    StringRefNull full_path,
                                    StringRef fallback)
{
  const char *file_name = BLI_path_basename(full_path.c_str());
  const char *file_name_end = BLI_path_extension_or_end(file_name);

  if (file_name[0] == '\0') {
    /* If there is no file name, default to the fallback. */
    return this->add_filepath(var_name, fallback);
  }
  else if (file_name_end == file_name) {
    /* When the filename has no extension, but starts with a period. */
    return this->add_filepath(var_name, StringRef(file_name));
  }
  else {
    /* Normal case. */
    return this->add_filepath(var_name, StringRef(file_name, file_name_end));
  }
}

bool VariableMap::add_path_up_to_file(StringRef var_name,
                                      StringRefNull full_path,
                                      StringRef fallback)
{
  /* Empty path. */
  if (full_path.is_empty()) {
    return this->add_filepath(var_name, fallback);
  }

  /* No filename at the end. */
  if (BLI_path_basename(full_path.c_str()) == full_path.end()) {
    return this->add_filepath(var_name, full_path);
  }

  Vector<char> dir_path(full_path.size() + 1);
  full_path.copy_unsafe(dir_path.data());

  const bool success = BLI_path_parent_dir(dir_path.data());

  if (!success || dir_path[0] == '\0') {
    /* If no path before the filename, default to the fallback. */
    return this->add_filepath(var_name, fallback);
  }

  return this->add_filepath(var_name, dir_path.data());
}

bool operator==(const Error &left, const Error &right)
{
  return left.type == right.type && left.byte_range == right.byte_range;
}

}  // namespace blender::bke::path_templates

using namespace blender::bke::path_templates;

std::optional<VariableMap> BKE_build_template_variables_for_prop(const bContext *C,
                                                                 PointerRNA *ptr,
                                                                 PropertyRNA *prop)
{
  /*
   * This function should be maintained such that it always produces variables
   * consistent with the variables produced elsewhere in the code base for the
   * same property. For example, render paths are processed in the rendering
   * code and the variables for that purpose are built there; this function
   * should produce variables consistent with that for the same render path
   * properties here.
   *
   * This function is organized into three sections: one for "general"
   * variables, one for "purpose-specific" variables, and one for
   * "type-specific" variables. (See the top-level documentation in
   * BKE_path_templates.hh for details on what that means).
   *
   * To add support for additional variables here:
   *
   * - For "general" variables, simply add them to
   *   #BKE_add_template_variables_general(). Nothing else special needs to be
   *   done.
   * - For "purpose-specific" variables, add them to the appropriate
   *   purpose-specific function (e.g.
   *   #BKE_add_template_variables_for_render_path()). If no function exists
   *   for your purpose yet, add a new enum item to #PropertyPathTemplateType
   *   and a corresponding new function, add your variable to the new function,
   *   and then call it from the `switch` on #RNA_property_path_template_type()
   *   below.
   * - For "type-specific" variables, add them to the appropriate type-specific
   *   function (e.g. #BKE_add_template_variables_for_node()). If no function
   *   exists for that type yet, create a new function for it, add the variable
   *   there, and then call it from the bottom section of this function, with an
   *   appropriate guard on the struct type.
   */

  /* No property passed, or it doesn't support path templates. */
  if (ptr == nullptr || prop == nullptr ||
      (RNA_property_flag(prop) & PROP_PATH_SUPPORTS_TEMPLATES) == 0)
  {
    return std::nullopt;
  }

  VariableMap variables;

  /* General variables. */
  BKE_add_template_variables_general(variables, ptr->owner_id);

  /* Purpose-specific variables. */
  switch (RNA_property_path_template_type(prop)) {
    case PROP_VARIABLES_NONE: {
      /* Do nothing: no purpose-specific variables. */
      break;
    }

    /* Scene render output path, the compositor's File Output node's paths, etc. */
    case PROP_VARIABLES_RENDER_OUTPUT: {
      const Scene *scene;
      if (GS(ptr->owner_id->name) == ID_SCE) {
        scene = reinterpret_cast<const Scene *>(ptr->owner_id);
      }
      else {
        scene = CTX_data_scene(C);
      }

      BKE_add_template_variables_for_render_path(variables, *scene);
      break;
    }
  }

  /* Type-specific variables. */

  /* Nodes. */
  if (std::optional<AncestorPointerRNA> node_rna_ptr = RNA_struct_search_closest_ancestor_by_type(
          ptr, &RNA_Node))
  {
    const bNode *bnode = reinterpret_cast<const bNode *>(node_rna_ptr->data);
    BKE_add_template_variables_for_node(variables, *bnode);
  }

  return variables;
}

void BKE_add_template_variables_general(VariableMap &variables, const ID *path_owner_id)
{
  /* Global blend filepath (a.k.a. path to the blend file that's currently
   * open). */
  {
    const char *g_blend_file_path = BKE_main_blendfile_path_from_global();

    variables.add_filename_only(
        "blend_name", g_blend_file_path, blender::StringRef(DATA_("Unsaved")));

    /* Note: fallback to `./` for unsaved files, which if used at the start of a
     * path is equivalent to the current working directory. This is consistent
     * with how `//` works. */
    variables.add_path_up_to_file("blend_dir", g_blend_file_path, blender::StringRef("./"));
  }

  /* Library blend filepath (a.k.a. path to the blend file that actually owns the ID). */
  if (path_owner_id) {
    const char *lib_blend_file_path = ID_BLEND_PATH_FROM_GLOBAL(path_owner_id);
    variables.add_filename_only(
        "blend_name_lib", lib_blend_file_path, blender::StringRef(DATA_("Unsaved")));

    /* Note: fallback to `./` for unsaved files, which if used at the start of a
     * path is equivalent to the current working directory. This is consistent
     * with how `//` works. */
    variables.add_path_up_to_file("blend_dir_lib", lib_blend_file_path, blender::StringRef("./"));
  }
}

void BKE_add_template_variables_for_render_path(VariableMap &variables, const Scene &scene)
{
  /* Resolution variables. */
  int res_x, res_y;
  BKE_render_resolution(&scene.r, false, &res_x, &res_y);
  variables.add_integer("resolution_x", res_x);
  variables.add_integer("resolution_y", res_y);

  /* FPS variable.
   *
   * FPS eval code copied from #BKE_cachefile_filepath_get().
   *
   * TODO: should probably use one function for this everywhere to ensure that
   * fps is computed consistently, but at the time of writing no such function
   * seems to exist. Every place in the code base just has its own bespoke
   * code, using different precision, etc. */
  const double fps = double(scene.r.frs_sec) / double(scene.r.frs_sec_base);
  variables.add_float("fps", fps);

  /* Scene name variable. */
  variables.add_string("scene_name", scene.id.name + 2);

  /* Camera name variable. */
  if (scene.camera) {
    variables.add_string("camera_name", scene.camera->id.name + 2);
  }
}

void BKE_add_template_variables_for_node(blender::bke::path_templates::VariableMap &variables,
                                         const bNode &owning_node)
{
  variables.add_string("node_name", owning_node.name);
}

/* -------------------------------------------------------------------- */

#define FORMAT_BUFFER_SIZE 512

namespace {

enum class FormatSpecifierType {
  /* No format specifier given. Use default formatting. */
  NONE = 0,

  /* The format specifier was a string of just "#" characters. E.g. "####". */
  INTEGER,

  /* The format specifier was a string of "#" characters with a single ".". E.g.
   * "###.##". */
  FLOAT,

  /* The format specifier was invalid due to incorrect syntax. */
  SYNTAX_ERROR,
};

/**
 * Specifies how a variable should be formatted into a string, or indicates a
 * parse error.
 */
struct FormatSpecifier {
  FormatSpecifierType type = FormatSpecifierType::NONE;

  /* For INTEGER and FLOAT formatting types, the number of digits indicated on
   * either side of the decimal point. */
  std::optional<uint8_t> integer_digit_count;
  std::optional<uint8_t> fractional_digit_count;
};

enum class TokenType {
  /* Either "{variable_name}" or "{variable_name:format_spec}". */
  VARIABLE_EXPRESSION,

  /* "{{", which is an escaped "{". */
  LEFT_CURLY_BRACE,

  /* "}}", which is an escaped "}". */
  RIGHT_CURLY_BRACE,

  /* Encountered a syntax error while trying to parse a variable expression. */
  VARIABLE_SYNTAX_ERROR,

  /* Encountered an unescaped curly brace in an invalid position. */
  UNESCAPED_CURLY_BRACE_ERROR,
};

/**
 * A token that was parsed and should be substituted in the string, or an error.
 */
struct Token {
  TokenType type = TokenType::VARIABLE_EXPRESSION;

  /* Byte index range (exclusive on the right) of the token or syntax error in
   * the path string. */
  blender::IndexRange byte_range;

  /* Reference to the variable name as written in the template string. Note
   * that this points into the template string, and does not own the value.
   *
   * Only relevant when `type == VARIABLE_EXPRESSION`. */
  blender::StringRef variable_name;

  /* Indicates how the variable's value should be formatted into a string. This
   * is derived from the format specification (e.g. the "###" in "{blah:###}").
   *
   * Only relevant when `type == VARIABLE_EXPRESSION`. */
  FormatSpecifier format;
};

}  // namespace

/**
 * Format an integer into a string, according to `format`.
 *
 * Note: if `format` is not valid for integers, the resulting string will be
 * empty.
 *
 * \return length of the produced string. Zero indicates an error.
 */
static int format_int_to_string(const FormatSpecifier &format,
                                const int64_t integer_value,
                                char r_output_string[FORMAT_BUFFER_SIZE])
{
  BLI_assert(format.type != FormatSpecifierType::SYNTAX_ERROR);

  r_output_string[0] = '\0';
  int output_length = 0;

  switch (format.type) {
    case FormatSpecifierType::NONE: {
      output_length =
          fmt::format_to_n(r_output_string, FORMAT_BUFFER_SIZE - 1, "{}", integer_value).size;
      r_output_string[output_length] = '\0';
      break;
    }

    case FormatSpecifierType::INTEGER: {
      BLI_assert(format.integer_digit_count.has_value());
      BLI_assert(*format.integer_digit_count > 0);
      output_length = fmt::format_to_n(r_output_string,
                                       FORMAT_BUFFER_SIZE - 1,
                                       "{:0{}}",
                                       integer_value,
                                       *format.integer_digit_count)
                          .size;
      r_output_string[output_length] = '\0';
      break;
    }

    case FormatSpecifierType::FLOAT: {
      /* Formatting an integer as a float: we do *not* defer to the float
       * formatter for this because we could lose precision with very large
       * numbers. Instead we simply print the integer, and then append ".000..."
       * to it. */
      BLI_assert(format.fractional_digit_count.has_value());
      BLI_assert(*format.fractional_digit_count > 0);

      if (format.integer_digit_count.has_value()) {
        BLI_assert(*format.integer_digit_count > 0);
        output_length = fmt::format_to_n(r_output_string,
                                         FORMAT_BUFFER_SIZE - 1,
                                         "{:0{}}",
                                         integer_value,
                                         *format.integer_digit_count)
                            .size;
      }
      else {
        output_length =
            fmt::format_to_n(r_output_string, FORMAT_BUFFER_SIZE - 1, "{}", integer_value).size;
      }

      r_output_string[output_length] = '.';
      output_length++;

      for (int i = 0; i < *format.fractional_digit_count && i < (FORMAT_BUFFER_SIZE - 1); i++) {
        r_output_string[output_length] = '0';
        output_length++;
      }

      r_output_string[output_length] = '\0';

      break;
    }

    case FormatSpecifierType::SYNTAX_ERROR: {
      BLI_assert_msg(
          false,
          "Format specifiers with invalid syntax should have been rejected before getting here.");
      break;
    }
  }

  return output_length;
}

/**
 * Format a floating point number into a string, according to `format`.
 *
 * Note: if `format` is not valid for floating point numbers, the resulting
 * string will be empty.
 *
 * \return length of the produced string. Zero indicates an error
 */
static int format_float_to_string(const FormatSpecifier &format,
                                  const double float_value,
                                  char r_output_string[FORMAT_BUFFER_SIZE])
{
  BLI_assert(format.type != FormatSpecifierType::SYNTAX_ERROR);

  r_output_string[0] = '\0';
  int output_length = 0;

  switch (format.type) {
    case FormatSpecifierType::NONE: {
      /* When no format specification is given, we attempt to replicate Python's
       * behavior in the same situation. The only major thing we can't replicate
       * via `libfmt` is that in Python whole numbers are printed with a trailing
       * ".0". So we handle that bit manually. */
      output_length =
          fmt::format_to_n(r_output_string, FORMAT_BUFFER_SIZE - 1, "{}", float_value).size;
      r_output_string[output_length] = '\0';

      /* If the string consists only of digits and a possible negative sign, then
       * we append a ".0" to match Python. */
      if (blender::StringRef(r_output_string).find_first_not_of("-0123456789") ==
          std::string::npos)
      {
        r_output_string[output_length] = '.';
        r_output_string[output_length + 1] = '0';
        r_output_string[output_length + 2] = '\0';
        output_length += 2;
      }
      break;
    }

    case FormatSpecifierType::INTEGER: {
      /* Defer to the integer formatter with a rounded value. */
      return format_int_to_string(format, std::round(float_value), r_output_string);
    }

    case FormatSpecifierType::FLOAT: {
      BLI_assert(format.fractional_digit_count.has_value());
      BLI_assert(*format.fractional_digit_count > 0);

      if (format.integer_digit_count.has_value()) {
        /* Both integer and fractional component lengths are specified. */
        BLI_assert(*format.integer_digit_count > 0);
        output_length = fmt::format_to_n(r_output_string,
                                         FORMAT_BUFFER_SIZE - 1,
                                         "{:0{}.{}f}",
                                         float_value,
                                         *format.integer_digit_count +
                                             *format.fractional_digit_count + 1,
                                         *format.fractional_digit_count)
                            .size;
        r_output_string[output_length] = '\0';
      }
      else {
        /* Only fractional component length is specified. */
        output_length = fmt::format_to_n(r_output_string,
                                         FORMAT_BUFFER_SIZE - 1,
                                         "{:.{}f}",
                                         float_value,
                                         *format.fractional_digit_count)
                            .size;
        r_output_string[output_length] = '\0';
      }

      break;
    }

    case FormatSpecifierType::SYNTAX_ERROR: {
      BLI_assert_msg(
          false,
          "Format specifiers with invalid syntax should have been rejected before getting here.");
      break;
    }
  }

  return output_length;
}

/**
 * Parse the "format specifier" part of a variable expression.
 *
 * The format specifier is e.g. the "##.###" in "{name:##.###}". The specifier
 * string should be passed alone (just the "##.###"), without the rest of the
 * variable expression.
 */
static FormatSpecifier parse_format_specifier(blender::StringRef format_specifier)
{
  FormatSpecifier format = {};

  /* A ":" was used, but no format specifier was given, which is invalid. */
  if (format_specifier.is_empty()) {
    format.type = FormatSpecifierType::SYNTAX_ERROR;
    return format;
  }

  /* If it's all digit specifiers, then format as an integer. */
  if (format_specifier.find_first_not_of("#") == std::string::npos) {
    format.integer_digit_count = format_specifier.size();

    format.type = FormatSpecifierType::INTEGER;
    return format;
  }

  /* If it's digit specifiers and a dot, format as a float. */
  const int64_t dot_index = format_specifier.find_first_of('.');
  const int64_t dot_index_last = format_specifier.find_last_of('.');
  const bool found_dot = dot_index != std::string::npos;
  const bool only_one_dot = dot_index == dot_index_last;
  if (format_specifier.find_first_not_of(".#") == std::string::npos && found_dot && only_one_dot) {
    blender::StringRef left = format_specifier.substr(0, dot_index);
    blender::StringRef right = format_specifier.substr(dot_index + 1);

    /* We currently require that the fractional digits are specified, so bail if
     * they aren't. */
    if (right.is_empty()) {
      format.type = FormatSpecifierType::SYNTAX_ERROR;
      return format;
    }

    if (!left.is_empty()) {
      format.integer_digit_count = left.size();
    }

    format.fractional_digit_count = right.size();

    format.type = FormatSpecifierType::FLOAT;
    return format;
  }

  format.type = FormatSpecifierType::SYNTAX_ERROR;
  return format;
}

/**
 * Find and parse the next valid token in `path` starting from index
 * `from_char`.
 *
 * \param path: The path string to parse.
 *
 * \param from_char: The char index to start from.
 *
 * \return The parsed token information, or #std::nullopt if no token is found in `path`.
 */
static std::optional<Token> next_token(blender::StringRef path, const int from_char)
{
  Token token;

  /* We use the magic number -1 here to indicate that a component hasn't been
   * found yet. When a component is found, the respective token here is set
   * to the byte offset it was found at. */
  int start = -1;                  /* "{" */
  int format_specifier_split = -1; /* ":" */
  int end = -1;                    /* "}" */

  for (int byte_index = from_char; byte_index < path.size(); byte_index++) {
    /* Check for escaped "{". */
    if (start == -1 && (byte_index + 1) < path.size() && path[byte_index] == '{' &&
        path[byte_index + 1] == '{')
    {
      Token token;
      token.type = TokenType::LEFT_CURLY_BRACE;
      token.byte_range = blender::IndexRange::from_begin_end(byte_index, byte_index + 2);
      return token;
    }

    /* Check for escaped "}".
     *
     * Note that we only do this check when not already inside a variable
     * expression, since it could be a valid closing "}" followed by additional
     * escaped closing braces. */
    if (start == -1 && (byte_index + 1) < path.size() && path[byte_index] == '}' &&
        path[byte_index + 1] == '}')
    {
      token.type = TokenType::RIGHT_CURLY_BRACE;
      token.byte_range = blender::IndexRange::from_begin_end(byte_index, byte_index + 2);
      return token;
    }

    /* Check for unescaped "}", which outside of a variable expression is
     * illegal. */
    if (start == -1 && path[byte_index] == '}') {
      token.type = TokenType::UNESCAPED_CURLY_BRACE_ERROR;
      token.byte_range = blender::IndexRange::from_begin_end(byte_index, byte_index + 1);
      return token;
    }

    /* Check if we've found a starting "{". */
    if (path[byte_index] == '{') {
      if (start != -1) {
        /* Already inside a variable expression. */
        token.type = TokenType::VARIABLE_SYNTAX_ERROR;
        token.byte_range = blender::IndexRange::from_begin_end(start, byte_index);
        return token;
      }
      start = byte_index;
      format_specifier_split = -1;
      continue;
    }

    /* If we haven't found a start, we shouldn't try to parse the other bits
     * yet. */
    if (start == -1) {
      continue;
    }

    /* Check if we've found a format splitter. */
    if (path[byte_index] == ':') {
      if (format_specifier_split == -1) {
        /* Only set if it's the first ":" we've encountered in the variable
         * expression. Subsequent ones will be handled in the format specifier
         * parsing. */
        format_specifier_split = byte_index;
      }
      continue;
    }

    /* Check if we've found the closing "}". */
    if (path[byte_index] == '}') {
      end = byte_index + 1; /* Exclusive end. */
      break;
    }
  }

  /* No variable expression found. */
  if (start == -1) {
    return std::nullopt;
  }

  /* Unclosed variable expression. Syntax error. */
  if (end == -1) {
    token.type = TokenType::VARIABLE_SYNTAX_ERROR;
    token.byte_range = blender::IndexRange::from_begin_end(start, path.size());
    return token;
  }

  /* Parse the variable expression we found. */
  token.byte_range = blender::IndexRange::from_begin_end(start, end);
  if (format_specifier_split == -1) {
    /* No format specifier. */
    token.variable_name = path.substr(start + 1, (end - 1) - (start + 1));
  }
  else {
    /* Found format specifier. */
    token.variable_name = path.substr(start + 1, format_specifier_split - (start + 1));
    token.format = parse_format_specifier(
        path.substr(format_specifier_split + 1, (end - 1) - (format_specifier_split + 1)));
    if (token.format.type == FormatSpecifierType::SYNTAX_ERROR) {
      token.type = TokenType::VARIABLE_SYNTAX_ERROR;
      return token;
    }
  }

  return token;
}

/* Parse the given template and return the list of tokens found, in the same
 * order as they appear in the template. */
static blender::Vector<Token> parse_template(blender::StringRef path)
{
  blender::Vector<Token> tokens;

  for (int bytes_read = 0; bytes_read < path.size();) {
    const std::optional<Token> token = next_token(path, bytes_read);

    if (!token.has_value()) {
      break;
    }

    bytes_read = token->byte_range.one_after_last();
    tokens.append(*token);
  }

  return tokens;
}

/* Convert a token to its corresponding syntax error. If the token doesn't have
 * an error, returns nullopt. */
static std::optional<Error> token_to_syntax_error(const Token &token)
{
  switch (token.type) {
    case TokenType::VARIABLE_SYNTAX_ERROR: {
      if (token.format.type == FormatSpecifierType::SYNTAX_ERROR) {
        return {{ErrorType::FORMAT_SPECIFIER, token.byte_range}};
      }
      else {
        return {{ErrorType::VARIABLE_SYNTAX, token.byte_range}};
      }
    }

    case TokenType::UNESCAPED_CURLY_BRACE_ERROR: {
      return {{ErrorType::UNESCAPED_CURLY_BRACE, token.byte_range}};
    }

    /* Non-errors. */
    case TokenType::VARIABLE_EXPRESSION:
    case TokenType::LEFT_CURLY_BRACE:
    case TokenType::RIGHT_CURLY_BRACE:
      return std::nullopt;
  }

  BLI_assert_msg(false, "Unhandled token type.");
  return std::nullopt;
}

bool BKE_path_contains_template_syntax(blender::StringRef path)
{
  return path.find_first_of("{}") != std::string_view::npos;
}

/**
 * Evaluates the path template in `in_path` and writes the result to `out_path`
 * if provided.
 *
 * \param out_path: buffer to write the evaluated path to. May be null, in which
 * case writing is skipped, and this function just acts to validate the
 * templating in the path.
 *
 * \param out_path_maxncpy: The maximum length that template expansion is
 * allowed to make the template-expanded path (in bytes), including the null
 * terminator. In general, this should be the size of the underlying allocation
 * of `out_path`.
 *
 * \param template_variables: map of variables and their values to use during
 * template substitution.
 *
 * \return An empty vector on success, or a vector of templating errors on
 * failure. Note that even if there are errors, `out_path` may get modified, and
 * it should be treated as bogus data in that case.
 */
static blender::Vector<Error> eval_template(char *out_path,
                                            const int out_path_maxncpy,
                                            blender::StringRef in_path,
                                            const VariableMap &template_variables)
{
  if (out_path) {
    in_path.copy_utf8_truncated(out_path, out_path_maxncpy);
  }

  const blender::Vector<Token> tokens = parse_template(in_path);

  if (tokens.is_empty()) {
    /* No tokens found, so nothing to do. */
    return {};
  }

  /* Accumulates errors as we process the tokens. */
  blender::Vector<Error> errors;

  /* Tracks the change in string length due to the modifications as we go. We
   * need this to properly map the token byte ranges to the being-modified
   * string. */
  int length_diff = 0;

  for (const Token &token : tokens) {
    /* Syntax errors. */
    if (std::optional<Error> error = token_to_syntax_error(token)) {
      errors.append(*error);
      continue;
    }

    char replacement_string[FORMAT_BUFFER_SIZE];

    switch (token.type) {
      /* Syntax errors should have been handled above. */
      case TokenType::VARIABLE_SYNTAX_ERROR:
      case TokenType::UNESCAPED_CURLY_BRACE_ERROR: {
        BLI_assert_msg(false, "Unhandled syntax error.");
        continue;
      }

      /* Curly brace escapes. */
      case TokenType::LEFT_CURLY_BRACE: {
        strcpy(replacement_string, "{");
        break;
      }
      case TokenType::RIGHT_CURLY_BRACE: {
        strcpy(replacement_string, "}");
        break;
      }

      /* Expand variable expression into the variable's value. */
      case TokenType::VARIABLE_EXPRESSION: {
        if (std::optional<blender::StringRefNull> string_value = template_variables.get_string(
                token.variable_name))
        {
          if (token.format.type != FormatSpecifierType::NONE) {
            /* String variables don't take format specifiers: error. */
            errors.append({ErrorType::FORMAT_SPECIFIER, token.byte_range});
            continue;
          }
          STRNCPY(replacement_string, string_value->c_str());
          BLI_path_make_safe_filename(replacement_string);
          break;
        }

        if (std::optional<blender::StringRefNull> path_value = template_variables.get_filepath(
                token.variable_name))
        {
          if (token.format.type != FormatSpecifierType::NONE) {
            /* Path variables don't take format specifiers: error. */
            errors.append({ErrorType::FORMAT_SPECIFIER, token.byte_range});
            continue;
          }
          STRNCPY(replacement_string, path_value->c_str());
          break;
        }

        if (std::optional<int64_t> integer_value = template_variables.get_integer(
                token.variable_name))
        {
          /* Integer variable found. */
          format_int_to_string(token.format, *integer_value, replacement_string);
          break;
        }

        if (std::optional<double> float_value = template_variables.get_float(token.variable_name))
        {
          /* Float variable found. */
          format_float_to_string(token.format, *float_value, replacement_string);
          break;
        }

        /* No matching variable found: error. */
        errors.append({ErrorType::UNKNOWN_VARIABLE, token.byte_range});
        continue;
      }
    }

    /* Perform the actual substitution with the expanded value. */
    if (out_path) {
      /* We're off the end of the available space. */
      if (token.byte_range.start() + length_diff >= out_path_maxncpy) {
        break;
      }

      BLI_string_replace_range(out_path,
                               out_path_maxncpy,
                               token.byte_range.start() + length_diff,
                               token.byte_range.one_after_last() + length_diff,
                               replacement_string);

      length_diff -= token.byte_range.size();
      length_diff += strlen(replacement_string);
    }
  }

  return errors;
}

blender::Vector<Error> BKE_path_validate_template(
    blender::StringRef path, const blender::bke::path_templates::VariableMap &template_variables)
{
  return eval_template(nullptr, 0, path, template_variables);
}

blender::Vector<Error> BKE_path_apply_template(char *path,
                                               int path_maxncpy,
                                               const VariableMap &template_variables)
{
  BLI_assert(path != nullptr);

  blender::Vector<char> path_buffer(path_maxncpy);

  const blender::Vector<Error> errors = eval_template(
      path_buffer.data(), path_buffer.size(), path, template_variables);

  if (errors.is_empty()) {
    /* No errors, so copy the modified path back to the original. */
    BLI_strncpy(path, path_buffer.data(), path_maxncpy);
  }
  return errors;
}

std::string BKE_path_template_error_to_string(const Error &error, blender::StringRef path)
{
  blender::StringRef subpath = path.substr(error.byte_range.start(), error.byte_range.size());

  switch (error.type) {
    case ErrorType::UNESCAPED_CURLY_BRACE: {
      return std::string("Unescaped curly brace '") + subpath + "'.";
    }

    case ErrorType::VARIABLE_SYNTAX: {
      return std::string("Invalid or incomplete template expression '") + subpath + "'.";
    }

    case ErrorType::FORMAT_SPECIFIER: {
      return std::string("Invalid format specifier in template expression '") + subpath + "'.";
    }

    case ErrorType::UNKNOWN_VARIABLE: {
      return std::string("Unknown variable referenced in template expression '") + subpath + "'.";
    }
  }

  BLI_assert_msg(false, "Unhandled error type.");
  return "Unknown error.";
}

void BKE_report_path_template_errors(ReportList *reports,
                                     const eReportType report_type,
                                     blender::StringRef path,
                                     blender::Span<Error> errors)
{
  BLI_assert(!errors.is_empty());

  std::string error_message = "Parse errors in path '" + path + "':";
  for (const Error &error : errors) {
    error_message += "\n- " + BKE_path_template_error_to_string(error, path);
  }

  BKE_report(reports, report_type, error_message.c_str());
}

std::optional<std::string> BKE_path_template_format_float(
    const blender::StringRef format_specifier, const double value)
{
  const FormatSpecifier format = parse_format_specifier(format_specifier);
  if (format.type == FormatSpecifierType::SYNTAX_ERROR) {
    return std::nullopt;
  }
  char buffer[FORMAT_BUFFER_SIZE];
  format_float_to_string(format, value, buffer);
  return buffer;
}

std::optional<std::string> BKE_path_template_format_int(const blender::StringRef format_specifier,
                                                        const int64_t value)
{
  const FormatSpecifier format = parse_format_specifier(format_specifier);
  if (format.type == FormatSpecifierType::SYNTAX_ERROR) {
    return std::nullopt;
  }
  char buffer[FORMAT_BUFFER_SIZE];
  format_int_to_string(format, value, buffer);
  return buffer;
}
