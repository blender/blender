/**
 * blenlib/BKE_image.h (mar-2001 nzc)
 *	
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef BKE_IMAGE_H
#define BKE_IMAGE_H

#ifdef __cplusplus
extern "C" {
#endif

struct Image;
struct ImBuf;
struct Tex;
struct anim;
struct Scene;

/* call from library */
void	free_image(struct Image *me);

void	BKE_stamp_info(struct Scene *scene, struct ImBuf *ibuf);
void	BKE_stamp_buf(struct Scene *scene, unsigned char *rect, float *rectf, int width, int height, int channels);
int		BKE_write_ibuf(struct Scene *scene, struct ImBuf *ibuf, char *name, int imtype, int subimtype, int quality);
void	BKE_makepicstring(char *string, char *base, int frame, int imtype, int use_ext);
void	BKE_add_image_extension(char *string, int imtype);
int		BKE_ftype_to_imtype(int ftype);
int		BKE_imtype_to_ftype(int imtype);
int		BKE_imtype_is_movie(int imtype);

struct anim *openanim(char * name, int flags);

void	converttopremul(struct ImBuf *ibuf);
void	image_de_interlace(struct Image *ima, int odd);
	
void	tag_image_time(struct Image *ima);
void	free_old_images(void);

/* ********************************** NEW IMAGE API *********************** */

/* ImageUser is in Texture, in Nodes, Background Image, Image Window, .... */
/* should be used in conjunction with an ID * to Image. */
struct ImageUser;
struct RenderPass;
struct RenderResult;

/* ima->source; where image comes from */
#define IMA_SRC_CHECK		0
#define IMA_SRC_FILE		1
#define IMA_SRC_SEQUENCE	2
#define IMA_SRC_MOVIE		3
#define IMA_SRC_GENERATED	4
#define IMA_SRC_VIEWER		5

/* ima->type, how to handle/generate it */
#define IMA_TYPE_IMAGE		0
#define IMA_TYPE_MULTILAYER	1
		/* generated */
#define IMA_TYPE_UV_TEST	2
		/* viewers */
#define IMA_TYPE_R_RESULT   4
#define IMA_TYPE_COMPOSITE	5

/* ima->ok */
#define IMA_OK				1
#define IMA_OK_LOADED		2

/* signals */
	/* reload only frees, doesn't read until image_get_ibuf() called */
#define IMA_SIGNAL_RELOAD			0
#define IMA_SIGNAL_FREE				1
	/* pack signals are executed */
#define IMA_SIGNAL_PACK				2
#define IMA_SIGNAL_REPACK			3
#define IMA_SIGNAL_UNPACK			4
	/* source changes, from image to sequence or movie, etc */
#define IMA_SIGNAL_SRC_CHANGE		5
	/* image-user gets a new image, check settings */
#define IMA_SIGNAL_USER_NEW_IMAGE	6

/* depending Image type, and (optional) ImageUser setting it returns ibuf */
/* always call to make signals work */
struct ImBuf *BKE_image_get_ibuf(struct Image *ima, struct ImageUser *iuser);

/* same as above, but can be used to retrieve images being rendered in
 * a thread safe way, always call both acquire and release */
struct ImBuf *BKE_image_acquire_ibuf(struct Image *ima, struct ImageUser *iuser, void **lock_r);
void BKE_image_release_ibuf(struct Image *ima, void *lock);

/* returns existing Image when filename/type is same (frame optional) */
struct Image *BKE_add_image_file(const char *name, int frame);

/* adds image, adds ibuf, generates color or pattern */
struct Image *BKE_add_image_size(int width, int height, char *name, int floatbuf, short uvtestgrid, float color[4]);

/* for reload, refresh, pack */
void BKE_image_signal(struct Image *ima, struct ImageUser *iuser, int signal);

/* ensures an Image exists for viewing nodes or render */
struct Image *BKE_image_verify_viewer(int type, const char *name);

/* force an ImBuf to become part of Image */
void BKE_image_assign_ibuf(struct Image *ima, struct ImBuf *ibuf);

/* called on frame change or before render */
void BKE_image_user_calc_frame(struct ImageUser *iuser, int cfra, int fieldnr);

/* produce image export path */
int BKE_get_image_export_path(struct Image *im, const char *dest_dir, char *abs, int abs_size, char *rel, int rel_size);

/* fix things in ImageUser when new image gets assigned */
void BKE_image_user_new_image(struct Image *ima, struct ImageUser *iuser);

/* sets index offset for multilayer files */
struct RenderPass *BKE_image_multilayer_index(struct RenderResult *rr, struct ImageUser *iuser);

/* for multilayer images as well as for render-viewer */
struct RenderResult *BKE_image_acquire_renderresult(struct Scene *scene, struct Image *ima);
void BKE_image_release_renderresult(struct Scene *scene, struct Image *ima);

/* frees all ibufs used by any image datablocks */
void	BKE_image_free_image_ibufs(void);
	
/* goes over all textures that use images */
void	BKE_image_free_all_textures(void);

/* does one image! */
void	BKE_image_free_anim_ibufs(struct Image *ima, int except_frame);

/* does all images with type MOVIE or SEQUENCE */
void BKE_image_all_free_anim_ibufs(int except_frame);

void BKE_image_memorypack(struct Image *ima);

/* prints memory statistics for images */
void BKE_image_print_memlist(void);

/* empty image block, of similar type and filename */
struct Image *BKE_image_copy(struct Image *ima);

/* merge source into dest, and free source */
void BKE_image_merge(struct Image *dest, struct Image *source);

#ifdef __cplusplus
}
#endif

#endif

