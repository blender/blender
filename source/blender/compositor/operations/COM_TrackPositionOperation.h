/*
 * Copyright 2012, Blender Foundation.
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
 *		Sergey Sharybin
 */


#ifndef _COM_TrackPositionOperation_h
#define _COM_TrackPositionOperation_h

#include <string.h>

#include "COM_NodeOperation.h"

#include "DNA_scene_types.h"
#include "DNA_movieclip_types.h"

#include "BLI_listbase.h"

/**
  * Class with implementation of green screen gradient rasterization
  */
class TrackPositionOperation : public NodeOperation {
protected:
	MovieClip *movieClip;
	int framenumber;
	char trackingObject[64];
	char trackName[64];
	int axis;
	bool relative;

	/**
	  * Determine the output resolution. The resolution is retrieved from the Renderer
	  */
	void determineResolution(unsigned int resolution[], unsigned int preferredResolution[]);

public:
	TrackPositionOperation();

	void setMovieClip(MovieClip *clip) {this->movieClip = clip;}
	void setTrackingObject(char *object) {strncpy(this->trackingObject, object, sizeof(this->trackingObject));}
	void setTrackName(char *track) {strncpy(this->trackName, track, sizeof(this->trackName));}
	void setFramenumber(int framenumber) {this->framenumber = framenumber;}
	void setAxis(int value) {this->axis = value;}
	void setRelative(bool value) {this->relative = value;}

	void executePixel(float *color, float x, float y, PixelSampler sampler);

	const bool isSetOperation() const { return true; }
};

#endif
