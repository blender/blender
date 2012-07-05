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
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "IMB_filter.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_moviecache.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_math_color.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_threads.h"

#include "BKE_utildefines.h"
#include "BKE_main.h"

#include "RNA_define.h"

#ifdef WITH_OCIO
#  include <ocio_capi.h>
#endif

/*********************** Global declarations *************************/

/* ** list of all supported color spaces, displays and views */
#ifdef WITH_OCIO
static ListBase global_colorspaces = {NULL};

static char global_role_linear[64];
static char global_role_color_picking[64];
static char global_role_texture_painting[64];

#endif

static ListBase global_displays = {NULL};
static ListBase global_views = {NULL};

static int global_tot_display = 0;
static int global_tot_view = 0;

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
	int view;
	float exposure;
	float gamma;
} ColormanageCacheViewSettings;

typedef struct ColormanageCacheDisplaySettings {
	int display;
} ColormanageCacheDisplaySettings;

typedef struct ColormanageCacheKey {
	int view;            /* view transformation used for display buffer */
	int display;         /* display device name */
} ColormanageCacheKey;

typedef struct ColormnaageCacheData {
	float exposure;  /* exposure value cached buffer is calculated with */
	float gamma;     /* gamma value cached buffer is calculated with */
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
	if (!ibuf->colormanage_cache) {
		ibuf->colormanage_cache = MEM_callocN(sizeof(ColormanageCache), "imbuf colormanage ca cache");
	}

	if (!ibuf->colormanage_cache->moviecache) {
		struct MovieCache *moviecache;

		moviecache = IMB_moviecache_create("colormanage cache", sizeof(ColormanageCacheKey), colormanage_hashhash, colormanage_hashcmp);

		ibuf->colormanage_cache->moviecache = moviecache;
	}

	return ibuf->colormanage_cache->moviecache;
}

static void colormanage_cachedata_set(ImBuf *ibuf, ColormnaageCacheData *data)
{
	if (!ibuf->colormanage_cache) {
		ibuf->colormanage_cache = MEM_callocN(sizeof(ColormanageCache), "imbuf colormanage ca cache");
	}

	ibuf->colormanage_cache->data = data;
}

