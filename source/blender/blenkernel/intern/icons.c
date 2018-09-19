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
 *
 */

/** \file blender/blenkernel/intern/icons.c
 *  \ingroup bke
 */


#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_group_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"
#include "DNA_world_types.h"
#include "DNA_brush_types.h"

#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_linklist_lockfree.h"
#include "BLI_string.h"
#include "BLI_threads.h"

#include "BKE_icons.h"
#include "BKE_global.h" /* only for G.background test */

#include "BLI_sys_types.h" // for intptr_t support

#include "GPU_texture.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_thumbs.h"

/* GLOBALS */

static GHash *gIcons = NULL;

static int gNextIconId = 1;

static int gFirstIconId = 1;

static GHash *gCachedPreviews = NULL;

/* Queue of icons for deferred deletion. */
typedef struct DeferredIconDeleteNode {
	struct DeferredIconDeleteNode *next;
	int icon_id;
} DeferredIconDeleteNode;
static LockfreeLinkList g_icon_delete_queue;

static void icon_free(void *val)
{
	Icon *icon = val;

	if (icon) {
		if (icon->drawinfo_free) {
			icon->drawinfo_free(icon->drawinfo);
		}
		else if (icon->drawinfo) {
			MEM_freeN(icon->drawinfo);
		}
		MEM_freeN(icon);
	}
}

/* create an id for a new icon and make sure that ids from deleted icons get reused
 * after the integer number range is used up */
static int get_next_free_id(void)
{
	BLI_assert(BLI_thread_is_main());
	int startId = gFirstIconId;

	/* if we haven't used up the int number range, we just return the next int */
	if (gNextIconId >= gFirstIconId)
		return gNextIconId++;

	/* now we try to find the smallest icon id not stored in the gIcons hash */
	while (BLI_ghash_lookup(gIcons, POINTER_FROM_INT(startId)) && startId >= gFirstIconId)
		startId++;

	/* if we found a suitable one that isn't used yet, return it */
	if (startId >= gFirstIconId)
		return startId;

	/* fail */
	return 0;
}

void BKE_icons_init(int first_dyn_id)
{
	BLI_assert(BLI_thread_is_main());

	gNextIconId = first_dyn_id;
	gFirstIconId = first_dyn_id;

	if (!gIcons) {
		gIcons = BLI_ghash_int_new(__func__);
		BLI_linklist_lockfree_init(&g_icon_delete_queue);
	}

	if (!gCachedPreviews) {
		gCachedPreviews = BLI_ghash_str_new(__func__);
	}
}

void BKE_icons_free(void)
{
	BLI_assert(BLI_thread_is_main());

	if (gIcons) {
		BLI_ghash_free(gIcons, NULL, icon_free);
		gIcons = NULL;
	}

	if (gCachedPreviews) {
		BLI_ghash_free(gCachedPreviews, MEM_freeN, BKE_previewimg_freefunc);
		gCachedPreviews = NULL;
	}

	BLI_linklist_lockfree_free(&g_icon_delete_queue, MEM_freeN);
}

void BKE_icons_deferred_free(void)
{
	BLI_assert(BLI_thread_is_main());

	for (DeferredIconDeleteNode *node =
	             (DeferredIconDeleteNode *)BLI_linklist_lockfree_begin(&g_icon_delete_queue);
	     node != NULL;
	     node = node->next)
	{
		BLI_ghash_remove(gIcons, POINTER_FROM_INT(node->icon_id), NULL, icon_free);
	}
	BLI_linklist_lockfree_clear(&g_icon_delete_queue, MEM_freeN);
}

static PreviewImage *previewimg_create_ex(size_t deferred_data_size)
{
	PreviewImage *prv_img = NULL;
	int i;

	prv_img = MEM_mallocN(sizeof(PreviewImage) + deferred_data_size, "img_prv");
	memset(prv_img, 0, sizeof(*prv_img));  /* leave deferred data dirty */

	if (deferred_data_size) {
		prv_img->tag |= PRV_TAG_DEFFERED;
	}

	for (i = 0; i < NUM_ICON_SIZES; ++i) {
		prv_img->flag[i] |= PRV_CHANGED;
		prv_img->changed_timestamp[i] = 0;
	}
	return prv_img;
}

