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

/** \file blender/blenkernel/intern/movieclip.c
 *  \ingroup bke
 */


#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#ifndef WIN32
#  include <unistd.h>
#else
#  include <io.h>
#endif

#include <time.h>

#ifdef _WIN32
#  define open _open
#  define close _close
#endif

#include "MEM_guardedalloc.h"

#include "DNA_constraint_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "BLI_utildefines.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_math.h"
#include "BLI_threads.h"

#include "BKE_animsys.h"
#include "BKE_constraint.h"
#include "BKE_colortools.h"
#include "BKE_library.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_movieclip.h"
#include "BKE_node.h"
#include "BKE_image.h"  /* openanim */
#include "BKE_tracking.h"
#include "BKE_sequencer.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_moviecache.h"

#ifdef WITH_OPENEXR
#  include "intern/openexr/openexr_multi.h"
#endif

#include "NOD_composite.h"

/*********************** movieclip buffer loaders *************************/

static int sequence_guess_offset(const char *full_name, int head_len, unsigned short numlen)
{
	char num[FILE_MAX] = {0};

	BLI_strncpy(num, full_name + head_len, numlen + 1);

	return atoi(num);
}

static int rendersize_to_proxy(MovieClipUser *user, int flag)
{
	if ((flag & MCLIP_USE_PROXY) == 0)
		return IMB_PROXY_NONE;

	switch (user->render_size) {
		case MCLIP_PROXY_RENDER_SIZE_25:
			return IMB_PROXY_25;

		case MCLIP_PROXY_RENDER_SIZE_50:
			return IMB_PROXY_50;

		case MCLIP_PROXY_RENDER_SIZE_75:
			return IMB_PROXY_75;

		case MCLIP_PROXY_RENDER_SIZE_100:
			return IMB_PROXY_100;

		case MCLIP_PROXY_RENDER_SIZE_FULL:
			return IMB_PROXY_NONE;
	}

	return IMB_PROXY_NONE;
}

static int rendersize_to_number(int render_size)
{
	switch (render_size) {
		case MCLIP_PROXY_RENDER_SIZE_25:
			return 25;

		case MCLIP_PROXY_RENDER_SIZE_50:
			return 50;

		case MCLIP_PROXY_RENDER_SIZE_75:
			return 75;

		case MCLIP_PROXY_RENDER_SIZE_100:
			return 100;

		case MCLIP_PROXY_RENDER_SIZE_FULL:
			return 100;
	}

	return 100;
}

static int get_timecode(MovieClip *clip, int flag)
{
	if ((flag & MCLIP_USE_PROXY) == 0)
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
	 * autoguess offset for now. could be something smarter in the future
	 */
	offset = sequence_guess_offset(clip->name, strlen(head), numlen);

	if (numlen)
		BLI_stringenc(name, head, tail, numlen, offset + framenr - clip->start_frame + clip->frame_offset);
	else
		BLI_strncpy(name, clip->name, sizeof(clip->name));

	BLI_path_abs(name, ID_BLEND_PATH(G.main, &clip->id));
}

/* supposed to work with sequences only */
static void get_proxy_fname(MovieClip *clip, int proxy_render_size, bool undistorted, int framenr, char *name)
{
	int size = rendersize_to_number(proxy_render_size);
	char dir[FILE_MAX], clipdir[FILE_MAX], clipfile[FILE_MAX];
	int proxynr = framenr - clip->start_frame + 1 + clip->frame_offset;

	BLI_split_dirfile(clip->name, clipdir, clipfile, FILE_MAX, FILE_MAX);

	if (clip->flag & MCLIP_USE_PROXY_CUSTOM_DIR) {
		BLI_strncpy(dir, clip->proxy.dir, sizeof(dir));
	}
	else {
		BLI_snprintf(dir, FILE_MAX, "%s/BL_proxy", clipdir);
	}

	if (undistorted)
		BLI_snprintf(name, FILE_MAX, "%s/%s/proxy_%d_undistorted/%08d", dir, clipfile, size, proxynr);
	else
		BLI_snprintf(name, FILE_MAX, "%s/%s/proxy_%d/%08d", dir, clipfile, size, proxynr);

	BLI_path_abs(name, G.main->name);
	BLI_path_frame(name, 1, 0);

	strcat(name, ".jpg");
}

static ImBuf *movieclip_load_sequence_file(MovieClip *clip, MovieClipUser *user, int framenr, int flag)
{
	struct ImBuf *ibuf;
	char name[FILE_MAX];
	int loadflag, use_proxy = FALSE;
	char *colorspace;

	use_proxy = (flag & MCLIP_USE_PROXY) && user->render_size != MCLIP_PROXY_RENDER_SIZE_FULL;
	if (use_proxy) {
		int undistort = user->render_flag & MCLIP_PROXY_RENDER_UNDISTORT;
		get_proxy_fname(clip, user->render_size, undistort, framenr, name);

		/* Well, this is a bit weird, but proxies for movie sources
		 * are built in the same exact color space as the input,
		 *
		 * But image sequences are built in the display space.
		 */
		if (clip->source == MCLIP_SRC_MOVIE) {
			colorspace = clip->colorspace_settings.name;
		}
		else {
			colorspace = NULL;
		}
	}
	else {
		get_sequence_fname(clip, framenr, name);
		colorspace = clip->colorspace_settings.name;
	}

	loadflag = IB_rect | IB_multilayer | IB_alphamode_detect;

	/* read ibuf */
	ibuf = IMB_loadiffname(name, loadflag, colorspace);

#ifdef WITH_OPENEXR
	if (ibuf) {
		if (ibuf->ftype == OPENEXR && ibuf->userdata) {
			IMB_exr_close(ibuf->userdata);
			ibuf->userdata = NULL;
		}
	}
#endif

	return ibuf;
}

