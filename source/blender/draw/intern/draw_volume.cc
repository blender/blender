/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * \brief Contains Volume object GPU attributes configuration.
 */

#include "DRW_gpu_wrapper.hh"
#include "DRW_render.hh"

#include "DNA_fluid_types.h"
#include "DNA_volume_types.h"

#include "BLI_string.h"

#include "BKE_global.hh"
#include "BKE_mesh.hh"
#include "BKE_modifier.hh"
#include "BKE_volume.hh"
#include "BKE_volume_render.hh"

#include "GPU_material.hh"

#include "draw_cache.hh"
#include "draw_common_c.hh"
#include "draw_manager_c.hh"

#include "draw_common.hh"

using namespace blender;
using namespace blender::draw;
using VolumeInfosBuf = blender::draw::UniformBuffer<VolumeInfos>;

static struct {
  GPUTexture *dummy_zero;
  GPUTexture *dummy_one;
} g_data = {};

struct VolumeUniformBufPool {
  Vector<VolumeInfosBuf *> ubos;
  uint used = 0;

  ~VolumeUniformBufPool()
  {
    for (VolumeInfosBuf *ubo : ubos) {
      delete ubo;
    }
  }

  void reset()
  {
    used = 0;
  }

  VolumeInfosBuf *alloc()
  {
    if (used >= ubos.size()) {
      VolumeInfosBuf *buf = new VolumeInfosBuf();
      ubos.append(buf);
    }
    return ubos[used++];
  }
};

void DRW_volume_ubos_pool_free(void *pool)
{
  delete reinterpret_cast<VolumeUniformBufPool *>(pool);
}

