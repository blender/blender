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
 * Contributor(s): Dalai Felinto
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_layer_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_LAYER_TYPES_H__
#define __DNA_LAYER_TYPES_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "DNA_freestyle_types.h"
#include "DNA_listBase.h"

typedef struct Base {
	struct Base *next, *prev;
	short flag;
	unsigned short local_view_bits;
	short sx, sy;
	struct Object *object;
	unsigned int lay DNA_DEPRECATED;
	int flag_legacy;
} Base;

typedef struct ViewLayerEngineData {
	struct ViewLayerEngineData *next, *prev;
	struct DrawEngineType *engine_type;
	void *storage;
	void (*free)(void *storage);
} ViewLayerEngineData;

typedef struct LayerCollection {
	struct LayerCollection *next, *prev;
	struct Collection *collection;
	struct SceneCollection *scene_collection DNA_DEPRECATED;
	short flag;
	short runtime_flag;
	short pad[2];
	/** Synced with collection->children. */
	ListBase layer_collections;
} LayerCollection;

typedef struct ViewLayer {
	struct ViewLayer *next, *prev;
	/** MAX_NAME. */
	char name[64];
	short flag;
	short runtime_flag;
	short pad[2];
	/** ObjectBase. */
	ListBase object_bases;
	/** Default allocated now. */
	struct SceneStats *stats;
	struct Base *basact;
	/** LayerCollection. */
	ListBase layer_collections;
	LayerCollection *active_collection;

	/* Old SceneRenderLayer data. */
	int layflag;
	/** Pass_xor has to be after passflag. */
	int passflag;
	float pass_alpha_threshold;
	int samples;

	struct Material *mat_override;
	/** Equivalent to datablocks ID properties. */
	struct IDProperty *id_properties;

	struct FreestyleConfig freestyle_config;

	/* Runtime data */
	/** ViewLayerEngineData. */
	ListBase drawdata;
	struct Base **object_bases_array;
	struct GHash *object_bases_hash;
} ViewLayer;

/* Base->flag */
enum {
	/* User controlled flags. */
	BASE_SELECTED         = (1 << 0), /* Object is selected. */
	BASE_HIDDEN           = (1 << 8), /* Object is hidden for editing. */

	/* Runtime evaluated flags. */
	BASE_VISIBLE          = (1 << 1), /* Object is enabled and visible. */
	BASE_SELECTABLE       = (1 << 2), /* Object can be selected. */
	BASE_FROMDUPLI        = (1 << 3), /* Object comes from duplicator. */
	/* BASE_DEPRECATED    = (1 << 4), */
	BASE_FROM_SET         = (1 << 5), /* Object comes from set. */
	BASE_ENABLED_VIEWPORT = (1 << 6), /* Object is enabled in viewport. */
	BASE_ENABLED_RENDER   = (1 << 7), /* Object is enabled in final render */
	BASE_ENABLED          = (1 << 9), /* Object is enabled. */
	BASE_HOLDOUT          = (1 << 10), /* Object masked out from render */
	BASE_INDIRECT_ONLY    = (1 << 11), /* Object only contributes indirectly to render */
};

/* LayerCollection->flag */
enum {
	/* LAYER_COLLECTION_DEPRECATED0 = (1 << 0), */
	/* LAYER_COLLECTION_DEPRECATED1 = (1 << 1), */
	/* LAYER_COLLECTION_DEPRECATED2 = (1 << 2), */
	/* LAYER_COLLECTION_DEPRECATED3 = (1 << 3), */
	LAYER_COLLECTION_EXCLUDE = (1 << 4),
	LAYER_COLLECTION_HOLDOUT = (1 << 5),
	LAYER_COLLECTION_INDIRECT_ONLY = (1 << 6),
};

/* Layer Collection->runtime_flag */
enum {
	LAYER_COLLECTION_HAS_OBJECTS = (1 << 0),
	LAYER_COLLECTION_HAS_VISIBLE_OBJECTS = (1 << 1),
	LAYER_COLLECTION_HAS_HIDDEN_OBJECTS = (1 << 2),
	LAYER_COLLECTION_HAS_ENABLED_OBJECTS = (1 << 3),
};

/* ViewLayer->flag */
enum {
	VIEW_LAYER_RENDER = (1 << 0),
	/* VIEW_LAYER_DEPRECATED  = (1 << 1), */
	VIEW_LAYER_FREESTYLE = (1 << 2),
};

/* ViewLayer->runtime_flag */
enum {
	VIEW_LAYER_HAS_HIDE = (1 << 0),
};

/****************************** Deprecated ******************************/

/* Compatibility with collections saved in early 2.8 versions,
 * used in file reading and versioning code. */
#define USE_COLLECTION_COMPAT_28

typedef struct SceneCollection {
	struct SceneCollection *next, *prev;
	/** MAX_NAME. */
	char name[64];
	/** For UI. */
	int active_object_index;
	short flag;
	char type;
	char pad;
	/** (Object *)LinkData->data. */
	ListBase objects;
	/** Nested collections. */
	ListBase scene_collections;
} SceneCollection;

#ifdef __cplusplus
}
#endif

#endif  /* __DNA_LAYER_TYPES_H__ */
