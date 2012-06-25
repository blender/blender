/*
 * Copyright 2012, Blender Foundation.
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
 *		Sergey Sharybin
 */

#include "COM_KeyingNode.h"

#include "COM_ExecutionSystem.h"

#include "COM_KeyingOperation.h"
#include "COM_KeyingBlurOperation.h"
#include "COM_KeyingDespillOperation.h"
#include "COM_KeyingClipOperation.h"

#include "COM_SeparateChannelOperation.h"
#include "COM_CombineChannelsOperation.h"
#include "COM_ConvertRGBToYCCOperation.h"
#include "COM_ConvertYCCToRGBOperation.h"
#include "COM_SetValueOperation.h"

#include "COM_DilateErodeOperation.h"

#include "COM_SetAlphaOperation.h"

KeyingNode::KeyingNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

OutputSocket *KeyingNode::setupPreBlur(ExecutionSystem *graph, InputSocket *inputImage, int size, OutputSocket **originalImage)
{
	ConvertRGBToYCCOperation *convertRGBToYCCOperation = new ConvertRGBToYCCOperation();
	convertRGBToYCCOperation->setMode(0);  /* ITU 601 */

	inputImage->relinkConnections(convertRGBToYCCOperation->getInputSocket(0), 0, graph);
	graph->addOperation(convertRGBToYCCOperation);

	CombineChannelsOperation *combineOperation = new CombineChannelsOperation();
	graph->addOperation(combineOperation);

	for (int channel = 0; channel < 4; channel++) {
		SeparateChannelOperation *separateOperation = new SeparateChannelOperation();
		separateOperation->setChannel(channel);
		addLink(graph, convertRGBToYCCOperation->getOutputSocket(0), separateOperation->getInputSocket(0));
		graph->addOperation(separateOperation);

		if (channel == 0 || channel == 3) {
			addLink(graph, separateOperation->getOutputSocket(0), combineOperation->getInputSocket(channel));
		}
		else {
			KeyingBlurOperation *blurXOperation = new KeyingBlurOperation();
			KeyingBlurOperation *blurYOperation = new KeyingBlurOperation();

			blurXOperation->setSize(size);
			blurXOperation->setAxis(KeyingBlurOperation::BLUR_AXIS_X);

			blurYOperation->setSize(size);
			blurYOperation->setAxis(KeyingBlurOperation::BLUR_AXIS_Y);

			addLink(graph, separateOperation->getOutputSocket(), blurXOperation->getInputSocket(0));
			addLink(graph, blurXOperation->getOutputSocket(), blurYOperation->getInputSocket(0));
			addLink(graph, blurYOperation->getOutputSocket(0), combineOperation->getInputSocket(channel));

			graph->addOperation(blurXOperation);
			graph->addOperation(blurYOperation);
		}
	}

	ConvertYCCToRGBOperation *convertYCCToRGBOperation = new ConvertYCCToRGBOperation();
	convertYCCToRGBOperation->setMode(0);  /* ITU 601 */
	addLink(graph, combineOperation->getOutputSocket(0), convertYCCToRGBOperation->getInputSocket(0));
	graph->addOperation(convertYCCToRGBOperation);

	*originalImage = convertRGBToYCCOperation->getInputSocket(0)->getConnection()->getFromSocket();

	return convertYCCToRGBOperation->getOutputSocket(0);
}

OutputSocket *KeyingNode::setupPostBlur(ExecutionSystem *graph, OutputSocket *postBlurInput, int size)
{
	KeyingBlurOperation *blurXOperation = new KeyingBlurOperation();
	KeyingBlurOperation *blurYOperation = new KeyingBlurOperation();

	blurXOperation->setSize(size);
	blurXOperation->setAxis(KeyingBlurOperation::BLUR_AXIS_X);

	blurYOperation->setSize(size);
	blurYOperation->setAxis(KeyingBlurOperation::BLUR_AXIS_Y);

	addLink(graph, postBlurInput, blurXOperation->getInputSocket(0));
	addLink(graph, blurXOperation->getOutputSocket(), blurYOperation->getInputSocket(0));

	graph->addOperation(blurXOperation);
	graph->addOperation(blurYOperation);

	return blurYOperation->getOutputSocket();
}

OutputSocket *KeyingNode::setupDilateErode(ExecutionSystem *graph, OutputSocket *dilateErodeInput, int distance)
{
	DilateStepOperation *dilateErodeOperation;

	if (distance > 0) {
		dilateErodeOperation = new DilateStepOperation();
		dilateErodeOperation->setIterations(distance);
	}
	else {
		dilateErodeOperation = new ErodeStepOperation();
		dilateErodeOperation->setIterations(-distance);
	}

	addLink(graph, dilateErodeInput, dilateErodeOperation->getInputSocket(0));

	graph->addOperation(dilateErodeOperation);

	return dilateErodeOperation->getOutputSocket(0);
}

