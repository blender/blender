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

#include "COM_ScreenLensDistortionOperation.h"

extern "C" {
#  include "BLI_math.h"
#  include "BLI_utildefines.h"
#  include "BLI_rand.h"
}

ScreenLensDistortionOperation::ScreenLensDistortionOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_VALUE);
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_COLOR);
	this->setComplex(true);
	this->m_inputProgram = NULL;
	this->m_distortion = 0.0f;
	this->m_dispersion = 0.0f;
	this->m_distortion_const = false;
	this->m_dispersion_const = false;
	this->m_variables_ready = false;
}

void ScreenLensDistortionOperation::setDistortion(float distortion)
{
	m_distortion = distortion;
	m_distortion_const = true;
}

void ScreenLensDistortionOperation::setDispersion(float dispersion)
{
	m_dispersion = dispersion;
	m_dispersion_const = true;
}

void ScreenLensDistortionOperation::initExecution()
{
	this->m_inputProgram = this->getInputSocketReader(0);
	this->initMutex();
	
	this->m_cx = 0.5f * (float)getWidth();
	this->m_cy = 0.5f * (float)getHeight();
	
	/* if both are constant, init variables once */
	if (m_distortion_const && m_dispersion_const) {
		updateVariables(m_distortion, m_dispersion);
		m_variables_ready = true;
	}
}

void *ScreenLensDistortionOperation::initializeTileData(rcti *rect)
{
	void *buffer = this->m_inputProgram->initializeTileData(NULL);

	/* get distortion/dispersion values once, by reading inputs at (0,0)
	 * XXX this assumes invariable values (no image inputs),
	 * we don't have a nice generic system for that yet
	 */
	if (!m_variables_ready) {
		this->lockMutex();
		
		if (!m_distortion_const) {
			float result[4];
			getInputSocketReader(1)->readSampled(result, 0, 0, COM_PS_NEAREST);
			m_distortion = result[0];
		}
		if (!m_dispersion_const) {
			float result[4];
			getInputSocketReader(2)->readSampled(result, 0, 0, COM_PS_NEAREST);
			m_dispersion = result[0];
		}
		
		updateVariables(m_distortion, m_dispersion);
		m_variables_ready = true;
		
		this->unlockMutex();
	}
	
	return buffer;
}

void ScreenLensDistortionOperation::get_uv(const float xy[2], float uv[2]) const
{
	uv[0] = m_sc * ((xy[0] + 0.5f) - m_cx) / m_cx;
	uv[1] = m_sc * ((xy[1] + 0.5f) - m_cy) / m_cy;
}

void ScreenLensDistortionOperation::distort_uv(const float uv[2], float t, float xy[2]) const
{
	float d = 1.0f / (1.0f + sqrtf(t));
	xy[0] = (uv[0] * d + 0.5f) * getWidth() - 0.5f;
	xy[1] = (uv[1] * d + 0.5f) * getHeight() - 0.5f;
}

bool ScreenLensDistortionOperation::get_delta(float r_sq, float k4, const float uv[2], float delta[2]) const
{
	float t = 1.0f - k4 * r_sq;
	if (t >= 0.0f) {
		distort_uv(uv, t, delta);
		return true;
	}
	else
		return false;
	
}

void ScreenLensDistortionOperation::accumulate(MemoryBuffer *buffer,
                                               int a, int b,
                                               float r_sq, const float uv[2],
                                               const float delta[3][2],
                                               float sum[4], int count[3]) const
{
	float color[4];
	
	float dsf = len_v2v2(delta[a], delta[b]) + 1.0f;
	int ds = m_jitter ? (dsf < 4.0f ? 2 : (int)sqrtf(dsf)) : (int)dsf;
	float sd = 1.0f / (float)ds;

	float k4 = m_k4[a];
	float dk4 = m_dk4[a];

	for (float z = 0; z < ds; ++z) {
		float tz = (z + (m_jitter ? BLI_frand() : 0.5f)) * sd;
		float t = 1.0f - (k4 + tz * dk4) * r_sq;
		
		float xy[2];
		distort_uv(uv, t, xy);
		buffer->readBilinear(color, xy[0], xy[1]);
		
		sum[a] += (1.0f - tz) * color[a], sum[b] += tz * color[b];
		++count[a];
		++count[b];
	}
}

