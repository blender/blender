/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <sstream>
#include <utility>

#include "makesrna_utils.hh"

#include "BLI_string.h"
#include "BLI_string_ref.hh"

struct StructSplitName {
  blender::StringRef namespace_name;
  blender::StringRef struct_name;
};

static StructSplitName rna_split_namespace_struct_name(blender::StringRef full_name)
{
  blender::StringRef namespace_name;
  blender::StringRef struct_name;
  int namespace_length = full_name.find_last_of("::");
  if (namespace_length != blender::StringRef::not_found) {
    namespace_name = full_name.substr(0, namespace_length - 1);
    struct_name = full_name.substr(namespace_length + 1);
  }
  else {
    struct_name = full_name;
  }
  return {namespace_name, struct_name};
}

void rna_write_struct_forward_declarations(std::ostringstream &stream,
                                           blender::Vector<blender::StringRef> structs)
{
  std::stable_sort(
      structs.begin(), structs.end(), [](const blender::StringRef a, const blender::StringRef b) {
        /* Keep structs within namespaces last. */
        const StructSplitName a_name_split = rna_split_namespace_struct_name(a);
        const StructSplitName b_name_split = rna_split_namespace_struct_name(b);
        return (a_name_split.namespace_name < b_name_split.namespace_name) ||
               (a_name_split.namespace_name == b_name_split.namespace_name &&
                BLI_strcasecmp(a_name_split.struct_name.data(), b_name_split.struct_name.data()) <
                    0);
      });

  /* For grouping structs within namespaces. */
  blender::StringRef last_namespace = "";
  for (const blender::StringRef full_name : structs) {
    const StructSplitName name_split = rna_split_namespace_struct_name(full_name);
    if (name_split.namespace_name != last_namespace && !last_namespace.is_empty()) {
      stream << "}; // namespace " << std::string_view(last_namespace) << '\n';
    }
    if (name_split.namespace_name != last_namespace && !name_split.namespace_name.is_empty()) {
      stream << "namespace " << std::string_view(name_split.namespace_name) << " {\n";
    }
    stream << "struct " << std::string_view(name_split.struct_name) << ";\n";
    last_namespace = name_split.namespace_name;
  }
  if (!last_namespace.is_empty()) {
    stream << "}; // namespace " << std::string_view(last_namespace) << '\n';
  }
}
