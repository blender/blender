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

/* define this to allow byte buffers be color managed */
#undef COLORMANAGE_BYTE_BUFFER

#define ACES_ODT_TONECORVE "ACES ODT Tonecurve"

/* ** list of all supported color spaces, displays and views */
#ifdef WITH_OCIO
static char global_role_scene_linear[64];
static char global_role_color_picking[64];
static char global_role_texture_painting[64];
#endif

static ListBase global_colorspaces = {NULL};
static ListBase global_displays = {NULL};
static ListBase global_views = {NULL};

static int global_tot_colorspace = 0;
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
		ibuf->colormanage_cache = MEM_callocN(sizeof(ColormanageCache), "imbuf colormanage cache");
	}

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
	if (!ibuf->colormanage_cache) {
		ibuf->colormanage_cache = MEM_callocN(sizeof(ColormanageCache), "imbuf colormanage cache");
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

		BLI_assert(cache_ibuf->x == ibuf->x &&
		           cache_ibuf->y == ibuf->y &&
		           cache_ibuf->channels == ibuf->channels);

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

static unsigned char *colormanage_cache_get_cache_data(ImBuf *ibuf, const ColormanageCacheViewSettings *view_settings,
                                                       const ColormanageCacheDisplaySettings *display_settings,
                                                       void **cache_handle, float *exposure, float *gamma)
{
	ColormanageCacheKey key;
	ColormnaageCacheData *cache_data;
	ImBuf *cache_ibuf;

	colormanage_settings_to_key(&key, view_settings, display_settings);

	cache_ibuf = colormanage_cache_get_ibuf(ibuf, &key, cache_handle);

	if (cache_ibuf) {
		cache_data = colormanage_cachedata_get(cache_ibuf);

		*exposure = cache_data->exposure;
		*gamma = cache_data->gamma;

		return (unsigned char *) cache_ibuf->rect;
	}

	return NULL;
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
	int tot_colorspace, tot_display, tot_display_view, index, viewindex, viewindex2;
	const char *name;

	/* get roles */
	colormanage_role_color_space_name_get(config, global_role_scene_linear, sizeof(global_role_scene_linear),
	                                      OCIO_ROLE_SCENE_LINEAR, "scene linear");

	colormanage_role_color_space_name_get(config, global_role_color_picking, sizeof(global_role_color_picking),
	                                      OCIO_ROLE_COLOR_PICKING, "color picking");

	colormanage_role_color_space_name_get(config, global_role_texture_painting, sizeof(global_role_texture_painting),
	                                      OCIO_ROLE_TEXTURE_PAINT, "texture_painting");

	/* load colorspaces */
	tot_colorspace = OCIO_configGetNumColorSpaces(config);
	for (index = 0 ; index < tot_colorspace; index++) {
		ConstColorSpaceRcPtr *ocio_colorspace;
		const char *description;

		name = OCIO_configGetColorSpaceNameByIndex(config, index);

		ocio_colorspace = OCIO_configGetColorSpace(config, name);
		description = OCIO_colorSpaceGetDescription(ocio_colorspace);

		colormanage_colorspace_add(name, description);

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
	colormanage_view_add(ACES_ODT_TONECORVE);
#endif
}

void IMB_colormanagement_exit(void)
{
#ifdef WITH_OCIO
	colormanage_free_config();
#endif
}

/*********************** Threaded display buffer transform routines *************************/

#ifdef WITH_OCIO
static void colormanage_processor_apply_threaded(int buffer_lines, int handle_size, void *init_customdata,
                                                 void (init_handle) (void *handle, int start_line, int tot_line,
                                                                     void *customdata),
                                                 void *(do_thread) (void *))
{
	void *handles;
	ListBase threads;

	int i, tot_thread = BLI_system_thread_count();
	int start_line, tot_line;

	handles = MEM_callocN(handle_size * tot_thread, "processor apply threaded handles");

	if (tot_thread > 1)
		BLI_init_threads(&threads, do_thread, tot_thread);

	start_line = 0;
	tot_line = ((float)(buffer_lines / tot_thread)) + 0.5f;

	for (i = 0; i < tot_thread; i++) {
		int cur_tot_line;
		void *handle = ((char *) handles) + handle_size * i;

		if (i < tot_thread - 1)
			cur_tot_line = tot_line;
		else
			cur_tot_line = buffer_lines - start_line;

		init_handle(handle, start_line, cur_tot_line, init_customdata);

		if (tot_thread > 1)
			BLI_insert_thread(&threads, handle);

		start_line += tot_line;
	}

	if (tot_thread > 1)
		BLI_end_threads(&threads);
	else
		do_thread(handles);

	MEM_freeN(handles);
}

typedef struct DisplayBufferThread {
	void *processor;

	float *buffer;
	unsigned char *byte_buffer;
	unsigned char *display_buffer;

	int width;
	int start_line;
	int tot_line;

	int channels;
	int dither;
	int predivide;

	int buffer_in_srgb;
} DisplayBufferThread;

typedef struct DisplayBufferInitData {
	ImBuf *ibuf;
	void *processor;
	float *buffer;
	unsigned char *byte_buffer;
	unsigned char *display_buffer;
	int width;
} DisplayBufferInitData;

static void display_buffer_init_handle(void *handle_v, int start_line, int tot_line, void *init_data_v)
{
	DisplayBufferThread *handle = (DisplayBufferThread *) handle_v;
	DisplayBufferInitData *init_data = (DisplayBufferInitData *) init_data_v;
	ImBuf *ibuf = init_data->ibuf;

	int predivide = ibuf->flags & IB_cm_predivide;
	int channels = ibuf->channels;
	int dither = ibuf->dither;

	int offset = channels * start_line * ibuf->x;

	memset(handle, 0, sizeof(DisplayBufferThread));

	handle->processor = init_data->processor;

	if (init_data->buffer)
		handle->buffer = init_data->buffer + offset;

	if (init_data->byte_buffer)
		handle->byte_buffer = init_data->byte_buffer + offset;

	handle->display_buffer = init_data->display_buffer + offset;

	handle->width = ibuf->x;

	handle->start_line = start_line;
	handle->tot_line = tot_line;

	handle->channels = channels;
	handle->dither = dither;
	handle->predivide = predivide;

	handle->buffer_in_srgb = ibuf->colormanagement_flags & IMB_COLORMANAGEMENT_SRGB_SOURCE;
}

static void display_buffer_apply_threaded(ImBuf *ibuf, float *buffer, unsigned char *byte_buffer,
                                          unsigned char *display_buffer,
                                          void *processor, void *(do_thread) (void *))
{
	DisplayBufferInitData init_data;

	/* XXX: IMB_buffer_byte_from_float_tonecurve isn't thread-safe because of
	 *      possible non-initialized sRGB conversion stuff. Make sure it's properly
	 *      initialized before starting threads, but likely this stuff should be
	 *      initialized somewhere before to avoid possible issues in other issues.
	 */
	BLI_init_srgb_conversion();

	init_data.ibuf = ibuf;
	init_data.processor = processor;
	init_data.buffer = buffer;
	init_data.byte_buffer = byte_buffer;
	init_data.display_buffer = display_buffer;

	colormanage_processor_apply_threaded(ibuf->y, sizeof(DisplayBufferThread), &init_data,
	                                     display_buffer_init_handle, do_thread);
}

static void *display_buffer_apply_get_linear_buffer(DisplayBufferThread *handle)
{
	float *linear_buffer = NULL;

	int channels = handle->channels;
	int width = handle->width;
	int height = handle->tot_line;

	int buffer_size = channels * width * height;

	/* TODO: do we actually need to handle alpha premultiply in some way here? */
	int predivide = handle->predivide;

	linear_buffer = MEM_callocN(buffer_size * sizeof(float), "color conversion linear buffer");

	if (!handle->buffer) {
		unsigned char *byte_buffer = handle->byte_buffer;

		/* OCIO_TODO: for now assume byte buffers are in sRGB space,
		 *            in the future it shall use color space specified
		 *            by user
		 */
		IMB_buffer_float_from_byte(linear_buffer, byte_buffer,
		                           IB_PROFILE_LINEAR_RGB, IB_PROFILE_SRGB,
		                           predivide, width, height, width, width);
	}
	else if (handle->buffer_in_srgb) {
		/* sequencer is working with float buffers which are in sRGB space,
		 * so we need to ensure float buffer is in linear space before
		 * applying all the view transformations
		 */
		float *buffer = handle->buffer;

		IMB_buffer_float_from_float(linear_buffer, buffer, channels,
		                            IB_PROFILE_LINEAR_RGB, IB_PROFILE_SRGB,
		                            predivide, width, height, width, width);
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

	float *linear_buffer = display_buffer_apply_get_linear_buffer(handle);

	IMB_buffer_byte_from_float_tonecurve(display_buffer, linear_buffer, channels, dither,
	                                     IB_PROFILE_SRGB, IB_PROFILE_LINEAR_RGB,
	                                     predivide, width, height, width, width,
	                                     tonecurve_func);

	if (linear_buffer != buffer)
		MEM_freeN(linear_buffer);

	return NULL;
}

static void display_buffer_apply_tonemap(ImBuf *ibuf, unsigned char *display_buffer,
                                         imb_tonecurveCb tonecurve_func)
{
	display_buffer_apply_threaded(ibuf, ibuf->rect_float, (unsigned char *)ibuf->rect,
	                              display_buffer, tonecurve_func,
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

	float *linear_buffer = display_buffer_apply_get_linear_buffer(handle);

	img = OCIO_createPackedImageDesc(linear_buffer, width, height, channels, sizeof(float),
	                                 channels * sizeof(float), channels * sizeof(float) * width);

	OCIO_processorApply(processor, img);

	OCIO_packedImageDescRelease(img);

	/* do conversion */
	IMB_buffer_byte_from_float(display_buffer, linear_buffer,
	                           channels, dither, IB_PROFILE_SRGB, IB_PROFILE_SRGB,
	                           predivide, width, height, width, width);

	if (linear_buffer != buffer)
		MEM_freeN(linear_buffer);

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

	/* assuming handling buffer was already converted to scene linear space */
	OCIO_displayTransformSetInputColorSpaceName(dt, global_role_scene_linear);
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

	processor = create_display_buffer_processor(view_transform, display, exposure, gamma);

	if (processor) {
		display_buffer_apply_threaded(ibuf, ibuf->rect_float, (unsigned char *) ibuf->rect,
		                              display_buffer, processor, do_display_buffer_apply_ocio_thread);
	}

	OCIO_processorRelease(processor);
}

static void colormanage_display_buffer_process(ImBuf *ibuf, unsigned char *display_buffer,
                                               const ColorManagedViewSettings *view_settings,
                                               const ColorManagedDisplaySettings *display_settings)
{
	const char *view_transform = view_settings->view_transform;

	if (!strcmp(view_transform, ACES_ODT_TONECORVE)) {
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

/*********************** Threaded color space transform routines *************************/

typedef struct ColorspaceTransformThread {
	void *processor;
	float *buffer;
	int width;
	int start_line;
	int tot_line;
	int channels;
} ColorspaceTransformThread;

typedef struct ColorspaceTransformInit {
	void *processor;
	float *buffer;
	int width;
	int height;
	int channels;
} ColorspaceTransformInitData;

static void colorspace_transform_init_handle(void *handle_v, int start_line, int tot_line, void *init_data_v)
{
	ColorspaceTransformThread *handle = (ColorspaceTransformThread *) handle_v;
	ColorspaceTransformInitData *init_data = (ColorspaceTransformInitData *) init_data_v;

	int channels = init_data->channels;
	int width = init_data->width;

	int offset = channels * start_line * width;

	memset(handle, 0, sizeof(ColorspaceTransformThread));

	handle->processor = init_data->processor;

	handle->buffer = init_data->buffer + offset;

	handle->width = width;

	handle->start_line = start_line;
	handle->tot_line = tot_line;

	handle->channels = channels;
}

static void colorspace_transform_apply_threaded(float *buffer, int width, int height, int channels,
                                                void *processor, void *(do_thread) (void *))
{
	ColorspaceTransformInitData init_data;

	init_data.processor = processor;
	init_data.buffer = buffer;
	init_data.width = width;
	init_data.height = height;
	init_data.channels = channels;

	colormanage_processor_apply_threaded(height, sizeof(ColorspaceTransformThread), &init_data,
	                                     colorspace_transform_init_handle, do_thread);
}

static void *do_color_space_transform_thread(void *handle_v)
{
	ColorspaceTransformThread *handle = (ColorspaceTransformThread *) handle_v;
	ConstProcessorRcPtr *processor = (ConstProcessorRcPtr *) handle->processor;
	PackedImageDesc *img;
	float *buffer = handle->buffer;
	int channels = handle->channels;
	int width = handle->width;
	int height = handle->tot_line;

	img = OCIO_createPackedImageDesc(buffer, width, height, channels, sizeof(float),
	                                 channels * sizeof(float), channels * sizeof(float) * width);

	OCIO_processorApply(processor, img);

	OCIO_packedImageDescRelease(img);

	return NULL;
}

static ConstProcessorRcPtr *create_colorspace_transform_processor(const char *from_colorspace,
                                                                  const char *to_colorspace)
{
	ConstConfigRcPtr *config = OCIO_getCurrentConfig();
	ConstProcessorRcPtr *processor;

	processor = OCIO_configGetProcessorWithNames(config, from_colorspace, to_colorspace);

	return processor;
}
#endif

void IMB_colormanagement_colorspace_transform(float *buffer, int width, int height, int channels,
                                              const char *from_colorspace, const char *to_colorspace)
{
#ifdef WITH_OCIO
	ConstProcessorRcPtr *processor;

	if (!strcmp(from_colorspace, "NONE")) {
		return;
	}

	if (!strcmp(from_colorspace, to_colorspace)) {
		/* if source and destination color spaces are identical, skip
		 * threading overhead and simply do nothing
		 */
		return;
	}

	processor = create_colorspace_transform_processor(from_colorspace, to_colorspace);

	if (processor) {
		colorspace_transform_apply_threaded(buffer, width, height, channels,
		                                    processor, do_color_space_transform_thread);

		OCIO_processorRelease(processor);
	}
#else
	(void) buffer;
	(void) width;
	(void) height;
	(void) channels;
	(void) from_colorspace;
	(void) to_colorspace;
#endif
}

void IMB_colormanagement_imbuf_make_scene_linear(ImBuf *ibuf, ColorManagedColorspaceSettings *colorspace_settings)
{
#ifdef WITH_OCIO
	if (ibuf->rect_float) {
		const char *from_colorspace = colorspace_settings->name;
		const char *to_colorspace = global_role_scene_linear;

		IMB_colormanagement_colorspace_transform(ibuf->rect_float, ibuf->x, ibuf->y, ibuf->channels,
		                                         from_colorspace, to_colorspace);
	}
#else
	(void) ibuf;
	(void) colorspace_settings;
#endif
}

#ifdef WITH_OCIO
static void colormanage_flags_allocate(ImBuf *ibuf)
{
	if (global_tot_display == 0)
		return;

	ibuf->display_buffer_flags = MEM_callocN(sizeof(unsigned int) * global_tot_display, "imbuf display_buffer_flags");
}
#endif

static void imbuf_verify_float(ImBuf *ibuf)
{
	/* multiple threads could request for display buffer at once and in case
	 * view transform is not used it'll lead to display buffer calculated
	 * several times
	 * it is harmless, but would take much more time (assuming thread lock
	 * happens faster than running float->byte conversion for average image)
	 */
	BLI_lock_thread(LOCK_COLORMANAGE);

	if (ibuf->rect_float && (ibuf->rect == NULL || (ibuf->userflags & IB_RECT_INVALID)))
		IMB_rect_from_float(ibuf);

	BLI_unlock_thread(LOCK_COLORMANAGE);
}

/*********************** Public display buffers interfaces *************************/

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

#if !defined(COLORMANAGE_BYTE_BUFFER)
	if (!ibuf->rect_float) {
		imbuf_verify_float(ibuf);

		return (unsigned char *) ibuf->rect;
	}
#endif

	if (!strcmp(view_transform, "NONE") ||
	    global_tot_display == 0 ||
	    global_tot_view == 0)
	{
		/* currently only view-transformation is allowed, input and display
		 * spaces are hard-coded, so if there's no view transform applying
		 * it's safe to suppose standard byte buffer is used for display
		 */

		imbuf_verify_float(ibuf);

		return (unsigned char *) ibuf->rect;
	}
	else {
		unsigned char *display_buffer;
		int buffer_size;
		ColormanageCacheViewSettings cache_view_settings;
		ColormanageCacheDisplaySettings cache_display_settings;

		colormanage_view_settings_to_cache(&cache_view_settings, view_settings);
		colormanage_display_settings_to_cache(&cache_display_settings, display_settings);

		BLI_lock_thread(LOCK_COLORMANAGE);

		/* ensure color management bit fields exists */
		if (!ibuf->display_buffer_flags)
			colormanage_flags_allocate(ibuf);

		display_buffer = colormanage_cache_get(ibuf, &cache_view_settings, &cache_display_settings, cache_handle);

		if (display_buffer) {
			BLI_unlock_thread(LOCK_COLORMANAGE);
			return display_buffer;
		}

		buffer_size = ibuf->channels * ibuf->x * ibuf->y * sizeof(float);
		display_buffer = MEM_callocN(buffer_size, "imbuf display buffer");

		colormanage_display_buffer_process(ibuf, display_buffer, view_settings, display_settings);

		colormanage_cache_put(ibuf, &cache_view_settings, &cache_display_settings, display_buffer, cache_handle);

		BLI_unlock_thread(LOCK_COLORMANAGE);

		return display_buffer;
	}
#else
	/* no OCIO support, simply return byte buffer which was
	 * generated from float buffer (if any) using standard
	 * profiles without applying any view / display transformation */

	(void) view_settings;
	(void) view_transform;
	(void) display_settings;

	imbuf_verify_float(ibuf);

	return (unsigned char*) ibuf->rect;
#endif
}

void IMB_display_buffer_to_imbuf_rect(ImBuf *ibuf, const ColorManagedViewSettings *view_settings,
                                      const ColorManagedDisplaySettings *display_settings)
{
#ifdef WITH_OCIO
	const char *view_transform = view_settings->view_transform;

#if !defined(COLORMANAGE_BYTE_BUFFER)
	if (!ibuf->rect_float)
		return;
#endif

	if (!strcmp(view_transform, "NONE") ||
	    global_tot_display == 0 ||
	    global_tot_view == 0)
	{
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

void IMB_display_buffer_release(void *cache_handle)
{
	if (cache_handle) {
		BLI_lock_thread(LOCK_COLORMANAGE);

		colormanage_cache_handle_release(cache_handle);

		BLI_unlock_thread(LOCK_COLORMANAGE);
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

static void colormanage_check_colorspace_settings(ColorManagedColorspaceSettings *colorspace_settings, const char *what)
{
	if (colorspace_settings->name[0] == '\0') {
		BLI_strncpy(colorspace_settings->name, "NONE", sizeof(colorspace_settings->name));
	}
	else if (!strcmp(colorspace_settings->name, "NONE")) {
		/* pass */
	}
	else {
		ColorSpace *colorspace = colormanage_colorspace_get_named(colorspace_settings->name);

		if (!colorspace) {
			printf("Blender color management: %s colorspace \"%s\" not found, setting NONE instead.\n",
			       what, colorspace_settings->name);

			BLI_strncpy(colorspace_settings->name, "NONE", sizeof(colorspace_settings->name));
		}
	}

	(void) what;
}
#endif

void IMB_colormanagement_check_file_config(Main *bmain)
{
#ifdef WITH_OCIO
	wmWindowManager *wm = bmain->wm.first;
	wmWindow *win;
	bScreen *sc;
	Scene *scene;
	Image *image;
	MovieClip *clip;

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

	/* ** check display device settings ** */

	if (wm) {
		for (win = wm->windows.first; win; win = win->next) {
			colormanage_check_display_settings(&win->display_settings, "window", default_display);

			colormanage_check_view_settings(&win->view_settings, "window", default_view);
		}
	}

	/* ** check view transform settings ** */
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
				else if (sl->spacetype == SPACE_SEQ) {
					SpaceSeq *sseq = (SpaceSeq *) sl;

					colormanage_check_view_settings(&sseq->view_settings, "sequencer editor", default_view);
				}
			}
		}
	}

	for (scene = bmain->scene.first; scene; scene = scene->id.next) {
		ImageFormatData *imf = 	&scene->r.im_format;

		colormanage_check_display_settings(&imf->display_settings, "scene", default_display);

		colormanage_check_view_settings(&imf->view_settings, "scene", default_view);
	}

	/* ** check input color space settings ** */

	for (image = bmain->image.first; image; image = image->id.next) {
		colormanage_check_colorspace_settings(&image->colorspace_settings, "image");
	}

	for (clip = bmain->movieclip.first; clip; clip = clip->id.next) {
		colormanage_check_colorspace_settings(&clip->colorspace_settings, "image");
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

ColorSpace *colormanage_colorspace_add(const char *name, const char *description)
{
	ColorSpace *colorspace;

	colorspace = MEM_callocN(sizeof(ColorSpace), "ColorSpace");
	colorspace->index = global_tot_colorspace + 1;

	BLI_strncpy(colorspace->name, name, sizeof(colorspace->name));

	if (description) {
		BLI_strncpy(colorspace->description, description, sizeof(colorspace->description));

		colormanage_description_strip(colorspace->description);
	}

	BLI_addtail(&global_colorspaces, colorspace);

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
	view = colormanage_view_get_named(ACES_ODT_TONECORVE);
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

void IMB_colormanagement_colorspace_items_add(EnumPropertyItem **items, int *totitem)
{
	ColorSpace *colorspace;

	for (colorspace = global_colorspaces.first; colorspace; colorspace = colorspace->next) {
		EnumPropertyItem item;

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
 * be color managed when they finished to be calculated. This gives
 * nice visual feedback without slowing things down.
 *
 * Updating happens for all display buffers generated for given
 * ImBuf at the time function is being called.
 */

#ifdef WITH_OCIO
static void partial_buffer_update_rect(unsigned char *display_buffer, const float *linear_buffer, int display_stride,
									   int linear_stride, int linear_offset_x, int linear_offset_y,
									   int channels, int dither, int predivide,
									   ConstProcessorRcPtr *processor, imb_tonecurveCb tonecurve_func,
									   int xmin, int ymin, int xmax, int ymax)
{
	int x, y;

	for (y = ymin; y <= ymax; y++) {
		for (x = xmin; x <= xmax; x++) {
			int display_index = (y * display_stride + x) * channels;
			int linear_index = ((y - linear_offset_y) * linear_stride + (x - linear_offset_x)) * channels;
			float pixel[4];

			if (processor) {
				copy_v4_v4(pixel, (float *) linear_buffer + linear_index);

				OCIO_processorApplyRGBA(processor, pixel);

				rgba_float_to_uchar(display_buffer + display_index, pixel);
			}
			else {
				IMB_buffer_byte_from_float_tonecurve(display_buffer + display_index, linear_buffer + linear_index,
				                                     channels, dither, IB_PROFILE_SRGB, IB_PROFILE_LINEAR_RGB,
				                                     predivide, 1, 1, 1, 1, tonecurve_func);
			}
		}
	}
}
#endif

void IMB_partial_display_buffer_update(ImBuf *ibuf, const float *linear_buffer,
                                       int stride, int offset_x, int offset_y,
                                       int xmin, int ymin, int xmax, int ymax)
{
#ifdef WITH_OCIO
	int display;

	int *display_buffer_flags;

	int channels = ibuf->channels;
	int predivide = ibuf->flags & IB_cm_predivide;
	int dither = ibuf->dither;

	BLI_lock_thread(LOCK_COLORMANAGE);

	if (!ibuf->display_buffer_flags) {
		/* there's no cached display buffers, so no need to iterate though bit fields */
		BLI_unlock_thread(LOCK_COLORMANAGE);

		return;
	}

	/* make a copy of flags, so other areas could calculate new display buffers
	 * and they'll be properly handled later
	 */
	display_buffer_flags = MEM_dupallocN(ibuf->display_buffer_flags);

	BLI_unlock_thread(LOCK_COLORMANAGE);

	for (display = 0; display < global_tot_display; display++) {
		ColormanageCacheDisplaySettings display_settings = {0};
		int display_index = display + 1; /* displays in configuration are 1-based */
		const char *display_name = IMB_colormanagement_display_get_indexed_name(display_index);
		int view_flags = display_buffer_flags[display];
		int view = 0;

		display_settings.display = display_index;

		while (view_flags != 0) {
			if (view_flags % 2 == 1) {
				ColormanageCacheViewSettings view_settings = {0};
				unsigned char *display_buffer;
				void *cache_handle;
				int view_index = view + 1; /* views in configuration are 1-based */
				float exposure, gamma;
				int buffer_width;

				view_settings.view = view_index;

				BLI_lock_thread(LOCK_COLORMANAGE);
				display_buffer = colormanage_cache_get_cache_data(ibuf, &view_settings, &display_settings,
				                                                  &cache_handle, &exposure, &gamma);

				/* in some rare cases buffer's dimension could be changing directly from
				 * different thread
				 * this i.e. happens when image editor acquires render result
				 */
				buffer_width = ibuf->x;
				BLI_unlock_thread(LOCK_COLORMANAGE);

				if (display_buffer) {
					const char *view_name = IMB_colormanagement_view_get_indexed_name(view_index);
					ConstProcessorRcPtr *processor = NULL;
					imb_tonecurveCb tonecurve_func = NULL;

					if (!strcmp(view_name, ACES_ODT_TONECORVE)) {
						tonecurve_func = IMB_ratio_preserving_odt_tonecurve;
					}
					else {
						processor = create_display_buffer_processor(view_name, display_name, exposure, gamma);
					}

					partial_buffer_update_rect(display_buffer, linear_buffer, buffer_width, stride,
                                               offset_x, offset_y, channels, dither, predivide,
                                               processor, tonecurve_func, xmin, ymin, xmax, ymax);

					if (processor)
						OCIO_processorRelease(processor);
				}

				IMB_display_buffer_release(cache_handle);
			}

			view_flags /= 2;
			view++;
		}
	}

	MEM_freeN(display_buffer_flags);
#else
	(void) ibuf;
	(void) linear_buffer;
	(void) xmin;
	(void) ymin;
	(void) xmax;
	(void) ymax;
#endif
}
