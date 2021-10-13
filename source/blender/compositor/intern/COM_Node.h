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
 *
 * Copyright 2011, Blender Foundation.
 */

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
  bNodeTree *editorNodeTree_;

  /**
   * \brief stores the reference to the SDNA bNode struct
   */
  bNode *editorNode_;

  /**
   * \brief Is this node part of the active group
   */
  bool inActiveGroup_;

  /**
   * \brief Instance key to identify the node in an instance hash table
   */
  bNodeInstanceKey instanceKey_;

 protected:
  /**
   * \brief the list of actual input-sockets \see NodeInput
   */
  Vector<NodeInput *> inputs;

  /**
   * \brief the list of actual output-sockets \see NodeOutput
   */
  Vector<NodeOutput *> outputs;

 public:
  Node(bNode *editorNode, bool create_sockets = true);
  virtual ~Node();

  /**
   * \brief get the reference to the SDNA bNode struct
   */
  bNode *getbNode() const
  {
    return editorNode_;
  }

  /**
   * \brief get the reference to the SDNA bNodeTree struct
   */
  bNodeTree *getbNodeTree() const
  {
    return editorNodeTree_;
  }

  /**
   * \brief set the reference to the bNode
   * \note used in Node instances to receive the storage/settings and complex
   * node for highlight during execution.
   * \param bNode:
   */
  void setbNode(bNode *node)
  {
    editorNode_ = node;
  }

  /**
   * \brief set the reference to the bNodeTree
   * \param bNodeTree:
   */
  void setbNodeTree(bNodeTree *nodetree)
  {
    editorNodeTree_ = nodetree;
  }

  /**
   * \brief get access to the vector of input sockets
   */
  const Vector<NodeInput *> &getInputSockets() const
  {
    return this->inputs;
  }

  /**
   * \brief get access to the vector of input sockets
   */
  const Vector<NodeOutput *> &getOutputSockets() const
  {
    return this->outputs;
  }

  /**
   * get the reference to a certain outputsocket
   * \param index:
   * the index of the needed outputsocket
   */
  NodeOutput *getOutputSocket(const unsigned int index = 0) const;

  /**
   * get the reference to a certain inputsocket
   * \param index:
   * the index of the needed inputsocket
   */
  NodeInput *getInputSocket(const unsigned int index) const;

  /**
   * \brief Is this node in the active group (the group that is being edited)
   * \param isInActiveGroup:
   */
  void setIsInActiveGroup(bool value)
  {
    inActiveGroup_ = value;
  }

  /**
   * \brief Is this node part of the active group
   * the active group is the group that is currently being edited. When no group is edited,
   * the active group will be the main tree (all nodes that are not part of a group will be active)
   * \return bool [false:true]
   */
  inline bool isInActiveGroup() const
  {
    return inActiveGroup_;
  }

  /**
   * \brief convert node to operation
   *
   * \todo this must be described further
   *
   * \param system: the ExecutionSystem where the operations need to be added
   * \param context: reference to the CompositorContext
   */
  virtual void convertToOperations(NodeConverter &converter,
                                   const CompositorContext &context) const = 0;

  void setInstanceKey(bNodeInstanceKey instance_key)
  {
    instanceKey_ = instance_key;
  }
  bNodeInstanceKey getInstanceKey() const
  {
    return instanceKey_;
  }

 protected:
  /**
   * \brief add an NodeInput to the collection of input-sockets
   * \note may only be called in an constructor
   * \param socket: the NodeInput to add
   */
  void addInputSocket(DataType datatype);
  void addInputSocket(DataType datatype, bNodeSocket *socket);

  /**
   * \brief add an NodeOutput to the collection of output-sockets
   * \note may only be called in an constructor
   * \param socket: the NodeOutput to add
   */
  void addOutputSocket(DataType datatype);
  void addOutputSocket(DataType datatype, bNodeSocket *socket);

  bNodeSocket *getEditorInputSocket(int editorNodeInputSocketIndex);
  bNodeSocket *getEditorOutputSocket(int editorNodeOutputSocketIndex);
};

/**
 * \brief NodeInput are sockets that can receive data/input
 * \ingroup Model
 */
class NodeInput {
 private:
  Node *node_;
  bNodeSocket *editorSocket_;

  DataType datatype_;

  /**
   * \brief link connected to this NodeInput.
   * An input socket can only have a single link
   */
  NodeOutput *link_;

 public:
  NodeInput(Node *node, bNodeSocket *b_socket, DataType datatype);

  Node *getNode() const
  {
    return node_;
  }
  DataType getDataType() const
  {
    return datatype_;
  }
  bNodeSocket *getbNodeSocket() const
  {
    return editorSocket_;
  }

  void setLink(NodeOutput *link);
  bool isLinked() const
  {
    return link_;
  }
  NodeOutput *getLink()
  {
    return link_;
  }

  float getEditorValueFloat() const;
  void getEditorValueColor(float *value) const;
  void getEditorValueVector(float *value) const;
};

/**
 * \brief NodeOutput are sockets that can send data/input
 * \ingroup Model
 */
class NodeOutput {
 private:
  Node *node_;
  bNodeSocket *editorSocket_;

  DataType datatype_;

 public:
  NodeOutput(Node *node, bNodeSocket *b_socket, DataType datatype);

  Node *getNode() const
  {
    return node_;
  }
  DataType getDataType() const
  {
    return datatype_;
  }
  bNodeSocket *getbNodeSocket() const
  {
    return editorSocket_;
  }

  float getEditorValueFloat();
  void getEditorValueColor(float *value);
  void getEditorValueVector(float *value);
};

}  // namespace blender::compositor
