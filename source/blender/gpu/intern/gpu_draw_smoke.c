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

#include "GPU_draw.h"
#include "GPU_glew.h"
#include "GPU_texture.h"

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

  float *spec_pixels = MEM_mallocN(TFUNC_WIDTH * 4 * 16 * 16 * sizeof(float), "spec_pixels");

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
  }
}

static GPUTexture *create_transfer_function(int type, const struct ColorBand *coba)
{
  float *data = MEM_mallocN(sizeof(float) * 4 * TFUNC_WIDTH, __func__);

  switch (type) {
    case TFUNC_FLAME_SPECTRUM:
      create_flame_spectrum_texture(data);
      break;
    case TFUNC_COLOR_RAMP:
      create_color_ramp(coba, data);
      break;
  }

  GPUTexture *tex = GPU_texture_create_1d(TFUNC_WIDTH, GPU_RGBA8, data, NULL);

  MEM_freeN(data);

  return tex;
}

static void swizzle_texture_channel_single(GPUTexture *tex)
{
  /* Swizzle texture channels so that we get useful RGBA values when sampling
   * a texture with fewer channels, e.g. when using density as color. */
  GPU_texture_bind(tex, 0);
  GPU_texture_swizzle_channel_auto(tex, 1);
  GPU_texture_unbind(tex);
}

static GPUTexture *create_field_texture(FluidDomainSettings *mds)
{
  float *field = NULL;

  switch (mds->coba_field) {
    case FLUID_DOMAIN_FIELD_DENSITY:
      field = manta_smoke_get_density(mds->fluid);
      break;
    case FLUID_DOMAIN_FIELD_HEAT:
      field = manta_smoke_get_heat(mds->fluid);
      break;
    case FLUID_DOMAIN_FIELD_FUEL:
      field = manta_smoke_get_fuel(mds->fluid);
      break;
    case FLUID_DOMAIN_FIELD_REACT:
      field = manta_smoke_get_react(mds->fluid);
      break;
    case FLUID_DOMAIN_FIELD_FLAME:
      field = manta_smoke_get_flame(mds->fluid);
      break;
    case FLUID_DOMAIN_FIELD_VELOCITY_X:
      field = manta_get_velocity_x(mds->fluid);
      break;
    case FLUID_DOMAIN_FIELD_VELOCITY_Y:
      field = manta_get_velocity_y(mds->fluid);
      break;
    case FLUID_DOMAIN_FIELD_VELOCITY_Z:
      field = manta_get_velocity_z(mds->fluid);
      break;
    case FLUID_DOMAIN_FIELD_COLOR_R:
      field = manta_smoke_get_color_r(mds->fluid);
      break;
    case FLUID_DOMAIN_FIELD_COLOR_G:
      field = manta_smoke_get_color_g(mds->fluid);
      break;
    case FLUID_DOMAIN_FIELD_COLOR_B:
      field = manta_smoke_get_color_b(mds->fluid);
      break;
    case FLUID_DOMAIN_FIELD_FORCE_X:
      field = manta_get_force_x(mds->fluid);
      break;
    case FLUID_DOMAIN_FIELD_FORCE_Y:
      field = manta_get_force_y(mds->fluid);
      break;
    case FLUID_DOMAIN_FIELD_FORCE_Z:
      field = manta_get_force_z(mds->fluid);
      break;
    default:
      return NULL;
  }

  GPUTexture *tex = GPU_texture_create_nD(
      mds->res[0], mds->res[1], mds->res[2], 3, field, GPU_R8, GPU_DATA_FLOAT, 0, true, NULL);

  swizzle_texture_channel_single(tex);
  return tex;
}

static GPUTexture *create_density_texture(FluidDomainSettings *mds, int highres)
{
  int *dim = (highres) ? mds->res_noise : mds->res;

  float *data;
  if (highres) {
    data = manta_smoke_turbulence_get_density(mds->fluid);
  }
  else {
    data = manta_smoke_get_density(mds->fluid);
  }

  GPUTexture *tex = GPU_texture_create_nD(
      dim[0], dim[1], dim[2], 3, data, GPU_R8, GPU_DATA_FLOAT, 0, true, NULL);

  swizzle_texture_channel_single(tex);

  return tex;
}

static GPUTexture *create_color_texture(FluidDomainSettings *mds, int highres)
{
  const bool has_color = (highres) ? manta_smoke_turbulence_has_colors(mds->fluid) :
                                     manta_smoke_has_colors(mds->fluid);

  if (!has_color) {
    return NULL;
  }

  int cell_count = (highres) ? manta_smoke_turbulence_get_cells(mds->fluid) : mds->total_cells;
  int *dim = (highres) ? mds->res_noise : mds->res;
  float *data = MEM_callocN(sizeof(float) * cell_count * 4, "smokeColorTexture");

  if (data == NULL) {
    return NULL;
  }

  if (highres) {
    manta_smoke_turbulence_get_rgba(mds->fluid, data, 0);
  }
  else {
    manta_smoke_get_rgba(mds->fluid, data, 0);
  }

  GPUTexture *tex = GPU_texture_create_nD(
      dim[0], dim[1], dim[2], 3, data, GPU_RGBA8, GPU_DATA_FLOAT, 0, true, NULL);

  MEM_freeN(data);

  return tex;
}

