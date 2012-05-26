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

#include "COM_GlareNode.h"
#include "DNA_node_types.h"
#include "COM_FogGlowImageOperation.h"
#include "COM_GlareThresholdOperation.h"
#include "COM_GlareSimpleStarOperation.h"
#include "COM_GlareStreaksOperation.h"
#include "COM_SetValueOperation.h"
#include "COM_MixBlendOperation.h"
#include "COM_FastGaussianBlurOperation.h"

GlareNode::GlareNode(bNode *editorNode): Node(editorNode)
{
}

void GlareNode::convertToOperations(ExecutionSystem *system, CompositorContext * context)\
{
	bNode *node = this->getbNode();
	NodeGlare *glare = (NodeGlare*)node->storage;
	
	switch (glare->type) {
	
	default:
	case 2: // streaks
		{
			GlareThresholdOperation *thresholdOperation = new GlareThresholdOperation();
			GlareStreaksOperation * glareoperation = new GlareStreaksOperation();
			SetValueOperation * mixvalueoperation = new SetValueOperation();
			MixBlendOperation * mixoperation = new MixBlendOperation();
	
			this->getInputSocket(0)->relinkConnections(thresholdOperation->getInputSocket(0), 0, system);
			addLink(system, thresholdOperation->getOutputSocket(), glareoperation->getInputSocket(0));
			addLink(system, mixvalueoperation->getOutputSocket(), mixoperation->getInputSocket(0));
			addLink(system, glareoperation->getOutputSocket(), mixoperation->getInputSocket(2));
			addLink(system, thresholdOperation->getInputSocket(0)->getConnection()->getFromSocket(), mixoperation->getInputSocket(1));
			this->getOutputSocket()->relinkConnections(mixoperation->getOutputSocket());
	
			thresholdOperation->setThreshold(glare->threshold);
			glareoperation->setGlareSettings(glare);
			mixvalueoperation->setValue(0.5f+glare->mix*0.5f);
			mixoperation->setResolutionInputSocketIndex(1);
	
			system->addOperation(glareoperation);
			system->addOperation(thresholdOperation);
			system->addOperation(mixvalueoperation);
			system->addOperation(mixoperation);
		}	
		break;
	case 1: // fog glow
		{
			GlareThresholdOperation *thresholdOperation = new GlareThresholdOperation();
			FastGaussianBlurOperation* bluroperation = new FastGaussianBlurOperation();
			SetValueOperation * valueoperation = new SetValueOperation();
			SetValueOperation * mixvalueoperation = new SetValueOperation();
			MixBlendOperation * mixoperation = new MixBlendOperation();
			mixoperation->setResolutionInputSocketIndex(1);
			this->getInputSocket(0)->relinkConnections(thresholdOperation->getInputSocket(0), 0, system);
			addLink(system, thresholdOperation->getOutputSocket(), bluroperation->getInputSocket(0));
			addLink(system, valueoperation->getOutputSocket(), bluroperation->getInputSocket(1));
			addLink(system, mixvalueoperation->getOutputSocket(), mixoperation->getInputSocket(0));
			addLink(system, bluroperation->getOutputSocket(), mixoperation->getInputSocket(2));
			addLink(system, thresholdOperation->getInputSocket(0)->getConnection()->getFromSocket(), mixoperation->getInputSocket(1));
	
			thresholdOperation->setThreshold(glare->threshold);
			NodeBlurData * data = new NodeBlurData();
			data->relative = 0;
			data->sizex = glare->size;
			data->sizey = glare->size;
			bluroperation->setData(data);
			bluroperation->deleteDataWhenFinished();
			bluroperation->setQuality(context->getQuality());
			valueoperation->setValue(1.0f);
			mixvalueoperation->setValue(0.5f+glare->mix*0.5f);
			this->getOutputSocket()->relinkConnections(mixoperation->getOutputSocket());
	
			system->addOperation(bluroperation);
			system->addOperation(thresholdOperation);
			system->addOperation(mixvalueoperation);
			system->addOperation(valueoperation);
			system->addOperation(mixoperation);
		}
		break;
		
	case 0: // simple star
		{
			GlareThresholdOperation *thresholdOperation = new GlareThresholdOperation();
			GlareSimpleStarOperation * glareoperation = new GlareSimpleStarOperation();
			SetValueOperation * mixvalueoperation = new SetValueOperation();
			MixBlendOperation * mixoperation = new MixBlendOperation();

			this->getInputSocket(0)->relinkConnections(thresholdOperation->getInputSocket(0), 0, system);
			addLink(system, thresholdOperation->getOutputSocket(), glareoperation->getInputSocket(0));
			addLink(system, mixvalueoperation->getOutputSocket(), mixoperation->getInputSocket(0));
			addLink(system, glareoperation->getOutputSocket(), mixoperation->getInputSocket(2));
			addLink(system, thresholdOperation->getInputSocket(0)->getConnection()->getFromSocket(), mixoperation->getInputSocket(1));
			this->getOutputSocket()->relinkConnections(mixoperation->getOutputSocket());

			thresholdOperation->setThreshold(glare->threshold);
			glareoperation->setGlareSettings(glare);
			mixvalueoperation->setValue(0.5f+glare->mix*0.5f);
			mixoperation->setResolutionInputSocketIndex(1);


			system->addOperation(glareoperation);
			system->addOperation(thresholdOperation);
			system->addOperation(mixvalueoperation);
			system->addOperation(mixoperation);
		}
		break;
	}
}