static void movieclip_open_anim_file(MovieClip *clip)
{
	char str[FILE_MAX];

	if (!clip->anim) {
		BLI_strncpy(str, clip->name, FILE_MAX);
		BLI_path_abs(str, ID_BLEND_PATH(G.main, &clip->id));

		/* FIXME: make several stream accessible in image editor, too */
		clip->anim = openanim(str, IB_rect, 0, clip->colorspace_settings.name);

		if (clip->anim) {
			if (clip->flag & MCLIP_USE_PROXY_CUSTOM_DIR) {
				char dir[FILE_MAX];
				BLI_strncpy(dir, clip->proxy.dir, sizeof(dir));
				BLI_path_abs(dir, G.main->name);
				IMB_anim_set_index_dir(clip->anim, dir);
			}
		}
	}
}

static ImBuf *movieclip_load_movie_file(MovieClip *clip, MovieClipUser *user, int framenr, int flag)
{
	ImBuf *ibuf = NULL;
	int tc = get_timecode(clip, flag);
	int proxy = rendersize_to_proxy(user, flag);

	movieclip_open_anim_file(clip);

	if (clip->anim) {
		int dur = IMB_anim_get_duration(clip->anim, tc);
		int fra = framenr - clip->start_frame + clip->frame_offset;

		ibuf = IMB_anim_absolute(clip->anim, fra, tc, proxy);
	}

	return ibuf;
}

static void movieclip_calc_length(MovieClip *clip)
{
	if (clip->source == MCLIP_SRC_MOVIE) {
		movieclip_open_anim_file(clip);

		if (clip->anim) {
			clip->len = IMB_anim_get_duration(clip->anim, clip->proxy.tc);
		}
	}
	else if (clip->source == MCLIP_SRC_SEQUENCE) {
		unsigned short numlen;
		char name[FILE_MAX], head[FILE_MAX], tail[FILE_MAX];

		BLI_stringdec(clip->name, head, tail, &numlen);

		if (numlen == 0) {
			/* there's no number group in file name, assume it's single framed sequence */
			clip->len = 1;
		}
		else {
			clip->len = 0;
			for (;;) {
				get_sequence_fname(clip, clip->len + clip->start_frame, name);

				if (BLI_exists(name))
					clip->len++;
				else
					break;
			}
		}
	}
}

/*********************** image buffer cache *************************/

typedef struct MovieClipCache {
	/* regular movie cache */
	struct MovieCache *moviecache;

	/* cached postprocessed shot */
	struct {
		ImBuf *ibuf;
		int framenr;
		int flag;

		/* cache for undistorted shot */
		float principal[2];
		float k1, k2, k3;
		short undistortion_used;

		int proxy;
		short render_flag;
	} postprocessed;

	/* cache for stable shot */
	struct {
		ImBuf *reference_ibuf;

		ImBuf *ibuf;
		int framenr;
		int postprocess_flag;

		float loc[2], scale, angle, aspect;
		int proxy, filter;
		short render_flag;
	} stabilized;

	int sequence_offset;
} MovieClipCache;

typedef struct MovieClipImBufCacheKey {
	int framenr;
	int proxy;
	short render_flag;
} MovieClipImBufCacheKey;

typedef struct MovieClipCachePriorityData {
	int framenr;
} MovieClipCachePriorityData;

static int user_frame_to_cache_frame(MovieClip *clip, int framenr)
{
	int index;

	index = framenr - clip->start_frame + clip->frame_offset;

	if (clip->source == MCLIP_SRC_SEQUENCE) {
		if (clip->cache->sequence_offset == -1) {
			unsigned short numlen;
			char head[FILE_MAX], tail[FILE_MAX];

			BLI_stringdec(clip->name, head, tail, &numlen);

			/* see comment in get_sequence_fname */
			clip->cache->sequence_offset = sequence_guess_offset(clip->name, strlen(head), numlen);
		}

		index += clip->cache->sequence_offset;
	}

	if (index < 0)
		return framenr - index;

	return framenr;
}

static void moviecache_keydata(void *userkey, int *framenr, int *proxy, int *render_flags)
{
	MovieClipImBufCacheKey *key = (MovieClipImBufCacheKey *)userkey;

	*framenr = key->framenr;
	*proxy = key->proxy;
	*render_flags = key->render_flag;
}

static unsigned int moviecache_hashhash(const void *keyv)
{
	MovieClipImBufCacheKey *key = (MovieClipImBufCacheKey *)keyv;
	int rval = key->framenr;

	return rval;
}

static int moviecache_hashcmp(const void *av, const void *bv)
{
	const MovieClipImBufCacheKey *a = (MovieClipImBufCacheKey *)av;
	const MovieClipImBufCacheKey *b = (MovieClipImBufCacheKey *)bv;

	if (a->framenr < b->framenr)
		return -1;
	else if (a->framenr > b->framenr)
		return 1;

	if (a->proxy < b->proxy)
		return -1;
	else if (a->proxy > b->proxy)
		return 1;

	if (a->render_flag < b->render_flag)
		return -1;
	else if (a->render_flag > b->render_flag)
		return 1;

	return 0;
}

static void *moviecache_getprioritydata(void *key_v)
{
	MovieClipImBufCacheKey *key = (MovieClipImBufCacheKey *) key_v;
	MovieClipCachePriorityData *priority_data;

	priority_data = MEM_callocN(sizeof(*priority_data), "movie cache clip priority data");
	priority_data->framenr = key->framenr;

	return priority_data;
}

static int moviecache_getitempriority(void *last_userkey_v, void *priority_data_v)
{
	MovieClipImBufCacheKey *last_userkey = (MovieClipImBufCacheKey *) last_userkey_v;
	MovieClipCachePriorityData *priority_data = (MovieClipCachePriorityData *) priority_data_v;

	return -abs(last_userkey->framenr - priority_data->framenr);
}

static void moviecache_prioritydeleter(void *priority_data_v)
{
	MovieClipCachePriorityData *priority_data = (MovieClipCachePriorityData *) priority_data_v;

	MEM_freeN(priority_data);
}

