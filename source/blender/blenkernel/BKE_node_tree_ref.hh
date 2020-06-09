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

namespace BKE {

using blender::Array;
using blender::IndexRange;
using blender::LinearAllocator;
using blender::Map;
using blender::MutableSpan;
using blender::Span;
using blender::StringRef;
using blender::StringRefNull;
using blender::Vector;

class SocketRef;
class InputSocketRef;
class OutputSocketRef;
class NodeRef;
class NodeTreeRef;

class SocketRef : blender::NonCopyable, blender::NonMovable {
 protected:
  NodeRef *m_node;
  bNodeSocket *m_bsocket;
  bool m_is_input;
  uint m_id;
  uint m_index;
  PointerRNA m_rna;
  Vector<SocketRef *> m_linked_sockets;
  Vector<SocketRef *> m_directly_linked_sockets;

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

class NodeRef : blender::NonCopyable, blender::NonMovable {
 private:
  NodeTreeRef *m_tree;
  bNode *m_bnode;
  PointerRNA m_rna;
  uint m_id;
  Vector<InputSocketRef *> m_inputs;
  Vector<OutputSocketRef *> m_outputs;

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

class NodeTreeRef : blender::NonCopyable, blender::NonMovable {
 private:
  LinearAllocator<> m_allocator;
  bNodeTree *m_btree;
  Vector<NodeRef *> m_nodes_by_id;
  Vector<SocketRef *> m_sockets_by_id;
  Vector<InputSocketRef *> m_input_sockets;
  Vector<OutputSocketRef *> m_output_sockets;
  Map<std::string, Vector<NodeRef *>> m_nodes_by_idname;

 public:
  NodeTreeRef(bNodeTree *btree);
  ~NodeTreeRef();

  Span<const NodeRef *> nodes() const;
  Span<const NodeRef *> nodes_with_idname(StringRef idname) const;

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
  return m_linked_sockets.as_span();
}

inline Span<const SocketRef *> SocketRef::directly_linked_sockets() const
{
  return m_directly_linked_sockets.as_span();
}

inline bool SocketRef::is_linked() const
{
  return m_linked_sockets.size() > 0;
}

inline const NodeRef &SocketRef::node() const
{
  return *m_node;
}

inline const NodeTreeRef &SocketRef::tree() const
{
  return m_node->tree();
}

inline uint SocketRef::id() const
{
  return m_id;
}

inline uint SocketRef::index() const
{
  return m_index;
}

inline bool SocketRef::is_input() const
{
  return m_is_input;
}

inline bool SocketRef::is_output() const
{
  return !m_is_input;
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
  return const_cast<PointerRNA *>(&m_rna);
}

inline StringRefNull SocketRef::idname() const
{
  return m_bsocket->idname;
}

inline StringRefNull SocketRef::name() const
{
  return m_bsocket->name;
}

inline bNodeSocket *SocketRef::bsocket() const
{
  return m_bsocket;
}

inline bNode *SocketRef::bnode() const
{
  return m_node->bnode();
}

inline bNodeTree *SocketRef::btree() const
{
  return m_node->btree();
}

/* --------------------------------------------------------------------
 * InputSocketRef inline methods.
 */

inline Span<const OutputSocketRef *> InputSocketRef::linked_sockets() const
{
  return m_linked_sockets.as_span().cast<const OutputSocketRef *>();
}

inline Span<const OutputSocketRef *> InputSocketRef::directly_linked_sockets() const
{
  return m_directly_linked_sockets.as_span().cast<const OutputSocketRef *>();
}

/* --------------------------------------------------------------------
 * OutputSocketRef inline methods.
 */

inline Span<const InputSocketRef *> OutputSocketRef::linked_sockets() const
{
  return m_linked_sockets.as_span().cast<const InputSocketRef *>();
}

inline Span<const InputSocketRef *> OutputSocketRef::directly_linked_sockets() const
{
  return m_directly_linked_sockets.as_span().cast<const InputSocketRef *>();
}

/* --------------------------------------------------------------------
 * NodeRef inline methods.
 */

inline const NodeTreeRef &NodeRef::tree() const
{
  return *m_tree;
}

inline Span<const InputSocketRef *> NodeRef::inputs() const
{
  return m_inputs.as_span();
}

inline Span<const OutputSocketRef *> NodeRef::outputs() const
{
  return m_outputs.as_span();
}

inline const InputSocketRef &NodeRef::input(uint index) const
{
  return *m_inputs[index];
}

inline const OutputSocketRef &NodeRef::output(uint index) const
{
  return *m_outputs[index];
}

inline bNode *NodeRef::bnode() const
{
  return m_bnode;
}

inline bNodeTree *NodeRef::btree() const
{
  return m_tree->btree();
}

inline PointerRNA *NodeRef::rna() const
{
  return const_cast<PointerRNA *>(&m_rna);
}

inline StringRefNull NodeRef::idname() const
{
  return m_bnode->idname;
}

inline StringRefNull NodeRef::name() const
{
  return m_bnode->name;
}

inline uint NodeRef::id() const
{
  return m_id;
}

inline bool NodeRef::is_reroute_node() const
{
  return m_bnode->type == NODE_REROUTE;
}

inline bool NodeRef::is_group_node() const
{
  return m_bnode->type == NODE_GROUP;
}

inline bool NodeRef::is_group_input_node() const
{
  return m_bnode->type == NODE_GROUP_INPUT;
}

inline bool NodeRef::is_group_output_node() const
{
  return m_bnode->type == NODE_GROUP_OUTPUT;
}

/* --------------------------------------------------------------------
 * NodeRef inline methods.
 */

inline Span<const NodeRef *> NodeTreeRef::nodes() const
{
  return m_nodes_by_id.as_span();
}

inline Span<const NodeRef *> NodeTreeRef::nodes_with_idname(StringRef idname) const
{
  const Vector<NodeRef *> *nodes = m_nodes_by_idname.lookup_ptr(idname);
  if (nodes == nullptr) {
    return {};
  }
  else {
    return nodes->as_span();
  }
}

inline Span<const SocketRef *> NodeTreeRef::sockets() const
{
  return m_sockets_by_id.as_span();
}

inline Span<const InputSocketRef *> NodeTreeRef::input_sockets() const
{
  return m_input_sockets.as_span();
}

inline Span<const OutputSocketRef *> NodeTreeRef::output_sockets() const
{
  return m_output_sockets.as_span();
}

inline bNodeTree *NodeTreeRef::btree() const
{
  return m_btree;
}

}  // namespace BKE

#endif /* __BKE_NODE_TREE_REF_HH__ */
