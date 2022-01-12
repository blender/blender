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
#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Depsgraph;
struct ID;
struct ImBuf;
struct Image;
struct ImageFormatData;
struct ImagePool;
struct ImageTile;
struct ImbFormatOptions;
struct ListBase;
struct Main;
struct Object;
struct RenderResult;
struct ReportList;
struct Scene;
struct StampData;
struct anim;

#define IMA_MAX_SPACE 64
#define IMA_UDIM_MAX 2000

void BKE_image_free_packedfiles(struct Image *image);
void BKE_image_free_views(struct Image *image);
void BKE_image_free_buffers(struct Image *image);
/**
 * Simply free the image data from memory,
 * on display the image can load again (except for render buffers).
 */
void BKE_image_free_buffers_ex(struct Image *image, bool do_lock);
void BKE_image_free_gputextures(struct Image *ima);
/**
 * Free (or release) any data used by this image (does not free the image itself).
 * \note Call from library.
 */
void BKE_image_free_data(struct Image *image);

typedef void(StampCallback)(void *data, const char *propname, char *propvalue, int len);

void BKE_render_result_stamp_info(struct Scene *scene,
                                  struct Object *camera,
                                  struct RenderResult *rr,
                                  bool allocate_only);
/**
 * Fills in the static stamp data (i.e. everything except things that can change per frame).
 * The caller is responsible for freeing the allocated memory.
 */
struct StampData *BKE_stamp_info_from_scene_static(const struct Scene *scene);
/**
 * Check whether the given metadata field name translates to a known field of a stamp.
 */
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
/**
 * \note imf->planes is ignored here, its assumed the image channels are already set.
 */
void BKE_imbuf_write_prepare(struct ImBuf *ibuf, const struct ImageFormatData *imf);
int BKE_imbuf_write(struct ImBuf *ibuf, const char *name, const struct ImageFormatData *imf);
/**
 * Same as #BKE_imbuf_write() but crappy workaround not to permanently modify _some_,
 * values in the imbuf.
 */
int BKE_imbuf_write_as(struct ImBuf *ibuf,
                       const char *name,
                       struct ImageFormatData *imf,
                       bool save_copy);
void BKE_image_path_from_imformat(char *string,
                                  const char *base,
                                  const char *relbase,
                                  int frame,
                                  const struct ImageFormatData *im_format,
                                  bool use_ext,
                                  bool use_frames,
                                  const char *suffix);
void BKE_image_path_from_imtype(char *string,
                                const char *base,
                                const char *relbase,
                                int frame,
                                char imtype,
                                bool use_ext,
                                bool use_frames,
                                const char *suffix);
int BKE_image_path_ensure_ext_from_imformat(char *string, const struct ImageFormatData *im_format);
int BKE_image_path_ensure_ext_from_imtype(char *string, char imtype);
char BKE_image_ftype_to_imtype(int ftype, const struct ImbFormatOptions *options);
int BKE_image_imtype_to_ftype(char imtype, struct ImbFormatOptions *r_options);

bool BKE_imtype_is_movie(char imtype);
bool BKE_imtype_supports_zbuf(char imtype);
bool BKE_imtype_supports_compress(char imtype);
bool BKE_imtype_supports_quality(char imtype);
bool BKE_imtype_requires_linear_float(char imtype);
char BKE_imtype_valid_channels(char imtype, bool write_file);
char BKE_imtype_valid_depths(char imtype);

/**
 * String is from command line `--render-format` argument,
 * keep in sync with `creator_args.c` help info.
 */
char BKE_imtype_from_arg(const char *arg);

void BKE_imformat_defaults(struct ImageFormatData *im_format);
void BKE_imbuf_to_image_format(struct ImageFormatData *im_format, const struct ImBuf *imbuf);

/**
 * Used by sequencer too.
 */
struct anim *openanim(const char *name,
                      int flags,
                      int streamindex,
                      char colorspace[IMA_MAX_SPACE]);
struct anim *openanim_noload(const char *name,
                             int flags,
                             int streamindex,
                             char colorspace[IMA_MAX_SPACE]);

