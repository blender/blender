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
 * The Original Code is Copyright (C) 2012 by Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Xavier Thomas,
 *                 Lukas Toenne,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

#include "IMB_colormanagement.h"
#include "IMB_colormanagement_intern.h"

#include <string.h>
#include <math.h>

#include "DNA_color_types.h"
#include "DNA_image_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "IMB_filter.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_filetype.h"
#include "IMB_moviecache.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_math_color.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_threads.h"

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_utildefines.h"
#include "BKE_main.h"

#include "RNA_define.h"

#ifdef WITH_OCIO
#  include <ocio_capi.h>
#else
/* so function can accept processor and care about disabled OCIO inside */
typedef struct ConstProcessorRcPtr {
	int pad;
} ConstProcessorRcPtr;
#endif

/*********************** Global declarations *************************/

#define MAX_COLORSPACE_NAME 64

/* ** list of all supported color spaces, displays and views */
#ifdef WITH_OCIO
static char global_role_scene_linear[MAX_COLORSPACE_NAME];
static char global_role_color_picking[MAX_COLORSPACE_NAME];
static char global_role_texture_painting[MAX_COLORSPACE_NAME];
static char global_role_default_byte[MAX_COLORSPACE_NAME];
static char global_role_default_float[MAX_COLORSPACE_NAME];
static char global_role_default_sequencer[MAX_COLORSPACE_NAME];
#endif

static ListBase global_colorspaces = {NULL};
static ListBase global_displays = {NULL};
static ListBase global_views = {NULL};

static int global_tot_colorspace = 0;
static int global_tot_display = 0;
static int global_tot_view = 0;

typedef struct ColormanageProcessor {
	ConstProcessorRcPtr *processor;
	CurveMapping *curve_mapping;

#ifndef WITH_OCIO
	/* this callback is only used in cases when Blender was build without OCIO
	 * and aimed to preserve compatibility with previous Blender versions
	 */
	void (*display_transform_cb_v3) (float result[3], const float pixel[3]);
	void (*display_transform_predivide_cb_v4) (float result[4], const float pixel[4]);
#endif
} ColormanageProcessor;

/*********************** Color managed cache *************************/

/* Cache Implementation Notes
 * ==========================
 *
 * All color management cache stuff is stored in two properties of
 * image buffers:
 *
 *   1. display_buffer_flags
 *
 *      This is a bit field which used to mark calculated transformations
 *      for particular image buffer. Index inside of this array means index
 *      of a color managed display. Element with given index matches view
 *      transformations applied for a given display. So if bit B of array
 *      element B is set to 1, this means display buffer with display index
 *      of A and view transform of B was ever calculated for this imbuf.
 *
 *      In contrast with indices in global lists of displays and views this
 *      indices are 0-based, not 1-based. This is needed to save some bytes
 *      of memory.
 *
 *   2. colormanage_cache
 *
 *      This is a pointer to a structure which holds all data which is
 *      needed for color management cache to work.
 *
 *      It contains two parts:
 *        - data
 *        - moviecache
 *
 *      Data field is used to store additional information about cached
 *      buffers which affects on whether cached buffer could be used.
 *      This data can't go to cache key because changes in this data
 *      shouldn't lead extra buffers adding to cache, it shall
 *      invalidate cached images.
 *
 *      Currently such a data contains only exposure and gamma, but
 *      would likely extended further.
 *
 *      data field is not null only for elements of cache, not used for
 *      original image buffers.
 *
 *      Color management cache is using generic MovieCache implementation
 *      to make it easier to deal with memory limitation.
 *
 *      Currently color management is using the same memory limitation
 *      pool as sequencer and clip editor are using which means color
 *      managed buffers would be removed from the cache as soon as new
 *      frames are loading for the movie clip and there's no space in
 *      cache.
 *
 *      Every image buffer has got own movie cache instance, which
 *      means keys for color managed buffers could be really simple
 *      and look up in this cache would be fast and independent from
 *      overall amount of color managed images.
 */

/* NOTE: ColormanageCacheViewSettings and ColormanageCacheDisplaySettings are
 *       quite the same as ColorManagedViewSettings and ColorManageDisplaySettings
 *       but they holds indexes of all transformations and color spaces, not
 *       their names.
 *
 *       This helps avoid extra colorsmace / display / view lookup without
 *       requiring to pass all variables which affects on display buffer
 *       to color management cache system and keeps calls small and nice.
 */
typedef struct ColormanageCacheViewSettings {
	int flag;
	int view;
	float exposure;
	float gamma;
	CurveMapping *curve_mapping;
} ColormanageCacheViewSettings;

typedef struct ColormanageCacheDisplaySettings {
	int display;
} ColormanageCacheDisplaySettings;

typedef struct ColormanageCacheKey {
	int view;            /* view transformation used for display buffer */
	int display;         /* display device name */
} ColormanageCacheKey;

typedef struct ColormnaageCacheData {
	int flag;        /* view flags of cached buffer */
	float exposure;  /* exposure value cached buffer is calculated with */
	float gamma;     /* gamma value cached buffer is calculated with */
	int predivide;   /* predivide flag of cached buffer */
	CurveMapping *curve_mapping;  /* curve mapping used for cached buffer */
	int curve_mapping_timestamp;  /* time stamp of curve mapping used for cached buffer */
} ColormnaageCacheData;

typedef struct ColormanageCache {
	struct MovieCache *moviecache;

	ColormnaageCacheData *data;
} ColormanageCache;

static struct MovieCache *colormanage_moviecache_get(const ImBuf *ibuf)
{
	if (!ibuf->colormanage_cache)
		return NULL;

	return ibuf->colormanage_cache->moviecache;
}

static ColormnaageCacheData *colormanage_cachedata_get(const ImBuf *ibuf)
{
	if (!ibuf->colormanage_cache)
		return NULL;

	return ibuf->colormanage_cache->data;
}

#ifdef WITH_OCIO
static unsigned int colormanage_hashhash(const void *key_v)
{
	ColormanageCacheKey *key = (ColormanageCacheKey *)key_v;

	unsigned int rval = (key->display << 16) | (key->view % 0xffff);

	return rval;
}

static int colormanage_hashcmp(const void *av, const void *bv)
{
	const ColormanageCacheKey *a = (ColormanageCacheKey *) av;
	const ColormanageCacheKey *b = (ColormanageCacheKey *) bv;

	if (a->view < b->view)
		return -1;
	else if (a->view > b->view)
		return 1;

	if (a->display < b->display)
		return -1;
	else if (a->display > b->display)
		return 1;

	return 0;
}

static struct MovieCache *colormanage_moviecache_ensure(ImBuf *ibuf)
{
	if (!ibuf->colormanage_cache)
		ibuf->colormanage_cache = MEM_callocN(sizeof(ColormanageCache), "imbuf colormanage cache");

	if (!ibuf->colormanage_cache->moviecache) {
		struct MovieCache *moviecache;

		moviecache = IMB_moviecache_create("colormanage cache", sizeof(ColormanageCacheKey),
		                                   colormanage_hashhash, colormanage_hashcmp);

		ibuf->colormanage_cache->moviecache = moviecache;
	}

	return ibuf->colormanage_cache->moviecache;
}

static void colormanage_cachedata_set(ImBuf *ibuf, ColormnaageCacheData *data)
{
	if (!ibuf->colormanage_cache)
		ibuf->colormanage_cache = MEM_callocN(sizeof(ColormanageCache), "imbuf colormanage cache");

	ibuf->colormanage_cache->data = data;
}

static void colormanage_view_settings_to_cache(ColormanageCacheViewSettings *cache_view_settings,
                                               const ColorManagedViewSettings *view_settings)
{
	int view = IMB_colormanagement_view_get_named_index(view_settings->view_transform);

	cache_view_settings->view = view;
	cache_view_settings->exposure = view_settings->exposure;
	cache_view_settings->gamma = view_settings->gamma;
	cache_view_settings->flag = view_settings->flag;
	cache_view_settings->curve_mapping = view_settings->curve_mapping;
}

static void colormanage_display_settings_to_cache(ColormanageCacheDisplaySettings *cache_display_settings,
                                                  const ColorManagedDisplaySettings *display_settings)
{
	int display = IMB_colormanagement_display_get_named_index(display_settings->display_device);

	cache_display_settings->display = display;
}

static void colormanage_settings_to_key(ColormanageCacheKey *key,
                                        const ColormanageCacheViewSettings *view_settings,
                                        const ColormanageCacheDisplaySettings *display_settings)
{
	key->view = view_settings->view;
	key->display = display_settings->display;
}

static ImBuf *colormanage_cache_get_ibuf(ImBuf *ibuf, ColormanageCacheKey *key, void **cache_handle)
{
	ImBuf *cache_ibuf;
	struct MovieCache *moviecache = colormanage_moviecache_get(ibuf);

	if (!moviecache) {
		/* if there's no moviecache it means no color management was applied on given image buffer before */

		return NULL;
	}

	*cache_handle = NULL;

	cache_ibuf = IMB_moviecache_get(moviecache, key);

	*cache_handle = cache_ibuf;

	return cache_ibuf;
}

