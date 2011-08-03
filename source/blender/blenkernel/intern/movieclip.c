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

/** \file blender/blenkernel/intern/movieclip.c
 *  \ingroup bke
 */


#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#endif

#include <time.h>

#ifdef _WIN32
#define open _open
#define close _close
#endif

#include "MEM_guardedalloc.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_object_types.h"	/* SELECT */

#include "BLI_utildefines.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_math.h"
#include "BLI_mempool.h"
#include "BLI_threads.h"

#include "BKE_library.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_utildefines.h"
#include "BKE_movieclip.h"
#include "BKE_moviecache.h"
#include "BKE_image.h"	/* openanim */
#include "BKE_tracking.h"
#include "BKE_animsys.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

/*********************** movieclip buffer loaders *************************/

static int sequence_guess_offset(const char *full_name, int head_len, int numlen)
{
	char num[FILE_MAX]= {0};

	strncpy(num, full_name+head_len, numlen);

	return atoi(num);
}

static ImBuf *movieclip_load_sequence_file(MovieClip *clip, int framenr)
{
	struct ImBuf *ibuf;
	unsigned short numlen;
	char name[FILE_MAX], head[FILE_MAX], tail[FILE_MAX];
	int flag, offset;

	BLI_strncpy(name, clip->name, sizeof(name));
	BLI_stringdec(name, head, tail, &numlen);

	/* movieclips always points to first image from sequence,
	   autoguess offset for now. could be something smarter in the future */
	offset= sequence_guess_offset(clip->name, strlen(head), numlen);

	if(numlen) BLI_stringenc(name, head, tail, numlen, offset+framenr-1);
	else strncpy(name, clip->name, sizeof(name));

	if(clip->id.lib)
		BLI_path_abs(name, clip->id.lib->filepath);
	else
		BLI_path_abs(name, G.main->name);

	flag= IB_rect|IB_multilayer;

	/* read ibuf */
	ibuf= IMB_loadiffname(name, flag);

	return ibuf;
}

static ImBuf *movieclip_load_movie_file(MovieClip *clip, int framenr)
{
	ImBuf *ibuf= NULL;
	char str[FILE_MAX];

	if(!clip->anim) {
		BLI_strncpy(str, clip->name, FILE_MAX);

		if(clip->id.lib)
			BLI_path_abs(str, clip->id.lib->filepath);
		else
			BLI_path_abs(str, G.main->name);

		clip->anim= openanim(str, IB_rect);
	}

	if(clip->anim) {
		int dur= IMB_anim_get_duration(clip->anim);
		int fra= framenr-1;

		if(fra<0)
			fra= 0;

		if(fra>(dur-1))
			fra= dur-1;

		ibuf= IMB_anim_absolute(clip->anim, fra);
	}

	return ibuf;
}

/*********************** image buffer cache *************************/

typedef struct MovieClipCache {
	/* regular moive cache */
	struct MovieCache *moviecache;

	/* cache for stable shot */
	int stable_framenr;
	float stable_loc[2], stable_scale;
	ImBuf *stableibuf;
} MovieClipCache;

typedef struct MovieClipImBufCacheKey {
	int framenr;
} MovieClipImBufCacheKey;

static void moviecache_keydata(void *userkey, int *framenr)
{
	MovieClipImBufCacheKey *key= (MovieClipImBufCacheKey*)userkey;

	*framenr= key->framenr;
}

static unsigned int moviecache_hashhash(const void *keyv)
{
	MovieClipImBufCacheKey *key= (MovieClipImBufCacheKey*)keyv;
	int rval= key->framenr;

	return rval;
}

static int moviecache_hashcmp(const void *av, const void *bv)
{
	const MovieClipImBufCacheKey *a= (MovieClipImBufCacheKey*)av;
	const MovieClipImBufCacheKey *b= (MovieClipImBufCacheKey*)bv;

	if(a->framenr<b->framenr) return -1;
	else if(a->framenr>b->framenr) return 1;

	return 0;
}

static ImBuf *get_imbuf_cache(MovieClip *clip, MovieClipUser *user)
{
	if(clip->cache) {
		MovieClipImBufCacheKey key;

		key.framenr= user?user->framenr:clip->lastframe;
		return BKE_moviecache_get(clip->cache->moviecache, &key);
	}

	return NULL;
}

