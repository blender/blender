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

#include "COM_DistanceMatteNode.h"
#include "BKE_node.h"
#include "COM_DistanceRGBMatteOperation.h"
#include "COM_DistanceYCCMatteOperation.h"
#include "COM_SetAlphaOperation.h"
#include "COM_ConvertOperation.h"

DistanceMatteNode::DistanceMatteNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void DistanceMatteNode::convertToOperations(NodeConverter &converter, const CompositorContext &context) const
{
	bNode *editorsnode = getbNode();
	NodeChroma *storage = (NodeChroma *)editorsnode->storage;
	
	NodeInput *inputSocketImage = this->getInputSocket(0);
	NodeInput *inputSocketKey = this->getInputSocket(1);
	NodeOutput *outputSocketImage = this->getOutputSocket(0);
	NodeOutput *outputSocketMatte = this->getOutputSocket(1);
	
	SetAlphaOperation *operationAlpha = new SetAlphaOperation();
	converter.addOperation(operationAlpha);
	
	/* work in RGB color space */
	NodeOperation *operation;
	if (storage->channel == 1) {
		DistanceRGBMatteOperation *matte = new DistanceRGBMatteOperation();
		matte->setSettings(storage);
		converter.addOperation(matte);
		
		converter.mapInputSocket(inputSocketImage, matte->getInputSocket(0));
		converter.mapInputSocket(inputSocketImage, operationAlpha->getInputSocket(0));
		
		converter.mapInputSocket(inputSocketKey, matte->getInputSocket(1));
		
		operation = matte;
	}
	/* work in YCbCr color space */
	else {
		DistanceYCCMatteOperation *matte = new DistanceYCCMatteOperation();
		matte->setSettings(storage);
		converter.addOperation(matte);
		
		ConvertRGBToYCCOperation *operationYCCImage = new ConvertRGBToYCCOperation();
		ConvertRGBToYCCOperation *operationYCCMatte = new ConvertRGBToYCCOperation();
		converter.addOperation(operationYCCImage);
		converter.addOperation(operationYCCMatte);
		
		converter.mapInputSocket(inputSocketImage, operationYCCImage->getInputSocket(0));
		converter.addLink(operationYCCImage->getOutputSocket(), matte->getInputSocket(0));
		converter.addLink(operationYCCImage->getOutputSocket(), operationAlpha->getInputSocket(0));
		
		converter.mapInputSocket(inputSocketKey, operationYCCMatte->getInputSocket(0));
		converter.addLink(operationYCCMatte->getOutputSocket(), matte->getInputSocket(1));
		
		operation = matte;
	}
	
	converter.addLink(operation->getOutputSocket(), operationAlpha->getInputSocket(1));
	
	converter.mapOutputSocket(outputSocketMatte, operation->getOutputSocket());
	converter.mapOutputSocket(outputSocketImage, operationAlpha->getOutputSocket());
	
	converter.addPreview(operationAlpha->getOutputSocket());
}