static unsigned char *colormanage_cache_get(ImBuf *ibuf, const ColormanageCacheViewSettings *view_settings,
                                            const ColormanageCacheDisplaySettings *display_settings,
                                            void **cache_handle)
{
	ColormanageCacheKey key;
	ImBuf *cache_ibuf;
	int view_flag = 1 << (view_settings->view - 1);
	int predivide = ibuf->flags & IB_cm_predivide;
	CurveMapping *curve_mapping = view_settings->curve_mapping;
	int curve_mapping_timestamp = curve_mapping ? curve_mapping->changed_timestamp : 0;

	colormanage_settings_to_key(&key, view_settings, display_settings);

	/* check whether image was marked as dirty for requested transform */
	if ((ibuf->display_buffer_flags[display_settings->display - 1] & view_flag) == 0) {
		return NULL;
	}

	cache_ibuf = colormanage_cache_get_ibuf(ibuf, &key, cache_handle);

	if (cache_ibuf) {
		ColormnaageCacheData *cache_data;

		BLI_assert(cache_ibuf->x == ibuf->x &&
		           cache_ibuf->y == ibuf->y &&
		           cache_ibuf->channels == ibuf->channels);

		/* only buffers with different color space conversions are being stored
		 * in cache separately. buffer which were used only different exposure/gamma
		 * are re-suing the same cached buffer
		 *
		 * check here which exposure/gamma/curve was used for cached buffer and if they're
		 * different from requested buffer should be re-generated
		 */
		cache_data = colormanage_cachedata_get(cache_ibuf);

		if (cache_data->exposure != view_settings->exposure ||
		    cache_data->gamma != view_settings->gamma ||
			cache_data->predivide != predivide ||
			cache_data->flag != view_settings->flag ||
			cache_data->curve_mapping != curve_mapping ||
			cache_data->curve_mapping_timestamp != curve_mapping_timestamp)
		{
			*cache_handle = NULL;

			IMB_freeImBuf(cache_ibuf);

			return NULL;
		}

		return (unsigned char *) cache_ibuf->rect;
	}

	return NULL;
}

static void colormanage_cache_put(ImBuf *ibuf, const ColormanageCacheViewSettings *view_settings,
                                  const ColormanageCacheDisplaySettings *display_settings,
                                  unsigned char *display_buffer, void **cache_handle)
{
	ColormanageCacheKey key;
	ImBuf *cache_ibuf;
	ColormnaageCacheData *cache_data;
	int view_flag = 1 << (view_settings->view - 1);
	int predivide = ibuf->flags & IB_cm_predivide;
	struct MovieCache *moviecache = colormanage_moviecache_ensure(ibuf);
	CurveMapping *curve_mapping = view_settings->curve_mapping;
	int curve_mapping_timestamp = curve_mapping ? curve_mapping->changed_timestamp : 0;

	colormanage_settings_to_key(&key, view_settings, display_settings);

	/* mark display buffer as valid */
	ibuf->display_buffer_flags[display_settings->display - 1] |= view_flag;

	/* buffer itself */
	cache_ibuf = IMB_allocImBuf(ibuf->x, ibuf->y, ibuf->planes, 0);
	cache_ibuf->rect = (unsigned int *) display_buffer;

	cache_ibuf->mall |= IB_rect;
	cache_ibuf->flags |= IB_rect;

	/* store data which is needed to check whether cached buffer could be used for color managed display settings */
	cache_data = MEM_callocN(sizeof(ColormnaageCacheData), "color manage cache imbuf data");
	cache_data->exposure = view_settings->exposure;
	cache_data->gamma = view_settings->gamma;
	cache_data->predivide = predivide;
	cache_data->flag = view_settings->flag;
	cache_data->curve_mapping = curve_mapping;
	cache_data->curve_mapping_timestamp = curve_mapping_timestamp;

	colormanage_cachedata_set(cache_ibuf, cache_data);

	*cache_handle = cache_ibuf;

	IMB_moviecache_put(moviecache, &key, cache_ibuf);
}
#endif

static void colormanage_cache_handle_release(void *cache_handle)
{
	ImBuf *cache_ibuf = cache_handle;

	IMB_freeImBuf(cache_ibuf);
}

/*********************** Initialization / De-initialization *************************/

#ifdef WITH_OCIO
static void colormanage_role_color_space_name_get(ConstConfigRcPtr *config, char *colorspace_name, const char *role)
{
	ConstColorSpaceRcPtr *ociocs;

	ociocs = OCIO_configGetColorSpace(config, role);

	if (ociocs) {
		const char *name = OCIO_colorSpaceGetName(ociocs);

		/* assume function was called with buffer properly allocated to MAX_COLORSPACE_NAME chars */
		BLI_strncpy(colorspace_name, name, MAX_COLORSPACE_NAME);
		OCIO_colorSpaceRelease(ociocs);
	}
	else {
		printf("Color management: Error could not find role %s role.\n", role);
	}
}

static void colormanage_load_config(ConstConfigRcPtr *config)
{
	int tot_colorspace, tot_display, tot_display_view, index, viewindex, viewindex2;
	const char *name;

	/* get roles */
	colormanage_role_color_space_name_get(config, global_role_scene_linear, OCIO_ROLE_SCENE_LINEAR);
	colormanage_role_color_space_name_get(config, global_role_color_picking, OCIO_ROLE_COLOR_PICKING);
	colormanage_role_color_space_name_get(config, global_role_texture_painting, OCIO_ROLE_TEXTURE_PAINT);
	colormanage_role_color_space_name_get(config, global_role_default_sequencer, OCIO_ROLE_DEFAULT_SEQUENCER);
	colormanage_role_color_space_name_get(config, global_role_default_byte, OCIO_ROLE_DEFAULT_BYTE);
	colormanage_role_color_space_name_get(config, global_role_default_float, OCIO_ROLE_DEFAULT_FLOAT);

	/* load colorspaces */
	tot_colorspace = OCIO_configGetNumColorSpaces(config);
	for (index = 0 ; index < tot_colorspace; index++) {
		ConstColorSpaceRcPtr *ocio_colorspace;
		const char *description;
		int is_invertible;

		name = OCIO_configGetColorSpaceNameByIndex(config, index);

		ocio_colorspace = OCIO_configGetColorSpace(config, name);
		description = OCIO_colorSpaceGetDescription(ocio_colorspace);
		is_invertible = OCIO_colorSpaceIsInvertible(ocio_colorspace);

		colormanage_colorspace_add(name, description, is_invertible);

		OCIO_colorSpaceRelease(ocio_colorspace);
	}

	/* load displays */
	viewindex2 = 0;
	tot_display = OCIO_configGetNumDisplays(config);

	for (index = 0 ; index < tot_display; index++) {
		const char *displayname;
		ColorManagedDisplay *display;

		displayname = OCIO_configGetDisplay(config, index);

		display = colormanage_display_add(displayname);

		/* load views */
		tot_display_view = OCIO_configGetNumViews(config, displayname);
		for (viewindex = 0 ; viewindex < tot_display_view; viewindex++, viewindex2++) {
			const char *viewname;
			ColorManagedView *view;
			LinkData *display_view;

			viewname = OCIO_configGetView(config, displayname, viewindex);

			/* first check if view transform with given name was already loaded */
			view = colormanage_view_get_named(viewname);

			if (!view) {
				view = colormanage_view_add(viewname);
			}

			display_view = BLI_genericNodeN(view);

			BLI_addtail(&display->views, display_view);
		}
	}

	global_tot_display = tot_display;
}

void colormanage_free_config(void)
{
	ColorSpace *colorspace;
	ColorManagedDisplay *display;

	/* free color spaces */
	colorspace = global_colorspaces.first;
	while (colorspace) {
		ColorSpace *colorspace_next = colorspace->next;

		/* free precomputer processors */
		if (colorspace->to_scene_linear)
			OCIO_processorRelease((ConstProcessorRcPtr *) colorspace->to_scene_linear);

		if (colorspace->from_scene_linear)
			OCIO_processorRelease((ConstProcessorRcPtr *) colorspace->from_scene_linear);

		/* free color space itself */
		MEM_freeN(colorspace);

		colorspace = colorspace_next;
	}

	/* free displays */
	display = global_displays.first;
	while (display) {
		ColorManagedDisplay *display_next = display->next;

		/* free precomputer processors */
		if (display->to_scene_linear)
			OCIO_processorRelease((ConstProcessorRcPtr *) display->to_scene_linear);

		if (display->from_scene_linear)
			OCIO_processorRelease((ConstProcessorRcPtr *) display->from_scene_linear);

		/* free list of views */
		BLI_freelistN(&display->views);

		MEM_freeN(display);
		display = display_next;
	}

	/* free views */
	BLI_freelistN(&global_views);
}
#endif

void IMB_colormanagement_init(void)
{
#ifdef WITH_OCIO
	const char *ocio_env;
	const char *configdir;
	char configfile[FILE_MAX];
	ConstConfigRcPtr *config = NULL;

	ocio_env = getenv("OCIO");

	if (ocio_env && ocio_env[0] != '\0')
		config = OCIO_configCreateFromEnv();

	if (config == NULL) {
		configdir = BLI_get_folder(BLENDER_DATAFILES, "colormanagement");

		if (configdir) 	{
			BLI_join_dirfile(configfile, sizeof(configfile), configdir, BCM_CONFIG_FILE);

			config = OCIO_configCreateFromFile(configfile);
		}
	}

	if (config) {
		OCIO_setCurrentConfig(config);

		colormanage_load_config(config);

		OCIO_configRelease(config);
	}
#endif

	BLI_init_srgb_conversion();
}

void IMB_colormanagement_exit(void)
{
#ifdef WITH_OCIO
	colormanage_free_config();
#endif
}

/*********************** Internal functions *************************/

void colormanage_cache_free(ImBuf *ibuf)
{
	if (ibuf->display_buffer_flags) {
		MEM_freeN(ibuf->display_buffer_flags);

		ibuf->display_buffer_flags = NULL;
	}

	if (ibuf->colormanage_cache) {
		ColormnaageCacheData *cache_data = colormanage_cachedata_get(ibuf);
		struct MovieCache *moviecache = colormanage_moviecache_get(ibuf);

		if (cache_data) {
			MEM_freeN(cache_data);
		}

		if (moviecache) {
			IMB_moviecache_free(moviecache);
		}

		MEM_freeN(ibuf->colormanage_cache);

		ibuf->colormanage_cache = NULL;
	}
}

static void display_transform_get_from_ctx(const bContext *C, ColorManagedViewSettings **view_settings_r,
                                           ColorManagedDisplaySettings **display_settings_r)
{
	Scene *scene = CTX_data_scene(C);
	SpaceImage *sima = CTX_wm_space_image(C);

	*view_settings_r = &scene->view_settings;
	*display_settings_r = &scene->display_settings;

	if (sima) {
		if ((sima->image->flag & IMA_VIEW_AS_RENDER) == 0)
			*view_settings_r = NULL;
	}
}

