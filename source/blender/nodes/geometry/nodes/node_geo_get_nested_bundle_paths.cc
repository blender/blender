/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_geo_bundle.hh"
#include "NOD_geometry_nodes_bundle.hh"
#include "NOD_geometry_nodes_list.hh"
#include "NOD_string_pattern.hh"

#include "RNA_enum_types.hh"

#include "UI_resources.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_get_nested_bundle_paths_cc {

enum class Mode {
  All = 0,
  BundleType = 1,
  DataType = 2,
};

static const EnumPropertyItem mode_items[] = {
    {int(Mode::All), "ALL", ICON_NONE, "All", "Output all paths"},
    {int(Mode::BundleType),
     "BUNDLE_TYPE",
     ICON_NONE,
     N_("Bundle Type"),
     N_("Find paths to bundles of a specific type")},
    {int(Mode::DataType),
     "DATA_TYPE",
     ICON_NONE,
     N_("Data Type"),
     N_("Find paths to values of a specific data type")},
    {0},
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Bundle>("Bundle"_ustr);
  b.add_input<decl::Menu>("Mode"_ustr).static_items(mode_items).optional_label();
  b.add_input<decl::Menu>("Pattern Mode"_ustr)
      .static_items(string_pattern_mode_items)
      .optional_label()
      .usage_by_menu("Mode"_ustr, int(Mode::BundleType));
  b.add_input<decl::String>("Bundle Type"_ustr)
      .optional_label()
      .usage_by_menu("Mode"_ustr, int(Mode::BundleType));
  b.add_input<decl::Menu>("Data Type"_ustr)
      .static_items(rna_enum_node_socket_data_type_items,
                    [](const EnumPropertyItem &item) {
                      return socket_type_supported_in_bundle(eNodeSocketDatatype(item.value),
                                                             NTREE_GEOMETRY);
                    })
      .optional_label()
      .usage_by_menu("Mode"_ustr, int(Mode::DataType));
  b.add_output<decl::String>("Paths"_ustr).structure_type(StructureType::List);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  BundlePtr bundle_ptr = params.extract_input<BundlePtr>("Bundle"_ustr);
  const Mode mode = params.extract_input<Mode>("Mode"_ustr);
  if (!bundle_ptr) {
    params.set_default_remaining_outputs();
    return;
  }
  const Bundle &bundle = *bundle_ptr;

  Vector<std::string> paths;
  switch (mode) {
    case Mode::All: {
      foreach_nested_bundle_item(
          bundle, [&](const Span<BundleKey> path, const BundleItemValue & /*value*/) {
            paths.append(Bundle::combine_path(path));
          });
      break;
    }
    case Mode::BundleType: {
      const std::string type_pattern = params.extract_input<std::string>("Bundle Type"_ustr);
      const StringPatternMode pattern_mode = params.extract_input<StringPatternMode>(
          "Pattern Mode"_ustr);
      std::string pattern_error;
      std::optional<StringPattern> pattern_fn = StringPattern::from_str(
          pattern_mode, type_pattern, pattern_error);
      if (!pattern_fn) {
        params.error_message_add(NodeWarningType::Error, pattern_error);
        break;
      }
      if (!type_pattern.empty()) {
        paths = gather_bundle_paths_by_bundle_type(
            bundle, [&](const StringRef type) { return pattern_fn->match(type); });
      }
      break;
    }
    case Mode::DataType:
      const eNodeSocketDatatype data_type = params.extract_input<eNodeSocketDatatype>(
          "Data Type"_ustr);
      paths = gather_bundle_paths_by_data_type(bundle, data_type);
      break;
  }

  /* Make sure the order is deterministic and doesn't depend on hash tables in the bundle. */
  std::ranges::sort(paths);

  params.set_output("Paths"_ustr, GList::from_container(std::move(paths)));
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "NodeGetNestedBundlePaths"_ustr);
  ntype.ui_name = "Get Nested Bundle Paths";
  ntype.ui_description = "Get paths to items in a nested bundle with a filter";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  ntype.default_width = bke::NodeWidth::_180;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_get_nested_bundle_paths_cc
