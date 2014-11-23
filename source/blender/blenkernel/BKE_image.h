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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __BKE_IMAGE_H__
#define __BKE_IMAGE_H__

/** \file BKE_image.h
 *  \ingroup bke
 *  \since March 2001
 *  \author nzc
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Image;
struct ImBuf;
struct Tex;
struct anim;
struct Scene;
struct Object;
struct ImageFormatData;
struct ImagePool;
struct Main;

#define IMA_MAX_SPACE       64

void   BKE_images_init(void);
void   BKE_images_exit(void);

void    BKE_image_free_buffers(struct Image *image);
/* call from library */
void    BKE_image_free(struct Image *image);

void    BKE_imbuf_stamp_info(struct Scene *scene, struct Object *camera, struct ImBuf *ibuf);
void    BKE_stamp_buf(struct Scene *scene, struct Object *camera, unsigned char *rect, float *rectf, int width, int height, int channels);
bool    BKE_imbuf_alpha_test(struct ImBuf *ibuf);
int     BKE_imbuf_write_stamp(struct Scene *scene, struct Object *camera, struct ImBuf *ibuf, const char *name, struct ImageFormatData *imf);
int     BKE_imbuf_write(struct ImBuf *ibuf, const char *name, struct ImageFormatData *imf);
int     BKE_imbuf_write_as(struct ImBuf *ibuf, const char *name, struct ImageFormatData *imf, const bool is_copy);
void    BKE_makepicstring(char *string, const char *base, const char *relbase, int frame,
                          const struct ImageFormatData *im_format, const bool use_ext, const bool use_frames);
void    BKE_makepicstring_from_type(char *string, const char *base, const char *relbase, int frame,
                                    const char imtype, const bool use_ext, const bool use_frames);
int     BKE_add_image_extension(char *string, const struct ImageFormatData *im_format);
int     BKE_add_image_extension_from_type(char *string, const char imtype);
char    BKE_ftype_to_imtype(const int ftype);
int     BKE_imtype_to_ftype(const char imtype);

bool    BKE_imtype_is_movie(const char imtype);
int     BKE_imtype_supports_zbuf(const char imtype);
int     BKE_imtype_supports_compress(const char imtype);
int     BKE_imtype_supports_quality(const char imtype);
int     BKE_imtype_requires_linear_float(const char imtype);
char    BKE_imtype_valid_channels(const char imtype, bool write_file);
char    BKE_imtype_valid_depths(const char imtype);

char    BKE_imtype_from_arg(const char *arg);

void    BKE_imformat_defaults(struct ImageFormatData *im_format);
void    BKE_imbuf_to_image_format(struct ImageFormatData *im_format, const struct ImBuf *imbuf);

struct anim *openanim(const char *name, int flags, int streamindex, char colorspace[IMA_MAX_SPACE]);

void    BKE_image_de_interlace(struct Image *ima, int odd);

void    BKE_image_make_local(struct Image *ima);

void    BKE_image_tag_time(struct Image *ima);
void    free_old_images(void);

/* ********************************** NEW IMAGE API *********************** */

/* ImageUser is in Texture, in Nodes, Background Image, Image Window, .... */
/* should be used in conjunction with an ID * to Image. */
struct ImageUser;
struct RenderPass;
struct RenderResult;

/* ima->source; where image comes from */
#define IMA_SRC_CHECK       0
#define IMA_SRC_FILE        1
#define IMA_SRC_SEQUENCE    2
#define IMA_SRC_MOVIE       3
#define IMA_SRC_GENERATED   4
#define IMA_SRC_VIEWER      5

/* ima->type, how to handle/generate it */
#define IMA_TYPE_IMAGE      0
#define IMA_TYPE_MULTILAYER 1
/* generated */
#define IMA_TYPE_UV_TEST    2
/* viewers */
#define IMA_TYPE_R_RESULT   4
#define IMA_TYPE_COMPOSITE  5

enum {
	IMA_GENTYPE_BLANK = 0,
	IMA_GENTYPE_GRID = 1,
	IMA_GENTYPE_GRID_COLOR = 2
};

/* ima->ok */
#define IMA_OK              1
#define IMA_OK_LOADED       2

/* signals */
/* reload only frees, doesn't read until image_get_ibuf() called */
#define IMA_SIGNAL_RELOAD           0
#define IMA_SIGNAL_FREE             1
/* source changes, from image to sequence or movie, etc */
#define IMA_SIGNAL_SRC_CHANGE       5
/* image-user gets a new image, check settings */
#define IMA_SIGNAL_USER_NEW_IMAGE   6
#define IMA_SIGNAL_COLORMANAGE      7

#define IMA_CHAN_FLAG_BW    1
#define IMA_CHAN_FLAG_RGB   2
#define IMA_CHAN_FLAG_ALPHA 4

/* checks whether there's an image buffer for given image and user */
bool BKE_image_has_ibuf(struct Image *ima, struct ImageUser *iuser);

/* same as above, but can be used to retrieve images being rendered in
 * a thread safe way, always call both acquire and release */
struct ImBuf *BKE_image_acquire_ibuf(struct Image *ima, struct ImageUser *iuser, void **lock_r);
void BKE_image_release_ibuf(struct Image *ima, struct ImBuf *ibuf, void *lock);

