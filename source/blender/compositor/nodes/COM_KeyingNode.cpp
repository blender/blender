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

#include "COM_MathBaseOperation.h"

#include "COM_ConvertOperation.h"
#include "COM_SetValueOperation.h"

#include "COM_DilateErodeOperation.h"

#include "COM_SetAlphaOperation.h"

#include "COM_GaussianAlphaXBlurOperation.h"
#include "COM_GaussianAlphaYBlurOperation.h"

KeyingNode::KeyingNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

NodeOperationOutput *KeyingNode::setupPreBlur(NodeConverter &converter, NodeInput *inputImage, int size) const
{
	ConvertRGBToYCCOperation *convertRGBToYCCOperation = new ConvertRGBToYCCOperation();
	convertRGBToYCCOperation->setMode(0);  /* ITU 601 */
	converter.addOperation(convertRGBToYCCOperation);
	
	converter.mapInputSocket(inputImage, convertRGBToYCCOperation->getInputSocket(0));
	
	CombineChannelsOperation *combineOperation = new CombineChannelsOperation();
	converter.addOperation(combineOperation);

	for (int channel = 0; channel < 4; channel++) {
		SeparateChannelOperation *separateOperation = new SeparateChannelOperation();
		separateOperation->setChannel(channel);
		converter.addOperation(separateOperation);
		
		converter.addLink(convertRGBToYCCOperation->getOutputSocket(0), separateOperation->getInputSocket(0));
		
		if (channel == 0 || channel == 3) {
			converter.addLink(separateOperation->getOutputSocket(0), combineOperation->getInputSocket(channel));
		}
		else {
			KeyingBlurOperation *blurXOperation = new KeyingBlurOperation();
			blurXOperation->setSize(size);
			blurXOperation->setAxis(KeyingBlurOperation::BLUR_AXIS_X);
			converter.addOperation(blurXOperation);
			
			KeyingBlurOperation *blurYOperation = new KeyingBlurOperation();
			blurYOperation->setSize(size);
			blurYOperation->setAxis(KeyingBlurOperation::BLUR_AXIS_Y);
			converter.addOperation(blurYOperation);
			
			converter.addLink(separateOperation->getOutputSocket(), blurXOperation->getInputSocket(0));
			converter.addLink(blurXOperation->getOutputSocket(), blurYOperation->getInputSocket(0));
			converter.addLink(blurYOperation->getOutputSocket(0), combineOperation->getInputSocket(channel));
		}
	}
	
	ConvertYCCToRGBOperation *convertYCCToRGBOperation = new ConvertYCCToRGBOperation();
	convertYCCToRGBOperation->setMode(0);  /* ITU 601 */
	converter.addOperation(convertYCCToRGBOperation);
	
	converter.addLink(combineOperation->getOutputSocket(0), convertYCCToRGBOperation->getInputSocket(0));
	
	return convertYCCToRGBOperation->getOutputSocket(0);
}

NodeOperationOutput *KeyingNode::setupPostBlur(NodeConverter &converter, NodeOperationOutput *postBlurInput, int size) const
{
	KeyingBlurOperation *blurXOperation = new KeyingBlurOperation();
	blurXOperation->setSize(size);
	blurXOperation->setAxis(KeyingBlurOperation::BLUR_AXIS_X);
	converter.addOperation(blurXOperation);
	
	KeyingBlurOperation *blurYOperation = new KeyingBlurOperation();
	blurYOperation->setSize(size);
	blurYOperation->setAxis(KeyingBlurOperation::BLUR_AXIS_Y);
	converter.addOperation(blurYOperation);
	
	converter.addLink(postBlurInput, blurXOperation->getInputSocket(0));
	converter.addLink(blurXOperation->getOutputSocket(), blurYOperation->getInputSocket(0));
	
	return blurYOperation->getOutputSocket();
}

NodeOperationOutput *KeyingNode::setupDilateErode(NodeConverter &converter, NodeOperationOutput *dilateErodeInput, int distance) const
{
	DilateDistanceOperation *dilateErodeOperation;
	if (distance > 0) {
		dilateErodeOperation = new DilateDistanceOperation();
		dilateErodeOperation->setDistance(distance);
	}
	else {
		dilateErodeOperation = new ErodeDistanceOperation();
		dilateErodeOperation->setDistance(-distance);
	}
	converter.addOperation(dilateErodeOperation);
	
	converter.addLink(dilateErodeInput, dilateErodeOperation->getInputSocket(0));
	
	return dilateErodeOperation->getOutputSocket(0);
}