static void drw_volume_globals_init()
{
  const float zero[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  const float one[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  g_data.dummy_zero = GPU_texture_create_3d(
      "dummy_zero", 1, 1, 1, 1, GPU_RGBA8, GPU_TEXTURE_USAGE_SHADER_READ, zero);
  g_data.dummy_one = GPU_texture_create_3d(
      "dummy_one", 1, 1, 1, 1, GPU_RGBA8, GPU_TEXTURE_USAGE_SHADER_READ, one);
  GPU_texture_extend_mode(g_data.dummy_zero, GPU_SAMPLER_EXTEND_MODE_REPEAT);
  GPU_texture_extend_mode(g_data.dummy_one, GPU_SAMPLER_EXTEND_MODE_REPEAT);
}

void DRW_volume_free()
{
  GPU_TEXTURE_FREE_SAFE(g_data.dummy_zero);
  GPU_TEXTURE_FREE_SAFE(g_data.dummy_one);
}

static GPUTexture *grid_default_texture(eGPUDefaultValue default_value)
{
  if (g_data.dummy_one == nullptr) {
    drw_volume_globals_init();
  }

  switch (default_value) {
    case GPU_DEFAULT_0:
      return g_data.dummy_zero;
    case GPU_DEFAULT_1:
      return g_data.dummy_one;
  }
  return g_data.dummy_zero;
}

void DRW_volume_init(DRWData *drw_data)
{
  if (drw_data->volume_grids_ubos == nullptr) {
    drw_data->volume_grids_ubos = new VolumeUniformBufPool();
  }
  VolumeUniformBufPool *pool = (VolumeUniformBufPool *)drw_data->volume_grids_ubos;
  pool->reset();

  if (g_data.dummy_one == nullptr) {
    drw_volume_globals_init();
  }
}

/* -------------------------------------------------------------------- */
/** \name New Draw Manager implementation
 * \{ */

namespace blender::draw {

template<typename PassType>
PassType *volume_world_grids_init(PassType &ps, ListBaseWrapper<GPUMaterialAttribute> &attrs)
{
  PassType *sub = &ps.sub("World Volume");
  for (const GPUMaterialAttribute *attr : attrs) {
    sub->bind_texture(attr->input_name, grid_default_texture(attr->default_value));
  }

  return sub;
}

template<typename PassType>
PassType *volume_object_grids_init(PassType &ps,
                                   Object *ob,
                                   ListBaseWrapper<GPUMaterialAttribute> &attrs)
{
  Volume *volume = (Volume *)ob->data;
  BKE_volume_load(volume, G.main);

  /* Render nothing if there is no attribute. */
  if (BKE_volume_num_grids(volume) == 0) {
    return nullptr;
  }

  VolumeUniformBufPool *pool = (VolumeUniformBufPool *)DST.vmempool->volume_grids_ubos;
  VolumeInfosBuf &volume_infos = *pool->alloc();

  volume_infos.density_scale = BKE_volume_density_scale(volume, ob->object_to_world().ptr());
  volume_infos.color_mul = float4(1.0f);
  volume_infos.temperature_mul = 1.0f;
  volume_infos.temperature_bias = 0.0f;

  PassType *sub = &ps.sub("Volume Object SubPass");

  /* Bind volume grid textures. */
  int grid_id = 0;
  for (const GPUMaterialAttribute *attr : attrs) {
    const blender::bke::VolumeGridData *volume_grid = BKE_volume_grid_find(volume, attr->name);
    const DRWVolumeGrid *drw_grid = (volume_grid) ?
                                        DRW_volume_batch_cache_get_grid(volume, volume_grid) :
                                        nullptr;
    /* Handle 3 cases here:
     * - Grid exists and texture was loaded -> use texture.
     * - Grid exists but has zero size or failed to load -> use zero.
     * - Grid does not exist -> use default value. */
    const GPUTexture *grid_tex = (drw_grid)    ? drw_grid->texture :
                                 (volume_grid) ? g_data.dummy_zero :
                                                 grid_default_texture(attr->default_value);
    /* TODO(@pragma37): bind_texture const support ? */
    sub->bind_texture(attr->input_name, (GPUTexture *)grid_tex);

    volume_infos.grids_xform[grid_id++] = drw_grid ? float4x4(drw_grid->object_to_texture) :
                                                     float4x4::identity();
  }

  volume_infos.push_update();

  sub->bind_ubo("drw_volume", volume_infos);

  return sub;
}

template<typename PassType>
PassType *drw_volume_object_mesh_init(PassType &ps,
                                      Scene *scene,
                                      Object *ob,
                                      ListBaseWrapper<GPUMaterialAttribute> &attrs)
{
  VolumeUniformBufPool *pool = (VolumeUniformBufPool *)DST.vmempool->volume_grids_ubos;
  VolumeInfosBuf &volume_infos = *pool->alloc();

  ModifierData *md = nullptr;

  volume_infos.density_scale = 1.0f;
  volume_infos.color_mul = float4(1.0f);
  volume_infos.temperature_mul = 1.0f;
  volume_infos.temperature_bias = 0.0f;

  bool has_fluid_modifier = (md = BKE_modifiers_findby_type(ob, eModifierType_Fluid)) &&
                            BKE_modifier_is_enabled(scene, md, eModifierMode_Realtime) &&
                            ((FluidModifierData *)md)->domain != nullptr;
  FluidModifierData *fmd = has_fluid_modifier ? (FluidModifierData *)md : nullptr;
  FluidDomainSettings *fds = has_fluid_modifier ? fmd->domain : nullptr;

  PassType *sub = nullptr;

  if (!has_fluid_modifier || (fds->type != FLUID_DOMAIN_TYPE_GAS)) {
    /* No volume attributes or fluid domain. */
    sub = &ps.sub("Volume Mesh SubPass");
    int grid_id = 0;
    for (const GPUMaterialAttribute *attr : attrs) {
      sub->bind_texture(attr->input_name, grid_default_texture(attr->default_value));
      volume_infos.grids_xform[grid_id++] = float4x4::identity();
    }
  }
  else if (fds->fluid) {
    /* Smoke Simulation. */
    DRW_smoke_ensure(fmd, fds->flags & FLUID_DOMAIN_USE_NOISE);

    sub = &ps.sub("Volume Modifier SubPass");

    float3 location, scale;
    BKE_mesh_texspace_get(static_cast<Mesh *>(ob->data), location, scale);
    float3 orco_mul = math::safe_rcp(scale * 2.0);
    float3 orco_add = (location - scale) * -orco_mul;
    /* Replace OrcoTexCoFactors with a matrix multiplication. */
    float4x4 orco_mat = math::from_scale<float4x4>(orco_mul);
    orco_mat.location() = orco_add;

    int grid_id = 0;
    for (const GPUMaterialAttribute *attr : attrs) {
      if (STREQ(attr->name, "density")) {
        sub->bind_texture(attr->input_name,
                          fds->tex_density ? &fds->tex_density : &g_data.dummy_one);
      }
      else if (STREQ(attr->name, "color")) {
        sub->bind_texture(attr->input_name, fds->tex_color ? &fds->tex_color : &g_data.dummy_one);
      }
      else if (STR_ELEM(attr->name, "flame", "temperature")) {
        sub->bind_texture(attr->input_name, fds->tex_flame ? &fds->tex_flame : &g_data.dummy_zero);
      }
      else {
        sub->bind_texture(attr->input_name, grid_default_texture(attr->default_value));
      }
      volume_infos.grids_xform[grid_id++] = orco_mat;
    }

    bool use_constant_color = ((fds->active_fields & FLUID_DOMAIN_ACTIVE_COLORS) == 0 &&
                               (fds->active_fields & FLUID_DOMAIN_ACTIVE_COLOR_SET) != 0);
    if (use_constant_color) {
      volume_infos.color_mul = float4(UNPACK3(fds->active_color), 1.0f);
    }

    /* Output is such that 0..1 maps to 0..1000K. */
    volume_infos.temperature_mul = fds->flame_max_temp - fds->flame_ignition;
    volume_infos.temperature_bias = fds->flame_ignition;
  }

  if (sub) {
    volume_infos.push_update();
    sub->bind_ubo("drw_volume", volume_infos);
  }

  return sub;
}

template<typename PassType>
PassType *volume_sub_pass_implementation(PassType &ps,
                                         Scene *scene,
                                         Object *ob,
                                         GPUMaterial *gpu_material)
{
  ListBase attr_list = GPU_material_attributes(gpu_material);
  ListBaseWrapper<GPUMaterialAttribute> attrs(attr_list);
  if (ob == nullptr) {
    return volume_world_grids_init(ps, attrs);
  }
  if (ob->type == OB_VOLUME) {
    return volume_object_grids_init(ps, ob, attrs);
  }
  return drw_volume_object_mesh_init(ps, scene, ob, attrs);
}

PassMain::Sub *volume_sub_pass(PassMain::Sub &ps,
                               Scene *scene,
                               Object *ob,
                               GPUMaterial *gpu_material)
{
  return volume_sub_pass_implementation(ps, scene, ob, gpu_material);
}

PassSimple::Sub *volume_sub_pass(PassSimple::Sub &ps,
                                 Scene *scene,
                                 Object *ob,
                                 GPUMaterial *gpu_material)
{
  return volume_sub_pass_implementation(ps, scene, ob, gpu_material);
}

}  // namespace blender::draw

/** \} */
