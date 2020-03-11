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

#include "BKE_object.h"
#include "BKE_fluid.h"

#include "BLI_rand.h"
#include "BLI_dynstr.h"
#include "BLI_string_utils.h"

#include "DNA_modifier_types.h"
#include "DNA_object_force_types.h"
#include "DNA_fluid_types.h"

#include "GPU_draw.h"

enum {
  VOLUME_SH_SLICE = 0,
  VOLUME_SH_COBA,
  VOLUME_SH_CUBIC,
};

#define VOLUME_SH_MAX (1 << (VOLUME_SH_CUBIC + 1))

static struct {
  struct GPUShader *volume_sh[VOLUME_SH_MAX];
  struct GPUShader *volume_coba_sh;
  struct GPUTexture *dummy_tex;
  struct GPUTexture *dummy_coba_tex;
} e_data = {{NULL}};

extern char datatoc_workbench_volume_vert_glsl[];
extern char datatoc_workbench_volume_frag_glsl[];
extern char datatoc_common_view_lib_glsl[];
extern char datatoc_gpu_shader_common_obinfos_lib_glsl[];

static GPUShader *volume_shader_get(bool slice, bool coba, bool cubic)
{
  int id = 0;
  id += (slice) ? (1 << VOLUME_SH_SLICE) : 0;
  id += (coba) ? (1 << VOLUME_SH_COBA) : 0;
  id += (cubic) ? (1 << VOLUME_SH_CUBIC) : 0;

  if (!e_data.volume_sh[id]) {
    DynStr *ds = BLI_dynstr_new();

    if (slice) {
      BLI_dynstr_append(ds, "#define VOLUME_SLICE\n");
    }
    if (coba) {
      BLI_dynstr_append(ds, "#define USE_COBA\n");
    }
    if (cubic) {
      BLI_dynstr_append(ds, "#define USE_TRICUBIC\n");
    }

    char *defines = BLI_dynstr_get_cstring(ds);
    BLI_dynstr_free(ds);

    char *libs = BLI_string_joinN(datatoc_common_view_lib_glsl,
                                  datatoc_gpu_shader_common_obinfos_lib_glsl);

    e_data.volume_sh[id] = DRW_shader_create_with_lib(datatoc_workbench_volume_vert_glsl,
                                                      NULL,
                                                      datatoc_workbench_volume_frag_glsl,
                                                      libs,
                                                      defines);

    MEM_freeN(libs);
    MEM_freeN(defines);
  }

  return e_data.volume_sh[id];
}

