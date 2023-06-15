/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/geom/geom.h"

#include "kernel/film/write.h"

CCL_NAMESPACE_BEGIN

ccl_device_inline void film_write_aov_pass_value(KernelGlobals kg,
                                                 ConstIntegratorState state,
                                                 ccl_global float *ccl_restrict render_buffer,
                                                 const int aov_id,
                                                 const float value)
{
  ccl_global float *buffer = film_pass_pixel_render_buffer(kg, state, render_buffer);
  film_write_pass_float(buffer + kernel_data.film.pass_aov_value + aov_id, value);
}

ccl_device_inline void film_write_aov_pass_color(KernelGlobals kg,
                                                 ConstIntegratorState state,
                                                 ccl_global float *ccl_restrict render_buffer,
                                                 const int aov_id,
                                                 const float3 color)
{
  ccl_global float *buffer = film_pass_pixel_render_buffer(kg, state, render_buffer);
  film_write_pass_float4(buffer + kernel_data.film.pass_aov_color + aov_id,
                         make_float4(color.x, color.y, color.z, 1.0f));
}

CCL_NAMESPACE_END
