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

static int rendersize_to_proxy(MovieClip *clip, int flag)
{
	if((flag&MCLIP_USE_PROXY)==0)
		return IMB_PROXY_NONE;

	switch(clip->render_size) {
		case MCLIP_PROXY_RENDER_SIZE_25:
			return IMB_PROXY_25;

		case MCLIP_PROXY_RENDER_SIZE_50:
			return IMB_PROXY_50;

		case MCLIP_PROXY_RENDER_SIZE_75:
			return IMB_PROXY_75;

		case MCLIP_PROXY_RENDER_SIZE_FULL:
			return IMB_PROXY_NONE;
	}

	return IMB_PROXY_NONE;
}

static int rendersize_to_number(int render_size)
{
	switch(render_size) {
		case MCLIP_PROXY_RENDER_SIZE_25:
			return 25;

		case MCLIP_PROXY_RENDER_SIZE_50:
			return 50;

		case MCLIP_PROXY_RENDER_SIZE_75:
			return 75;

		case MCLIP_PROXY_RENDER_SIZE_FULL:
			return 100;
	}

	return 100;
}

static int get_timecode(MovieClip *clip, int flag)
{
	if((flag&MCLIP_USE_PROXY)==0)
		return IMB_TC_NONE;

	return clip->proxy.tc;
}

static void get_sequence_fname(MovieClip *clip, int framenr, char *name)
{
	unsigned short numlen;
	char head[FILE_MAX], tail[FILE_MAX];
	int offset;

	BLI_strncpy(name, clip->name, sizeof(clip->name));
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
}

/* supposed to work with sequences only */
static void get_proxy_fname(MovieClip *clip, int proxy_render_size, int framenr, char *name)
{
	int size= rendersize_to_number(proxy_render_size);
	char dir[FILE_MAX], curname[FILE_MAX], clipdir[FILE_MAX], clipfile[FILE_MAX];

	get_sequence_fname(clip, framenr, curname);
	BLI_split_dirfile(curname, clipdir, clipfile);

	if(clip->flag&MCLIP_USE_PROXY_CUSTOM_DIR) {
		strcpy(dir, clip->proxy.dir);
	} else {
		BLI_snprintf(dir, FILE_MAX, "%s/BL_proxy", clipdir);
	}

	BLI_snprintf(name, FILE_MAX, "%s/images/%d/%s_proxy", dir, size,  clipfile);

	BLI_path_abs(name, G.main->name);
	BLI_path_frame(name, 1, 0);

	strcat(name, ".jpg");
}

static ImBuf *movieclip_load_sequence_file(MovieClip *clip, int framenr, int flag)
{
	struct ImBuf *ibuf;
	char name[FILE_MAX];
	int loadflag, size;

	size= rendersize_to_number(clip->render_size);

	if(flag&MCLIP_USE_PROXY && size!=100) get_proxy_fname(clip, clip->render_size, framenr, name);
	else get_sequence_fname(clip, framenr, name);

	loadflag= IB_rect|IB_multilayer;

	/* read ibuf */
	ibuf= IMB_loadiffname(name, loadflag);

	return ibuf;
}

static ImBuf *movieclip_load_movie_file(MovieClip *clip, int framenr, int flag)
{
	ImBuf *ibuf= NULL;
	int tc= get_timecode(clip, flag);
	int proxy= rendersize_to_proxy(clip, flag);
	char str[FILE_MAX];

	if(!clip->anim) {
		BLI_strncpy(str, clip->name, FILE_MAX);

		if(clip->id.lib)
			BLI_path_abs(str, clip->id.lib->filepath);
		else
			BLI_path_abs(str, G.main->name);

		/* FIXME: make several stream accessible in image editor, too */
		clip->anim= openanim(str, IB_rect, 0);

		if(clip->anim) {
			if(clip->flag&MCLIP_USE_PROXY_CUSTOM_DIR) {
				char dir[FILE_MAX];
				strcpy(dir, clip->proxy.dir);
				BLI_path_abs(dir, G.main->name);
				IMB_anim_set_index_dir(clip->anim, dir);
			}
		}
	}

	if(clip->anim) {
		int dur= IMB_anim_get_duration(clip->anim, tc);
		int fra= framenr-1;

		dur= IMB_anim_get_duration(clip->anim, tc);
		fra= framenr-1;

		if(fra<0)
			fra= 0;

		if(fra>(dur-1))
			fra= dur-1;

		ibuf= IMB_anim_absolute(clip->anim, fra, tc, proxy);
	}

	return ibuf;
}

