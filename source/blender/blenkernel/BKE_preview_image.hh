/* SPDX-FileCopyrightText: 2006-2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <memory>
#include <optional>

#include "BLI_sys_types.h"

#include "DNA_ID_enums.h"

struct BlendWriter;
struct BlendDataReader;
struct ID;
struct ImBuf;
struct PreviewImage;

enum ThumbSource : int8_t;

namespace blender::bke {

struct PreviewDeferredLoadingData;

struct PreviewImageRuntime {
  /** Used to store data to defer the loading of the preview. If empty, loading is not deferred. */
  std::unique_ptr<PreviewDeferredLoadingData> deferred_loading_data;
  PreviewImageRuntime();
  PreviewImageRuntime(const PreviewImageRuntime &other);
  ~PreviewImageRuntime();
};

}  // namespace blender::bke

void BKE_preview_images_init();
void BKE_preview_images_free();

/**
 * Free the preview image for use in list.
 */
void BKE_previewimg_freefunc(void *link);

/**
 * Free the preview image.
 */
void BKE_previewimg_free(PreviewImage **prv);

/** Must be called after reading a preview image from file. */
void BKE_previewimg_runtime_data_clear(PreviewImage *prv);

/**
 * Clear the preview image or icon, but does not free it.
 */
void BKE_previewimg_clear(PreviewImage *prv);

/**
 * Clear the preview image or icon at a specific size.
 */
void BKE_previewimg_clear_single(PreviewImage *prv, enum eIconSizes size);

/**
 * Get the preview from any pointer.
 */
PreviewImage **BKE_previewimg_id_get_p(const ID *id);
PreviewImage *BKE_previewimg_id_get(const ID *id);

bool BKE_previewimg_id_supports_jobs(const ID *id);

/**
 * Trigger deferred loading of a custom image file into the preview buffer.
 */
void BKE_previewimg_id_custom_set(ID *id, const char *filepath);

/**
 * Free the preview image belonging to the id.
 */
void BKE_previewimg_id_free(ID *id);

/**
 * Create a new preview image.
 */
PreviewImage *BKE_previewimg_create();

/**
 * Create a copy of the preview image.
 */
PreviewImage *BKE_previewimg_copy(const PreviewImage *prv);

/**
 * Duplicate preview image from \a id and clear icon_id,
 * to be used by data-block copy functions.
 */
void BKE_previewimg_id_copy(ID *new_id, const ID *old_id);

/**
 * Retrieve existing or create new preview image.
 */
PreviewImage *BKE_previewimg_id_ensure(ID *id);

/**
 * Handle deferred (lazy) loading/generation of preview image, if needed.
 * For now, only used with file thumbnails.
 */
void BKE_previewimg_ensure(PreviewImage *prv, int size);

const char *BKE_previewimg_deferred_filepath_get(const PreviewImage *prv);
std::optional<int> BKE_previewimg_deferred_thumb_source_get(const PreviewImage *prv);

/**
 * Create an #ImBuf holding a copy of the preview image buffer in \a prv.
 * \note The returned image buffer has to be freed (#IMB_freeImBuf()).
 */
ImBuf *BKE_previewimg_to_imbuf(PreviewImage *prv, int size);

void BKE_previewimg_finish(PreviewImage *prv, int size);
bool BKE_previewimg_is_finished(const PreviewImage *prv, int size);

PreviewImage *BKE_previewimg_cached_get(const char *name);

/**
 * Generate an empty #PreviewImage, if not yet existing.
 */
PreviewImage *BKE_previewimg_cached_ensure(const char *name);

/**
 * Generate a #PreviewImage from given `filepath`, using thumbnails management, if not yet
 * existing. Does not actually generate the preview, #BKE_previewimg_ensure() must be called for
 * that.
 */
PreviewImage *BKE_previewimg_cached_thumbnail_read(const char *name,
                                                   const char *filepath,
                                                   int source,
                                                   bool force_update);

void BKE_previewimg_cached_release(const char *name);

void BKE_previewimg_deferred_release(PreviewImage *prv);

void BKE_previewimg_blend_write(BlendWriter *writer, const PreviewImage *prv);
void BKE_previewimg_blend_read(BlendDataReader *reader, PreviewImage *prv);