static void put_imbuf_cache(MovieClip *clip, MovieClipUser *user, ImBuf *ibuf)
{
	MovieClipImBufCacheKey key;

	if(!clip->cache) {
		clip->cache= MEM_callocN(sizeof(MovieClipCache), "movieClipCache");

		clip->cache->moviecache= BKE_moviecache_create(sizeof(MovieClipImBufCacheKey), moviecache_hashhash,
				moviecache_hashcmp, moviecache_keydata);
	}

	key.framenr= user?user->framenr:clip->lastframe;

	BKE_moviecache_put(clip->cache->moviecache, &key, ibuf);
}

/*********************** common functions *************************/

/* only image block itself */
static MovieClip *movieclip_alloc(const char *name)
{
	MovieClip *clip;

	clip= alloc_libblock(&G.main->movieclip, ID_MC, name);

	clip->aspx= clip->aspy= 1.0f;

	clip->tracking.camera.sensor_width= 35.0f;
	clip->tracking.camera.sensor_height= 18.0f;
	clip->tracking.camera.units= CAMERA_UNITS_MM;

	clip->tracking.settings.frames_limit= 20;
	clip->tracking.settings.keyframe1= 1;
	clip->tracking.settings.keyframe2= 30;
	clip->tracking.settings.dist= 1;

	clip->tracking.stabilization.scaleinf= 1.f;
	clip->tracking.stabilization.locinf= 1.f;

	return clip;
}

/* checks if image was already loaded, then returns same image
   otherwise creates new.
   does not load ibuf itself
   pass on optional frame for #name images */
MovieClip *BKE_add_movieclip_file(const char *name)
{
	MovieClip *clip;
	MovieClipUser user;
	int file, len, width, height;
	const char *libname;
	char str[FILE_MAX], strtest[FILE_MAX];

	BLI_strncpy(str, name, sizeof(str));
	BLI_path_abs(str, G.main->name);

	/* exists? */
	file= open(str, O_BINARY|O_RDONLY);
	if(file== -1) return NULL;
	close(file);

	/* ** first search an identical clip ** */
	for(clip= G.main->movieclip.first; clip; clip= clip->id.next) {
		BLI_strncpy(strtest, clip->name, sizeof(clip->name));
		BLI_path_abs(strtest, G.main->name);

		if(strcmp(strtest, str)==0) {
			BLI_strncpy(clip->name, name, sizeof(clip->name));  /* for stringcode */
			clip->id.us++;  /* officially should not, it doesn't link here! */

			return clip;
		}
	}

	/* ** add new movieclip ** */

	/* create a short library name */
	len= strlen(name);

	while (len > 0 && name[len - 1] != '/' && name[len - 1] != '\\') len--;
	libname= name+len;

	clip= movieclip_alloc(libname);
	BLI_strncpy(clip->name, name, sizeof(clip->name));

	if(BLI_testextensie_array(name, imb_ext_movie)) clip->source= MCLIP_SRC_MOVIE;
	else clip->source= MCLIP_SRC_SEQUENCE;

	user.framenr= 1;
	BKE_movieclip_acquire_size(clip, &user, &width, &height);
	if(width && height) {
		clip->tracking.camera.principal[0]= ((float)width)/2;
		clip->tracking.camera.principal[1]= ((float)height)/2;
	}

	return clip;
}

ImBuf *BKE_movieclip_acquire_ibuf(MovieClip *clip, MovieClipUser *user)
{
	ImBuf *ibuf= NULL;
	int framenr= user?user->framenr:clip->lastframe;

	/* cache isn't threadsafe itself and also loading of movies
	   can't happen from concurent threads that's why we use lock here */
	BLI_lock_thread(LOCK_MOVIECLIP);

	/* cache is supposed to be threadsafe */
	ibuf= get_imbuf_cache(clip, user);

	if(!ibuf) {
		if(clip->source==MCLIP_SRC_SEQUENCE)
			ibuf= movieclip_load_sequence_file(clip, framenr);
		else {
			ibuf= movieclip_load_movie_file(clip, framenr);
		}

		if(ibuf)
			put_imbuf_cache(clip, user, ibuf);
	}

	if(ibuf) {
		clip->lastframe= framenr;

		clip->lastsize[0]= ibuf->x;
		clip->lastsize[1]= ibuf->y;
	}

	BLI_unlock_thread(LOCK_MOVIECLIP);

	return ibuf;
}