/*********************** image buffer cache *************************/

typedef struct MovieClipCache {
	/* regular moive cache */
	struct MovieCache *moviecache;

	/* cache for stable shot */
	int stable_framenr;
	float stable_loc[2], stable_scale, stable_angle;
	ImBuf *stableibuf;
} MovieClipCache;

typedef struct MovieClipImBufCacheKey {
	int framenr;
	int proxy;
} MovieClipImBufCacheKey;

static void moviecache_keydata(void *userkey, int *framenr, int *proxy)
{
	MovieClipImBufCacheKey *key= (MovieClipImBufCacheKey*)userkey;

	*framenr= key->framenr;
	*proxy= key->proxy;
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

	if(a->proxy<b->proxy) return -1;
	else if(a->proxy>b->proxy) return 1;

	return 0;
}

static ImBuf *get_imbuf_cache(MovieClip *clip, MovieClipUser *user, int flag)
{
	if(clip->cache) {
		MovieClipImBufCacheKey key;

		key.framenr= user?user->framenr:clip->lastframe;

		if(flag&MCLIP_USE_PROXY) key.proxy= rendersize_to_proxy(clip, flag);
		else key.proxy= IMB_PROXY_NONE;

		return BKE_moviecache_get(clip->cache->moviecache, &key);
	}

	return NULL;
}

static void put_imbuf_cache(MovieClip *clip, MovieClipUser *user, ImBuf *ibuf, int flag)
{
	MovieClipImBufCacheKey key;

	if(!clip->cache) {
		clip->cache= MEM_callocN(sizeof(MovieClipCache), "movieClipCache");

		clip->cache->moviecache= BKE_moviecache_create(sizeof(MovieClipImBufCacheKey), moviecache_hashhash,
				moviecache_hashcmp, moviecache_keydata);
	}

	key.framenr= user?user->framenr:clip->lastframe;

	if(flag&MCLIP_USE_PROXY) key.proxy= rendersize_to_proxy(clip, flag);
	else key.proxy= IMB_PROXY_NONE;

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
	clip->tracking.camera.pixel_aspect= 1.0f;
	clip->tracking.camera.units= CAMERA_UNITS_MM;

	clip->tracking.settings.tracker= TRACKER_SAD;
	clip->tracking.settings.frames_limit= 0;
	clip->tracking.settings.keyframe1= 1;
	clip->tracking.settings.keyframe2= 30;
	clip->tracking.settings.dist= 1;
	clip->tracking.settings.corr= 0.75;

	clip->tracking.stabilization.scaleinf= 1.f;
	clip->tracking.stabilization.locinf= 1.f;
	clip->tracking.stabilization.rotinf= 1.f;
	clip->tracking.stabilization.maxscale= 2.f;

	clip->proxy.build_size_flags= IMB_PROXY_25;
	clip->proxy.build_tc_flags= IMB_TC_RECORD_RUN|IMB_TC_FREE_RUN|IMB_TC_INTERPOLATED_REC_DATE_FREE_RUN;
	clip->proxy.quality= 90;
	clip->render_size= MCLIP_PROXY_RENDER_SIZE_FULL;

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

		clip->tracking.camera.focal= 24.f*width/clip->tracking.camera.sensor_width;
	}

	return clip;
}

