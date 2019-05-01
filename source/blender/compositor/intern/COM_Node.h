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

#ifndef __COM_NODE_H__
#define __COM_NODE_H__

#include "DNA_node_types.h"
#include <vector>
#include <string>
#include <algorithm>

/* common node includes
 * added here so node files don't have to include themselves
 */
#include "COM_CompositorContext.h"
#include "COM_NodeConverter.h"

class Node;
class NodeOperation;
class NodeConverter;

/**
 * My node documentation.
 */
class Node {
 public:
  typedef std::vector<NodeInput *> Inputs;
  typedef std::vector<NodeOutput *> Outputs;

 private:
  /**
   * \brief stores the reference to the SDNA bNode struct
   */
  bNodeTree *m_editorNodeTree;

  /**
   * \brief stores the reference to the SDNA bNode struct
   */
  bNode *m_editorNode;

  /**
   * \brief the list of actual inputsockets \see NodeInput
   */
  Inputs m_inputsockets;

  /**
   * \brief the list of actual outputsockets \see NodeOutput
   */
  Outputs m_outputsockets;

  /**
   * \brief Is this node part of the active group
   */
  bool m_inActiveGroup;

  /**
   * \brief Instance key to identify the node in an instance hash table
   */
  bNodeInstanceKey m_instanceKey;

 protected:
  /**
   * \brief get access to the vector of input sockets
   */
  const Inputs &getInputSockets() const
  {
    return this->m_inputsockets;
  }

  /**
   * \brief get access to the vector of input sockets
   */
  const Outputs &getOutputSockets() const
  {
    return this->m_outputsockets;
  }

 public:
  Node(bNode *editorNode, bool create_sockets = true);
  virtual ~Node();

  /**
   * \brief get the reference to the SDNA bNode struct
   */
  bNode *getbNode() const
  {
    return m_editorNode;
  }

  /**
   * \brief get the reference to the SDNA bNodeTree struct
   */
  bNodeTree *getbNodeTree() const
  {
    return m_editorNodeTree;
  }

  /**
   * \brief set the reference to the bNode
   * \note used in Node instances to receive the storage/settings and complex
   * node for highlight during execution.
   * \param bNode:
   */
  void setbNode(bNode *node)
  {
    this->m_editorNode = node;
  }

  /**
   * \brief set the reference to the bNodeTree
   * \param bNodeTree:
   */
  void setbNodeTree(bNodeTree *nodetree)
  {
    this->m_editorNodeTree = nodetree;
  }

  /**
   * \brief Return the number of input sockets of this node.
   */
  unsigned int getNumberOfInputSockets() const
  {
    return this->m_inputsockets.size();
  }

  /**
   * \brief Return the number of output sockets of this node.
   */
  unsigned int getNumberOfOutputSockets() const
  {
    return this->m_outputsockets.size();
  }

  /**
   * get the reference to a certain outputsocket
   * \param index:
   * the index of the needed outputsocket
   */
  NodeOutput *getOutputSocket(const unsigned int index) const;

  /**
   * get the reference to the first outputsocket
   * \param index:
   * the index of the needed outputsocket
   */
  inline NodeOutput *getOutputSocket() const
  {
    return getOutputSocket(0);
  }

  /**
   * get the reference to a certain inputsocket
   * \param index:
   * the index of the needed inputsocket
   */
  NodeInput *getInputSocket(const unsigned int index) const;

  /** Check if this is an input node
   * An input node is a node that only has output sockets and no input sockets
   */
  bool isInputNode() const
  {
    return m_inputsockets.empty();
  }

  /**
   * \brief Is this node in the active group (the group that is being edited)
   * \param isInActiveGroup:
   */
  void setIsInActiveGroup(bool value)
  {
    this->m_inActiveGroup = value;
  }

  /**
   * \brief Is this node part of the active group
   * the active group is the group that is currently being edited. When no group is edited,
   * the active group will be the main tree (all nodes that are not part of a group will be active)
   * \return bool [false:true]
   */
  inline bool isInActiveGroup() const
  {
    return this->m_inActiveGroup;
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

  /**
   * Create dummy warning operation, use when we can't get the source data.
   */
  NodeOperation *convertToOperations_invalid_index(NodeConverter *compiler, int index) const;
  /**
   * when a node has no valid data (missing image or a group nodes ID pointer is NULL)
   * call this function from #convertToOperations, this way the node sockets are converted
   * into valid outputs, without this the compositor system gets confused and crashes, see [#32490]
   */
  void convertToOperations_invalid(NodeConverter *compiler) const;

  void setInstanceKey(bNodeInstanceKey instance_key)
  {
    m_instanceKey = instance_key;
  }
  bNodeInstanceKey getInstanceKey() const
  {
    return m_instanceKey;
  }

 protected:
  /**
   * \brief add an NodeInput to the collection of inputsockets
   * \note may only be called in an constructor
   * \param socket: the NodeInput to add
   */
  void addInputSocket(DataType datatype);
  void addInputSocket(DataType datatype, bNodeSocket *socket);

  /**
   * \brief add an NodeOutput to the collection of outputsockets
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
  Node *m_node;
  bNodeSocket *m_editorSocket;

  DataType m_datatype;

  /**
   * \brief link connected to this NodeInput.
   * An input socket can only have a single link
   */
  NodeOutput *m_link;

 public:
  NodeInput(Node *node, bNodeSocket *b_socket, DataType datatype);

  Node *getNode() const
  {
    return this->m_node;
  }
  DataType getDataType() const
  {
    return m_datatype;
  }
  bNodeSocket *getbNodeSocket() const
  {
    return this->m_editorSocket;
  }

  void setLink(NodeOutput *link);
  bool isLinked() const
  {
    return m_link;
  }
  NodeOutput *getLink()
  {
    return m_link;
  }

  float getEditorValueFloat();
  void getEditorValueColor(float *value);
  void getEditorValueVector(float *value);
};

/**
 * \brief NodeOutput are sockets that can send data/input
 * \ingroup Model
 */
class NodeOutput {
 private:
  Node *m_node;
  bNodeSocket *m_editorSocket;

  DataType m_datatype;

 public:
  NodeOutput(Node *node, bNodeSocket *b_socket, DataType datatype);

  Node *getNode() const
  {
    return this->m_node;
  }
  DataType getDataType() const
  {
    return m_datatype;
  }
  bNodeSocket *getbNodeSocket() const
  {
    return this->m_editorSocket;
  }

  float getEditorValueFloat();
  void getEditorValueColor(float *value);
  void getEditorValueVector(float *value);
};

#endif /* __COM_NODE_H__ */
