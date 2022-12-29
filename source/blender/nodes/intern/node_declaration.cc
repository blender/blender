/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_node_declaration.hh"

#include "BKE_geometry_fields.hh"
#include "BKE_node.h"

namespace blender::nodes {

void build_node_declaration(const bNodeType &typeinfo, NodeDeclaration &r_declaration)
{
  NodeDeclarationBuilder node_decl_builder{r_declaration};
  typeinfo.declare(node_decl_builder);
  node_decl_builder.finalize();
}

void NodeDeclarationBuilder::finalize()
{
  if (is_function_node_) {
    for (SocketDeclarationPtr &socket_decl : declaration_.inputs) {
      if (socket_decl->input_field_type != InputSocketFieldType::Implicit) {
        socket_decl->input_field_type = InputSocketFieldType::IsSupported;
      }
    }
    for (SocketDeclarationPtr &socket_decl : declaration_.outputs) {
      socket_decl->output_field_dependency = OutputFieldDependency::ForDependentField();
    }
  }
}

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
