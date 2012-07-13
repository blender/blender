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

#ifndef _COM_BokehBilateralBlurOperation_h
#define _COM_BilateralBlurOperation_h
#include "COM_NodeOperation.h"
#include "COM_QualityStepHelper.h"

class BilateralBlurOperation : public NodeOperation, public QualityStepHelper {
private:
	SocketReader *m_inputColorProgram;
	SocketReader *m_inputDeterminatorProgram;
	NodeBilateralBlurData *m_data;
	float m_space;

public:
	BilateralBlurOperation();

	/**
	 * the inner loop of this program
	 */
	void executePixel(float *color, int x, int y, void *data);
	
	/**
	 * Initialize the execution
	 */
	void initExecution();
	
	/**
	 * Deinitialize the execution
	 */
	void deinitExecution();
	
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
	
	void setData(NodeBilateralBlurData *data) { this->m_data = data; }
};
#endif
