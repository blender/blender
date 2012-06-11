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
#include "COM_GaussianBokehBlurOperation.h"
#include "COM_SetValueOperation.h"

#include "COM_DilateErodeOperation.h"

#include "COM_SetAlphaOperation.h"

// #define USE_GAUSSIAN_BLUR

KeyingNode::KeyingNode(bNode *editorNode): Node(editorNode)
{
}

OutputSocket *KeyingNode::setupPreBlur(ExecutionSystem *graph, InputSocket *inputImage, int size, OutputSocket **originalImage)
{
	memset(&preBlurData, 0, sizeof(preBlurData));
	preBlurData.sizex = size;
	preBlurData.sizey = size;

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
#ifdef USE_GAUSSIAN_BLUR
			SetValueOperation *setValueOperation = new SetValueOperation();
			setValueOperation->setValue(1.0f);
			graph->addOperation(setValueOperation);

			GaussianBokehBlurOperation *blurOperation = new GaussianBokehBlurOperation();
			blurOperation->setData(&preBlurData);
			blurOperation->setQuality(COM_QUALITY_HIGH);

			addLink(graph, separateOperation->getOutputSocket(0), blurOperation->getInputSocket(0));
			addLink(graph, setValueOperation->getOutputSocket(0), blurOperation->getInputSocket(1));
			addLink(graph, blurOperation->getOutputSocket(0), combineOperation->getInputSocket(channel));
			graph->addOperation(blurOperation);
#else
			KeyingBlurOperation *blurOperation = new KeyingBlurOperation();

			blurOperation->setSize(size);

			addLink(graph, separateOperation->getOutputSocket(0), blurOperation->getInputSocket(0));
			addLink(graph, blurOperation->getOutputSocket(0), combineOperation->getInputSocket(channel));
			graph->addOperation(blurOperation);
#endif
		}
	}

	ConvertYCCToRGBOperation *convertYCCToRGBOperation = new ConvertYCCToRGBOperation();
	convertYCCToRGBOperation->setMode(0);  /* ITU 601 */
	addLink(graph, combineOperation->getOutputSocket(0), convertYCCToRGBOperation->getInputSocket(0));
	graph->addOperation(convertYCCToRGBOperation);

	*originalImage = convertRGBToYCCOperation->getInputSocket(0)->getConnection()->getFromSocket();

	return convertYCCToRGBOperation->getOutputSocket(0);
}

OutputSocket *KeyingNode::setupPostBlur(ExecutionSystem *graph, OutputSocket *postBLurInput, int size)
{
#ifdef USE_GAUSSIAN_BLUR
	memset(&postBlurData, 0, sizeof(postBlurData));

	postBlurData.sizex = size;
	postBlurData.sizey = size;

	SetValueOperation *setValueOperation = new SetValueOperation();

	setValueOperation->setValue(1.0f);
	graph->addOperation(setValueOperation);

	GaussianBokehBlurOperation *blurOperation = new GaussianBokehBlurOperation();
	blurOperation->setData(&postBlurData);
	blurOperation->setQuality(COM_QUALITY_HIGH);

	addLink(graph, postBLurInput, blurOperation->getInputSocket(0));
	addLink(graph, setValueOperation->getOutputSocket(0), blurOperation->getInputSocket(1));

	graph->addOperation(blurOperation);

	return blurOperation->getOutputSocket();
#else
	KeyingBlurOperation *blurOperation = new KeyingBlurOperation();

	blurOperation->setSize(size);

	addLink(graph, postBLurInput, blurOperation->getInputSocket(0));

	graph->addOperation(blurOperation);

	return blurOperation->getOutputSocket();
#endif
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

OutputSocket *KeyingNode::setupDespill(ExecutionSystem *graph, OutputSocket *despillInput, InputSocket *inputScreen, float factor)
{
	KeyingDespillOperation *despillOperation = new KeyingDespillOperation();

	despillOperation->setDespillFactor(factor);

	addLink(graph, despillInput, despillOperation->getInputSocket(0));
	inputScreen->relinkConnections(despillOperation->getInputSocket(1), 1, graph);

	graph->addOperation(despillOperation);

	return despillOperation->getOutputSocket(0);
}

OutputSocket *KeyingNode::setupClip(ExecutionSystem *graph, OutputSocket *clipInput, float clipBlack, float clipWhite)
{
	KeyingClipOperation *clipOperation = new KeyingClipOperation();

	clipOperation->setClipBlack(clipBlack);
	clipOperation->setClipWhite(clipWhite);

	addLink(graph, clipInput, clipOperation->getInputSocket(0));

	graph->addOperation(clipOperation);

	return clipOperation->getOutputSocket(0);
}

void KeyingNode::convertToOperations(ExecutionSystem *graph, CompositorContext *context)
{
	InputSocket *inputImage = this->getInputSocket(0);
	InputSocket *inputScreen = this->getInputSocket(1);
	OutputSocket *outputImage = this->getOutputSocket(0);
	OutputSocket *outputMatte = this->getOutputSocket(1);
	OutputSocket *postprocessedMatte, *postprocessedImage, *originalImage;

	bNode *editorNode = this->getbNode();
	NodeKeyingData *keying_data = (NodeKeyingData *) editorNode->storage;

	/* keying operation */
	KeyingOperation *keyingOperation = new KeyingOperation();

	keyingOperation->setScreenBalance(keying_data->screen_balance);

	inputScreen->relinkConnections(keyingOperation->getInputSocket(1), 1, graph);

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

	if (keying_data->clip_black > 0.0f || keying_data->clip_white< 1.0f)
		postprocessedMatte = setupClip(graph, postprocessedMatte, keying_data->clip_black, keying_data->clip_white);

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
		postprocessedImage = setupDespill(graph, postprocessedImage, inputScreen, keying_data->despill_factor);
	}

	/* connect result to output sockets */
	outputImage->relinkConnections(postprocessedImage);
	outputMatte->relinkConnections(postprocessedMatte);

	graph->addOperation(alphaOperation);
}