#ifdef WITH_OCIO
static ConstProcessorRcPtr *create_display_buffer_processor(const char *view_transform, const char *display,
                                                            float exposure, float gamma)
{
	ConstConfigRcPtr *config = OCIO_getCurrentConfig();
	DisplayTransformRcPtr *dt;
	ConstProcessorRcPtr *processor;

	if (!config) {
		/* there's no valid OCIO configuration, can't create processor */

		return NULL;
	}

	dt = OCIO_createDisplayTransform();

	/* assuming handling buffer was already converted to scene linear space */
	OCIO_displayTransformSetInputColorSpaceName(dt, global_role_scene_linear);
	OCIO_displayTransformSetView(dt, view_transform);
	OCIO_displayTransformSetDisplay(dt, display);

	/* fstop exposure control */
	if (exposure != 0.0f) {
		MatrixTransformRcPtr *mt;
		float gain = powf(2.0f, exposure);
		const float scale4f[] = {gain, gain, gain, gain};
		float m44[16], offset4[4];

		OCIO_matrixTransformScale(m44, offset4, scale4f);
		mt = OCIO_createMatrixTransform();
		OCIO_matrixTransformSetValue(mt, m44, offset4);
		OCIO_displayTransformSetLinearCC(dt, (ConstTransformRcPtr *) mt);

		OCIO_matrixTransformRelease(mt);
	}

	/* post-display gamma transform */
	if (gamma != 1.0f) {
		ExponentTransformRcPtr *et;
		float exponent = 1.0f / MAX2(FLT_EPSILON, gamma);
		const float exponent4f[] = {exponent, exponent, exponent, exponent};

		et = OCIO_createExponentTransform();
		OCIO_exponentTransformSetValue(et, exponent4f);
		OCIO_displayTransformSetDisplayCC(dt, (ConstTransformRcPtr *) et);

		OCIO_exponentTransformRelease(et);
	}

	processor = OCIO_configGetProcessor(config, (ConstTransformRcPtr *) dt);

	OCIO_displayTransformRelease(dt);
	OCIO_configRelease(config);

	return processor;
}

static ConstProcessorRcPtr *create_colorspace_transform_processor(const char *from_colorspace,
                                                                  const char *to_colorspace)
{
	ConstConfigRcPtr *config = OCIO_getCurrentConfig();
	ConstProcessorRcPtr *processor;

	if (!config) {
		/* there's no valid OCIO configuration, can't create processor */

		return NULL;
	}

	processor = OCIO_configGetProcessorWithNames(config, from_colorspace, to_colorspace);

	OCIO_configRelease(config);

	return processor;
}

static ConstProcessorRcPtr *colorspace_to_scene_linear_processor(ColorSpace *colorspace)
{
	if (colorspace->to_scene_linear == NULL) {
		BLI_lock_thread(LOCK_COLORMANAGE);

		if (colorspace->to_scene_linear == NULL) {
			ConstProcessorRcPtr *to_scene_linear;
			to_scene_linear = create_colorspace_transform_processor(colorspace->name, global_role_scene_linear);
			colorspace->to_scene_linear = (struct ConstProcessorRcPtr *) to_scene_linear;
		}

		BLI_unlock_thread(LOCK_COLORMANAGE);
	}

	return (ConstProcessorRcPtr *) colorspace->to_scene_linear;
}

static ConstProcessorRcPtr *colorspace_from_scene_linear_processor(ColorSpace *colorspace)
{
	if (colorspace->from_scene_linear == NULL) {
		BLI_lock_thread(LOCK_COLORMANAGE);

		if (colorspace->from_scene_linear == NULL) {
			ConstProcessorRcPtr *from_scene_linear;
			from_scene_linear = create_colorspace_transform_processor(global_role_scene_linear, colorspace->name);
			colorspace->from_scene_linear = (struct ConstProcessorRcPtr *) from_scene_linear;
		}

		BLI_unlock_thread(LOCK_COLORMANAGE);
	}

	return (ConstProcessorRcPtr *) colorspace->from_scene_linear;
}

static ConstProcessorRcPtr *display_from_scene_linear_processor(ColorManagedDisplay *display)
{
	if (display->from_scene_linear == NULL) {
		BLI_lock_thread(LOCK_COLORMANAGE);

		if (display->from_scene_linear == NULL) {
			const char *view_name = colormanage_view_get_default_name(display);
			ConstConfigRcPtr *config = OCIO_getCurrentConfig();
			ConstProcessorRcPtr *processor = NULL;

			if (view_name && config) {
				const char *view_colorspace = OCIO_configGetDisplayColorSpaceName(config, display->name, view_name);
				processor = OCIO_configGetProcessorWithNames(config, global_role_scene_linear, view_colorspace);

				OCIO_configRelease(config);
			}

			display->from_scene_linear = (struct ConstProcessorRcPtr *) processor;
		}

		BLI_unlock_thread(LOCK_COLORMANAGE);
	}

	return (ConstProcessorRcPtr *) display->from_scene_linear;
}

static ConstProcessorRcPtr *display_to_scene_linear_processor(ColorManagedDisplay *display)
{
	if (display->to_scene_linear == NULL) {
		BLI_lock_thread(LOCK_COLORMANAGE);

		if (display->to_scene_linear == NULL) {
			const char *view_name = colormanage_view_get_default_name(display);
			ConstConfigRcPtr *config = OCIO_getCurrentConfig();
			ConstProcessorRcPtr *processor = NULL;

			if (view_name && config) {
				const char *view_colorspace = OCIO_configGetDisplayColorSpaceName(config, display->name, view_name);
				processor = OCIO_configGetProcessorWithNames(config, view_colorspace, global_role_scene_linear);

				OCIO_configRelease(config);
			}

			display->to_scene_linear = (struct ConstProcessorRcPtr *) processor;
		}

		BLI_unlock_thread(LOCK_COLORMANAGE);
	}

	return (ConstProcessorRcPtr *) display->to_scene_linear;
}

static void init_default_view_settings(const ColorManagedDisplaySettings *display_settings,
                                       ColorManagedViewSettings *view_settings)
{
	ColorManagedDisplay *display;
	ColorManagedView *default_view;

	display = colormanage_display_get_named(display_settings->display_device);
	default_view = colormanage_view_get_default(display);

	if (default_view)
		BLI_strncpy(view_settings->view_transform, default_view->name, sizeof(view_settings->view_transform));
	else
		view_settings->view_transform[0] = '\0';

	view_settings->flag = 0;
	view_settings->gamma = 1.0f;
	view_settings->exposure = 0.0f;
	view_settings->curve_mapping = NULL;
}
#endif

static void curve_mapping_apply_pixel(CurveMapping *curve_mapping, float *pixel, int channels)
{
	if (channels == 1) {
		pixel[0] = curvemap_evaluateF(curve_mapping->cm, pixel[0]);
	}
	else if (channels == 2) {
		pixel[0] = curvemap_evaluateF(curve_mapping->cm, pixel[0]);
		pixel[1] = curvemap_evaluateF(curve_mapping->cm, pixel[1]);
	}
	else {
		curvemapping_evaluate_premulRGBF(curve_mapping, pixel, pixel);
	}
}

void colorspace_set_default_role(char *colorspace, int size, int role)
{
	if (colorspace && colorspace[0] == '\0') {
		const char *role_colorspace;

		role_colorspace = IMB_colormanagement_role_colorspace_name_get(role);

		BLI_strncpy(colorspace, role_colorspace, size);
	}
}

void colormanage_imbuf_make_linear(ImBuf *ibuf, const char *from_colorspace)
{
#ifdef WITH_OCIO
	if (ibuf->rect_float) {
		const char *to_colorspace = global_role_scene_linear;
		int predivide = ibuf->flags & IB_cm_predivide;

		if (ibuf->rect)
			imb_freerectImBuf(ibuf);

		IMB_colormanagement_transform(ibuf->rect_float, ibuf->x, ibuf->y, ibuf->channels,
		                              from_colorspace, to_colorspace, predivide);

		ibuf->profile = IB_PROFILE_LINEAR_RGB;
	}
#else
	(void) ibuf;
	(void) role;
#endif
}

/*********************** Generic functions *************************/

#ifdef WITH_OCIO
static void colormanage_check_display_settings(ColorManagedDisplaySettings *display_settings, const char *what,
                                               const ColorManagedDisplay *default_display)
{
	if (display_settings->display_device[0] == '\0') {
		BLI_strncpy(display_settings->display_device, default_display->name, sizeof(display_settings->display_device));
	}
	else {
		ColorManagedDisplay *display = colormanage_display_get_named(display_settings->display_device);

		if (!display) {
			printf("Color management: display \"%s\" used by %s not found, setting to default (\"%s\").\n",
			       display_settings->display_device, what, default_display->name);

			BLI_strncpy(display_settings->display_device, default_display->name,
			            sizeof(display_settings->display_device));
		}
	}
}

static void colormanage_check_view_settings(ColorManagedDisplaySettings *display_settings,
                                            ColorManagedViewSettings *view_settings, const char *what)
{
	ColorManagedDisplay *display;
	ColorManagedView *default_view;

	if (view_settings->view_transform[0] == '\0') {
		display = colormanage_display_get_named(display_settings->display_device);
		default_view = colormanage_view_get_default(display);

		if (default_view)
			BLI_strncpy(view_settings->view_transform, default_view->name, sizeof(view_settings->view_transform));
	}
	else {
		ColorManagedView *view = colormanage_view_get_named(view_settings->view_transform);

		if (!view) {
			display = colormanage_display_get_named(display_settings->display_device);
			default_view = colormanage_view_get_default(display);

			if (default_view) {
				printf("Color management: %s view \"%s\" not found, setting default \"%s\".\n",
				       what, view_settings->view_transform, default_view->name);

				BLI_strncpy(view_settings->view_transform, default_view->name, sizeof(view_settings->view_transform));
			}
		}
	}

	/* OCIO_TODO: move to do_versions() */
	if (view_settings->exposure == 0.0f && view_settings->gamma == 0.0f) {
		view_settings->exposure = 0.0f;
		view_settings->gamma = 1.0f;
	}
}