PreviewImage *BKE_previewimg_create(void)
{
	return previewimg_create_ex(0);
}

void BKE_previewimg_freefunc(void *link)
{
	PreviewImage *prv = (PreviewImage *)link;
	if (prv) {
		int i;

		for (i = 0; i < NUM_ICON_SIZES; ++i) {
			if (prv->rect[i]) {
				MEM_freeN(prv->rect[i]);
			}
			if (prv->gputexture[i])
				GPU_texture_free(prv->gputexture[i]);
		}

		MEM_freeN(prv);
	}
}

void BKE_previewimg_free(PreviewImage **prv)
{
	if (prv && (*prv)) {
		BKE_previewimg_freefunc(*prv);
		*prv = NULL;
	}
}

void BKE_previewimg_clear_single(struct PreviewImage *prv, enum eIconSizes size)
{
	MEM_SAFE_FREE(prv->rect[size]);
	if (prv->gputexture[size]) {
		GPU_texture_free(prv->gputexture[size]);
	}
	prv->h[size] = prv->w[size] = 0;
	prv->flag[size] |= PRV_CHANGED;
	prv->flag[size] &= ~PRV_USER_EDITED;
	prv->changed_timestamp[size] = 0;
}

void BKE_previewimg_clear(struct PreviewImage *prv)
{
	int i;
	for (i = 0; i < NUM_ICON_SIZES; ++i) {
		BKE_previewimg_clear_single(prv, i);
	}
}

PreviewImage *BKE_previewimg_copy(const PreviewImage *prv)
{
	PreviewImage *prv_img = NULL;
	int i;

	if (prv) {
		prv_img = MEM_dupallocN(prv);
		for (i = 0; i < NUM_ICON_SIZES; ++i) {
			if (prv->rect[i]) {
				prv_img->rect[i] = MEM_dupallocN(prv->rect[i]);
			}
			prv_img->gputexture[i] = NULL;
		}
	}
	return prv_img;
}

/** Duplicate preview image from \a id and clear icon_id, to be used by datablock copy functions. */
void BKE_previewimg_id_copy(ID *new_id, const ID *old_id)
{
	PreviewImage **old_prv_p = BKE_previewimg_id_get_p(old_id);
	PreviewImage **new_prv_p = BKE_previewimg_id_get_p(new_id);

	if (old_prv_p && *old_prv_p) {
		BLI_assert(new_prv_p != NULL && ELEM(*new_prv_p, NULL, *old_prv_p));
//		const int new_icon_id = get_next_free_id();

//		if (new_icon_id == 0) {
//			return;  /* Failure. */
//		}
		*new_prv_p = BKE_previewimg_copy(*old_prv_p);
		new_id->icon_id = (*new_prv_p)->icon_id = 0;
	}
}

PreviewImage **BKE_previewimg_id_get_p(const ID *id)
{
	switch (GS(id->name)) {
#define ID_PRV_CASE(id_code, id_struct) case id_code: { return &((id_struct *)id)->preview; } ((void)0)
		ID_PRV_CASE(ID_MA, Material);
		ID_PRV_CASE(ID_TE, Tex);
		ID_PRV_CASE(ID_WO, World);
		ID_PRV_CASE(ID_LA, Lamp);
		ID_PRV_CASE(ID_IM, Image);
		ID_PRV_CASE(ID_BR, Brush);
		ID_PRV_CASE(ID_OB, Object);
		ID_PRV_CASE(ID_GR, Group);
		ID_PRV_CASE(ID_SCE, Scene);
#undef ID_PRV_CASE
		default:
			break;
	}

	return NULL;
}

