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

#include "COM_MixNode.h"

#include "COM_MixBlendOperation.h"
#include "COM_MixAddOperation.h"
#include "COM_MixMultiplyOperation.h"
#include "COM_MixBurnOperation.h"
#include "COM_MixColorOperation.h"
#include "COM_MixDarkenOperation.h"
#include "COM_MixDifferenceOperation.h"
#include "COM_MixDivideOperation.h"
#include "COM_MixHueOperation.h"
#include "COM_MixLightenOperation.h"
#include "COM_MixLinearLightOperation.h"
#include "COM_MixOverlayOperation.h"
#include "COM_MixSaturationOperation.h"
#include "COM_MixScreenOperation.h"
#include "COM_MixSoftLightOperation.h"
#include "COM_MixSubtractOperation.h"
#include "COM_MixValueOperation.h"
#include "COM_MixDodgeOperation.h"

#include "COM_ExecutionSystem.h"
#include "COM_SetValueOperation.h"
#include "DNA_material_types.h" // the ramp types


MixNode::MixNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void MixNode::convertToOperations(ExecutionSystem *graph, CompositorContext *context)
{
	InputSocket *valueSocket = this->getInputSocket(0);
	InputSocket *color1Socket = this->getInputSocket(1);
	InputSocket *color2Socket = this->getInputSocket(2);
	OutputSocket *outputSocket = this->getOutputSocket(0);
	bNode *editorNode = this->getbNode();
	
	MixBaseOperation *convertProg;
	
	switch (editorNode->custom1) {
		case MA_RAMP_ADD:
			convertProg = new MixAddOperation();
			break;
		case MA_RAMP_MULT:
			convertProg = new MixMultiplyOperation();
			break;
		case MA_RAMP_LIGHT:
			convertProg = new MixLightenOperation();
			break;
		case MA_RAMP_BURN:
			convertProg = new MixBurnOperation();
			break;
		case MA_RAMP_HUE:
			convertProg = new MixHueOperation();
			break;
		case MA_RAMP_COLOR:
			convertProg = new MixColorOperation();
			break;
		case MA_RAMP_SOFT:
			convertProg = new MixSoftLightOperation();
			break;
		case MA_RAMP_SCREEN:
			convertProg = new MixScreenOperation();
			break;
		case MA_RAMP_LINEAR:
			convertProg = new MixLinearLightOperation();
			break;
		case MA_RAMP_DIFF:
			convertProg = new MixDifferenceOperation();
			break;
		case MA_RAMP_SAT:
			convertProg = new MixSaturationOperation();
			break;
		case MA_RAMP_DIV:
			convertProg = new MixDivideOperation();
			break;
		case MA_RAMP_SUB:
			convertProg = new MixSubtractOperation();
			break;
		case MA_RAMP_DARK:
			convertProg = new MixDarkenOperation();
			break;
		case MA_RAMP_OVERLAY:
			convertProg = new MixOverlayOperation();
			break;
		case MA_RAMP_VAL:
			convertProg = new MixValueOperation();
			break;
		case MA_RAMP_DODGE:
			convertProg = new MixDodgeOperation();
			break;

		case MA_RAMP_BLEND:
		default:
			convertProg = new MixBlendOperation();
			break;
	}
	convertProg->setUseValueAlphaMultiply(this->getbNode()->custom2);

	valueSocket->relinkConnections(convertProg->getInputSocket(0), 0, graph);
	color1Socket->relinkConnections(convertProg->getInputSocket(1), 1, graph);
	color2Socket->relinkConnections(convertProg->getInputSocket(2), 2, graph);
	outputSocket->relinkConnections(convertProg->getOutputSocket(0));
	addPreviewOperation(graph, convertProg->getOutputSocket(0));
	
	convertProg->getInputSocket(2)->setResizeMode(color2Socket->getResizeMode());
	
	graph->addOperation(convertProg);
}
