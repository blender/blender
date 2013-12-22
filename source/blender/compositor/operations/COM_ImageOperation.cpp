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
#include "DNA_image_types.h"
#include "BKE_image.h"
#include "BLI_math.h"

extern "C" {
#  include "RE_pipeline.h"
#  include "RE_shader_ext.h"
#  include "RE_render_ext.h"
#  include "IMB_imbuf.h"
#  include "IMB_imbuf_types.h"
#  include "IMB_colormanagement.h"
}

BaseImageOperation::BaseImageOperation() : NodeOperation()
{
	this->m_image = NULL;
	this->m_buffer = NULL;
	this->m_imageFloatBuffer = NULL;
	this->m_imageByteBuffer = NULL;
	this->m_imageUser = NULL;
	this->m_imagewidth = 0;
	this->m_imageheight = 0;
	this->m_framenumber = 0;
	this->m_depthBuffer = NULL;
	this->m_numberOfChannels = 0;
}
ImageOperation::ImageOperation() : BaseImageOperation()
{
	this->addOutputSocket(COM_DT_COLOR);
}
ImageAlphaOperation::ImageAlphaOperation() : BaseImageOperation()
{
	this->addOutputSocket(COM_DT_VALUE);
}
ImageDepthOperation::ImageDepthOperation() : BaseImageOperation()
{
	this->addOutputSocket(COM_DT_VALUE);
}

ImBuf *BaseImageOperation::getImBuf()
{
	ImBuf *ibuf;
	
	ibuf = BKE_image_acquire_ibuf(this->m_image, this->m_imageUser, NULL);
	if (ibuf == NULL || (ibuf->rect == NULL && ibuf->rect_float == NULL)) {
		BKE_image_release_ibuf(this->m_image, ibuf, NULL);
		return NULL;
	}
	return ibuf;
}


void BaseImageOperation::initExecution()
{
	ImBuf *stackbuf = getImBuf();
	this->m_buffer = stackbuf;
	if (stackbuf) {
		this->m_imageFloatBuffer = stackbuf->rect_float;
		this->m_imageByteBuffer = stackbuf->rect;
		this->m_depthBuffer = stackbuf->zbuf_float;
		this->m_imagewidth = stackbuf->x;
		this->m_imageheight = stackbuf->y;
		this->m_numberOfChannels = stackbuf->channels;
	}
}

void BaseImageOperation::deinitExecution()
{
	this->m_imageFloatBuffer = NULL;
	this->m_imageByteBuffer = NULL;
	BKE_image_release_ibuf(this->m_image, this->m_buffer, NULL);
}

void BaseImageOperation::determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2])
{
	ImBuf *stackbuf = getImBuf();

	resolution[0] = 0;
	resolution[1] = 0;

	if (stackbuf) {
		resolution[0] = stackbuf->x;
		resolution[1] = stackbuf->y;
	}

	BKE_image_release_ibuf(this->m_image, stackbuf, NULL);
}

static void sampleImageAtLocation(ImBuf *ibuf, float x, float y, PixelSampler sampler, bool make_linear_rgb, float color[4])
{
	if (ibuf->rect_float) {
		switch (sampler) {
			case COM_PS_NEAREST:
				nearest_interpolation_color(ibuf, NULL, color, x, y);
				break;
			case COM_PS_BILINEAR:
				bilinear_interpolation_color(ibuf, NULL, color, x, y);
				break;
			case COM_PS_BICUBIC:
				bicubic_interpolation_color(ibuf, NULL, color, x, y);
				break;
		}
	}
	else {
		unsigned char byte_color[4];
		switch (sampler) {
			case COM_PS_NEAREST:
				nearest_interpolation_color(ibuf, byte_color, NULL, x, y);
				break;
			case COM_PS_BILINEAR:
				bilinear_interpolation_color(ibuf, byte_color, NULL, x, y);
				break;
			case COM_PS_BICUBIC:
				bicubic_interpolation_color(ibuf, byte_color, NULL, x, y);
				break;
		}
		rgba_uchar_to_float(color, byte_color);
		if (make_linear_rgb) {
			IMB_colormanagement_colorspace_to_scene_linear_v4(color, false, ibuf->rect_colorspace);
		}
	}
}

void ImageOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	if (this->m_imageFloatBuffer == NULL && this->m_imageByteBuffer == NULL) {
		zero_v4(output);
	}
	else {
		sampleImageAtLocation(this->m_buffer, x, y, sampler, true, output);
	}
}

void ImageAlphaOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float tempcolor[4];

	if (this->m_imageFloatBuffer == NULL && this->m_imageByteBuffer == NULL) {
		output[0] = 0.0f;
	}
	else {
		tempcolor[3] = 1.0f;
		sampleImageAtLocation(this->m_buffer, x, y, sampler, false, tempcolor);
		output[0] = tempcolor[3];
	}
}

void ImageDepthOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	if (this->m_depthBuffer == NULL) {
		output[0] = 0.0f;
	}
	else {
		if (x < 0 || y < 0 || x >= this->getWidth() || y >= this->getHeight())
			output[0] = 0.0f;
		else {
			int offset = y * this->m_width + x;
			output[0] = this->m_depthBuffer[offset];
		}
	}
}