static ImBuf *get_imbuf_cache(MovieClip *clip, MovieClipUser *user, int flag)
{
	if (clip->cache) {
		MovieClipImBufCacheKey key;

		key.framenr = user_frame_to_cache_frame(clip, user->framenr);

		if (flag & MCLIP_USE_PROXY) {
			key.proxy = rendersize_to_proxy(user, flag);
			key.render_flag = user->render_flag;
		}
		else {
			key.proxy = IMB_PROXY_NONE;
			key.render_flag = 0;
		}

		return IMB_moviecache_get(clip->cache->moviecache, &key);
	}

	return NULL;
}

static bool has_imbuf_cache(MovieClip *clip, MovieClipUser *user, int flag)
{
	if (clip->cache) {
		MovieClipImBufCacheKey key;

		key.framenr = user_frame_to_cache_frame(clip, user->framenr);

		if (flag & MCLIP_USE_PROXY) {
			key.proxy = rendersize_to_proxy(user, flag);
			key.render_flag = user->render_flag;
		}
		else {
			key.proxy = IMB_PROXY_NONE;
			key.render_flag = 0;
		}

		return IMB_moviecache_has_frame(clip->cache->moviecache, &key);
	}

	return FALSE;
}

static bool put_imbuf_cache(MovieClip *clip, MovieClipUser *user, ImBuf *ibuf, int flag, bool destructive)
{
	MovieClipImBufCacheKey key;

	if (!clip->cache) {
		struct MovieCache *moviecache;

		// char cache_name[64];
		// BLI_snprintf(cache_name, sizeof(cache_name), "movie %s", clip->id.name);

		clip->cache = MEM_callocN(sizeof(MovieClipCache), "movieClipCache");

		moviecache = IMB_moviecache_create("movieclip", sizeof(MovieClipImBufCacheKey), moviecache_hashhash, moviecache_hashcmp);

		IMB_moviecache_set_getdata_callback(moviecache, moviecache_keydata);
		IMB_moviecache_set_priority_callback(moviecache, moviecache_getprioritydata, moviecache_getitempriority,
		                                     moviecache_prioritydeleter);

		clip->cache->moviecache = moviecache;
		clip->cache->sequence_offset = -1;
	}

	key.framenr = user_frame_to_cache_frame(clip, user->framenr);

	if (flag & MCLIP_USE_PROXY) {
		key.proxy = rendersize_to_proxy(user, flag);
		key.render_flag = user->render_flag;
	}
	else {
		key.proxy = IMB_PROXY_NONE;
		key.render_flag = 0;
	}

	if (destructive) {
		IMB_moviecache_put(clip->cache->moviecache, &key, ibuf);
		return true;
	}
	else {
		return IMB_moviecache_put_if_possible(clip->cache->moviecache, &key, ibuf);
	}
}

/*********************** common functions *************************/

/* only image block itself */
static MovieClip *movieclip_alloc(Main *bmain, const char *name)
{
	MovieClip *clip;

	clip = BKE_libblock_alloc(bmain, ID_MC, name);

	clip->aspx = clip->aspy = 1.0f;

	BKE_tracking_settings_init(&clip->tracking);
	BKE_color_managed_colorspace_settings_init(&clip->colorspace_settings);

	clip->proxy.build_size_flag = IMB_PROXY_25;
	clip->proxy.build_tc_flag = IMB_TC_RECORD_RUN |
	                            IMB_TC_FREE_RUN |
	                            IMB_TC_INTERPOLATED_REC_DATE_FREE_RUN |
	                            IMB_TC_RECORD_RUN_NO_GAPS;
	clip->proxy.quality = 90;

	clip->start_frame = 1;
	clip->frame_offset = 0;

	return clip;
}

static void movieclip_load_get_szie(MovieClip *clip)
{
	int width, height;
	MovieClipUser user = {0};

	user.framenr = 1;
	BKE_movieclip_get_size(clip, &user, &width, &height);

	if (width && height) {
		clip->tracking.camera.principal[0] = ((float)width) / 2.0f;
		clip->tracking.camera.principal[1] = ((float)height) / 2.0f;
	}
	else {
		clip->lastsize[0] = clip->lastsize[1] = IMG_SIZE_FALLBACK;
	}
}

static void detect_clip_source(MovieClip *clip)
{
	ImBuf *ibuf;
	char name[FILE_MAX];

	BLI_strncpy(name, clip->name, sizeof(name));
	BLI_path_abs(name, G.main->name);

	ibuf = IMB_testiffname(name, IB_rect | IB_multilayer);
	if (ibuf) {
		clip->source = MCLIP_SRC_SEQUENCE;
		IMB_freeImBuf(ibuf);
	}
	else {
		clip->source = MCLIP_SRC_MOVIE;
	}
}

/* checks if image was already loaded, then returns same image
 * otherwise creates new.
 * does not load ibuf itself
 * pass on optional frame for #name images */
MovieClip *BKE_movieclip_file_add(Main *bmain, const char *name)
{
	MovieClip *clip;
	int file, len;
	const char *libname;
	char str[FILE_MAX], strtest[FILE_MAX];

	BLI_strncpy(str, name, sizeof(str));
	BLI_path_abs(str, bmain->name);

	/* exists? */
	file = BLI_open(str, O_BINARY | O_RDONLY, 0);
	if (file < 0)
		return NULL;
	close(file);

	/* ** first search an identical clip ** */
	for (clip = bmain->movieclip.first; clip; clip = clip->id.next) {
		BLI_strncpy(strtest, clip->name, sizeof(clip->name));
		BLI_path_abs(strtest, G.main->name);

		if (strcmp(strtest, str) == 0) {
			BLI_strncpy(clip->name, name, sizeof(clip->name));  /* for stringcode */
			clip->id.us++;  /* officially should not, it doesn't link here! */

			return clip;
		}
	}

	/* ** add new movieclip ** */

	/* create a short library name */
	len = strlen(name);

	while (len > 0 && name[len - 1] != '/' && name[len - 1] != '\\')
		len--;
	libname = name + len;

	clip = movieclip_alloc(bmain, libname);
	BLI_strncpy(clip->name, name, sizeof(clip->name));

	detect_clip_source(clip);

	movieclip_load_get_szie(clip);
	if (clip->lastsize[0]) {
		int width = clip->lastsize[0];

		clip->tracking.camera.focal = 24.0f * width / clip->tracking.camera.sensor_width;
	}

	movieclip_calc_length(clip);

	return clip;
}

