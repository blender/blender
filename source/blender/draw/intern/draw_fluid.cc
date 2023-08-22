/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * GPU fluid drawing functions.
 */

#include <cstring>

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "DNA_fluid_types.h"
#include "DNA_modifier_types.h"

#include "MEM_guardedalloc.h"

#include "BKE_colorband.h"

#include "IMB_colormanagement.h"

#include "GPU_texture.h"

#include "draw_manager.h"

#include "draw_common.h" /* Own include. */

#ifdef WITH_FLUID
#  include "manta_fluid_API.h"
#endif

/* -------------------------------------------------------------------- */
/** \name Private API
 * \{ */

#ifdef WITH_FLUID

enum {
  TFUNC_FLAME_SPECTRUM = 0,
  TFUNC_COLOR_RAMP = 1,
};

#  define TFUNC_WIDTH 256

static void create_flame_spectrum_texture(float *data)
{
#  define FIRE_THRESH 7
#  define MAX_FIRE_ALPHA 0.06f
#  define FULL_ON_FIRE 100

  float *spec_pixels = (float *)MEM_mallocN(TFUNC_WIDTH * 4 * 16 * 16 * sizeof(float),
                                            "spec_pixels");

  IMB_colormanagement_blackbody_temperature_to_rgb_table(data, TFUNC_WIDTH, 1500, 3000);

  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 16; j++) {
      for (int k = 0; k < TFUNC_WIDTH; k++) {
        int index = (j * TFUNC_WIDTH * 16 + i * TFUNC_WIDTH + k) * 4;
        if (k >= FIRE_THRESH) {
          spec_pixels[index] = (data[k * 4]);
          spec_pixels[index + 1] = (data[k * 4 + 1]);
          spec_pixels[index + 2] = (data[k * 4 + 2]);
          spec_pixels[index + 3] = MAX_FIRE_ALPHA *
                                   ((k > FULL_ON_FIRE) ?
                                        1.0f :
                                        (k - FIRE_THRESH) / (float(FULL_ON_FIRE) - FIRE_THRESH));
        }
        else {
          zero_v4(&spec_pixels[index]);
        }
      }
    }
  }

  memcpy(data, spec_pixels, sizeof(float) * 4 * TFUNC_WIDTH);

  MEM_freeN(spec_pixels);

#  undef FIRE_THRESH
#  undef MAX_FIRE_ALPHA
#  undef FULL_ON_FIRE
}

static void create_color_ramp(const ColorBand *coba, float *data)
{
  for (int i = 0; i < TFUNC_WIDTH; i++) {
    BKE_colorband_evaluate(coba, float(i) / TFUNC_WIDTH, &data[i * 4]);
    straight_to_premul_v4(&data[i * 4]);
  }
}

static GPUTexture *create_transfer_function(int type, const ColorBand *coba)
{
  float *data = (float *)MEM_mallocN(sizeof(float[4]) * TFUNC_WIDTH, __func__);

  switch (type) {
    case TFUNC_FLAME_SPECTRUM:
      create_flame_spectrum_texture(data);
      break;
    case TFUNC_COLOR_RAMP:
      create_color_ramp(coba, data);
      break;
  }

  GPUTexture *tex = GPU_texture_create_1d(
      "transf_func", TFUNC_WIDTH, 1, GPU_SRGB8_A8, GPU_TEXTURE_USAGE_SHADER_READ, data);

  MEM_freeN(data);

  return tex;
}

static void swizzle_texture_channel_single(GPUTexture *tex)
{
  /* Swizzle texture channels so that we get useful RGBA values when sampling
   * a texture with fewer channels, e.g. when using density as color. */
  GPU_texture_swizzle_set(tex, "rrr1");
}