static void colormanage_check_colorspace_settings(ColorManagedColorspaceSettings *colorspace_settings, const char *what)
{
	if (colorspace_settings->name[0] == '\0') {
		/* pass */
	}
	else {
		ColorSpace *colorspace = colormanage_colorspace_get_named(colorspace_settings->name);

		if (!colorspace) {
			printf("Color management: %s colorspace \"%s\" not found, setting NONE instead.\n",
			       what, colorspace_settings->name);

			BLI_strncpy(colorspace_settings->name, "", sizeof(colorspace_settings->name));
		}
	}

	(void) what;
}
#endif

void IMB_colormanagement_check_file_config(Main *bmain)
{
#ifdef WITH_OCIO
	Scene *scene;
	Image *image;
	MovieClip *clip;

	ColorManagedDisplay *default_display;

	default_display = colormanage_display_get_default();

	if (!default_display) {
		/* happens when OCIO configuration is incorrect */
		return;
	}

	for (scene = bmain->scene.first; scene; scene = scene->id.next) {
		ColorManagedColorspaceSettings *sequencer_colorspace_settings;

		colormanage_check_display_settings(&scene->display_settings, "scene", default_display);
		colormanage_check_view_settings(&scene->display_settings, &scene->view_settings, "scene");

		sequencer_colorspace_settings = &scene->sequencer_colorspace_settings;

		colormanage_check_colorspace_settings(sequencer_colorspace_settings, "sequencer");

		if (sequencer_colorspace_settings->name[0] == '\0') {
			BLI_strncpy(sequencer_colorspace_settings->name, global_role_default_sequencer, MAX_COLORSPACE_NAME);
		}
	}

	/* ** check input color space settings ** */

	for (image = bmain->image.first; image; image = image->id.next) {
		colormanage_check_colorspace_settings(&image->colorspace_settings, "image");
	}

	for (clip = bmain->movieclip.first; clip; clip = clip->id.next) {
		colormanage_check_colorspace_settings(&clip->colorspace_settings, "clip");
	}
#else
	(void) bmain;
#endif
}

void IMB_colormanagement_validate_settings(ColorManagedDisplaySettings *display_settings,
                                           ColorManagedViewSettings *view_settings)
{
#ifdef WITH_OCIO
	ColorManagedDisplay *display;
	ColorManagedView *default_view;
	LinkData *view_link;

	display = colormanage_display_get_named(display_settings->display_device);
	default_view = colormanage_view_get_default(display);

	for (view_link = display->views.first; view_link; view_link = view_link->next) {
		ColorManagedView *view = view_link->data;

		if (!strcmp(view->name, view_settings->view_transform))
			break;
	}

	if (view_link == NULL)
		BLI_strncpy(view_settings->view_transform, default_view->name, sizeof(view_settings->view_transform));
#else
	(void) display_settings;
	(void) view_settings;
#endif
}

const char *IMB_colormanagement_role_colorspace_name_get(int role)
{
#ifdef WITH_OCIO
	switch (role) {
		case COLOR_ROLE_SCENE_LINEAR:
			return global_role_scene_linear;
			break;
		case COLOR_ROLE_COLOR_PICKING:
			return global_role_color_picking;
			break;
		case COLOR_ROLE_TEXTURE_PAINTING:
			return global_role_texture_painting;
			break;
		case COLOR_ROLE_DEFAULT_SEQUENCER:
			return global_role_default_sequencer;
			break;
		case COLOR_ROLE_DEFAULT_FLOAT:
			return global_role_default_float;
			break;
		case COLOR_ROLE_DEFAULT_BYTE:
			return global_role_default_byte;
			break;
		default:
			printf("Unknown role was passed to %s\n", __func__);
			BLI_assert(0);
	}
#else
	(void) role;
#endif

	return NULL;
}

void IMB_colormanagement_imbuf_float_from_rect(ImBuf *ibuf)
{
	int predivide = ibuf->flags & IB_cm_predivide;

	if (ibuf->rect == NULL)
		return;

	if (ibuf->rect_float == NULL) {
		if (imb_addrectfloatImBuf(ibuf) == 0)
			return;
	}

	/* first, create float buffer in non-linear space */
	IMB_buffer_float_from_byte(ibuf->rect_float, (unsigned char *) ibuf->rect, IB_PROFILE_SRGB, IB_PROFILE_SRGB,
	                           FALSE, ibuf->x, ibuf->y, ibuf->x, ibuf->x);

	/* then make float be in linear space */
	IMB_colormanagement_colorspace_to_scene_linear(ibuf->rect_float, ibuf->x, ibuf->y, ibuf->channels,
	                                               ibuf->rect_colorspace, predivide);
}

/*********************** Threaded display buffer transform routines *************************/

#ifdef WITH_OCIO
typedef struct DisplayBufferThread {
	ColormanageProcessor *cm_processor;

	float *buffer;
	unsigned char *byte_buffer;

	float *display_buffer;
	unsigned char *display_buffer_byte;

	int width;
	int start_line;
	int tot_line;

	int channels;
	float dither;
	int predivide;

	const char *byte_colorspace;
	const char *float_colorspace;
} DisplayBufferThread;

typedef struct DisplayBufferInitData {
	ImBuf *ibuf;
	ColormanageProcessor *cm_processor;
	float *buffer;
	unsigned char *byte_buffer;

	float *display_buffer;
	unsigned char *display_buffer_byte;

	int width;

	const char *byte_colorspace;
	const char *float_colorspace;
} DisplayBufferInitData;

static void display_buffer_init_handle(void *handle_v, int start_line, int tot_line, void *init_data_v)
{
	DisplayBufferThread *handle = (DisplayBufferThread *) handle_v;
	DisplayBufferInitData *init_data = (DisplayBufferInitData *) init_data_v;
	ImBuf *ibuf = init_data->ibuf;

	int predivide = ibuf->flags & IB_cm_predivide;
	int channels = ibuf->channels;
	float dither = ibuf->dither;

	int offset = channels * start_line * ibuf->x;

	memset(handle, 0, sizeof(DisplayBufferThread));

	handle->cm_processor = init_data->cm_processor;

	if (init_data->buffer)
		handle->buffer = init_data->buffer + offset;

	if (init_data->byte_buffer)
		handle->byte_buffer = init_data->byte_buffer + offset;

	if (init_data->display_buffer)
		handle->display_buffer = init_data->display_buffer + offset;

	if (init_data->display_buffer_byte)
		handle->display_buffer_byte = init_data->display_buffer_byte + offset;

	handle->width = ibuf->x;

	handle->start_line = start_line;
	handle->tot_line = tot_line;

	handle->channels = channels;
	handle->dither = dither;
	handle->predivide = predivide;

	handle->byte_colorspace = init_data->byte_colorspace;
	handle->float_colorspace = init_data->float_colorspace;
}

static void *display_buffer_apply_get_linear_buffer(DisplayBufferThread *handle)
{
	float *linear_buffer = NULL;

	int channels = handle->channels;
	int width = handle->width;
	int height = handle->tot_line;

	int buffer_size = channels * width * height;

	int predivide = handle->predivide;

	linear_buffer = MEM_callocN(buffer_size * sizeof(float), "color conversion linear buffer");

	if (!handle->buffer) {
		unsigned char *byte_buffer = handle->byte_buffer;

		const char *from_colorspace = handle->byte_colorspace;
		const char *to_colorspace = global_role_scene_linear;

		float *fp;
		unsigned char *cp;
		int i;

		/* first convert byte buffer to float, keep in image space */
		for (i = 0, fp = linear_buffer, cp = byte_buffer;
		     i < channels * width * height;
			 i++, fp++, cp++)
		{
			*fp = (float)(*cp) / 255.0f;
		}

		/* convert float buffer to scene linear space */
		IMB_colormanagement_transform(linear_buffer, width, height, channels,
		                              from_colorspace, to_colorspace, predivide);
	}
	else if (handle->float_colorspace) {
		/* currently float is non-linear only in sequencer, which is working
		 * in it's own color space even to handle float buffers.
		 * This color space is the same for byte and float images.
		 * Need to convert float buffer to linear space before applying display transform
		 */

		const char *from_colorspace = handle->float_colorspace;
		const char *to_colorspace = global_role_scene_linear;

		memcpy(linear_buffer, handle->buffer, buffer_size * sizeof(float));

		IMB_colormanagement_transform(linear_buffer, width, height, channels,
		                              from_colorspace, to_colorspace, predivide);
	}
	else {
		/* some processors would want to modify float original buffer
		 * before converting it into display byte buffer, so we need to
		 * make sure original's ImBuf buffers wouldn't be modified by
		 * using duplicated buffer here
		 *
		 * NOTE: MEM_dupallocN can't be used because buffer could be
		 *       specified as an offset inside allocated buffer
		 */

		memcpy(linear_buffer, handle->buffer, buffer_size * sizeof(float));
	}

	return linear_buffer;
}

static void *do_display_buffer_apply_thread(void *handle_v)
{
	DisplayBufferThread *handle = (DisplayBufferThread *) handle_v;
	ColormanageProcessor *cm_processor = handle->cm_processor;
	float *buffer = handle->buffer;
	float *display_buffer = handle->display_buffer;
	unsigned char *display_buffer_byte = handle->display_buffer_byte;
	int channels = handle->channels;
	int width = handle->width;
	int height = handle->tot_line;
	float dither = handle->dither;
	int predivide = handle->predivide;

	float *linear_buffer = display_buffer_apply_get_linear_buffer(handle);

	/* apply processor */
	IMB_colormanagement_processor_apply(cm_processor, linear_buffer, width, height, channels, predivide);

	/* copy result to output buffers */
	if (display_buffer_byte) {
		/* do conversion */
		IMB_buffer_byte_from_float(display_buffer_byte, linear_buffer,
		                           channels, dither, IB_PROFILE_SRGB, IB_PROFILE_SRGB,
		                           predivide, width, height, width, width);
	}

	if (display_buffer)
		memcpy(display_buffer, linear_buffer, width * height * channels * sizeof(float));

	if (linear_buffer != buffer)
		MEM_freeN(linear_buffer);

	return NULL;
}

