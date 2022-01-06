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

#include "session/display_driver.h"

#include "util/thread.h"
#include "util/unique_ptr.h"
#include "util/vector.h"

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
  virtual void next_tile_begin() override;

  virtual bool update_begin(const Params &params, int texture_width, int texture_height) override;
  virtual void update_end() override;

  virtual half4 *map_texture_buffer() override;
  virtual void unmap_texture_buffer() override;

  virtual GraphicsInterop graphics_interop_get() override;

  virtual void draw(const Params &params) override;

  virtual void flush() override;

  /* Helper function which allocates new GPU context. */
  void gl_context_create();
  bool gl_context_enable();
  void gl_context_disable();
  void gl_context_dispose();

  /* Destroy all GPU resources which are being used by this object. */
  void gl_resources_destroy();

  BL::RenderEngine b_engine_;

  /* OpenGL context which is used the render engine doesn't have its own. */
  void *gl_context_ = nullptr;
  /* The when Blender RenderEngine side context is not available and the DisplayDriver is to create
   * its own context. */
  bool use_gl_context_ = false;
  /* Mutex used to guard the `gl_context_`. */
  thread_mutex gl_context_mutex_;

  /* Content of the display is to be filled with zeroes. */
  std::atomic<bool> need_clear_ = true;

  unique_ptr<BlenderDisplayShader> display_shader_;

  /* Opaque storage for an internal state and data for tiles. */
  struct Tiles;
  unique_ptr<Tiles> tiles_;

  void *gl_render_sync_ = nullptr;
  void *gl_upload_sync_ = nullptr;

  float2 zoom_ = make_float2(1.0f, 1.0f);
};

CCL_NAMESPACE_END
