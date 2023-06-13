/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#include "kernel/geom/geom.h"

#include "kernel/camera/camera.h"

#include "kernel/film/cryptomatte_passes.h"
#include "kernel/film/write.h"

CCL_NAMESPACE_BEGIN

ccl_device_inline size_t film_write_cryptomatte_pass(ccl_global float *ccl_restrict buffer,
                                                     size_t depth,
                                                     float id,
                                                     float matte_weight)
{
  film_write_cryptomatte_slots(buffer, depth * 2, id, matte_weight);
  return depth * 4;
}

ccl_device_inline void film_write_data_passes(KernelGlobals kg,
                                              IntegratorState state,
                                              ccl_private const ShaderData *sd,
                                              ccl_global float *ccl_restrict render_buffer)
{
#ifdef __PASSES__
  const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);

  if (!(path_flag & PATH_RAY_TRANSPARENT_BACKGROUND)) {
    return;
  }

  /* Don't write data passes for paths that were split off for shadow catchers
   * to avoid double-counting. */
  if (path_flag & PATH_RAY_SHADOW_CATCHER_PASS) {
    return;
  }

  const int flag = kernel_data.film.pass_flag;

  if (!(flag & PASS_ANY)) {
    return;
  }

  ccl_global float *buffer = film_pass_pixel_render_buffer(kg, state, render_buffer);

  if (!(path_flag & PATH_RAY_SINGLE_PASS_DONE)) {
    if (INTEGRATOR_STATE(state, path, sample) == 0) {
      if (flag & PASSMASK(DEPTH)) {
        const float depth = camera_z_depth(kg, sd->P);
        film_overwrite_pass_float(buffer + kernel_data.film.pass_depth, depth);
      }
      if (flag & PASSMASK(OBJECT_ID)) {
        const float id = object_pass_id(kg, sd->object);
        film_overwrite_pass_float(buffer + kernel_data.film.pass_object_id, id);
      }
      if (flag & PASSMASK(MATERIAL_ID)) {
        const float id = shader_pass_id(kg, sd);
        film_overwrite_pass_float(buffer + kernel_data.film.pass_material_id, id);
      }
      if (flag & PASSMASK(POSITION)) {
        const float3 position = sd->P;
        film_overwrite_pass_float3(buffer + kernel_data.film.pass_position, position);
      }
    }

    if (!(sd->flag & SD_TRANSPARENT) || kernel_data.film.pass_alpha_threshold == 0.0f ||
        average(surface_shader_alpha(kg, sd)) >= kernel_data.film.pass_alpha_threshold)
    {
      if (flag & PASSMASK(NORMAL)) {
        const float3 normal = surface_shader_average_normal(kg, sd);
        film_write_pass_float3(buffer + kernel_data.film.pass_normal, normal);
      }
      if (flag & PASSMASK(ROUGHNESS)) {
        const float roughness = surface_shader_average_roughness(sd);
        film_write_pass_float(buffer + kernel_data.film.pass_roughness, roughness);
      }
      if (flag & PASSMASK(UV)) {
        const float3 uv = primitive_uv(kg, sd);
        film_write_pass_float3(buffer + kernel_data.film.pass_uv, uv);
      }
      if (flag & PASSMASK(MOTION)) {
        const float4 speed = primitive_motion_vector(kg, sd);
        film_write_pass_float4(buffer + kernel_data.film.pass_motion, speed);
        film_write_pass_float(buffer + kernel_data.film.pass_motion_weight, 1.0f);
      }

      INTEGRATOR_STATE_WRITE(state, path, flag) |= PATH_RAY_SINGLE_PASS_DONE;
    }
  }

  if (kernel_data.film.cryptomatte_passes) {
    const Spectrum throughput = INTEGRATOR_STATE(state, path, throughput);
    const float matte_weight = average(throughput) *
                               (1.0f - average(surface_shader_transparency(kg, sd)));
    if (matte_weight > 0.0f) {
      ccl_global float *cryptomatte_buffer = buffer + kernel_data.film.pass_cryptomatte;
      if (kernel_data.film.cryptomatte_passes & CRYPT_OBJECT) {
        const float id = object_cryptomatte_id(kg, sd->object);
        cryptomatte_buffer += film_write_cryptomatte_pass(
            cryptomatte_buffer, kernel_data.film.cryptomatte_depth, id, matte_weight);
      }
      if (kernel_data.film.cryptomatte_passes & CRYPT_MATERIAL) {
        const float id = kernel_data_fetch(shaders, (sd->shader & SHADER_MASK)).cryptomatte_id;
        cryptomatte_buffer += film_write_cryptomatte_pass(
            cryptomatte_buffer, kernel_data.film.cryptomatte_depth, id, matte_weight);
      }
      if (kernel_data.film.cryptomatte_passes & CRYPT_ASSET) {
        const float id = object_cryptomatte_asset_id(kg, sd->object);
        cryptomatte_buffer += film_write_cryptomatte_pass(
            cryptomatte_buffer, kernel_data.film.cryptomatte_depth, id, matte_weight);
      }
    }
  }

  if (flag & PASSMASK(DIFFUSE_COLOR)) {
    const Spectrum throughput = INTEGRATOR_STATE(state, path, throughput);
    film_write_pass_spectrum(buffer + kernel_data.film.pass_diffuse_color,
                             surface_shader_diffuse(kg, sd) * throughput);
  }
  if (flag & PASSMASK(GLOSSY_COLOR)) {
    const Spectrum throughput = INTEGRATOR_STATE(state, path, throughput);
    film_write_pass_spectrum(buffer + kernel_data.film.pass_glossy_color,
                             surface_shader_glossy(kg, sd) * throughput);
  }
  if (flag & PASSMASK(TRANSMISSION_COLOR)) {
    const Spectrum throughput = INTEGRATOR_STATE(state, path, throughput);
    film_write_pass_spectrum(buffer + kernel_data.film.pass_transmission_color,
                             surface_shader_transmission(kg, sd) * throughput);
  }
  if (flag & PASSMASK(MIST)) {
    /* Bring depth into 0..1 range. */
    const float mist_start = kernel_data.film.mist_start;
    const float mist_inv_depth = kernel_data.film.mist_inv_depth;

    const float depth = camera_distance(kg, sd->P);
    float mist = saturatef((depth - mist_start) * mist_inv_depth);

    /* Falloff */
    const float mist_falloff = kernel_data.film.mist_falloff;

    if (mist_falloff == 1.0f)
      ;
    else if (mist_falloff == 2.0f)
      mist = mist * mist;
    else if (mist_falloff == 0.5f)
      mist = sqrtf(mist);
    else
      mist = powf(mist, mist_falloff);

    /* Modulate by transparency */
    const Spectrum throughput = INTEGRATOR_STATE(state, path, throughput);
    const Spectrum alpha = surface_shader_alpha(kg, sd);
    const float mist_output = (1.0f - mist) * average(throughput * alpha);

    /* Note that the final value in the render buffer we want is 1 - mist_output,
     * to avoid having to tracking this in the Integrator state we do the negation
     * after rendering. */
    film_write_pass_float(buffer + kernel_data.film.pass_mist, mist_output);
  }
