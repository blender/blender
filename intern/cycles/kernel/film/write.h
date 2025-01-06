/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/globals.h"

#include "kernel/integrator/state.h"

#include "kernel/util/colorspace.h"

#ifdef __KERNEL_GPU__
#  define __ATOMIC_PASS_WRITE__
#endif

CCL_NAMESPACE_BEGIN

/* Get pointer to pixel in render buffer. */

ccl_device_forceinline ccl_global float *film_pass_pixel_render_buffer(
    KernelGlobals kg, ConstIntegratorState state, ccl_global float *ccl_restrict render_buffer)
{
  const uint32_t render_pixel_index = INTEGRATOR_STATE(state, path, render_pixel_index);
  const uint64_t render_buffer_offset = (uint64_t)render_pixel_index *
                                        kernel_data.film.pass_stride;
  return render_buffer + render_buffer_offset;
}

ccl_device_forceinline ccl_global float *film_pass_pixel_render_buffer_shadow(
    KernelGlobals kg,
    ConstIntegratorShadowState state,
    ccl_global float *ccl_restrict render_buffer)
{
  const uint32_t render_pixel_index = INTEGRATOR_STATE(state, shadow_path, render_pixel_index);
  const uint64_t render_buffer_offset = (uint64_t)render_pixel_index *
                                        kernel_data.film.pass_stride;
  return render_buffer + render_buffer_offset;
}

/* Accumulate in passes. */

ccl_device_inline void film_write_pass_float(ccl_global float *ccl_restrict buffer,
                                             const float value)
{
#ifdef __ATOMIC_PASS_WRITE__
  atomic_add_and_fetch_float(buffer, value);
#else
  *buffer += value;
#endif
}

ccl_device_inline void film_write_pass_float3(ccl_global float *ccl_restrict buffer,
                                              const float3 value)
{
#ifdef __ATOMIC_PASS_WRITE__
  ccl_global float *buf_x = buffer + 0;
  ccl_global float *buf_y = buffer + 1;
  ccl_global float *buf_z = buffer + 2;

  atomic_add_and_fetch_float(buf_x, value.x);
  atomic_add_and_fetch_float(buf_y, value.y);
  atomic_add_and_fetch_float(buf_z, value.z);
#else
  buffer[0] += value.x;
  buffer[1] += value.y;
  buffer[2] += value.z;
#endif
}

ccl_device_inline void film_write_pass_spectrum(ccl_global float *ccl_restrict buffer,
                                                Spectrum value)
{
  film_write_pass_float3(buffer, spectrum_to_rgb(value));
}

ccl_device_inline void film_write_pass_float4(ccl_global float *ccl_restrict buffer,
                                              const float4 value)
{
#ifdef __ATOMIC_PASS_WRITE__
  ccl_global float *buf_x = buffer + 0;
  ccl_global float *buf_y = buffer + 1;
  ccl_global float *buf_z = buffer + 2;
  ccl_global float *buf_w = buffer + 3;

  atomic_add_and_fetch_float(buf_x, value.x);
  atomic_add_and_fetch_float(buf_y, value.y);
  atomic_add_and_fetch_float(buf_z, value.z);
  atomic_add_and_fetch_float(buf_w, value.w);
#else
  buffer[0] += value.x;
  buffer[1] += value.y;
  buffer[2] += value.z;
  buffer[3] += value.w;
#endif
}

/* Overwrite for passes that only write on sample 0. This assumes only a single thread will write
 * to this pixel and no atomics are needed. */

ccl_device_inline void film_overwrite_pass_float(ccl_global float *ccl_restrict buffer,
                                                 const float value)
{
  *buffer = value;
}

ccl_device_inline void film_overwrite_pass_float3(ccl_global float *ccl_restrict buffer,
                                                  const float3 value)
{
  buffer[0] = value.x;
  buffer[1] = value.y;
  buffer[2] = value.z;
}

/* Read back from passes. */

ccl_device_inline float kernel_read_pass_float(const ccl_global float *ccl_restrict buffer)
{
  return *buffer;
}

ccl_device_inline float3 kernel_read_pass_float3(ccl_global float *ccl_restrict buffer)
{
  return make_float3(buffer[0], buffer[1], buffer[2]);
}

ccl_device_inline float4 kernel_read_pass_float4(ccl_global float *ccl_restrict buffer)
{
  return make_float4(buffer[0], buffer[1], buffer[2], buffer[3]);
}

CCL_NAMESPACE_END
