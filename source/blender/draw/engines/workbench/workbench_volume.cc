/* SPDX-FileCopyrightText: 2018 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#include "workbench_private.h"

#include "DNA_fluid_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_force_types.h"
#include "DNA_volume_types.h"

#include "BLI_dynstr.h"
#include "BLI_listbase.h"
#include "BLI_rand.h"
#include "BLI_string_utils.h"

#include "BKE_fluid.h"
#include "BKE_global.h"
#include "BKE_object.h"
#include "BKE_volume.h"
#include "BKE_volume_render.h"

void workbench_volume_engine_init(WORKBENCH_Data *vedata)
{
  WORKBENCH_TextureList *txl = vedata->txl;

  if (txl->dummy_volume_tx == nullptr) {
    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ;

    const float zero[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    const float one[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    txl->dummy_volume_tx = GPU_texture_create_3d(
        "dummy_volume", 1, 1, 1, 1, GPU_RGBA8, usage, zero);
    txl->dummy_shadow_tx = GPU_texture_create_3d(
        "dummy_shadow", 1, 1, 1, 1, GPU_RGBA8, usage, one);
    txl->dummy_coba_tx = GPU_texture_create_1d("dummy_coba", 1, 1, GPU_RGBA8, usage, zero);
  }
}

void workbench_volume_cache_init(WORKBENCH_Data *vedata)
{
  vedata->psl->volume_ps = DRW_pass_create(
      "Volumes", DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA_PREMUL | DRW_STATE_CULL_FRONT);

  vedata->stl->wpd->volumes_do = false;
}

static void workbench_volume_modifier_cache_populate(WORKBENCH_Data *vedata,
                                                     Object *ob,
                                                     ModifierData *md)
{
  FluidModifierData *fmd = (FluidModifierData *)md;
  FluidDomainSettings *fds = fmd->domain;
  WORKBENCH_PrivateData *wpd = vedata->stl->wpd;
  WORKBENCH_TextureList *txl = vedata->txl;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  DRWShadingGroup *grp = nullptr;

  if (!fds->fluid) {
    return;
  }

  wpd->volumes_do = true;
  if (fds->use_coba) {
    DRW_smoke_ensure_coba_field(fmd);
  }
  else if (fds->type == FLUID_DOMAIN_TYPE_GAS) {
    DRW_smoke_ensure(fmd, fds->flags & FLUID_DOMAIN_USE_NOISE);
  }
  else {
    return;
  }

  if ((!fds->use_coba && (fds->tex_density == nullptr && fds->tex_color == nullptr)) ||
      (fds->use_coba && fds->tex_field == nullptr))
  {
    return;
  }

  const bool use_slice = (fds->axis_slice_method == AXIS_SLICE_SINGLE);
  const bool show_phi = ELEM(fds->coba_field,
                             FLUID_DOMAIN_FIELD_PHI,
                             FLUID_DOMAIN_FIELD_PHI_IN,
                             FLUID_DOMAIN_FIELD_PHI_OUT,
                             FLUID_DOMAIN_FIELD_PHI_OBSTACLE);
  const bool show_flags = (fds->coba_field == FLUID_DOMAIN_FIELD_FLAGS);
  const bool show_pressure = (fds->coba_field == FLUID_DOMAIN_FIELD_PRESSURE);
  eWORKBENCH_VolumeInterpType interp_type = WORKBENCH_VOLUME_INTERP_LINEAR;

  switch ((FLUID_DisplayInterpolationMethod)fds->interp_method) {
    case FLUID_DISPLAY_INTERP_LINEAR:
      interp_type = WORKBENCH_VOLUME_INTERP_LINEAR;
      break;
    case FLUID_DISPLAY_INTERP_CUBIC:
      interp_type = WORKBENCH_VOLUME_INTERP_CUBIC;
      break;
    case FLUID_DISPLAY_INTERP_CLOSEST:
      interp_type = WORKBENCH_VOLUME_INTERP_CLOSEST;
      break;
  }

  GPUShader *sh = workbench_shader_volume_get(use_slice, fds->use_coba, interp_type, true);

  if (use_slice) {
    float invviewmat[4][4];
    DRW_view_viewmat_get(nullptr, invviewmat, true);

    const int axis = (fds->slice_axis == SLICE_AXIS_AUTO) ?
                         axis_dominant_v3_single(invviewmat[2]) :
                         fds->slice_axis - 1;
    float dim[3];
    BKE_object_dimensions_get(ob, dim);
    /* 0.05f to achieve somewhat the same opacity as the full view. */
    float step_length = max_ff(1e-16f, dim[axis] * 0.05f);

    grp = DRW_shgroup_create(sh, vedata->psl->volume_ps);
    DRW_shgroup_uniform_block(grp, "world_data", wpd->world_ubo);
    DRW_shgroup_uniform_float_copy(grp, "slicePosition", fds->slice_depth);
    DRW_shgroup_uniform_int_copy(grp, "sliceAxis", axis);
    DRW_shgroup_uniform_float_copy(grp, "stepLength", step_length);
    DRW_shgroup_state_disable(grp, DRW_STATE_CULL_FRONT);
  }
  else {
    double noise_ofs;
    BLI_halton_1d(3, 0.0, wpd->taa_sample, &noise_ofs);
    float dim[3], step_length, max_slice;
    float slice_count[3] = {float(fds->res[0]), float(fds->res[1]), float(fds->res[2])};
    mul_v3_fl(slice_count, max_ff(0.001f, fds->slice_per_voxel));
    max_slice = max_fff(slice_count[0], slice_count[1], slice_count[2]);
    BKE_object_dimensions_get(ob, dim);
    invert_v3(slice_count);
    mul_v3_v3(dim, slice_count);
    step_length = len_v3(dim);

    grp = DRW_shgroup_create(sh, vedata->psl->volume_ps);
    DRW_shgroup_uniform_block(grp, "world_data", wpd->world_ubo);
    DRW_shgroup_uniform_int_copy(grp, "samplesLen", max_slice);
    DRW_shgroup_uniform_float_copy(grp, "stepLength", step_length);
    DRW_shgroup_uniform_float_copy(grp, "noiseOfs", noise_ofs);
    DRW_shgroup_state_enable(grp, DRW_STATE_CULL_FRONT);
  }

  if (fds->use_coba) {
    if (show_flags) {
      DRW_shgroup_uniform_texture(grp, "flagTexture", fds->tex_field);
    }
    else {
      DRW_shgroup_uniform_texture(grp, "densityTexture", fds->tex_field);
    }
    if (!show_phi && !show_flags && !show_pressure) {
      DRW_shgroup_uniform_texture(grp, "transferTexture", fds->tex_coba);
    }
    DRW_shgroup_uniform_float_copy(grp, "gridScale", fds->grid_scale);
    DRW_shgroup_uniform_bool_copy(grp, "showPhi", show_phi);
    DRW_shgroup_uniform_bool_copy(grp, "showFlags", show_flags);
    DRW_shgroup_uniform_bool_copy(grp, "showPressure", show_pressure);
  }
  else {
    static float white[3] = {1.0f, 1.0f, 1.0f};
    bool use_constant_color = ((fds->active_fields & FLUID_DOMAIN_ACTIVE_COLORS) == 0 &&
                               (fds->active_fields & FLUID_DOMAIN_ACTIVE_COLOR_SET) != 0);
    DRW_shgroup_uniform_texture(
        grp, "densityTexture", (fds->tex_color) ? fds->tex_color : fds->tex_density);
    DRW_shgroup_uniform_texture(grp, "shadowTexture", fds->tex_shadow);
    DRW_shgroup_uniform_texture(
        grp, "flameTexture", (fds->tex_flame) ? fds->tex_flame : txl->dummy_volume_tx);
    DRW_shgroup_uniform_texture(
        grp, "flameColorTexture", (fds->tex_flame) ? fds->tex_flame_coba : txl->dummy_coba_tx);
    DRW_shgroup_uniform_vec3(
        grp, "activeColor", (use_constant_color) ? fds->active_color : white, 1);
  }
  DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", &dtxl->depth);
  DRW_shgroup_uniform_float_copy(grp, "densityScale", 10.0f * fds->display_thickness);

  if (use_slice) {
    DRW_shgroup_call(grp, DRW_cache_quad_get(), ob);
  }
  else {
    DRW_shgroup_call(grp, DRW_cache_cube_get(), ob);
  }
}