void BKE_previewimg_id_free(ID *id)
{
	PreviewImage **prv_p = BKE_previewimg_id_get_p(id);
	if (prv_p) {
		BKE_previewimg_free(prv_p);
	}
}

PreviewImage *BKE_previewimg_id_ensure(ID *id)
{
	PreviewImage **prv_p = BKE_previewimg_id_get_p(id);

	if (prv_p) {
		if (*prv_p == NULL) {
			*prv_p = BKE_previewimg_create();
		}
		return *prv_p;
	}

	return NULL;
}

PreviewImage *BKE_previewimg_cached_get(const char *name)
{
	return BLI_ghash_lookup(gCachedPreviews, name);
}

/**
 * Generate an empty PreviewImage, if not yet existing.
 */
PreviewImage *BKE_previewimg_cached_ensure(const char *name)
{
	PreviewImage *prv = NULL;
	void **key_p, **prv_p;

	if (!BLI_ghash_ensure_p_ex(gCachedPreviews, name, &key_p, &prv_p)) {
		*key_p = BLI_strdup(name);
		*prv_p = BKE_previewimg_create();
	}
	prv = *prv_p;
	BLI_assert(prv);

	return prv;
}

/**
 * Generate a PreviewImage from given file path, using thumbnails management, if not yet existing.
 */
PreviewImage *BKE_previewimg_cached_thumbnail_read(
        const char *name, const char *path, const int source, bool force_update)
{
	PreviewImage *prv = NULL;
	void **prv_p;

	prv_p = BLI_ghash_lookup_p(gCachedPreviews, name);

	if (prv_p) {
		prv = *prv_p;
		BLI_assert(prv);
	}

	if (prv && force_update) {
		const char *prv_deferred_data = PRV_DEFERRED_DATA(prv);
		if (((int)prv_deferred_data[0] == source) && STREQ(&prv_deferred_data[1], path)) {
			/* If same path, no need to re-allocate preview, just clear it up. */
			BKE_previewimg_clear(prv);
		}
		else {
			BKE_previewimg_free(&prv);
		}
	}

	if (!prv) {
		/* We pack needed data for lazy loading (source type, in a single char, and path). */
		const size_t deferred_data_size = strlen(path) + 2;
		char *deferred_data;

		prv = previewimg_create_ex(deferred_data_size);
		deferred_data = PRV_DEFERRED_DATA(prv);
		deferred_data[0] = source;
		memcpy(&deferred_data[1], path, deferred_data_size - 1);

		force_update = true;
	}

	if (force_update) {
		if (prv_p) {
			*prv_p = prv;
		}
		else {
			BLI_ghash_insert(gCachedPreviews, BLI_strdup(name), prv);
		}
	}

	return prv;
}

void BKE_previewimg_cached_release_pointer(PreviewImage *prv)
{
	if (prv) {
		if (prv->tag & PRV_TAG_DEFFERED_RENDERING) {
			/* We cannot delete the preview while it is being loaded in another thread... */
			prv->tag |= PRV_TAG_DEFFERED_DELETE;
			return;
		}
		if (prv->icon_id) {
			BKE_icon_delete(prv->icon_id);
		}
		BKE_previewimg_freefunc(prv);
	}
}

void BKE_previewimg_cached_release(const char *name)
{
	PreviewImage *prv = BLI_ghash_popkey(gCachedPreviews, name, MEM_freeN);

	BKE_previewimg_cached_release_pointer(prv);
}

/** Handle deferred (lazy) loading/generation of preview image, if needed.
 * For now, only used with file thumbnails. */
