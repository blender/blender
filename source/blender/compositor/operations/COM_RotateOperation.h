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

#ifndef _COM_RotateOperation_h_
#define _COM_RotateOperation_h_

#include "COM_NodeOperation.h"

class RotateOperation: public NodeOperation {
private:
	SocketReader *imageSocket;
	SocketReader *degreeSocket;
	float centerX;
	float centerY;
	float cosine;
	float sine;
	bool doDegree2RadConversion;
	bool isDegreeSet;
public:
	RotateOperation();
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
	void executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]);
	void initExecution();
	void deinitExecution();
	void setDoDegree2RadConversion(bool abool) {this->doDegree2RadConversion = abool;}
	
	void ensureDegree();
};

#endif