void ScreenLensDistortionOperation::executePixel(float output[4], int x, int y, void *data)
{
	MemoryBuffer *buffer = (MemoryBuffer *)data;
	float xy[2] = { (float)x, (float)y };
	float uv[2];
	get_uv(xy, uv);
	float uv_dot = len_squared_v2(uv);

	int count[3] = { 0, 0, 0 };
	float delta[3][2];
	float sum[4] = { 0, 0, 0, 0 };

	bool valid_r = get_delta(uv_dot, m_k4[0], uv, delta[0]);
	bool valid_g = get_delta(uv_dot, m_k4[1], uv, delta[1]);
	bool valid_b = get_delta(uv_dot, m_k4[2], uv, delta[2]);

	if (valid_r && valid_g && valid_b) {
		accumulate(buffer, 0, 1, uv_dot, uv, delta, sum, count);
		accumulate(buffer, 1, 2, uv_dot, uv, delta, sum, count);
		
		if (count[0]) output[0] = 2.0f * sum[0] / (float)count[0];
		if (count[1]) output[1] = 2.0f * sum[1] / (float)count[1];
		if (count[2]) output[2] = 2.0f * sum[2] / (float)count[2];
		
		/* set alpha */
		output[3] = 1.0f;
	}
	else {
		zero_v4(output);
	}
}

void ScreenLensDistortionOperation::deinitExecution()
{
	this->deinitMutex();
	this->m_inputProgram = NULL;
}

void ScreenLensDistortionOperation::determineUV(float result[6], float x, float y) const
{
	const float xy[2] = {x, y};
	float uv[2];
	get_uv(xy, uv);
	float uv_dot = len_squared_v2(uv);
	
	copy_v2_v2(result + 0, xy);
	copy_v2_v2(result + 2, xy);
	copy_v2_v2(result + 4, xy);
	get_delta(uv_dot, m_k4[0], uv, result + 0);
	get_delta(uv_dot, m_k4[1], uv, result + 2);
	get_delta(uv_dot, m_k4[2], uv, result + 4);
}

bool ScreenLensDistortionOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInputValue;
	newInputValue.xmin = 0;
	newInputValue.ymin = 0;
	newInputValue.xmax = 2;
	newInputValue.ymax = 2;
	
	NodeOperation *operation = getInputOperation(1);
	if (operation->determineDependingAreaOfInterest(&newInputValue, readOperation, output) ) {
		return true;
	}

	operation = getInputOperation(2);
	if (operation->determineDependingAreaOfInterest(&newInputValue, readOperation, output) ) {
		return true;
	}
	
	/* XXX the original method of estimating the area-of-interest does not work
	 * it assumes a linear increase/decrease of mapped coordinates, which does not
	 * yield correct results for the area and leaves uninitialized buffer areas.
	 * So now just use the full image area, which may not be as efficient but works at least ...
	 */
#if 1
	rcti imageInput;
	
	operation = getInputOperation(0);
	imageInput.xmax = operation->getWidth();
	imageInput.xmin = 0;
	imageInput.ymax = operation->getHeight();
	imageInput.ymin = 0;

	if (operation->determineDependingAreaOfInterest(&imageInput, readOperation, output) ) {
		return true;
	}
	return false;
