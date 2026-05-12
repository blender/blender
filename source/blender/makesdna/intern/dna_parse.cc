/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2002-2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 *
 * \brief Parser for DNA header files used by `makesdna`.
 *
 * The parser understands a basic subset of C/C++ needed for parsing structs
 * and their members. Code surrounded by `#ifdef __cplusplus` is skipped to
 * avoid having to parse more advanced C++.
 */

#include "dna_parse.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>

#include "BLI_map.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_string_ref.hh"
#include "BLI_utildefines.h"

#include "dna_utils.h"

namespace blender {

int debugSDNA = 0;

namespace dna {

#define DEBUG_PRINTF(debug_level, ...) \
  { \
    if (debugSDNA > debug_level) { \
      printf(__VA_ARGS__); \
    } \
  } \
  ((void)0)

/* -------------------------------------------------------------------- */
/** \name File I/O
 * \{ */

Span<const char *> default_dna_header_filenames()
{
  static const char *files[] = {
#include "dna_includes_as_strings.h"
  };
  return Span<const char *>(files, sizeof(files) / sizeof(files[0]));
}

[[nodiscard]] static bool read_file_data(const StringRefNull filepath, Vector<char> &data)
{
  std::ifstream file(filepath, std::ios::binary | std::ios::ate);
  if (!file) {
    return false;
  }
  const std::streamsize size = file.tellg();
  if (size < 0) {
    return false;
  }
  file.seekg(0, std::ios::beg);
  data.resize(size);
  return file.read(data.data(), size) ? true : false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Token Stream
 * \{ */

/* Token kind is either an identifier or number, or an ascii character. */
using TokenKind = int;
constexpr TokenKind TOKEN_IDENTIFIER = 256;
constexpr TokenKind TOKEN_NUMBER = 257;
constexpr TokenKind TOKEN_END = 258;

struct Token {
  TokenKind kind;
  StringRef text;
};

class TokenStream {
  Vector<Token> tokens_;
  int64_t pos_ = 0;

 public:
  void append(Token &&token)
  {
    tokens_.append(std::move(token));
  }

  bool at_end() const
  {
    return pos_ >= tokens_.size();
  }

  TokenKind kind(int64_t offset = 0) const
  {
    return (pos_ + offset < tokens_.size()) ? tokens_[pos_ + offset].kind : TOKEN_END;
  }

  const Token &peek(int64_t offset = 0) const
  {
    return tokens_[pos_ + offset];
  }

  const Token &consume()
  {
    return tokens_[pos_++];
  }

  void advance()
  {
    pos_++;
  }

  bool consume(TokenKind k)
  {
    if (this->kind() == k) {
      pos_++;
      return true;
    }
    return false;
  }

  bool consume_keyword(StringRef kw)
  {
    if (this->kind() == TOKEN_IDENTIFIER && this->peek().text == kw) {
      pos_++;
      return true;
    }
    return false;
  }

  bool consume_expression(StringRef expr)
  {
    int64_t n = 0;

    while (!expr.is_empty()) {
      if (this->kind(n) == TOKEN_END) {
        return false;
      }

      if (expr[0] == ' ') {
        expr = expr.drop_prefix(1);
        continue;
      }

      const StringRef text = this->peek(n).text;
      if (!expr.startswith(text)) {
        return false;
      }
      expr = expr.drop_prefix(text.size());
      n++;
    }

    pos_ += n;
    return true;
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tokenizer
 * \{ */

static bool is_identifier_start(char c)
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool is_identifier_continuation(char c)
{
  return is_identifier_start(c) || (c >= '0' && c <= '9');
}

/** Turn a DNA header into a token stream for parsing. Tokens will reference the
 * source string data so it must be kept alive. */
static TokenStream tokenize_dna_header(StringRef source)
{
  TokenStream stream;
  const char *cur = source.begin();
  const char *end = source.end();

  while (cur < end) {
    const char c = *cur;

    /* White-space. */
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      cur++;
      continue;
    }

    /* Discard C++ style comment. */
    if (c == '/' && cur + 1 < end && cur[1] == '/') {
      cur += 2;
      while (cur < end && *cur != '\n') {
        cur++;
      }
      continue;
    }

    /* Discard C style comment. */
    if (c == '/' && cur + 1 < end && cur[1] == '*') {
      cur += 2;
      while (cur + 1 < end && !(cur[0] == '*' && cur[1] == '/')) {
        cur++;
      }
      if (cur + 1 < end) {
        cur += 2;
      }
      continue;
    }

    /* Discard string literal. */
    if (c == '"') {
      cur++;
      while (cur < end && *cur != '"') {
        if (*cur == '\\' && cur + 1 < end) {
          cur++;
        }
        cur++;
      }
      if (cur < end) {
        cur++;
      }
      continue;
    }

    /* Add identifier, :: scope separators are considered part of it. */
    if (is_identifier_start(c)) {
      const char *begin = cur;
      while (cur < end && is_identifier_continuation(*cur)) {
        cur++;
      }
      while (cur + 1 < end && cur[0] == ':' && cur[1] == ':') {
        cur += 2;
        while (cur < end && is_identifier_continuation(*cur)) {
          cur++;
        }
      }
      StringRef text(begin, cur - begin);
      stream.append({TOKEN_IDENTIFIER, text});
      continue;
    }

    /* Number. */
    if (c >= '0' && c <= '9') {
      const char *begin = cur;
      while (cur < end && *cur >= '0' && *cur <= '9') {
        cur++;
      }
      stream.append({TOKEN_NUMBER, StringRef(begin, cur - begin)});
      continue;
    }

    /* Add anything else as a single character token. */
    stream.append({c, StringRef(cur, 1)});
    cur++;
  }

  return stream;
}

/** Strip tokens that we want to ignore for parsing. */
static TokenStream strip_ignored_tokens(TokenStream stream)
{
  TokenStream stripped_stream;
  while (!stream.at_end()) {
    /* Skip blocks with C++ code that we can't parse. */
    if (stream.consume_expression("#ifdef __cplusplus") ||
        stream.consume_expression("#if defined(__cplusplus)"))
    {
      while (!stream.at_end() && !stream.consume_expression("#endif")) {
        stream.advance();
      }
      continue;
    }
    /* Skip DNA macros that we don't need to parse. */
    if (stream.consume_keyword("DNA_DEPRECATED")) {
      continue;
    }
    if (stream.consume_keyword("DNA_DEFINE_CXX_METHODS")) {
      while (!stream.at_end() && stream.kind() != ')') {
        stream.advance();
      }
      if (!stream.at_end()) {
        stream.advance();
      }
      continue;
    }
    stripped_stream.append(Token{stream.consume()});
  }
  return stripped_stream;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Name Validation
 * \{ */

/** Validate a struct type name. */
static bool is_valid_type_name(const StringRefNull type_name, const StringRefNull filepath)
{
  if (type_name.is_empty()) {
    fprintf(stderr,
            "File '%s' contains struct we can't parse \"%s\"\n",
            filepath.c_str(),
            type_name.c_str());
    return false;
  }
  if (ELEM(type_name, "long", "ulong")) {
    fprintf(stderr,
            "File '%s' contains use of \"%s\" in DNA struct which is not allowed\n",
            filepath.c_str(),
            type_name.c_str());
    return false;
  }
  return true;
}

/** Validate a member name. */
static bool is_valid_member_name(const StringRefNull name, const StringRefNull filepath)
{
  /* Strip pointer/array decorators: e.g. `*var[3]` -> `var`. */
  const StringRef name_strip = DNA_member_id_string_ref(name);

  /* Enforce '_pad123' naming convention, disallow 'pad123' or 'pad_123',
   * special exception for [a-z] after since there is a 'pad_rot_angle' preference. */
  if (!name_strip.is_empty() && name_strip[0] == '_' && !name_strip.startswith("_pad")) {
    fprintf(stderr,
            "File '%s': only '_pad' variables can start with an underscore, found '%s'\n",
            filepath.c_str(),
            name.c_str());
    return false;
  }

  if (name_strip.startswith("pad")) {
    const StringRef rest = name_strip.drop_prefix(3);
    if (!rest.is_empty() && rest[0] >= 'a' && rest[0] <= 'z') {
      /* May be part of a word, allow that. */
      return true;
    }
    bool has_only_digit_or_none = true;
    for (char c : rest) {
      if (!((c >= '0' && c <= '9') || c == '_')) {
        has_only_digit_or_none = false;
        break;
      }
    }
    if (has_only_digit_or_none) {
      fprintf(stderr,
              "File '%s': padding variables must be formatted '_pad[number]', found '%s'\n",
              filepath.c_str(),
              name.c_str());
      return false;
    }
  }

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Parser
 * \{ */

/** Consume a balanced `open ... close` group. The current token must be #open. */
static void skip_balanced(TokenStream &stream, TokenKind open, TokenKind close)
{
  stream.advance();
  int depth = 1;
  while (!stream.at_end() && depth > 0) {
    const TokenKind k = stream.consume().kind;
    if (k == open) {
      depth++;
    }
    else if (k == close) {
      depth--;
    }
  }
}

/** Skip member default initializer from `=` until `;`. */
static void skip_initializer(TokenStream &stream)
{
  int depth = 0;
  while (!stream.at_end()) {
    const TokenKind k = stream.kind();
    if (depth == 0 && (k == ',' || k == ';')) {
      return;
    }
    if (k == '{' || k == '(') {
      depth++;
    }
    else if (k == '}' || k == ')') {
      depth--;
    }
    stream.advance();
  }
}

/** Append array `[size]` suffixes to the name. */
static void append_array_suffix(TokenStream &stream, std::string &out)
{
  while (!stream.at_end() && stream.kind() == '[') {
    out += '[';
    stream.advance();
    int depth = 1;
    while (!stream.at_end() && depth > 0) {
      const Token &t = stream.consume();
      out += t.text;
      if (t.kind == '[') {
        depth++;
      }
      else if (t.kind == ']') {
        depth--;
      }
    }
  }
}

/** Append template `<...>` arguments to the name. */
static void append_template_suffix(TokenStream &stream, std::string &out)
{
  if (!stream.consume('<')) {
    return;
  }
  out += '<';
  int depth = 1;
  while (!stream.at_end() && depth > 0) {
    const Token &t = stream.consume();
    out += t.text;
    if (t.kind == '<') {
      depth++;
    }
    else if (t.kind == '>') {
      depth--;
    }
  }
}

/**
 * Parse a parenthesized declaration: `(*name)(args)` (function pointer) or
 * `(*name)[N][M]` (multi-dimensional array pointer). Both are stored in SDNA
 * as `(*name)()`.
 */
[[nodiscard]] static bool parse_paren_declaration(TokenStream &stream, std::string &out)
{
  stream.advance();
  out += '(';
  while (stream.consume('*')) {
    out += '*';
  }
  if (stream.at_end() || stream.kind() != TOKEN_IDENTIFIER) {
    return false;
  }
  out += stream.consume().text;
  if (!stream.consume(')')) {
    return false;
  }
  out += ')';

  if (!stream.at_end() && stream.kind() == '(') {
    skip_balanced(stream, '(', ')');
  }
  else {
    while (!stream.at_end() && stream.kind() == '[') {
      skip_balanced(stream, '[', ']');
    }
  }
  out += "()";
  return true;
}

/** Parse one member declaration. */
[[nodiscard]] static bool parse_member_declaration(TokenStream &stream, std::string &out)
{
  out.clear();

  /* Pointer prefixes. */
  while (stream.consume('*')) {
    out += '*';
  }

  /* Function pointer or multi-dimensional array pointer: `(*name)...` */
  if (!stream.at_end() && stream.kind() == '(') {
    return parse_paren_declaration(stream, out);
  }

  /* Plain identifier with potential array suffixes. */
  if (stream.at_end() || stream.kind() != TOKEN_IDENTIFIER) {
    return false;
  }
  out += stream.consume().text;
  append_array_suffix(stream, out);
  return true;
}

/** Parse the body of a struct. */
[[nodiscard]] static bool parse_struct_body(TokenStream &stream,
                                            const StringRefNull filepath,
                                            ParsedStruct &r_struct)
{
  while (!stream.at_end() && stream.kind() != '}') {
    /* Skip qualifiers. */
    while (stream.consume_keyword("struct") || stream.consume_keyword("unsigned") ||
           stream.consume_keyword("const"))
    {
    }

    if (stream.at_end() || stream.kind() != TOKEN_IDENTIFIER) {
      fprintf(stderr,
              "File '%s' contains a member declaration that can't be parsed\n",
              filepath.c_str());
      return false;
    }

    /* Consume the identifier and any template arguments (e.g. `ListBaseT<Foo>`)
     * into the type name. `substitute_cpp_types` later rewrites templated type
     * names into their canonical SDNA form. */
    std::string type_name(stream.consume().text);
    append_template_suffix(stream, type_name);
    if (!is_valid_type_name(type_name, filepath)) {
      return false;
    }

    DEBUG_PRINTF(1, "\t|\t|\tfound type %s (", type_name.c_str());

    /* Comma-separated declarations terminated by `;`. */
    while (true) {
      std::string member_name;
      if (!parse_member_declaration(stream, member_name)) {
        fprintf(stderr,
                "File '%s' contains a member declaration that can't be parsed\n",
                filepath.c_str());
        return false;
      }
      if (!is_valid_member_name(member_name, filepath)) {
        return false;
      }

      DEBUG_PRINTF(1, "%s ||", member_name.c_str());

      r_struct.members.append({.type_name = type_name, .member_name = member_name});

      if (stream.consume('=')) {
        skip_initializer(stream);
      }
      if (stream.consume(';')) {
        break;
      }
      if (!stream.consume(',')) {
        fprintf(stderr,
                "File '%s' contains a member declaration that can't be parsed\n",
                filepath.c_str());
        return false;
      }
    }

    DEBUG_PRINTF(1, ")\n");
  }
  if (!stream.consume('}')) {
    fprintf(stderr, "File '%s' contains a struct without a closing brace\n", filepath.c_str());
    return false;
  }
  return true;
}

/** Skip the body of a struct by consuming tokens up to the matching `}`. */
static void skip_struct_body(TokenStream &stream)
{
  int depth = 1;
  while (!stream.at_end() && depth > 0) {
    const TokenKind k = stream.consume().kind;
    if (k == '{') {
      depth++;
    }
    else if (k == '}') {
      depth--;
    }
  }
}

/** Parse an enum declaration with its underlying type. */
[[nodiscard]] static bool parse_enum_declaration(TokenStream &stream,
                                                 const StringRefNull filepath,
                                                 Vector<ParsedEnum> &r_enums)
{
  stream.consume_keyword("class");
  stream.consume_keyword("struct");

  std::string name;
  if (!stream.at_end() && stream.kind() == TOKEN_IDENTIFIER) {
    name = stream.consume().text;
  }

  std::string underlying_type;
  if (stream.consume(':')) {
    if (stream.at_end() || stream.kind() != TOKEN_IDENTIFIER) {
      fprintf(stderr,
              "File '%s' contains enum '%s' with unparseable underlying type\n",
              filepath.c_str(),
              name.c_str());
      return false;
    }
    underlying_type = stream.consume().text;
  }

  if (!stream.consume(';')) {
    if (stream.at_end() || stream.kind() != '{') {
      return true;
    }
    skip_balanced(stream, '{', '}');
  }

  if (!name.empty()) {
    r_enums.append({.type_name = std::move(name), .underlying_type = std::move(underlying_type)});
  }

  return true;
}

static bool parse_dna_header(const StringRefNull filepath,
                             Vector<ParsedStruct> &r_structs,
                             Vector<ParsedEnum> &r_enums)
{
  Vector<char> buffer;
  if (!read_file_data(filepath, buffer)) {
    fprintf(stderr, "Can't read file %s\n", filepath.c_str());
    return false;
  }

  TokenStream stream = tokenize_dna_header(StringRef(buffer.data(), buffer.size()));
  stream = strip_ignored_tokens(stream);

  bool skip_next_struct = false;

  while (!stream.at_end()) {
    /* `# #` markers in the source flag the next struct definition to be skipped. */
    if (stream.kind() == '#' && stream.kind(1) == '#') {
      stream.advance();
      stream.advance();
      skip_next_struct = true;
      continue;
    }

    if (stream.consume_keyword("enum")) {
      if (!parse_enum_declaration(stream, filepath, r_enums)) {
        return false;
      }
      continue;
    }

    if (!stream.consume_keyword("struct")) {
      stream.advance();
      continue;
    }
    if (stream.kind() != TOKEN_IDENTIFIER) {
      continue;
    }
    const StringRef name = stream.consume().text;
    if (!stream.consume('{')) {
      continue;
    }

    if (skip_next_struct) {
      skip_next_struct = false;
      skip_struct_body(stream);
      continue;
    }

    const std::string struct_name(name);
    if (!is_valid_type_name(struct_name, filepath)) {
      return false;
    }

    ParsedStruct parsed_struct;
    parsed_struct.type_name = struct_name;
    DEBUG_PRINTF(1, "\t|\t|-- detected struct %s\n", parsed_struct.type_name.c_str());

    if (!parse_struct_body(stream, filepath, parsed_struct)) {
      return false;
    }

    r_structs.append(std::move(parsed_struct));
  }

  return true;
}

bool parse_dna_headers(const StringRefNull base_directory,
                       Vector<ParsedStruct> &r_structs,
                       Vector<ParsedEnum> &r_enums,
                       Span<const char *> include_files)
{
  for (const char *filename : include_files) {
    const std::string path = std::string(base_directory) + filename;
    if (!parse_dna_header(path, r_structs, r_enums)) {
      return false;
    }
  }
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Type Substitution
 * \{ */

/** `ListBaseT<...>` -> `ListBase`. */
static void substitute_listbase_t(ParsedMember &member)
{
  if (member.type_name.starts_with("ListBaseT<") && member.type_name.ends_with(">")) {
    member.type_name = "ListBase";
  }
}

/** Replace vector and matrix types with C arrays. */
struct CppVectorMatrixTypeMap {
  StringRef cpp_type;
  StringRef scalar;
  int dimensions[2];
  int alignment;
};

static const CppVectorMatrixTypeMap cpp_array_type_mappings[] = {
    {"int2", "int", {2, 0}, alignof(blender::int2)},
    {"int3", "int", {3, 0}, alignof(blender::int3)},
    {"int4", "int", {4, 0}, alignof(blender::int4)},
    {"float2", "float", {2, 0}, alignof(blender::float2)},
    {"float3", "float", {3, 0}, alignof(blender::float3)},
    {"float4", "float", {4, 0}, alignof(blender::float4)},
    {"float2x2", "float", {2, 2}, alignof(blender::float2x2)},
    {"float3x3", "float", {3, 3}, alignof(blender::float3x3)},
    {"float4x4", "float", {4, 4}, alignof(blender::float4x4)},
};

static_assert(sizeof(blender::int2) == sizeof(int[2]));
static_assert(sizeof(blender::int3) == sizeof(int[3]));
static_assert(sizeof(blender::int4) == sizeof(int[4]));
static_assert(sizeof(blender::float2) == sizeof(float[2]));
static_assert(sizeof(blender::float3) == sizeof(float[3]));
static_assert(sizeof(blender::float4) == sizeof(float[4]));
static_assert(sizeof(blender::float2x2) == sizeof(float[2][2]));
static_assert(sizeof(blender::float3x3) == sizeof(float[3][3]));
static_assert(sizeof(blender::float4x4) == sizeof(float[4][4]));

static void substitute_vector_or_matrix(ParsedMember &member)
{
  const CppVectorMatrixTypeMap *mapping = nullptr;
  for (const CppVectorMatrixTypeMap &m : cpp_array_type_mappings) {
    if (member.type_name == m.cpp_type) {
      mapping = &m;
      break;
    }
  }
  if (mapping == nullptr || member.member_name.empty()) {
    return;
  }
  member.type_name = mapping->scalar;
  member.alignment = mapping->alignment;

  /* Members like `float (*var)[3][3]` have the `[3][3]` stripped in
   * #parse_paren_declaration. So also don't add it for `float3x3 *var` either,
   * rather keep it as `float` `*var`. */
  if (member.member_name[0] != '*' && member.member_name[0] != '(') {
    member.member_name += '[';
    member.member_name += char('0' + mapping->dimensions[0]);
    member.member_name += ']';
    if (mapping->dimensions[1] != 0) {
      member.member_name += '[';
      member.member_name += char('0' + mapping->dimensions[1]);
      member.member_name += ']';
    }
  }
}

/** Replace member types matching an enum with the enum's underlying type. */
[[nodiscard]] static bool substitute_enum(const ParsedStruct &parsed_struct,
                                          ParsedMember &member,
                                          const Map<StringRef, const ParsedEnum *> &enum_map)
{
  const ParsedEnum *const *found = enum_map.lookup_ptr(member.type_name);
  if (found == nullptr) {
    return true;
  }
  const ParsedEnum &parsed_enum = **found;
  if (parsed_enum.underlying_type.empty()) {
    fprintf(stderr,
            "Struct '%s' member '%s' uses enum '%s' without an explicit underlying type\n",
            parsed_struct.type_name.c_str(),
            member.member_name.c_str(),
            parsed_enum.type_name.c_str());
    return false;
  }
  member.type_name = parsed_enum.underlying_type;
  return true;
}

/** Map modern C integer type names to old SDNA names. */
static void substitute_sdna_integer_type(ParsedMember &member)
{
  static const std::pair<const char *, const char *> integer_type_aliases[] = {
      {"uint8_t", "uchar"},
      {"int16_t", "short"},
      {"uint16_t", "ushort"},
      {"int32_t", "int"},
      {"uint32_t", "int"},
  };

  for (const auto &[alias, sdna_name] : integer_type_aliases) {
    if (member.type_name == alias) {
      member.type_name = sdna_name;
      break;
    }
  }
}

bool substitute_cpp_types(Vector<ParsedStruct> &structs,
                          const Span<ParsedEnum> enums,
                          bool /*for_rna*/)
{
  Map<StringRef, const ParsedEnum *> enum_map;
  enum_map.reserve(enums.size());
  for (const ParsedEnum &parsed_enum : enums) {
    enum_map.add(parsed_enum.type_name, &parsed_enum);
  }

  for (ParsedStruct &parsed_struct : structs) {
    for (ParsedMember &member : parsed_struct.members) {
      substitute_listbase_t(member);
      substitute_vector_or_matrix(member);
      if (!substitute_enum(parsed_struct, member, enum_map)) {
        return false;
      }
      substitute_sdna_integer_type(member);
    }
  }
  return true;
}

/** \} */

}  // namespace dna
}  // namespace blender
