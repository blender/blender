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
 *		Thomas Beck (plasmasolutions.de)
 */

#include "COM_TranslateOperation.h"

TranslateOperation::TranslateOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_VALUE);
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_COLOR);
	this->setResolutionInputSocketIndex(0);
	this->m_inputOperation = NULL;
	this->m_inputXOperation = NULL;
	this->m_inputYOperation = NULL;
	this->m_isDeltaSet = false;
}
void TranslateOperation::initExecution()
{
	this->m_inputOperation = this->getInputSocketReader(0);
	this->m_inputXOperation = this->getInputSocketReader(1);
	this->m_inputYOperation = this->getInputSocketReader(2);

	ensureDelta();

	//Calculate the relative offset once per execution, no need to do this per pixel
	this->m_relativeOffsetX = fmodf(this->getDeltaX(), this->getWidth());
	this->m_relativeOffsetY = fmodf(this->getDeltaY(), this->getHeight());

}

void TranslateOperation::deinitExecution()
{
	this->m_inputOperation = NULL;
	this->m_inputXOperation = NULL;
	this->m_inputYOperation = NULL;
}


void TranslateOperation::executePixel(float output[4], float x, float y, PixelSampler sampler)
{
	ensureDelta();

	float originalXPos = x - this->getDeltaX();
	float originalYPos = y - this->getDeltaY();

	switch(m_wrappingType) {
		case 0:
			//Intentionally empty, originalXPos and originalYPos have been set before
			break;
		case 1:
			// wrap only on the x-axis
			originalXPos = this->getWrappedOriginalXPos(x);
			break;
		case 2:
			// wrap only on the y-axis
			originalYPos = this->getWrappedOriginalYPos(y);
			break;
		case 3:
			// wrap on both
			originalXPos = this->getWrappedOriginalXPos(x);
			originalYPos = this->getWrappedOriginalYPos(y);
			break;
	}

	this->m_inputOperation->read(output, originalXPos , originalYPos, sampler);

}

bool TranslateOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInput;

	ensureDelta();

	newInput.xmin = input->xmin - this->getDeltaX();
	newInput.xmax = input->xmax - this->getDeltaX();
	newInput.ymin = input->ymin - this->getDeltaY();
	newInput.ymax = input->ymax - this->getDeltaY();

	if (m_wrappingType == 1 || m_wrappingType == 3){
		// wrap only on the x-axis if tile is wrapping
		newInput.xmin = getWrappedOriginalXPos(input->xmin);
		newInput.xmax = getWrappedOriginalXPos(input->xmax);
		if(newInput.xmin > newInput.xmax){
			newInput.xmin = 0;
			newInput.xmax = this->getWidth();
		}
	}
	if(m_wrappingType == 2 || m_wrappingType == 3) {
		// wrap only on the y-axis if tile is wrapping
		newInput.ymin = getWrappedOriginalYPos(input->ymin);
		newInput.ymax = getWrappedOriginalYPos(input->ymax);
		if (newInput.ymin > newInput.ymax){
			newInput.ymin = 0;
			newInput.ymax = this->getHeight();
		}
	}


	return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}

void TranslateOperation::setWrapping(char wrapping_type)
{
	m_wrappingType = wrapping_type;
}

float TranslateOperation::getWrappedOriginalXPos(float x)
{
	float originalXPos = 0;

	// Positive offset: Append image data from the left
	if ( this->m_relativeOffsetX > 0 ) {
		if ( x < this->m_relativeOffsetX )
			originalXPos = this->getWidth() - this->m_relativeOffsetX + x;
		else
			originalXPos =  x - this->m_relativeOffsetX;
	} else {
		// Negative offset: Append image data from the right
		if (x < (this->getWidth() + this->m_relativeOffsetX))
			originalXPos = x - this->m_relativeOffsetX;
		else
			originalXPos = x - (this->getWidth() + this->m_relativeOffsetX);
	}

	while (originalXPos < 0) originalXPos += this->m_width;
	return fmodf(originalXPos, this->getWidth());
}


float TranslateOperation::getWrappedOriginalYPos(float y)
{
	float originalYPos = 0;

	// Positive offset: Append image data from the bottom
	if (  this->m_relativeOffsetY > 0 ) {
		if ( y < this->m_relativeOffsetY )
			originalYPos = this->getHeight()- this->m_relativeOffsetY + y;
		else
			originalYPos =  y - this->m_relativeOffsetY;
	} else {
		// Negative offset: Append image data from the top
		if (y < (this->getHeight() + this->m_relativeOffsetY))
			originalYPos = y - this->m_relativeOffsetY;
		else
			originalYPos = y - (this->getHeight() + this->m_relativeOffsetY);
	}

	while (originalYPos < 0) originalYPos += this->m_height;
	return fmodf(originalYPos, this->getHeight());
}