void BKE_image_tag_time(struct Image *ima);

/* ********************************** NEW IMAGE API *********************** */

/* ImageUser is in Texture, in Nodes, Background Image, Image Window, .... */
/* should be used in conjunction with an ID * to Image. */
struct ImageUser;
struct RenderData;
struct RenderPass;
struct RenderResult;

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

/**
 * Checks whether there's an image buffer for given image and user.
 */
bool BKE_image_has_ibuf(struct Image *ima, struct ImageUser *iuser);

/**
 * Return image buffer for given image and user:
 * - will lock render result if image type is render result and lock is not NULL
 * - will return NULL if image is NULL or image type is render or composite result and lock is NULL
 *
 * References the result, #BKE_image_release_ibuf should be used to de-reference.
 */
struct ImBuf *BKE_image_acquire_ibuf(struct Image *ima, struct ImageUser *iuser, void **r_lock);
void BKE_image_release_ibuf(struct Image *ima, struct ImBuf *ibuf, void *lock);

struct ImagePool *BKE_image_pool_new(void);
void BKE_image_pool_free(struct ImagePool *pool);
struct ImBuf *BKE_image_pool_acquire_ibuf(struct Image *ima,
                                          struct ImageUser *iuser,
                                          struct ImagePool *pool);
void BKE_image_pool_release_ibuf(struct Image *ima, struct ImBuf *ibuf, struct ImagePool *pool);

/**
 * Set an alpha mode based on file extension.
 */
char BKE_image_alpha_mode_from_extension_ex(const char *filepath);
void BKE_image_alpha_mode_from_extension(struct Image *image);

/**
 * Returns a new image or NULL if it can't load.
 */
struct Image *BKE_image_load(struct Main *bmain, const char *filepath);
/**
 * Returns existing Image when filename/type is same.
 *
 * Checks if image was already loaded, then returns same image otherwise creates new
 * (does not load ibuf itself).
 */
struct Image *BKE_image_load_exists_ex(struct Main *bmain, const char *filepath, bool *r_exists);
struct Image *BKE_image_load_exists(struct Main *bmain, const char *filepath);

/**
 * Adds new image block, creates ImBuf and initializes color.
 */
struct Image *BKE_image_add_generated(struct Main *bmain,
                                      unsigned int width,
                                      unsigned int height,
                                      const char *name,
                                      int depth,
                                      int floatbuf,
                                      short gen_type,
                                      const float color[4],
                                      bool stereo3d,
                                      bool is_data,
                                      bool tiled);
/**
 * Create an image from ibuf. The reference-count of ibuf is increased,
 * caller should take care to drop its reference by calling #IMB_freeImBuf if needed.
 */
struct Image *BKE_image_add_from_imbuf(struct Main *bmain, struct ImBuf *ibuf, const char *name);

/**
 * For reload, refresh, pack.
 */
void BKE_imageuser_default(struct ImageUser *iuser);
void BKE_image_init_imageuser(struct Image *ima, struct ImageUser *iuser);
void BKE_image_signal(struct Main *bmain, struct Image *ima, struct ImageUser *iuser, int signal);

void BKE_image_walk_all_users(const struct Main *mainp,
                              void *customdata,
                              void callback(struct Image *ima,
                                            struct ID *iuser_id,
                                            struct ImageUser *iuser,
                                            void *customdata));

/**
 * Ensures an Image exists for viewing nodes or render
 * forces existence of 1 Image for render-output or nodes, returns Image.
 *
 * \param name: Only for default, when making new one.
 */
struct Image *BKE_image_ensure_viewer(struct Main *bmain, int type, const char *name);
/**
 * Ensures the view node cache is compatible with the scene views.
 * Reset the image cache and views when the Viewer Nodes views don't match the scene views.
 */
void BKE_image_ensure_viewer_views(const struct RenderData *rd,
                                   struct Image *ima,
                                   struct ImageUser *iuser);

/**
 * Called on frame change or before render.
 */
