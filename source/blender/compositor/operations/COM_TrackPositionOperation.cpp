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

#include "COM_TrackPositionOperation.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_math_color.h"

#include "DNA_scene_types.h"

extern "C" {
	#include "BKE_movieclip.h"
	#include "BKE_tracking.h"
}

TrackPositionOperation::TrackPositionOperation() : NodeOperation()
{
	this->addOutputSocket(COM_DT_VALUE);
	this->movieClip = NULL;
	this->framenumber = 0;
	this->trackingObject[0] = 0;
	this->trackName[0] = 0;
	this->axis = 0;
	this->relative = false;
}

void TrackPositionOperation::executePixel(float *outputValue, float x, float y, PixelSampler sampler)
{
	MovieClipUser user = {0};
	MovieTracking *tracking = &movieClip->tracking;
	MovieTrackingObject *object = BKE_tracking_object_get_named(tracking, this->trackingObject);
	MovieTrackingTrack *track;
	MovieTrackingMarker *marker;
	int width, height;

	outputValue[0] = 0.0f;

	if (!object)
		return;

	track = BKE_tracking_track_get_named(tracking, object, this->trackName);

	if (!track)
		return;

	BKE_movieclip_user_set_frame(&user, this->framenumber);
	BKE_movieclip_get_size(this->movieClip, &user, &width, &height);

	marker = BKE_tracking_marker_get(track, this->framenumber);

	outputValue[0] = marker->pos[this->axis];

	if (this->relative) {
		int i;

		for (i = 0; i < track->markersnr; i++) {
			marker = &track->markers[i];

			if ((marker->flag & MARKER_DISABLED) == 0) {
				outputValue[0] -= marker->pos[this->axis];

				break;
			}
		}
	}

	if (this->axis == 0)
		outputValue[0] *= width;
	else
		outputValue[0] *= height;
}

void TrackPositionOperation::determineResolution(unsigned int resolution[], unsigned int preferredResolution[])
{
	resolution[0] = preferredResolution[0];
	resolution[1] = preferredResolution[1];
}
