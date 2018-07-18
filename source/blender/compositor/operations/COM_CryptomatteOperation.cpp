/*
 * Copyright 2018, Blender Foundation.
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
 * Contributor: Lukas Stockner, Stefan Werner
 */

#include "COM_CryptomatteOperation.h"

CryptomatteOperation::CryptomatteOperation(size_t num_inputs) : NodeOperation()
{
	for(size_t i = 0; i < num_inputs; i++) {
		this->addInputSocket(COM_DT_COLOR);
	}
	inputs.resize(num_inputs);
	this->addOutputSocket(COM_DT_COLOR);
	this->setComplex(true);
}

void CryptomatteOperation::initExecution()
{
	for (size_t i = 0; i < inputs.size(); i++) {
		inputs[i] = this->getInputSocketReader(i);
	}
}

void CryptomatteOperation::addObjectIndex(float objectIndex)
{
	if (objectIndex != 0.0f) {
		m_objectIndex.push_back(objectIndex);
	}
}

void CryptomatteOperation::executePixel(float output[4],
                                   int x,
                                   int y,
                                   void *data)
{
	float input[4];
	output[0] = output[1] = output[2] = output[3] = 0.0f;
	for (size_t i = 0; i < inputs.size(); i++) {
		inputs[i]->read(input, x, y, data);
		if (i == 0) {
			/* Write the frontmost object as false color for picking. */
			output[0] = input[0];
			uint32_t m3hash;
			::memcpy(&m3hash, &input[0], sizeof(uint32_t));
			/* Since the red channel is likely to be out of display range,
			 * setting green and blue gives more meaningful images. */
			output[1] = ((float) ((m3hash << 8)) / (float) UINT32_MAX);
			output[2] = ((float) ((m3hash << 16)) / (float) UINT32_MAX);
		}
		for(size_t i = 0; i < m_objectIndex.size(); i++) {
			if (m_objectIndex[i] == input[0]) {
				output[3] += input[1];
			}
			if (m_objectIndex[i] == input[2]) {
				output[3] += input[3];
			}
		}
	}
}
