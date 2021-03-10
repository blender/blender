/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

/** \file
 * \ingroup nodes
 *
 * NodeTreeRef makes querying information about a bNodeTree more efficient. It is an immutable data
 * structure. It should not be used after anymore, after the underlying node tree changed.
 *
 * The following queries are supported efficiently:
 *  - socket -> index of socket
 *  - socket -> directly linked sockets
 *  - socket -> directly linked links
 *  - socket -> linked sockets when skipping reroutes
 *  - socket -> node
 *  - socket/node -> rna pointer
 *  - node -> inputs/outputs
 *  - node -> tree
 *  - tree -> all nodes
 *  - tree -> all (input/output) sockets
 *  - idname -> nodes
 *
 * Every socket has an id. The id-space is shared between input and output sockets.
 * When storing data per socket, it is often better to use the id as index into an array, instead
 * of a hash table.
 *
 * Every node has an id as well. The same rule regarding hash tables applies.
 *
 * There is an utility to export this data structure as graph in dot format.
 */

#include "BLI_array.hh"
#include "BLI_linear_allocator.hh"
#include "BLI_map.hh"
#include "BLI_multi_value_map.hh"
#include "BLI_string_ref.hh"
#include "BLI_timeit.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

#include "BKE_node.h"

#include "DNA_node_types.h"

#include "RNA_access.h"

namespace blender::nodes {

class SocketRef;
class InputSocketRef;
class OutputSocketRef;
class NodeRef;
class NodeTreeRef;
class LinkRef;
class InternalLinkRef;

class SocketRef : NonCopyable, NonMovable {
 protected:
  NodeRef *node_;
  bNodeSocket *bsocket_;
  bool is_input_;
  int id_;
  int index_;
  PointerRNA rna_;
  Vector<SocketRef *> linked_sockets_;
  Vector<SocketRef *> directly_linked_sockets_;
  Vector<LinkRef *> directly_linked_links_;

  friend NodeTreeRef;

 public:
  Span<const SocketRef *> linked_sockets() const;
  Span<const SocketRef *> directly_linked_sockets() const;
  Span<const LinkRef *> directly_linked_links() const;
  bool is_linked() const;

  const NodeRef &node() const;
  const NodeTreeRef &tree() const;

  int id() const;
  int index() const;

  bool is_input() const;
  bool is_output() const;

  const SocketRef &as_base() const;
  const InputSocketRef &as_input() const;
  const OutputSocketRef &as_output() const;

  PointerRNA *rna() const;

  StringRefNull idname() const;
  StringRefNull name() const;
  StringRefNull identifier() const;
  bNodeSocketType *typeinfo() const;

  bNodeSocket *bsocket() const;
  bNode *bnode() const;
  bNodeTree *btree() const;

  bool is_available() const;
};

class InputSocketRef final : public SocketRef {
 public:
  Span<const OutputSocketRef *> linked_sockets() const;
  Span<const OutputSocketRef *> directly_linked_sockets() const;

  bool is_multi_input_socket() const;
};

class OutputSocketRef final : public SocketRef {
 public:
  Span<const InputSocketRef *> linked_sockets() const;
  Span<const InputSocketRef *> directly_linked_sockets() const;
};

class NodeRef : NonCopyable, NonMovable {
 private:
  NodeTreeRef *tree_;
  bNode *bnode_;
  PointerRNA rna_;
  int id_;
  Vector<InputSocketRef *> inputs_;
  Vector<OutputSocketRef *> outputs_;
  Vector<InternalLinkRef *> internal_links_;

  friend NodeTreeRef;

 public:
  const NodeTreeRef &tree() const;

  Span<const InputSocketRef *> inputs() const;
  Span<const OutputSocketRef *> outputs() const;
  Span<const InternalLinkRef *> internal_links() const;

  const InputSocketRef &input(int index) const;
  const OutputSocketRef &output(int index) const;

  bNode *bnode() const;
  bNodeTree *btree() const;

  PointerRNA *rna() const;
  StringRefNull idname() const;
  StringRefNull name() const;
  bNodeType *typeinfo() const;

  int id() const;

  bool is_reroute_node() const;
  bool is_group_node() const;
  bool is_group_input_node() const;
  bool is_group_output_node() const;
  bool is_muted() const;
};

class LinkRef : NonCopyable, NonMovable {
 private:
  OutputSocketRef *from_;
  InputSocketRef *to_;
  bNodeLink *blink_;

  friend NodeTreeRef;

 public:
  const OutputSocketRef &from() const;
  const InputSocketRef &to() const;

  bNodeLink *blink() const;
};

class InternalLinkRef : NonCopyable, NonMovable {
 private:
  InputSocketRef *from_;
  OutputSocketRef *to_;
  bNodeLink *blink_;