#endif
}

ccl_device_inline void film_write_data_passes_background(
    KernelGlobals kg, IntegratorState state, ccl_global float *ccl_restrict render_buffer)
{
#ifdef __PASSES__
  const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);

  if (!(path_flag & PATH_RAY_TRANSPARENT_BACKGROUND)) {
    return;
  }

  /* Don't write data passes for paths that were split off for shadow catchers
   * to avoid double-counting. */
  if (path_flag & PATH_RAY_SHADOW_CATCHER_PASS) {
    return;
  }

  const int flag = kernel_data.film.pass_flag;

  if (!(flag & PASS_ANY)) {
    return;
  }

  if (!(path_flag & PATH_RAY_SINGLE_PASS_DONE)) {
    ccl_global float *buffer = film_pass_pixel_render_buffer(kg, state, render_buffer);

    if (INTEGRATOR_STATE(state, path, sample) == 0) {
      if (flag & PASSMASK(DEPTH)) {
        film_overwrite_pass_float(buffer + kernel_data.film.pass_depth, 0.0f);
      }
      if (flag & PASSMASK(OBJECT_ID)) {
        film_overwrite_pass_float(buffer + kernel_data.film.pass_object_id, 0.0f);
      }
      if (flag & PASSMASK(MATERIAL_ID)) {
        film_overwrite_pass_float(buffer + kernel_data.film.pass_material_id, 0.0f);
      }
      if (flag & PASSMASK(POSITION)) {
        film_overwrite_pass_float3(buffer + kernel_data.film.pass_position, zero_float3());
      }
    }
  }
#endif
}

CCL_NAMESPACE_END
