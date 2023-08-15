/* SPDX-FileCopyrightText: 2021 Blender Foundation
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

struct ObjectKey {
  /** Hash value of the key. */
  uint64_t hash_value;
  /** Original Object or source object for duplis. */
  Object *ob;
  /** Original Parent object for duplis. */
  Object *parent;
  /** Dupli objects recursive unique identifier */
  int id[MAX_DUPLI_RECUR];
  /** Used for particle system hair. */
  int sub_key_;
#ifdef DEBUG
  char name[64];
#endif
  ObjectKey() : ob(nullptr), parent(nullptr){};

  ObjectKey(Object *ob_, Object *parent_, int id_[MAX_DUPLI_RECUR], int sub_key_ = 0)
      : ob(ob_), parent(parent_), sub_key_(sub_key_)
  {
    if (id_) {
      memcpy(id, id_, sizeof(id));
    }
    else {
      memset(id, 0, sizeof(id));
    }
    /* Compute hash on creation so we avoid the cost of it for every sync. */
    hash_value = BLI_ghashutil_ptrhash(ob);
    hash_value = BLI_ghashutil_combine_hash(hash_value, BLI_ghashutil_ptrhash(parent));
    for (int i = 0; i < MAX_DUPLI_RECUR; i++) {
      if (id[i] != 0) {
        hash_value = BLI_ghashutil_combine_hash(hash_value, BLI_ghashutil_inthash(id[i]));
      }
      else {
        break;
      }
    }
    if (sub_key_ != 0) {
      hash_value = BLI_ghashutil_combine_hash(hash_value, sub_key_);
    }
#ifdef DEBUG
    STRNCPY(name, ob->id.name);
#endif
  }

  ObjectKey(Object *ob, DupliObject *dupli, Object *parent, int sub_key_ = 0)
      : ObjectKey(ob, parent, dupli ? dupli->persistent_id : nullptr, sub_key_){};

  ObjectKey(Object *ob, int sub_key_ = 0)
      : ObjectKey(ob, DRW_object_get_dupli(ob), DRW_object_get_dupli_parent(ob), sub_key_){};

  uint64_t hash() const
  {
    return hash_value;
  }

  bool operator<(const ObjectKey &k) const
  {
    if (ob != k.ob) {
      return (ob < k.ob);
    }
    if (parent != k.parent) {
      return (parent < k.parent);
    }
    if (sub_key_ != k.sub_key_) {
      return (sub_key_ < k.sub_key_);
    }
    return memcmp(id, k.id, sizeof(id)) < 0;
  }

  bool operator==(const ObjectKey &k) const
  {
    if (ob != k.ob) {
      return false;
    }
    if (parent != k.parent) {
      return false;
    }
    if (sub_key_ != k.sub_key_) {
      return false;
    }
    return memcmp(id, k.id, sizeof(id)) == 0;
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sync Module
 *
 * \{ */

struct ObjectHandle : public DrawData {
  ObjectKey object_key;

  void reset_recalc_flag()
  {
    if (recalc != 0) {
      recalc = 0;
    }
  }
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
  void sync_gpencil(Object *ob, ObjectHandle &ob_handle, ResourceHandle res_handle);
  void sync_curves(Object *ob,
                   ObjectHandle &ob_handle,
                   ResourceHandle res_handle,
                   ModifierData *modifier_data = nullptr,
                   ParticleSystem *particle_sys = nullptr);
  void sync_light_probe(Object *ob, ObjectHandle &ob_handle);
};

using HairHandleCallback = FunctionRef<void(ObjectHandle, ModifierData &, ParticleSystem &)>;
void foreach_hair_particle_handle(Object *ob, ObjectHandle ob_handle, HairHandleCallback callback);

/** \} */

}  // namespace blender::eevee
