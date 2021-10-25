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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, 2006, full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/image.c
 *  \ingroup bke
 */


#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <math.h>
#ifndef WIN32
#  include <unistd.h>
#else
#  include <io.h>
#endif

#include <time.h>

#include "MEM_guardedalloc.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_moviecache.h"

#ifdef WITH_OPENEXR
#  include "intern/openexr/openexr_multi.h"
#endif

#include "DNA_packedFile_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_camera_types.h"
#include "DNA_sequence_types.h"
#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_blenlib.h"
#include "BLI_math_vector.h"
#include "BLI_mempool.h"
#include "BLI_threads.h"
#include "BLI_timecode.h"  /* for stamp timecode format */
#include "BLI_utildefines.h"

#include "BKE_bmfont.h"
#include "BKE_colortools.h"
#include "BKE_global.h"
#include "BKE_icons.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_library_remap.h"
#include "BKE_main.h"
#include "BKE_packedFile.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_node.h"
#include "BKE_sequencer.h" /* seq_foreground_frame_get() */

#include "BLF_api.h"

#include "PIL_time.h"

#include "RE_pipeline.h"

#include "GPU_draw.h"

#include "BLI_sys_types.h" // for intptr_t support

/* for image user iteration */
#include "DNA_node_types.h"
#include "DNA_space_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

static SpinLock image_spin;

/* prototypes */
static int image_num_files(struct Image *ima);
static ImBuf *image_acquire_ibuf(Image *ima, ImageUser *iuser, void **r_lock);
static void image_update_views_format(Image *ima, ImageUser *iuser);
static void image_add_view(Image *ima, const char *viewname, const char *filepath);

/* max int, to indicate we don't store sequences in ibuf */
#define IMA_NO_INDEX    0x7FEFEFEF

/* quick lookup: supports 1 million frames, thousand passes */
#define IMA_MAKE_INDEX(frame, index)    (((frame) << 10) + (index))
#define IMA_INDEX_FRAME(index)           ((index) >> 10)
/*
#define IMA_INDEX_PASS(index)           (index & ~1023)
*/

/* ******** IMAGE CACHE ************* */

typedef struct ImageCacheKey {
	int index;
} ImageCacheKey;

static unsigned int imagecache_hashhash(const void *key_v)
{
	const ImageCacheKey *key = key_v;
	return key->index;
}

static bool imagecache_hashcmp(const void *a_v, const void *b_v)
{
	const ImageCacheKey *a = a_v;
	const ImageCacheKey *b = b_v;

	return (a->index != b->index);
}

static void imagecache_keydata(void *userkey, int *framenr, int *proxy, int *render_flags)
{
	ImageCacheKey *key = userkey;

	*framenr = IMA_INDEX_FRAME(key->index);
	*proxy = IMB_PROXY_NONE;
	*render_flags = 0;
}

static void imagecache_put(Image *image, int index, ImBuf *ibuf)
{
	ImageCacheKey key;

	if (image->cache == NULL) {
		// char cache_name[64];
		// BLI_snprintf(cache_name, sizeof(cache_name), "Image Datablock %s", image->id.name);

		image->cache = IMB_moviecache_create("Image Datablock Cache", sizeof(ImageCacheKey),
		                                     imagecache_hashhash, imagecache_hashcmp);
		IMB_moviecache_set_getdata_callback(image->cache, imagecache_keydata);
	}

	key.index = index;

	IMB_moviecache_put(image->cache, &key, ibuf);
}

static struct ImBuf *imagecache_get(Image *image, int index)
{
	if (image->cache) {
		ImageCacheKey key;
		key.index = index;
		return IMB_moviecache_get(image->cache, &key);
	}

	return NULL;
}

void BKE_images_init(void)
{
	BLI_spin_init(&image_spin);
}

void BKE_images_exit(void)
{
	BLI_spin_end(&image_spin);
}

/* ******** IMAGE PROCESSING ************* */

static void de_interlace_ng(struct ImBuf *ibuf) /* neogeo fields */
{
	struct ImBuf *tbuf1, *tbuf2;

	if (ibuf == NULL) return;
	if (ibuf->flags & IB_fields) return;
	ibuf->flags |= IB_fields;

	if (ibuf->rect) {
		/* make copies */
		tbuf1 = IMB_allocImBuf(ibuf->x, (ibuf->y >> 1), (unsigned char)32, (int)IB_rect);
		tbuf2 = IMB_allocImBuf(ibuf->x, (ibuf->y >> 1), (unsigned char)32, (int)IB_rect);

		ibuf->x *= 2;

		IMB_rectcpy(tbuf1, ibuf, 0, 0, 0, 0, ibuf->x, ibuf->y);
		IMB_rectcpy(tbuf2, ibuf, 0, 0, tbuf2->x, 0, ibuf->x, ibuf->y);

		ibuf->x /= 2;
		IMB_rectcpy(ibuf, tbuf1, 0, 0, 0, 0, tbuf1->x, tbuf1->y);
		IMB_rectcpy(ibuf, tbuf2, 0, tbuf2->y, 0, 0, tbuf2->x, tbuf2->y);

		IMB_freeImBuf(tbuf1);
		IMB_freeImBuf(tbuf2);
	}
	ibuf->y /= 2;
}

static void de_interlace_st(struct ImBuf *ibuf) /* standard fields */
{
	struct ImBuf *tbuf1, *tbuf2;

	if (ibuf == NULL) return;
	if (ibuf->flags & IB_fields) return;
	ibuf->flags |= IB_fields;

	if (ibuf->rect) {
		/* make copies */
		tbuf1 = IMB_allocImBuf(ibuf->x, (ibuf->y >> 1), (unsigned char)32, IB_rect);
		tbuf2 = IMB_allocImBuf(ibuf->x, (ibuf->y >> 1), (unsigned char)32, IB_rect);

		ibuf->x *= 2;

		IMB_rectcpy(tbuf1, ibuf, 0, 0, 0, 0, ibuf->x, ibuf->y);
		IMB_rectcpy(tbuf2, ibuf, 0, 0, tbuf2->x, 0, ibuf->x, ibuf->y);

		ibuf->x /= 2;
		IMB_rectcpy(ibuf, tbuf2, 0, 0, 0, 0, tbuf2->x, tbuf2->y);
		IMB_rectcpy(ibuf, tbuf1, 0, tbuf2->y, 0, 0, tbuf1->x, tbuf1->y);

		IMB_freeImBuf(tbuf1);
		IMB_freeImBuf(tbuf2);
	}
	ibuf->y /= 2;
}

void BKE_image_de_interlace(Image *ima, int odd)
{
	ImBuf *ibuf = BKE_image_acquire_ibuf(ima, NULL, NULL);
	if (ibuf) {
		if (odd)
			de_interlace_st(ibuf);
		else
			de_interlace_ng(ibuf);
	}
	BKE_image_release_ibuf(ima, ibuf, NULL);
}

/* ***************** ALLOC & FREE, DATA MANAGING *************** */

static void image_free_cached_frames(Image *image)
{
	if (image->cache) {
		IMB_moviecache_free(image->cache);
		image->cache = NULL;
	}
}

static void image_free_packedfiles(Image *ima)
{
	while (ima->packedfiles.last) {
		ImagePackedFile *imapf = ima->packedfiles.last;
		if (imapf->packedfile) {
			freePackedFile(imapf->packedfile);
		}
		BLI_remlink(&ima->packedfiles, imapf);
		MEM_freeN(imapf);
	}
}

void BKE_image_free_packedfiles(Image *ima)
{
	image_free_packedfiles(ima);
}

void BKE_image_free_views(Image *image)
{
	BLI_freelistN(&image->views);
}

static void image_free_anims(Image *ima)
{
	while (ima->anims.last) {
		ImageAnim *ia = ima->anims.last;
		if (ia->anim) {
			IMB_free_anim(ia->anim);
			ia->anim = NULL;
		}
		BLI_remlink(&ima->anims, ia);
		MEM_freeN(ia);
	}
}

/**
 * Simply free the image data from memory,
 * on display the image can load again (except for render buffers).
 */
void BKE_image_free_buffers_ex(Image *ima, bool do_lock)
{
	if (do_lock) {
		BLI_spin_lock(&image_spin);
	}
	image_free_cached_frames(ima);

	image_free_anims(ima);

	if (ima->rr) {
		RE_FreeRenderResult(ima->rr);
		ima->rr = NULL;
	}

	if (!G.background) {
		/* Background mode doesn't use opnegl,
		 * so we can avoid freeing GPU images and save some
		 * time by skipping mutex lock.
		 */
		GPU_free_image(ima);
	}

	ima->ok = IMA_OK;

	if (do_lock) {
		BLI_spin_unlock(&image_spin);
	}
}

void BKE_image_free_buffers(Image *ima)
{
	BKE_image_free_buffers_ex(ima, false);
}

/** Free (or release) any data used by this image (does not free the image itself). */
void BKE_image_free(Image *ima)
{
	int a;

	/* Also frees animdata. */
	BKE_image_free_buffers(ima);

	image_free_packedfiles(ima);

	for (a = 0; a < IMA_MAX_RENDER_SLOT; a++) {
		if (ima->renders[a]) {
			RE_FreeRenderResult(ima->renders[a]);
			ima->renders[a] = NULL;
		}
	}

	BKE_image_free_views(ima);
	MEM_SAFE_FREE(ima->stereo3d_format);

	BKE_icon_id_delete(&ima->id);
	BKE_previewimg_free(&ima->preview);
}

/* only image block itself */
static void image_init(Image *ima, short source, short type)
{
	BLI_assert(MEMCMP_STRUCT_OFS_IS_ZERO(ima, id));

	ima->ok = IMA_OK;

	ima->xrep = ima->yrep = 1;
	ima->aspx = ima->aspy = 1.0;
	ima->gen_x = 1024; ima->gen_y = 1024;
	ima->gen_type = IMA_GENTYPE_GRID;

	ima->source = source;
	ima->type = type;

	if (source == IMA_SRC_VIEWER)
		ima->flag |= IMA_VIEW_AS_RENDER;

	BKE_color_managed_colorspace_settings_init(&ima->colorspace_settings);
	ima->stereo3d_format = MEM_callocN(sizeof(Stereo3dFormat), "Image Stereo Format");
}

void BKE_image_init(struct Image *image)
{
	if (image) {
		image_init(image, IMA_SRC_GENERATED, IMA_TYPE_UV_TEST);
	}
}

static Image *image_alloc(Main *bmain, const char *name, short source, short type)
{
	Image *ima;

	ima = BKE_libblock_alloc(bmain, ID_IM, name);
	if (ima) {
		image_init(ima, source, type);
	}

	return ima;
}

/* Get the ibuf from an image cache by it's index and frame.
 * Local use here only.
 *
 * Returns referenced image buffer if it exists, callee is to
 * call IMB_freeImBuf to de-reference the image buffer after
 * it's done handling it.
 */
static ImBuf *image_get_cached_ibuf_for_index_frame(Image *ima, int index, int frame)
{
	if (index != IMA_NO_INDEX) {
		index = IMA_MAKE_INDEX(frame, index);
	}

	return imagecache_get(ima, index);
}

/* no ima->ibuf anymore, but listbase */
static void image_assign_ibuf(Image *ima, ImBuf *ibuf, int index, int frame)
{
	if (ibuf) {
		if (index != IMA_NO_INDEX)
			index = IMA_MAKE_INDEX(frame, index);

		imagecache_put(ima, index, ibuf);
	}
}

static void copy_image_packedfiles(ListBase *lb_dst, const ListBase *lb_src)
{
	const ImagePackedFile *imapf_src;

	BLI_listbase_clear(lb_dst);
	for (imapf_src = lb_src->first; imapf_src; imapf_src = imapf_src->next) {
		ImagePackedFile *imapf_dst = MEM_mallocN(sizeof(ImagePackedFile), "Image Packed Files (copy)");
		BLI_strncpy(imapf_dst->filepath, imapf_src->filepath, sizeof(imapf_dst->filepath));

		if (imapf_src->packedfile)
			imapf_dst->packedfile = dupPackedFile(imapf_src->packedfile);

		BLI_addtail(lb_dst, imapf_dst);
	}
}

/* empty image block, of similar type and filename */
Image *BKE_image_copy(Main *bmain, const Image *ima)
{
	Image *nima = image_alloc(bmain, ima->id.name + 2, ima->source, ima->type);

	BLI_strncpy(nima->name, ima->name, sizeof(ima->name));

	nima->flag = ima->flag;
	nima->tpageflag = ima->tpageflag;

	nima->gen_x = ima->gen_x;
	nima->gen_y = ima->gen_y;
	nima->gen_type = ima->gen_type;
	copy_v4_v4(nima->gen_color, ima->gen_color);

	nima->animspeed = ima->animspeed;

	nima->aspx = ima->aspx;
	nima->aspy = ima->aspy;

	BKE_color_managed_colorspace_settings_copy(&nima->colorspace_settings, &ima->colorspace_settings);

	copy_image_packedfiles(&nima->packedfiles, &ima->packedfiles);

	/* nima->stere3d_format is already allocated by image_alloc... */
	*nima->stereo3d_format = *ima->stereo3d_format;
	BLI_duplicatelist(&nima->views, &ima->views);

	BKE_previewimg_id_copy(&nima->id, &ima->id);

	BKE_id_copy_ensure_local(bmain, &ima->id, &nima->id);

	return nima;
}

void BKE_image_make_local(Main *bmain, Image *ima, const bool lib_local)
{
	BKE_id_make_local_generic(bmain, &ima->id, true, lib_local);
}

void BKE_image_merge(Image *dest, Image *source)
{
	/* sanity check */
	if (dest && source && dest != source) {
		BLI_spin_lock(&image_spin);
		if (source->cache != NULL) {
			struct MovieCacheIter *iter;
			iter = IMB_moviecacheIter_new(source->cache);
			while (!IMB_moviecacheIter_done(iter)) {
				ImBuf *ibuf = IMB_moviecacheIter_getImBuf(iter);
				ImageCacheKey *key = IMB_moviecacheIter_getUserKey(iter);
				imagecache_put(dest, key->index, ibuf);
				IMB_moviecacheIter_step(iter);
			}
			IMB_moviecacheIter_free(iter);
		}
		BLI_spin_unlock(&image_spin);

		BKE_libblock_free(G.main, source);
	}
}

/* note, we could be clever and scale all imbuf's but since some are mipmaps its not so simple */
bool BKE_image_scale(Image *image, int width, int height)
{
	ImBuf *ibuf;
	void *lock;

	ibuf = BKE_image_acquire_ibuf(image, NULL, &lock);

	if (ibuf) {
		IMB_scaleImBuf(ibuf, width, height);
		ibuf->userflags |= IB_BITMAPDIRTY;
	}

	BKE_image_release_ibuf(image, ibuf, lock);

	return (ibuf != NULL);
}

bool BKE_image_has_bindcode(Image *ima)
{
	bool has_bindcode = false;
	for (int i = 0; i < TEXTARGET_COUNT; i++) {
		if (ima->bindcode[i]) {
			has_bindcode = true;
			break;
		}
	}
	return has_bindcode;
}

static void image_init_color_management(Image *ima)
{
	ImBuf *ibuf;
	char name[FILE_MAX];

	BKE_image_user_file_path(NULL, ima, name);

	/* will set input color space to image format default's */
	ibuf = IMB_loadiffname(name, IB_test | IB_alphamode_detect, ima->colorspace_settings.name);

	if (ibuf) {
		if (ibuf->flags & IB_alphamode_premul)
			ima->alpha_mode = IMA_ALPHA_PREMUL;
		else
			ima->alpha_mode = IMA_ALPHA_STRAIGHT;

		IMB_freeImBuf(ibuf);
	}
}

char BKE_image_alpha_mode_from_extension_ex(const char *filepath)
{
	if (BLI_testextensie_n(filepath, ".exr", ".cin", ".dpx", ".hdr", NULL)) {
		return IMA_ALPHA_PREMUL;
	}
	else {
		return IMA_ALPHA_STRAIGHT;
	}
}

void BKE_image_alpha_mode_from_extension(Image *image)
{
	image->alpha_mode = BKE_image_alpha_mode_from_extension_ex(image->name);
}

Image *BKE_image_load(Main *bmain, const char *filepath)
{
	Image *ima;
	int file;
	char str[FILE_MAX];

	BLI_strncpy(str, filepath, sizeof(str));
	BLI_path_abs(str, bmain->name);

	/* exists? */
	file = BLI_open(str, O_BINARY | O_RDONLY, 0);
	if (file == -1)
		return NULL;
	close(file);

	ima = image_alloc(bmain, BLI_path_basename(filepath), IMA_SRC_FILE, IMA_TYPE_IMAGE);
	BLI_strncpy(ima->name, filepath, sizeof(ima->name));

	if (BLI_testextensie_array(filepath, imb_ext_movie))
		ima->source = IMA_SRC_MOVIE;

	image_init_color_management(ima);

	return ima;
}

/* checks if image was already loaded, then returns same image */
/* otherwise creates new. */
/* does not load ibuf itself */
/* pass on optional frame for #name images */
Image *BKE_image_load_exists_ex(const char *filepath, bool *r_exists)
{
	Image *ima;
	char str[FILE_MAX], strtest[FILE_MAX];

	BLI_strncpy(str, filepath, sizeof(str));
	BLI_path_abs(str, G.main->name);

	/* first search an identical filepath */
	for (ima = G.main->image.first; ima; ima = ima->id.next) {
		if (ima->source != IMA_SRC_VIEWER && ima->source != IMA_SRC_GENERATED) {
			BLI_strncpy(strtest, ima->name, sizeof(ima->name));
			BLI_path_abs(strtest, ID_BLEND_PATH(G.main, &ima->id));

			if (BLI_path_cmp(strtest, str) == 0) {
				if ((BKE_image_has_anim(ima) == false) ||
				    (ima->id.us == 0))
				{
					id_us_plus(&ima->id);  /* officially should not, it doesn't link here! */
					if (ima->ok == 0)
						ima->ok = IMA_OK;
					if (r_exists)
						*r_exists = true;
					return ima;
				}
			}
		}
	}

	if (r_exists)
		*r_exists = false;
	return BKE_image_load(G.main, filepath);
}

Image *BKE_image_load_exists(const char *filepath)
{
	return BKE_image_load_exists_ex(filepath, NULL);
}

static ImBuf *add_ibuf_size(unsigned int width, unsigned int height, const char *name, int depth, int floatbuf, short gen_type,
                            const float color[4], ColorManagedColorspaceSettings *colorspace_settings)
{
	ImBuf *ibuf;
	unsigned char *rect = NULL;
	float *rect_float = NULL;

	if (floatbuf) {
		ibuf = IMB_allocImBuf(width, height, depth, IB_rectfloat);

		if (colorspace_settings->name[0] == '\0') {
			const char *colorspace = IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_DEFAULT_FLOAT);

			BLI_strncpy(colorspace_settings->name, colorspace, sizeof(colorspace_settings->name));
		}

		if (ibuf != NULL) {
			rect_float = ibuf->rect_float;
			IMB_colormanagement_check_is_data(ibuf, colorspace_settings->name);
		}
	}
	else {
		ibuf = IMB_allocImBuf(width, height, depth, IB_rect);

		if (colorspace_settings->name[0] == '\0') {
			const char *colorspace = IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_DEFAULT_BYTE);

			BLI_strncpy(colorspace_settings->name, colorspace, sizeof(colorspace_settings->name));
		}

		if (ibuf != NULL) {
			rect = (unsigned char *)ibuf->rect;
			IMB_colormanagement_assign_rect_colorspace(ibuf, colorspace_settings->name);
		}
	}

	if (!ibuf) {
		return NULL;
	}

	BLI_strncpy(ibuf->name, name, sizeof(ibuf->name));
	ibuf->userflags |= IB_BITMAPDIRTY;

	switch (gen_type) {
		case IMA_GENTYPE_GRID:
			BKE_image_buf_fill_checker(rect, rect_float, width, height);
			break;
		case IMA_GENTYPE_GRID_COLOR:
			BKE_image_buf_fill_checker_color(rect, rect_float, width, height);
			break;
		default:
			BKE_image_buf_fill_color(rect, rect_float, width, height, color);
			break;
	}

	return ibuf;
}