static void real_ibuf_size(MovieClip *clip, ImBuf *ibuf, int *width, int *height)
{
	*width= ibuf->x;
	*height= ibuf->y;

	if(clip->flag&MCLIP_USE_PROXY) {
		switch(clip->render_size) {
			case MCLIP_PROXY_RENDER_SIZE_25:
				(*width)*= 4;
				(*height)*= 4;
				break;

			case MCLIP_PROXY_RENDER_SIZE_50:
				(*width)*= 2;
				(*height)*= 2;
				break;

			case MCLIP_PROXY_RENDER_SIZE_75:
				*width= ((float)*width)*4.f/3.f;
				*height= ((float)*height)*4.f/3.f;
				break;
               }
       }
}

ImBuf *BKE_movieclip_acquire_ibuf(MovieClip *clip, MovieClipUser *user)
{
	ImBuf *ibuf= NULL;
	int framenr= user?user->framenr:clip->lastframe;

	/* cache isn't threadsafe itself and also loading of movies
	   can't happen from concurent threads that's why we use lock here */
	BLI_lock_thread(LOCK_MOVIECLIP);

	/* cache is supposed to be threadsafe */
	ibuf= get_imbuf_cache(clip, user, clip->flag);

	if(!ibuf) {
		if(clip->source==MCLIP_SRC_SEQUENCE)
			ibuf= movieclip_load_sequence_file(clip, framenr, clip->flag);
		else {
			ibuf= movieclip_load_movie_file(clip, framenr, clip->flag);
		}

		if(ibuf)
			put_imbuf_cache(clip, user, ibuf, clip->flag);
	}

	if(ibuf) {
		clip->lastframe= framenr;

		real_ibuf_size(clip, ibuf, &clip->lastsize[0], &clip->lastsize[1]);
	}

	BLI_unlock_thread(LOCK_MOVIECLIP);

	return ibuf;
}

ImBuf *BKE_movieclip_acquire_ibuf_flag(MovieClip *clip, MovieClipUser *user, int flag)
{
	ImBuf *ibuf= NULL;
	int framenr= user?user->framenr:clip->lastframe;

	/* cache isn't threadsafe itself and also loading of movies
	   can't happen from concurent threads that's why we use lock here */
	BLI_lock_thread(LOCK_MOVIECLIP);

	/* cache is supposed to be threadsafe */
	ibuf= get_imbuf_cache(clip, user, flag);

	if(!ibuf) {
		if(clip->source==MCLIP_SRC_SEQUENCE) {
			ibuf= movieclip_load_sequence_file(clip, framenr, flag);
		} else {
			ibuf= movieclip_load_movie_file(clip, framenr, flag);
		}

		if(ibuf) {
			int bits= MCLIP_USE_PROXY|MCLIP_USE_PROXY_CUSTOM_DIR;

			if((flag&bits)==(clip->flag&bits))
				put_imbuf_cache(clip, user, ibuf, clip->flag);
		}
	}

	BLI_unlock_thread(LOCK_MOVIECLIP);

	return ibuf;
}

