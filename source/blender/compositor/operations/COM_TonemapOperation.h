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

#ifndef _COM_TonemapOperation_h
#define _COM_TonemapOperation_h
#include "COM_NodeOperation.h"
#include "DNA_node_types.h"

/**
 * @brief temporarily storage during execution of Tonemap
 * @ingroup operation
 */
typedef struct AvgLogLum {
	float al;
	float auto_key;
	float lav;
	float cav[4];
	float igm;
} AvgLogLum;

/**
 * @brief base class of tonemap, implementing the simple tonemap
 * @ingroup operation
 */
class TonemapOperation : public NodeOperation {
protected:
	/**
	 * @brief Cached reference to the reader
	 */
	SocketReader *imageReader;
	
	/**
	 * @brief settings of the Tonemap
	 */
	NodeTonemap *data;
	
	/**
	 * @brief temporarily cache of the execution storage
	 */
	AvgLogLum *cachedInstance;

public:
	TonemapOperation();
	
	/**
	 * the inner loop of this program
	 */
	void executePixel(float *color, int x, int y, MemoryBuffer * inputBuffers[], void *data);
	
	/**
	 * Initialize the execution
	 */
	void initExecution();
	
	void *initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers);
	void deinitializeTileData(rcti *rect, MemoryBuffer **memoryBuffers, void *data);
	
	/**
	 * Deinitialize the execution
	 */
	void deinitExecution();
	
	void setData(NodeTonemap *data) { this->data = data; }
	
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);


};

/**
 * @brief class of tonemap, implementing the photoreceptor tonemap
 * most parts have already been done in TonemapOperation
 * @ingroup operation
 */

class PhotoreceptorTonemapOperation : public TonemapOperation {
public:
	/**
	 * the inner loop of this program
	 */
	void executePixel(float *color, int x, int y, MemoryBuffer * inputBuffers[], void *data);
};

#endif