void BKE_previewimg_ensure(PreviewImage *prv, const int size)
{
	if ((prv->tag & PRV_TAG_DEFFERED) != 0) {
		const bool do_icon = ((size == ICON_SIZE_ICON) && !prv->rect[ICON_SIZE_ICON]);
		const bool do_preview = ((size == ICON_SIZE_PREVIEW) && !prv->rect[ICON_SIZE_PREVIEW]);

		if (do_icon || do_preview) {
			ImBuf *thumb;
			char *prv_deferred_data = PRV_DEFERRED_DATA(prv);
			int source =  prv_deferred_data[0];
			char *path = &prv_deferred_data[1];
			int icon_w, icon_h;

			thumb = IMB_thumb_manage(path, THB_LARGE, source);

			if (thumb) {
				/* PreviewImage assumes premultiplied alhpa... */
				IMB_premultiply_alpha(thumb);

				if (do_preview) {
					prv->w[ICON_SIZE_PREVIEW] = thumb->x;
					prv->h[ICON_SIZE_PREVIEW] = thumb->y;
					prv->rect[ICON_SIZE_PREVIEW] = MEM_dupallocN(thumb->rect);
					prv->flag[ICON_SIZE_PREVIEW] &= ~(PRV_CHANGED | PRV_USER_EDITED);
				}
				if (do_icon) {
					if (thumb->x > thumb->y) {
						icon_w = ICON_RENDER_DEFAULT_HEIGHT;
						icon_h = (thumb->y * icon_w) / thumb->x + 1;
					}
					else if (thumb->x < thumb->y) {
						icon_h = ICON_RENDER_DEFAULT_HEIGHT;
						icon_w = (thumb->x * icon_h) / thumb->y + 1;
					}
					else {
						icon_w = icon_h = ICON_RENDER_DEFAULT_HEIGHT;
					}

					IMB_scaleImBuf(thumb, icon_w, icon_h);
					prv->w[ICON_SIZE_ICON] = icon_w;
					prv->h[ICON_SIZE_ICON] = icon_h;
					prv->rect[ICON_SIZE_ICON] = MEM_dupallocN(thumb->rect);
					prv->flag[ICON_SIZE_ICON] &= ~(PRV_CHANGED | PRV_USER_EDITED);
				}
				IMB_freeImBuf(thumb);
			}
		}
	}
}

void BKE_icon_changed(const int icon_id)
{
	BLI_assert(BLI_thread_is_main());

	Icon *icon = NULL;

	if (!icon_id || G.background) return;

	icon = BLI_ghash_lookup(gIcons, POINTER_FROM_INT(icon_id));

	if (icon) {
		/* We *only* expect ID-tied icons here, not non-ID icon/preview! */
		BLI_assert(icon->id_type != 0);

		/* Do not enforce creation of previews for valid ID types using BKE_previewimg_id_ensure() here ,
		 * we only want to ensure *existing* preview images are properly tagged as changed/invalid, that's all. */
		PreviewImage **p_prv = BKE_previewimg_id_get_p((ID *)icon->obj);

		/* If we have previews, they all are now invalid changed. */
		if (p_prv && *p_prv) {
			int i;
			for (i = 0; i < NUM_ICON_SIZES; ++i) {
				(*p_prv)->flag[i] |= PRV_CHANGED;
				(*p_prv)->changed_timestamp[i]++;
			}
		}
	}
}

static int icon_id_ensure_create_icon(struct ID *id)
{
	BLI_assert(BLI_thread_is_main());

	Icon *new_icon = NULL;

	new_icon = MEM_mallocN(sizeof(Icon), __func__);

	new_icon->obj = id;
	new_icon->id_type = GS(id->name);

	/* next two lines make sure image gets created */
	new_icon->drawinfo = NULL;
	new_icon->drawinfo_free = NULL;

	BLI_ghash_insert(gIcons, POINTER_FROM_INT(id->icon_id), new_icon);

	return id->icon_id;
}

int BKE_icon_id_ensure(struct ID *id)
{
	/* Never handle icons in non-main thread! */
	BLI_assert(BLI_thread_is_main());

	if (!id || G.background) {
		return 0;
	}

	if (id->icon_id)
		return id->icon_id;

	id->icon_id = get_next_free_id();

	if (!id->icon_id) {
		printf("%s: Internal error - not enough IDs\n", __func__);
		return 0;
	}

	/* Ensure we synchronize ID icon_id with its previewimage if it has one. */
	PreviewImage **p_prv = BKE_previewimg_id_get_p(id);
	if (p_prv && *p_prv) {
		BLI_assert(ELEM((*p_prv)->icon_id, 0, id->icon_id));
		(*p_prv)->icon_id = id->icon_id;
	}

	return icon_id_ensure_create_icon(id);
}

