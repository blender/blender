/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "integrator/pass_accessor.h"

#include "kernel/types.h"
#include "session/buffers.h"

#include "util/log.h"

CCL_NAMESPACE_BEGIN

/* --------------------------------------------------------------------
 * Pass input information.
 */

PassAccessor::PassAccessInfo::PassAccessInfo(const BufferPass &pass)
    : type(pass.type),
      mode(pass.mode),
      include_albedo(pass.include_albedo),
      is_lightgroup(!pass.lightgroup.empty()),
      offset(pass.offset)
{
}

/* --------------------------------------------------------------------
 * Pass destination.
 */

PassAccessor::Destination::Destination(float *pixels, const int num_components)
    : pixels(pixels), num_components(num_components)
{
}

PassAccessor::Destination::Destination(const PassType pass_type, const PassMode pass_mode)
{
  const PassInfo pass_info = Pass::get_info(pass_type, pass_mode);
  num_components = pass_info.num_components;
}

/* --------------------------------------------------------------------
 * Pass source.
 */

PassAccessor::Source::Source(const float *pixels, const int num_components)
    : pixels(pixels), num_components(num_components)
{
}

/* --------------------------------------------------------------------
 * Pass accessor.
 */

PassAccessor::PassAccessor(const PassAccessInfo &pass_access_info,
                           const float exposure,
                           const int num_samples)
    : pass_access_info_(pass_access_info), exposure_(exposure), num_samples_(num_samples)
{
}

bool PassAccessor::get_render_tile_pixels(const RenderBuffers *render_buffers,
                                          const Destination &destination) const
{
  if (render_buffers == nullptr || render_buffers->buffer.data() == nullptr) {
    return false;
  }

  return get_render_tile_pixels(render_buffers, render_buffers->params, destination);
}

static void pad_pixels(const BufferParams &buffer_params,
                       const PassAccessor::Destination &destination,
                       const int src_num_components)
{
  /* When requesting a single channel pass as RGBA, or RGB pass as RGBA,
   * fill in the additional components for convenience. */
  const int dest_num_components = destination.num_components;

  if (src_num_components >= dest_num_components) {
    return;
  }

  const size_t size = static_cast<size_t>(buffer_params.width) * buffer_params.height;
  if (destination.pixels) {
    const size_t pixel_stride = destination.pixel_stride ? destination.pixel_stride :
                                                           destination.num_components;

    float *pixel = destination.pixels + pixel_stride * destination.offset;

    for (size_t i = 0; i < size; i++, pixel += dest_num_components) {
      if (dest_num_components >= 3 && src_num_components == 1) {
        pixel[1] = pixel[0];
        pixel[2] = pixel[0];
      }
      if (dest_num_components >= 4) {
        pixel[3] = 1.0f;
      }
    }
  }

  if (destination.pixels_half_rgba) {
    const half one = float_to_half_display(1.0f);
    half4 *pixel = destination.pixels_half_rgba + destination.offset;

    for (size_t i = 0; i < size; i++, pixel++) {
      if (dest_num_components >= 3 && src_num_components == 1) {
        pixel[0].y = pixel[0].x;
        pixel[0].z = pixel[0].x;
      }
      if (dest_num_components >= 4) {
        pixel[0].w = one;
      }
    }
  }
}

