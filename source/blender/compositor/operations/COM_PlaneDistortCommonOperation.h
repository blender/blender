
/*
 * Copyright 2013, Blender Foundation.
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
 *		Sergey Sharybin
 */

#ifndef _COM_PlaneTrackCommonOperation_h
#define _COM_PlaneTrackCommonOperation_h

#include <string.h>

#include "COM_NodeOperation.h"

#include "DNA_movieclip_types.h"
#include "DNA_tracking_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"


class PlaneDistortWarpImageOperation : public NodeOperation {
protected:
	SocketReader *m_pixelReader;
	float m_frameSpaceCorners[4][2];  /* Corners coordinates in pixel space. */
	float m_perspectiveMatrix[3][3];

public:
	PlaneDistortWarpImageOperation();

	void calculateCorners(const float corners[4][2], bool normalized);
	void calculatePerspectiveMatrix();

	void initExecution();
	void deinitExecution();

	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
	void pixelTransform(const float xy[2], float r_uv[2], float r_deriv[2][2]);

	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
};


class PlaneDistortMaskOperation : public NodeOperation {
protected:
	int m_osa;
	float m_jitter[32][2];
	float m_frameSpaceCorners[4][2];  /* Corners coordinates in pixel space. */

public:
	PlaneDistortMaskOperation();
	
	void calculateCorners(const float corners[4][2], bool normalized);
	
	void initExecution();
	
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};

#endif
