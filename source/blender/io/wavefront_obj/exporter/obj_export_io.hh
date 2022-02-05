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
 */

/** \file
 * \ingroup obj
 */

#pragma once

#include <cstdio>
#include <string>
#include <system_error>
#include <type_traits>

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

/**
 * File format and syntax agnostic file writer.
 */
template<eFileType filetype> class FormattedFileHandler : NonCopyable, NonMovable {
 private:
  std::FILE *outfile_ = nullptr;
  std::string outfile_path_;

 public:
  FormattedFileHandler(std::string outfile_path) noexcept(false)
      : outfile_path_(std::move(outfile_path))
  {
    outfile_ = BLI_fopen(outfile_path_.c_str(), "w");
    if (!outfile_) {
      throw std::system_error(errno, std::system_category(), "Cannot open file " + outfile_path_);
    }
  }

  ~FormattedFileHandler()
  {
    if (outfile_ && std::fclose(outfile_)) {
      std::cerr << "Error: could not close the file '" << outfile_path_
                << "'  properly, it may be corrupted." << std::endl;
    }
  }

  /**
   * Example invocation: `writer->write<eMTLSyntaxElement::newmtl>("foo")`.
   *
   * \param key Must match what the instance's filetype expects; i.e., `eMTLSyntaxElement` for
   * `eFileType::MTL`.
   */
  template<typename FileTypeTraits<filetype>::SyntaxType key, typename... T>
  constexpr void write(T &&...args) const
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

  template<typename... T> constexpr void write_impl(const char *fmt, T &&...args) const
  {
    if constexpr (sizeof...(T) == 0) {
      std::fputs(fmt, outfile_);
    }
    else {
      std::fprintf(outfile_, fmt, convert_to_primitive(std::forward<T>(args))...);
    }
  }
};

}  // namespace blender::io::obj
