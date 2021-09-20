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

#include "device/device_graphics_interop.h"
#include "util/util_half.h"
#include "util/util_thread.h"
#include "util/util_types.h"

CCL_NAMESPACE_BEGIN

class BufferParams;

/* GPUDisplay class takes care of drawing render result in a viewport. The render result is stored
 * in a GPU-side texture, which is updated from a path tracer and drawn by an application.
 *
 * The base GPUDisplay does some special texture state tracking, which allows render Session to
 * make decisions on whether reset for an updated state is possible or not. This state should only
 * be tracked in a base class and a particular implementation should not worry about it.
 *
 * The subclasses should only implement the pure virtual methods, which allows them to not worry
 * about parent method calls, which helps them to be as small and reliable as possible. */

class GPUDisplayParams {
 public:
  /* Offset of the display within a viewport.
   * For example, set to a lower-bottom corner of border render in Blender's viewport. */
  int2 offset = make_int2(0, 0);

  /* Full viewport size.
   *
   * NOTE: Is not affected by the resolution divider. */
  int2 full_size = make_int2(0, 0);

  /* Effective vieport size.
   * In the case of border render, size of the border rectangle.
   *
   * NOTE: Is not affected by the resolution divider. */
  int2 size = make_int2(0, 0);

  bool modified(const GPUDisplayParams &other) const
  {
    return !(offset == other.offset && full_size == other.full_size && size == other.size);
  }
};

class GPUDisplay {
 public:
  GPUDisplay() = default;
  virtual ~GPUDisplay() = default;

  /* Reset the display for the new state of render session. Is called whenever session is reset,
   * which happens on changes like viewport navigation or viewport dimension change.
   *
   * This call will configure parameters for a changed buffer and reset the texture state. */
  void reset(const BufferParams &buffer_params);

  const GPUDisplayParams &get_params() const
  {
    return params_;
  }

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
   * NOTE: The GPUDisplay should be marked for an update being in process with `update_begin()`.
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
   * NOTE: The GPUDisplay should be marked for an update being in process with `update_begin()`.
   *
   * NOTE: Texture buffer can not be mapped while graphics interopeability is active. This means
   * that `map_texture_buffer()` is not allowed between `graphics_interop_begin()` and
   * `graphics_interop_end()` calls.
   */

  /* Map pixels memory form texture to a buffer available for write from CPU. Width and height will
   * define a requested size of the texture to write to.
   * Upon success a non-null pointer is returned and the texture buffer is to be unmapped.
   * If an error happens during mapping, or if mapoping is not supported by this GPU display a
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

  /* Get GPUDisplay graphics interoperability information which acts as a destination for the
   * device API. */
  DeviceGraphicsInteropDestination graphics_interop_get();

  /* (De)activate GPU display for graphics interoperability outside of regular display udpate
   * routines. */
  virtual void graphics_interop_activate();
  virtual void graphics_interop_deactivate();

  /* --------------------------------------------------------------------
   * Drawing.
   */

  /* Clear the texture by filling it with all zeroes.
   *
   * This call might happen in parallel with draw, but can never happen in parallel with the
   * update.
   *
   * The actual zero-ing can be deferred to a later moment. What is important is that after clear
   * and before pixels update the drawing texture will be fully empty, and that partial update
   * after clear will write new pixel values for an updating area, leaving everything else zeroed.
   *
   * If the GPU display supports graphics interoperability then the zeroing the display is to be
   * delegated to the device via the `DeviceGraphicsInteropDestination`. */
  virtual void clear() = 0;

  /* Draw the current state of the texture.
   *
   * Returns true if this call did draw an updated state of the texture. */
  bool draw();

 protected:
  /* Implementation-specific calls which subclasses are to implement.
   * These `do_foo()` method corresponds to their `foo()` calls, but they are purely virtual to
   * simplify their particular implementation. */
  virtual bool do_update_begin(const GPUDisplayParams &params,
                               int texture_width,
                               int texture_height) = 0;
  virtual void do_update_end() = 0;

  virtual void do_copy_pixels_to_texture(const half4 *rgba_pixels,
                                         int texture_x,
                                         int texture_y,
                                         int pixels_width,
                                         int pixels_height) = 0;

  virtual half4 *do_map_texture_buffer() = 0;
  virtual void do_unmap_texture_buffer() = 0;

  /* Note that this might be called in parallel to do_update_begin() and do_update_end(),
   * the subclass is responsible for appropriate mutex locks to avoid multiple threads
   * editing and drawing the texture at the same time. */
  virtual void do_draw(const GPUDisplayParams &params) = 0;

  virtual DeviceGraphicsInteropDestination do_graphics_interop_get() = 0;

 private:
  thread_mutex mutex_;
  GPUDisplayParams params_;

  /* Mark texture as its content has been updated.
   * Used from places which knows that the texture content has been brough up-to-date, so that the
   * drawing knows whether it can be performed, and whether drawing happenned with an up-to-date
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
    /* Denotes whether possibly existing state of GPU side texture is still usable.
     * It will not be usable in cases like render border did change (in this case we don't want
     * previous texture to be rendered at all).
     *
     * However, if only navigation or object in scene did change, then the outdated state of the
     * texture is still usable for draw, preventing display viewport flickering on navigation and
     * object modifications. */
    bool is_usable = false;

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
