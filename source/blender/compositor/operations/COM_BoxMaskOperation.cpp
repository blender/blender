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

#include "COM_BoxMaskOperation.h"
#include "BLI_math.h"
#include "DNA_node_types.h"

BoxMaskOperation::BoxMaskOperation(): NodeOperation() {
	this->addInputSocket(COM_DT_VALUE);
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_VALUE);
	this->inputMask = NULL;
	this->inputValue = NULL;
	this->cosine = 0.0f;
	this->sine = 0.0f;
}
void BoxMaskOperation::initExecution() {
	this->inputMask = this->getInputSocketReader(0);
	this->inputValue = this->getInputSocketReader(1);
	const double rad = DEG2RAD(this->data->rotation);
	this->cosine = cos(rad);
	this->sine = sin(rad);
	this->aspectRatio = ((float)this->getWidth())/this->getHeight();
}

void BoxMaskOperation::executePixel(float* color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	float inputMask[4];
	float inputValue[4];
	
	float rx = x/this->getWidth();
	float ry = y/this->getHeight();
	
	const float dy = (ry - this->data->y)/this->aspectRatio;
	const float dx = rx - this->data->x;
	rx = this->data->x+(this->cosine*dx + this->sine*dy);
	ry = this->data->y+(-this->sine*dx + this->cosine*dy);
	
	this->inputMask->read(inputMask, x, y, sampler, inputBuffers);
	this->inputValue->read(inputValue, x, y, sampler, inputBuffers);
	
	float halfHeight = (this->data->height)/2.0f;
	float halfWidth = this->data->width/2.0f;
	bool inside = rx > this->data->x-halfWidth
			&& rx < this->data->x+halfWidth
			&& ry > this->data->y-halfHeight
			&& ry < this->data->y+halfHeight;
	
	switch (this->maskType) {
		case CMP_NODE_MASKTYPE_ADD:
			if (inside) {
				color[0] = max(inputMask[0],inputValue[0]);
			}
			else {
				color[0] = inputMask[0];
			}
			break;
		case CMP_NODE_MASKTYPE_SUBTRACT:
			if (inside) {
				color[0] = inputMask[0]-inputValue[0];
				CLAMP(color[0], 0, 1);
			}
			else {
				color[0] = inputMask[0];
			}
			break;
		case CMP_NODE_MASKTYPE_MULTIPLY:
			if (inside) {
				color[0] = inputMask[0]*inputValue[0];
			}
			else {
				color[0] = 0;
			}
			break;
		case CMP_NODE_MASKTYPE_NOT:
		if (inside) {
			if (inputMask[0]>0.0f) {
				color[0] = 0;
			}
			else {
				color[0] = inputValue[0];
			}
		}
		else {
			color[0] = inputMask[0];
		}
			break;
	}


}

void BoxMaskOperation::deinitExecution() {
	this->inputMask = NULL;
	this->inputValue = NULL;
}

