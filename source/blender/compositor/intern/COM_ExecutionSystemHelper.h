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

class ExecutionGroup;

#ifndef _COM_ExecutionSystemHelper_h
#define _COM_ExecutionSystemHelper_h

#include "DNA_node_types.h"
#include <vector>
#include "COM_Node.h"
#include "COM_SocketConnection.h"
#include "BKE_text.h"
#include "COM_ExecutionGroup.h"

using namespace std;

/**
 *
 */
class ExecutionSystemHelper {

public:

	/**
	 * @brief add an bNodeTree to the nodes list and connections
	 * @param system Execution system
	 * @param nodes_start Starting index in the system's nodes list for nodes in this tree.
	 * @param tree bNodeTree to add
	 * @return Node representing the "Compositor node" of the maintree. or NULL when a subtree is added
	 */
	static void addbNodeTree(ExecutionSystem &system, int nodes_start, bNodeTree *tree, bNode *groupnode);

	/**
	 * @brief add an editor node to the system.
	 * this node is converted to a Node instance.
	 * and the converted node is returned
	 *
	 * @param bNode node to add
	 * @return Node that represents the bNode or null when not able to convert.
	 */
	static Node *addNode(vector<Node *>& nodes, bNode *bNode, bool isInActiveGroup);

	/**
	 * @brief Add a Node to a list
	 *
	 * @param nodes the list where the node needs to be added to
	 * @param node the node to be added
	 */
	static void addNode(vector<Node *>& nodes, Node *node);

	/**
	 * @brief Add an operation to the operation list
	 *
	 * The id of the operation is updated.
	 *
	 * @param operations the list where the operation need to be added to
	 * @param operation the operation to add
	 */
	static void addOperation(vector<NodeOperation *> &operations, NodeOperation *operation);

	/**
	 * @brief Add an ExecutionGroup to a list
	 *
	 * The id of the ExecutionGroup is updated.
	 *
	 * @param executionGroups the list where the executionGroup need to be added to
	 * @param executionGroup the ExecutionGroup to add
	 */
	static void addExecutionGroup(vector<ExecutionGroup *>& executionGroups, ExecutionGroup *executionGroup);

	/**
	 * Find all Node Operations that needs to be executed.
	 * @param rendering
	 * the rendering parameter will tell what type of execution we are doing
	 * FALSE is editing, TRUE is rendering
	 */
	static void findOutputNodeOperations(vector<NodeOperation *> *result, vector<NodeOperation *>& operations, bool rendering);

	/**
	 * @brief add a bNodeLink to the list of links
	 * the bNodeLink will be wrapped in a SocketConnection
	 *
	 * @note Cyclic links will be ignored
	 *
	 * @param node_range list of possible nodes for lookup.
	 * @param links list of links to add the bNodeLink to
	 * @param bNodeLink the link to be added
	 * @return the created SocketConnection or NULL
	 */
	static SocketConnection *addNodeLink(NodeRange &node_range, vector<SocketConnection *>& links, bNodeLink *bNodeLink);

	/**
	 * @brief create a new SocketConnection and add to a vector of links
	 * @param links the vector of links
	 * @param fromSocket the startpoint of the connection
	 * @param toSocket the endpoint of the connection
	 * @return the new created SocketConnection
	 */
	static SocketConnection *addLink(vector<SocketConnection *>& links, OutputSocket *fromSocket, InputSocket *toSocket);

	/**
	 * @brief dumps the content of the execution system to standard out
	 * @param system the execution system to dump
	 */
	static void debugDump(ExecutionSystem *system);

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("COM:ExecutionSystemHelper")
#endif
};

#endif /* _COM_ExecutionSystemHelper_h */
