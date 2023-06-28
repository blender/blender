/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * Module that handles light probe update tagging.
 * Lighting data is contained in their respective module `IrradianceCache` and `ReflectionProbes`.
 */

#include "DNA_lightprobe_types.h"
#include "WM_api.h"

#include "eevee_instance.hh"
#include "eevee_lightprobe.hh"

#include "draw_debug.hh"

namespace blender::eevee {

void LightProbeModule::begin_sync()
{
  auto_bake_enabled_ = inst_.is_viewport() &&
                       (inst_.scene->eevee.flag & SCE_EEVEE_GI_AUTOBAKE) != 0;
}

void LightProbeModule::sync_grid(const Object *ob, ObjectHandle &handle)
{
  IrradianceGrid &grid = grid_map_.lookup_or_add_default(handle.object_key);
  grid.used = true;
  if (handle.recalc != 0 || grid.initialized == false) {
    grid.initialized = true;
    grid.updated = true;
    grid.object_to_world = float4x4(ob->object_to_world);
    grid.world_to_object = float4x4(
        math::normalize(math::transpose(float3x3(grid.object_to_world))));

    grid.cache = ob->lightprobe_cache;
    /* Force reupload. */
    inst_.irradiance_cache.bricks_free(grid.bricks);
  }
}

void LightProbeModule::sync_cube(ObjectHandle &handle)
{
  ReflectionCube &cube = cube_map_.lookup_or_add_default(handle.object_key);
  cube.used = true;
  if (handle.recalc != 0 || cube.initialized == false) {
    cube.initialized = true;
    cube_update_ = true;
  }
}

void LightProbeModule::sync_probe(const Object *ob, ObjectHandle &handle)
{
  const ::LightProbe *lightprobe = static_cast<const ::LightProbe *>(ob->data);
  switch (lightprobe->type) {
    case LIGHTPROBE_TYPE_CUBE:
      sync_cube(handle);
      return;
    case LIGHTPROBE_TYPE_PLANAR:
      /* TODO(fclem): Remove support? Add support? */
      return;
    case LIGHTPROBE_TYPE_GRID:
      sync_grid(ob, handle);
      return;
  }
  BLI_assert_unreachable();
}

void LightProbeModule::end_sync()
{
  {
    /* Check for deleted or updated grid. */
    grid_update_ = false;
    auto it_end = grid_map_.items().end();
    for (auto it = grid_map_.items().begin(); it != it_end; ++it) {
      IrradianceGrid &grid = (*it).value;
      if (grid.updated) {
        grid.updated = false;
        grid_update_ = true;
      }
      if (!grid.used) {
        inst_.irradiance_cache.bricks_free(grid.bricks);
        grid_map_.remove(it);
        grid_update_ = true;
        continue;
      }
      /* Untag for next sync. */
      grid.used = false;
    }
  }
  {
    /* Check for deleted or updated cube. */
    cube_update_ = false;
    auto it_end = cube_map_.items().end();
    for (auto it = cube_map_.items().begin(); it != it_end; ++it) {
      ReflectionCube &cube = (*it).value;
      if (cube.updated) {
        cube.updated = false;
        cube_update_ = true;
      }
      if (!cube.used) {
        cube_map_.remove(it);
        cube_update_ = true;
        continue;
      }
      /* Untag for next sync. */
      cube.used = false;
    }
  }

#if 0 /* TODO make this work with new per object light cache. */
  /* If light-cache auto-update is enable we tag the relevant part
   * of the cache to update and fire up a baking job. */
  if (auto_bake_enabled_ && (grid_update_ || cube_update_)) {
    Scene *original_scene = DEG_get_input_scene(inst_.depsgraph);
    LightCache *light_cache = original_scene->eevee.light_cache_data;

    if (light_cache != nullptr) {
      if (grid_update_) {
        light_cache->flag |= LIGHTCACHE_UPDATE_GRID;
      }
      /* TODO(fclem): Reflection Cube-map should capture albedo + normal and be
       * relit at runtime. So no dependency like in the old system. */
      if (cube_update_) {
        light_cache->flag |= LIGHTCACHE_UPDATE_CUBE;
      }
      /* Tag the lightcache to auto update. */
      light_cache->flag |= LIGHTCACHE_UPDATE_AUTO;
      /* Use a notifier to trigger the operator after drawing. */
      /* TODO(fclem): Avoid usage of global DRW. */
      WM_event_add_notifier(DRW_context_state_get()->evil_C, NC_LIGHTPROBE, original_scene);
    }
  }
#endif
}

}  // namespace blender::eevee
