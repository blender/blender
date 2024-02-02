/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_compiler_attrs.h"
#include "BLI_utildefines.h"

#include "BLI_rect.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Depsgraph;
struct ID;
struct ImBuf;
struct ImBufAnim;
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

typedef void(StampCallback)(void *data,
                            const char *propname,
                            char *propvalue,
                            int propvalue_maxncpy);

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
void BKE_imbuf_stamp_info(const struct RenderResult *rr, struct ImBuf *ibuf);
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
int BKE_imbuf_write_stamp(const struct Scene *scene,
                          const struct RenderResult *rr,
                          struct ImBuf *ibuf,
                          const char *filepath,
                          const struct ImageFormatData *imf);
/**
 * \note imf->planes is ignored here, its assumed the image channels are already set.
 */
int BKE_imbuf_write(struct ImBuf *ibuf, const char *filepath, const struct ImageFormatData *imf);
/**
 * Same as #BKE_imbuf_write() but crappy workaround not to permanently modify _some_,
 * values in the imbuf.
 */
int BKE_imbuf_write_as(struct ImBuf *ibuf,
                       const char *filepath,
                       const struct ImageFormatData *imf,
                       bool save_copy);

/**
 * Used by sequencer too.
 */
struct ImBufAnim *openanim(const char *filepath,
                           int flags,
                           int streamindex,
                           char colorspace[IMA_MAX_SPACE]);
struct ImBufAnim *openanim_noload(const char *filepath,
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
 * For a non-viewer single-buffer image (single frame file, or generated image) replace its image
 * buffer with the given one.
 * If an unsupported image type (multi-layer, image sequence, ...) the function will assert in the
 * debug mode and will have an undefined behavior in the release mode.
 */
void BKE_image_replace_imbuf(struct Image *image, struct ImBuf *ibuf);

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
void BKE_image_user_file_path(const struct ImageUser *iuser,
                              const struct Image *ima,
                              char *filepath);
void BKE_image_user_file_path_ex(const struct Main *bmain,
                                 const struct ImageUser *iuser,
                                 const struct Image *ima,
                                 char *filepath,
                                 const bool resolve_udim,
                                 const bool resolve_multiview);
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
void BKE_image_multiview_index(const struct Image *ima, struct ImageUser *iuser);

/**
 * For multi-layer images as well as for render-viewer
 * and because rendered results use fake layer/passes, don't correct for wrong indices here.
 */
bool BKE_image_is_multilayer(const struct Image *ima);
bool BKE_image_is_multiview(const struct Image *ima);
bool BKE_image_is_stereo(const struct Image *ima);
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
                                  size_t data_len);

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
bool BKE_image_scale(struct Image *image, int width, int height, struct ImageUser *iuser);

/**
 * Check if texture has alpha `planes == 32 || planes == 16`.
 */
bool BKE_image_has_alpha(struct Image *image);

/**
 * Check if texture has GPU texture code.
 */
bool BKE_image_has_opengl_texture(struct Image *ima);

/**
 * Get tile index for tiled images.
 * \return The string length.
 */
int BKE_image_get_tile_label(const struct Image *ima,
                             const struct ImageTile *tile,
                             char *label,
                             int label_maxncpy);

/**
 * Checks whether the given filepath refers to a UDIM tiled texture.
 * If yes, the range from the lowest to the highest tile is returned.
 *
 * \param filepath: may be modified to ensure a UDIM token is present.
 * \param tiles: may be filled even if the result ultimately is false!
 */
bool BKE_image_get_tile_info(char *filepath,
                             struct ListBase *tiles,
                             int *r_tile_start,
                             int *r_tile_range);

struct ImageTile *BKE_image_add_tile(struct Image *ima, int tile_number, const char *label);
bool BKE_image_remove_tile(struct Image *ima, struct ImageTile *tile);
void BKE_image_reassign_tile(struct Image *ima, struct ImageTile *tile, int new_tile_number);
void BKE_image_sort_tiles(struct Image *ima);

bool BKE_image_fill_tile(struct Image *ima, struct ImageTile *tile);

typedef enum {
  UDIM_TILE_FORMAT_NONE = 0,
  UDIM_TILE_FORMAT_UDIM = 1,
  UDIM_TILE_FORMAT_UVTILE = 2
} eUDIM_TILE_FORMAT;

/**
 * Checks if the filename portion of the path contains a UDIM token.
 */
bool BKE_image_is_filename_tokenized(char *filepath);

/**
 * Ensures that `filename` contains a UDIM token if we find a supported format pattern.
 * \note This must only be the name component (without slashes).
 */
void BKE_image_ensure_tile_token(char *filepath, size_t filepath_maxncpy);
void BKE_image_ensure_tile_token_filename_only(char *filename, size_t filename_maxncpy);

/**
 * When provided with an absolute virtual `filepath`, check to see if at least
 * one concrete file exists.
 * NOTE: This function requires directory traversal and may be inefficient in time-critical,
 * or iterative, code paths.
 */
