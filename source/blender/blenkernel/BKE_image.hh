/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#include "DNA_listBase.h"

#include "BLI_compiler_attrs.hh"
#include "BLI_function_ref.hh"
#include "BLI_mutex.hh"
#include "BLI_string_ref.hh"

#include "IMB_imbuf_enums.h"

#include <cstdint>
#include <limits>
#include <optional>

namespace blender {
namespace gpu {
class Texture;
}  // namespace gpu

namespace ocio {
class ColorSpace;
}  // namespace ocio
using ColorSpace = ocio::ColorSpace;

struct rcti;
struct Depsgraph;
struct ID;
struct ImBuf;
struct ImBufCache;
struct MovieReader;
struct Image;
struct ImageFormatData;
struct ImagePool;
struct ImageTile;
struct ImbFormatOptions;
struct Library;
struct Main;
struct Object;
struct PartialUpdateRegister;
struct PartialUpdateUser;
struct RenderResult;
struct RenderSlot;
struct ReportList;
struct Scene;
struct StampData;
enum eImbFileType : int8_t;

#define IMA_MAX_SPACE 64
#define IMA_UDIM_MAX 2000

namespace bke {

struct ImageRuntime {
  /* Mutex used to guarantee thread-safe access to the cached ImBuf of the corresponding image ID.
   */
  Mutex cache_mutex;

  ImBufCache *cache = nullptr;

  /** Register containing partial updates. */
  PartialUpdateRegister *partial_update_register = nullptr;
  /** Partial update user for gpu::Textures stored inside the Image. */
  PartialUpdateUser *partial_update_user = nullptr;

  /* The image's current update count. See deg::set_id_update_count for more information. */
  uint64_t update_count = 0;

  /* Log GPU load errors once per image. */
  bool gpu_load_error_logged = false;

