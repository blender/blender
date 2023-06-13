/* SPDX-FileCopyrightText: 2006-2007 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 *
 * Resizable Icons for Blender
 *
 * There is some thread safety for this API but it is rather weak. Registering or unregistering
 * icons is thread safe, changing data of icons from multiple threads is not. Practically this
 * should be fine since only the main thread modifies icons. Should that change, more locks or a
 * different design need to be introduced.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_compiler_attrs.h"

#include "DNA_ID_enums.h"

typedef void (*DrawInfoFreeFP)(void *drawinfo);

enum {
  /** ID preview: obj is #ID. */
  ICON_DATA_ID = 0,
  /** Arbitrary Image buffer: obj is #ImBuf */
  ICON_DATA_IMBUF,
  /** Preview: obj is #PreviewImage */
  ICON_DATA_PREVIEW,
  /** 2D triangles: obj is #Icon_Geom */
  ICON_DATA_GEOM,
  /** Studio-light. */
  ICON_DATA_STUDIOLIGHT,
  /** GPencil Layer color preview (annotations): obj is #bGPDlayer */
  ICON_DATA_GPLAYER,
};

/**
 * \note See comment at the top regarding thread safety.
 */
struct Icon {
  void *drawinfo;
  /**
   * Data defined by #obj_type
   * \note for #ICON_DATA_GEOM the memory is owned by the icon,
   * could be made into a flag if we want that to be optional.
   */
  void *obj;
  char obj_type;
  /** Internal use only. */
  char flag;
  /** #ID_Type or 0 when not used for ID preview. */
  short id_type;
  DrawInfoFreeFP drawinfo_free;
};

/** Used for #ICON_DATA_GEOM, assigned to #Icon.obj. */
struct Icon_Geom {
  int icon_id;
  int coords_len;
  int coords_range[2];
  unsigned char (*coords)[2];
  unsigned char (*colors)[4];
  /* when not NULL, the memory of coords and colors is a sub-region of this pointer. */
  const void *mem;
};

typedef struct Icon Icon;

struct BlendDataReader;
struct BlendWriter;
struct ID;
struct ImBuf;
struct PreviewImage;
struct StudioLight;
struct bGPDlayer;

void BKE_icons_init(int first_dyn_id);

/**
 * Return icon id for library object or create new icon if not found.
 */
int BKE_icon_id_ensure(struct ID *id);

/**
 * Return icon id for Grease Pencil layer (color preview) or create new icon if not found.
 */
int BKE_icon_gplayer_color_ensure(struct bGPDlayer *gpl);

/**
 * Return icon id of given preview, or create new icon if not found.
 */
int BKE_icon_preview_ensure(struct ID *id, struct PreviewImage *preview);

/**
 * Create an icon as owner or \a ibuf. The icon-ID is not stored in \a ibuf,
 * it needs to be stored separately.
 * \note Transforms ownership of \a ibuf to the newly created icon.
 */
int BKE_icon_imbuf_create(struct ImBuf *ibuf) ATTR_WARN_UNUSED_RESULT;
struct ImBuf *BKE_icon_imbuf_get_buffer(int icon_id) ATTR_WARN_UNUSED_RESULT;

/**
 * Retrieve icon for id.
 */
struct Icon *BKE_icon_get(int icon_id);

/**
 * Set icon for id if not already defined.
 * Used for inserting the internal icons.
 */
void BKE_icon_set(int icon_id, struct Icon *icon);

/**
 * Remove icon and free data if library object becomes invalid.
 */
void BKE_icon_id_delete(struct ID *id);

/**
 * Remove icon and free data.
 */
bool BKE_icon_delete(int icon_id);
bool BKE_icon_delete_unmanaged(int icon_id);

/**
 * Report changes - icon needs to be recalculated.
 */
void BKE_icon_changed(int icon_id);

/**
 * Free all icons.
 */
void BKE_icons_free(void);

/**
 * Free all icons marked for deferred deletion.
 */