static void real_ibuf_size(MovieClip *clip, MovieClipUser *user, ImBuf *ibuf, int *width, int *height)
{
	*width = ibuf->x;
	*height = ibuf->y;

	if (clip->flag & MCLIP_USE_PROXY) {
		switch (user->render_size) {
			case MCLIP_PROXY_RENDER_SIZE_25:
				(*width) *= 4;
				(*height) *= 4;
				break;

			case MCLIP_PROXY_RENDER_SIZE_50:
				(*width) *= 2.0f;
				(*height) *= 2.0f;
				break;

			case MCLIP_PROXY_RENDER_SIZE_75:
				*width = ((float)*width) * 4.0f / 3.0f;
				*height = ((float)*height) * 4.0f / 3.0f;
				break;
		}
	}
}

static ImBuf *get_undistorted_ibuf(MovieClip *clip, struct MovieDistortion *distortion, ImBuf *ibuf)
{
	ImBuf *undistibuf;

	if (distortion)
		undistibuf = BKE_tracking_distortion_exec(distortion, &clip->tracking, ibuf, ibuf->x, ibuf->y, 0.0f, 1);
	else
		undistibuf = BKE_tracking_undistort_frame(&clip->tracking, ibuf, ibuf->x, ibuf->y, 0.0f);

	IMB_scaleImBuf(undistibuf, ibuf->x, ibuf->y);

	return undistibuf;
}

static int need_undistortion_postprocess(MovieClipUser *user)
{
	int result = 0;

	/* only full undistorted render can be used as on-fly undistorting image */
	result |= (user->render_size == MCLIP_PROXY_RENDER_SIZE_FULL) &&
	          (user->render_flag & MCLIP_PROXY_RENDER_UNDISTORT) != 0;

	return result;
}

static int need_postprocessed_frame(MovieClipUser *user, int postprocess_flag)
{
	int result = postprocess_flag;

	result |= need_undistortion_postprocess(user);

	return result;
}

static int check_undistortion_cache_flags(MovieClip *clip)
{
	MovieClipCache *cache = clip->cache;
	MovieTrackingCamera *camera = &clip->tracking.camera;

	/* check for distortion model changes */
	if (!equals_v2v2(camera->principal, cache->postprocessed.principal))
		return FALSE;

	if (!equals_v3v3(&camera->k1, &cache->postprocessed.k1))
		return FALSE;

	return TRUE;
}

static ImBuf *get_postprocessed_cached_frame(MovieClip *clip, MovieClipUser *user, int flag, int postprocess_flag)
{
	MovieClipCache *cache = clip->cache;
	int framenr = user->framenr;
	short proxy = IMB_PROXY_NONE;
	int render_flag = 0;

	if (flag & MCLIP_USE_PROXY) {
		proxy = rendersize_to_proxy(user, flag);
		render_flag = user->render_flag;
	}

	/* no cache or no cached postprocessed image */
	if (!clip->cache || !clip->cache->postprocessed.ibuf)
		return NULL;

	/* postprocessing happened for other frame */
	if (cache->postprocessed.framenr != framenr)
		return NULL;

	/* cached ibuf used different proxy settings */
	if (cache->postprocessed.render_flag != render_flag || cache->postprocessed.proxy != proxy)
		return NULL;

	if (cache->postprocessed.flag != postprocess_flag)
		return NULL;

	if (need_undistortion_postprocess(user)) {
		if (!check_undistortion_cache_flags(clip))
			return NULL;
	}
	else if (cache->postprocessed.undistortion_used)
		return NULL;

	IMB_refImBuf(cache->postprocessed.ibuf);

	return cache->postprocessed.ibuf;
}

static ImBuf *postprocess_frame(MovieClip *clip, MovieClipUser *user, ImBuf *ibuf, int postprocess_flag)
{
	ImBuf *postproc_ibuf = NULL;

	if (need_undistortion_postprocess(user)) {
		postproc_ibuf = get_undistorted_ibuf(clip, NULL, ibuf);
	}
	else {
		postproc_ibuf = IMB_dupImBuf(ibuf);
	}

	if (postprocess_flag) {
		bool disable_red   = (postprocess_flag & MOVIECLIP_DISABLE_RED) != 0,
			 disable_green = (postprocess_flag & MOVIECLIP_DISABLE_GREEN) != 0,
			 disable_blue  = (postprocess_flag & MOVIECLIP_DISABLE_BLUE) != 0,
			 grayscale     = (postprocess_flag & MOVIECLIP_PREVIEW_GRAYSCALE) != 0;

		if (disable_red || disable_green || disable_blue || grayscale)
			BKE_tracking_disable_channels(postproc_ibuf, disable_red, disable_green, disable_blue, 1);
	}

	return postproc_ibuf;
}

static void put_postprocessed_frame_to_cache(MovieClip *clip, MovieClipUser *user, ImBuf *ibuf,
                                               int flag, int postprocess_flag)
{
	MovieClipCache *cache = clip->cache;
	MovieTrackingCamera *camera = &clip->tracking.camera;

	cache->postprocessed.framenr = user->framenr;
	cache->postprocessed.flag = postprocess_flag;

	if (flag & MCLIP_USE_PROXY) {
		cache->postprocessed.proxy = rendersize_to_proxy(user, flag);
		cache->postprocessed.render_flag = user->render_flag;
	}
	else {
		cache->postprocessed.proxy = IMB_PROXY_NONE;
		cache->postprocessed.render_flag = 0;
	}

	if (need_undistortion_postprocess(user)) {
		copy_v2_v2(cache->postprocessed.principal, camera->principal);
		copy_v3_v3(&cache->postprocessed.k1, &camera->k1);
		cache->postprocessed.undistortion_used = TRUE;
	}
	else {
		cache->postprocessed.undistortion_used = FALSE;
	}

	IMB_refImBuf(ibuf);

	if (cache->postprocessed.ibuf)
		IMB_freeImBuf(cache->postprocessed.ibuf);

	cache->postprocessed.ibuf = ibuf;
}

