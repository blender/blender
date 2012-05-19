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

#include "COM_MovieDistortionOperation.h"

extern "C" {
	#include "BKE_tracking.h"

#include "BLI_linklist.h"
}


vector<DistortionCache*> s_cache;


MovieDistortionOperation::MovieDistortionOperation(bool distortion) : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_COLOR);
	this->setResolutionInputSocketIndex(0);
	this->inputOperation = NULL;
	this->movieClip = NULL;
	this->cache = NULL;
	this->distortion = distortion;
}
void MovieDistortionOperation::initExecution()
{
	this->inputOperation = this->getInputSocketReader(0);
	if (this->movieClip) {
		for (int i = 0 ; i < s_cache.size() ; i ++) {
			DistortionCache *c = (DistortionCache*)s_cache[i];
			if (c->isCacheFor(this->movieClip, this->width, this->height, this->distortion)) {
				this->cache = c;
				return;
			}
		}
		DistortionCache *newC = new DistortionCache(this->movieClip, this->width, this->height, this->distortion);
		s_cache.push_back(newC);
		this->cache = newC;
	}
	else {
		this->cache = NULL;
	}
}

void MovieDistortionOperation::deinitExecution()
{
	this->inputOperation = NULL;
	this->movieClip = NULL;
}


void MovieDistortionOperation::executePixel(float *color,float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[])
{
	
	if (this->cache != NULL) {
		float u, v;
		this->cache->getUV(&this->movieClip->tracking, x, y, &u, &v);
		this->inputOperation->read(color, u, v, sampler, inputBuffers);
	} 
	else {
		this->inputOperation->read(color, x, y, sampler, inputBuffers);
	}
}

bool MovieDistortionOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInput;
	
	newInput.xmax = input->xmax + 100;
	newInput.xmin = input->xmin - 100;
	newInput.ymax = input->ymax + 100;
	newInput.ymin = input->ymin - 100;
	
	return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}
