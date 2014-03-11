/*
 * Copyright 2013, Blender Foundation.
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
 *		Sergey Sharybin
 */

#include "COM_PlaneTrackOperation.h"
#include "COM_ReadBufferOperation.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_math_color.h"

extern "C" {
#  include "BLI_jitter.h"

#  include "BKE_movieclip.h"
#  include "BKE_node.h"
#  include "BKE_tracking.h"
}

/* ******** PlaneTrackCommon ******** */

PlaneTrackCommon::PlaneTrackCommon()
{
	this->m_movieClip = NULL;
	this->m_framenumber = 0;
	this->m_trackingObjectName[0] = '\0';
	this->m_planeTrackName[0] = '\0';
}

void PlaneTrackCommon::readCornersFromTrack(float corners[4][2])
{
	MovieTracking *tracking;
	MovieTrackingObject *object;

	if (!this->m_movieClip)
		return;
	
	tracking = &this->m_movieClip->tracking;
	
	object = BKE_tracking_object_get_named(tracking, this->m_trackingObjectName);
	if (object) {
		MovieTrackingPlaneTrack *plane_track;
		
		plane_track = BKE_tracking_plane_track_get_named(tracking, object, this->m_planeTrackName);
		
		if (plane_track) {
			MovieTrackingPlaneMarker *plane_marker;
			int clip_framenr = BKE_movieclip_remap_scene_to_clip_frame(this->m_movieClip, this->m_framenumber);
			
			plane_marker = BKE_tracking_plane_marker_get(plane_track, clip_framenr);
			copy_v2_v2(corners[0], plane_marker->corners[0]);
			copy_v2_v2(corners[1], plane_marker->corners[1]);
			copy_v2_v2(corners[2], plane_marker->corners[2]);
			copy_v2_v2(corners[3], plane_marker->corners[3]);
		}
	}
}

void PlaneTrackCommon::determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2])
{
	resolution[0] = 0;
	resolution[1] = 0;
	
	if (this->m_movieClip) {
		int width, height;
		MovieClipUser user = {0};
		
		BKE_movieclip_user_set_frame(&user, this->m_framenumber);
		BKE_movieclip_get_size(this->m_movieClip, &user, &width, &height);
		
		resolution[0] = width;
		resolution[1] = height;
	}
}


/* ******** PlaneTrackMaskOperation ******** */

void PlaneTrackMaskOperation::initExecution()
{
	PlaneDistortMaskOperation::initExecution();
	
	float corners[4][2];
	readCornersFromTrack(corners);
	calculateCorners(corners, true);
}

/* ******** PlaneTrackWarpImageOperation ******** */

void PlaneTrackWarpImageOperation::initExecution()
{
	PlaneDistortWarpImageOperation::initExecution();
	
	float corners[4][2];
	readCornersFromTrack(corners);
	calculateCorners(corners, true);
	calculatePerspectiveMatrix();
}
