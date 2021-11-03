/*
 * Copyright 2021 Blender Foundation
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

#include "integrator/path_trace_tile.h"
#include "integrator/pass_accessor_cpu.h"
#include "integrator/path_trace.h"

#include "scene/film.h"
#include "scene/pass.h"
#include "scene/scene.h"
#include "session/buffers.h"

CCL_NAMESPACE_BEGIN

PathTraceTile::PathTraceTile(PathTrace &path_trace)
    : OutputDriver::Tile(path_trace.get_render_tile_offset(),
                         path_trace.get_render_tile_size(),
                         path_trace.get_render_size(),
                         path_trace.get_render_tile_params().layer,
                         path_trace.get_render_tile_params().view),
      path_trace_(path_trace),
      copied_from_device_(false)
{
}

bool PathTraceTile::get_pass_pixels(const string_view pass_name,
                                    const int num_channels,
                                    float *pixels) const
{
  /* NOTE: The code relies on a fact that session is fully update and no scene/buffer modification
   * is happening while this function runs. */

  if (!copied_from_device_) {
    /* Copy from device on demand. */
    path_trace_.copy_render_tile_from_device();
    const_cast<PathTraceTile *>(this)->copied_from_device_ = true;
  }

  const BufferParams &buffer_params = path_trace_.get_render_tile_params();

  const BufferPass *pass = buffer_params.find_pass(pass_name);
  if (pass == nullptr) {
    return false;
  }

  const bool has_denoised_result = path_trace_.has_denoised_result();
  if (pass->mode == PassMode::DENOISED && !has_denoised_result) {
    pass = buffer_params.find_pass(pass->type);
    if (pass == nullptr) {
      /* Happens when denoised result pass is requested but is never written by the kernel. */
      return false;
    }
  }

  pass = buffer_params.get_actual_display_pass(pass);

  const float exposure = buffer_params.exposure;
  const int num_samples = path_trace_.get_num_render_tile_samples();

  PassAccessor::PassAccessInfo pass_access_info(*pass);
  pass_access_info.use_approximate_shadow_catcher = buffer_params.use_approximate_shadow_catcher;
  pass_access_info.use_approximate_shadow_catcher_background =
      pass_access_info.use_approximate_shadow_catcher && !buffer_params.use_transparent_background;

  const PassAccessorCPU pass_accessor(pass_access_info, exposure, num_samples);
  const PassAccessor::Destination destination(pixels, num_channels);

  return path_trace_.get_render_tile_pixels(pass_accessor, destination);
}

bool PathTraceTile::set_pass_pixels(const string_view pass_name,
                                    const int num_channels,
                                    const float *pixels) const
{
  /* NOTE: The code relies on a fact that session is fully update and no scene/buffer modification
   * is happening while this function runs. */

  const BufferParams &buffer_params = path_trace_.get_render_tile_params();
  const BufferPass *pass = buffer_params.find_pass(pass_name);
  if (!pass) {
    return false;
  }

  const float exposure = buffer_params.exposure;
  const int num_samples = 1;

  const PassAccessor::PassAccessInfo pass_access_info(*pass);
  PassAccessorCPU pass_accessor(pass_access_info, exposure, num_samples);
  PassAccessor::Source source(pixels, num_channels);

  return path_trace_.set_render_tile_pixels(pass_accessor, source);
}

CCL_NAMESPACE_END
