/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * \brief Contains procedural GPU hair drawing methods.
 */

#include "DRW_render.hh"

#include "BLI_math_matrix.h"

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
#include "draw_hair_private.hh"
#include "draw_manager.hh"
#include "draw_shader.hh"
#include "draw_shader_shared.hh"

static blender::gpu::VertBuf *g_dummy_vbo = nullptr;
static blender::draw::UniformBuffer<CurvesInfos> *g_dummy_curves_info = nullptr;

static void drw_hair_ensure_vbo()
{
  if (g_dummy_vbo != nullptr) {
    return;
  }
  /* initialize vertex format */
  GPUVertFormat format = {0};
  uint dummy_id = GPU_vertformat_attr_add(&format, "dummy", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);

  g_dummy_vbo = GPU_vertbuf_create_with_format_ex(
      format, GPU_USAGE_STATIC | GPU_USAGE_FLAG_BUFFER_TEXTURE_ONLY);

  const float vert[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  GPU_vertbuf_data_alloc(*g_dummy_vbo, 1);
  GPU_vertbuf_attr_fill(g_dummy_vbo, dummy_id, vert);
  /* Create VBO immediately to bind to texture buffer. */
  GPU_vertbuf_use(g_dummy_vbo);

  g_dummy_curves_info = MEM_new<blender::draw::UniformBuffer<CurvesInfos>>("g_dummy_curves_info");
  memset(
      g_dummy_curves_info->is_point_attribute, 0, sizeof(g_dummy_curves_info->is_point_attribute));
  g_dummy_curves_info->push_update();
}

void DRW_hair_init()
{
  drw_hair_ensure_vbo();
}

static void drw_hair_particle_cache_update_compute(ParticleHairCache *cache, const int subdiv)
{
  const int strands_len = cache->strands_len;
  const int final_points_len = cache->final[subdiv].strands_res * strands_len;
  if (final_points_len > 0) {
    using namespace blender::draw;
    GPUShader *shader = DRW_shader_hair_refine_get(PART_REFINE_CATMULL_ROM);

    /* TODO(fclem): Remove Global access. */
    PassSimple &pass = *DST.vmempool->curves_refine;
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
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Scene *scene = draw_ctx->scene;

  int subdiv = scene->r.hair_subdiv;
  int thickness_res = (scene->r.hair_type == SCE_HAIR_SHAPE_STRAND) ? 1 : 2;

  ParticleHairCache *cache = drw_hair_particle_cache_get(
      object, psys, md, nullptr, subdiv, thickness_res);

  return cache->final[subdiv].proc_buf;
}

void DRW_hair_duplimat_get(Object *object,
                           ParticleSystem * /*psys*/,
                           ModifierData * /*md*/,
                           float (*dupli_mat)[4])
{
  Object *dupli_parent = DRW_object_get_dupli_parent(object);
  DupliObject *dupli_object = DRW_object_get_dupli(object);

  if ((dupli_parent != nullptr) && (dupli_object != nullptr)) {
    if (dupli_object->type & OB_DUPLICOLLECTION) {
      unit_m4(dupli_mat);
      Collection *collection = dupli_parent->instance_collection;
      if (collection != nullptr) {
        sub_v3_v3(dupli_mat[3], collection->instance_offset);
      }
      mul_m4_m4m4(dupli_mat, dupli_parent->object_to_world().ptr(), dupli_mat);
    }
    else {
      copy_m4_m4(dupli_mat, dupli_object->ob->object_to_world().ptr());
      invert_m4(dupli_mat);
      mul_m4_m4m4(dupli_mat, object->object_to_world().ptr(), dupli_mat);
    }
  }
  else {
    unit_m4(dupli_mat);
  }
}

void DRW_hair_free()
{
  GPU_VERTBUF_DISCARD_SAFE(g_dummy_vbo);
  MEM_delete(g_dummy_curves_info);
}

/* New Draw Manager. */
#include "draw_common.hh"

namespace blender::draw {

static PassSimple *g_pass = nullptr;

void hair_init()
{
  if (!g_pass) {
    g_pass = MEM_new<PassSimple>("drw_hair g_pass", "Update Hair Pass");
  }
  g_pass->init();
  g_pass->state_set(DRW_STATE_NO_DRAW);
}

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

  const int strands_len = cache->strands_len;
  const int final_points_len = cache->final[subdiv].strands_res * strands_len;
  if (final_points_len > 0) {
    PassSimple::Sub &ob_ps = g_pass->sub("Object Pass");

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

void hair_update(Manager &manager)
{
  manager.submit(*g_pass);
  GPU_memory_barrier(GPU_BARRIER_SHADER_STORAGE);
}

void hair_free()
{
  MEM_delete(g_pass);
  g_pass = nullptr;
}

template<typename PassT>
blender::gpu::Batch *hair_sub_pass_setup_implementation(PassT &sub_ps,
                                                        const Scene *scene,
                                                        Object *object,
                                                        ParticleSystem *psys,
                                                        ModifierData *md,
                                                        GPUMaterial *gpu_material)
{
  /** NOTE: This still relies on the old DRW_hair implementation. */

  int subdiv = scene->r.hair_subdiv;
  int thickness_res = (scene->r.hair_type == SCE_HAIR_SHAPE_STRAND) ? 1 : 2;
  ParticleHairCache *hair_cache = drw_hair_particle_cache_get(
      object, psys, md, gpu_material, subdiv, thickness_res);

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

  /* Fix issue with certain driver not drawing anything if there is nothing bound to
   * "ac", "au", "u" or "c". */
  if (hair_cache->num_uv_layers == 0) {
    sub_ps.bind_texture("u", g_dummy_vbo);
    sub_ps.bind_texture("au", g_dummy_vbo);
    sub_ps.bind_texture("a", g_dummy_vbo);
  }
  if (hair_cache->num_col_layers == 0) {
    sub_ps.bind_texture("c", g_dummy_vbo);
    sub_ps.bind_texture("ac", g_dummy_vbo);
  }

  float4x4 dupli_mat;
  DRW_hair_duplimat_get(object, psys, md, dupli_mat.ptr());

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

  sub_ps.bind_ubo("drw_curves", *g_dummy_curves_info);
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
                                         Object *object,
                                         ParticleSystem *psys,
                                         ModifierData *md,
                                         GPUMaterial *gpu_material)
{
  return hair_sub_pass_setup_implementation(sub_ps, scene, object, psys, md, gpu_material);
}

blender::gpu::Batch *hair_sub_pass_setup(PassSimple::Sub &sub_ps,
                                         const Scene *scene,
                                         Object *object,
                                         ParticleSystem *psys,
                                         ModifierData *md,
                                         GPUMaterial *gpu_material)
{
  return hair_sub_pass_setup_implementation(sub_ps, scene, object, psys, md, gpu_material);
}

}  // namespace blender::draw
