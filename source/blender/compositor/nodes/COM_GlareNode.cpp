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
#include "COM_GlareThresholdOperation.h"
#include "COM_GlareSimpleStarOperation.h"
#include "COM_GlareStreaksOperation.h"
#include "COM_SetValueOperation.h"
#include "COM_MixGlareOperation.h"
#include "COM_FastGaussianBlurOperation.h"
#include "COM_GlareGhostOperation.h"
#include "COM_GlareFogGlowOperation.h"

GlareNode::GlareNode(bNode *editorNode): Node(editorNode)
{
}

void GlareNode::convertToOperations(ExecutionSystem *system, CompositorContext * context)\
{
	bNode *node = this->getbNode();
	NodeGlare *glare = (NodeGlare*)node->storage;
	
	GlareBaseOperation * glareoperation = NULL;
	
	switch (glare->type) {
	
	default:
	case 3:
		glareoperation = new GlareGhostOperation();
		break;
	case 2: // streaks
		glareoperation = new GlareStreaksOperation();
		break;
	case 1: // fog glow
		glareoperation = new GlareFogGlowOperation();
		break;
	case 0: // simple star
		glareoperation = new GlareSimpleStarOperation();
		break;
	}
	GlareThresholdOperation *thresholdOperation = new GlareThresholdOperation();
	SetValueOperation * mixvalueoperation = new SetValueOperation();
	MixGlareOperation * mixoperation = new MixGlareOperation();
	mixoperation->getInputSocket(2)->setResizeMode(COM_SC_FIT);

	this->getInputSocket(0)->relinkConnections(thresholdOperation->getInputSocket(0), 0, system);
	addLink(system, thresholdOperation->getOutputSocket(), glareoperation->getInputSocket(0));
	addLink(system, mixvalueoperation->getOutputSocket(), mixoperation->getInputSocket(0));
	addLink(system, glareoperation->getOutputSocket(), mixoperation->getInputSocket(2));
	addLink(system, thresholdOperation->getInputSocket(0)->getConnection()->getFromSocket(), mixoperation->getInputSocket(1));
	this->getOutputSocket()->relinkConnections(mixoperation->getOutputSocket());

	thresholdOperation->setGlareSettings(glare);
	glareoperation->setGlareSettings(glare);
	mixvalueoperation->setValue(0.5f+glare->mix*0.5f);
	mixoperation->setResolutionInputSocketIndex(1);

	system->addOperation(glareoperation);
	system->addOperation(thresholdOperation);
	system->addOperation(mixvalueoperation);
	system->addOperation(mixoperation);
	
}
