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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * GPU fluid drawing functions.
 */

#include <string.h>

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_fluid_types.h"
#include "DNA_modifier_types.h"

#include "MEM_guardedalloc.h"

#include "BKE_colorband.h"

#include "GPU_texture.h"

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

  blackbody_temperature_to_rgb_table(data, TFUNC_WIDTH, 1500, 3000);

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
                                        (k - FIRE_THRESH) / ((float)FULL_ON_FIRE - FIRE_THRESH));
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

static void create_color_ramp(const struct ColorBand *coba, float *data)
{
  for (int i = 0; i < TFUNC_WIDTH; i++) {
    BKE_colorband_evaluate(coba, (float)i / TFUNC_WIDTH, &data[i * 4]);
    straight_to_premul_v4(&data[i * 4]);
  }
}

static GPUTexture *create_transfer_function(int type, const struct ColorBand *coba)
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

  GPUTexture *tex = GPU_texture_create_1d(TFUNC_WIDTH, GPU_SRGB8_A8, data, NULL);

  MEM_freeN(data);

  return tex;
}

static void swizzle_texture_channel_single(GPUTexture *tex)
{
  /* Swizzle texture channels so that we get useful RGBA values when sampling
   * a texture with fewer channels, e.g. when using density as color. */
  GPU_texture_bind(tex, 0);
  GPU_texture_swizzle_set(tex, "rrr1");
  GPU_texture_unbind(tex);
}

static GPUTexture *create_field_texture(FluidDomainSettings *fds)
{
  float *field = NULL;

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
    default:
      return NULL;
  }

  GPUTexture *tex = GPU_texture_create_nD(
      UNPACK3(fds->res), 3, field, GPU_R8, GPU_DATA_FLOAT, 0, true, NULL);

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

  GPUTexture *tex = GPU_texture_create_nD(
      UNPACK3(dim), 3, data, GPU_R8, GPU_DATA_FLOAT, 0, true, NULL);

  swizzle_texture_channel_single(tex);

  return tex;
}

static GPUTexture *create_color_texture(FluidDomainSettings *fds, int highres)
{
  const bool has_color = (highres) ? manta_noise_has_colors(fds->fluid) :
                                     manta_smoke_has_colors(fds->fluid);

  if (!has_color) {
    return NULL;
  }

  int cell_count = (highres) ? manta_noise_get_cells(fds->fluid) : fds->total_cells;
  int *dim = (highres) ? fds->res_noise : fds->res;
  float *data = (float *)MEM_callocN(sizeof(float) * cell_count * 4, "smokeColorTexture");

  if (data == NULL) {
    return NULL;
  }

  if (highres) {
    manta_noise_get_rgba(fds->fluid, data, 0);
  }
  else {
    manta_smoke_get_rgba(fds->fluid, data, 0);
  }

  GPUTexture *tex = GPU_texture_create_nD(
      dim[0], dim[1], dim[2], 3, data, GPU_RGBA8, GPU_DATA_FLOAT, 0, true, NULL);

  MEM_freeN(data);

  return tex;
}

static GPUTexture *create_flame_texture(FluidDomainSettings *fds, int highres)
{
  float *source = NULL;
  const bool has_fuel = (highres) ? manta_noise_has_fuel(fds->fluid) :
                                    manta_smoke_has_fuel(fds->fluid);
  int *dim = (highres) ? fds->res_noise : fds->res;

  if (!has_fuel) {
    return NULL;
  }

  if (highres) {
    source = manta_noise_get_flame(fds->fluid);
  }
  else {
    source = manta_smoke_get_flame(fds->fluid);
  }

  GPUTexture *tex = GPU_texture_create_nD(
      dim[0], dim[1], dim[2], 3, source, GPU_R8, GPU_DATA_FLOAT, 0, true, NULL);

  swizzle_texture_channel_single(tex);

  return tex;
}

