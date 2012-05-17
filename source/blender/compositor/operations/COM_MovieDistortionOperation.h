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
	int width;
	int height;
	bool inverted;
	float *buffer;
	int *bufferCalculated;
public:
	DistortionCache(MovieClip* movieclip, int width, int height, bool inverted) {
		this->k1 = movieclip->tracking.camera.k1;
		this->k2 = movieclip->tracking.camera.k2;
		this->k3 = movieclip->tracking.camera.k3;
		this->width = width;
		this->height = height;
		this->inverted = inverted;
		this->bufferCalculated = new int[this->width*this->height];
		this->buffer = new float[this->width*this->height*2];
		for (int i = 0 ; i < this->width*this->height ; i ++) {
			this->bufferCalculated[i] = 0;
		}
	}
	bool isCacheFor(MovieClip* movieclip, int width, int height, bool inverted) {
		return this->k1 == movieclip->tracking.camera.k1 &&
			this->k2 == movieclip->tracking.camera.k2 &&
			this->k3 == movieclip->tracking.camera.k3 &&
			this->inverted == inverted &&
			this->width == width && 
			this->height == height;
	}
	
	void getUV(MovieTracking* trackingData, int x, int y, float *u, float*v) {
		if (x<0 || x >= this->width || y <0 || y >= this->height) {
			*u = x;
			*v = y;
		} else {
			int offset = y * this->width + x;
			int offset2 = offset*2;
			if (!bufferCalculated[offset]) {
				float in[2];
				float out[2];
				in[0] = x;
				in[1] = y;
				if (inverted) {
					BKE_tracking_invert_intrinsics(trackingData, in, out);
				} else {
					BKE_tracking_apply_intrinsics(trackingData, in, out);
				}
				buffer[offset2] = out[0];
				buffer[offset2+1] = out[1];
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

public:
	MovieDistortionOperation(bool distortion);
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
	void executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]);

	void initExecution();
	void deinitExecution();
	
	void setMovieClip(MovieClip* clip) {this->movieClip = clip;}

};

#endif
