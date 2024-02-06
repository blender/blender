/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "device/device.h"

#include "integrator/pass_accessor_cpu.h"

#include "session/buffers.h"

#include "util/log.h"
#include "util/tbb.h"

// clang-format off
#include "kernel/device/cpu/compat.h"
#include "kernel/device/cpu/globals.h"
#include "kernel/types.h"
#include "kernel/film/read.h"
// clang-format on

CCL_NAMESPACE_BEGIN

/* --------------------------------------------------------------------
 * Kernel processing.
 */

inline void PassAccessorCPU::run_get_pass_kernel_processor_float(
    const KernelFilmConvert *kfilm_convert,
    const RenderBuffers *render_buffers,
    const BufferParams &buffer_params,
    const Destination &destination,
    const CPUKernels::FilmConvertFunction func) const
{
  /* NOTE: No overlays are applied since they are not used for final renders.
   * Can be supported via some sort of specialization to avoid code duplication. */

  DCHECK_EQ(destination.stride, 0) << "Custom stride for float destination is not implemented.";

  const int64_t pass_stride = buffer_params.pass_stride;
  const int64_t buffer_row_stride = buffer_params.stride * buffer_params.pass_stride;

  const float *window_data = render_buffers->buffer.data() + buffer_params.window_x * pass_stride +
                             buffer_params.window_y * buffer_row_stride;

  const int pixel_stride = destination.pixel_stride ? destination.pixel_stride :
                                                      destination.num_components;

  parallel_for(0, buffer_params.window_height, [&](int64_t y) {
    const float *buffer = window_data + y * buffer_row_stride;
    float *pixel = destination.pixels + destination.pixel_offset +
                   (y * buffer_params.width + destination.offset) * pixel_stride;
    func(kfilm_convert, buffer, pixel, buffer_params.window_width, pass_stride, pixel_stride);
  });
}

inline void PassAccessorCPU::run_get_pass_kernel_processor_half_rgba(
    const KernelFilmConvert *kfilm_convert,
    const RenderBuffers *render_buffers,
    const BufferParams &buffer_params,
    const Destination &destination,
    const CPUKernels::FilmConvertHalfRGBAFunction func) const
{
  const int64_t pass_stride = buffer_params.pass_stride;
  const int64_t buffer_row_stride = buffer_params.stride * buffer_params.pass_stride;

  const float *window_data = render_buffers->buffer.data() + buffer_params.window_x * pass_stride +
                             buffer_params.window_y * buffer_row_stride;

  half4 *dst_start = destination.pixels_half_rgba + destination.offset;
  const int destination_stride = destination.stride != 0 ? destination.stride :
                                                           buffer_params.width;

  parallel_for(0, buffer_params.window_height, [&](int64_t y) {
    const float *buffer = window_data + y * buffer_row_stride;
    half4 *pixel = dst_start + y * destination_stride;
    func(kfilm_convert, buffer, pixel, buffer_params.window_width, pass_stride);
  });
}

/* --------------------------------------------------------------------
 * Pass accessors.
 */

#define DEFINE_PASS_ACCESSOR(pass) \
  void PassAccessorCPU::get_pass_##pass(const RenderBuffers *render_buffers, \
                                        const BufferParams &buffer_params, \
                                        const Destination &destination) const \
  { \
    const CPUKernels &kernels = Device::get_cpu_kernels(); \
    KernelFilmConvert kfilm_convert; \
    init_kernel_film_convert(&kfilm_convert, buffer_params, destination); \
\
    if (destination.pixels) { \
      run_get_pass_kernel_processor_float(&kfilm_convert, \
                                          render_buffers, \
                                          buffer_params, \
                                          destination, \
                                          kernels.film_convert_##pass); \
    } \
\
    if (destination.pixels_half_rgba) { \
      run_get_pass_kernel_processor_half_rgba(&kfilm_convert, \
                                              render_buffers, \
                                              buffer_params, \
                                              destination, \
                                              kernels.film_convert_half_rgba_##pass); \
    } \
  }

/* Float (scalar) passes. */
DEFINE_PASS_ACCESSOR(depth)
DEFINE_PASS_ACCESSOR(mist)
DEFINE_PASS_ACCESSOR(sample_count)
DEFINE_PASS_ACCESSOR(float)

/* Float3 passes. */
DEFINE_PASS_ACCESSOR(light_path)
DEFINE_PASS_ACCESSOR(shadow_catcher)
DEFINE_PASS_ACCESSOR(float3)

/* Float4 passes. */
DEFINE_PASS_ACCESSOR(motion)
DEFINE_PASS_ACCESSOR(cryptomatte)
DEFINE_PASS_ACCESSOR(shadow_catcher_matte_with_shadow)
DEFINE_PASS_ACCESSOR(combined)
DEFINE_PASS_ACCESSOR(float4)

#undef DEFINE_PASS_ACCESSOR

CCL_NAMESPACE_END
