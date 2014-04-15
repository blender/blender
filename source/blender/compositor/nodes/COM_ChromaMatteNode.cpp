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

#include "COM_ChromaMatteNode.h"
#include "BKE_node.h"
#include "COM_ChromaMatteOperation.h"
#include "COM_ConvertOperation.h"
#include "COM_SetAlphaOperation.h"

ChromaMatteNode::ChromaMatteNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void ChromaMatteNode::convertToOperations(NodeConverter &converter, const CompositorContext &context) const
{
	bNode *editorsnode = getbNode();
	
	NodeInput *inputSocketImage = this->getInputSocket(0);
	NodeInput *inputSocketKey = this->getInputSocket(1);
	NodeOutput *outputSocketImage = this->getOutputSocket(0);
	NodeOutput *outputSocketMatte = this->getOutputSocket(1);
	
	ConvertRGBToYCCOperation *operationRGBToYCC_Image = new ConvertRGBToYCCOperation();
	ConvertRGBToYCCOperation *operationRGBToYCC_Key = new ConvertRGBToYCCOperation();
	operationRGBToYCC_Image->setMode(0); /* BLI_YCC_ITU_BT601 */
	operationRGBToYCC_Key->setMode(0); /* BLI_YCC_ITU_BT601 */
	converter.addOperation(operationRGBToYCC_Image);
	converter.addOperation(operationRGBToYCC_Key);
	
	ChromaMatteOperation *operation = new ChromaMatteOperation();
	operation->setSettings((NodeChroma *)editorsnode->storage);
	converter.addOperation(operation);
	
	SetAlphaOperation *operationAlpha = new SetAlphaOperation();
	converter.addOperation(operationAlpha);
	
	converter.mapInputSocket(inputSocketImage, operationRGBToYCC_Image->getInputSocket(0));
	converter.mapInputSocket(inputSocketKey, operationRGBToYCC_Key->getInputSocket(0));
	converter.addLink(operationRGBToYCC_Image->getOutputSocket(), operation->getInputSocket(0));
	converter.addLink(operationRGBToYCC_Key->getOutputSocket(), operation->getInputSocket(1));
	converter.mapOutputSocket(outputSocketMatte, operation->getOutputSocket());
	
	converter.mapInputSocket(inputSocketImage, operationAlpha->getInputSocket(0));
	converter.addLink(operation->getOutputSocket(), operationAlpha->getInputSocket(1));
	converter.mapOutputSocket(outputSocketImage, operationAlpha->getOutputSocket());
	
	converter.addPreview(operationAlpha->getOutputSocket());
}
