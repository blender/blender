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
#endif

static ListBase global_displays = {NULL};
static ListBase global_views = {NULL};

static int global_tot_display = 0;
static int global_tot_view = 0;

/*********************** Color managed cache *************************/

/* Currently it's original ImBuf pointer is used to distinguish which
 * datablock, frame number, possible postprocessing display buffer was
 * created for.
 *
 * This makes it's possible to easy define key for color managed cache
 * which would work for Images, Movie Clips, Sequencer Strips and so.
 *
 * This also allows to easily control memory usage -- all color managed
 * buffers are concentrated in single cache and it's really easy to
 * control maximal memory usage for all color management related stuff
 * (currently supports only maximal memory usage, but it could be
 * improved further to support removing buffers when they are not needed
 * anymore but memory usage didn't exceed it's limit).
 *
 * This ImBuf is being referenced by cache key, so it could accessed
 * anytime on runtime while cache element is valid. This is needed to
 * support removing display buffers from cache when ImBuf they were
 * created for is being freed.
 *
 * Technically it works in the following way:
 * - ImBuf is being referenced first time when display buffer is
 *   creating for it and being put into the cache
 * - On any further display buffer created for this ImBuf user
 *   reference counter is not being incremented
 * - There's count of color management users in ImBuf which is
 *   being incremented every time display buffer is creating for
 *   giver ImBuf.
 * - Hence, we always know how many display buffers is created
 *   for the ImBuf and if there's any display buffers created
 *   this ImBuf would be referenced by color management stuff and
 *   actual data for it wouldn't be freed even when this ImBuf is
 *   being freed by user, who created it.
 * - When all external users finished working with this ImBuf it's
 *   reference counter would be 0.
 * - On every new display buffer adding to the cache review of
 *   the cache happens and all cached display buffers who's ImBuf's
 *   user counter is zero are being removed from the cache.
 * - On every display buffer removed from the cache ImBuf's color
 *   management user counter is being decremented. As soon as it's
 *   becoming zero, original ImBuf is being freed completely.
 */

typedef struct ColormanageCacheKey {
	ImBuf *ibuf;         /* image buffer for which display buffer was created */
	int view_transform;  /* view transformation used for display buffer */
	int display;         /* display device name */
} ColormanageCacheKey;

typedef struct ColormnaageCacheImBufData {
	float exposure;  /* exposure value cached buffer is calculated with */
	float gamma;     /* gamma value cached buffer is calculated with */
} ColormnaageCacheImBufData;

static struct MovieCache *colormanage_cache = NULL;

static unsigned int colormanage_hashhash(const void *key_v)
{
	ColormanageCacheKey *key = (ColormanageCacheKey *)key_v;

	unsigned int rval = *(unsigned int *) key->ibuf;

	return rval;
}

static int colormanage_hashcmp(const void *av, const void *bv)
{
	const ColormanageCacheKey *a = (ColormanageCacheKey *) av;
	const ColormanageCacheKey *b = (ColormanageCacheKey *) bv;

	if (a->ibuf < b->ibuf)
		return -1;
	else if (a->ibuf > b->ibuf)
		return 1;

	if (a->view_transform < b->view_transform)
		return -1;
	else if (a->view_transform > b->view_transform)
		return 1;

	if (a->display < b->display)
		return -1;
	else if (a->display > b->display)
		return 1;

	return 0;
}

static int colormanage_checkkeyunused(void *key_v)
{
	ColormanageCacheKey *key = (ColormanageCacheKey *)key_v;

	return key->ibuf->refcounter == 0;
}

static void colormanage_keydeleter(void *key_v)
{
	ColormanageCacheKey *key = (ColormanageCacheKey *)key_v;
	ImBuf *cache_ibuf = key->ibuf;

	cache_ibuf->colormanage_refcounter--;

	if (cache_ibuf->colormanage_refcounter == 0) {
		IMB_freeImBuf(key->ibuf);
	}
}

static void colormanage_cache_init(void)
{
	colormanage_cache = IMB_moviecache_create(sizeof(ColormanageCacheKey), colormanage_keydeleter,
	                                          colormanage_hashhash, colormanage_hashcmp,
	                                          NULL, colormanage_checkkeyunused);
}

static void colormanage_cache_exit(void)
{
	IMB_moviecache_free(colormanage_cache);
}

#ifdef WITH_OCIO
static ImBuf *colormanage_cache_get_ibuf(ImBuf *ibuf, int view_transform, int display, void **cache_handle)
{
	ImBuf *cache_ibuf;
	ColormanageCacheKey key;

	*cache_handle = NULL;

	key.ibuf = ibuf;
	key.view_transform = view_transform;
	key.display = display;

	cache_ibuf = IMB_moviecache_get(colormanage_cache, &key);

	*cache_handle = cache_ibuf;

	return cache_ibuf;
}

