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

#ifndef _COM_ConvertDepthToRadiusOperation_h
#define _COM_ConvertDepthToRadiusOperation_h
#include "COM_NodeOperation.h"
#include "DNA_object_types.h"

/**
 * this program converts an input colour to an output value.
 * it assumes we are in sRGB colour space.
 */
class ConvertDepthToRadiusOperation : public NodeOperation {
private:
	/**
	 * Cached reference to the inputProgram
	 */
	SocketReader *m_inputOperation;
	float m_fStop;
	float m_aspect;
	float m_maxRadius;
	float m_inverseFocalDistance;
	float m_aperture;
	float m_cam_lens;
	float m_dof_sp;
	Object *m_cameraObject;
public:
	/**
	 * Default constructor
	 */
	ConvertDepthToRadiusOperation();
	
	/**
	 * the inner loop of this program
	 */
	void executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer * inputBuffers[]);
	
	/**
	 * Initialize the execution
	 */
	void initExecution();
	
	/**
	 * Deinitialize the execution
	 */
	void deinitExecution();
	
	void setfStop(float fStop) { this->m_fStop = fStop; }
	void setMaxRadius(float maxRadius) { this->m_maxRadius = maxRadius; }
	void setCameraObject(Object *camera) { this->m_cameraObject = camera; }
	float determineFocalDistance();
};
#endif