/* adds new image block, creates ImBuf and initializes color */
Image *BKE_image_add_generated(
        Main *bmain, unsigned int width, unsigned int height, const char *name,
        int depth, int floatbuf, short gen_type, const float color[4], const bool stereo3d)
{
	/* on save, type is changed to FILE in editsima.c */
	Image *ima = image_alloc(bmain, name, IMA_SRC_GENERATED, IMA_TYPE_UV_TEST);

	if (ima) {
		int view_id;
		const char *names[2] = {STEREO_LEFT_NAME, STEREO_RIGHT_NAME};

		/* BLI_strncpy(ima->name, name, FILE_MAX); */ /* don't do this, this writes in ain invalid filepath! */
		ima->gen_x = width;
		ima->gen_y = height;
		ima->gen_type = gen_type;
		ima->gen_flag |= (floatbuf ? IMA_GEN_FLOAT : 0);
		ima->gen_depth = depth;
		copy_v4_v4(ima->gen_color, color);

		for (view_id = 0; view_id < 2; view_id++) {
			ImBuf *ibuf;
			ibuf = add_ibuf_size(width, height, ima->name, depth, floatbuf, gen_type, color, &ima->colorspace_settings);
			image_assign_ibuf(ima, ibuf, stereo3d ? view_id : IMA_NO_INDEX, 0);

			/* image_assign_ibuf puts buffer to the cache, which increments user counter. */
			IMB_freeImBuf(ibuf);
			if (!stereo3d) break;

			image_add_view(ima, names[view_id], "");
		}

		ima->ok = IMA_OK_LOADED;
	}

	return ima;
}

/* Create an image image from ibuf. The refcount of ibuf is increased,
 * caller should take care to drop its reference by calling
 * IMB_freeImBuf if needed. */
Image *BKE_image_add_from_imbuf(ImBuf *ibuf, const char *name)
{
	/* on save, type is changed to FILE in editsima.c */
	Image *ima;

	if (name == NULL) {
		name = BLI_path_basename(ibuf->name);
	}

	ima = image_alloc(G.main, name, IMA_SRC_FILE, IMA_TYPE_IMAGE);

	if (ima) {
		BLI_strncpy(ima->name, ibuf->name, FILE_MAX);
		image_assign_ibuf(ima, ibuf, IMA_NO_INDEX, 0);
		ima->ok = IMA_OK_LOADED;
	}

	return ima;
}

/* packs rects from memory as PNG
 * convert multiview images to R_IMF_VIEWS_INDIVIDUAL
 */
static void image_memorypack_multiview(Image *ima)
{
	ImageView *iv;
	int i;

	image_free_packedfiles(ima);

	for (i = 0, iv = ima->views.first; iv; iv = iv->next, i++) {
		ImBuf *ibuf = image_get_cached_ibuf_for_index_frame(ima, i, 0);

		ibuf->ftype = IMB_FTYPE_PNG;
		ibuf->planes = R_IMF_PLANES_RGBA;

		/* if the image was a R_IMF_VIEWS_STEREO_3D we force _L, _R suffices */
		if (ima->views_format == R_IMF_VIEWS_STEREO_3D) {
			const char *suffix[2] = {STEREO_LEFT_SUFFIX, STEREO_RIGHT_SUFFIX};
			BLI_path_suffix(iv->filepath, FILE_MAX, suffix[i], "");
		}

		IMB_saveiff(ibuf, iv->filepath, IB_rect | IB_mem);

		if (ibuf->encodedbuffer == NULL) {
			printf("memory save for pack error\n");
			IMB_freeImBuf(ibuf);
			image_free_packedfiles(ima);
			return;
		}
		else {
			ImagePackedFile *imapf;
			PackedFile *pf = MEM_callocN(sizeof(*pf), "PackedFile");

			pf->data = ibuf->encodedbuffer;
			pf->size = ibuf->encodedsize;

			imapf = MEM_mallocN(sizeof(ImagePackedFile), "Image PackedFile");
			BLI_strncpy(imapf->filepath, iv->filepath, sizeof(imapf->filepath));
			imapf->packedfile = pf;
			BLI_addtail(&ima->packedfiles, imapf);

			ibuf->encodedbuffer = NULL;
			ibuf->encodedsize = 0;
			ibuf->userflags &= ~IB_BITMAPDIRTY;
		}
		IMB_freeImBuf(ibuf);
	}

	if (ima->source == IMA_SRC_GENERATED) {
		ima->source = IMA_SRC_FILE;
		ima->type = IMA_TYPE_IMAGE;
	}
	ima->views_format = R_IMF_VIEWS_INDIVIDUAL;
}

/* packs rect from memory as PNG */
void BKE_image_memorypack(Image *ima)
{
	ImBuf *ibuf;

	if (BKE_image_is_multiview(ima)) {
		image_memorypack_multiview(ima);
		return;
	}

	ibuf = image_get_cached_ibuf_for_index_frame(ima, IMA_NO_INDEX, 0);

	if (ibuf == NULL)
		return;

	image_free_packedfiles(ima);

	ibuf->ftype = IMB_FTYPE_PNG;
	ibuf->planes = R_IMF_PLANES_RGBA;

	IMB_saveiff(ibuf, ibuf->name, IB_rect | IB_mem);
	if (ibuf->encodedbuffer == NULL) {
		printf("memory save for pack error\n");
	}
	else {
		ImagePackedFile *imapf;
		PackedFile *pf = MEM_callocN(sizeof(*pf), "PackedFile");

		pf->data = ibuf->encodedbuffer;
		pf->size = ibuf->encodedsize;

		imapf = MEM_mallocN(sizeof(ImagePackedFile), "Image PackedFile");
		BLI_strncpy(imapf->filepath, ima->name, sizeof(imapf->filepath));
		imapf->packedfile = pf;
		BLI_addtail(&ima->packedfiles, imapf);

		ibuf->encodedbuffer = NULL;
		ibuf->encodedsize = 0;
		ibuf->userflags &= ~IB_BITMAPDIRTY;

		if (ima->source == IMA_SRC_GENERATED) {
			ima->source = IMA_SRC_FILE;
			ima->type = IMA_TYPE_IMAGE;
		}
	}

	IMB_freeImBuf(ibuf);
}

void BKE_image_packfiles(ReportList *reports, Image *ima, const char *basepath)
{
	const int totfiles = image_num_files(ima);

	if (totfiles == 1) {
		ImagePackedFile *imapf = MEM_mallocN(sizeof(ImagePackedFile), "Image packed file");
		BLI_addtail(&ima->packedfiles, imapf);
		imapf->packedfile = newPackedFile(reports, ima->name, basepath);
		if (imapf->packedfile) {
			BLI_strncpy(imapf->filepath, ima->name, sizeof(imapf->filepath));
		}
		else {
			BLI_freelinkN(&ima->packedfiles, imapf);
		}
	}
	else {
		ImageView *iv;
		for (iv = ima->views.first; iv; iv = iv->next) {
			ImagePackedFile *imapf = MEM_mallocN(sizeof(ImagePackedFile), "Image packed file");
			BLI_addtail(&ima->packedfiles, imapf);

			imapf->packedfile = newPackedFile(reports, iv->filepath, basepath);
			if (imapf->packedfile) {
				BLI_strncpy(imapf->filepath, iv->filepath, sizeof(imapf->filepath));
			}
			else {
				BLI_freelinkN(&ima->packedfiles, imapf);
			}
		}
	}
}

void BKE_image_packfiles_from_mem(ReportList *reports, Image *ima, char *data, const size_t data_len)
{
	const int totfiles = image_num_files(ima);

	if (totfiles != 1) {
		BKE_report(reports, RPT_ERROR, "Cannot pack multiview images from raw data currently...");
	}
	else {
		ImagePackedFile *imapf = MEM_mallocN(sizeof(ImagePackedFile), __func__);
		BLI_addtail(&ima->packedfiles, imapf);
		imapf->packedfile = newPackedFileMemory(data, data_len);
		BLI_strncpy(imapf->filepath, ima->name, sizeof(imapf->filepath));
	}
}

void BKE_image_tag_time(Image *ima)
{
	ima->lastused = PIL_check_seconds_timer_i();
}

#if 0
static void tag_all_images_time()
{
	Image *ima;
	int ctime = PIL_check_seconds_timer_i();

	ima = G.main->image.first;
	while (ima) {
		if (ima->bindcode || ima->repbind || ima->ibufs.first) {
			ima->lastused = ctime;
		}
	}
}
#endif

static uintptr_t image_mem_size(Image *image)
{
	uintptr_t size = 0;

	/* viewers have memory depending on other rules, has no valid rect pointer */
	if (image->source == IMA_SRC_VIEWER)
		return 0;

	BLI_spin_lock(&image_spin);
	if (image->cache != NULL) {
		struct MovieCacheIter *iter = IMB_moviecacheIter_new(image->cache);

		while (!IMB_moviecacheIter_done(iter)) {
			ImBuf *ibuf = IMB_moviecacheIter_getImBuf(iter);
			ImBuf *ibufm;
			int level;

			if (ibuf->rect) {
				size += MEM_allocN_len(ibuf->rect);
			}
			if (ibuf->rect_float) {
				size += MEM_allocN_len(ibuf->rect_float);
			}

			for (level = 0; level < IMB_MIPMAP_LEVELS; level++) {
				ibufm = ibuf->mipmap[level];
				if (ibufm) {
					if (ibufm->rect) {
						size += MEM_allocN_len(ibufm->rect);
					}
					if (ibufm->rect_float) {
						size += MEM_allocN_len(ibufm->rect_float);
					}
				}
			}

			IMB_moviecacheIter_step(iter);
		}
		IMB_moviecacheIter_free(iter);
	}
	BLI_spin_unlock(&image_spin);

	return size;
}

void BKE_image_print_memlist(void)
{
	Image *ima;
	uintptr_t size, totsize = 0;

	for (ima = G.main->image.first; ima; ima = ima->id.next)
		totsize += image_mem_size(ima);

	printf("\ntotal image memory len: %.3f MB\n", (double)totsize / (double)(1024 * 1024));

	for (ima = G.main->image.first; ima; ima = ima->id.next) {
		size = image_mem_size(ima);

		if (size)
			printf("%s len: %.3f MB\n", ima->id.name + 2, (double)size / (double)(1024 * 1024));
	}
}

static bool imagecache_check_dirty(ImBuf *ibuf, void *UNUSED(userkey), void *UNUSED(userdata))
{
	return (ibuf->userflags & IB_BITMAPDIRTY) == 0;
}

void BKE_image_free_all_textures(void)
{
#undef CHECK_FREED_SIZE

	Tex *tex;
	Image *ima;
#ifdef CHECK_FREED_SIZE
	uintptr_t tot_freed_size = 0;
#endif

	for (ima = G.main->image.first; ima; ima = ima->id.next)
		ima->id.tag &= ~LIB_TAG_DOIT;

	for (tex = G.main->tex.first; tex; tex = tex->id.next)
		if (tex->ima)
			tex->ima->id.tag |= LIB_TAG_DOIT;

	for (ima = G.main->image.first; ima; ima = ima->id.next) {
		if (ima->cache && (ima->id.tag & LIB_TAG_DOIT)) {
#ifdef CHECK_FREED_SIZE
			uintptr_t old_size = image_mem_size(ima);
#endif

			IMB_moviecache_cleanup(ima->cache, imagecache_check_dirty, NULL);

#ifdef CHECK_FREED_SIZE
			tot_freed_size += old_size - image_mem_size(ima);
#endif
		}
	}
#ifdef CHECK_FREED_SIZE
	printf("%s: freed total %lu MB\n", __func__, tot_freed_size / (1024 * 1024));
#endif
}

static bool imagecache_check_free_anim(ImBuf *ibuf, void *UNUSED(userkey), void *userdata)
{
	int except_frame = *(int *)userdata;
	return (ibuf->userflags & IB_BITMAPDIRTY) == 0 &&
	       (ibuf->index != IMA_NO_INDEX) &&
	       (except_frame != IMA_INDEX_FRAME(ibuf->index));
}

/* except_frame is weak, only works for seqs without offset... */
void BKE_image_free_anim_ibufs(Image *ima, int except_frame)
{
	BLI_spin_lock(&image_spin);
	if (ima->cache != NULL) {
		IMB_moviecache_cleanup(ima->cache, imagecache_check_free_anim, &except_frame);
	}
	BLI_spin_unlock(&image_spin);
}

void BKE_image_all_free_anim_ibufs(int cfra)
{
	Image *ima;

	for (ima = G.main->image.first; ima; ima = ima->id.next)
		if (BKE_image_is_animated(ima))
			BKE_image_free_anim_ibufs(ima, cfra);
}


/* *********** READ AND WRITE ************** */

int BKE_image_imtype_to_ftype(const char imtype, ImbFormatOptions *r_options)
{
	memset(r_options, 0, sizeof(*r_options));

	if (imtype == R_IMF_IMTYPE_TARGA) {
		return IMB_FTYPE_TGA;
	}
	else if (imtype == R_IMF_IMTYPE_RAWTGA) {
		r_options->flag = RAWTGA;
		return IMB_FTYPE_TGA;
	}
	else if (imtype == R_IMF_IMTYPE_IRIS) {
		return IMB_FTYPE_IMAGIC;
	}
#ifdef WITH_HDR
	else if (imtype == R_IMF_IMTYPE_RADHDR) {
		return IMB_FTYPE_RADHDR;
	}
#endif
	else if (imtype == R_IMF_IMTYPE_PNG) {
		r_options->quality = 15;
		return IMB_FTYPE_PNG;
	}
#ifdef WITH_DDS
	else if (imtype == R_IMF_IMTYPE_DDS) {
		return IMB_FTYPE_DDS;
	}
#endif
	else if (imtype == R_IMF_IMTYPE_BMP) {
		return IMB_FTYPE_BMP;
	}
#ifdef WITH_TIFF
	else if (imtype == R_IMF_IMTYPE_TIFF) {
		return IMB_FTYPE_TIF;
	}
#endif
	else if (imtype == R_IMF_IMTYPE_OPENEXR || imtype == R_IMF_IMTYPE_MULTILAYER) {
		return IMB_FTYPE_OPENEXR;
	}
#ifdef WITH_CINEON
	else if (imtype == R_IMF_IMTYPE_CINEON) {
		return IMB_FTYPE_CINEON;
	}
	else if (imtype == R_IMF_IMTYPE_DPX) {
		return IMB_FTYPE_DPX;
	}
#endif
#ifdef WITH_OPENJPEG
	else if (imtype == R_IMF_IMTYPE_JP2) {
		r_options->flag |= JP2_JP2;
		r_options->quality = 90;
		return IMB_FTYPE_JP2;
	}
#endif
	else {
		r_options->quality = 90;
		return IMB_FTYPE_JPG;
	}
}

char BKE_image_ftype_to_imtype(const int ftype, const ImbFormatOptions *options)
{
	if (ftype == 0) {
		return R_IMF_IMTYPE_TARGA;
	}
	else if (ftype == IMB_FTYPE_IMAGIC) {
		return R_IMF_IMTYPE_IRIS;
	}
#ifdef WITH_HDR
	else if (ftype == IMB_FTYPE_RADHDR) {
		return R_IMF_IMTYPE_RADHDR;
	}
#endif
	else if (ftype == IMB_FTYPE_PNG) {
		return R_IMF_IMTYPE_PNG;
	}
#ifdef WITH_DDS
	else if (ftype == IMB_FTYPE_DDS) {
		return R_IMF_IMTYPE_DDS;
	}
#endif
	else if (ftype == IMB_FTYPE_BMP) {
		return R_IMF_IMTYPE_BMP;
	}
#ifdef WITH_TIFF
	else if (ftype == IMB_FTYPE_TIF) {
		return R_IMF_IMTYPE_TIFF;
	}
#endif
	else if (ftype == IMB_FTYPE_OPENEXR) {
		return R_IMF_IMTYPE_OPENEXR;
	}
#ifdef WITH_CINEON
	else if (ftype == IMB_FTYPE_CINEON) {
		return R_IMF_IMTYPE_CINEON;
	}
	else if (ftype == IMB_FTYPE_DPX) {
		return R_IMF_IMTYPE_DPX;
	}
#endif
	else if (ftype == IMB_FTYPE_TGA) {
		if (options && (options->flag & RAWTGA)) {
			return R_IMF_IMTYPE_RAWTGA;
		}
		else {
			return R_IMF_IMTYPE_TARGA;
		}
	}
#ifdef WITH_OPENJPEG
	else if (ftype == IMB_FTYPE_JP2) {
		return R_IMF_IMTYPE_JP2;
	}
#endif
	else {
		return R_IMF_IMTYPE_JPEG90;
	}
}


bool BKE_imtype_is_movie(const char imtype)
{
	switch (imtype) {
		case R_IMF_IMTYPE_AVIRAW:
		case R_IMF_IMTYPE_AVIJPEG:
		case R_IMF_IMTYPE_QUICKTIME:
		case R_IMF_IMTYPE_FFMPEG:
		case R_IMF_IMTYPE_H264:
		case R_IMF_IMTYPE_THEORA:
		case R_IMF_IMTYPE_XVID:
		case R_IMF_IMTYPE_FRAMESERVER:
			return true;
	}
	return false;
}

int BKE_imtype_supports_zbuf(const char imtype)
{
	switch (imtype) {
		case R_IMF_IMTYPE_IRIZ:
		case R_IMF_IMTYPE_OPENEXR: /* but not R_IMF_IMTYPE_MULTILAYER */
			return 1;
	}
	return 0;
}

int BKE_imtype_supports_compress(const char imtype)
{
	switch (imtype) {
		case R_IMF_IMTYPE_PNG:
			return 1;
	}
	return 0;
}

int BKE_imtype_supports_quality(const char imtype)
{
	switch (imtype) {
		case R_IMF_IMTYPE_JPEG90:
		case R_IMF_IMTYPE_JP2:
		case R_IMF_IMTYPE_AVIJPEG:
			return 1;
	}
	return 0;
}

int BKE_imtype_requires_linear_float(const char imtype)
{
	switch (imtype) {
		case R_IMF_IMTYPE_CINEON:
		case R_IMF_IMTYPE_DPX:
		case R_IMF_IMTYPE_RADHDR:
		case R_IMF_IMTYPE_OPENEXR:
		case R_IMF_IMTYPE_MULTILAYER:
			return true;
	}
	return 0;
}

char BKE_imtype_valid_channels(const char imtype, bool write_file)
{
	char chan_flag = IMA_CHAN_FLAG_RGB; /* assume all support rgb */

	/* alpha */
	switch (imtype) {
		case R_IMF_IMTYPE_BMP:
			if (write_file) break;
			ATTR_FALLTHROUGH;
		case R_IMF_IMTYPE_TARGA:
		case R_IMF_IMTYPE_RAWTGA:
		case R_IMF_IMTYPE_IRIS:
		case R_IMF_IMTYPE_PNG:
		case R_IMF_IMTYPE_TIFF:
		case R_IMF_IMTYPE_OPENEXR:
		case R_IMF_IMTYPE_MULTILAYER:
		case R_IMF_IMTYPE_DDS:
		case R_IMF_IMTYPE_JP2:
		case R_IMF_IMTYPE_QUICKTIME:
		case R_IMF_IMTYPE_DPX:
			chan_flag |= IMA_CHAN_FLAG_ALPHA;
			break;
	}

	/* bw */
	switch (imtype) {
		case R_IMF_IMTYPE_PNG:
		case R_IMF_IMTYPE_JPEG90:
		case R_IMF_IMTYPE_TARGA:
		case R_IMF_IMTYPE_RAWTGA:
		case R_IMF_IMTYPE_TIFF:
		case R_IMF_IMTYPE_IRIS:
			chan_flag |= IMA_CHAN_FLAG_BW;
			break;
	}

	return chan_flag;
}

