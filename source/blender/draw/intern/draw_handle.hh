/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup draw
 *
 * A unique identifier for each object component.
 * It is used to access each component data such as matrices and object attributes.
 * It is valid only for the current draw, it is not persistent.
 *
 * The most significant bit is used to encode if the object needs to invert the front face winding
 * because of its object matrix handedness. This is handy because this means sorting inside
 * #DrawGroup command will put all inverted commands last.
 *
 * Default value of 0 points toward an non-cull-able object with unit bounding box centered at
 * the origin.
 */

#include "BKE_duplilist.hh"
#include "draw_shader_shared.hh"

/* random */
#include "BLI_hash.h"

/* find_rgba_attribute */
#include "GPU_material.hh"

/* particles_matrix */
#include "BLI_math_matrix.hh"
#include "DNA_collection_types.h"

/* parent_is_in_edit_paint_mode */
#include "BKE_context.hh"
#include "BKE_paint.hh"
#include "DNA_layer_types.h"
#include "DRW_render.hh"

/* ObjectKey */
#include "DEG_depsgraph_query.hh"

struct DupliCacheManager;

namespace blender::draw {

/**
 * Index for getting a specific resource from the Draw Manager resource arrays.
 * (e.g. object matrices)
 * Last bit contains handedness.
 *
 * NOTE: From the draw_pass and draw_command perspective, the 0 index is still valid and points to
 * default initialized Manager resources. Valid ResourceHandles start at index 1.
 */
struct ResourceIndex {
  uint32_t raw;

  ResourceIndex() = default;
  ResourceIndex(uint raw_) : raw(raw_) {};
  ResourceIndex(uint index, bool inverted_handedness)
  {
    raw = index;
    SET_FLAG_FROM_TEST(raw, inverted_handedness, 0x80000000u);
  }

  bool has_inverted_handedness() const
  {
    return (raw & 0x80000000u) != 0;
  }

  uint resource_index() const
  {
    return (raw & 0x7FFFFFFFu);
  }
};

/**
 * Refers to a range of contiguous indices in the Draw Manager resource arrays.
 * Typically used to render instances of an object, but can represent a single instance too.
 * The associated objects must share handedness and state so they can be rendered together.
 */
struct ResourceIndexRange {
  /* First handle in the range. */
  ResourceIndex first = 0;
  /* Number of handles in the range. */
  uint32_t count = 1;

  ResourceIndexRange() = default;
  ResourceIndexRange(ResourceIndex index) : first(index), count(1) {}
  ResourceIndexRange(ResourceIndex index, uint len) : first(index), count(len) {}

  bool has_inverted_handedness() const
  {
    return first.has_inverted_handedness();
  }

  IndexRange index_range() const
  {
    BLI_assert(count > 0);
    BLI_assert(first.raw != 0 || count == 1);
    return {first.raw, count};
  }
};

/**
 * Safety wrapper around ResourceIndex, meant to be used by engine code.
 * Valid handles can only be created by the Draw Manager.
 *
 * NOTE: This class is deprecated.
 * Some Draw Manager functions can't work with ranged synchronization and returns ResourceHandles
 * for clarity, but engine code should always use ResourceHandleRange.
 */
class ResourceHandle {
  friend class Manager;
  friend class ResourceHandleRange;

  ResourceIndex index_ = {};

  ResourceHandle(uint raw) : index_(raw) {}
  ResourceHandle(uint index, bool inverted_handedness) : index_(index, inverted_handedness) {}

 public:
  ResourceHandle() = default;

  bool is_valid() const
  {
    return index_.raw != 0;
  }

  bool has_inverted_handedness() const
  {
    return index_.has_inverted_handedness();
  }

  uint resource_index() const
  {
    return index_.resource_index();
  }

  operator ResourceIndex() const
  {
    BLI_assert(is_valid());
    return index_;
  }
};

/**
 * Safety wrapper around ResourceIndexRange, meant to be used by engine code.
 * Valid handles can only be created by the Draw Manager.
 */
class ResourceHandleRange {
  friend class Manager;

  ResourceIndexRange index_ = {};

  ResourceHandleRange(ResourceHandle handle, uint len) : index_(handle.index_, len) {}

 public:
  ResourceHandleRange() = default;
  ResourceHandleRange(ResourceHandle handle) : index_(handle.index_) {}

  bool is_valid() const
  {
    return index_.first.raw != 0;
  }

  bool has_inverted_handedness() const
  {
    return index_.has_inverted_handedness();
  }

  IndexRange index_range() const
  {
    return index_.index_range();
  }

  operator ResourceIndexRange() const
  {
    BLI_assert(is_valid());
    return index_;
  }

  /* These functions are to keep existing engine code to work.
   * Should be used only for objects and code paths that don't support ranged synchronization. */

  operator ResourceHandle() const
  {
    BLI_assert(index_.count == 1);
    return ResourceHandle(index_.first.raw);
  }