static ImBuf *movieclip_get_postprocessed_ibuf(MovieClip *clip, MovieClipUser *user, int flag,
                                               int postprocess_flag, int cache_flag)
{
	ImBuf *ibuf = NULL;
	int framenr = user->framenr, need_postprocess = FALSE;

	/* cache isn't threadsafe itself and also loading of movies
	 * can't happen from concurent threads that's why we use lock here */
	BLI_lock_thread(LOCK_MOVIECLIP);

	/* try to obtain cached postprocessed frame first */
	if (need_postprocessed_frame(user, postprocess_flag)) {
		ibuf = get_postprocessed_cached_frame(clip, user, flag, postprocess_flag);

		if (!ibuf)
			need_postprocess = TRUE;
	}

	if (!ibuf)
		ibuf = get_imbuf_cache(clip, user, flag);

	if (!ibuf) {
		int use_sequence = FALSE;

		/* undistorted proxies for movies should be read as image sequence */
		use_sequence = (user->render_flag & MCLIP_PROXY_RENDER_UNDISTORT) &&
		               (user->render_size != MCLIP_PROXY_RENDER_SIZE_FULL);

		if (clip->source == MCLIP_SRC_SEQUENCE || use_sequence) {
			ibuf = movieclip_load_sequence_file(clip, user, framenr, flag);
		}
		else {
			ibuf = movieclip_load_movie_file(clip, user, framenr, flag);
		}

		if (ibuf && (cache_flag & MOVIECLIP_CACHE_SKIP) == 0)
			put_imbuf_cache(clip, user, ibuf, flag, true);
	}

	if (ibuf) {
		clip->lastframe = framenr;
		real_ibuf_size(clip, user, ibuf, &clip->lastsize[0], &clip->lastsize[1]);

		/* postprocess frame and put to cache if needed*/
		if (need_postprocess) {
			ImBuf *tmpibuf = ibuf;
			ibuf = postprocess_frame(clip, user, tmpibuf, postprocess_flag);
			IMB_freeImBuf(tmpibuf);
			if (ibuf && (cache_flag & MOVIECLIP_CACHE_SKIP) == 0) {
				put_postprocessed_frame_to_cache(clip, user, ibuf, flag, postprocess_flag);
			}
		}
	}

	BLI_unlock_thread(LOCK_MOVIECLIP);

	return ibuf;
}

ImBuf *BKE_movieclip_get_ibuf(MovieClip *clip, MovieClipUser *user)
{
	return BKE_movieclip_get_ibuf_flag(clip, user, clip->flag, 0);
}

ImBuf *BKE_movieclip_get_ibuf_flag(MovieClip *clip, MovieClipUser *user, int flag, int cache_flag)
{
	return movieclip_get_postprocessed_ibuf(clip, user, flag, 0, cache_flag);
}

ImBuf *BKE_movieclip_get_postprocessed_ibuf(MovieClip *clip, MovieClipUser *user, int postprocess_flag)
{
	return movieclip_get_postprocessed_ibuf(clip, user, clip->flag, postprocess_flag, 0);
}

static ImBuf *get_stable_cached_frame(MovieClip *clip, MovieClipUser *user, ImBuf *reference_ibuf,
                                      int framenr, int postprocess_flag)
{
	MovieClipCache *cache = clip->cache;
	MovieTracking *tracking = &clip->tracking;
	ImBuf *stableibuf;
	float tloc[2], tscale, tangle;
	short proxy = IMB_PROXY_NONE;
	int render_flag = 0;
	int clip_framenr = BKE_movieclip_remap_scene_to_clip_frame(clip, framenr);

	if (clip->flag & MCLIP_USE_PROXY) {
		proxy = rendersize_to_proxy(user, clip->flag);
		render_flag = user->render_flag;
	}

	/* there's no cached frame or it was calculated for another frame */
	if (!cache->stabilized.ibuf || cache->stabilized.framenr != framenr)
		return NULL;

	if (cache->stabilized.reference_ibuf != reference_ibuf)
		return NULL;

	/* cached ibuf used different proxy settings */
	if (cache->stabilized.render_flag != render_flag || cache->stabilized.proxy != proxy)
		return NULL;

	if (cache->stabilized.postprocess_flag != postprocess_flag)
		return NULL;

	/* stabilization also depends on pixel aspect ratio */
	if (cache->stabilized.aspect != tracking->camera.pixel_aspect)
		return NULL;

	if (cache->stabilized.filter != tracking->stabilization.filter)
		return NULL;

	stableibuf = cache->stabilized.ibuf;

	BKE_tracking_stabilization_data_get(&clip->tracking, clip_framenr, stableibuf->x, stableibuf->y, tloc, &tscale, &tangle);

	/* check for stabilization parameters */
	if (tscale != cache->stabilized.scale ||
	    tangle != cache->stabilized.angle ||
	    !equals_v2v2(tloc, cache->stabilized.loc))
	{
		return NULL;
	}

	IMB_refImBuf(stableibuf);

	return stableibuf;
}

