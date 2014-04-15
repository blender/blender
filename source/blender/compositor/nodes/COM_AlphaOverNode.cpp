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

#include "COM_AlphaOverNode.h"

#include "COM_MixOperation.h"
#include "COM_AlphaOverKeyOperation.h"
#include "COM_AlphaOverMixedOperation.h"
#include "COM_AlphaOverPremultiplyOperation.h"

#include "COM_SetValueOperation.h"
#include "DNA_material_types.h" // the ramp types

void AlphaOverNode::convertToOperations(NodeConverter &converter, const CompositorContext &context) const
{
	NodeInput *color1Socket = this->getInputSocket(1);
	NodeInput *color2Socket = this->getInputSocket(2);
	bNode *editorNode = this->getbNode();
	
	MixBaseOperation *convertProg;
	NodeTwoFloats *ntf = (NodeTwoFloats *)editorNode->storage;
	if (ntf->x != 0.0f) {
		AlphaOverMixedOperation *mixOperation  = new AlphaOverMixedOperation();
		mixOperation->setX(ntf->x);
		convertProg = mixOperation;
	
	}
	else if (editorNode->custom1) {
		convertProg = new AlphaOverKeyOperation();
	}
	else {
		convertProg = new AlphaOverPremultiplyOperation();
	}
	
	convertProg->setUseValueAlphaMultiply(false);
	if (color1Socket->isLinked()) {
		convertProg->setResolutionInputSocketIndex(1);
	}
	else if (color2Socket->isLinked()) {
		convertProg->setResolutionInputSocketIndex(2);
	}
	else {
		convertProg->setResolutionInputSocketIndex(0);
	}
	
	converter.addOperation(convertProg);
	converter.mapInputSocket(getInputSocket(0), convertProg->getInputSocket(0));
	converter.mapInputSocket(getInputSocket(1), convertProg->getInputSocket(1));
	converter.mapInputSocket(getInputSocket(2), convertProg->getInputSocket(2));
	converter.mapOutputSocket(getOutputSocket(0), convertProg->getOutputSocket(0));
}
