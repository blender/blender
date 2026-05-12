/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <string>

#include "BLI_span.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

namespace blender::dna {

struct ParsedMember {
  /** Single-identifier type name, e.g. `int`, `float`, `Material`. */
  std::string type_name;
  /** Canonical full member name, e.g. `*var`, `arr[4]`, `(*func)()`. */
  std::string member_name;
  /** Original C++ code full member name, before renames. */
  std::string alias_member_name;
  /** Required alignment in bytes. */
  int alignment = 0;
};

struct ParsedStruct {
  std::string type_name;
  /** Original C++ code full member name, before renames. */
  std::string alias_type_name;
  Vector<ParsedMember> members;
};

struct ParsedEnum {
  std::string type_name;
  /** Underlying integer type, e.g. `int8_t`. */
  std::string underlying_type;
};

/** List of all DNA header filenames. */
Span<const char *> default_dna_header_filenames();

/** Extract structs, their members, and enums from DNA headers. */
[[nodiscard]] bool parse_dna_headers(StringRefNull base_directory,
                                     Vector<ParsedStruct> &r_structs,
                                     Vector<ParsedEnum> &r_enums,
                                     Span<const char *> include_files);

/** Convert C++ types to plain C types understood by DNA. */
[[nodiscard]] bool substitute_cpp_types(Vector<ParsedStruct> &structs,
                                        Span<ParsedEnum> enums,
                                        bool for_rna);

}  // namespace blender::dna