static float *rescale_3d(const int dim[3],
                         const int final_dim[3],
                         int channels,
                         const float *fpixels)
{
  const uint w = dim[0], h = dim[1], d = dim[2];
  const uint fw = final_dim[0], fh = final_dim[1], fd = final_dim[2];
  const uint xf = w / fw, yf = h / fh, zf = d / fd;
  const uint pixel_count = fw * fh * fd;
  float *nfpixels = (float *)MEM_mallocN(channels * sizeof(float) * pixel_count, __func__);

  if (nfpixels) {
    printf("Performance: You need to scale a 3D texture, feel the pain!\n");

    for (uint k = 0; k < fd; k++) {
      for (uint j = 0; j < fh; j++) {
        for (uint i = 0; i < fw; i++) {
          /* Obviously doing nearest filtering here,
           * it's going to be slow in any case, let's not make it worse. */
          float xb = i * xf;
          float yb = j * yf;
          float zb = k * zf;
          uint offset = k * (fw * fh) + i * fh + j;
          uint offset_orig = (zb) * (w * h) + (xb)*h + (yb);

          if (channels == 4) {
            nfpixels[offset * 4] = fpixels[offset_orig * 4];
            nfpixels[offset * 4 + 1] = fpixels[offset_orig * 4 + 1];
            nfpixels[offset * 4 + 2] = fpixels[offset_orig * 4 + 2];
            nfpixels[offset * 4 + 3] = fpixels[offset_orig * 4 + 3];
          }
          else if (channels == 1) {
            nfpixels[offset] = fpixels[offset_orig];
          }
          else {
            BLI_assert(0);
          }
        }
      }
    }
  }
  return nfpixels;
}

/* Will resize input to fit GL system limits. */
static GPUTexture *create_volume_texture(const int dim[3],
                                         eGPUTextureFormat texture_format,
                                         eGPUDataFormat data_format,
                                         const void *data)
{
  GPUTexture *tex = nullptr;
  int final_dim[3] = {UNPACK3(dim)};

  if (data == nullptr) {
    return nullptr;
  }

  while (true) {
    tex = GPU_texture_create_3d("volume",
                                UNPACK3(final_dim),
                                1,
                                texture_format,
                                GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_MIP_SWIZZLE_VIEW,
                                nullptr);

    if (tex != nullptr) {
      break;
    }

    if (final_dim[0] == 1 && final_dim[1] == 1 && final_dim[2] == 1) {
      break;
    }

    for (int i = 0; i < 3; i++) {
      final_dim[i] = max_ii(1, final_dim[i] / 2);
    }
  }

  if (tex == nullptr) {
    printf("Error: Could not create 3D texture.\n");
    tex = GPU_texture_create_error(3, false);
  }
  else if (equals_v3v3_int(dim, final_dim)) {
    /* No need to resize, just upload the data. */
    GPU_texture_update_sub(tex, data_format, data, 0, 0, 0, UNPACK3(final_dim));
  }
  else if (data_format != GPU_DATA_FLOAT) {
    printf("Error: Could not allocate 3D texture and not attempting to rescale non-float data.\n");
    tex = GPU_texture_create_error(3, false);
  }
  else {
    /* We need to resize the input. */
    int channels = ELEM(texture_format, GPU_R8, GPU_R16F, GPU_R32F) ? 1 : 4;
    float *rescaled_data = rescale_3d(dim, final_dim, channels, static_cast<const float *>(data));
    if (rescaled_data) {
      GPU_texture_update_sub(tex, GPU_DATA_FLOAT, rescaled_data, 0, 0, 0, UNPACK3(final_dim));
      MEM_freeN(rescaled_data);
    }
    else {
      printf("Error: Could not allocate rescaled 3d texture!\n");
      GPU_texture_free(tex);
      tex = GPU_texture_create_error(3, false);
    }
  }
  return tex;
}

