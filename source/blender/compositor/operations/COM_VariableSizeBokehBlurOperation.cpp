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
#include "COM_OpenCLDevice.h"

extern "C" {
#  include "RE_pipeline.h"
}

VariableSizeBokehBlurOperation::VariableSizeBokehBlurOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_COLOR, COM_SC_NO_RESIZE); // do not resize the bokeh image.
	this->addInputSocket(COM_DT_VALUE); // radius
#ifdef COM_DEFOCUS_SEARCH
	this->addInputSocket(COM_DT_COLOR, COM_SC_NO_RESIZE); // inverse search radius optimization structure.
#endif
	this->addOutputSocket(COM_DT_COLOR);
	this->setComplex(true);
	this->setOpenCL(true);

	this->m_inputProgram = NULL;
	this->m_inputBokehProgram = NULL;
	this->m_inputSizeProgram = NULL;
	this->m_maxBlur = 32.0f;
	this->m_threshold = 1.0f;
	this->m_do_size_scale = false;
#ifdef COM_DEFOCUS_SEARCH
	this->m_inputSearchProgram = NULL;
#endif
}


void VariableSizeBokehBlurOperation::initExecution()
{
	this->m_inputProgram = getInputSocketReader(0);
	this->m_inputBokehProgram = getInputSocketReader(1);
	this->m_inputSizeProgram = getInputSocketReader(2);
#ifdef COM_DEFOCUS_SEARCH
	this->m_inputSearchProgram = getInputSocketReader(3);
#endif
	QualityStepHelper::initExecution(COM_QH_INCREASE);
}
struct VariableSizeBokehBlurTileData {
	MemoryBuffer *color;
	MemoryBuffer *bokeh;
	MemoryBuffer *size;
	int maxBlurScalar;
};

void *VariableSizeBokehBlurOperation::initializeTileData(rcti *rect)
{
	VariableSizeBokehBlurTileData *data = new VariableSizeBokehBlurTileData();
	data->color = (MemoryBuffer *)this->m_inputProgram->initializeTileData(rect);
	data->bokeh = (MemoryBuffer *)this->m_inputBokehProgram->initializeTileData(rect);
	data->size = (MemoryBuffer *)this->m_inputSizeProgram->initializeTileData(rect);


	rcti rect2;
	this->determineDependingAreaOfInterest(rect, (ReadBufferOperation *)this->m_inputSizeProgram, &rect2);

	const float max_dim = max(m_width, m_height);
	const float scalar = this->m_do_size_scale ? (max_dim / 100.0f) : 1.0f;

	data->maxBlurScalar = (int)(data->size->getMaximumValue(&rect2) * scalar);
	CLAMP(data->maxBlurScalar, 1.0f, this->m_maxBlur);
	return data;
}

void VariableSizeBokehBlurOperation::deinitializeTileData(rcti * /*rect*/, void *data)
{
	VariableSizeBokehBlurTileData *result = (VariableSizeBokehBlurTileData *)data;
	delete result;
}

