/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <atomic>

#include "app/opengl/shader.h"

#include "session/display_driver.h"

#include "util/function.h"
#include "util/unique_ptr.h"

CCL_NAMESPACE_BEGIN

class OpenGLDisplayDriver : public DisplayDriver {
 public:
  /* Callbacks for enabling and disabling the OpenGL context. Must be provided to support enabling
   * the context on the Cycles render thread independent of the main thread. */
  OpenGLDisplayDriver(const function<bool()> &gl_context_enable,
                      const function<void()> &gl_context_disable);
  ~OpenGLDisplayDriver();

  virtual void graphics_interop_activate() override;
  virtual void graphics_interop_deactivate() override;

  virtual void clear() override;

  void set_zoom(float zoom_x, float zoom_y);

 protected:
  virtual void next_tile_begin() override;

  virtual bool update_begin(const Params &params, int texture_width, int texture_height) override;
  virtual void update_end() override;

  virtual half4 *map_texture_buffer() override;
  virtual void unmap_texture_buffer() override;

  virtual GraphicsInterop graphics_interop_get() override;

  virtual void draw(const Params &params) override;

  /* Make sure texture is allocated and its initial configuration is performed. */
  bool gl_texture_resources_ensure();

  /* Ensure all runtime GPU resources needed for drawing are allocated.
   * Returns true if all resources needed for drawing are available. */
  bool gl_draw_resources_ensure();

  /* Destroy all GPU resources which are being used by this object. */
  void gl_resources_destroy();

  /* Update GPU texture dimensions and content if needed (new pixel data was provided).
   *
   * NOTE: The texture needs to be bound. */
  void texture_update_if_needed();

  /* Update vertex buffer with new coordinates of vertex positions and texture coordinates.
   * This buffer is used to render texture in the viewport.
   *
   * NOTE: The buffer needs to be bound. */
  void vertex_buffer_update(const Params &params);

  /* Texture which contains pixels of the render result. */
  struct {
    /* Indicates whether texture creation was attempted and succeeded.
     * Used to avoid multiple attempts of texture creation on GPU issues or GPU context
     * misconfiguration. */
    bool creation_attempted = false;
    bool is_created = false;

    /* OpenGL resource IDs of the texture itself and Pixel Buffer Object (PBO) used to write
     * pixels to it.
     *
     * NOTE: Allocated on the engine's context. */
    uint gl_id = 0;
    uint gl_pbo_id = 0;

    /* Is true when new data was written to the PBO, meaning, the texture might need to be resized
     * and new data is to be uploaded to the GPU. */
    bool need_update = false;

    /* Content of the texture is to be filled with zeroes. */
    std::atomic<bool> need_clear = true;

    /* Dimensions of the texture in pixels. */
    int width = 0;
    int height = 0;

    /* Dimensions of the underlying PBO. */
    int buffer_width = 0;
    int buffer_height = 0;
  } texture_;

  OpenGLShader display_shader_;

  /* Special track of whether GPU resources were attempted to be created, to avoid attempts of
   * their re-creation on failure on every redraw. */
  bool gl_draw_resource_creation_attempted_ = false;
  bool gl_draw_resources_created_ = false;

  /* Vertex buffer which hold vertices of a triangle fan which is textures with the texture
   * holding the render result. */
  uint vertex_buffer_ = 0;

  void *gl_render_sync_ = nullptr;
  void *gl_upload_sync_ = nullptr;

  float2 zoom_ = make_float2(1.0f, 1.0f);

  function<bool()> gl_context_enable_ = nullptr;
  function<void()> gl_context_disable_ = nullptr;
};

CCL_NAMESPACE_END