static GPUTexture *create_field_texture(FluidDomainSettings *fds, bool single_precision)
{
  void *field = nullptr;
  eGPUDataFormat data_format = GPU_DATA_FLOAT;
  eGPUTextureFormat texture_format = GPU_R8;

  if (single_precision) {
    texture_format = GPU_R32F;
  }

  switch (fds->coba_field) {
    case FLUID_DOMAIN_FIELD_DENSITY:
      field = manta_smoke_get_density(fds->fluid);
      break;
    case FLUID_DOMAIN_FIELD_HEAT:
      field = manta_smoke_get_heat(fds->fluid);
      break;
    case FLUID_DOMAIN_FIELD_FUEL:
      field = manta_smoke_get_fuel(fds->fluid);
      break;
    case FLUID_DOMAIN_FIELD_REACT:
      field = manta_smoke_get_react(fds->fluid);
      break;
    case FLUID_DOMAIN_FIELD_FLAME:
      field = manta_smoke_get_flame(fds->fluid);
      break;
    case FLUID_DOMAIN_FIELD_VELOCITY_X:
      field = manta_get_velocity_x(fds->fluid);
      break;
    case FLUID_DOMAIN_FIELD_VELOCITY_Y:
      field = manta_get_velocity_y(fds->fluid);
      break;
    case FLUID_DOMAIN_FIELD_VELOCITY_Z:
      field = manta_get_velocity_z(fds->fluid);
      break;
    case FLUID_DOMAIN_FIELD_COLOR_R:
      field = manta_smoke_get_color_r(fds->fluid);
      break;
    case FLUID_DOMAIN_FIELD_COLOR_G:
      field = manta_smoke_get_color_g(fds->fluid);
      break;
    case FLUID_DOMAIN_FIELD_COLOR_B:
      field = manta_smoke_get_color_b(fds->fluid);
      break;
    case FLUID_DOMAIN_FIELD_FORCE_X:
      field = manta_get_force_x(fds->fluid);
      break;
    case FLUID_DOMAIN_FIELD_FORCE_Y:
      field = manta_get_force_y(fds->fluid);
      break;
    case FLUID_DOMAIN_FIELD_FORCE_Z:
      field = manta_get_force_z(fds->fluid);
      break;
    case FLUID_DOMAIN_FIELD_PHI:
      field = manta_get_phi(fds->fluid);
      texture_format = GPU_R16F;
      break;
    case FLUID_DOMAIN_FIELD_PHI_IN:
      field = manta_get_phi_in(fds->fluid);
      texture_format = GPU_R16F;
      break;
    case FLUID_DOMAIN_FIELD_PHI_OUT:
      field = manta_get_phiout_in(fds->fluid);
      texture_format = GPU_R16F;
      break;
    case FLUID_DOMAIN_FIELD_PHI_OBSTACLE:
      field = manta_get_phiobs_in(fds->fluid);
      texture_format = GPU_R16F;
      break;
    case FLUID_DOMAIN_FIELD_FLAGS:
      field = manta_smoke_get_flags(fds->fluid);
      data_format = GPU_DATA_INT;
      texture_format = GPU_R8UI;
      break;
    case FLUID_DOMAIN_FIELD_PRESSURE:
      field = manta_get_pressure(fds->fluid);
      texture_format = GPU_R16F;
      break;
    default:
      return nullptr;
  }

  if (field == nullptr) {
    return nullptr;
  }

  GPUTexture *tex = create_volume_texture(fds->res, texture_format, data_format, field);
  swizzle_texture_channel_single(tex);
  return tex;
}

static GPUTexture *create_density_texture(FluidDomainSettings *fds, int highres)
{
  int *dim = (highres) ? fds->res_noise : fds->res;

  float *data;
  if (highres) {
    data = manta_noise_get_density(fds->fluid);
  }
  else {
    data = manta_smoke_get_density(fds->fluid);
  }

  if (data == nullptr) {
    return nullptr;
  }

  GPUTexture *tex = create_volume_texture(dim, GPU_R8, GPU_DATA_FLOAT, data);
  swizzle_texture_channel_single(tex);
  return tex;
}

static GPUTexture *create_color_texture(FluidDomainSettings *fds, int highres)
{
  const bool has_color = (highres) ? manta_noise_has_colors(fds->fluid) :
                                     manta_smoke_has_colors(fds->fluid);

  if (!has_color) {
    return nullptr;
  }

  int cell_count = (highres) ? manta_noise_get_cells(fds->fluid) : fds->total_cells;
  int *dim = (highres) ? fds->res_noise : fds->res;
  float *data = (float *)MEM_callocN(sizeof(float) * cell_count * 4, "smokeColorTexture");

  if (data == nullptr) {
    return nullptr;
  }

  if (highres) {
    manta_noise_get_rgba(fds->fluid, data, 0);
  }
  else {
    manta_smoke_get_rgba(fds->fluid, data, 0);
  }

  GPUTexture *tex = create_volume_texture(dim, GPU_RGBA8, GPU_DATA_FLOAT, data);

  MEM_freeN(data);

  return tex;
}

static GPUTexture *create_flame_texture(FluidDomainSettings *fds, int highres)
{
  float *source = nullptr;
  const bool has_fuel = (highres) ? manta_noise_has_fuel(fds->fluid) :
                                    manta_smoke_has_fuel(fds->fluid);
  int *dim = (highres) ? fds->res_noise : fds->res;

  if (!has_fuel) {
    return nullptr;
  }

  if (highres) {
    source = manta_noise_get_flame(fds->fluid);
  }
  else {
    source = manta_smoke_get_flame(fds->fluid);
  }

  GPUTexture *tex = create_volume_texture(dim, GPU_R8, GPU_DATA_FLOAT, source);
  swizzle_texture_channel_single(tex);
  return tex;
}