void VariableSizeBokehBlurOperation::executePixel(float output[4], int x, int y, void *data)
{
	VariableSizeBokehBlurTileData *tileData = (VariableSizeBokehBlurTileData *)data;
	MemoryBuffer *inputProgramBuffer = tileData->color;
	MemoryBuffer *inputBokehBuffer = tileData->bokeh;
	MemoryBuffer *inputSizeBuffer = tileData->size;
	float *inputSizeFloatBuffer = inputSizeBuffer->getBuffer();
	float *inputProgramFloatBuffer = inputProgramBuffer->getBuffer();
	float readColor[4];
	float bokeh[4];
	float tempSize[4];
	float multiplier_accum[4];
	float color_accum[4];

	const float max_dim = max(m_width, m_height);
	const float scalar = this->m_do_size_scale ? (max_dim / 100.0f) : 1.0f;
	int maxBlurScalar = tileData->maxBlurScalar;

	BLI_assert(inputBokehBuffer->getWidth()  == COM_BLUR_BOKEH_PIXELS);
	BLI_assert(inputBokehBuffer->getHeight() == COM_BLUR_BOKEH_PIXELS);

#ifdef COM_DEFOCUS_SEARCH
	float search[4];
	this->m_inputSearchProgram->read(search, x / InverseSearchRadiusOperation::DIVIDER, y / InverseSearchRadiusOperation::DIVIDER, NULL);
	int minx = search[0];
	int miny = search[1];
	int maxx = search[2];
	int maxy = search[3];
#else
	int minx = max(x - maxBlurScalar, 0);
	int miny = max(y - maxBlurScalar, 0);
	int maxx = min(x + maxBlurScalar, (int)m_width);
	int maxy = min(y + maxBlurScalar, (int)m_height);
#endif
	{
		inputSizeBuffer->readNoCheck(tempSize, x, y);
		inputProgramBuffer->readNoCheck(readColor, x, y);

		copy_v4_v4(color_accum, readColor);
		copy_v4_fl(multiplier_accum, 1.0f);
		float size_center = tempSize[0] * scalar;

		const int addXStepValue = QualityStepHelper::getStep();
		const int addYStepValue = addXStepValue;
		const int addXStepColor = addXStepValue * COM_NUM_CHANNELS_COLOR;

		if (size_center > this->m_threshold) {
			for (int ny = miny; ny < maxy; ny += addYStepValue) {
				float dy = ny - y;
				int offsetValueNy = ny * inputSizeBuffer->getWidth();
				int offsetValueNxNy = offsetValueNy + (minx);
				int offsetColorNxNy = offsetValueNxNy * COM_NUM_CHANNELS_COLOR;
				for (int nx = minx; nx < maxx; nx += addXStepValue) {
					if (nx != x || ny != y) {
						float size = min(inputSizeFloatBuffer[offsetValueNxNy] * scalar, size_center);
						if (size > this->m_threshold) {
							float dx = nx - x;
							if (size > fabsf(dx) && size > fabsf(dy)) {
								float uv[2] = {
									(float)(COM_BLUR_BOKEH_PIXELS / 2) + (dx / size) * (float)((COM_BLUR_BOKEH_PIXELS / 2) - 1),
									(float)(COM_BLUR_BOKEH_PIXELS / 2) + (dy / size) * (float)((COM_BLUR_BOKEH_PIXELS / 2) - 1)};
								inputBokehBuffer->read(bokeh, uv[0], uv[1]);
								madd_v4_v4v4(color_accum, bokeh, &inputProgramFloatBuffer[offsetColorNxNy]);
								add_v4_v4(multiplier_accum, bokeh);
							}
						}
					}
					offsetColorNxNy += addXStepColor;
					offsetValueNxNy += addXStepValue;				}
			}
		}

		output[0] = color_accum[0] / multiplier_accum[0];
		output[1] = color_accum[1] / multiplier_accum[1];
		output[2] = color_accum[2] / multiplier_accum[2];
		output[3] = color_accum[3] / multiplier_accum[3];

		/* blend in out values over the threshold, otherwise we get sharp, ugly transitions */
		if ((size_center > this->m_threshold) &&
		    (size_center < this->m_threshold * 2.0f))
		{
			/* factor from 0-1 */
			float fac = (size_center - this->m_threshold) / this->m_threshold;
			interp_v4_v4v4(output, readColor, output, fac);
		}
	}

}