static void display_buffer_apply_threaded(ImBuf *ibuf, float *buffer, unsigned char *byte_buffer, float *display_buffer,
                                          unsigned char *display_buffer_byte, ColormanageProcessor *cm_processor)
{
	DisplayBufferInitData init_data;

	init_data.ibuf = ibuf;
	init_data.cm_processor = cm_processor;
	init_data.buffer = buffer;
	init_data.byte_buffer = byte_buffer;
	init_data.display_buffer = display_buffer;
	init_data.display_buffer_byte = display_buffer_byte;

	if (ibuf->rect_colorspace != NULL) {
		init_data.byte_colorspace = ibuf->rect_colorspace->name;
	}
	else {
		/* happens for viewer images, which are not so simple to determine where to
		 * set image buffer's color spaces
		 */
		init_data.byte_colorspace = global_role_default_byte;
	}

	if (ibuf->float_colorspace != NULL) {
		/* sequencer stores float buffers in non-linear space */
		init_data.float_colorspace = ibuf->float_colorspace->name;
	}
	else {
		init_data.float_colorspace = NULL;
	}

	IMB_processor_apply_threaded(ibuf->y, sizeof(DisplayBufferThread), &init_data,
	                             display_buffer_init_handle, do_display_buffer_apply_thread);
}

static void colormanage_display_buffer_process_ex(ImBuf *ibuf, float *display_buffer, unsigned char *display_buffer_byte,
                                                  const ColorManagedViewSettings *view_settings,
                                                  const ColorManagedDisplaySettings *display_settings)
{
	ColormanageProcessor *cm_processor;

	cm_processor = IMB_colormanagement_display_processor_new(view_settings, display_settings);

	display_buffer_apply_threaded(ibuf, ibuf->rect_float, (unsigned char *) ibuf->rect,
	                              display_buffer, display_buffer_byte, cm_processor);

	IMB_colormanagement_processor_free(cm_processor);
}

static void colormanage_display_buffer_process(ImBuf *ibuf, unsigned char *display_buffer,
                                               const ColorManagedViewSettings *view_settings,
                                               const ColorManagedDisplaySettings *display_settings)
{
	colormanage_display_buffer_process_ex(ibuf, NULL, display_buffer, view_settings, display_settings);
}
#endif

/*********************** Threaded processor transform routines *************************/

typedef struct ProcessorTransformThread {
	ColormanageProcessor *cm_processor;
	float *buffer;
	int width;
	int start_line;
	int tot_line;
	int channels;
	int predivide;
} ProcessorTransformThread;

typedef struct ProcessorTransformInit {
	ColormanageProcessor *cm_processor;
	float *buffer;
	int width;
	int height;
	int channels;
	int predivide;
} ProcessorTransformInitData;

static void processor_transform_init_handle(void *handle_v, int start_line, int tot_line, void *init_data_v)
{
	ProcessorTransformThread *handle = (ProcessorTransformThread *) handle_v;
	ProcessorTransformInitData *init_data = (ProcessorTransformInitData *) init_data_v;

	int channels = init_data->channels;
	int width = init_data->width;
	int predivide = init_data->predivide;

	int offset = channels * start_line * width;

	memset(handle, 0, sizeof(ProcessorTransformThread));

	handle->cm_processor = init_data->cm_processor;

	handle->buffer = init_data->buffer + offset;

	handle->width = width;

	handle->start_line = start_line;
	handle->tot_line = tot_line;

	handle->channels = channels;
	handle->predivide = predivide;
}

static void *do_processor_transform_thread(void *handle_v)
{
	ProcessorTransformThread *handle = (ProcessorTransformThread *) handle_v;
	float *buffer = handle->buffer;
	int channels = handle->channels;
	int width = handle->width;
	int height = handle->tot_line;
	int predivide = handle->predivide;

	IMB_colormanagement_processor_apply(handle->cm_processor, buffer, width, height, channels, predivide);

	return NULL;
}

static void processor_transform_apply_threaded(float *buffer, int width, int height, int channels,
                                               ColormanageProcessor *cm_processor, int predivide)
{
	ProcessorTransformInitData init_data;

	init_data.cm_processor = cm_processor;
	init_data.buffer = buffer;
	init_data.width = width;
	init_data.height = height;
	init_data.channels = channels;
	init_data.predivide = predivide;

	IMB_processor_apply_threaded(height, sizeof(ProcessorTransformThread), &init_data,
	                             processor_transform_init_handle, do_processor_transform_thread);
}

/*********************** Color space transformation functions *************************/

/* convert the whole buffer from specified by name color space to another - internal implementation */
static void colormanagement_transform_ex(float *buffer, int width, int height, int channels, const char *from_colorspace,
                                         const char *to_colorspace, int predivide, int do_threaded)
{
	ColormanageProcessor *cm_processor;

	if (from_colorspace[0] == '\0') {
		return;
	}

	if (!strcmp(from_colorspace, to_colorspace)) {
		/* if source and destination color spaces are identical, skip
		 * threading overhead and simply do nothing
		 */
		return;
	}

	cm_processor = IMB_colormanagement_colorspace_processor_new(from_colorspace, to_colorspace);

	if (do_threaded)
		processor_transform_apply_threaded(buffer, width, height, channels, cm_processor, predivide);
	else
		IMB_colormanagement_processor_apply(cm_processor, buffer, width, height, channels, predivide);

	IMB_colormanagement_processor_free(cm_processor);
}

/* convert the whole buffer from specified by name color space to another */
void IMB_colormanagement_transform(float *buffer, int width, int height, int channels,
                                   const char *from_colorspace, const char *to_colorspace, int predivide)
{
	colormanagement_transform_ex(buffer, width, height, channels, from_colorspace, to_colorspace, predivide, FALSE);
}

/* convert the whole buffer from specified by name color space to another
 * will do threaded conversion
 */
void IMB_colormanagement_transform_threaded(float *buffer, int width, int height, int channels,
                                            const char *from_colorspace, const char *to_colorspace, int predivide)
{
	colormanagement_transform_ex(buffer, width, height, channels, from_colorspace, to_colorspace, predivide, TRUE);
}

void IMB_colormanagement_transform_v4(float pixel[4], const char *from_colorspace, const char *to_colorspace)
{
	ColormanageProcessor *cm_processor;

	if (from_colorspace[0] == '\0') {
		return;
	}

	if (!strcmp(from_colorspace, to_colorspace)) {
		/* if source and destination color spaces are identical, skip
		 * threading overhead and simply do nothing
		 */
		return;
	}

	cm_processor = IMB_colormanagement_colorspace_processor_new(from_colorspace, to_colorspace);

	IMB_colormanagement_processor_apply_v4(cm_processor, pixel);

	IMB_colormanagement_processor_free(cm_processor);
}

/* convert pixel from specified by descriptor color space to scene linear
 * used by performance-critical areas such as renderer and baker
 */
void IMB_colormanagement_colorspace_to_scene_linear_v3(float pixel[3], ColorSpace *colorspace)
{
#ifdef WITH_OCIO
	ConstProcessorRcPtr *processor;

	if (!colorspace) {
		/* OCIO_TODO: make sure it never happens */

		printf("%s: perform conversion from unknown color space\n", __func__);

		return;
	}

	processor = colorspace_to_scene_linear_processor(colorspace);

	if (processor)
		OCIO_processorApplyRGB(processor, pixel);
#else
	(void) pixel;
	(void) colorspace;
#endif
}

/* same as above, but converts colors in opposite direction */
void IMB_colormanagement_scene_linear_to_colorspace_v3(float pixel[3], ColorSpace *colorspace)
{
#ifdef WITH_OCIO
	ConstProcessorRcPtr *processor;

	if (!colorspace) {
		/* OCIO_TODO: make sure it never happens */

		printf("%s: perform conversion from unknown color space\n", __func__);

		return;
	}

	processor = colorspace_from_scene_linear_processor(colorspace);

	if (processor)
		OCIO_processorApplyRGB(processor, pixel);

#else
	(void) pixel;
	(void) colorspace;
#endif
}

void IMB_colormanagement_colorspace_to_scene_linear(float *buffer, int width, int height, int channels, struct ColorSpace *colorspace, int predivide)
{
#ifdef WITH_OCIO
	ConstProcessorRcPtr *processor;

	if (!colorspace) {
		/* OCIO_TODO: make sure it never happens */

		printf("%s: perform conversion from unknown color space\n", __func__);

		return;
	}

	processor = colorspace_to_scene_linear_processor(colorspace);

	if (processor) {
		PackedImageDesc *img;

		img = OCIO_createPackedImageDesc(buffer, width, height, channels, sizeof(float),
		                                 channels * sizeof(float), channels * sizeof(float) * width);

		if (predivide)
			OCIO_processorApply_predivide(processor, img);
		else
			OCIO_processorApply(processor, img);

		OCIO_packedImageDescRelease(img);
	}
#else
	(void) buffer;
	(void) channels;
	(void) width;
	(void) height;
	(void) colorspace;
	(void) predivide;
#endif
}

/* convert pixel from scene linear to display space using default view
 * used by performance-critical areas such as color-related widgets where we want to reduce
 * amount of per-widget allocations
 */
void IMB_colormanagement_scene_linear_to_display_v3(float pixel[3], ColorManagedDisplay *display)
{
#ifdef WITH_OCIO
	ConstProcessorRcPtr *processor;

	processor = display_from_scene_linear_processor(display);

	if (processor)
		OCIO_processorApplyRGB(processor, pixel);
#else
	(void) pixel;
	(void) display;
#endif
}

/* same as above, but converts color in opposite direction */
void IMB_colormanagement_display_to_scene_linear_v3(float pixel[3], ColorManagedDisplay *display)
{
#ifdef WITH_OCIO
	ConstProcessorRcPtr *processor;

	processor = display_to_scene_linear_processor(display);

	if (processor)
		OCIO_processorApplyRGB(processor, pixel);
#else
	(void) pixel;
	(void) display;
#endif
}