static bool get_smoke_velocity_field(FluidDomainSettings *fds,
                                     float **r_velocity_x,
                                     float **r_velocity_y,
                                     float **r_velocity_z)
{
  const char vector_field = fds->vector_field;
  switch ((FLUID_DisplayVectorField)vector_field) {
    case FLUID_DOMAIN_VECTOR_FIELD_VELOCITY:
      *r_velocity_x = manta_get_velocity_x(fds->fluid);
      *r_velocity_y = manta_get_velocity_y(fds->fluid);
      *r_velocity_z = manta_get_velocity_z(fds->fluid);
      break;
    case FLUID_DOMAIN_VECTOR_FIELD_GUIDE_VELOCITY:
      *r_velocity_x = manta_get_guide_velocity_x(fds->fluid);
      *r_velocity_y = manta_get_guide_velocity_y(fds->fluid);
      *r_velocity_z = manta_get_guide_velocity_z(fds->fluid);
      break;
    case FLUID_DOMAIN_VECTOR_FIELD_FORCE:
      *r_velocity_x = manta_get_force_x(fds->fluid);
      *r_velocity_y = manta_get_force_y(fds->fluid);
      *r_velocity_z = manta_get_force_z(fds->fluid);
      break;
  }

  return *r_velocity_x && *r_velocity_y && *r_velocity_z;
}

