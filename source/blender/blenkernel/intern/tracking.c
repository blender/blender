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

#include "DNA_movieclip_types.h"
#include "DNA_object_types.h"	/* SELECT */
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_ghash.h"

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

/*********************** common functions *************************/

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

	/* marker's center should be in center of pattern */
	if(event==CLAMP_PAT_DIM || event==CLAMP_PAT_POS) {
		float dim[2];
		sub_v2_v2v2(dim, track->pat_max, track->pat_min);

		for(a= 0; a<2; a++) {
			track->pat_min[a]= -dim[a]/2.f;
			track->pat_max[a]= dim[a]/2.f;
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
	float pat[2]= {5.5f, 5.5f}, search[2]= {80.5f, 80.5f}; /* TODO: move to default setting? */

	pat[0] /= (float)width;
	pat[1] /= (float)height;

	search[0] /= (float)width;
	search[1] /= (float)height;

	track= MEM_callocN(sizeof(MovieTrackingTrack), "add_marker_exec track");
	strcpy(track->name, "Track");

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
	} else if(action==TRACK_CLEAR_ALL) {
		MovieTrackingMarker *marker, marker_new;

		marker= BKE_tracking_get_marker(track, ref_frame);
		if(marker)
			marker_new= *marker;

		MEM_freeN(track->markers);
		track->markers= NULL;
		track->markersnr= 0;

		if(marker)
			BKE_tracking_insert_marker(track, &marker_new);
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
}

/*********************** tracking *************************/

typedef struct TrackContext {
	MovieTrackingTrack *track;

#ifdef WITH_LIBMV
	struct libmv_RegionTracker *region_tracker;
#endif
} TrackContext;

typedef struct MovieTrackingContext {
	MovieClipUser user;
	MovieClip *clip;

	TrackContext *track_context;
	int num_tracks;

	GHash *hash;
	MovieTrackingSettings settings;

	int backwards;
	int sync_frame;
} MovieTrackingContext;

MovieTrackingContext *BKE_tracking_context_new(MovieClip *clip, MovieClipUser *user, int backwards)
{
	MovieTrackingContext *context= MEM_callocN(sizeof(MovieTrackingContext), "trackingContext");
	MovieTracking *tracking= &clip->tracking;
	MovieTrackingSettings *settings= &tracking->settings;
	MovieTrackingTrack *track;
	TrackContext *track_context;

	context->settings= *settings;
	context->backwards= backwards;
	context->hash= BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "tracking trackHash");
	context->sync_frame= user->framenr;

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

		BKE_movieclip_acquire_size(clip, user, &width, &height);

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
						float search_size_x= (track->search_max[0]-track->search_min[0])*width;
						float search_size_y= (track->search_max[1]-track->search_min[1])*height;
						float pattern_size_x= (track->pat_max[0]-track->pat_min[0])*width;
						float pattern_size_y= (track->pat_max[1]-track->pat_min[1])*height;

						int level= log(2.0f * MIN2(search_size_x, search_size_y) / MAX2(pattern_size_x, pattern_size_y))/M_LN2;

						track_context->region_tracker= libmv_regionTrackerNew(100, level, 0.2);
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
		libmv_regionTrackerDestroy(track_context->region_tracker);
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

static ImBuf *acquire_area_imbuf(ImBuf *ibuf, MovieTrackingTrack *track, MovieTrackingMarker *marker,
			float min[2], float max[2], int margin, float pos[2], int origin[2])
{
	ImBuf *tmpibuf;
	int x, y;
	int x1, y1, x2, y2, w, h;

	x= marker->pos[0]*ibuf->x;
	y= marker->pos[1]*ibuf->y;
	x1= x-(int)(-min[0]*ibuf->x);
	y1= y-(int)(-min[1]*ibuf->y);
	x2= x+(int)(max[0]*ibuf->x);
	y2= y+(int)(max[1]*ibuf->y);

	/* dimensions should be odd */
	w= (x2-x1)|1;
	h= (y2-y1)|1;

	/* happens due to rounding issues */
	if(x1+w<=x) x1++;
	if(y1+h<=y) y1++;

	tmpibuf= IMB_allocImBuf(w+margin*2, h+margin*2, 32, IB_rect);
	IMB_rectcpy(tmpibuf, ibuf, 0, 0, x1-margin, y1-margin, w+margin*2, h+margin*2);

	if(pos != NULL) {
		pos[0]= x-x1+(marker->pos[0]*ibuf->x-x)+margin;
		pos[1]= y-y1+(marker->pos[1]*ibuf->y-y)+margin;
	}

	if(origin != NULL) {
		origin[0]= x1-margin;
		origin[1]= y1-margin;
	}

	disable_imbuf_channels(tmpibuf, track);

	return tmpibuf;
}

ImBuf *BKE_tracking_acquire_pattern_imbuf(ImBuf *ibuf, MovieTrackingTrack *track, MovieTrackingMarker *marker,
			int margin, float pos[2], int origin[2])
{
	return acquire_area_imbuf(ibuf, track, marker, track->pat_min, track->pat_max, margin, pos, origin);
}

ImBuf *BKE_tracking_acquire_search_imbuf(ImBuf *ibuf, MovieTrackingTrack *track, MovieTrackingMarker *marker,
			int margin, float pos[2], int origin[2])
{
	return acquire_area_imbuf(ibuf, track, marker, track->search_min, track->search_max, margin, pos, origin);
}

#ifdef WITH_LIBMV
static float *acquire_search_floatbuf(ImBuf *ibuf, MovieTrackingTrack *track, MovieTrackingMarker *marker,
			int *width_r, int *height_r, float pos[2], int origin[2])
{
	ImBuf *tmpibuf;
	float *pixels, *fp;
	int x, y, width, height;

	width= (track->search_max[0]-track->search_min[0])*ibuf->x;
	height= (track->search_max[1]-track->search_min[1])*ibuf->y;

	tmpibuf= BKE_tracking_acquire_search_imbuf(ibuf, track, marker, 0, pos, origin);
	disable_imbuf_channels(tmpibuf, track);

	*width_r= width;
	*height_r= height;

	fp= pixels= MEM_callocN(width*height*sizeof(float), "tracking floatBuf");
	for(y= 0; y<(int)height; y++) {
		for (x= 0; x<(int)width; x++) {
			int pixel= tmpibuf->x*y + x;

			if(tmpibuf->rect_float) {
				float *rrgbf= ibuf->rect_float + pixel*4;

				*fp= (0.2126*rrgbf[0] + 0.7152*rrgbf[1] + 0.0722*rrgbf[2])/255;
			} else {
				char *rrgb= (char*)tmpibuf->rect + pixel*4;

				*fp= (0.2126*rrgb[0] + 0.7152*rrgb[1] + 0.0722*rrgb[2])/255;
			}

			fp++;
		}
	}

	IMB_freeImBuf(tmpibuf);

	return pixels;
}
#endif

void BKE_tracking_sync(MovieTrackingContext *context)
{
	TrackContext *track_context;
	MovieTrackingTrack *track;
	ListBase tracks= {NULL, NULL};
	ListBase *old_tracks= &context->clip->tracking.tracks;
	int a, sel_type, newframe;
	void *sel;

	BKE_movieclip_last_selection(context->clip, &sel_type, &sel);

	/* duplicate currently tracking tracks to list of displaying tracks */
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
				if(sel_type==MCLIP_SEL_TRACK && sel==cur)
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
			BKE_movieclip_set_selection(context->clip, MCLIP_SEL_TRACK, new_track);

		BLI_addtail(&tracks, new_track);
	}

	/* move tracks, which could be added by user during tracking */
	track= old_tracks->first;
	while(track) {
		MovieTrackingTrack *next= track->next;

		track->next= track->prev= NULL;
		BLI_addtail(&tracks, track);

		track= next;
	}

	context->clip->tracking.tracks= tracks;

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
	ImBuf *ibuf, *ibuf_new;
	int curfra= context->user.framenr;
	int a, ok= 0;

	/* nothing to track, avoid unneeded frames reading to save time and memory */
	if(!context->num_tracks)
		return 0;

	ibuf= BKE_movieclip_acquire_ibuf(context->clip, &context->user);
	if(!ibuf) return 0;

	if(context->backwards) context->user.framenr--;
	else context->user.framenr++;

	ibuf_new= BKE_movieclip_acquire_ibuf(context->clip, &context->user);
	if(!ibuf_new) {
		IMB_freeImBuf(ibuf);
		return 0;
	}

	#pragma omp parallel for private(a) shared(ibuf, ibuf_new, ok)
	for(a= 0; a<context->num_tracks; a++) {
		TrackContext *track_context= &context->track_context[a];
		MovieTrackingTrack *track= track_context->track;
		MovieTrackingMarker *marker= BKE_tracking_get_marker(track, curfra);

		if(marker && (marker->flag&MARKER_DISABLED)==0 && marker->framenr==curfra) {
#ifdef WITH_LIBMV
			int width, height, origin[2];
			float pos[2];
			float *patch, *patch_new;
			double x1, y1, x2, y2;
			int wndx, wndy;
			MovieTrackingMarker marker_new;

			patch= acquire_search_floatbuf(ibuf, track, marker, &width, &height, pos, origin);
			patch_new= acquire_search_floatbuf(ibuf_new, track, marker, &width, &height, pos, origin);

			x1= pos[0];
			y1= pos[1];

			x2= x1;
			y2= y1;

			wndx= (int)((track->pat_max[0]-track->pat_min[0])*ibuf->x)/2;
			wndy= (int)((track->pat_max[1]-track->pat_min[1])*ibuf->y)/2;

			if(libmv_regionTrackerTrack(track_context->region_tracker, patch, patch_new,
						width, height, MAX2(wndx, wndy),
						x1, y1, &x2, &y2)) {
				memset(&marker_new, 0, sizeof(marker_new));
				marker_new.pos[0]= (origin[0]+x2)/ibuf_new->x;
				marker_new.pos[1]= (origin[1]+y2)/ibuf_new->y;
				marker_new.flag|= MARKER_TRACKED;

				if(context->backwards) marker_new.framenr= curfra-1;
				else marker_new.framenr= curfra+1;

				#pragma omp critical
				{
					BKE_tracking_insert_marker(track, &marker_new);
				}

			} else {
				marker_new= *marker;

				if(context->backwards) marker_new.framenr--;
				else marker_new.framenr++;

				marker_new.flag|= MARKER_DISABLED;

				#pragma omp critical
				{
					BKE_tracking_insert_marker(track, &marker_new);
				}
			}

			MEM_freeN(patch);
			MEM_freeN(patch_new);

			ok= 1;
#endif
		}
	}

	IMB_freeImBuf(ibuf);
	IMB_freeImBuf(ibuf_new);

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

static int retrive_libmv_reconstruct(MovieTracking *tracking, struct libmv_Reconstruction *libmv_reconstruction)
{
	int tracknr= 0;
	int sfra= INT_MAX, efra= INT_MIN, a, origin_set= 0;
	MovieTrackingTrack *track;
	MovieTrackingReconstruction *reconstruction= &tracking->reconstruction;
	MovieReconstructedCamera *reconstructed;
	float origin[3]= {0.0f, 0.f, 0.0f};
	int ok= 1;

	track= tracking->tracks.first;
	while(track) {
		double pos[3];

		if(libmv_reporojectionPointForTrack(libmv_reconstruction, tracknr, pos)) {
			track->bundle_pos[0]= pos[0];
			track->bundle_pos[1]= pos[1];
			track->bundle_pos[2]= pos[2];

			track->flag|= TRACK_HAS_BUNDLE;
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

float BKE_tracking_solve_reconstruction(MovieTracking *tracking, int width, int height, float aspx, float aspy)
{
#if WITH_LIBMV
	{
		MovieTrackingCamera *camera= &tracking->camera;
		struct libmv_Tracks *tracks= create_libmv_tracks(tracking, width*aspx, height*aspy);
		struct libmv_Reconstruction *reconstruction = libmv_solveReconstruction(tracks,
		        tracking->settings.keyframe1, tracking->settings.keyframe2,
		        camera->focal,
		        camera->principal[0]*aspx, camera->principal[1]*aspy,
		        camera->k1, camera->k2, camera->k3);
		float error= libmv_reprojectionError(reconstruction);

		tracking->reconstruction.error= error;

		if(!retrive_libmv_reconstruct(tracking, reconstruction))
			error= -1.f;

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

MovieReconstructedCamera *BKE_tracking_get_reconstructed_camera(MovieTracking *tracking, int framenr)
{
	MovieTrackingReconstruction *reconstruction= &tracking->reconstruction;
	int a= 0, d= 1;

	if(!reconstruction->camnr)
		return NULL;

	if(reconstruction->last_camera<reconstruction->camnr)
		a= reconstruction->last_camera;

	if(reconstruction->cameras[a].framenr>=framenr)
		d= -1;

	while(a>=0 && a<reconstruction->camnr) {
		if(reconstruction->cameras[a].framenr==framenr) {
			reconstruction->last_camera= a;
			return &reconstruction->cameras[a];

			break;
		}

		a+= d;
	}

	return NULL;
}

void BKE_get_tracking_mat(Scene *scene, float mat[4][4])
{
	if(!scene->camera)
		scene->camera= scene_find_camera(scene);

	if(scene->camera)
		where_is_object_mat(scene, scene->camera, mat);
	else
		unit_m4(mat);
}

void BKE_tracking_projection_matrix(MovieTracking *tracking, int framenr, int winx, int winy, float mat[4][4])
{
	MovieReconstructedCamera *camera;
	float lens= tracking->camera.focal*32.0f/(float)winx;
	float viewfac, pixsize, left, right, bottom, top, clipsta, clipend;
	float winmat[4][4];

	clipsta= 0.1f;
	clipend= 1000.0f;

	if(winx >= winy)
		viewfac= (lens*winx)/32.0f;
	else
		viewfac= (lens*winy)/32.0f;

	pixsize= clipsta/viewfac;

	left= -0.5f*(float)winx*pixsize;
	bottom= -0.5f*(float)winy*pixsize;
	right=  0.5f*(float)winx*pixsize;
	top=  0.5f*(float)winy*pixsize;

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

	/* normalize coords */
	x= (co[0]-camera->principal[0]) / camera->focal;
	y= (co[1]-camera->principal[1]) / camera->focal;

	libmv_applyCameraIntrinsics(camera->focal, camera->principal[0], camera->principal[1],
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

	libmv_InvertIntrinsics(camera->focal, camera->principal[0], camera->principal[1],
				camera->k1, camera->k2, camera->k3, x, y, &x, &y);

	nco[0]= x * camera->focal + camera->principal[0];
	nco[1]= y * camera->focal + camera->principal[1];
#endif
}

#ifdef WITH_LIBMV
/* flips upside-down */
static unsigned char *acquire_ucharbuf(ImBuf *ibuf)
{
	int x, y;
	unsigned char *pixels, *fp;

	fp= pixels= MEM_callocN(ibuf->x*ibuf->y*sizeof(unsigned char), "tracking ucharBuf");
	for(y= 0; y<ibuf->y; y++) {
		for (x= 0; x<ibuf->x; x++) {
			int pixel= ibuf->x*(ibuf->y-y-1) + x;

			if(ibuf->rect_float) {
				float *rrgbf= ibuf->rect_float + pixel*4;

				//*fp= 0.2126f*rrgbf[0] + 0.7152f*rrgbf[1] + 0.0722f*rrgbf[2];
				*fp= (11*rrgbf[0]+16*rrgbf[1]+5*rrgbf[2])/32;
			} else {
				char *rrgb= (char*)ibuf->rect + pixel*4;

				//*fp= 0.2126f*rrgb[0] + 0.7152f*rrgb[1] + 0.0722f*rrgb[2];
				*fp= (11*rrgb[0]+16*rrgb[1]+5*rrgb[2])/32;
			}

			fp++;
		}
	}

	return pixels;
}
#endif

void BKE_tracking_detect(MovieTracking *tracking, ImBuf *ibuf, int framenr)
{
#ifdef WITH_LIBMV
	struct libmv_Corners *corners;
	unsigned char *pixels= acquire_ucharbuf(ibuf);
	int a;

	corners= libmv_detectCorners(pixels, ibuf->x, ibuf->y, ibuf->x);
	MEM_freeN(pixels);

	a= libmv_countCorners(corners);
	while(a--) {
		MovieTrackingTrack *track;
		double x, y, size, score;

		libmv_getCorner(corners, a, &x, &y, &score, &size);

		track= BKE_tracking_add_track(tracking, x/ibuf->x, 1.0f-(y/ibuf->y), framenr, ibuf->x, ibuf->y);
		track->flag|= SELECT;
		track->pat_flag|= SELECT;
		track->search_flag|= SELECT;
	}

	libmv_destroyCorners(corners);
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

	median[0]= (max[0]+min[0])/2.f;
	median[1]= (max[1]+min[1])/2.f;

	return ok;
}

static float stabilization_auto_scale_factor(MovieTracking *tracking)
{
	float firstmedian[2];
	MovieTrackingStabilization *stab= &tracking->stabilization;

	if(stab->ok)
		return stab->scale;

	if(stabilization_median_point(tracking, 1, firstmedian)) {
		int sfra= INT_MAX, efra= INT_MIN, cfra;
		float delta[2]= {0.f, 0.f}, scalex, scaley, near[2]={1.f, 1.f};
		MovieTrackingTrack *track;

		track= tracking->tracks.first;
		while(track) {
			if(track->flag&TRACK_USE_2D_STAB) {
				if(track->markersnr) {
					sfra= MIN2(sfra, track->markers[0].framenr);
					efra= MAX2(efra, track->markers[track->markersnr-1].framenr);
				}
			}

			track= track->next;
		}

		for(cfra=sfra; cfra<=efra; cfra++) {
			float median[2], d[2];

			stabilization_median_point(tracking, cfra, median);

			sub_v2_v2v2(d, firstmedian, median);
			d[0]= fabsf(d[0]);
			d[1]= fabsf(d[1]);

			delta[0]= MAX2(delta[0], d[0]);
			delta[1]= MAX2(delta[1], d[1]);

			near[0]= MIN3(near[0], median[0], 1.f-median[0]);
			near[1]= MIN3(near[1], median[1], 1.f-median[1]);
		}

		near[0]= MAX2(near[0], 0.05);
		near[1]= MAX2(near[1], 0.05);

		scalex= 1.f+delta[0]/near[0];
		scaley= 1.f+delta[1]/near[1];

		stab->scale= MAX2(scalex, scaley);
	} else {
		stab->scale= 1.f;
	}

	stab->ok= 1;

	return stab->scale;
}

static void calculate_stabdata(MovieTrackingStabilization *stab, float width, float height,
			float firstmedian[2], float curmedian[2], float loc[2], float *scale)
{
	mul_v2_fl(loc, stab->locinf);
	*scale= (stab->scale-1.f)*stab->scaleinf+1.f;

	loc[0]= (firstmedian[0]-curmedian[0])*width*(*scale);
	loc[1]= (firstmedian[1]-curmedian[1])*height*(*scale);

	loc[0]-= (firstmedian[0]*(*scale)-firstmedian[0])*width;
	loc[1]-= (firstmedian[1]*(*scale)-firstmedian[1])*height;
}

static ImBuf* stabilize_acquire_ibuf(ImBuf *cacheibuf, ImBuf *srcibuf, int fill)
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
			float col[4]= {0.f, 0.f, 0.f, 0.f};
			IMB_rectfill(cacheibuf, col);
		}
	}
	else {
		cacheibuf= IMB_allocImBuf(srcibuf->x, srcibuf->y, srcibuf->depth, flags);
		cacheibuf->profile= srcibuf->profile;
	}

	return cacheibuf;
}

void BKE_tracking_stabilization_data(MovieTracking *tracking, int framenr, int width, int height, float loc[2], float *scale)
{
	float firstmedian[2], curmedian[2];
	MovieTrackingStabilization *stab= &tracking->stabilization;

	if((stab->flag&TRACKING_2D_STABILIZATION)==0) {
		zero_v2(loc);
		*scale= 1.f;

		return;
	}

	if(stabilization_median_point(tracking, 1, firstmedian)) {
		stabilization_median_point(tracking, framenr, curmedian);

		if((stab->flag&TRACKING_AUTOSCALE)==0)
				stab->scale= 1.f;

		if(!stab->ok) {
			if(stab->flag&TRACKING_AUTOSCALE)
				stabilization_auto_scale_factor(tracking);

			calculate_stabdata(stab, width, height, firstmedian, curmedian, loc, scale);

			stab->ok= 1;
		} else {
			calculate_stabdata(stab, width, height, firstmedian, curmedian, loc, scale);
		}
	} else {
		zero_v2(loc);
		*scale= 1.f;
	}
}

ImBuf *BKE_tracking_stabilize_shot(MovieTracking *tracking, int framenr, ImBuf *ibuf, float loc[2], float *scale)
{
	float tloc[2], tscale;
	MovieTrackingStabilization *stab= &tracking->stabilization;
	ImBuf *tmpibuf;
	float width= ibuf->x, height= ibuf->y;

	if(loc)		copy_v2_v2(tloc, loc);
	if(scale)	tscale= *scale;

	if((stab->flag&TRACKING_2D_STABILIZATION)==0) {
		if(loc)		zero_v2(loc);
		if(scale) 	*scale= 1.f;

		return ibuf;
	}

	BKE_tracking_stabilization_data(tracking, framenr, width, height, tloc, &tscale);

	tmpibuf= stabilize_acquire_ibuf(NULL, ibuf, 1);

	if(tscale!=1.f) {
		ImBuf *scaleibuf;
		float scale= (stab->scale-1.f)*stab->scaleinf+1.f;

		stabilization_auto_scale_factor(tracking);

		scaleibuf= stabilize_acquire_ibuf(stab->scaleibuf, ibuf, 0);
		stab->scaleibuf= scaleibuf;

		IMB_rectcpy(scaleibuf, ibuf, 0, 0, 0, 0, ibuf->x, ibuf->y);
		IMB_scalefastImBuf(scaleibuf, ibuf->x*scale, ibuf->y*scale);

		ibuf= scaleibuf;
	}

	IMB_rectcpy(tmpibuf, ibuf, tloc[0], tloc[1], 0, 0, ibuf->x, ibuf->y);

	tmpibuf->userflags|= IB_MIPMAP_INVALID;

	if(tmpibuf->rect_float)
		tmpibuf->userflags|= IB_RECT_INVALID;

	if(loc)		copy_v2_v2(loc, tloc);
	if(scale)	*scale= tscale;

	return tmpibuf;
}

void BKE_tracking_stabdata_to_mat4(float loc[2], float scale, float mat[4][4])
{
	unit_m4(mat);

	mat[0][0]*= scale;
	mat[1][1]*= scale;
	mat[2][2]*= scale;

	copy_v2_v2(mat[3], loc);
}
