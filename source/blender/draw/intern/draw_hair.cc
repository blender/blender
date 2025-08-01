/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * \brief Contains procedural GPU hair drawing methods.
 */

#include "DNA_scene_types.h"
#include "DRW_render.hh"

#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"

#include "DNA_collection_types.h"
#include "DNA_modifier_types.h"
#include "DNA_particle_types.h"

#include "BKE_duplilist.hh"

#include "GPU_batch.hh"
#include "GPU_capabilities.hh"
#include "GPU_material.hh"
#include "GPU_shader.hh"
#include "GPU_texture.hh"
#include "GPU_vertex_buffer.hh"

#include "DRW_gpu_wrapper.hh"

#include "draw_common_c.hh"
#include "draw_context_private.hh"
#include "draw_hair_private.hh"
#include "draw_manager.hh"
#include "draw_shader.hh"
#include "draw_shader_shared.hh"

static void drw_hair_particle_cache_update_compute(ParticleHairCache *cache, const int subdiv)
{
  const int strands_len = cache->strands_len;
  const int final_points_len = cache->final[subdiv].strands_res * strands_len;
  if (final_points_len > 0) {
    using namespace blender::draw;
    GPUShader *shader = DRW_shader_hair_refine_get(PART_REFINE_CATMULL_ROM);

    /* TODO(fclem): Remove Global access. */
    PassSimple &pass = drw_get().data->curves_module->refine;
    pass.shader_set(shader);
    pass.bind_texture("hairPointBuffer", cache->proc_point_buf);
    pass.bind_texture("hairStrandBuffer", cache->proc_strand_buf);
    pass.bind_texture("hairStrandSegBuffer", cache->proc_strand_seg_buf);
    pass.push_constant("hairStrandsRes", &cache->final[subdiv].strands_res);
    pass.bind_ssbo("posTime", cache->final[subdiv].proc_buf);

    const int max_strands_per_call = GPU_max_work_group_count(0);
    int strands_start = 0;
    while (strands_start < strands_len) {
      int batch_strands_len = std::min(strands_len - strands_start, max_strands_per_call);
      pass.push_constant("hairStrandOffset", strands_start);
      pass.dispatch(int3(batch_strands_len, cache->final[subdiv].strands_res, 1));
      strands_start += batch_strands_len;
    }
  }
}

static ParticleHairCache *drw_hair_particle_cache_get(Object *object,
                                                      ParticleSystem *psys,
                                                      ModifierData *md,
                                                      GPUMaterial *gpu_material,
                                                      int subdiv,
                                                      int thickness_res)
{
  using namespace blender::draw;
  ParticleHairCache *cache;
  bool update = particles_ensure_procedural_data(
      object, psys, md, &cache, gpu_material, subdiv, thickness_res);

  if (update) {
    drw_hair_particle_cache_update_compute(cache, subdiv);
  }
  return cache;
}

blender::gpu::VertBuf *DRW_hair_pos_buffer_get(Object *object,
                                               ParticleSystem *psys,
                                               ModifierData *md)
{
  const DRWContext *draw_ctx = DRW_context_get();
  Scene *scene = draw_ctx->scene;

  int subdiv = scene->r.hair_subdiv;
  int thickness_res = (scene->r.hair_type == SCE_HAIR_SHAPE_STRAND) ? 1 : 2;

  ParticleHairCache *cache = drw_hair_particle_cache_get(
      object, psys, md, nullptr, subdiv, thickness_res);

  return cache->final[subdiv].proc_buf;
}

/* New Draw Manager. */
#include "draw_common.hh"

