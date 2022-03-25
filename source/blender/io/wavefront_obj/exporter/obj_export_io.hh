/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#pragma once

#include <cstdio>
#include <string>
#include <system_error>
#include <type_traits>
#include <vector>

#include "BLI_compiler_attrs.h"
#include "BLI_fileops.h"
#include "BLI_string_ref.hh"
#include "BLI_utility_mixins.hh"

namespace blender::io::obj {

enum class eFileType {
  OBJ,
  MTL,
};

enum class eOBJSyntaxElement {
  vertex_coords,
  uv_vertex_coords,
  normal,
  poly_element_begin,
  vertex_uv_normal_indices,
  vertex_normal_indices,
  vertex_uv_indices,
  vertex_indices,
  poly_element_end,
  poly_usemtl,
  edge,
  cstype,
  nurbs_degree,
  curve_element_begin,
  curve_element_end,
  nurbs_parameter_begin,
  nurbs_parameters,
  nurbs_parameter_end,
  nurbs_group_end,
  new_line,
  mtllib,
  smooth_group,
  object_group,
  object_name,
  /* Use rarely. New line is NOT included for string. */
  string,
};

enum class eMTLSyntaxElement {
  newmtl,
  Ni,
  d,
  Ns,
  illum,
  Ka,
  Kd,
  Ks,
  Ke,
  map_Kd,
  map_Ks,
  map_Ns,
  map_d,
  map_refl,
  map_Ke,
  map_Bump,
  /* Use rarely. New line is NOT included for string. */
  string,
};

template<eFileType filetype> struct FileTypeTraits;

/* Used to prevent mixing of say OBJ file format with MTL syntax elements. */
template<> struct FileTypeTraits<eFileType::OBJ> {
  using SyntaxType = eOBJSyntaxElement;
};

template<> struct FileTypeTraits<eFileType::MTL> {
  using SyntaxType = eMTLSyntaxElement;
};

struct FormattingSyntax {
  /* Formatting syntax with the file format key like `newmtl %s\n`. */
  const char *fmt = nullptr;
  /* Number of arguments needed by the syntax. */
  const int total_args = 0;
  /* Whether types of the given arguments are accepted by the syntax above. Fail to compile by
   * default.
   */
  const bool are_types_valid = false;
};

/**
 * Type dependent but always false. Use to add a `constexpr` conditional compile-time error.
 */
template<typename T> struct always_false : std::false_type {
};

template<typename... T>
constexpr bool is_type_float = (... && std::is_floating_point_v<std::decay_t<T>>);

template<typename... T>
constexpr bool is_type_integral = (... && std::is_integral_v<std::decay_t<T>>);

template<typename... T>
constexpr bool is_type_string_related = (... && std::is_constructible_v<std::string, T>);

/* GCC (at least 9.3) while compiling the obj_exporter_tests.cc with optimizations on,
 * results in "obj_export_io.hh:205:18: warning: ‘%s’ directive output truncated writing 34 bytes
 * into a region of size 6" and similar warnings. Yes the output is truncated, and that is covered
 * as an edge case by tests on purpose. */
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wformat-truncation"
#endif
template<typename... T>
constexpr FormattingSyntax syntax_elem_to_formatting(const eOBJSyntaxElement key)
{
  switch (key) {
    case eOBJSyntaxElement::vertex_coords: {
      return {"v %f %f %f\n", 3, is_type_float<T...>};
    }
    case eOBJSyntaxElement::uv_vertex_coords: {
      return {"vt %f %f\n", 2, is_type_float<T...>};
    }
    case eOBJSyntaxElement::normal: {
      return {"vn %.4f %.4f %.4f\n", 3, is_type_float<T...>};
    }
    case eOBJSyntaxElement::poly_element_begin: {
      return {"f", 0, is_type_string_related<T...>};
    }
    case eOBJSyntaxElement::vertex_uv_normal_indices: {
      return {" %d/%d/%d", 3, is_type_integral<T...>};
    }
    case eOBJSyntaxElement::vertex_normal_indices: {
      return {" %d//%d", 2, is_type_integral<T...>};
    }
    case eOBJSyntaxElement::vertex_uv_indices: {
      return {" %d/%d", 2, is_type_integral<T...>};
    }
    case eOBJSyntaxElement::vertex_indices: {
      return {" %d", 1, is_type_integral<T...>};
    }
    case eOBJSyntaxElement::poly_usemtl: {
      return {"usemtl %s\n", 1, is_type_string_related<T...>};
    }
    case eOBJSyntaxElement::edge: {
      return {"l %d %d\n", 2, is_type_integral<T...>};
    }
    case eOBJSyntaxElement::cstype: {
      return {"cstype bspline\n", 0, is_type_string_related<T...>};
    }
    case eOBJSyntaxElement::nurbs_degree: {
      return {"deg %d\n", 1, is_type_integral<T...>};
    }
    case eOBJSyntaxElement::curve_element_begin: {
      return {"curv 0.0 1.0", 0, is_type_string_related<T...>};
    }
    case eOBJSyntaxElement::nurbs_parameter_begin: {
      return {"parm u 0.0", 0, is_type_string_related<T...>};
    }
    case eOBJSyntaxElement::nurbs_parameters: {
      return {" %f", 1, is_type_float<T...>};
    }
    case eOBJSyntaxElement::nurbs_parameter_end: {
      return {" 1.0\n", 0, is_type_string_related<T...>};
    }
    case eOBJSyntaxElement::nurbs_group_end: {
      return {"end\n", 0, is_type_string_related<T...>};
    }
    case eOBJSyntaxElement::poly_element_end: {
      ATTR_FALLTHROUGH;
    }
    case eOBJSyntaxElement::curve_element_end: {
      ATTR_FALLTHROUGH;
    }
    case eOBJSyntaxElement::new_line: {
      return {"\n", 0, is_type_string_related<T...>};
    }
    case eOBJSyntaxElement::mtllib: {
      return {"mtllib %s\n", 1, is_type_string_related<T...>};
    }
    case eOBJSyntaxElement::smooth_group: {
      return {"s %d\n", 1, is_type_integral<T...>};
    }
    case eOBJSyntaxElement::object_group: {
      return {"g %s\n", 1, is_type_string_related<T...>};
    }
    case eOBJSyntaxElement::object_name: {
      return {"o %s\n", 1, is_type_string_related<T...>};
    }
    case eOBJSyntaxElement::string: {
      return {"%s", 1, is_type_string_related<T...>};
    }
  }
}

template<typename... T>
constexpr FormattingSyntax syntax_elem_to_formatting(const eMTLSyntaxElement key)
{
  switch (key) {
    case eMTLSyntaxElement::newmtl: {
      return {"newmtl %s\n", 1, is_type_string_related<T...>};
    }
    case eMTLSyntaxElement::Ni: {
      return {"Ni %.6f\n", 1, is_type_float<T...>};
    }
    case eMTLSyntaxElement::d: {
      return {"d %.6f\n", 1, is_type_float<T...>};
    }
    case eMTLSyntaxElement::Ns: {
      return {"Ns %.6f\n", 1, is_type_float<T...>};
    }
    case eMTLSyntaxElement::illum: {
      return {"illum %d\n", 1, is_type_integral<T...>};
    }
    case eMTLSyntaxElement::Ka: {
      return {"Ka %.6f %.6f %.6f\n", 3, is_type_float<T...>};
    }
    case eMTLSyntaxElement::Kd: {
      return {"Kd %.6f %.6f %.6f\n", 3, is_type_float<T...>};
    }
    case eMTLSyntaxElement::Ks: {
      return {"Ks %.6f %.6f %.6f\n", 3, is_type_float<T...>};
    }
    case eMTLSyntaxElement::Ke: {
      return {"Ke %.6f %.6f %.6f\n", 3, is_type_float<T...>};
    }
    /* Keep only one space between options since filepaths may have leading spaces too. */
    case eMTLSyntaxElement::map_Kd: {
      return {"map_Kd %s %s\n", 2, is_type_string_related<T...>};
    }
    case eMTLSyntaxElement::map_Ks: {
      return {"map_Ks %s %s\n", 2, is_type_string_related<T...>};
    }
    case eMTLSyntaxElement::map_Ns: {
      return {"map_Ns %s %s\n", 2, is_type_string_related<T...>};
    }
    case eMTLSyntaxElement::map_d: {
      return {"map_d %s %s\n", 2, is_type_string_related<T...>};
    }
    case eMTLSyntaxElement::map_refl: {
      return {"map_refl %s %s\n", 2, is_type_string_related<T...>};
    }
    case eMTLSyntaxElement::map_Ke: {
      return {"map_Ke %s %s\n", 2, is_type_string_related<T...>};
    }
    case eMTLSyntaxElement::map_Bump: {
      return {"map_Bump %s %s\n", 2, is_type_string_related<T...>};
    }
    case eMTLSyntaxElement::string: {
      return {"%s", 1, is_type_string_related<T...>};
    }
  }
}
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif

/**
 * File format and syntax agnostic file buffer writer.
 * All writes are done into an internal chunked memory buffer
 * (list of default 64 kilobyte blocks).
 * Call write_fo_file once in a while to write the memory buffer(s)
 * into the given file.
 */
template<eFileType filetype,
         size_t buffer_chunk_size = 64 * 1024,
         size_t write_local_buffer_size = 1024>
class FormatHandler : NonCopyable, NonMovable {
 private:
  typedef std::vector<char> VectorChar;
  std::vector<VectorChar> blocks_;