OutputSocket *KeyingNode::setupDespill(ExecutionSystem *graph, OutputSocket *despillInput, OutputSocket *inputScreen, float factor)
{
	KeyingDespillOperation *despillOperation = new KeyingDespillOperation();

	despillOperation->setDespillFactor(factor);

	addLink(graph, despillInput, despillOperation->getInputSocket(0));
	addLink(graph, inputScreen, despillOperation->getInputSocket(1));

	graph->addOperation(despillOperation);

	return despillOperation->getOutputSocket(0);
}

OutputSocket *KeyingNode::setupClip(ExecutionSystem *graph, OutputSocket *clipInput, int kernelRadius, float kernelTolerance,
                                    float clipBlack, float clipWhite, bool edgeMatte)
{
	KeyingClipOperation *clipOperation = new KeyingClipOperation();

	clipOperation->setKernelRadius(kernelRadius);
	clipOperation->setKernelTolerance(kernelTolerance);

	clipOperation->setClipBlack(clipBlack);
	clipOperation->setClipWhite(clipWhite);
	clipOperation->setIsEdgeMatte(edgeMatte);

	addLink(graph, clipInput, clipOperation->getInputSocket(0));

	graph->addOperation(clipOperation);

	return clipOperation->getOutputSocket(0);
}

void KeyingNode::convertToOperations(ExecutionSystem *graph, CompositorContext *context)
{
	InputSocket *inputImage = this->getInputSocket(0);
	InputSocket *inputScreen = this->getInputSocket(1);
	InputSocket *inputGarbageMatte = this->getInputSocket(2);
	InputSocket *inputCoreMatte = this->getInputSocket(3);
	OutputSocket *outputImage = this->getOutputSocket(0);
	OutputSocket *outputMatte = this->getOutputSocket(1);
	OutputSocket *outputEdges = this->getOutputSocket(2);
	OutputSocket *postprocessedMatte = NULL, *postprocessedImage = NULL, *originalImage = NULL, *edgesMatte = NULL;

	bNode *editorNode = this->getbNode();
	NodeKeyingData *keying_data = (NodeKeyingData *) editorNode->storage;

	/* keying operation */
	KeyingOperation *keyingOperation = new KeyingOperation();

	keyingOperation->setScreenBalance(keying_data->screen_balance);

	inputScreen->relinkConnections(keyingOperation->getInputSocket(1), 1, graph);
	inputGarbageMatte->relinkConnections(keyingOperation->getInputSocket(2), 2, graph);
	inputCoreMatte->relinkConnections(keyingOperation->getInputSocket(3), 3, graph);

	if (keying_data->blur_pre) {
		/* chroma preblur operation for input of keying operation  */
		OutputSocket *preBluredImage = setupPreBlur(graph, inputImage, keying_data->blur_pre, &originalImage);
		addLink(graph, preBluredImage, keyingOperation->getInputSocket(0));
	}
	else {
		inputImage->relinkConnections(keyingOperation->getInputSocket(0), 0, graph);
		originalImage = keyingOperation->getInputSocket(0)->getConnection()->getFromSocket();
	}

	graph->addOperation(keyingOperation);

	postprocessedMatte = keyingOperation->getOutputSocket();

	if (keying_data->clip_black > 0.0f || keying_data->clip_white < 1.0f) {
		postprocessedMatte = setupClip(graph, postprocessedMatte,
		                               keying_data->edge_kernel_radius, keying_data->edge_kernel_tolerance,
		                               keying_data->clip_black, keying_data->clip_white, false);
	}

	if (outputEdges->isConnected()) {
		edgesMatte = setupClip(graph, postprocessedMatte,
		                       keying_data->edge_kernel_radius, keying_data->edge_kernel_tolerance,
		                       keying_data->clip_black, keying_data->clip_white, true);
	}

	/* apply blur on matte if needed */
	if (keying_data->blur_post)
		postprocessedMatte = setupPostBlur(graph, postprocessedMatte, keying_data->blur_post);

	/* matte dilate/erode */
	if (keying_data->dilate_distance != 0) {
		postprocessedMatte = setupDilateErode(graph, postprocessedMatte, keying_data->dilate_distance);
	}

	/* set alpha channel to output image */
	SetAlphaOperation *alphaOperation = new SetAlphaOperation();
	addLink(graph, originalImage, alphaOperation->getInputSocket(0));
	addLink(graph, postprocessedMatte, alphaOperation->getInputSocket(1));

	postprocessedImage = alphaOperation->getOutputSocket();

	/* despill output image */
	if (keying_data->despill_factor > 0.0f) {
		postprocessedImage = setupDespill(graph, postprocessedImage,
		                                  keyingOperation->getInputSocket(1)->getConnection()->getFromSocket(),
		                                  keying_data->despill_factor);
	}

	/* connect result to output sockets */
	outputImage->relinkConnections(postprocessedImage);
	outputMatte->relinkConnections(postprocessedMatte);

	if (edgesMatte)
		outputEdges->relinkConnections(edgesMatte);

	graph->addOperation(alphaOperation);
}