bool PassAccessor::get_render_tile_pixels(const RenderBuffers *render_buffers,
                                          const BufferParams &buffer_params,
                                          const Destination &destination) const
{
  if (render_buffers == nullptr || render_buffers->buffer.data() == nullptr) {
    return false;
  }

  const PassType type = pass_access_info_.type;
  const PassMode mode = pass_access_info_.mode;
  const PassInfo pass_info = Pass::get_info(
      type, mode, pass_access_info_.include_albedo, pass_access_info_.is_lightgroup);
  int num_written_components = pass_info.num_components;

  if (pass_info.num_components == 1) {
    if (is_volume_guiding_pass(type)) {
      get_pass_rgbe(render_buffers, buffer_params, destination);
      num_written_components = 3;
    }
    /* Single channel passes. */
    else if (mode == PassMode::DENOISED) {
      /* Denoised passes store their final pixels, no need in special calculation. */
      get_pass_float(render_buffers, buffer_params, destination);
    }
    else if (type == PASS_DEPTH) {
      get_pass_depth(render_buffers, buffer_params, destination);
    }
    else if (type == PASS_MIST) {
      get_pass_mist(render_buffers, buffer_params, destination);
    }
    else if (type == PASS_VOLUME_MAJORANT) {
      get_pass_volume_majorant(render_buffers, buffer_params, destination);
    }
    else if (type == PASS_SAMPLE_COUNT) {
      get_pass_sample_count(render_buffers, buffer_params, destination);
    }
    else {
      get_pass_float(render_buffers, buffer_params, destination);
    }
  }
  else if (type == PASS_MOTION) {
    /* Motion pass. */
    DCHECK_EQ(destination.num_components, 4) << "Motion pass must have 4 components";
    get_pass_motion(render_buffers, buffer_params, destination);
  }
  else if (type == PASS_CRYPTOMATTE) {
    /* Cryptomatte pass. */
    DCHECK_EQ(destination.num_components, 4) << "Cryptomatte pass must have 4 components";
    get_pass_cryptomatte(render_buffers, buffer_params, destination);
  }
  else {
    /* RGB, RGBA and vector passes. */
    DCHECK(destination.num_components == 3 || destination.num_components == 4)
        << pass_type_as_string(type) << " pass must have 3 or 4 components";

    if (type == PASS_SHADOW_CATCHER_MATTE && pass_access_info_.use_approximate_shadow_catcher) {
      /* Denoised matte with shadow needs to do calculation (will use denoised shadow catcher pass
       * to approximate shadow with). */
      get_pass_shadow_catcher_matte_with_shadow(render_buffers, buffer_params, destination);
    }
    else if (type == PASS_SHADOW_CATCHER && mode != PassMode::DENOISED) {
      /* Shadow catcher pass. */
      get_pass_shadow_catcher(render_buffers, buffer_params, destination);
    }
    else if ((pass_info.divide_type != PASS_NONE || pass_info.direct_type != PASS_NONE ||
              pass_info.indirect_type != PASS_NONE) &&
             mode != PassMode::DENOISED)
    {
      /* RGB lighting passes that need to divide out color and/or sum direct and indirect.
       * These can also optionally write alpha like the combined pass. */
      get_pass_light_path(render_buffers, buffer_params, destination);
      num_written_components = 4;
    }
    else {
      /* Passes that need no special computation, or denoised passes that already
       * had the computation done. */
      if (pass_info.num_components == 3) {
        get_pass_float3(render_buffers, buffer_params, destination);

        /* Use alpha for colors passes. */
        if (type == PASS_DIFFUSE_COLOR || type == PASS_GLOSSY_COLOR ||
            type == PASS_TRANSMISSION_COLOR)
        {
          num_written_components = destination.num_components;
        }
      }
      else if (pass_info.num_components == 4) {
        if (destination.num_components == 3) {
          /* Special case for denoiser access of RGBA passes ignoring alpha channel. */
          get_pass_float3(render_buffers, buffer_params, destination);
        }
        else if (type == PASS_COMBINED || type == PASS_SHADOW_CATCHER ||
                 type == PASS_SHADOW_CATCHER_MATTE)
        {
          /* Passes with transparency as 4th component. */
          get_pass_combined(render_buffers, buffer_params, destination);
        }
        else {
          /* Passes with alpha as 4th component. */
          get_pass_float4(render_buffers, buffer_params, destination);
        }
      }
    }
  }

  pad_pixels(buffer_params, destination, num_written_components);

  return true;
}

