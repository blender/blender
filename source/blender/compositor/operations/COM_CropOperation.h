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

#ifndef _COM_CropOperation_h_
#define _COM_CropOperation_h_

#include "COM_NodeOperation.h"

class CropBaseOperation : public NodeOperation {
protected:
	SocketReader *m_inputOperation;
	NodeTwoXYs *m_settings;
	bool m_relative;
	int m_xmax;
	int m_xmin;
	int m_ymax;
	int m_ymin;
	
	void updateArea();
public:
	CropBaseOperation();
	void initExecution();
	void deinitExecution();
	void setCropSettings(NodeTwoXYs *settings) { this->m_settings = settings; }
	void setRelative(bool rel) { this->m_relative = rel; }
};

class CropOperation : public CropBaseOperation {
private:
public:
	CropOperation();
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};

class CropImageOperation : public CropBaseOperation {
private:
public:
	CropImageOperation();
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
	void determineResolution(unsigned int resolution[2], unsigned int preferedResolution[2]);
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);

};
#endif