 public:
  /* Write contents to the buffer(s) into a file, and clear the buffers. */
  void write_to_file(FILE *f)
  {
    for (const auto &b : blocks_)
      fwrite(b.data(), 1, b.size(), f);
    blocks_.clear();
  }

  std::string get_as_string() const
  {
    std::string s;
    for (const auto &b : blocks_)
      s.append(b.data(), b.size());
    return s;
  }
  size_t get_block_count() const
  {
    return blocks_.size();
  }

  void append_from(FormatHandler<filetype, buffer_chunk_size, write_local_buffer_size> &v)
  {
    blocks_.insert(blocks_.end(),
                   std::make_move_iterator(v.blocks_.begin()),
                   std::make_move_iterator(v.blocks_.end()));
    v.blocks_.clear();
  }

  /**
   * Example invocation: `writer->write<eMTLSyntaxElement::newmtl>("foo")`.
   *
   * \param key: Must match what the instance's filetype expects; i.e., `eMTLSyntaxElement` for
   * `eFileType::MTL`.
   */
  template<typename FileTypeTraits<filetype>::SyntaxType key, typename... T>
  constexpr void write(T &&...args)
  {
    /* Get format syntax, number of arguments expected and whether types of given arguments are
     * valid.
     */
    constexpr FormattingSyntax fmt_nargs_valid = syntax_elem_to_formatting<T...>(key);
    BLI_STATIC_ASSERT(fmt_nargs_valid.are_types_valid &&
                          (sizeof...(T) == fmt_nargs_valid.total_args),
                      "Types of all arguments and the number of arguments should match what the "
                      "formatting specifies.");
    write_impl(fmt_nargs_valid.fmt, std::forward<T>(args)...);
  }