ImBuf *BKE_movieclip_acquire_stable_ibuf(MovieClip *clip, MovieClipUser *user, float loc[2], float *scale, float *angle)
{
	ImBuf *ibuf, *stableibuf= NULL;
	int framenr= user?user->framenr:clip->lastframe;

	ibuf= BKE_movieclip_acquire_ibuf(clip, user);

	if(clip->tracking.stabilization.flag&TRACKING_2D_STABILIZATION) {
		float tloc[2], tscale, tangle;

		if(clip->cache->stableibuf && clip->cache->stable_framenr==framenr) {
			stableibuf= clip->cache->stableibuf;

			BKE_tracking_stabilization_data(&clip->tracking, framenr, stableibuf->x, stableibuf->y, tloc, &tscale, &tangle);

			if(!equals_v2v2(tloc, clip->cache->stable_loc) || tscale!=clip->cache->stable_scale || tangle!=clip->cache->stable_angle) {
				stableibuf= NULL;
			}
		}

		if(!stableibuf) {
			if(clip->cache->stableibuf)
				IMB_freeImBuf(clip->cache->stableibuf);

			stableibuf= BKE_tracking_stabilize(&clip->tracking, framenr, ibuf, tloc, &tscale, &tangle);

			copy_v2_v2(clip->cache->stable_loc, tloc);
			clip->cache->stable_scale= tscale;
			clip->cache->stable_angle= tangle;
			clip->cache->stable_framenr= framenr;
			clip->cache->stableibuf= stableibuf;
		}

		IMB_refImBuf(stableibuf);

		if(loc)		copy_v2_v2(loc, tloc);
		if(scale)	*scale= tscale;
		if(angle)	*angle= tangle;
	} else {
		if(loc)		zero_v2(loc);
		if(scale)	*scale= 1.f;
		if(angle)	*angle= 0.f;

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
			real_ibuf_size(clip, ibuf, width, height);
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
	*aspy = clip->aspy/clip->aspx/clip->tracking.camera.pixel_aspect;
}

/* get segments of cached frames. useful for debugging cache policies */
void BKE_movieclip_get_cache_segments(MovieClip *clip, int *totseg_r, int **points_r)
{
	*totseg_r= 0;
	*points_r= NULL;

	if(clip->cache) {
		int proxy= rendersize_to_proxy(clip, clip->flag);

		BKE_moviecache_get_cache_segments(clip->cache->moviecache, proxy, totseg_r, points_r);
	}
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
}

void BKE_movieclip_deselect_track(MovieClip *clip, MovieTrackingTrack *track, int area)
{
	BKE_tracking_track_flag(track, area, SELECT, 1);
}

void BKE_movieclip_update_scopes(MovieClip *clip, MovieClipUser *user, MovieClipScopes *scopes)
{
	if(scopes->ok) return;

	if(scopes->track_preview) {
		IMB_freeImBuf(scopes->track_preview);
		scopes->track_preview= NULL;
	}

	scopes->marker= NULL;
	scopes->track= NULL;

	if(clip) {
		if(clip->tracking.act_track) {
			MovieTrackingTrack *track= clip->tracking.act_track;
			MovieTrackingMarker *marker= BKE_tracking_get_marker(track, user->framenr);

			if(marker->flag&MARKER_DISABLED) {
				scopes->track_disabled= 1;
			} else {
				ImBuf *ibuf= BKE_movieclip_acquire_ibuf(clip, user);

				scopes->track_disabled= 0;

				if(ibuf && ibuf->rect) {
					ImBuf *tmpibuf;

					tmpibuf= BKE_tracking_acquire_pattern_imbuf(ibuf, track, marker, 1, 1, scopes->track_pos, NULL);

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

void BKE_movieclip_build_proxy_frame(MovieClip *clip, int cfra, int proxy_render_size)
{
	char name[FILE_MAXFILE+FILE_MAXDIR];
	int quality, rectx, recty, size;
	ImBuf *ibuf;
	MovieClipUser user;

	size= rendersize_to_number(proxy_render_size);
	user.framenr= cfra;

	get_proxy_fname(clip, proxy_render_size, cfra, name);

	ibuf= BKE_movieclip_acquire_ibuf_flag(clip, &user, 0);

	if(ibuf) {
		ImBuf *scaleibuf= ibuf;

		rectx= ibuf->x*size/100.f;
		recty= ibuf->y*size/100.f;

		if (ibuf->x != rectx || ibuf->y != recty) {
			if(ibuf->refcounter)	/* means buffer came from cache and shouldn't be modified */
				scaleibuf= IMB_dupImBuf(ibuf);

			IMB_scalefastImBuf(scaleibuf, (short)rectx, (short)recty);
		}

		quality= clip->proxy.quality;
		scaleibuf->ftype= JPG | quality;

		/* unsupported feature only confuses other s/w */
		if(scaleibuf->depth==32)
			scaleibuf->depth= 24;

		BLI_lock_thread(LOCK_MOVIECLIP);

		BLI_make_existing_file(name);
		if(IMB_saveiff(scaleibuf, name, IB_rect)==0)
			perror(name);

		BLI_unlock_thread(LOCK_MOVIECLIP);

		IMB_freeImBuf(ibuf);
		if(scaleibuf!=ibuf) IMB_freeImBuf(scaleibuf);
	}
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
