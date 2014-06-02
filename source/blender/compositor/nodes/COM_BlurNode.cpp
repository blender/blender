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

#include "COM_BlurNode.h"
#include "DNA_node_types.h"
#include "COM_GaussianXBlurOperation.h"
#include "COM_GaussianYBlurOperation.h"
#include "COM_GaussianAlphaXBlurOperation.h"
#include "COM_GaussianAlphaYBlurOperation.h"
#include "COM_ExecutionSystem.h"
#include "COM_GaussianBokehBlurOperation.h"
#include "COM_FastGaussianBlurOperation.h"
#include "COM_MathBaseOperation.h"
#include "COM_SetValueOperation.h"
#include "COM_GammaCorrectOperation.h"

BlurNode::BlurNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void BlurNode::convertToOperations(NodeConverter &converter, const CompositorContext &context) const
{
	bNode *editorNode = this->getbNode();
	NodeBlurData *data = (NodeBlurData *)editorNode->storage;
	NodeInput *inputSizeSocket = this->getInputSocket(1);
	bool connectedSizeSocket = inputSizeSocket->isLinked();

	const float size = this->getInputSocket(1)->getEditorValueFloat();
	
	CompositorQuality quality = context.getQuality();
	NodeOperation *input_operation = NULL, *output_operation = NULL;

	if (data->filtertype == R_FILTER_FAST_GAUSS) {
		FastGaussianBlurOperation *operationfgb = new FastGaussianBlurOperation();
		operationfgb->setData(data);
		converter.addOperation(operationfgb);
		
		converter.mapInputSocket(getInputSocket(1), operationfgb->getInputSocket(1));
		
		input_operation = operationfgb;
		output_operation = operationfgb;
	}
	else if (editorNode->custom1 & CMP_NODEFLAG_BLUR_VARIABLE_SIZE) {
		MathAddOperation *clamp = new MathAddOperation();
		SetValueOperation *zero = new SetValueOperation();
		zero->setValue(0.0f);
		clamp->setUseClamp(true);
		
		converter.addOperation(clamp);
		converter.addOperation(zero);
		converter.mapInputSocket(getInputSocket(1), clamp->getInputSocket(0));
		converter.addLink(zero->getOutputSocket(), clamp->getInputSocket(1));
		
		GaussianAlphaXBlurOperation *operationx = new GaussianAlphaXBlurOperation();
		operationx->setData(data);
		operationx->setQuality(quality);
		operationx->setSize(1.0f);
		operationx->setFalloff(PROP_SMOOTH);
		operationx->setSubtract(false);
		
		converter.addOperation(operationx);
		converter.addLink(clamp->getOutputSocket(), operationx->getInputSocket(0));
		
		GaussianAlphaYBlurOperation *operationy = new GaussianAlphaYBlurOperation();
		operationy->setData(data);
		operationy->setQuality(quality);
		operationy->setSize(1.0f);
		operationy->setFalloff(PROP_SMOOTH);
		operationy->setSubtract(false);
		
		converter.addOperation(operationy);
		converter.addLink(operationx->getOutputSocket(), operationy->getInputSocket(0));
		
		GaussianBlurReferenceOperation *operation = new GaussianBlurReferenceOperation();
		operation->setData(data);
		operation->setQuality(quality);
		
		converter.addOperation(operation);
		converter.addLink(operationy->getOutputSocket(), operation->getInputSocket(1));
		
		output_operation = operation;
		input_operation = operation;
	}
	else if (!data->bokeh) {
		GaussianXBlurOperation *operationx = new GaussianXBlurOperation();
		operationx->setData(data);
		operationx->setQuality(quality);
		
		converter.addOperation(operationx);
		converter.mapInputSocket(getInputSocket(1), operationx->getInputSocket(1));
		
		GaussianYBlurOperation *operationy = new GaussianYBlurOperation();
		operationy->setData(data);
		operationy->setQuality(quality);

		converter.addOperation(operationy);
		converter.mapInputSocket(getInputSocket(1), operationy->getInputSocket(1));
		converter.addLink(operationx->getOutputSocket(), operationy->getInputSocket(0));

		if (!connectedSizeSocket) {
			operationx->setSize(size);
			operationy->setSize(size);
		}

		input_operation = operationx;
		output_operation = operationy;
	}
	else {
		GaussianBokehBlurOperation *operation = new GaussianBokehBlurOperation();
		operation->setData(data);
		operation->setQuality(quality);
		
		converter.addOperation(operation);
		converter.mapInputSocket(getInputSocket(1), operation->getInputSocket(1));

		if (!connectedSizeSocket) {
			operation->setSize(size);
		}

		input_operation = operation;
		output_operation = operation;
	}

	if (data->gamma) {
		GammaCorrectOperation *correct = new GammaCorrectOperation();
		GammaUncorrectOperation *inverse = new GammaUncorrectOperation();
		converter.addOperation(correct);
		converter.addOperation(inverse);
		
		converter.mapInputSocket(getInputSocket(0), correct->getInputSocket(0));
		converter.addLink(correct->getOutputSocket(), input_operation->getInputSocket(0));
		converter.addLink(output_operation->getOutputSocket(), inverse->getInputSocket(0));
		converter.mapOutputSocket(getOutputSocket(), inverse->getOutputSocket());
		
		converter.addPreview(inverse->getOutputSocket());
	}
	else {
		converter.mapInputSocket(getInputSocket(0), input_operation->getInputSocket(0));
		converter.mapOutputSocket(getOutputSocket(), output_operation->getOutputSocket());
		
		converter.addPreview(output_operation->getOutputSocket());
	}
}
