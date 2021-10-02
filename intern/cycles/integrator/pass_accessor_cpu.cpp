/*
 * Copyright 2011-2021 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "integrator/pass_accessor_cpu.h"

#include "render/buffers.h"
#include "util/util_logging.h"
#include "util/util_tbb.h"

// clang-format off
#include "kernel/device/cpu/compat.h"
#include "kernel/device/cpu/globals.h"
#include "kernel/kernel_types.h"
#include "kernel/kernel_film.h"
// clang-format on

CCL_NAMESPACE_BEGIN

/* --------------------------------------------------------------------
 * Kernel processing.
 */

template<typename Processor>
inline void PassAccessorCPU::run_get_pass_kernel_processor(const RenderBuffers *render_buffers,
                                                           const BufferParams &buffer_params,
                                                           const Destination &destination,
                                                           const Processor &processor) const
{
  KernelFilmConvert kfilm_convert;
  init_kernel_film_convert(&kfilm_convert, buffer_params, destination);

  if (destination.pixels) {
    /* NOTE: No overlays are applied since they are not used for final renders.
     * Can be supported via some sort of specialization to avoid code duplication. */

    run_get_pass_kernel_processor_float(
        &kfilm_convert, render_buffers, buffer_params, destination, processor);
  }

  if (destination.pixels_half_rgba) {
    /* TODO(sergey): Consider adding specialization to avoid per-pixel overlay check. */

    if (destination.num_components == 1) {
      run_get_pass_kernel_processor_half_rgba(&kfilm_convert,
                                              render_buffers,
                                              buffer_params,
                                              destination,
                                              [&processor](const KernelFilmConvert *kfilm_convert,
                                                           ccl_global const float *buffer,
                                                           float *pixel_rgba) {
                                                float pixel;
                                                processor(kfilm_convert, buffer, &pixel);

                                                pixel_rgba[0] = pixel;
                                                pixel_rgba[1] = pixel;
                                                pixel_rgba[2] = pixel;
                                                pixel_rgba[3] = 1.0f;
                                              });
    }
    else if (destination.num_components == 3) {
      run_get_pass_kernel_processor_half_rgba(&kfilm_convert,
                                              render_buffers,
                                              buffer_params,
                                              destination,
                                              [&processor](const KernelFilmConvert *kfilm_convert,
                                                           ccl_global const float *buffer,
                                                           float *pixel_rgba) {
                                                processor(kfilm_convert, buffer, pixel_rgba);
                                                pixel_rgba[3] = 1.0f;
                                              });
    }
    else if (destination.num_components == 4) {
      run_get_pass_kernel_processor_half_rgba(
          &kfilm_convert, render_buffers, buffer_params, destination, processor);
    }
  }
}

template<typename Processor>
inline void PassAccessorCPU::run_get_pass_kernel_processor_float(
    const KernelFilmConvert *kfilm_convert,
    const RenderBuffers *render_buffers,
    const BufferParams &buffer_params,
    const Destination &destination,
    const Processor &processor) const
{
  DCHECK_EQ(destination.stride, 0) << "Custom stride for float destination is not implemented.";

  const float *buffer_data = render_buffers->buffer.data();
  const int pixel_stride = destination.pixel_stride ? destination.pixel_stride :
                                                      destination.num_components;

  tbb::parallel_for(0, buffer_params.height, [&](int64_t y) {
    int64_t pixel_index = y * buffer_params.width;
    for (int64_t x = 0; x < buffer_params.width; ++x, ++pixel_index) {
      const int64_t input_pixel_offset = pixel_index * buffer_params.pass_stride;
      const float *buffer = buffer_data + input_pixel_offset;
      float *pixel = destination.pixels + (pixel_index + destination.offset) * pixel_stride;

      processor(kfilm_convert, buffer, pixel);
    }
  });
}

template<typename Processor>
inline void PassAccessorCPU::run_get_pass_kernel_processor_half_rgba(
    const KernelFilmConvert *kfilm_convert,
    const RenderBuffers *render_buffers,
    const BufferParams &buffer_params,
    const Destination &destination,
    const Processor &processor) const
{
  const float *buffer_data = render_buffers->buffer.data();

  half4 *dst_start = destination.pixels_half_rgba + destination.offset;
  const int destination_stride = destination.stride != 0 ? destination.stride :
                                                           buffer_params.width;

  tbb::parallel_for(0, buffer_params.height, [&](int64_t y) {
    int64_t pixel_index = y * buffer_params.width;
    half4 *dst_row_start = dst_start + y * destination_stride;
    for (int64_t x = 0; x < buffer_params.width; ++x, ++pixel_index) {
      const int64_t input_pixel_offset = pixel_index * buffer_params.pass_stride;
      const float *buffer = buffer_data + input_pixel_offset;

      float pixel[4];
      processor(kfilm_convert, buffer, pixel);

      film_apply_pass_pixel_overlays_rgba(kfilm_convert, buffer, pixel);

      half4 *pixel_half_rgba = dst_row_start + x;
      float4_store_half(&pixel_half_rgba->x, make_float4(pixel[0], pixel[1], pixel[2], pixel[3]));
    }
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
    run_get_pass_kernel_processor( \
        render_buffers, buffer_params, destination, film_get_pass_pixel_##pass); \
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
