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

#ifdef _WIN32
#  define open _open
#  define close _close
#endif

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
#include "DNA_userdef_types.h"
#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_blenlib.h"
#include "BLI_threads.h"
#include "BLI_timecode.h"  /* for stamp timecode format */
#include "BLI_utildefines.h"

#include "BKE_bmfont.h"
#include "BKE_colortools.h"
#include "BKE_global.h"
#include "BKE_icons.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_packedFile.h"
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

#include "WM_api.h"

static SpinLock image_spin;

/* max int, to indicate we don't store sequences in ibuf */
#define IMA_NO_INDEX    0x7FEFEFEF

/* quick lookup: supports 1 million frames, thousand passes */
#define IMA_MAKE_INDEX(frame, index)    ((frame) << 10) + index
#define IMA_INDEX_FRAME(index)          (index >> 10)
/*
#define IMA_INDEX_PASS(index)           (index & ~1023)
*/

/* ******** IMAGE CACHE ************* */

typedef struct ImageCacheKey {
	int index;
} ImageCacheKey;

static unsigned int imagecache_hashhash(const void *key_v)
{
	const ImageCacheKey *key = (ImageCacheKey *) key_v;
	return key->index;
}

static int imagecache_hashcmp(const void *a_v, const void *b_v)
{
	const ImageCacheKey *a = (ImageCacheKey *) a_v;
	const ImageCacheKey *b = (ImageCacheKey *) b_v;

	return a->index - b->index;
}

