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

#include "COM_ColorCorrectionOperation.h"
#include "BLI_math.h"

ColorCorrectionOperation::ColorCorrectionOperation(): NodeOperation() {
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_COLOR);
	this->inputImage = NULL;
	this->inputMask = NULL;
	this->redChannelEnabled = true;
	this->greenChannelEnabled = true;
	this->blueChannelEnabled = true;
}
void ColorCorrectionOperation::initExecution() {
	this->inputImage = this->getInputSocketReader(0);
	this->inputMask = this->getInputSocketReader(1);
}

void ColorCorrectionOperation::executePixel(float* output, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	float inputImageColor[4];
	float inputMask[4];
	this->inputImage->read(inputImageColor, x, y, sampler, inputBuffers);
	this->inputMask->read(inputMask, x, y, sampler, inputBuffers);
	
	float level = (inputImageColor[0] + inputImageColor[1] + inputImageColor[2])/3.0f;
	float contrast= this->data->master.contrast;
	float saturation = this->data->master.saturation;
	float gamma = this->data->master.gamma;
	float gain = this->data->master.gain;
	float lift = this->data->master.lift;
	float r, g, b;
	
	float value = inputMask[0];
	value = min(1.0f, value);
	const float mvalue= 1.0f - value;
	
	float levelShadows = 0.0;
	float levelMidtones = 0.0;
	float levelHighlights = 0.0;
#define MARGIN 0.10
#define MARGIN_DIV (0.5/MARGIN)
	if ( level < this->data->startmidtones-MARGIN) {
		levelShadows = 1.0f;
	} else if (level < this->data->startmidtones+MARGIN) {
		levelMidtones = ((level-this->data->startmidtones)*MARGIN_DIV)+0.5;
		levelShadows = 1.0- levelMidtones;
	} else if (level < this->data->endmidtones-MARGIN) {
		levelMidtones = 1.0f;
	} else if (level < this->data->endmidtones+MARGIN) {
		levelHighlights = ((level-this->data->endmidtones)*MARGIN_DIV)+0.5;
		levelMidtones = 1.0- levelHighlights;
	} else {
		levelHighlights = 1.0f;
	}
#undef MARGIN
#undef MARGIN_DIV
	contrast *= (levelShadows*this->data->shadows.contrast)+(levelMidtones*this->data->midtones.contrast)+(levelHighlights*this->data->highlights.contrast);
	saturation *= (levelShadows*this->data->shadows.saturation)+(levelMidtones*this->data->midtones.saturation)+(levelHighlights*this->data->highlights.saturation);
	gamma *= (levelShadows*this->data->shadows.gamma)+(levelMidtones*this->data->midtones.gamma)+(levelHighlights*this->data->highlights.gamma);
	gain *= (levelShadows*this->data->shadows.gain)+(levelMidtones*this->data->midtones.gain)+(levelHighlights*this->data->highlights.gain);
	lift += (levelShadows*this->data->shadows.lift)+(levelMidtones*this->data->midtones.lift)+(levelHighlights*this->data->highlights.lift);
	
	r = inputImageColor[0];
	g = inputImageColor[1];
	b = inputImageColor[2];
	
	float invgamma = 1.0f/gamma;
	float luma = 0.2126 * r + 0.7152 * g + 0.0722 * b;
	r = ( luma + saturation * (r - luma));
	g = ( luma + saturation * (g - luma));
	b = ( luma + saturation * (b - luma));
	CLAMP (r, 0.0f, 1.0f);
	CLAMP (g, 0.0f, 1.0f);
	CLAMP (b, 0.0f, 1.0f);
	
	r = 0.5+((r-0.5)*contrast);
	g = 0.5+((g-0.5)*contrast);
	b = 0.5+((b-0.5)*contrast);
	
	r = powf(r*gain+lift, invgamma);
	g = powf(g*gain+lift, invgamma);
	b = powf(b*gain+lift, invgamma);
	
	
	// mix with mask
	r = mvalue*inputImageColor[0] + value * r;
	g = mvalue*inputImageColor[1] + value * g;
	b = mvalue*inputImageColor[2] + value * b;
	
	if (this->redChannelEnabled) {
		output[0] = r;
	} else {
		output[0] = inputImageColor[0];
	}
	if (this->greenChannelEnabled) {
		output[1] = g;
	} else {
		output[1] = inputImageColor[1];
	}
	if (this->blueChannelEnabled) {
		output[2] = b;
	} else {
		output[2] = inputImageColor[2];
	}
	output[3] = inputImageColor[3];
}

void ColorCorrectionOperation::deinitExecution() {
	this->inputImage = NULL;
	this->inputMask = NULL;
}

