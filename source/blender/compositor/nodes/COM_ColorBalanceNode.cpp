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
#include "COM_MixOperation.h"

ColorBalanceNode::ColorBalanceNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void ColorBalanceNode::convertToOperations(NodeConverter &converter, const CompositorContext &context) const
{
	bNode *node = this->getbNode();
	NodeColorBalance *n = (NodeColorBalance *)node->storage;
	
	NodeInput *inputSocket = this->getInputSocket(0);
	NodeInput *inputImageSocket = this->getInputSocket(1);
	NodeOutput *outputSocket = this->getOutputSocket(0);
	
	NodeOperation *operation;
	if (node->custom1 == 0) {
		ColorBalanceLGGOperation *operationLGG = new ColorBalanceLGGOperation();

		float lift_lgg[3], gamma_inv[3];
		for (int c = 0; c < 3; c++) {
			lift_lgg[c] = 2.0f - n->lift[c];
			gamma_inv[c] = (n->gamma[c] != 0.0f) ? 1.0f / n->gamma[c] : 1000000.0f;
		}

		operationLGG->setGain(n->gain);
		operationLGG->setLift(lift_lgg);
		operationLGG->setGammaInv(gamma_inv);
		operation = operationLGG;
	}
	else {
		ColorBalanceASCCDLOperation *operationCDL = new ColorBalanceASCCDLOperation();
		operationCDL->setOffset(n->offset);
		operationCDL->setPower(n->power);
		operationCDL->setSlope(n->slope);
		operation = operationCDL;
	}
	converter.addOperation(operation);
	
	converter.mapInputSocket(inputSocket, operation->getInputSocket(0));
	converter.mapInputSocket(inputImageSocket, operation->getInputSocket(1));
	converter.mapOutputSocket(outputSocket, operation->getOutputSocket(0));
}
