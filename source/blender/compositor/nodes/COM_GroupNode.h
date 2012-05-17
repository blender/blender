/*
 * Copyright 2011, Blender Foundation.
 *
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
 * Contributor: 
 *		Jeroen Bakker 
 *		Monique Dewanchand
 */

#ifndef _COM_GroupNode_h_
#define _COM_GroupNode_h_

#include "COM_Node.h"
#include "COM_ExecutionSystem.h"

/**
  * @brief Represents a group node
  * @ingroup Node
  */
class GroupNode: public Node {
public:
	GroupNode(bNode *editorNode);
	void convertToOperations(ExecutionSystem* graph, CompositorContext * context);

	/**
	  * @brief check if this node a group node.
	  * @returns true
	  */
	bool isGroupNode() const { return true; }

	/**
	  * @brief ungroup this group node.
	  * during ungroup the subtree (internal nodes and links) of the group node
	  * are added to the ExecutionSystem.
	  *
	  * Between the main tree and the subtree proxy nodes will be added
	  * to translate between InputSocket and OutputSocket
	  *
	  * @param system the ExecutionSystem where to add the subtree
	  */
	void ungroup(ExecutionSystem &system);
};

#endif
