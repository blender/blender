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

#ifndef _COM_TranslateOperation_h_
#define _COM_TranslateOperation_h_

#include "COM_NodeOperation.h"

class TranslateOperation : public NodeOperation {
private:
	SocketReader *m_inputOperation;
	SocketReader *m_inputXOperation;
	SocketReader *m_inputYOperation;
	float m_deltaX;
	float m_deltaY;
	bool m_isDeltaSet;
	float m_factorX;
	float m_factorY;
public:
	TranslateOperation();
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);

	void initExecution();
	void deinitExecution();

	float getDeltaX() { return this->m_deltaX * this->m_factorX; }
	float getDeltaY() { return this->m_deltaY * this->m_factorY; }
	
	inline void ensureDelta() {
		if (!this->m_isDeltaSet) {
			float tempDelta[4];
			this->m_inputXOperation->readSampled(tempDelta, 0, 0, COM_PS_NEAREST);
			this->m_deltaX = tempDelta[0];
			this->m_inputYOperation->readSampled(tempDelta, 0, 0, COM_PS_NEAREST);
			this->m_deltaY = tempDelta[0];
			this->m_isDeltaSet = true;
		}
	}

	void setFactorXY(float factorX, float factorY);
};

#endif
