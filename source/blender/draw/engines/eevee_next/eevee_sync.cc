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

void SyncModule::view_update()
{
  if (DEG_id_type_updated(inst_.depsgraph, ID_WO)) {
    world_updated_ = true;
  }
}

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

WorldHandle SyncModule::sync_world()
{
  WorldHandle handle;
  handle.recalc = world_updated_ ? int(ID_RECALC_SHADING) : 0;
  world_updated_ = false;
  return handle;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Common
 * \{ */

static inline void geometry_call(PassMain::Sub *sub_pass,
                                 GPUBatch *geom,
                                 ResourceHandle resource_handle)
{
  if (sub_pass != nullptr) {
    sub_pass->draw(geom, resource_handle);
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

  GPUBatch **mat_geom = DRW_cache_object_surface_material_get(
      ob, material_array.gpu_materials.data(), material_array.gpu_materials.size());

  if (mat_geom == nullptr) {
    return;
  }

  if ((ob->dt < OB_SOLID) && !DRW_state_is_scene_render()) {
    /** NOTE:
     * EEVEE doesn't render meshes with bounds or wire display type in the viewport,
     * but Cycles does. */
    return;
  }

  bool is_alpha_blend = false;
  float inflate_bounds = 0.0f;
  for (auto i : material_array.gpu_materials.index_range()) {
    GPUBatch *geom = mat_geom[i];
    if (geom == nullptr) {
      continue;
    }

    Material &material = material_array.materials[i];
    GPUMaterial *gpu_material = material_array.gpu_materials[i];

    if (material.has_volume && (i == 0)) {
      /* Only support single volume material for now. */
      geometry_call(material.volume_occupancy.sub_pass, geom, res_handle);
      inst_.pipelines.volume.material_call(material.volume_material, ob, res_handle);
      /* Do not render surface if we are rendering a volume object
       * and do not have a surface closure. */
      if (gpu_material && !GPU_material_has_surface_output(gpu_material)) {
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
    geometry_call(material.reflection_probe_prepass.sub_pass, geom, res_handle);
    geometry_call(material.reflection_probe_shading.sub_pass, geom, res_handle);

    is_alpha_blend = is_alpha_blend || material.is_alpha_blend_transparent;

    ::Material *mat = GPU_material_get_material(gpu_material);
    inst_.cryptomatte.sync_material(mat);

    if (GPU_material_has_displacement_output(gpu_material)) {
      inflate_bounds = math::max(inflate_bounds, mat->inflate_bounds);
    }
  }

  if (inflate_bounds != 0.0f) {
    inst_.manager->update_handle_bounds(res_handle, ob_ref, inflate_bounds);
  }

  inst_.manager->extract_object_attributes(res_handle, ob_ref, material_array.gpu_materials);

  inst_.shadows.sync_object(ob, ob_handle, res_handle, is_alpha_blend);
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
  /* Needed for mesh cache validation, to prevent two copies of
   * of vertex color arrays from being sent to the GPU (e.g.
   * when switching from eevee to workbench).
   */
  if (ob_ref.object->sculpt && ob_ref.object->sculpt->pbvh) {
    BKE_pbvh_is_drawing_set(ob_ref.object->sculpt->pbvh, pbvh_draw);
  }

  if (!pbvh_draw) {
    return false;
  }

  bool has_motion = false;
  MaterialArray &material_array = inst_.materials.material_array_get(ob, has_motion);

  bool is_alpha_blend = false;
  float inflate_bounds = 0.0f;
  for (SculptBatch &batch :
       sculpt_batches_per_material_get(ob_ref.object, material_array.gpu_materials))
  {
    GPUBatch *geom = batch.batch;
    if (geom == nullptr) {
      continue;
    }

    Material &material = material_array.materials[batch.material_slot];

    if (material.has_volume && (batch.material_slot == 0)) {
      /* Only support single volume material for now. */
      geometry_call(material.volume_occupancy.sub_pass, geom, res_handle);
      inst_.pipelines.volume.material_call(material.volume_material, ob, res_handle);
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
    geometry_call(material.reflection_probe_prepass.sub_pass, geom, res_handle);
    geometry_call(material.reflection_probe_shading.sub_pass, geom, res_handle);

    is_alpha_blend = is_alpha_blend || material.is_alpha_blend_transparent;

    GPUMaterial *gpu_material = material_array.gpu_materials[batch.material_slot];
    ::Material *mat = GPU_material_get_material(gpu_material);
    inst_.cryptomatte.sync_material(mat);

    if (GPU_material_has_displacement_output(gpu_material)) {
      inflate_bounds = math::max(inflate_bounds, mat->inflate_bounds);
    }
  }

  /* Use a valid bounding box. The PBVH module already does its own culling, but a valid */
  /* bounding box is still needed for directional shadow tile-map bounds computation. */
  const Bounds<float3> bounds = BKE_pbvh_bounding_box(ob_ref.object->sculpt->pbvh);
  const float3 center = math::midpoint(bounds.min, bounds.max);
  const float3 half_extent = bounds.max - center + inflate_bounds;
  inst_.manager->update_handle_bounds(res_handle, center, half_extent);

  inst_.manager->extract_object_attributes(res_handle, ob_ref, material_array.gpu_materials);

  inst_.shadows.sync_object(ob, ob_handle, res_handle, is_alpha_blend);
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
    GPUBatch *geometry = point_cloud_sub_pass_setup(object_pass, ob, matpass.gpumat);
    object_pass.draw(geometry, res_handle);
  };

  if (material.has_volume) {
    /* Only support single volume material for now. */
    drawcall_add(material.volume_occupancy);
    inst_.pipelines.volume.material_call(material.volume_material, ob, res_handle);

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
  drawcall_add(material.reflection_probe_prepass);
  drawcall_add(material.reflection_probe_shading);

  inst_.cryptomatte.sync_object(ob, res_handle);
  GPUMaterial *gpu_material =
      inst_.materials.material_array_get(ob, has_motion).gpu_materials[material_slot - 1];
  ::Material *mat = GPU_material_get_material(gpu_material);
  inst_.cryptomatte.sync_material(mat);

  bool is_alpha_blend = material.is_alpha_blend_transparent;

  if (GPU_material_has_displacement_output(gpu_material) && mat->inflate_bounds != 0.0f) {
    inst_.manager->update_handle_bounds(res_handle, ob_ref, mat->inflate_bounds);
  }

  inst_.shadows.sync_object(ob, ob_handle, res_handle, is_alpha_blend);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Volume Objects
 * \{ */

void SyncModule::sync_volume(Object *ob, ObjectHandle & /*ob_handle*/, ResourceHandle res_handle)
{
  if (!inst_.use_volumes) {
    return;
  }

  const int material_slot = VOLUME_MATERIAL_NR;

  /* Motion is not supported on volumes yet. */
  const bool has_motion = false;

  Material &material = inst_.materials.material_get(
      ob, has_motion, material_slot - 1, MAT_GEOM_VOLUME);

  /* Use bounding box tag empty spaces. */
  GPUBatch *geom = DRW_cache_cube_get();

  geometry_call(material.volume_occupancy.sub_pass, geom, res_handle);

  inst_.pipelines.volume.material_call(material.volume_material, ob, res_handle);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPencil
 * \{ */

#define DO_BATCHING true

struct gpIterData {
  Instance &inst;
  Object *ob;
  MaterialArray &material_array;
  int cfra;

  /* Drawcall batching. */
  GPUBatch *geom = nullptr;
  Material *material = nullptr;
  int vfirst = 0;
  int vcount = 0;
  bool instancing = false;

  gpIterData(Instance &inst_, Object *ob_, ObjectHandle &ob_handle, ResourceHandle resource_handle)
      : inst(inst_),
        ob(ob_),
        material_array(inst_.materials.material_array_get(
            ob_,
            inst_.velocity.step_object_sync(
                ob, ob_handle.object_key, resource_handle, ob_handle.recalc)))
  {
    cfra = DEG_get_ctime(inst.depsgraph);
  };
};

static void gpencil_drawcall_flush(gpIterData &iter)
{
#if 0 /* Incompatible with new draw manager. */
  if (iter.geom != nullptr) {
    geometry_call(iter.material->shading.sub_pass,
                  iter.ob,
                  iter.geom,
                  iter.vfirst,
                  iter.vcount,
                  iter.instancing);
    geometry_call(iter.material->prepass.sub_pass,
                  iter.ob,
                  iter.geom,
                  iter.vfirst,
                  iter.vcount,
                  iter.instancing);
    geometry_call(iter.material->shadow.sub_pass,
                  iter.ob,
                  iter.geom,
                  iter.vfirst,
                  iter.vcount,
                  iter.instancing);
  }
#endif
  iter.geom = nullptr;
  iter.vfirst = -1;
  iter.vcount = 0;
}

/* Group draw-calls that are consecutive and with the same type. Reduces GPU driver overhead. */
static void gpencil_drawcall_add(gpIterData &iter,
                                 GPUBatch *geom,
                                 Material *material,
                                 int v_first,
                                 int v_count,
                                 bool instancing)
{
  int last = iter.vfirst + iter.vcount;
  /* Interrupt draw-call grouping if the sequence is not consecutive. */
  if (!DO_BATCHING || (geom != iter.geom) || (material != iter.material) || (v_first - last > 3)) {
    gpencil_drawcall_flush(iter);
  }
  iter.geom = geom;
  iter.material = material;
  iter.instancing = instancing;
  if (iter.vfirst == -1) {
    iter.vfirst = v_first;
  }
  iter.vcount = v_first + v_count - iter.vfirst;
}

static void gpencil_stroke_sync(bGPDlayer * /*gpl*/,
                                bGPDframe * /*gpf*/,
                                bGPDstroke *gps,
                                void *thunk)
{
  gpIterData &iter = *(gpIterData *)thunk;

  Material *material = &iter.material_array.materials[gps->mat_nr];
  MaterialGPencilStyle *gp_style = BKE_gpencil_material_settings(iter.ob, gps->mat_nr + 1);

  bool hide_material = (gp_style->flag & GP_MATERIAL_HIDE) != 0;
  bool show_stroke = ((gp_style->flag & GP_MATERIAL_STROKE_SHOW) != 0) ||
                     (!DRW_state_is_image_render() && ((gps->flag & GP_STROKE_NOFILL) != 0));
  bool show_fill = (gps->tot_triangles > 0) && ((gp_style->flag & GP_MATERIAL_FILL_SHOW) != 0);

  if (hide_material) {
    return;
  }

  GPUBatch *geom = DRW_cache_gpencil_get(iter.ob, iter.cfra);

  if (show_fill) {
    int vfirst = gps->runtime.fill_start * 3;
    int vcount = gps->tot_triangles * 3;
    gpencil_drawcall_add(iter, geom, material, vfirst, vcount, false);
  }

  if (show_stroke) {
    /* Start one vert before to have gl_InstanceID > 0 (see shader). */
    int vfirst = gps->runtime.stroke_start * 3;
    /* Include "potential" cyclic vertex and start adj vertex (see shader). */
    int vcount = gps->totpoints + 1 + 1;
    gpencil_drawcall_add(iter, geom, material, vfirst, vcount, true);
  }
}

void SyncModule::sync_gpencil(Object *ob, ObjectHandle &ob_handle, ResourceHandle res_handle)
{
  /* TODO(fclem): Waiting for a user option to use the render engine instead of gpencil engine. */
  if (true) {
    inst_.gpencil_engine_enabled = true;
    return;
  }
  /* Is this a surface or curves? */
  if (!inst_.use_surfaces) {
    return;
  }

  UNUSED_VARS(res_handle);

  gpIterData iter(inst_, ob, ob_handle, res_handle);

  BKE_gpencil_visible_stroke_iter((bGPdata *)ob->data, nullptr, gpencil_stroke_sync, &iter);

  gpencil_drawcall_flush(iter);

  bool is_alpha_blend = true; /* TODO material.is_alpha_blend. */
  inst_.shadows.sync_object(ob, ob_handle, res_handle, is_alpha_blend);
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
      GPUBatch *geometry = hair_sub_pass_setup(
          sub_pass, inst_.scene, ob, particle_sys, modifier_data, matpass.gpumat);
      sub_pass.draw(geometry, res_handle);
    }
    else {
      PassMain::Sub &sub_pass = matpass.sub_pass->sub("Curves SubPass");
      GPUBatch *geometry = curves_sub_pass_setup(sub_pass, inst_.scene, ob, matpass.gpumat);
      sub_pass.draw(geometry, res_handle);
    }
  };

  if (material.has_volume) {
    /* Only support single volume material for now. */
    drawcall_add(material.volume_occupancy);
    inst_.pipelines.volume.material_call(material.volume_material, ob, res_handle);
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
  drawcall_add(material.reflection_probe_prepass);
  drawcall_add(material.reflection_probe_shading);

  inst_.cryptomatte.sync_object(ob, res_handle);
  GPUMaterial *gpu_material =
      inst_.materials.material_array_get(ob, has_motion).gpu_materials[mat_nr - 1];
  ::Material *mat = GPU_material_get_material(gpu_material);
  inst_.cryptomatte.sync_material(mat);

  bool is_alpha_blend = material.is_alpha_blend_transparent;

  if (GPU_material_has_displacement_output(gpu_material) && mat->inflate_bounds != 0.0f) {
    inst_.manager->update_handle_bounds(res_handle, ob_ref, mat->inflate_bounds);
  }

  inst_.shadows.sync_object(ob, ob_handle, res_handle, is_alpha_blend);
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
