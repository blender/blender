/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * Converts the different renderable object types to draw-calls.
 */

#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"
#include "DNA_curves_types.h"
#include "DNA_modifier_types.h"
#include "DNA_particle_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_volume_types.h"

#include "draw_cache.hh"
#include "draw_common.hh"
#include "draw_sculpt.hh"

#include "eevee_instance.hh"

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name Recalc
 *
 * \{ */

ObjectHandle SyncModule::sync_object(const ObjectRef &ob_ref,
                                     const ResourceHandleRange &res_handle,
                                     uint sub_key)
{
  return ObjectHandle(ob_ref, res_handle, inst_.get_recalc_flags(ob_ref), sub_key);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Common
 * \{ */

static inline void geometry_call(PassMain::Sub *sub_pass,
                                 gpu::Batch *geom,
                                 ResourceHandleRange resource_handle)
{
  if (sub_pass != nullptr) {
    sub_pass->draw(geom, resource_handle);
  }
}

static inline void geometry_volume_call(PassMain::Sub *pass,
                                        GPUMaterial *gpumat,
                                        Scene *scene,
                                        Object *ob,
                                        gpu::Batch *geom,
                                        ResourceHandle res_handle)
{
  BLI_assert(ob->type != OB_VOLUME);
  if (pass != nullptr) {
    PassMain::Sub *object_pass = volume_sub_pass(*pass, scene, ob, gpumat);
    if (object_pass != nullptr) {
      object_pass->draw(geom, {ResourceID(res_handle)});
    }
  }
}

/* -------------------------------------------------------------------- */
/** \name Common Helpers
 * \{ */

void SyncModule::sync_common_passes(const Material &material,
                                    FunctionRef<void(const MaterialPass &)> sync_cb)
{
  sync_cb(material.shadow);
  sync_cb(material.shading);
  sync_cb(material.prepass);

  sync_cb(material.capture);
  sync_cb(material.lightprobe_sphere_prepass);
  sync_cb(material.lightprobe_sphere_shading);
  sync_cb(material.planar_probe_prepass);
  sync_cb(material.planar_probe_shading);
}

void SyncModule::sync_volume_passes(const ObjectHandle &ob_handle,
                                    const Material &material,
                                    FunctionRef<void(const MaterialPass &, int)> sync_cb)
{
  if (material.volume_occupancy.gpumat == nullptr || material.volume_material.gpumat == nullptr) {
    return;
  }

  blender::Material *blender_mat = GPU_material_get_material(material.volume_material.gpumat);

  for (int instance : IndexRange(ob_handle.instances_count())) {
    /* TODO(fclem): This is against design. Sync shouldn't depend on view properties (camera). */
    VolumeLayer *layer = inst_.pipelines.volume.register_and_get_layer(
        VolumeObjectBounds(inst_.camera, ob_handle, instance));

    if (!layer) {
      continue;
    }

    sync_cb(MaterialPass{material.volume_occupancy.gpumat,
                         layer->occupancy_add(
                             ob_handle.object, blender_mat, material.volume_occupancy.gpumat)},
            instance);

    sync_cb(MaterialPass{material.volume_material.gpumat,
                         layer->material_add(
                             ob_handle.object, blender_mat, material.volume_material.gpumat)},
            instance);
  }
}

void SyncModule::sync_alpha_blended_passes(const ObjectHandle &ob_handle,
                                           const Material &material,
                                           FunctionRef<void(const MaterialPass &, int)> sync_cb)
{
  const bool hide_on_camera = ob_handle.object->visibility_flag & OB_HIDE_CAMERA;
  if (hide_on_camera || !material.is_alpha_blend_transparent) {
    return;
  }

  blender::Material *blender_mat = GPU_material_get_material(material.shading.gpumat);

  for (int instance : IndexRange(ob_handle.instances_count())) {
    PassMain::Sub *prepass = nullptr;
    PassMain::Sub *matpass = nullptr;

    inst_.pipelines.forward.transparent_add(ob_handle.object,
                                            ob_handle.object_to_world(instance).location(),
                                            blender_mat,
                                            material.shading.gpumat,
                                            prepass,
                                            matpass);

    sync_cb(MaterialPass{material.overlap_masking.gpumat, prepass}, instance);
    sync_cb(MaterialPass{material.shading.gpumat, matpass}, instance);
  }
}

void SyncModule::sync_common(const ObjectHandle &ob_handle,
                             Span<Material *> materials,
                             Span<GPUMaterial *> gpu_materials)
{
  bool has_volume = false;
  bool is_alpha_blend = false;
  bool has_transparent_shadows = false;
  float inflate_bounds = 0.0f;
  for (const Material *material : materials) {
    has_volume |= material->has_volume;
    if (material->has_volume && !material->has_surface) {
      continue;
    }

    is_alpha_blend |= material->is_alpha_blend_transparent;
    has_transparent_shadows |= material->has_transparent_shadows;

    GPUMaterial *gpu_material = material->shading.gpumat;
    blender::Material *bl_material = GPU_material_get_material(gpu_material);

    if (GPU_material_has_displacement_output(gpu_material)) {
      inflate_bounds = math::max(inflate_bounds, bl_material->inflate_bounds);
    }

    inst_.cryptomatte.sync_material(bl_material);
  }

  inst_.cryptomatte.sync_object(ob_handle);

  inst_.shadows.sync_object(ob_handle, is_alpha_blend, has_transparent_shadows);

  if (has_volume) {
    inst_.volume.object_sync(ob_handle);
  }

  if (inflate_bounds != 0.0f) {
    inst_.manager->update_handle_bounds(ob_handle.res_handle, ob_handle, inflate_bounds);
  }

  inst_.manager->extract_object_attributes(ob_handle.res_handle, ob_handle, gpu_materials);
}

/** \} */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh
 * \{ */

void SyncModule::sync_mesh(const ObjectRef &ob_ref)
{
  if (!inst_.use_surfaces) {
    return;
  }

  if ((ob_ref.object->dt < OB_SOLID) &&
      (inst_.is_viewport() && inst_.v3d->shading.type != OB_RENDER))
  {
    /** Do not render objects with display type lower than solid when in material preview mode. */
    return;
  }

  ObjectHandle ob_handle = sync_object(ob_ref, inst_.manager->unique_handle(ob_ref));

  bool has_motion = inst_.velocity.step_object_sync(ob_handle);

  MaterialArray &material_array = inst_.materials.material_array_get(ob_handle, has_motion);

  Span<gpu::Batch *> mat_geom = DRW_cache_object_surface_material_get(
      ob_handle.object, material_array.gpu_materials);
  if (mat_geom.is_empty()) {
    return;
  }

  Vector<Material *, 8> synced_materials;

  for (auto i : material_array.gpu_materials.index_range()) {
    gpu::Batch *geom = mat_geom[i];
    if (geom == nullptr) {
      continue;
    }

    Material &material = material_array.materials[i];
    synced_materials.append(&material);

    if (material.has_volume) {
      sync_volume_passes(ob_handle, material, [&](const MaterialPass &pass, int instance) {
        geometry_volume_call(pass.sub_pass,
                             pass.gpumat,
                             inst_.scene,
                             ob_handle.object,
                             geom,
                             ob_handle.res_handle.sub_handle(instance));
      });

      /* Do not render surface if we are rendering a volume object
       * and do not have a surface closure. */
      if (!material.has_surface) {
        continue;
      }
    }

    sync_common_passes(material, [&](const MaterialPass &pass) {
      geometry_call(pass.sub_pass, geom, ob_handle.res_handle);
    });

    sync_alpha_blended_passes(ob_handle, material, [&](const MaterialPass &pass, int instance) {
      geometry_call(pass.sub_pass, geom, ob_handle.res_handle.sub_handle(instance));
    });
  }

  sync_common(ob_handle, synced_materials.as_span(), material_array.gpu_materials);
}

bool SyncModule::sync_sculpt(const ObjectRef &ob_ref)
{
  if (!inst_.use_surfaces) {
    return false;
  }

  bool pbvh_draw = BKE_sculptsession_use_pbvh_draw(ob_ref.object, inst_.rv3d) &&
                   !inst_.is_image_render;
  if (!pbvh_draw) {
    return false;
  }

  ObjectHandle ob_handle = sync_object(ob_ref, inst_.manager->unique_handle_for_sculpt(ob_ref));

  bool has_motion = false;
  MaterialArray &material_array = inst_.materials.material_array_get(ob_handle, has_motion);

  Vector<Material *, 8> synced_materials;

  for (SculptBatch &batch :
       sculpt_batches_per_material_get(ob_ref.object, material_array.gpu_materials))
  {
    gpu::Batch *geom = batch.batch;
    if (geom == nullptr) {
      continue;
    }

    Material &material = material_array.materials[batch.material_slot];
    synced_materials.append(&material);

    if (material.has_volume) {
      sync_volume_passes(ob_handle, material, [&](const MaterialPass &pass, int instance) {
        geometry_volume_call(pass.sub_pass,
                             pass.gpumat,
                             inst_.scene,
                             ob_handle.object,
                             geom,
                             ob_handle.res_handle.sub_handle(instance));
      });

      /* Do not render surface if we are rendering a volume object
       * and do not have a surface closure. */
      if (!material.has_surface) {
        continue;
      }
    }

    sync_common_passes(material, [&](const MaterialPass &pass) {
      geometry_call(pass.sub_pass, geom, ob_handle.res_handle);
    });

    sync_alpha_blended_passes(ob_handle, material, [&](const MaterialPass &pass, int instance) {
      geometry_call(pass.sub_pass, geom, ob_handle.res_handle.sub_handle(instance));
    });
  }

  sync_common(ob_handle, synced_materials.as_span(), material_array.gpu_materials);

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Point Cloud
 * \{ */

void SyncModule::sync_pointcloud(const ObjectRef &ob_ref)
{
  const int material_slot = POINTCLOUD_MATERIAL_NR;

  ObjectHandle ob_handle = sync_object(ob_ref, inst_.manager->unique_handle(ob_ref));

  bool has_motion = inst_.velocity.step_object_sync(ob_handle);

  Material material = inst_.materials.material_get(
      ob_handle, has_motion, material_slot - 1, MAT_GEOM_POINTCLOUD);

  auto drawcall_add = [&](const MaterialPass &matpass, bool dual_sided = false) {
    if (matpass.sub_pass == nullptr) {
      return;
    }
    PassMain::Sub &object_pass = matpass.sub_pass->sub("Point Cloud Sub Pass");
    gpu::Batch *geometry = pointcloud_sub_pass_setup(
        object_pass, ob_handle.object, matpass.gpumat);
    if (dual_sided) {
      /* WORKAROUND: Hack to generate backfaces. Should also be baked into the Index Buf too at
       * some point in the future. */
      object_pass.push_constant("ptcloud_backface", false);
      object_pass.draw(geometry, ob_handle.res_handle);
      object_pass.push_constant("ptcloud_backface", true);
      object_pass.draw(geometry, ob_handle.res_handle);
    }
    else {
      object_pass.push_constant("ptcloud_backface", false);
      object_pass.draw(geometry, ob_handle.res_handle);
    }
  };

  if (material.has_volume) {
    /* Volumes not supported for now. */
    /* NOTE: Point Cloud volumes used to work at some point,
     * but we didn't catch the regression due to the test being disabled. :( */
    if (!material.has_surface) {
      /* Do not render surface if we are rendering a volume object
       * and do not have a surface closure. */
      return;
    }
  }

  sync_common_passes(material, [&](const MaterialPass &pass) { drawcall_add(pass); });

  sync_alpha_blended_passes(ob_handle, material, [&](const MaterialPass &pass, int /*instance*/) {
    drawcall_add(pass);
  });

  sync_common(ob_handle, {&material}, {material.shading.gpumat});
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Volume Objects
 * \{ */

void SyncModule::sync_volume(const ObjectRef &ob_ref)
{
  if (!inst_.use_volumes) {
    return;
  }

  ObjectHandle ob_handle = sync_object(ob_ref, inst_.manager->unique_handle(ob_ref));

  const int material_slot = VOLUME_MATERIAL_NR;

  /* Motion is not supported on volumes yet. */
  const bool has_motion = false;

  Material material = inst_.materials.material_get(
      ob_handle, has_motion, material_slot - 1, MAT_GEOM_VOLUME);

  if (!material.has_volume) {
    return;
  }

  /* Do not render the object if there is no attribute used in the volume.
   * This mimic Cycles behavior (see #124061). */
  ListBaseT<GPUMaterialAttribute> attr_list = GPU_material_attributes(
      material.volume_material.gpumat);
  if (BLI_listbase_is_empty(&attr_list)) {
    return;
  }

  /* Use bounding box tag empty spaces. */
  gpu::Batch *geom = inst_.volume.unit_cube_batch_get();
  bool is_rendered = false;

  sync_volume_passes(ob_handle, material, [&](const MaterialPass &pass, int /*instance*/) {
    if (pass.sub_pass == nullptr) {
      return;
    }
    BLI_assert_msg(!ob_handle.is_range(),
                   "volume_object_grids_init pass setup is object/instance specific.");
    PassMain::Sub *object_pass = volume_sub_pass(
        *pass.sub_pass, inst_.scene, ob_handle.object, pass.gpumat);
    if (object_pass != nullptr) {
      object_pass->draw(geom, ob_handle.res_handle);
      is_rendered = true;
    }
  });

  if (!is_rendered) {
    return;
  }

  inst_.volume.object_sync(ob_handle);

  inst_.manager->extract_object_attributes(
      ob_handle.res_handle, ob_handle, material.volume_material.gpumat);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Hair
 * \{ */

void SyncModule::sync_curves(const ObjectRef &ob_ref, HairParticleInfo const *hair_particle)
{
  if (!inst_.use_curves) {
    return;
  }

  int mat_nr = CURVES_MATERIAL_NR;
  if (hair_particle != nullptr) {
    mat_nr = hair_particle->psys.part->omat;
  }

  ObjectHandle ob_handle = hair_particle ?
                               ObjectHandle(ob_ref,
                                            inst_.manager->resource_handle_for_psys(
                                                ob_ref, ob_ref.object_to_world()),
                                            hair_particle->recalc_flags,
                                            hair_particle->sub_key) :
                               sync_object(ob_ref, inst_.manager->unique_handle(ob_ref));

  bool has_motion = inst_.velocity.step_object_sync(ob_handle, hair_particle);
  Material material = inst_.materials.material_get(
      ob_handle, has_motion, mat_nr - 1, MAT_GEOM_CURVES);

  auto drawcall_add = [&](const MaterialPass &matpass) {
    if (matpass.sub_pass == nullptr) {
      return;
    }
    if (hair_particle != nullptr) {
      PassMain::Sub &sub_pass = matpass.sub_pass->sub("Hair SubPass");
      gpu::Batch *geometry = hair_sub_pass_setup(
          sub_pass, inst_.scene, ob_ref, &hair_particle->psys, &hair_particle->md, matpass.gpumat);
      sub_pass.draw(geometry, ob_handle.res_handle);
    }
    else {
      PassMain::Sub &sub_pass = matpass.sub_pass->sub("Curves SubPass");
      const char *error = nullptr;
      gpu::Batch *geometry = curves_sub_pass_setup(
          sub_pass, inst_.scene, ob_ref.object, error, matpass.gpumat);
      if (error) {
        inst_.info_append(error);
      }
      sub_pass.draw(geometry, ob_handle.res_handle);
    }
  };

  if (material.has_volume) {
    /* Volumes not supported for now. */
    if (!material.has_surface) {
      /* Do not render surface if we are rendering a volume object
       * and do not have a surface closure. */
      return;
    }
  }

  sync_common_passes(material, [&](const MaterialPass &pass) { drawcall_add(pass); });

  sync_alpha_blended_passes(ob_handle, material, [&](const MaterialPass &pass, int /*instance*/) {
    drawcall_add(pass);
  });

  sync_common(ob_handle, {&material}, {material.shading.gpumat});
}

/** \} */

void foreach_hair_particle(Instance &inst, ObjectRef &ob_ref, HairHandleCallback callback)
{
  uint sub_key = 1;

  for (ModifierData &md : ob_ref.object->modifiers) {
    if (md.type == eModifierType_ParticleSystem) {
      ParticleSystem *particle_sys = reinterpret_cast<ParticleSystemModifierData *>(&md)->psys;
      ParticleSettings *part_settings = particle_sys->part;
      /* Only use the viewport drawing mode for material preview. */
      const int draw_as = (part_settings->draw_as == PART_DRAW_REND || !inst.is_viewport()) ?
                              part_settings->ren_as :
                              part_settings->draw_as;
      if (draw_as != PART_DRAW_PATH ||
          !DRW_object_is_visible_psys_in_active_context(ob_ref.object, particle_sys))
      {
        continue;
      }
      callback({md, *particle_sys, uint(particle_sys->recalc), sub_key++});
    }
  }
}

}  // namespace blender::eevee