#else
	rcti newInput;
	const float margin = 2;
	
	BLI_rcti_init_minmax(&newInput);
	
	if (m_dispersion_const && m_distortion_const) {
		/* update from fixed distortion/dispersion */
#define UPDATE_INPUT(x, y) \
		{ \
			float coords[6]; \
			determineUV(coords, x, y); \
			newInput.xmin = min_ffff(newInput.xmin, coords[0], coords[2], coords[4]); \
			newInput.ymin = min_ffff(newInput.ymin, coords[1], coords[3], coords[5]); \
			newInput.xmax = max_ffff(newInput.xmax, coords[0], coords[2], coords[4]); \
			newInput.ymax = max_ffff(newInput.ymax, coords[1], coords[3], coords[5]); \
		} (void)0

		UPDATE_INPUT(input->xmin, input->xmax);
		UPDATE_INPUT(input->xmin, input->ymax);
		UPDATE_INPUT(input->xmax, input->ymax);
		UPDATE_INPUT(input->xmax, input->ymin);

#undef UPDATE_INPUT
	}
	else {
		/* use maximum dispersion 1.0 if not const */
		float dispersion = m_dispersion_const ? m_dispersion : 1.0f;

#define UPDATE_INPUT(x, y, distortion) \
		{ \
			float coords[6]; \
			updateVariables(distortion, dispersion); \
			determineUV(coords, x, y); \
			newInput.xmin = min_ffff(newInput.xmin, coords[0], coords[2], coords[4]); \
			newInput.ymin = min_ffff(newInput.ymin, coords[1], coords[3], coords[5]); \
			newInput.xmax = max_ffff(newInput.xmax, coords[0], coords[2], coords[4]); \
			newInput.ymax = max_ffff(newInput.ymax, coords[1], coords[3], coords[5]); \
		} (void)0
		
		if (m_distortion_const) {
			/* update from fixed distortion */
			UPDATE_INPUT(input->xmin, input->xmax, m_distortion);
			UPDATE_INPUT(input->xmin, input->ymax, m_distortion);
			UPDATE_INPUT(input->xmax, input->ymax, m_distortion);
			UPDATE_INPUT(input->xmax, input->ymin, m_distortion);
		}
		else {
			/* update from min/max distortion (-1..1) */
			UPDATE_INPUT(input->xmin, input->xmax, -1.0f);
			UPDATE_INPUT(input->xmin, input->ymax, -1.0f);
			UPDATE_INPUT(input->xmax, input->ymax, -1.0f);
			UPDATE_INPUT(input->xmax, input->ymin, -1.0f);

			UPDATE_INPUT(input->xmin, input->xmax, 1.0f);
			UPDATE_INPUT(input->xmin, input->ymax, 1.0f);
			UPDATE_INPUT(input->xmax, input->ymax, 1.0f);
			UPDATE_INPUT(input->xmax, input->ymin, 1.0f);

#undef UPDATE_INPUT
		}
	}

	newInput.xmin -= margin;
	newInput.ymin -= margin;
	newInput.xmax += margin;
	newInput.ymax += margin;

	operation = getInputOperation(0);
	if (operation->determineDependingAreaOfInterest(&newInput, readOperation, output)) {
		return true;
	}
	return false;
#endif
}

void ScreenLensDistortionOperation::updateVariables(float distortion, float dispersion)
{
	m_k[1] = max_ff(min_ff(distortion, 1.0f), -0.999f);
	// smaller dispersion range for somewhat more control
	float d = 0.25f * max_ff(min_ff(dispersion, 1.0f), 0.0f);
	m_k[0] = max_ff(min_ff((m_k[1] + d), 1.0f), -0.999f);
	m_k[2] = max_ff(min_ff((m_k[1] - d), 1.0f), -0.999f);
	m_maxk = max_fff(m_k[0], m_k[1], m_k[2]);
	m_sc = (m_fit && (m_maxk > 0.0f)) ? (1.0f / (1.0f + 2.0f * m_maxk)) :
	                                    (1.0f / (1.0f +        m_maxk));
	m_dk4[0] = 4.0f * (m_k[1] - m_k[0]);
	m_dk4[1] = 4.0f * (m_k[2] - m_k[1]);
	m_dk4[2] = 0.0f; /* unused */

	mul_v3_v3fl(m_k4, m_k, 4.0f);
}