char BKE_imtype_valid_depths(const char imtype)
{
	switch (imtype) {
		case R_IMF_IMTYPE_RADHDR:
			return R_IMF_CHAN_DEPTH_32;
		case R_IMF_IMTYPE_TIFF:
			return R_IMF_CHAN_DEPTH_8 | R_IMF_CHAN_DEPTH_16;
		case R_IMF_IMTYPE_OPENEXR:
			return R_IMF_CHAN_DEPTH_16 | R_IMF_CHAN_DEPTH_32;
		case R_IMF_IMTYPE_MULTILAYER:
			return R_IMF_CHAN_DEPTH_16 | R_IMF_CHAN_DEPTH_32;
		/* eeh, cineon does some strange 10bits per channel */
		case R_IMF_IMTYPE_DPX:
			return R_IMF_CHAN_DEPTH_8 | R_IMF_CHAN_DEPTH_10 | R_IMF_CHAN_DEPTH_12 | R_IMF_CHAN_DEPTH_16;
		case R_IMF_IMTYPE_CINEON:
			return R_IMF_CHAN_DEPTH_10;
		case R_IMF_IMTYPE_JP2:
			return R_IMF_CHAN_DEPTH_8 | R_IMF_CHAN_DEPTH_12 | R_IMF_CHAN_DEPTH_16;
		case R_IMF_IMTYPE_PNG:
			return R_IMF_CHAN_DEPTH_8 | R_IMF_CHAN_DEPTH_16;
		/* most formats are 8bit only */
		default:
			return R_IMF_CHAN_DEPTH_8;
	}
}


/* string is from command line --render-format arg, keep in sync with
 * creator_args.c help info */
char BKE_imtype_from_arg(const char *imtype_arg)
{
	if      (STREQ(imtype_arg, "TGA")) return R_IMF_IMTYPE_TARGA;
	else if (STREQ(imtype_arg, "IRIS")) return R_IMF_IMTYPE_IRIS;
#ifdef WITH_DDS
	else if (STREQ(imtype_arg, "DDS")) return R_IMF_IMTYPE_DDS;
#endif
	else if (STREQ(imtype_arg, "JPEG")) return R_IMF_IMTYPE_JPEG90;
	else if (STREQ(imtype_arg, "IRIZ")) return R_IMF_IMTYPE_IRIZ;
	else if (STREQ(imtype_arg, "RAWTGA")) return R_IMF_IMTYPE_RAWTGA;
	else if (STREQ(imtype_arg, "AVIRAW")) return R_IMF_IMTYPE_AVIRAW;
	else if (STREQ(imtype_arg, "AVIJPEG")) return R_IMF_IMTYPE_AVIJPEG;
	else if (STREQ(imtype_arg, "PNG")) return R_IMF_IMTYPE_PNG;
	else if (STREQ(imtype_arg, "QUICKTIME")) return R_IMF_IMTYPE_QUICKTIME;
	else if (STREQ(imtype_arg, "BMP")) return R_IMF_IMTYPE_BMP;
#ifdef WITH_HDR
	else if (STREQ(imtype_arg, "HDR")) return R_IMF_IMTYPE_RADHDR;
#endif
#ifdef WITH_TIFF
	else if (STREQ(imtype_arg, "TIFF")) return R_IMF_IMTYPE_TIFF;
#endif
#ifdef WITH_OPENEXR
	else if (STREQ(imtype_arg, "EXR")) return R_IMF_IMTYPE_OPENEXR;
	else if (STREQ(imtype_arg, "MULTILAYER")) return R_IMF_IMTYPE_MULTILAYER;
#endif
	else if (STREQ(imtype_arg, "FFMPEG")) return R_IMF_IMTYPE_FFMPEG;
	else if (STREQ(imtype_arg, "FRAMESERVER")) return R_IMF_IMTYPE_FRAMESERVER;
#ifdef WITH_CINEON
	else if (STREQ(imtype_arg, "CINEON")) return R_IMF_IMTYPE_CINEON;
	else if (STREQ(imtype_arg, "DPX")) return R_IMF_IMTYPE_DPX;
#endif
#ifdef WITH_OPENJPEG
	else if (STREQ(imtype_arg, "JP2")) return R_IMF_IMTYPE_JP2;
#endif
	else return R_IMF_IMTYPE_INVALID;
}

static bool do_add_image_extension(char *string, const char imtype, const ImageFormatData *im_format)
{
	const char *extension = NULL;
	const char *extension_test;
	(void)im_format;  /* may be unused, depends on build options */

	if (imtype == R_IMF_IMTYPE_IRIS) {
		if (!BLI_testextensie(string, extension_test = ".rgb"))
			extension = extension_test;
	}
	else if (imtype == R_IMF_IMTYPE_IRIZ) {
		if (!BLI_testextensie(string, extension_test = ".rgb"))
			extension = extension_test;
	}
#ifdef WITH_HDR
	else if (imtype == R_IMF_IMTYPE_RADHDR) {
		if (!BLI_testextensie(string, extension_test = ".hdr"))
			extension = extension_test;
	}
#endif
	else if (ELEM(imtype, R_IMF_IMTYPE_PNG, R_IMF_IMTYPE_FFMPEG, R_IMF_IMTYPE_H264, R_IMF_IMTYPE_THEORA, R_IMF_IMTYPE_XVID)) {
		if (!BLI_testextensie(string, extension_test = ".png"))
			extension = extension_test;
	}
#ifdef WITH_DDS
	else if (imtype == R_IMF_IMTYPE_DDS) {
		if (!BLI_testextensie(string, extension_test = ".dds"))
			extension = extension_test;
	}
#endif
	else if (ELEM(imtype, R_IMF_IMTYPE_TARGA, R_IMF_IMTYPE_RAWTGA)) {
		if (!BLI_testextensie(string, extension_test = ".tga"))
			extension = extension_test;
	}
	else if (imtype == R_IMF_IMTYPE_BMP) {
		if (!BLI_testextensie(string, extension_test = ".bmp"))
			extension = extension_test;
	}
#ifdef WITH_TIFF
	else if (imtype == R_IMF_IMTYPE_TIFF) {
		if (!BLI_testextensie_n(string, extension_test = ".tif", ".tiff", NULL)) {
			extension = extension_test;
		}
	}
#endif
#ifdef WITH_OPENIMAGEIO
	else if (imtype == R_IMF_IMTYPE_PSD) {
		if (!BLI_testextensie(string, extension_test = ".psd"))
			extension = extension_test;
	}
#endif
#ifdef WITH_OPENEXR
	else if (imtype == R_IMF_IMTYPE_OPENEXR || imtype == R_IMF_IMTYPE_MULTILAYER) {
		if (!BLI_testextensie(string, extension_test = ".exr"))
			extension = extension_test;
	}
#endif
#ifdef WITH_CINEON
	else if (imtype == R_IMF_IMTYPE_CINEON) {
		if (!BLI_testextensie(string, extension_test = ".cin"))
			extension = extension_test;
	}
	else if (imtype == R_IMF_IMTYPE_DPX) {
		if (!BLI_testextensie(string, extension_test = ".dpx"))
			extension = extension_test;
	}
#endif
#ifdef WITH_OPENJPEG
	else if (imtype == R_IMF_IMTYPE_JP2) {
		if (im_format) {
			if (im_format->jp2_codec == R_IMF_JP2_CODEC_JP2) {
				if (!BLI_testextensie(string, extension_test = ".jp2"))
					extension = extension_test;
			}
			else if (im_format->jp2_codec == R_IMF_JP2_CODEC_J2K) {
				if (!BLI_testextensie(string, extension_test = ".j2c"))
					extension = extension_test;
			}
			else
				BLI_assert(!"Unsupported jp2 codec was specified in im_format->jp2_codec");
		}
		else {
			if (!BLI_testextensie(string, extension_test = ".jp2"))
				extension = extension_test;
		}
	}
#endif
	else { //   R_IMF_IMTYPE_AVIRAW, R_IMF_IMTYPE_AVIJPEG, R_IMF_IMTYPE_JPEG90, R_IMF_IMTYPE_QUICKTIME etc
		if (!(BLI_testextensie_n(string, extension_test = ".jpg", ".jpeg", NULL)))
			extension = extension_test;
	}

	if (extension) {
		/* prefer this in many cases to avoid .png.tga, but in certain cases it breaks */
		/* remove any other known image extension */
		if (BLI_testextensie_array(string, imb_ext_image) ||
		    (G.have_quicktime && BLI_testextensie_array(string, imb_ext_image_qt)))
		{
			return BLI_replace_extension(string, FILE_MAX, extension);
		}
		else {
			return BLI_ensure_extension(string, FILE_MAX, extension);
		}

	}
	else {
		return false;
	}
}

int BKE_image_path_ensure_ext_from_imformat(char *string, const ImageFormatData *im_format)
{
	return do_add_image_extension(string, im_format->imtype, im_format);
}

int BKE_image_path_ensure_ext_from_imtype(char *string, const char imtype)
{
	return do_add_image_extension(string, imtype, NULL);
}

void BKE_imformat_defaults(ImageFormatData *im_format)
{
	memset(im_format, 0, sizeof(*im_format));
	im_format->planes = R_IMF_PLANES_RGBA;
	im_format->imtype = R_IMF_IMTYPE_PNG;
	im_format->depth = R_IMF_CHAN_DEPTH_8;
	im_format->quality = 90;
	im_format->compress = 15;

	BKE_color_managed_display_settings_init(&im_format->display_settings);
	BKE_color_managed_view_settings_init(&im_format->view_settings);
}

void BKE_imbuf_to_image_format(struct ImageFormatData *im_format, const ImBuf *imbuf)
{
	int ftype        = imbuf->ftype;
	int custom_flags = imbuf->foptions.flag;
	char quality     = imbuf->foptions.quality;

	BKE_imformat_defaults(im_format);

	/* file type */

	if (ftype == IMB_FTYPE_IMAGIC)
		im_format->imtype = R_IMF_IMTYPE_IRIS;

#ifdef WITH_HDR
	else if (ftype == IMB_FTYPE_RADHDR)
		im_format->imtype = R_IMF_IMTYPE_RADHDR;
#endif

	else if (ftype == IMB_FTYPE_PNG) {
		im_format->imtype = R_IMF_IMTYPE_PNG;

		if (custom_flags & PNG_16BIT)
			im_format->depth = R_IMF_CHAN_DEPTH_16;

		im_format->compress = quality;
	}

#ifdef WITH_DDS
	else if (ftype == IMB_FTYPE_DDS)
		im_format->imtype = R_IMF_IMTYPE_DDS;
#endif

	else if (ftype == IMB_FTYPE_BMP)
		im_format->imtype = R_IMF_IMTYPE_BMP;

#ifdef WITH_TIFF
	else if (ftype == IMB_FTYPE_TIF) {
		im_format->imtype = R_IMF_IMTYPE_TIFF;
		if (custom_flags & TIF_16BIT)
			im_format->depth = R_IMF_CHAN_DEPTH_16;
		if (custom_flags & TIF_COMPRESS_NONE)
			im_format->tiff_codec = R_IMF_TIFF_CODEC_NONE;
		if (custom_flags & TIF_COMPRESS_DEFLATE)
			im_format->tiff_codec = R_IMF_TIFF_CODEC_DEFLATE;
		if (custom_flags & TIF_COMPRESS_LZW)
			im_format->tiff_codec = R_IMF_TIFF_CODEC_LZW;
		if (custom_flags & TIF_COMPRESS_PACKBITS)
			im_format->tiff_codec = R_IMF_TIFF_CODEC_PACKBITS;
	}
#endif

#ifdef WITH_OPENEXR
	else if (ftype == IMB_FTYPE_OPENEXR) {
		im_format->imtype = R_IMF_IMTYPE_OPENEXR;
		if (custom_flags & OPENEXR_HALF)
			im_format->depth = R_IMF_CHAN_DEPTH_16;
		if (custom_flags & OPENEXR_COMPRESS)
			im_format->exr_codec = R_IMF_EXR_CODEC_ZIP;  // Can't determine compression
		if (imbuf->zbuf_float)
			im_format->flag |= R_IMF_FLAG_ZBUF;
	}
#endif

#ifdef WITH_CINEON
	else if (ftype == IMB_FTYPE_CINEON)
		im_format->imtype = R_IMF_IMTYPE_CINEON;
	else if (ftype == IMB_FTYPE_DPX)
		im_format->imtype = R_IMF_IMTYPE_DPX;
#endif

	else if (ftype == IMB_FTYPE_TGA) {
		if (custom_flags & RAWTGA)
			im_format->imtype = R_IMF_IMTYPE_RAWTGA;
		else
			im_format->imtype = R_IMF_IMTYPE_TARGA;
	}
#ifdef WITH_OPENJPEG
	else if (ftype == IMB_FTYPE_JP2) {
		im_format->imtype = R_IMF_IMTYPE_JP2;
		im_format->quality = quality;

		if (custom_flags & JP2_16BIT)
			im_format->depth = R_IMF_CHAN_DEPTH_16;
		else if (custom_flags & JP2_12BIT)
			im_format->depth = R_IMF_CHAN_DEPTH_12;

		if (custom_flags & JP2_YCC)
			im_format->jp2_flag |= R_IMF_JP2_FLAG_YCC;

		if (custom_flags & JP2_CINE) {
			im_format->jp2_flag |= R_IMF_JP2_FLAG_CINE_PRESET;
			if (custom_flags & JP2_CINE_48FPS)
				im_format->jp2_flag |= R_IMF_JP2_FLAG_CINE_48;
		}

		if (custom_flags & JP2_JP2)
			im_format->jp2_codec = R_IMF_JP2_CODEC_JP2;
		else if (custom_flags & JP2_J2K)
			im_format->jp2_codec = R_IMF_JP2_CODEC_J2K;
		else
			BLI_assert(!"Unsupported jp2 codec was specified in file type");
	}
#endif

	else {
		im_format->imtype = R_IMF_IMTYPE_JPEG90;
		im_format->quality = quality;
	}

	/* planes */
	im_format->planes = imbuf->planes;
}


#define STAMP_NAME_SIZE ((MAX_ID_NAME - 2) + 16)
/* could allow access externally - 512 is for long names,
 * STAMP_NAME_SIZE is for id names, allowing them some room for description */
typedef struct StampData {
	char file[512];
	char note[512];
	char date[512];
	char marker[512];
	char time[512];
	char frame[512];
	char camera[STAMP_NAME_SIZE];
	char cameralens[STAMP_NAME_SIZE];
	char scene[STAMP_NAME_SIZE];
	char strip[STAMP_NAME_SIZE];
	char rendertime[STAMP_NAME_SIZE];
	char memory[STAMP_NAME_SIZE];
} StampData;
#undef STAMP_NAME_SIZE

static void stampdata(Scene *scene, Object *camera, StampData *stamp_data, int do_prefix)
{
	char text[256];
	struct tm *tl;
	time_t t;

	if (scene->r.stamp & R_STAMP_FILENAME) {
		BLI_snprintf(stamp_data->file, sizeof(stamp_data->file), do_prefix ? "File %s" : "%s", G.relbase_valid ? G.main->name : "<untitled>");
	}
	else {
		stamp_data->file[0] = '\0';
	}

	if (scene->r.stamp & R_STAMP_NOTE) {
		/* Never do prefix for Note */
		BLI_snprintf(stamp_data->note, sizeof(stamp_data->note), "%s", scene->r.stamp_udata);
	}
	else {
		stamp_data->note[0] = '\0';
	}

	if (scene->r.stamp & R_STAMP_DATE) {
		t = time(NULL);
		tl = localtime(&t);
		BLI_snprintf(text, sizeof(text), "%04d/%02d/%02d %02d:%02d:%02d", tl->tm_year + 1900, tl->tm_mon + 1, tl->tm_mday, tl->tm_hour, tl->tm_min, tl->tm_sec);
		BLI_snprintf(stamp_data->date, sizeof(stamp_data->date), do_prefix ? "Date %s" : "%s", text);
	}
	else {
		stamp_data->date[0] = '\0';
	}

	if (scene->r.stamp & R_STAMP_MARKER) {
		const char *name = BKE_scene_find_last_marker_name(scene, CFRA);

		if (name) BLI_strncpy(text, name, sizeof(text));
		else BLI_strncpy(text, "<none>", sizeof(text));

		BLI_snprintf(stamp_data->marker, sizeof(stamp_data->marker), do_prefix ? "Marker %s" : "%s", text);
	}
	else {
		stamp_data->marker[0] = '\0';
	}

	if (scene->r.stamp & R_STAMP_TIME) {
		const short timecode_style = USER_TIMECODE_SMPTE_FULL;
		BLI_timecode_string_from_time(text, sizeof(text), 0, FRA2TIME(scene->r.cfra), FPS, timecode_style);
		BLI_snprintf(stamp_data->time, sizeof(stamp_data->time), do_prefix ? "Timecode %s" : "%s", text);
	}
	else {
		stamp_data->time[0] = '\0';
	}

	if (scene->r.stamp & R_STAMP_FRAME) {
		char fmtstr[32];
		int digits = 1;

		if (scene->r.efra > 9)
			digits = 1 + (int) log10(scene->r.efra);

		BLI_snprintf(fmtstr, sizeof(fmtstr), do_prefix ? "Frame %%0%di" : "%%0%di", digits);
		BLI_snprintf(stamp_data->frame, sizeof(stamp_data->frame), fmtstr, scene->r.cfra);
	}
	else {
		stamp_data->frame[0] = '\0';
	}

	if (scene->r.stamp & R_STAMP_CAMERA) {
		BLI_snprintf(stamp_data->camera, sizeof(stamp_data->camera), do_prefix ? "Camera %s" : "%s", camera ? camera->id.name + 2 : "<none>");
	}
	else {
		stamp_data->camera[0] = '\0';
	}

	if (scene->r.stamp & R_STAMP_CAMERALENS) {
		if (camera && camera->type == OB_CAMERA) {
			BLI_snprintf(text, sizeof(text), "%.2f", ((Camera *)camera->data)->lens);
		}
		else {
			BLI_strncpy(text, "<none>", sizeof(text));
		}

		BLI_snprintf(stamp_data->cameralens, sizeof(stamp_data->cameralens), do_prefix ? "Lens %s" : "%s", text);
	}
	else {
		stamp_data->cameralens[0] = '\0';
	}

	if (scene->r.stamp & R_STAMP_SCENE) {
		BLI_snprintf(stamp_data->scene, sizeof(stamp_data->scene), do_prefix ? "Scene %s" : "%s", scene->id.name + 2);
	}
	else {
		stamp_data->scene[0] = '\0';
	}

	if (scene->r.stamp & R_STAMP_SEQSTRIP) {
		Sequence *seq = BKE_sequencer_foreground_frame_get(scene, scene->r.cfra);

		if (seq) BLI_strncpy(text, seq->name + 2, sizeof(text));
		else BLI_strncpy(text, "<none>", sizeof(text));

		BLI_snprintf(stamp_data->strip, sizeof(stamp_data->strip), do_prefix ? "Strip %s" : "%s", text);
	}
	else {
		stamp_data->strip[0] = '\0';
	}

	{
		Render *re = RE_GetRender(scene->id.name);
		RenderStats *stats = re ? RE_GetStats(re) : NULL;

		if (stats && (scene->r.stamp & R_STAMP_RENDERTIME)) {
			BLI_timecode_string_from_time_simple(text, sizeof(text), stats->lastframetime);

			BLI_snprintf(stamp_data->rendertime, sizeof(stamp_data->rendertime), do_prefix ? "RenderTime %s" : "%s", text);
		}
		else {
			stamp_data->rendertime[0] = '\0';
		}

		if (stats && (scene->r.stamp & R_STAMP_MEMORY)) {
			BLI_snprintf(stamp_data->memory, sizeof(stamp_data->memory), do_prefix ? "Peak Memory %.2fM" : "%.2fM", stats->mem_peak);
		}
		else {
			stamp_data->memory[0] = '\0';
		}
	}
}