static ImBuf *put_stabilized_frame_to_cache(MovieClip *clip, MovieClipUser *user, ImBuf *ibuf,
                                            int framenr, int postprocess_flag)
{
	MovieClipCache *cache = clip->cache;
	MovieTracking *tracking = &clip->tracking;
	ImBuf *stableibuf;
	float tloc[2], tscale, tangle;
	int clip_framenr = BKE_movieclip_remap_scene_to_clip_frame(clip, framenr);

	stableibuf = BKE_tracking_stabilize_frame(&clip->tracking, clip_framenr, ibuf, tloc, &tscale, &tangle);

	copy_v2_v2(cache->stabilized.loc, tloc);

	cache->stabilized.reference_ibuf = ibuf;
	cache->stabilized.scale = tscale;
	cache->stabilized.angle = tangle;
	cache->stabilized.framenr = framenr;
	cache->stabilized.aspect = tracking->camera.pixel_aspect;
	cache->stabilized.filter = tracking->stabilization.filter;

	if (clip->flag & MCLIP_USE_PROXY) {
		cache->stabilized.proxy = rendersize_to_proxy(user, clip->flag);
		cache->stabilized.render_flag = user->render_flag;
	}
	else {
		cache->stabilized.proxy = IMB_PROXY_NONE;
		cache->stabilized.render_flag = 0;
	}

	cache->stabilized.postprocess_flag = postprocess_flag;

	if (cache->stabilized.ibuf)
		IMB_freeImBuf(cache->stabilized.ibuf);

	cache->stabilized.ibuf = stableibuf;

	IMB_refImBuf(stableibuf);

	return stableibuf;
}

ImBuf *BKE_movieclip_get_stable_ibuf(MovieClip *clip, MovieClipUser *user, float loc[2], float *scale, float *angle,
                                     int postprocess_flag)
{
	ImBuf *ibuf, *stableibuf = NULL;
	int framenr = user->framenr;

	ibuf = BKE_movieclip_get_postprocessed_ibuf(clip, user, postprocess_flag);

	if (!ibuf)
		return NULL;

	if (clip->tracking.stabilization.flag & TRACKING_2D_STABILIZATION) {
		MovieClipCache *cache = clip->cache;

		stableibuf = get_stable_cached_frame(clip, user, ibuf, framenr, postprocess_flag);

		if (!stableibuf)
			stableibuf = put_stabilized_frame_to_cache(clip, user, ibuf, framenr, postprocess_flag);

		if (loc)
			copy_v2_v2(loc, cache->stabilized.loc);

		if (scale)
			*scale = cache->stabilized.scale;

		if (angle)
			*angle = cache->stabilized.angle;
	}
	else {
		if (loc)
			zero_v2(loc);

		if (scale)
			*scale = 1.0f;

		if (angle)
			*angle = 0.0f;

		stableibuf = ibuf;
	}

	if (stableibuf != ibuf) {
		IMB_freeImBuf(ibuf);
		ibuf = stableibuf;
	}

	return ibuf;

}

bool BKE_movieclip_has_frame(MovieClip *clip, MovieClipUser *user)
{
	ImBuf *ibuf = BKE_movieclip_get_ibuf(clip, user);

	if (ibuf) {
		IMB_freeImBuf(ibuf);
		return TRUE;
	}

	return FALSE;
}

