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

#ifndef __COM_BLURBASEOPERATION_H__
#define __COM_BLURBASEOPERATION_H__
#include "COM_NodeOperation.h"
#include "COM_QualityStepHelper.h"

#define MAX_GAUSSTAB_RADIUS 30000

#ifdef __SSE2__
#  include <emmintrin.h>
#endif

class BlurBaseOperation : public NodeOperation, public QualityStepHelper {
private:

protected:

	BlurBaseOperation(DataType data_type);
	float *make_gausstab(float rad, int size);
#ifdef __SSE2__
	__m128 *convert_gausstab_sse(const float *gaustab, int size);
#endif
	float *make_dist_fac_inverse(float rad, int size, int falloff);

	void updateSize();

	/**
	 * Cached reference to the inputProgram
	 */
	SocketReader *m_inputProgram;
	SocketReader *m_inputSize;
	NodeBlurData m_data;

	float m_size;
	bool m_sizeavailable;

	bool m_extend_bounds;

public:
	/**
	 * Initialize the execution
	 */
	void initExecution();

	/**
	 * Deinitialize the execution
	 */
	void deinitExecution();

	void setData(const NodeBlurData *data);

	void setSize(float size) { this->m_size = size; this->m_sizeavailable = true; }

	void setExtendBounds(bool extend_bounds) { this->m_extend_bounds = extend_bounds; }

	void determineResolution(unsigned int resolution[2],
	                         unsigned int preferredResolution[2]);
};
#endif
