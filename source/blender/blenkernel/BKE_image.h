/*
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
 */
#ifndef __BKE_IMAGE_H__
#define __BKE_IMAGE_H__

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Depsgraph;
struct ID;
struct ImBuf;
struct Image;
struct ImageFormatData;
struct ImagePool;
struct ImbFormatOptions;
struct Main;
struct Object;
struct RenderResult;
struct ReportList;
struct Scene;
struct StampData;
struct anim;

#define IMA_MAX_SPACE 64

void BKE_images_init(void);
void BKE_images_exit(void);

void BKE_image_free_packedfiles(struct Image *image);
void BKE_image_free_views(struct Image *image);
void BKE_image_free_buffers(struct Image *image);
void BKE_image_free_buffers_ex(struct Image *image, bool do_lock);
/* call from library */
void BKE_image_free(struct Image *image);

void BKE_image_init(struct Image *image);

typedef void(StampCallback)(void *data, const char *propname, char *propvalue, int len);

void BKE_render_result_stamp_info(struct Scene *scene,
                                  struct Object *camera,
                                  struct RenderResult *rr,
                                  bool allocate_only);
/**
 * Fills in the static stamp data (i.e. everything except things that can change per frame).
 * The caller is responsible for freeing the allocated memory.
 */
struct StampData *BKE_stamp_info_from_scene_static(struct Scene *scene);
bool BKE_stamp_is_known_field(const char *field_name);
void BKE_imbuf_stamp_info(struct RenderResult *rr, struct ImBuf *ibuf);
void BKE_stamp_info_from_imbuf(struct RenderResult *rr, struct ImBuf *ibuf);
void BKE_stamp_info_callback(void *data,
                             struct StampData *stamp_data,
                             StampCallback callback,
                             bool noskip);
void BKE_render_result_stamp_data(struct RenderResult *rr, const char *key, const char *value);
struct StampData *BKE_stamp_data_copy(const struct StampData *stamp_data);
void BKE_stamp_data_free(struct StampData *stamp_data);
void BKE_image_stamp_buf(struct Scene *scene,
                         struct Object *camera,
                         const struct StampData *stamp_data_template,
                         unsigned char *rect,
                         float *rectf,
                         int width,
                         int height,
                         int channels);
bool BKE_imbuf_alpha_test(struct ImBuf *ibuf);
int BKE_imbuf_write_stamp(struct Scene *scene,
                          struct RenderResult *rr,
                          struct ImBuf *ibuf,
                          const char *name,
                          const struct ImageFormatData *imf);
void BKE_imbuf_write_prepare(struct ImBuf *ibuf, const struct ImageFormatData *imf);
int BKE_imbuf_write(struct ImBuf *ibuf, const char *name, const struct ImageFormatData *imf);
int BKE_imbuf_write_as(struct ImBuf *ibuf,
                       const char *name,
                       struct ImageFormatData *imf,
                       const bool is_copy);
void BKE_image_path_from_imformat(char *string,
                                  const char *base,
                                  const char *relbase,
                                  int frame,
                                  const struct ImageFormatData *im_format,
                                  const bool use_ext,
                                  const bool use_frames,
                                  const char *suffix);
void BKE_image_path_from_imtype(char *string,
                                const char *base,
                                const char *relbase,
                                int frame,
                                const char imtype,
                                const bool use_ext,
                                const bool use_frames,
                                const char *suffix);
int BKE_image_path_ensure_ext_from_imformat(char *string, const struct ImageFormatData *im_format);
int BKE_image_path_ensure_ext_from_imtype(char *string, const char imtype);
char BKE_image_ftype_to_imtype(const int ftype, const struct ImbFormatOptions *options);
int BKE_image_imtype_to_ftype(const char imtype, struct ImbFormatOptions *r_options);

bool BKE_imtype_is_movie(const char imtype);
int BKE_imtype_supports_zbuf(const char imtype);
int BKE_imtype_supports_compress(const char imtype);
int BKE_imtype_supports_quality(const char imtype);
int BKE_imtype_requires_linear_float(const char imtype);
char BKE_imtype_valid_channels(const char imtype, bool write_file);
char BKE_imtype_valid_depths(const char imtype);

char BKE_imtype_from_arg(const char *arg);

void BKE_imformat_defaults(struct ImageFormatData *im_format);
void BKE_imbuf_to_image_format(struct ImageFormatData *im_format, const struct ImBuf *imbuf);

struct anim *openanim(const char *name,
                      int flags,
                      int streamindex,
                      char colorspace[IMA_MAX_SPACE]);
struct anim *openanim_noload(const char *name,
                             int flags,
                             int streamindex,
                             char colorspace[IMA_MAX_SPACE]);

void BKE_image_make_local(struct Main *bmain, struct Image *ima, const bool lib_local);

void BKE_image_tag_time(struct Image *ima);

/* ********************************** NEW IMAGE API *********************** */