/* Will always add prefix. */
static void stampdata_from_template(StampData *stamp_data,
                                    const Scene *scene,
                                    const StampData *stamp_data_template)
{
	if (scene->r.stamp & R_STAMP_FILENAME) {
		BLI_snprintf(stamp_data->file, sizeof(stamp_data->file), "File %s", stamp_data_template->file);
	}
	else {
		stamp_data->file[0] = '\0';
	}
	if (scene->r.stamp & R_STAMP_NOTE) {
		BLI_snprintf(stamp_data->note, sizeof(stamp_data->note), "%s", stamp_data_template->note);
	}
	else {
		stamp_data->note[0] = '\0';
	}
	if (scene->r.stamp & R_STAMP_DATE) {
		BLI_snprintf(stamp_data->date, sizeof(stamp_data->date), "Date %s", stamp_data_template->date);
	}
	else {
		stamp_data->date[0] = '\0';
	}
	if (scene->r.stamp & R_STAMP_MARKER) {
		BLI_snprintf(stamp_data->marker, sizeof(stamp_data->marker), "Marker %s", stamp_data_template->marker);
	}
	else {
		stamp_data->marker[0] = '\0';
	}
	if (scene->r.stamp & R_STAMP_TIME) {
		BLI_snprintf(stamp_data->time, sizeof(stamp_data->time), "Timecode %s", stamp_data_template->time);
	}
	else {
		stamp_data->time[0] = '\0';
	}
	if (scene->r.stamp & R_STAMP_FRAME) {
		BLI_snprintf(stamp_data->frame, sizeof(stamp_data->frame), "Frame %s", stamp_data_template->frame);
	}
	else {
		stamp_data->frame[0] = '\0';
	}
	if (scene->r.stamp & R_STAMP_CAMERA) {
		BLI_snprintf(stamp_data->camera, sizeof(stamp_data->camera), "Camera %s", stamp_data_template->camera);
	}
	else {
		stamp_data->camera[0] = '\0';
	}
	if (scene->r.stamp & R_STAMP_CAMERALENS) {
		BLI_snprintf(stamp_data->cameralens, sizeof(stamp_data->cameralens), "Lens %s", stamp_data_template->cameralens);
	}
	else {
		stamp_data->cameralens[0] = '\0';
	}
	if (scene->r.stamp & R_STAMP_SCENE) {
		BLI_snprintf(stamp_data->scene, sizeof(stamp_data->scene), "Scene %s", stamp_data_template->scene);
	}
	else {
		stamp_data->scene[0] = '\0';
	}
	if (scene->r.stamp & R_STAMP_SEQSTRIP) {
		BLI_snprintf(stamp_data->strip, sizeof(stamp_data->strip), "Strip %s", stamp_data_template->strip);
	}
	else {
		stamp_data->strip[0] = '\0';
	}
	if (scene->r.stamp & R_STAMP_RENDERTIME) {
		BLI_snprintf(stamp_data->rendertime, sizeof(stamp_data->rendertime), "RenderTime %s", stamp_data_template->rendertime);
	}
	else {
		stamp_data->rendertime[0] = '\0';
	}
	if (scene->r.stamp & R_STAMP_MEMORY) {
		BLI_snprintf(stamp_data->memory, sizeof(stamp_data->memory), "Peak Memory %s", stamp_data_template->memory);
	}
	else {
		stamp_data->memory[0] = '\0';
	}
}

void BKE_image_stamp_buf(
        Scene *scene, Object *camera, const StampData *stamp_data_template,
        unsigned char *rect, float *rectf, int width, int height, int channels)
{
	struct StampData stamp_data;
	float w, h, pad;
	int x, y, y_ofs;
	float h_fixed;
	const int mono = blf_mono_font_render; // XXX
	struct ColorManagedDisplay *display;
	const char *display_device;

	/* vars for calculating wordwrap */
	struct {
		struct ResultBLF info;
		rctf rect;
	} wrap;

	/* this could be an argument if we want to operate on non linear float imbuf's
	 * for now though this is only used for renders which use scene settings */

#define TEXT_SIZE_CHECK(str, w, h) \
	((str[0]) && ((void)(h = h_fixed), (w = BLF_width(mono, str, sizeof(str)))))

	/* must enable BLF_WORD_WRAP before using */
#define TEXT_SIZE_CHECK_WORD_WRAP(str, w, h) \
	((str[0]) && (BLF_boundbox_ex(mono, str, sizeof(str), &wrap.rect, &wrap.info), \
	 (void)(h = h_fixed * wrap.info.lines), (w = BLI_rctf_size_x(&wrap.rect))))

#define BUFF_MARGIN_X 2
#define BUFF_MARGIN_Y 1

	if (!rect && !rectf)
		return;

	display_device = scene->display_settings.display_device;
	display = IMB_colormanagement_display_get_named(display_device);

	if (stamp_data_template == NULL) {
		stampdata(scene, camera, &stamp_data, (scene->r.stamp & R_STAMP_HIDE_LABELS) == 0);
	}
	else {
		stampdata_from_template(&stamp_data, scene, stamp_data_template);
	}

	/* TODO, do_versions */
	if (scene->r.stamp_font_id < 8)
		scene->r.stamp_font_id = 12;

	/* set before return */
	BLF_size(mono, scene->r.stamp_font_id, 72);
	BLF_wordwrap(mono, width - (BUFF_MARGIN_X * 2));

	BLF_buffer(mono, rectf, rect, width, height, channels, display);
	BLF_buffer_col(mono, scene->r.fg_stamp);
	pad = BLF_width_max(mono);

	/* use 'h_fixed' rather than 'h', aligns better */
	h_fixed = BLF_height_max(mono);
	y_ofs = -BLF_descender(mono);

	x = 0;
	y = height;

	if (TEXT_SIZE_CHECK(stamp_data.file, w, h)) {
		/* Top left corner */
		y -= h;

		/* also a little of space to the background. */
		buf_rectfill_area(rect, rectf, width, height, scene->r.bg_stamp, display,
		                  x - BUFF_MARGIN_X, y - BUFF_MARGIN_Y, w + BUFF_MARGIN_X, y + h + BUFF_MARGIN_Y);

		/* and draw the text. */
		BLF_position(mono, x, y + y_ofs, 0.0);
		BLF_draw_buffer(mono, stamp_data.file, BLF_DRAW_STR_DUMMY_MAX);

		/* the extra pixel for background. */
		y -= BUFF_MARGIN_Y * 2;
	}

	/* Top left corner, below File */
	if (TEXT_SIZE_CHECK(stamp_data.date, w, h)) {
		y -= h;

		/* and space for background. */
		buf_rectfill_area(rect, rectf, width, height, scene->r.bg_stamp, display,
		                  0, y - BUFF_MARGIN_Y, w + BUFF_MARGIN_X, y + h + BUFF_MARGIN_Y);

		BLF_position(mono, x, y + y_ofs, 0.0);
		BLF_draw_buffer(mono, stamp_data.date, BLF_DRAW_STR_DUMMY_MAX);

		/* the extra pixel for background. */
		y -= BUFF_MARGIN_Y * 2;
	}

	/* Top left corner, below File, Date */
	if (TEXT_SIZE_CHECK(stamp_data.rendertime, w, h)) {
		y -= h;

		/* and space for background. */
		buf_rectfill_area(rect, rectf, width, height, scene->r.bg_stamp, display,
		                  0, y - BUFF_MARGIN_Y, w + BUFF_MARGIN_X, y + h + BUFF_MARGIN_Y);

		BLF_position(mono, x, y + y_ofs, 0.0);
		BLF_draw_buffer(mono, stamp_data.rendertime, BLF_DRAW_STR_DUMMY_MAX);

		/* the extra pixel for background. */
		y -= BUFF_MARGIN_Y * 2;
	}

	/* Top left corner, below File, Date, Rendertime */
	if (TEXT_SIZE_CHECK(stamp_data.memory, w, h)) {
		y -= h;

		/* and space for background. */
		buf_rectfill_area(rect, rectf, width, height, scene->r.bg_stamp, display,
		                  0, y - BUFF_MARGIN_Y, w + BUFF_MARGIN_X, y + h + BUFF_MARGIN_Y);

		BLF_position(mono, x, y + y_ofs, 0.0);
		BLF_draw_buffer(mono, stamp_data.memory, BLF_DRAW_STR_DUMMY_MAX);

		/* the extra pixel for background. */
		y -= BUFF_MARGIN_Y * 2;
	}

	/* Top left corner, below File, Date, Memory, Rendertime */
	BLF_enable(mono, BLF_WORD_WRAP);
	if (TEXT_SIZE_CHECK_WORD_WRAP(stamp_data.note, w, h)) {
		y -= h;

		/* and space for background. */
		buf_rectfill_area(rect, rectf, width, height, scene->r.bg_stamp, display,
		                  0, y - BUFF_MARGIN_Y, w + BUFF_MARGIN_X, y + h + BUFF_MARGIN_Y);

		BLF_position(mono, x, y + y_ofs + (h - h_fixed), 0.0);
		BLF_draw_buffer(mono, stamp_data.note, BLF_DRAW_STR_DUMMY_MAX);
	}
	BLF_disable(mono, BLF_WORD_WRAP);

	x = 0;
	y = 0;

	/* Bottom left corner, leaving space for timing */
	if (TEXT_SIZE_CHECK(stamp_data.marker, w, h)) {

		/* extra space for background. */
		buf_rectfill_area(rect, rectf, width, height, scene->r.bg_stamp,  display,
		                  x - BUFF_MARGIN_X, y - BUFF_MARGIN_Y, w + BUFF_MARGIN_X, y + h + BUFF_MARGIN_Y);

		/* and pad the text. */
		BLF_position(mono, x, y + y_ofs, 0.0);
		BLF_draw_buffer(mono, stamp_data.marker, BLF_DRAW_STR_DUMMY_MAX);

		/* space width. */
		x += w + pad;
	}

	/* Left bottom corner */
	if (TEXT_SIZE_CHECK(stamp_data.time, w, h)) {

		/* extra space for background */
		buf_rectfill_area(rect, rectf, width, height, scene->r.bg_stamp, display,
		                  x - BUFF_MARGIN_X, y, x + w + BUFF_MARGIN_X, y + h + BUFF_MARGIN_Y);

		/* and pad the text. */
		BLF_position(mono, x, y + y_ofs, 0.0);
		BLF_draw_buffer(mono, stamp_data.time, BLF_DRAW_STR_DUMMY_MAX);

		/* space width. */
		x += w + pad;
	}

	if (TEXT_SIZE_CHECK(stamp_data.frame, w, h)) {

		/* extra space for background. */
		buf_rectfill_area(rect, rectf, width, height, scene->r.bg_stamp, display,
		                  x - BUFF_MARGIN_X, y - BUFF_MARGIN_Y, x + w + BUFF_MARGIN_X, y + h + BUFF_MARGIN_Y);

		/* and pad the text. */
		BLF_position(mono, x, y + y_ofs, 0.0);
		BLF_draw_buffer(mono, stamp_data.frame, BLF_DRAW_STR_DUMMY_MAX);

		/* space width. */
		x += w + pad;
	}

	if (TEXT_SIZE_CHECK(stamp_data.camera, w, h)) {

		/* extra space for background. */
		buf_rectfill_area(rect, rectf, width, height, scene->r.bg_stamp, display,
		                  x - BUFF_MARGIN_X, y - BUFF_MARGIN_Y, x + w + BUFF_MARGIN_X, y + h + BUFF_MARGIN_Y);
		BLF_position(mono, x, y + y_ofs, 0.0);
		BLF_draw_buffer(mono, stamp_data.camera, BLF_DRAW_STR_DUMMY_MAX);

		/* space width. */
		x += w + pad;
	}

	if (TEXT_SIZE_CHECK(stamp_data.cameralens, w, h)) {

		/* extra space for background. */
		buf_rectfill_area(rect, rectf, width, height, scene->r.bg_stamp, display,
		                  x - BUFF_MARGIN_X, y - BUFF_MARGIN_Y, x + w + BUFF_MARGIN_X, y + h + BUFF_MARGIN_Y);
		BLF_position(mono, x, y + y_ofs, 0.0);
		BLF_draw_buffer(mono, stamp_data.cameralens, BLF_DRAW_STR_DUMMY_MAX);
	}

	if (TEXT_SIZE_CHECK(stamp_data.scene, w, h)) {

		/* Bottom right corner, with an extra space because blenfont is too strict! */
		x = width - w - 2;

		/* extra space for background. */
		buf_rectfill_area(rect, rectf, width, height, scene->r.bg_stamp, display,
		                  x - BUFF_MARGIN_X, y - BUFF_MARGIN_Y, x + w + BUFF_MARGIN_X, y + h + BUFF_MARGIN_Y);

		/* and pad the text. */
		BLF_position(mono, x, y + y_ofs, 0.0);
		BLF_draw_buffer(mono, stamp_data.scene, BLF_DRAW_STR_DUMMY_MAX);
	}

	if (TEXT_SIZE_CHECK(stamp_data.strip, w, h)) {

		/* Top right corner, with an extra space because blenfont is too strict! */
		x = width - w - pad;
		y = height - h;

		/* extra space for background. */
		buf_rectfill_area(rect, rectf, width, height, scene->r.bg_stamp, display,
		                  x - BUFF_MARGIN_X, y - BUFF_MARGIN_Y, x + w + BUFF_MARGIN_X, y + h + BUFF_MARGIN_Y);

		BLF_position(mono, x, y + y_ofs, 0.0);
		BLF_draw_buffer(mono, stamp_data.strip, BLF_DRAW_STR_DUMMY_MAX);
	}

	/* cleanup the buffer. */
	BLF_buffer(mono, NULL, NULL, 0, 0, 0, NULL);
	BLF_wordwrap(mono, 0);

#undef TEXT_SIZE_CHECK
#undef TEXT_SIZE_CHECK_WORD_WRAP
#undef BUFF_MARGIN_X
#undef BUFF_MARGIN_Y
}

void BKE_render_result_stamp_info(Scene *scene, Object *camera, struct RenderResult *rr, bool allocate_only)
{
	struct StampData *stamp_data;

	if (!(scene && (scene->r.stamp & R_STAMP_ALL)) && !allocate_only)
		return;

	if (!rr->stamp_data) {
		stamp_data = MEM_callocN(sizeof(StampData), "RenderResult.stamp_data");
	}
	else {
		stamp_data = rr->stamp_data;
	}

	if (!allocate_only)
		stampdata(scene, camera, stamp_data, 0);

	if (!rr->stamp_data) {
		rr->stamp_data = stamp_data;
	}
}

void BKE_stamp_info_callback(void *data, struct StampData *stamp_data, StampCallback callback, bool noskip)
{
	if ((callback == NULL) || (stamp_data == NULL)) {
		return;
	}

#define CALL(member, value_str) \
	if (noskip || stamp_data->member[0]) { \
		callback(data, value_str, stamp_data->member, sizeof(stamp_data->member)); \
	} ((void)0)

	CALL(file, "File");
	CALL(note, "Note");
	CALL(date, "Date");
	CALL(marker, "Marker");
	CALL(time, "Time");
	CALL(frame, "Frame");
	CALL(camera, "Camera");
	CALL(cameralens, "Lens");
	CALL(scene, "Scene");
	CALL(strip, "Strip");
	CALL(rendertime, "RenderTime");
	CALL(memory, "Memory");

#undef CALL
}

/* wrap for callback only */
static void metadata_change_field(void *data, const char *propname, char *propvalue, int UNUSED(len))
{
	IMB_metadata_change_field(data, propname, propvalue);
}

static void metadata_get_field(void *data, const char *propname, char *propvalue, int len)
{
	IMB_metadata_get_field(data, propname, propvalue, len);
}

void BKE_imbuf_stamp_info(RenderResult *rr, struct ImBuf *ibuf)
{
	struct StampData *stamp_data = rr->stamp_data;

	BKE_stamp_info_callback(ibuf, stamp_data, metadata_change_field, false);
}

void BKE_stamp_info_from_imbuf(RenderResult *rr, struct ImBuf *ibuf)
{
	struct StampData *stamp_data = rr->stamp_data;

	BKE_stamp_info_callback(ibuf, stamp_data, metadata_get_field, true);
}

bool BKE_imbuf_alpha_test(ImBuf *ibuf)
{
	int tot;
	if (ibuf->rect_float) {
		const float *buf = ibuf->rect_float;
		for (tot = ibuf->x * ibuf->y; tot--; buf += 4) {
			if (buf[3] < 1.0f) {
				return true;
			}
		}
	}
	else if (ibuf->rect) {
		unsigned char *buf = (unsigned char *)ibuf->rect;
		for (tot = ibuf->x * ibuf->y; tot--; buf += 4) {
			if (buf[3] != 255) {
				return true;
			}
		}
	}

	return false;
}

/* note: imf->planes is ignored here, its assumed the image channels
 * are already set */