 private:
  /* Remove this after upgrading to C++20. */
  template<typename T> using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

  /**
   * Make #std::string etc., usable for `fprintf` family. int float etc. are not affected.
   * \return: `const char *` or the original argument if the argument is
   * not related to #std::string.
   */
  template<typename T> constexpr auto convert_to_primitive(T &&arg) const
  {
    if constexpr (std::is_same_v<remove_cvref_t<T>, std::string> ||
                  std::is_same_v<remove_cvref_t<T>, blender::StringRefNull>) {
      return arg.c_str();
    }
    else if constexpr (std::is_same_v<remove_cvref_t<T>, blender::StringRef>) {
      BLI_STATIC_ASSERT(
          (always_false<T>::value),
          "Null-terminated string not present. Please use blender::StringRefNull instead.");
      /* Another trick to cause a compile-time error: returning nothing to #std::printf. */
      return;
    }
    else {
      /* For int, float etc. */
      return std::forward<T>(arg);
    }
  }

  /* Ensure the last block contains at least this amount of free space.
   * If not, add a new block with max of block size & the amount of space needed. */
  void ensure_space(size_t at_least)
  {
    if (blocks_.empty() || (blocks_.back().capacity() - blocks_.back().size() < at_least)) {
      VectorChar &b = blocks_.emplace_back(VectorChar());
      b.reserve(std::max(at_least, buffer_chunk_size));
    }
  }

  template<typename... T> constexpr void write_impl(const char *fmt, T &&...args)
  {
    if constexpr (sizeof...(T) == 0) {
      /* No arguments: just emit the format string. */
      size_t len = strlen(fmt);
      ensure_space(len);
      VectorChar &bb = blocks_.back();
      bb.insert(bb.end(), fmt, fmt + len);
    }
    else {
      /* Format into a local buffer. */
      char buf[write_local_buffer_size];
      int needed = std::snprintf(
          buf, write_local_buffer_size, fmt, convert_to_primitive(std::forward<T>(args))...);
      if (needed < 0)
        throw std::system_error(
            errno, std::system_category(), "Failed to format obj export string into a buffer");
      ensure_space(needed + 1); /* Ensure space for zero terminator. */
      VectorChar &bb = blocks_.back();
      if (needed < write_local_buffer_size) {
        /* String formatted successfully into the local buffer, copy it. */
        bb.insert(bb.end(), buf, buf + needed);
      }
      else {
        /* Would need more space than the local buffer: insert said space and format again into
         * that. */
        size_t bbEnd = bb.size();
        bb.insert(bb.end(), needed, ' ');
        std::snprintf(
            bb.data() + bbEnd, needed + 1, fmt, convert_to_primitive(std::forward<T>(args))...);
      }
    }
  }
};

}  // namespace blender::io::obj