static unsigned char *colormanage_cache_get(ImBuf *ibuf, int view_transform, int display,
                                            float exposure, float gamma, void **cache_handle)
{
	ImBuf *cache_ibuf = colormanage_cache_get_ibuf(ibuf, view_transform, display, cache_handle);

	if (cache_ibuf) {
		ColormnaageCacheImBufData *cache_data;

		/* only buffers with different color space conversions are being stored
		 * in cache separately. buffer which were used only different exposure/gamma
		 * are re-suing the same cached buffer
		 *
		 * check here which exposure/gamma was used for cached buffer and if they're
		 * different from requested buffer should be re-generated
		 */
		cache_data = (ColormnaageCacheImBufData *) cache_ibuf->colormanage_cache_data;
		if (cache_data->exposure != exposure || cache_data->gamma != gamma) {
			IMB_freeImBuf(cache_ibuf);

			return NULL;
		}

		return (unsigned char *) cache_ibuf->rect;
	}

	return NULL;
}

static void colormanage_cache_put(ImBuf *ibuf, int view_transform, int display, float exposure, float gamma,
                                  unsigned char *display_buffer, void **cache_handle)
{
	ColormanageCacheKey key;
	ImBuf *cache_ibuf;
	ColormnaageCacheImBufData *cache_data;

	key.ibuf = ibuf;
	key.view_transform = view_transform;
	key.display = display;

	/* buffer itself */
	cache_ibuf = IMB_allocImBuf(ibuf->x, ibuf->y, ibuf->planes, 0);
	cache_ibuf->rect = (unsigned int *) display_buffer;

	cache_ibuf->mall |= IB_rect;
	cache_ibuf->flags |= IB_rect;

	/* store data which is needed to check whether cached buffer could be used for color managed display settings */
	cache_data = MEM_callocN(sizeof(ColormnaageCacheImBufData), "color manage cache imbuf data");
	cache_data->exposure = exposure;
	cache_data->gamma = gamma;

	cache_ibuf->colormanage_cache_data = cache_data;

	*cache_handle = cache_ibuf;

	/* mark source buffer as having color managed buffer and increment color managed buffers count for it */
	if ((ibuf->colormanage_flags & IMB_COLORMANAGED) == 0) {
		ibuf->colormanage_flags |= IMB_COLORMANAGED;

		IMB_refImBuf(ibuf);
	}

	ibuf->colormanage_refcounter++;

	IMB_moviecache_put(colormanage_cache, &key, cache_ibuf);
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
static unsigned char *colormanage_cache_get_validated(ImBuf *ibuf, int view_transform, int display, void **cache_handle)
{
	ImBuf *cache_ibuf = colormanage_cache_get_ibuf(ibuf, view_transform, display, cache_handle);

	if (cache_ibuf) {
		if (cache_ibuf->x != ibuf->x || cache_ibuf->y != ibuf->y) {
			ColormnaageCacheImBufData *cache_data;
			unsigned char *display_buffer;
			int buffer_size;

			/* use the same settings as original cached buffer  */
			cache_data = (ColormnaageCacheImBufData *) cache_ibuf->colormanage_cache_data;

			buffer_size = ibuf->channels * ibuf->x * ibuf->y * sizeof(float);
			display_buffer = MEM_callocN(buffer_size, "imbuf validated display buffer");

			colormanage_cache_put(ibuf, view_transform, display, cache_data->exposure, cache_data->gamma,
			                      display_buffer, cache_handle);

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
	ColormnaageCacheImBufData *cache_data;

	cache_data = (ColormnaageCacheImBufData *) cache_ibuf->colormanage_cache_data;

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
static void colormanage_load_config(ConstConfigRcPtr* config)
{
	ConstColorSpaceRcPtr *ociocs;
	int tot_colorspace, tot_display, tot_display_view, index, viewindex, viewindex2;
	const char *name;

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

	colormanage_cache_init();
}

void IMB_colormanagement_exit(void)
{
#ifdef WITH_OCIO
	colormanage_free_config();
#endif

	colormanage_cache_exit();
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

	/* OCIO_TODO: get rid of hardcoded input and display spaces */
	OCIO_displayTransformSetInputColorSpaceName(dt, "aces");

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
                                      const char *display)
{
	ConstProcessorRcPtr *processor;
	const float gamma = view_settings->gamma;
	const float exposure = view_settings->exposure;
	const char *view_transform = view_settings->view_transform;
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
#endif

void IMB_colormanage_flags_allocate(ImBuf *ibuf)
{
	if (global_tot_display == 0)
		return;

	ibuf->display_buffer_flags = MEM_callocN(sizeof(unsigned int) * global_tot_display, "imbuf display_buffer_flags");
}

void IMB_colormanage_flags_free(ImBuf *ibuf)
{
	if (ibuf->display_buffer_flags) {
		MEM_freeN(ibuf->display_buffer_flags);

		ibuf->display_buffer_flags = NULL;
	}
}

void IMB_colormanage_cache_data_free(ImBuf *ibuf)
{
	if (ibuf->colormanage_cache_data) {
		MEM_freeN(ibuf->colormanage_cache_data);

		ibuf->colormanage_cache_data = NULL;
	}
}

unsigned char *IMB_display_buffer_acquire(ImBuf *ibuf, const ColorManagedViewSettings *view_settings,
                                          const char *display, void **cache_handle)
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
		int view_transform_index = IMB_colormanagement_view_get_named_index(view_transform);
		int display_index = IMB_colormanagement_display_get_named_index(display);
		int view_transform_flag = 1 << (view_transform_index - 1);

		float exposure = view_settings->exposure;
		float gamma = view_settings->gamma;

		/* ensure color management bit fields exists */
		if (!ibuf->display_buffer_flags)
			IMB_colormanage_flags_allocate(ibuf);

		/* check whether display buffer isn't marked as dirty and if so try to get buffer from cache */
		if (ibuf->display_buffer_flags[display_index - 1] & view_transform_flag) {
			display_buffer = colormanage_cache_get(ibuf, view_transform_index, display_index,
			                                       exposure, gamma, cache_handle);

			if (display_buffer) {
				return display_buffer;
			}
		}

		/* OCIO_TODO: in case when image is being resized it is possible
		 *            to save buffer allocation here
		 *
		 *            actually not because there might be other users of
		 *            that buffer which better not to change
		 */

		buffer_size = ibuf->channels * ibuf->x * ibuf->y * sizeof(float);
		display_buffer = MEM_callocN(buffer_size, "imbuf display buffer");

		if (!strcmp(view_transform, "ACES ODT Tonecurve")) {
			/* special case for Mango team, this does not actually apply
			 * any input space -> display space conversion and just applies
			 * a tonecurve for better linear float -> sRGB byte conversion
			 */
			display_buffer_apply_tonemap(ibuf, display_buffer, IMB_ratio_preserving_odt_tonecurve);
		}
		else {
			display_buffer_apply_ocio(ibuf, display_buffer, view_settings, display);
		}

		colormanage_cache_put(ibuf, view_transform_index, display_index, exposure, gamma,
		                      display_buffer, cache_handle);

		ibuf->display_buffer_flags[display_index - 1] |= view_transform_flag;

		return display_buffer;
	}
#else
	/* no OCIO support, simply return byte buffer which was
	 * generated from float buffer (if any) using standard
	 * profiles without applying any view / display transformation */

	(void) view_settings;
	(void) view_transform;
	(void) display;

	if (!ibuf->rect) {
		IMB_rect_from_float(ibuf);
	}

	return (unsigned char*) ibuf->rect;
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
static void colormanage_check_view_settings(ColorManagedViewSettings *view_settings, const char *editor,
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
			       editor, view_settings->view_transform, default_view->name);

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
			if (win->display_device[0] == '\0') {
				BLI_strncpy(win->display_device, default_display->name, sizeof(win->display_device));
			}
			else {
				ColorManagedDisplay *display = colormanage_display_get_named(win->display_device);

				if (!display) {
					printf("Blender color management: Window display \"%s\" not found, setting to default (\"%s\").\n",
						   win->display_device, default_display->name);

					BLI_strncpy(win->display_device, default_display->name, sizeof(win->display_device));
				}
			}

			colormanage_check_view_settings(&win->view_settings, "window", default_view);
		}
	}

	for(sc = bmain->screen.first; sc; sc= sc->id.next) {
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
		int display_index = display + 1; /* displays in configuration are 1-based */
		const char *display_name = IMB_colormanagement_display_get_indexed_name(display_index);
		int view_flags = ibuf->display_buffer_flags[display];
		int view = 0;

		while (view_flags != 0) {
			if (view_flags % 2 == 1) {
				unsigned char *display_buffer;
				void *cache_handle;
				int view_index = view + 1; /* views in configuration are 1-based */

				display_buffer = colormanage_cache_get_validated(ibuf, view_index, display_index, &cache_handle);

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
