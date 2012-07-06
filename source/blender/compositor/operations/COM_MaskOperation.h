/*
 * Copyright 2012, Blender Foundation.
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
 *		Sergey Sharybin
 */


#ifndef _COM_MaskOperation_h
#define _COM_MaskOperation_h

#include "COM_NodeOperation.h"
#include "DNA_scene_types.h"
#include "DNA_mask_types.h"
#include "BLI_listbase.h"
#include "IMB_imbuf_types.h"

/**
 * Class with implementation of mask rasterization
 */
class MaskOperation : public NodeOperation {
protected:
	Mask *m_mask;
	int m_maskWidth;
	int m_maskHeight;
	int m_framenumber;
	bool m_do_smooth;
	bool m_do_feather;
	float *m_rasterizedMask;
	ListBase m_maskLayers;

	/**
	 * Determine the output resolution. The resolution is retrieved from the Renderer
	 */
	void determineResolution(unsigned int resolution[], unsigned int preferredResolution[]);

public:
	MaskOperation();

	void initExecution();
	void deinitExecution();

	void *initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers);

	void setMask(Mask *mask) { this->m_mask = mask; }
	void setMaskWidth(int width) { this->m_maskWidth = width; }
	void setMaskHeight(int height) { this->m_maskHeight = height; }
	void setFramenumber(int framenumber) { this->m_framenumber = framenumber; }
	void setSmooth(bool smooth) { this->m_do_smooth = smooth; }
	void setFeather(bool feather) { this->m_do_feather = feather; }

	void executePixel(float *color, int x, int y, MemoryBuffer *inputBuffers[], void *data);
};

#endif
