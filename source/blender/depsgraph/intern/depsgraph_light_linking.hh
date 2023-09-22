/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include <cstdint>

#include "BLI_map.hh"

#include "BKE_light_linking.h" /* LightLinkingType */

#include "DNA_collection_types.h" /* eCollectionLightLinkingState */
#include "DNA_object_types.h"

struct Collection;
struct CollectionLightLinking;
struct Object;
struct Scene;

namespace blender::deg::light_linking {

namespace internal {

/* Set of light as seen from a receiver perspective. */
class LightSet {
 public:
  /* Maximum possible identifier of a light set. The identifier is 0-based.
   * The limitation is imposed by the fact that its identifier is converted to a bitmask. */
  static constexpr int MAX_ID = 63;

  /* Identifier of a light set which is not explicitly linked to anything. */
  static constexpr int DEFAULT_ID = 0;

  bool operator==(const LightSet &other) const;
  bool operator!=(const LightSet &other) const
  {
    return !(*this == other);
  }

  uint64_t hash() const;

  /* Lights which are explicitly included/excluded into the light set.
   *
   * The light is denoted as a bit mask of a light linking collection. This mask is allocated for
   * every unique light linking collection on an emitter. */
  uint64_t include_collection_mask = 0;
  uint64_t exclude_collection_mask = 0;
};

class EmitterSetMembership {
 public:
  /* Bitmask which indicates the emitter belongs to all light sets. */
  static constexpr uint64_t SET_MEMBERSHIP_ALL = ~uint64_t(0);

  /* Get final membership mask in the light sets, considering its inclusion and exclusion. */
  uint64_t get_mask() const;

  /* Bit masks of the emitter membership in the light sets. */
  uint64_t included_sets_mask = 0;
  uint64_t excluded_sets_mask = 0;
};

/* Packed information about emitter.
 * Emitter is actually corresponding to a light linking collection on an object. */
class EmitterData {
 public:
  /* Maximum possible identifier of a light linking collection. The identifier is 0-based.
   * The limitation is imposed by the fact that its identifier is converted to a bitmask. */
  static constexpr int MAX_COLLECTION_ID = 63;

  /* Mask of a light linking collection this emitter uses in its configuration.
   * A single bit is set in this bit-field which corresponds to an identifier of a light linking
   * collection in the scene. */
  uint64_t collection_mask = 0;

  /* Membership masks for the light and shadow linking. */
  EmitterSetMembership light_membership;
  EmitterSetMembership shadow_membership;
};

/* Helper class which deals with keeping per-emitter data. */
class EmitterDataMap {
  using MapType = Map<const Collection *, EmitterData>;

 public:
  explicit EmitterDataMap(const LightLinkingType link_type) : link_type_(link_type) {}

  /* Returns true if there is no information about emitters at all. */
  bool is_empty() const
  {
    return emitter_data_map_.is_empty();
  }

  /* Entirely clear the state, become ready for a new light linking relations build. */
  void clear();

  /* Ensure that the data exists for the given emitter.
   * The emitter must be original and have light linking collection.
   *
   * Note that there is limited number of emitters possible within a scene, When this number is
   * exceeded an error is printed and a nullptr is returned. */
  EmitterData *ensure_data_if_possible(const Scene &scene, const Object &emitter);

  /* Get emitter data for the given original or evaluated object.
   * If the light linking is not configured for this emitted nullptr is returned. */
  const EmitterData *get_data(const Object &emitter) const;

  /* Returns true if the underlying data of the light linking emitter has been handled, and there
   * is no need to handle the emitter.
   * The emitter must be original object. */
  bool can_skip_emitter(const Object &emitter) const;

  /* Returns an iterator over all emitter data in the map. */
  MapType::MutableValueIterator values()
  {
    return emitter_data_map_.values();
  }

 private:
  /* Get linked collection depending on whether this is emitter information for light or shadow
   * linking. */
  /* TODO(sergey): Check whether template specialization is preferred here. */
  inline const Collection *get_collection(const Object &emitter) const
  {
    return BKE_light_linking_collection_get(&emitter, link_type_);
  }

  LightLinkingType link_type_ = LIGHT_LINKING_RECEIVER;

  /* Emitter-centric information: indexed by an original emitter object, contains accumulated
   * information about emitter. */
  MapType emitter_data_map_;

  /* Next unique light linking collection ID. */
  uint64_t next_collection_id_ = 0;
};

/* Common part of receiver (for light linking) and blocker (for shadow lining) data. */
class LinkingData {
 public:
  explicit LinkingData(const LightLinkingType link_type) : link_type_(link_type) {}

  /* Entirely clear the state, become ready for a new light linking relations build. */
  void clear();

  /* Link the given object with the given light linking state. */
  void link_object(const EmitterData &emitter_data,
                   eCollectionLightLinkingState link_state,
                   const Object &object);

