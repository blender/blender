/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_vector_types.hh"
#include "BLI_string_ref.hh"

#include "DNA_ID.h"
#include "DNA_scene_types.h"
#include "DNA_vec_types.h"

#include "GPU_shader.h"
#include "GPU_texture.h"

#include "COM_domain.hh"
#include "COM_render_context.hh"
#include "COM_result.hh"
#include "COM_static_cache_manager.hh"
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
 * Finally, the class have an instance of a static resource manager for acquiring cached resources
 * efficiently. */
class Context {
 private:
  /* A texture pool that can be used to allocate textures for the compositor efficiently. */
  TexturePool &texture_pool_;
  /* A static cache manager that can be used to acquire cached resources for the compositor
   * efficiently. */
  StaticCacheManager cache_manager_;

 public:
  Context(TexturePool &texture_pool);

  /* Get the compositing scene. */
  virtual const Scene &get_scene() const = 0;

  /* Get the node tree used for compositing. */
  virtual const bNodeTree &get_node_tree() const = 0;

  /* True if the compositor should write file outputs, false otherwise. */
  virtual bool use_file_output() const = 0;

  /* True if the compositor should write the composite output, otherwise, the compositor is assumed
   * to not support the composite output and just displays its viewer output. In that case, the
   * composite output will be used as a fallback viewer if no other viewer exists */
  virtual bool use_composite_output() const = 0;

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
   * region. Callers should check the validity of the region through is_valid_compositing_region(),
   * since the region can be zero sized. */
  virtual rcti get_compositing_region() const = 0;

  /* Get the texture where the result of the compositor should be written. This should be called by
   * the composite output node to get its target texture. */
  virtual GPUTexture *get_output_texture() = 0;

  /* Get the texture where the result of the compositor viewer should be written, given the domain
   * of the result to be viewed. This should be called by viewer output nodes to get their target
   * texture. */
  virtual GPUTexture *get_viewer_output_texture(Domain domain) = 0;

  /* Get the texture where the given render pass is stored. This should be called by the Render
   * Layer node to populate its outputs. */
  virtual GPUTexture *get_input_texture(const Scene *scene,
                                        int view_layer,
                                        const char *pass_name) = 0;

  /* Get the name of the view currently being rendered. */
  virtual StringRef get_view_name() = 0;

  /* Get the precision of the intermediate results of the compositor. */
  virtual ResultPrecision get_precision() const = 0;

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

  /* Get a pointer to the render context of this context. A render context stores information about
   * the current render. It might be null if the compositor is not being evaluated as part of a
   * render pipeline. */
  virtual RenderContext *render_context() const;

  /* Get the size of the compositing region. See get_compositing_region(). The output size is
   * sanitized such that it is at least 1 in both dimensions. However, the developer is expected to
   * gracefully handled zero sizes regions by checking the is_valid_compositing_region method. */
  int2 get_compositing_region_size() const;

  /* Returns true if the compositing region has a valid size, that is, has at least one pixel in
   * both dimensions, returns false otherwise. */
  bool is_valid_compositing_region() const;

  /* Get the normalized render percentage of the active scene. */
  float get_render_percentage() const;

  /* Get the current frame number of the active scene. */
  int get_frame_number() const;

  /* Get the current time in seconds of the active scene. */
  float get_time() const;

  /* Get a GPU shader with the given info name and precision. */
  GPUShader *get_shader(const char *info_name, ResultPrecision precision);

  /* Get a GPU shader with the given info name and context's precision. */
  GPUShader *get_shader(const char *info_name);

  /* Create a result of the given type and precision using the context's texture pool. */
  Result create_result(ResultType type, ResultPrecision precision);

  /* Create a result of the given type using the context's texture pool and precision. */
  Result create_result(ResultType type);

  /* Create a temporary result of the given type and precision using the context's texture pool.
   * See Result::Temporary for more information. */
  Result create_temporary_result(ResultType type, ResultPrecision precision);

  /* Create a temporary result of the given type using the context's texture pool and precision.
   * See Result::Temporary for more information. */
  Result create_temporary_result(ResultType type);

  /* Get a reference to the texture pool of this context. */
  TexturePool &texture_pool();

  /* Get a reference to the static cache manager of this context. */
  StaticCacheManager &cache_manager();
};

}  // namespace blender::realtime_compositor
