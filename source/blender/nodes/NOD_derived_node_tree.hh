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
 * DerivedNodeTree provides a flattened view on a bNodeTree, i.e. node groups are inlined. It
 * builds on top of NodeTreeRef and supports similar queries efficiently.
 *
 * Every inlined node remembers its path to the parent ("call stack").
 *
 * Unlinked group node inputs are handled separately from other sockets.
 *
 * There is a dot graph exporter for debugging purposes.
 */

#include "NOD_node_tree_ref.hh"

namespace blender::nodes {

class DSocket;
class DInputSocket;
class DOutputSocket;
class DNode;
class DParentNode;
class DGroupInput;
class DerivedNodeTree;

class DSocket : NonCopyable, NonMovable {
 protected:
  DNode *node_;
  const SocketRef *socket_ref_;
  int id_;

  friend DerivedNodeTree;

 public:
  const DNode &node() const;

  int id() const;
  int index() const;

  bool is_input() const;
  bool is_output() const;

  const DSocket &as_base() const;
  const DInputSocket &as_input() const;
  const DOutputSocket &as_output() const;

  PointerRNA *rna() const;
  StringRefNull idname() const;
  StringRefNull name() const;

  const SocketRef &socket_ref() const;
  bNodeSocket *bsocket() const;

  bool is_available() const;
};

class DInputSocket : public DSocket {
 private:
  Vector<DOutputSocket *> linked_sockets_;
  Vector<DGroupInput *> linked_group_inputs_;

  friend DerivedNodeTree;

 public:
  const InputSocketRef &socket_ref() const;

  Span<const DOutputSocket *> linked_sockets() const;
  Span<const DGroupInput *> linked_group_inputs() const;

  bool is_linked() const;
};

class DOutputSocket : public DSocket {
 private:
  Vector<DInputSocket *> linked_sockets_;

  friend DerivedNodeTree;

 public:
  const OutputSocketRef &socket_ref() const;
  Span<const DInputSocket *> linked_sockets() const;
};

class DGroupInput : NonCopyable, NonMovable {
 private:
  const InputSocketRef *socket_ref_;
  DParentNode *parent_;
  Vector<DInputSocket *> linked_sockets_;
  int id_;

  friend DerivedNodeTree;

 public:
  const InputSocketRef &socket_ref() const;
  bNodeSocket *bsocket() const;
  const DParentNode *parent() const;
  Span<const DInputSocket *> linked_sockets() const;
  int id() const;
  StringRefNull name() const;
};

class DNode : NonCopyable, NonMovable {
 private:
  const NodeRef *node_ref_;
  DParentNode *parent_;

  Span<DInputSocket *> inputs_;
  Span<DOutputSocket *> outputs_;

  int id_;

  friend DerivedNodeTree;

 public:
  const NodeRef &node_ref() const;
  const DParentNode *parent() const;

  Span<const DInputSocket *> inputs() const;
  Span<const DOutputSocket *> outputs() const;

  const DInputSocket &input(int index) const;
  const DOutputSocket &output(int index) const;

  const DInputSocket &input(int index, StringRef expected_name) const;
  const DOutputSocket &output(int index, StringRef expected_name) const;

  int id() const;

  PointerRNA *rna() const;
  StringRefNull idname() const;
  StringRefNull name() const;

 private:
  void destruct_with_sockets();
};

class DParentNode : NonCopyable, NonMovable {
 private:
  const NodeRef *node_ref_;
  DParentNode *parent_;
  int id_;

  friend DerivedNodeTree;

 public:
  const DParentNode *parent() const;
  const NodeRef &node_ref() const;
  int id() const;
};

using NodeTreeRefMap = Map<bNodeTree *, std::unique_ptr<const NodeTreeRef>>;

class DerivedNodeTree : NonCopyable, NonMovable {
 private:
  LinearAllocator<> allocator_;
  Vector<DNode *> nodes_by_id_;
  Vector<DGroupInput *> group_inputs_;
  Vector<DParentNode *> parent_nodes_;