void PassAccessor::init_kernel_film_convert(KernelFilmConvert *kfilm_convert,
                                            const BufferParams &buffer_params,
                                            const Destination &destination) const
{
  const PassType type = pass_access_info_.type;
  const PassMode mode = pass_access_info_.mode;
  const PassInfo &pass_info = Pass::get_info(
      type, mode, pass_access_info_.include_albedo, pass_access_info_.is_lightgroup);

  kfilm_convert->pass_offset = pass_access_info_.offset;
  kfilm_convert->pass_stride = buffer_params.pass_stride;

  kfilm_convert->pass_use_exposure = pass_info.use_exposure;
  kfilm_convert->pass_use_filter = pass_info.use_filter;

  /* TODO(sergey): Some of the passes needs to become denoised when denoised pass is accessed. */
  if (pass_info.direct_type != PASS_NONE) {
    kfilm_convert->pass_offset = buffer_params.get_pass_offset(pass_info.direct_type);
  }
  kfilm_convert->pass_indirect = buffer_params.get_pass_offset(pass_info.indirect_type);
  kfilm_convert->pass_divide = buffer_params.get_pass_offset(pass_info.divide_type);

  kfilm_convert->pass_combined = buffer_params.get_pass_offset(PASS_COMBINED);
  kfilm_convert->pass_sample_count = buffer_params.get_pass_offset(PASS_SAMPLE_COUNT);
  kfilm_convert->pass_adaptive_aux_buffer = buffer_params.get_pass_offset(
      PASS_ADAPTIVE_AUX_BUFFER);
  kfilm_convert->pass_motion_weight = buffer_params.get_pass_offset(PASS_MOTION_WEIGHT);
  kfilm_convert->pass_shadow_catcher = buffer_params.get_pass_offset(PASS_SHADOW_CATCHER, mode);
  kfilm_convert->pass_shadow_catcher_sample_count = buffer_params.get_pass_offset(
      PASS_SHADOW_CATCHER_SAMPLE_COUNT);
  kfilm_convert->pass_shadow_catcher_matte = buffer_params.get_pass_offset(
      PASS_SHADOW_CATCHER_MATTE, mode);

  /* Background is not denoised, so always use noisy pass. */
  kfilm_convert->pass_background = buffer_params.get_pass_offset(PASS_BACKGROUND);

  /* If we have a sample count pass, we must perform the division in the kernel instead
   * (unless the sample count pass is the one being read). */
  const bool divide_by_samples = (type == PASS_SAMPLE_COUNT) ||
                                 (kfilm_convert->pass_sample_count == PASS_UNUSED);
  if (pass_info.use_filter && divide_by_samples) {
    kfilm_convert->scale = num_samples_ != 0 ? pass_info.scale / num_samples_ : 0.0f;
  }
  else {
    kfilm_convert->scale = pass_info.scale;
  }

  if (pass_info.use_exposure) {
    kfilm_convert->exposure = exposure_;
  }
  else {
    kfilm_convert->exposure = 1.0f;
  }

  kfilm_convert->scale_exposure = kfilm_convert->scale * kfilm_convert->exposure;

  kfilm_convert->use_approximate_shadow_catcher = pass_access_info_.use_approximate_shadow_catcher;
  kfilm_convert->use_approximate_shadow_catcher_background =
      pass_access_info_.use_approximate_shadow_catcher_background;
  kfilm_convert->show_active_pixels = pass_access_info_.show_active_pixels;

  kfilm_convert->num_components = destination.num_components;
  kfilm_convert->pixel_stride = destination.pixel_stride ? destination.pixel_stride :
                                                           destination.num_components;

  kfilm_convert->is_denoised = (mode == PassMode::DENOISED);
}

bool PassAccessor::set_render_tile_pixels(RenderBuffers *render_buffers, const Source &source)
{
  if (render_buffers == nullptr || render_buffers->buffer.data() == nullptr) {
    return false;
  }

  const PassInfo pass_info = Pass::get_info(pass_access_info_.type,
                                            pass_access_info_.mode,
                                            pass_access_info_.include_albedo,
                                            pass_access_info_.is_lightgroup);

  const BufferParams &buffer_params = render_buffers->params;

  float *buffer_data = render_buffers->buffer.data();
  const int size = buffer_params.width * buffer_params.height;

  const int out_stride = buffer_params.pass_stride;
  const int in_stride = source.num_components;
  const int num_components_to_copy = min(source.num_components, pass_info.num_components);

  float *out = buffer_data + pass_access_info_.offset;
  const float *in = source.pixels + source.offset * in_stride;

  for (int i = 0; i < size; i++, out += out_stride, in += in_stride) {
    memcpy(out, in, sizeof(float) * num_components_to_copy);
  }

  return true;
}

CCL_NAMESPACE_END
