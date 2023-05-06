/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

#include "scene/pass.h"
#include "util/half.h"
#include "util/string.h"
#include "util/types.h"

CCL_NAMESPACE_BEGIN

class RenderBuffers;
class BufferPass;
class BufferParams;
struct KernelFilmConvert;

/* Helper class which allows to access pass data.
 * Is designed in a way that it is created once when the pass data is known, and then pixels gets
 * progressively update from various render buffers. */
class PassAccessor {
 public:
  class PassAccessInfo {
   public:
    PassAccessInfo() = default;
    explicit PassAccessInfo(const BufferPass &pass);

    PassType type = PASS_NONE;
    PassMode mode = PassMode::NOISY;
    bool include_albedo = false;
    bool is_lightgroup = false;
    int offset = -1;

    /* For the shadow catcher matte pass: whether to approximate shadow catcher pass into its
     * matte pass, so that both artificial objects and shadows can be alpha-overed onto a backdrop.
     */
    bool use_approximate_shadow_catcher = false;

    /* When approximate shadow catcher matte is used alpha-over the result on top of background. */
    bool use_approximate_shadow_catcher_background = false;

    bool show_active_pixels = false;
  };

  class Destination {
   public:
    Destination() = default;
    Destination(float *pixels, int num_components);
    Destination(const PassType pass_type, half4 *pixels);

    /* Destination will be initialized with the number of components which is native for the given
     * pass type. */
    explicit Destination(const PassType pass_type);

    /* CPU-side pointers. only usable by the `PassAccessorCPU`. */
    float *pixels = nullptr;
    half4 *pixels_half_rgba = nullptr;

    /* Device-side pointers. */
    device_ptr d_pixels = 0;
    device_ptr d_pixels_half_rgba = 0;

    /* Number of components per pixel in the floating-point destination.
     * Is ignored for half4 destination (where number of components is implied to be 4). */
    int num_components = 0;

    /* Offset in pixels from the beginning of pixels storage.
     * Allows to get pixels of render buffer into a partial slice of the destination. */
    int offset = 0;

    /* Number of floats per pixel. When zero is the same as `num_components`.
     *
     * NOTE: Is ignored for half4 destination, as the half4 pixels are always 4-component
     * half-floats. */
    int pixel_stride = 0;

    /* Row stride in pixel elements:
     *  - For the float destination stride is a number of floats per row.
     *  - For the half4 destination stride is a number of half4 per row. */
    int stride = 0;
  };

  class Source {
   public:
    Source() = default;
    Source(const float *pixels, int num_components);

    /* CPU-side pointers. only usable by the `PassAccessorCPU`. */
    const float *pixels = nullptr;
    int num_components = 0;

    /* Offset in pixels from the beginning of pixels storage.
     * Allows to get pixels of render buffer into a partial slice of the destination. */
    int offset = 0;
  };

  PassAccessor(const PassAccessInfo &pass_access_info, float exposure, int num_samples);

  virtual ~PassAccessor() = default;

  /* Get pass data from the given render buffers, perform needed filtering, and store result into
   * the pixels.
   * The result is stored sequentially starting from the very beginning of the pixels memory. */
  bool get_render_tile_pixels(const RenderBuffers *render_buffers,
                              const Destination &destination) const;
  bool get_render_tile_pixels(const RenderBuffers *render_buffers,
                              const BufferParams &buffer_params,
                              const Destination &destination) const;
  /* Set pass data for the given render buffers. Used for baking to read from passes. */
  bool set_render_tile_pixels(RenderBuffers *render_buffers, const Source &source);

  const PassAccessInfo &get_pass_access_info() const
  {
    return pass_access_info_;
  }

 protected:
  virtual void init_kernel_film_convert(KernelFilmConvert *kfilm_convert,
                                        const BufferParams &buffer_params,
                                        const Destination &destination) const;

#define DECLARE_PASS_ACCESSOR(pass) \
  virtual void get_pass_##pass(const RenderBuffers *render_buffers, \
                               const BufferParams &buffer_params, \
                               const Destination &destination) const = 0;

  /* Float (scalar) passes. */
  DECLARE_PASS_ACCESSOR(depth)
  DECLARE_PASS_ACCESSOR(mist)
  DECLARE_PASS_ACCESSOR(sample_count)
  DECLARE_PASS_ACCESSOR(float)

  /* Float3 passes. */
  DECLARE_PASS_ACCESSOR(light_path)
  DECLARE_PASS_ACCESSOR(shadow_catcher)
  DECLARE_PASS_ACCESSOR(float3)

  /* Float4 passes. */
  DECLARE_PASS_ACCESSOR(motion)
  DECLARE_PASS_ACCESSOR(cryptomatte)
  DECLARE_PASS_ACCESSOR(shadow_catcher_matte_with_shadow)
  DECLARE_PASS_ACCESSOR(combined)
  DECLARE_PASS_ACCESSOR(float4)

#undef DECLARE_PASS_ACCESSOR

  PassAccessInfo pass_access_info_;

  float exposure_ = 0.0f;
  int num_samples_ = 0;
};

CCL_NAMESPACE_END