/* ImageUser is in Texture, in Nodes, Background Image, Image Window, .... */
/* should be used in conjunction with an ID * to Image. */
struct ImageUser;
struct RenderData;
struct RenderPass;
struct RenderResult;

/* ima->source; where image comes from */
#define IMA_SRC_CHECK 0
#define IMA_SRC_FILE 1
#define IMA_SRC_SEQUENCE 2
#define IMA_SRC_MOVIE 3
#define IMA_SRC_GENERATED 4
#define IMA_SRC_VIEWER 5

/* ima->type, how to handle/generate it */
#define IMA_TYPE_IMAGE 0
#define IMA_TYPE_MULTILAYER 1
/* generated */
#define IMA_TYPE_UV_TEST 2
/* viewers */
#define IMA_TYPE_R_RESULT 4
#define IMA_TYPE_COMPOSITE 5

enum {
  IMA_GENTYPE_BLANK = 0,
  IMA_GENTYPE_GRID = 1,
  IMA_GENTYPE_GRID_COLOR = 2,
};

/* ima->ok */
#define IMA_OK 1
#define IMA_OK_LOADED 2

/* signals */
/* reload only frees, doesn't read until image_get_ibuf() called */
#define IMA_SIGNAL_RELOAD 0
#define IMA_SIGNAL_FREE 1
/* source changes, from image to sequence or movie, etc */
#define IMA_SIGNAL_SRC_CHANGE 5
/* image-user gets a new image, check settings */
#define IMA_SIGNAL_USER_NEW_IMAGE 6
#define IMA_SIGNAL_COLORMANAGE 7

#define IMA_CHAN_FLAG_BW 1
#define IMA_CHAN_FLAG_RGB 2
#define IMA_CHAN_FLAG_ALPHA 4

/* checks whether there's an image buffer for given image and user */
bool BKE_image_has_ibuf(struct Image *ima, struct ImageUser *iuser);

/* same as above, but can be used to retrieve images being rendered in
 * a thread safe way, always call both acquire and release */
struct ImBuf *BKE_image_acquire_ibuf(struct Image *ima, struct ImageUser *iuser, void **r_lock);
void BKE_image_release_ibuf(struct Image *ima, struct ImBuf *ibuf, void *lock);

struct ImagePool *BKE_image_pool_new(void);
void BKE_image_pool_free(struct ImagePool *pool);
struct ImBuf *BKE_image_pool_acquire_ibuf(struct Image *ima,
                                          struct ImageUser *iuser,
                                          struct ImagePool *pool);
void BKE_image_pool_release_ibuf(struct Image *ima, struct ImBuf *ibuf, struct ImagePool *pool);

/* set an alpha mode based on file extension */
char BKE_image_alpha_mode_from_extension_ex(const char *filepath);
void BKE_image_alpha_mode_from_extension(struct Image *image);

/* returns a new image or NULL if it can't load */
struct Image *BKE_image_load(struct Main *bmain, const char *filepath);
/* returns existing Image when filename/type is same (frame optional) */
struct Image *BKE_image_load_exists_ex(struct Main *bmain, const char *filepath, bool *r_exists);
struct Image *BKE_image_load_exists(struct Main *bmain, const char *filepath);

/* adds image, adds ibuf, generates color or pattern */
struct Image *BKE_image_add_generated(struct Main *bmain,
                                      unsigned int width,
                                      unsigned int height,
                                      const char *name,
                                      int depth,
                                      int floatbuf,
                                      short gen_type,
                                      const float color[4],
                                      const bool stereo3d);
/* adds image from imbuf, owns imbuf */
struct Image *BKE_image_add_from_imbuf(struct Main *bmain, struct ImBuf *ibuf, const char *name);

/* for reload, refresh, pack */
void BKE_imageuser_default(struct ImageUser *iuser);
void BKE_image_init_imageuser(struct Image *ima, struct ImageUser *iuser);
void BKE_image_signal(struct Main *bmain, struct Image *ima, struct ImageUser *iuser, int signal);

void BKE_image_walk_all_users(const struct Main *mainp,
                              void *customdata,
                              void callback(struct Image *ima,
                                            struct ImageUser *iuser,
                                            void *customdata));

/* ensures an Image exists for viewing nodes or render */
struct Image *BKE_image_verify_viewer(struct Main *bmain, int type, const char *name);
/* ensures the view node cache is compatible with the scene views */
void BKE_image_verify_viewer_views(const struct RenderData *rd,
                                   struct Image *ima,
                                   struct ImageUser *iuser);

/* called on frame change or before render */
void BKE_image_user_frame_calc(struct ImageUser *iuser, int cfra);
int BKE_image_user_frame_get(const struct ImageUser *iuser, int cfra, bool *r_is_in_range);
void BKE_image_user_file_path(struct ImageUser *iuser, struct Image *ima, char *path);
void BKE_image_editors_update_frame(const struct Main *bmain, int cfra);

