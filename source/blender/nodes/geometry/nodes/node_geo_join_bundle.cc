/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <fmt/format.h>

#include "NOD_geometry_nodes_bundle.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_join_bundle {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Bundle>("Bundle").multi_input().description(
      "Bundles to join together on the top level for each bundle. When there are duplicates, only "
      "the first occurrence is used");
  b.add_output<decl::Bundle>("Bundle").align_with_previous().propagate_all().reference_pass_all();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeoNodesMultiInput<BundlePtr> bundles = params.extract_input<GeoNodesMultiInput<BundlePtr>>(
      "Bundle");

  if (bundles.values.is_empty()) {
    params.set_default_remaining_outputs();
    return;
  }

  BundlePtr output_bundle;
  int bundle_i = 0;
  for (; bundle_i < bundles.values.size(); bundle_i++) {
    BundlePtr &bundle = bundles.values[bundle_i];
    if (bundle) {
      output_bundle = std::move(bundle);
      bundle_i++;
      break;
    }
  }
  if (!output_bundle) {
    output_bundle = Bundle::create();
  }
  else if (!output_bundle->is_mutable()) {
    output_bundle = output_bundle->copy();
  }
  else {
    output_bundle->tag_ensured_mutable();
  }
  Bundle &mutable_output_bundle = const_cast<Bundle &>(*output_bundle);

  VectorSet<StringRef> overridden_keys;
  for (; bundle_i < bundles.values.size(); bundle_i++) {
    BundlePtr &bundle = bundles.values[bundle_i];
    if (!bundle) {
      continue;
    }
    for (const Bundle::StoredItem &item : bundle->items()) {
      if (!mutable_output_bundle.add(item.key, item.value)) {
        overridden_keys.add(item.key);
      }
    }
  }

  if (!overridden_keys.is_empty()) {
    std::string message = fmt::format(
        "{}: {}", TIP_("Duplicate keys"), fmt::join(overridden_keys, ", "));
    params.error_message_add(NodeWarningType::Info, std::move(message));
  }

  params.set_output("Bundle", output_bundle);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(&ntype, "NodeJoinBundle");
  ntype.ui_name = "Join Bundle";
  ntype.ui_description = "Join multiple bundles together";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_join_bundle
