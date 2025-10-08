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

#include "BKE_duplilist.hh"
#include "BLI_map.hh"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DRW_render.hh"

#include "draw_handle.hh"

namespace blender::eevee {

using namespace draw;

class Instance;

/* -------------------------------------------------------------------- */
/** \name Sync Module
 *
 * \{ */

struct BaseHandle {
  unsigned int recalc;
};

struct ObjectHandle : BaseHandle {
  ObjectKey object_key;
};

struct WorldHandle : public BaseHandle {};

struct SceneHandle : public BaseHandle {};

class SyncModule {
 private:
  Instance &inst_;

  Map<ObjectKey, ObjectHandle> ob_handles = {};

 public:
  SyncModule(Instance &inst) : inst_(inst) {};
  ~SyncModule() {};

  ObjectHandle &sync_object(const ObjectRef &ob_ref);
  WorldHandle sync_world(const ::World &world);

  void sync_mesh(Object *ob, ObjectHandle &ob_handle, const ObjectRef &ob_ref);
  bool sync_sculpt(Object *ob, ObjectHandle &ob_handle, const ObjectRef &ob_ref);
  void sync_pointcloud(Object *ob, ObjectHandle &ob_handle, const ObjectRef &ob_ref);
  void sync_volume(Object *ob, ObjectHandle &ob_handle, const ObjectRef &ob_ref);
  void sync_curves(Object *ob,
                   ObjectHandle &ob_handle,
                   const ObjectRef &ob_ref,
                   ResourceHandleRange res_handle = {},
                   ModifierData *modifier_data = nullptr,
                   ParticleSystem *particle_sys = nullptr);
};

using HairHandleCallback = FunctionRef<void(ObjectHandle, ModifierData &, ParticleSystem &)>;
void foreach_hair_particle_handle(Instance &inst,
                                  ObjectRef &ob_ref,
                                  ObjectHandle ob_handle,
                                  HairHandleCallback callback);

/** \} */

}  // namespace blender::eevee