void BKE_image_user_frame_calc(struct Image *ima, struct ImageUser *iuser, int cfra);
int BKE_image_user_frame_get(const struct ImageUser *iuser, int cfra, bool *r_is_in_range);
void BKE_image_user_file_path(struct ImageUser *iuser, struct Image *ima, char *path);
void BKE_image_user_file_path_ex(struct ImageUser *iuser,
                                 struct Image *ima,
                                 char *path,
                                 bool resolve_udim);
void BKE_image_editors_update_frame(const struct Main *bmain, int cfra);

/**
 * Dependency graph update for image user users.
 */
bool BKE_image_user_id_has_animation(struct ID *id);
void BKE_image_user_id_eval_animation(struct Depsgraph *depsgraph, struct ID *id);

/**
 * Sets index offset for multi-layer files and because rendered results use fake layer/passes,
 * don't correct for wrong indices here.
 */
struct RenderPass *BKE_image_multilayer_index(struct RenderResult *rr, struct ImageUser *iuser);

/**
 * Sets index offset for multi-view files.
 */
void BKE_image_multiview_index(struct Image *ima, struct ImageUser *iuser);

/**
 * For multi-layer images as well as for render-viewer
 * and because rendered results use fake layer/passes, don't correct for wrong indices here.
 */
bool BKE_image_is_multilayer(struct Image *ima);
bool BKE_image_is_multiview(struct Image *ima);
bool BKE_image_is_stereo(struct Image *ima);
struct RenderResult *BKE_image_acquire_renderresult(struct Scene *scene, struct Image *ima);
void BKE_image_release_renderresult(struct Scene *scene, struct Image *ima);

/**
 * For multi-layer images as well as for single-layer.
 */
bool BKE_image_is_openexr(struct Image *ima);

/**
 * For multiple slot render, call this before render.
 */
void BKE_image_backup_render(struct Scene *scene, struct Image *ima, bool free_current_slot);

/**
 * For single-layer OpenEXR saving.
 */
bool BKE_image_save_openexr_multiview(struct Image *ima,
                                      struct ImBuf *ibuf,
                                      const char *filepath,
                                      int flags);

/**
 * Goes over all textures that use images.
 */
void BKE_image_free_all_textures(struct Main *bmain);

/**
 * Operates on one image only!
 * \param except_frame: This is weak, only works for sequences without offset.
 */
void BKE_image_free_anim_ibufs(struct Image *ima, int except_frame);

/**
 * Does all images with type MOVIE or SEQUENCE.
 */
void BKE_image_all_free_anim_ibufs(struct Main *bmain, int cfra);

void BKE_image_free_all_gputextures(struct Main *bmain);
/**
 * Same as above but only free animated images.
 */
void BKE_image_free_anim_gputextures(struct Main *bmain);
void BKE_image_free_old_gputextures(struct Main *bmain);

/**
 * Pack image to memory.
 */
bool BKE_image_memorypack(struct Image *ima);
void BKE_image_packfiles(struct ReportList *reports, struct Image *ima, const char *basepath);
void BKE_image_packfiles_from_mem(struct ReportList *reports,
                                  struct Image *ima,
                                  char *data,
                                  const size_t data_len);

/**
 * Prints memory statistics for images.
 */
void BKE_image_print_memlist(struct Main *bmain);

/**
 * Merge source into `dest`, and free `source`.
 */
void BKE_image_merge(struct Main *bmain, struct Image *dest, struct Image *source);

/**
 * Scale the image.
 */
bool BKE_image_scale(struct Image *image, int width, int height);

/**
 * Check if texture has alpha (depth=32).
 */
bool BKE_image_has_alpha(struct Image *image);

/**
 * Check if texture has GPU texture code.
 */
bool BKE_image_has_opengl_texture(struct Image *ima);

/**
 * Get tile index for tiled images.
 */
void BKE_image_get_tile_label(struct Image *ima,
                              struct ImageTile *tile,
                              char *label,
                              int len_label);

/**
 * Checks whether the given filepath refers to a UDIM tiled texture.
 * If yes, the range from the lowest to the highest tile is returned.
 *
 * `filepath` may be modified to ensure a UDIM token is present.
 * `tiles` may be filled even if the result ultimately is false!
 */
