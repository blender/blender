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

#include "COM_DirectionalBlurOperation.h"
#include "BLI_math.h"
#include "COM_OpenCLDevice.h"
extern "C" {
#  include "RE_pipeline.h"
}

DirectionalBlurOperation::DirectionalBlurOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_COLOR);
	this->setComplex(true);

	this->setOpenCL(true);
	this->m_inputProgram = NULL;
}

void DirectionalBlurOperation::initExecution()
{
	this->m_inputProgram = getInputSocketReader(0);
	QualityStepHelper::initExecution(COM_QH_INCREASE);
	const float angle = this->m_data->angle;
	const float zoom = this->m_data->zoom;
	const float spin = this->m_data->spin;
	const float iterations = this->m_data->iter;
	const float distance = this->m_data->distance;
	const float center_x = this->m_data->center_x;
	const float center_y = this->m_data->center_y;
	const float width = getWidth();
	const float height = getHeight();

	const float a = angle;
	const float itsc = 1.0f / powf(2.0f, (float)iterations);
	float D;

	D = distance * sqrtf(width * width + height * height);
	this->m_center_x_pix = center_x * width;
	this->m_center_y_pix = center_y * height;

	this->m_tx  =  itsc * D * cosf(a);
	this->m_ty  = -itsc *D *sinf(a);
	this->m_sc  =  itsc * zoom;
	this->m_rot =  itsc * spin;

}

void DirectionalBlurOperation::executePixel(float output[4], int x, int y, void *data)
{
	const int iterations = pow(2.0f, this->m_data->iter);
	float col[4] = {0, 0, 0, 0};
	float col2[4] = {0, 0, 0, 0};
	this->m_inputProgram->readSampled(col2, x, y, COM_PS_NEAREST);
	float ltx = this->m_tx;
	float lty = this->m_ty;
	float lsc = this->m_sc;
	float lrot = this->m_rot;
	/* blur the image */
	for (int i = 0; i < iterations; ++i) {
		const float cs = cos(lrot), ss = sin(lrot);
		const float isc = 1.0f / (1.0f + lsc);

		const float v = isc * (y - this->m_center_y_pix) + lty;
		const float u = isc * (x - this->m_center_x_pix) + ltx;

		this->m_inputProgram->readSampled(col,
		                           cs * u + ss * v + this->m_center_x_pix,
		                           cs * v - ss * u + this->m_center_y_pix,
		                           COM_PS_NEAREST);

		add_v4_v4(col2, col);

		/* double transformations */
		ltx += this->m_tx;
		lty += this->m_ty;
		lrot += this->m_rot;
		lsc += this->m_sc;
	}

	mul_v4_v4fl(output, col2, 1.0f / (iterations + 1));
}

void DirectionalBlurOperation::executeOpenCL(OpenCLDevice *device,
                                       MemoryBuffer *outputMemoryBuffer, cl_mem clOutputBuffer, 
                                       MemoryBuffer **inputMemoryBuffers, list<cl_mem> *clMemToCleanUp, 
                                       list<cl_kernel> *clKernelsToCleanUp) 
{
	cl_kernel directionalBlurKernel = device->COM_clCreateKernel("directionalBlurKernel", NULL);

	cl_int iterations = pow(2.0f, this->m_data->iter);
	cl_float2 ltxy = {this->m_tx,  this->m_ty};
	cl_float2 centerpix = {this->m_center_x_pix, this->m_center_y_pix};
	cl_float lsc = this->m_sc;
	cl_float lrot = this->m_rot;
	
	device->COM_clAttachMemoryBufferToKernelParameter(directionalBlurKernel, 0, -1, clMemToCleanUp, inputMemoryBuffers, this->m_inputProgram);
	device->COM_clAttachOutputMemoryBufferToKernelParameter(directionalBlurKernel, 1, clOutputBuffer);
	device->COM_clAttachMemoryBufferOffsetToKernelParameter(directionalBlurKernel, 2, outputMemoryBuffer);
	clSetKernelArg(directionalBlurKernel, 3, sizeof(cl_int), &iterations);
	clSetKernelArg(directionalBlurKernel, 4, sizeof(cl_float), &lsc);
	clSetKernelArg(directionalBlurKernel, 5, sizeof(cl_float), &lrot);
	clSetKernelArg(directionalBlurKernel, 6, sizeof(cl_float2), &ltxy);
	clSetKernelArg(directionalBlurKernel, 7, sizeof(cl_float2), &centerpix);
	
	device->COM_clEnqueueRange(directionalBlurKernel, outputMemoryBuffer, 8, this);
}


void DirectionalBlurOperation::deinitExecution()
{
	this->m_inputProgram = NULL;
}

bool DirectionalBlurOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInput;

	newInput.xmax = this->getWidth();
	newInput.xmin = 0;
	newInput.ymax = this->getHeight();
	newInput.ymin = 0;

	return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}