void BKE_movieclip_get_size(MovieClip *clip, MovieClipUser *user, int *width, int *height)
{
#if 0
	/* originally was needed to support image sequences with different image dimensions,
	 * which might be useful for such things as reconstruction of unordered image sequence,
	 * or painting/rotoscoping of non-equal-sized images, but this ended up in unneeded
	 * cache lookups and even unwanted non-proxied files loading when doing mask parenting,
	 * so let's disable this for now and assume image sequence consists of images with
	 * equal sizes (sergey)
	 */
	if (user->framenr == clip->lastframe) {
#endif
	if (clip->lastsize[0] != 0 && clip->lastsize[1] != 0) {
		*width = clip->lastsize[0];
		*height = clip->lastsize[1];
	}
	else {
		ImBuf *ibuf = BKE_movieclip_get_ibuf(clip, user);

		if (ibuf && ibuf->x && ibuf->y) {
			real_ibuf_size(clip, user, ibuf, width, height);
		}
		else {
			*width = clip->lastsize[0];
			*height = clip->lastsize[1];
		}

		if (ibuf)
			IMB_freeImBuf(ibuf);
	}
}
void BKE_movieclip_get_size_fl(MovieClip *clip, MovieClipUser *user, float size[2])
{
	int width, height;
	BKE_movieclip_get_size(clip, user, &width, &height);

	size[0] = (float)width;
	size[1] = (float)height;
}

int BKE_movieclip_get_duration(MovieClip *clip)
{
	if (!clip->len) {
		movieclip_calc_length(clip);
	}

	return clip->len;
}

void BKE_movieclip_get_aspect(MovieClip *clip, float *aspx, float *aspy)
{
	*aspx = 1.0;

	/* x is always 1 */
	*aspy = clip->aspy / clip->aspx / clip->tracking.camera.pixel_aspect;
}

/* get segments of cached frames. useful for debugging cache policies */
void BKE_movieclip_get_cache_segments(MovieClip *clip, MovieClipUser *user, int *totseg_r, int **points_r)
{
	*totseg_r = 0;
	*points_r = NULL;

	if (clip->cache) {
		int proxy = rendersize_to_proxy(user, clip->flag);

		IMB_moviecache_get_cache_segments(clip->cache->moviecache, proxy, user->render_flag, totseg_r, points_r);
	}
}

void BKE_movieclip_user_set_frame(MovieClipUser *iuser, int framenr)
{
	/* TODO: clamp framenr here? */

	iuser->framenr = framenr;
}

static void free_buffers(MovieClip *clip)
{
	if (clip->cache) {
		IMB_moviecache_free(clip->cache->moviecache);

		if (clip->cache->postprocessed.ibuf)
			IMB_freeImBuf(clip->cache->postprocessed.ibuf);

		if (clip->cache->stabilized.ibuf)
			IMB_freeImBuf(clip->cache->stabilized.ibuf);

		MEM_freeN(clip->cache);
		clip->cache = NULL;
	}

	if (clip->anim) {
		IMB_free_anim(clip->anim);
		clip->anim = NULL;
	}

	BKE_free_animdata((ID *) clip);
}

void BKE_movieclip_clear_cache(MovieClip *clip)
{
	free_buffers(clip);
}

void BKE_movieclip_reload(MovieClip *clip)
{
	/* clear cache */
	free_buffers(clip);

	clip->tracking.stabilization.ok = FALSE;

	/* update clip source */
	detect_clip_source(clip);

	clip->lastsize[0] = clip->lastsize[1] = 0;
	movieclip_load_get_szie(clip);

	movieclip_calc_length(clip);

	/* same as for image update -- don't use notifiers because they are not 100% sure to succeeded
	 * (node trees which are not currently visible wouldn't be refreshed)
	 */
	{
		Scene *scene;
		for (scene = G.main->scene.first; scene; scene = scene->id.next) {
			if (scene->nodetree) {
				nodeUpdateID(scene->nodetree, &clip->id);
			}
		}
	}
}

void BKE_movieclip_update_scopes(MovieClip *clip, MovieClipUser *user, MovieClipScopes *scopes)
{
	if (scopes->ok)
		return;

	if (scopes->track_preview) {
		IMB_freeImBuf(scopes->track_preview);
		scopes->track_preview = NULL;
	}

	if (scopes->track_search) {
		IMB_freeImBuf(scopes->track_search);
		scopes->track_search = NULL;
	}

	scopes->marker = NULL;
	scopes->track = NULL;
	scopes->track_locked = TRUE;

	if (clip) {
		MovieTrackingTrack *act_track = BKE_tracking_track_get_active(&clip->tracking);

		if (act_track) {
			MovieTrackingTrack *track = act_track;
			int framenr = BKE_movieclip_remap_scene_to_clip_frame(clip, user->framenr);
			MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);

			scopes->marker = marker;
			scopes->track = track;

			if (marker->flag & MARKER_DISABLED) {
				scopes->track_disabled = TRUE;
			}
			else {
				ImBuf *ibuf = BKE_movieclip_get_ibuf(clip, user);

				scopes->track_disabled = FALSE;

				if (ibuf && (ibuf->rect || ibuf->rect_float)) {
					ImBuf *search_ibuf;
					MovieTrackingMarker undist_marker = *marker;

					if (user->render_flag & MCLIP_PROXY_RENDER_UNDISTORT) {
						int width, height;
						float aspy = 1.0f / clip->tracking.camera.pixel_aspect;

						BKE_movieclip_get_size(clip, user, &width, &height);

						undist_marker.pos[0] *= width;
						undist_marker.pos[1] *= height * aspy;

						BKE_tracking_undistort_v2(&clip->tracking, undist_marker.pos, undist_marker.pos);

						undist_marker.pos[0] /= width;
						undist_marker.pos[1] /= height * aspy;
					}

					search_ibuf = BKE_tracking_get_search_imbuf(ibuf, track, &undist_marker, TRUE, TRUE);

					if (search_ibuf) {
						if (!search_ibuf->rect_float) {
							/* sampling happens in float buffer */
							IMB_float_from_rect(search_ibuf);
						}

						scopes->track_search = search_ibuf;
					}

					scopes->undist_marker = undist_marker;

					scopes->frame_width = ibuf->x;
					scopes->frame_height = ibuf->y;

					scopes->use_track_mask = track->flag & TRACK_PREVIEW_ALPHA;
				}

				IMB_freeImBuf(ibuf);
			}

			if ((track->flag & TRACK_LOCKED) == 0) {
				float pat_min[2], pat_max[2];

				scopes->track_locked = FALSE;

				/* XXX: would work fine with non-transformed patterns, but would likely fail
				 *      with transformed patterns, but that would be easier to debug when
				 *      we'll have real pattern sampling (at least to test) */
				BKE_tracking_marker_pattern_minmax(marker, pat_min, pat_max);

				scopes->slide_scale[0] = pat_max[0] - pat_min[0];
				scopes->slide_scale[1] = pat_max[1] - pat_min[1];
			}
		}
	}

	scopes->framenr = user->framenr;
	scopes->ok = TRUE;
}

static void movieclip_build_proxy_ibuf(MovieClip *clip, ImBuf *ibuf, int cfra, int proxy_render_size, bool undistorted, bool threaded)
{
	char name[FILE_MAX];
	int quality, rectx, recty;
	int size = rendersize_to_number(proxy_render_size);
	ImBuf *scaleibuf;

	get_proxy_fname(clip, proxy_render_size, undistorted, cfra, name);

	rectx = ibuf->x * size / 100.0f;
	recty = ibuf->y * size / 100.0f;

	scaleibuf = IMB_dupImBuf(ibuf);

	if (threaded)
		IMB_scaleImBuf_threaded(scaleibuf, (short)rectx, (short)recty);
	else
		IMB_scaleImBuf(scaleibuf, (short)rectx, (short)recty);

	quality = clip->proxy.quality;
	scaleibuf->ftype = JPG | quality;

	/* unsupported feature only confuses other s/w */
	if (scaleibuf->planes == 32)
		scaleibuf->planes = 24;

	/* TODO: currently the most weak part of multithreaded proxies,
	 *       could be solved in a way that thread only prepares memory
	 *       buffer and write to disk happens separately
	 */
	BLI_lock_thread(LOCK_MOVIECLIP);

	BLI_make_existing_file(name);
	if (IMB_saveiff(scaleibuf, name, IB_rect) == 0)
		perror(name);

	BLI_unlock_thread(LOCK_MOVIECLIP);

	IMB_freeImBuf(scaleibuf);
}

/* note: currently used by proxy job for movies, threading happens within single frame
 * (meaning scaling shall be threaded)
 */
void BKE_movieclip_build_proxy_frame(MovieClip *clip, int clip_flag, struct MovieDistortion *distortion,
                                     int cfra, int *build_sizes, int build_count, bool undistorted)
{
	ImBuf *ibuf;
	MovieClipUser user;

	if (!build_count)
		return;

	user.framenr = cfra;
	user.render_flag = 0;
	user.render_size = MCLIP_PROXY_RENDER_SIZE_FULL;

	ibuf = BKE_movieclip_get_ibuf_flag(clip, &user, clip_flag, MOVIECLIP_CACHE_SKIP);

	if (ibuf) {
		ImBuf *tmpibuf = ibuf;
		int i;

		if (undistorted)
			tmpibuf = get_undistorted_ibuf(clip, distortion, ibuf);

		for (i = 0; i < build_count; i++)
			movieclip_build_proxy_ibuf(clip, tmpibuf, cfra, build_sizes[i], undistorted, true);

		IMB_freeImBuf(ibuf);

		if (tmpibuf != ibuf)
			IMB_freeImBuf(tmpibuf);
	}
}

