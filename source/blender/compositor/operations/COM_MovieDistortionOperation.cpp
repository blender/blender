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

#include "COM_MovieDistortionOperation.h"

extern "C" {
	#include "BKE_tracking.h"
	#include "BKE_movieclip.h"
	#include "BLI_linklist.h"
}


vector<DistortionCache *> s_cache;

void deintializeDistortionCache(void) 
{
	while (s_cache.size() > 0) {
		DistortionCache * cache = s_cache.back();
		s_cache.pop_back();
		delete cache;
	}
}

MovieDistortionOperation::MovieDistortionOperation(bool distortion) : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_COLOR);
	this->setResolutionInputSocketIndex(0);
	this->m_inputOperation = NULL;
	this->m_movieClip = NULL;
	this->m_cache = NULL;
	this->m_distortion = distortion;
}

void MovieDistortionOperation::initExecution()
{
	this->m_inputOperation = this->getInputSocketReader(0);
	if (this->m_movieClip) {
		MovieClipUser clipUser = {0};
		int calibration_width, calibration_height;

		BKE_movieclip_user_set_frame(&clipUser, this->m_framenumber);
		BKE_movieclip_get_size(this->m_movieClip, &clipUser, &calibration_width, &calibration_height);

		for (unsigned int i = 0; i < s_cache.size(); i++) {
			DistortionCache *c = (DistortionCache *)s_cache[i];
			if (c->isCacheFor(this->m_movieClip, this->m_width, this->m_height,
			                  calibration_width, calibration_height, this->m_distortion))
			{
				this->m_cache = c;
				this->m_cache->updateLastUsage();
				return;
			}
		}
		DistortionCache *newC = new DistortionCache(this->m_movieClip, this->m_width, this->m_height,
		                                            calibration_width, calibration_height, this->m_distortion);
		s_cache.push_back(newC);
		this->m_cache = newC;
	}
	else {
		this->m_cache = NULL;
	}
}

void MovieDistortionOperation::deinitExecution()
{
	this->m_inputOperation = NULL;
	this->m_movieClip = NULL;
	while (s_cache.size() > COM_DISTORTIONCACHE_MAXSIZE) {
		double minTime = PIL_check_seconds_timer();
		vector<DistortionCache*>::iterator minTimeIterator = s_cache.begin();
		for (vector<DistortionCache*>::iterator it = s_cache.begin(); it < s_cache.end(); it ++) {
			DistortionCache * cache = *it;
			if (cache->getTimeLastUsage() < minTime) {
				minTime = cache->getTimeLastUsage();
				minTimeIterator = it;
			}
		}
		s_cache.erase(minTimeIterator);
	}
}


void MovieDistortionOperation::executePixel(float output[4], float x, float y, PixelSampler sampler)
{
	
	if (this->m_cache != NULL) {
		float u, v;
		this->m_cache->getUV(&this->m_movieClip->tracking, x, y, &u, &v);
		this->m_inputOperation->read(output, u, v, COM_PS_BILINEAR);
	}
	else {
		this->m_inputOperation->read(output, x, y, COM_PS_BILINEAR);
	}
}