static void colormanage_view_settings_to_cache(ColormanageCacheViewSettings *cache_view_settings,
                                               const ColorManagedViewSettings *view_settings)
{
	int view = IMB_colormanagement_view_get_named_index(view_settings->view_transform);

	cache_view_settings->view = view;
	cache_view_settings->exposure = view_settings->exposure;
	cache_view_settings->gamma = view_settings->gamma;
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
		/* if there's no moviecache it means no color management was applied before */

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

	colormanage_settings_to_key(&key, view_settings, display_settings);

	/* check whether image was marked as dirty for requested transform */
	if ((ibuf->display_buffer_flags[display_settings->display - 1] & view_flag) == 0) {
		return NULL;
	}

	cache_ibuf = colormanage_cache_get_ibuf(ibuf, &key, cache_handle);

	if (cache_ibuf) {
		ColormnaageCacheData *cache_data;

		/* only buffers with different color space conversions are being stored
		 * in cache separately. buffer which were used only different exposure/gamma
		 * are re-suing the same cached buffer
		 *
		 * check here which exposure/gamma was used for cached buffer and if they're
		 * different from requested buffer should be re-generated
		 */
		cache_data = colormanage_cachedata_get(cache_ibuf);

		if (cache_data->exposure != view_settings->exposure ||
			cache_data->gamma != view_settings->gamma)
		{
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
	struct MovieCache *moviecache = colormanage_moviecache_ensure(ibuf);

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

	colormanage_cachedata_set(cache_ibuf, cache_data);

	*cache_handle = cache_ibuf;

	IMB_moviecache_put(moviecache, &key, cache_ibuf);
}

/* validation function checks whether there's buffer with given display transform
 * in the cache and if so, check whether it matches resolution of source buffer.
 * if resolution is different new buffer would be put into the cache and it'll
 * be returned as a result
 *
 * this function does not check exposure / gamma because currently it's only
 * used by partial buffer update functions which uses the same exposure / gamma
 * settings as cached buffer had
 */
static unsigned char *colormanage_cache_get_validated(ImBuf *ibuf, const ColormanageCacheViewSettings *view_settings,
                                                      const ColormanageCacheDisplaySettings *display_settings,
                                                      void **cache_handle)
{
	ColormanageCacheKey key;
	ImBuf *cache_ibuf;

	colormanage_settings_to_key(&key, view_settings, display_settings);

	cache_ibuf = colormanage_cache_get_ibuf(ibuf, &key, cache_handle);

	if (cache_ibuf) {
		if (cache_ibuf->x != ibuf->x || cache_ibuf->y != ibuf->y) {
			ColormanageCacheViewSettings new_view_settings = *view_settings;
			ColormnaageCacheData *cache_data;
			unsigned char *display_buffer;
			int buffer_size;

			/* use the same settings as original cached buffer  */
			cache_data = colormanage_cachedata_get(cache_ibuf);
			new_view_settings.exposure = cache_data->exposure;
			new_view_settings.gamma = cache_data->gamma;

			buffer_size = ibuf->channels * ibuf->x * ibuf->y * sizeof(float);
			display_buffer = MEM_callocN(buffer_size, "imbuf validated display buffer");

			colormanage_cache_put(ibuf, &new_view_settings, display_settings, display_buffer, cache_handle);

			IMB_freeImBuf(cache_ibuf);

			return display_buffer;
		}

		return (unsigned char *) cache_ibuf->rect;
	}

	return NULL;
}

/* return view settings which are stored in cached buffer, not in key itself */
static void colormanage_cache_get_cache_data(void *cache_handle, float *exposure, float *gamma)
{
	ImBuf *cache_ibuf = (ImBuf *) cache_handle;
	ColormnaageCacheData *cache_data;

	cache_data = colormanage_cachedata_get(cache_ibuf);

	*exposure = cache_data->exposure;
	*gamma = cache_data->gamma;
}
#endif

static void colormanage_cache_handle_release(void *cache_handle)
{
	ImBuf *cache_ibuf = cache_handle;

	IMB_freeImBuf(cache_ibuf);
}

/*********************** Initialization / De-initialization *************************/

#ifdef WITH_OCIO
static void colormanage_role_color_space_name_get(ConstConfigRcPtr *config, char *colorspace_name,
                                                  int max_colorspace_name, const char *role, const char *role_name)
{
	ConstColorSpaceRcPtr *ociocs;

	ociocs = OCIO_configGetColorSpace(config, role);

	if (ociocs) {
		const char *name = OCIO_colorSpaceGetName(ociocs);

		BLI_strncpy(colorspace_name, name, max_colorspace_name);
		OCIO_colorSpaceRelease(ociocs);
	}
	else {
		printf("Blender color management: Error could not find %s role.\n", role_name);
	}
}

static void colormanage_load_config(ConstConfigRcPtr *config)
{
	ConstColorSpaceRcPtr *ociocs;
	int tot_colorspace, tot_display, tot_display_view, index, viewindex, viewindex2;
	const char *name;

	/* get roles */
	colormanage_role_color_space_name_get(config, global_role_linear, sizeof(global_role_linear),
	                                      OCIO_ROLE_SCENE_LINEAR, "scene linear");

	colormanage_role_color_space_name_get(config, global_role_color_picking, sizeof(global_role_color_picking),
	                                      OCIO_ROLE_COLOR_PICKING, "color picking");

	colormanage_role_color_space_name_get(config, global_role_texture_painting, sizeof(global_role_texture_painting),
	                                      OCIO_ROLE_TEXTURE_PAINT, "texture_painting");

	/* load colorspaces */
	tot_colorspace = OCIO_configGetNumColorSpaces(config);
	for (index = 0 ; index < tot_colorspace; index++) {
		ColorSpace *colorspace;

		name = OCIO_configGetColorSpaceNameByIndex(config, index);
		ociocs = OCIO_configGetColorSpace(config, name);

		colorspace = MEM_callocN(sizeof(ColorSpace), "ColorSpace");
		colorspace->index = index + 1;

		BLI_strncpy(colorspace->name, name, sizeof(colorspace->name));

		BLI_addtail(&global_colorspaces, colorspace);

		OCIO_colorSpaceRelease(ociocs);
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
	ColorManagedView *view;

	colorspace = global_colorspaces.first;
	while (colorspace) {
		ColorSpace *colorspace_next = colorspace->next;

		MEM_freeN(colorspace);
		colorspace = colorspace_next;
	}

	display = global_displays.first;
	while (display) {
		ColorManagedDisplay *display_next = display->next;
		LinkData *display_view = display->views.first;

		while (display_view) {
			LinkData *display_view_next = display_view->next;

			MEM_freeN(display_view);
			display_view = display_view_next;
		}

		MEM_freeN(display);
		display = display_next;
	}

	view = global_views.first;
	while (view) {
		ColorManagedView *view_next = view->next;

		MEM_freeN(view);
		view = view_next;
	}
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

	if (ocio_env) {
		config = OCIO_configCreateFromEnv();
	}
	else {
		configdir = BLI_get_folder(BLENDER_DATAFILES, "colormanagement");

		if (configdir) 	{
			BLI_join_dirfile(configfile, sizeof(configfile), configdir, BCM_CONFIG_FILE);

			config = OCIO_configCreateFromFile(configfile);
		}
	}

	if (config) {
		OCIO_setCurrentConfig(config);

		colormanage_load_config(config);
	}

	OCIO_configRelease(config);

	/* special views, which does not depend on OCIO  */
	colormanage_view_add("ACES ODT Tonecurve");
#endif
}

void IMB_colormanagement_exit(void)
{
#ifdef WITH_OCIO
	colormanage_free_config();
#endif
}

/*********************** Public display buffers interfaces *************************/

#ifdef WITH_OCIO
typedef struct DisplayBufferThread {
	void *processor;

	float *buffer;
	unsigned char *display_buffer;

	int width;
	int start_line;
	int tot_line;

	int channels;
	int dither;
	int predivide;
} DisplayBufferThread;

static void display_buffer_apply_threaded(ImBuf *ibuf, float *buffer, unsigned char *display_buffer,
                                          void *processor, void *(do_thread) (void *))
{
	DisplayBufferThread handles[BLENDER_MAX_THREADS];
	ListBase threads;

	int predivide = ibuf->flags & IB_cm_predivide;
	int i, tot_thread = BLI_system_thread_count();
	int start_line, tot_line;

	if (tot_thread > 1)
		BLI_init_threads(&threads, do_thread, tot_thread);

	start_line = 0;
	tot_line = ((float)(ibuf->y / tot_thread)) + 0.5f;

	for (i = 0; i < tot_thread; i++) {
		int offset = ibuf->channels * start_line * ibuf->x;

		handles[i].processor = processor;

		handles[i].buffer = buffer + offset;
		handles[i].display_buffer = display_buffer + offset;
		handles[i].width = ibuf->x;

		handles[i].start_line = start_line;

		if (i < tot_thread - 1) {
			handles[i].tot_line = tot_line;
		}
		else {
			handles[i].tot_line = ibuf->y - start_line;
		}

		handles[i].channels = ibuf->channels;
		handles[i].dither = ibuf->dither;
		handles[i].predivide = predivide;

		if (tot_thread > 1)
			BLI_insert_thread(&threads, &handles[i]);

		start_line += tot_line;
	}

	if (tot_thread > 1)
		BLI_end_threads(&threads);
	else
		do_thread(&handles[0]);
}

static void *do_display_buffer_apply_tonemap_thread(void *handle_v)
{
	DisplayBufferThread *handle = (DisplayBufferThread *) handle_v;
	imb_tonecurveCb tonecurve_func = (imb_tonecurveCb) handle->processor;

	float *buffer = handle->buffer;
	unsigned char *display_buffer = handle->display_buffer;

	int channels = handle->channels;
	int width = handle->width;
	int height = handle->tot_line;
	int dither = handle->dither;
	int predivide = handle->predivide;

	IMB_buffer_byte_from_float_tonecurve(display_buffer, buffer, channels, dither,
	                                     IB_PROFILE_SRGB, IB_PROFILE_LINEAR_RGB,
	                                     predivide, width, height, width, width,
	                                     tonecurve_func);

	return NULL;
}

static void display_buffer_apply_tonemap(ImBuf *ibuf, unsigned char *display_buffer,
                                         imb_tonecurveCb tonecurve_func)
{
	display_buffer_apply_threaded(ibuf, ibuf->rect_float, display_buffer, tonecurve_func,
	                              do_display_buffer_apply_tonemap_thread);
}

static void *do_display_buffer_apply_ocio_thread(void *handle_v)
{
	DisplayBufferThread *handle = (DisplayBufferThread *) handle_v;
	ConstProcessorRcPtr *processor = (ConstProcessorRcPtr *) handle->processor;
	PackedImageDesc *img;
	float *buffer = handle->buffer;
	unsigned char *display_buffer = handle->display_buffer;
	int channels = handle->channels;
	int width = handle->width;
	int height = handle->tot_line;
	int dither = handle->dither;
	int predivide = handle->predivide;

	img = OCIO_createPackedImageDesc(buffer, width, height, channels, sizeof(float),
	                                 channels * sizeof(float), channels * sizeof(float) * width);

	OCIO_processorApply(processor, img);

	OCIO_packedImageDescRelease(img);

	/* do conversion */
	IMB_buffer_byte_from_float(display_buffer, buffer,
	                           channels, dither, IB_PROFILE_SRGB, IB_PROFILE_SRGB,
	                           predivide, width, height, width, width);

	return NULL;
}

static ConstProcessorRcPtr *create_display_buffer_processor(const char *view_transform, const char *display,
                                                            float exposure, float gamma)
{
	ConstConfigRcPtr *config = OCIO_getCurrentConfig();
	DisplayTransformRcPtr *dt;
	ExponentTransformRcPtr *et;
	MatrixTransformRcPtr *mt;
	ConstProcessorRcPtr *processor;

	float exponent = 1.0f / MAX2(FLT_EPSILON, gamma);
	const float exponent4f[] = {exponent, exponent, exponent, exponent};

	float gain = powf(2.0f, exposure);
	const float scale4f[] = {gain, gain, gain, gain};
	float m44[16], offset4[4];

	if (!config) {
		/* there's no valid OCIO configuration, can't create processor */

		return NULL;
	}

	dt = OCIO_createDisplayTransform();

	/* OCIO_TODO: get rid of hardcoded input space */
	OCIO_displayTransformSetInputColorSpaceName(dt, global_role_linear);

	OCIO_displayTransformSetView(dt, view_transform);
	OCIO_displayTransformSetDisplay(dt, display);

	/* fstop exposure control */
	OCIO_matrixTransformScale(m44, offset4, scale4f);
	mt = OCIO_createMatrixTransform();
	OCIO_matrixTransformSetValue(mt, m44, offset4);
	OCIO_displayTransformSetLinearCC(dt, (ConstTransformRcPtr *) mt);

	/* post-display gamma transform */
	et = OCIO_createExponentTransform();
	OCIO_exponentTransformSetValue(et, exponent4f);
	OCIO_displayTransformSetDisplayCC(dt, (ConstTransformRcPtr *) et);

	processor = OCIO_configGetProcessor(config, (ConstTransformRcPtr *) dt);

	OCIO_exponentTransformRelease(et);
	OCIO_displayTransformRelease(dt);
	OCIO_configRelease(config);

	return processor;
}

static void display_buffer_apply_ocio(ImBuf *ibuf, unsigned char *display_buffer,
                                      const ColorManagedViewSettings *view_settings,
                                      const ColorManagedDisplaySettings *display_settings)
{
	ConstProcessorRcPtr *processor;
	const float gamma = view_settings->gamma;
	const float exposure = view_settings->exposure;
	const char *view_transform = view_settings->view_transform;
	const char *display = display_settings->display_device;
	float *rect_float;

	rect_float = MEM_dupallocN(ibuf->rect_float);

	processor = create_display_buffer_processor(view_transform, display, exposure, gamma);

	if (processor) {
		display_buffer_apply_threaded(ibuf, rect_float, display_buffer, processor,
		                              do_display_buffer_apply_ocio_thread);
	}

	OCIO_processorRelease(processor);

	MEM_freeN(rect_float);
}

static void colormanage_display_buffer_process(ImBuf *ibuf, unsigned char *display_buffer,
                                               const ColorManagedViewSettings *view_settings,
                                               const ColorManagedDisplaySettings *display_settings)
{
	const char *view_transform = view_settings->view_transform;

	if (!strcmp(view_transform, "ACES ODT Tonecurve")) {
		/* special case for Mango team, this does not actually apply
		 * any input space -> display space conversion and just applies
		 * a tonecurve for better linear float -> sRGB byte conversion
		 */
		display_buffer_apply_tonemap(ibuf, display_buffer, IMB_ratio_preserving_odt_tonecurve);
	}
	else {
		display_buffer_apply_ocio(ibuf, display_buffer, view_settings, display_settings);
	}

}

static void colormanage_flags_allocate(ImBuf *ibuf)
{
	if (global_tot_display == 0)
		return;

	ibuf->display_buffer_flags = MEM_callocN(sizeof(unsigned int) * global_tot_display, "imbuf display_buffer_flags");
}
#endif

void IMB_colormanage_cache_free(ImBuf *ibuf)
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

unsigned char *IMB_display_buffer_acquire(ImBuf *ibuf, const ColorManagedViewSettings *view_settings,
                                          const ColorManagedDisplaySettings *display_settings, void **cache_handle)
{
	const char *view_transform = view_settings->view_transform;

	*cache_handle = NULL;

#ifdef WITH_OCIO

	if (!ibuf->x || !ibuf->y)
		return NULL;

	/* OCIO_TODO: support colormanaged byte buffers */
	if (!strcmp(view_transform, "NONE") ||
	    !ibuf->rect_float ||
	    global_tot_display == 0 ||
	    global_tot_view == 0)
	{
		/* currently only view-transformation is allowed, input and display
		 * spaces are hard-coded, so if there's no view transform applying
		 * it's safe to suppose standard byte buffer is used for display
		 */

		if (!ibuf->rect)
			IMB_rect_from_float(ibuf);

		return (unsigned char *) ibuf->rect;
	}
	else {
		unsigned char *display_buffer;
		int buffer_size;
		ColormanageCacheViewSettings cache_view_settings;
		ColormanageCacheDisplaySettings cache_display_settings;

		colormanage_view_settings_to_cache(&cache_view_settings, view_settings);
		colormanage_display_settings_to_cache(&cache_display_settings, display_settings);

		/* ensure color management bit fields exists */
		if (!ibuf->display_buffer_flags)
			colormanage_flags_allocate(ibuf);

		display_buffer = colormanage_cache_get(ibuf, &cache_view_settings, &cache_display_settings, cache_handle);

		if (display_buffer) {
			return display_buffer;
		}

		/* OCIO_TODO: in case when image is being resized it is possible
		 *            to save buffer allocation here
		 *
		 *            actually not because there might be other users of
		 *            that buffer which better not to change
		 */

		buffer_size = ibuf->channels * ibuf->x * ibuf->y * sizeof(float);
		display_buffer = MEM_callocN(buffer_size, "imbuf display buffer");

		colormanage_display_buffer_process(ibuf, display_buffer, view_settings, display_settings);

		colormanage_cache_put(ibuf, &cache_view_settings, &cache_display_settings, display_buffer, cache_handle);

		return display_buffer;
	}
#else
	/* no OCIO support, simply return byte buffer which was
	 * generated from float buffer (if any) using standard
	 * profiles without applying any view / display transformation */

	(void) view_settings;
	(void) view_transform;
	(void) display_settings;

	if (!ibuf->rect) {
		IMB_rect_from_float(ibuf);
	}

	return (unsigned char*) ibuf->rect;
#endif
}

void IMB_display_buffer_to_imbuf_rect(ImBuf *ibuf, const ColorManagedViewSettings *view_settings,
                                      const ColorManagedDisplaySettings *display_settings)
{
#ifdef WITH_OCIO
	const char *view_transform = view_settings->view_transform;

	if (!ibuf->rect_float)
		return;

	if (!strcmp(view_transform, "NONE") ||
	    !ibuf->rect_float ||
	    global_tot_display == 0 ||
	    global_tot_view == 0)
	{
		if (!ibuf->rect)
			IMB_rect_from_float(ibuf);
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

	if (!ibuf->rect)
		IMB_rect_from_float(ibuf);
#endif
}

void IMB_display_buffer_release(void *cache_handle)
{
	if (cache_handle) {
		colormanage_cache_handle_release(cache_handle);
	}
}

void IMB_display_buffer_invalidate(ImBuf *ibuf)
{
	/* if there's no display_buffer_flags this means there's no color managed
	 * buffers created for this imbuf, no need to invalidate
	 */
	if (ibuf->display_buffer_flags) {
		memset(ibuf->display_buffer_flags, 0, global_tot_display * sizeof(unsigned int));
	}
}

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
			printf("Blender color management: display \"%s\" used by %s not found, setting to default (\"%s\").\n",
			       display_settings->display_device, what, default_display->name);

			BLI_strncpy(display_settings->display_device, default_display->name,
			            sizeof(display_settings->display_device));
		}
	}
}

static void colormanage_check_view_settings(ColorManagedViewSettings *view_settings, const char *what,
                                            const ColorManagedView *default_view)
{
	if (view_settings->view_transform[0] == '\0') {
		BLI_strncpy(view_settings->view_transform, "NONE", sizeof(view_settings->view_transform));
	}
	else if (!strcmp(view_settings->view_transform, "NONE")) {
		/* pass */
	}
	else {
		ColorManagedView *view = colormanage_view_get_named(view_settings->view_transform);

		if (!view) {
			printf("Blender color management: %s view \"%s\" not found, setting default \"%s\".\n",
			       what, view_settings->view_transform, default_view->name);

			BLI_strncpy(view_settings->view_transform, default_view->name, sizeof(view_settings->view_transform));
		}
	}

	/* OCIO_TODO: move to do_versions() */
	if (view_settings->exposure == 0.0f && view_settings->gamma == 0.0f) {
		view_settings->flag |= COLORMANAGE_VIEW_USE_GLOBAL;
		view_settings->exposure = 0.0f;
		view_settings->gamma = 1.0f;
	}
}
#endif

void IMB_colormanagement_check_file_config(Main *bmain)
{
#ifdef WITH_OCIO
	wmWindowManager *wm = bmain->wm.first;
	wmWindow *win;
	bScreen *sc;
	Scene *scene;

	ColorManagedDisplay *default_display;
	ColorManagedView *default_view;

	default_display = colormanage_display_get_default();

	if (!default_display) {
		/* happens when OCIO configuration is incorrect */
		return;
	}

	default_view = colormanage_view_get_default(default_display);

	if (!default_view) {
		/* happens when OCIO configuration is incorrect */
		return;
	}

	if (wm) {
		for (win = wm->windows.first; win; win = win->next) {
			colormanage_check_display_settings(&win->display_settings, "window", default_display);

			colormanage_check_view_settings(&win->view_settings, "window", default_view);
		}
	}

	for (sc = bmain->screen.first; sc; sc = sc->id.next) {
		ScrArea *sa;

		for (sa = sc->areabase.first; sa; sa = sa->next) {
			SpaceLink *sl;
			for (sl = sa->spacedata.first; sl; sl = sl->next) {

				if (sl->spacetype == SPACE_IMAGE) {
					SpaceImage *sima = (SpaceImage *) sl;

					colormanage_check_view_settings(&sima->view_settings, "image editor", default_view);
				}
				else if (sl->spacetype == SPACE_NODE) {
					SpaceNode *snode = (SpaceNode *) sl;

					colormanage_check_view_settings(&snode->view_settings, "node editor", default_view);
				}
				else if (sl->spacetype == SPACE_CLIP) {
					SpaceClip *sclip = (SpaceClip *) sl;

					colormanage_check_view_settings(&sclip->view_settings, "clip editor", default_view);
				}
			}
		}
	}

	for (scene = bmain->scene.first; scene; scene = scene->id.next) {
		ImageFormatData *imf = 	&scene->r.im_format;

		colormanage_check_display_settings(&imf->display_settings, "scene", default_display);

		colormanage_check_view_settings(&imf->view_settings, "scene", default_view);
	}
#else
	(void) bmain;
#endif
}

const ColorManagedViewSettings *IMB_view_settings_get_effective(wmWindow *win,
		const ColorManagedViewSettings *view_settings)
{
	if (view_settings->flag & COLORMANAGE_VIEW_USE_GLOBAL) {
		return &win->view_settings;
	}

	return view_settings;
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

/*********************** View functions *************************/

#ifdef WITH_OCIO
ColorManagedView *colormanage_view_get_default(const ColorManagedDisplay *display)
{
	ConstConfigRcPtr *config = OCIO_getCurrentConfig();
	const char *name;

	if (!config) {
		/* no valid OCIO configuration, can't get default view */

		return NULL;
	}

	name = OCIO_configGetDefaultView(config, display->name);

	OCIO_configRelease(config);

	if (name[0] == '\0')
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

	return "NONE";
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

	/* OCIO_TODO: try to get rid of such a hackish stuff */
	view = colormanage_view_get_named("ACES ODT Tonecurve");
	if (view) {
		colormanagement_view_item_add(items, totitem, view);
	}

	if (display) {
		LinkData *display_view;

		for (display_view = display->views.first; display_view; display_view = display_view->next) {
			view = display_view->data;

			colormanagement_view_item_add(items, totitem, view);
		}
	}
}

/*********************** Partial display buffer update  *************************/

/*
 * Partial display update is supposed to be used by such areas as compositor,
 * which re-calculates parts of the images and requires updating only
 * specified areas of buffers to provide better visual feedback.
 *
 * To achieve this special context is being constructed. This context is
 * holding all buffers which were color managed and transformations which
 * need to be applied on this buffers to make them valid.
 *
 * Updating happens for all buffers from this context using given linear
 * float buffer and rectangle area which shall be updated.
 *
 * Updating every rectangle is thread-save operation due to buffers are
 * referenced by the context, so they shouldn't have been deleted
 * during execution.
 */

typedef struct PartialBufferUpdateItem {
	struct PartialBufferUpdateItem *next, *prev;

	unsigned char *display_buffer;
	void *cache_handle;

	int display, view;

#ifdef WITH_OCIO
	ConstProcessorRcPtr *processor;
#endif

	imb_tonecurveCb tonecurve_func;
} PartialBufferUpdateItem;

typedef struct PartialBufferUpdateContext {
	int buffer_width;
	int dither, predivide;

	ListBase items;
} PartialBufferUpdateContext;

PartialBufferUpdateContext *IMB_partial_buffer_update_context_new(ImBuf *ibuf)
{
	PartialBufferUpdateContext *context = NULL;

#ifdef WITH_OCIO
	int display;

	context = MEM_callocN(sizeof(PartialBufferUpdateContext), "partial buffer update context");

	context->buffer_width = ibuf->x;

	context->predivide = ibuf->flags & IB_cm_predivide;
	context->dither = ibuf->dither;

	if (!ibuf->display_buffer_flags) {
		/* there's no cached display buffers, so no need to iterate though bit fields */
		return context;
	}

	for (display = 0; display < global_tot_display; display++) {
		ColormanageCacheDisplaySettings display_settings = {0};
		int display_index = display + 1; /* displays in configuration are 1-based */
		const char *display_name = IMB_colormanagement_display_get_indexed_name(display_index);
		int view_flags = ibuf->display_buffer_flags[display];
		int view = 0;

		display_settings.display = display_index;

		while (view_flags != 0) {
			if (view_flags % 2 == 1) {
				ColormanageCacheViewSettings view_settings = {0};
				unsigned char *display_buffer;
				void *cache_handle;
				int view_index = view + 1; /* views in configuration are 1-based */

				view_settings.view = view_index;

				display_buffer =
					colormanage_cache_get_validated(ibuf, &view_settings, &display_settings, &cache_handle);

				if (display_buffer) {
					PartialBufferUpdateItem *item;
					const char *view_name = IMB_colormanagement_view_get_indexed_name(view_index);
					float exposure, gamma;

					colormanage_cache_get_cache_data(cache_handle, &exposure, &gamma);

					item = MEM_callocN(sizeof(PartialBufferUpdateItem), "partial buffer update item");

					item->display_buffer = display_buffer;
					item->cache_handle = cache_handle;
					item->display = display_index;
					item->view = view_index;

					if (!strcmp(view_name, "ACES ODT Tonecurve")) {
						item->tonecurve_func = IMB_ratio_preserving_odt_tonecurve;
					}
					else {
						ConstProcessorRcPtr *processor;

						processor = create_display_buffer_processor(view_name, display_name, exposure, gamma);

						item->processor = processor;
					}

					BLI_addtail(&context->items, item);
				}
			}

			view_flags /= 2;
			view++;
		}
	}
#else
	(void) ibuf;
#endif

	return context;
}

void IMB_partial_buffer_update_rect(PartialBufferUpdateContext *context, const float *linear_buffer, struct rcti *rect)
{
#ifdef WITH_OCIO
	PartialBufferUpdateItem *item;

	for (item = context->items.first; item; item = item->next) {
		if (item->processor || item->tonecurve_func) {
			unsigned char *display_buffer = item->display_buffer;
			int x, y;

			for (y = rect->ymin; y < rect->ymax; y++) {
				for (x = rect->xmin; x < rect->xmax; x++) {
					int index = (y * context->buffer_width + x) * 4;
					float pixel[4];

					if (item->processor) {
						copy_v4_v4(pixel, (float *)linear_buffer + index);

						OCIO_processorApplyRGBA(item->processor, pixel);

						rgba_float_to_uchar(display_buffer + index, pixel);
					}
					else {
						IMB_buffer_byte_from_float_tonecurve(display_buffer + index, linear_buffer + index,
						                                     4, context->dither, IB_PROFILE_SRGB, IB_PROFILE_LINEAR_RGB,
						                                     context->predivide, 1, 1, 1, 1, item->tonecurve_func);
					}
				}
			}
		}
	}
#else
	(void) context;
	(void) linear_buffer;
	(void) rect;
#endif
}

void IMB_partial_buffer_update_free(PartialBufferUpdateContext *context, ImBuf *ibuf)
{
#ifdef WITH_OCIO
	PartialBufferUpdateItem *item;

	IMB_display_buffer_invalidate(ibuf);

	item = context->items.first;
	while (item) {
		PartialBufferUpdateItem *item_next = item->next;

		/* displays are 1-based, need to go to 0-based arrays indices */
		ibuf->display_buffer_flags[item->display - 1] |= (1 << (item->view - 1));

		colormanage_cache_handle_release(item->cache_handle);

		OCIO_processorRelease(item->processor);

		MEM_freeN(item);

		item = item_next;
	}

	MEM_freeN(context);
#else
	(void) context;
	(void) ibuf;
#endif
}
