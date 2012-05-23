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

#include "COM_ColorBalanceNode.h"
#include "COM_ColorBalanceLGGOperation.h"
#include "COM_ColorBalanceASCCDLOperation.h"
#include "COM_ExecutionSystem.h"
#include "BKE_node.h"
#include "COM_MixBlendOperation.h"

ColorBalanceNode::ColorBalanceNode(bNode *editorNode): Node(editorNode)
{
}
void ColorBalanceNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context)
{
	InputSocket *inputSocket = this->getInputSocket(0);
	InputSocket *inputImageSocket = this->getInputSocket(1);
	OutputSocket *outputSocket = this->getOutputSocket(0);
	
	bNode *node = this->getbNode();
	NodeColorBalance *n = (NodeColorBalance *)node->storage;
	NodeOperation*operation;
	if (node->custom1 == 0) {
		ColorBalanceLGGOperation *operationLGG = new ColorBalanceLGGOperation();
		{
				int c;
	
				for (c = 0; c < 3; c++) {
						n->lift_lgg[c] = 2.0f - n->lift[c];
						n->gamma_inv[c] = (n->gamma[c] != 0.0f) ? 1.0f/n->gamma[c] : 1000000.0f;
				}
		}
	
		operationLGG->setGain(n->gain);
		operationLGG->setLift(n->lift_lgg);
		operationLGG->setGammaInv(n->gamma_inv);
		operation = operationLGG;
	}
	else {
		ColorBalanceASCCDLOperation *operationCDL = new ColorBalanceASCCDLOperation();
		operationCDL->setGain(n->gain);
		operationCDL->setLift(n->lift);
		operationCDL->setGamma(n->gamma);
		operation = operationCDL;
	}
	
	inputSocket->relinkConnections(operation->getInputSocket(0), 0, graph);
	inputImageSocket->relinkConnections(operation->getInputSocket(1), 0, graph);
	outputSocket->relinkConnections(operation->getOutputSocket(0));
	graph->addOperation(operation);
}
