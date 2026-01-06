/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 *
 * \brief Object groups, one object can be in many groups at once.
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_defs.h"
#include "DNA_listBase.h"

namespace blender {

namespace bke {
struct CollectionRuntime;
}  // namespace bke

struct Collection;
struct Object;
struct GHash;

enum IOHandlerPanelFlag {
  IO_HANDLER_PANEL_OPEN = 1 << 0,
};

/* Light linking state of object or collection: defines how they react to the emitters in the
 * scene. See the comment for the link_state in the CollectionLightLinking for the details. */
enum eCollectionLightLinkingState {
  COLLECTION_LIGHT_LINKING_STATE_INCLUDE = 0,
  COLLECTION_LIGHT_LINKING_STATE_EXCLUDE = 1,
};

enum eCollectionLineArt_Usage {
  COLLECTION_LRT_INCLUDE = 0,
  COLLECTION_LRT_OCCLUSION_ONLY = (1 << 0),
  COLLECTION_LRT_EXCLUDE = (1 << 1),
  COLLECTION_LRT_INTERSECTION_ONLY = (1 << 2),
  COLLECTION_LRT_NO_INTERSECTION = (1 << 3),
  COLLECTION_LRT_FORCE_INTERSECTION = (1 << 4),
};

enum eCollectionLineArt_Flags {
  COLLECTION_LRT_USE_INTERSECTION_MASK = (1 << 0),
  COLLECTION_LRT_USE_INTERSECTION_PRIORITY = (1 << 1),
};

/** #Collection.flag */
enum {
  /** Disable in viewports. */
  COLLECTION_HIDE_VIEWPORT = (1 << 0),
  /** Not selectable in viewport. */
  COLLECTION_HIDE_SELECT = (1 << 1),
  // COLLECTION_DISABLED_DEPRECATED = (1 << 2), /* DIRTY */
  /** Disable in renders. */
  COLLECTION_HIDE_RENDER = (1 << 3),
  /** Runtime: object_cache is populated. */
  COLLECTION_HAS_OBJECT_CACHE = (1 << 4),
  /** Is master collection embedded in the scene. */
  COLLECTION_IS_MASTER = (1 << 5),
  /** for object_cache_instanced. */
  COLLECTION_HAS_OBJECT_CACHE_INSTANCED = (1 << 6),
};

#define COLLECTION_FLAG_ALL_RUNTIME \
  (COLLECTION_HAS_OBJECT_CACHE | COLLECTION_HAS_OBJECT_CACHE_INSTANCED)

/** #Collection.color_tag */
enum CollectionColorTag {
  COLLECTION_COLOR_NONE = -1,
  COLLECTION_COLOR_01,
  COLLECTION_COLOR_02,
  COLLECTION_COLOR_03,
  COLLECTION_COLOR_04,
  COLLECTION_COLOR_05,
  COLLECTION_COLOR_06,
  COLLECTION_COLOR_07,
  COLLECTION_COLOR_08,

  COLLECTION_COLOR_TOT,
};

/* Light linking relation of a collection or an object. */
struct CollectionLightLinking {
  /* Light and shadow linking configuration, an enumerator of eCollectionLightLinkingState.
   * The meaning depends on whether the collection is specified as a light or shadow linking on the
   * Object's LightLinking.
   *
   * For the light linking collection:
   *
   *   - INCLUDE: the receiver is included into the light linking and is only receiving lights from
   *     emitters which include it in their light linking collections. The receiver is not affected
   *     by regular scene lights.
   *
   *   - EXCLUDE: the receiver does not receive light from this emitter, but is lit by regular
   *     lights in the scene or by emitters which are linked to it via INCLUDE on their
   *     light_state.
   *
   * For the shadow linking collection:
   *
   *   - INCLUDE: the collection or object casts shadows from the emitter. It does not cast shadow
   *     from light sources which do not have INCLUDE on their light linking configuration for it.
   *
   *   - EXCLUDE: the collection or object does not cast shadow when lit by this emitter, but does
   *     for other light sources in the scene. */
  uint8_t link_state = 0;

  uint8_t _pad[3] = {};
};

struct CollectionObject {
  struct CollectionObject *next = nullptr, *prev = nullptr;
  struct Object *ob = nullptr;

  CollectionLightLinking light_linking;
  int _pad = {};
};

struct CollectionChild {
  struct CollectionChild *next = nullptr, *prev = nullptr;
  struct Collection *collection = nullptr;

  CollectionLightLinking light_linking;
  int _pad = {};
};

/* Collection IO property storage and access. */
struct CollectionExport {
  struct CollectionExport *next = nullptr, *prev = nullptr;

  /** Identifier that matches the #FileHandlerType.idname. */
  char fh_idname[64] = "";
  char name[64] = "";

  IDProperty *export_properties = nullptr;
  uint32_t flag = 0;

  uint32_t _pad0 = {};
};

struct Collection {
#ifdef __cplusplus
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_GR;
#endif

  ID id;

  /** The ID owning this collection, in case it is an embedded one. */
  ID *owner_id = nullptr;

  /** CollectionObject. */
  ListBaseT<CollectionObject> gobject = {nullptr, nullptr};
  /** CollectionChild. */
  ListBaseT<CollectionChild> children = {nullptr, nullptr};

  char _pad0[4] = {};

  int active_exporter_index = 0;
  ListBaseT<CollectionExport> exporters = {nullptr, nullptr};

  struct PreviewImage *preview = nullptr;

  DNA_DEPRECATED unsigned int layer = 0;
  float instance_offset[3] = {};

  uint8_t flag = 0;
  int8_t color_tag = COLLECTION_COLOR_NONE;

  char _pad1[2] = {};

  uint8_t lineart_usage = 0; /* #eCollectionLineArt_Usage */
  uint8_t lineart_flags = 0; /* #eCollectionLineArt_Flags */
  uint8_t lineart_intersection_mask = 0;
  uint8_t lineart_intersection_priority = 0;

  DNA_DEPRECATED struct ViewLayer *view_layer = nullptr;

  /* Keep last. */
  bke::CollectionRuntime *runtime = nullptr;
};

}  // namespace blender
