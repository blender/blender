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

#include "COM_MovieClipOperation.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
extern "C" {
#  include "BKE_movieclip.h"
#  include "IMB_imbuf.h"
}
#include "BKE_image.h"

MovieClipBaseOperation::MovieClipBaseOperation() : NodeOperation()
{
	this->m_movieClip = NULL;
	this->m_movieClipBuffer = NULL;
	this->m_movieClipUser = NULL;
	this->m_movieClipwidth = 0;
	this->m_movieClipheight = 0;
	this->m_framenumber = 0;
}


void MovieClipBaseOperation::initExecution()
{
	if (this->m_movieClip) {
		BKE_movieclip_user_set_frame(this->m_movieClipUser, this->m_framenumber);
		ImBuf *ibuf;

		if (this->m_cacheFrame)
			ibuf = BKE_movieclip_get_ibuf(this->m_movieClip, this->m_movieClipUser);
		else
			ibuf = BKE_movieclip_get_ibuf_flag(this->m_movieClip, this->m_movieClipUser, this->m_movieClip->flag, MOVIECLIP_CACHE_SKIP);

		if (ibuf) {
			this->m_movieClipBuffer = ibuf;
			if (ibuf->rect_float == NULL || ibuf->userflags & IB_RECT_INVALID) {
				IMB_float_from_rect(ibuf);
				ibuf->userflags &= ~IB_RECT_INVALID;
			}
		}
	}
}

void MovieClipBaseOperation::deinitExecution()
{
	if (this->m_movieClipBuffer) {
		IMB_freeImBuf(this->m_movieClipBuffer);

		this->m_movieClipBuffer = NULL;
	}
}

void MovieClipBaseOperation::determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2])
{
	resolution[0] = 0;
	resolution[1] = 0;

	if (this->m_movieClip) {
		int width, height;

		BKE_movieclip_get_size(this->m_movieClip, this->m_movieClipUser, &width, &height);

		resolution[0] = width;
		resolution[1] = height;
	}
}

void MovieClipBaseOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	ImBuf *ibuf = this->m_movieClipBuffer;

	if (ibuf == NULL) {
		zero_v4(output);
	}
	else if (ibuf->rect == NULL && ibuf->rect_float == NULL) {
		/* Happens for multilayer exr, i.e. */
		zero_v4(output);
	}
	else {
		switch (sampler) {
			case COM_PS_NEAREST:
				nearest_interpolation_color(ibuf, NULL, output, x, y);
				break;
			case COM_PS_BILINEAR:
				bilinear_interpolation_color(ibuf, NULL, output, x - 0.5f, y - 0.5f);
				break;
			case COM_PS_BICUBIC:
				bicubic_interpolation_color(ibuf, NULL, output, x - 0.5f, y - 0.5f);
				break;
		}
	}
}

MovieClipOperation::MovieClipOperation() : MovieClipBaseOperation()
{
	this->addOutputSocket(COM_DT_COLOR);
}

MovieClipAlphaOperation::MovieClipAlphaOperation() : MovieClipBaseOperation()
{
	this->addOutputSocket(COM_DT_VALUE);
}

void MovieClipAlphaOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	MovieClipBaseOperation::executePixelSampled(output, x, y, sampler);
	output[0] = output[3];
	output[1] = 0.0f;
	output[2] = 0.0f;
	output[3] = 0.0f;
}
