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

#pragma once

#include "session/display_driver.h"

#include "util/half.h"
#include "util/thread.h"
#include "util/types.h"
#include "util/unique_ptr.h"

CCL_NAMESPACE_BEGIN

class BufferParams;

/* PathTraceDisplay is used for efficient render buffer display.
 *
 * The host applications implements a DisplayDriver, storing a render pass in a GPU-side
 * textures. This texture is continuously updated by the path tracer and drawn by the host
 * application.
 *
 * PathTraceDisplay is a wrapper around the DisplayDriver, adding thread safety, state tracking
 * and error checking. */

class PathTraceDisplay {
 public:
  PathTraceDisplay(unique_ptr<DisplayDriver> driver);
  virtual ~PathTraceDisplay() = default;

  /* Reset the display for the new state of render session. Is called whenever session is reset,
   * which happens on changes like viewport navigation or viewport dimension change.
   *
   * This call will configure parameters for a changed buffer and reset the texture state. */
  void reset(const BufferParams &buffer_params);

  /* --------------------------------------------------------------------
   * Update procedure.
   *
   * These calls indicates a desire of the caller to update content of the displayed texture. */

  /* Returns true when update is ready. Update should be finished with update_end().
   *
   * If false is returned then no update is possible, and no update_end() call is needed.
   *
   * The texture width and height denotes an actual resolution of the underlying render result. */
  bool update_begin(int texture_width, int texture_height);

  void update_end();

  /* Get currently configured texture size of the display (as configured by `update_begin()`. */
  int2 get_texture_size() const;

  /* --------------------------------------------------------------------
   * Texture update from CPU buffer.
   *
   * NOTE: The PathTraceDisplay should be marked for an update being in process with
   * `update_begin()`.
   *
   * Most portable implementation, which must be supported by all platforms. Might not be the most
   * efficient one.
   */

  /* Copy buffer of rendered pixels of a given size into a given position of the texture.
   *
   * This function does not acquire a lock. The reason for this is is to allow use of this function
   * for partial updates from different devices. In this case the caller will acquire the lock
   * once, update all the slices and release
   * the lock once. This will ensure that draw() will never use partially updated texture. */
  void copy_pixels_to_texture(
      const half4 *rgba_pixels, int texture_x, int texture_y, int pixels_width, int pixels_height);

  /* --------------------------------------------------------------------
   * Texture buffer mapping.
   *
   * This functionality is used to update GPU-side texture content without need to maintain CPU
   * side buffer on the caller.
   *
   * NOTE: The PathTraceDisplay should be marked for an update being in process with
   * `update_begin()`.
   *
   * NOTE: Texture buffer can not be mapped while graphics interoperability is active. This means
   * that `map_texture_buffer()` is not allowed between `graphics_interop_begin()` and
   * `graphics_interop_end()` calls.
   */

  /* Map pixels memory form texture to a buffer available for write from CPU. Width and height will
   * define a requested size of the texture to write to.
   * Upon success a non-null pointer is returned and the texture buffer is to be unmapped.
   * If an error happens during mapping, or if mapping is not supported by this GPU display a
   * null pointer is returned and the buffer is NOT to be unmapped.
   *
   * NOTE: Usually the implementation will rely on a GPU context of some sort, and the GPU context
   * is often can not be bound to two threads simultaneously, and can not be released from a
   * different thread. This means that the mapping API should be used from the single thread only,
   */
  half4 *map_texture_buffer();
  void unmap_texture_buffer();

  /* --------------------------------------------------------------------
   * Graphics interoperability.
   *
   * A special code path which allows to update texture content directly from the GPU compute
   * device. Complementary part of DeviceGraphicsInterop.
   *
   * NOTE: Graphics interoperability can not be used while the texture buffer is mapped. This means
   * that `graphics_interop_get()` is not allowed between `map_texture_buffer()` and
   * `unmap_texture_buffer()` calls. */

  /* Get PathTraceDisplay graphics interoperability information which acts as a destination for the
   * device API. */
  DisplayDriver::GraphicsInterop graphics_interop_get();

  /* (De)activate GPU display for graphics interoperability outside of regular display update
   * routines. */
  void graphics_interop_activate();
  void graphics_interop_deactivate();

  /* --------------------------------------------------------------------
   * Drawing.
   */

  /* Clear the texture by filling it with all zeroes.
   *
   * This call might happen in parallel with draw, but can never happen in parallel with the
   * update.
   *
   * The actual zeroing can be deferred to a later moment. What is important is that after clear
   * and before pixels update the drawing texture will be fully empty, and that partial update
   * after clear will write new pixel values for an updating area, leaving everything else zeroed.
   *
   * If the GPU display supports graphics interoperability then the zeroing the display is to be
   * delegated to the device via the `DisplayDriver::GraphicsInterop`. */
  void clear();

  /* Draw the current state of the texture.
   *
   * Returns true if this call did draw an updated state of the texture. */
  bool draw();

 private:
  /* Display driver implemented by the host application. */
  unique_ptr<DisplayDriver> driver_;

  /* Current display parameters */
  thread_mutex mutex_;
  DisplayDriver::Params params_;

  /* Mark texture as its content has been updated.
   * Used from places which knows that the texture content has been brought up-to-date, so that the
   * drawing knows whether it can be performed, and whether drawing happened with an up-to-date
   * texture state. */
  void mark_texture_updated();

  /* State of the update process. */
  struct {
    /* True when update is in process, indicated by `update_begin()` / `update_end()`. */
    bool is_active = false;
  } update_state_;

  /* State of the texture, which is needed for an integration with render session and interactive
   * updates and navigation. */
  struct {
    /* Texture is considered outdated after `reset()` until the next call of
     * `copy_pixels_to_texture()`. */
    bool is_outdated = true;

    /* Texture size in pixels. */
    int2 size = make_int2(0, 0);
  } texture_state_;

  /* State of the texture buffer. Is tracked to perform sanity checks. */
  struct {
    /* True when the texture buffer is mapped with `map_texture_buffer()`. */
    bool is_mapped = false;
  } texture_buffer_state_;
};

CCL_NAMESPACE_END