bool BKE_image_tile_filepath_exists(const char *filepath);

/**
 * Retrieves the UDIM token format and returns the pattern from the provided `filepath`.
 * The returned pattern is typically passed to either #BKE_image_get_tile_number_from_filepath or
 * #BKE_image_set_filepath_from_tile_number.
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
void BKE_image_get_tile_uv(const struct Image *ima, const int tile_number, float r_uv[2]);

/**
 * Return the tile_number for the closest UDIM tile to `co`.
 */
int BKE_image_find_nearest_tile_with_offset(const struct Image *image,
                                            const float co[2],
                                            float r_uv_offset[2]) ATTR_NONNULL(2, 3);
int BKE_image_find_nearest_tile(const struct Image *image, const float co[2])
    ATTR_NONNULL(2) ATTR_WARN_UNUSED_RESULT;

void BKE_image_get_size(struct Image *image, struct ImageUser *iuser, int *r_width, int *r_height);
void BKE_image_get_size_fl(struct Image *image, struct ImageUser *iuser, float r_size[2]);
void BKE_image_get_aspect(struct Image *image, float *r_aspx, float *r_aspy);

/* `image_gen.cc` */

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

bool BKE_image_is_dirty_writable(struct Image *image, bool *r_is_writable);

/**
 * Guess offset for the first frame in the sequence.
 */
int BKE_image_sequence_guess_offset(struct Image *image);
bool BKE_image_has_anim(struct Image *image);
bool BKE_image_has_packedfile(const struct Image *image);
bool BKE_image_has_filepath(const struct Image *ima);
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
struct ImBuf *BKE_image_get_ibuf_with_name(struct Image *image, const char *filepath);
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
 * Ensure that the cached GPU texture inside the image matches the pass, layer, and view of the
 * given image user, if not, invalidate the cache such that the next call to the GPU texture
 * retrieval functions such as BKE_image_get_gpu_texture updates the cache with an image that
 * matches the give image user.
 *
 * This is provided as a separate function and not implemented as part of the GPU texture retrieval
 * functions because the current cache system only allows a single pass, layer, and stereo view to
 * be cached, so possible frequent cache invalidation can have performance implications,
 * and making invalidation explicit by calling this function will help make that clear and pave the
 * way for a more complete cache system in the future.
 */
void BKE_image_ensure_gpu_texture(struct Image *image, struct ImageUser *iuser);

/**
 * Get the #GPUTexture for a given `Image`.
 *
 *
 *
 * The requested GPU texture will be cached for subsequent calls, but only a single layer, pass,
 * and view can be cached at a time, so the cache should be invalidated in operators and RNA
 * callbacks that change the layer, pass, or view of the image to maintain a correct cache state.
 * However, in some cases, multiple layers, passes, or views might be needed at the same time, like
 * is the case for the realtime compositor. This is currently not supported, so the caller should
 * ensure that the requested layer is indeed the cached one and invalidated the cached otherwise by
 * calling BKE_image_ensure_gpu_texture. This is a workaround until image can support a more
 * complete caching system.
 */
struct GPUTexture *BKE_image_get_gpu_texture(struct Image *image, struct ImageUser *iuser);

/*
 * Like BKE_image_get_gpu_texture, but can also get render or compositing result.
 */
struct GPUTexture *BKE_image_get_gpu_viewer_texture(struct Image *image, struct ImageUser *iuser);

/*
 * Like BKE_image_get_gpu_texture, but can also return array and tile mapping texture for UDIM
 * tiles as used in material shaders.
 */
typedef struct ImageGPUTextures {
  struct GPUTexture *texture;
  struct GPUTexture *tile_mapping;
} ImageGPUTextures;

ImageGPUTextures BKE_image_get_gpu_material_texture(struct Image *image,
                                                    struct ImageUser *iuser,
                                                    const bool use_tile_mapping);

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
void BKE_image_update_gputexture_delayed(struct Image *ima,
                                         struct ImageTile *image_tile,
                                         struct ImBuf *ibuf,
                                         int x,
                                         int y,
                                         int w,
                                         int h);

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

/* --- image_partial_update.cc --- */
/** Image partial updates. */
struct PartialUpdateUser;

/**
 * \brief Create a new PartialUpdateUser. An Object that contains data to use partial updates.
 */
struct PartialUpdateUser *BKE_image_partial_update_create(const struct Image *image);

/**
 * \brief free a partial update user.
 */
void BKE_image_partial_update_free(struct PartialUpdateUser *user);

/* --- partial updater (image side) --- */

void BKE_image_partial_update_register_free(struct Image *image);
/** \brief Mark a region of the image to update. */
void BKE_image_partial_update_mark_region(struct Image *image,
                                          const struct ImageTile *image_tile,
                                          const struct ImBuf *image_buffer,
                                          const rcti *updated_region);
/** \brief Mark the whole image to be updated. */
void BKE_image_partial_update_mark_full_update(struct Image *image);

#ifdef __cplusplus
}
#endif
