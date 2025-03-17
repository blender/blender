/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_fileops.h"
#include "BLI_string_utf8.h"

#include "node_geometry_util.hh"

#include "fmt/core.h"

namespace blender::nodes::node_geo_import_text {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::String>("Path")
      .subtype(PROP_FILEPATH)
      .path_filter("*.txt")
      .hide_label()
      .description("Path to a text file");

  b.add_output<decl::String>("String");
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const std::optional<std::string> path = params.ensure_absolute_path(
      params.extract_input<std::string>("Path"));
  if (!path) {
    params.set_default_remaining_outputs();
    return;
  }

  size_t buffer_len;
  void *buffer = BLI_file_read_text_as_mem(path->c_str(), 0, &buffer_len);
  if (!buffer) {
    const std::string message = fmt::format(fmt::runtime(TIP_("Cannot open file: {}")), *path);
    params.error_message_add(NodeWarningType::Error, message);
    params.set_default_remaining_outputs();
    return;
  }
  BLI_SCOPED_DEFER([&]() { MEM_freeN(buffer); });
  if (BLI_str_utf8_invalid_byte(static_cast<const char *>(buffer), buffer_len) != -1) {
    params.error_message_add(NodeWarningType::Error,
                             TIP_("File contains invalid UTF-8 characters"));
    params.set_default_remaining_outputs();
    return;
  }

  params.set_output("String", std::string(static_cast<char *>(buffer), buffer_len));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeImportText");
  ntype.ui_name = "Import Text";
  ntype.ui_description = "Import a string from a text file";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_import_text
