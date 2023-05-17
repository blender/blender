/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_node_declaration.hh"
#include "NOD_socket_declarations.hh"
#include "NOD_socket_declarations_geometry.hh"

#include "BKE_geometry_fields.hh"
#include "BKE_node.hh"

namespace blender::nodes {

void build_node_declaration(const bNodeType &typeinfo, NodeDeclaration &r_declaration)
{
  NodeDeclarationBuilder node_decl_builder{r_declaration};
  typeinfo.declare(node_decl_builder);
  node_decl_builder.finalize();
}

void build_node_declaration_dynamic(const bNodeTree &node_tree,
                                    const bNode &node,
                                    NodeDeclaration &r_declaration)
{
  r_declaration.inputs.clear();
  r_declaration.outputs.clear();
  node.typeinfo->declare_dynamic(node_tree, node, r_declaration);
}

void NodeDeclarationBuilder::finalize()
{
  if (is_function_node_) {
    for (std::unique_ptr<BaseSocketDeclarationBuilder> &socket_builder : input_builders_) {
      SocketDeclaration &socket_decl = *socket_builder->declaration();
      if (socket_decl.input_field_type != InputSocketFieldType::Implicit) {
        socket_decl.input_field_type = InputSocketFieldType::IsSupported;
      }
    }
    for (std::unique_ptr<BaseSocketDeclarationBuilder> &socket_builder : output_builders_) {
      SocketDeclaration &socket_decl = *socket_builder->declaration();
      socket_decl.output_field_dependency = OutputFieldDependency::ForDependentField();
      socket_builder->reference_pass_all_ = true;
    }
  }

  Vector<int> geometry_inputs;
  for (const int i : declaration_.inputs.index_range()) {
    if (dynamic_cast<decl::Geometry *>(declaration_.inputs[i].get())) {
      geometry_inputs.append(i);
    }
  }
  Vector<int> geometry_outputs;
  for (const int i : declaration_.outputs.index_range()) {
    if (dynamic_cast<decl::Geometry *>(declaration_.outputs[i].get())) {
      geometry_outputs.append(i);
    }
  }

  for (std::unique_ptr<BaseSocketDeclarationBuilder> &socket_builder : input_builders_) {
    if (socket_builder->field_on_all_) {
      aal::RelationsInNode &relations = this->get_anonymous_attribute_relations();
      const int field_input = socket_builder->index_;
      for (const int geometry_input : geometry_inputs) {
        relations.eval_relations.append({field_input, geometry_input});
      }
    }
  }
  for (std::unique_ptr<BaseSocketDeclarationBuilder> &socket_builder : output_builders_) {
    if (socket_builder->field_on_all_) {
      aal::RelationsInNode &relations = this->get_anonymous_attribute_relations();
      const int field_output = socket_builder->index_;
      for (const int geometry_output : geometry_outputs) {
        relations.available_relations.append({field_output, geometry_output});
      }
    }
    if (socket_builder->reference_pass_all_) {
      aal::RelationsInNode &relations = this->get_anonymous_attribute_relations();
      const int field_output = socket_builder->index_;
      for (const int input_i : declaration_.inputs.index_range()) {
        SocketDeclaration &input_socket_decl = *declaration_.inputs[input_i];
        if (input_socket_decl.input_field_type != InputSocketFieldType::None) {
          relations.reference_relations.append({input_i, field_output});
        }
      }
    }
    if (socket_builder->propagate_from_all_) {
      aal::RelationsInNode &relations = this->get_anonymous_attribute_relations();
      const int geometry_output = socket_builder->index_;
      for (const int geometry_input : geometry_inputs) {
        relations.propagate_relations.append({geometry_input, geometry_output});
      }
    }
  }
}

namespace anonymous_attribute_lifetime {

bool operator==(const RelationsInNode &a, const RelationsInNode &b)
{
  return a.propagate_relations == b.propagate_relations &&
         a.reference_relations == b.reference_relations && a.eval_relations == b.eval_relations &&
         a.available_relations == b.available_relations &&
         a.available_on_none == b.available_on_none;
}

bool operator!=(const RelationsInNode &a, const RelationsInNode &b)
{
  return !(a == b);
}

std::ostream &operator<<(std::ostream &stream, const RelationsInNode &relations)
{
  stream << "Propagate Relations: " << relations.propagate_relations.size() << "\n";
  for (const PropagateRelation &relation : relations.propagate_relations) {
    stream << "  " << relation.from_geometry_input << " -> " << relation.to_geometry_output
           << "\n";
  }
  stream << "Reference Relations: " << relations.reference_relations.size() << "\n";
  for (const ReferenceRelation &relation : relations.reference_relations) {
    stream << "  " << relation.from_field_input << " -> " << relation.to_field_output << "\n";
  }
  stream << "Eval Relations: " << relations.eval_relations.size() << "\n";
  for (const EvalRelation &relation : relations.eval_relations) {
    stream << "  eval " << relation.field_input << " on " << relation.geometry_input << "\n";
  }
  stream << "Available Relations: " << relations.available_relations.size() << "\n";
  for (const AvailableRelation &relation : relations.available_relations) {
    stream << "  " << relation.field_output << " available on " << relation.geometry_output
           << "\n";
  }
  stream << "Available on None: " << relations.available_on_none.size() << "\n";
  for (const int i : relations.available_on_none) {
    stream << "  output " << i << " available on none\n";
  }
  return stream;
}

}  // namespace anonymous_attribute_lifetime

bool NodeDeclaration::matches(const bNode &node) const
{
  auto check_sockets = [&](ListBase sockets, Span<SocketDeclarationPtr> socket_decls) {
    const int tot_sockets = BLI_listbase_count(&sockets);
    if (tot_sockets != socket_decls.size()) {
      return false;
    }
    int i;
    LISTBASE_FOREACH_INDEX (const bNodeSocket *, socket, &sockets, i) {
      const SocketDeclaration &socket_decl = *socket_decls[i];
      if (!socket_decl.matches(*socket)) {
        return false;
      }
    }
    return true;
  };

  if (!check_sockets(node.inputs, inputs)) {
    return false;
  }
  if (!check_sockets(node.outputs, outputs)) {
    return false;
  }
  return true;
}

bNodeSocket &SocketDeclaration::update_or_build(bNodeTree &ntree,
                                                bNode &node,
                                                bNodeSocket &socket) const
{
  /* By default just rebuild. */
  BLI_assert(socket.in_out == this->in_out);
  UNUSED_VARS_NDEBUG(socket);
  return this->build(ntree, node);
}

void SocketDeclaration::set_common_flags(bNodeSocket &socket) const
{
  SET_FLAG_FROM_TEST(socket.flag, compact, SOCK_COMPACT);
  SET_FLAG_FROM_TEST(socket.flag, hide_value, SOCK_HIDE_VALUE);
  SET_FLAG_FROM_TEST(socket.flag, hide_label, SOCK_HIDE_LABEL);
  SET_FLAG_FROM_TEST(socket.flag, is_multi_input, SOCK_MULTI_INPUT);
  SET_FLAG_FROM_TEST(socket.flag, no_mute_links, SOCK_NO_INTERNAL_LINK);
  SET_FLAG_FROM_TEST(socket.flag, is_unavailable, SOCK_UNAVAIL);
}

bool SocketDeclaration::matches_common_data(const bNodeSocket &socket) const
{
  if (socket.name != this->name) {
    return false;
  }
  if (socket.identifier != this->identifier) {
    return false;
  }
  if (((socket.flag & SOCK_COMPACT) != 0) != this->compact) {
    return false;
  }
  if (((socket.flag & SOCK_HIDE_VALUE) != 0) != this->hide_value) {
    return false;
  }
  if (((socket.flag & SOCK_HIDE_LABEL) != 0) != this->hide_label) {
    return false;
  }
  if (((socket.flag & SOCK_MULTI_INPUT) != 0) != this->is_multi_input) {
    return false;
  }
  if (((socket.flag & SOCK_NO_INTERNAL_LINK) != 0) != this->no_mute_links) {
    return false;
  }
  if (((socket.flag & SOCK_UNAVAIL) != 0) != this->is_unavailable) {
    return false;
  }
  return true;
}

namespace implicit_field_inputs {

void position(const bNode & /*node*/, void *r_value)
{
  new (r_value) fn::ValueOrField<float3>(bke::AttributeFieldInput::Create<float3>("position"));
}

void normal(const bNode & /*node*/, void *r_value)
{
  new (r_value)
      fn::ValueOrField<float3>(fn::Field<float3>(std::make_shared<bke::NormalFieldInput>()));
}

void index(const bNode & /*node*/, void *r_value)
{
  new (r_value) fn::ValueOrField<int>(fn::Field<int>(std::make_shared<fn::IndexFieldInput>()));
}

void id_or_index(const bNode & /*node*/, void *r_value)
{
  new (r_value)
      fn::ValueOrField<int>(fn::Field<int>(std::make_shared<bke::IDAttributeFieldInput>()));
}

}  // namespace implicit_field_inputs

}  // namespace blender::nodes
