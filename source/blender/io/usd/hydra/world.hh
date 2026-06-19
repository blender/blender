/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <pxr/base/gf/matrix4d.h>
#include <pxr/imaging/hd/dataSource.h>
#include <pxr/usd/sdf/path.h>

#include "BLI_string.hh"

#include "DNA_space_enums.h"

namespace blender {

struct Main;
struct Scene;
struct View3D;
struct World;

namespace io::hydra {

struct PopulateContext;

/** Build the dome-light parameter container for the active world or studiolight. */
pxr::HdContainerDataSourceHandle build_world_data_source(Main *bmain,
                                                         Scene *scene,
                                                         const View3D *view3d,
                                                         pxr::GfMatrix4d *r_transform);

/* Caches the inputs that feed the world dome-light prim so we can skip rebuilding it (and
 * dirtying its texture binding every frame) when nothing relevant has changed. Updates to the
 * World ID itself are caught via `ID_RECALC_SHADING`; the rest captures view3d state that is
 * not reflected in any recalc.
 * TODO: an image reloaded outside the World shader graph won't invalidate this. */
class EmittedWorld {
 public:
  /* Emit the world prim into `ctx` (or skip if cached inputs match). `world_shading_changed`
   * should be true when `ID_RECALC_SHADING` is set on `scene->world`. */
  void emit(PopulateContext &ctx,
            Main *bmain,
            Scene *scene,
            const View3D *view3d,
            const pxr::SdfPath &world_path,
            bool world_shading_changed);

  void clear();

 private:
  struct Inputs {
    const World *world = nullptr;
    bool use_scene_world = true;
    int shading_type = 0;
    char studiolight[FILE_MAXFILE] = {0};
    float studiolight_intensity = 0.0f;
    float studiolight_rot_z = 0.0f;

    bool operator==(const Inputs &other) const
    {
      return world == other.world && use_scene_world == other.use_scene_world &&
             shading_type == other.shading_type && STREQ(studiolight, other.studiolight) &&
             studiolight_intensity == other.studiolight_intensity &&
             studiolight_rot_z == other.studiolight_rot_z;
    }
  };

  Inputs cached_inputs_;
  bool used_ = false;
};

}  // namespace io::hydra
}  // namespace blender