struct ImagePool *BKE_image_pool_new(void);
void BKE_image_pool_free(struct ImagePool *pool);
struct ImBuf *BKE_image_pool_acquire_ibuf(struct Image *ima, struct ImageUser *iuser, struct ImagePool *pool);
void BKE_image_pool_release_ibuf(struct Image *ima, struct ImBuf *ibuf, struct ImagePool *pool);

/* set an alpha mode based on file extension */
char  BKE_image_alpha_mode_from_extension_ex(const char *filepath);
void BKE_image_alpha_mode_from_extension(struct Image *image);

/* returns a new image or NULL if it can't load */
struct Image *BKE_image_load(struct Main *bmain, const char *filepath);
/* returns existing Image when filename/type is same (frame optional) */
struct Image *BKE_image_load_exists_ex(const char *filepath, bool *r_exists);
struct Image *BKE_image_load_exists(const char *filepath);

/* adds image, adds ibuf, generates color or pattern */
struct Image *BKE_image_add_generated(
        struct Main *bmain, unsigned int width, unsigned int height, const char *name, int depth, int floatbuf, short gen_type, const float color[4]);
/* adds image from imbuf, owns imbuf */
struct Image *BKE_image_add_from_imbuf(struct ImBuf *ibuf);

/* for reload, refresh, pack */
void BKE_image_signal(struct Image *ima, struct ImageUser *iuser, int signal);

void BKE_image_walk_all_users(const struct Main *mainp, void *customdata,
                              void callback(struct Image *ima, struct ImageUser *iuser, void *customdata));

/* ensures an Image exists for viewing nodes or render */
struct Image *BKE_image_verify_viewer(int type, const char *name);

/* force an ImBuf to become part of Image */
void BKE_image_assign_ibuf(struct Image *ima, struct ImBuf *ibuf);

/* called on frame change or before render */
void BKE_image_user_frame_calc(struct ImageUser *iuser, int cfra, int fieldnr);
void BKE_image_user_check_frame_calc(struct ImageUser *iuser, int cfra, int fieldnr);
int  BKE_image_user_frame_get(const struct ImageUser *iuser, int cfra, int fieldnr, bool *r_is_in_range);
void BKE_image_user_file_path(struct ImageUser *iuser, struct Image *ima, char *path); 
void BKE_image_update_frame(const struct Main *bmain, int cfra);

/* sets index offset for multilayer files */
struct RenderPass *BKE_image_multilayer_index(struct RenderResult *rr, struct ImageUser *iuser);

/* for multilayer images as well as for render-viewer */
struct RenderResult *BKE_image_acquire_renderresult(struct Scene *scene, struct Image *ima);
void BKE_image_release_renderresult(struct Scene *scene, struct Image *ima);

/* for multiple slot render, call this before render */
void BKE_image_backup_render(struct Scene *scene, struct Image *ima);
	
/* goes over all textures that use images */
void    BKE_image_free_all_textures(void);

/* does one image! */
void    BKE_image_free_anim_ibufs(struct Image *ima, int except_frame);

/* does all images with type MOVIE or SEQUENCE */
void BKE_image_all_free_anim_ibufs(int except_frame);

void BKE_image_memorypack(struct Image *ima);

/* prints memory statistics for images */
void BKE_image_print_memlist(void);

/* empty image block, of similar type and filename */
struct Image *BKE_image_copy(struct Main *bmain, struct Image *ima);

/* merge source into dest, and free source */
void BKE_image_merge(struct Image *dest, struct Image *source);

/* scale the image */
bool BKE_image_scale(struct Image *image, int width, int height);

/* check if texture has alpha (depth=32) */
bool BKE_image_has_alpha(struct Image *image);

void BKE_image_get_size(struct Image *image, struct ImageUser *iuser, int *width, int *height);
void BKE_image_get_size_fl(struct Image *image, struct ImageUser *iuser, float size[2]);
void BKE_image_get_aspect(struct Image *image, float *aspx, float *aspy);

/* image_gen.c */
void BKE_image_buf_fill_color(unsigned char *rect, float *rect_float, int width, int height, const float color[4]);
void BKE_image_buf_fill_checker(unsigned char *rect, float *rect_float, int height, int width);
void BKE_image_buf_fill_checker_color(unsigned char *rect, float *rect_float, int height, int width);

/* Cycles hookup */
unsigned char *BKE_image_get_pixels_for_frame(struct Image *image, int frame);
float *BKE_image_get_float_pixels_for_frame(struct Image *image, int frame);

/* Guess offset for the first frame in the sequence */
int BKE_image_sequence_guess_offset(struct Image *image);

bool BKE_image_is_animated(struct Image *image);
bool BKE_image_is_dirty(struct Image *image);
void BKE_image_file_format_set(struct Image *image, int ftype);
bool BKE_image_has_loaded_ibuf(struct Image *image);
struct ImBuf *BKE_image_get_ibuf_with_name(struct Image *image, const char *name);
struct ImBuf *BKE_image_get_first_ibuf(struct Image *image);
#ifdef __cplusplus
}
#endif

#endif

