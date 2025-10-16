/* SPDX-FileCopyrightText: 2006-2007 Blender Authors
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

#include "BLI_compiler_attrs.h"
#include "BLI_sys_types.h"

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

struct ID;
struct ImBuf;
struct PreviewImage;
struct StudioLight;
struct bGPDlayer;

void BKE_icons_init(int first_dyn_id);

/**
 * Return icon id for library object or create new icon if not found.
 */
int BKE_icon_id_ensure(ID *id);

/**
 * Return icon id for Grease Pencil layer (color preview) or create new icon if not found.
 */
int BKE_icon_gplayer_color_ensure(bGPDlayer *gpl);

/**
 * Return icon id of given preview, or create new icon if not found.
 */
int BKE_icon_preview_ensure(ID *id, PreviewImage *preview);

/**
 * Create an icon as owner or \a ibuf. The icon-ID is not stored in \a ibuf,
 * it needs to be stored separately.
 * \note Transforms ownership of \a ibuf to the newly created icon.
 */
int BKE_icon_imbuf_create(ImBuf *ibuf) ATTR_WARN_UNUSED_RESULT;
ImBuf *BKE_icon_imbuf_get_buffer(int icon_id) ATTR_WARN_UNUSED_RESULT;

/**
 * Retrieve icon for id.
 */
Icon *BKE_icon_get(int icon_id);

/**
 * Set icon for id if not already defined.
 * Used for inserting the internal icons.
 */
void BKE_icon_set(int icon_id, Icon *icon);

/**
 * Remove icon and free data if library object becomes invalid.
 */
void BKE_icon_id_delete(ID *id);

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
void BKE_icons_free();

/**
 * Free all icons marked for deferred deletion.
 */
void BKE_icons_deferred_free();

int BKE_icon_geom_ensure(Icon_Geom *geom);
Icon_Geom *BKE_icon_geom_from_memory(uchar *data, size_t data_len);
Icon_Geom *BKE_icon_geom_from_file(const char *filename);

ImBuf *BKE_icon_geom_rasterize(const Icon_Geom *geom, unsigned int size_x, unsigned int size_y);
void BKE_icon_geom_invert_lightness(Icon_Geom *geom);

int BKE_icon_ensure_studio_light(StudioLight *sl, int id_type);

#define ICON_RENDER_DEFAULT_HEIGHT 32