void workbench_volume_engine_init(void)
{
  if (!e_data.dummy_tex) {
    float pixel[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    e_data.dummy_tex = GPU_texture_create_3d(1, 1, 1, GPU_RGBA8, pixel, NULL);
    e_data.dummy_coba_tex = GPU_texture_create_1d(1, GPU_RGBA8, pixel, NULL);
  }
}

void workbench_volume_engine_free(void)
{
  for (int i = 0; i < VOLUME_SH_MAX; i++) {
    DRW_SHADER_FREE_SAFE(e_data.volume_sh[i]);
  }
  DRW_TEXTURE_FREE_SAFE(e_data.dummy_tex);
  DRW_TEXTURE_FREE_SAFE(e_data.dummy_coba_tex);
}

void workbench_volume_cache_init(WORKBENCH_Data *vedata)
{
  vedata->psl->volume_pass = DRW_pass_create(
      "Volumes", DRW_STATE_WRITE_COLOR | DRW_STATE_BLEND_ALPHA_PREMUL | DRW_STATE_CULL_FRONT);
}

void workbench_volume_cache_populate(WORKBENCH_Data *vedata,
                                     Scene *UNUSED(scene),
                                     Object *ob,
                                     ModifierData *md)
{
  FluidModifierData *mmd = (FluidModifierData *)md;
  FluidDomainSettings *mds = mmd->domain;
  WORKBENCH_PrivateData *wpd = vedata->stl->g_data;
  WORKBENCH_EffectInfo *effect_info = vedata->stl->effects;
  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();
  DRWShadingGroup *grp = NULL;

  /* Don't try to show liquid domains here */
  if (!mds->fluid || !(mds->type == FLUID_DOMAIN_TYPE_GAS)) {
    return;
  }

  wpd->volumes_do = true;
  if (mds->use_coba) {
    GPU_create_smoke_coba_field(mmd);
  }
  else if (!(mds->flags & FLUID_DOMAIN_USE_NOISE)) {
    GPU_create_smoke(mmd, 0);
  }
  else if (mds->flags & FLUID_DOMAIN_USE_NOISE) {
    GPU_create_smoke(mmd, 1);
  }

  if ((!mds->use_coba && (mds->tex_density == NULL && mds->tex_color == NULL)) ||
      (mds->use_coba && mds->tex_field == NULL)) {
    return;
  }

  const bool use_slice = (mds->slice_method == FLUID_DOMAIN_SLICE_AXIS_ALIGNED &&
                          mds->axis_slice_method == AXIS_SLICE_SINGLE);
  const bool cubic_interp = (mds->interp_method == VOLUME_INTERP_CUBIC);
  GPUShader *sh = volume_shader_get(use_slice, mds->use_coba, cubic_interp);

  if (use_slice) {
    float invviewmat[4][4];
    DRW_view_viewmat_get(NULL, invviewmat, true);

    const int axis = (mds->slice_axis == SLICE_AXIS_AUTO) ?
                         axis_dominant_v3_single(invviewmat[2]) :
                         mds->slice_axis - 1;
    float dim[3];
    BKE_object_dimensions_get(ob, dim);
    /* 0.05f to achieve somewhat the same opacity as the full view.  */
    float step_length = max_ff(1e-16f, dim[axis] * 0.05f);

    grp = DRW_shgroup_create(sh, vedata->psl->volume_pass);
    DRW_shgroup_uniform_float_copy(grp, "slicePosition", mds->slice_depth);
    DRW_shgroup_uniform_int_copy(grp, "sliceAxis", axis);
    DRW_shgroup_uniform_float_copy(grp, "stepLength", step_length);
    DRW_shgroup_state_disable(grp, DRW_STATE_CULL_FRONT);
  }
  else {
    double noise_ofs;
    BLI_halton_1d(3, 0.0, effect_info->jitter_index, &noise_ofs);
    float dim[3], step_length, max_slice;
    float slice_ct[3] = {mds->res[0], mds->res[1], mds->res[2]};
    mul_v3_fl(slice_ct, max_ff(0.001f, mds->slice_per_voxel));
    max_slice = max_fff(slice_ct[0], slice_ct[1], slice_ct[2]);
    BKE_object_dimensions_get(ob, dim);
    invert_v3(slice_ct);
    mul_v3_v3(dim, slice_ct);
    step_length = len_v3(dim);

    grp = DRW_shgroup_create(sh, vedata->psl->volume_pass);
    DRW_shgroup_uniform_vec4(grp, "viewvecs[0]", (float *)wpd->viewvecs, 3);
    DRW_shgroup_uniform_int_copy(grp, "samplesLen", max_slice);
    DRW_shgroup_uniform_float_copy(grp, "stepLength", step_length);
    DRW_shgroup_uniform_float_copy(grp, "noiseOfs", noise_ofs);
    DRW_shgroup_state_enable(grp, DRW_STATE_CULL_FRONT);
  }

  if (mds->use_coba) {
    DRW_shgroup_uniform_texture(grp, "densityTexture", mds->tex_field);
    DRW_shgroup_uniform_texture(grp, "transferTexture", mds->tex_coba);
  }
  else {
    static float white[3] = {1.0f, 1.0f, 1.0f};
    bool use_constant_color = ((mds->active_fields & FLUID_DOMAIN_ACTIVE_COLORS) == 0 &&
                               (mds->active_fields & FLUID_DOMAIN_ACTIVE_COLOR_SET) != 0);
    DRW_shgroup_uniform_texture(
        grp, "densityTexture", (mds->tex_color) ? mds->tex_color : mds->tex_density);
    DRW_shgroup_uniform_texture(grp, "shadowTexture", mds->tex_shadow);
    DRW_shgroup_uniform_texture(
        grp, "flameTexture", (mds->tex_flame) ? mds->tex_flame : e_data.dummy_tex);
    DRW_shgroup_uniform_texture(
        grp, "flameColorTexture", (mds->tex_flame) ? mds->tex_flame_coba : e_data.dummy_coba_tex);
    DRW_shgroup_uniform_vec3(
        grp, "activeColor", (use_constant_color) ? mds->active_color : white, 1);
  }
  DRW_shgroup_uniform_texture_ref(grp, "depthBuffer", &dtxl->depth);
  DRW_shgroup_uniform_float_copy(grp, "densityScale", 10.0f * mds->display_thickness);

  if (use_slice) {
    DRW_shgroup_call(grp, DRW_cache_quad_get(), ob);
  }
  else {
    DRW_shgroup_call(grp, DRW_cache_cube_get(), ob);
  }

  BLI_addtail(&wpd->smoke_domains, BLI_genericNodeN(mmd));
}

void workbench_volume_smoke_textures_free(WORKBENCH_PrivateData *wpd)
{
  /* Free Smoke Textures after rendering */
  /* XXX This is a waste of processing and GPU bandwidth if nothing
   * is updated. But the problem is since Textures are stored in the
   * modifier we don't want them to take precious VRAM if the
   * modifier is not used for display. We should share them for
   * all viewport in a redraw at least. */
  for (LinkData *link = wpd->smoke_domains.first; link; link = link->next) {
    FluidModifierData *mmd = (FluidModifierData *)link->data;
    GPU_free_smoke(mmd);
  }
  BLI_freelistN(&wpd->smoke_domains);
}