  friend NodeTreeRef;

 public:
  const InputSocketRef &from() const;
  const OutputSocketRef &to() const;

  bNodeLink *blink() const;
};

class NodeTreeRef : NonCopyable, NonMovable {
 private:
  LinearAllocator<> allocator_;
  bNodeTree *btree_;
  Vector<NodeRef *> nodes_by_id_;
  Vector<SocketRef *> sockets_by_id_;
  Vector<InputSocketRef *> input_sockets_;
  Vector<OutputSocketRef *> output_sockets_;
  Vector<LinkRef *> links_;
  MultiValueMap<const bNodeType *, NodeRef *> nodes_by_type_;

 public:
  NodeTreeRef(bNodeTree *btree);
  ~NodeTreeRef();

  Span<const NodeRef *> nodes() const;
  Span<const NodeRef *> nodes_by_type(StringRefNull idname) const;
  Span<const NodeRef *> nodes_by_type(const bNodeType *nodetype) const;

  Span<const SocketRef *> sockets() const;
  Span<const InputSocketRef *> input_sockets() const;
  Span<const OutputSocketRef *> output_sockets() const;

  Span<const LinkRef *> links() const;

  bool has_link_cycles() const;

  bNodeTree *btree() const;

  std::string to_dot() const;

 private:
  /* Utility functions used during construction. */
  InputSocketRef &find_input_socket(Map<bNode *, NodeRef *> &node_mapping,
                                    bNode *bnode,
                                    bNodeSocket *bsocket);
  OutputSocketRef &find_output_socket(Map<bNode *, NodeRef *> &node_mapping,
                                      bNode *bnode,
                                      bNodeSocket *bsocket);
  void find_origins_skipping_reroutes(InputSocketRef &socket, Vector<SocketRef *> &r_origins);
};

using NodeTreeRefMap = Map<bNodeTree *, std::unique_ptr<const NodeTreeRef>>;

const NodeTreeRef &get_tree_ref_from_map(NodeTreeRefMap &node_tree_refs, bNodeTree &btree);

namespace node_tree_ref_types {
using nodes::InputSocketRef;
using nodes::NodeRef;
using nodes::NodeTreeRef;
using nodes::NodeTreeRefMap;
using nodes::OutputSocketRef;
using nodes::SocketRef;
}  // namespace node_tree_ref_types

/* --------------------------------------------------------------------
 * SocketRef inline methods.
 */

inline Span<const SocketRef *> SocketRef::linked_sockets() const
{
  return linked_sockets_;
}

inline Span<const SocketRef *> SocketRef::directly_linked_sockets() const
{
  return directly_linked_sockets_;
}

inline Span<const LinkRef *> SocketRef::directly_linked_links() const
{
  return directly_linked_links_;
}

inline bool SocketRef::is_linked() const
{
  return linked_sockets_.size() > 0;
}

inline const NodeRef &SocketRef::node() const
{
  return *node_;
}

inline const NodeTreeRef &SocketRef::tree() const
{
  return node_->tree();
}

inline int SocketRef::id() const
{
  return id_;
}

inline int SocketRef::index() const
{
  return index_;
}

inline bool SocketRef::is_input() const
{
  return is_input_;
}

inline bool SocketRef::is_output() const
{
  return !is_input_;
}

inline const SocketRef &SocketRef::as_base() const
{
  return *this;
}

inline const InputSocketRef &SocketRef::as_input() const
{
  BLI_assert(this->is_input());
  return static_cast<const InputSocketRef &>(*this);
}

inline const OutputSocketRef &SocketRef::as_output() const
{
  BLI_assert(this->is_output());
  return static_cast<const OutputSocketRef &>(*this);
}

inline PointerRNA *SocketRef::rna() const
{
  return const_cast<PointerRNA *>(&rna_);
}

inline StringRefNull SocketRef::idname() const
{
  return bsocket_->idname;
}

inline StringRefNull SocketRef::name() const
{
  return bsocket_->name;
}

inline StringRefNull SocketRef::identifier() const
{
  return bsocket_->identifier;
}

inline bNodeSocketType *SocketRef::typeinfo() const
{
  return bsocket_->typeinfo;
}

inline bNodeSocket *SocketRef::bsocket() const
{
  return bsocket_;
}

inline bNode *SocketRef::bnode() const
{
  return node_->bnode();
}

inline bNodeTree *SocketRef::btree() const
{
  return node_->btree();
}

inline bool SocketRef::is_available() const
{
  return (bsocket_->flag & SOCK_UNAVAIL) == 0;
}

/* --------------------------------------------------------------------
 * InputSocketRef inline methods.
 */

inline Span<const OutputSocketRef *> InputSocketRef::linked_sockets() const
{
  return linked_sockets_.as_span().cast<const OutputSocketRef *>();
}

inline Span<const OutputSocketRef *> InputSocketRef::directly_linked_sockets() const
{
  return directly_linked_sockets_.as_span().cast<const OutputSocketRef *>();
}

inline bool InputSocketRef::is_multi_input_socket() const
{
  return bsocket_->flag & SOCK_MULTI_INPUT;
}

/* --------------------------------------------------------------------
 * OutputSocketRef inline methods.
 */

inline Span<const InputSocketRef *> OutputSocketRef::linked_sockets() const
{
  return linked_sockets_.as_span().cast<const InputSocketRef *>();
}

inline Span<const InputSocketRef *> OutputSocketRef::directly_linked_sockets() const
{
  return directly_linked_sockets_.as_span().cast<const InputSocketRef *>();
}

/* --------------------------------------------------------------------
 * NodeRef inline methods.
 */

inline const NodeTreeRef &NodeRef::tree() const
{
  return *tree_;
}

inline Span<const InputSocketRef *> NodeRef::inputs() const
{
  return inputs_;
}

inline Span<const OutputSocketRef *> NodeRef::outputs() const
{
  return outputs_;
}

inline Span<const InternalLinkRef *> NodeRef::internal_links() const
{
  return internal_links_;
}

inline const InputSocketRef &NodeRef::input(int index) const
{
  return *inputs_[index];
}

inline const OutputSocketRef &NodeRef::output(int index) const
{
  return *outputs_[index];
}

inline bNode *NodeRef::bnode() const
{
  return bnode_;
}

inline bNodeTree *NodeRef::btree() const
{
  return tree_->btree();
}

inline PointerRNA *NodeRef::rna() const
{
  return const_cast<PointerRNA *>(&rna_);
}

inline StringRefNull NodeRef::idname() const
{
  return bnode_->idname;
}

inline StringRefNull NodeRef::name() const
{
  return bnode_->name;
}

inline bNodeType *NodeRef::typeinfo() const
{
  return bnode_->typeinfo;
}

inline int NodeRef::id() const
{
  return id_;
}

inline bool NodeRef::is_reroute_node() const
{
  return bnode_->type == NODE_REROUTE;
}

inline bool NodeRef::is_group_node() const
{
  return bnode_->type == NODE_GROUP;
}

inline bool NodeRef::is_group_input_node() const
{
  return bnode_->type == NODE_GROUP_INPUT;
}

inline bool NodeRef::is_group_output_node() const
{
  return bnode_->type == NODE_GROUP_OUTPUT;
}

inline bool NodeRef::is_muted() const
{
  return (bnode_->flag & NODE_MUTED) != 0;
}

/* --------------------------------------------------------------------
 * LinkRef inline methods.
 */

inline const OutputSocketRef &LinkRef::from() const
{
  return *from_;
}

inline const InputSocketRef &LinkRef::to() const
{
  return *to_;
}

inline bNodeLink *LinkRef::blink() const
{
  return blink_;
}

/* --------------------------------------------------------------------
 * InternalLinkRef inline methods.
 */

inline const InputSocketRef &InternalLinkRef::from() const
{
  return *from_;
}

inline const OutputSocketRef &InternalLinkRef::to() const
{
  return *to_;
}

inline bNodeLink *InternalLinkRef::blink() const
{
  return blink_;
}

/* --------------------------------------------------------------------
 * NodeTreeRef inline methods.
 */

inline Span<const NodeRef *> NodeTreeRef::nodes() const
{
  return nodes_by_id_;
}

inline Span<const NodeRef *> NodeTreeRef::nodes_by_type(StringRefNull idname) const
{
  const bNodeType *nodetype = nodeTypeFind(idname.c_str());
  return this->nodes_by_type(nodetype);
}

inline Span<const NodeRef *> NodeTreeRef::nodes_by_type(const bNodeType *nodetype) const
{
  return nodes_by_type_.lookup(nodetype);
}

inline Span<const SocketRef *> NodeTreeRef::sockets() const
{
  return sockets_by_id_;
}

inline Span<const InputSocketRef *> NodeTreeRef::input_sockets() const
{
  return input_sockets_;
}

inline Span<const OutputSocketRef *> NodeTreeRef::output_sockets() const
{
  return output_sockets_;
}

inline Span<const LinkRef *> NodeTreeRef::links() const
{
  return links_;
}

inline bNodeTree *NodeTreeRef::btree() const
{
  return btree_;
}

}  // namespace blender::nodes