static void imagecache_put(Image *image, int index, ImBuf *ibuf)
{
	ImageCacheKey key;

	if (image->cache == NULL) {
		// char cache_name[64];
		// BLI_snprintf(cache_name, sizeof(cache_name), "Image Datablock %s", image->id.name);

		image->cache = IMB_moviecache_create("Image Datablock Cache", sizeof(ImageCacheKey),
		                                     imagecache_hashhash, imagecache_hashcmp);
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

static void image_free_cahced_frames(Image *image)
{
	if (image->cache) {
		IMB_moviecache_free(image->cache);
		image->cache = NULL;
	}
}

static void image_free_buffers(Image *ima)
{
	image_free_cahced_frames(ima);

	if (ima->anim) IMB_free_anim(ima->anim);
	ima->anim = NULL;

	if (ima->rr) {
		RE_FreeRenderResult(ima->rr);
		ima->rr = NULL;
	}

	GPU_free_image(ima);

	ima->ok = IMA_OK;
}

/* called by library too, do not free ima itself */
void BKE_image_free(Image *ima)
{
	int a;

	image_free_buffers(ima);
	if (ima->packedfile) {
		freePackedFile(ima->packedfile);
		ima->packedfile = NULL;
	}
	BKE_icon_delete(&ima->id);
	ima->id.icon_id = 0;

	BKE_previewimg_free(&ima->preview);

	for (a = 0; a < IMA_MAX_RENDER_SLOT; a++) {
		if (ima->renders[a]) {
			RE_FreeRenderResult(ima->renders[a]);
			ima->renders[a] = NULL;
		}
	}
}

/* only image block itself */
static Image *image_alloc(Main *bmain, const char *name, short source, short type)
{
	Image *ima;

	ima = BKE_libblock_alloc(bmain, ID_IM, name);
	if (ima) {
		ima->ok = IMA_OK;

		ima->xrep = ima->yrep = 1;
		ima->aspx = ima->aspy = 1.0;
		ima->gen_x = 1024; ima->gen_y = 1024;
		ima->gen_type = 1;   /* no defines yet? */

		ima->source = source;
		ima->type = type;

		if (source == IMA_SRC_VIEWER)
			ima->flag |= IMA_VIEW_AS_RENDER;

		BKE_color_managed_colorspace_settings_init(&ima->colorspace_settings);
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

/* empty image block, of similar type and filename */
Image *BKE_image_copy(Main *bmain, Image *ima)
{
	Image *nima = image_alloc(bmain, ima->id.name + 2, ima->source, ima->type);

	BLI_strncpy(nima->name, ima->name, sizeof(ima->name));

	nima->flag = ima->flag;
	nima->tpageflag = ima->tpageflag;

	nima->gen_x = ima->gen_x;
	nima->gen_y = ima->gen_y;
	nima->gen_type = ima->gen_type;

	nima->animspeed = ima->animspeed;

	nima->aspx = ima->aspx;
	nima->aspy = ima->aspy;

	BKE_color_managed_colorspace_settings_copy(&nima->colorspace_settings, &ima->colorspace_settings);

	if (ima->packedfile)
		nima->packedfile = dupPackedFile(ima->packedfile);

	return nima;
}

static void extern_local_image(Image *UNUSED(ima))
{
	/* Nothing to do: images don't link to other IDs. This function exists to
	 * match id_make_local pattern. */
}

void BKE_image_make_local(struct Image *ima)
{
	Main *bmain = G.main;
	Tex *tex;
	Brush *brush;
	Mesh *me;
	int is_local = FALSE, is_lib = FALSE;

	/* - only lib users: do nothing
	 * - only local users: set flag
	 * - mixed: make copy
	 */

	if (ima->id.lib == NULL) return;

	/* Can't take short cut here: must check meshes at least because of bogus
	 * texface ID refs. - z0r */
#if 0
	if (ima->id.us == 1) {
		id_clear_lib_data(bmain, &ima->id);
		extern_local_image(ima);
		return;
	}
#endif

	for (tex = bmain->tex.first; tex; tex = tex->id.next) {
		if (tex->ima == ima) {
			if (tex->id.lib) is_lib = TRUE;
			else is_local = TRUE;
		}
	}
	for (brush = bmain->brush.first; brush; brush = brush->id.next) {
		if (brush->clone.image == ima) {
			if (brush->id.lib) is_lib = TRUE;
			else is_local = TRUE;
		}
	}
	for (me = bmain->mesh.first; me; me = me->id.next) {
		if (me->mtface) {
			MTFace *tface;
			int a, i;

			for (i = 0; i < me->fdata.totlayer; i++) {
				if (me->fdata.layers[i].type == CD_MTFACE) {
					tface = (MTFace *)me->fdata.layers[i].data;

					for (a = 0; a < me->totface; a++, tface++) {
						if (tface->tpage == ima) {
							if (me->id.lib) is_lib = TRUE;
							else is_local = TRUE;
						}
					}
				}
			}
		}

		if (me->mtpoly) {
			MTexPoly *mtpoly;
			int a, i;

			for (i = 0; i < me->pdata.totlayer; i++) {
				if (me->pdata.layers[i].type == CD_MTEXPOLY) {
					mtpoly = (MTexPoly *)me->pdata.layers[i].data;

					for (a = 0; a < me->totpoly; a++, mtpoly++) {
						if (mtpoly->tpage == ima) {
							if (me->id.lib) is_lib = TRUE;
							else is_local = TRUE;
						}
					}
				}
			}
		}

	}

	if (is_local && is_lib == FALSE) {
		id_clear_lib_data(bmain, &ima->id);
		extern_local_image(ima);
	}
	else if (is_local && is_lib) {
		Image *ima_new = BKE_image_copy(bmain, ima);

		ima_new->id.us = 0;

		/* Remap paths of new ID using old library as base. */
		BKE_id_lib_local_paths(bmain, ima->id.lib, &ima_new->id);

		tex = bmain->tex.first;
		while (tex) {
			if (tex->id.lib == NULL) {
				if (tex->ima == ima) {
					tex->ima = ima_new;
					ima_new->id.us++;
					ima->id.us--;
				}
			}
			tex = tex->id.next;
		}
		brush = bmain->brush.first;
		while (brush) {
			if (brush->id.lib == NULL) {
				if (brush->clone.image == ima) {
					brush->clone.image = ima_new;
					ima_new->id.us++;
					ima->id.us--;
				}
			}
			brush = brush->id.next;
		}
		/* Transfer references in texfaces. Texfaces don't add to image ID
		 * user count *unless* there are no other users. See
		 * readfile.c:lib_link_mtface. */
		me = bmain->mesh.first;
		while (me) {
			if (me->mtface) {
				MTFace *tface;
				int a, i;

				for (i = 0; i < me->fdata.totlayer; i++) {
					if (me->fdata.layers[i].type == CD_MTFACE) {
						tface = (MTFace *)me->fdata.layers[i].data;

						for (a = 0; a < me->totface; a++, tface++) {
							if (tface->tpage == ima) {
								tface->tpage = ima_new;
								if (ima_new->id.us == 0) {
									tface->tpage->id.us = 1;
								}
								id_lib_extern((ID *)ima_new);
							}
						}
					}
				}
			}

			if (me->mtpoly) {
				MTexPoly *mtpoly;
				int a, i;

				for (i = 0; i < me->pdata.totlayer; i++) {
					if (me->pdata.layers[i].type == CD_MTEXPOLY) {
						mtpoly = (MTexPoly *)me->pdata.layers[i].data;

						for (a = 0; a < me->totpoly; a++, mtpoly++) {
							if (mtpoly->tpage == ima) {
								mtpoly->tpage = ima_new;
								if (ima_new->id.us == 0) {
									mtpoly->tpage->id.us = 1;
								}
								id_lib_extern((ID *)ima_new);
							}
						}
					}
				}
			}

			me = me->id.next;
		}
	}
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
int BKE_image_scale(Image *image, int width, int height)
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
	int file, len;
	const char *libname;
	char str[FILE_MAX];

	BLI_strncpy(str, filepath, sizeof(str));
	BLI_path_abs(str, bmain->name);

	/* exists? */
	file = BLI_open(str, O_BINARY | O_RDONLY, 0);
	if (file < 0)
		return NULL;
	close(file);

	/* create a short library name */
	len = strlen(filepath);

	while (len > 0 && filepath[len - 1] != '/' && filepath[len - 1] != '\\') len--;
	libname = filepath + len;

	ima = image_alloc(bmain, libname, IMA_SRC_FILE, IMA_TYPE_IMAGE);
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
Image *BKE_image_load_exists(const char *filepath)
{
	Image *ima;
	char str[FILE_MAX], strtest[FILE_MAX];

	BLI_strncpy(str, filepath, sizeof(str));
	BLI_path_abs(str, G.main->name);

	/* first search an identical image */
	for (ima = G.main->image.first; ima; ima = ima->id.next) {
		if (ima->source != IMA_SRC_VIEWER && ima->source != IMA_SRC_GENERATED) {
			BLI_strncpy(strtest, ima->name, sizeof(ima->name));
			BLI_path_abs(strtest, ID_BLEND_PATH(G.main, &ima->id));

			if (BLI_path_cmp(strtest, str) == 0) {
				if (ima->anim == NULL || ima->id.us == 0) {
					BLI_strncpy(ima->name, filepath, sizeof(ima->name));    /* for stringcode */
					ima->id.us++;                                       /* officially should not, it doesn't link here! */
					if (ima->ok == 0)
						ima->ok = IMA_OK;
					/* RETURN! */
					return ima;
				}
			}
		}
	}

	return BKE_image_load(G.main, filepath);
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

	if (rect_float) {
		/* both byte and float buffers are filling in sRGB space, need to linearize float buffer after BKE_image_buf_fill* functions */

		IMB_buffer_float_from_float(rect_float, rect_float, ibuf->channels, IB_PROFILE_LINEAR_RGB, IB_PROFILE_SRGB,
		                            TRUE, ibuf->x, ibuf->y, ibuf->x, ibuf->x);
	}

	return ibuf;
}

/* adds new image block, creates ImBuf and initializes color */
Image *BKE_image_add_generated(Main *bmain, unsigned int width, unsigned int height, const char *name, int depth, int floatbuf, short gen_type, const float color[4])
{
	/* on save, type is changed to FILE in editsima.c */
	Image *ima = image_alloc(bmain, name, IMA_SRC_GENERATED, IMA_TYPE_UV_TEST);

	if (ima) {
		ImBuf *ibuf;

		/* BLI_strncpy(ima->name, name, FILE_MAX); */ /* don't do this, this writes in ain invalid filepath! */
		ima->gen_x = width;
		ima->gen_y = height;
		ima->gen_type = gen_type;
		ima->gen_flag |= (floatbuf ? IMA_GEN_FLOAT : 0);
		ima->gen_depth = depth;

		ibuf = add_ibuf_size(width, height, ima->name, depth, floatbuf, gen_type, color, &ima->colorspace_settings);
		image_assign_ibuf(ima, ibuf, IMA_NO_INDEX, 0);

		/* image_assign_ibuf puts buffer to the cache, which increments user counter. */
		IMB_freeImBuf(ibuf);

		ima->ok = IMA_OK_LOADED;
	}

	return ima;
}

/* creates an image image owns the imbuf passed */
Image *BKE_image_add_from_imbuf(ImBuf *ibuf)
{
	/* on save, type is changed to FILE in editsima.c */
	Image *ima;

	ima = image_alloc(G.main, BLI_path_basename(ibuf->name), IMA_SRC_FILE, IMA_TYPE_IMAGE);

	if (ima) {
		BLI_strncpy(ima->name, ibuf->name, FILE_MAX);
		image_assign_ibuf(ima, ibuf, IMA_NO_INDEX, 0);
		ima->ok = IMA_OK_LOADED;
	}

	return ima;
}

/* packs rect from memory as PNG */
void BKE_image_memorypack(Image *ima)
{
	ImBuf *ibuf = image_get_cached_ibuf_for_index_frame(ima, IMA_NO_INDEX, 0);

	if (ibuf == NULL)
		return;
	if (ima->packedfile) {
		freePackedFile(ima->packedfile);
		ima->packedfile = NULL;
	}

	ibuf->ftype = PNG;
	ibuf->planes = R_IMF_PLANES_RGBA;

	IMB_saveiff(ibuf, ibuf->name, IB_rect | IB_mem);
	if (ibuf->encodedbuffer == NULL) {
		printf("memory save for pack error\n");
	}
	else {
		PackedFile *pf = MEM_callocN(sizeof(*pf), "PackedFile");

		pf->data = ibuf->encodedbuffer;
		pf->size = ibuf->encodedsize;
		ima->packedfile = pf;
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

void BKE_image_tag_time(Image *ima)
{
	if (ima)
		ima->lastused = (int)PIL_check_seconds_timer();
}

#if 0
static void tag_all_images_time()
{
	Image *ima;
	int ctime = (int)PIL_check_seconds_timer();

	ima = G.main->image.first;
	while (ima) {
		if (ima->bindcode || ima->repbind || ima->ibufs.first) {
			ima->lastused = ctime;
		}
	}
}
#endif

void free_old_images(void)
{
	Image *ima;
	static int lasttime = 0;
	int ctime = (int)PIL_check_seconds_timer();

	/*
	 * Run garbage collector once for every collecting period of time
	 * if textimeout is 0, that's the option to NOT run the collector
	 */
	if (U.textimeout == 0 || ctime % U.texcollectrate || ctime == lasttime)
		return;

	/* of course not! */
	if (G.is_rendering)
		return;

	lasttime = ctime;

	ima = G.main->image.first;
	while (ima) {
		if ((ima->flag & IMA_NOCOLLECT) == 0 && ctime - ima->lastused > U.textimeout) {
			/* If it's in GL memory, deallocate and set time tag to current time
			 * This gives textures a "second chance" to be used before dying. */
			if (ima->bindcode || ima->repbind) {
				GPU_free_image(ima);
				ima->lastused = ctime;
			}
			/* Otherwise, just kill the buffers */
			else {
				image_free_buffers(ima);
			}
		}
		ima = ima->id.next;
	}
}

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

			for (level = 0; level < IB_MIPMAP_LEVELS; level++) {
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
		ima->id.flag &= ~LIB_DOIT;

	for (tex = G.main->tex.first; tex; tex = tex->id.next)
		if (tex->ima)
			tex->ima->id.flag |= LIB_DOIT;

	for (ima = G.main->image.first; ima; ima = ima->id.next) {
		if (ima->cache && (ima->id.flag & LIB_DOIT)) {
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

int BKE_imtype_to_ftype(const char imtype)
{
	if (imtype == R_IMF_IMTYPE_TARGA)
		return TGA;
	else if (imtype == R_IMF_IMTYPE_RAWTGA)
		return RAWTGA;
	else if (imtype == R_IMF_IMTYPE_IRIS)
		return IMAGIC;
#ifdef WITH_HDR
	else if (imtype == R_IMF_IMTYPE_RADHDR)
		return RADHDR;
#endif
	else if (imtype == R_IMF_IMTYPE_PNG)
		return PNG | 15;
#ifdef WITH_DDS
	else if (imtype == R_IMF_IMTYPE_DDS)
		return DDS;
#endif
	else if (imtype == R_IMF_IMTYPE_BMP)
		return BMP;
#ifdef WITH_TIFF
	else if (imtype == R_IMF_IMTYPE_TIFF)
		return TIF;
#endif
	else if (imtype == R_IMF_IMTYPE_OPENEXR || imtype == R_IMF_IMTYPE_MULTILAYER)
		return OPENEXR;
#ifdef WITH_CINEON
	else if (imtype == R_IMF_IMTYPE_CINEON)
		return CINEON;
	else if (imtype == R_IMF_IMTYPE_DPX)
		return DPX;
#endif
#ifdef WITH_OPENJPEG
	else if (imtype == R_IMF_IMTYPE_JP2)
		return JP2;
#endif
	else
		return JPG | 90;
}

char BKE_ftype_to_imtype(const int ftype)
{
	if (ftype == 0)
		return R_IMF_IMTYPE_TARGA;
	else if (ftype == IMAGIC)
		return R_IMF_IMTYPE_IRIS;
#ifdef WITH_HDR
	else if (ftype & RADHDR)
		return R_IMF_IMTYPE_RADHDR;
#endif
	else if (ftype & PNG)
		return R_IMF_IMTYPE_PNG;
#ifdef WITH_DDS
	else if (ftype & DDS)
		return R_IMF_IMTYPE_DDS;
#endif
	else if (ftype & BMP)
		return R_IMF_IMTYPE_BMP;
#ifdef WITH_TIFF
	else if (ftype & TIF)
		return R_IMF_IMTYPE_TIFF;
#endif
	else if (ftype & OPENEXR)
		return R_IMF_IMTYPE_OPENEXR;
#ifdef WITH_CINEON
	else if (ftype & CINEON)
		return R_IMF_IMTYPE_CINEON;
	else if (ftype & DPX)
		return R_IMF_IMTYPE_DPX;
#endif
	else if (ftype & TGA)
		return R_IMF_IMTYPE_TARGA;
	else if (ftype & RAWTGA)
		return R_IMF_IMTYPE_RAWTGA;
#ifdef WITH_OPENJPEG
	else if (ftype & JP2)
		return R_IMF_IMTYPE_JP2;
#endif
	else
		return R_IMF_IMTYPE_JPEG90;
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
			return TRUE;
	}
	return 0;
}

char BKE_imtype_valid_channels(const char imtype)
{
	char chan_flag = IMA_CHAN_FLAG_RGB; /* assume all support rgb */

	/* alpha */
	switch (imtype) {
		case R_IMF_IMTYPE_TARGA:
		case R_IMF_IMTYPE_IRIS:
		case R_IMF_IMTYPE_PNG:
		/* case R_IMF_IMTYPE_BMP: */ /* read but not write */
		case R_IMF_IMTYPE_RADHDR:
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
			return R_IMF_CHAN_DEPTH_32;
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
 * creator.c help info */
char BKE_imtype_from_arg(const char *imtype_arg)
{
	if      (!strcmp(imtype_arg, "TGA")) return R_IMF_IMTYPE_TARGA;
	else if (!strcmp(imtype_arg, "IRIS")) return R_IMF_IMTYPE_IRIS;
#ifdef WITH_DDS
	else if (!strcmp(imtype_arg, "DDS")) return R_IMF_IMTYPE_DDS;
#endif
	else if (!strcmp(imtype_arg, "JPEG")) return R_IMF_IMTYPE_JPEG90;
	else if (!strcmp(imtype_arg, "IRIZ")) return R_IMF_IMTYPE_IRIZ;
	else if (!strcmp(imtype_arg, "RAWTGA")) return R_IMF_IMTYPE_RAWTGA;
	else if (!strcmp(imtype_arg, "AVIRAW")) return R_IMF_IMTYPE_AVIRAW;
	else if (!strcmp(imtype_arg, "AVIJPEG")) return R_IMF_IMTYPE_AVIJPEG;
	else if (!strcmp(imtype_arg, "PNG")) return R_IMF_IMTYPE_PNG;
	else if (!strcmp(imtype_arg, "QUICKTIME")) return R_IMF_IMTYPE_QUICKTIME;
	else if (!strcmp(imtype_arg, "BMP")) return R_IMF_IMTYPE_BMP;
#ifdef WITH_HDR
	else if (!strcmp(imtype_arg, "HDR")) return R_IMF_IMTYPE_RADHDR;
#endif
#ifdef WITH_TIFF
	else if (!strcmp(imtype_arg, "TIFF")) return R_IMF_IMTYPE_TIFF;
#endif
#ifdef WITH_OPENEXR
	else if (!strcmp(imtype_arg, "EXR")) return R_IMF_IMTYPE_OPENEXR;
	else if (!strcmp(imtype_arg, "MULTILAYER")) return R_IMF_IMTYPE_MULTILAYER;
#endif
	else if (!strcmp(imtype_arg, "MPEG")) return R_IMF_IMTYPE_FFMPEG;
	else if (!strcmp(imtype_arg, "FRAMESERVER")) return R_IMF_IMTYPE_FRAMESERVER;
#ifdef WITH_CINEON
	else if (!strcmp(imtype_arg, "CINEON")) return R_IMF_IMTYPE_CINEON;
	else if (!strcmp(imtype_arg, "DPX")) return R_IMF_IMTYPE_DPX;
#endif
#ifdef WITH_OPENJPEG
	else if (!strcmp(imtype_arg, "JP2")) return R_IMF_IMTYPE_JP2;
#endif
	else return R_IMF_IMTYPE_INVALID;
}

static int do_add_image_extension(char *string, const char imtype, const ImageFormatData *im_format)
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
	else if (ELEM5(imtype, R_IMF_IMTYPE_PNG, R_IMF_IMTYPE_FFMPEG, R_IMF_IMTYPE_H264, R_IMF_IMTYPE_THEORA, R_IMF_IMTYPE_XVID)) {
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
	else if (ELEM(imtype, R_IMF_IMTYPE_OPENEXR, R_IMF_IMTYPE_MULTILAYER)) {
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
		return FALSE;
	}
}

int BKE_add_image_extension(char *string, const ImageFormatData *im_format)
{
	return do_add_image_extension(string, im_format->imtype, im_format);
}

int BKE_add_image_extension_from_type(char *string, const char imtype)
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
	int ftype        = imbuf->ftype & ~IB_CUSTOM_FLAGS_MASK;
	int custom_flags = imbuf->ftype & IB_CUSTOM_FLAGS_MASK;

	BKE_imformat_defaults(im_format);

	/* file type */

	if (ftype == IMAGIC)
		im_format->imtype = R_IMF_IMTYPE_IRIS;

#ifdef WITH_HDR
	else if (ftype == RADHDR)
		im_format->imtype = R_IMF_IMTYPE_RADHDR;
#endif

	else if (ftype == PNG) {
		im_format->imtype = R_IMF_IMTYPE_PNG;

		if (custom_flags & PNG_16BIT)
			im_format->depth = R_IMF_CHAN_DEPTH_16;
	}

#ifdef WITH_DDS
	else if (ftype == DDS)
		im_format->imtype = R_IMF_IMTYPE_DDS;
#endif

	else if (ftype == BMP)
		im_format->imtype = R_IMF_IMTYPE_BMP;

#ifdef WITH_TIFF
	else if (ftype == TIF) {
		im_format->imtype = R_IMF_IMTYPE_TIFF;
		if (custom_flags & TIF_16BIT)
			im_format->depth = R_IMF_CHAN_DEPTH_16;
	}
#endif

#ifdef WITH_OPENEXR
	else if (ftype == OPENEXR) {
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
	else if (ftype == CINEON)
		im_format->imtype = R_IMF_IMTYPE_CINEON;
	else if (ftype == DPX)
		im_format->imtype = R_IMF_IMTYPE_DPX;
#endif

	else if (ftype == TGA) {
		im_format->imtype = R_IMF_IMTYPE_TARGA;
	}
	else if (ftype == RAWTGA) {
		im_format->imtype = R_IMF_IMTYPE_RAWTGA;
	}

#ifdef WITH_OPENJPEG
	else if (ftype & JP2) {
		im_format->imtype = R_IMF_IMTYPE_JP2;
		im_format->quality = custom_flags & ~JPG_MSK;

		if (ftype & JP2_16BIT)
			im_format->depth = R_IMF_CHAN_DEPTH_16;
		else if (ftype & JP2_12BIT)
			im_format->depth = R_IMF_CHAN_DEPTH_12;

		if (ftype & JP2_YCC)
			im_format->jp2_flag |= R_IMF_JP2_FLAG_YCC;

		if (ftype & JP2_CINE) {
			im_format->jp2_flag |= R_IMF_JP2_FLAG_CINE_PRESET;
			if (ftype & JP2_CINE_48FPS)
				im_format->jp2_flag |= R_IMF_JP2_FLAG_CINE_48;
		}

		if (ftype & JP2_JP2)
			im_format->jp2_codec = R_IMF_JP2_CODEC_JP2;
		else if (ftype & JP2_J2K)
			im_format->jp2_codec = R_IMF_JP2_CODEC_J2K;
		else
			BLI_assert(!"Unsupported jp2 codec was specified in file type");
	}
#endif

	else {
		im_format->imtype = R_IMF_IMTYPE_JPEG90;
		im_format->quality = custom_flags & ~JPG_MSK;
	}

	/* planes */
	/* TODO(sergey): Channels doesn't correspond actual planes used for image buffer
	 *               For example byte buffer will have 4 channels but it might easily
	 *               be BW or RGB image.
	 *
	 *               Need to use im_format->planes = imbuf->planes instead?
	 */
	switch (imbuf->channels) {
		case 0:
		case 4: im_format->planes = R_IMF_PLANES_RGBA;
			break;
		case 3: im_format->planes = R_IMF_PLANES_RGB;
			break;
		case 1: im_format->planes = R_IMF_PLANES_BW;
			break;
		default: im_format->planes = R_IMF_PLANES_RGB;
			break;
	}

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
		char *name = BKE_scene_find_last_marker_name(scene, CFRA);

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
			BLI_timestr(stats->lastframetime, text, sizeof(text));

			BLI_snprintf(stamp_data->rendertime, sizeof(stamp_data->rendertime), do_prefix ? "RenderTime %s" : "%s", text);
		}
		else {
			stamp_data->rendertime[0] = '\0';
		}
	}
}

void BKE_stamp_buf(Scene *scene, Object *camera, unsigned char *rect, float *rectf, int width, int height, int channels)
{
	struct StampData stamp_data;
	float w, h, pad;
	int x, y, y_ofs;
	float h_fixed;
	const int mono = blf_mono_font_render; // XXX
	struct ColorManagedDisplay *display;
	const char *display_device;

	/* this could be an argument if we want to operate on non linear float imbuf's
	 * for now though this is only used for renders which use scene settings */

#define TEXT_SIZE_CHECK(str, w, h) \
	((str[0]) && ((void)(h = h_fixed), (w = BLF_width(mono, str, sizeof(str)))))

#define BUFF_MARGIN_X 2
#define BUFF_MARGIN_Y 1

	if (!rect && !rectf)
		return;

	display_device = scene->display_settings.display_device;
	display = IMB_colormanagement_display_get_named(display_device);

	stampdata(scene, camera, &stamp_data, 1);

	/* TODO, do_versions */
	if (scene->r.stamp_font_id < 8)
		scene->r.stamp_font_id = 12;

	/* set before return */
	BLF_size(mono, scene->r.stamp_font_id, 72);

	BLF_buffer(mono, rectf, rect, width, height, channels, display);
	BLF_buffer_col(mono, scene->r.fg_stamp[0], scene->r.fg_stamp[1], scene->r.fg_stamp[2], 1.0);
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
		BLF_draw_buffer(mono, stamp_data.file);

		/* the extra pixel for background. */
		y -= BUFF_MARGIN_Y * 2;
	}

	/* Top left corner, below File */
	if (TEXT_SIZE_CHECK(stamp_data.note, w, h)) {
		y -= h;

		/* and space for background. */
		buf_rectfill_area(rect, rectf, width, height, scene->r.bg_stamp, display,
		                  0, y - BUFF_MARGIN_Y, w + BUFF_MARGIN_X, y + h + BUFF_MARGIN_Y);

		BLF_position(mono, x, y + y_ofs, 0.0);
		BLF_draw_buffer(mono, stamp_data.note);

		/* the extra pixel for background. */
		y -= BUFF_MARGIN_Y * 2;
	}

	/* Top left corner, below File (or Note) */
	if (TEXT_SIZE_CHECK(stamp_data.date, w, h)) {
		y -= h;

		/* and space for background. */
		buf_rectfill_area(rect, rectf, width, height, scene->r.bg_stamp, display,
		                  0, y - BUFF_MARGIN_Y, w + BUFF_MARGIN_X, y + h + BUFF_MARGIN_Y);

		BLF_position(mono, x, y + y_ofs, 0.0);
		BLF_draw_buffer(mono, stamp_data.date);

		/* the extra pixel for background. */
		y -= BUFF_MARGIN_Y * 2;
	}

	/* Top left corner, below File, Date or Note */
	if (TEXT_SIZE_CHECK(stamp_data.rendertime, w, h)) {
		y -= h;

		/* and space for background. */
		buf_rectfill_area(rect, rectf, width, height, scene->r.bg_stamp, display,
		                  0, y - BUFF_MARGIN_Y, w + BUFF_MARGIN_X, y + h + BUFF_MARGIN_Y);

		BLF_position(mono, x, y + y_ofs, 0.0);
		BLF_draw_buffer(mono, stamp_data.rendertime);
	}

	x = 0;
	y = 0;

	/* Bottom left corner, leaving space for timing */
	if (TEXT_SIZE_CHECK(stamp_data.marker, w, h)) {

		/* extra space for background. */
		buf_rectfill_area(rect, rectf, width, height, scene->r.bg_stamp,  display,
		                  x - BUFF_MARGIN_X, y - BUFF_MARGIN_Y, w + BUFF_MARGIN_X, y + h + BUFF_MARGIN_Y);

		/* and pad the text. */
		BLF_position(mono, x, y + y_ofs, 0.0);
		BLF_draw_buffer(mono, stamp_data.marker);

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
		BLF_draw_buffer(mono, stamp_data.time);

		/* space width. */
		x += w + pad;
	}

	if (TEXT_SIZE_CHECK(stamp_data.frame, w, h)) {

		/* extra space for background. */
		buf_rectfill_area(rect, rectf, width, height, scene->r.bg_stamp, display,
		                  x - BUFF_MARGIN_X, y - BUFF_MARGIN_Y, x + w + BUFF_MARGIN_X, y + h + BUFF_MARGIN_Y);

		/* and pad the text. */
		BLF_position(mono, x, y + y_ofs, 0.0);
		BLF_draw_buffer(mono, stamp_data.frame);

		/* space width. */
		x += w + pad;
	}

	if (TEXT_SIZE_CHECK(stamp_data.camera, w, h)) {

		/* extra space for background. */
		buf_rectfill_area(rect, rectf, width, height, scene->r.bg_stamp, display,
		                  x - BUFF_MARGIN_X, y - BUFF_MARGIN_Y, x + w + BUFF_MARGIN_X, y + h + BUFF_MARGIN_Y);
		BLF_position(mono, x, y + y_ofs, 0.0);
		BLF_draw_buffer(mono, stamp_data.camera);

		/* space width. */
		x += w + pad;
	}

	if (TEXT_SIZE_CHECK(stamp_data.cameralens, w, h)) {

		/* extra space for background. */
		buf_rectfill_area(rect, rectf, width, height, scene->r.bg_stamp, display,
		                  x - BUFF_MARGIN_X, y - BUFF_MARGIN_Y, x + w + BUFF_MARGIN_X, y + h + BUFF_MARGIN_Y);
		BLF_position(mono, x, y + y_ofs, 0.0);
		BLF_draw_buffer(mono, stamp_data.cameralens);
	}

	if (TEXT_SIZE_CHECK(stamp_data.scene, w, h)) {

		/* Bottom right corner, with an extra space because blenfont is too strict! */
		x = width - w - 2;

		/* extra space for background. */
		buf_rectfill_area(rect, rectf, width, height, scene->r.bg_stamp, display,
		                  x - BUFF_MARGIN_X, y - BUFF_MARGIN_Y, x + w + BUFF_MARGIN_X, y + h + BUFF_MARGIN_Y);

		/* and pad the text. */
		BLF_position(mono, x, y + y_ofs, 0.0);
		BLF_draw_buffer(mono, stamp_data.scene);
	}

	if (TEXT_SIZE_CHECK(stamp_data.strip, w, h)) {

		/* Top right corner, with an extra space because blenfont is too strict! */
		x = width - w - pad;
		y = height - h;

		/* extra space for background. */
		buf_rectfill_area(rect, rectf, width, height, scene->r.bg_stamp, display,
		                  x - BUFF_MARGIN_X, y - BUFF_MARGIN_Y, x + w + BUFF_MARGIN_X, y + h + BUFF_MARGIN_Y);

		BLF_position(mono, x, y + y_ofs, 0.0);
		BLF_draw_buffer(mono, stamp_data.strip);
	}

	/* cleanup the buffer. */
	BLF_buffer(mono, NULL, NULL, 0, 0, 0, NULL);

#undef TEXT_SIZE_CHECK
#undef BUFF_MARGIN_X
#undef BUFF_MARGIN_Y
}

void BKE_imbuf_stamp_info(Scene *scene, Object *camera, struct ImBuf *ibuf)
{
	struct StampData stamp_data;

	if (!ibuf) return;

	/* fill all the data values, no prefix */
	stampdata(scene, camera, &stamp_data, 0);

	if (stamp_data.file[0]) IMB_metadata_change_field(ibuf, "File",        stamp_data.file);
	if (stamp_data.note[0]) IMB_metadata_change_field(ibuf, "Note",        stamp_data.note);
	if (stamp_data.date[0]) IMB_metadata_change_field(ibuf, "Date",        stamp_data.date);
	if (stamp_data.marker[0]) IMB_metadata_change_field(ibuf, "Marker",    stamp_data.marker);
	if (stamp_data.time[0]) IMB_metadata_change_field(ibuf, "Time",        stamp_data.time);
	if (stamp_data.frame[0]) IMB_metadata_change_field(ibuf, "Frame",      stamp_data.frame);
	if (stamp_data.camera[0]) IMB_metadata_change_field(ibuf, "Camera",    stamp_data.camera);
	if (stamp_data.cameralens[0]) IMB_metadata_change_field(ibuf, "Lens",  stamp_data.cameralens);
	if (stamp_data.scene[0]) IMB_metadata_change_field(ibuf, "Scene",      stamp_data.scene);
	if (stamp_data.strip[0]) IMB_metadata_change_field(ibuf, "Strip",      stamp_data.strip);
	if (stamp_data.rendertime[0]) IMB_metadata_change_field(ibuf, "RenderTime", stamp_data.rendertime);
}

bool BKE_imbuf_alpha_test(ImBuf *ibuf)
{
	int tot;
	if (ibuf->rect_float) {
		float *buf = ibuf->rect_float;
		for (tot = ibuf->x * ibuf->y; tot--; buf += 4) {
			if (buf[3] < 1.0f) {
				return TRUE;
			}
		}
	}
	else if (ibuf->rect) {
		unsigned char *buf = (unsigned char *)ibuf->rect;
		for (tot = ibuf->x * ibuf->y; tot--; buf += 4) {
			if (buf[3] != 255) {
				return TRUE;
			}
		}
	}

	return FALSE;
}

/* note: imf->planes is ignored here, its assumed the image channels
 * are already set */
int BKE_imbuf_write(ImBuf *ibuf, const char *name, ImageFormatData *imf)
{
	char imtype = imf->imtype;
	char compress = imf->compress;
	char quality = imf->quality;

	int ok;

	if (imtype == R_IMF_IMTYPE_IRIS) {
		ibuf->ftype = IMAGIC;
	}
#ifdef WITH_HDR
	else if (imtype == R_IMF_IMTYPE_RADHDR) {
		ibuf->ftype = RADHDR;
	}
#endif
	else if (ELEM5(imtype, R_IMF_IMTYPE_PNG, R_IMF_IMTYPE_FFMPEG, R_IMF_IMTYPE_H264, R_IMF_IMTYPE_THEORA, R_IMF_IMTYPE_XVID)) {
		ibuf->ftype = PNG;

		if (imtype == R_IMF_IMTYPE_PNG) {
			if (imf->depth == R_IMF_CHAN_DEPTH_16)
				ibuf->ftype |= PNG_16BIT;

			ibuf->ftype |= compress;
		}

	}
#ifdef WITH_DDS
	else if (imtype == R_IMF_IMTYPE_DDS) {
		ibuf->ftype = DDS;
	}
#endif
	else if (imtype == R_IMF_IMTYPE_BMP) {
		ibuf->ftype = BMP;
	}
#ifdef WITH_TIFF
	else if (imtype == R_IMF_IMTYPE_TIFF) {
		ibuf->ftype = TIF;

		if (imf->depth == R_IMF_CHAN_DEPTH_16)
			ibuf->ftype |= TIF_16BIT;
	}
#endif
#ifdef WITH_OPENEXR
	else if (imtype == R_IMF_IMTYPE_OPENEXR || imtype == R_IMF_IMTYPE_MULTILAYER) {
		ibuf->ftype = OPENEXR;
		if (imf->depth == R_IMF_CHAN_DEPTH_16)
			ibuf->ftype |= OPENEXR_HALF;
		ibuf->ftype |= (imf->exr_codec & OPENEXR_COMPRESS);

		if (!(imf->flag & R_IMF_FLAG_ZBUF))
			ibuf->zbuf_float = NULL;    /* signal for exr saving */

	}
#endif
#ifdef WITH_CINEON
	else if (imtype == R_IMF_IMTYPE_CINEON) {
		ibuf->ftype = CINEON;
		if (imf->cineon_flag & R_IMF_CINEON_FLAG_LOG) {
			ibuf->ftype |= CINEON_LOG;
		}
		if (imf->depth == R_IMF_CHAN_DEPTH_16) {
			ibuf->ftype |= CINEON_16BIT;
		}
		else if (imf->depth == R_IMF_CHAN_DEPTH_12) {
			ibuf->ftype |= CINEON_12BIT;
		}
		else if (imf->depth == R_IMF_CHAN_DEPTH_10) {
			ibuf->ftype |= CINEON_10BIT;
		}
	}
	else if (imtype == R_IMF_IMTYPE_DPX) {
		ibuf->ftype = DPX;
		if (imf->cineon_flag & R_IMF_CINEON_FLAG_LOG) {
			ibuf->ftype |= CINEON_LOG;
		}
		if (imf->depth == R_IMF_CHAN_DEPTH_16) {
			ibuf->ftype |= CINEON_16BIT;
		}
		else if (imf->depth == R_IMF_CHAN_DEPTH_12) {
			ibuf->ftype |= CINEON_12BIT;
		}
		else if (imf->depth == R_IMF_CHAN_DEPTH_10) {
			ibuf->ftype |= CINEON_10BIT;
		}
	}
#endif
	else if (imtype == R_IMF_IMTYPE_TARGA) {
		ibuf->ftype = TGA;
	}
	else if (imtype == R_IMF_IMTYPE_RAWTGA) {
		ibuf->ftype = RAWTGA;
	}
#ifdef WITH_OPENJPEG
	else if (imtype == R_IMF_IMTYPE_JP2) {
		if (quality < 10) quality = 90;
		ibuf->ftype = JP2 | quality;

		if (imf->depth == R_IMF_CHAN_DEPTH_16) {
			ibuf->ftype |= JP2_16BIT;
		}
		else if (imf->depth == R_IMF_CHAN_DEPTH_12) {
			ibuf->ftype |= JP2_12BIT;
		}

		if (imf->jp2_flag & R_IMF_JP2_FLAG_YCC) {
			ibuf->ftype |= JP2_YCC;
		}

		if (imf->jp2_flag & R_IMF_JP2_FLAG_CINE_PRESET) {
			ibuf->ftype |= JP2_CINE;
			if (imf->jp2_flag & R_IMF_JP2_FLAG_CINE_48)
				ibuf->ftype |= JP2_CINE_48FPS;
		}

		if (imf->jp2_codec == R_IMF_JP2_CODEC_JP2)
			ibuf->ftype |= JP2_JP2;
		else if (imf->jp2_codec == R_IMF_JP2_CODEC_J2K)
			ibuf->ftype |= JP2_J2K;
		else
			BLI_assert(!"Unsupported jp2 codec was specified in im_format->jp2_codec");
	}
#endif
	else {
		/* R_IMF_IMTYPE_JPEG90, etc. default we save jpegs */
		if (quality < 10) quality = 90;
		ibuf->ftype = JPG | quality;
	}

	BLI_make_existing_file(name);

	ok = IMB_saveiff(ibuf, name, IB_rect | IB_zbuf | IB_zbuffloat);
	if (ok == 0) {
		perror(name);
	}

	return(ok);
}

/* same as BKE_imbuf_write() but crappy workaround not to perminantly modify
 * _some_, values in the imbuf */
int BKE_imbuf_write_as(ImBuf *ibuf, const char *name, ImageFormatData *imf,
                       const short save_copy)
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
	}

	return ok;
}

int BKE_imbuf_write_stamp(Scene *scene, struct Object *camera, ImBuf *ibuf, const char *name, struct ImageFormatData *imf)
{
	if (scene && scene->r.stamp & R_STAMP_ALL)
		BKE_imbuf_stamp_info(scene, camera, ibuf);

	return BKE_imbuf_write(ibuf, name, imf);
}


static void do_makepicstring(char *string, const char *base, const char *relbase, int frame, const char imtype,
                             const ImageFormatData *im_format, const short use_ext, const short use_frames)
{
	if (string == NULL) return;
	BLI_strncpy(string, base, FILE_MAX - 10);   /* weak assumption */
	BLI_path_abs(string, relbase);

	if (use_frames)
		BLI_path_frame(string, frame, 4);

	if (use_ext)
		do_add_image_extension(string, imtype, im_format);
}

void BKE_makepicstring(char *string, const char *base, const char *relbase, int frame, const ImageFormatData *im_format, const short use_ext, const short use_frames)
{
	do_makepicstring(string, base, relbase, frame, im_format->imtype, im_format, use_ext, use_frames);
}

void BKE_makepicstring_from_type(char *string, const char *base, const char *relbase, int frame, const char imtype, const short use_ext, const short use_frames)
{
	do_makepicstring(string, base, relbase, frame, imtype, NULL, use_ext, use_frames);
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

void BKE_image_assign_ibuf(Image *ima, ImBuf *ibuf)
{
	image_assign_ibuf(ima, ibuf, IMA_NO_INDEX, 0);
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

void BKE_image_signal(Image *ima, ImageUser *iuser, int signal)
{
	if (ima == NULL)
		return;

	BLI_spin_lock(&image_spin);

	switch (signal) {
		case IMA_SIGNAL_FREE:
			image_free_buffers(ima);
			if (iuser)
				iuser->ok = 1;
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
				image_free_buffers(ima);
#else
			/* image buffers for non-sequence multilayer will share buffers with RenderResult,
			 * however sequence multilayer will own buffers. Such logic makes switching from
			 * single multilayer file to sequence completely unstable
			 * since changes in nodes seems this workaround isn't needed anymore, all sockets
			 * are nicely detecting anyway, but freeing buffers always here makes multilayer
			 * sequences behave stable
			 */
			image_free_buffers(ima);
#endif

			ima->ok = 1;
			if (iuser)
				iuser->ok = 1;

			BKE_image_walk_all_users(G.main, ima, image_tag_frame_recalc);

			break;

		case IMA_SIGNAL_RELOAD:
			/* try to repack file */
			if (ima->packedfile) {
				PackedFile *pf;
				pf = newPackedFile(NULL, ima->name, ID_BLEND_PATH(G.main, &ima->id));
				if (pf) {
					freePackedFile(ima->packedfile);
					ima->packedfile = pf;
					image_free_buffers(ima);
				}
				else {
					printf("ERROR: Image not available. Keeping packed image\n");
				}
			}
			else
				image_free_buffers(ima);

			if (iuser)
				iuser->ok = 1;

			break;
		case IMA_SIGNAL_USER_NEW_IMAGE:
			if (iuser) {
				iuser->ok = 1;
				if (ima->source == IMA_SRC_FILE || ima->source == IMA_SRC_SEQUENCE) {
					if (ima->type == IMA_TYPE_MULTILAYER) {
						iuser->multi_index = 0;
						iuser->layer = iuser->pass = 0;
					}
				}
			}
			break;
		case IMA_SIGNAL_COLORMANAGE:
			image_free_buffers(ima);

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
		short index = 0, rl_index = 0, rp_index;

		for (rl = rr->layers.first; rl; rl = rl->next, rl_index++) {
			rp_index = 0;
			for (rpass = rl->passes.first; rpass; rpass = rpass->next, index++, rp_index++)
				if (iuser->layer == rl_index && iuser->pass == rp_index)
					break;
			if (rpass)
				break;
		}

		if (rpass)
			iuser->multi_index = index;
		else
			iuser->multi_index = 0;
	}
	if (rpass == NULL) {
		rl = rr->layers.first;
		if (rl)
			rpass = rl->passes.first;
	}

	return rpass;
}

RenderResult *BKE_image_acquire_renderresult(Scene *scene, Image *ima)
{
	if (ima->rr) {
		return ima->rr;
	}
	else if (ima->type == IMA_TYPE_R_RESULT) {
		if (ima->render_slot == ima->last_render_slot)
			return RE_AcquireResultRead(RE_GetRender(scene->id.name));
		else
			return ima->renders[ima->render_slot];
	}
	else
		return NULL;
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

void BKE_image_backup_render(Scene *scene, Image *ima)
{
	/* called right before rendering, ima->renders contains render
	 * result pointers for everything but the current render */
	Render *re = RE_GetRender(scene->id.name);
	int slot = ima->render_slot, last = ima->last_render_slot;

	if (slot != last) {
		if (ima->renders[slot]) {
			RE_FreeRenderResult(ima->renders[slot]);
			ima->renders[slot] = NULL;
		}

		ima->renders[last] = NULL;
		RE_SwapResult(re, &ima->renders[last]);
	}

	ima->last_render_slot = slot;
}

/* after imbuf load, openexr type can return with a exrhandle open */
/* in that case we have to build a render-result */
static void image_create_multilayer(Image *ima, ImBuf *ibuf, int framenr)
{
	const char *colorspace = ima->colorspace_settings.name;
	int predivide = ima->alpha_mode == IMA_ALPHA_PREMUL;

	ima->rr = RE_MultilayerConvert(ibuf->userdata, colorspace, predivide, ibuf->x, ibuf->y);

#ifdef WITH_OPENEXR
	IMB_exr_close(ibuf->userdata);
#endif

	ibuf->userdata = NULL;
	if (ima->rr)
		ima->rr->framenr = framenr;
}

/* common stuff to do with images after loading */
static void image_initialize_after_load(Image *ima, ImBuf *ibuf)
{
	/* preview is NULL when it has never been used as an icon before */
	if (G.background == 0 && ima->preview == NULL)
		BKE_icon_changed(BKE_icon_getid(&ima->id));

	/* fields */
	if (ima->flag & IMA_FIELDS) {
		if (ima->flag & IMA_STD_FIELD) de_interlace_st(ibuf);
		else de_interlace_ng(ibuf);
	}
	/* timer */
	ima->lastused = clock() / CLOCKS_PER_SEC;

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

static ImBuf *image_load_sequence_file(Image *ima, ImageUser *iuser, int frame)
{
	struct ImBuf *ibuf;
	char name[FILE_MAX];
	int flag;

	/* XXX temp stuff? */
	if (ima->lastframe != frame)
		ima->tpageflag |= IMA_TPAGE_REFRESH;

	ima->lastframe = frame;
	BKE_image_user_file_path(iuser, ima, name);

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
		if (ibuf->ftype == OPENEXR && ibuf->userdata) {
			image_create_multilayer(ima, ibuf, frame);
			ima->type = IMA_TYPE_MULTILAYER;
			IMB_freeImBuf(ibuf);
			ibuf = NULL;
		}
		else {
			image_initialize_after_load(ima, ibuf);
			image_assign_ibuf(ima, ibuf, 0, frame);
		}
#else
		image_initialize_after_load(ima, ibuf);
		image_assign_ibuf(ima, ibuf, 0, frame);
#endif
	}
	else
		ima->ok = 0;

	if (iuser)
		iuser->ok = ima->ok;

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
			image_free_cahced_frames(ima);
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


static ImBuf *image_load_movie_file(Image *ima, ImageUser *iuser, int frame)
{
	struct ImBuf *ibuf = NULL;

	ima->lastframe = frame;

	if (ima->anim == NULL) {
		char str[FILE_MAX];

		BKE_image_user_file_path(iuser, ima, str);

		/* FIXME: make several stream accessible in image editor, too*/
		ima->anim = openanim(str, IB_rect, 0, ima->colorspace_settings.name);

		/* let's initialize this user */
		if (ima->anim && iuser && iuser->frames == 0)
			iuser->frames = IMB_anim_get_duration(ima->anim,
			                                      IMB_TC_RECORD_RUN);
	}

	if (ima->anim) {
		int dur = IMB_anim_get_duration(ima->anim,
		                                IMB_TC_RECORD_RUN);
		int fra = frame - 1;

		if (fra < 0) fra = 0;
		if (fra > (dur - 1)) fra = dur - 1;
		ibuf = IMB_makeSingleUser(
		    IMB_anim_absolute(ima->anim, fra,
		                      IMB_TC_RECORD_RUN,
		                      IMB_PROXY_NONE));

		if (ibuf) {
			image_initialize_after_load(ima, ibuf);
			image_assign_ibuf(ima, ibuf, 0, frame);
		}
		else
			ima->ok = 0;
	}
	else
		ima->ok = 0;

	if (iuser)
		iuser->ok = ima->ok;

	return ibuf;
}

/* warning, 'iuser' can be NULL */
static ImBuf *image_load_image_file(Image *ima, ImageUser *iuser, int cfra)
{
	struct ImBuf *ibuf;
	char str[FILE_MAX];
	int assign = 0, flag;

	/* always ensure clean ima */
	image_free_buffers(ima);

	/* is there a PackedFile with this image ? */
	if (ima->packedfile) {
		flag = IB_rect | IB_multilayer;
		flag |= imbuf_alpha_flags_for_image(ima);

		ibuf = IMB_ibImageFromMemory((unsigned char *)ima->packedfile->data, ima->packedfile->size, flag,
		                             ima->colorspace_settings.name, "<packed data>");
	}
	else {
		flag = IB_rect | IB_multilayer | IB_metadata;
		flag |= imbuf_alpha_flags_for_image(ima);

		/* get the right string */
		BKE_image_user_frame_calc(iuser, cfra, 0);
		BKE_image_user_file_path(iuser, ima, str);

		/* read ibuf */
		ibuf = IMB_loadiffname(str, flag, ima->colorspace_settings.name);
	}

	if (ibuf) {
		/* handle multilayer case, don't assign ibuf. will be handled in BKE_image_acquire_ibuf */
		if (ibuf->ftype == OPENEXR && ibuf->userdata) {
			image_create_multilayer(ima, ibuf, cfra);
			ima->type = IMA_TYPE_MULTILAYER;
			IMB_freeImBuf(ibuf);
			ibuf = NULL;
		}
		else {
			image_initialize_after_load(ima, ibuf);
			assign = 1;

			/* check if the image is a font image... */
			detectBitmapFont(ibuf);

			/* make packed file for autopack */
			if ((ima->packedfile == NULL) && (G.fileflags & G_AUTOPACK))
				ima->packedfile = newPackedFile(NULL, str, ID_BLEND_PATH(G.main, &ima->id));
		}
	}
	else
		ima->ok = 0;

	if (assign)
		image_assign_ibuf(ima, ibuf, IMA_NO_INDEX, 0);

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
static ImBuf *image_get_render_result(Image *ima, ImageUser *iuser, void **lock_r)
{
	Render *re;
	RenderResult rres;
	float *rectf, *rectz;
	unsigned int *rect;
	float dither;
	int channels, layer, pass;
	ImBuf *ibuf;
	int from_render = (ima->render_slot == ima->last_render_slot);
	bool byte_buffer_in_display_space = false;

	if (!(iuser && iuser->scene))
		return NULL;

	/* if we the caller is not going to release the lock, don't give the image */
	if (!lock_r)
		return NULL;

	re = RE_GetRender(iuser->scene->id.name);

	channels = 4;
	layer = iuser->layer;
	pass = iuser->pass;

	if (from_render) {
		RE_AcquireResultImage(re, &rres);
	}
	else if (ima->renders[ima->render_slot]) {
		rres = *(ima->renders[ima->render_slot]);
		rres.have_combined = rres.rectf != NULL;
	}
	else
		memset(&rres, 0, sizeof(RenderResult));

	if (!(rres.rectx > 0 && rres.recty > 0)) {
		if (from_render)
			RE_ReleaseResultImage(re);
		return NULL;
	}

	/* release is done in BKE_image_release_ibuf using lock_r */
	if (from_render) {
		BLI_lock_thread(LOCK_VIEWER);
		*lock_r = re;
	}

	/* this gives active layer, composite or sequence result */
	rect = (unsigned int *)rres.rect32;
	rectf = rres.rectf;
	rectz = rres.rectz;
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
			RenderPass *rpass;

			/* there's no combined pass, is in renderlayer itself */
			if (pass == 0) {
				rectf = rl->rectf;
				if (rectf == NULL) {
					/* Happens when Save Buffers is enabled.
					 * Use display buffer stored in the render layer.
					 */
					rect = (unsigned int *) rl->display_buffer;
					byte_buffer_in_display_space = true;
				}
			}
			else {
				rpass = BLI_findlink(&rl->passes, pass - 1);
				if (rpass) {
					channels = rpass->channels;
					rectf = rpass->rect;
					dither = 0.0f; /* don't dither passes */
				}
			}

			for (rpass = rl->passes.first; rpass; rpass = rpass->next)
				if (rpass->passtype == SCE_PASS_Z)
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

static void image_get_frame_and_index(Image *ima, ImageUser *iuser, int *frame_r, int *index_r)
{
	int frame = 0, index = 0;

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
			index = iuser ? iuser->multi_index : IMA_NO_INDEX;
		}
	}

	*frame_r = frame;
	*index_r = index;
}

/* Get the ibuf from an image cache for a given image user.
 *
 * Returns referenced image buffer if it exists, callee is to
 * call IMB_freeImBuf to de-reference the image buffer after
 * it's done handling it.
 */
static ImBuf *image_get_cached_ibuf(Image *ima, ImageUser *iuser, int *frame_r, int *index_r)
{
	ImBuf *ibuf = NULL;
	int frame = 0, index = 0;

	/* see if we already have an appropriate ibuf, with image source and type */
	if (ima->source == IMA_SRC_MOVIE) {
		frame = iuser ? iuser->framenr : ima->lastframe;
		ibuf = image_get_cached_ibuf_for_index_frame(ima, 0, frame);
		/* XXX temp stuff? */
		if (ima->lastframe != frame)
			ima->tpageflag |= IMA_TPAGE_REFRESH;
		ima->lastframe = frame;
	}
	else if (ima->source == IMA_SRC_SEQUENCE) {
		if (ima->type == IMA_TYPE_IMAGE) {
			frame = iuser ? iuser->framenr : ima->lastframe;
			ibuf = image_get_cached_ibuf_for_index_frame(ima, 0, frame);

			/* XXX temp stuff? */
			if (ima->lastframe != frame) {
				ima->tpageflag |= IMA_TPAGE_REFRESH;
			}
			ima->lastframe = frame;
		}
		else if (ima->type == IMA_TYPE_MULTILAYER) {
			frame = iuser ? iuser->framenr : ima->lastframe;
			index = iuser ? iuser->multi_index : IMA_NO_INDEX;
			ibuf = image_get_cached_ibuf_for_index_frame(ima, index, frame);
		}
	}
	else if (ima->source == IMA_SRC_FILE) {
		if (ima->type == IMA_TYPE_IMAGE)
			ibuf = image_get_cached_ibuf_for_index_frame(ima, IMA_NO_INDEX, 0);
		else if (ima->type == IMA_TYPE_MULTILAYER)
			ibuf = image_get_cached_ibuf_for_index_frame(ima, iuser ? iuser->multi_index : IMA_NO_INDEX, 0);
	}
	else if (ima->source == IMA_SRC_GENERATED) {
		ibuf = image_get_cached_ibuf_for_index_frame(ima, IMA_NO_INDEX, 0);
	}
	else if (ima->source == IMA_SRC_VIEWER) {
		/* always verify entirely, not that this shouldn't happen
		 * as part of texture sampling in rendering anyway, so not
		 * a big bottleneck */
	}

	if (frame_r)
		*frame_r = frame;

	if (index_r)
		*index_r = index;

	return ibuf;
}

BLI_INLINE bool image_quick_test(Image *ima, ImageUser *iuser)
{
	if (ima == NULL)
		return FALSE;

	if (iuser) {
		if (iuser->ok == 0)
			return FALSE;
	}
	else if (ima->ok == 0)
		return FALSE;

	return TRUE;
}

/* Checks optional ImageUser and verifies/creates ImBuf.
 *
 * not thread-safe, so callee should worry about thread locks
 */
static ImBuf *image_acquire_ibuf(Image *ima, ImageUser *iuser, void **lock_r)
{
	ImBuf *ibuf = NULL;
	float color[] = {0, 0, 0, 1};
	int frame = 0, index = 0;

	if (lock_r)
		*lock_r = NULL;

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
			                     color, &ima->colorspace_settings);
			image_assign_ibuf(ima, ibuf, IMA_NO_INDEX, 0);
			ima->ok = IMA_OK_LOADED;
		}
		else if (ima->source == IMA_SRC_VIEWER) {
			if (ima->type == IMA_TYPE_R_RESULT) {
				/* always verify entirely, and potentially
				 * returns pointer to release later */
				ibuf = image_get_render_result(ima, iuser, lock_r);
				if (ibuf) {
					ibuf->userflags |= IB_PERSISTENT;
				}
			}
			else if (ima->type == IMA_TYPE_COMPOSITE) {
				/* requires lock/unlock, otherwise don't return image */
				if (lock_r) {
					/* unlock in BKE_image_release_ibuf */
					BLI_lock_thread(LOCK_VIEWER);
					*lock_r = ima;

					/* XXX anim play for viewer nodes not yet supported */
					frame = 0; // XXX iuser ? iuser->framenr : 0;
					ibuf = image_get_cached_ibuf_for_index_frame(ima, 0, frame);

					if (!ibuf) {
						/* Composite Viewer, all handled in compositor */
						/* fake ibuf, will be filled in compositor */
						ibuf = IMB_allocImBuf(256, 256, 32, IB_rect);
						image_assign_ibuf(ima, ibuf, 0, frame);
					}
					ibuf->userflags |= IB_PERSISTENT;
				}
			}
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
ImBuf *BKE_image_acquire_ibuf(Image *ima, ImageUser *iuser, void **lock_r)
{
	ImBuf *ibuf;

	BLI_spin_lock(&image_spin);

	ibuf = image_acquire_ibuf(ima, iuser, lock_r);

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
int BKE_image_has_ibuf(Image *ima, ImageUser *iuser)
{
	ImBuf *ibuf;

	/* quick reject tests */
	if (!image_quick_test(ima, iuser))
		return FALSE;

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
} ImagePool;

ImagePool *BKE_image_pool_new(void)
{
	ImagePool *pool = MEM_callocN(sizeof(ImagePool), "Image Pool");

	return pool;
}

void BKE_image_pool_free(ImagePool *pool)
{
	ImagePoolEntry *entry, *next_entry;

	/* use single lock to dereference all the image buffers */
	BLI_spin_lock(&image_spin);

	for (entry = pool->image_buffers.first; entry; entry = next_entry) {
		next_entry = entry->next;

		if (entry->ibuf)
			IMB_freeImBuf(entry->ibuf);

		MEM_freeN(entry);
	}

	BLI_spin_unlock(&image_spin);

	MEM_freeN(pool);
}

BLI_INLINE ImBuf *image_pool_find_entry(ImagePool *pool, Image *image, int frame, int index, int *found)
{
	ImagePoolEntry *entry;

	*found = FALSE;

	for (entry = pool->image_buffers.first; entry; entry = entry->next) {
		if (entry->image == image && entry->frame == frame && entry->index == index) {
			*found = TRUE;
			return entry->ibuf;
		}
	}

	return NULL;
}

ImBuf *BKE_image_pool_acquire_ibuf(Image *ima, ImageUser *iuser, ImagePool *pool)
{
	ImBuf *ibuf;
	int index, frame, found;

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

		entry = MEM_callocN(sizeof(ImagePoolEntry), "Image Pool Entry");
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

int BKE_image_user_frame_get(const ImageUser *iuser, int cfra, int fieldnr, short *r_is_in_range)
{
	const int len = (iuser->fie_ima * iuser->frames) / 2;

	if (r_is_in_range) {
		*r_is_in_range = FALSE;
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
				*r_is_in_range = TRUE;
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
				*r_is_in_range = TRUE;
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
		short is_in_range;
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
	BLI_strncpy(filepath, ima->name, FILE_MAX);

	if (ima->source == IMA_SRC_SEQUENCE) {
		char head[FILE_MAX], tail[FILE_MAX];
		unsigned short numlen;
		int frame = iuser ? iuser->framenr : ima->lastframe;

		BLI_stringdec(filepath, head, tail, &numlen);
		BLI_stringenc(filepath, head, tail, numlen, frame);
	}

	BLI_path_abs(filepath, ID_BLEND_PATH(G.main, &ima->id));
}

int BKE_image_has_alpha(struct Image *image)
{
	ImBuf *ibuf;
	void *lock;
	int planes;

	ibuf = BKE_image_acquire_ibuf(image, NULL, &lock);
	planes = (ibuf ? ibuf->planes : 0);
	BKE_image_release_ibuf(image, ibuf, lock);

	if (planes == 32)
		return 1;
	else
		return 0;
}

void BKE_image_get_size(Image *image, ImageUser *iuser, int *width, int *height)
{
	ImBuf *ibuf = NULL;
	void *lock;

	ibuf = BKE_image_acquire_ibuf(image, iuser, &lock);

	if (ibuf && ibuf->x > 0 && ibuf->y > 0) {
		*width = ibuf->x;
		*height = ibuf->y;
	}
	else {
		*width  = IMG_SIZE_FALLBACK;
		*height = IMG_SIZE_FALLBACK;
	}

	BKE_image_release_ibuf(image, ibuf, lock);
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
	iuser.ok = TRUE;

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
	iuser.ok = TRUE;

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
	unsigned short numlen;
	char head[FILE_MAX], tail[FILE_MAX];
	char num[FILE_MAX] = {0};

	BLI_stringdec(image->name, head, tail, &numlen);
	BLI_strncpy(num, image->name + strlen(head), numlen + 1);

	return atoi(num);
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

void BKE_image_file_format_set(Image *image, int ftype)
{
#if 0
	ImBuf *ibuf = BKE_image_acquire_ibuf(image, NULL, NULL);
	if (ibuf) {
		ibuf->ftype = ftype;
	}
	BKE_image_release_ibuf(image, ibuf, NULL);
#endif

	BLI_spin_lock(&image_spin);
	if (image->cache != NULL) {
		struct MovieCacheIter *iter = IMB_moviecacheIter_new(image->cache);

		while (!IMB_moviecacheIter_done(iter)) {
			ImBuf *ibuf = IMB_moviecacheIter_getImBuf(iter);
			ibuf->ftype = ftype;
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