  uint32_t raw() const
  {
    BLI_assert(index_.count == 1);
    return index_.first.raw;
  }

  uint resource_index() const
  {
    BLI_assert(index_.count == 1);
    return index_.first.resource_index();
  }
};

/* TODO(fclem): Move to somewhere more appropriated after cleaning up the header dependencies. */
class ObjectRef {
  friend class Manager;
  friend class ObjectKey;
  friend DupliCacheManager;

 private:
  /** Duplicated object that corresponds to the current object. */
  DupliObject *const dupli_object_ = nullptr;
  /** Object that created the dupli-list the current object is part of. */
  Object *const dupli_parent_ = nullptr;

  /** List of (render-compatible) duplis when rendering a ranges. */
  const VectorList<DupliObject *> *duplis_ = nullptr;

  /** Unique handle per object ref. */
  ResourceHandleRange handle_ = {};
  ResourceHandleRange sculpt_handle_ = {};

 public:
  Object *const object;

  explicit ObjectRef(Object *ob,
                     Object *dupli_parent = nullptr,
                     DupliObject *dupli_object = nullptr);
  explicit ObjectRef(Object &ob, Object *dupli_parent, const VectorList<DupliObject *> &duplis);

  /* Is the object coming from a Dupli system. */
  bool is_dupli() const
  {
    return dupli_parent_ != nullptr;
  }

  bool is_active(const Object *active_object) const
  {
    return (dupli_parent_ ? dupli_parent_ : object) == active_object;
  }

  float random() const
  {
    if (duplis_) {
      /* NOTE: The random property is only used by EEVEE, which currently doesn't support
      instancing optimizations. However, ObjectInfos always call this function so the code is still
      reachable even if its result won't be used. */
      // BLI_assert_unreachable();
      /* TODO: This should fill a span instead. */
      return 0.0;
    }

    if (dupli_parent_ == nullptr) {
      /* TODO(fclem): this is rather costly to do at draw time. Maybe we can
       * put it in ob->runtime and make depsgraph ensure it is up to date. */
      return BLI_hash_int_2d(BLI_hash_string(object->id.name + 2), 0) * (1.0f / (float)0xFFFFFFFF);
    }
    return dupli_object_->random_id * (1.0f / (float)0xFFFFFFFF);
  }

  bool find_rgba_attribute(const GPUUniformAttr &attr, float r_value[4]) const
  {
    if (duplis_) {
      /* NOTE: This function is only called for EEVEE, which currently doesn't support instancing
       * optimizations, so this code should be unreachable. */
      BLI_assert_unreachable();
      /* TODO: r_value should be a Span. */
      return false;
    }

    /* If requesting instance data, check the parent particle system and object. */
    if (attr.use_dupli) {
      return BKE_object_dupli_find_rgba_attribute(
          object, dupli_object_, dupli_parent_, attr.name, r_value);
    }
    return BKE_object_dupli_find_rgba_attribute(object, nullptr, nullptr, attr.name, r_value);
  }

  LightLinking *light_linking() const
  {
    return dupli_parent_ ? dupli_parent_->light_linking : object->light_linking;
  }

  int recalc_flags(uint64_t last_update) const
  {
    /* TODO: There should also be a way to get the min last_update for all objects in the range. */
    auto get_flags = [&](const ObjectRuntimeHandle &runtime) {
      int flags = 0;
      SET_FLAG_FROM_TEST(flags, runtime.last_update_transform > last_update, ID_RECALC_TRANSFORM);
      SET_FLAG_FROM_TEST(flags, runtime.last_update_geometry > last_update, ID_RECALC_GEOMETRY);
      SET_FLAG_FROM_TEST(flags, runtime.last_update_shading > last_update, ID_RECALC_SHADING);
      return flags;
    };

    int flags = get_flags(*object->runtime);
    if (dupli_parent_) {
      flags |= get_flags(*dupli_parent_->runtime);
    }

    return flags;
  }

  /* Particle data are stored in world space. If an object is instanced, the associated particle
   * systems need to be offset appropriately. */
  float4x4 particles_matrix() const
  {
    if (duplis_) {
      /* NOTE: Objects with particles don't support instancing optimizations yet, so this code
       * should be unreachable. */
      BLI_assert_unreachable();
      /* TODO: This should fill a span instead. */
      return float4x4::identity();
    }

    /* TODO: Pass particle systems as a separate ObRef? */
    float4x4 dupli_mat = float4x4::identity();
    if (dupli_parent_ && dupli_object_) {
      if (dupli_object_->type & OB_DUPLICOLLECTION) {
        Collection *collection = dupli_parent_->instance_collection;
        if (collection != nullptr) {
          dupli_mat[3] -= float4(float3(collection->instance_offset), 0.0f);
        }
        dupli_mat = dupli_parent_->object_to_world() * dupli_mat;
      }
      else {
        dupli_mat = object->object_to_world() * math::invert(dupli_object_->ob->object_to_world());
      }
    }
    return dupli_mat;
  }

