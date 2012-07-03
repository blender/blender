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

#include "COM_MovieClipAttributeOperation.h"
extern "C" {
	#include "BKE_tracking.h"
	#include "BKE_movieclip.h"
}
MovieClipAttributeOperation::MovieClipAttributeOperation() : NodeOperation()
{
	this->addOutputSocket(COM_DT_VALUE);
	this->m_valueSet = false;
	this->m_framenumber = 0;
	this->m_attribute = MCA_X;
}

void MovieClipAttributeOperation::executePixel(float *outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[])
{
	if (!this->m_valueSet) {
		float loc[2], scale, angle;
		loc[0] = 0.0f;
		loc[1] = 0.0f;
		scale = 1.0f;
		angle = 0.0f;
		if (this->m_clip) {
			int clip_framenr = BKE_movieclip_remap_scene_to_clip_frame(this->m_clip, this->m_framenumber);
			BKE_tracking_stabilization_data_get(&this->m_clip->tracking, clip_framenr, getWidth(), getHeight(), loc, &scale, &angle);
		}
		switch (this->m_attribute) {
			case MCA_SCALE:
				this->m_value = scale;
				break;
			case MCA_ANGLE:
				this->m_value = angle;
				break;
			case MCA_X:
				this->m_value = loc[0];
				break;
			case MCA_Y:
				this->m_value = loc[1];
				break;
		}
		this->m_valueSet = true;
	}
	outputValue[0] = this->m_value;
}

void MovieClipAttributeOperation::determineResolution(unsigned int resolution[], unsigned int preferredResolution[])
{
	resolution[0] = preferredResolution[0];
	resolution[1] = preferredResolution[1];
}

