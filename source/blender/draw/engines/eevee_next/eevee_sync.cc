/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * Converts the different renderable object types to drawcalls.
 */

#include "eevee_engine.h"

#include "BKE_gpencil_legacy.h"
#include "BKE_object.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"
#include "DEG_depsgraph_query.hh"
#include "DNA_curves_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_modifier_types.h"
#include "DNA_particle_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_volume_types.h"

#include "draw_common.hh"
#include "draw_sculpt.hh"

#include "eevee_instance.hh"

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name Recalc
 *
 * \{ */

ObjectHandle &SyncModule::sync_object(const ObjectRef &ob_ref)
{
  ObjectKey key(ob_ref.object);

  ObjectHandle &handle = ob_handles.lookup_or_add_cb(key, [&]() {
    ObjectHandle new_handle;
    new_handle.object_key = key;
    return new_handle;
  });

  handle.recalc = inst_.get_recalc_flags(ob_ref);

  return handle;
}

WorldHandle SyncModule::sync_world(const ::World &world)
{
  WorldHandle handle;
  handle.recalc = inst_.get_recalc_flags(world);
  return handle;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Common
 * \{ */

static inline void geometry_call(PassMain::Sub *sub_pass,
                                 gpu::Batch *geom,
                                 ResourceHandle resource_handle)
{
  if (sub_pass != nullptr) {
    sub_pass->draw(geom, resource_handle);
  }
}

static inline void volume_call(
    MaterialPass &matpass, Scene *scene, Object *ob, gpu::Batch *geom, ResourceHandle res_handle)
{
  if (matpass.sub_pass != nullptr) {
    PassMain::Sub *object_pass = volume_sub_pass(*matpass.sub_pass, scene, ob, matpass.gpumat);
    if (object_pass != nullptr) {
      object_pass->draw(geom, res_handle);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh
 * \{ */

void SyncModule::sync_mesh(Object *ob,
                           ObjectHandle &ob_handle,
                           ResourceHandle res_handle,
                           const ObjectRef &ob_ref)
{
  if (!inst_.use_surfaces) {
    return;
  }

  bool has_motion = inst_.velocity.step_object_sync(
      ob, ob_handle.object_key, res_handle, ob_handle.recalc);

  MaterialArray &material_array = inst_.materials.material_array_get(ob, has_motion);

  gpu::Batch **mat_geom = DRW_cache_object_surface_material_get(
      ob, material_array.gpu_materials.data(), material_array.gpu_materials.size());

  if (mat_geom == nullptr) {
    return;
  }

  if ((ob->dt < OB_SOLID) && (inst_.is_viewport() && inst_.v3d->shading.type != OB_RENDER)) {
    /** Do not render objects with display type lower than solid when in material preview mode. */
    return;
  }

  bool is_alpha_blend = false;
  bool has_transparent_shadows = false;
  bool has_volume = false;
  float inflate_bounds = 0.0f;
  for (auto i : material_array.gpu_materials.index_range()) {
    gpu::Batch *geom = mat_geom[i];
    if (geom == nullptr) {
      continue;
    }

    Material &material = material_array.materials[i];
    GPUMaterial *gpu_material = material_array.gpu_materials[i];

    if (material.has_volume) {
      volume_call(material.volume_occupancy, inst_.scene, ob, geom, res_handle);
      volume_call(material.volume_material, inst_.scene, ob, geom, res_handle);
      has_volume = true;
      /* Do not render surface if we are rendering a volume object
       * and do not have a surface closure. */
      if (!material.has_surface) {
        continue;
      }
    }

    geometry_call(material.capture.sub_pass, geom, res_handle);
    geometry_call(material.overlap_masking.sub_pass, geom, res_handle);
    geometry_call(material.prepass.sub_pass, geom, res_handle);
    geometry_call(material.shading.sub_pass, geom, res_handle);
    geometry_call(material.shadow.sub_pass, geom, res_handle);

    geometry_call(material.planar_probe_prepass.sub_pass, geom, res_handle);
    geometry_call(material.planar_probe_shading.sub_pass, geom, res_handle);
    geometry_call(material.lightprobe_sphere_prepass.sub_pass, geom, res_handle);
    geometry_call(material.lightprobe_sphere_shading.sub_pass, geom, res_handle);

    is_alpha_blend = is_alpha_blend || material.is_alpha_blend_transparent;
    has_transparent_shadows = has_transparent_shadows || material.has_transparent_shadows;

    ::Material *mat = GPU_material_get_material(gpu_material);
    inst_.cryptomatte.sync_material(mat);

    if (GPU_material_has_displacement_output(gpu_material)) {
      inflate_bounds = math::max(inflate_bounds, mat->inflate_bounds);
    }
  }

  if (has_volume) {
    inst_.volume.object_sync(ob_handle);
  }

  if (inflate_bounds != 0.0f) {
    inst_.manager->update_handle_bounds(res_handle, ob_ref, inflate_bounds);
  }

  inst_.manager->extract_object_attributes(res_handle, ob_ref, material_array.gpu_materials);

  inst_.shadows.sync_object(ob, ob_handle, res_handle, is_alpha_blend, has_transparent_shadows);
  inst_.cryptomatte.sync_object(ob, res_handle);
}

bool SyncModule::sync_sculpt(Object *ob,
                             ObjectHandle &ob_handle,
                             ResourceHandle res_handle,
                             const ObjectRef &ob_ref)
{
  if (!inst_.use_surfaces) {
    return false;
  }

  bool pbvh_draw = BKE_sculptsession_use_pbvh_draw(ob, inst_.rv3d) && !DRW_state_is_image_render();
  if (!pbvh_draw) {
    return false;
  }

  bool has_motion = false;
  MaterialArray &material_array = inst_.materials.material_array_get(ob, has_motion);

  bool is_alpha_blend = false;
  bool has_transparent_shadows = false;
  bool has_volume = false;
  float inflate_bounds = 0.0f;
  for (SculptBatch &batch :
       sculpt_batches_per_material_get(ob_ref.object, material_array.gpu_materials))
  {
    gpu::Batch *geom = batch.batch;
    if (geom == nullptr) {
      continue;
    }

    Material &material = material_array.materials[batch.material_slot];

    if (material.has_volume) {
      volume_call(material.volume_occupancy, inst_.scene, ob, geom, res_handle);
      volume_call(material.volume_material, inst_.scene, ob, geom, res_handle);
      has_volume = true;
      /* Do not render surface if we are rendering a volume object
       * and do not have a surface closure. */
      if (material.has_surface == false) {
        continue;
      }
    }

    geometry_call(material.capture.sub_pass, geom, res_handle);
    geometry_call(material.overlap_masking.sub_pass, geom, res_handle);
    geometry_call(material.prepass.sub_pass, geom, res_handle);
    geometry_call(material.shading.sub_pass, geom, res_handle);
    geometry_call(material.shadow.sub_pass, geom, res_handle);

    geometry_call(material.planar_probe_prepass.sub_pass, geom, res_handle);
    geometry_call(material.planar_probe_shading.sub_pass, geom, res_handle);
    geometry_call(material.lightprobe_sphere_prepass.sub_pass, geom, res_handle);
    geometry_call(material.lightprobe_sphere_shading.sub_pass, geom, res_handle);

    is_alpha_blend = is_alpha_blend || material.is_alpha_blend_transparent;
    has_transparent_shadows = has_transparent_shadows || material.has_transparent_shadows;

    GPUMaterial *gpu_material = material_array.gpu_materials[batch.material_slot];
    ::Material *mat = GPU_material_get_material(gpu_material);
    inst_.cryptomatte.sync_material(mat);

    if (GPU_material_has_displacement_output(gpu_material)) {
      inflate_bounds = math::max(inflate_bounds, mat->inflate_bounds);
    }
  }

  if (has_volume) {
    inst_.volume.object_sync(ob_handle);
  }

  /* Use a valid bounding box. The pbvh::Tree module already does its own culling, but a valid */
  /* bounding box is still needed for directional shadow tile-map bounds computation. */
  const Bounds<float3> bounds = bke::pbvh::bounds_get(*bke::object::pbvh_get(*ob_ref.object));
  const float3 center = math::midpoint(bounds.min, bounds.max);
  const float3 half_extent = bounds.max - center + inflate_bounds;
  inst_.manager->update_handle_bounds(res_handle, center, half_extent);

  inst_.manager->extract_object_attributes(res_handle, ob_ref, material_array.gpu_materials);

  inst_.shadows.sync_object(ob, ob_handle, res_handle, is_alpha_blend, has_transparent_shadows);
  inst_.cryptomatte.sync_object(ob, res_handle);

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Point Cloud
 * \{ */

void SyncModule::sync_point_cloud(Object *ob,
                                  ObjectHandle &ob_handle,
                                  ResourceHandle res_handle,
                                  const ObjectRef &ob_ref)
{
  const int material_slot = POINTCLOUD_MATERIAL_NR;

  bool has_motion = inst_.velocity.step_object_sync(
      ob, ob_handle.object_key, res_handle, ob_handle.recalc);

  Material &material = inst_.materials.material_get(
      ob, has_motion, material_slot - 1, MAT_GEOM_POINT_CLOUD);

  auto drawcall_add = [&](MaterialPass &matpass) {
    if (matpass.sub_pass == nullptr) {
      return;
    }
    PassMain::Sub &object_pass = matpass.sub_pass->sub("Point Cloud Sub Pass");
    gpu::Batch *geometry = point_cloud_sub_pass_setup(object_pass, ob, matpass.gpumat);
    object_pass.draw(geometry, res_handle);
  };

  if (material.has_volume) {
    /* Only support single volume material for now. */
    drawcall_add(material.volume_occupancy);
    drawcall_add(material.volume_material);
    inst_.volume.object_sync(ob_handle);

    /* Do not render surface if we are rendering a volume object
     * and do not have a surface closure. */
    if (material.has_surface == false) {
      return;
    }
  }

  drawcall_add(material.capture);
  drawcall_add(material.overlap_masking);
  drawcall_add(material.prepass);
  drawcall_add(material.shading);
  drawcall_add(material.shadow);

  drawcall_add(material.planar_probe_prepass);
  drawcall_add(material.planar_probe_shading);
  drawcall_add(material.lightprobe_sphere_prepass);
  drawcall_add(material.lightprobe_sphere_shading);

  inst_.cryptomatte.sync_object(ob, res_handle);
  GPUMaterial *gpu_material = material.shading.gpumat;
  ::Material *mat = GPU_material_get_material(gpu_material);
  inst_.cryptomatte.sync_material(mat);

  if (GPU_material_has_displacement_output(gpu_material) && mat->inflate_bounds != 0.0f) {
    inst_.manager->update_handle_bounds(res_handle, ob_ref, mat->inflate_bounds);
  }

  inst_.manager->extract_object_attributes(res_handle, ob_ref, material.shading.gpumat);

  inst_.shadows.sync_object(ob,
                            ob_handle,
                            res_handle,
                            material.is_alpha_blend_transparent,
                            material.has_transparent_shadows);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Volume Objects
 * \{ */

void SyncModule::sync_volume(Object *ob,
                             ObjectHandle &ob_handle,
                             ResourceHandle res_handle,
                             const ObjectRef &ob_ref)
{
  if (!inst_.use_volumes) {
    return;
  }

  const int material_slot = VOLUME_MATERIAL_NR;

  /* Motion is not supported on volumes yet. */
  const bool has_motion = false;

  Material &material = inst_.materials.material_get(
      ob, has_motion, material_slot - 1, MAT_GEOM_VOLUME);

  if (!GPU_material_has_volume_output(material.volume_material.gpumat)) {
    return;
  }

  /* Do not render the object if there is no attribute used in the volume.
   * This mimic Cycles behavior (see #124061). */
  ListBase attr_list = GPU_material_attributes(material.volume_material.gpumat);
  if (BLI_listbase_is_empty(&attr_list)) {
    return;
  }

  auto drawcall_add = [&](MaterialPass &matpass, gpu::Batch *geom, ResourceHandle res_handle) {
    if (matpass.sub_pass == nullptr) {
      return false;
    }
    PassMain::Sub *object_pass = volume_sub_pass(
        *matpass.sub_pass, inst_.scene, ob, matpass.gpumat);
    if (object_pass != nullptr) {
      object_pass->draw(geom, res_handle);
      return true;
    }
    return false;
  };

  /* Use bounding box tag empty spaces. */
  gpu::Batch *geom = DRW_cache_cube_get();

  bool is_rendered = false;
  is_rendered |= drawcall_add(material.volume_occupancy, geom, res_handle);
  is_rendered |= drawcall_add(material.volume_material, geom, res_handle);

  if (!is_rendered) {
    return;
  }

  inst_.manager->extract_object_attributes(res_handle, ob_ref, material.volume_material.gpumat);

  inst_.volume.object_sync(ob_handle);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Hair
 * \{ */

void SyncModule::sync_curves(Object *ob,
                             ObjectHandle &ob_handle,
                             ResourceHandle res_handle,
                             const ObjectRef &ob_ref,
                             ModifierData *modifier_data,
                             ParticleSystem *particle_sys)
{
  if (!inst_.use_curves) {
    return;
  }

  int mat_nr = CURVES_MATERIAL_NR;
  if (particle_sys != nullptr) {
    mat_nr = particle_sys->part->omat;
  }

  bool has_motion = inst_.velocity.step_object_sync(
      ob, ob_handle.object_key, res_handle, ob_handle.recalc, modifier_data, particle_sys);
  Material &material = inst_.materials.material_get(ob, has_motion, mat_nr - 1, MAT_GEOM_CURVES);

  auto drawcall_add = [&](MaterialPass &matpass) {
    if (matpass.sub_pass == nullptr) {
      return;
    }
    if (particle_sys != nullptr) {
      PassMain::Sub &sub_pass = matpass.sub_pass->sub("Hair SubPass");
      gpu::Batch *geometry = hair_sub_pass_setup(
          sub_pass, inst_.scene, ob, particle_sys, modifier_data, matpass.gpumat);
      sub_pass.draw(geometry, res_handle);
    }
    else {
      PassMain::Sub &sub_pass = matpass.sub_pass->sub("Curves SubPass");
      gpu::Batch *geometry = curves_sub_pass_setup(sub_pass, inst_.scene, ob, matpass.gpumat);
      sub_pass.draw(geometry, res_handle);
    }
  };

  if (material.has_volume) {
    /* Only support single volume material for now. */
    drawcall_add(material.volume_occupancy);
    drawcall_add(material.volume_material);
    inst_.volume.object_sync(ob_handle);
    /* Do not render surface if we are rendering a volume object
     * and do not have a surface closure. */
    if (material.has_surface == false) {
      return;
    }
  }

  drawcall_add(material.capture);
  drawcall_add(material.overlap_masking);
  drawcall_add(material.prepass);
  drawcall_add(material.shading);
  drawcall_add(material.shadow);

  drawcall_add(material.planar_probe_prepass);
  drawcall_add(material.planar_probe_shading);
  drawcall_add(material.lightprobe_sphere_prepass);
  drawcall_add(material.lightprobe_sphere_shading);

  inst_.cryptomatte.sync_object(ob, res_handle);
  GPUMaterial *gpu_material = material.shading.gpumat;
  ::Material *mat = GPU_material_get_material(gpu_material);
  inst_.cryptomatte.sync_material(mat);

  if (GPU_material_has_displacement_output(gpu_material) && mat->inflate_bounds != 0.0f) {
    inst_.manager->update_handle_bounds(res_handle, ob_ref, mat->inflate_bounds);
  }

  inst_.manager->extract_object_attributes(res_handle, ob_ref, material.shading.gpumat);

  inst_.shadows.sync_object(ob,
                            ob_handle,
                            res_handle,
                            material.is_alpha_blend_transparent,
                            material.has_transparent_shadows);
}

/** \} */

void foreach_hair_particle_handle(Object *ob, ObjectHandle ob_handle, HairHandleCallback callback)
{
  int sub_key = 1;

  LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
    if (md->type == eModifierType_ParticleSystem) {
      ParticleSystem *particle_sys = reinterpret_cast<ParticleSystemModifierData *>(md)->psys;
      ParticleSettings *part_settings = particle_sys->part;
      const int draw_as = (part_settings->draw_as == PART_DRAW_REND) ? part_settings->ren_as :
                                                                       part_settings->draw_as;
      if (draw_as != PART_DRAW_PATH ||
          !DRW_object_is_visible_psys_in_active_context(ob, particle_sys))
      {
        continue;
      }

      ObjectHandle particle_sys_handle = ob_handle;
      particle_sys_handle.object_key = ObjectKey(ob, sub_key++);
      particle_sys_handle.recalc = particle_sys->recalc;

      callback(particle_sys_handle, *md, *particle_sys);
    }
  }
}

}  // namespace blender::eevee