#endif /* WITH_FLUID */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public API
 * \{ */

void DRW_smoke_ensure_coba_field(FluidModifierData *fmd)
{
#ifndef WITH_FLUID
  UNUSED_VARS(fmd);
#else
  if (fmd->type & MOD_FLUID_TYPE_DOMAIN) {
    FluidDomainSettings *fds = fmd->domain;

    if (!fds->tex_field) {
      fds->tex_field = create_field_texture(fds, false);
      BLI_addtail(&DST.vmempool->smoke_textures, BLI_genericNodeN(&fds->tex_field));
    }
    if (!fds->tex_coba && !ELEM(fds->coba_field,
                                FLUID_DOMAIN_FIELD_PHI,
                                FLUID_DOMAIN_FIELD_PHI_IN,
                                FLUID_DOMAIN_FIELD_PHI_OUT,
                                FLUID_DOMAIN_FIELD_PHI_OBSTACLE,
                                FLUID_DOMAIN_FIELD_FLAGS,
                                FLUID_DOMAIN_FIELD_PRESSURE))
    {
      fds->tex_coba = create_transfer_function(TFUNC_COLOR_RAMP, fds->coba);
      BLI_addtail(&DST.vmempool->smoke_textures, BLI_genericNodeN(&fds->tex_coba));
    }
  }
#endif
}

void DRW_smoke_ensure(FluidModifierData *fmd, int highres)
{
#ifndef WITH_FLUID
  UNUSED_VARS(fmd, highres);
#else
  if (fmd->type & MOD_FLUID_TYPE_DOMAIN) {
    FluidDomainSettings *fds = fmd->domain;

    if (!fds->tex_density) {
      fds->tex_density = create_density_texture(fds, highres);
      BLI_addtail(&DST.vmempool->smoke_textures, BLI_genericNodeN(&fds->tex_density));
    }
    if (!fds->tex_color) {
      fds->tex_color = create_color_texture(fds, highres);
      BLI_addtail(&DST.vmempool->smoke_textures, BLI_genericNodeN(&fds->tex_color));
    }
    if (!fds->tex_flame) {
      fds->tex_flame = create_flame_texture(fds, highres);
      BLI_addtail(&DST.vmempool->smoke_textures, BLI_genericNodeN(&fds->tex_flame));
    }
    if (!fds->tex_flame_coba && fds->tex_flame) {
      fds->tex_flame_coba = create_transfer_function(TFUNC_FLAME_SPECTRUM, nullptr);
      BLI_addtail(&DST.vmempool->smoke_textures, BLI_genericNodeN(&fds->tex_flame_coba));
    }
    if (!fds->tex_shadow) {
      fds->tex_shadow = create_volume_texture(
          fds->res, GPU_R8, GPU_DATA_FLOAT, manta_smoke_get_shadow(fds->fluid));
      BLI_addtail(&DST.vmempool->smoke_textures, BLI_genericNodeN(&fds->tex_shadow));
    }
  }
#endif /* WITH_FLUID */
}

void DRW_smoke_ensure_velocity(FluidModifierData *fmd)
{
#ifndef WITH_FLUID
  UNUSED_VARS(fmd);
#else
  if (fmd->type & MOD_FLUID_TYPE_DOMAIN) {
    FluidDomainSettings *fds = fmd->domain;
    float *vel_x = nullptr, *vel_y = nullptr, *vel_z = nullptr;

    if (!get_smoke_velocity_field(fds, &vel_x, &vel_y, &vel_z)) {
      fds->vector_field = FLUID_DOMAIN_VECTOR_FIELD_VELOCITY;
      get_smoke_velocity_field(fds, &vel_x, &vel_y, &vel_z);
    }

    if (ELEM(nullptr, vel_x, vel_y, vel_z)) {
      return;
    }

    if (!fds->tex_velocity_x) {
      fds->tex_velocity_x = GPU_texture_create_3d(
          "velx", UNPACK3(fds->res), 1, GPU_R16F, GPU_TEXTURE_USAGE_SHADER_READ, vel_x);
      fds->tex_velocity_y = GPU_texture_create_3d(
          "vely", UNPACK3(fds->res), 1, GPU_R16F, GPU_TEXTURE_USAGE_SHADER_READ, vel_y);
      fds->tex_velocity_z = GPU_texture_create_3d(
          "velz", UNPACK3(fds->res), 1, GPU_R16F, GPU_TEXTURE_USAGE_SHADER_READ, vel_z);
      BLI_addtail(&DST.vmempool->smoke_textures, BLI_genericNodeN(&fds->tex_velocity_x));
      BLI_addtail(&DST.vmempool->smoke_textures, BLI_genericNodeN(&fds->tex_velocity_y));
      BLI_addtail(&DST.vmempool->smoke_textures, BLI_genericNodeN(&fds->tex_velocity_z));
    }
  }
#endif /* WITH_FLUID */
}

void DRW_fluid_ensure_flags(FluidModifierData *fmd)
{
#ifndef WITH_FLUID
  UNUSED_VARS(fmd);
#else
  if (fmd->type & MOD_FLUID_TYPE_DOMAIN) {
    FluidDomainSettings *fds = fmd->domain;
    if (!fds->tex_flags) {
      fds->tex_flags = create_volume_texture(
          fds->res, GPU_R8UI, GPU_DATA_INT, manta_smoke_get_flags(fds->fluid));
      BLI_addtail(&DST.vmempool->smoke_textures, BLI_genericNodeN(&fds->tex_flags));

      swizzle_texture_channel_single(fds->tex_flags);
    }
  }
#endif /* WITH_FLUID */
}

void DRW_fluid_ensure_range_field(FluidModifierData *fmd)
{
#ifndef WITH_FLUID
  UNUSED_VARS(fmd);
#else
  if (fmd->type & MOD_FLUID_TYPE_DOMAIN) {
    FluidDomainSettings *fds = fmd->domain;

    if (!fds->tex_range_field) {
      fds->tex_range_field = create_field_texture(fds, true);
      BLI_addtail(&DST.vmempool->smoke_textures, BLI_genericNodeN(&fds->tex_range_field));
    }
  }
#endif /* WITH_FLUID */
}

void DRW_smoke_init(DRWData *drw_data)
{
  BLI_listbase_clear(&drw_data->smoke_textures);
}

void DRW_smoke_exit(DRWData *drw_data)
{
  /* Free Smoke Textures after rendering */
  /* XXX This is a waste of processing and GPU bandwidth if nothing
   * is updated. But the problem is since Textures are stored in the
   * modifier we don't want them to take precious VRAM if the
   * modifier is not used for display. We should share them for
   * all viewport in a redraw at least. */
  LISTBASE_FOREACH (LinkData *, link, &drw_data->smoke_textures) {
    GPU_TEXTURE_FREE_SAFE(*(GPUTexture **)link->data);
  }
  BLI_freelistN(&drw_data->smoke_textures);
}

/** \} */