  Vector<DSocket *> sockets_by_id_;
  Vector<DInputSocket *> input_sockets_;
  Vector<DOutputSocket *> output_sockets_;

  MultiValueMap<const bNodeType *, DNode *> nodes_by_type_;

 public:
  DerivedNodeTree(bNodeTree *btree, NodeTreeRefMap &node_tree_refs);
  ~DerivedNodeTree();

  Span<const DNode *> nodes() const;
  Span<const DNode *> nodes_by_type(StringRefNull idname) const;
  Span<const DNode *> nodes_by_type(const bNodeType *nodetype) const;

  Span<const DSocket *> sockets() const;
  Span<const DInputSocket *> input_sockets() const;
  Span<const DOutputSocket *> output_sockets() const;

  Span<const DGroupInput *> group_inputs() const;

  std::string to_dot() const;

 private:
  /* Utility functions used during construction. */
  void insert_nodes_and_links_in_id_order(const NodeTreeRef &tree_ref,
                                          DParentNode *parent,
                                          Vector<DNode *> &all_nodes);
  DNode &create_node(const NodeRef &node_ref,
                     DParentNode *parent,
                     MutableSpan<DSocket *> r_sockets_map);
  void expand_groups(Vector<DNode *> &all_nodes,
                     Vector<DGroupInput *> &all_group_inputs,
                     Vector<DParentNode *> &all_parent_nodes,
                     NodeTreeRefMap &node_tree_refs);
  void expand_group_node(DNode &group_node,
                         Vector<DNode *> &all_nodes,
                         Vector<DGroupInput *> &all_group_inputs,
                         Vector<DParentNode *> &all_parent_nodes,
                         NodeTreeRefMap &node_tree_refs);
  void create_group_inputs_for_unlinked_inputs(DNode &node,
                                               Vector<DGroupInput *> &all_group_inputs);
  void relink_group_inputs(const NodeTreeRef &group_ref,
                           Span<DNode *> nodes_by_id,
                           DNode &group_node);
  void relink_group_outputs(const NodeTreeRef &group_ref,
                            Span<DNode *> nodes_by_id,
                            DNode &group_node);
  void remove_expanded_group_interfaces(Vector<DNode *> &all_nodes);
  void remove_unused_group_inputs(Vector<DGroupInput *> &all_group_inputs);
  void store_in_this_and_init_ids(Vector<DNode *> &&all_nodes,
                                  Vector<DGroupInput *> &&all_group_inputs,
                                  Vector<DParentNode *> &&all_parent_nodes);
};

/* --------------------------------------------------------------------
 * DSocket inline methods.
 */

inline const DNode &DSocket::node() const
{
  return *node_;
}

inline int DSocket::id() const
{
  return id_;
}

inline int DSocket::index() const
{
  return socket_ref_->index();
}

inline bool DSocket::is_input() const
{
  return socket_ref_->is_input();
}

inline bool DSocket::is_output() const
{
  return socket_ref_->is_output();
}

inline const DSocket &DSocket::as_base() const
{
  return *this;
}

inline const DInputSocket &DSocket::as_input() const
{
  return static_cast<const DInputSocket &>(*this);
}

inline const DOutputSocket &DSocket::as_output() const
{
  return static_cast<const DOutputSocket &>(*this);
}

inline PointerRNA *DSocket::rna() const
{
  return socket_ref_->rna();
}

inline StringRefNull DSocket::idname() const
{
  return socket_ref_->idname();
}

inline StringRefNull DSocket::name() const
{
  return socket_ref_->name();
}

inline const SocketRef &DSocket::socket_ref() const
{
  return *socket_ref_;
}

inline bNodeSocket *DSocket::bsocket() const
{
  return socket_ref_->bsocket();
}

inline bool DSocket::is_available() const
{
  return (socket_ref_->bsocket()->flag & SOCK_UNAVAIL) == 0;
}

/* --------------------------------------------------------------------
 * DInputSocket inline methods.
 */

inline const InputSocketRef &DInputSocket::socket_ref() const
{
  return socket_ref_->as_input();
}

inline Span<const DOutputSocket *> DInputSocket::linked_sockets() const
{
  return linked_sockets_;
}

inline Span<const DGroupInput *> DInputSocket::linked_group_inputs() const
{
  return linked_group_inputs_;
}

inline bool DInputSocket::is_linked() const
{
  return linked_sockets_.size() > 0 || linked_group_inputs_.size() > 0;
}

/* --------------------------------------------------------------------
 * DOutputSocket inline methods.
 */

inline const OutputSocketRef &DOutputSocket::socket_ref() const
{
  return socket_ref_->as_output();
}

inline Span<const DInputSocket *> DOutputSocket::linked_sockets() const
{
  return linked_sockets_;
}

/* --------------------------------------------------------------------
 * DGroupInput inline methods.
 */

inline const InputSocketRef &DGroupInput::socket_ref() const
{
  return *socket_ref_;
}

inline bNodeSocket *DGroupInput::bsocket() const
{
  return socket_ref_->bsocket();
}

inline const DParentNode *DGroupInput::parent() const
{
  return parent_;
}

inline Span<const DInputSocket *> DGroupInput::linked_sockets() const
{
  return linked_sockets_;
}

inline int DGroupInput::id() const
{
  return id_;
}

inline StringRefNull DGroupInput::name() const
{
  return socket_ref_->name();
}

/* --------------------------------------------------------------------
 * DNode inline methods.
 */

inline const NodeRef &DNode::node_ref() const
{
  return *node_ref_;
}

inline const DParentNode *DNode::parent() const
{
  return parent_;
}

inline Span<const DInputSocket *> DNode::inputs() const
{
  return inputs_;
}

inline Span<const DOutputSocket *> DNode::outputs() const
{
  return outputs_;
}

inline const DInputSocket &DNode::input(int index) const
{
  return *inputs_[index];
}

inline const DOutputSocket &DNode::output(int index) const
{
  return *outputs_[index];
}

inline const DInputSocket &DNode::input(int index, StringRef expected_name) const
{
  const DInputSocket &socket = *inputs_[index];
  BLI_assert(socket.name() == expected_name);
  UNUSED_VARS_NDEBUG(expected_name);
  return socket;
}

inline const DOutputSocket &DNode::output(int index, StringRef expected_name) const
{
  const DOutputSocket &socket = *outputs_[index];
  BLI_assert(socket.name() == expected_name);
  UNUSED_VARS_NDEBUG(expected_name);
  return socket;
}

inline int DNode::id() const
{
  return id_;
}

inline PointerRNA *DNode::rna() const
{
  return node_ref_->rna();
}

inline StringRefNull DNode::idname() const
{
  return node_ref_->idname();
}

inline StringRefNull DNode::name() const
{
  return node_ref_->name();
}

/* --------------------------------------------------------------------
 * DParentNode inline methods.
 */

inline const DParentNode *DParentNode::parent() const
{
  return parent_;
}

inline const NodeRef &DParentNode::node_ref() const
{
  return *node_ref_;
}

inline int DParentNode::id() const
{
  return id_;
}

/* --------------------------------------------------------------------
 * DerivedNodeTree inline methods.
 */

inline Span<const DNode *> DerivedNodeTree::nodes() const
{
  return nodes_by_id_;
}

inline Span<const DNode *> DerivedNodeTree::nodes_by_type(StringRefNull idname) const
{
  const bNodeType *nodetype = nodeTypeFind(idname.c_str());
  return this->nodes_by_type(nodetype);
}

inline Span<const DNode *> DerivedNodeTree::nodes_by_type(const bNodeType *nodetype) const
{
  return nodes_by_type_.lookup(nodetype);
}

inline Span<const DSocket *> DerivedNodeTree::sockets() const
{
  return sockets_by_id_;
}

inline Span<const DInputSocket *> DerivedNodeTree::input_sockets() const
{
  return input_sockets_;
}

inline Span<const DOutputSocket *> DerivedNodeTree::output_sockets() const
{
  return output_sockets_;
}

inline Span<const DGroupInput *> DerivedNodeTree::group_inputs() const
{
  return group_inputs_;
}

}  // namespace blender::nodes
