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

#include "COM_ViewLevelsNode.h"
#include "DNA_scene_types.h"
#include "COM_ExecutionSystem.h"
#include "COM_CalculateMeanOperation.h"
#include "COM_CalculateStandardDeviationOperation.h"

ViewLevelsNode::ViewLevelsNode(bNode *editorNode): Node(editorNode)
{
}
void ViewLevelsNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context)
{
	InputSocket * input = this->getInputSocket(0);
	bool firstOperationConnected = false;
	if (input->isConnected()) {
		OutputSocket *inputSocket = input->getConnection()->getFromSocket();
		// add preview to inputSocket;
		
		OutputSocket * socket = this->getOutputSocket(0);
		if (socket->isConnected()) {
			// calculate mean operation
			CalculateMeanOperation * operation = new CalculateMeanOperation();
			input->relinkConnections(operation->getInputSocket(0), 0, graph);
			firstOperationConnected = true;
			operation->setSetting(this->getbNode()->custom1);
			socket->relinkConnections(operation->getOutputSocket());
			graph->addOperation(operation);
		}

		socket = this->getOutputSocket(1);
		if (socket->isConnected()) {
			// calculate standard deviation operation
			CalculateStandardDeviationOperation * operation = new CalculateStandardDeviationOperation();
			if (firstOperationConnected) {
				addLink(graph, inputSocket, operation->getInputSocket(0));
			}
			else {
				input->relinkConnections(operation->getInputSocket(0), 0, graph);
			}
			operation->setSetting(this->getbNode()->custom1);
			socket->relinkConnections(operation->getOutputSocket());
			graph->addOperation(operation);
		}
	}
}
	