/* note: currently used by proxy job for sequences, threading happens within sequence
 * (different threads handles different frames, no threading within frame is needed)
 */
void BKE_movieclip_build_proxy_frame_for_ibuf(MovieClip *clip, ImBuf *ibuf, struct MovieDistortion *distortion,
                                              int cfra, int *build_sizes, int build_count, bool undistorted)
{
	if (!build_count)
		return;

	if (ibuf) {
		ImBuf *tmpibuf = ibuf;
		int i;

		if (undistorted)
			tmpibuf = get_undistorted_ibuf(clip, distortion, ibuf);

		for (i = 0; i < build_count; i++)
			movieclip_build_proxy_ibuf(clip, tmpibuf, cfra, build_sizes[i], undistorted, false);

		if (tmpibuf != ibuf)
			IMB_freeImBuf(tmpibuf);
	}
}

void BKE_movieclip_free(MovieClip *clip)
{
	free_buffers(clip);

	BKE_tracking_free(&clip->tracking);
}

void BKE_movieclip_unlink(Main *bmain, MovieClip *clip)
{
	bScreen *scr;
	ScrArea *area;
	SpaceLink *sl;
	Scene *sce;
	Object *ob;

	for (scr = bmain->screen.first; scr; scr = scr->id.next) {
		for (area = scr->areabase.first; area; area = area->next) {
			for (sl = area->spacedata.first; sl; sl = sl->next) {
				if (sl->spacetype == SPACE_CLIP) {
					SpaceClip *sc = (SpaceClip *) sl;

					if (sc->clip == clip)
						sc->clip = NULL;
				}
				else if (sl->spacetype == SPACE_VIEW3D) {
					View3D *v3d = (View3D *) sl;
					BGpic *bgpic;

					for (bgpic = v3d->bgpicbase.first; bgpic; bgpic = bgpic->next) {
						if (bgpic->clip == clip)
							bgpic->clip = NULL;
					}
				}
			}
		}
	}

	for (sce = bmain->scene.first; sce; sce = sce->id.next) {
		if (sce->clip == clip)
			sce->clip = NULL;
	}

	for (ob = bmain->object.first; ob; ob = ob->id.next) {
		bConstraint *con;

		for (con = ob->constraints.first; con; con = con->next) {
			if (con->type == CONSTRAINT_TYPE_FOLLOWTRACK) {
				bFollowTrackConstraint *data = (bFollowTrackConstraint *) con->data;

				if (data->clip == clip)
					data->clip = NULL;
			}
			else if (con->type == CONSTRAINT_TYPE_CAMERASOLVER) {
				bCameraSolverConstraint *data = (bCameraSolverConstraint *) con->data;

				if (data->clip == clip)
					data->clip = NULL;
			}
			else if (con->type == CONSTRAINT_TYPE_OBJECTSOLVER) {
				bObjectSolverConstraint *data = (bObjectSolverConstraint *) con->data;

				if (data->clip == clip)
					data->clip = NULL;
			}
		}
	}

	FOREACH_NODETREE(bmain, ntree, id) {
		BKE_node_tree_unlink_id((ID *)clip, ntree);
	} FOREACH_NODETREE_END

	clip->id.us = 0;
}

float BKE_movieclip_remap_scene_to_clip_frame(MovieClip *clip, float framenr)
{
	return framenr - (float) clip->start_frame + 1.0f;
}

float BKE_movieclip_remap_clip_to_scene_frame(MovieClip *clip, float framenr)
{
	return framenr + (float) clip->start_frame - 1.0f;
}

void BKE_movieclip_filename_for_frame(MovieClip *clip, MovieClipUser *user, char *name)
{
	if (clip->source == MCLIP_SRC_SEQUENCE) {
		int use_proxy;

		use_proxy = (clip->flag & MCLIP_USE_PROXY) && user->render_size != MCLIP_PROXY_RENDER_SIZE_FULL;

		if (use_proxy) {
			int undistort = user->render_flag & MCLIP_PROXY_RENDER_UNDISTORT;
			get_proxy_fname(clip, user->render_size, undistort, user->framenr, name);
		}
		else {
			get_sequence_fname(clip, user->framenr, name);
		}
	}
	else {
		BLI_strncpy(name, clip->name, FILE_MAX);
		BLI_path_abs(name, ID_BLEND_PATH(G.main, &clip->id));
	}
}

ImBuf *BKE_movieclip_anim_ibuf_for_frame(MovieClip *clip, MovieClipUser *user)
{
	ImBuf *ibuf = NULL;

	if (clip->source == MCLIP_SRC_MOVIE) {
		BLI_lock_thread(LOCK_MOVIECLIP);
		ibuf = movieclip_load_movie_file(clip, user, user->framenr, clip->flag);
		BLI_unlock_thread(LOCK_MOVIECLIP);
	}

	return ibuf;
}

bool BKE_movieclip_has_cached_frame(MovieClip *clip, MovieClipUser *user)
{
	bool has_frame = false;

	BLI_lock_thread(LOCK_MOVIECLIP);
	has_frame = has_imbuf_cache(clip, user, clip->flag);
	BLI_unlock_thread(LOCK_MOVIECLIP);

	return has_frame;
}

bool BKE_movieclip_put_frame_if_possible(MovieClip *clip, MovieClipUser *user, ImBuf *ibuf)
{
	bool result;

	BLI_lock_thread(LOCK_MOVIECLIP);
	result = put_imbuf_cache(clip, user, ibuf, clip->flag, false);
	BLI_unlock_thread(LOCK_MOVIECLIP);

	return result;
}
