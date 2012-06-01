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
extern "C" {
	#include "BKE_tracking.h"
}

class DistortionCache {
private:
	float k1;
	float k2;
	float k3;
	float principal_x;
	float principal_y;
	float pixel_aspect;
	int width;
	int height;
	int calibration_width;
	int calibration_height;
	bool inverted;
	float *buffer;
	int *bufferCalculated;
public:
	DistortionCache(MovieClip *movieclip, int width, int height, int calibration_width, int calibration_height, bool inverted) {
		this->k1 = movieclip->tracking.camera.k1;
		this->k2 = movieclip->tracking.camera.k2;
		this->k3 = movieclip->tracking.camera.k3;
		this->principal_x = movieclip->tracking.camera.principal[0];
		this->principal_y = movieclip->tracking.camera.principal[1];
		this->pixel_aspect = movieclip->tracking.camera.pixel_aspect;
		this->width = width;
		this->height = height;
		this->calibration_width = calibration_width;
		this->calibration_height = calibration_height;
		this->inverted = inverted;
		this->bufferCalculated = new int[this->width*this->height];
		this->buffer = new float[this->width*this->height*2];
		for (int i = 0 ; i < this->width*this->height ; i ++) {
			this->bufferCalculated[i] = 0;
		}
	}
	bool isCacheFor(MovieClip *movieclip, int width, int height, int calibration_width, int claibration_height, bool inverted) {
		return this->k1 == movieclip->tracking.camera.k1 &&
			this->k2 == movieclip->tracking.camera.k2 &&
			this->k3 == movieclip->tracking.camera.k3 &&
			this->principal_x == movieclip->tracking.camera.principal[0] &&
			this->principal_y == movieclip->tracking.camera.principal[1] &&
			this->pixel_aspect == movieclip->tracking.camera.pixel_aspect &&
			this->inverted == inverted &&
			this->width == width &&
			this->height == height &&
			this->calibration_width == calibration_width &&
			this->calibration_height == calibration_height;
	}
	
	void getUV(MovieTracking *trackingData, int x, int y, float *u, float*v) {
		if (x<0 || x >= this->width || y <0 || y >= this->height) {
			*u = x;
			*v = y;
		}
		else {
			int offset = y * this->width + x;
			int offset2 = offset * 2;

			if (!bufferCalculated[offset]) {
				//float overscan = 0.0f;
				float w = (float)this->width/* / (1 + overscan) */;
				float h = (float)this->height/* / (1 + overscan) */;
				float aspx = (float)w / this->calibration_width;
				float aspy = (float)h / this->calibration_height;
				float in[2];
				float out[2];

				in[0] = (x /* - 0.5 * overscan * w */) / aspx;
				in[1] = (y /* - 0.5 * overscan * h */) / aspy / this->pixel_aspect;

				if (inverted) {
					BKE_tracking_invert_intrinsics(trackingData, in, out);
				}
				else {
					BKE_tracking_apply_intrinsics(trackingData, in, out);
				}

				buffer[offset2] = out[0] * aspx /* + 0.5 * overscan * w */;
				buffer[offset2+1] = (out[1] * aspy /* + 0.5 * overscan * h */) * this->pixel_aspect;

				bufferCalculated[offset] = 1;
			}
			*u = buffer[offset2];
			*v = buffer[offset2+1];
		}
	}
};

class MovieDistortionOperation: public NodeOperation {
private:
	DistortionCache *cache;
	SocketReader *inputOperation;
	MovieClip * movieClip;

protected:
	bool distortion;
	int framenumber;

public:
	MovieDistortionOperation(bool distortion);
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
	void executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]);

	void initExecution();
	void deinitExecution();
	
	void setMovieClip(MovieClip *clip) {this->movieClip = clip;}
	void setFramenumber(int framenumber) {this->framenumber = framenumber;}
};

#endif
