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
#include "BLI_math.h"
#include "BLI_utildefines.h"
extern "C" {
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
}

void *ScreenLensDistortionOperation::initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers)
{
	void *buffer = this->m_inputProgram->initializeTileData(NULL, memoryBuffers);
	updateDispersionAndDistortion(memoryBuffers);
	return buffer;
}

void ScreenLensDistortionOperation::executePixel(float *outputColor, int x, int y, MemoryBuffer *inputBuffers[], void *data)
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

	if ((t = 1.f - this->m_kr4 * uv_dot) >= 0.f) {
		d = 1.f / (1.f + sqrtf(t));
		ln[0] = (u * d + 0.5f) * width - 0.5f, ln[1] = (v * d + 0.5f) * height - 0.5f;
		sta = 1;
	}
	if ((t = 1.f - this->m_kg4 * uv_dot) >= 0.f) {
		d = 1.f / (1.f + sqrtf(t));
		ln[2] = (u * d + 0.5f) * width - 0.5f, ln[3] = (v * d + 0.5f) * height - 0.5f;
		mid = 1;
	}
	if ((t = 1.f - this->m_kb4 * uv_dot) >= 0.f) {
		d = 1.f / (1.f + sqrtf(t));
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
			const float dsf = sqrtf((float)dx * dx + dy * dy) + 1.f;
			const int ds = (int)(jit ? ((dsf < 4.f) ? 2.f : sqrtf(dsf)) : dsf);
			const float sd = 1.f / (float)ds;

			for (z = 0; z < ds; ++z) {
				const float tz = ((float)z + (jit ? BLI_frand() : 0.5f)) * sd;
				t = 1.0f - (this->m_kr4 + tz * this->m_drg) * uv_dot;
				d = 1.0f / (1.f + sqrtf(t));
				const float nx = (u * d + 0.5f) * width - 0.5f;
				const float ny = (v * d + 0.5f) * height - 0.5f;
				buffer->readCubic(color, nx, ny);
				tc[0] += (1.f - tz) * color[0], tc[1] += tz * color[1];
				dr++, dg++;
			}
		}
		{
			// GB
			const int dx = ln[4] - ln[2], dy = ln[5] - ln[3];
			const float dsf = sqrtf((float)dx * dx + dy * dy) + 1.f;
			const int ds = (int)(jit ? ((dsf < 4.f) ? 2.f : sqrtf(dsf)) : dsf);
			const float sd = 1.f / (float)ds;

			for (z = 0; z < ds; ++z) {
				const float tz = ((float)z + (jit ? BLI_frand() : 0.5f)) * sd;
				t = 1.f - (this->m_kg4 + tz * this->m_dgb) * uv_dot;
				d = 1.f / (1.f + sqrtf(t));
				const float nx = (u * d + 0.5f) * width - 0.5f;
				const float ny = (v * d + 0.5f) * height - 0.5f;
				buffer->readCubic(color, nx, ny);
				tc[1] += (1.f - tz) * color[1], tc[2] += tz * color[2];
				dg++, db++;
			}

		}
		if (dr) outputColor[0] = 2.0f * tc[0] / (float)dr;
		if (dg) outputColor[1] = 2.0f * tc[1] / (float)dg;
		if (db) outputColor[2] = 2.0f * tc[2] / (float)db;

		/* set alpha */
		outputColor[3] = 1.0f;
	}
	else {
		outputColor[0] = 0.0f;
		outputColor[1] = 0.0f;
		outputColor[2] = 0.0f;
		outputColor[3] = 0.0f;
	}
}

void ScreenLensDistortionOperation::deinitExecution()
{
	this->m_inputProgram = NULL;
}

void ScreenLensDistortionOperation::determineUV(float result[2], float x, float y) const
{
	const float v = this->m_sc * ((y + 0.5f) - this->m_cy) / this->m_cy;
	const float u = this->m_sc * ((x + 0.5f) - this->m_cx) / this->m_cx;
	const float t = ABS(MIN3(this->m_kr, this->m_kg, this->m_kb) * 4);
	float d = 1.f / (1.f + sqrtf(t));
	result[0] = (u * d + 0.5f) * getWidth() - 0.5f;
	result[1] = (v * d + 0.5f) * getHeight() - 0.5f;
}

bool ScreenLensDistortionOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInput;
	newInput.xmin = 0;
	newInput.ymin = 0;
	newInput.ymax = this->m_inputProgram->getHeight();
	newInput.xmax = this->m_inputProgram->getWidth();
	return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}

void ScreenLensDistortionOperation::updateDispersionAndDistortion(MemoryBuffer **inputBuffers)
{
	if (!this->m_valuesAvailable) {
		float result[4];
		this->getInputSocketReader(1)->read(result, 0, 0, COM_PS_NEAREST, inputBuffers);
		this->m_distortion = result[0];
		this->getInputSocketReader(2)->read(result, 0, 0, COM_PS_NEAREST, inputBuffers);
		this->m_dispersion = result[0];
		this->m_kg = MAX2(MIN2(this->m_distortion, 1.f), -0.999f);
		// smaller dispersion range for somewhat more control
		const float d = 0.25f * MAX2(MIN2(this->m_dispersion, 1.f), 0.f);
		this->m_kr = MAX2(MIN2((this->m_kg + d), 1.0f), -0.999f);
		this->m_kb = MAX2(MIN2((this->m_kg - d), 1.0f), -0.999f);
		this->m_maxk = MAX3(this->m_kr, this->m_kg, this->m_kb);
		this->m_sc = (this->m_data->fit && (this->m_maxk > 0.f)) ? (1.f / (1.f + 2.f * this->m_maxk)) : (1.f / (1.f + this->m_maxk));
		this->m_drg = 4.f * (this->m_kg - this->m_kr);
		this->m_dgb = 4.f * (this->m_kb - this->m_kg);
	
		this->m_kr4 = this->m_kr * 4.0f;
		this->m_kg4 = this->m_kg * 4.0f;
		this->m_kb4 = this->m_kb * 4.0f;
		this->m_cx = 0.5f * (float)getWidth();
		this->m_cy = 0.5f * (float)getHeight();
		this->m_valuesAvailable = true;
	}
}
