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

#ifndef __BKE_NODE_TREE_REF_HH__
#define __BKE_NODE_TREE_REF_HH__

/** \file
 * \ingroup bke
 *
 * NodeTreeRef makes querying information about a bNodeTree more efficient. It is an immutable data
 * structure. It should not be used after anymore, after the underlying node tree changed.
 *
 * The following queries are supported efficiently:
 *  - socket -> index of socket
 *  - socket -> directly linked sockets
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
#include "BLI_string_ref.hh"
#include "BLI_timeit.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

#include "BKE_node.h"

#include "DNA_node_types.h"

#include "RNA_access.h"

namespace blender::bke {

class SocketRef;
class InputSocketRef;
class OutputSocketRef;
class NodeRef;
class NodeTreeRef;

class SocketRef : NonCopyable, NonMovable {
 protected:
  NodeRef *node_;
  bNodeSocket *bsocket_;
  bool is_input_;
  uint id_;
  uint index_;
  PointerRNA rna_;
  Vector<SocketRef *> linked_sockets_;
  Vector<SocketRef *> directly_linked_sockets_;

  friend NodeTreeRef;

 public:
  Span<const SocketRef *> linked_sockets() const;
  Span<const SocketRef *> directly_linked_sockets() const;
  bool is_linked() const;

  const NodeRef &node() const;
  const NodeTreeRef &tree() const;

  uint id() const;
  uint index() const;

  bool is_input() const;
  bool is_output() const;

  const SocketRef &as_base() const;
  const InputSocketRef &as_input() const;
  const OutputSocketRef &as_output() const;

  PointerRNA *rna() const;

  StringRefNull idname() const;
  StringRefNull name() const;

  bNodeSocket *bsocket() const;
  bNode *bnode() const;
  bNodeTree *btree() const;
};

class InputSocketRef final : public SocketRef {
 public:
  Span<const OutputSocketRef *> linked_sockets() const;
  Span<const OutputSocketRef *> directly_linked_sockets() const;
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
  uint id_;
  Vector<InputSocketRef *> inputs_;
  Vector<OutputSocketRef *> outputs_;

  friend NodeTreeRef;

 public:
  const NodeTreeRef &tree() const;

  Span<const InputSocketRef *> inputs() const;
  Span<const OutputSocketRef *> outputs() const;

  const InputSocketRef &input(uint index) const;
  const OutputSocketRef &output(uint index) const;

  bNode *bnode() const;
  bNodeTree *btree() const;

  PointerRNA *rna() const;
  StringRefNull idname() const;
  StringRefNull name() const;

  uint id() const;

  bool is_reroute_node() const;
  bool is_group_node() const;
  bool is_group_input_node() const;
  bool is_group_output_node() const;
};

class NodeTreeRef : NonCopyable, NonMovable {
 private:
  LinearAllocator<> allocator_;
  bNodeTree *btree_;
  Vector<NodeRef *> nodes_by_id_;
  Vector<SocketRef *> sockets_by_id_;
  Vector<InputSocketRef *> input_sockets_;
  Vector<OutputSocketRef *> output_sockets_;
  Map<const bNodeType *, Vector<NodeRef *>> nodes_by_type_;

 public:
  NodeTreeRef(bNodeTree *btree);
  ~NodeTreeRef();

  Span<const NodeRef *> nodes() const;
  Span<const NodeRef *> nodes_by_type(StringRefNull idname) const;
  Span<const NodeRef *> nodes_by_type(const bNodeType *nodetype) const;

  Span<const SocketRef *> sockets() const;
  Span<const InputSocketRef *> input_sockets() const;
  Span<const OutputSocketRef *> output_sockets() const;

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
  void find_targets_skipping_reroutes(OutputSocketRef &socket_ref, Vector<SocketRef *> &r_targets);
};

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

inline uint SocketRef::id() const
{
  return id_;
}

inline uint SocketRef::index() const
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
  return *(const InputSocketRef *)this;
}

inline const OutputSocketRef &SocketRef::as_output() const
{
  BLI_assert(this->is_output());
  return *(const OutputSocketRef *)this;
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

inline const InputSocketRef &NodeRef::input(uint index) const
{
  return *inputs_[index];
}

inline const OutputSocketRef &NodeRef::output(uint index) const
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

inline uint NodeRef::id() const
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

/* --------------------------------------------------------------------
 * NodeRef inline methods.
 */

inline Span<const NodeRef *> NodeTreeRef::nodes() const
{
  return nodes_by_id_;
}

inline Span<const NodeRef *> NodeTreeRef::nodes_by_type(StringRefNull idname) const
{
  const bNodeType *nodetype = nodeTypeFind(idname.data());
  return this->nodes_by_type(nodetype);
}

inline Span<const NodeRef *> NodeTreeRef::nodes_by_type(const bNodeType *nodetype) const
{
  const Vector<NodeRef *> *nodes = nodes_by_type_.lookup_ptr(nodetype);
  if (nodes == nullptr) {
    return {};
  }
  else {
    return *nodes;
  }
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

inline bNodeTree *NodeTreeRef::btree() const
{
  return btree_;
}

}  // namespace blender::bke

#endif /* __BKE_NODE_TREE_REF_HH__ */