void IMB_colormanagement_pixel_to_display_space_v4(float result[4], const float pixel[4],
                                                   const ColorManagedViewSettings *view_settings,
                                                   const ColorManagedDisplaySettings *display_settings)
{
	ColormanageProcessor *cm_processor;

	copy_v4_v4(result, pixel);

	cm_processor = IMB_colormanagement_display_processor_new(view_settings, display_settings);
	IMB_colormanagement_processor_apply_v4(cm_processor, result);
	IMB_colormanagement_processor_free(cm_processor);
}

void IMB_colormanagement_pixel_to_display_space_v3(float result[3], const float pixel[3],
                                                   const ColorManagedViewSettings *view_settings,
                                                   const ColorManagedDisplaySettings *display_settings)
{
	ColormanageProcessor *cm_processor;

	copy_v3_v3(result, pixel);

	cm_processor = IMB_colormanagement_display_processor_new(view_settings, display_settings);
	IMB_colormanagement_processor_apply_v3(cm_processor, result);
	IMB_colormanagement_processor_free(cm_processor);
}

void IMB_colormanagement_imbuf_assign_spaces(ImBuf *ibuf, ColorManagedColorspaceSettings *colorspace_settings)
{
#ifdef WITH_OCIO
	if (colorspace_settings) {
		if (colorspace_settings->name[0] == '\0') {
			/* when opening new image, assign it's color space based on default roles */

			if (ibuf->rect_float)
				BLI_strncpy(colorspace_settings->name, global_role_default_float, MAX_COLORSPACE_NAME);
			else
				BLI_strncpy(colorspace_settings->name, global_role_default_byte, MAX_COLORSPACE_NAME);
		}

		ibuf->rect_colorspace = colormanage_colorspace_get_named(colorspace_settings->name);
	}
	else {
		if (ibuf->rect_float)
			ibuf->rect_colorspace = colormanage_colorspace_get_named(global_role_default_float);
		else
			ibuf->rect_colorspace = colormanage_colorspace_get_named(global_role_default_byte);
	}
#else
	(void) ibuf;
	(void) colorspace_settings;
#endif
}

void IMB_colormanagement_imbuf_assign_float_space(ImBuf *ibuf, ColorManagedColorspaceSettings *colorspace_settings)
{
	ibuf->float_colorspace = colormanage_colorspace_get_named(colorspace_settings->name);
}

void IMB_colormanagement_imbuf_make_display_space(ImBuf *ibuf, const ColorManagedViewSettings *view_settings,
                                                  const ColorManagedDisplaySettings *display_settings)
{
#ifdef WITH_OCIO
	/* OCIO_TODO: byte buffer management is not supported here yet */
	if (!ibuf->rect_float)
		return;

	if (global_tot_display == 0 || global_tot_view == 0) {
		IMB_buffer_float_from_float(ibuf->rect_float, ibuf->rect_float, ibuf->channels, IB_PROFILE_LINEAR_RGB, ibuf->profile,
		                            ibuf->flags & IB_cm_predivide, ibuf->x, ibuf->y, ibuf->x, ibuf->x);
	}
	else {
		colormanage_display_buffer_process_ex(ibuf, ibuf->rect_float, NULL, view_settings, display_settings);
	}
#else
	(void) view_settings;
	(void) display_settings;

	IMB_buffer_float_from_float(ibuf->rect_float, ibuf->rect_float, ibuf->channels, IB_PROFILE_LINEAR_RGB, ibuf->profile,
	                            ibuf->flags & IB_cm_predivide, ibuf->x, ibuf->y, ibuf->x, ibuf->x);
#endif
}

static void imbuf_verify_float(ImBuf *ibuf)
{
	/* multiple threads could request for display buffer at once and in case
	 * view transform is not used it'll lead to display buffer calculated
	 * several times
	 * it is harmless, but would take much more time (assuming thread lock
	 * happens faster than running float->byte conversion for average image)
	 */
	BLI_lock_thread(LOCK_COLORMANAGE);

	if (ibuf->rect_float && (ibuf->rect == NULL || (ibuf->userflags & IB_RECT_INVALID))) {
		IMB_rect_from_float(ibuf);

		ibuf->userflags &= ~IB_RECT_INVALID;
	}

	BLI_unlock_thread(LOCK_COLORMANAGE);
}

/*********************** Public display buffers interfaces *************************/

/* acquire display buffer for given image buffer using specified view and display settings */
unsigned char *IMB_display_buffer_acquire(ImBuf *ibuf, const ColorManagedViewSettings *view_settings,
                                          const ColorManagedDisplaySettings *display_settings, void **cache_handle)
{
	*cache_handle = NULL;

	if (!ibuf->x || !ibuf->y)
		return NULL;

	if (global_tot_display == 0 || global_tot_view == 0) {
		/* if there's no view transform or display transforms, fallback to standard sRGB/linear conversion
		 * the same logic would be used if OCIO is disabled
		 */

		imbuf_verify_float(ibuf);

		return (unsigned char *) ibuf->rect;
	}
#ifdef WITH_OCIO
	else {
		unsigned char *display_buffer;
		int buffer_size;
		ColormanageCacheViewSettings cache_view_settings;
		ColormanageCacheDisplaySettings cache_display_settings;
		ColorManagedViewSettings default_view_settings;
		const ColorManagedViewSettings *applied_view_settings;

		if (view_settings) {
			applied_view_settings = view_settings;
		}
		else {
			/* if no view settings were specified, use default display transformation
			 * this happens for images which don't want to be displayed with render settings
			 */

			init_default_view_settings(display_settings,  &default_view_settings);
			applied_view_settings = &default_view_settings;
		}

		colormanage_view_settings_to_cache(&cache_view_settings, applied_view_settings);
		colormanage_display_settings_to_cache(&cache_display_settings, display_settings);

		BLI_lock_thread(LOCK_COLORMANAGE);

		/* ensure color management bit fields exists */
		if (!ibuf->display_buffer_flags) {
			if (global_tot_display)
				ibuf->display_buffer_flags = MEM_callocN(sizeof(unsigned int) * global_tot_display, "imbuf display_buffer_flags");
		}
		 else if (ibuf->userflags & IB_DISPLAY_BUFFER_INVALID) {
			/* all display buffers were marked as invalid from other areas,
			 * now propagate this flag to internal color management routines
			 */
			memset(ibuf->display_buffer_flags, 0, global_tot_display * sizeof(unsigned int));

			ibuf->userflags &= ~IB_DISPLAY_BUFFER_INVALID;
		}

		display_buffer = colormanage_cache_get(ibuf, &cache_view_settings, &cache_display_settings, cache_handle);

		if (display_buffer) {
			BLI_unlock_thread(LOCK_COLORMANAGE);
			return display_buffer;
		}

		buffer_size = ibuf->channels * ibuf->x * ibuf->y * sizeof(float);
		display_buffer = MEM_callocN(buffer_size, "imbuf display buffer");

		colormanage_display_buffer_process(ibuf, display_buffer, applied_view_settings, display_settings);

		colormanage_cache_put(ibuf, &cache_view_settings, &cache_display_settings, display_buffer, cache_handle);

		BLI_unlock_thread(LOCK_COLORMANAGE);

		return display_buffer;
	}
#else
	(void) view_settings;
	(void) display_settings;

	return NULL;
#endif
}

/* same as IMB_display_buffer_acquire but gets view and display settings from context */
unsigned char *IMB_display_buffer_acquire_ctx(const bContext *C, ImBuf *ibuf, void **cache_handle)
{
	ColorManagedViewSettings *view_settings;
	ColorManagedDisplaySettings *display_settings;

	display_transform_get_from_ctx(C, &view_settings, &display_settings);

	return IMB_display_buffer_acquire(ibuf, view_settings, display_settings, cache_handle);
}

/* covert float buffer to display space and store it in image buffer's byte array */
void IMB_display_buffer_to_imbuf_rect(ImBuf *ibuf, const ColorManagedViewSettings *view_settings,
                                      const ColorManagedDisplaySettings *display_settings)
{
#ifdef WITH_OCIO
	if (global_tot_display == 0 || global_tot_view == 0) {
		imbuf_verify_float(ibuf);
	}
	else {
		if (!ibuf->rect) {
			imb_addrectImBuf(ibuf);
		}

		colormanage_display_buffer_process(ibuf, (unsigned char *) ibuf->rect, view_settings, display_settings);
	}
#else
	(void) view_settings;
	(void) display_settings;

	imbuf_verify_float(ibuf);
#endif
}

void IMB_display_buffer_transform_apply(unsigned char *display_buffer, float *linear_buffer, int width, int height,
                                        int channels, const ColorManagedViewSettings *view_settings,
                                        const ColorManagedDisplaySettings *display_settings, int predivide)
{
#ifdef WITH_OCIO
	if (global_tot_display == 0 || global_tot_view == 0) {
		IMB_buffer_byte_from_float(display_buffer, linear_buffer, 4, 0.0f, IB_PROFILE_SRGB, IB_PROFILE_LINEAR_RGB, FALSE,
		                           width, height, width, width);
	}
	else {
		float *buffer;
		ColormanageProcessor *cm_processor = IMB_colormanagement_display_processor_new(view_settings, display_settings);

		buffer = MEM_callocN(channels * width * height * sizeof(float), "display transform temp buffer");
		memcpy(buffer, linear_buffer, channels * width * height * sizeof(float));

		IMB_colormanagement_processor_apply(cm_processor, buffer, width, height, channels, predivide);

		IMB_colormanagement_processor_free(cm_processor);

		IMB_buffer_byte_from_float(display_buffer, buffer, channels, 0.0f, IB_PROFILE_SRGB, IB_PROFILE_SRGB,
		                           FALSE, width, height, width, width);

		MEM_freeN(buffer);
	}
#else
	(void) view_settings;
	(void) display_settings;

	IMB_buffer_byte_from_float(display_buffer, linear_buffer, channels, 0.0f, IB_PROFILE_SRGB, IB_PROFILE_LINEAR_RGB, predivide,
	                           width, height, width, width);
#endif
}

void IMB_display_buffer_release(void *cache_handle)
{
	if (cache_handle) {
		BLI_lock_thread(LOCK_COLORMANAGE);

		colormanage_cache_handle_release(cache_handle);

		BLI_unlock_thread(LOCK_COLORMANAGE);
	}
}

/*********************** Display functions *************************/