ImBuf *BKE_movieclip_acquire_stable_ibuf(MovieClip *clip, MovieClipUser *user, float loc[2], float *scale)
{
	ImBuf *ibuf, *stableibuf= NULL;
	int framenr= user?user->framenr:clip->lastframe;

	ibuf= BKE_movieclip_acquire_ibuf(clip, user);

	if(clip->tracking.stabilization.flag&TRACKING_2D_STABILIZATION) {
		float tloc[2], tscale;

		if(clip->cache->stableibuf && clip->cache->stable_framenr==framenr) {
			stableibuf= clip->cache->stableibuf;

			BKE_tracking_stabilization_data(&clip->tracking, framenr, stableibuf->x, stableibuf->y, tloc, &tscale);

			if(!equals_v2v2(tloc, clip->cache->stable_loc) || tscale!=clip->cache->stable_scale) {
				stableibuf= NULL;
			}
		}

		if(!stableibuf) {
			if(clip->cache->stableibuf)
				IMB_freeImBuf(clip->cache->stableibuf);

			stableibuf= BKE_tracking_stabilize_shot(&clip->tracking, framenr, ibuf, tloc, &tscale);

			copy_v2_v2(clip->cache->stable_loc, tloc);
			clip->cache->stable_scale= tscale;
			clip->cache->stable_framenr= framenr;
			clip->cache->stableibuf= stableibuf;
		}

		IMB_refImBuf(stableibuf);

		if(loc)		copy_v2_v2(loc, tloc);
		if(scale)	*scale= tscale;
	} else {
		if(loc)		zero_v2(loc);
		if(scale)	*scale= 1.f;

		stableibuf= ibuf;
	}

	if(stableibuf!=ibuf) {
		IMB_freeImBuf(ibuf);
		ibuf= stableibuf;
	}

	return ibuf;

}

int BKE_movieclip_has_frame(MovieClip *clip, MovieClipUser *user)
{
	ImBuf *ibuf= BKE_movieclip_acquire_ibuf(clip, user);

	if(ibuf) {
		IMB_freeImBuf(ibuf);
		return 1;
	}

	return 0;
}

void BKE_movieclip_acquire_size(MovieClip *clip, MovieClipUser *user, int *width, int *height)
{
	if(!user || user->framenr==clip->lastframe) {
		*width= clip->lastsize[0];
		*height= clip->lastsize[1];
	} else {
		ImBuf *ibuf= BKE_movieclip_acquire_ibuf(clip, user);

		if(ibuf && ibuf->x && ibuf->y) {
			*width= ibuf->x;
			*height= ibuf->y;
		} else {
			*width= 0;
			*height= 0;
		}

		if(ibuf)
			IMB_freeImBuf(ibuf);
	}
}

void BKE_movieclip_aspect(MovieClip *clip, float *aspx, float *aspy)
{
	*aspx= *aspy= 1.0;

	/* x is always 1 */
	*aspy = clip->aspy/clip->aspx;
}

/* get segments of cached frames. useful for debugging cache policies */
void BKE_movieclip_get_cache_segments(MovieClip *clip, int *totseg_r, int **points_r)
{
	*totseg_r= 0;
	*points_r= NULL;

	if(clip->cache)
		BKE_moviecache_get_cache_segments(clip->cache->moviecache, totseg_r, points_r);
}

void BKE_movieclip_user_set_frame(MovieClipUser *iuser, int framenr)
{
	/* TODO: clamp framenr here? */

	iuser->framenr= framenr;
}

static void free_buffers(MovieClip *clip)
{
	if(clip->cache) {
		BKE_moviecache_free(clip->cache->moviecache);

		if(clip->cache->stableibuf)
			IMB_freeImBuf(clip->cache->stableibuf);

		MEM_freeN(clip->cache);
		clip->cache= NULL;
	}

	if(clip->anim) {
		IMB_free_anim(clip->anim);
		clip->anim= FALSE;
	}
}

void BKE_movieclip_reload(MovieClip *clip)
{
	/* clear cache */
	free_buffers(clip);

	clip->tracking.stabilization.ok= 0;

	/* update clip source */
	if(BLI_testextensie_array(clip->name, imb_ext_movie)) clip->source= MCLIP_SRC_MOVIE;
	else clip->source= MCLIP_SRC_SEQUENCE;
}

