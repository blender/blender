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

#include "COM_PlaneTrackMaskOperation.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_math_color.h"

extern "C" {
#  include "BLI_jitter.h"

#  include "BKE_movieclip.h"
#  include "BKE_node.h"
#  include "BKE_tracking.h"
}

PlaneTrackMaskOperation::PlaneTrackMaskOperation() : PlaneTrackCommonOperation()
{
	this->addOutputSocket(COM_DT_VALUE);

	/* Currently hardcoded to 8 samples. */
	this->m_osa = 8;
}

void PlaneTrackMaskOperation::initExecution()
{
	PlaneTrackCommonOperation::initExecution();

	BLI_jitter_init(this->m_jitter[0], this->m_osa);
}

void PlaneTrackMaskOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
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