  /* Compute unique sets of emitters used by receivers or blockers.
   *
   * This must be called at the end of depsgraph relations build after all emitters have been
   * added, and before runtime data can be set as part of evaluation. */
  void end_build(const Scene &scene, EmitterDataMap &emitter_data_map);

  /* Get an unique index the given object is receiving light or casting shadow from.
   * The object can either be original or evaluated.
   *
   * If the object is not linked to any emitter LightSet::DEFAULT_ID is returned. */
  uint64_t get_light_set_for(const Object &object) const;

 private:
  /* Ensure that the light set exists for the given receiver/blocker object.
   * The object must be original. */
  LightSet &ensure_light_set_for(const Object &object);

  /* Update the emitter light/shadow set membership after the final unique light set identifier
   * is known.
   * The light_set_mask consists of a single bit set corresponding to the light set index. */
  void update_emitters_membership(EmitterDataMap &emitter_data_map,
                                  const LightSet &light_set,
                                  uint64_t light_set_mask);

  /* Clear data which is only needed during the build. */
  void clear_after_build();

  /* Get light set membership information of the emitter data depending whether this linking
   * data is a light or shadow linking. */
  /* TODO(sergey): Check whether template specialization is preferred here. */
  inline EmitterSetMembership &get_emitter_set_membership(EmitterData &emitter_data) const
  {
    if (link_type_ == LIGHT_LINKING_BLOCKER) {
      return emitter_data.shadow_membership;
    }

    return emitter_data.light_membership;
  }

  LightLinkingType link_type_ = LIGHT_LINKING_RECEIVER;

  /* Receiver/blocker-centric view of light sets: indexed by an original receiver object, contains
   * light set which defines from which emitters it receives light from or casts shadow when is lit
   * ny.
   *
   * NOTE: Only available during build. */
  Map<const Object *, LightSet> light_linked_sets_;

  /* Map from an original receiver/blocker object: map to index of light set for this
   * receiver/blocker. */
  /* TODO(sergey): What is the generic term for receiver and blocker which is less generic than
   * object? */
  Map<const Object *, uint64_t> object_light_sets_;
};

}  // namespace internal

/* Cached light linking evaluation data.
 *
 * This cache is only valid within a specific dependency graph, hence the dependency graph is
 * expected to own this cache.
 *
 * This cache takes care of making it efficient to lookup emitter masks, emitters which affect
 * given receiver and so on. */
class Cache {
  using EmitterData = internal::EmitterData;
  using EmitterDataMap = internal::EmitterDataMap;
  using LinkingData = internal::LinkingData;

 public:
  /* Entirely clear the cache.
   * Should be called whenever the dependency graph is being re-built, in the beginning of the
   * build process. */
  void clear();

  /* Add emitter to the cache.
   *
   * This call does nothing if the emitter does not have light configured linking (as in, if it
   * has light linking collection set to nullptr).
   *
   * The emitter must be original. This is asserted, but in release builds passing evaluated
   * object leads to an undefined behavior. */
  void add_emitter(const Scene &scene, const Object &emitter);

  /* Compute unique sets of emitters used by receivers and blockers.
   *
   * This must be called at the end of depsgraph relations build after all emitters have been
   * added, and before runtime data can be set as part of evaluation. */
  void end_build(const Scene &scene);

  /* Set runtime light linking data on evaluated object. */
  void eval_runtime_data(Object &object_eval) const;

 private:
  /* Add emitter information specific for light and shadow linking. */
  void add_light_linking_emitter(const Scene &scene, const Object &emitter);
  void add_shadow_linking_emitter(const Scene &scene, const Object &emitter);

  /* Add receiver or blocker object with the given light linking configuration.
   *
   * The term receiver here is meant in a wider meaning of it. For the light linking it is a
   * receiver of light, but for the shadow linking is it actually a shadow caster. */
  void add_receiver_object(const EmitterData &emitter_data,
                           const CollectionLightLinking &collection_light_linking,
                           const Object &receiver);
  void add_blocker_object(const EmitterData &emitter_data,
                          const CollectionLightLinking &collection_light_linking,
                          const Object &blocker);

  /* Returns true if there is light linking configuration in the scene. */
  bool has_light_linking() const
  {
    return !light_emitter_data_map_.is_empty() || !shadow_emitter_data_map_.is_empty();
  }

  /* Per-emitter light and shadow linking information. */
  EmitterDataMap light_emitter_data_map_{LIGHT_LINKING_RECEIVER};
  EmitterDataMap shadow_emitter_data_map_{LIGHT_LINKING_BLOCKER};

  /* Light and shadow linking data. */
  LinkingData light_linking_{LIGHT_LINKING_RECEIVER};
  LinkingData shadow_linking_{LIGHT_LINKING_BLOCKER};
};

}  // namespace blender::deg::light_linking
