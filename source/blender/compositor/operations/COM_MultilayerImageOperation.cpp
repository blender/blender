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
#  include "IMB_imbuf.h"
#  include "IMB_imbuf_types.h"
}

MultilayerBaseOperation::MultilayerBaseOperation(int passindex) : BaseImageOperation()
{
	this->m_passId = passindex;
}
ImBuf *MultilayerBaseOperation::getImBuf()
{
	RenderPass *rpass = (RenderPass *)BLI_findlink(&this->m_renderlayer->passes, this->m_passId);
	if (rpass) {
		this->m_imageUser->pass = m_passId;
		BKE_image_multilayer_index(this->m_image->rr, this->m_imageUser);
		return BaseImageOperation::getImBuf();
	}
	return NULL;
}

void MultilayerColorOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	if (this->m_imageFloatBuffer == NULL) {
		zero_v4(output);
	}
	else {
		if (this->m_numberOfChannels == 4) {
			switch (sampler) {
				case COM_PS_NEAREST:
					nearest_interpolation_color(this->m_buffer, NULL, output, x, y);
					break;
				case COM_PS_BILINEAR:
					bilinear_interpolation_color(this->m_buffer, NULL, output, x - 0.5f, y - 0.5f);
					break;
				case COM_PS_BICUBIC:
					bicubic_interpolation_color(this->m_buffer, NULL, output, x - 0.5f, y - 0.5f);
					break;
			}
		}
		else {
			int yi = y;
			int xi = x;
			if (xi < 0 || yi < 0 || (unsigned int)xi >= this->getWidth() || (unsigned int)yi >= this->getHeight())
				zero_v4(output);
			else {
				int offset = (yi * this->getWidth() + xi) * 3;
				copy_v3_v3(output, &this->m_imageFloatBuffer[offset]);
			}
		}
	}
}

void MultilayerValueOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	if (this->m_imageFloatBuffer == NULL) {
		output[0] = 0.0f;
	}
	else {
		int yi = y;
		int xi = x;
		if (xi < 0 || yi < 0 || (unsigned int)xi >= this->getWidth() || (unsigned int)yi >= this->getHeight())
			output[0] = 0.0f;
		else {
			float result = this->m_imageFloatBuffer[yi * this->getWidth() + xi];
			output[0] = result;
		}
	}
}

void MultilayerVectorOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	if (this->m_imageFloatBuffer == NULL) {
		output[0] = 0.0f;
	}
	else {
		int yi = y;
		int xi = x;
		if (xi < 0 || yi < 0 || (unsigned int)xi >= this->getWidth() || (unsigned int)yi >= this->getHeight())
			output[0] = 0.0f;
		else {
			int offset = (yi * this->getWidth() + xi) * 3;
			copy_v3_v3(output, &this->m_imageFloatBuffer[offset]);
		}
	}
}