#endif /* WITH_FLUID */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public API
 * \{ */

void DRW_smoke_free(FluidModifierData *fmd)
{
  if (fmd->type & MOD_FLUID_TYPE_DOMAIN && fmd->domain) {
    if (fmd->domain->tex_density) {
      GPU_texture_free(fmd->domain->tex_density);
      fmd->domain->tex_density = NULL;
    }

    if (fmd->domain->tex_color) {
      GPU_texture_free(fmd->domain->tex_color);
      fmd->domain->tex_color = NULL;
    }

    if (fmd->domain->tex_shadow) {
      GPU_texture_free(fmd->domain->tex_shadow);
      fmd->domain->tex_shadow = NULL;
    }

    if (fmd->domain->tex_flame) {
      GPU_texture_free(fmd->domain->tex_flame);
      fmd->domain->tex_flame = NULL;
    }

    if (fmd->domain->tex_flame_coba) {
      GPU_texture_free(fmd->domain->tex_flame_coba);
      fmd->domain->tex_flame_coba = NULL;
    }

    if (fmd->domain->tex_coba) {
      GPU_texture_free(fmd->domain->tex_coba);
      fmd->domain->tex_coba = NULL;
    }

    if (fmd->domain->tex_field) {
      GPU_texture_free(fmd->domain->tex_field);
      fmd->domain->tex_field = NULL;
    }
  }
}

void DRW_smoke_ensure_coba_field(FluidModifierData *fmd)
{
#ifndef WITH_FLUID
  UNUSED_VARS(fmd);
#else
  if (fmd->type & MOD_FLUID_TYPE_DOMAIN) {
    FluidDomainSettings *fds = fmd->domain;

    if (!fds->tex_field) {
      fds->tex_field = create_field_texture(fds);
    }
    if (!fds->tex_coba) {
      fds->tex_coba = create_transfer_function(TFUNC_COLOR_RAMP, fds->coba);
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
    }
    if (!fds->tex_color) {
      fds->tex_color = create_color_texture(fds, highres);
    }
    if (!fds->tex_flame) {
      fds->tex_flame = create_flame_texture(fds, highres);
    }
    if (!fds->tex_flame_coba && fds->tex_flame) {
      fds->tex_flame_coba = create_transfer_function(TFUNC_FLAME_SPECTRUM, NULL);
    }
    if (!fds->tex_shadow) {
      fds->tex_shadow = GPU_texture_create_nD(UNPACK3(fds->res),
                                              3,
                                              manta_smoke_get_shadow(fds->fluid),
                                              GPU_R8,
                                              GPU_DATA_FLOAT,
                                              0,
                                              true,
                                              NULL);
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

    const float *vel_x = manta_get_velocity_x(fds->fluid);
    const float *vel_y = manta_get_velocity_y(fds->fluid);
    const float *vel_z = manta_get_velocity_z(fds->fluid);

    if (ELEM(NULL, vel_x, vel_y, vel_z)) {
      return;
    }

    if (!fds->tex_velocity_x) {
      fds->tex_velocity_x = GPU_texture_create_3d(UNPACK3(fds->res), GPU_R16F, vel_x, NULL);
      fds->tex_velocity_y = GPU_texture_create_3d(UNPACK3(fds->res), GPU_R16F, vel_y, NULL);
      fds->tex_velocity_z = GPU_texture_create_3d(UNPACK3(fds->res), GPU_R16F, vel_z, NULL);
    }
  }
#endif /* WITH_FLUID */
}

/* TODO Unify with the other DRW_smoke_free. */
void DRW_smoke_free_velocity(FluidModifierData *fmd)
{
  if (fmd->type & MOD_FLUID_TYPE_DOMAIN && fmd->domain) {
    if (fmd->domain->tex_velocity_x) {
      GPU_texture_free(fmd->domain->tex_velocity_x);
    }

    if (fmd->domain->tex_velocity_y) {
      GPU_texture_free(fmd->domain->tex_velocity_y);
    }

    if (fmd->domain->tex_velocity_z) {
      GPU_texture_free(fmd->domain->tex_velocity_z);
    }

    fmd->domain->tex_velocity_x = NULL;
    fmd->domain->tex_velocity_y = NULL;
    fmd->domain->tex_velocity_z = NULL;
  }
}

/** \} */
