/*
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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
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

#include <stddef.h>
#include <limits.h>
#include <math.h>
#include <memory.h>

#include "MEM_guardedalloc.h"

#include "DNA_gpencil_types.h"
#include "DNA_camera_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_object_types.h"	/* SELECT */
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_math_base.h"
#include "BLI_listbase.h"
#include "BLI_ghash.h"
#include "BLI_path_util.h"

#include "BKE_global.h"
#include "BKE_tracking.h"
#include "BKE_movieclip.h"
#include "BKE_object.h"
#include "BKE_scene.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#ifdef WITH_LIBMV
#include "libmv-capi.h"
#endif

typedef struct MovieDistortion {
	struct libmv_CameraIntrinsics *intrinsics;
} MovieDistortion;

/*********************** common functions *************************/

void BKE_tracking_clamp_track(MovieTrackingTrack *track, int event)
{
	int a;
	float pat_min[2];
	float pat_max[2];
	float max_pyramid_level_factor = 1.0;
	if (track->tracker == TRACKER_KLT) {
		max_pyramid_level_factor = 1 << (track->pyramid_levels - 1);
	}

	/* sort */
	for(a= 0; a<2; a++) {
		if(track->pat_min[a]>track->pat_max[a])
			SWAP(float, track->pat_min[a], track->pat_max[a]);

		if(track->search_min[a]>track->search_max[a])
			SWAP(float, track->search_min[a], track->search_max[a]);
	}

	/* compute the effective pattern size, which differs from the fine resolution
	 * pattern size for the pyramid KLT tracker */
	for(a= 0; a<2; a++) {
		pat_min[a] = max_pyramid_level_factor * track->pat_min[a];
		pat_max[a] = max_pyramid_level_factor * track->pat_max[a];
	}

	if(event==CLAMP_PAT_DIM) {
		for(a= 0; a<2; a++) {
			/* search shouldn't be resized smaller than pattern */
			track->search_min[a]= MIN2(pat_min[a], track->search_min[a]);
			track->search_max[a]= MAX2(pat_max[a], track->search_max[a]);
		}
	}
	else if(event==CLAMP_PAT_POS) {
		float dim[2];
		sub_v2_v2v2(dim, track->pat_max, pat_min);

		for(a= 0; a<2; a++) {
			/* pattern shouldn't be moved outside of search */
			if(pat_min[a] < track->search_min[a]) {
				track->pat_min[a]= track->search_min[a] - (pat_min[a] - track->pat_min[a]);
				track->pat_max[a]=  (pat_min[a] - track->pat_min[a])+dim[a];
			}
			if(track->pat_max[a] > track->search_max[a]) {
				track->pat_max[a]= track->search_max[a] - (pat_max[a] - track->pat_max[a]);
				track->pat_min[a]= track->pat_max[a]-dim[a] - (pat_min[a] - track->pat_min[a]);
			}
		}
	}
	else if(event==CLAMP_SEARCH_DIM) {
		float max_pyramid_level_factor = 1.0;
		if (track->tracker == TRACKER_KLT) {
			max_pyramid_level_factor = 1 << (track->pyramid_levels - 1);
		}
		for(a= 0; a<2; a++) {
			/* search shouldn't be resized smaller than pattern */
			track->search_min[a]= MIN2(pat_min[a], track->search_min[a]);
			track->search_max[a]= MAX2(pat_max[a], track->search_max[a]);
		}
	}
	else if(event==CLAMP_SEARCH_POS) {
		float dim[2];
		sub_v2_v2v2(dim, track->search_max, track->search_min);

		for(a= 0; a<2; a++) {
			/* search shouldn't be moved inside pattern */
			if(track->search_min[a] > pat_min[a]) {
				track->search_min[a]= pat_min[a];
				track->search_max[a]= track->search_min[a]+dim[a];
			}
			if(track->search_max[a] < pat_max[a]) {
				track->search_max[a]= pat_max[a];
				track->search_min[a]= track->search_max[a]-dim[a];
			}
		}
	}

	else if(event==CLAMP_PYRAMID_LEVELS || (event==CLAMP_SEARCH_DIM && track->tracker == TRACKER_KLT))  {
		float dim[2];
		sub_v2_v2v2(dim, track->pat_max, track->pat_min);
		{
			float search_ratio = 2.3f * max_pyramid_level_factor;

			/* resize the search area to something sensible based
			 * on the number of pyramid levels */
			for(a= 0; a<2; a++) {
				track->search_min[a]= search_ratio * track->pat_min[a];
				track->search_max[a]= search_ratio * track->pat_max[a];
			}
		}
	}

	/* marker's center should be in center of pattern */
	if(event==CLAMP_PAT_DIM || event==CLAMP_PAT_POS) {
		float dim[2];
		sub_v2_v2v2(dim, track->pat_max, track->pat_min);

		for(a= 0; a<2; a++) {
			track->pat_min[a]= -dim[a]/2.0f;
			track->pat_max[a]= dim[a]/2.0f;
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

MovieTrackingTrack *BKE_tracking_add_track(MovieTracking *tracking, float x, float y,
			int framenr, int width, int height)
{
	MovieTrackingTrack *track;
	MovieTrackingMarker marker;

	/* pick reasonable defaults */
	float pat[2]= {5.5f, 5.5f}, search[2]= {25.5f, 25.5f}; /* TODO: move to default setting? */

	pat[0] /= (float)width;
	pat[1] /= (float)height;

	search[0] /= (float)width;
	search[1] /= (float)height;

	track= MEM_callocN(sizeof(MovieTrackingTrack), "add_marker_exec track");
	strcpy(track->name, "Track");

	/* default to KLT tracker */
	track->tracker = TRACKER_KLT;
	track->pyramid_levels = 2;

	/* set SAD defaults even though it's not selected by default */
	track->minimum_correlation= 0.75f;

	memset(&marker, 0, sizeof(marker));
	marker.pos[0]= x;
	marker.pos[1]= y;
	marker.framenr= framenr;

	copy_v2_v2(track->pat_max, pat);
	negate_v2_v2(track->pat_min, pat);

	copy_v2_v2(track->search_max, search);
	negate_v2_v2(track->search_min, search);

	BKE_tracking_insert_marker(track, &marker);

	BLI_addtail(&tracking->tracks, track);
	BKE_track_unique_name(tracking, track);

	return track;
}

void BKE_tracking_insert_marker(MovieTrackingTrack *track, MovieTrackingMarker *marker)
{
	MovieTrackingMarker *old_marker= BKE_tracking_get_marker(track, marker->framenr);

	if(old_marker && old_marker->framenr==marker->framenr) {
		*old_marker= *marker;
	} else {
		int a= track->markersnr;

		while(a--) {
			if(track->markers[a].framenr<marker->framenr)
				break;
		}

		track->markersnr++;

		if(track->markers) track->markers= MEM_reallocN(track->markers, sizeof(MovieTrackingMarker)*track->markersnr);
		else track->markers= MEM_callocN(sizeof(MovieTrackingMarker), "MovieTracking markers");

		memmove(track->markers+a+2, track->markers+a+1, (track->markersnr-a-2)*sizeof(MovieTrackingMarker));
		track->markers[a+1]= *marker;

		track->last_marker= a+1;
	}
}

void BKE_tracking_delete_marker(MovieTrackingTrack *track, int framenr)
{
	int a= 0;

	while(a<track->markersnr) {
		if(track->markers[a].framenr==framenr) {
			if(track->markersnr>1) {
				memmove(track->markers+a, track->markers+a+1, (track->markersnr-a-1)*sizeof(MovieTrackingMarker));
				track->markersnr--;
				track->markers= MEM_reallocN(track->markers, sizeof(MovieTrackingMarker)*track->markersnr);
			} else {
				MEM_freeN(track->markers);
				track->markers= NULL;
				track->markersnr= 0;
			}

			break;
		}

		a++;
	}
}

MovieTrackingMarker *BKE_tracking_get_marker(MovieTrackingTrack *track, int framenr)
{
	int a= track->markersnr-1;

	if(!track->markersnr)
		return NULL;

	/* approximate pre-first framenr marker with first marker */
	if(framenr<track->markers[0].framenr)
		return &track->markers[0];

	if(track->last_marker<track->markersnr)
		a= track->last_marker;

	if(track->markers[a].framenr<=framenr) {
		while(a<track->markersnr && track->markers[a].framenr<=framenr) {
			if(track->markers[a].framenr==framenr) {
				track->last_marker= a;
				return &track->markers[a];
			}
			a++;
		}

		/* if there's no marker for exact position, use nearest marker from left side */
		return &track->markers[a-1];
	} else {
		while(a>=0 && track->markers[a].framenr>=framenr) {
			if(track->markers[a].framenr==framenr) {
				track->last_marker= a;
				return &track->markers[a];
			}

			a--;
		}

		/* if there's no marker for exact position, use nearest marker from left side */
		return &track->markers[a];
	}

	return NULL;
}

MovieTrackingMarker *BKE_tracking_ensure_marker(MovieTrackingTrack *track, int framenr)
{
	MovieTrackingMarker *marker= BKE_tracking_get_marker(track, framenr);

	if(marker && marker->framenr!=framenr) {
		MovieTrackingMarker marker_new;

		marker_new= *marker;
		marker_new.framenr= framenr;

		BKE_tracking_insert_marker(track, &marker_new);
		marker= BKE_tracking_get_marker(track, framenr);
	}

	return marker;
}

MovieTrackingMarker *BKE_tracking_exact_marker(MovieTrackingTrack *track, int framenr)
{
	MovieTrackingMarker *marker= BKE_tracking_get_marker(track, framenr);

	if(marker && marker->framenr!=framenr)
		return NULL;

	return marker;
}

int BKE_tracking_has_marker(MovieTrackingTrack *track, int framenr)
{
	return BKE_tracking_exact_marker(track, framenr) != 0;
}

void BKE_tracking_free_track(MovieTrackingTrack *track)
{
	if(track->markers) MEM_freeN(track->markers);
}

MovieTrackingTrack *BKE_tracking_copy_track(MovieTrackingTrack *track)
{
	MovieTrackingTrack *new_track= MEM_dupallocN(track);

	new_track->next= new_track->prev= NULL;

	if(new_track->markers)
		new_track->markers= MEM_dupallocN(new_track->markers);

	return new_track;
}

static void put_disabled_marker(MovieTrackingTrack *track, MovieTrackingMarker *ref_marker, int before, int overwrite)
{
	MovieTrackingMarker marker_new;

	marker_new= *ref_marker;
	marker_new.flag&= ~MARKER_TRACKED;
	marker_new.flag|= MARKER_DISABLED;

	if(before) marker_new.framenr--;
	else marker_new.framenr++;

	if(!BKE_tracking_has_marker(track, marker_new.framenr) || overwrite)
		BKE_tracking_insert_marker(track, &marker_new);
}

void BKE_tracking_clear_path(MovieTrackingTrack *track, int ref_frame, int action)
{
	int a;

	if(action==TRACK_CLEAR_REMAINED) {
		a= 1;
		while(a<track->markersnr) {
			if(track->markers[a].framenr>ref_frame) {
				track->markersnr= a;
				track->markers= MEM_reallocN(track->markers, sizeof(MovieTrackingMarker)*track->markersnr);

				break;
			}

			a++;
		}

		if(track->markersnr)
			put_disabled_marker(track, &track->markers[track->markersnr-1], 0, 1);
	} else if(action==TRACK_CLEAR_UPTO) {
		a= track->markersnr-1;
		while(a>=0) {
			if(track->markers[a].framenr<=ref_frame) {
				memmove(track->markers, track->markers+a, (track->markersnr-a)*sizeof(MovieTrackingMarker));

				track->markersnr= track->markersnr-a;
				track->markers= MEM_reallocN(track->markers, sizeof(MovieTrackingMarker)*track->markersnr);

				break;
			}

			a--;
		}

		if(track->markersnr)
			put_disabled_marker(track, &track->markers[0], 1, 1);
	} else if(action==TRACK_CLEAR_ALL) {
		MovieTrackingMarker *marker, marker_new;

		marker= BKE_tracking_get_marker(track, ref_frame);
		marker_new= *marker;

		MEM_freeN(track->markers);
		track->markers= NULL;
		track->markersnr= 0;

		BKE_tracking_insert_marker(track, &marker_new);

		put_disabled_marker(track, &marker_new, 1, 1);
		put_disabled_marker(track, &marker_new, 0, 1);
	}
}

int BKE_tracking_test_join_tracks(MovieTrackingTrack *dst_track, MovieTrackingTrack *src_track)
{
	int i, a= 0, b= 0, tot= dst_track->markersnr+src_track->markersnr;
	int count= 0;

	for(i= 0; i<tot; i++) {
		if(a>=src_track->markersnr) {
			b++;
			count++;
		}
		else if(b>=dst_track->markersnr) {
			a++;
			count++;
		}
		else if(src_track->markers[a].framenr<dst_track->markers[b].framenr) {
			a++;
			count++;
		} else if(src_track->markers[a].framenr>dst_track->markers[b].framenr) {
			b++;
			count++;
		} else {
			if((src_track->markers[a].flag&MARKER_DISABLED)==0 && (dst_track->markers[b].flag&MARKER_DISABLED)==0)
				return 0;

			a++;
			b++;
			count++;
		}
	}

	return count;
}

void BKE_tracking_join_tracks(MovieTrackingTrack *dst_track, MovieTrackingTrack *src_track)
{
	int i, a= 0, b= 0, tot;
	MovieTrackingMarker *markers;

	tot= BKE_tracking_test_join_tracks(dst_track, src_track);

	markers= MEM_callocN(tot*sizeof(MovieTrackingMarker), "tracking joined tracks");

	for(i= 0; i<tot; i++) {
		if(b>=dst_track->markersnr) {
			markers[i]= src_track->markers[a++];
		}
		else if(a>=src_track->markersnr) {
			markers[i]= dst_track->markers[b++];
		}
		else if(src_track->markers[a].framenr<dst_track->markers[b].framenr) {
			markers[i]= src_track->markers[a++];
		} else if(src_track->markers[a].framenr>dst_track->markers[b].framenr) {
			markers[i]= dst_track->markers[b++];
		} else {
			if((src_track->markers[a].flag&MARKER_DISABLED)) markers[i]= dst_track->markers[b];
			else markers[i]= src_track->markers[a++];

			a++;
			b++;
		}
	}

	MEM_freeN(dst_track->markers);

	dst_track->markers= markers;
	dst_track->markersnr= tot;
}

void BKE_tracking_free(MovieTracking *tracking)
{
	MovieTrackingTrack *track;

	for(track= tracking->tracks.first; track; track= track->next) {
		BKE_tracking_free_track(track);
	}

	BLI_freelistN(&tracking->tracks);

	if(tracking->reconstruction.cameras)
		MEM_freeN(tracking->reconstruction.cameras);

	if(tracking->stabilization.scaleibuf)
		IMB_freeImBuf(tracking->stabilization.scaleibuf);

	if(tracking->camera.intrinsics)
		BKE_tracking_distortion_destroy(tracking->camera.intrinsics);
}

/*********************** tracking *************************/

typedef struct TrackContext {
	MovieTrackingTrack *track;

#ifdef WITH_LIBMV
	float keyframed_pos[2];

	/* ** KLT tracker ** */
	struct libmv_RegionTracker *region_tracker;
	float *patch;			/* keyframed patch */

	/* ** SAD tracker ** */
	int patsize;			/* size of pattern (currently only 16x16 due to libmv side) */
	unsigned char *pattern;	/* keyframed pattern */
	unsigned char *warped;	/* warped version of reference */
#endif
} TrackContext;

typedef struct MovieTrackingContext {
	MovieClipUser user;
	MovieClip *clip;

	int first_time, frames;

	TrackContext *track_context;
	int num_tracks;

	GHash *hash;
	MovieTrackingSettings settings;

	short backwards, disable_failed;
	int sync_frame;
} MovieTrackingContext;

MovieTrackingContext *BKE_tracking_context_new(MovieClip *clip, MovieClipUser *user, short backwards, short disable_failed)
{
	MovieTrackingContext *context= MEM_callocN(sizeof(MovieTrackingContext), "trackingContext");
	MovieTracking *tracking= &clip->tracking;
	MovieTrackingSettings *settings= &tracking->settings;
	MovieTrackingTrack *track;
	TrackContext *track_context;

	context->settings= *settings;
	context->backwards= backwards;
	context->disable_failed= disable_failed;
	context->hash= BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "tracking trackHash");
	context->sync_frame= user->framenr;
	context->first_time= 1;

	/* count */
	track= tracking->tracks.first;
	while(track) {
		if(TRACK_SELECTED(track) && (track->flag&TRACK_LOCKED)==0) {
			MovieTrackingMarker *marker= BKE_tracking_get_marker(track, user->framenr);

			if((marker->flag&MARKER_DISABLED)==0)
				context->num_tracks++;
		}

		track= track->next;
	}

	if(context->num_tracks) {
		int width, height;

		BKE_movieclip_get_size(clip, user, &width, &height);

		/* create tracking data */
		context->track_context= MEM_callocN(sizeof(TrackContext)*context->num_tracks, "tracking track_context");

		track_context= context->track_context;
		track= tracking->tracks.first;
		while(track) {
			if(TRACK_SELECTED(track) && (track->flag&TRACK_LOCKED)==0) {
				MovieTrackingMarker *marker= BKE_tracking_get_marker(track, user->framenr);

				if((marker->flag&MARKER_DISABLED)==0) {
					MovieTrackingTrack *new_track= BKE_tracking_copy_track(track);

					track_context->track= new_track;
#ifdef WITH_LIBMV
					{
						if(track_context->track->tracker==TRACKER_KLT) {
							float search_size_x= (track->search_max[0]-track->search_min[0])*width;
							float search_size_y= (track->search_max[1]-track->search_min[1])*height;
							float pattern_size_x= (track->pat_max[0]-track->pat_min[0])*width;
							float pattern_size_y= (track->pat_max[1]-track->pat_min[1])*height;

							/* compute the maximum pyramid size */
							double search_to_pattern_ratio= MIN2(search_size_x,  search_size_y)
								/ MAX2(pattern_size_x, pattern_size_y);
							double log2_search_to_pattern_ratio = log(floor(search_to_pattern_ratio)) / M_LN2;
							int max_pyramid_levels= floor(log2_search_to_pattern_ratio + 1);

							/* try to accomodate the user's choice of pyramid level in a way
							 * that doesn't cause the coarsest pyramid pattern to be larger
							 * than the search size */
							int level= MIN2(track_context->track->pyramid_levels, max_pyramid_levels);
							track_context->region_tracker= libmv_regionTrackerNew(100, level);
						}
						else if(track_context->track->tracker==TRACKER_SAD) {
							/* nothing to initialize */
						}
					}
#endif

					BLI_ghash_insert(context->hash, new_track, track);

					track_context++;
				}
			}

			track= track->next;
		}
	}

	context->clip= clip;
	context->user= *user;

	return context;
}

void BKE_tracking_context_free(MovieTrackingContext *context)
{
	int a;
	TrackContext *track_context;

	for(a= 0, track_context= context->track_context; a<context->num_tracks; a++, track_context++) {
		BKE_tracking_free_track(context->track_context[a].track);

#if WITH_LIBMV
		if(track_context->region_tracker)
			libmv_regionTrackerDestroy(track_context->region_tracker);

		if(track_context->patch)
			MEM_freeN(track_context->patch);

		if(track_context->pattern)
			MEM_freeN(track_context->pattern);

		if(track_context->warped)
			MEM_freeN(track_context->warped);
#endif

		MEM_freeN(track_context->track);
	}

	if(context->track_context)
		MEM_freeN(context->track_context);

	BLI_ghash_free(context->hash, NULL, NULL);

	MEM_freeN(context);
}

static void disable_imbuf_channels(ImBuf *ibuf, MovieTrackingTrack *track)
{
	int x, y;

	if((track->flag&(TRACK_DISABLE_RED|TRACK_DISABLE_GREEN|TRACK_DISABLE_BLUE))==0)
		return;

	for(y= 0; y<ibuf->y; y++) {
		for (x= 0; x<ibuf->x; x++) {
			int pixel= ibuf->x*y + x;

			if(ibuf->rect_float) {
				float *rrgbf= ibuf->rect_float + pixel*4;

				if(track->flag&TRACK_DISABLE_RED)	rrgbf[0]= 0;
				if(track->flag&TRACK_DISABLE_GREEN)	rrgbf[1]= 0;
				if(track->flag&TRACK_DISABLE_BLUE)	rrgbf[2]= 0;
			} else {
				char *rrgb= (char*)ibuf->rect + pixel*4;

				if(track->flag&TRACK_DISABLE_RED)	rrgb[0]= 0;
				if(track->flag&TRACK_DISABLE_GREEN)	rrgb[1]= 0;
				if(track->flag&TRACK_DISABLE_BLUE)	rrgb[2]= 0;
			}
		}
	}
}

static ImBuf *get_area_imbuf(ImBuf *ibuf, MovieTrackingTrack *track, MovieTrackingMarker *marker,
			float min[2], float max[2], int margin, int anchored, float pos[2], int origin[2])
{
	ImBuf *tmpibuf;
	int x, y;
	int x1, y1, x2, y2, w, h;
	float mpos[2];

	copy_v2_v2(mpos, marker->pos);
	if(anchored)
		add_v2_v2(mpos, track->offset);

	x= mpos[0]*ibuf->x;
	y= mpos[1]*ibuf->y;
	x1= x-(int)(-min[0]*ibuf->x);
	y1= y-(int)(-min[1]*ibuf->y);
	x2= x+(int)(max[0]*ibuf->x);
	y2= y+(int)(max[1]*ibuf->y);

	/* dimensions should be odd */
	w= (x2-x1)|1;
	h= (y2-y1)|1;

	tmpibuf= IMB_allocImBuf(w+margin*2, h+margin*2, 32, IB_rect);
	IMB_rectcpy(tmpibuf, ibuf, 0, 0, x1-margin, y1-margin, w+margin*2, h+margin*2);

	if(pos != NULL) {
		pos[0]= mpos[0]*ibuf->x-x1+margin;
		pos[1]= mpos[1]*ibuf->y-y1+margin;
	}

	if(origin != NULL) {
		origin[0]= x1-margin;
		origin[1]= y1-margin;
	}

	disable_imbuf_channels(tmpibuf, track);

	return tmpibuf;
}

ImBuf *BKE_tracking_get_pattern_imbuf(ImBuf *ibuf, MovieTrackingTrack *track, MovieTrackingMarker *marker,
			int margin, int anchored, float pos[2], int origin[2])
{
	return get_area_imbuf(ibuf, track, marker, track->pat_min, track->pat_max, margin, anchored, pos, origin);
}

ImBuf *BKE_tracking_get_search_imbuf(ImBuf *ibuf, MovieTrackingTrack *track, MovieTrackingMarker *marker,
			int margin, int anchored, float pos[2], int origin[2])
{
	return get_area_imbuf(ibuf, track, marker, track->search_min, track->search_max, margin, anchored, pos, origin);
}

#ifdef WITH_LIBMV
static float *get_search_floatbuf(ImBuf *ibuf, MovieTrackingTrack *track, MovieTrackingMarker *marker,
			int *width_r, int *height_r, float pos[2], int origin[2])
{
	ImBuf *tmpibuf;
	float *pixels, *fp;
	int x, y, width, height;

	width= (track->search_max[0]-track->search_min[0])*ibuf->x;
	height= (track->search_max[1]-track->search_min[1])*ibuf->y;

	tmpibuf= BKE_tracking_get_search_imbuf(ibuf, track, marker, 0, 0, pos, origin);
	disable_imbuf_channels(tmpibuf, track);

	*width_r= width;
	*height_r= height;

	fp= pixels= MEM_callocN(width*height*sizeof(float), "tracking floatBuf");
	for(y= 0; y<(int)height; y++) {
		for (x= 0; x<(int)width; x++) {
			int pixel= tmpibuf->x*y + x;

			if(tmpibuf->rect_float) {
				float *rrgbf= tmpibuf->rect_float + pixel*4;

				*fp= 0.2126*rrgbf[0] + 0.7152*rrgbf[1] + 0.0722*rrgbf[2];
			} else {
				unsigned char *rrgb= (unsigned char*)tmpibuf->rect + pixel*4;

				*fp= (0.2126*rrgb[0] + 0.7152*rrgb[1] + 0.0722*rrgb[2])/255.0f;
			}

			fp++;
		}
	}

	IMB_freeImBuf(tmpibuf);

	return pixels;
}

static unsigned char *get_ucharbuf(ImBuf *ibuf)
{
	int x, y;
	unsigned char *pixels, *cp;

	cp= pixels= MEM_callocN(ibuf->x*ibuf->y*sizeof(unsigned char), "tracking ucharBuf");
	for(y= 0; y<ibuf->y; y++) {
		for (x= 0; x<ibuf->x; x++) {
			int pixel= ibuf->x*y + x;

			if(ibuf->rect_float) {
				float *rrgbf= ibuf->rect_float + pixel*4;

				*cp= FTOCHAR(0.2126f*rrgbf[0] + 0.7152f*rrgbf[1] + 0.0722f*rrgbf[2]);
			} else {
				unsigned char *rrgb= (unsigned char*)ibuf->rect + pixel*4;

				*cp= 0.2126f*rrgb[0] + 0.7152f*rrgb[1] + 0.0722f*rrgb[2];
			}

			cp++;
		}
	}

	return pixels;
}

static unsigned char *get_search_bytebuf(ImBuf *ibuf, MovieTrackingTrack *track, MovieTrackingMarker *marker,
			int *width_r, int *height_r, float pos[2], int origin[2])
{
	ImBuf *tmpibuf;
	unsigned char *pixels;

	tmpibuf= BKE_tracking_get_search_imbuf(ibuf, track, marker, 0, 0, pos, origin);
	disable_imbuf_channels(tmpibuf, track);

	*width_r= tmpibuf->x;
	*height_r= tmpibuf->y;

	pixels= get_ucharbuf(tmpibuf);

	IMB_freeImBuf(tmpibuf);

	return pixels;
}

static ImBuf *get_frame_ibuf(MovieTrackingContext *context, int framenr)
{
	ImBuf *ibuf;
	int framenr_old= context->user.framenr;

	context->user.framenr= framenr;

	ibuf= BKE_movieclip_get_ibuf_flag(context->clip, &context->user, 0);

	context->user.framenr= framenr_old;

	return ibuf;
}

static ImBuf *get_keyframed_ibuf(MovieTrackingContext *context, MovieTrackingTrack *track,
			MovieTrackingMarker *marker, MovieTrackingMarker **marker_keyed)
{
	int framenr= marker->framenr;
	int a= marker-track->markers;

	*marker_keyed= marker;

	while(a>=0 && a<track->markersnr) {
		int next= (context->backwards) ? a+1 : a-1;
		int is_keyframed= 0;
		MovieTrackingMarker *marker= &track->markers[a];
		MovieTrackingMarker *next_marker= NULL;

		if(next>=0 && next<track->markersnr)
			next_marker= &track->markers[next];

		/* if next mrker is disabled, stop searching keyframe and use current frame as keyframe */
		if(next_marker && next_marker->flag&MARKER_DISABLED)
			is_keyframed= 1;

		is_keyframed|= (marker->flag&MARKER_TRACKED)==0;

		if(is_keyframed) {
			framenr= marker->framenr;
			*marker_keyed= marker;
			break;
		}

		a= next;
	}

	return get_frame_ibuf(context, framenr);
}

static ImBuf *get_adjust_ibuf(MovieTrackingContext *context, MovieTrackingTrack *track, MovieTrackingMarker *marker,
			int curfra, MovieTrackingMarker **marker_keyed)
{
	ImBuf *ibuf= NULL;

	if(context->settings.adjframes == 0) {
		ibuf= get_keyframed_ibuf(context, track, marker, marker_keyed);
	} else {
		ibuf= get_frame_ibuf(context, curfra);

		/* use current marker as keyframed position */
		*marker_keyed= marker;
	}

	return ibuf;
}

static void get_warped(TrackContext *track_context, int x, int y, int width, unsigned char *image)
{
	int i, j;

	for(i=0; i<track_context->patsize; i++) {
		for(j=0; j<track_context->patsize; j++) {
			track_context->warped[i*track_context->patsize+j]=
					image[(y+i-track_context->patsize/2)*width+x+j-track_context->patsize/2];
		}
	}
}

#endif

void BKE_tracking_sync(MovieTrackingContext *context)
{
	TrackContext *track_context;
	MovieTracking *tracking= &context->clip->tracking;
	MovieTrackingTrack *track;
	ListBase tracks= {NULL, NULL}, new_tracks= {NULL, NULL};
	ListBase *old_tracks= &context->clip->tracking.tracks;
	int a, newframe;

	/* duplicate currently tracking tracks to temporary list.
	   this is needed to keep names in unique state and it's faster to change names
	   of currently tracking tracks (if needed) */
	for(a= 0, track_context= context->track_context; a<context->num_tracks; a++, track_context++) {
		int replace_sel= 0;
		MovieTrackingTrack *new_track, *old;

		track= track_context->track;

		/* find original of tracking track in list of previously displayed tracks */
		old= BLI_ghash_lookup(context->hash, track);
		if(old) {
			MovieTrackingTrack *cur= old_tracks->first;

			while(cur) {
				if(cur==old)
					break;

				cur= cur->next;
			}

			/* original track was found, re-use flags and remove this track */
			if(cur) {
				if(cur==tracking->act_track)
					replace_sel= 1;

				track->flag= cur->flag;
				track->pat_flag= cur->pat_flag;
				track->search_flag= cur->search_flag;

				BKE_tracking_free_track(cur);
				BLI_freelinkN(old_tracks, cur);
			}
		}

		new_track= BKE_tracking_copy_track(track);

		BLI_ghash_remove(context->hash, track, NULL, NULL); /* XXX: are we actually need this */
		BLI_ghash_insert(context->hash, track, new_track);

		if(replace_sel)		/* update current selection in clip */
			tracking->act_track= new_track;

		BLI_addtail(&tracks, new_track);
	}

	/* move all tracks, which aren't tracking */
	track= old_tracks->first;
	while(track) {
		MovieTrackingTrack *next= track->next;

		track->next= track->prev= NULL;
		BLI_addtail(&new_tracks, track);

		track= next;
	}

	/* now move all tracks which are currently tracking and keep their names unique */
	track= tracks.first;
	while(track) {
		MovieTrackingTrack *next= track->next;

		BLI_remlink(&tracks, track);

		track->next= track->prev= NULL;
		BLI_addtail(&new_tracks, track);

		BLI_uniquename(&new_tracks, track, "Track", '.', offsetof(MovieTrackingTrack, name), sizeof(track->name));

		track= next;
	}

	context->clip->tracking.tracks= new_tracks;

	if(context->backwards) newframe= context->user.framenr+1;
	else newframe= context->user.framenr-1;

	context->sync_frame= newframe;
}

void BKE_tracking_sync_user(MovieClipUser *user, MovieTrackingContext *context)
{
	user->framenr= context->sync_frame;
}

int BKE_tracking_next(MovieTrackingContext *context)
{
	ImBuf *ibuf_new;
	int curfra= context->user.framenr;
	int a, ok= 0;

	/* nothing to track, avoid unneeded frames reading to save time and memory */
	if(!context->num_tracks)
		return 0;

	if(context->backwards) context->user.framenr--;
	else context->user.framenr++;

	ibuf_new= BKE_movieclip_get_ibuf_flag(context->clip, &context->user, 0);
	if(!ibuf_new)
		return 0;

	#pragma omp parallel for private(a) shared(ibuf_new, ok) if(context->num_tracks>1)
	for(a= 0; a<context->num_tracks; a++) {
		TrackContext *track_context= &context->track_context[a];
		MovieTrackingTrack *track= track_context->track;
		MovieTrackingMarker *marker= BKE_tracking_get_marker(track, curfra);

		if(marker && (marker->flag&MARKER_DISABLED)==0 && marker->framenr==curfra) {
#ifdef WITH_LIBMV
			int width, height, origin[2], tracked= 0, need_readjust= 0;
			float pos[2], margin[2];
			double x1, y1, x2, y2;
			ImBuf *ibuf= NULL;
			MovieTrackingMarker marker_new, *marker_keyed;
			int onbound= 0, coords_correct= 0;
			int nextfra;

			if(!context->settings.adjframes) need_readjust= context->first_time;
			else need_readjust= context->frames%context->settings.adjframes == 0;

			if(context->backwards) nextfra= curfra-1;
			else nextfra= curfra+1;

			/* margin from frame boundaries */
			sub_v2_v2v2(margin, track->pat_max, track->pat_min);

			margin[0]= MAX2(margin[0], (float)context->settings.margin / ibuf_new->x);
			margin[1]= MAX2(margin[1], (float)context->settings.margin / ibuf_new->y);

			/* do not track markers which are too close to boundary */
			if(marker->pos[0]<margin[0] || marker->pos[0]>1.0f-margin[0] ||
			   marker->pos[1]<margin[1] || marker->pos[1]>1.0f-margin[1]) {
				onbound= 1;
			}
			else if(track_context->track->tracker==TRACKER_KLT) {
				int wndx, wndy;
				float *patch_new;

				if(need_readjust) {
					/* calculate patch for keyframed position */
					ibuf= get_adjust_ibuf(context, track, marker, curfra, &marker_keyed);

					if(track_context->patch)
						MEM_freeN(track_context->patch);

					track_context->patch= get_search_floatbuf(ibuf, track, marker_keyed, &width, &height, track_context->keyframed_pos, origin);

					IMB_freeImBuf(ibuf);
				}

				patch_new= get_search_floatbuf(ibuf_new, track, marker, &width, &height, pos, origin);

				x1= track_context->keyframed_pos[0];
				y1= track_context->keyframed_pos[1];

				x2= pos[0];
				y2= pos[1];

				wndx= (int)((track->pat_max[0]-track->pat_min[0])*ibuf_new->x)/2;
				wndy= (int)((track->pat_max[1]-track->pat_min[1])*ibuf_new->y)/2;

				tracked= libmv_regionTrackerTrack(track_context->region_tracker, track_context->patch, patch_new,
							width, height, MAX2(wndx, wndy), x1, y1, &x2, &y2);

				MEM_freeN(patch_new);
			}
			else if(track_context->track->tracker==TRACKER_SAD) {
				unsigned char *image_new;
				float correlation;
				float warp[3][2]={{0}};

				if(need_readjust) {
					unsigned char *image;

					/* calculate pattern for keyframed position */
					ibuf= get_adjust_ibuf(context, track, marker, curfra, &marker_keyed);

					image= get_search_bytebuf(ibuf, track, marker_keyed, &width, &height, pos, origin);

					memset(warp, 0, sizeof(warp));
					warp[0][0]= 1;
					warp[1][1]= 1;
					warp[2][0]= pos[0];
					warp[2][1]= pos[1];

					/* pattern size is hardcoded to 16x16px in libmv */
					track_context->patsize= 16;

					if(!track_context->pattern)
						track_context->pattern= MEM_callocN(sizeof(unsigned char)*track_context->patsize*track_context->patsize, "trackking pattern");

					libmv_SADSamplePattern(image, width, warp, track_context->pattern);

					MEM_freeN(image);
					IMB_freeImBuf(ibuf);
				}

				image_new= get_search_bytebuf(ibuf_new, track, marker, &width, &height, pos, origin);

				if(track_context->warped==NULL) {
					unsigned char *image_old;

					ibuf= get_frame_ibuf(context, curfra);

					if(track_context->warped==NULL)
						track_context->warped= MEM_callocN(sizeof(unsigned char)*track_context->patsize*track_context->patsize, "trackking warped");

					image_old= get_search_bytebuf(ibuf, track, marker, &width, &height, pos, origin);
					get_warped(track_context, pos[0], pos[1], width, image_old);
					IMB_freeImBuf(ibuf);
					MEM_freeN(image_old);
				}

				memset(warp, 0, sizeof(warp));
				warp[0][0]= 1;
				warp[1][1]= 1;
				warp[2][0]= pos[0];
				warp[2][1]= pos[1];

				correlation= libmv_SADTrackerTrack(track_context->pattern, track_context->warped, image_new, width, width, height, warp);

				x2= warp[2][0];
				y2= warp[2][1];

				tracked= track_context->track->minimum_correlation < correlation;

				if(tracked)
					get_warped(track_context, x2, y2, width, image_new);

				MEM_freeN(image_new);
			}

			coords_correct= !isnan(x2) && !isnan(y2) && finite(x2) && finite(y2);
			if(coords_correct && (tracked || !context->disable_failed)) {
				if(context->first_time) {
					#pragma omp critical
					{
						/* check if there's no keyframe/tracked markers before tracking marker.
						    if so -- create disabled marker before currently tracking "segment" */
						put_disabled_marker(track, marker, 1, 0);
					}
				}

				memset(&marker_new, 0, sizeof(marker_new));

				if(!onbound) {
					marker_new.pos[0]= (origin[0]+x2)/ibuf_new->x;
					marker_new.pos[1]= (origin[1]+y2)/ibuf_new->y;
				} else {
					copy_v2_v2(marker_new.pos, marker->pos);
				}

				marker_new.flag|= MARKER_TRACKED;
				marker_new.framenr= nextfra;

				#pragma omp critical
				{
					BKE_tracking_insert_marker(track, &marker_new);
				}

				/* make currently tracked segment be finished with disabled marker */
				#pragma omp critical
				{
					put_disabled_marker(track, &marker_new, 0, 0);
				}
			} else {
				marker_new= *marker;

				marker_new.framenr= nextfra;
				marker_new.flag|= MARKER_DISABLED;

				#pragma omp critical
				{
					BKE_tracking_insert_marker(track, &marker_new);
				}
			}

			ok= 1;
#endif
		}
	}

	IMB_freeImBuf(ibuf_new);

	context->first_time= 0;
	context->frames++;

	return ok;
}

#if WITH_LIBMV
static struct libmv_Tracks *create_libmv_tracks(MovieTracking *tracking, int width, int height)
{
	int tracknr= 0;
	MovieTrackingTrack *track;
	struct libmv_Tracks *tracks= libmv_tracksNew();

	track= tracking->tracks.first;
	while(track) {
		int a= 0;

		for(a= 0; a<track->markersnr; a++) {
			MovieTrackingMarker *marker= &track->markers[a];

			if((marker->flag&MARKER_DISABLED)==0)
				libmv_tracksInsert(tracks, marker->framenr, tracknr,
							marker->pos[0]*width, marker->pos[1]*height);
		}

		track= track->next;
		tracknr++;
	}

	return tracks;
}

static int retrieve_libmv_reconstruct(MovieTracking *tracking, struct libmv_Reconstruction *libmv_reconstruction)
{
	int tracknr= 0;
	int sfra= INT_MAX, efra= INT_MIN, a, origin_set= 0;
	MovieTrackingTrack *track;
	MovieTrackingReconstruction *reconstruction= &tracking->reconstruction;
	MovieReconstructedCamera *reconstructed;
	float origin[3]= {0.0f, 0.0f, 0.0f};
	int ok= 1;

	track= tracking->tracks.first;
	while(track) {
		double pos[3];

		if(libmv_reporojectionPointForTrack(libmv_reconstruction, tracknr, pos)) {
			track->bundle_pos[0]= pos[0];
			track->bundle_pos[1]= pos[1];
			track->bundle_pos[2]= pos[2];

			track->flag|= TRACK_HAS_BUNDLE;
			track->error= libmv_reporojectionErrorForTrack(libmv_reconstruction, tracknr);
		} else {
			track->flag&= ~TRACK_HAS_BUNDLE;
			ok= 0;

			printf("No bundle for track #%d '%s'\n", tracknr, track->name);
		}

		if(track->markersnr) {
			if(track->markers[0].framenr<sfra) sfra= track->markers[0].framenr;
			if(track->markers[track->markersnr-1].framenr>efra) efra= track->markers[track->markersnr-1].framenr;
		}

		track= track->next;
		tracknr++;
	}

	if(reconstruction->cameras)
		MEM_freeN(reconstruction->cameras);

	reconstruction->camnr= 0;
	reconstruction->cameras= NULL;
	reconstructed= MEM_callocN((efra-sfra+1)*sizeof(MovieReconstructedCamera), "temp reconstructed camera");

	for(a= sfra; a<=efra; a++) {
		double matd[4][4];

		if(libmv_reporojectionCameraForImage(libmv_reconstruction, a, matd)) {
			int i, j;
			float mat[4][4];
			float error= libmv_reporojectionErrorForImage(libmv_reconstruction, a);

			for(i=0; i<4; i++)
				for(j= 0; j<4; j++)
					mat[i][j]= matd[i][j];

			if(!origin_set) {
				copy_v3_v3(origin, mat[3]);
				origin_set= 1;
			}

			if(origin_set)
				sub_v3_v3(mat[3], origin);

			copy_m4_m4(reconstructed[reconstruction->camnr].mat, mat);
			reconstructed[reconstruction->camnr].framenr= a;
			reconstructed[reconstruction->camnr].error= error;
			reconstruction->camnr++;
		} else {
			ok= 0;
			printf("No camera for frame %d\n", a);
		}
	}

	if(reconstruction->camnr) {
		reconstruction->cameras= MEM_callocN(reconstruction->camnr*sizeof(MovieReconstructedCamera), "reconstructed camera");
		memcpy(reconstruction->cameras, reconstructed, reconstruction->camnr*sizeof(MovieReconstructedCamera));
	}

	if(origin_set) {
		track= tracking->tracks.first;
		while(track) {
			if(track->flag&TRACK_HAS_BUNDLE)
				sub_v3_v3(track->bundle_pos, origin);

			track= track->next;
		}
	}

	MEM_freeN(reconstructed);

	return ok;
}

#endif

float BKE_tracking_solve_reconstruction(MovieTracking *tracking, int width, int height)
{
#if WITH_LIBMV
	{
		MovieTrackingCamera *camera= &tracking->camera;
		float aspy= 1.0f/tracking->camera.pixel_aspect;
		struct libmv_Tracks *tracks= create_libmv_tracks(tracking, width, height*aspy);
		struct libmv_Reconstruction *reconstruction = libmv_solveReconstruction(tracks,
		        tracking->settings.keyframe1, tracking->settings.keyframe2,
		        camera->focal,
		        camera->principal[0], camera->principal[1]*aspy,
		        camera->k1, camera->k2, camera->k3);
		float error= libmv_reprojectionError(reconstruction);

		tracking->reconstruction.error= error;

		if(!retrieve_libmv_reconstruct(tracking, reconstruction))
			error= -1.0f;

		libmv_destroyReconstruction(reconstruction);
		libmv_tracksDestroy(tracks);

		tracking->reconstruction.flag|= TRACKING_RECONSTRUCTED;

		return error;
	}
#endif
}

void BKE_track_unique_name(MovieTracking *tracking, MovieTrackingTrack *track)
{
	BLI_uniquename(&tracking->tracks, track, "Track", '.', offsetof(MovieTrackingTrack, name), sizeof(track->name));
}

MovieTrackingTrack *BKE_find_track_by_name(MovieTracking *tracking, const char *name)
{
	MovieTrackingTrack *track= tracking->tracks.first;

	while(track) {
		if(!strcmp(track->name, name))
			return track;

		track= track->next;
	}

	return NULL;
}

static int reconstruction_camera_index(MovieTracking *tracking, int framenr, int nearest)
{
	MovieTrackingReconstruction *reconstruction= &tracking->reconstruction;
	MovieReconstructedCamera *cameras= reconstruction->cameras;
	int a= 0, d= 1;

	if(!reconstruction->camnr)
		return -1;

	if(framenr<cameras[0].framenr) {
		if(nearest) return 0;
		else return -1;
	}

	if(framenr>cameras[reconstruction->camnr-1].framenr) {
		if(nearest) return reconstruction->camnr-1;
		else return -1;
	}

	if(reconstruction->last_camera<reconstruction->camnr)
		a= reconstruction->last_camera;

	if(cameras[a].framenr>=framenr)
		d= -1;

	while(a>=0 && a<reconstruction->camnr) {
		int cfra= cameras[a].framenr;

		/* check if needed framenr was "skipped" -- no data for requested frame */

		if(d>0 && cfra>framenr) {
			/* interpolate with previous position */
			if(nearest) return a-1;
			else break;
		}

		if(d<0 && cfra<framenr) {
			/* interpolate with next position */
			if(nearest) return a;
			else break;
		}

		if(cfra==framenr) {
			reconstruction->last_camera= a;

			return a;
		}

		a+= d;
	}

	return -1;
}

MovieReconstructedCamera *BKE_tracking_get_reconstructed_camera(MovieTracking *tracking, int framenr)
{
	int a= reconstruction_camera_index(tracking, framenr, 0);

	if(a==-1)
		return NULL;

	return &tracking->reconstruction.cameras[a];
}

void BKE_tracking_get_interpolated_camera(MovieTracking *tracking, int framenr, float mat[4][4])
{
	MovieTrackingReconstruction *reconstruction= &tracking->reconstruction;
	MovieReconstructedCamera *cameras= reconstruction->cameras;
	int a= reconstruction_camera_index(tracking, framenr, 1);

	if(a==-1) {
		unit_m4(mat);
		return;
	}

	if(cameras[a].framenr!=framenr && a>0 && a<reconstruction->camnr-1) {
		float t= ((float)framenr-cameras[a].framenr) / (cameras[a+1].framenr-cameras[a].framenr);

		blend_m4_m4m4(mat, cameras[a].mat, cameras[a+1].mat, t);
	} else {
		copy_m4_m4(mat, cameras[a].mat);
	}
}

void BKE_get_tracking_mat(Scene *scene, Object *ob, float mat[4][4])
{
	if(!ob) {
		if(scene->camera) ob= scene->camera;
		else ob= scene_find_camera(scene);
	}

	if(ob)
		where_is_object_mat(scene, ob, mat);
	else
		unit_m4(mat);
}

void BKE_tracking_camera_shift(MovieTracking *tracking, int winx, int winy, float *shiftx, float *shifty)
{
	*shiftx= (0.5f*winx-tracking->camera.principal[0]) / winx;
	*shifty= (0.5f*winy-tracking->camera.principal[1]) / winx;
}

void BKE_tracking_camera_to_blender(MovieTracking *tracking, Scene *scene, Camera *camera, int width, int height)
{
	float focal= tracking->camera.focal;

	camera->sensor_x= tracking->camera.sensor_width;
	camera->sensor_fit= CAMERA_SENSOR_FIT_AUTO;
	camera->lens= focal*camera->sensor_x/width;

	scene->r.xsch= width*tracking->camera.pixel_aspect;
	scene->r.ysch= height;

	scene->r.xasp= 1.0f;
	scene->r.yasp= 1.0f;

	BKE_tracking_camera_shift(tracking, width, height, &camera->shiftx, &camera->shifty);
}

void BKE_tracking_projection_matrix(MovieTracking *tracking, int framenr, int winx, int winy, float mat[4][4])
{
	MovieReconstructedCamera *camera;
	float lens= tracking->camera.focal*tracking->camera.sensor_width/(float)winx;
	float viewfac, pixsize, left, right, bottom, top, clipsta, clipend;
	float winmat[4][4];
	float ycor= 1.0f/tracking->camera.pixel_aspect;
	float shiftx, shifty, winside= MAX2(winx, winy);

	BKE_tracking_camera_shift(tracking, winx, winy, &shiftx, &shifty);

	clipsta= 0.1f;
	clipend= 1000.0f;

	if(winx >= winy)
		viewfac= (lens*winx)/tracking->camera.sensor_width;
	else
		viewfac= (ycor*lens*winy)/tracking->camera.sensor_width;

	pixsize= clipsta/viewfac;

	left= -0.5f*(float)winx + shiftx*winside;
	bottom= -0.5f*(ycor)*(float)winy + shifty*winside;
	right=  0.5f*(float)winx + shiftx*winside;
	top=  0.5f*(ycor)*(float)winy + shifty*winside;

	left *= pixsize;
	right *= pixsize;
	bottom *= pixsize;
	top *= pixsize;

	perspective_m4(winmat, left, right, bottom, top, clipsta, clipend);

	camera= BKE_tracking_get_reconstructed_camera(tracking, framenr);
	if(camera) {
		float imat[4][4];

		invert_m4_m4(imat, camera->mat);
		mul_m4_m4m4(mat, imat, winmat);
	} else copy_m4_m4(mat, winmat);
}

void BKE_tracking_apply_intrinsics(MovieTracking *tracking, float co[2], float nco[2])
{
	MovieTrackingCamera *camera= &tracking->camera;

#ifdef WITH_LIBMV
	double x, y;
	float aspy= 1.0f/tracking->camera.pixel_aspect;

	/* normalize coords */
	x= (co[0]-camera->principal[0]) / camera->focal;
	y= (co[1]-camera->principal[1] * aspy) / camera->focal;

	libmv_applyCameraIntrinsics(camera->focal, camera->principal[0], camera->principal[1] * aspy,
				camera->k1, camera->k2, camera->k3, x, y, &x, &y);

	/* result is in image coords already */
	nco[0]= x;
	nco[1]= y;
#endif
}

void BKE_tracking_invert_intrinsics(MovieTracking *tracking, float co[2], float nco[2])
{
	MovieTrackingCamera *camera= &tracking->camera;

#ifdef WITH_LIBMV
	double x= co[0], y= co[1];
	float aspy= 1.0f/tracking->camera.pixel_aspect;

	libmv_InvertIntrinsics(camera->focal, camera->principal[0], camera->principal[1] * aspy,
				camera->k1, camera->k2, camera->k3, x, y, &x, &y);

	nco[0]= x * camera->focal + camera->principal[0];
	nco[1]= y * camera->focal + camera->principal[1] * aspy;
#endif
}

static int point_in_stroke(bGPDstroke *stroke, float x, float y)
{
	int i, prev;
	int count= 0;
	bGPDspoint *points= stroke->points;

	prev= stroke->totpoints-1;

	for(i= 0; i<stroke->totpoints; i++) {
		if((points[i].y<y && points[prev].y>=y) || (points[prev].y<y && points[i].y>=y)) {
			float fac= (y-points[i].y)/(points[prev].y-points[i].y);

			if (points[i].x+fac*(points[prev].x-points[i].x)<x)
				count++;
		}

		prev= i;
	}

	return count%2;
}

static int point_in_layer(bGPDlayer *layer, float x, float y)
{
	bGPDframe *frame= layer->frames.first;

	while(frame) {
		bGPDstroke *stroke= frame->strokes.first;
		while(stroke) {
			if(point_in_stroke(stroke, x, y))
				return 1;

			stroke= stroke->next;
		}
		frame= frame->next;
	}

	return 0;
}

static void retrieve_libmv_features(MovieTracking *tracking, struct libmv_Features *features,
			int framenr, int width, int height, bGPDlayer *layer, int place_outside_layer)
{
#ifdef WITH_LIBMV
	int a;

	a= libmv_countFeatures(features);
	while(a--) {
		MovieTrackingTrack *track;
		double x, y, size, score;
		int ok= 1;
		float xu, yu;

		libmv_getFeature(features, a, &x, &y, &score, &size);

		xu= x/width;
		yu= y/height;

		if(layer)
			ok= point_in_layer(layer, xu, yu)!=place_outside_layer;

		if(ok) {
			track= BKE_tracking_add_track(tracking, xu, yu, framenr, width, height);
			track->flag|= SELECT;
			track->pat_flag|= SELECT;
			track->search_flag|= SELECT;
		}
	}
#endif
}

void BKE_tracking_detect_fast(MovieTracking *tracking, ImBuf *ibuf,
			int framenr, int margin, int min_trackness, int min_distance, bGPDlayer *layer,
			int place_outside_layer)
{
#ifdef WITH_LIBMV
	struct libmv_Features *features;
	unsigned char *pixels= get_ucharbuf(ibuf);

	features= libmv_detectFeaturesFAST(pixels, ibuf->x, ibuf->y, ibuf->x, margin, min_trackness, min_distance);

	MEM_freeN(pixels);

	retrieve_libmv_features(tracking, features, framenr, ibuf->x, ibuf->y, layer, place_outside_layer);

	libmv_destroyFeatures(features);
#endif
}

MovieTrackingTrack *BKE_tracking_indexed_bundle(MovieTracking *tracking, int bundlenr)
{
	MovieTrackingTrack *track= tracking->tracks.first;
	int cur= 1;

	while(track) {
		if(track->flag&TRACK_HAS_BUNDLE) {
			if(cur==bundlenr)
				return track;

			cur++;
		}

		track= track->next;
	}

	return NULL;
}

static int stabilization_median_point(MovieTracking *tracking, int framenr, float median[2])
{
	int ok= 0;
	float min[2], max[2];
	MovieTrackingTrack *track;

	INIT_MINMAX2(min, max);

	track= tracking->tracks.first;
	while(track) {
		if(track->flag&TRACK_USE_2D_STAB) {
			MovieTrackingMarker *marker= BKE_tracking_get_marker(track, framenr);

			DO_MINMAX2(marker->pos, min, max);

			ok= 1;
		}

		track= track->next;
	}

	median[0]= (max[0]+min[0])/2.0f;
	median[1]= (max[1]+min[1])/2.0f;

	return ok;
}

static void calculate_stabdata(MovieTracking *tracking, int framenr, float width, float height,
			float firstmedian[2], float median[2], float loc[2], float *scale, float *angle)
{
	MovieTrackingStabilization *stab= &tracking->stabilization;

	*scale= (stab->scale-1.0f)*stab->scaleinf+1.0f;
	*angle= 0.0f;

	loc[0]= (firstmedian[0]-median[0])*width*(*scale);
	loc[1]= (firstmedian[1]-median[1])*height*(*scale);

	mul_v2_fl(loc, stab->locinf);

	if(stab->rot_track && stab->rotinf) {
		MovieTrackingMarker *marker;
		float a[2], b[2];
		float x0= (float)width/2.0f, y0= (float)height/2.0f;
		float x= median[0]*width, y= median[1]*height;

		marker= BKE_tracking_get_marker(stab->rot_track, 1);
		sub_v2_v2v2(a, marker->pos, firstmedian);
		a[0]*= width;
		a[1]*= height;

		marker= BKE_tracking_get_marker(stab->rot_track, framenr);
		sub_v2_v2v2(b, marker->pos, median);
		b[0]*= width;
		b[1]*= height;

		*angle= -atan2(a[0]*b[1]-a[1]*b[0], a[0]*b[0]+a[1]*b[1]);
		*angle*= stab->rotinf;

		/* convert to rotation around image center */
		loc[0]-= (x0 + (x-x0)*cos(*angle)-(y-y0)*sin(*angle) - x)*(*scale);
		loc[1]-= (y0 + (x-x0)*sin(*angle)+(y-y0)*cos(*angle) - y)*(*scale);
	}
}

static float stabilization_auto_scale_factor(MovieTracking *tracking, int width, int height)
{
	float firstmedian[2];
	MovieTrackingStabilization *stab= &tracking->stabilization;

	if(stab->ok)
		return stab->scale;

	if(stabilization_median_point(tracking, 1, firstmedian)) {
		int sfra= INT_MAX, efra= INT_MIN, cfra;
		float delta[2]= {0.0f, 0.0f}, scalex= 1.0f, scaley= 1.0f;
		MovieTrackingTrack *track;

		stab->scale= 1.0f;

		track= tracking->tracks.first;
		while(track) {
			if(track->flag&TRACK_USE_2D_STAB || track==stab->rot_track) {
				if(track->markersnr) {
					sfra= MIN2(sfra, track->markers[0].framenr);
					efra= MAX2(efra, track->markers[track->markersnr-1].framenr);
				}
			}

			track= track->next;
		}

		for(cfra=sfra; cfra<=efra; cfra++) {
			float median[2], near[2];
			float loc[2], scale, angle;

			stabilization_median_point(tracking, cfra, median);

			calculate_stabdata(tracking, cfra, width, height, firstmedian, median,
						loc, &scale, &angle);

			if(angle==0.0f) {
				loc[0]= fabsf(loc[0]);
				loc[1]= fabsf(loc[1]);

				delta[0]= MAX2(delta[0], loc[0]);
				delta[1]= MAX2(delta[1], loc[1]);

				near[0]= MIN2(median[0], 1.0f-median[0]);
				near[1]= MIN2(median[1], 1.0f-median[1]);
				near[0]= MAX2(near[0], 0.05f);
				near[1]= MAX2(near[1], 0.05f);

				scalex= 1.0f+delta[0]/(near[0]*width);
				scaley= 1.0f+delta[1]/(near[1]*height);
			} else {
				int i;
				float mat[4][4];
				float points[4][2]={{0.0f, 0.0f}, {0.0f, height}, {width, height}, {width, 0.0f}};

				BKE_tracking_stabdata_to_mat4(width, height, loc, scale, angle, mat);

				for(i= 0; i<4; i++) {
					int j;
					float a[3]= {0.0f, 0.0f, 0.0f}, b[3]= {0.0f, 0.0f, 0.0f};

					copy_v3_v3(a, points[i]);
					copy_v3_v3(b, points[(i+1)%4]);

					mul_m4_v3(mat, a);
					mul_m4_v3(mat, b);

					for(j= 0; j<4; j++) {
						float point[3]= {points[j][0], points[j][1], 0.0f};
						float v1[3], v2[3];

						sub_v3_v3v3(v1, b, a);
						sub_v3_v3v3(v2, point, a);

						if(cross_v2v2(v1, v2) >= 0.0f) {
							float dist= dist_to_line_v2(point, a, b);
							if(i%2==0) {
								scalex= MAX2(scalex, (width+2*dist)/width);
							} else {
								scaley= MAX2(scaley, (height+2*dist)/height);
							}
						}
					}
				}
			}
		}

		stab->scale= MAX2(scalex, scaley);

		if(stab->maxscale>0.0f)
			stab->scale= MIN2(stab->scale, stab->maxscale);
	} else {
		stab->scale= 1.0f;
	}

	stab->ok= 1;

	return stab->scale;
}

static ImBuf* stabilize_alloc_ibuf(ImBuf *cacheibuf, ImBuf *srcibuf, int fill)
{
	int flags;

	if(cacheibuf && (cacheibuf->x != srcibuf->x || cacheibuf->y != srcibuf->y)) {
		IMB_freeImBuf(cacheibuf);
		cacheibuf= NULL;
	}

	flags= IB_rect;

	if(srcibuf->rect_float)
		flags|= IB_rectfloat;

	if(cacheibuf) {
		if(fill) {
			float col[4]= {0.0f, 0.0f, 0.0f, 0.0f};
			IMB_rectfill(cacheibuf, col);
		}
	}
	else {
		cacheibuf= IMB_allocImBuf(srcibuf->x, srcibuf->y, srcibuf->depth, flags);
		cacheibuf->profile= srcibuf->profile;
	}

	return cacheibuf;
}

void BKE_tracking_stabilization_data(MovieTracking *tracking, int framenr, int width, int height, float loc[2], float *scale, float *angle)
{
	float firstmedian[2], median[2];
	MovieTrackingStabilization *stab= &tracking->stabilization;

	if((stab->flag&TRACKING_2D_STABILIZATION)==0) {
		zero_v2(loc);
		*scale= 1.0f;
		*angle= 0.0f;

		return;
	}

	if(stabilization_median_point(tracking, 1, firstmedian)) {
		stabilization_median_point(tracking, framenr, median);

		if((stab->flag&TRACKING_AUTOSCALE)==0)
			stab->scale= 1.0f;

		if(!stab->ok) {
			if(stab->flag&TRACKING_AUTOSCALE)
				stabilization_auto_scale_factor(tracking, width, height);

			calculate_stabdata(tracking, framenr, width, height, firstmedian, median, loc, scale, angle);

			stab->ok= 1;
		} else {
			calculate_stabdata(tracking, framenr, width, height, firstmedian, median, loc, scale, angle);
		}
	} else {
		zero_v2(loc);
		*scale= 1.0f;
		*angle= 0.0f;
	}
}

ImBuf *BKE_tracking_stabilize(MovieTracking *tracking, int framenr, ImBuf *ibuf, float loc[2], float *scale, float *angle)
{
	float tloc[2], tscale, tangle;
	MovieTrackingStabilization *stab= &tracking->stabilization;
	ImBuf *tmpibuf;
	float width= ibuf->x, height= ibuf->y;

	if(loc)		copy_v2_v2(tloc, loc);
	if(scale)	tscale= *scale;

	if((stab->flag&TRACKING_2D_STABILIZATION)==0) {
		if(loc)		zero_v2(loc);
		if(scale) 	*scale= 1.0f;

		return ibuf;
	}

	BKE_tracking_stabilization_data(tracking, framenr, width, height, tloc, &tscale, &tangle);

	tmpibuf= stabilize_alloc_ibuf(NULL, ibuf, 1);

	/* scale would be handled by matrix transformation when angle is non-zero */
	if(tscale!=1.0f && tangle==0.0f) {
		ImBuf *scaleibuf;

		stabilization_auto_scale_factor(tracking, width, height);

		scaleibuf= stabilize_alloc_ibuf(stab->scaleibuf, ibuf, 0);
		stab->scaleibuf= scaleibuf;

		IMB_rectcpy(scaleibuf, ibuf, 0, 0, 0, 0, ibuf->x, ibuf->y);
		IMB_scalefastImBuf(scaleibuf, ibuf->x*tscale, ibuf->y*tscale);

		ibuf= scaleibuf;
	}

	if(tangle==0.0f) {
		/* if angle is zero, then it's much faster to use rect copy
		   but could be issues with subpixel precisions */
		IMB_rectcpy(tmpibuf, ibuf, tloc[0]-(tscale-1.0f)*width/2.0f, tloc[1]-(tscale-1.0f)*height/2.0f, 0, 0, ibuf->x, ibuf->y);
	} else {
		float mat[4][4];
		int i, j;

		BKE_tracking_stabdata_to_mat4(ibuf->x, ibuf->y, tloc, tscale, tangle, mat);
		invert_m4(mat);

		for(j=0; j<tmpibuf->y; j++) {
			for(i=0; i<tmpibuf->x;i++) {
				float vec[3]= {i, j, 0};

				mul_v3_m4v3(vec, mat, vec);

				/* TODO: add selector for interpolation method */
				neareast_interpolation(ibuf, tmpibuf, vec[0], vec[1], i, j);
			}
		}
	}

	tmpibuf->userflags|= IB_MIPMAP_INVALID;

	if(tmpibuf->rect_float)
		tmpibuf->userflags|= IB_RECT_INVALID;

	if(loc)		copy_v2_v2(loc, tloc);
	if(scale)	*scale= tscale;
	if(angle)	*angle= tangle;

	return tmpibuf;
}

void BKE_tracking_stabdata_to_mat4(int width, int height, float loc[2], float scale, float angle, float mat[4][4])
{
	float lmat[4][4], rmat[4][4], smat[4][4], cmat[4][4], icmat[4][4];
	float svec[3]= {scale, scale, scale};

	unit_m4(rmat);
	unit_m4(lmat);
	unit_m4(smat);
	unit_m4(cmat);

	/* image center as rotation center */
	cmat[3][0]= (float)width/2.0f;
	cmat[3][1]= (float)height/2.0f;
	invert_m4_m4(icmat, cmat);

	size_to_mat4(smat, svec);		/* scale matrix */
	add_v2_v2(lmat[3], loc);		/* tranlation matrix */
	rotate_m4(rmat, 'Z', angle);	/* rotation matrix */

	/* compose transformation matrix */
	mul_serie_m4(mat, lmat, cmat, rmat, smat, icmat, NULL, NULL, NULL);
}

MovieDistortion *BKE_tracking_distortion_create(void)
{
	MovieDistortion *distortion;

	distortion= MEM_callocN(sizeof(MovieDistortion), "BKE_tracking_distortion_create");

	return distortion;
}

MovieDistortion *BKE_tracking_distortion_copy(MovieDistortion *distortion)
{
	MovieDistortion *new_distortion;

	new_distortion= MEM_callocN(sizeof(MovieDistortion), "BKE_tracking_distortion_create");

#ifdef WITH_LIBMV
	new_distortion->intrinsics= libmv_CameraIntrinsicsCopy(distortion->intrinsics);
#endif

	return new_distortion;
}

void BKE_tracking_distortion_update(MovieDistortion *distortion, MovieTracking *tracking, int width, int height)
{
	MovieTrackingCamera *camera= &tracking->camera;
	float aspy= 1.0f/tracking->camera.pixel_aspect;

#ifdef WITH_LIBMV
	if(!distortion->intrinsics) {
		distortion->intrinsics= libmv_CameraIntrinsicsNew(camera->focal,
				camera->principal[0], camera->principal[1] * aspy,
				camera->k1, camera->k2, camera->k3, width, height * aspy);
	} else {
		libmv_CameraIntrinsicsUpdate(distortion->intrinsics, camera->focal,
				camera->principal[0], camera->principal[1] * aspy,
				camera->k1, camera->k2, camera->k3, width, height * aspy);
	}
#endif
}

ImBuf *BKE_tracking_distortion_exec(MovieDistortion *distortion, MovieTracking *tracking,
			ImBuf *ibuf, int width, int height, float overscan, int undistort)
{
	ImBuf *resibuf;

	BKE_tracking_distortion_update(distortion, tracking, width, height);

	resibuf= IMB_dupImBuf(ibuf);

	if(ibuf->rect_float) {
#ifdef WITH_LIBMV
		if(undistort) {
			libmv_CameraIntrinsicsUndistortFloat(distortion->intrinsics,
						ibuf->rect_float, resibuf->rect_float,
						ibuf->x, ibuf->y, overscan, ibuf->channels);
		} else {
			libmv_CameraIntrinsicsDistortFloat(distortion->intrinsics,
						ibuf->rect_float, resibuf->rect_float,
						ibuf->x, ibuf->y, overscan, ibuf->channels);
		}
#endif

		ibuf->userflags|= IB_RECT_INVALID;
	} else {
#ifdef WITH_LIBMV
		if(undistort) {
				libmv_CameraIntrinsicsUndistortByte(distortion->intrinsics,
							(unsigned char*)ibuf->rect, (unsigned char*)resibuf->rect,
							ibuf->x, ibuf->y, overscan, ibuf->channels);
		} else {
			libmv_CameraIntrinsicsDistortByte(distortion->intrinsics,
						(unsigned char*)ibuf->rect, (unsigned char*)resibuf->rect,
						ibuf->x, ibuf->y, overscan, ibuf->channels);
		}
#endif
	}

	return resibuf;
}

void BKE_tracking_distortion_destroy(MovieDistortion *distortion)
{
#ifdef WITH_LIBMV
	libmv_CameraIntrinsicsDestroy(distortion->intrinsics);
#endif

	MEM_freeN(distortion);
}

ImBuf *BKE_tracking_undistort(MovieTracking *tracking, ImBuf *ibuf, int width, int height, float overscan)
{
	MovieTrackingCamera *camera= &tracking->camera;

	if(camera->intrinsics == NULL)
		camera->intrinsics= BKE_tracking_distortion_create();

	return BKE_tracking_distortion_exec(camera->intrinsics, tracking, ibuf, width, height, overscan, 1);
}

ImBuf *BKE_tracking_distort(MovieTracking *tracking, ImBuf *ibuf, int width, int height, float overscan)
{
	MovieTrackingCamera *camera= &tracking->camera;

	if(camera->intrinsics == NULL)
		camera->intrinsics= BKE_tracking_distortion_create();

	return BKE_tracking_distortion_exec(camera->intrinsics, tracking, ibuf, width, height, overscan, 0);
}
