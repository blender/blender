/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2018, Blender Foundation.
 */

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

  if (txl->dummy_volume_tx == NULL) {
    const float zero[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    const float one[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    txl->dummy_volume_tx = GPU_texture_create_3d(1, 1, 1, GPU_RGBA8, zero, NULL);
    txl->dummy_shadow_tx = GPU_texture_create_3d(1, 1, 1, GPU_RGBA8, one, NULL);
    txl->dummy_coba_tx = GPU_texture_create_1d(1, GPU_RGBA8, zero, NULL);
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
  DRWShadingGroup *grp = NULL;

  /* Don't try to show liquid domains here */
  if (!fds->fluid || !(fds->type == FLUID_DOMAIN_TYPE_GAS)) {
    return;
  }

  wpd->volumes_do = true;
  if (fds->use_coba) {
    DRW_smoke_ensure_coba_field(fmd);
  }
  else {
    DRW_smoke_ensure(fmd, fds->flags & FLUID_DOMAIN_USE_NOISE);
  }

  if ((!fds->use_coba && (fds->tex_density == NULL && fds->tex_color == NULL)) ||
      (fds->use_coba && fds->tex_field == NULL)) {
    return;
  }

  const bool use_slice = (fds->slice_method == FLUID_DOMAIN_SLICE_AXIS_ALIGNED &&
                          fds->axis_slice_method == AXIS_SLICE_SINGLE);
  const bool cubic_interp = (fds->interp_method == VOLUME_INTERP_CUBIC);
  GPUShader *sh = workbench_shader_volume_get(use_slice, fds->use_coba, cubic_interp, true);

  if (use_slice) {
    float invviewmat[4][4];
    DRW_view_viewmat_get(NULL, invviewmat, true);

    const int axis = (fds->slice_axis == SLICE_AXIS_AUTO) ?
                         axis_dominant_v3_single(invviewmat[2]) :
                         fds->slice_axis - 1;
    float dim[3];
    BKE_object_dimensions_get(ob, dim);
    /* 0.05f to achieve somewhat the same opacity as the full view.  */
    float step_length = max_ff(1e-16f, dim[axis] * 0.05f);

    grp = DRW_shgroup_create(sh, vedata->psl->volume_ps);
    DRW_shgroup_uniform_float_copy(grp, "slicePosition", fds->slice_depth);
    DRW_shgroup_uniform_int_copy(grp, "sliceAxis", axis);
    DRW_shgroup_uniform_float_copy(grp, "stepLength", step_length);
    DRW_shgroup_state_disable(grp, DRW_STATE_CULL_FRONT);
  }
  else {
    double noise_ofs;
    BLI_halton_1d(3, 0.0, wpd->taa_sample, &noise_ofs);
    float dim[3], step_length, max_slice;
    float slice_ct[3] = {fds->res[0], fds->res[1], fds->res[2]};
    mul_v3_fl(slice_ct, max_ff(0.001f, fds->slice_per_voxel));
    max_slice = max_fff(slice_ct[0], slice_ct[1], slice_ct[2]);
    BKE_object_dimensions_get(ob, dim);
    invert_v3(slice_ct);
    mul_v3_v3(dim, slice_ct);
    step_length = len_v3(dim);

    grp = DRW_shgroup_create(sh, vedata->psl->volume_ps);
    DRW_shgroup_uniform_block(grp, "world_block", wpd->world_ubo);
    DRW_shgroup_uniform_int_copy(grp, "samplesLen", max_slice);
    DRW_shgroup_uniform_float_copy(grp, "stepLength", step_length);
    DRW_shgroup_uniform_float_copy(grp, "noiseOfs", noise_ofs);
    DRW_shgroup_state_enable(grp, DRW_STATE_CULL_FRONT);
  }

  if (fds->use_coba) {
    DRW_shgroup_uniform_texture(grp, "densityTexture", fds->tex_field);
    DRW_shgroup_uniform_texture(grp, "transferTexture", fds->tex_coba);
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

  BLI_addtail(&wpd->smoke_domains, BLI_genericNodeN(fmd));
}

static void workbench_volume_material_color(WORKBENCH_PrivateData *wpd,
                                            Object *ob,
                                            eV3DShadingColorType color_type,
                                            float color[3])
{
  Material *ma = BKE_object_material_get(ob, VOLUME_MATERIAL_NR);
  WORKBENCH_UBO_Material ubo_data;
  workbench_material_ubo_data(wpd, ob, ma, &ubo_data, color_type);
  copy_v3_v3(color, ubo_data.base_color);
}

static void workbench_volume_object_cache_populate(WORKBENCH_Data *vedata,
                                                   Object *ob,
                                                   eV3DShadingColorType color_type)
{
  /* Create 3D textures. */
  Volume *volume = ob->data;
  BKE_volume_load(volume, G.main);
  VolumeGrid *volume_grid = BKE_volume_grid_active_get(volume);
  if (volume_grid == NULL) {
    return;
  }
  DRWVolumeGrid *grid = DRW_volume_batch_cache_get_grid(volume, volume_grid);
  if (grid == NULL) {
    return;
  }

  WORKBENCH_PrivateData *wpd = vedata->stl->wpd;
  WORKBENCH_TextureList *txl = vedata->txl;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

  wpd->volumes_do = true;

  /* Create shader. */
  GPUShader *sh = workbench_shader_volume_get(false, false, false, false);

  /* Compute color. */
  float color[3];
  workbench_volume_material_color(wpd, ob, color_type, color);

  /* Combined texture to object, and object to world transform. */
  float texture_to_world[4][4];
  mul_m4_m4m4(texture_to_world, ob->obmat, grid->texture_to_object);

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
  float slice_ct[3] = {resolution[0], resolution[1], resolution[2]};
  mul_v3_fl(slice_ct, max_ff(0.001f, 5.0f));
  max_slice = max_fff(slice_ct[0], slice_ct[1], slice_ct[2]);
  invert_v3(slice_ct);
  mul_v3_v3(slice_ct, world_size);
  step_length = len_v3(slice_ct);

  /* Compute density scale. */
  const float density_scale = volume->display.density *
                              BKE_volume_density_scale(volume, ob->obmat);

  /* Set uniforms. */
  DRWShadingGroup *grp = DRW_shgroup_create(sh, vedata->psl->volume_ps);
  DRW_shgroup_uniform_block(grp, "world_block", wpd->world_ubo);
  DRW_shgroup_uniform_int_copy(grp, "samplesLen", max_slice);
  DRW_shgroup_uniform_float_copy(grp, "stepLength", step_length);
  DRW_shgroup_uniform_float_copy(grp, "noiseOfs", noise_ofs);
  DRW_shgroup_state_enable(grp, DRW_STATE_CULL_FRONT);

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
                                     Scene *UNUSED(scene),
                                     Object *ob,
                                     ModifierData *md,
                                     eV3DShadingColorType color_type)
{
  if (md == NULL) {
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

void workbench_volume_draw_finish(WORKBENCH_Data *vedata)
{
  WORKBENCH_PrivateData *wpd = vedata->stl->wpd;

  /* Free Smoke Textures after rendering */
  /* XXX This is a waste of processing and GPU bandwidth if nothing
   * is updated. But the problem is since Textures are stored in the
   * modifier we don't want them to take precious VRAM if the
   * modifier is not used for display. We should share them for
   * all viewport in a redraw at least. */
  LISTBASE_FOREACH (LinkData *, link, &wpd->smoke_domains) {
    FluidModifierData *fmd = (FluidModifierData *)link->data;
    DRW_smoke_free(fmd);
  }
  BLI_freelistN(&wpd->smoke_domains);
}
