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

#include "COM_VariableSizeBokehBlurOperation.h"
#include "BLI_math.h"

extern "C" {
	#include "RE_pipeline.h"
}

VariableSizeBokehBlurOperation::VariableSizeBokehBlurOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_COLOR, COM_SC_NO_RESIZE); // do not resize the bokeh image.
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_COLOR);
	this->setComplex(true);

	this->inputProgram = NULL;
	this->inputBokehProgram = NULL;
	this->inputSizeProgram = NULL;
	this->maxBlur = 32.0f;
	this->threshold = 0.0f;
}


void VariableSizeBokehBlurOperation::initExecution()
{
	this->inputProgram = getInputSocketReader(0);
	this->inputBokehProgram = getInputSocketReader(1);
	this->inputSizeProgram = getInputSocketReader(2);
	QualityStepHelper::initExecution(COM_QH_INCREASE);
}

void VariableSizeBokehBlurOperation::executePixel(float *color, int x, int y, MemoryBuffer *inputBuffers[], void *data)
{
	float readColor[4];
	float bokeh[4];
	float tempSize[4];
	float multiplier_accum[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	float color_accum[4]      = {0.0f, 0.0f, 0.0f, 0.0f};

	int miny = y - maxBlur;
	int maxy = y + maxBlur;
	int minx = x - maxBlur;
	int maxx = x + maxBlur;
	{
		inputProgram->read(readColor, x, y, COM_PS_NEAREST, inputBuffers);
		color_accum[0] += readColor[0];
		color_accum[1] += readColor[1];
		color_accum[2] += readColor[2];
		color_accum[3] += readColor[3];
		add_v4_v4(color_accum, readColor);
		add_v3_fl(multiplier_accum, 1.0f);
		
		for (int ny = miny; ny < maxy; ny += QualityStepHelper::getStep()) {
			for (int nx = minx; nx < maxx; nx += QualityStepHelper::getStep()) {
				if (nx >= 0 && nx < this->getWidth() && ny >= 0 && ny < getHeight()) {
					inputSizeProgram->read(tempSize, nx, ny, COM_PS_NEAREST, inputBuffers);
					float size = tempSize[0];
//					size += this->threshold;
					float dx = nx - x;
					float dy = ny - y;
					if (nx == x && ny == y) {
						/* pass */
					}
					else if (size >= fabsf(dx) && size >= fabsf(dy)) {
						float u = 256 + dx * 256 / size;
						float v = 256 + dy * 256 / size;
						inputBokehProgram->read(bokeh, u, v, COM_PS_NEAREST, inputBuffers);
						inputProgram->read(readColor, nx, ny, COM_PS_NEAREST, inputBuffers);
						madd_v4_v4v4(color_accum, bokeh, readColor);
						add_v4_v4(multiplier_accum, bokeh);
					}
				}
			}
		}

		color[0] = color_accum[0] * (1.0f / multiplier_accum[0]);
		color[1] = color_accum[1] * (1.0f / multiplier_accum[1]);
		color[2] = color_accum[2] * (1.0f / multiplier_accum[2]);
		color[3] = color_accum[3] * (1.0f / multiplier_accum[3]);
	}

}

void VariableSizeBokehBlurOperation::deinitExecution()
{
	this->inputProgram = NULL;
	this->inputBokehProgram = NULL;
	this->inputSizeProgram = NULL;
}

bool VariableSizeBokehBlurOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInput;
	rcti bokehInput;

	newInput.xmax = input->xmax + maxBlur + 2;
	newInput.xmin = input->xmin - maxBlur + 2;
	newInput.ymax = input->ymax + maxBlur - 2;
	newInput.ymin = input->ymin - maxBlur - 2;
	bokehInput.xmax = 512;
	bokehInput.xmin = 0;
	bokehInput.ymax = 512;
	bokehInput.ymin = 0;

	NodeOperation *operation = getInputOperation(2);
	if (operation->determineDependingAreaOfInterest(&newInput, readOperation, output) ) {
		return true;
	}
	operation = getInputOperation(1);
	if (operation->determineDependingAreaOfInterest(&bokehInput, readOperation, output) ) {
		return true;
	}
	operation = getInputOperation(0);
	if (operation->determineDependingAreaOfInterest(&newInput, readOperation, output) ) {
		return true;
	}
	return false;
}
