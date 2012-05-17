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

#include "COM_SeparateRGBANode.h"

#include "COM_SeparateChannelOperation.h"
#include "COM_ExecutionSystem.h"
#include "COM_SetValueOperation.h"
#include "DNA_material_types.h" // the ramp types


SeparateRGBANode::SeparateRGBANode(bNode *editorNode): Node(editorNode) {
}


void SeparateRGBANode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
	InputSocket *imageSocket = this->getInputSocket(0);
	OutputSocket *outputRSocket = this->getOutputSocket(0);
	OutputSocket *outputGSocket = this->getOutputSocket(1);
	OutputSocket *outputBSocket = this->getOutputSocket(2);
	OutputSocket *outputASocket = this->getOutputSocket(3);

	if (outputRSocket->isConnected()) {
		SeparateChannelOperation *operation = new SeparateChannelOperation();
		operation->setChannel(0);
		imageSocket->relinkConnections(operation->getInputSocket(0), true, 0, true, graph);
		outputRSocket->relinkConnections(operation->getOutputSocket(0));
		graph->addOperation(operation);
	 }
	if (outputGSocket->isConnected()) {
		SeparateChannelOperation *operation = new SeparateChannelOperation();
		operation->setChannel(1);
		imageSocket->relinkConnections(operation->getInputSocket(0), true, 0, true, graph);
		outputGSocket->relinkConnections(operation->getOutputSocket(0));
		graph->addOperation(operation);
	 }
	if (outputBSocket->isConnected()) {
		SeparateChannelOperation *operation = new SeparateChannelOperation();
		operation->setChannel(2);
		imageSocket->relinkConnections(operation->getInputSocket(0), true, 0, true, graph);
		outputBSocket->relinkConnections(operation->getOutputSocket(0));
		graph->addOperation(operation);
	 }
	if (outputASocket->isConnected()) {
		SeparateChannelOperation *operation = new SeparateChannelOperation();
		operation->setChannel(3);
		imageSocket->relinkConnections(operation->getInputSocket(0), true, 0, true, graph);
		outputASocket->relinkConnections(operation->getOutputSocket(0));
		graph->addOperation(operation);
	 }
}
