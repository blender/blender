/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2021-2022 Blender Foundation */

#pragma once

#include "util/half.h"
#include "util/types.h"

CCL_NAMESPACE_BEGIN

/* Display driver for efficient interactive display of renders.
 *
 * Host applications implement this interface for viewport rendering. For best performance, we
 * recommend:
 * - Allocating a texture on the GPU to be interactively updated
 * - Using the graphics interop mechanism to avoid CPU-GPU copying overhead
 * - Using a dedicated or thread-safe graphics API context for updates, to avoid
 *   blocking the host application.
 */
class DisplayDriver {
 public:
  DisplayDriver() = default;
  virtual ~DisplayDriver() = default;

  /* Render buffer parameters. */
  struct Params {
   public:
    /* Render resolution, ignoring progressive resolution changes.
     * The texture buffer should be allocated with this size. */
    int2 size = make_int2(0, 0);

    /* For border rendering, the full resolution of the render, and the offset within that larger
     * render. */
    int2 full_size = make_int2(0, 0);
    int2 full_offset = make_int2(0, 0);

    bool modified(const Params &other) const
    {
      return !(full_offset == other.full_offset && full_size == other.full_size &&
               size == other.size);
    }
  };

  virtual void next_tile_begin() = 0;

  /* Update the render from the rendering thread.
   *
   * Cycles periodically updates the render to be displayed. For multithreaded updates with
   * potentially multiple rendering devices, it will call these methods as follows.
   *
   * if (driver.update_begin(params, width, height)) {
   *     parallel_for_each(rendering_device) {
   *         buffer = driver.map_texture_buffer();
   *         if (buffer) {
   *             fill(buffer);
   *             driver.unmap_texture_buffer();
   *         }
   *     }
   *     driver.update_end();
   * }
   *
   * The parameters may dynamically change due to camera changes in the scene, and resources should
   * be re-allocated accordingly.
   *
   * The width and height passed to update_begin() are the effective render resolution taking into
   * account progressive resolution changes, which may be equal to or smaller than the params.size.
   * For efficiency, changes in this resolution should be handled without re-allocating resources,
   * but rather by using a subset of the full resolution buffer. */
  virtual bool update_begin(const Params &params, int width, int height) = 0;
  virtual void update_end() = 0;

  /* Optionally flush outstanding display commands before ending the render loop. */
  virtual void flush(){};

  virtual half4 *map_texture_buffer() = 0;
  virtual void unmap_texture_buffer() = 0;

  /* Optionally return a handle to a native graphics API texture buffer. If supported,
   * the rendering device may write directly to this buffer instead of calling
   * map_texture_buffer() and unmap_texture_buffer(). */
  class GraphicsInterop {
   public:
    /* Dimensions of the buffer, in pixels. */
    int buffer_width = 0;
    int buffer_height = 0;

    /* OpenGL pixel buffer object. */
    int64_t opengl_pbo_id = 0;

    /* Clear the entire buffer before doing partial write to it. */
    bool need_clear = false;

    /* Enforce re-creation of the graphics interop object.
     *
     * When this field is true then the graphics interop will be re-created no matter what the
     * rest of the configuration is.
     * When this field is false the graphics interop will be re-created if the PBO or buffer size
     * did change.
     *
     * This allows to ensure graphics interop is re-created when there is a possibility that an
     * underlying PBO was re-allocated but did not change its ID. */
    bool need_recreate = false;
  };

  virtual GraphicsInterop graphics_interop_get()
  {
    return GraphicsInterop();
  }

  /* (De)activate graphics context required for editing or deleting the graphics interop
   * object.
   *
   * For example, destruction of the CUDA object associated with an OpenGL requires the
   * OpenGL context to be active. */
  virtual void graphics_interop_activate(){};
  virtual void graphics_interop_deactivate(){};

  /* Clear the display buffer by filling it with zeros. */
  virtual void clear() = 0;

  /* Draw the render using the native graphics API.
   *
   * Note that this may be called in parallel to updates. The implementation is responsible for
   * mutex locking or other mechanisms to avoid conflicts.
   *
   * The parameters may have changed since the last update. The implementation is responsible for
   * deciding to skip or adjust render display for such changes.
   *
   * Host application drawing the render buffer should use Session.draw(), which will
   * call this method. */
  virtual void draw(const Params &params) = 0;
};

CCL_NAMESPACE_END