bool BKE_image_get_tile_info(char *filepath,
                             struct ListBase *tiles,
                             int *tile_start,
                             int *tile_range);

struct ImageTile *BKE_image_add_tile(struct Image *ima, int tile_number, const char *label);
bool BKE_image_remove_tile(struct Image *ima, struct ImageTile *tile);
void BKE_image_reassign_tile(struct Image *ima, struct ImageTile *tile, int new_tile_number);
void BKE_image_sort_tiles(struct Image *ima);

bool BKE_image_fill_tile(struct Image *ima,
                         struct ImageTile *tile,
                         int width,
                         int height,
                         const float color[4],
                         int gen_type,
                         int planes,
                         bool is_float);

typedef enum {
  UDIM_TILE_FORMAT_NONE = 0,
  UDIM_TILE_FORMAT_UDIM = 1,
  UDIM_TILE_FORMAT_UVTILE = 2
} eUDIM_TILE_FORMAT;

/**
 * Ensures that `filename` contains a UDIM token if we find a supported format pattern.
 * \note This must only be the name component (without slashes).
 */
void BKE_image_ensure_tile_token(char *filename);

/**
 * When provided with an absolute virtual filepath, check to see if at least
 * one concrete file exists.
 * Note: This function requires directory traversal and may be inefficient in time-critical,
 * or iterative, code paths.
 */
bool BKE_image_tile_filepath_exists(const char *filepath);

/**
 * Retrieves the UDIM token format and returns the pattern from the provided `filepath`.
 * The returned pattern is typically passed to either `BKE_image_get_tile_number_from_filepath` or
 * `BKE_image_set_filepath_from_tile_number`.
 */
char *BKE_image_get_tile_strformat(const char *filepath, eUDIM_TILE_FORMAT *r_tile_format);
bool BKE_image_get_tile_number_from_filepath(const char *filepath,
                                             const char *pattern,
                                             eUDIM_TILE_FORMAT tile_format,
                                             int *r_tile_number);
void BKE_image_set_filepath_from_tile_number(char *filepath,
                                             const char *pattern,
                                             eUDIM_TILE_FORMAT tile_format,
                                             int tile_number);

struct ImageTile *BKE_image_get_tile(struct Image *ima, int tile_number);
struct ImageTile *BKE_image_get_tile_from_iuser(struct Image *ima, const struct ImageUser *iuser);

int BKE_image_get_tile_from_pos(struct Image *ima,
                                const float uv[2],
                                float r_uv[2],
                                float r_ofs[2]);
/**
 * Return the tile_number for the closest UDIM tile.
 */
int BKE_image_find_nearest_tile(const struct Image *image, const float co[2]);

void BKE_image_get_size(struct Image *image, struct ImageUser *iuser, int *r_width, int *r_height);
void BKE_image_get_size_fl(struct Image *image, struct ImageUser *iuser, float r_size[2]);
void BKE_image_get_aspect(struct Image *image, float *r_aspx, float *r_aspy);

/* image_gen.c */

void BKE_image_buf_fill_color(
    unsigned char *rect, float *rect_float, int width, int height, const float color[4]);
void BKE_image_buf_fill_checker(unsigned char *rect, float *rect_float, int width, int height);
void BKE_image_buf_fill_checker_color(unsigned char *rect,
                                      float *rect_float,
                                      int width,
                                      int height);

/* Cycles hookup */

unsigned char *BKE_image_get_pixels_for_frame(struct Image *image, int frame, int tile);
float *BKE_image_get_float_pixels_for_frame(struct Image *image, int frame, int tile);

/* Image modifications */

bool BKE_image_is_dirty(struct Image *image);
void BKE_image_mark_dirty(struct Image *image, struct ImBuf *ibuf);
bool BKE_image_buffer_format_writable(struct ImBuf *ibuf);

bool BKE_image_is_dirty_writable(struct Image *image, bool *is_format_writable);

/**
 * Guess offset for the first frame in the sequence.
 */
