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
	short refcount;
	short sx, sy;
	struct Object *object;
	unsigned int lay;
	int flag_legacy;
	struct IDProperty *collection_properties; /* used by depsgraph, flushed from collection-tree */
} Base;

typedef struct CollectionOverride {
	struct CollectionOverride *next, *prev;
	char name[64]; /* MAX_NAME */
	/* TODO proper data */
} CollectionOverride;

typedef struct ViewLayerEngineData {
	struct ViewLayerEngineData *next, *prev;
	struct DrawEngineType *engine_type;
	void *storage;
	void (*free)(void *storage);
} ViewLayerEngineData;

typedef struct LayerCollection {
	struct LayerCollection *next, *prev;
	struct SceneCollection *scene_collection;
	short flag;
	/* TODO(sergey): Get rid of this once we've got CoW in DEG, */
	short flag_evaluated;
	short pad[2];
	ListBase object_bases;      /* (ObjectBase *)LinkData->data - synced with collection->objects and collection->filter_objects */
	ListBase overrides;
	ListBase layer_collections; /* synced with collection->collections */
	struct IDProperty *properties;  /* overrides */
	struct IDProperty *properties_evaluated;
} LayerCollection;

typedef struct ViewLayer {
	struct ViewLayer *next, *prev;
	char name[64]; /* MAX_NAME */
	short active_collection;
	short flag;
	short pad[2];
	ListBase object_bases;      /* ObjectBase */
	struct SceneStats *stats;   /* default allocated now */
	struct Base *basact;
	ListBase layer_collections; /* LayerCollection */
	struct IDProperty *properties;  /* overrides */
	struct IDProperty *properties_evaluated;

	/* Old SceneRenderLayer data. */
	int layflag;
	int passflag;			/* pass_xor has to be after passflag */
	int pass_xor;
	float pass_alpha_threshold;

	struct IDProperty *id_properties; /* Equivalent to datablocks ID properties. */

	struct FreestyleConfig freestyle_config;

	/* Runtime data */
	ListBase drawdata;    /* ViewLayerEngineData */
} ViewLayer;

typedef struct SceneCollection {
	struct SceneCollection *next, *prev;
	char name[64]; /* MAX_NAME */
	char filter[64]; /* MAX_NAME */
	int active_object_index; /* for UI */
	char type;
	char pad[3];
	ListBase objects;           /* (Object *)LinkData->data */
	ListBase filter_objects;    /* (Object *)LinkData->data */
	ListBase scene_collections; /* nested collections */
} SceneCollection;

/* Base->flag */
enum {
	BASE_SELECTED         = (1 << 0),
	BASE_VISIBLED         = (1 << 1),
	BASE_SELECTABLED      = (1 << 2),
	BASE_FROMDUPLI        = (1 << 3),
	BASE_DIRTY_ENGINE_SETTINGS = (1 << 4),
	BASE_FROM_SET         = (1 << 5), /* To be set only by the depsgraph */
};

/* LayerCollection->flag */
enum {
	COLLECTION_VISIBLE    = (1 << 0),
	COLLECTION_SELECTABLE = (1 << 1),
	COLLECTION_DISABLED   = (1 << 2),
};

/* ViewLayer->flag */
enum {
	VIEW_LAYER_RENDER = (1 << 0),
	VIEW_LAYER_ENGINE_DIRTY  = (1 << 1),
	VIEW_LAYER_FREESTYLE = (1 << 2),
};

/* SceneCollection->type */
enum {
	COLLECTION_TYPE_NONE =  0,
	COLLECTION_TYPE_GROUP_INTERNAL = 1,
};

/* *************************************************************** */
/* Engine Settings */

/* CollectionEngineSettings->type */
typedef enum CollectionEngineSettingsType {
	COLLECTION_MODE_NONE = 0,
	COLLECTION_MODE_OBJECT = 1,
	COLLECTION_MODE_EDIT = 2,
	COLLECTION_MODE_PAINT_WEIGHT = 5,
	COLLECTION_MODE_PAINT_VERTEX = 6,
} CollectionModeSettingsType;

/* *************************************************************** */


#ifdef __cplusplus
}
#endif

#endif  /* __DNA_LAYER_TYPES_H__ */

