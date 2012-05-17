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

#ifndef _COM_ConvertColorProfileOperation_h
#define _COM_ConvertColorProfileOperation_h
#include "COM_NodeOperation.h"


/**
  * this program converts an input colour to an output value.
  * it assumes we are in sRGB colour space.
  */
class ConvertColorProfileOperation : public NodeOperation {
private:
	/**
	  * Cached reference to the inputProgram
	  */
	SocketReader * inputOperation;
	
	/**
	  * @brief color profile where to convert from
	  */
	int fromProfile;
	
	/**
	  * @brief color profile where to convert to
	  */
	int toProfile;
	
	/**
	  * @brief is color predivided
	  */
	bool predivided;
public:
	/**
	  * Default constructor
	  */
	ConvertColorProfileOperation();
	
	/**
	  * the inner loop of this program
	  */
	void executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]);
	
	/**
	  * Initialize the execution
	  */
	void initExecution();
	
	/**
	  * Deinitialize the execution
	  */
	void deinitExecution();
	
	void setFromColorProfile(int colorProfile) {this->fromProfile = colorProfile;}
	void setToColorProfile(int colorProfile) {this->toProfile = colorProfile;}
	void setPredivided(bool predivided) {this->predivided = predivided;}
};
#endif
