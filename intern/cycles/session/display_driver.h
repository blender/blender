/* SPDX-FileCopyrightText: 2021-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/half.h"
#include "util/types.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

/* Info about the display device that will be used for graphics interop, so it
 * can be verified if interop is compatible with the rendering device. */
class GraphicsInteropDevice {
 public:
  enum Type {
    NONE,
    OPENGL,
    VULKAN,
    METAL,
  };

  Type type = NONE;
  vector<uint8_t> uuid;
};

/* Handle to a native graphics API pixel buffer. If supported, the rendering device
 * may write directly to this buffer instead of calling map_texture_buffer() and
 * unmap_texture_buffer().
 *
 * This must be a pixel buffer with the specified with and height, and half float
 * with RGBA channels. */
class GraphicsInteropBuffer {
 public:
  GraphicsInteropBuffer() = default;
  ~GraphicsInteropBuffer();

  GraphicsInteropBuffer(const GraphicsInteropBuffer &other) = delete;
  GraphicsInteropBuffer &operator=(const GraphicsInteropBuffer &other) = delete;
  GraphicsInteropBuffer(GraphicsInteropBuffer &&other) = delete;
  GraphicsInteropBuffer &operator=(GraphicsInteropBuffer &&other) = delete;

  /* Display Driver API. */

  /* Assign handle. For Vulkan, this transfers ownership of the handle. */
  void assign(GraphicsInteropDevice::Type type, int64_t handle, size_t size);
  /* Is a handle assigned? */
  bool is_empty() const;
  /* Zero memory. */
  void zero();
  /* Clear handle. */
  void clear();

  /* Device graphics interop API. */

  /* Get type of handle. */
  GraphicsInteropDevice::Type get_type() const;
  /* Get size of buffer. */
  size_t get_size() const;

  /* Is there a new handle to take ownership of? */
  bool has_new_handle() const;
  /* Take ownership of the handle. */
  int64_t take_handle();

  /* Take ownership of zeroing the buffer. */
  bool take_zero();

 protected:
  /* The handle is expected to be:
   * - OpenGL: pixel buffer object ID.
   * - Vulkan on Windows: opaque handle for VkBuffer.
   * - Vulkan on Unix: opaque file descriptor for VkBuffer.
   * - Metal: MTLBuffer with unified memory. */
  GraphicsInteropDevice::Type type_ = GraphicsInteropDevice::NONE;
  int64_t handle_ = 0;
  bool own_handle_ = false;

  /* Actual size of the memory, which must be `>= width * height * sizeof(half4)`. */
  size_t size_ = 0;

  /* Clear the entire buffer before doing partial write to it. */
  bool need_zero_ = false;
};

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
  virtual bool update_begin(const Params &params, const int width, const int height) = 0;
  virtual void update_end() = 0;

  /* Optionally flush outstanding display commands before ending the render loop. */
  virtual void flush() {};

  virtual half4 *map_texture_buffer() = 0;
  virtual void unmap_texture_buffer() = 0;

  GraphicsInteropBuffer graphics_interop_buffer_;

  /* Graphics interop to avoid CPU - GPU transfer. See GraphicsInteropBuffer for details. */
  virtual GraphicsInteropDevice graphics_interop_get_device()
  {
    return GraphicsInteropDevice();
  }

  virtual void graphics_interop_update_buffer() {}

  GraphicsInteropBuffer &graphics_interop_get_buffer()
  {
    return graphics_interop_buffer_;
  }

  /* (De)activate graphics context required for editing or deleting the graphics interop
   * object.
   *
   * For example, destruction of the CUDA object associated with an OpenGL requires the
   * OpenGL context to be active. */
  virtual void graphics_interop_activate() {};
  virtual void graphics_interop_deactivate() {};

  /* Clear the display buffer by filling it with zeros. */
  virtual void zero() = 0;

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
