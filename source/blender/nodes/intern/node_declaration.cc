/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_node_declaration.hh"

#include "BKE_node.h"

namespace blender::nodes {

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

  if (!check_sockets(node.inputs, inputs_)) {
    return false;
  }
  if (!check_sockets(node.outputs, outputs_)) {
    return false;
  }
  return true;
}

bNodeSocket &SocketDeclaration::update_or_build(bNodeTree &ntree,
                                                bNode &node,
                                                bNodeSocket &socket) const
{
  /* By default just rebuild. */
  BLI_assert(socket.in_out == in_out_);
  UNUSED_VARS_NDEBUG(socket);
  return this->build(ntree, node);
}

void SocketDeclaration::set_common_flags(bNodeSocket &socket) const
{
  SET_FLAG_FROM_TEST(socket.flag, compact_, SOCK_COMPACT);
  SET_FLAG_FROM_TEST(socket.flag, hide_value_, SOCK_HIDE_VALUE);
  SET_FLAG_FROM_TEST(socket.flag, hide_label_, SOCK_HIDE_LABEL);
  SET_FLAG_FROM_TEST(socket.flag, is_multi_input_, SOCK_MULTI_INPUT);
  SET_FLAG_FROM_TEST(socket.flag, no_mute_links_, SOCK_NO_INTERNAL_LINK);
  SET_FLAG_FROM_TEST(socket.flag, is_unavailable_, SOCK_UNAVAIL);
}

bool SocketDeclaration::matches_common_data(const bNodeSocket &socket) const
{
  if (socket.name != name_) {
    return false;
  }
  if (socket.identifier != identifier_) {
    return false;
  }
  if (((socket.flag & SOCK_COMPACT) != 0) != compact_) {
    return false;
  }
  if (((socket.flag & SOCK_HIDE_VALUE) != 0) != hide_value_) {
    return false;
  }
  if (((socket.flag & SOCK_HIDE_LABEL) != 0) != hide_label_) {
    return false;
  }
  if (((socket.flag & SOCK_MULTI_INPUT) != 0) != is_multi_input_) {
    return false;
  }
  if (((socket.flag & SOCK_NO_INTERNAL_LINK) != 0) != no_mute_links_) {
    return false;
  }
  if (((socket.flag & SOCK_UNAVAIL) != 0) != is_unavailable_) {
    return false;
  }
  return true;
}

}  // namespace blender::nodes