void BKE_icons_deferred_free(void);

/**
 * Free the preview image for use in list.
 */
void BKE_previewimg_freefunc(void *link);

/**
 * Free the preview image.
 */
void BKE_previewimg_free(struct PreviewImage **prv);

/**
 * Clear the preview image or icon, but does not free it.
 */
void BKE_previewimg_clear(struct PreviewImage *prv);

/**
 * Clear the preview image or icon at a specific size.
 */
void BKE_previewimg_clear_single(struct PreviewImage *prv, enum eIconSizes size);

/**
 * Get the preview from any pointer.
 */
struct PreviewImage **BKE_previewimg_id_get_p(const struct ID *id);
struct PreviewImage *BKE_previewimg_id_get(const struct ID *id);

bool BKE_previewimg_id_supports_jobs(const struct ID *id);

/**
 * Trigger deferred loading of a custom image file into the preview buffer.
 */
void BKE_previewimg_id_custom_set(struct ID *id, const char *filepath);

/**
 * Free the preview image belonging to the id.
 */
void BKE_previewimg_id_free(struct ID *id);

/**
 * Create a new preview image.
 */
struct PreviewImage *BKE_previewimg_create(void);

/**
 * Create a copy of the preview image.
 */
struct PreviewImage *BKE_previewimg_copy(const struct PreviewImage *prv);

/**
 * Duplicate preview image from \a id and clear icon_id,
 * to be used by data-block copy functions.
 */
void BKE_previewimg_id_copy(struct ID *new_id, const struct ID *old_id);

/**
 * Retrieve existing or create new preview image.
 */
struct PreviewImage *BKE_previewimg_id_ensure(struct ID *id);

/**
 * Handle deferred (lazy) loading/generation of preview image, if needed.
 * For now, only used with file thumbnails.
 */
void BKE_previewimg_ensure(struct PreviewImage *prv, int size);

/**
 * Create an #ImBuf holding a copy of the preview image buffer in \a prv.
 * \note The returned image buffer has to be free'd (#IMB_freeImBuf()).
 */
struct ImBuf *BKE_previewimg_to_imbuf(struct PreviewImage *prv, int size);

void BKE_previewimg_finish(struct PreviewImage *prv, int size);
bool BKE_previewimg_is_finished(const struct PreviewImage *prv, int size);

struct PreviewImage *BKE_previewimg_cached_get(const char *name);

/**
 * Generate an empty #PreviewImage, if not yet existing.
 */
struct PreviewImage *BKE_previewimg_cached_ensure(const char *name);

/**
 * Generate a #PreviewImage from given `filepath`, using thumbnails management, if not yet
 * existing. Does not actually generate the preview, #BKE_previewimg_ensure() must be called for
 * that.
 */
struct PreviewImage *BKE_previewimg_cached_thumbnail_read(const char *name,
                                                          const char *filepath,
                                                          int source,
                                                          bool force_update);

void BKE_previewimg_cached_release(const char *name);

void BKE_previewimg_deferred_release(struct PreviewImage *prv);

void BKE_previewimg_blend_write(struct BlendWriter *writer, const struct PreviewImage *prv);
void BKE_previewimg_blend_read(struct BlendDataReader *reader, struct PreviewImage *prv);

int BKE_icon_geom_ensure(struct Icon_Geom *geom);
struct Icon_Geom *BKE_icon_geom_from_memory(uchar *data, size_t data_len);
struct Icon_Geom *BKE_icon_geom_from_file(const char *filename);

struct ImBuf *BKE_icon_geom_rasterize(const struct Icon_Geom *geom,
                                      unsigned int size_x,
                                      unsigned int size_y);
void BKE_icon_geom_invert_lightness(struct Icon_Geom *geom);

int BKE_icon_ensure_studio_light(struct StudioLight *sl, int id_type);

#define ICON_RENDER_DEFAULT_HEIGHT 32

#ifdef __cplusplus
}
#endif
