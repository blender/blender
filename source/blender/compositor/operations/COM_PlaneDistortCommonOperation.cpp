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
 *		Lukas Toenne
 */

#include "COM_PlaneDistortCommonOperation.h"

#include "MEM_guardedalloc.h"

extern "C" {
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_math_color.h"
#include "BLI_jitter.h"

#include "BKE_movieclip.h"
#include "BKE_node.h"
#include "BKE_tracking.h"
}

/* ******** PlaneDistort WarpImage ******** */

BLI_INLINE void warpCoord(float x, float y, float matrix[3][3], float uv[2], float deriv[2][2])
{
	float vec[3] = {x, y, 1.0f};
	mul_m3_v3(matrix, vec);
	uv[0] = vec[0] / vec[2];
	uv[1] = vec[1] / vec[2];

	deriv[0][0] = (matrix[0][0] - matrix[0][2] * uv[0]) / vec[2];
	deriv[1][0] = (matrix[0][1] - matrix[0][2] * uv[1]) / vec[2];
	deriv[0][1] = (matrix[1][0] - matrix[1][2] * uv[0]) / vec[2];
	deriv[1][1] = (matrix[1][1] - matrix[1][2] * uv[1]) / vec[2];
}

PlaneDistortWarpImageOperation::PlaneDistortWarpImageOperation() :
    NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR, COM_SC_NO_RESIZE);
	this->addOutputSocket(COM_DT_COLOR);
	this->m_pixelReader = NULL;
	this->setComplex(true);
}

void PlaneDistortWarpImageOperation::calculateCorners(const float corners[4][2], bool normalized)
{
	if (normalized) {
		for (int i = 0; i < 4; i++) {
			this->m_frameSpaceCorners[i][0] = corners[i][0] * this->getWidth();
			this->m_frameSpaceCorners[i][1] = corners[i][1] * this->getHeight();
		}
	}
	else {
		for (int i = 0; i < 4; i++) {
			this->m_frameSpaceCorners[i][0] = corners[i][0];
			this->m_frameSpaceCorners[i][1] = corners[i][1];
		}
	}
}

void PlaneDistortWarpImageOperation::calculatePerspectiveMatrix()
{
	const int width = this->m_pixelReader->getWidth();
	const int height = this->m_pixelReader->getHeight();
	float frame_corners[4][2] = {{0.0f, 0.0f},
	                             {(float) width, 0.0f},
	                             {(float) width, (float) height},
	                             {0.0f, (float) height}};
	BKE_tracking_homography_between_two_quads(this->m_frameSpaceCorners,
	                                          frame_corners,
	                                          this->m_perspectiveMatrix);
}

void PlaneDistortWarpImageOperation::initExecution()
{
	this->m_pixelReader = this->getInputSocketReader(0);
}

void PlaneDistortWarpImageOperation::deinitExecution()
{
	this->m_pixelReader = NULL;
}

void PlaneDistortWarpImageOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float xy[2] = {x, y};
	float uv[2];
	float deriv[2][2];

	pixelTransform(xy, uv, deriv);

	m_pixelReader->readFiltered(output, uv[0], uv[1], deriv[0], deriv[1], COM_PS_BILINEAR);
}

void PlaneDistortWarpImageOperation::pixelTransform(const float xy[2], float r_uv[2], float r_deriv[2][2])
{
	warpCoord(xy[0], xy[1], m_perspectiveMatrix, r_uv, r_deriv);
}

bool PlaneDistortWarpImageOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	float UVs[4][2];
	float deriv[2][2];

	/* TODO(sergey): figure out proper way to do this. */
	warpCoord(input->xmin - 2, input->ymin - 2, this->m_perspectiveMatrix, UVs[0], deriv);
	warpCoord(input->xmax + 2, input->ymin - 2, this->m_perspectiveMatrix, UVs[1], deriv);
	warpCoord(input->xmax + 2, input->ymax + 2, this->m_perspectiveMatrix, UVs[2], deriv);
	warpCoord(input->xmin - 2, input->ymax + 2, this->m_perspectiveMatrix, UVs[3], deriv);

	float min[2], max[2];
	INIT_MINMAX2(min, max);
	for (int i = 0; i < 4; i++) {
		minmax_v2v2_v2(min, max, UVs[i]);
	}

	rcti newInput;

	newInput.xmin = min[0] - 1;
	newInput.ymin = min[1] - 1;
	newInput.xmax = max[0] + 1;
	newInput.ymax = max[1] + 1;

	return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}


/* ******** PlaneDistort Mask ******** */

PlaneDistortMaskOperation::PlaneDistortMaskOperation() :
    NodeOperation()
{
	addOutputSocket(COM_DT_VALUE);

	/* Currently hardcoded to 8 samples. */
	m_osa = 8;
}

void PlaneDistortMaskOperation::calculateCorners(const float corners[4][2], bool normalized)
{
	if (normalized) {
		for (int i = 0; i < 4; i++) {
			this->m_frameSpaceCorners[i][0] = corners[i][0] * this->getWidth();
			this->m_frameSpaceCorners[i][1] = corners[i][1] * this->getHeight();
		}
	}
	else {
		for (int i = 0; i < 4; i++) {
			this->m_frameSpaceCorners[i][0] = corners[i][0];
			this->m_frameSpaceCorners[i][1] = corners[i][1];
		}
	}
}

void PlaneDistortMaskOperation::initExecution()
{
	BLI_jitter_init(m_jitter, m_osa);
}

void PlaneDistortMaskOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float point[2];

	int inside_counter = 0;
	for (int sample = 0; sample < this->m_osa; sample++) {
		point[0] = x + this->m_jitter[sample][0];
		point[1] = y + this->m_jitter[sample][1];

		if (isect_point_tri_v2(point, this->m_frameSpaceCorners[0], this->m_frameSpaceCorners[1], this->m_frameSpaceCorners[2]) ||
		    isect_point_tri_v2(point, this->m_frameSpaceCorners[0], this->m_frameSpaceCorners[2], this->m_frameSpaceCorners[3]))
		{
			inside_counter++;
		}
	}

	output[0] = (float) inside_counter / this->m_osa;
}