NodeOperationOutput *KeyingNode::setupFeather(NodeConverter &converter, const CompositorContext &context,
                                   NodeOperationOutput *featherInput, int falloff, int distance) const
{
	/* this uses a modified gaussian blur function otherwise its far too slow */
	CompositorQuality quality = context.getQuality();

	/* initialize node data */
	NodeBlurData data;
	memset(&data, 0, sizeof(NodeBlurData));
	data.filtertype = R_FILTER_GAUSS;
	if (distance > 0) {
		data.sizex = data.sizey = distance;
	}
	else {
		data.sizex = data.sizey = -distance;
	}

	GaussianAlphaXBlurOperation *operationx = new GaussianAlphaXBlurOperation();
	operationx->setData(&data);
	operationx->setQuality(quality);
	operationx->setSize(1.0f);
	operationx->setSubtract(distance < 0);
	operationx->setFalloff(falloff);
	converter.addOperation(operationx);
	
	GaussianAlphaYBlurOperation *operationy = new GaussianAlphaYBlurOperation();
	operationy->setData(&data);
	operationy->setQuality(quality);
	operationy->setSize(1.0f);
	operationy->setSubtract(distance < 0);
	operationy->setFalloff(falloff);
	converter.addOperation(operationy);

	converter.addLink(featherInput, operationx->getInputSocket(0));
	converter.addLink(operationx->getOutputSocket(), operationy->getInputSocket(0));

	return operationy->getOutputSocket();
}

NodeOperationOutput *KeyingNode::setupDespill(NodeConverter &converter, NodeOperationOutput *despillInput, NodeInput *inputScreen,
                                   float factor, float colorBalance) const
{
	KeyingDespillOperation *despillOperation = new KeyingDespillOperation();
	despillOperation->setDespillFactor(factor);
	despillOperation->setColorBalance(colorBalance);
	converter.addOperation(despillOperation);
	
	converter.addLink(despillInput, despillOperation->getInputSocket(0));
	converter.mapInputSocket(inputScreen, despillOperation->getInputSocket(1));
	
	return despillOperation->getOutputSocket(0);
}

NodeOperationOutput *KeyingNode::setupClip(NodeConverter &converter, NodeOperationOutput *clipInput, int kernelRadius, float kernelTolerance,
                                float clipBlack, float clipWhite, bool edgeMatte) const
{
	KeyingClipOperation *clipOperation = new KeyingClipOperation();
	clipOperation->setKernelRadius(kernelRadius);
	clipOperation->setKernelTolerance(kernelTolerance);
	clipOperation->setClipBlack(clipBlack);
	clipOperation->setClipWhite(clipWhite);
	clipOperation->setIsEdgeMatte(edgeMatte);
	converter.addOperation(clipOperation);
	
	converter.addLink(clipInput, clipOperation->getInputSocket(0));
	
	return clipOperation->getOutputSocket(0);
}