static void workbench_volume_material_color(WORKBENCH_PrivateData *wpd,
                                            Object *ob,
                                            eV3DShadingColorType color_type,
                                            float color[3])
{
  Material *ma = BKE_object_material_get_eval(ob, VOLUME_MATERIAL_NR);
  WORKBENCH_UBO_Material ubo_data;
  workbench_material_ubo_data(wpd, ob, ma, &ubo_data, color_type);
  copy_v3_v3(color, ubo_data.base_color);
}

static void workbench_volume_object_cache_populate(WORKBENCH_Data *vedata,
                                                   Object *ob,
                                                   eV3DShadingColorType color_type)
{
  /* Create 3D textures. */
  Volume *volume = static_cast<Volume *>(ob->data);
  BKE_volume_load(volume, G.main);
  const VolumeGrid *volume_grid = BKE_volume_grid_active_get_for_read(volume);
  if (volume_grid == nullptr) {
    return;
  }
  DRWVolumeGrid *grid = DRW_volume_batch_cache_get_grid(volume, volume_grid);
  if (grid == nullptr) {
    return;
  }

  WORKBENCH_PrivateData *wpd = vedata->stl->wpd;
  WORKBENCH_TextureList *txl = vedata->txl;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  DRWShadingGroup *grp = nullptr;

  wpd->volumes_do = true;
  const bool use_slice = (volume->display.axis_slice_method == AXIS_SLICE_SINGLE);
  eWORKBENCH_VolumeInterpType interp_type = WORKBENCH_VOLUME_INTERP_LINEAR;

  switch ((VolumeDisplayInterpMethod)volume->display.interpolation_method) {
    case VOLUME_DISPLAY_INTERP_LINEAR:
      interp_type = WORKBENCH_VOLUME_INTERP_LINEAR;
      break;
    case VOLUME_DISPLAY_INTERP_CUBIC:
      interp_type = WORKBENCH_VOLUME_INTERP_CUBIC;
      break;
    case VOLUME_DISPLAY_INTERP_CLOSEST:
      interp_type = WORKBENCH_VOLUME_INTERP_CLOSEST;
      break;
  }

  /* Create shader. */
  GPUShader *sh = workbench_shader_volume_get(use_slice, false, interp_type, false);

  /* Compute color. */
  float color[3];
  workbench_volume_material_color(wpd, ob, color_type, color);

  /* Combined texture to object, and object to world transform. */
  float texture_to_world[4][4];
  mul_m4_m4m4(texture_to_world, ob->object_to_world, grid->texture_to_object);

  if (use_slice) {
    float invviewmat[4][4];
    DRW_view_viewmat_get(nullptr, invviewmat, true);

    const int axis = (volume->display.slice_axis == SLICE_AXIS_AUTO) ?
                         axis_dominant_v3_single(invviewmat[2]) :
                         volume->display.slice_axis - 1;

    float dim[3];
    BKE_object_dimensions_get(ob, dim);
    /* 0.05f to achieve somewhat the same opacity as the full view. */
    float step_length = max_ff(1e-16f, dim[axis] * 0.05f);

    const float slice_position = volume->display.slice_depth;

    grp = DRW_shgroup_create(sh, vedata->psl->volume_ps);
    DRW_shgroup_uniform_block(grp, "world_data", wpd->world_ubo);
    DRW_shgroup_uniform_float_copy(grp, "slicePosition", slice_position);
    DRW_shgroup_uniform_int_copy(grp, "sliceAxis", axis);
    DRW_shgroup_uniform_float_copy(grp, "stepLength", step_length);
    DRW_shgroup_state_disable(grp, DRW_STATE_CULL_FRONT);
  }
  else {
    /* Compute world space dimensions for step size. */
    float world_size[3];
    mat4_to_size(world_size, texture_to_world);
    abs_v3(world_size);

    /* Compute step parameters. */
    double noise_ofs;
    BLI_halton_1d(3, 0.0, wpd->taa_sample, &noise_ofs);
    float step_length, max_slice;
    int resolution[3];
    GPU_texture_get_mipmap_size(grid->texture, 0, resolution);
    float slice_count[3] = {float(resolution[0]), float(resolution[1]), float(resolution[2])};
    mul_v3_fl(slice_count, max_ff(0.001f, 5.0f));
    max_slice = max_fff(slice_count[0], slice_count[1], slice_count[2]);
    invert_v3(slice_count);
    mul_v3_v3(slice_count, world_size);
    step_length = len_v3(slice_count);

    /* Set uniforms. */
    grp = DRW_shgroup_create(sh, vedata->psl->volume_ps);
    DRW_shgroup_uniform_block(grp, "world_data", wpd->world_ubo);
    DRW_shgroup_uniform_int_copy(grp, "samplesLen", max_slice);
    DRW_shgroup_uniform_float_copy(grp, "stepLength", step_length);
    DRW_shgroup_uniform_float_copy(grp, "noiseOfs", noise_ofs);
    DRW_shgroup_state_enable(grp, DRW_STATE_CULL_FRONT);
  }

  /* Compute density scale. */
  const float density_scale = volume->display.density *
                              BKE_volume_density_scale(volume, ob->object_to_world);

  DRW_shgroup_uniform_texture(grp, "densityTexture", grid->texture);
  /* TODO: implement shadow texture, see manta_smoke_calc_transparency. */
  DRW_shgroup_uniform_texture(grp, "shadowTexture", txl->dummy_shadow_tx);
  DRW_shgroup_uniform_vec3_copy(grp, "activeColor", color);

  DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", &dtxl->depth);
  DRW_shgroup_uniform_float_copy(grp, "densityScale", density_scale);

  DRW_shgroup_uniform_mat4(grp, "volumeObjectToTexture", grid->object_to_texture);
  DRW_shgroup_uniform_mat4(grp, "volumeTextureToObject", grid->texture_to_object);

  DRW_shgroup_call(grp, DRW_cache_cube_get(), ob);
}

void workbench_volume_cache_populate(WORKBENCH_Data *vedata,
                                     Scene * /*scene*/,
                                     Object *ob,
                                     ModifierData *md,
                                     eV3DShadingColorType color_type)
{
  if (md == nullptr) {
    workbench_volume_object_cache_populate(vedata, ob, color_type);
  }
  else {
    workbench_volume_modifier_cache_populate(vedata, ob, md);
  }
}

void workbench_volume_draw_pass(WORKBENCH_Data *vedata)
{
  WORKBENCH_PassList *psl = vedata->psl;
  WORKBENCH_PrivateData *wpd = vedata->stl->wpd;
  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();

  if (wpd->volumes_do) {
    GPU_framebuffer_bind(dfbl->color_only_fb);
    DRW_draw_pass(psl->volume_ps);
  }
}
