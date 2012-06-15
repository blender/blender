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

#ifndef _COM_PreviewOperation_h
#define _COM_PreviewOperation_h
#include "COM_NodeOperation.h"
#include "DNA_image_types.h"
#include "BLI_rect.h"

class PreviewOperation : public NodeOperation {
protected:
	unsigned char *outputBuffer;

	/**
	 * @brief holds reference to the SDNA bNode, where this nodes will render the preview image for
	 */
	bNode *node;
	SocketReader *input;
	float divider;

public:
	PreviewOperation();
	bool isOutputOperation(bool rendering) const { return true; }
	void initExecution();
	void deinitExecution();
	const CompositorPriority getRenderPriority() const;
	
	void executeRegion(rcti *rect, unsigned int tileNumber, MemoryBuffer **memoryBuffers);
	void determineResolution(unsigned int resolution[], unsigned int preferredResolution[]);
	void setbNode(bNode *node) { this->node = node; }
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
	bool isPreviewOperation() { return true; }
	
};
#endif
