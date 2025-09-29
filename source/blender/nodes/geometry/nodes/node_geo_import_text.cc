/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_fileops.h"
#include "BLI_generic_key_string.hh"
#include "BLI_memory_cache_file_load.hh"
#include "BLI_memory_counter.hh"
#include "BLI_string_utf8.h"

#include "node_geometry_util.hh"

#include "fmt/core.h"

namespace blender::nodes::node_geo_import_text {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::String>("Path")
      .subtype(PROP_FILEPATH)
      .path_filter("*.txt")
      .optional_label()
      .description("Path to a text file");

  b.add_output<decl::String>("String");
}

class LoadTextCache : public memory_cache::CachedValue {
 public:
  std::string text;
  Vector<geo_eval_log::NodeWarning> warnings;

  void count_memory(MemoryCounter &counter) const override
  {
    counter.add(this->text.size());
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  const std::optional<std::string> path = params.ensure_absolute_path(
      params.extract_input<std::string>("Path"));
  if (!path) {
    params.set_default_remaining_outputs();
    return;
  }

  std::shared_ptr<const LoadTextCache> cached_value = memory_cache::get_loaded<LoadTextCache>(
      GenericStringKey{"import_text_node"}, {StringRefNull(*path)}, [&]() {
        auto cached_value = std::make_unique<LoadTextCache>();

        size_t buffer_len;
        void *buffer = BLI_file_read_text_as_mem(path->c_str(), 0, &buffer_len);
        if (!buffer) {
          const std::string message = fmt::format(fmt::runtime(TIP_("Cannot open file: {}")),
                                                  *path);
          cached_value->warnings.append({NodeWarningType::Error, message});
          return cached_value;
        }
        BLI_SCOPED_DEFER([&]() { MEM_freeN(buffer); });
        if (BLI_str_utf8_invalid_byte(static_cast<const char *>(buffer), buffer_len) != -1) {
          cached_value->warnings.append(
              {NodeWarningType::Error, TIP_("File contains invalid UTF-8 characters")});
          return cached_value;
        }
        cached_value->text = std::string(static_cast<char *>(buffer), buffer_len);
        return cached_value;
      });

  for (const geo_eval_log::NodeWarning &warning : cached_value->warnings) {
    params.error_message_add(warning.type, warning.message);
  }

  params.set_output("String", cached_value->text);
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
