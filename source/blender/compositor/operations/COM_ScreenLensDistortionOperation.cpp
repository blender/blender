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
	#include "BLI_math.h"
	#include "BLI_utildefines.h"
	#include "BLI_rand.h"
}

ScreenLensDistortionOperation::ScreenLensDistortionOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_VALUE);
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_COLOR);
	this->setComplex(true);
	this->m_inputProgram = NULL;
	this->m_valuesAvailable = false;
	this->m_dispersion = 0.0f;
	this->m_distortion = 0.0f;
}
void ScreenLensDistortionOperation::initExecution()
{
	this->m_inputProgram = this->getInputSocketReader(0);
	this->initMutex();
	this->m_cx = 0.5f * (float)getWidth();
	this->m_cy = 0.5f * (float)getHeight();
	
}

void *ScreenLensDistortionOperation::initializeTileData(rcti *rect)
{
	void *buffer = this->m_inputProgram->initializeTileData(NULL);
	updateDispersionAndDistortion();
	return buffer;
}

void ScreenLensDistortionOperation::executePixel(float output[4], int x, int y, void *data)
{
	const float height = this->getHeight();
	const float width = this->getWidth();
	MemoryBuffer *buffer = (MemoryBuffer *)data;

	int dr = 0, dg = 0, db = 0;
	float d, t, ln[6] = {0, 0, 0, 0, 0, 0};
	float tc[4] = {0, 0, 0, 0};
	const float v = this->m_sc * ((y + 0.5f) - this->m_cy) / this->m_cy;
	const float u = this->m_sc * ((x + 0.5f) - this->m_cx) / this->m_cx;
	const float uv_dot = u * u + v * v;
	int sta = 0, mid = 0, end = 0;

	if ((t = 1.0f - this->m_kr4 * uv_dot) >= 0.0f) {
		d = 1.0f / (1.0f + sqrtf(t));
		ln[0] = (u * d + 0.5f) * width - 0.5f, ln[1] = (v * d + 0.5f) * height - 0.5f;
		sta = 1;
	}
	if ((t = 1.0f - this->m_kg4 * uv_dot) >= 0.0f) {
		d = 1.0f / (1.0f + sqrtf(t));
		ln[2] = (u * d + 0.5f) * width - 0.5f, ln[3] = (v * d + 0.5f) * height - 0.5f;
		mid = 1;
	}
	if ((t = 1.0f - this->m_kb4 * uv_dot) >= 0.0f) {
		d = 1.0f / (1.0f + sqrtf(t));
		ln[4] = (u * d + 0.5f) * width - 0.5f, ln[5] = (v * d + 0.5f) * height - 0.5f;
		end = 1;
	}

	if (sta && mid && end) {
		float jit = this->m_data->jit;
		float z;
		float color[4];
		{
			// RG
			const int dx = ln[2] - ln[0], dy = ln[3] - ln[1];
			const float dsf = sqrtf((float)dx * dx + dy * dy) + 1.0f;
			const int ds = (int)(jit ? ((dsf < 4.0f) ? 2.0f : sqrtf(dsf)) : dsf);
			const float sd = 1.0f / (float)ds;

			for (z = 0; z < ds; ++z) {
				const float tz = (z + (jit ? BLI_frand() : 0.5f)) * sd;
				t = 1.0f - (this->m_kr4 + tz * this->m_drg) * uv_dot;
				d = 1.0f / (1.0f + sqrtf(t));
				const float nx = (u * d + 0.5f) * width - 0.5f;
				const float ny = (v * d + 0.5f) * height - 0.5f;
				buffer->readCubic(color, nx, ny);
				tc[0] += (1.0f - tz) * color[0], tc[1] += tz * color[1];
				dr++, dg++;
			}
		}
		{
			// GB
			const int dx = ln[4] - ln[2], dy = ln[5] - ln[3];
			const float dsf = sqrtf((float)dx * dx + dy * dy) + 1.0f;
			const int ds = (int)(jit ? ((dsf < 4.0f) ? 2.0f : sqrtf(dsf)) : dsf);
			const float sd = 1.0f / (float)ds;

			for (z = 0; z < ds; ++z) {
				const float tz = (z + (jit ? BLI_frand() : 0.5f)) * sd;
				t = 1.0f - (this->m_kg4 + tz * this->m_dgb) * uv_dot;
				d = 1.0f / (1.0f + sqrtf(t));
				const float nx = (u * d + 0.5f) * width - 0.5f;
				const float ny = (v * d + 0.5f) * height - 0.5f;
				buffer->readCubic(color, nx, ny);
				tc[1] += (1.0f - tz) * color[1], tc[2] += tz * color[2];
				dg++, db++;
			}

		}
		if (dr) output[0] = 2.0f * tc[0] / (float)dr;
		if (dg) output[1] = 2.0f * tc[1] / (float)dg;
		if (db) output[2] = 2.0f * tc[2] / (float)db;

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

void ScreenLensDistortionOperation::determineUV(float result[6], float x, float y, float distortion, float dispersion) 
{
	if (!this->m_valuesAvailable) {
		updateVariables(distortion, dispersion);
	}
	determineUV(result, x, y);
}

void ScreenLensDistortionOperation::determineUV(float result[6], float x, float y) const
{
	const float height = this->getHeight();
	const float width = this->getWidth();
	
	result[0] = x;
	result[1] = y;
	result[2] = x;
	result[3] = y;
	result[4] = x;
	result[5] = y;
	
	float d, t;
	const float v = this->m_sc * ((y + 0.5f) - this->m_cy) / this->m_cy;
	const float u = this->m_sc * ((x + 0.5f) - this->m_cx) / this->m_cx;
	const float uv_dot = u * u + v * v;

	if ((t = 1.0f - this->m_kr4 * uv_dot) >= 0.0f) {
		d = 1.0f / (1.0f + sqrtf(t));
		result[0] = (u * d + 0.5f) * width - 0.5f, result[1] = (v * d + 0.5f) * height - 0.5f;
	}
	if ((t = 1.0f - this->m_kg4 * uv_dot) >= 0.0f) {
		d = 1.0f / (1.0f + sqrtf(t));
		result[2] = (u * d + 0.5f) * width - 0.5f, result[3] = (v * d + 0.5f) * height - 0.5f;
	}
	if ((t = 1.0f - this->m_kb4 * uv_dot) >= 0.0f) {
		d = 1.0f / (1.0f + sqrtf(t));
		result[4] = (u * d + 0.5f) * width - 0.5f, result[5] = (v * d + 0.5f) * height - 0.5f;
	}
	
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

#define UPDATE_INPUT  { \
		newInput.xmin = min_ffff(newInput.xmin, coords[0], coords[2], coords[4]); \
		newInput.ymin = min_ffff(newInput.ymin, coords[1], coords[3], coords[5]); \
		newInput.xmax = max_ffff(newInput.xmax, coords[0], coords[2], coords[4]); \
		newInput.ymax = max_ffff(newInput.ymax, coords[1], coords[3], coords[5]); \
	} (void)0
	
	rcti newInput;
	const float margin = 2;
	float coords[6];
	if (m_valuesAvailable) {
		determineUV(coords, input->xmin, input->ymin);
		newInput.xmin = coords[0];
		newInput.ymin = coords[1];
		newInput.xmax = coords[0];
		newInput.ymax = coords[1];
		UPDATE_INPUT;
		determineUV(coords, input->xmin, input->ymax);
		UPDATE_INPUT;
		determineUV(coords, input->xmax, input->ymax);
		UPDATE_INPUT;
		determineUV(coords, input->xmax, input->ymin);
		UPDATE_INPUT;
	}
	else {
		determineUV(coords, input->xmin, input->ymin, 1.0f, 1.0f);
		newInput.xmin = coords[0];
		newInput.ymin = coords[1];
		newInput.xmax = coords[0];
		newInput.ymax = coords[1];
		UPDATE_INPUT;
		determineUV(coords, input->xmin, input->ymin, -1.0f, 1.0f);
		UPDATE_INPUT;
		
		determineUV(coords, input->xmin, input->ymax, -1.0f, 1.0f);
		UPDATE_INPUT;
		determineUV(coords, input->xmin, input->ymax, 1.0f, 1.0f);
		UPDATE_INPUT;
		
		determineUV(coords, input->xmax, input->ymax, -1.0f, 1.0f);
		UPDATE_INPUT;
		determineUV(coords, input->xmax, input->ymax, 1.0f, 1.0f);
		UPDATE_INPUT;
		
		determineUV(coords, input->xmax, input->ymin, -1.0f, 1.0f);
		UPDATE_INPUT;
		determineUV(coords, input->xmax, input->ymin, 1.0f, 1.0f);
		UPDATE_INPUT;
	}

#undef UPDATE_INPUT
	newInput.xmin -= margin;
	newInput.ymin -= margin;
	newInput.xmax += margin;
	newInput.ymax += margin;

	operation = getInputOperation(0);
	if (operation->determineDependingAreaOfInterest(&newInput, readOperation, output)) {
		return true;
	}
	return false;
}

void ScreenLensDistortionOperation::updateVariables(float distortion, float dispersion)
{
	this->m_kg = max_ff(min_ff(distortion, 1.0f), -0.999f);
	// smaller dispersion range for somewhat more control
	const float d = 0.25f * max_ff(min_ff(dispersion, 1.0f), 0.0f);
	this->m_kr = max_ff(min_ff((this->m_kg + d), 1.0f), -0.999f);
	this->m_kb = max_ff(min_ff((this->m_kg - d), 1.0f), -0.999f);
	this->m_maxk = max_fff(this->m_kr, this->m_kg, this->m_kb);
	this->m_sc = (this->m_data->fit && (this->m_maxk > 0.0f)) ? (1.0f / (1.0f + 2.0f * this->m_maxk)) :
	                                                            (1.0f / (1.0f +        this->m_maxk));
	this->m_drg = 4.0f * (this->m_kg - this->m_kr);
	this->m_dgb = 4.0f * (this->m_kb - this->m_kg);

	this->m_kr4 = this->m_kr * 4.0f;
	this->m_kg4 = this->m_kg * 4.0f;
	this->m_kb4 = this->m_kb * 4.0f;
}

void ScreenLensDistortionOperation::updateDispersionAndDistortion()
{
	if (this->m_valuesAvailable) return;
	
	this->lockMutex();
	if (!this->m_valuesAvailable) {
		float result[4];
		this->getInputSocketReader(1)->read(result, 0, 0, COM_PS_NEAREST);
		this->m_distortion = result[0];
		this->getInputSocketReader(2)->read(result, 0, 0, COM_PS_NEAREST);
		this->m_dispersion = result[0];
		updateVariables(this->m_distortion, this->m_dispersion);
		this->m_valuesAvailable = true;
	}
	this->unlockMutex();
}