  int preview_instance_index() const
  {
    if (dupli_object_) {
      return dupli_object_->preview_instance_index;
    }
    return -1;
  }

  const blender::bke::GeometrySet *preview_base_geometry() const
  {
    if (dupli_object_) {
      return dupli_object_->preview_base_geometry;
    }
    return nullptr;
  }

  bool parent_is_in_edit_paint_mode(const Object *active_object,
                                    eObjectMode ob_mode,
                                    eContextObjectMode ctx_mode) const
  {
    /* TODO: Deduplicate code with Overlay engine.
     * Move to BKE ? Or check if T72490 is still relevant. */

    if (!dupli_parent_ || active_object != dupli_parent_) {
      return false;
    }

    if (object->base_flag & BASE_FROM_DUPLI) {
      /* TODO: Is this code reachable? */
      return false;
    }

    if (dupli_parent_->sculpt && (dupli_parent_->sculpt->mode_type == OB_MODE_SCULPT)) {
      return true;
    }

    if (ob_mode & (OB_MODE_ALL_PAINT | OB_MODE_ALL_PAINT_GPENCIL)) {
      return true;
    }

    if (DRW_object_is_in_edit_mode(dupli_parent_)) {
      /* Also check for context mode as the object mode is not 100% reliable. (see T72490) */
      switch (dupli_parent_->type) {
        case OB_MESH:
          return ctx_mode == CTX_MODE_EDIT_MESH;
        case OB_ARMATURE:
          return ctx_mode == CTX_MODE_EDIT_ARMATURE;
        case OB_CURVES_LEGACY:
          return ctx_mode == CTX_MODE_EDIT_CURVE;
        case OB_SURF:
          return ctx_mode == CTX_MODE_EDIT_SURFACE;
        case OB_LATTICE:
          return ctx_mode == CTX_MODE_EDIT_LATTICE;
        case OB_MBALL:
          return ctx_mode == CTX_MODE_EDIT_METABALL;
        case OB_FONT:
          return ctx_mode == CTX_MODE_EDIT_TEXT;
        case OB_CURVES:
          return ctx_mode == CTX_MODE_EDIT_CURVES;
        case OB_POINTCLOUD:
          return ctx_mode == CTX_MODE_EDIT_POINTCLOUD;
        case OB_GREASE_PENCIL:
          return ctx_mode == CTX_MODE_EDIT_GREASE_PENCIL;
        case OB_VOLUME:
          /* No edit mode yet. */
          return false;
      }
    }
    return false;
  }
};

/* -------------------------------------------------------------------- */
/** \name ObjectKey
 *
 * Unique key to identify each object in a hash-map.
 * Note that we get a unique key for each object component.
 * \{ */

class ObjectKey {
  /** Hash value of the key. */
  uint64_t hash_value_ = 0;
  /** Original Object or source object for duplis. */
  Object *ob_ = nullptr;
  /** Original Parent object for duplis. */
  Object *parent_ = nullptr;
  /** Dupli objects recursive unique identifier */
  int id_[MAX_DUPLI_RECUR];
  /** Used for particle system hair. */
  int sub_key_ = 0;

 public:
  ObjectKey() = default;

  ObjectKey(const ObjectRef &ob_ref, int sub_key = 0)
  {
    ob_ = DEG_get_original(ob_ref.object);
    hash_value_ = get_default_hash(ob_);

    if (DupliObject *dupli = ob_ref.dupli_object_) {
      parent_ = ob_ref.dupli_parent_;
      hash_value_ = get_default_hash(hash_value_, get_default_hash(parent_));
      for (int i : IndexRange(MAX_DUPLI_RECUR)) {
        id_[i] = dupli->persistent_id[i];
        if (id_[i] == std::numeric_limits<int>::max()) {
          break;
        }
        hash_value_ = get_default_hash(hash_value_, get_default_hash(id_[i]));
      }
    }

    if (sub_key != 0) {
      sub_key_ = sub_key;
      hash_value_ = get_default_hash(hash_value_, get_default_hash(sub_key_));
    }
  }

  uint64_t hash() const
  {
    return hash_value_;
  }

  bool operator==(const ObjectKey &k) const
  {
    if (hash_value_ != k.hash_value_) {
      return false;
    }
    if (ob_ != k.ob_) {
      return false;
    }
    if (parent_ != k.parent_) {
      return false;
    }
    if (sub_key_ != k.sub_key_) {
      return false;
    }
    if (parent_) {
      for (int i : IndexRange(MAX_DUPLI_RECUR)) {
        if (id_[i] != k.id_[i]) {
          return false;
        }
        if (id_[i] == std::numeric_limits<int>::max()) {
          break;
        }
      }
    }
    return true;
  }
};

/** \} */

};  // namespace blender::draw
