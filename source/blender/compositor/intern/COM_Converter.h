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

#include "COM_NodeOperation.h"

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

struct bNode;

namespace blender::compositor {

class Node;
class NodeOperationInput;
class NodeOperationOutput;
class NodeOperationBuilder;

/**
 * \brief Wraps a bNode in its Node instance.
 *
 * For all nodetypes a wrapper class is created.
 *
 * \note When adding a new node to blender, this method needs to be changed to return the correct
 * Node instance.
 *
 * \see Node
 */
Node *COM_convert_bnode(bNode *b_node);

/**
 * \brief True if the node is considered 'fast'.
 *
 * Slow nodes will be skipped if fast execution is required.
 */
bool COM_bnode_is_fast_node(const bNode &b_node);

/**
 * \brief This function will add a datetype conversion rule when the to-socket does not support the
 * from-socket actual data type.
 */
NodeOperation *COM_convert_data_type(const NodeOperationOutput &from,
                                     const NodeOperationInput &to);

/**
 * \brief This function will add a resolution rule based on the settings of the NodeInput.
 *
 * \note Conversion logic is implemented in this function.
 * \see InputSocketResizeMode for the possible conversions.
 */
void COM_convert_resolution(NodeOperationBuilder &builder,
                            NodeOperationOutput *fromSocket,
                            NodeOperationInput *toSocket);

}  // namespace blender::compositor
