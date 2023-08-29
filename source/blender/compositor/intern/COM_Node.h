/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_vector.hh"

#include "DNA_node_types.h"

/* common node includes
 * added here so node files don't have to include themselves
 */
#include "COM_CompositorContext.h"
#include "COM_NodeConverter.h"

namespace blender::compositor {

class NodeOperation;
class NodeConverter;

/**
 * My node documentation.
 */
class Node {
 private:
  /**
   * \brief stores the reference to the SDNA bNode struct
   */
  bNodeTree *editor_node_tree_;

  /**
   * \brief stores the reference to the SDNA bNode struct
   */
  const bNode *editor_node_;

  /**
   * \brief Is this node part of the active group
   */
  bool in_active_group_;

  /**
   * \brief Instance key to identify the node in an instance hash table
   */
  bNodeInstanceKey instance_key_;

 protected:
  /**
   * \brief the list of actual input-sockets \see NodeInput
   */
  Vector<NodeInput *> inputs_;

  /**
   * \brief the list of actual output-sockets \see NodeOutput
   */
  Vector<NodeOutput *> outputs_;

 public:
  Node(bNode *editor_node, bool create_sockets = true);
  virtual ~Node();

  /**
   * \brief get the reference to the SDNA bNode struct
   */
  const bNode *get_bnode() const
  {
    return editor_node_;
  }

  /**
   * \brief get the reference to the SDNA bNodeTree struct
   */
  bNodeTree *get_bnodetree() const
  {
    return editor_node_tree_;
  }

  /**
   * \brief set the reference to the bNode
   * \note used in Node instances to receive the storage/settings and complex
   * node for highlight during execution.
   * \param bNode:
   */
  void set_bnode(bNode *node)
  {
    editor_node_ = node;
  }

  /**
   * \brief set the reference to the bNodeTree
   * \param bNodeTree:
   */
  void set_bnodetree(bNodeTree *nodetree)
  {
    editor_node_tree_ = nodetree;
  }

  /**
   * \brief get access to the vector of input sockets
   */
  const Vector<NodeInput *> &get_input_sockets() const
  {
    return inputs_;
  }

  /**
   * \brief get access to the vector of input sockets
   */
  const Vector<NodeOutput *> &get_output_sockets() const
  {
    return outputs_;
  }

  /**
   * Get the reference to a certain output-socket.
   * \param index: The index of the needed output-socket.
   */
  NodeOutput *get_output_socket(unsigned int index = 0) const;

  /**
   * get the reference to a certain input-socket.
   * \param index: The index of the needed input-socket.
   */
  NodeInput *get_input_socket(unsigned int index) const;

  /**
   * \brief Is this node in the active group (the group that is being edited)
   * \param is_in_active_group:
   */
  void set_is_in_active_group(bool value)
  {
    in_active_group_ = value;
  }

  /**
   * \brief Is this node part of the active group
   * the active group is the group that is currently being edited. When no group is edited,
   * the active group will be the main tree (all nodes that are not part of a group will be active)
   * \return bool [false:true]
   */
  inline bool is_in_active_group() const
  {
    return in_active_group_;
  }

  /**
   * \brief convert node to operation
   *
   * \todo this must be described further
   *
   * \param system: the ExecutionSystem where the operations need to be added
   * \param context: reference to the CompositorContext
   */
  virtual void convert_to_operations(NodeConverter &converter,
                                     const CompositorContext &context) const = 0;

  void set_instance_key(bNodeInstanceKey instance_key)
  {
    instance_key_ = instance_key;
  }
  bNodeInstanceKey get_instance_key() const
  {
    return instance_key_;
  }

 protected:
  /**
   * \brief add an NodeInput to the collection of input-sockets
   * \note may only be called in an constructor
   * \param socket: the NodeInput to add
   */
  void add_input_socket(DataType datatype);
  void add_input_socket(DataType datatype, bNodeSocket *socket);

  /**
   * \brief add an NodeOutput to the collection of output-sockets
   * \note may only be called in an constructor
   * \param socket: the NodeOutput to add
   */
  void add_output_socket(DataType datatype);
  void add_output_socket(DataType datatype, bNodeSocket *socket);

  bNodeSocket *get_editor_input_socket(int editor_node_input_socket_index);
  bNodeSocket *get_editor_output_socket(int editor_node_output_socket_index);
};

/**
 * \brief NodeInput are sockets that can receive data/input
 * \ingroup Model
 */
class NodeInput {
 private:
  Node *node_;
  bNodeSocket *editor_socket_;

  DataType datatype_;

  /**
   * \brief link connected to this NodeInput.
   * An input socket can only have a single link
   */
  NodeOutput *link_;

 public:
  NodeInput(Node *node, bNodeSocket *b_socket, DataType datatype);

  Node *get_node() const
  {
    return node_;
  }
  DataType get_data_type() const
  {
    return datatype_;
  }
  bNodeSocket *get_bnode_socket() const
  {
    return editor_socket_;
  }

  void set_link(NodeOutput *link);
  bool is_linked() const
  {
    return link_;
  }
  NodeOutput *get_link()
  {
    return link_;
  }

  float get_editor_value_float() const;
  void get_editor_value_color(float *value) const;
  void get_editor_value_vector(float *value) const;
};

/**
 * \brief NodeOutput are sockets that can send data/input
 * \ingroup Model
 */
class NodeOutput {
 private:
  Node *node_;
  bNodeSocket *editor_socket_;

  DataType datatype_;

 public:
  NodeOutput(Node *node, bNodeSocket *b_socket, DataType datatype);

  Node *get_node() const
  {
    return node_;
  }
  DataType get_data_type() const
  {
    return datatype_;
  }
  bNodeSocket *get_bnode_socket() const
  {
    return editor_socket_;
  }

  float get_editor_value_float();
  void get_editor_value_color(float *value);
  void get_editor_value_vector(float *value);
};

}  // namespace blender::compositor
