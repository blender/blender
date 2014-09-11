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

#ifndef _COM_MovieDistortionOperation_h_
#define _COM_MovieDistortionOperation_h_

#include "COM_NodeOperation.h"
#include "DNA_movieclip_types.h"
#include "MEM_guardedalloc.h"

extern "C" {
#  include "BKE_tracking.h"
#  include "PIL_time.h"
}

#define COM_DISTORTIONCACHE_MAXSIZE 10

class DistortionCache {
private:
	float m_k1;
	float m_k2;
	float m_k3;
	float m_principal_x;
	float m_principal_y;
	float m_pixel_aspect;
	int m_width;
	int m_height;
	int m_calibration_width;
	int m_calibration_height;
	bool m_inverted;
	float *m_buffer;
	int *m_bufferCalculated;
	double timeLastUsage;
	int m_margin[2];
	
public:
	DistortionCache(MovieClip *movieclip,
	                int width, int height,
	                int calibration_width, int calibration_height,
	                bool inverted,
	                const int margin[2]) {
		this->m_k1 = movieclip->tracking.camera.k1;
		this->m_k2 = movieclip->tracking.camera.k2;
		this->m_k3 = movieclip->tracking.camera.k3;
		this->m_principal_x = movieclip->tracking.camera.principal[0];
		this->m_principal_y = movieclip->tracking.camera.principal[1];
		this->m_pixel_aspect = movieclip->tracking.camera.pixel_aspect;
		this->m_width = width;
		this->m_height = height;
		this->m_calibration_width = calibration_width;
		this->m_calibration_height = calibration_height;
		this->m_inverted = inverted;
		this->m_bufferCalculated = (int *)MEM_callocN(sizeof(int) * this->m_width * this->m_height, __func__);
		this->m_buffer = (float *)MEM_mallocN(sizeof(float) * this->m_width * this->m_height * 2, __func__);
		copy_v2_v2_int(this->m_margin, margin);
		this->updateLastUsage();
	}
	
	~DistortionCache() {
		if (this->m_buffer) {
			MEM_freeN(this->m_buffer);
			this->m_buffer = NULL;
		}
		
		if (this->m_bufferCalculated) {
			MEM_freeN(this->m_bufferCalculated);
			this->m_bufferCalculated = NULL;
		}
	}
	
	void updateLastUsage() {
		this->timeLastUsage = PIL_check_seconds_timer();
	}
	
	inline double getTimeLastUsage() {
		return this->timeLastUsage;
	}

	bool isCacheFor(MovieClip *movieclip,
	                int width, int height,
	                int calibration_width, int claibration_height,
	                bool inverted) {
		return this->m_k1 == movieclip->tracking.camera.k1 &&
		       this->m_k2 == movieclip->tracking.camera.k2 &&
		       this->m_k3 == movieclip->tracking.camera.k3 &&
		       this->m_principal_x == movieclip->tracking.camera.principal[0] &&
		       this->m_principal_y == movieclip->tracking.camera.principal[1] &&
		       this->m_pixel_aspect == movieclip->tracking.camera.pixel_aspect &&
		       this->m_inverted == inverted &&
		       this->m_width == width &&
		       this->m_height == height &&
		       this->m_calibration_width == calibration_width &&
		       this->m_calibration_height == claibration_height;
	}
	
	void getUV(MovieTracking *trackingData, int x, int y, float *u, float *v)
	{
		if (x < 0 || x >= this->m_width || y < 0 || y >= this->m_height) {
			*u = x;
			*v = y;
		}
		else {
			int offset = y * this->m_width + x;
			int offset2 = offset * 2;

			if (!this->m_bufferCalculated[offset]) {
				//float overscan = 0.0f;
				const float w = (float)this->m_width /* / (1 + overscan) */;
				const float h = (float)this->m_height /* / (1 + overscan) */;
				const float aspx = w / (float)this->m_calibration_width;
				const float aspy = h / (float)this->m_calibration_height;
				float in[2];
				float out[2];

				in[0] = (x /* - 0.5 * overscan * w */) / aspx;
				in[1] = (y /* - 0.5 * overscan * h */) / aspy / this->m_pixel_aspect;

				if (this->m_inverted) {
					BKE_tracking_undistort_v2(trackingData, in, out);
				}
				else {
					BKE_tracking_distort_v2(trackingData, in, out);
				}

				this->m_buffer[offset2] = out[0] * aspx /* + 0.5 * overscan * w */;
				this->m_buffer[offset2 + 1] = (out[1] * aspy /* + 0.5 * overscan * h */) * this->m_pixel_aspect;

				this->m_bufferCalculated[offset] = 1;
			}
			*u = this->m_buffer[offset2];
			*v = this->m_buffer[offset2 + 1];
		}
	}

	void getMargin(int margin[2])
	{
		copy_v2_v2_int(margin, m_margin);
	}
};

class MovieDistortionOperation : public NodeOperation {
private:
	DistortionCache *m_cache;
	SocketReader *m_inputOperation;
	MovieClip *m_movieClip;
	int m_margin[2];

protected:
	bool m_distortion;
	int m_framenumber;

public:
	MovieDistortionOperation(bool distortion);
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);

	void initExecution();
	void deinitExecution();
	
	void setMovieClip(MovieClip *clip) { this->m_movieClip = clip; }
	void setFramenumber(int framenumber) { this->m_framenumber = framenumber; }
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);

};

void deintializeDistortionCache(void);

#endif