  float view_offset[2] = {};
  float view_zoom = 1.0f;
};

}  // namespace bke
/**
 * Clear the autosave information.
 *
 * \note At minimum, this should be called anytime the `packedfiles` list would be written to.
 */
void BKE_image_clear_autosave(Image *image);

void BKE_image_free_packedfiles(Image *image);
void BKE_image_free_views(Image *image);
void BKE_image_free_buffers(Image *image);
/**
 * Simply free the image data from memory,
 * on display the image can load again (except for render buffers).
 */
void BKE_image_free_buffers_ex(Image *image, bool do_lock);
/**
 * Free (or release) any data used by this image (does not free the image itself).
 * \note Call from library.
 */
void BKE_image_free_data(Image *image);

typedef void(StampCallback)(void *data,
                            const char *propname,
                            char *propvalue,
                            int propvalue_maxncpy);

void BKE_render_result_stamp_info(Scene *scene,
                                  Object *camera,
                                  RenderResult *rr,
                                  bool allocate_only);
/**
 * Fills in the static stamp data (i.e. everything except things that can change per frame).
 * The caller is responsible for freeing the allocated memory.
 */
StampData *BKE_stamp_info_from_scene_static(const Scene *scene);
/**
 * Check whether the given metadata field name translates to a known field of a stamp.
 */
bool BKE_stamp_is_known_field(const char *field_name);
void BKE_imbuf_stamp_info(const RenderResult *rr, ImBuf *ibuf);
void BKE_stamp_info_from_imbuf(RenderResult *rr, ImBuf *ibuf);
void BKE_stamp_info_callback(void *data,
                             StampData *stamp_data,
                             StampCallback callback,
                             bool noskip);
void BKE_image_multilayer_stamp_info_callback(void *data,
                                              const Image &image,
                                              StampCallback callback,
                                              bool noskip);
void BKE_render_result_stamp_data(RenderResult *rr, const char *key, const char *value);
StampData *BKE_stamp_data_copy(const StampData *stamp_data);
void BKE_stamp_data_free(StampData *stamp_data);
void BKE_image_stamp_buf(Scene *scene,
                         Object *camera,
                         const StampData *stamp_data_template,
                         ImBuf *ibuf);
bool BKE_imbuf_alpha_test(ImBuf *ibuf);
bool BKE_imbuf_write_stamp(const Scene *scene,
                           const RenderResult *rr,
                           ImBuf *ibuf,
                           const char *filepath,
                           const ImageFormatData *imf);
/**
 * \note imf->planes is ignored here, its assumed the image channels are already set.
 */
bool BKE_imbuf_write(ImBuf *ibuf, const char *filepath, const ImageFormatData *imf);
/**
 * Same as #BKE_imbuf_write() but crappy workaround not to permanently modify _some_,
 * values in the imbuf.
 */
bool BKE_imbuf_write_as(ImBuf *ibuf,
                        const char *filepath,
                        const ImageFormatData *imf,
                        bool save_copy);

/**
 * Used by sequencer too.
 */
MovieReader *openanim(const char *filepath,
                      ImBufFlags ibuf_flags,
                      int streamindex,
                      bool keep_original_colorspace,
                      char colorspace[IMA_MAX_SPACE]);
MovieReader *openanim_noload(const char *filepath,
                             ImBufFlags flags,
                             int streamindex,
                             bool keep_original_colorspace,
                             char colorspace[IMA_MAX_SPACE]);

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
bool BKE_image_has_ibuf(Image *ima, ImageUser *iuser);

/**
 * Return image buffer for given image and user:
 * - will lock render result if image type is render result and lock is not NULL
 * - will return NULL if image is NULL or image type is render or composite result and lock is NULL
 *
 * References the result, #BKE_image_release_ibuf should be used to de-reference.
 */
ImBuf *BKE_image_acquire_ibuf(Image *ima, ImageUser *iuser, void **r_lock);

/**
 * Identical to BKE_image_acquire_ibuf but assumes the caller will use the GPU data of the image
 * buffer if it exists without the need to make it available on the host. This essentially skips
 * GPU data reading to the host and is thus more performant.
 *
 * If #r_load_failed is provided, it is set to true when the image is known to have failed
 * loading when the #ImBuf is null, as opposed to not having been loaded yet.
 */
ImBuf *BKE_image_acquire_ibuf_gpu(Image *ima,
                                  ImageUser *iuser,
                                  void **r_lock,
                                  bool *r_load_failed = nullptr);

/**
 * Return image buffer for given image, user, pass, and view.
 * Is thread-safe, so another thread can be changing image while this function is executed.
 *
 * If the image is single-layer then the pass name is completely ignored.
 *
 * If the image is multi-layer then this function does all needed internal configurations to read
 * the pass. There is no need to acquire a temporary ImBuf prior to this call (which is what some
 * legacy code had to do to ensure proper type and RenderResult).
 *
 * References the result, #BKE_image_release_ibuf should be used to de-reference.
 */
ImBuf *BKE_image_acquire_multilayer_view_ibuf(const RenderData &render_data,
                                              Image &image,
                                              const ImageUser &image_user,
                                              const char *pass_name,
                                              const char *view_name);

void BKE_image_release_ibuf(Image *ima, ImBuf *ibuf, void *lock);

/**
 * Return image buffer of preview for given image
 * r_width & r_height are optional and return the _original size_ of the image.
 */
ImBuf *BKE_image_preview(Image *ima, short max_size, short *r_width, short *r_height);

ImagePool *BKE_image_pool_new();
void BKE_image_pool_free(ImagePool *pool);
ImBuf *BKE_image_pool_acquire_ibuf(Image *ima, ImageUser *iuser, ImagePool *pool);
void BKE_image_pool_release_ibuf(Image *ima, ImBuf *ibuf, ImagePool *pool);

/**
 * Set an alpha mode based on file extension.
 */
char BKE_image_alpha_mode_from_extension_ex(const char *filepath);
void BKE_image_alpha_mode_from_extension(Image *image);

/**
 * Returns a new image or NULL if it can't load.
 *
 * \note: The `_in_lib` version allows to add a new image in a given library. It also affects the
 * root path used for relative filepaths. See also #BKE_id_new and #BKE_id_new_in_lib
 * documentation for more details.
 */
Image *BKE_image_load(Main *bmain, const char *filepath);
Image *BKE_image_load_in_lib(Main *bmain,
                             std::optional<Library *> owner_library,
                             const char *filepath);
/**
 * Returns existing Image when filename/type is same.
 *
 * Checks if image was already loaded, then returns same image otherwise creates new
 * (does not load ibuf itself).
 *
 * \note: The `_in_lib` version allows to find an existing (or add a new) image in a given library.
 * It also affects the root path used for relative filepaths. See also #BKE_id_new and
 * #BKE_id_new_in_lib documentation for more details.
 */
Image *BKE_image_load_exists(Main *bmain, const char *filepath, bool *r_exists = nullptr);
Image *BKE_image_load_exists_in_lib(Main *bmain,
                                    std::optional<Library *> owner_library,
                                    const char *filepath,
                                    bool *r_exists = nullptr);

/**
 * Adds new image block, creates ImBuf and initializes color.
 */
Image *BKE_image_add_generated(Main *bmain,
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
Image *BKE_image_add_from_imbuf(Main *bmain, ImBuf *ibuf, const char *name);

/**
 * For a non-viewer single-buffer image (single frame file, or generated image) replace its image
 * buffer with the given one.
 * If an unsupported image type (multi-layer, image sequence, ...) the function will assert in the
 * debug mode and will have an undefined behavior in the release mode.
 */
void BKE_image_replace_imbuf(Image *image, ImBuf *ibuf);

/**
 * For reload, refresh, pack.
 */
void BKE_imageuser_default(ImageUser *iuser);
void BKE_image_init_imageuser(Image *ima, ImageUser *iuser);
void BKE_image_signal(Main *bmain, Image *ima, ImageUser *iuser, int signal);

void BKE_image_walk_all_users(
    const Main *mainp,
    void *customdata,
    void callback(Image *ima, ID *iuser_id, ImageUser *iuser, void *customdata));

/**
 * Ensures an Image exists for viewing nodes or render
 * forces existence of 1 Image for render-output or nodes, returns Image.
 *
 * \param name: Only for default, when making new one.
 */
Image *BKE_image_ensure_viewer(Main *bmain, int type, const char *name);
/**
 * Ensures the view node cache is compatible with the scene views.
 * Reset the image cache and views when the Viewer Nodes views don't match the scene views.
 */
void BKE_image_ensure_viewer_views(const RenderData *rd, Image *ima, ImageUser *iuser);

/**
 * Called on frame change or before render.
 */
void BKE_image_user_frame_calc(Image *ima, ImageUser *iuser, int cfra);
int BKE_image_user_frame_get(const ImageUser *iuser, int cfra, bool *r_is_in_range);
/**
 * Test if two image users resolve to the same image buffer.
 */
bool BKE_image_user_match(const ImageUser &a, const ImageUser &b);
void BKE_image_user_file_path(const ImageUser *iuser, const Image *ima, char *filepath);
void BKE_image_user_file_path_ex(const Main *bmain,
                                 const ImageUser *iuser,
                                 const Image *ima,
                                 char *filepath,
                                 const bool resolve_udim,
                                 const bool resolve_multiview);
void BKE_image_editors_update_frame(const Main *bmain, int cfra);

/**
 * Dependency graph update for image user users.
 */
bool BKE_image_user_id_has_animation(ID *id);
void BKE_image_user_id_eval_animation(Depsgraph *depsgraph, ID *id);

/**
 * Sets index offset for multi-layer files and because rendered results use fake layer/passes,
 * don't correct for wrong indices here.
 */
RenderPass *BKE_image_multilayer_index(RenderResult *rr, ImageUser *iuser);

/**
 * Sets index offset for multi-view files.
 */
void BKE_image_multiview_index(const Image *ima, ImageUser *iuser);

/**
 * For multi-layer images as well as for render-viewer
 * and because rendered results use fake layer/passes, don't correct for wrong indices here.
 */
bool BKE_image_is_multilayer(const Image *ima);
bool BKE_image_is_multiview(const Image *ima);
bool BKE_image_is_stereo(const Image *ima);

/**
 * Acquire render result associated with the give image.
 *
 * The returned render result is user-counted It is then required to call *
 * #BKE_image_release_renderresult with the non-null render result returned by this function.
 *
 * It is possible to use ibuf acquire/release API while a render result is held.
 *
 * It is allowed to call #BKE_image_release_renderresult with render_result of nullptr, but it is
 * not required.
 */
RenderResult *BKE_image_acquire_renderresult(Scene *scene, Image *ima);
void BKE_image_release_renderresult(Scene *scene, Image *ima, RenderResult *render_result);

/**
 * For multi-layer images as well as for single-layer.
 */
bool BKE_image_is_openexr(Image *ima);

/**
 * For multiple slot render, call this before render.
 */
void BKE_image_backup_render(Scene *scene, Image *ima, bool free_current_slot);

/**
 * Goes over all textures that use images.
 */
void BKE_image_free_all_textures(Main *bmain);

/**
 * Free cached frame buffers of all images with type MOVIE or SEQUENCE.
 * \param except_frame: This is weak, only works for sequences without offset.
 */
void BKE_image_all_free_anim_ibufs(Main *bmain, int except_frame);

/**
 * Time-based garbage collection for CPU and GPU image caches.
 */
void BKE_image_free_old_buffers(Main *bmain);

/**
 * Pack image to memory.
 */
bool BKE_image_memorypack(Image *ima);
void BKE_image_packfiles(ReportList *reports, Image *ima, const char *basepath);
void BKE_image_packfiles_from_mem(ReportList *reports, Image *ima, char *data, size_t data_len);

/**
 * High-level pack function.
 *
 * Packs image data, handling dirty state and raw data input.
 * Does nothing if image is already packed and not dirty (unless data is provided).
 */
void BKE_image_packfile_ensure(
    Main *bmain, Image *image, ReportList *reports, const char *data, int data_len);

/**
 * Populate the runtime cache for an image based on the autosave information.
 */
void BKE_image_populate_cache_from_autosave(Image *ima);

/**
 * Pack the current buffer data as part of the autosave process.
 *
 * \see BKE_image_memorypack
 */
bool BKE_image_autosave_memorypack(Image *ima);

/**
 * Prints memory statistics for images.
 */
void BKE_image_print_memlist(Main *bmain);

/**
 * Merge source into `dest`, and free `source`.
 */
void BKE_image_merge(Main *bmain, Image *dest, Image *source);

/**
 * Scale the image.
 */
bool BKE_image_scale(Image *image, int width, int height, ImageUser *iuser);

/**
 * Check if image might contain alpha.
 */
bool BKE_image_has_alpha(Image *image);

/**
 * Get tile index for tiled images.
 * \return The string length.
 */
int BKE_image_get_tile_label(const Image *ima,
                             const ImageTile *tile,
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
                             ListBaseT<LinkData> *tiles,
                             int *r_tile_start,
                             int *r_tile_range);

ImageTile *BKE_image_add_tile(Image *ima, int tile_number, const char *label);
bool BKE_image_remove_tile(Image *ima, ImageTile *tile);
void BKE_image_reassign_tile(Image *ima, ImageTile *tile, int new_tile_number);
void BKE_image_sort_tiles(Image *ima);

bool BKE_image_fill_tile(Image *ima, ImageTile *tile);

enum eUDIM_TILE_FORMAT {
  UDIM_TILE_FORMAT_NONE = 0,
  UDIM_TILE_FORMAT_UDIM = 1,
  UDIM_TILE_FORMAT_UVTILE = 2
};

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

ImageTile *BKE_image_get_tile(Image *ima, int tile_number);
ImageTile *BKE_image_get_tile_from_iuser(Image *ima, const ImageUser *iuser);

int BKE_image_get_tile_from_pos(Image *ima, const float uv[2], float r_uv[2], float r_ofs[2]);
void BKE_image_get_tile_uv(const Image *ima, const int tile_number, float r_uv[2]);

/**
 * Return the tile_number for the closest UDIM tile to `co`.
 */
int BKE_image_find_nearest_tile_with_offset(const Image *image,
                                            const float co[2],
                                            float r_uv_offset[2]) ATTR_NONNULL(2, 3);
int BKE_image_find_nearest_tile(const Image *image, const float co[2])
    ATTR_NONNULL(2) ATTR_WARN_UNUSED_RESULT;

/**
 * Iterate over Cycles texture cache associated with #source_filepath_abs, in
 * texture cache directory #cache_dir.
 */
void BKE_image_texture_cache_filepaths_foreach(
    const char *source_filepath_abs,
    const char *cache_dir,
    blender::FunctionRef<void(blender::StringRef cache_filepath)> callback);

void BKE_image_get_size(Image *image, ImageUser *iuser, int *r_width, int *r_height);
void BKE_image_get_size_fl(Image *image, ImageUser *iuser, float r_size[2]);
void BKE_image_get_aspect(Image *image, float *r_aspx, float *r_aspy);

/* `image_gen.cc` */

void BKE_image_buf_fill_color(
    unsigned char *rect, float *rect_float, int width, int height, const float color[4]);
void BKE_image_buf_fill_checker(unsigned char *rect, float *rect_float, int width, int height);
void BKE_image_buf_fill_checker_color(unsigned char *rect,
                                      float *rect_float,
                                      int width,
                                      int height);

/* Cycles hookup */

unsigned char *BKE_image_get_pixels_for_frame(Image *image, int frame, int tile);
float *BKE_image_get_float_pixels_for_frame(Image *image, int frame, int tile);

/* Image modifications */

bool BKE_image_is_dirty(Image *image);
void BKE_image_mark_dirty(Image *image, ImBuf *ibuf);
bool BKE_image_buffer_format_writable(ImBuf *ibuf);

bool BKE_image_is_dirty_writable(Image *image, bool *r_is_writable);

/**
 * Guess offset for the first frame in the sequence.
 */
int BKE_image_sequence_guess_offset(Image *image);
bool BKE_image_has_anim(Image *image);
bool BKE_image_has_packedfile(const Image *image);
bool BKE_image_has_filepath(const Image *ima);
/**
 * Checks the image buffer changes with time (not keyframed values).
 */
bool BKE_image_is_animated(Image *image);
/**
 * Checks whether the image consists of multiple buffers.
 */
bool BKE_image_has_multiple_ibufs(Image *image);
void BKE_image_file_format_set(Image *image, eImbFileType ftype, const ImbFormatOptions *options);
bool BKE_image_has_loaded_ibuf(Image *image);
/**
 * References the result, #BKE_image_release_ibuf is to be called to de-reference.
 * Use lock=NULL when calling #BKE_image_release_ibuf().
 */
ImBuf *BKE_image_get_ibuf_with_name(Image *image, const char *filepath);
/**
 * References the result, #BKE_image_release_ibuf is to be called to de-reference.
 * Use lock=NULL when calling #BKE_image_release_ibuf().
 *
 * TODO(sergey): This is actually "get first item from the cache", which is
 *               not so much predictable. But using first loaded image buffer
 *               was also malicious logic and all the areas which uses this
 *               function are to be re-considered.
 */
ImBuf *BKE_image_get_first_ibuf(Image *image);

RenderSlot *BKE_image_add_renderslot(Image *ima, const char *name);
bool BKE_image_remove_renderslot(Image *ima, ImageUser *iuser, int slot);
RenderSlot *BKE_image_get_renderslot(Image *ima, int index);
bool BKE_image_clear_renderslot(Image *ima, ImageUser *iuser, int slot);

/* --- image_partial_update.cc --- */
/** Image partial updates. */
/**
 * \brief Create a new PartialUpdateUser. An Object that contains data to use partial updates.
 */
PartialUpdateUser *BKE_image_partial_update_create(const Image *image);

/**
 * \brief free a partial update user.
 */
void BKE_image_partial_update_free(PartialUpdateUser *user);

/* --- partial updater (image side) --- */

void BKE_image_partial_update_register_free(Image *image);
/** \brief Mark a region of the image to update. */
void BKE_image_partial_update_mark_region(Image *image,
                                          const ImageTile *image_tile,
                                          const ImBuf *image_buffer,
                                          const rcti *updated_region);
/** \brief Mark the whole image to be updated. */
void BKE_image_partial_update_mark_full_update(Image *image);

}  // namespace blender
