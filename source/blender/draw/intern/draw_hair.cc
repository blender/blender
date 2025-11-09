/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * \brief Contains procedural GPU hair drawing methods.
 */

#include "BKE_customdata.hh"

#include "DNA_modifier_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"

#include "DRW_render.hh"

#include "GPU_batch.hh"
#include "GPU_material.hh"
#include "GPU_shader.hh"
#include "GPU_texture.hh"
#include "GPU_vertex_buffer.hh"

#include "draw_common_c.hh"
#include "draw_context_private.hh"
#include "draw_hair_private.hh"
#include "draw_manager.hh"
#include "draw_shader_shared.hh"

/* New Draw Manager. */
#include "draw_common.hh"

namespace blender::draw {

blender::gpu::VertBuf *hair_pos_buffer_get(Scene *scene,
                                           Object *object,
                                           ParticleSystem *psys,
                                           ModifierData *md)
{
  /* TODO(fclem): Remove Global access. */
  CurvesModule &module = *drw_get().data->curves_module;

  drw_particle_update_ptcache(object, psys);
  ParticleDrawSource source = drw_particle_get_hair_source(
      object, psys, md, nullptr, scene->r.hair_subdiv);

  CurvesEvalCache &cache = hair_particle_get_eval_cache(source);
  cache.ensure_positions(module, source);

  return cache.evaluated_pos_rad_buf.get();
}

template<typename PassT>
blender::gpu::Batch *hair_sub_pass_setup_implementation(PassT &sub_ps,
                                                        const Scene *scene,
                                                        const ObjectRef &ob_ref,
                                                        ParticleSystem *psys,
                                                        ModifierData *md,
                                                        GPUMaterial *gpu_material)
{
  /** NOTE: This still relies on the old DRW_hair implementation. */
  Object *object = ob_ref.object;

  drw_particle_update_ptcache(object, psys);

  ParticleDrawSource source = drw_particle_get_hair_source(
      object, psys, md, nullptr, scene->r.hair_subdiv);

  CurvesEvalCache &cache = hair_particle_get_eval_cache(source);

  const int face_per_segment = (scene->r.hair_type == SCE_HAIR_SHAPE_STRAND)   ? 0 :
                               (scene->r.hair_type == SCE_HAIR_SHAPE_CYLINDER) ? 3 :
                                                                                 1;

  if (source.evaluated_points_num() == 0) {
    /* Nothing to draw. Just return an empty drawcall that will be skipped. */
    bool unused_error;
    return cache.batch_get(0, 0, face_per_segment, false, unused_error);
  }

  /* TODO(fclem): Remove Global access. */
  CurvesModule &module = *drw_get().data->curves_module;

  cache.ensure_positions(module, source);
  cache.ensure_attributes(module, source, gpu_material);

  gpu::VertBufPtr &indirection_buf = cache.indirection_buf_get(module, source, face_per_segment);

  {
    ParticleSystemModifierData *psmd = (ParticleSystemModifierData *)source.md;
    Mesh &mesh = *psmd->mesh_final;
    const StringRef active_uv = mesh.active_uv_map_name();
    curves_bind_resources(
        sub_ps, module, cache, face_per_segment, gpu_material, indirection_buf, active_uv);
  }

  bool unused_error;
  return cache.batch_get(
      source.evaluated_points_num(), source.curves_num(), face_per_segment, false, unused_error);
}

blender::gpu::Batch *hair_sub_pass_setup(PassMain::Sub &sub_ps,
                                         const Scene *scene,
                                         const ObjectRef &ob_ref,
                                         ParticleSystem *psys,
                                         ModifierData *md,
                                         GPUMaterial *gpu_material)
{
  return hair_sub_pass_setup_implementation(sub_ps, scene, ob_ref, psys, md, gpu_material);
}

blender::gpu::Batch *hair_sub_pass_setup(PassSimple::Sub &sub_ps,
                                         const Scene *scene,
                                         const ObjectRef &ob_ref,
                                         ParticleSystem *psys,
                                         ModifierData *md,
                                         GPUMaterial *gpu_material)
{
  return hair_sub_pass_setup_implementation(sub_ps, scene, ob_ref, psys, md, gpu_material);
}

}  // namespace blender::draw
