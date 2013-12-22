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

#include "COM_MuteNode.h"
#include "COM_SocketConnection.h"
#include "COM_SetValueOperation.h"
#include "COM_SetVectorOperation.h"
#include "COM_SetColorOperation.h"

extern "C" {
#  include "BLI_listbase.h"
}

MuteNode::MuteNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void MuteNode::reconnect(ExecutionSystem *graph, OutputSocket *output)
{
	vector<InputSocket *> &inputsockets = this->getInputSockets();
	for (unsigned int index = 0; index < inputsockets.size(); index++) {
		InputSocket *input = inputsockets[index];
		if (input->getDataType() == output->getDataType()) {
			if (input->isConnected()) {
				output->relinkConnections(input->getConnection()->getFromSocket(), false);
				/* output connections have been redirected,
				 * remove the input connection to completely unlink the node.
				 */
				input->unlinkConnections(graph);
				return;
			}
		}
	}

	createDefaultOutput(graph, output);
}

void MuteNode::createDefaultOutput(ExecutionSystem *graph, OutputSocket *output)
{
	NodeOperation *operation = NULL;
	switch (output->getDataType()) {
		case COM_DT_VALUE:
		{
			SetValueOperation *valueoperation = new SetValueOperation();
			valueoperation->setValue(0.0f);
			operation = valueoperation;
			break;
		}
		case COM_DT_VECTOR:
		{
			SetVectorOperation *vectoroperation = new SetVectorOperation();
			vectoroperation->setX(0.0f);
			vectoroperation->setY(0.0f);
			vectoroperation->setW(0.0f);
			operation = vectoroperation;
			break;
		}
		case COM_DT_COLOR:
		{
			SetColorOperation *coloroperation = new SetColorOperation();
			coloroperation->setChannel1(0.0f);
			coloroperation->setChannel2(0.0f);
			coloroperation->setChannel3(0.0f);
			coloroperation->setChannel4(0.0f);
			operation = coloroperation;
			break;
		}
	}

	if (operation) {
		output->relinkConnections(operation->getOutputSocket(), false);
		graph->addOperation(operation);
	}

	output->clearConnections();
}

template<class SocketType> void MuteNode::fillSocketMap(vector<SocketType *> &sockets, SocketMap &socketMap)
{
	for (typename vector<SocketType *>::iterator it = sockets.begin(); it != sockets.end(); it++) {
		Socket *socket = (Socket *) *it;

		socketMap.insert(std::pair<bNodeSocket *, Socket *>(socket->getbNodeSocket(), socket));
	}
}

void MuteNode::convertToOperations(ExecutionSystem *graph, CompositorContext *context)
{
	bNode *editorNode = this->getbNode();
	vector<OutputSocket *> &outputsockets = this->getOutputSockets();

	/* mute node is also used for unknown nodes and couple of nodes in fast mode
	 * can't use generic routines in that case
	 */
	if (editorNode->flag & NODE_MUTED) {
		vector<InputSocket *> &inputsockets = this->getInputSockets();
		vector<OutputSocket *> relinkedsockets;
		SocketMap socketMap;
		bNodeLink *link;

		this->fillSocketMap<OutputSocket>(outputsockets, socketMap);
		this->fillSocketMap<InputSocket>(inputsockets, socketMap);

		for (link = (bNodeLink *) editorNode->internal_links.first; link; link = link->next) {
			if (link->fromnode == editorNode) {
				InputSocket *fromSocket = (InputSocket *) socketMap.find(link->fromsock)->second;
				OutputSocket *toSocket = (OutputSocket *) socketMap.find(link->tosock)->second;

				if (toSocket->isConnected()) {
					if (fromSocket->isConnected()) {
						toSocket->relinkConnections(fromSocket->getConnection()->getFromSocket(), false);
					}
					else {
						createDefaultOutput(graph, toSocket);
					}

					relinkedsockets.push_back(toSocket);
				}
			}
		}

		/* in some cases node could be marked as muted, but it wouldn't have internal connections
		 * this happens in such cases as muted render layer node
		 *
		 * to deal with such cases create default operation for not-relinked output sockets
		 */

		for (unsigned int index = 0; index < outputsockets.size(); index++) {
			OutputSocket *output = outputsockets[index];

			if (output->isConnected()) {
				bool relinked = false;
				vector<OutputSocket *>::iterator it;

				for (it = relinkedsockets.begin(); it != relinkedsockets.end(); it++) {
					if (*it == output) {
						relinked = true;
						break;
					}
				}

				if (!relinked)
					createDefaultOutput(graph, output);
			}
		}
	}
	else {
		for (unsigned int index = 0; index < outputsockets.size(); index++) {
			OutputSocket *output = outputsockets[index];
			if (output->isConnected()) {
				reconnect(graph, output);
			}
		}
	}
}