#ifdef WITH_OCIO
ColorManagedDisplay *colormanage_display_get_default(void)
{
	ConstConfigRcPtr *config = OCIO_getCurrentConfig();
	const char *display;

	if (!config) {
		/* no valid OCIO configuration, can't get default display */

		return NULL;
	}

	display = OCIO_configGetDefaultDisplay(config);

	OCIO_configRelease(config);

	if (display[0] == '\0')
		return NULL;

	return colormanage_display_get_named(display);
}
#endif

ColorManagedDisplay *colormanage_display_add(const char *name)
{
	ColorManagedDisplay *display;
	int index = 0;

	if (global_displays.last) {
		ColorManagedDisplay *last_display = global_displays.last;

		index = last_display->index;
	}

	display = MEM_callocN(sizeof(ColorManagedDisplay), "ColorManagedDisplay");

	display->index = index + 1;

	BLI_strncpy(display->name, name, sizeof(display->name));

	BLI_addtail(&global_displays, display);

	return display;
}

ColorManagedDisplay *colormanage_display_get_named(const char *name)
{
	ColorManagedDisplay *display;

	for (display = global_displays.first; display; display = display->next) {
		if (!strcmp(display->name, name))
			return display;
	}

	return NULL;
}

ColorManagedDisplay *colormanage_display_get_indexed(int index)
{
	/* display indices are 1-based */
	return BLI_findlink(&global_displays, index - 1);
}

int IMB_colormanagement_display_get_named_index(const char *name)
{
	ColorManagedDisplay *display;

	display = colormanage_display_get_named(name);

	if (display) {
		return display->index;
	}

	return 0;
}

const char *IMB_colormanagement_display_get_indexed_name(int index)
{
	ColorManagedDisplay *display;

	display = colormanage_display_get_indexed(index);

	if (display) {
		return display->name;
	}

	return NULL;
}

const char *IMB_colormanagement_display_get_default_name(void)
{
#ifdef WITH_OCIO
	ColorManagedDisplay *display = colormanage_display_get_default();

	return display->name;
#else
	return NULL;
#endif
}

/* used by performance-critical pixel processing areas, such as color widgets */
ColorManagedDisplay *IMB_colormanagement_display_get_named(const char *name)
{
	return colormanage_display_get_named(name);
}

/*********************** View functions *************************/

#ifdef WITH_OCIO
const char *colormanage_view_get_default_name(const ColorManagedDisplay *display)
{
	ConstConfigRcPtr *config = OCIO_getCurrentConfig();
	const char *name;

	if (!config) {
		/* no valid OCIO configuration, can't get default view */

		return NULL;
	}

	name = OCIO_configGetDefaultView(config, display->name);

	OCIO_configRelease(config);

	return name;
}

ColorManagedView *colormanage_view_get_default(const ColorManagedDisplay *display)
{
	const char *name = colormanage_view_get_default_name(display);

	if (!name || name[0] == '\0')
		return NULL;

	return colormanage_view_get_named(name);
}
#endif

ColorManagedView *colormanage_view_add(const char *name)
{
	ColorManagedView *view;
	int index = global_tot_view;

	view = MEM_callocN(sizeof(ColorManagedView), "ColorManagedView");
	view->index = index + 1;
	BLI_strncpy(view->name, name, sizeof(view->name));

	BLI_addtail(&global_views, view);

	global_tot_view++;

	return view;
}

ColorManagedView *colormanage_view_get_named(const char *name)
{
	ColorManagedView *view;

	for (view = global_views.first; view; view = view->next) {
		if (!strcmp(view->name, name))
			return view;
	}

	return NULL;
}

ColorManagedView *colormanage_view_get_indexed(int index)
{
	/* view transform indices are 1-based */
	return BLI_findlink(&global_views, index - 1);
}

int IMB_colormanagement_view_get_named_index(const char *name)
{
	ColorManagedView *view = colormanage_view_get_named(name);

	if (view) {
		return view->index;
	}

	return 0;
}

const char *IMB_colormanagement_view_get_indexed_name(int index)
{
	ColorManagedView *view = colormanage_view_get_indexed(index);

	if (view) {
		return view->name;
	}

	return NULL;
}

const char *IMB_colormanagement_view_get_default_name(const char *display_name)
{
#if WITH_OCIO
	ColorManagedDisplay *display = colormanage_display_get_named(display_name);
	ColorManagedView *view = colormanage_view_get_default(display);

	if (view) {
		return view->name;
	}
#else
	(void) display_name;
#endif

	return NULL;
}

/*********************** Color space functions *************************/

static void colormanage_description_strip(char *description)
{
	int i, n;

	for (i = strlen(description) - 1; i >= 0; i--) {
		if (ELEM(description[i], '\r', '\n')) {
			description[i] = '\0';
		}
		else {
			break;
		}
	}

	for (i = 0, n = strlen(description); i < n; i++) {
		if (ELEM(description[i], '\r', '\n')) {
			description[i] = ' ';
		}
	}
}

ColorSpace *colormanage_colorspace_add(const char *name, const char *description, int is_invertible)
{
	ColorSpace *colorspace, *prev_space;
	int counter = 1;

	colorspace = MEM_callocN(sizeof(ColorSpace), "ColorSpace");

	BLI_strncpy(colorspace->name, name, sizeof(colorspace->name));

	if (description) {
		BLI_strncpy(colorspace->description, description, sizeof(colorspace->description));

		colormanage_description_strip(colorspace->description);
	}

	colorspace->is_invertible = is_invertible;

	for (prev_space = global_colorspaces.first; prev_space; prev_space = prev_space->next) {
		if (BLI_strcasecmp(prev_space->name, colorspace->name) > 0)
			break;

		prev_space->index = counter++;
	}

	if (!prev_space)
		BLI_addtail(&global_colorspaces, colorspace);
	else
		BLI_insertlinkbefore(&global_colorspaces, prev_space, colorspace);

	colorspace->index = counter++;
	for (; prev_space; prev_space = prev_space->next) {
		prev_space->index = counter++;
	}

	global_tot_colorspace++;

	return colorspace;
}

ColorSpace *colormanage_colorspace_get_named(const char *name)
{
	ColorSpace *colorspace;

	for (colorspace = global_colorspaces.first; colorspace; colorspace = colorspace->next) {
		if (!strcmp(colorspace->name, name))
			return colorspace;
	}

	return NULL;
}

ColorSpace *colormanage_colorspace_get_roled(int role)
{
	const char *role_colorspace = IMB_colormanagement_role_colorspace_name_get(role);

	return colormanage_colorspace_get_named(role_colorspace);
}

ColorSpace *colormanage_colorspace_get_indexed(int index)
{
	/* display indices are 1-based */
	return BLI_findlink(&global_colorspaces, index - 1);
}

int IMB_colormanagement_colorspace_get_named_index(const char *name)
{
	ColorSpace *colorspace;

	colorspace = colormanage_colorspace_get_named(name);

	if (colorspace) {
		return colorspace->index;
	}

	return 0;
}

const char *IMB_colormanagement_colorspace_get_indexed_name(int index)
{
	ColorSpace *colorspace;

	colorspace = colormanage_colorspace_get_indexed(index);

	if (colorspace) {
		return colorspace->name;
	}

	return "";
}

void IMB_colormanagment_colorspace_from_ibuf_ftype(ColorManagedColorspaceSettings *colorspace_settings, ImBuf *ibuf)
{
	ImFileType *type;

	for (type = IMB_FILE_TYPES; type->is_a; type++) {
		if (type->save && type->ftype(type, ibuf)) {
			const char *role_colorspace;

			role_colorspace = IMB_colormanagement_role_colorspace_name_get(type->default_save_role);

			BLI_strncpy(colorspace_settings->name, role_colorspace, sizeof(colorspace_settings->name));
		}
	}
}

/*********************** RNA helper functions *************************/

void IMB_colormanagement_display_items_add(EnumPropertyItem **items, int *totitem)
{
	ColorManagedDisplay *display;

	for (display = global_displays.first; display; display = display->next) {
		EnumPropertyItem item;

		item.value = display->index;
		item.name = display->name;
		item.identifier = display->name;
		item.icon = 0;
		item.description = "";

		RNA_enum_item_add(items, totitem, &item);
	}
}

static void colormanagement_view_item_add(EnumPropertyItem **items, int *totitem, ColorManagedView *view)
{
	EnumPropertyItem item;

	item.value = view->index;
	item.name = view->name;
	item.identifier = view->name;
	item.icon = 0;
	item.description = "";

	RNA_enum_item_add(items, totitem, &item);
}

void IMB_colormanagement_view_items_add(EnumPropertyItem **items, int *totitem, const char *display_name)
{
	ColorManagedDisplay *display = colormanage_display_get_named(display_name);
	ColorManagedView *view;

	if (display) {
		LinkData *display_view;

		for (display_view = display->views.first; display_view; display_view = display_view->next) {
			view = display_view->data;

			colormanagement_view_item_add(items, totitem, view);
		}
	}
}

void IMB_colormanagement_colorspace_items_add(EnumPropertyItem **items, int *totitem)
{
	ColorSpace *colorspace;

	for (colorspace = global_colorspaces.first; colorspace; colorspace = colorspace->next) {
		EnumPropertyItem item;

		if (!colorspace->is_invertible)
			continue;

		item.value = colorspace->index;
		item.name = colorspace->name;
		item.identifier = colorspace->name;
		item.icon = 0;

		if (colorspace->description)
			item.description = colorspace->description;
		else
			item.description = "";

		RNA_enum_item_add(items, totitem, &item);
	}
}

/*********************** Partial display buffer update  *************************/

/*
 * Partial display update is supposed to be used by such areas as
 * compositor and renderer, This areas are calculating tiles of the
 * images and because of performance reasons only this tiles should
 * be color managed.
 * This gives nice visual feedback without slowing things down.
 *
 * Updating happens for active display transformation only, all
 * the rest buffers would be marked as dirty
 */