/* area - which part of marker should be selected. see TRACK_AREA_* constants */
void BKE_movieclip_select_track(MovieClip *clip, MovieTrackingTrack *track, int area, int extend)
{
	if(extend) {
		BKE_tracking_track_flag(track, area, SELECT, 0);
	} else {
		MovieTrackingTrack *cur= clip->tracking.tracks.first;

		while(cur) {
			if(cur==track) {
				BKE_tracking_track_flag(cur, TRACK_AREA_ALL, SELECT, 1);
				BKE_tracking_track_flag(cur, area, SELECT, 0);
			}
			else {
				BKE_tracking_track_flag(cur, TRACK_AREA_ALL, SELECT, 1);
			}

			cur= cur->next;
		}
	}

	if(!TRACK_SELECTED(track))
		BKE_movieclip_set_selection(clip, MCLIP_SEL_NONE, NULL);
}

void BKE_movieclip_deselect_track(MovieClip *clip, MovieTrackingTrack *track, int area)
{
	BKE_tracking_track_flag(track, area, SELECT, 1);

	if(!TRACK_SELECTED(track))
		BKE_movieclip_set_selection(clip, MCLIP_SEL_NONE, NULL);
}

void BKE_movieclip_set_selection(MovieClip *clip, int type, void *sel)
{
	clip->sel_type= type;

	if(type == MCLIP_SEL_NONE) clip->last_sel= NULL;
	else clip->last_sel= sel;
}

void BKE_movieclip_last_selection(MovieClip *clip, int *type, void **sel)
{
	*type= clip->sel_type;

	if(clip->sel_type == MCLIP_SEL_NONE) *sel= NULL;
	else *sel= clip->last_sel;
}

void BKE_movieclip_update_scopes(MovieClip *clip, MovieClipUser *user, MovieClipScopes *scopes)
{
	void *sel;
	int sel_type;

	if(scopes->ok) return;

	if(scopes->track_preview) {
		IMB_freeImBuf(scopes->track_preview);
		scopes->track_preview= NULL;
	}

	scopes->marker= NULL;
	scopes->track= NULL;

	if(clip) {
		BKE_movieclip_last_selection(clip, &sel_type, &sel);

		if(sel_type==MCLIP_SEL_TRACK) {
			MovieTrackingTrack *track= (MovieTrackingTrack*)sel;
			MovieTrackingMarker *marker= BKE_tracking_get_marker(track, user->framenr);

			if(marker->flag&MARKER_DISABLED) {
				scopes->track_disabled= 1;
			} else {
				ImBuf *ibuf= BKE_movieclip_acquire_ibuf(clip, user);

				scopes->track_disabled= 0;

				if(ibuf && ibuf->rect) {
					ImBuf *tmpibuf;

					tmpibuf= BKE_tracking_acquire_pattern_imbuf(ibuf, track, marker, 1, scopes->track_pos, NULL);

					if(tmpibuf->rect_float)
						IMB_rect_from_float(tmpibuf);

					if(tmpibuf->rect)
						scopes->track_preview= tmpibuf;
					else
						IMB_freeImBuf(tmpibuf);
				}

				IMB_freeImBuf(ibuf);
			}

			if((track->flag&TRACK_LOCKED)==0) {
				scopes->marker= marker;
				scopes->track= track;
				scopes->slide_scale[0]= track->pat_max[0]-track->pat_min[0];
				scopes->slide_scale[1]= track->pat_max[1]-track->pat_min[1];
			}
		}
	}

	scopes->framenr= user->framenr;
	scopes->ok= 1;
}

void free_movieclip(MovieClip *clip)
{
	free_buffers(clip);

	BKE_tracking_free(&clip->tracking);
}

void unlink_movieclip(Main *bmain, MovieClip *clip)
{
	bScreen *scr;
	ScrArea *area;
	SpaceLink *sl;

	/* text space */
	for(scr= bmain->screen.first; scr; scr= scr->id.next) {
		for(area= scr->areabase.first; area; area= area->next) {
			for(sl= area->spacedata.first; sl; sl= sl->next) {
				if(sl->spacetype==SPACE_CLIP) {
					SpaceClip *sc= (SpaceClip*) sl;

					if(sc->clip==clip) {
						sc->clip= NULL;
					}
				}
			}
		}
	}

	clip->id.us= 0;
}
