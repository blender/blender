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
 *		Lukas Tönne
 */

#include "COM_MultilayerImageOperation.h"
extern "C" {
	#include "IMB_imbuf.h"
	#include "IMB_imbuf_types.h"
}

MultilayerBaseOperation::MultilayerBaseOperation(int pass) : BaseImageOperation()
{
	this->m_passId = pass;
}
ImBuf *MultilayerBaseOperation::getImBuf()
{
	RenderPass *rpass;
	rpass = (RenderPass *)BLI_findlink(&this->m_renderlayer->passes, this->m_passId);
	if (rpass) {
		this->m_imageUser->pass = this->m_passId;
		BKE_image_multilayer_index(this->m_image->rr, this->m_imageUser);
		return BaseImageOperation::getImBuf();
	}
	return NULL;
}

void MultilayerColorOperation::executePixel(float *color, float x, float y, PixelSampler sampler)
{
	int yi = y;
	int xi = x;
	if (this->m_imageBuffer == NULL || xi < 0 || yi < 0 || (unsigned int)xi >= this->getWidth() || (unsigned int)yi >= this->getHeight() ) {
		color[0] = 0.0f;
		color[1] = 0.0f;
		color[2] = 0.0f;
		color[3] = 0.0f;
	}
	else {
		if (this->m_numberOfChannels == 4) {
			switch (sampler) {
				case COM_PS_NEAREST:
					neareast_interpolation_color(this->m_buffer, NULL, color, x, y);
					break;
				case COM_PS_BILINEAR:
					bilinear_interpolation_color(this->m_buffer, NULL, color, x, y);
					break;
				case COM_PS_BICUBIC:
					bicubic_interpolation_color(this->m_buffer, NULL, color, x, y);
					break;
			}
		}
		else {
			int offset = (yi * this->getWidth() + xi) * 3;
			copy_v3_v3(color, &this->m_imageBuffer[offset]);
		}
	}
}

void MultilayerValueOperation::executePixel(float *color, float x, float y, PixelSampler sampler)
{
	int yi = y;
	int xi = x;
	if (this->m_imageBuffer == NULL || xi < 0 || yi < 0 || (unsigned int)xi >= this->getWidth() || (unsigned int)yi >= this->getHeight() ) {
		color[0] = 0.0f;
	}
	else {
		float result = this->m_imageBuffer[yi * this->getWidth() + xi];
		color[0] = result;
	}
}

void MultilayerVectorOperation::executePixel(float *color, float x, float y, PixelSampler sampler)
{
	int yi = y;
	int xi = x;
	if (this->m_imageBuffer == NULL || xi < 0 || yi < 0 || (unsigned int)xi >= this->getWidth() || (unsigned int)yi >= this->getHeight() ) {
		color[0] = 0.0f;
	}
	else {
		int offset = (yi * this->getWidth() + xi) * 3;
		copy_v3_v3(color, &this->m_imageBuffer[offset]);
	}
}
