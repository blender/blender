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
 *		Dalai Felinto
 */

#include "COM_LuminanceMatteNode.h"
#include "BKE_node.h"
#include "COM_LuminanceMatteOperation.h"
#include "COM_ConvertOperation.h"
#include "COM_SetAlphaOperation.h"

LuminanceMatteNode::LuminanceMatteNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void LuminanceMatteNode::convertToOperations(NodeConverter &converter, const CompositorContext &context) const
{
	bNode *editorsnode = getbNode();
	NodeInput *inputSocket = this->getInputSocket(0);
	NodeOutput *outputSocketImage = this->getOutputSocket(0);
	NodeOutput *outputSocketMatte = this->getOutputSocket(1);

	ConvertRGBToYUVOperation *rgbToYUV = new ConvertRGBToYUVOperation();
	LuminanceMatteOperation *operationSet = new LuminanceMatteOperation();
	operationSet->setSettings((NodeChroma *)editorsnode->storage);
	converter.addOperation(rgbToYUV);
	converter.addOperation(operationSet);

	converter.mapInputSocket(inputSocket, rgbToYUV->getInputSocket(0));
	converter.addLink(rgbToYUV->getOutputSocket(), operationSet->getInputSocket(0));
	converter.mapOutputSocket(outputSocketMatte, operationSet->getOutputSocket(0));

	SetAlphaOperation *operation = new SetAlphaOperation();
	converter.addOperation(operation);
	
	converter.mapInputSocket(inputSocket, operation->getInputSocket(0));
	converter.addLink(operationSet->getOutputSocket(), operation->getInputSocket(1));
	converter.mapOutputSocket(outputSocketImage, operation->getOutputSocket());
	
	converter.addPreview(operation->getOutputSocket());
}
