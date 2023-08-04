/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * Converts the different renderable object types to drawcalls.
 */

#include "eevee_engine.h"

#include "BKE_gpencil_legacy.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_pbvh_api.hh"
#include "DEG_depsgraph_query.h"
#include "DNA_curves_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_modifier_types.h"
#include "DNA_particle_types.h"

#include "draw_common.hh"
#include "draw_sculpt.hh"

#include "eevee_instance.hh"

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name Draw Data
 *
 * \{ */

static void draw_data_init_cb(DrawData *dd)
{
  /* Object has just been created or was never evaluated by the engine. */
  dd->recalc = ID_RECALC_ALL;
}

ObjectHandle &SyncModule::sync_object(Object *ob)
{
  DrawEngineType *owner = (DrawEngineType *)&DRW_engine_viewport_eevee_next_type;
  DrawData *dd = DRW_drawdata_ensure(
      (ID *)ob, owner, sizeof(eevee::ObjectHandle), draw_data_init_cb, nullptr);
  ObjectHandle &eevee_dd = *reinterpret_cast<ObjectHandle *>(dd);

  if (eevee_dd.object_key.ob == nullptr) {
    ob = DEG_get_original_object(ob);
    eevee_dd.object_key = ObjectKey(ob);
  }

  const int recalc_flags = ID_RECALC_COPY_ON_WRITE | ID_RECALC_TRANSFORM | ID_RECALC_SHADING |
                           ID_RECALC_GEOMETRY;
  if ((eevee_dd.recalc & recalc_flags) != 0) {
    inst_.sampling.reset();
  }

  return eevee_dd;
}

WorldHandle &SyncModule::sync_world(::World *world)
{
  DrawEngineType *owner = (DrawEngineType *)&DRW_engine_viewport_eevee_next_type;
  DrawData *dd = DRW_drawdata_ensure(
      (ID *)world, owner, sizeof(eevee::WorldHandle), draw_data_init_cb, nullptr);
  WorldHandle &eevee_dd = *reinterpret_cast<WorldHandle *>(dd);

  const int recalc_flags = ID_RECALC_ALL;
  if ((eevee_dd.recalc & recalc_flags) != 0) {
    inst_.sampling.reset();
  }
  return eevee_dd;
}

SceneHandle &SyncModule::sync_scene(::Scene *scene)
{
  DrawEngineType *owner = (DrawEngineType *)&DRW_engine_viewport_eevee_next_type;
  DrawData *dd = DRW_drawdata_ensure(
      (ID *)scene, owner, sizeof(eevee::SceneHandle), draw_data_init_cb, nullptr);
  SceneHandle &eevee_dd = *reinterpret_cast<SceneHandle *>(dd);

  const int recalc_flags = ID_RECALC_ALL;
  if ((eevee_dd.recalc & recalc_flags) != 0) {
    inst_.sampling.reset();
  }
  return eevee_dd;
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
  bool has_motion = inst_.velocity.step_object_sync(
      ob, ob_handle.object_key, res_handle, ob_handle.recalc);

  MaterialArray &material_array = inst_.materials.material_array_get(ob, has_motion);

  GPUBatch **mat_geom = DRW_cache_object_surface_material_get(
      ob, material_array.gpu_materials.data(), material_array.gpu_materials.size());

  if (mat_geom == nullptr) {
    return;
  }

  bool is_shadow_caster = false;
  bool is_alpha_blend = false;
  bool do_probe_sync = inst_.do_probe_sync();
  for (auto i : material_array.gpu_materials.index_range()) {
    GPUBatch *geom = mat_geom[i];
    if (geom == nullptr) {
      continue;
    }
    Material &material = material_array.materials[i];
    geometry_call(material.shading.sub_pass, geom, res_handle);
    geometry_call(material.prepass.sub_pass, geom, res_handle);
    geometry_call(material.shadow.sub_pass, geom, res_handle);
    geometry_call(material.capture.sub_pass, geom, res_handle);
    if (do_probe_sync) {
      geometry_call(material.probe_prepass.sub_pass, geom, res_handle);
      geometry_call(material.probe_shading.sub_pass, geom, res_handle);
    }

    is_shadow_caster = is_shadow_caster || material.shadow.sub_pass != nullptr;
    is_alpha_blend = is_alpha_blend || material.is_alpha_blend_transparent;

    GPUMaterial *gpu_material = material_array.gpu_materials[i];
    ::Material *mat = GPU_material_get_material(gpu_material);
    inst_.cryptomatte.sync_material(mat);
  }

  inst_.manager->extract_object_attributes(res_handle, ob_ref, material_array.gpu_materials);

  inst_.shadows.sync_object(ob_handle, res_handle, is_shadow_caster, is_alpha_blend);
  inst_.cryptomatte.sync_object(ob, res_handle);
}

bool SyncModule::sync_sculpt(Object *ob,
                             ObjectHandle &ob_handle,
                             ResourceHandle res_handle,
                             const ObjectRef &ob_ref)
{
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

  /* Use a valid bounding box. The PBVH module already does its own culling,
   * but a valid bounding box is still needed for directional shadow tilemap bounds computation. */
  float3 min, max;
  BKE_pbvh_bounding_box(ob_ref.object->sculpt->pbvh, min, max);
  float3 center = (min + max) * 0.5;
  float3 half_extent = max - center;
  res_handle = inst_.manager->resource_handle(ob_ref, nullptr, &center, &half_extent);

  bool has_motion = false;
  MaterialArray &material_array = inst_.materials.material_array_get(ob, has_motion);

  bool is_shadow_caster = false;
  bool is_alpha_blend = false;
  bool do_probe_sync = inst_.do_probe_sync();
  for (SculptBatch &batch :
       sculpt_batches_per_material_get(ob_ref.object, material_array.gpu_materials))
  {
    GPUBatch *geom = batch.batch;
    if (geom == nullptr) {
      continue;
    }

    Material &material = material_array.materials[batch.material_slot];

    geometry_call(material.shading.sub_pass, geom, res_handle);
    geometry_call(material.prepass.sub_pass, geom, res_handle);
    geometry_call(material.shadow.sub_pass, geom, res_handle);

    /* TODO(Miguel Pozo): Is this needed ? */
    geometry_call(material.capture.sub_pass, geom, res_handle);
    if (do_probe_sync) {
      geometry_call(material.probe_prepass.sub_pass, geom, res_handle);
      geometry_call(material.probe_shading.sub_pass, geom, res_handle);
    }

    is_shadow_caster = is_shadow_caster || material.shadow.sub_pass != nullptr;
    is_alpha_blend = is_alpha_blend || material.is_alpha_blend_transparent;

    GPUMaterial *gpu_material = material_array.gpu_materials[batch.material_slot];
    ::Material *mat = GPU_material_get_material(gpu_material);
    inst_.cryptomatte.sync_material(mat);
  }

  inst_.manager->extract_object_attributes(res_handle, ob_ref, material_array.gpu_materials);

  inst_.shadows.sync_object(ob_handle, res_handle, is_shadow_caster, is_alpha_blend);
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
                                  const ObjectRef & /*ob_ref*/)
{
  int material_slot = 1;

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

  drawcall_add(material.shading);
  drawcall_add(material.prepass);
  drawcall_add(material.shadow);

  inst_.cryptomatte.sync_object(ob, res_handle);
  GPUMaterial *gpu_material =
      inst_.materials.material_array_get(ob, has_motion).gpu_materials[material_slot - 1];
  ::Material *mat = GPU_material_get_material(gpu_material);
  inst_.cryptomatte.sync_material(mat);

  bool is_caster = material.shadow.sub_pass != nullptr;
  bool is_alpha_blend = material.is_alpha_blend_transparent;
  inst_.shadows.sync_object(ob_handle, res_handle, is_caster, is_alpha_blend);
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
  UNUSED_VARS(res_handle);

  gpIterData iter(inst_, ob, ob_handle, res_handle);

  BKE_gpencil_visible_stroke_iter((bGPdata *)ob->data, nullptr, gpencil_stroke_sync, &iter);

  gpencil_drawcall_flush(iter);

  bool is_caster = true;      /* TODO material.shadow.sub_pass. */
  bool is_alpha_blend = true; /* TODO material.is_alpha_blend. */
  inst_.shadows.sync_object(ob_handle, res_handle, is_caster, is_alpha_blend);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Hair
 * \{ */

void SyncModule::sync_curves(Object *ob,
                             ObjectHandle &ob_handle,
                             ResourceHandle res_handle,
                             ModifierData *modifier_data,
                             ParticleSystem *particle_sys)
{
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

  drawcall_add(material.shading);
  drawcall_add(material.prepass);
  drawcall_add(material.shadow);

  inst_.cryptomatte.sync_object(ob, res_handle);
  GPUMaterial *gpu_material =
      inst_.materials.material_array_get(ob, has_motion).gpu_materials[mat_nr - 1];
  ::Material *mat = GPU_material_get_material(gpu_material);
  inst_.cryptomatte.sync_material(mat);

  bool is_caster = material.shadow.sub_pass != nullptr;
  bool is_alpha_blend = material.is_alpha_blend_transparent;
  inst_.shadows.sync_object(ob_handle, res_handle, is_caster, is_alpha_blend);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Light Probes
 * \{ */

void SyncModule::sync_light_probe(Object *ob, ObjectHandle &ob_handle)
{
  inst_.light_probes.sync_probe(ob, ob_handle);
  inst_.reflection_probes.sync_object(ob, ob_handle);
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
          !DRW_object_is_visible_psys_in_active_context(ob, particle_sys)) {
        continue;
      }

      ObjectHandle particle_sys_handle = {nullptr};
      particle_sys_handle.object_key = ObjectKey(ob_handle.object_key.ob, sub_key++);
      particle_sys_handle.recalc = particle_sys->recalc;

      callback(particle_sys_handle, *md, *particle_sys);
    }
  }
}

}  // namespace blender::eevee
