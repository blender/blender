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
 * The Original Code is Copyright (C) 2006-2007 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_ICONS_H__
#define __BKE_ICONS_H__

/** \file BKE_icons.h
 *  \ingroup bke
 *
 * Resizable Icons for Blender
 */

typedef void (*DrawInfoFreeFP)(void *drawinfo);

enum {
	/** ID preview: obj is #ID. */
	ICON_DATA_ID = 0,
	/** Preview: obj is #PreviewImage */
	ICON_DATA_PREVIEW,
	/** 2D triangles: obj is #Icon_Geom */
	ICON_DATA_GEOM,
	/** Studiolight */
	ICON_DATA_STUDIOLIGHT,
	/** GPencil Layer color preview (annotations): obj is #bGPDlayer */
	ICON_DATA_GPLAYER,
};

struct Icon {
	void *drawinfo;
	/**
	 * Data defined by #obj_type
	 * \note for #ICON_DATA_GEOM the memory is owned by the icon,
	 * could be made into a flag if we want that to be optional.
	 */
	void *obj;
	char  obj_type;
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
	const unsigned char (*coords)[2];
	const unsigned char (*colors)[4];
	/* when not NULL, the memory of coords and colors is a sub-region of this pointer. */
	const void *mem;
};

typedef struct Icon Icon;

struct ImBuf;
struct PreviewImage;
struct ID;
struct StudioLight;
struct bGPDlayer;

enum eIconSizes;

void BKE_icons_init(int first_dyn_id);

/* return icon id for library object or create new icon if not found */
int BKE_icon_id_ensure(struct ID *id);

/* return icon id for Grease Pencil layer (color preview) or create new icon if not found */
int BKE_icon_gplayer_color_ensure(struct bGPDlayer *gpl);

int BKE_icon_preview_ensure(struct ID *id, struct PreviewImage *preview);

/* retrieve icon for id */
struct Icon *BKE_icon_get(const int icon_id);

/* set icon for id if not already defined */
/* used for inserting the internal icons */
void BKE_icon_set(const int icon_id, struct Icon *icon);

/* remove icon and free data if library object becomes invalid */
void BKE_icon_id_delete(struct ID *id);

bool BKE_icon_delete(const int icon_id);
bool BKE_icon_delete_unmanaged(const int icon_id);

/* report changes - icon needs to be recalculated */
void BKE_icon_changed(const int icon_id);

/* free all icons */
void BKE_icons_free(void);

/* free all icons marked for deferred deletion */
void BKE_icons_deferred_free(void);

/* free the preview image for use in list */
void BKE_previewimg_freefunc(void *link);

/* free the preview image */
void BKE_previewimg_free(struct PreviewImage **prv);

/* clear the preview image or icon, but does not free it */
void BKE_previewimg_clear(struct PreviewImage *prv);

/* clear the preview image or icon at a specific size */
void BKE_previewimg_clear_single(struct PreviewImage *prv, enum eIconSizes size);

/* get the preview from any pointer */
struct PreviewImage **BKE_previewimg_id_get_p(const struct ID *id);

/* free the preview image belonging to the id */
void BKE_previewimg_id_free(struct ID *id);

/* create a new preview image */
struct PreviewImage *BKE_previewimg_create(void);

/* create a copy of the preview image */
struct PreviewImage *BKE_previewimg_copy(const struct PreviewImage *prv);

void BKE_previewimg_id_copy(struct ID *new_id, const struct ID *old_id);

/* retrieve existing or create new preview image */
struct PreviewImage *BKE_previewimg_id_ensure(struct ID *id);

void BKE_previewimg_ensure(struct PreviewImage *prv, const int size);

struct PreviewImage *BKE_previewimg_cached_get(const char *name);

struct PreviewImage *BKE_previewimg_cached_ensure(const char *name);

struct PreviewImage *BKE_previewimg_cached_thumbnail_read(
        const char *name, const char *path, const int source, bool force_update);

void BKE_previewimg_cached_release(const char *name);
void BKE_previewimg_cached_release_pointer(struct PreviewImage *prv);

int BKE_icon_geom_ensure(struct Icon_Geom *geom);
struct Icon_Geom *BKE_icon_geom_from_memory(const uchar *data, size_t data_len);
struct Icon_Geom *BKE_icon_geom_from_file(const char *filename);

struct ImBuf *BKE_icon_geom_rasterize(
        const struct Icon_Geom *geom,
        const unsigned int size_x, const unsigned int size_y);

int BKE_icon_ensure_studio_light(struct StudioLight *sl, int id_type);

#define ICON_RENDER_DEFAULT_HEIGHT 32

#endif /*  __BKE_ICONS_H__ */
