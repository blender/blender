/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_vec_types.hh"
#include "BLI_string_ref.hh"

#include "DNA_scene_types.h"

#include "GPU_texture.h"

#include "COM_static_shader_manager.hh"
#include "COM_texture_pool.hh"

namespace blender::realtime_compositor {

/* ------------------------------------------------------------------------------------------------
 * Context
 *
 * A Context is an abstract class that is implemented by the caller of the evaluator to provide the
 * necessary data and functionalities for the correct operation of the evaluator. This includes
 * providing input data like render passes and the active scene, as well as references to the data
 * where the output of the evaluator will be written. The class also provides a reference to the
 * texture pool which should be implemented by the caller and provided during construction.
 * Finally, the class have an instance of a static shader manager for convenient shader
 * acquisition. */
class Context {
 private:
  /* A texture pool that can be used to allocate textures for the compositor efficiently. */
  TexturePool &texture_pool_;
  /* A static shader manager that can be used to acquire shaders for the compositor efficiently. */
  StaticShaderManager shader_manager_;

 public:
  Context(TexturePool &texture_pool);

  /* Get the active compositing scene. */
  virtual const Scene *get_scene() const = 0;

  /* Get the dimensions of the output. */
  virtual int2 get_output_size() = 0;

  /* Get the texture representing the output where the result of the compositor should be
   * written. This should be called by output nodes to get their target texture. */
  virtual GPUTexture *get_output_texture() = 0;

  /* Get the texture where the given render pass is stored. This should be called by the Render
   * Layer node to populate its outputs. */
  virtual GPUTexture *get_input_texture(int view_layer, eScenePassType pass_type) = 0;

  /* Get the name of the view currently being rendered. */
  virtual StringRef get_view_name() = 0;

  /* Set an info message. This is called by the compositor evaluator to inform or warn the user
   * about something, typically an error. The implementation should display the message in an
   * appropriate place, which can be directly in the UI or just logged to the output stream. */
  virtual void set_info_message(StringRef message) const = 0;

  /* Get the current frame number of the active scene. */
  int get_frame_number() const;

  /* Get the current time in seconds of the active scene. */
  float get_time() const;

  /* Get a reference to the texture pool of this context. */
  TexturePool &texture_pool();

  /* Get a reference to the static shader manager of this context. */
  StaticShaderManager &shader_manager();
};

}  // namespace blender::realtime_compositor
