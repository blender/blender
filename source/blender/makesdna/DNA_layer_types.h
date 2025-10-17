/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_freestyle_types.h"
#include "DNA_listBase.h"

#include "BLI_enum_flags.hh"

/**
 * Render-passes for EEVEE.
 * #ViewLayerEEVEE.render_passes
 */
typedef enum eViewLayerEEVEEPassType {
  EEVEE_RENDER_PASS_COMBINED = (1 << 0),
  EEVEE_RENDER_PASS_DEPTH = (1 << 1),
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
  EEVEE_RENDER_PASS_UNUSED_14 = (1 << 14), /* EEVEE_RENDER_PASS_BLOOM */
  EEVEE_RENDER_PASS_AOV = (1 << 15),
  /*
   * TODO(@jbakker): Clean up conflicting bits after EEVEE has been removed.
   * #EEVEE_RENDER_PASS_CRYPTOMATTE is for EEVEE, `EEVEE_RENDER_PASS_CRYTPOMATTE_*` are for
   * EEVEE-Next.
   */
  EEVEE_RENDER_PASS_CRYPTOMATTE = (1 << 16),
  EEVEE_RENDER_PASS_CRYPTOMATTE_OBJECT = (1 << 16),
  EEVEE_RENDER_PASS_CRYPTOMATTE_ASSET = (1 << 17),
  EEVEE_RENDER_PASS_CRYPTOMATTE_MATERIAL = (1 << 18),
  EEVEE_RENDER_PASS_VECTOR = (1 << 19),
  EEVEE_RENDER_PASS_TRANSPARENT = (1 << 20),
  EEVEE_RENDER_PASS_POSITION = (1 << 21),
} eViewLayerEEVEEPassType;
#define EEVEE_RENDER_PASS_MAX_BIT 21
ENUM_OPERATORS(eViewLayerEEVEEPassType)

/* #ViewLayer::grease_pencil_flags */
typedef enum eViewLayerGreasePencilFlags {
  GREASE_PENCIL_AS_SEPARATE_PASS = (1 << 0),
} eViewLayerGreasePencilFlags;

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
ENUM_OPERATORS(eViewLayerCryptomatteFlags)
#define VIEW_LAYER_CRYPTOMATTE_ALL \
  (VIEW_LAYER_CRYPTOMATTE_OBJECT | VIEW_LAYER_CRYPTOMATTE_MATERIAL | VIEW_LAYER_CRYPTOMATTE_ASSET)

typedef struct Base {
  struct Base *next, *prev;

  struct Object *object;

  /* Pointer to an original base. Is initialized for evaluated view layer.
   * NOTE: Only allowed to be accessed from within active dependency graph. */
  struct Base *base_orig;

  unsigned int lay DNA_DEPRECATED;
  /* Final flags, including both accumulated collection flags and object's
   * restriction flags. */
  short flag;
  /* Flags which are based on the collections flags evaluation, does not
   * include flags from object's restrictions. */
  short flag_from_collection;
  short flag_legacy;
  unsigned short local_view_bits;
  unsigned short local_collections_bits;
  char _pad1[2];
} Base;

typedef struct LayerCollection {
  struct LayerCollection *next, *prev;
  struct Collection *collection;
  void *_pad1;
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
  float ambient_occlusion_distance;
} ViewLayerEEVEE;

/** AOV Render-pass definition. */
typedef struct ViewLayerAOV {
  struct ViewLayerAOV *next, *prev;

  /* Name of the AOV */
  char name[64];
  int flag;
  /* Type of AOV (color/value)
   * matches `eViewLayerAOVType` */
  int type;
} ViewLayerAOV;

/** Light-group Render-pass definition. */
typedef struct ViewLayerLightgroup {
  struct ViewLayerLightgroup *next, *prev;

  /* Name of the Light-group. */
  char name[64];
} ViewLayerLightgroup;

/* Light-group membership information. */
typedef struct LightgroupMembership {
  /* Name of the Light-group. */
  char name[64];
} LightgroupMembership;

typedef struct ViewLayer {
  struct ViewLayer *next, *prev;
  char name[/*MAX_NAME*/ 64];
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
  int grease_pencil_flags;

  int samples;

  struct Material *mat_override;
  struct World *world_override;
  /** Equivalent to data-blocks user-defined ID properties. */
  struct IDProperty *id_properties;
  /** Equivalent to data-blocks system-defined ID properties. */
  struct IDProperty *system_properties;

  struct FreestyleConfig freestyle_config;
  struct ViewLayerEEVEE eevee;

  /** List containing #ViewLayerAOV. */
  ListBase aovs;
  ViewLayerAOV *active_aov;

  /** List containing #ViewLayerLightgroup. */
  ListBase lightgroups;
  ViewLayerLightgroup *active_lightgroup;

  /* Runtime data */
  struct Base **object_bases_array;
  struct GHash *object_bases_hash;
} ViewLayer;

/* Base->flag */
enum {
  /* User controlled flags. */
  BASE_SELECTED = (1 << 0), /* Object is selected. */
  BASE_HIDDEN = (1 << 8),   /* Object is hidden for editing. */

  /* Runtime evaluated flags. */

  /* Object is enabled and potentially visible in a viewport. Layer collection
   * visibility, local collection visibility, and local view are not part of this
   * and may cause the object to be hidden depending on the 3D viewport settings.
   *
   * Objects with this flag will be considered visible by the viewport depsgraph
   * and be evaluated as a result.
   *
   * This implies BASE_ENABLED_VIEWPORT. */
  BASE_ENABLED_AND_MAYBE_VISIBLE_IN_VIEWPORT = (1 << 1),

  /* Object can be selected. */
  BASE_SELECTABLE = (1 << 2),

  /* Object comes from a duplicator. */
  BASE_FROM_DUPLI = (1 << 3),

  /* Object is enabled and visible in a viewport with default viewport settings,
   * (so without any local view or local collection visibility overrides). Used
   * when editors other than the 3D viewport need to know if an object is visible. */
  BASE_ENABLED_AND_VISIBLE_IN_DEFAULT_VIEWPORT = (1 << 4),

  /* Object comes from a scene set. */
  BASE_FROM_SET = (1 << 5),

  /* Object is enabled for viewport or final render respectively. Only enabled
   * objects can be pulled into the depsgraph for evaluation, either through being
   * directly visible, as a dependency of another object, or as part of colliders
   * and effectors for physics. */
  BASE_ENABLED_VIEWPORT = (1 << 6),
  BASE_ENABLED_RENDER = (1 << 7),

  /* BASE_DEPRECATED          = (1 << 9), */

  /* Object masked out from render */
  BASE_HOLDOUT = (1 << 10),
  /* Object only contributes indirectly to render */
  BASE_INDIRECT_ONLY = (1 << 11),
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
  VIEW_LAYER_OUT_OF_SYNC = (1 << 3),
  VIEW_LAYER_HAS_EXPORT_COLLECTIONS = (1 << 4),
};
