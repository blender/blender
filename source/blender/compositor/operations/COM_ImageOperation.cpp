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

#include "COM_ImageOperation.h"

#include "BLI_listbase.h"
#include "DNA_scene_types.h"
#include "DNA_image_types.h"
#include "BKE_image.h"
#include "BLI_math.h"

extern "C" {
	#include "RE_pipeline.h"
	#include "RE_shader_ext.h"
	#include "RE_render_ext.h"
	#include "IMB_imbuf.h"
	#include "IMB_imbuf_types.h"
}

BaseImageOperation::BaseImageOperation(): NodeOperation() {
	this->image = NULL;
	this->buffer = NULL;
	this->imageBuffer = NULL;
	this->imageUser = NULL;
	this->imagewidth = 0;
	this->imageheight = 0;
	this->framenumber = 0;
	this->depthBuffer = NULL;
	this->numberOfChannels = 0;
}
ImageOperation::ImageOperation(): BaseImageOperation() {
	this->addOutputSocket(COM_DT_COLOR);
}
ImageAlphaOperation::ImageAlphaOperation(): BaseImageOperation() {
	this->addOutputSocket(COM_DT_VALUE);
}
ImageDepthOperation::ImageDepthOperation(): BaseImageOperation() {
	this->addOutputSocket(COM_DT_VALUE);
}

ImBuf* BaseImageOperation::getImBuf() {
	ImBuf *ibuf;
	
	ibuf= BKE_image_get_ibuf(this->image, this->imageUser);
	if(ibuf==NULL || (ibuf->rect==NULL && ibuf->rect_float==NULL)) {
			return NULL;
	}
	
	if (ibuf->rect_float == NULL) {
			IMB_float_from_rect(ibuf);
	}
	return ibuf;
}


void BaseImageOperation::initExecution() {
	ImBuf *stackbuf= getImBuf();
	this->buffer = stackbuf;
	if (stackbuf) {
		this->imageBuffer = stackbuf->rect_float;
		this->depthBuffer = stackbuf->zbuf_float;
		this->imagewidth = stackbuf->x;
		this->imageheight = stackbuf->y;
		this->numberOfChannels = stackbuf->channels;
	}
}

void BaseImageOperation::deinitExecution() {
	this->imageBuffer= NULL;
}

void BaseImageOperation::determineResolution(unsigned int resolution[], unsigned int preferredResolution[]) {
	ImBuf *stackbuf= getImBuf();
	if (stackbuf) {
		resolution[0] = stackbuf->x;
		resolution[1] = stackbuf->y;
	}
}

void ImageOperation::executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	if (this->imageBuffer == NULL || x < 0 || y < 0 || x >= this->getWidth() || y >= this->getHeight() ) {
		color[0] = 0.0f;
		color[1] = 0.0f;
		color[2] = 0.0f;
		color[3] = 0.0f;
	} else {
		switch (sampler) {
		case COM_PS_NEAREST:
			neareast_interpolation_color(this->buffer, NULL, color, x, y);
			break;
		case COM_PS_BILINEAR:
			bilinear_interpolation_color(this->buffer, NULL, color, x, y);
			break;
		case COM_PS_BICUBIC:
			bicubic_interpolation_color(this->buffer, NULL, color, x, y);
			break;
		}
	}
}

void ImageAlphaOperation::executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	float tempcolor[4];

	if (this->imageBuffer == NULL || x < 0 || y < 0 || x >= this->getWidth() || y >= this->getHeight() ) {
		color[0] = 0.0f;
	} else {
		tempcolor[3] = 1.0f;
		switch (sampler) {
		case COM_PS_NEAREST:
			neareast_interpolation_color(this->buffer, NULL, tempcolor, x, y);
			break;
		case COM_PS_BILINEAR:
			bilinear_interpolation_color(this->buffer, NULL, tempcolor, x, y);
			break;
		case COM_PS_BICUBIC:
			bicubic_interpolation_color(this->buffer, NULL, tempcolor, x, y);
			break;
		}
		color[0] = tempcolor[3];
	}
}

void ImageDepthOperation::executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	if (this->depthBuffer == NULL || x < 0 || y < 0 || x >= this->getWidth() || y >= this->getHeight() ) {
		color[0] = 0.0f;
	} else {
		int offset = y * width + x;
		color[0] = this->depthBuffer[offset];
	}
}