namespace blender::draw {

static ParticleHairCache *hair_particle_cache_get(Object *object,
                                                  ParticleSystem *psys,
                                                  ModifierData *md,
                                                  GPUMaterial *gpu_material,
                                                  int subdiv,
                                                  int thickness_res)
{
  using namespace blender::draw;
  ParticleHairCache *cache;
  bool update = particles_ensure_procedural_data(
      object, psys, md, &cache, gpu_material, subdiv, thickness_res);

  if (!update) {
    return cache;
  }

  CurvesModule &module = *drw_get().data->curves_module;

  const int strands_len = cache->strands_len;
  const int final_points_len = cache->final[subdiv].strands_res * strands_len;
  if (final_points_len > 0) {
    PassSimple::Sub &ob_ps = module.refine.sub("Object Pass");

    ob_ps.shader_set(DRW_shader_hair_refine_get(PART_REFINE_CATMULL_ROM));

    ob_ps.bind_texture("hairPointBuffer", cache->proc_point_buf);
    ob_ps.bind_texture("hairStrandBuffer", cache->proc_strand_buf);
    ob_ps.bind_texture("hairStrandSegBuffer", cache->proc_strand_seg_buf);
    ob_ps.push_constant("hairStrandsRes", &cache->final[subdiv].strands_res);
    ob_ps.bind_ssbo("posTime", cache->final[subdiv].proc_buf);

    const int max_strands_per_call = GPU_max_work_group_count(0);
    int strands_start = 0;
    while (strands_start < strands_len) {
      int batch_strands_len = std::min(strands_len - strands_start, max_strands_per_call);
      PassSimple::Sub &sub_ps = ob_ps.sub("Sub Pass");
      sub_ps.push_constant("hairStrandOffset", strands_start);
      sub_ps.dispatch(int3(batch_strands_len, cache->final[subdiv].strands_res, 1));
      strands_start += batch_strands_len;
    }
  }

  return cache;
}

blender::gpu::VertBuf *hair_pos_buffer_get(Scene *scene,
                                           Object *object,
                                           ParticleSystem *psys,
                                           ModifierData *md)
{
  int subdiv = scene->r.hair_subdiv;
  int thickness_res = (scene->r.hair_type == SCE_HAIR_SHAPE_STRAND) ? 1 : 2;

  ParticleHairCache *cache = hair_particle_cache_get(
      object, psys, md, nullptr, subdiv, thickness_res);

  return cache->final[subdiv].proc_buf;
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

  int subdiv = scene->r.hair_subdiv;
  int thickness_res = (scene->r.hair_type == SCE_HAIR_SHAPE_STRAND) ? 1 : 2;
  ParticleHairCache *hair_cache = drw_hair_particle_cache_get(
      object, psys, md, gpu_material, subdiv, thickness_res);

  /* TODO(fclem): Remove Global access. */
  CurvesModule &module = *drw_get().data->curves_module;

  /* Ensure we have no unbound resources.
   * Required for Vulkan.
   * Fixes issues with certain GL drivers not drawing anything. */
  sub_ps.bind_texture("u", module.dummy_vbo);
  sub_ps.bind_texture("au", module.dummy_vbo);
  sub_ps.bind_texture("a", module.dummy_vbo);
  sub_ps.bind_texture("c", module.dummy_vbo);
  sub_ps.bind_texture("ac", module.dummy_vbo);
  if (gpu_material) {
    ListBase attr_list = GPU_material_attributes(gpu_material);
    ListBaseWrapper<GPUMaterialAttribute> attrs(attr_list);
    for (const GPUMaterialAttribute *attr : attrs) {
      sub_ps.bind_texture(attr->input_name, module.dummy_vbo);
    }
  }

  /* TODO: optimize this. Only bind the ones #GPUMaterial needs. */
  for (int i : IndexRange(hair_cache->num_uv_layers)) {
    for (int n = 0; n < MAX_LAYER_NAME_CT && hair_cache->uv_layer_names[i][n][0] != '\0'; n++) {
      sub_ps.bind_texture(hair_cache->uv_layer_names[i][n], hair_cache->uv_tex[i]);
    }
  }
  for (int i : IndexRange(hair_cache->num_col_layers)) {
    for (int n = 0; n < MAX_LAYER_NAME_CT && hair_cache->col_layer_names[i][n][0] != '\0'; n++) {
      sub_ps.bind_texture(hair_cache->col_layer_names[i][n], hair_cache->col_tex[i]);
    }
  }

  float4x4 dupli_mat = ob_ref.particles_matrix();

  /* Get hair shape parameters. */
  ParticleSettings *part = psys->part;
  float hair_rad_shape = part->shape;
  float hair_rad_root = part->rad_root * part->rad_scale * 0.5f;
  float hair_rad_tip = part->rad_tip * part->rad_scale * 0.5f;
  bool hair_close_tip = (part->shape_flag & PART_SHAPE_CLOSE_TIP) != 0;

  sub_ps.bind_texture("hairPointBuffer", hair_cache->final[subdiv].proc_buf);
  if (hair_cache->proc_length_buf) {
    sub_ps.bind_texture("l", hair_cache->proc_length_buf);
  }

  sub_ps.bind_ubo("drw_curves", module.ubo_pool.dummy_get());
  sub_ps.push_constant("hairStrandsRes", &hair_cache->final[subdiv].strands_res, 1);
  sub_ps.push_constant("hairThicknessRes", thickness_res);
  sub_ps.push_constant("hairRadShape", hair_rad_shape);
  sub_ps.push_constant("hairDupliMatrix", dupli_mat);
  sub_ps.push_constant("hairRadRoot", hair_rad_root);
  sub_ps.push_constant("hairRadTip", hair_rad_tip);
  sub_ps.push_constant("hairCloseTip", hair_close_tip);

  return hair_cache->final[subdiv].proc_hairs[thickness_res - 1];
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