void VariableSizeBokehBlurOperation::executeOpenCL(OpenCLDevice *device,
                                       MemoryBuffer *outputMemoryBuffer, cl_mem clOutputBuffer,
                                       MemoryBuffer **inputMemoryBuffers, list<cl_mem> *clMemToCleanUp,
                                       list<cl_kernel> * /*clKernelsToCleanUp*/)
{
	cl_kernel defocusKernel = device->COM_clCreateKernel("defocusKernel", NULL);

	cl_int step = this->getStep();
	cl_int maxBlur;
	cl_float threshold = this->m_threshold;

	MemoryBuffer *sizeMemoryBuffer = this->m_inputSizeProgram->getInputMemoryBuffer(inputMemoryBuffers);

	const float max_dim = max(m_width, m_height);
	cl_float scalar = this->m_do_size_scale ? (max_dim / 100.0f) : 1.0f;

	maxBlur = (cl_int)min_ff(sizeMemoryBuffer->getMaximumValue() * scalar,
	                         (float)this->m_maxBlur);

	device->COM_clAttachMemoryBufferToKernelParameter(defocusKernel, 0, -1, clMemToCleanUp, inputMemoryBuffers, this->m_inputProgram);
	device->COM_clAttachMemoryBufferToKernelParameter(defocusKernel, 1,  -1, clMemToCleanUp, inputMemoryBuffers, this->m_inputBokehProgram);
	device->COM_clAttachMemoryBufferToKernelParameter(defocusKernel, 2,  4, clMemToCleanUp, inputMemoryBuffers, this->m_inputSizeProgram);
	device->COM_clAttachOutputMemoryBufferToKernelParameter(defocusKernel, 3, clOutputBuffer);
	device->COM_clAttachMemoryBufferOffsetToKernelParameter(defocusKernel, 5, outputMemoryBuffer);
	clSetKernelArg(defocusKernel, 6, sizeof(cl_int), &step);
	clSetKernelArg(defocusKernel, 7, sizeof(cl_int), &maxBlur);
	clSetKernelArg(defocusKernel, 8, sizeof(cl_float), &threshold);
	clSetKernelArg(defocusKernel, 9, sizeof(cl_float), &scalar);
	device->COM_clAttachSizeToKernelParameter(defocusKernel, 10, this);

	device->COM_clEnqueueRange(defocusKernel, outputMemoryBuffer, 11, this);
}

void VariableSizeBokehBlurOperation::deinitExecution()
{
	this->m_inputProgram = NULL;
	this->m_inputBokehProgram = NULL;
	this->m_inputSizeProgram = NULL;
#ifdef COM_DEFOCUS_SEARCH
	this->m_inputSearchProgram = NULL;
#endif
}

bool VariableSizeBokehBlurOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInput;
	rcti bokehInput;

	const float max_dim = max(m_width, m_height);
	const float scalar = this->m_do_size_scale ? (max_dim / 100.0f) : 1.0f;
	int maxBlurScalar = this->m_maxBlur * scalar;

	newInput.xmax = input->xmax + maxBlurScalar + 2;
	newInput.xmin = input->xmin - maxBlurScalar + 2;
	newInput.ymax = input->ymax + maxBlurScalar - 2;
	newInput.ymin = input->ymin - maxBlurScalar - 2;
	bokehInput.xmax = COM_BLUR_BOKEH_PIXELS;
	bokehInput.xmin = 0;
	bokehInput.ymax = COM_BLUR_BOKEH_PIXELS;
	bokehInput.ymin = 0;


	NodeOperation *operation = getInputOperation(2);
	if (operation->determineDependingAreaOfInterest(&newInput, readOperation, output) ) {
		return true;
	}
	operation = getInputOperation(1);
	if (operation->determineDependingAreaOfInterest(&bokehInput, readOperation, output) ) {
		return true;
	}
#ifdef COM_DEFOCUS_SEARCH
	rcti searchInput;
	searchInput.xmax = (input->xmax / InverseSearchRadiusOperation::DIVIDER) + 1;
	searchInput.xmin = (input->xmin / InverseSearchRadiusOperation::DIVIDER) - 1;
	searchInput.ymax = (input->ymax / InverseSearchRadiusOperation::DIVIDER) + 1;
	searchInput.ymin = (input->ymin / InverseSearchRadiusOperation::DIVIDER) - 1;
	operation = getInputOperation(3);
	if (operation->determineDependingAreaOfInterest(&searchInput, readOperation, output) ) {
		return true;
	}
#endif
	operation = getInputOperation(0);
	if (operation->determineDependingAreaOfInterest(&newInput, readOperation, output) ) {
		return true;
	}
	return false;
}

