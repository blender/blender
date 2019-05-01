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

#ifndef __COM_CONVERTER_H__
#define __COM_CONVERTER_H__

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

struct bNode;

class Node;
class NodeOperation;
class NodeOperationInput;
class NodeOperationOutput;
class NodeOperationBuilder;

/**
 * \brief Conversion methods for the compositor
 */
class Converter {
 public:
  /**
   * \brief Convert/wraps a bNode in its Node instance.
   *
   * For all nodetypes a wrapper class is created.
   *
   * \note When adding a new node to blender, this method needs to be changed to return the correct
   * Node instance.
   *
   * \see Node
   */
  static Node *convert(bNode *b_node);

  /**
   * \brief True if the node is considered 'fast'.
   *
   * Slow nodes will be skipped if fast execution is required.
   */
  static bool is_fast_node(bNode *b_node);

  /**
   * \brief This method will add a datetype conversion rule when the to-socket does not support the
   * from-socket actual data type.
   *
   * \note this method is called when conversion is needed.
   *
   * \param link: the NodeLink what needs conversion
   * \param system: the ExecutionSystem to add the conversion to.
   * \see NodeLink - a link between two sockets
   */
  static NodeOperation *convertDataType(NodeOperationOutput *from, NodeOperationInput *to);

  /**
   * \brief This method will add a resolution rule based on the settings of the NodeInput.
   *
   * \note Conversion logic is implemented in this method
   * \see InputSocketResizeMode for the possible conversions.
   *
   * \param link: the NodeLink what needs conversion
   * \param system: the ExecutionSystem to add the conversion to.
   * \see NodeLink - a link between two sockets
   */
  static void convertResolution(NodeOperationBuilder &builder,
                                NodeOperationOutput *fromSocket,
                                NodeOperationInput *toSocket);

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("COM:Converter")
#endif
};
#endif