/* dependency graph update for image user users */
bool BKE_image_user_id_has_animation(struct ID *id);
void BKE_image_user_id_eval_animation(struct Depsgraph *depsgrah, struct ID *id);

/* sets index offset for multilayer files */
struct RenderPass *BKE_image_multilayer_index(struct RenderResult *rr, struct ImageUser *iuser);

/* sets index offset for multiview files */
void BKE_image_multiview_index(struct Image *ima, struct ImageUser *iuser);

/* for multilayer images as well as for render-viewer */
bool BKE_image_is_multilayer(struct Image *ima);
bool BKE_image_is_multiview(struct Image *ima);
bool BKE_image_is_stereo(struct Image *ima);
struct RenderResult *BKE_image_acquire_renderresult(struct Scene *scene, struct Image *ima);
void BKE_image_release_renderresult(struct Scene *scene, struct Image *ima);

/* for multilayer images as well as for singlelayer */
bool BKE_image_is_openexr(struct Image *ima);

/* for multiple slot render, call this before render */
void BKE_image_backup_render(struct Scene *scene, struct Image *ima, bool free_current_slot);

/* for singlelayer openexr saving */
bool BKE_image_save_openexr_multiview(struct Image *ima,
                                      struct ImBuf *ibuf,
                                      const char *filepath,
                                      const int flags);

/* goes over all textures that use images */
void BKE_image_free_all_textures(struct Main *bmain);

/* does one image! */
void BKE_image_free_anim_ibufs(struct Image *ima, int except_frame);

/* does all images with type MOVIE or SEQUENCE */
void BKE_image_all_free_anim_ibufs(struct Main *bmain, int except_frame);

bool BKE_image_memorypack(struct Image *ima);
void BKE_image_packfiles(struct ReportList *reports, struct Image *ima, const char *basepath);
void BKE_image_packfiles_from_mem(struct ReportList *reports,
                                  struct Image *ima,
                                  char *data,
                                  const size_t data_len);

/* prints memory statistics for images */
void BKE_image_print_memlist(struct Main *bmain);

/* empty image block, of similar type and filename */
void BKE_image_copy_data(struct Main *bmain,
                         struct Image *ima_dst,
                         const struct Image *ima_src,
                         const int flag);
struct Image *BKE_image_copy(struct Main *bmain, const struct Image *ima);

/* merge source into dest, and free source */
void BKE_image_merge(struct Main *bmain, struct Image *dest, struct Image *source);

/* scale the image */
bool BKE_image_scale(struct Image *image, int width, int height);

/* check if texture has alpha (depth=32) */
bool BKE_image_has_alpha(struct Image *image);

/* check if texture has gpu texture code */
bool BKE_image_has_opengl_texture(struct Image *ima);

void BKE_image_get_size(struct Image *image, struct ImageUser *iuser, int *width, int *height);
void BKE_image_get_size_fl(struct Image *image, struct ImageUser *iuser, float size[2]);
void BKE_image_get_aspect(struct Image *image, float *aspx, float *aspy);

/* image_gen.c */
void BKE_image_buf_fill_color(
    unsigned char *rect, float *rect_float, int width, int height, const float color[4]);
void BKE_image_buf_fill_checker(unsigned char *rect, float *rect_float, int height, int width);
void BKE_image_buf_fill_checker_color(unsigned char *rect,
                                      float *rect_float,
                                      int height,
                                      int width);

/* Cycles hookup */
unsigned char *BKE_image_get_pixels_for_frame(struct Image *image, int frame);
float *BKE_image_get_float_pixels_for_frame(struct Image *image, int frame);

/* Image modifications */
bool BKE_image_is_dirty(struct Image *image);
void BKE_image_mark_dirty(struct Image *image, struct ImBuf *ibuf);

/* Guess offset for the first frame in the sequence */
int BKE_image_sequence_guess_offset(struct Image *image);
bool BKE_image_has_anim(struct Image *image);
bool BKE_image_has_packedfile(struct Image *image);
bool BKE_image_is_animated(struct Image *image);
void BKE_image_file_format_set(struct Image *image,
                               int ftype,
                               const struct ImbFormatOptions *options);
bool BKE_image_has_loaded_ibuf(struct Image *image);
struct ImBuf *BKE_image_get_ibuf_with_name(struct Image *image, const char *name);
struct ImBuf *BKE_image_get_first_ibuf(struct Image *image);

struct RenderSlot *BKE_image_add_renderslot(struct Image *ima, const char *name);
bool BKE_image_remove_renderslot(struct Image *ima, struct ImageUser *iuser, int slot);
struct RenderSlot *BKE_image_get_renderslot(struct Image *ima, int slot);
bool BKE_image_clear_renderslot(struct Image *ima, struct ImageUser *iuser, int slot);

#ifdef __cplusplus
}
#endif

#endif
