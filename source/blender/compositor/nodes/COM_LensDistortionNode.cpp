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

#include "COM_LensDistortionNode.h"
#include "COM_ExecutionSystem.h"
#include "COM_ProjectorLensDistortionOperation.h"
#include "COM_ScreenLensDistortionOperation.h"

LensDistortionNode::LensDistortionNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void LensDistortionNode::convertToOperations(NodeConverter &converter, const CompositorContext &context) const
{
	bNode *editorNode = this->getbNode();
	NodeLensDist *data = (NodeLensDist *)editorNode->storage;
	if (data->proj) {
		ProjectorLensDistortionOperation *operation = new ProjectorLensDistortionOperation();
		converter.addOperation(operation);
		
		converter.mapInputSocket(getInputSocket(0), operation->getInputSocket(0));
		converter.mapInputSocket(getInputSocket(2), operation->getInputSocket(1));
		converter.mapOutputSocket(getOutputSocket(0), operation->getOutputSocket(0));
	}
	else {
		ScreenLensDistortionOperation *operation = new ScreenLensDistortionOperation();
		operation->setFit(data->fit);
		operation->setJitter(data->jit);

		if (!getInputSocket(1)->isLinked())
			operation->setDistortion(getInputSocket(1)->getEditorValueFloat());
		if (!getInputSocket(2)->isLinked())
			operation->setDispersion(getInputSocket(2)->getEditorValueFloat());
		
		converter.addOperation(operation);
		
		converter.mapInputSocket(getInputSocket(0), operation->getInputSocket(0));
		converter.mapInputSocket(getInputSocket(1), operation->getInputSocket(1));
		converter.mapInputSocket(getInputSocket(2), operation->getInputSocket(2));
		converter.mapOutputSocket(getOutputSocket(0), operation->getOutputSocket(0));
	}
}