void BKE_imbuf_write_prepare(ImBuf *ibuf, const ImageFormatData *imf)
{
	char imtype = imf->imtype;
	char compress = imf->compress;
	char quality = imf->quality;

	/* initialize all from image format */
	ibuf->foptions.flag = 0;

	if (imtype == R_IMF_IMTYPE_IRIS) {
		ibuf->ftype = IMB_FTYPE_IMAGIC;
	}
#ifdef WITH_HDR
	else if (imtype == R_IMF_IMTYPE_RADHDR) {
		ibuf->ftype = IMB_FTYPE_RADHDR;
	}
#endif
	else if (ELEM(imtype, R_IMF_IMTYPE_PNG, R_IMF_IMTYPE_FFMPEG, R_IMF_IMTYPE_H264, R_IMF_IMTYPE_THEORA, R_IMF_IMTYPE_XVID)) {
		ibuf->ftype = IMB_FTYPE_PNG;

		if (imtype == R_IMF_IMTYPE_PNG) {
			if (imf->depth == R_IMF_CHAN_DEPTH_16)
				ibuf->foptions.flag |= PNG_16BIT;

			ibuf->foptions.quality = compress;
		}

	}
#ifdef WITH_DDS
	else if (imtype == R_IMF_IMTYPE_DDS) {
		ibuf->ftype = IMB_FTYPE_DDS;
	}
#endif
	else if (imtype == R_IMF_IMTYPE_BMP) {
		ibuf->ftype = IMB_FTYPE_BMP;
	}
#ifdef WITH_TIFF
	else if (imtype == R_IMF_IMTYPE_TIFF) {
		ibuf->ftype = IMB_FTYPE_TIF;

		if (imf->depth == R_IMF_CHAN_DEPTH_16) {
			ibuf->foptions.flag |= TIF_16BIT;
		}
		if (imf->tiff_codec == R_IMF_TIFF_CODEC_NONE) {
			ibuf->foptions.flag |= TIF_COMPRESS_NONE;
		}
		else if (imf->tiff_codec == R_IMF_TIFF_CODEC_DEFLATE) {
			ibuf->foptions.flag |= TIF_COMPRESS_DEFLATE;
		}
		else if (imf->tiff_codec == R_IMF_TIFF_CODEC_LZW) {
			ibuf->foptions.flag |= TIF_COMPRESS_LZW;
		}
		else if (imf->tiff_codec == R_IMF_TIFF_CODEC_PACKBITS) {
			ibuf->foptions.flag |= TIF_COMPRESS_PACKBITS;
		}
	}
#endif
#ifdef WITH_OPENEXR
	else if (ELEM(imtype, R_IMF_IMTYPE_OPENEXR, R_IMF_IMTYPE_MULTILAYER)) {
		ibuf->ftype = IMB_FTYPE_OPENEXR;
		if (imf->depth == R_IMF_CHAN_DEPTH_16)
			ibuf->foptions.flag |= OPENEXR_HALF;
		ibuf->foptions.flag |= (imf->exr_codec & OPENEXR_COMPRESS);

		if (!(imf->flag & R_IMF_FLAG_ZBUF)) {
			/* Signal for exr saving. */
			IMB_freezbuffloatImBuf(ibuf);
		}

	}
#endif
#ifdef WITH_CINEON
	else if (imtype == R_IMF_IMTYPE_CINEON) {
		ibuf->ftype = IMB_FTYPE_CINEON;
		if (imf->cineon_flag & R_IMF_CINEON_FLAG_LOG) {
			ibuf->foptions.flag |= CINEON_LOG;
		}
		if (imf->depth == R_IMF_CHAN_DEPTH_16) {
			ibuf->foptions.flag |= CINEON_16BIT;
		}
		else if (imf->depth == R_IMF_CHAN_DEPTH_12) {
			ibuf->foptions.flag |= CINEON_12BIT;
		}
		else if (imf->depth == R_IMF_CHAN_DEPTH_10) {
			ibuf->foptions.flag |= CINEON_10BIT;
		}
	}
	else if (imtype == R_IMF_IMTYPE_DPX) {
		ibuf->ftype = IMB_FTYPE_DPX;
		if (imf->cineon_flag & R_IMF_CINEON_FLAG_LOG) {
			ibuf->foptions.flag |= CINEON_LOG;
		}
		if (imf->depth == R_IMF_CHAN_DEPTH_16) {
			ibuf->foptions.flag |= CINEON_16BIT;
		}
		else if (imf->depth == R_IMF_CHAN_DEPTH_12) {
			ibuf->foptions.flag |= CINEON_12BIT;
		}
		else if (imf->depth == R_IMF_CHAN_DEPTH_10) {
			ibuf->foptions.flag |= CINEON_10BIT;
		}
	}
#endif
	else if (imtype == R_IMF_IMTYPE_TARGA) {
		ibuf->ftype = IMB_FTYPE_TGA;
	}
	else if (imtype == R_IMF_IMTYPE_RAWTGA) {
		ibuf->ftype = IMB_FTYPE_TGA;
		ibuf->foptions.flag = RAWTGA;
	}
#ifdef WITH_OPENJPEG
	else if (imtype == R_IMF_IMTYPE_JP2) {
		if (quality < 10) quality = 90;
		ibuf->ftype = IMB_FTYPE_JP2;
		ibuf->foptions.quality = quality;

		if (imf->depth == R_IMF_CHAN_DEPTH_16) {
			ibuf->foptions.flag |= JP2_16BIT;
		}
		else if (imf->depth == R_IMF_CHAN_DEPTH_12) {
			ibuf->foptions.flag |= JP2_12BIT;
		}

		if (imf->jp2_flag & R_IMF_JP2_FLAG_YCC) {
			ibuf->foptions.flag |= JP2_YCC;
		}

		if (imf->jp2_flag & R_IMF_JP2_FLAG_CINE_PRESET) {
			ibuf->foptions.flag |= JP2_CINE;
			if (imf->jp2_flag & R_IMF_JP2_FLAG_CINE_48)
				ibuf->foptions.flag |= JP2_CINE_48FPS;
		}

		if (imf->jp2_codec == R_IMF_JP2_CODEC_JP2)
			ibuf->foptions.flag |= JP2_JP2;
		else if (imf->jp2_codec == R_IMF_JP2_CODEC_J2K)
			ibuf->foptions.flag |= JP2_J2K;
		else
			BLI_assert(!"Unsupported jp2 codec was specified in im_format->jp2_codec");
	}
#endif
	else {
		/* R_IMF_IMTYPE_JPEG90, etc. default we save jpegs */
		if (quality < 10) quality = 90;
		ibuf->ftype = IMB_FTYPE_JPG;
		ibuf->foptions.quality = quality;
	}
}

int BKE_imbuf_write(ImBuf *ibuf, const char *name, const ImageFormatData *imf)
{
	int ok;

	BKE_imbuf_write_prepare(ibuf, imf);

	BLI_make_existing_file(name);

	ok = IMB_saveiff(ibuf, name, IB_rect | IB_zbuf | IB_zbuffloat);
	if (ok == 0) {
		perror(name);
	}

	return(ok);
}

/* same as BKE_imbuf_write() but crappy workaround not to permanently modify
 * _some_, values in the imbuf */
int BKE_imbuf_write_as(ImBuf *ibuf, const char *name, ImageFormatData *imf,
                       const bool save_copy)
{
	ImBuf ibuf_back = *ibuf;
	int ok;

	/* all data is rgba anyway,
	 * this just controls how to save for some formats */
	ibuf->planes = imf->planes;

	ok = BKE_imbuf_write(ibuf, name, imf);

	if (save_copy) {
		/* note that we are not restoring _all_ settings */
		ibuf->planes = ibuf_back.planes;
		ibuf->ftype =  ibuf_back.ftype;
		ibuf->foptions =  ibuf_back.foptions;
	}

	return ok;
}

int BKE_imbuf_write_stamp(
        Scene *scene, struct RenderResult *rr, ImBuf *ibuf, const char *name,
        const struct ImageFormatData *imf)
{
	if (scene && scene->r.stamp & R_STAMP_ALL)
		BKE_imbuf_stamp_info(rr, ibuf);

	return BKE_imbuf_write(ibuf, name, imf);
}

static void do_makepicstring(
        char *string, const char *base, const char *relbase, int frame, const char imtype,
        const ImageFormatData *im_format, const short use_ext, const short use_frames,
        const char *suffix)
{
	if (string == NULL) return;
	BLI_strncpy(string, base, FILE_MAX - 10);   /* weak assumption */
	BLI_path_abs(string, relbase);

	if (use_frames)
		BLI_path_frame(string, frame, 4);

	if (suffix)
		BLI_path_suffix(string, FILE_MAX, suffix, "");

	if (use_ext)
		do_add_image_extension(string, imtype, im_format);
}

void BKE_image_path_from_imformat(
        char *string, const char *base, const char *relbase, int frame,
        const ImageFormatData *im_format, const bool use_ext, const bool use_frames, const char *suffix)
{
	do_makepicstring(string, base, relbase, frame, im_format->imtype, im_format, use_ext, use_frames, suffix);
}

void BKE_image_path_from_imtype(
        char *string, const char *base, const char *relbase, int frame,
        const char imtype, const bool use_ext, const bool use_frames, const char *view)
{
	do_makepicstring(string, base, relbase, frame, imtype, NULL, use_ext, use_frames, view);
}

struct anim *openanim_noload(const char *name, int flags, int streamindex, char colorspace[IMA_MAX_SPACE])
{
	struct anim *anim;

	anim = IMB_open_anim(name, flags, streamindex, colorspace);
	return anim;
}

/* used by sequencer too */
struct anim *openanim(const char *name, int flags, int streamindex, char colorspace[IMA_MAX_SPACE])
{
	struct anim *anim;
	struct ImBuf *ibuf;

	anim = IMB_open_anim(name, flags, streamindex, colorspace);
	if (anim == NULL) return NULL;

	ibuf = IMB_anim_absolute(anim, 0, IMB_TC_NONE, IMB_PROXY_NONE);
	if (ibuf == NULL) {
		if (BLI_exists(name))
			printf("not an anim: %s\n", name);
		else
			printf("anim file doesn't exist: %s\n", name);
		IMB_free_anim(anim);
		return NULL;
	}
	IMB_freeImBuf(ibuf);

	return(anim);
}

/* ************************* New Image API *************** */


/* Notes about Image storage
 * - packedfile
 *   -> written in .blend
 * - filename
 *   -> written in .blend
 * - movie
 *   -> comes from packedfile or filename
 * - renderresult
 *   -> comes from packedfile or filename
 * - listbase
 *   -> ibufs from exrhandle
 * - flipbook array
 *   -> ibufs come from movie, temporary renderresult or sequence
 * - ibuf
 *   -> comes from packedfile or filename or generated
 */


/* forces existence of 1 Image for renderout or nodes, returns Image */
/* name is only for default, when making new one */
Image *BKE_image_verify_viewer(int type, const char *name)
{
	Image *ima;

	for (ima = G.main->image.first; ima; ima = ima->id.next)
		if (ima->source == IMA_SRC_VIEWER)
			if (ima->type == type)
				break;

	if (ima == NULL)
		ima = image_alloc(G.main, name, IMA_SRC_VIEWER, type);

	/* happens on reload, imagewindow cannot be image user when hidden*/
	if (ima->id.us == 0)
		id_us_plus(&ima->id);

	return ima;
}

static void image_viewer_create_views(const RenderData *rd, Image *ima)
{
	if ((rd->scemode & R_MULTIVIEW) == 0) {
		image_add_view(ima, "", "");
	}
	else {
		SceneRenderView *srv;
		for (srv = rd->views.first; srv; srv = srv->next) {
			if (BKE_scene_multiview_is_render_view_active(rd, srv) == false)
				continue;
			image_add_view(ima, srv->name, "");
		}
	}
}

/* Reset the image cache and views when the Viewer Nodes views don't match the scene views */
void BKE_image_verify_viewer_views(const RenderData *rd, Image *ima, ImageUser *iuser)
{
	bool do_reset;
	const bool is_multiview = (rd->scemode & R_MULTIVIEW) != 0;

	BLI_lock_thread(LOCK_DRAW_IMAGE);

	if (!BKE_scene_multiview_is_stereo3d(rd))
		iuser->flag &= ~IMA_SHOW_STEREO;

	/* see if all scene render views are in the image view list */
	do_reset = (BKE_scene_multiview_num_views_get(rd) != BLI_listbase_count(&ima->views));

	/* multiview also needs to be sure all the views are synced */
	if (is_multiview && !do_reset) {
		SceneRenderView *srv;
		ImageView *iv;

		for (iv = ima->views.first; iv; iv = iv->next) {
			srv = BLI_findstring(&rd->views, iv->name, offsetof(SceneRenderView, name));
			if ((srv == NULL) || (BKE_scene_multiview_is_render_view_active(rd, srv) == false)) {
				do_reset = true;
				break;
			}
		}
	}

	if (do_reset) {
		BLI_spin_lock(&image_spin);

		image_free_cached_frames(ima);
		BKE_image_free_views(ima);

		/* add new views */
		image_viewer_create_views(rd, ima);

		BLI_spin_unlock(&image_spin);
	}

	BLI_unlock_thread(LOCK_DRAW_IMAGE);
}

void BKE_image_walk_all_users(const Main *mainp, void *customdata,
                              void callback(Image *ima, ImageUser *iuser, void *customdata))
{
	wmWindowManager *wm;
	wmWindow *win;
	Tex *tex;

	/* texture users */
	for (tex = mainp->tex.first; tex; tex = tex->id.next) {
		if (tex->type == TEX_IMAGE && tex->ima) {
			callback(tex->ima, &tex->iuser, customdata);
		}

		if (tex->nodetree) {
			bNode *node;
			for (node = tex->nodetree->nodes.first; node; node = node->next) {
				if (node->id && node->type == TEX_NODE_IMAGE) {
					Image *ima = (Image *)node->id;
					ImageUser *iuser = node->storage;
					callback(ima, iuser, customdata);
				}
			}
		}
	}

	/* image window, compo node users */
	for (wm = mainp->wm.first; wm; wm = wm->id.next) { /* only 1 wm */
		for (win = wm->windows.first; win; win = win->next) {
			ScrArea *sa;
			for (sa = win->screen->areabase.first; sa; sa = sa->next) {
				if (sa->spacetype == SPACE_VIEW3D) {
					View3D *v3d = sa->spacedata.first;
					BGpic *bgpic;
					for (bgpic = v3d->bgpicbase.first; bgpic; bgpic = bgpic->next) {
						callback(bgpic->ima, &bgpic->iuser, customdata);
					}
				}
				else if (sa->spacetype == SPACE_IMAGE) {
					SpaceImage *sima = sa->spacedata.first;
					callback(sima->image, &sima->iuser, customdata);
				}
				else if (sa->spacetype == SPACE_NODE) {
					SpaceNode *snode = sa->spacedata.first;
					if (snode->nodetree && snode->nodetree->type == NTREE_COMPOSIT) {
						bNode *node;
						for (node = snode->nodetree->nodes.first; node; node = node->next) {
							if (node->id && node->type == CMP_NODE_IMAGE) {
								Image *ima = (Image *)node->id;
								ImageUser *iuser = node->storage;
								callback(ima, iuser, customdata);
							}
						}
					}
				}
			}
		}
	}
}

static void image_tag_frame_recalc(Image *ima, ImageUser *iuser, void *customdata)
{
	Image *changed_image = customdata;

	if (ima == changed_image && BKE_image_is_animated(ima)) {
		iuser->flag |= IMA_NEED_FRAME_RECALC;
	}
}

static void image_init_imageuser(Image *ima, ImageUser *iuser)
{
	RenderResult *rr = ima->rr;

	iuser->multi_index = 0;
	iuser->layer = iuser->pass = iuser->view = 0;

	if (rr)
		BKE_image_multilayer_index(rr, iuser);
}

void BKE_image_init_imageuser(Image *ima, ImageUser *iuser)
{
	image_init_imageuser(ima, iuser);
}

void BKE_image_signal(Image *ima, ImageUser *iuser, int signal)
{
	if (ima == NULL)
		return;

	BLI_spin_lock(&image_spin);

	switch (signal) {
		case IMA_SIGNAL_FREE:
			BKE_image_free_buffers(ima);

			if (iuser) {
				iuser->ok = 1;
				if (iuser->scene) {
					image_update_views_format(ima, iuser);
				}
			}
			break;
		case IMA_SIGNAL_SRC_CHANGE:
			if (ima->type == IMA_TYPE_UV_TEST)
				if (ima->source != IMA_SRC_GENERATED)
					ima->type = IMA_TYPE_IMAGE;

			if (ima->source == IMA_SRC_GENERATED) {
				if (ima->gen_x == 0 || ima->gen_y == 0) {
					ImBuf *ibuf = image_get_cached_ibuf_for_index_frame(ima, IMA_NO_INDEX, 0);
					if (ibuf) {
						ima->gen_x = ibuf->x;
						ima->gen_y = ibuf->y;
						IMB_freeImBuf(ibuf);
					}
				}

				/* Changing source type to generated will likely change file format
				 * used by generated image buffer. Saving different file format to
				 * the old name might confuse other applications.
				 *
				 * Here we ensure original image path wouldn't be used when saving
				 * generated image.
				 */
				ima->name[0] = '\0';
			}

#if 0
			/* force reload on first use, but not for multilayer, that makes nodes and buttons in ui drawing fail */
			if (ima->type != IMA_TYPE_MULTILAYER)
				BKE_image_free_buffers(ima);
#else
			/* image buffers for non-sequence multilayer will share buffers with RenderResult,
			 * however sequence multilayer will own buffers. Such logic makes switching from
			 * single multilayer file to sequence completely unstable
			 * since changes in nodes seems this workaround isn't needed anymore, all sockets
			 * are nicely detecting anyway, but freeing buffers always here makes multilayer
			 * sequences behave stable
			 */
			BKE_image_free_buffers(ima);
#endif

			ima->ok = 1;
			if (iuser)
				iuser->ok = 1;

			BKE_image_walk_all_users(G.main, ima, image_tag_frame_recalc);

			break;

		case IMA_SIGNAL_RELOAD:
			/* try to repack file */
			if (BKE_image_has_packedfile(ima)) {
				const int totfiles = image_num_files(ima);

				if (totfiles != BLI_listbase_count_ex(&ima->packedfiles, totfiles + 1)) {
					/* in case there are new available files to be loaded */
					image_free_packedfiles(ima);
					BKE_image_packfiles(NULL, ima, ID_BLEND_PATH(G.main, &ima->id));
				}
				else {
					ImagePackedFile *imapf;
					for (imapf = ima->packedfiles.first; imapf; imapf = imapf->next) {
						PackedFile *pf;
						pf = newPackedFile(NULL, imapf->filepath, ID_BLEND_PATH(G.main, &ima->id));
						if (pf) {
							freePackedFile(imapf->packedfile);
							imapf->packedfile = pf;
						}
						else {
							printf("ERROR: Image \"%s\" not available. Keeping packed image\n", imapf->filepath);
						}
					}
				}

				if (BKE_image_has_packedfile(ima))
					BKE_image_free_buffers(ima);
			}
			else
				BKE_image_free_buffers(ima);

			if (iuser) {
				iuser->ok = 1;
				if (iuser->scene) {
					image_update_views_format(ima, iuser);
				}
			}

			break;
		case IMA_SIGNAL_USER_NEW_IMAGE:
			if (iuser) {
				iuser->ok = 1;
				if (ima->source == IMA_SRC_FILE || ima->source == IMA_SRC_SEQUENCE) {
					if (ima->type == IMA_TYPE_MULTILAYER) {
						image_init_imageuser(ima, iuser);
					}
				}
			}
			break;
		case IMA_SIGNAL_COLORMANAGE:
			BKE_image_free_buffers(ima);

			ima->ok = 1;

			if (iuser)
				iuser->ok = 1;

			break;
	}

	BLI_spin_unlock(&image_spin);

	/* don't use notifiers because they are not 100% sure to succeeded
	 * this also makes sure all scenes are accounted for. */
	{
		Scene *scene;
		for (scene = G.main->scene.first; scene; scene = scene->id.next) {
			if (scene->nodetree) {
				nodeUpdateID(scene->nodetree, &ima->id);
			}
		}
	}
}

/* return renderpass for a given pass index and active view */
/* fallback to available if there are missing passes for active view */
static RenderPass *image_render_pass_get(RenderLayer *rl, const int pass, const int view, int *r_passindex)
{
	RenderPass *rpass_ret = NULL;
	RenderPass *rpass;

	int rp_index = 0;
	const char *rp_name = "";

	for (rpass = rl->passes.first; rpass; rpass = rpass->next, rp_index++) {
		if (rp_index == pass) {
			rpass_ret = rpass;
			if (view == 0) {
				/* no multiview or left eye */
				break;
			}
			else {
				rp_name = rpass->name;
			}
		}
		/* multiview */
		else if (rp_name[0] &&
		         STREQ(rpass->name, rp_name) &&
		         (rpass->view_id == view))
		{
			rpass_ret = rpass;
			break;
		}
	}

	/* fallback to the first pass in the layer */
	if (rpass_ret == NULL) {
		rp_index = 0;
		rpass_ret = rl->passes.first;
	}

	if (r_passindex) {
		*r_passindex = (rpass == rpass_ret ? rp_index : pass);
	}

	return rpass_ret;
}

/* if layer or pass changes, we need an index for the imbufs list */
/* note it is called for rendered results, but it doesnt use the index! */
/* and because rendered results use fake layer/passes, don't correct for wrong indices here */
RenderPass *BKE_image_multilayer_index(RenderResult *rr, ImageUser *iuser)
{
	RenderLayer *rl;
	RenderPass *rpass = NULL;

	if (rr == NULL)
		return NULL;

	if (iuser) {
		short index = 0, rv_index, rl_index = 0;
		bool is_stereo = (iuser->flag & IMA_SHOW_STEREO) && RE_RenderResult_is_stereo(rr);

		rv_index = is_stereo ? iuser->multiview_eye : iuser->view;
		if (RE_HasFakeLayer(rr)) rl_index += 1;

		for (rl = rr->layers.first; rl; rl = rl->next, rl_index++) {
			if (iuser->layer == rl_index) {
				int rp_index;
				rpass = image_render_pass_get(rl, iuser->pass, rv_index, &rp_index);
				iuser->multi_index = index + rp_index;
				break;
			}
			else {
				index += BLI_listbase_count(&rl->passes);
			}
		}
	}

	return rpass;
}

void BKE_image_multiview_index(Image *ima, ImageUser *iuser)
{
	if (iuser) {
		bool is_stereo = BKE_image_is_stereo(ima) && (iuser->flag & IMA_SHOW_STEREO);
		if (is_stereo) {
			iuser->multi_index = iuser->multiview_eye;
		}
		else {
			if ((iuser->view < 0) || (iuser->view >= BLI_listbase_count_ex(&ima->views, iuser->view + 1))) {
				iuser->multi_index = iuser->view = 0;
			}
			else {
				iuser->multi_index = iuser->view;
			}
		}
	}
}