static GPUTexture *create_flame_texture(FluidDomainSettings *mds, int highres)
{
  float *source = NULL;
  const bool has_fuel = (highres) ? manta_smoke_turbulence_has_fuel(mds->fluid) :
                                    manta_smoke_has_fuel(mds->fluid);
  int *dim = (highres) ? mds->res_noise : mds->res;

  if (!has_fuel) {
    return NULL;
  }

  if (highres) {
    source = manta_smoke_turbulence_get_flame(mds->fluid);
  }
  else {
    source = manta_smoke_get_flame(mds->fluid);
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

void GPU_free_smoke(FluidModifierData *mmd)
{
  if (mmd->type & MOD_FLUID_TYPE_DOMAIN && mmd->domain) {
    if (mmd->domain->tex_density) {
      GPU_texture_free(mmd->domain->tex_density);
      mmd->domain->tex_density = NULL;
    }

    if (mmd->domain->tex_color) {
      GPU_texture_free(mmd->domain->tex_color);
      mmd->domain->tex_color = NULL;
    }

    if (mmd->domain->tex_shadow) {
      GPU_texture_free(mmd->domain->tex_shadow);
      mmd->domain->tex_shadow = NULL;
    }

    if (mmd->domain->tex_flame) {
      GPU_texture_free(mmd->domain->tex_flame);
      mmd->domain->tex_flame = NULL;
    }

    if (mmd->domain->tex_flame_coba) {
      GPU_texture_free(mmd->domain->tex_flame_coba);
      mmd->domain->tex_flame_coba = NULL;
    }

    if (mmd->domain->tex_coba) {
      GPU_texture_free(mmd->domain->tex_coba);
      mmd->domain->tex_coba = NULL;
    }

    if (mmd->domain->tex_field) {
      GPU_texture_free(mmd->domain->tex_field);
      mmd->domain->tex_field = NULL;
    }
  }
}

void GPU_create_smoke_coba_field(FluidModifierData *mmd)
{
#ifndef WITH_FLUID
  UNUSED_VARS(mmd);
#else
  if (mmd->type & MOD_FLUID_TYPE_DOMAIN) {
    FluidDomainSettings *mds = mmd->domain;

    if (!mds->tex_field) {
      mds->tex_field = create_field_texture(mds);
    }
    if (!mds->tex_coba) {
      mds->tex_coba = create_transfer_function(TFUNC_COLOR_RAMP, mds->coba);
    }
  }
#endif
}

void GPU_create_smoke(FluidModifierData *mmd, int highres)
{
#ifndef WITH_FLUID
  UNUSED_VARS(mmd, highres);
#else
  if (mmd->type & MOD_FLUID_TYPE_DOMAIN) {
    FluidDomainSettings *mds = mmd->domain;

    if (!mds->tex_density) {
      mds->tex_density = create_density_texture(mds, highres);
    }
    if (!mds->tex_color) {
      mds->tex_color = create_color_texture(mds, highres);
    }
    if (!mds->tex_flame) {
      mds->tex_flame = create_flame_texture(mds, highres);
    }
    if (!mds->tex_flame_coba && mds->tex_flame) {
      mds->tex_flame_coba = create_transfer_function(TFUNC_FLAME_SPECTRUM, NULL);
    }
    if (!mds->tex_shadow) {
      mds->tex_shadow = GPU_texture_create_nD(mds->res[0],
                                              mds->res[1],
                                              mds->res[2],
                                              3,
                                              manta_smoke_get_shadow(mds->fluid),
                                              GPU_R8,
                                              GPU_DATA_FLOAT,
                                              0,
                                              true,
                                              NULL);
    }
  }
#endif /* WITH_FLUID */
}

void GPU_create_smoke_velocity(FluidModifierData *mmd)
{
#ifndef WITH_FLUID
  UNUSED_VARS(mmd);
#else
  if (mmd->type & MOD_FLUID_TYPE_DOMAIN) {
    FluidDomainSettings *mds = mmd->domain;

    const float *vel_x = manta_get_velocity_x(mds->fluid);
    const float *vel_y = manta_get_velocity_y(mds->fluid);
    const float *vel_z = manta_get_velocity_z(mds->fluid);

    if (ELEM(NULL, vel_x, vel_y, vel_z)) {
      return;
    }

    if (!mds->tex_velocity_x) {
      mds->tex_velocity_x = GPU_texture_create_3d(
          mds->res[0], mds->res[1], mds->res[2], GPU_R16F, vel_x, NULL);
      mds->tex_velocity_y = GPU_texture_create_3d(
          mds->res[0], mds->res[1], mds->res[2], GPU_R16F, vel_y, NULL);
      mds->tex_velocity_z = GPU_texture_create_3d(
          mds->res[0], mds->res[1], mds->res[2], GPU_R16F, vel_z, NULL);
    }
  }
#endif /* WITH_FLUID */
}

/* TODO Unify with the other GPU_free_smoke. */
void GPU_free_smoke_velocity(FluidModifierData *mmd)
{
  if (mmd->type & MOD_FLUID_TYPE_DOMAIN && mmd->domain) {
    if (mmd->domain->tex_velocity_x) {
      GPU_texture_free(mmd->domain->tex_velocity_x);
    }

    if (mmd->domain->tex_velocity_y) {
      GPU_texture_free(mmd->domain->tex_velocity_y);
    }

    if (mmd->domain->tex_velocity_z) {
      GPU_texture_free(mmd->domain->tex_velocity_z);
    }

    mmd->domain->tex_velocity_x = NULL;
    mmd->domain->tex_velocity_y = NULL;
    mmd->domain->tex_velocity_z = NULL;
  }
}

/** \} */
