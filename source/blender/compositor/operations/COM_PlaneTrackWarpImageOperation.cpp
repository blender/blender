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

BLI_INLINE bool resolveUV(const float x, const float y, const float corners[4][2], float uv[2])
{
	float point[2];
	bool inside;

	inside = isPointInsideQuad(x, y, corners);

	point[0] = x;
	point[1] = y;

	/* Use reverse bilinear to get UV coordinates within original frame */
	resolve_quad_uv(uv, point, corners[0], corners[1], corners[2], corners[3]);

	return inside;
}

BLI_INLINE void resolveUVAndDxDy(const float x, const float y, const float corners[4][2],
                                 float *u_r, float *v_r, float *dx_r, float *dy_r)
{
	float inputUV[2];
	float uv_a[2], uv_b[2];

	float dx, dy;
	float uv_l, uv_r;
	float uv_u, uv_d;

	bool ok1, ok2;

	resolveUV(x, y, corners, inputUV);

	/* adaptive sampling, red (U) channel */
	ok1 = resolveUV(x - 1, y, corners, uv_a);
	ok2 = resolveUV(x + 1, y, corners, uv_b);
	uv_l = ok1 ? fabsf(inputUV[0] - uv_a[0]) : 0.0f;
	uv_r = ok2 ? fabsf(inputUV[0] - uv_b[0]) : 0.0f;

	dx = 0.5f * (uv_l + uv_r);

	/* adaptive sampling, green (V) channel */
	ok1 = resolveUV(x, y - 1, corners, uv_a);
	ok2 = resolveUV(x, y + 1, corners, uv_b);
	uv_u = ok1 ? fabsf(inputUV[1] - uv_a[1]) : 0.f;
	uv_d = ok2 ? fabsf(inputUV[1] - uv_b[1]) : 0.f;

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
}

void PlaneTrackWarpImageOperation::deinitExecution()
{
	this->m_pixelReader = NULL;
}

void PlaneTrackWarpImageOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float color_accum[4];

	zero_v4(color_accum);
	for (int sample = 0; sample < this->m_osa; sample++) {
		float current_x = x + this->m_jitter[sample][0],
		      current_y = y + this->m_jitter[sample][1];
		if (isPointInsideQuad(current_x, current_y, this->m_frameSpaceCorners)) {
			float current_color[4];
			float u, v, dx, dy;

			resolveUVAndDxDy(current_x, current_y, this->m_frameSpaceCorners, &u, &v, &dx, &dy);

			u *= this->m_pixelReader->getWidth();
			v *= this->m_pixelReader->getHeight();

			this->m_pixelReader->readFiltered(current_color, u, v, dx, dy, COM_PS_NEAREST);
			add_v4_v4(color_accum, current_color);
		}
	}

	mul_v4_v4fl(output, color_accum, 1.0f / this->m_osa);
}

bool PlaneTrackWarpImageOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	float frame_space_corners[4][2];

	for (int i = 0; i < 4; i++) {
		frame_space_corners[i][0] = this->m_corners[i][0] * this->getWidth();
		frame_space_corners[i][1] = this->m_corners[i][1] * this->getHeight();
	}

	float UVs[4][2];

	/* TODO(sergey): figure out proper way to do this. */
	resolveUV(input->xmin - 2, input->ymin - 2, frame_space_corners, UVs[0]);
	resolveUV(input->xmax + 2, input->ymin - 2, frame_space_corners, UVs[1]);
	resolveUV(input->xmax + 2, input->ymax + 2, frame_space_corners, UVs[2]);
	resolveUV(input->xmin - 2, input->ymax + 2, frame_space_corners, UVs[3]);

	float min[2], max[2];
	INIT_MINMAX2(min, max);
	for (int i = 0; i < 4; i++) {
		minmax_v2v2_v2(min, max, UVs[i]);
	}

	rcti newInput;

	newInput.xmin = min[0] * readOperation->getWidth() - 1;
	newInput.ymin = min[1] * readOperation->getHeight() - 1;
	newInput.xmax = max[0] * readOperation->getWidth() + 1;
	newInput.ymax = max[1] * readOperation->getHeight() + 1;

	return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}