/* if layer or pass changes, we need an index for the imbufs list */
/* note it is called for rendered results, but it doesnt use the index! */
/* and because rendered results use fake layer/passes, don't correct for wrong indices here */
bool BKE_image_is_multilayer(Image *ima)
{
	if (ELEM(ima->source, IMA_SRC_FILE, IMA_SRC_SEQUENCE)) {
		if (ima->type == IMA_TYPE_MULTILAYER) {
			return true;
		}
	}
	else if (ima->source == IMA_SRC_VIEWER) {
		if (ima->type == IMA_TYPE_R_RESULT) {
			return true;
		}
	}
	return false;
}

bool BKE_image_is_multiview(Image *ima)
{
	return (BLI_listbase_count_ex(&ima->views, 2) > 1);
}

bool BKE_image_is_stereo(Image *ima)
{
	return BKE_image_is_multiview(ima) &&
	       (BLI_findstring(&ima->views, STEREO_LEFT_NAME, offsetof(ImageView, name)) &&
	        BLI_findstring(&ima->views, STEREO_RIGHT_NAME, offsetof(ImageView, name)));
}

static void image_init_multilayer_multiview(Image *ima, RenderResult *rr)
{
	/* update image views from render views, but only if they actually changed,
	 * to avoid invalid memory access during render. ideally these should always
	 * be acquired with a mutex along with the render result, but there are still
	 * some places with just an image pointer that need to access views */
	if (rr && BLI_listbase_count(&ima->views) == BLI_listbase_count(&rr->views)) {
		ImageView *iv = ima->views.first;
		RenderView *rv = rr->views.first;
		bool modified = false;
		for (; rv; rv = rv->next, iv = iv->next) {
			modified |= !STREQ(rv->name, iv->name);
		}
		if (!modified)
			return;
	}

	BKE_image_free_views(ima);

	if (rr) {
		for (RenderView *rv = rr->views.first; rv; rv = rv->next) {
			ImageView *iv = MEM_callocN(sizeof(ImageView), "Viewer Image View");
			BLI_strncpy(iv->name, rv->name, sizeof(iv->name));
			BLI_addtail(&ima->views, iv);
		}
	}
}


RenderResult *BKE_image_acquire_renderresult(Scene *scene, Image *ima)
{
	RenderResult *rr = NULL;
	if (ima->rr) {
		rr = ima->rr;
	}
	else if (ima->type == IMA_TYPE_R_RESULT) {
		if (ima->render_slot == ima->last_render_slot)
			rr = RE_AcquireResultRead(RE_GetRender(scene->id.name));
		else
			rr = ima->renders[ima->render_slot];

		/* set proper views */
		image_init_multilayer_multiview(ima, rr);
	}

	return rr;
}

void BKE_image_release_renderresult(Scene *scene, Image *ima)
{
	if (ima->rr) {
		/* pass */
	}
	else if (ima->type == IMA_TYPE_R_RESULT) {
		if (ima->render_slot == ima->last_render_slot)
			RE_ReleaseResult(RE_GetRender(scene->id.name));
	}
}

bool BKE_image_is_openexr(struct Image *ima)
{
#ifdef WITH_OPENEXR
	if (ELEM(ima->source, IMA_SRC_FILE, IMA_SRC_SEQUENCE)) {
		return BLI_testextensie(ima->name, ".exr");
	}
#else
	UNUSED_VARS(ima);
#endif
	return false;
}

void BKE_image_backup_render(Scene *scene, Image *ima, bool free_current_slot)
{
	/* called right before rendering, ima->renders contains render
	 * result pointers for everything but the current render */
	Render *re = RE_GetRender(scene->id.name);
	int slot = ima->render_slot, last = ima->last_render_slot;

	if (slot != last) {
		ima->renders[last] = NULL;
		RE_SwapResult(re, &ima->renders[last]);

		if (ima->renders[slot]) {
			if (free_current_slot) {
				RE_FreeRenderResult(ima->renders[slot]);
				ima->renders[slot] = NULL;
			}
			else {
				RE_SwapResult(re, &ima->renders[slot]);
			}
		}
	}

	ima->last_render_slot = slot;
}

/**************************** multiview save openexr *********************************/
#ifdef WITH_OPENEXR
static const char *image_get_view_cb(void *base, const int view_id)
{
	Image *ima = base;
	ImageView *iv = BLI_findlink(&ima->views, view_id);
	return iv ? iv->name : "";
}
#endif  /* WITH_OPENEXR */

#ifdef WITH_OPENEXR
static ImBuf *image_get_buffer_cb(void *base, const int view_id)
{
	Image *ima = base;
	ImageUser iuser = {0};

	iuser.view = view_id;
	iuser.ok = 1;

	BKE_image_multiview_index(ima, &iuser);

	return image_acquire_ibuf(ima, &iuser, NULL);
}
#endif  /* WITH_OPENEXR */

bool BKE_image_save_openexr_multiview(Image *ima, ImBuf *ibuf, const char *filepath, const int flags)
{
#ifdef WITH_OPENEXR
	char name[FILE_MAX];
	bool ok;

	BLI_strncpy(name, filepath, sizeof(name));
	BLI_path_abs(name, G.main->name);

	ibuf->userdata = ima;
	ok = IMB_exr_multiview_save(ibuf, name, flags, BLI_listbase_count(&ima->views), image_get_view_cb, image_get_buffer_cb);
	ibuf->userdata = NULL;

	return ok;
#else
	UNUSED_VARS(ima, ibuf, filepath, flags);
	return false;
#endif
}

/**************************** multiview load openexr *********************************/

static void image_add_view(Image *ima, const char *viewname, const char *filepath)
{
	ImageView *iv;

	iv = MEM_mallocN(sizeof(ImageView), "Viewer Image View");
	BLI_strncpy(iv->name, viewname, sizeof(iv->name));
	BLI_strncpy(iv->filepath, filepath, sizeof(iv->filepath));

	/* For stereo drawing we need to ensure:
	 * STEREO_LEFT_NAME  == STEREO_LEFT_ID and
	 * STEREO_RIGHT_NAME == STEREO_RIGHT_ID */

	if (STREQ(viewname, STEREO_LEFT_NAME)) {
		BLI_addhead(&ima->views, iv);
	}
	else if (STREQ(viewname, STEREO_RIGHT_NAME)) {
		ImageView *left_iv = BLI_findstring(&ima->views, STEREO_LEFT_NAME, offsetof(ImageView, name));

		if (left_iv == NULL) {
			BLI_addhead(&ima->views, iv);
		}
		else {
			BLI_insertlinkafter(&ima->views, left_iv, iv);
		}
	}
	else {
		BLI_addtail(&ima->views, iv);
	}
}

#ifdef WITH_OPENEXR
static void image_add_view_cb(void *base, const char *str)
{
	Image *ima = base;
	image_add_view(ima, str, ima->name);
}

static void image_add_buffer_cb(void *base, const char *str, ImBuf *ibuf, const int frame)
{
	Image *ima = base;
	int id;
	bool predivide = (ima->alpha_mode == IMA_ALPHA_PREMUL);
	const char *colorspace = ima->colorspace_settings.name;
	const char *to_colorspace = IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_SCENE_LINEAR);

	if (ibuf == NULL)
		return;

	id = BLI_findstringindex(&ima->views, str, offsetof(ImageView, name));

	if (id == -1)
		return;

	if (ibuf->channels >= 3)
		IMB_colormanagement_transform(ibuf->rect_float, ibuf->x, ibuf->y, ibuf->channels,
		                              colorspace, to_colorspace, predivide);

	image_assign_ibuf(ima, ibuf, id, frame);
	IMB_freeImBuf(ibuf);
}
#endif  /* WITH_OPENEXR */

/* after imbuf load, openexr type can return with a exrhandle open */
/* in that case we have to build a render-result */
#ifdef WITH_OPENEXR
static void image_create_multiview(Image *ima, ImBuf *ibuf, const int frame)
{
	BKE_image_free_views(ima);

	IMB_exr_multiview_convert(ibuf->userdata, ima, image_add_view_cb, image_add_buffer_cb, frame);

	IMB_exr_close(ibuf->userdata);
}
#endif  /* WITH_OPENEXR */

/* after imbuf load, openexr type can return with a exrhandle open */
/* in that case we have to build a render-result */
#ifdef WITH_OPENEXR
static void image_create_multilayer(Image *ima, ImBuf *ibuf, int framenr)
{
	const char *colorspace = ima->colorspace_settings.name;
	bool predivide = (ima->alpha_mode == IMA_ALPHA_PREMUL);

	/* only load rr once for multiview */
	if (!ima->rr)
		ima->rr = RE_MultilayerConvert(ibuf->userdata, colorspace, predivide, ibuf->x, ibuf->y);

	IMB_exr_close(ibuf->userdata);

	ibuf->userdata = NULL;
	if (ima->rr)
		ima->rr->framenr = framenr;

	/* set proper views */
	image_init_multilayer_multiview(ima, ima->rr);
}
#endif  /* WITH_OPENEXR */

/* common stuff to do with images after loading */
static void image_initialize_after_load(Image *ima, ImBuf *ibuf)
{
	/* preview is NULL when it has never been used as an icon before */
	if (G.background == 0 && ima->preview == NULL)
		BKE_icon_changed(BKE_icon_id_ensure(&ima->id));

	/* fields */
	if (ima->flag & IMA_FIELDS) {
		if (ima->flag & IMA_STD_FIELD) de_interlace_st(ibuf);
		else de_interlace_ng(ibuf);
	}
	/* timer */
	BKE_image_tag_time(ima);

	ima->ok = IMA_OK_LOADED;

}

static int imbuf_alpha_flags_for_image(Image *ima)
{
	int flag = 0;

	if (ima->flag & IMA_IGNORE_ALPHA)
		flag |= IB_ignore_alpha;
	else if (ima->alpha_mode == IMA_ALPHA_PREMUL)
		flag |= IB_alphamode_premul;

	return flag;
}

/* the number of files will vary according to the stereo format */
static int image_num_files(Image *ima)
{
	const bool is_multiview = BKE_image_is_multiview(ima);

	if (!is_multiview) {
		return 1;
	}
	else if (ima->views_format == R_IMF_VIEWS_STEREO_3D) {
		return 1;
	}
	/* R_IMF_VIEWS_INDIVIDUAL */
	else {
		return BLI_listbase_count(&ima->views);
	}
}

static ImBuf *load_sequence_single(Image *ima, ImageUser *iuser, int frame, const int view_id, bool *r_assign)
{
	struct ImBuf *ibuf;
	char name[FILE_MAX];
	int flag;
	ImageUser iuser_t = {0};

	/* XXX temp stuff? */
	if (ima->lastframe != frame)
		ima->tpageflag |= IMA_TPAGE_REFRESH;

	ima->lastframe = frame;

	if (iuser) {
		iuser_t = *iuser;
	}
	else {
		/* TODO(sergey): Do we need to initialize something here? */
	}

	iuser_t.view = view_id;
	BKE_image_user_file_path(&iuser_t, ima, name);

	flag = IB_rect | IB_multilayer;
	flag |= imbuf_alpha_flags_for_image(ima);

	/* read ibuf */
	ibuf = IMB_loadiffname(name, flag, ima->colorspace_settings.name);

#if 0
	if (ibuf) {
		printf(AT " loaded %s\n", name);
	}
	else {
		printf(AT " missed %s\n", name);
	}
#endif

	if (ibuf) {
#ifdef WITH_OPENEXR
		/* handle multilayer case, don't assign ibuf. will be handled in BKE_image_acquire_ibuf */
		if (ibuf->ftype == IMB_FTYPE_OPENEXR && ibuf->userdata) {
			/* handle singlelayer multiview case assign ibuf based on available views */
			if (IMB_exr_has_singlelayer_multiview(ibuf->userdata)) {
				image_create_multiview(ima, ibuf, frame);
				IMB_freeImBuf(ibuf);
				ibuf = NULL;
			}
			else if (IMB_exr_has_multilayer(ibuf->userdata)) {
				/* handle multilayer case, don't assign ibuf. will be handled in BKE_image_acquire_ibuf */
				image_create_multilayer(ima, ibuf, frame);
				ima->type = IMA_TYPE_MULTILAYER;
				IMB_freeImBuf(ibuf);
				ibuf = NULL;
			}
		}
		else {
			image_initialize_after_load(ima, ibuf);
			*r_assign = true;
		}
#else
		image_initialize_after_load(ima, ibuf);
		*r_assign = true;
#endif
	}

	return ibuf;
}

static ImBuf *image_load_sequence_file(Image *ima, ImageUser *iuser, int frame)
{
	struct ImBuf *ibuf = NULL;
	const bool is_multiview = BKE_image_is_multiview(ima);
	const int totfiles = image_num_files(ima);
	bool assign = false;

	if (!is_multiview) {
		ibuf = load_sequence_single(ima, iuser, frame, 0, &assign);
		if (assign) {
			image_assign_ibuf(ima, ibuf, 0, frame);
		}
	}
	else {
		const int totviews = BLI_listbase_count(&ima->views);
		int i;
		struct ImBuf **ibuf_arr;

		ibuf_arr = MEM_mallocN(sizeof(ImBuf *) * totviews, "Image Views Imbufs");

		for (i = 0; i < totfiles; i++)
			ibuf_arr[i] = load_sequence_single(ima, iuser, frame, i, &assign);

		if (BKE_image_is_stereo(ima) && ima->views_format == R_IMF_VIEWS_STEREO_3D)
			IMB_ImBufFromStereo3d(ima->stereo3d_format, ibuf_arr[0], &ibuf_arr[0], &ibuf_arr[1]);

		/* return the original requested ImBuf */
		ibuf = ibuf_arr[(iuser ? iuser->multi_index : 0)];

		if (assign) {
			for (i = 0; i < totviews; i++) {
				image_assign_ibuf(ima, ibuf_arr[i], i, frame);
			}
		}

		/* "remove" the others (decrease their refcount) */
		for (i = 0; i < totviews; i++) {
			if (ibuf_arr[i] != ibuf) {
				IMB_freeImBuf(ibuf_arr[i]);
			}
		}

		/* cleanup */
		MEM_freeN(ibuf_arr);
	}

	return ibuf;
}

static ImBuf *image_load_sequence_multilayer(Image *ima, ImageUser *iuser, int frame)
{
	struct ImBuf *ibuf = NULL;

	/* either we load from RenderResult, or we have to load a new one */

	/* check for new RenderResult */
	if (ima->rr == NULL || frame != ima->rr->framenr) {
		if (ima->rr) {
			/* Cached image buffers shares pointers with render result,
			 * need to ensure there's no image buffers are hanging around
			 * with dead links after freeing the render result.
			 */
			image_free_cached_frames(ima);
			RE_FreeRenderResult(ima->rr);
			ima->rr = NULL;
		}

		ibuf = image_load_sequence_file(ima, iuser, frame);

		if (ibuf) { /* actually an error */
			ima->type = IMA_TYPE_IMAGE;
			printf("error, multi is normal image\n");
		}
	}
	if (ima->rr) {
		RenderPass *rpass = BKE_image_multilayer_index(ima->rr, iuser);

		if (rpass) {
			// printf("load from pass %s\n", rpass->name);
			/* since we free  render results, we copy the rect */
			ibuf = IMB_allocImBuf(ima->rr->rectx, ima->rr->recty, 32, 0);
			ibuf->rect_float = MEM_dupallocN(rpass->rect);
			ibuf->flags |= IB_rectfloat;
			ibuf->mall = IB_rectfloat;
			ibuf->channels = rpass->channels;

			image_initialize_after_load(ima, ibuf);
			image_assign_ibuf(ima, ibuf, iuser ? iuser->multi_index : 0, frame);

		}
		// else printf("pass not found\n");
	}
	else
		ima->ok = 0;

	if (iuser)
		iuser->ok = ima->ok;

	return ibuf;
}

static ImBuf *load_movie_single(Image *ima, ImageUser *iuser, int frame, const int view_id)
{
	struct ImBuf *ibuf = NULL;
	ImageAnim *ia;

	ia = BLI_findlink(&ima->anims, view_id);

	if (ia->anim == NULL) {
		char str[FILE_MAX];
		int flags = IB_rect;
		ImageUser iuser_t;

		if (ima->flag & IMA_DEINTERLACE) {
			flags |= IB_animdeinterlace;
		}

		if (iuser)
			iuser_t = *iuser;

		iuser_t.view = view_id;

		BKE_image_user_file_path(&iuser_t, ima, str);

		/* FIXME: make several stream accessible in image editor, too*/
		ia->anim = openanim(str, flags, 0, ima->colorspace_settings.name);

		/* let's initialize this user */
		if (ia->anim && iuser && iuser->frames == 0)
			iuser->frames = IMB_anim_get_duration(ia->anim,
			                                      IMB_TC_RECORD_RUN);
	}

	if (ia->anim) {
		int dur = IMB_anim_get_duration(ia->anim,
		                                IMB_TC_RECORD_RUN);
		int fra = frame - 1;

		if (fra < 0) fra = 0;
		if (fra > (dur - 1)) fra = dur - 1;
		ibuf = IMB_makeSingleUser(
		    IMB_anim_absolute(ia->anim, fra,
		                      IMB_TC_RECORD_RUN,
		                      IMB_PROXY_NONE));

		if (ibuf) {
			image_initialize_after_load(ima, ibuf);
		}
		else
			ima->ok = 0;
	}
	else
		ima->ok = 0;

	return ibuf;
}

static ImBuf *image_load_movie_file(Image *ima, ImageUser *iuser, int frame)
{
	struct ImBuf *ibuf = NULL;
	const bool is_multiview = BKE_image_is_multiview(ima);
	const int totfiles = image_num_files(ima);
	int i;

	if (totfiles != BLI_listbase_count_ex(&ima->anims, totfiles + 1)) {
		image_free_anims(ima);

		for (i = 0; i < totfiles; i++) {
			/* allocate the ImageAnim */
			ImageAnim *ia = MEM_callocN(sizeof(ImageAnim), "Image Anim");
			BLI_addtail(&ima->anims, ia);
		}
	}

	if (!is_multiview) {
		ibuf = load_movie_single(ima, iuser, frame, 0);
		image_assign_ibuf(ima, ibuf, 0, frame);
	}
	else {
		struct ImBuf **ibuf_arr;
		const int totviews = BLI_listbase_count(&ima->views);

		ibuf_arr = MEM_mallocN(sizeof(ImBuf *) * totviews, "Image Views (movie) Imbufs");

		for (i = 0; i < totfiles; i++) {
			ibuf_arr[i] = load_movie_single(ima, iuser, frame, i);
		}

		if (BKE_image_is_stereo(ima) && ima->views_format == R_IMF_VIEWS_STEREO_3D)
			IMB_ImBufFromStereo3d(ima->stereo3d_format, ibuf_arr[0], &ibuf_arr[0], &ibuf_arr[1]);

		for (i = 0; i < totviews; i++) {
			if (ibuf_arr[i]) {
				image_assign_ibuf(ima, ibuf_arr[i], i, frame);
			}
			else {
				ima->ok = 0;
			}
		}

		/* return the original requested ImBuf */
		ibuf = ibuf_arr[(iuser ? iuser->multi_index : 0)];

		/* "remove" the others (decrease their refcount) */
		for (i = 0; i < totviews; i++) {
			if (ibuf_arr[i] != ibuf) {
				IMB_freeImBuf(ibuf_arr[i]);
			}
		}

		/* cleanup */
		MEM_freeN(ibuf_arr);
	}

	if (iuser)
		iuser->ok = ima->ok;

	return ibuf;
}

