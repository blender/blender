/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_node.hh"

#include "NOD_geometry.hh"
#include "NOD_node_declaration.hh"

#include "NOD_common.h"
#include "node_common.h"
#include "node_geometry_util.hh"

#include "RNA_access.hh"

namespace blender::nodes {

static void set_default_input_field(const bNodeTreeInterfaceSocket &input, SocketDeclaration &decl)
{
  if (dynamic_cast<decl::Vector *>(&decl)) {
    if (input.default_input == GEO_NODE_DEFAULT_FIELD_INPUT_NORMAL_FIELD) {
      decl.implicit_input_fn = std::make_unique<ImplicitInputValueFn>(
          implicit_field_inputs::normal);
      decl.hide_value = true;
    }
    else if (input.default_input == GEO_NODE_DEFAULT_FIELD_INPUT_POSITION_FIELD) {
      decl.implicit_input_fn = std::make_unique<ImplicitInputValueFn>(
          implicit_field_inputs::position);
      decl.hide_value = true;
    }
  }
  else if (dynamic_cast<decl::Int *>(&decl)) {
    if (input.default_input == GEO_NODE_DEFAULT_FIELD_INPUT_INDEX_FIELD) {
      decl.implicit_input_fn = std::make_unique<ImplicitInputValueFn>(
          implicit_field_inputs::index);
      decl.hide_value = true;
    }
    else if (input.default_input == GEO_NODE_DEFAULT_FIELD_INPUT_ID_INDEX_FIELD) {
      decl.implicit_input_fn = std::make_unique<ImplicitInputValueFn>(
          implicit_field_inputs::id_or_index);
      decl.hide_value = true;
    }
  }
}

static void node_group_declare(const bNodeTree &node_tree,
                               const bNode &node,
                               NodeDeclarationBuilder &b)
{
  NodeDeclaration &r_declaration = b.declaration();
  const bNodeTree *group = reinterpret_cast<const bNodeTree *>(node.id);
  if (!group) {
    return;
  }
  node_group_declare(node_tree, node, b);
  if (!node.id) {
    return;
  }
  if (ID_IS_LINKED(&group->id) && (group->id.tag & LIB_TAG_MISSING)) {
    return;
  }

  group->ensure_interface_cache();
  const Span<const bNodeTreeInterfaceSocket *> inputs = group->interface_inputs();
  const FieldInferencingInterface &field_interface = *group->runtime->field_inferencing_interface;
  for (const int i : inputs.index_range()) {
    SocketDeclaration &decl = *r_declaration.inputs[i];
    decl.input_field_type = field_interface.inputs[i];
    set_default_input_field(*inputs[i], decl);
  }

  for (const int i : r_declaration.outputs.index_range()) {
    r_declaration.outputs[i]->output_field_dependency = field_interface.outputs[i];
  }
}

static void register_node_type_geo_group()
{
  static bNodeType ntype;

  node_type_base_custom(&ntype, "GeometryNodeGroup", "Group", "GROUP", NODE_CLASS_GROUP);
  ntype.type = NODE_GROUP;
  ntype.poll = geo_node_poll_default;
  ntype.poll_instance = node_group_poll_instance;
  ntype.insert_link = node_insert_link_default;
  ntype.rna_ext.srna = RNA_struct_find("GeometryNodeGroup");
  BLI_assert(ntype.rna_ext.srna != nullptr);
  RNA_struct_blender_type_set(ntype.rna_ext.srna, &ntype);

  bke::node_type_size(&ntype, 140, 60, 400);
  ntype.labelfunc = node_group_label;
  ntype.declare = node_group_declare;

  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(register_node_type_geo_group)

}  // namespace blender::nodes

void register_node_type_geo_custom_group(bNodeType *ntype)
{
  /* These methods can be overridden but need a default implementation otherwise. */
  if (ntype->poll == nullptr) {
    ntype->poll = geo_node_poll_default;
  }
  if (ntype->insert_link == nullptr) {
    ntype->insert_link = node_insert_link_default;
  }
  ntype->declare = blender::nodes::node_group_declare;
}
