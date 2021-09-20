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

#include "render/gpu_display.h"

#include "render/buffers.h"
#include "util/util_logging.h"

CCL_NAMESPACE_BEGIN

void GPUDisplay::reset(const BufferParams &buffer_params)
{
  thread_scoped_lock lock(mutex_);

  const GPUDisplayParams old_params = params_;

  params_.offset = make_int2(buffer_params.full_x, buffer_params.full_y);
  params_.full_size = make_int2(buffer_params.full_width, buffer_params.full_height);
  params_.size = make_int2(buffer_params.width, buffer_params.height);

  /* If the parameters did change tag texture as unusable. This avoids drawing old texture content
   * in an updated configuration of the viewport. For example, avoids drawing old frame when render
   * border did change.
   * If the parameters did not change, allow drawing the current state of the texture, which will
   * not count as an up-to-date redraw. This will avoid flickering when doping camera navigation by
   * showing a previously rendered frame for until the new one is ready. */
  if (old_params.modified(params_)) {
    texture_state_.is_usable = false;
  }

  texture_state_.is_outdated = true;
}

void GPUDisplay::mark_texture_updated()
{
  texture_state_.is_outdated = false;
  texture_state_.is_usable = true;
}

/* --------------------------------------------------------------------
 * Update procedure.
 */

bool GPUDisplay::update_begin(int texture_width, int texture_height)
{
  DCHECK(!update_state_.is_active);

  if (update_state_.is_active) {
    LOG(ERROR) << "Attempt to re-activate update process.";
    return false;
  }

  /* Get parameters within a mutex lock, to avoid reset() modifying them at the same time.
   * The update itself is non-blocking however, for better performance and to avoid
   * potential deadlocks due to locks held by the subclass. */
  GPUDisplayParams params;
  {
    thread_scoped_lock lock(mutex_);
    params = params_;
    texture_state_.size = make_int2(texture_width, texture_height);
  }

  if (!do_update_begin(params, texture_width, texture_height)) {
    LOG(ERROR) << "GPUDisplay implementation could not begin update.";
    return false;
  }

  update_state_.is_active = true;

  return true;
}

void GPUDisplay::update_end()
{
  DCHECK(update_state_.is_active);

  if (!update_state_.is_active) {
    LOG(ERROR) << "Attempt to deactivate inactive update process.";
    return;
  }

  do_update_end();

  update_state_.is_active = false;
}

int2 GPUDisplay::get_texture_size() const
{
  return texture_state_.size;
}

/* --------------------------------------------------------------------
 * Texture update from CPU buffer.
 */

void GPUDisplay::copy_pixels_to_texture(
    const half4 *rgba_pixels, int texture_x, int texture_y, int pixels_width, int pixels_height)
{
  DCHECK(update_state_.is_active);

  if (!update_state_.is_active) {
    LOG(ERROR) << "Attempt to copy pixels data outside of GPUDisplay update.";
    return;
  }

  mark_texture_updated();
  do_copy_pixels_to_texture(rgba_pixels, texture_x, texture_y, pixels_width, pixels_height);
}

/* --------------------------------------------------------------------
 * Texture buffer mapping.
 */

half4 *GPUDisplay::map_texture_buffer()
{
  DCHECK(!texture_buffer_state_.is_mapped);
  DCHECK(update_state_.is_active);

  if (texture_buffer_state_.is_mapped) {
    LOG(ERROR) << "Attempt to re-map an already mapped texture buffer.";
    return nullptr;
  }

  if (!update_state_.is_active) {
    LOG(ERROR) << "Attempt to copy pixels data outside of GPUDisplay update.";
    return nullptr;
  }

  half4 *mapped_rgba_pixels = do_map_texture_buffer();

  if (mapped_rgba_pixels) {
    texture_buffer_state_.is_mapped = true;
  }

  return mapped_rgba_pixels;
}

void GPUDisplay::unmap_texture_buffer()
{
  DCHECK(texture_buffer_state_.is_mapped);

  if (!texture_buffer_state_.is_mapped) {
    LOG(ERROR) << "Attempt to unmap non-mapped texture buffer.";
    return;
  }

  texture_buffer_state_.is_mapped = false;

  mark_texture_updated();
  do_unmap_texture_buffer();
}

/* --------------------------------------------------------------------
 * Graphics interoperability.
 */

DeviceGraphicsInteropDestination GPUDisplay::graphics_interop_get()
{
  DCHECK(!texture_buffer_state_.is_mapped);
  DCHECK(update_state_.is_active);

  if (texture_buffer_state_.is_mapped) {
    LOG(ERROR)
        << "Attempt to use graphics interoperability mode while the texture buffer is mapped.";
    return DeviceGraphicsInteropDestination();
  }

  if (!update_state_.is_active) {
    LOG(ERROR) << "Attempt to use graphics interoperability outside of GPUDisplay update.";
    return DeviceGraphicsInteropDestination();
  }

  /* Assume that interop will write new values to the texture. */
  mark_texture_updated();

  return do_graphics_interop_get();
}

void GPUDisplay::graphics_interop_activate()
{
}

void GPUDisplay::graphics_interop_deactivate()
{
}

/* --------------------------------------------------------------------
 * Drawing.
 */

bool GPUDisplay::draw()
{
  /* Get parameters within a mutex lock, to avoid reset() modifying them at the same time.
   * The drawing itself is non-blocking however, for better performance and to avoid
   * potential deadlocks due to locks held by the subclass. */
  GPUDisplayParams params;
  bool is_usable;
  bool is_outdated;

  {
    thread_scoped_lock lock(mutex_);
    params = params_;
    is_usable = texture_state_.is_usable;
    is_outdated = texture_state_.is_outdated;
  }

  if (is_usable) {
    do_draw(params);
  }

  return !is_outdated;
}

CCL_NAMESPACE_END