#ifdef WITH_OCIO
static void partial_buffer_update_rect(ImBuf *ibuf, unsigned char *display_buffer, const float *linear_buffer,
                                       const unsigned char *byte_buffer, int display_stride, int linear_stride,
                                       int linear_offset_x, int linear_offset_y, ColormanageProcessor *cm_processor,
                                       int xmin, int ymin, int xmax, int ymax)
{
	int x, y;
	int channels = ibuf->channels;
	int predivide = ibuf->flags & IB_cm_predivide;
	float dither = ibuf->dither;
	ColorSpace *rect_colorspace = ibuf->rect_colorspace;
	float *display_buffer_float = NULL;
	int width = xmax - xmin;
	int height = ymax - ymin;

	if (dither != 0.0f) {
		display_buffer_float = MEM_callocN(channels * width * height * sizeof(float), "display buffer for dither");
	}

	for (y = ymin; y < ymax; y++) {
		for (x = xmin; x < xmax; x++) {
			int display_index = (y * display_stride + x) * channels;
			int linear_index = ((y - linear_offset_y) * linear_stride + (x - linear_offset_x)) * channels;
			float pixel[4];

			if (linear_buffer) {
				copy_v4_v4(pixel, (float *) linear_buffer + linear_index);
			}
			else if (byte_buffer) {
				rgba_uchar_to_float(pixel, byte_buffer + linear_index);

				IMB_colormanagement_colorspace_to_scene_linear_v3(pixel, rect_colorspace);
			}

			if (predivide)
				IMB_colormanagement_processor_apply_v4(cm_processor, pixel);
			else
				IMB_colormanagement_processor_apply_v4(cm_processor, pixel);

			if (display_buffer_float) {
				int index = ((y - ymin) * width + (x - xmin)) * channels;

				copy_v4_v4(display_buffer_float + index, pixel);
			}
			else {
				rgba_float_to_uchar(display_buffer + display_index, pixel);
			}
		}
	}

	if (display_buffer_float) {
		int display_index = (ymin * display_stride + xmin) * channels;

		IMB_buffer_byte_from_float(display_buffer + display_index, display_buffer_float, channels, dither,
		                           IB_PROFILE_SRGB, IB_PROFILE_SRGB, FALSE, width, height, display_stride, width);

		MEM_freeN(display_buffer_float);
	}
}
#endif

void IMB_partial_display_buffer_update(ImBuf *ibuf, const float *linear_buffer, const unsigned char *byte_buffer,
                                       int stride, int offset_x, int offset_y, const ColorManagedViewSettings *view_settings,
                                       const ColorManagedDisplaySettings *display_settings,
                                       int xmin, int ymin, int xmax, int ymax)
{
	if (ibuf->rect && ibuf->rect_float) {
		/* update byte buffer created by legacy color management */

		unsigned char *rect = (unsigned char *) ibuf->rect;
		int predivide = ibuf->flags & IB_cm_predivide;
		int channels = ibuf->channels;
		int profile_from = ibuf->profile;
		int width = xmax - xmin;
		int height = ymax - ymin;
		int rect_index = (ymin * ibuf->x + xmin) * channels;
		int linear_index = ((ymin - offset_y) * stride + (xmin - offset_x)) * channels;

		if (profile_from == IB_PROFILE_NONE)
			profile_from = IB_PROFILE_LINEAR_RGB;

		IMB_buffer_byte_from_float(rect + rect_index, linear_buffer + linear_index, channels, ibuf->dither,
		                           IB_PROFILE_SRGB, profile_from, predivide, width, height, ibuf->x, stride);
	}

#ifdef WITH_OCIO
	if (ibuf->display_buffer_flags) {
		ColormanageCacheViewSettings cache_view_settings;
		ColormanageCacheDisplaySettings cache_display_settings;
		void *cache_handle = NULL;
		unsigned char *display_buffer = NULL;
		int view_flag, display_index, buffer_width;

		colormanage_view_settings_to_cache(&cache_view_settings, view_settings);
		colormanage_display_settings_to_cache(&cache_display_settings, display_settings);

		view_flag = 1 << (cache_view_settings.view - 1);
		display_index = cache_display_settings.display - 1;

		BLI_lock_thread(LOCK_COLORMANAGE);
		if ((ibuf->userflags & IB_DISPLAY_BUFFER_INVALID) == 0)
			display_buffer = colormanage_cache_get(ibuf, &cache_view_settings, &cache_display_settings, &cache_handle);

		/* in some rare cases buffer's dimension could be changing directly from
		 * different thread
		 * this i.e. happens when image editor acquires render result
		 */
		buffer_width = ibuf->x;

		/* mark all other buffers as invalid */
		memset(ibuf->display_buffer_flags, 0, global_tot_display * sizeof(unsigned int));
		ibuf->display_buffer_flags[display_index] |= view_flag;

		BLI_unlock_thread(LOCK_COLORMANAGE);

		if (display_buffer) {
			ColormanageProcessor *cm_processor;

			cm_processor = IMB_colormanagement_display_processor_new(view_settings, display_settings);

			partial_buffer_update_rect(ibuf, display_buffer, linear_buffer, byte_buffer, buffer_width, stride,
			                           offset_x, offset_y, cm_processor, xmin, ymin, xmax, ymax);

			IMB_colormanagement_processor_free(cm_processor);

			IMB_display_buffer_release(cache_handle);
		}
	}
#else
	(void) byte_buffer;
	(void) view_settings;
	(void) display_settings;
#endif
}

/*********************** Pixel processor functions *************************/

ColormanageProcessor *IMB_colormanagement_display_processor_new(const ColorManagedViewSettings *view_settings,
                                                                const ColorManagedDisplaySettings *display_settings)
{
	ColormanageProcessor *cm_processor;

	cm_processor = MEM_callocN(sizeof(ColormanageProcessor), "colormanagement processor");

#ifdef WITH_OCIO
	{
		ColorManagedViewSettings default_view_settings;
		const ColorManagedViewSettings *applied_view_settings;

		if (view_settings) {
			applied_view_settings = view_settings;
		}
		else {
			init_default_view_settings(display_settings,  &default_view_settings);
			applied_view_settings = &default_view_settings;
		}

		cm_processor->processor = create_display_buffer_processor(applied_view_settings->view_transform, display_settings->display_device,
		                                                          applied_view_settings->exposure, applied_view_settings->gamma);

		if (applied_view_settings->flag & COLORMANAGE_VIEW_USE_CURVES) {
			cm_processor->curve_mapping = curvemapping_copy(applied_view_settings->curve_mapping);
			curvemapping_premultiply(cm_processor->curve_mapping, FALSE);
		}
	}
#else
	(void) view_settings;
	(void) display_settings;

	/* assume input is in linear space and color management is always enabled
	 * seams to be quite reasonable behavior in cases there's no OCIO
	 */
	cm_processor->display_transform_cb_v3 = linearrgb_to_srgb_v3_v3;
	cm_processor->display_transform_predivide_cb_v4 = linearrgb_to_srgb_predivide_v4;
#endif

	return cm_processor;
}

ColormanageProcessor *IMB_colormanagement_colorspace_processor_new(const char *from_colorspace, const char *to_colorspace)
{
	ColormanageProcessor *cm_processor;

	cm_processor = MEM_callocN(sizeof(ColormanageProcessor), "colormanagement processor");

#ifdef WITH_OCIO
	cm_processor->processor = create_colorspace_transform_processor(from_colorspace, to_colorspace);
#else
	(void) from_colorspace;
	(void) to_colorspace;
#endif

	return cm_processor;
}

void IMB_colormanagement_processor_apply_v4(ColormanageProcessor *cm_processor, float pixel[4])
{
	if (cm_processor->curve_mapping)
		curvemapping_evaluate_premulRGBF(cm_processor->curve_mapping, pixel, pixel);

#ifdef WITH_OCIO
	OCIO_processorApplyRGBA(cm_processor->processor, pixel);
#else
	if (cm_processor->display_transform_cb_v3)
		cm_processor->display_transform_cb_v3(pixel, pixel);
#endif
}

void IMB_colormanagement_processor_apply_v3(ColormanageProcessor *cm_processor, float pixel[3])
{
	if (cm_processor->curve_mapping)
		curvemapping_evaluate_premulRGBF(cm_processor->curve_mapping, pixel, pixel);

#ifdef WITH_OCIO
	OCIO_processorApplyRGB(cm_processor->processor, pixel);
#else
	if (cm_processor->display_transform_cb_v3)
		cm_processor->display_transform_cb_v3(pixel, pixel);
#endif
}

void IMB_colormanagement_processor_apply(ColormanageProcessor *cm_processor, float *buffer, int width, int height,
                                         int channels, int predivide)
{
	/* apply curve mapping */
	if (cm_processor->curve_mapping) {
		int x, y;

		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x++) {
				float *pixel = buffer + channels * (y * width + x);

				curve_mapping_apply_pixel(cm_processor->curve_mapping, pixel, channels);
			}
		}
	}

#ifdef WITH_OCIO
	{
		PackedImageDesc *img;

		/* apply OCIO processor */
		img = OCIO_createPackedImageDesc(buffer, width, height, channels, sizeof(float),
		                                 channels * sizeof(float), channels * sizeof(float) * width);

		if (predivide)
			OCIO_processorApply_predivide(cm_processor->processor, img);
		else
			OCIO_processorApply(cm_processor->processor, img);

		OCIO_packedImageDescRelease(img);
	}
#else
	if (cm_processor->display_transform_cb_v3) {
		int x, y;

		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x++) {
				float *pixel = buffer + channels * (y * width + x);

				if (channels == 3) {
					cm_processor->display_transform_cb_v3(pixel, pixel);
				}
				else if (channels == 4) {
					if (!predivide)
						cm_processor->display_transform_predivide_cb_v4(pixel, pixel);
					else
						cm_processor->display_transform_cb_v3(pixel, pixel);
				}
			}
		}
	}
#endif
}

void IMB_colormanagement_processor_free(ColormanageProcessor *cm_processor)
{
#ifdef WITH_OCIO
	if (cm_processor->curve_mapping)
		curvemapping_free(cm_processor->curve_mapping);

	OCIO_processorRelease(cm_processor->processor);
#endif

	MEM_freeN(cm_processor);
}
