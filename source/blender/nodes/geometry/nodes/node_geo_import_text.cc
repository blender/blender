/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_ID.h"

#include "BKE_main.hh"

#include "BLI_fileops.hh"
#include "BLI_generic_key_string.hh"
#include "BLI_memory_cache_file_load.hh"
#include "BLI_memory_counter.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.hh"
#include "BLI_string_utf8.hh"

#include "COM_node_operation.hh"

#include "node_geometry_util.hh"

#include "fmt/core.h"

namespace blender::nodes::node_geo_import_text {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::String>("Path"_ustr)
      .subtype(PROP_FILEPATH)
      .path_filter("*.txt")
      .optional_label()
      .description("Path to a text file");

  b.add_output<decl::String>("String"_ustr);
}

class LoadTextCache : public memory_cache::CachedValue {
 public:
  std::string text;
  Vector<NodeWarning> warnings;

  void count_memory(MemoryCounter &counter) const override
  {
    counter.add(this->text.size());
  }
};

static std::shared_ptr<const LoadTextCache> get_cached_text(const StringRefNull path)
{
  auto load_fn = [path]() {
    auto cached_value = std::make_unique<LoadTextCache>();

    size_t buffer_len;
    char *buffer = BLI_file_read_text_as_mem(path.c_str(), 0, &buffer_len);
    if (!buffer) {
      const std::string message = fmt::format(fmt::runtime(TIP_("Cannot open file: {}")), path);
      cached_value->warnings.append({NodeWarningType::Error, message});
      return cached_value;
    }
    BLI_SCOPED_DEFER([&]() { MEM_delete(buffer); });
    if (BLI_str_utf8_invalid_byte(buffer, buffer_len) != -1) {
      cached_value->warnings.append(
          {NodeWarningType::Error, TIP_("File contains invalid UTF-8 characters")});
      return cached_value;
    }
    cached_value->text = std::string(buffer, buffer_len);
    return cached_value;
  };

  return memory_cache::get_loaded<LoadTextCache>(
      GenericStringKey{"import_text_node"}, {path}, load_fn);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const std::optional<std::string> path = params.ensure_absolute_path(
      params.extract_input<std::string>("Path"_ustr));
  if (!path) {
    params.set_default_remaining_outputs();
    return;
  }

  std::shared_ptr<const LoadTextCache> cached_value = get_cached_text(*path);

  for (const NodeWarning &warning : cached_value->warnings) {
    params.error_message_add(warning.type, warning.message);
  }

  params.set_output("String"_ustr, cached_value->text);
}

class ImportTextOperation : public compositor::NodeOperation {
 public:
  using compositor::NodeOperation::NodeOperation;

  void execute() override
  {
    compositor::Result &result = this->get_result("String");
    if (!result.should_compute()) {
      return;
    }

    const std::optional<std::string> path = this->ensure_absolute_path(
        this->get_input("Path").get_single_value_default<std::string>());
    if (!path) {
      result.allocate_single_value();
      result.set_single_value(std::string(""));
      return;
    }

    std::shared_ptr<const LoadTextCache> cached_value = get_cached_text(*path);

    for (const NodeWarning &warning : cached_value->warnings) {
      this->add_warning(warning.type, warning.message);
    }

    result.allocate_single_value();
    result.set_single_value(cached_value->text);
  }

 private:
  std::optional<std::string> ensure_absolute_path(const StringRefNull path) const
  {
    if (path.is_empty()) {
      return std::nullopt;
    }
    if (!BLI_path_is_rel(path.c_str())) {
      return std::string(path);
    }
    const Main &bmain = this->context().get_main();
    const bNodeTree &tree = this->node().owner_tree();
    const char *base_path = ID_BLEND_PATH(&bmain, &tree.id);
    if (!base_path || base_path[0] == '\0') {
      return std::nullopt;
    }
    char absolute_path[FILE_MAX];
    STRNCPY(absolute_path, path.c_str());
    BLI_path_abs(absolute_path, base_path);
    return absolute_path;
  }
};

static compositor::NodeOperation *get_compositor_operation(compositor::Context &context,
                                                           const bNode &node)
{
  return new ImportTextOperation(context, node);
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_cmp_node_type_base(&ntype, "GeometryNodeImportText"_ustr);
  ntype.ui_name = "Import Text";
  ntype.ui_description = "Import a string from a text file";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  ntype.get_compositor_operation = get_compositor_operation;

  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_import_text
