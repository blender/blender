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

#include "COM_EllipseMaskOperation.h"
#include "BLI_math.h"
#include "DNA_node_types.h"

EllipseMaskOperation::EllipseMaskOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_VALUE);
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_VALUE);
	this->m_inputMask = NULL;
	this->m_inputValue = NULL;
	this->m_cosine = 0.0f;
	this->m_sine = 0.0f;
}
void EllipseMaskOperation::initExecution()
{
	this->m_inputMask = this->getInputSocketReader(0);
	this->m_inputValue = this->getInputSocketReader(1);
	const double rad = DEG2RAD((double)this->m_data->rotation);
	this->m_cosine = cos(rad);
	this->m_sine = sin(rad);
	this->m_aspectRatio = ((float)this->getWidth()) / this->getHeight();
}

void EllipseMaskOperation::executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[])
{
	float inputMask[4];
	float inputValue[4];
	
	float rx = x / this->getWidth();
	float ry = y / this->getHeight();
	
	const float dy = (ry - this->m_data->y) / this->m_aspectRatio;
	const float dx = rx - this->m_data->x;
	rx = this->m_data->x + (this->m_cosine * dx + this->m_sine * dy);
	ry = this->m_data->y + (-this->m_sine * dx + this->m_cosine * dy);
	
	this->m_inputMask->read(inputMask, x, y, sampler, inputBuffers);
	this->m_inputValue->read(inputValue, x, y, sampler, inputBuffers);
	
	const float halfHeight = (this->m_data->height) / 2.0f;
	const float halfWidth = this->m_data->width / 2.0f;
	float sx = rx - this->m_data->x;
	sx *= sx;
	const float tx = halfWidth * halfWidth;
	float sy = ry - this->m_data->y;
	sy *= sy;
	const float ty = halfHeight * halfHeight;
	
	bool inside = ((sx / tx) + (sy / ty)) < 1.0f;
	
	switch (this->m_maskType) {
		case CMP_NODE_MASKTYPE_ADD:
			if (inside) {
				color[0] = max(inputMask[0], inputValue[0]);
			}
			else {
				color[0] = inputMask[0];
			}
			break;
		case CMP_NODE_MASKTYPE_SUBTRACT:
			if (inside) {
				color[0] = inputMask[0] - inputValue[0];
				CLAMP(color[0], 0, 1);
			}
			else {
				color[0] = inputMask[0];
			}
			break;
		case CMP_NODE_MASKTYPE_MULTIPLY:
			if (inside) {
				color[0] = inputMask[0] * inputValue[0];
			}
			else {
				color[0] = 0;
			}
			break;
		case CMP_NODE_MASKTYPE_NOT:
			if (inside) {
				if (inputMask[0] > 0.0f) {
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

void EllipseMaskOperation::deinitExecution()
{
	this->m_inputMask = NULL;
	this->m_inputValue = NULL;
}