void KeyingNode::convertToOperations(NodeConverter &converter, const CompositorContext &context) const
{
	bNode *editorNode = this->getbNode();
	NodeKeyingData *keying_data = (NodeKeyingData *) editorNode->storage;
	
	NodeInput *inputImage = this->getInputSocket(0);
	NodeInput *inputScreen = this->getInputSocket(1);
	NodeInput *inputGarbageMatte = this->getInputSocket(2);
	NodeInput *inputCoreMatte = this->getInputSocket(3);
	NodeOutput *outputImage = this->getOutputSocket(0);
	NodeOutput *outputMatte = this->getOutputSocket(1);
	NodeOutput *outputEdges = this->getOutputSocket(2);
	NodeOperationOutput *postprocessedMatte = NULL, *postprocessedImage = NULL, *edgesMatte = NULL;
	
	/* keying operation */
	KeyingOperation *keyingOperation = new KeyingOperation();
	keyingOperation->setScreenBalance(keying_data->screen_balance);
	converter.addOperation(keyingOperation);
	
	converter.mapInputSocket(inputScreen, keyingOperation->getInputSocket(1));
	
	if (keying_data->blur_pre) {
		/* chroma preblur operation for input of keying operation  */
		NodeOperationOutput *preBluredImage = setupPreBlur(converter, inputImage, keying_data->blur_pre);
		converter.addLink(preBluredImage, keyingOperation->getInputSocket(0));
	}
	else {
		converter.mapInputSocket(inputImage, keyingOperation->getInputSocket(0));
	}
	
	postprocessedMatte = keyingOperation->getOutputSocket();
	
	/* black / white clipping */
	if (keying_data->clip_black > 0.0f || keying_data->clip_white < 1.0f) {
		postprocessedMatte = setupClip(converter, postprocessedMatte,
		                               keying_data->edge_kernel_radius, keying_data->edge_kernel_tolerance,
		                               keying_data->clip_black, keying_data->clip_white, false);
	}
	
	/* output edge matte */
	edgesMatte = setupClip(converter, postprocessedMatte,
	                       keying_data->edge_kernel_radius, keying_data->edge_kernel_tolerance,
	                       keying_data->clip_black, keying_data->clip_white, true);
	
	/* apply garbage matte */
	if (inputGarbageMatte->isLinked()) {
		SetValueOperation *valueOperation = new SetValueOperation();
		valueOperation->setValue(1.0f);
		converter.addOperation(valueOperation);
		
		MathSubtractOperation *subtractOperation = new MathSubtractOperation();
		converter.addOperation(subtractOperation);
		
		MathMinimumOperation *minOperation = new MathMinimumOperation();
		converter.addOperation(minOperation);
		
		converter.addLink(valueOperation->getOutputSocket(), subtractOperation->getInputSocket(0));
		converter.mapInputSocket(inputGarbageMatte, subtractOperation->getInputSocket(1));
		
		converter.addLink(subtractOperation->getOutputSocket(), minOperation->getInputSocket(0));
		converter.addLink(postprocessedMatte, minOperation->getInputSocket(1));
		
		postprocessedMatte = minOperation->getOutputSocket();
	}
	
	/* apply core matte */
	if (inputCoreMatte->isLinked()) {
		MathMaximumOperation *maxOperation = new MathMaximumOperation();
		converter.addOperation(maxOperation);
		
		converter.mapInputSocket(inputCoreMatte, maxOperation->getInputSocket(0));
		converter.addLink(postprocessedMatte, maxOperation->getInputSocket(1));
		
		postprocessedMatte = maxOperation->getOutputSocket();
	}
	
	/* apply blur on matte if needed */
	if (keying_data->blur_post)
		postprocessedMatte = setupPostBlur(converter, postprocessedMatte, keying_data->blur_post);

	/* matte dilate/erode */
	if (keying_data->dilate_distance != 0) {
		postprocessedMatte = setupDilateErode(converter, postprocessedMatte, keying_data->dilate_distance);
	}

	/* matte feather */
	if (keying_data->feather_distance != 0) {
		postprocessedMatte = setupFeather(converter, context, postprocessedMatte, keying_data->feather_falloff,
		                                  keying_data->feather_distance);
	}

	/* set alpha channel to output image */
	SetAlphaOperation *alphaOperation = new SetAlphaOperation();
	converter.addOperation(alphaOperation);
	
	converter.mapInputSocket(inputImage, alphaOperation->getInputSocket(0));
	converter.addLink(postprocessedMatte, alphaOperation->getInputSocket(1));

	postprocessedImage = alphaOperation->getOutputSocket();

	/* despill output image */
	if (keying_data->despill_factor > 0.0f) {
		postprocessedImage = setupDespill(converter, postprocessedImage,
		                                  inputScreen,
		                                  keying_data->despill_factor,
		                                  keying_data->despill_balance);
	}

	/* connect result to output sockets */
	converter.mapOutputSocket(outputImage, postprocessedImage);
	converter.mapOutputSocket(outputMatte, postprocessedMatte);

	if (edgesMatte)
		converter.mapOutputSocket(outputEdges, edgesMatte);
}
