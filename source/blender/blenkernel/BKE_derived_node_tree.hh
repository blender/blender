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

#ifndef __BKE_DERIVED_NODE_TREE_HH__
#define __BKE_DERIVED_NODE_TREE_HH__

/** \file
 * \ingroup bke
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

#include "BKE_node_tree_ref.hh"

namespace blender::bke {

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
  uint id_;

  friend DerivedNodeTree;

 public:
  const DNode &node() const;

  uint id() const;
  uint index() const;

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
  uint id_;

  friend DerivedNodeTree;

 public:
  const InputSocketRef &socket_ref() const;
  bNodeSocket *bsocket() const;
  const DParentNode *parent() const;
  Span<const DInputSocket *> linked_sockets() const;
  uint id() const;
  StringRefNull name() const;
};

class DNode : NonCopyable, NonMovable {
 private:
  const NodeRef *node_ref_;
  DParentNode *parent_;

  Span<DInputSocket *> inputs_;
  Span<DOutputSocket *> outputs_;

  uint id_;

  friend DerivedNodeTree;

 public:
  const NodeRef &node_ref() const;
  const DParentNode *parent() const;

  Span<const DInputSocket *> inputs() const;
  Span<const DOutputSocket *> outputs() const;

  const DInputSocket &input(uint index) const;
  const DOutputSocket &output(uint index) const;

  uint id() const;

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
  uint id_;

  friend DerivedNodeTree;

 public:
  const DParentNode *parent() const;
  const NodeRef &node_ref() const;
  uint id() const;
};

using NodeTreeRefMap = Map<bNodeTree *, std::unique_ptr<const NodeTreeRef>>;

class DerivedNodeTree : NonCopyable, NonMovable {
 private:
  LinearAllocator<> allocator_;
  bNodeTree *btree_;
  Vector<DNode *> nodes_by_id_;
  Vector<DGroupInput *> group_inputs_;
  Vector<DParentNode *> parent_nodes_;

  Vector<DSocket *> sockets_by_id_;
  Vector<DInputSocket *> input_sockets_;
  Vector<DOutputSocket *> output_sockets_;

  Map<const bNodeType *, Vector<DNode *>> nodes_by_type_;

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

inline uint DSocket::id() const
{
  return id_;
}

inline uint DSocket::index() const
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
  return *(DInputSocket *)this;
}

inline const DOutputSocket &DSocket::as_output() const
{
  return *(DOutputSocket *)this;
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
  return linked_sockets_.as_span();
}

inline Span<const DGroupInput *> DInputSocket::linked_group_inputs() const
{
  return linked_group_inputs_.as_span();
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
  return linked_sockets_.as_span();
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
  return linked_sockets_.as_span();
}

inline uint DGroupInput::id() const
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

inline const DInputSocket &DNode::input(uint index) const
{
  return *inputs_[index];
}

inline const DOutputSocket &DNode::output(uint index) const
{
  return *outputs_[index];
}

inline uint DNode::id() const
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

inline uint DParentNode::id() const
{
  return id_;
}

/* --------------------------------------------------------------------
 * DerivedNodeTree inline methods.
 */

inline Span<const DNode *> DerivedNodeTree::nodes() const
{
  return nodes_by_id_.as_span();
}

inline Span<const DNode *> DerivedNodeTree::nodes_by_type(StringRefNull idname) const
{
  const bNodeType *nodetype = nodeTypeFind(idname.data());
  return this->nodes_by_type(nodetype);
}

inline Span<const DNode *> DerivedNodeTree::nodes_by_type(const bNodeType *nodetype) const
{
  const Vector<DNode *> *nodes = nodes_by_type_.lookup_ptr(nodetype);
  if (nodes == nullptr) {
    return {};
  }
  else {
    return nodes->as_span();
  }
}

inline Span<const DSocket *> DerivedNodeTree::sockets() const
{
  return sockets_by_id_.as_span();
}

inline Span<const DInputSocket *> DerivedNodeTree::input_sockets() const
{
  return input_sockets_.as_span();
}

inline Span<const DOutputSocket *> DerivedNodeTree::output_sockets() const
{
  return output_sockets_.as_span();
}

inline Span<const DGroupInput *> DerivedNodeTree::group_inputs() const
{
  return group_inputs_.as_span();
}

}  // namespace blender::bke

#endif /* __BKE_DERIVED_NODE_TREE_HH__ */