static ImBuf *load_image_single(
        Image *ima, ImageUser *iuser, int cfra,
        const int view_id,
        const bool has_packed,
        bool *r_assign)
{
	char filepath[FILE_MAX];
	struct ImBuf *ibuf = NULL;
	int flag;

	/* is there a PackedFile with this image ? */
	if (has_packed) {
		ImagePackedFile *imapf;

		flag = IB_rect | IB_multilayer;
		flag |= imbuf_alpha_flags_for_image(ima);

		imapf = BLI_findlink(&ima->packedfiles, view_id);
		if (imapf->packedfile) {
			ibuf = IMB_ibImageFromMemory(
			       (unsigned char *)imapf->packedfile->data, imapf->packedfile->size, flag,
			       ima->colorspace_settings.name, "<packed data>");
		}
	}
	else {
		ImageUser iuser_t;

		flag = IB_rect | IB_multilayer | IB_metadata;
		flag |= imbuf_alpha_flags_for_image(ima);

		/* get the correct filepath */
		BKE_image_user_frame_calc(iuser, cfra, 0);

		if (iuser)
			iuser_t = *iuser;
		else
			iuser_t.framenr = ima->lastframe;

		iuser_t.view = view_id;

		BKE_image_user_file_path(&iuser_t, ima, filepath);

		/* read ibuf */
		ibuf = IMB_loadiffname(filepath, flag, ima->colorspace_settings.name);
	}

	if (ibuf) {
#ifdef WITH_OPENEXR
		if (ibuf->ftype == IMB_FTYPE_OPENEXR && ibuf->userdata) {
			if (IMB_exr_has_singlelayer_multiview(ibuf->userdata)) {
				/* handle singlelayer multiview case assign ibuf based on available views */
				image_create_multiview(ima, ibuf, cfra);
				IMB_freeImBuf(ibuf);
				ibuf = NULL;
			}
			else if (IMB_exr_has_multilayer(ibuf->userdata)) {
				/* handle multilayer case, don't assign ibuf. will be handled in BKE_image_acquire_ibuf */
				image_create_multilayer(ima, ibuf, cfra);
				ima->type = IMA_TYPE_MULTILAYER;
				IMB_freeImBuf(ibuf);
				ibuf = NULL;
			}
		}
		else
#endif
		{
			image_initialize_after_load(ima, ibuf);
			*r_assign = true;

			/* check if the image is a font image... */
			detectBitmapFont(ibuf);

			/* make packed file for autopack */
			if ((has_packed == false) && (G.fileflags & G_AUTOPACK)) {
				ImagePackedFile *imapf = MEM_mallocN(sizeof(ImagePackedFile), "Image Packefile");
				BLI_addtail(&ima->packedfiles, imapf);

				BLI_strncpy(imapf->filepath, filepath, sizeof(imapf->filepath));
				imapf->packedfile = newPackedFile(NULL, filepath, ID_BLEND_PATH(G.main, &ima->id));
			}
		}
	}
	else {
		ima->ok = 0;
	}

	return ibuf;
}

/* warning, 'iuser' can be NULL
 * note: Image->views was already populated (in image_update_views_format)
 */
static ImBuf *image_load_image_file(Image *ima, ImageUser *iuser, int cfra)
{
	struct ImBuf *ibuf = NULL;
	bool assign = false;
	const bool is_multiview = BKE_image_is_multiview(ima);
	const int totfiles = image_num_files(ima);
	bool has_packed = BKE_image_has_packedfile(ima);

	/* always ensure clean ima */
	BKE_image_free_buffers(ima);

	/* this should never happen, but just playing safe */
	if (has_packed) {
		if (totfiles != BLI_listbase_count_ex(&ima->packedfiles, totfiles + 1)) {
			image_free_packedfiles(ima);
			has_packed = false;
		}
	}

	if (!is_multiview) {
		ibuf = load_image_single(ima, iuser, cfra, 0, has_packed, &assign);
		if (assign) {
			image_assign_ibuf(ima, ibuf, IMA_NO_INDEX, 0);
		}
	}
	else {
		struct ImBuf **ibuf_arr;
		const int totviews = BLI_listbase_count(&ima->views);
		int i;
		BLI_assert(totviews > 0);

		ibuf_arr = MEM_callocN(sizeof(ImBuf *) * totviews, "Image Views Imbufs");

		for (i = 0; i < totfiles; i++)
			ibuf_arr[i] = load_image_single(ima, iuser, cfra, i, has_packed, &assign);

		/* multi-views/multi-layers OpenEXR files directly populate ima, and return NULL ibuf... */
		if (BKE_image_is_stereo(ima) && ima->views_format == R_IMF_VIEWS_STEREO_3D &&
		    ibuf_arr[0] && totfiles == 1 && totviews >= 2)
		{
			IMB_ImBufFromStereo3d(ima->stereo3d_format, ibuf_arr[0], &ibuf_arr[0], &ibuf_arr[1]);
		}

		/* return the original requested ImBuf */
		i = (iuser && iuser->multi_index < totviews) ? iuser->multi_index : 0;
		ibuf = ibuf_arr[i];

		if (assign) {
			for (i = 0; i < totviews; i++) {
				image_assign_ibuf(ima, ibuf_arr[i], i, 0);
			}
		}

		/* "remove" the others (decrease their refcount) */
		for (i = 0; i < totviews; i++) {
			if (ibuf_arr[i] != ibuf) {
				IMB_freeImBuf(ibuf_arr[i]);
			}
		}

		/* cleanup */
		MEM_freeN(ibuf_arr);
	}

	if (iuser)
		iuser->ok = ima->ok;

	return ibuf;
}

static ImBuf *image_get_ibuf_multilayer(Image *ima, ImageUser *iuser)
{
	ImBuf *ibuf = NULL;

	if (ima->rr == NULL) {
		ibuf = image_load_image_file(ima, iuser, 0);
		if (ibuf) { /* actually an error */
			ima->type = IMA_TYPE_IMAGE;
			return ibuf;
		}
	}
	if (ima->rr) {
		RenderPass *rpass = BKE_image_multilayer_index(ima->rr, iuser);

		if (rpass) {
			ibuf = IMB_allocImBuf(ima->rr->rectx, ima->rr->recty, 32, 0);

			image_initialize_after_load(ima, ibuf);

			ibuf->rect_float = rpass->rect;
			ibuf->flags |= IB_rectfloat;
			ibuf->channels = rpass->channels;

			image_assign_ibuf(ima, ibuf, iuser ? iuser->multi_index : IMA_NO_INDEX, 0);
		}
	}

	if (ibuf == NULL)
		ima->ok = 0;
	if (iuser)
		iuser->ok = ima->ok;

	return ibuf;
}


/* showing RGBA result itself (from compo/sequence) or
 * like exr, using layers etc */
/* always returns a single ibuf, also during render progress */
static ImBuf *image_get_render_result(Image *ima, ImageUser *iuser, void **r_lock)
{
	Render *re;
	RenderResult rres;
	RenderView *rv;
	float *rectf, *rectz;
	unsigned int *rect;
	float dither;
	int channels, layer, pass;
	ImBuf *ibuf;
	int from_render = (ima->render_slot == ima->last_render_slot);
	int actview;
	bool byte_buffer_in_display_space = false;

	if (!(iuser && iuser->scene))
		return NULL;

	/* if we the caller is not going to release the lock, don't give the image */
	if (!r_lock)
		return NULL;

	re = RE_GetRender(iuser->scene->id.name);

	channels = 4;
	layer = iuser->layer;
	pass = iuser->pass;
	actview = iuser->view;

	if (BKE_image_is_stereo(ima) && (iuser->flag & IMA_SHOW_STEREO))
		actview = iuser->multiview_eye;

	if (from_render) {
		RE_AcquireResultImage(re, &rres, actview);
	}
	else if (ima->renders[ima->render_slot]) {
		rres = *(ima->renders[ima->render_slot]);
		rres.have_combined = ((RenderView *)rres.views.first)->rectf != NULL;
	}
	else
		memset(&rres, 0, sizeof(RenderResult));

	if (!(rres.rectx > 0 && rres.recty > 0)) {
		if (from_render)
			RE_ReleaseResultImage(re);
		return NULL;
	}

	/* release is done in BKE_image_release_ibuf using r_lock */
	if (from_render) {
		BLI_lock_thread(LOCK_VIEWER);
		*r_lock = re;
		rv = NULL;
	}
	else {
		rv = BLI_findlink(&rres.views, actview);
		if (rv == NULL)
			rv = rres.views.first;
	}

	/* this gives active layer, composite or sequence result */
	if (rv == NULL) {
		rect = (unsigned int *)rres.rect32;
		rectf = rres.rectf;
		rectz = rres.rectz;
	}
	else {
		rect = (unsigned int *)rv->rect32;
		rectf = rv->rectf;
		rectz = rv->rectz;
	}

	dither = iuser->scene->r.dither_intensity;

	/* combined layer gets added as first layer */
	if (rres.have_combined && layer == 0) {
		/* pass */
	}
	else if (rect && layer == 0) {
		/* rect32 is set when there's a Sequence pass, this pass seems
		 * to have layer=0 (this is from image_buttons.c)
		 * in this case we ignore float buffer, because it could have
		 * hung from previous pass which was float
		 */
		rectf = NULL;
	}
	else if (rres.layers.first) {
		RenderLayer *rl = BLI_findlink(&rres.layers, layer - (rres.have_combined ? 1 : 0));
		if (rl) {
			RenderPass *rpass = image_render_pass_get(rl, pass, actview, NULL);
			if (rpass) {
				rectf = rpass->rect;
				if (pass == 0) {
					if (rectf == NULL) {
						/* Happens when Save Buffers is enabled.
						 * Use display buffer stored in the render layer.
						 */
						rect = (unsigned int *) rl->display_buffer;
						byte_buffer_in_display_space = true;
					}
				}
				else {
					channels = rpass->channels;
					dither = 0.0f; /* don't dither passes */
				}
			}

			for (rpass = rl->passes.first; rpass; rpass = rpass->next)
				if (STREQ(rpass->name, RE_PASSNAME_Z) && rpass->view_id == actview)
					rectz = rpass->rect;
		}
	}

	ibuf = image_get_cached_ibuf_for_index_frame(ima, IMA_NO_INDEX, 0);

	/* make ibuf if needed, and initialize it */
	if (ibuf == NULL) {
		ibuf = IMB_allocImBuf(rres.rectx, rres.recty, 32, 0);
		image_assign_ibuf(ima, ibuf, IMA_NO_INDEX, 0);
	}

	/* Set color space settings for a byte buffer.
	 *
	 * This is mainly to make it so color management treats byte buffer
	 * from render result with Save Buffers enabled as final display buffer
	 * and doesnt' apply any color management on it.
	 *
	 * For other cases we need to be sure it stays to default byte buffer space.
	 */
	if (ibuf->rect != rect) {
		if (byte_buffer_in_display_space) {
			const char *colorspace =
				IMB_colormanagement_get_display_colorspace_name(&iuser->scene->view_settings,
			                                                    &iuser->scene->display_settings);
			IMB_colormanagement_assign_rect_colorspace(ibuf, colorspace);
		}
		else {
			const char *colorspace = IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_DEFAULT_BYTE);
			IMB_colormanagement_assign_rect_colorspace(ibuf, colorspace);
		}
	}

	/* invalidate color managed buffers if render result changed */
	BLI_lock_thread(LOCK_COLORMANAGE);
	if (ibuf->x != rres.rectx || ibuf->y != rres.recty || ibuf->rect_float != rectf) {
		ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;
	}

	ibuf->x = rres.rectx;
	ibuf->y = rres.recty;

	if (rect) {
		imb_freerectImBuf(ibuf);
		ibuf->rect = rect;
	}
	else {
		/* byte buffer of render result has been freed, make sure image buffers
		 * does not reference to this buffer anymore
		 * need check for whether byte buffer was allocated and owned by image itself
		 * or if it's reusing buffer from render result
		 */
		if ((ibuf->mall & IB_rect) == 0)
			ibuf->rect = NULL;
	}

	if (rectf) {
		ibuf->rect_float = rectf;
		ibuf->flags |= IB_rectfloat;
		ibuf->channels = channels;
	}
	else {
		ibuf->rect_float = NULL;
		ibuf->flags &= ~IB_rectfloat;
	}

	if (rectz) {
		ibuf->zbuf_float = rectz;
		ibuf->flags |= IB_zbuffloat;
	}
	else {
		ibuf->zbuf_float = NULL;
		ibuf->flags &= ~IB_zbuffloat;
	}

	BLI_unlock_thread(LOCK_COLORMANAGE);

	ibuf->dither = dither;

	ima->ok = IMA_OK_LOADED;

	return ibuf;
}

static int image_get_multiview_index(Image *ima, ImageUser *iuser)
{
	const bool is_multilayer = BKE_image_is_multilayer(ima);
	const bool is_backdrop = (ima->source == IMA_SRC_VIEWER) && (ima->type ==  IMA_TYPE_COMPOSITE) && (iuser == NULL);
	int index = BKE_image_is_animated(ima) ? 0 : IMA_NO_INDEX;

	if (is_multilayer) {
		return iuser ? iuser->multi_index : index;
	}
	else if (is_backdrop) {
		if (BKE_image_is_stereo(ima)) {
			/* backdrop hackaround (since there is no iuser */
			return ima->eye;
		}
	}
	else if (BKE_image_is_multiview(ima)) {
		return iuser ? iuser->multi_index : index;
	}

	return index;
}

static void image_get_frame_and_index(Image *ima, ImageUser *iuser, int *r_frame, int *r_index)
{
	int frame = 0, index = image_get_multiview_index(ima, iuser);

	/* see if we already have an appropriate ibuf, with image source and type */
	if (ima->source == IMA_SRC_MOVIE) {
		frame = iuser ? iuser->framenr : ima->lastframe;
	}
	else if (ima->source == IMA_SRC_SEQUENCE) {
		if (ima->type == IMA_TYPE_IMAGE) {
			frame = iuser ? iuser->framenr : ima->lastframe;
		}
		else if (ima->type == IMA_TYPE_MULTILAYER) {
			frame = iuser ? iuser->framenr : ima->lastframe;
		}
	}

	*r_frame = frame;
	*r_index = index;
}

/* Get the ibuf from an image cache for a given image user.
 *
 * Returns referenced image buffer if it exists, callee is to
 * call IMB_freeImBuf to de-reference the image buffer after
 * it's done handling it.
 */
static ImBuf *image_get_cached_ibuf(Image *ima, ImageUser *iuser, int *r_frame, int *r_index)
{
	ImBuf *ibuf = NULL;
	int frame = 0, index = image_get_multiview_index(ima, iuser);

	/* see if we already have an appropriate ibuf, with image source and type */
	if (ima->source == IMA_SRC_MOVIE) {
		frame = iuser ? iuser->framenr : ima->lastframe;
		ibuf = image_get_cached_ibuf_for_index_frame(ima, index, frame);
		/* XXX temp stuff? */
		if (ima->lastframe != frame)
			ima->tpageflag |= IMA_TPAGE_REFRESH;
		ima->lastframe = frame;
	}
	else if (ima->source == IMA_SRC_SEQUENCE) {
		if (ima->type == IMA_TYPE_IMAGE) {
			frame = iuser ? iuser->framenr : ima->lastframe;
			ibuf = image_get_cached_ibuf_for_index_frame(ima, index, frame);

			/* XXX temp stuff? */
			if (ima->lastframe != frame) {
				ima->tpageflag |= IMA_TPAGE_REFRESH;
			}
			ima->lastframe = frame;

			/* counter the fact that image is set as invalid when loading a frame
			 * that is not in the cache (through image_acquire_ibuf for instance),
			 * yet we have valid frames in the cache loaded */
			if (ibuf) {
				ima->ok = IMA_OK_LOADED;

				if (iuser)
					iuser->ok = ima->ok;
			}
		}
		else if (ima->type == IMA_TYPE_MULTILAYER) {
			frame = iuser ? iuser->framenr : ima->lastframe;
			ibuf = image_get_cached_ibuf_for_index_frame(ima, index, frame);
		}
	}
	else if (ima->source == IMA_SRC_FILE) {
		if (ima->type == IMA_TYPE_IMAGE)
			ibuf = image_get_cached_ibuf_for_index_frame(ima, index, 0);
		else if (ima->type == IMA_TYPE_MULTILAYER)
			ibuf = image_get_cached_ibuf_for_index_frame(ima, index, 0);
	}
	else if (ima->source == IMA_SRC_GENERATED) {
		ibuf = image_get_cached_ibuf_for_index_frame(ima, index, 0);
	}
	else if (ima->source == IMA_SRC_VIEWER) {
		/* always verify entirely, not that this shouldn't happen
		 * as part of texture sampling in rendering anyway, so not
		 * a big bottleneck */
	}

	if (r_frame)
		*r_frame = frame;

	if (r_index)
		*r_index = index;

	return ibuf;
}

BLI_INLINE bool image_quick_test(Image *ima, ImageUser *iuser)
{
	if (ima == NULL)
		return false;

	if (iuser) {
		if (iuser->ok == 0)
			return false;
	}
	else if (ima->ok == 0)
		return false;

	return true;
}

/* Checks optional ImageUser and verifies/creates ImBuf.
 *
 * not thread-safe, so callee should worry about thread locks
 */
static ImBuf *image_acquire_ibuf(Image *ima, ImageUser *iuser, void **r_lock)
{
	ImBuf *ibuf = NULL;
	int frame = 0, index = 0;

	if (r_lock)
		*r_lock = NULL;

	/* quick reject tests */
	if (!image_quick_test(ima, iuser))
		return NULL;

	ibuf = image_get_cached_ibuf(ima, iuser, &frame, &index);

	if (ibuf == NULL) {
		/* we are sure we have to load the ibuf, using source and type */
		if (ima->source == IMA_SRC_MOVIE) {
			/* source is from single file, use flipbook to store ibuf */
			ibuf = image_load_movie_file(ima, iuser, frame);
		}
		else if (ima->source == IMA_SRC_SEQUENCE) {
			if (ima->type == IMA_TYPE_IMAGE) {
				/* regular files, ibufs in flipbook, allows saving */
				ibuf = image_load_sequence_file(ima, iuser, frame);
			}
			/* no else; on load the ima type can change */
			if (ima->type == IMA_TYPE_MULTILAYER) {
				/* only 1 layer/pass stored in imbufs, no exrhandle anim storage, no saving */
				ibuf = image_load_sequence_multilayer(ima, iuser, frame);
			}
		}
		else if (ima->source == IMA_SRC_FILE) {

			if (ima->type == IMA_TYPE_IMAGE)
				ibuf = image_load_image_file(ima, iuser, frame);  /* cfra only for '#', this global is OK */
			/* no else; on load the ima type can change */
			if (ima->type == IMA_TYPE_MULTILAYER)
				/* keeps render result, stores ibufs in listbase, allows saving */
				ibuf = image_get_ibuf_multilayer(ima, iuser);
		}
		else if (ima->source == IMA_SRC_GENERATED) {
			/* generated is: ibuf is allocated dynamically */
			/* UV testgrid or black or solid etc */
			if (ima->gen_x == 0) ima->gen_x = 1024;
			if (ima->gen_y == 0) ima->gen_y = 1024;
			if (ima->gen_depth == 0) ima->gen_depth = 24;
			ibuf = add_ibuf_size(ima->gen_x, ima->gen_y, ima->name, ima->gen_depth, (ima->gen_flag & IMA_GEN_FLOAT) != 0, ima->gen_type,
			                     ima->gen_color, &ima->colorspace_settings);
			image_assign_ibuf(ima, ibuf, index, 0);
			ima->ok = IMA_OK_LOADED;
		}
		else if (ima->source == IMA_SRC_VIEWER) {
			if (ima->type == IMA_TYPE_R_RESULT) {
				/* always verify entirely, and potentially
				 * returns pointer to release later */
				ibuf = image_get_render_result(ima, iuser, r_lock);
			}
			else if (ima->type == IMA_TYPE_COMPOSITE) {
				/* requires lock/unlock, otherwise don't return image */
				if (r_lock) {
					/* unlock in BKE_image_release_ibuf */
					BLI_lock_thread(LOCK_VIEWER);
					*r_lock = ima;

					/* XXX anim play for viewer nodes not yet supported */
					frame = 0; // XXX iuser ? iuser->framenr : 0;
					ibuf = image_get_cached_ibuf_for_index_frame(ima, index, frame);

					if (!ibuf) {
						/* Composite Viewer, all handled in compositor */
						/* fake ibuf, will be filled in compositor */
						ibuf = IMB_allocImBuf(256, 256, 32, IB_rect | IB_rectfloat);
						image_assign_ibuf(ima, ibuf, index, frame);
					}
				}
			}
		}

		/* We only want movies and sequences to be memory limited. */
		if (ibuf != NULL && !ELEM(ima->source, IMA_SRC_MOVIE, IMA_SRC_SEQUENCE)) {
			ibuf->userflags |= IB_PERSISTENT;
		}
	}

	BKE_image_tag_time(ima);

	return ibuf;
}

