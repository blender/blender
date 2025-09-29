/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_geometry_fields.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_named_layer_selection__cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::String>("Name").is_layer_name().optional_label();
  b.add_output<decl::Bool>("Selection").field_source_reference_all();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  std::string name = params.extract_input<std::string>("Name");
  if (name.empty()) {
    params.set_default_remaining_outputs();
    return;
  }

  Field<bool> selection_field{
      std::make_shared<bke::NamedLayerSelectionFieldInput>(std::move(name))};
  params.set_output("Selection", std::move(selection_field));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(
      &ntype, "GeometryNodeInputNamedLayerSelection", GEO_NODE_INPUT_NAMED_LAYER_SELECTION);
  ntype.ui_name = "Named Layer Selection";
  ntype.ui_description = "Output a selection of a Grease Pencil layer";
  ntype.enum_name_legacy = "INPUT_NAMED_LAYER_SELECTION";
  ntype.nclass = NODE_CLASS_INPUT;
  blender::bke::node_type_size(ntype, 160, 140, NODE_DEFAULT_MAX_WIDTH);
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_input_named_layer_selection__cc
