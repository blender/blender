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
#include "DNA_scene_types.h"
#include "COM_ExecutionSystem.h"
#include "COM_ProjectorLensDistortionOperation.h"
#include "COM_ScreenLensDistortionOperation.h"

LensDistortionNode::LensDistortionNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void LensDistortionNode::convertToOperations(ExecutionSystem *graph, CompositorContext *context)
{
	bNode *editorNode = this->getbNode();
	NodeLensDist *data = (NodeLensDist *)editorNode->storage;
	if (data->proj) {
		ProjectorLensDistortionOperation *operation = new ProjectorLensDistortionOperation();

		this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0), 0, graph);
		operation->setDispertion(this->getInputSocket(2)->getStaticValues()[0]);
		this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket(0));

		operation->setData(data);
		graph->addOperation(operation);

	}
	else {
		ScreenLensDistortionOperation *operation = new ScreenLensDistortionOperation();

		this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0), 0, graph);
		operation->setDistortion(this->getInputSocket(1)->getStaticValues()[0]);
		operation->setDispertion(this->getInputSocket(2)->getStaticValues()[0]);
		this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket(0));

		operation->setData(data);
		graph->addOperation(operation);
	}

}
