/*
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/tracking.c
 *  \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "DNA_movieclip_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_listbase.h"

#include "BKE_tracking.h"

void BKE_tracking_clamp_track(MovieTrackingTrack *track, int event)
{
	int a;

	/* sort */
	for(a= 0; a<2; a++) {
		if(track->pat_min[a]>track->pat_max[a])
			SWAP(float, track->pat_min[a], track->pat_max[a]);

		if(track->search_min[a]>track->search_max[a])
			SWAP(float, track->search_min[a], track->search_max[a]);
	}

	if(event==CLAMP_PAT_DIM) {
		for(a= 0; a<2; a++) {
			/* pattern shouldn't be resized bigger than search */
			track->pat_min[a]= MAX2(track->pat_min[a], track->search_min[a]);
			track->pat_max[a]= MIN2(track->pat_max[a], track->search_max[a]);
		}
	}
	else if(event==CLAMP_PAT_POS) {
		float dim[2];
		sub_v2_v2v2(dim, track->pat_max, track->pat_min);

		for(a= 0; a<2; a++) {
			/* pattern shouldn't be moved outside of search */
			if(track->pat_min[a] < track->search_min[a]) {
				track->pat_min[a]= track->search_min[a];
				track->pat_max[a]= track->pat_min[a]+dim[a];
			}
			if(track->pat_max[a] > track->search_max[a]) {
				track->pat_max[a]= track->search_max[a];
				track->pat_min[a]= track->pat_max[a]-dim[a];
			}

			/* marker's center should be inside pattern */
			if(track->pat_min[a] > 0.0f) {
				track->pat_min[a]= 0.0f;
				track->pat_max[a]= dim[a];
			}
			if(track->pat_max[a] < 0.0f) {
				track->pat_max[a]= 0.0f;
				track->pat_min[a]= -dim[a];
			}
		}
	}
	else if(event==CLAMP_SEARCH_DIM) {
		for(a= 0; a<2; a++) {
			/* search shouldn't be resized smaller than pattern */
			track->search_min[a]= MIN2(track->pat_min[a], track->search_min[a]);
			track->search_max[a]= MAX2(track->pat_max[a], track->search_max[a]);
		}
	}
	else if(event==CLAMP_SEARCH_POS) {
		float dim[2];
		sub_v2_v2v2(dim, track->search_max, track->search_min);

		for(a= 0; a<2; a++) {
			/* search shouldn't be moved inside pattern */
			if(track->search_min[a] > track->pat_min[a]) {
				track->search_min[a]= track->pat_min[a];
				track->search_max[a]= track->search_min[a]+dim[a];
			}
			if(track->search_max[a] < track->pat_max[a]) {
				track->search_max[a]= track->pat_max[a];
				track->search_min[a]= track->search_max[a]-dim[a];
			}
		}
	}
}

void BKE_tracking_track_flag(MovieTrackingTrack *track, int area, int flag, int clear)
{
	if(area==TRACK_AREA_NONE)
		return;

	if(clear) {
		if(area&TRACK_AREA_POINT)	track->flag&= ~flag;
		if(area&TRACK_AREA_PAT)		track->pat_flag&= ~flag;
		if(area&TRACK_AREA_SEARCH)	track->search_flag&= ~flag;
	} else {
		if(area&TRACK_AREA_POINT)	track->flag|= flag;
		if(area&TRACK_AREA_PAT)		track->pat_flag|= flag;
		if(area&TRACK_AREA_SEARCH)	track->search_flag|= flag;
	}
}

void BKE_tracking_insert_marker(MovieTrackingTrack *track, MovieTrackingMarker *marker)
{
	track->markersnr++;

	if(track->markers) track->markers= MEM_reallocN(track->markers, sizeof(MovieTrackingMarker)*track->markersnr);
	else track->markers= MEM_callocN(sizeof(MovieTrackingMarker), "MovieTracking markers");

	track->markers[track->markersnr-1]= *marker;
}

MovieTrackingMarker *BKE_tracking_get_marker(MovieTrackingTrack *track, int framenr)
{
	/* XXX: silly, but ok before markers would be tracked */
	return &track->markers[0];
}

int BKE_tracking_has_marker(MovieTrackingTrack *track, int framenr)
{
	return BKE_tracking_get_marker(track, framenr) != 0;
}

void BKE_tracking_free_track(MovieTrackingTrack *track)
{
	if(track->markers) MEM_freeN(track->markers);
}

void BKE_tracking_free(MovieTracking *tracking)
{
	MovieTrackingTrack *track;

	for(track= tracking->tracks.first; track; track= track->next) {
		BKE_tracking_free_track(track);
	}

	BLI_freelistN(&tracking->tracks);
	BLI_freelistN(&tracking->bundles);
}