/**
 * Return icon id of given preview, or create new icon if not found.
 */
int BKE_icon_preview_ensure(ID *id, PreviewImage *preview)
{
	Icon *new_icon = NULL;

	if (!preview || G.background)
		return 0;

	if (id) {
		BLI_assert(BKE_previewimg_id_ensure(id) == preview);
	}

	if (preview->icon_id) {
		BLI_assert(!id || !id->icon_id || id->icon_id == preview->icon_id);
		return preview->icon_id;
	}

	if (id && id->icon_id) {
		preview->icon_id = id->icon_id;
		return preview->icon_id;
	}

	preview->icon_id = get_next_free_id();

	if (!preview->icon_id) {
		printf("%s: Internal error - not enough IDs\n", __func__);
		return 0;
	}

	/* Ensure we synchronize ID icon_id with its previewimage if available, and generate suitable 'ID' icon. */
	if (id) {
		id->icon_id = preview->icon_id;
		return icon_id_ensure_create_icon(id);
	}

	new_icon = MEM_mallocN(sizeof(Icon), __func__);

	new_icon->obj = preview;
	new_icon->id_type = 0;  /* Special, tags as non-ID icon/preview. */

	/* next two lines make sure image gets created */
	new_icon->drawinfo = NULL;
	new_icon->drawinfo_free = NULL;

	BLI_ghash_insert(gIcons, POINTER_FROM_INT(preview->icon_id), new_icon);

	return preview->icon_id;
}

Icon *BKE_icon_get(const int icon_id)
{
	BLI_assert(BLI_thread_is_main());

	Icon *icon = NULL;

	icon = BLI_ghash_lookup(gIcons, POINTER_FROM_INT(icon_id));

	if (!icon) {
		printf("%s: Internal error, no icon for icon ID: %d\n", __func__, icon_id);
		return NULL;
	}

	return icon;
}

void BKE_icon_set(const int icon_id, struct Icon *icon)
{
	BLI_assert(BLI_thread_is_main());

	void **val_p;

	if (BLI_ghash_ensure_p(gIcons, POINTER_FROM_INT(icon_id), &val_p)) {
		printf("%s: Internal error, icon already set: %d\n", __func__, icon_id);
		return;
	}

	*val_p = icon;
}

static void icon_add_to_deferred_delete_queue(int icon_id)
{
	DeferredIconDeleteNode *node =
	        MEM_mallocN(sizeof(DeferredIconDeleteNode), __func__);
	node->icon_id = icon_id;
	BLI_linklist_lockfree_insert(&g_icon_delete_queue,
	                             (LockfreeLinkNode *)node);
}

void BKE_icon_id_delete(struct ID *id)
{
	const int icon_id = id->icon_id;
	if (!icon_id) return;  /* no icon defined for library object */
	id->icon_id = 0;

	if (!BLI_thread_is_main()) {
		icon_add_to_deferred_delete_queue(icon_id);
		return;
	}

	BKE_icons_deferred_free();
	BLI_ghash_remove(gIcons, POINTER_FROM_INT(icon_id), NULL, icon_free);
}

/**
 * Remove icon and free data.
 */
void BKE_icon_delete(const int icon_id)
{
	Icon *icon;

	if (!icon_id) return;  /* no icon defined for library object */

	icon = BLI_ghash_popkey(gIcons, POINTER_FROM_INT(icon_id), NULL);

	if (icon) {
		if (icon->id_type != 0) {
			((ID *)(icon->obj))->icon_id = 0;
		}
		else {
			((PreviewImage *)(icon->obj))->icon_id = 0;
		}
		icon_free(icon);
	}
}