int BKE_image_sequence_guess_offset(struct Image *image);
bool BKE_image_has_anim(struct Image *image);
bool BKE_image_has_packedfile(const struct Image *image);
bool BKE_image_has_filepath(struct Image *ima);
/**
 * Checks the image buffer changes with time (not keyframed values).
 */
bool BKE_image_is_animated(struct Image *image);
/**
 * Checks whether the image consists of multiple buffers.
 */
bool BKE_image_has_multiple_ibufs(struct Image *image);
void BKE_image_file_format_set(struct Image *image,
                               int ftype,
                               const struct ImbFormatOptions *options);
bool BKE_image_has_loaded_ibuf(struct Image *image);
/**
 * References the result, #BKE_image_release_ibuf is to be called to de-reference.
 * Use lock=NULL when calling #BKE_image_release_ibuf().
 */
struct ImBuf *BKE_image_get_ibuf_with_name(struct Image *image, const char *name);
/**
 * References the result, #BKE_image_release_ibuf is to be called to de-reference.
 * Use lock=NULL when calling #BKE_image_release_ibuf().
 *
 * TODO(sergey): This is actually "get first item from the cache", which is
 *               not so much predictable. But using first loaded image buffer
 *               was also malicious logic and all the areas which uses this
 *               function are to be re-considered.
 */
struct ImBuf *BKE_image_get_first_ibuf(struct Image *image);

/**
 * Not to be use directly.
 */
struct GPUTexture *BKE_image_create_gpu_texture_from_ibuf(struct Image *image, struct ImBuf *ibuf);

/**
 * Get the #GPUTexture for a given `Image`.
 *
 * `iuser` and `ibuf` are mutual exclusive parameters. The caller can pass the `ibuf` when already
 * available. It is also required when requesting the #GPUTexture for a render result.
 */
struct GPUTexture *BKE_image_get_gpu_texture(struct Image *image,
                                             struct ImageUser *iuser,
                                             struct ImBuf *ibuf);
struct GPUTexture *BKE_image_get_gpu_tiles(struct Image *image,
                                           struct ImageUser *iuser,
                                           struct ImBuf *ibuf);
struct GPUTexture *BKE_image_get_gpu_tilemap(struct Image *image,
                                             struct ImageUser *iuser,
                                             struct ImBuf *ibuf);
/**
 * Is the alpha of the `GPUTexture` for a given image/ibuf premultiplied.
 */
bool BKE_image_has_gpu_texture_premultiplied_alpha(struct Image *image, struct ImBuf *ibuf);
/**
 * Partial update of texture for texture painting.
 * This is often much quicker than fully updating the texture for high resolution images.
 */
void BKE_image_update_gputexture(
    struct Image *ima, struct ImageUser *iuser, int x, int y, int w, int h);
/**
 * Mark areas on the #GPUTexture that needs to be updated. The areas are marked in chunks.
 * The next time the #GPUTexture is used these tiles will be refreshes. This saves time
 * when writing to the same place multiple times This happens for during foreground rendering.
 */
void BKE_image_update_gputexture_delayed(
    struct Image *ima, struct ImBuf *ibuf, int x, int y, int w, int h);
/**
 * Called on entering and exiting texture paint mode,
 * temporary disabling/enabling mipmapping on all images for quick texture
 * updates with glTexSubImage2D. images that didn't change don't have to be re-uploaded to OpenGL.
 */
void BKE_image_paint_set_mipmap(struct Main *bmain, bool mipmap);

/**
 * Delayed free of OpenGL buffers by main thread.
 */
void BKE_image_free_unused_gpu_textures(void);

struct RenderSlot *BKE_image_add_renderslot(struct Image *ima, const char *name);
bool BKE_image_remove_renderslot(struct Image *ima, struct ImageUser *iuser, int slot);
struct RenderSlot *BKE_image_get_renderslot(struct Image *ima, int index);
bool BKE_image_clear_renderslot(struct Image *ima, struct ImageUser *iuser, int slot);

#ifdef __cplusplus
}
#endif
