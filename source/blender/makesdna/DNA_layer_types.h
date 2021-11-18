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
 */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_freestyle_types.h"
#include "DNA_listBase.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Render-passes for EEVEE.
 * #ViewLayerEEVEE.render_passes
 */
typedef enum eViewLayerEEVEEPassType {
  EEVEE_RENDER_PASS_COMBINED = (1 << 0),
  EEVEE_RENDER_PASS_Z = (1 << 1),
  EEVEE_RENDER_PASS_MIST = (1 << 2),
  EEVEE_RENDER_PASS_NORMAL = (1 << 3),
  EEVEE_RENDER_PASS_DIFFUSE_LIGHT = (1 << 4),
  EEVEE_RENDER_PASS_DIFFUSE_COLOR = (1 << 5),
  EEVEE_RENDER_PASS_SPECULAR_LIGHT = (1 << 6),
  EEVEE_RENDER_PASS_SPECULAR_COLOR = (1 << 7),
  EEVEE_RENDER_PASS_UNUSED_8 = (1 << 8),
  EEVEE_RENDER_PASS_VOLUME_LIGHT = (1 << 9),
  EEVEE_RENDER_PASS_EMIT = (1 << 10),
  EEVEE_RENDER_PASS_ENVIRONMENT = (1 << 11),
  EEVEE_RENDER_PASS_SHADOW = (1 << 12),
  EEVEE_RENDER_PASS_AO = (1 << 13),
  EEVEE_RENDER_PASS_BLOOM = (1 << 14),
  EEVEE_RENDER_PASS_AOV = (1 << 15),
  EEVEE_RENDER_PASS_CRYPTOMATTE = (1 << 16),
} eViewLayerEEVEEPassType;
#define EEVEE_RENDER_PASS_MAX_BIT 17

/* #ViewLayerAOV.type */
typedef enum eViewLayerAOVType {
  AOV_TYPE_VALUE = 0,
  AOV_TYPE_COLOR = 1,
} eViewLayerAOVType;

/* #ViewLayerAOV.flag */
typedef enum eViewLayerAOVFlag {
  AOV_CONFLICT = (1 << 0),
} eViewLayerAOVFlag;

/* #ViewLayer.cryptomatte_flag */
typedef enum eViewLayerCryptomatteFlags {
  VIEW_LAYER_CRYPTOMATTE_OBJECT = (1 << 0),
  VIEW_LAYER_CRYPTOMATTE_MATERIAL = (1 << 1),
  VIEW_LAYER_CRYPTOMATTE_ASSET = (1 << 2),
  VIEW_LAYER_CRYPTOMATTE_ACCURATE = (1 << 3),
} eViewLayerCryptomatteFlags;
#define VIEW_LAYER_CRYPTOMATTE_ALL \
  (VIEW_LAYER_CRYPTOMATTE_OBJECT | VIEW_LAYER_CRYPTOMATTE_MATERIAL | VIEW_LAYER_CRYPTOMATTE_ASSET)

typedef struct Base {
  struct Base *next, *prev;

  /* Flags which are based on the collections flags evaluation, does not
   * include flags from object's restrictions. */
  short flag_from_collection;

  /* Final flags, including both accumulated collection flags and object's
   * restriction flags. */
  short flag;

  unsigned short local_view_bits;
  short sx, sy;
  char _pad1[6];
  struct Object *object;
  unsigned int lay DNA_DEPRECATED;
  int flag_legacy;
  unsigned short local_collections_bits;
  short _pad2[3];

  /* Pointer to an original base. Is initialized for evaluated view layer.
   * NOTE: Only allowed to be accessed from within active dependency graph. */
  struct Base *base_orig;
  void *_pad;
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
  char _pad[4];

  /** Synced with collection->children. */
  ListBase layer_collections;

  unsigned short local_collections_bits;
  short _pad2[3];
} LayerCollection;

/* Type containing EEVEE settings per view-layer */
typedef struct ViewLayerEEVEE {
  int render_passes;
  int _pad[1];
} ViewLayerEEVEE;

