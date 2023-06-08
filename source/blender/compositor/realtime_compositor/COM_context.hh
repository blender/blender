/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_vector_types.hh"
#include "BLI_string_ref.hh"

#include "DNA_ID.h"
#include "DNA_scene_types.h"
#include "DNA_vec_types.h"

#include "GPU_texture.h"

#include "COM_static_cache_manager.hh"
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
 * Finally, the class have an instance of a static shader manager and a static resource manager
 * for acquiring cached shaders and resources efficiently. */
class Context {
 private:
  /* A texture pool that can be used to allocate textures for the compositor efficiently. */
  TexturePool &texture_pool_;
  /* A static shader manager that can be used to acquire shaders for the compositor efficiently. */
  StaticShaderManager shader_manager_;
  /* A static cache manager that can be used to acquire cached resources for the compositor
   * efficiently. */
  StaticCacheManager cache_manager_;

 public:
  Context(TexturePool &texture_pool);

  /* Get the node tree used for compositing. */
  virtual const bNodeTree &get_node_tree() const = 0;

  /* True if compositor should do write file outputs, false if only running for viewing. */
  virtual bool use_file_output() const = 0;

  /* True if color management should be used for texture evaluation. */
  virtual bool use_texture_color_management() const = 0;

  /* Get the render settings for compositing. */
  virtual const RenderData &get_render_data() const = 0;

  /* Get the width and height of the render passes and of the output texture returned by the
   * get_input_texture and get_output_texture methods respectively. */
  virtual int2 get_render_size() const = 0;

  /* Get the rectangular region representing the area of the input that the compositor will operate
   * on. Conversely, the compositor will only update the region of the output that corresponds to
   * the compositing region. In the base case, the compositing region covers the entirety of the
   * render region with a lower bound of zero and an upper bound of the render size returned by the
   * get_render_size method. In other cases, the compositing region might be a subset of the render
   * region. */
  virtual rcti get_compositing_region() const = 0;

  /* Get the texture representing the output where the result of the compositor should be
   * written. This should be called by output nodes to get their target texture. */
  virtual GPUTexture *get_output_texture() = 0;

  /* Get the texture where the given render pass is stored. This should be called by the Render
   * Layer node to populate its outputs. */
  virtual GPUTexture *get_input_texture(int view_layer, const char *pass_name) = 0;

  /* Get the name of the view currently being rendered. */
  virtual StringRef get_view_name() = 0;

  /* Set an info message. This is called by the compositor evaluator to inform or warn the user
   * about something, typically an error. The implementation should display the message in an
   * appropriate place, which can be directly in the UI or just logged to the output stream. */
  virtual void set_info_message(StringRef message) const = 0;

  /* Returns the ID recalculate flag of the given ID and reset it to zero. The given ID is assumed
   * to be one that has a DrawDataList and conforms to the IdDdtTemplate.
   *
   * The ID recalculate flag is a mechanism through which one can identify if an ID has changed
   * since the last time the flag was reset, hence why the method reset the flag after querying it,
   * that is, to ready it to track the next change. */
  virtual IDRecalcFlag query_id_recalc_flag(ID *id) const = 0;

  /* Get the size of the compositing region. See get_compositing_region(). */
  int2 get_compositing_region_size() const;

  /* Get the normalized render percentage of the active scene. */
  float get_render_percentage() const;

  /* Get the current frame number of the active scene. */
  int get_frame_number() const;

  /* Get the current time in seconds of the active scene. */
  float get_time() const;

  /* Get a reference to the texture pool of this context. */
  TexturePool &texture_pool();

  /* Get a reference to the static shader manager of this context. */
  StaticShaderManager &shader_manager();

  /* Get a reference to the static cache manager of this context. */
  StaticCacheManager &cache_manager();
};

}  // namespace blender::realtime_compositor
