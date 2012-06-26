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

#ifndef _COM_MapValueOperation_h
#define _COM_MapValueOperation_h
#include "COM_NodeOperation.h"
#include "DNA_texture_types.h"

/**
 * this program converts an input colour to an output value.
 * it assumes we are in sRGB colour space.
 */
class MapValueOperation : public NodeOperation {
private:
	/**
	 * Cached reference to the inputProgram
	 */
	SocketReader *m_inputOperation;
	TexMapping *m_settings;
public:
	/**
	 * Default constructor
	 */
	MapValueOperation();
	
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
	
	/**
	 * @brief set the TexMapping settings
	 */
	void setSettings(TexMapping *settings) { this->m_settings = settings; }
	
};
#endif