/* AOV Renderpass definition. */
typedef struct ViewLayerAOV {
  struct ViewLayerAOV *next, *prev;

  /* Name of the AOV */
  char name[64];
  int flag;
  /* Type of AOV (color/value)
   * matches `eViewLayerAOVType` */
  int type;
} ViewLayerAOV;
typedef struct ViewLayer {
  struct ViewLayer *next, *prev;
  /** MAX_NAME. */
  char name[64];
  short flag;
  char _pad[6];
  /** ObjectBase. */
  ListBase object_bases;
  /** Default allocated now. */
  struct SceneStats *stats;
  struct Base *basact;

  /** A view layer has one top level layer collection, because a scene has only one top level
   * collection. The layer_collections list always contains a single element. ListBase is
   * convenient when applying functions to all layer collections recursively. */
  ListBase layer_collections;
  LayerCollection *active_collection;

  /* Old SceneRenderLayer data. */
  int layflag;
  /** Pass_xor has to be after passflag. */
  int passflag;
  float pass_alpha_threshold;
  short cryptomatte_flag;
  short cryptomatte_levels;
  char _pad1[4];

  int samples;

  struct Material *mat_override;
  /** Equivalent to datablocks ID properties. */
  struct IDProperty *id_properties;

  struct FreestyleConfig freestyle_config;
  struct ViewLayerEEVEE eevee;

  /* List containing the `ViewLayerAOV`s */
  ListBase aovs;
  ViewLayerAOV *active_aov;

  /* Runtime data */
  /** ViewLayerEngineData. */
  ListBase drawdata;
  struct Base **object_bases_array;
  struct GHash *object_bases_hash;
} ViewLayer;

/* Base->flag */
enum {
  /* User controlled flags. */
  BASE_SELECTED = (1 << 0), /* Object is selected. */
  BASE_HIDDEN = (1 << 8),   /* Object is hidden for editing. */

  /* Runtime evaluated flags. */
  BASE_VISIBLE_DEPSGRAPH = (1 << 1), /* Object is enabled and visible for the depsgraph. */
  BASE_SELECTABLE = (1 << 2),        /* Object can be selected. */
  BASE_FROM_DUPLI = (1 << 3),        /* Object comes from duplicator. */
  BASE_VISIBLE_VIEWLAYER = (1 << 4), /* Object is enabled and visible for the viewlayer. */
  BASE_FROM_SET = (1 << 5),          /* Object comes from set. */
  BASE_ENABLED_VIEWPORT = (1 << 6),  /* Object is enabled in viewport. */
  BASE_ENABLED_RENDER = (1 << 7),    /* Object is enabled in final render */
  /* BASE_DEPRECATED          = (1 << 9), */
  BASE_HOLDOUT = (1 << 10),       /* Object masked out from render */
  BASE_INDIRECT_ONLY = (1 << 11), /* Object only contributes indirectly to render */
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
  LAYER_COLLECTION_HIDE = (1 << 7),
  LAYER_COLLECTION_PREVIOUSLY_EXCLUDED = (1 << 8),
};

/* Layer Collection->runtime_flag
 * Keep it synced with base->flag based on g_base_collection_flags. */
enum {
  LAYER_COLLECTION_HAS_OBJECTS = (1 << 0),
  /* LAYER_COLLECTION_VISIBLE_DEPSGRAPH = (1 << 1), */ /* UNUSED */
  LAYER_COLLECTION_HIDE_VIEWPORT = (1 << 2),
  LAYER_COLLECTION_VISIBLE_VIEW_LAYER = (1 << 4),
};

/* ViewLayer->flag */
enum {
  VIEW_LAYER_RENDER = (1 << 0),
  /* VIEW_LAYER_DEPRECATED  = (1 << 1), */
  VIEW_LAYER_FREESTYLE = (1 << 2),
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
  char _pad;
  /** (Object *)LinkData->data. */
  ListBase objects;
  /** Nested collections. */
  ListBase scene_collections;
} SceneCollection;

#ifdef __cplusplus
}
#endif