#ifdef COM_DEFOCUS_SEARCH
// InverseSearchRadiusOperation
InverseSearchRadiusOperation::InverseSearchRadiusOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_VALUE, COM_SC_NO_RESIZE); // radius
	this->addOutputSocket(COM_DT_COLOR);
	this->setComplex(true);
	this->m_inputRadius = NULL;
}

void InverseSearchRadiusOperation::initExecution()
{
	this->m_inputRadius = this->getInputSocketReader(0);
}

void *InverseSearchRadiusOperation::initializeTileData(rcti *rect)
{
	MemoryBuffer *data = new MemoryBuffer(COM_DT_COLOR, rect);
	float *buffer = data->getBuffer();
	int x, y;
	int width = this->m_inputRadius->getWidth();
	int height = this->m_inputRadius->getHeight();
	float temp[4];
	int offset = 0;
	for (y = rect->ymin; y < rect->ymax ; y++) {
		for (x = rect->xmin; x < rect->xmax ; x++) {
			int rx = x * DIVIDER;
			int ry = y * DIVIDER;
			buffer[offset] = MAX2(rx - m_maxBlur, 0);
			buffer[offset + 1] = MAX2(ry - m_maxBlur, 0);
			buffer[offset + 2] = MIN2(rx + DIVIDER + m_maxBlur, width);
			buffer[offset + 3] = MIN2(ry + DIVIDER + m_maxBlur, height);
			offset += 4;
		}
	}
//	for (x = rect->xmin; x < rect->xmax ; x++) {
//		for (y = rect->ymin; y < rect->ymax ; y++) {
//			int rx = x * DIVIDER;
//			int ry = y * DIVIDER;
//			float radius = 0.0f;
//			float maxx = x;
//			float maxy = y;

//			for (int x2 = 0 ; x2 < DIVIDER ; x2 ++) {
//				for (int y2 = 0 ; y2 < DIVIDER ; y2 ++) {
//					this->m_inputRadius->read(temp, rx+x2, ry+y2, COM_PS_NEAREST);
//					if (radius < temp[0]) {
//						radius = temp[0];
//						maxx = x2;
//						maxy = y2;
//					}
//				}
//			}
//			int impactRadius = ceil(radius / DIVIDER);
//			for (int x2 = x - impactRadius ; x2 < x + impactRadius ; x2 ++) {
//				for (int y2 = y - impactRadius ; y2 < y + impactRadius ; y2 ++) {
//					data->read(temp, x2, y2);
//					temp[0] = MIN2(temp[0], maxx);
//					temp[1] = MIN2(temp[1], maxy);
//					temp[2] = MAX2(temp[2], maxx);
//					temp[3] = MAX2(temp[3], maxy);
//					data->writePixel(x2, y2, temp);
//				}
//			}
//		}
//	}
	return data;
}

void InverseSearchRadiusOperation::executePixelChunk(float output[4], int x, int y, void *data)
{
	MemoryBuffer *buffer = (MemoryBuffer *)data;
	buffer->readNoCheck(output, x, y);
}

void InverseSearchRadiusOperation::deinitializeTileData(rcti *rect, void *data)
{
	if (data) {
		MemoryBuffer *mb = (MemoryBuffer *)data;
		delete mb;
	}
}

void InverseSearchRadiusOperation::deinitExecution()
{
	this->m_inputRadius = NULL;
}

void InverseSearchRadiusOperation::determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2])
{
	NodeOperation::determineResolution(resolution, preferredResolution);
	resolution[0] = resolution[0] / DIVIDER;
	resolution[1] = resolution[1] / DIVIDER;
}

bool InverseSearchRadiusOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newRect;
	newRect.ymin = input->ymin * DIVIDER - m_maxBlur;
	newRect.ymax = input->ymax * DIVIDER + m_maxBlur;
	newRect.xmin = input->xmin * DIVIDER - m_maxBlur;
	newRect.xmax = input->xmax * DIVIDER + m_maxBlur;
	return NodeOperation::determineDependingAreaOfInterest(&newRect, readOperation, output);
}
#endif
