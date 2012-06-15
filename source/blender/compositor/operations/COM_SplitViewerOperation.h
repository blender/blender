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

#ifndef _COM_SplitViewerOperation_h
#define _COM_SplitViewerOperation_h
#include "COM_ViewerBaseOperation.h"
#include "DNA_image_types.h"
#include "BLI_rect.h"

class SplitViewerOperation : public ViewerBaseOperation {
private:
	SocketReader *image1Input;
	SocketReader *image2Input;

	float splitPercentage;
	bool xSplit;
public:
	SplitViewerOperation();
	void executeRegion(rcti *rect, unsigned int tileNumber, MemoryBuffer **memoryBuffers);
	void initExecution();
	void deinitExecution();
	void setSplitPercentage(float splitPercentage) { this->splitPercentage = splitPercentage; }
	void setXSplit(bool xsplit) { this->xSplit = xsplit; }
};
#endif