/* return image buffer for given image and user
 *
 * - will lock render result if image type is render result and lock is not NULL
 * - will return NULL if image type if render or composite result and lock is NULL
 *
 * references the result, BKE_image_release_ibuf should be used to de-reference
 */
ImBuf *BKE_image_acquire_ibuf(Image *ima, ImageUser *iuser, void **r_lock)
{
	ImBuf *ibuf;

	BLI_spin_lock(&image_spin);

	ibuf = image_acquire_ibuf(ima, iuser, r_lock);

	BLI_spin_unlock(&image_spin);

	return ibuf;
}

void BKE_image_release_ibuf(Image *ima, ImBuf *ibuf, void *lock)
{
	if (lock) {
		/* for getting image during threaded render / compositing, need to release */
		if (lock == ima) {
			BLI_unlock_thread(LOCK_VIEWER); /* viewer image */
		}
		else if (lock) {
			RE_ReleaseResultImage(lock); /* render result */
			BLI_unlock_thread(LOCK_VIEWER); /* view image imbuf */
		}
	}

	if (ibuf) {
		BLI_spin_lock(&image_spin);
		IMB_freeImBuf(ibuf);
		BLI_spin_unlock(&image_spin);
	}
}

/* checks whether there's an image buffer for given image and user */
bool BKE_image_has_ibuf(Image *ima, ImageUser *iuser)
{
	ImBuf *ibuf;

	/* quick reject tests */
	if (!image_quick_test(ima, iuser))
		return false;

	BLI_spin_lock(&image_spin);

	ibuf = image_get_cached_ibuf(ima, iuser, NULL, NULL);

	if (!ibuf)
		ibuf = image_acquire_ibuf(ima, iuser, NULL);

	BLI_spin_unlock(&image_spin);

	IMB_freeImBuf(ibuf);

	return ibuf != NULL;
}

/* ******** Pool for image buffers ********  */

typedef struct ImagePoolEntry {
	struct ImagePoolEntry *next, *prev;
	Image *image;
	ImBuf *ibuf;
	int index;
	int frame;
} ImagePoolEntry;

typedef struct ImagePool {
	ListBase image_buffers;
	BLI_mempool *memory_pool;
} ImagePool;

ImagePool *BKE_image_pool_new(void)
{
	ImagePool *pool = MEM_callocN(sizeof(ImagePool), "Image Pool");
	pool->memory_pool = BLI_mempool_create(sizeof(ImagePoolEntry), 0, 128, BLI_MEMPOOL_NOP);

	return pool;
}

void BKE_image_pool_free(ImagePool *pool)
{
	/* Use single lock to dereference all the image buffers. */
	BLI_spin_lock(&image_spin);
	for (ImagePoolEntry *entry = pool->image_buffers.first;
	     entry != NULL;
	     entry = entry->next)
	{
		if (entry->ibuf) {
			IMB_freeImBuf(entry->ibuf);
		}
	}
	BLI_spin_unlock(&image_spin);

	BLI_mempool_destroy(pool->memory_pool);
	MEM_freeN(pool);
}

BLI_INLINE ImBuf *image_pool_find_entry(ImagePool *pool, Image *image, int frame, int index, bool *found)
{
	ImagePoolEntry *entry;

	*found = false;

	for (entry = pool->image_buffers.first; entry; entry = entry->next) {
		if (entry->image == image && entry->frame == frame && entry->index == index) {
			*found = true;
			return entry->ibuf;
		}
	}

	return NULL;
}

ImBuf *BKE_image_pool_acquire_ibuf(Image *ima, ImageUser *iuser, ImagePool *pool)
{
	ImBuf *ibuf;
	int index, frame;
	bool found;

	if (!image_quick_test(ima, iuser))
		return NULL;

	if (pool == NULL) {
		/* pool could be NULL, in this case use general acquire function */
		return BKE_image_acquire_ibuf(ima, iuser, NULL);
	}

	image_get_frame_and_index(ima, iuser, &frame, &index);

	ibuf = image_pool_find_entry(pool, ima, frame, index, &found);
	if (found)
		return ibuf;

	BLI_spin_lock(&image_spin);

	ibuf = image_pool_find_entry(pool, ima, frame, index, &found);

	/* will also create entry even in cases image buffer failed to load,
	 * prevents trying to load the same buggy file multiple times
	 */
	if (!found) {
		ImagePoolEntry *entry;

		ibuf = image_acquire_ibuf(ima, iuser, NULL);

		entry = BLI_mempool_alloc(pool->memory_pool);
		entry->image = ima;
		entry->frame = frame;
		entry->index = index;
		entry->ibuf = ibuf;

		BLI_addtail(&pool->image_buffers, entry);
	}

	BLI_spin_unlock(&image_spin);

	return ibuf;
}

void BKE_image_pool_release_ibuf(Image *ima, ImBuf *ibuf, ImagePool *pool)
{
	/* if pool wasn't actually used, use general release stuff,
	 * for pools image buffers will be dereferenced on pool free
	 */
	if (pool == NULL) {
		BKE_image_release_ibuf(ima, ibuf, NULL);
	}
}

int BKE_image_user_frame_get(const ImageUser *iuser, int cfra, int fieldnr, bool *r_is_in_range)
{
	const int len = (iuser->fie_ima * iuser->frames) / 2;

	if (r_is_in_range) {
		*r_is_in_range = false;
	}

	if (len == 0) {
		return 0;
	}
	else {
		int framenr;
		cfra = cfra - iuser->sfra + 1;

		/* cyclic */
		if (iuser->cycl) {
			cfra = ((cfra) % len);
			if (cfra < 0) cfra += len;
			if (cfra == 0) cfra = len;

			if (r_is_in_range) {
				*r_is_in_range = true;
			}
		}

		if (cfra < 0) {
			cfra = 0;
		}
		else if (cfra > len) {
			cfra = len;
		}
		else {
			if (r_is_in_range) {
				*r_is_in_range = true;
			}
		}

		/* convert current frame to current field */
		cfra = 2 * (cfra);
		if (fieldnr) cfra++;

		/* transform to images space */
		framenr = (cfra + iuser->fie_ima - 2) / iuser->fie_ima;
		if (framenr > iuser->frames) framenr = iuser->frames;

		if (iuser->cycl) {
			framenr = ((framenr) % len);
			while (framenr < 0) framenr += len;
			if (framenr == 0) framenr = len;
		}

		/* important to apply after else we cant loop on frames 100 - 110 for eg. */
		framenr += iuser->offset;

		return framenr;
	}
}

void BKE_image_user_frame_calc(ImageUser *iuser, int cfra, int fieldnr)
{
	if (iuser) {
		bool is_in_range;
		const int framenr = BKE_image_user_frame_get(iuser, cfra, fieldnr, &is_in_range);

		if (is_in_range) {
			iuser->flag |= IMA_USER_FRAME_IN_RANGE;
		}
		else {
			iuser->flag &= ~IMA_USER_FRAME_IN_RANGE;
		}

		/* allows image users to handle redraws */
		if (iuser->flag & IMA_ANIM_ALWAYS)
			if (framenr != iuser->framenr)
				iuser->flag |= IMA_ANIM_REFRESHED;

		iuser->framenr = framenr;
		if (iuser->ok == 0) iuser->ok = 1;
	}
}

void BKE_image_user_check_frame_calc(ImageUser *iuser, int cfra, int fieldnr)
{
	if ((iuser->flag & IMA_ANIM_ALWAYS) || (iuser->flag & IMA_NEED_FRAME_RECALC)) {
		BKE_image_user_frame_calc(iuser, cfra, fieldnr);

		iuser->flag &= ~IMA_NEED_FRAME_RECALC;
	}
}

/* goes over all ImageUsers, and sets frame numbers if auto-refresh is set */
static void image_update_frame(struct Image *UNUSED(ima), struct ImageUser *iuser, void *customdata)
{
	int cfra = *(int *)customdata;

	BKE_image_user_check_frame_calc(iuser, cfra, 0);
}

void BKE_image_update_frame(const Main *bmain, int cfra)
{
	BKE_image_walk_all_users(bmain, &cfra, image_update_frame);
}

void BKE_image_user_file_path(ImageUser *iuser, Image *ima, char *filepath)
{
	if (BKE_image_is_multiview(ima) && (ima->rr == NULL)) {
		ImageView *iv = BLI_findlink(&ima->views, iuser->view);
		if (iv->filepath[0])
			BLI_strncpy(filepath, iv->filepath, FILE_MAX);
		else
			BLI_strncpy(filepath, ima->name, FILE_MAX);
	}
	else {
		BLI_strncpy(filepath, ima->name, FILE_MAX);
	}

	if (ima->source == IMA_SRC_SEQUENCE) {
		char head[FILE_MAX], tail[FILE_MAX];
		unsigned short numlen;
		int frame = iuser ? iuser->framenr : ima->lastframe;

		BLI_stringdec(filepath, head, tail, &numlen);
		BLI_stringenc(filepath, head, tail, numlen, frame);
	}

	BLI_path_abs(filepath, ID_BLEND_PATH(G.main, &ima->id));
}

bool BKE_image_has_alpha(struct Image *image)
{
	ImBuf *ibuf;
	void *lock;
	int planes;

	ibuf = BKE_image_acquire_ibuf(image, NULL, &lock);
	planes = (ibuf ? ibuf->planes : 0);
	BKE_image_release_ibuf(image, ibuf, lock);

	if (planes == 32)
		return true;
	else
		return false;
}

void BKE_image_get_size(Image *image, ImageUser *iuser, int *width, int *height)
{
	ImBuf *ibuf = NULL;
	void *lock;

	if (image != NULL) {
		ibuf = BKE_image_acquire_ibuf(image, iuser, &lock);
	}

	if (ibuf && ibuf->x > 0 && ibuf->y > 0) {
		*width = ibuf->x;
		*height = ibuf->y;
	}
	else if (image != NULL && image->type == IMA_TYPE_R_RESULT &&
	         iuser != NULL && iuser->scene != NULL)
	{
		Scene *scene = iuser->scene;
		*width = (scene->r.xsch * scene->r.size) / 100;
		*height = (scene->r.ysch * scene->r.size) / 100;
		if ((scene->r.mode & R_BORDER) && (scene->r.mode & R_CROP)) {
			*width *= BLI_rctf_size_x(&scene->r.border);
			*height *= BLI_rctf_size_y(&scene->r.border);
		}
	}
	else {
		*width  = IMG_SIZE_FALLBACK;
		*height = IMG_SIZE_FALLBACK;
	}

	if (image != NULL) {
		BKE_image_release_ibuf(image, ibuf, lock);
	}
}

void BKE_image_get_size_fl(Image *image, ImageUser *iuser, float size[2])
{
	int width, height;
	BKE_image_get_size(image, iuser, &width, &height);

	size[0] = (float)width;
	size[1] = (float)height;

}

void BKE_image_get_aspect(Image *image, float *aspx, float *aspy)
{
	*aspx = 1.0;

	/* x is always 1 */
	if (image)
		*aspy = image->aspy / image->aspx;
	else
		*aspy = 1.0f;
}

unsigned char *BKE_image_get_pixels_for_frame(struct Image *image, int frame)
{
	ImageUser iuser = {NULL};
	void *lock;
	ImBuf *ibuf;
	unsigned char *pixels = NULL;

	iuser.framenr = frame;
	iuser.ok = true;

	ibuf = BKE_image_acquire_ibuf(image, &iuser, &lock);

	if (ibuf) {
		pixels = (unsigned char *) ibuf->rect;

		if (pixels)
			pixels = MEM_dupallocN(pixels);

		BKE_image_release_ibuf(image, ibuf, lock);
	}

	if (!pixels)
		return NULL;

	return pixels;
}

float *BKE_image_get_float_pixels_for_frame(struct Image *image, int frame)
{
	ImageUser iuser = {NULL};
	void *lock;
	ImBuf *ibuf;
	float *pixels = NULL;

	iuser.framenr = frame;
	iuser.ok = true;

	ibuf = BKE_image_acquire_ibuf(image, &iuser, &lock);

	if (ibuf) {
		pixels = ibuf->rect_float;

		if (pixels)
			pixels = MEM_dupallocN(pixels);

		BKE_image_release_ibuf(image, ibuf, lock);
	}

	if (!pixels)
		return NULL;

	return pixels;
}

int BKE_image_sequence_guess_offset(Image *image)
{
	return BLI_stringdec(image->name, NULL, NULL, NULL);
}

bool BKE_image_has_anim(Image *ima)
{
	return (BLI_listbase_is_empty(&ima->anims) == false);
}

bool BKE_image_has_packedfile(Image *ima)
{
	return (BLI_listbase_is_empty(&ima->packedfiles) == false);
}

/**
 * Checks the image buffer changes (not keyframed values)
 *
 * to see if we need to call #BKE_image_user_check_frame_calc
 */
bool BKE_image_is_animated(Image *image)
{
	return ELEM(image->source, IMA_SRC_MOVIE, IMA_SRC_SEQUENCE);
}

bool BKE_image_is_dirty(Image *image)
{
	bool is_dirty = false;

	BLI_spin_lock(&image_spin);
	if (image->cache != NULL) {
		struct MovieCacheIter *iter = IMB_moviecacheIter_new(image->cache);

		while (!IMB_moviecacheIter_done(iter)) {
			ImBuf *ibuf = IMB_moviecacheIter_getImBuf(iter);
			if (ibuf->userflags & IB_BITMAPDIRTY) {
				is_dirty = true;
				break;
			}
			IMB_moviecacheIter_step(iter);
		}
		IMB_moviecacheIter_free(iter);
	}
	BLI_spin_unlock(&image_spin);

	return is_dirty;
}

void BKE_image_file_format_set(Image *image, int ftype, const ImbFormatOptions *options)
{
#if 0
	ImBuf *ibuf = BKE_image_acquire_ibuf(image, NULL, NULL);
	if (ibuf) {
		ibuf->ftype = ftype;
		ibuf->foptions = options;
	}
	BKE_image_release_ibuf(image, ibuf, NULL);
#endif

	BLI_spin_lock(&image_spin);
	if (image->cache != NULL) {
		struct MovieCacheIter *iter = IMB_moviecacheIter_new(image->cache);

		while (!IMB_moviecacheIter_done(iter)) {
			ImBuf *ibuf = IMB_moviecacheIter_getImBuf(iter);
			ibuf->ftype = ftype;
			ibuf->foptions = *options;
			IMB_moviecacheIter_step(iter);
		}
		IMB_moviecacheIter_free(iter);
	}
	BLI_spin_unlock(&image_spin);
}

bool BKE_image_has_loaded_ibuf(Image *image)
{
	bool has_loaded_ibuf = false;

	BLI_spin_lock(&image_spin);
	if (image->cache != NULL) {
		struct MovieCacheIter *iter = IMB_moviecacheIter_new(image->cache);

		while (!IMB_moviecacheIter_done(iter)) {
			has_loaded_ibuf = true;
			break;
		}
		IMB_moviecacheIter_free(iter);
	}
	BLI_spin_unlock(&image_spin);

	return has_loaded_ibuf;
}

/* References the result, BKE_image_release_ibuf is to be called to de-reference.
 * Use lock=NULL when calling BKE_image_release_ibuf().
 */
ImBuf *BKE_image_get_ibuf_with_name(Image *image, const char *name)
{
	ImBuf *ibuf = NULL;

	BLI_spin_lock(&image_spin);
	if (image->cache != NULL) {
		struct MovieCacheIter *iter = IMB_moviecacheIter_new(image->cache);

		while (!IMB_moviecacheIter_done(iter)) {
			ImBuf *current_ibuf = IMB_moviecacheIter_getImBuf(iter);
			if (STREQ(current_ibuf->name, name)) {
				ibuf = current_ibuf;
				IMB_refImBuf(ibuf);
				break;
			}
			IMB_moviecacheIter_step(iter);
		}
		IMB_moviecacheIter_free(iter);
	}
	BLI_spin_unlock(&image_spin);

	return ibuf;
}

/* References the result, BKE_image_release_ibuf is to be called to de-reference.
 * Use lock=NULL when calling BKE_image_release_ibuf().
 *
 * TODO(sergey): This is actually "get first entry from the cache", which is
 *               not so much predictable. But using first loaded image buffer
 *               was also malicious logic and all the areas which uses this
 *               function are to be re-considered.
 */
ImBuf *BKE_image_get_first_ibuf(Image *image)
{
	ImBuf *ibuf = NULL;

	BLI_spin_lock(&image_spin);
	if (image->cache != NULL) {
		struct MovieCacheIter *iter = IMB_moviecacheIter_new(image->cache);

		while (!IMB_moviecacheIter_done(iter)) {
			ibuf = IMB_moviecacheIter_getImBuf(iter);
			IMB_refImBuf(ibuf);
			break;
		}
		IMB_moviecacheIter_free(iter);
	}
	BLI_spin_unlock(&image_spin);

	return ibuf;
}

static void image_update_views_format(Image *ima, ImageUser *iuser)
{
	SceneRenderView *srv;
	ImageView *iv;
	Scene *scene = iuser->scene;
	const bool is_multiview = ((scene->r.scemode & R_MULTIVIEW) != 0) &&
	                          ((ima->flag & IMA_USE_VIEWS) != 0);

	/* reset the image views */
	BKE_image_free_views(ima);

	if (!is_multiview) {
		/* nothing to do */
	}
	else if (ima->views_format == R_IMF_VIEWS_STEREO_3D) {
		int i;
		const char *names[2] = {STEREO_LEFT_NAME, STEREO_RIGHT_NAME};

		for (i = 0; i < 2; i++) {
			image_add_view(ima, names[i], ima->name);
		}
		return;
	}
	else {
		/* R_IMF_VIEWS_INDIVIDUAL */
		char prefix[FILE_MAX] = {'\0'};
		char *name = ima->name;
		const char *ext = NULL;

		BKE_scene_multiview_view_prefix_get(scene, name, prefix, &ext);

		if (prefix[0] == '\0') {
			BKE_image_free_views(ima);
			return;
		}

		/* create all the image views */
		for (srv = scene->r.views.first; srv; srv = srv->next) {
			if (BKE_scene_multiview_is_render_view_active(&scene->r, srv)) {
				char filepath[FILE_MAX];
				BLI_snprintf(filepath, sizeof(filepath), "%s%s%s", prefix, srv->suffix, ext);
				image_add_view(ima, srv->name, filepath);
			}
		}

		/* check if the files are all available */
		iv = ima->views.last;
		while (iv) {
			int file;
			char str[FILE_MAX];

			BLI_strncpy(str, iv->filepath, sizeof(str));
			BLI_path_abs(str, G.main->name);

			/* exists? */
			file = BLI_open(str, O_BINARY | O_RDONLY, 0);
			if (file == -1) {
				ImageView *iv_del = iv;
				iv = iv->prev;
				BLI_remlink(&ima->views, iv_del);
				MEM_freeN(iv_del);
			}
			else {
				iv = iv->prev;
				close(file);
			}
		}

		/* all good */
		if (!BKE_image_is_multiview(ima)) {
			BKE_image_free_views(ima);
		}
	}
}
