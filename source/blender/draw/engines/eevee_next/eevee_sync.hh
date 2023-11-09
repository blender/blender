/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * Structures to identify unique data blocks. The keys are unique so we are able to
 * match ids across frame updates.
 */

#pragma once

#include "BKE_duplilist.h"
#include "BLI_ghash.h"
#include "BLI_map.hh"
#include "DEG_depsgraph_query.hh"
#include "DNA_object_types.h"
#include "DRW_render.h"
#include "GPU_material.h"

#include "eevee_shader_shared.hh"

namespace blender::eevee {

class Instance;

/* -------------------------------------------------------------------- */
/** \name ObjectKey
 *
 * Unique key to identify each object in the hash-map.
 * Note that we get a unique key for each object component.
 * \{ */

class ObjectKey {
  /** Hash value of the key. */
  uint64_t hash_value_;
  /** Original Object or source object for duplis. */
  Object *ob_;
  /** Original Parent object for duplis. */
  Object *parent_;
  /** Dupli objects recursive unique identifier */
  int id_[MAX_DUPLI_RECUR];
  /** Used for particle system hair. */
  int sub_key_;

 public:
  ObjectKey() = default;

  ObjectKey(Object *ob, int sub_key = 0)
  {
    /* Since we use `memcmp` for comparison,
     * we have to ensure the padding bytes are initialized as well. */
    memset(this, 0, sizeof(*this));

    ob_ = DEG_get_original_object(ob);
    hash_value_ = BLI_ghashutil_ptrhash(ob_);

    if (DupliObject *dupli = DRW_object_get_dupli(ob)) {
      parent_ = DRW_object_get_dupli_parent(ob);
      hash_value_ = BLI_ghashutil_combine_hash(hash_value_, BLI_ghashutil_ptrhash(parent_));
      for (int i : IndexRange(MAX_DUPLI_RECUR)) {
        id_[i] = dupli->persistent_id[i];
        if (id_[i] == INT_MAX) {
          break;
        }
        hash_value_ = BLI_ghashutil_combine_hash(hash_value_, BLI_ghashutil_inthash(id_[i]));
      }
    }

    if (sub_key != 0) {
      sub_key_ = sub_key;
      hash_value_ = BLI_ghashutil_combine_hash(hash_value_, BLI_ghashutil_inthash(sub_key_));
    }
  }

  uint64_t hash() const
  {
    return hash_value_;
  }

  bool operator<(const ObjectKey &k) const
  {
    return memcmp(this, &k, sizeof(*this)) < 0;
  }

  bool operator==(const ObjectKey &k) const
  {
    return memcmp(this, &k, sizeof(*this)) == 0;
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sync Module
 *
 * \{ */

struct BaseHandle {
  /* Accumulated recalc flags, which corresponds to ID->recalc flags. */
  unsigned int recalc;
  void reset_recalc_flag()
  {
    if (recalc != 0) {
      recalc = 0;
    }
  }
};

struct ObjectHandle : public BaseHandle {
  ObjectKey object_key;
};

struct WorldHandle : public DrawData {
  void reset_recalc_flag()
  {
    if (recalc != 0) {
      recalc = 0;
    }
  }
};

struct SceneHandle : public DrawData {
  void reset_recalc_flag()
  {
    if (recalc != 0) {
      recalc = 0;
    }
  }
};

class SyncModule {
 private:
  Instance &inst_;

  Map<ObjectKey, ObjectHandle> ob_handles = {};

 public:
  SyncModule(Instance &inst) : inst_(inst){};
  ~SyncModule(){};

  ObjectHandle &sync_object(Object *ob);
  WorldHandle &sync_world(::World *world);
  SceneHandle &sync_scene(::Scene *scene);

  void sync_mesh(Object *ob,
                 ObjectHandle &ob_handle,
                 ResourceHandle res_handle,
                 const ObjectRef &ob_ref);
  bool sync_sculpt(Object *ob,
                   ObjectHandle &ob_handle,
                   ResourceHandle res_handle,
                   const ObjectRef &ob_ref);
  void sync_point_cloud(Object *ob,
                        ObjectHandle &ob_handle,
                        ResourceHandle res_handle,
                        const ObjectRef &ob_ref);
  void sync_volume(Object *ob, ObjectHandle &ob_handle, ResourceHandle res_handle);
  void sync_gpencil(Object *ob, ObjectHandle &ob_handle, ResourceHandle res_handle);
  void sync_curves(Object *ob,
                   ObjectHandle &ob_handle,
                   ResourceHandle res_handle,
                   const ObjectRef &ob_ref,
                   ModifierData *modifier_data = nullptr,
                   ParticleSystem *particle_sys = nullptr);
  void sync_light_probe(Object *ob, ObjectHandle &ob_handle);
};

using HairHandleCallback = FunctionRef<void(ObjectHandle, ModifierData &, ParticleSystem &)>;
void foreach_hair_particle_handle(Object *ob, ObjectHandle ob_handle, HairHandleCallback callback);

/** \} */

}  // namespace blender::eevee
