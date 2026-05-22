/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_geometry_fields.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_named_layer_selection__cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::String>("Name"_ustr).is_layer_name().optional_label();
  b.add_output<decl::Bool>("Selection"_ustr)
      .structure_type(StructureType::Field)
      .propagate_references();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  std::string name = params.extract_input<std::string>("Name"_ustr);
  if (name.empty()) {
    params.set_default_remaining_outputs();
    return;
  }
  params.set_output("Selection"_ustr,
                    Field<bool>::from_input<bke::NamedLayerSelectionFieldInput>(std::move(name)));
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(
      &ntype, "GeometryNodeInputNamedLayerSelection"_ustr, GEO_NODE_INPUT_NAMED_LAYER_SELECTION);
  ntype.ui_name = "Named Layer Selection";
  ntype.ui_description = "Output a selection of a Grease Pencil layer";
  ntype.enum_name_legacy = "INPUT_NAMED_LAYER_SELECTION";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.default_width = bke::NodeWidth::_160;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_input_named_layer_selection__cc
