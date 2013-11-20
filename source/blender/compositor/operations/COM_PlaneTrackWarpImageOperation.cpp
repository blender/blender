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

#include "COM_PlaneTrackWarpImageOperation.h"
#include "COM_ReadBufferOperation.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_math_color.h"

extern "C" {
	#include "BLI_jitter.h"

	#include "BKE_movieclip.h"
	#include "BKE_node.h"
	#include "BKE_tracking.h"
}

BLI_INLINE bool isPointInsideQuad(const float x, const float y, const float corners[4][2])
{
	float point[2];

	point[0] = x;
	point[1] = y;

	return isect_point_tri_v2(point, corners[0], corners[1], corners[2]) ||
	       isect_point_tri_v2(point, corners[0], corners[2], corners[3]);
}

BLI_INLINE void warpCoord(float x, float y, float matrix[3][3], float uv[2])
{
	float vec[3] = {x, y, 1.0f};
	mul_m3_v3(matrix, vec);
	vec[0] /= vec[2];
	vec[1] /= vec[2];

	copy_v2_v2(uv, vec);
}

BLI_INLINE void resolveUVAndDxDy(const float x, const float y, float matrix[3][3],
                                 float *u_r, float *v_r, float *dx_r, float *dy_r)
{
	float inputUV[2];
	float uv_a[2], uv_b[2];

	float dx, dy;
	float uv_l, uv_r;
	float uv_u, uv_d;

	warpCoord(x, y, matrix, inputUV);

	/* adaptive sampling, red (U) channel */
	warpCoord(x - 1, y, matrix, uv_a);
	warpCoord(x + 1, y, matrix, uv_b);
	uv_l = fabsf(inputUV[0] - uv_a[0]);
	uv_r = fabsf(inputUV[0] - uv_b[0]);

	dx = 0.5f * (uv_l + uv_r);

	/* adaptive sampling, green (V) channel */
	warpCoord(x, y - 1, matrix, uv_a);
	warpCoord(x, y + 1, matrix, uv_b);
	uv_u = fabsf(inputUV[1] - uv_a[1]);
	uv_d = fabsf(inputUV[1] - uv_b[1]);

	dy = 0.5f * (uv_u + uv_d);

	*dx_r = dx;
	*dy_r = dy;

	*u_r = inputUV[0];
	*v_r = inputUV[1];
}

PlaneTrackWarpImageOperation::PlaneTrackWarpImageOperation() : PlaneTrackCommonOperation()
{
	this->addInputSocket(COM_DT_COLOR, COM_SC_NO_RESIZE);
	this->addOutputSocket(COM_DT_COLOR);
	this->m_pixelReader = NULL;
	this->setComplex(true);

	/* Currently hardcoded to 8 samples. */
	this->m_osa = 8;
}

void PlaneTrackWarpImageOperation::initExecution()
{
	PlaneTrackCommonOperation::initExecution();

	this->m_pixelReader = this->getInputSocketReader(0);

	BLI_jitter_init(this->m_jitter[0], this->m_osa);

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

void PlaneTrackWarpImageOperation::deinitExecution()
{
	this->m_pixelReader = NULL;
}

void PlaneTrackWarpImageOperation::executePixelSampled(float output[4], float x_, float y_, PixelSampler sampler)
{
	float color_accum[4];

	zero_v4(color_accum);
	for (int sample = 0; sample < this->m_osa; sample++) {
		float current_x = x_ + this->m_jitter[sample][0],
		      current_y = y_ + this->m_jitter[sample][1];
		if (isPointInsideQuad(current_x, current_y, this->m_frameSpaceCorners)) {
			float current_color[4];
			float u, v, dx, dy;

			resolveUVAndDxDy(current_x, current_y, m_perspectiveMatrix, &u, &v, &dx, &dy);

			/* derivatives are to be in normalized space.. */
			dx /= this->m_pixelReader->getWidth();
			dy /= this->m_pixelReader->getHeight();

			this->m_pixelReader->readFiltered(current_color, u, v, dx, dy, COM_PS_NEAREST);
			add_v4_v4(color_accum, current_color);
		}
	}

	mul_v4_v4fl(output, color_accum, 1.0f / this->m_osa);
}

bool PlaneTrackWarpImageOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	float UVs[4][2];

	/* TODO(sergey): figure out proper way to do this. */
	warpCoord(input->xmin - 2, input->ymin - 2, this->m_perspectiveMatrix, UVs[0]);
	warpCoord(input->xmax + 2, input->ymin - 2, this->m_perspectiveMatrix, UVs[1]);
	warpCoord(input->xmax + 2, input->ymax + 2, this->m_perspectiveMatrix, UVs[2]);
	warpCoord(input->xmin - 2, input->ymax + 2, this->m_perspectiveMatrix, UVs[3]);

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
