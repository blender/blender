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

#include <atomic>

#include "MEM_guardedalloc.h"

#include "RNA_blender_cpp.h"

#include "render/display_driver.h"

#include "util/util_thread.h"
#include "util/util_unique_ptr.h"

CCL_NAMESPACE_BEGIN

/* Base class of shader used for display driver rendering. */
class BlenderDisplayShader {
 public:
  static constexpr const char *position_attribute_name = "pos";
  static constexpr const char *tex_coord_attribute_name = "texCoord";

  /* Create shader implementation suitable for the given render engine and scene configuration. */
  static unique_ptr<BlenderDisplayShader> create(BL::RenderEngine &b_engine, BL::Scene &b_scene);

  BlenderDisplayShader() = default;
  virtual ~BlenderDisplayShader() = default;

  virtual void bind(int width, int height) = 0;
  virtual void unbind() = 0;

  /* Get attribute location for position and texture coordinate respectively.
   * NOTE: The shader needs to be bound to have access to those. */
  virtual int get_position_attrib_location();
  virtual int get_tex_coord_attrib_location();

 protected:
  /* Get program of this display shader.
   * NOTE: The shader needs to be bound to have access to this. */
  virtual uint get_shader_program() = 0;

  /* Cached values of various OpenGL resources. */
  int position_attribute_location_ = -1;
  int tex_coord_attribute_location_ = -1;
};

/* Implementation of display rendering shader used in the case when render engine does not support
 * display space shader. */
class BlenderFallbackDisplayShader : public BlenderDisplayShader {
 public:
  virtual void bind(int width, int height) override;
  virtual void unbind() override;

 protected:
  virtual uint get_shader_program() override;

  void create_shader_if_needed();
  void destroy_shader();

  uint shader_program_ = 0;
  int image_texture_location_ = -1;
  int fullscreen_location_ = -1;

  /* Shader compilation attempted. Which means, that if the shader program is 0 then compilation or
   * linking has failed. Do not attempt to re-compile the shader. */
  bool shader_compile_attempted_ = false;
};

class BlenderDisplaySpaceShader : public BlenderDisplayShader {
 public:
  BlenderDisplaySpaceShader(BL::RenderEngine &b_engine, BL::Scene &b_scene);

  virtual void bind(int width, int height) override;
  virtual void unbind() override;

 protected:
  virtual uint get_shader_program() override;

  BL::RenderEngine b_engine_;
  BL::Scene &b_scene_;

  /* Cached values of various OpenGL resources. */
  uint shader_program_ = 0;
};

/* Display driver implementation which is specific for Blender viewport integration. */
class BlenderDisplayDriver : public DisplayDriver {
 public:
  BlenderDisplayDriver(BL::RenderEngine &b_engine, BL::Scene &b_scene);
  ~BlenderDisplayDriver();

  virtual void graphics_interop_activate() override;
  virtual void graphics_interop_deactivate() override;

  virtual void clear() override;

  void set_zoom(float zoom_x, float zoom_y);

 protected:
  virtual bool update_begin(const Params &params, int texture_width, int texture_height) override;
  virtual void update_end() override;

  virtual half4 *map_texture_buffer() override;
  virtual void unmap_texture_buffer() override;

  virtual GraphicsInterop graphics_interop_get() override;

  virtual void draw(const Params &params) override;

  /* Helper function which allocates new GPU context. */
  void gl_context_create();
  bool gl_context_enable();
  void gl_context_disable();
  void gl_context_dispose();

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

  BL::RenderEngine b_engine_;

  /* OpenGL context which is used the render engine doesn't have its own. */
  void *gl_context_ = nullptr;
  /* The when Blender RenderEngine side context is not available and the DisplayDriver is to create
   * its own context. */
  bool use_gl_context_ = false;
  /* Mutex used to guard the `gl_context_`. */
  thread_mutex gl_context_mutex_;

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

    /* Display parameters the texture has been updated for. */
    Params params;
  } texture_;

  unique_ptr<BlenderDisplayShader> display_shader_;

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
};

CCL_NAMESPACE_END
