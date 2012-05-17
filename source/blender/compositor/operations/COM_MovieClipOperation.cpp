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
#include "DNA_scene_types.h"
#include "BLI_math.h"
extern "C" {
	#include "BKE_movieclip.h"
	#include "IMB_imbuf.h"
}
#include "BKE_image.h"

MovieClipOperation::MovieClipOperation(): NodeOperation() {
	this->addOutputSocket(COM_DT_COLOR);
	this->movieClip = NULL;
	this->movieClipBuffer = NULL;
	this->movieClipUser = NULL;
	this->movieClipwidth = 0;
	this->movieClipheight = 0;
	this->framenumber = 0;
}


void MovieClipOperation::initExecution() {
	if (this->movieClip) {
		BKE_movieclip_user_set_frame(this->movieClipUser, this->framenumber);
		ImBuf *ibuf;
		ibuf= BKE_movieclip_get_ibuf(this->movieClip, this->movieClipUser);
		if (ibuf) {
			this->movieClipBuffer = ibuf;
			if (ibuf->rect_float == NULL || ibuf->userflags&IB_RECT_INVALID) {
				IMB_float_from_rect(ibuf);
				ibuf->userflags&= ~IB_RECT_INVALID;
			}
		}
	}
}

void MovieClipOperation::deinitExecution() {
	this->movieClipBuffer = NULL;
}

void MovieClipOperation::determineResolution(unsigned int resolution[], unsigned int preferredResolution[]) {
	ImBuf *ibuf;
	if (this->movieClip) {
		ibuf= BKE_movieclip_get_ibuf(this->movieClip, this->movieClipUser);
		if (ibuf) {
			resolution[0] = ibuf->x;
			resolution[1] = ibuf->y;
		}
	}
}

void MovieClipOperation::executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	if (this->movieClipBuffer == NULL || x < 0 || y < 0 || x >= this->getWidth() || y >= this->getHeight() ) {
		color[0] = 0.0f;
		color[1] = 0.0f;
		color[2] = 0.0f;
		color[3] = 0.0f;
	}
	else {
		switch (sampler) {
		case COM_PS_NEAREST:
			neareast_interpolation_color(this->movieClipBuffer, NULL, color, x, y);
			break;
		case COM_PS_BILINEAR:
			bilinear_interpolation_color(this->movieClipBuffer, NULL, color, x, y);
			break;
		case COM_PS_BICUBIC:
			bicubic_interpolation_color(this->movieClipBuffer, NULL, color, x, y);
			break;
		}
	}
}
