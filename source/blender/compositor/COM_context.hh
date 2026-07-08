/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_bounds_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_string_ref.hh"

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "GPU_shader.hh"

#include "BKE_compute_context_cache.hh"

#include "COM_domain.hh"
#include "COM_meta_data.hh"
#include "COM_render_context.hh"
#include "COM_result.hh"
#include "COM_static_cache_manager.hh"

namespace blender {
struct Main;
}  // namespace blender

namespace blender::nodes::eval_log {
class NodesEvalLog;
}  // namespace blender::nodes::eval_log

namespace blender::compositor {

/* ------------------------------------------------------------------------------------------------
 * Context
 *
 * A Context is an abstract class that is implemented by the caller of the evaluator to provide the
 * necessary data and functionalities for the correct operation of the evaluator. This includes
 * providing input data like render passes and the active scene, as well as callbacks to write the
 * outputs of the compositor. Finally, the class have a reference to a static resource manager for
 * acquiring cached resources efficiently. */
class Context {
 private:
  /* A static cache manager that can be used to acquire cached resources for the compositor
   * efficiently. */
  StaticCacheManager &cache_manager_;

 public:
  Context(StaticCacheManager &cache_manager);

  virtual const Main &get_main() const = 0;

  /* Get the compositing scene. */
  virtual const Scene &get_scene() const = 0;

  /* Returns the domain that the inputs and outputs of the context will be in. */
  virtual Domain get_compositing_domain() const = 0;

  /* Write the result of the compositor viewer. */
  virtual void write_viewer(Result &viewer_result) = 0;

  /* True if the compositor should use GPU acceleration. */
  virtual bool use_gpu() const = 0;

  /* Returns the hash of the currently active compute context. */
  virtual const ComputeContextHash &get_active_compute_context_hash() const = 0;

  /* Get the strip that the compositing modifier is applied to. */
  virtual const Strip *get_strip() const;

  /* Get the pass with the given name in the given view layer and scene. Freeing the pass is the
   * caller's responsibility. */
  virtual Result get_pass(const Scene *scene, int view_layer, const char *name);

  /* Get the render settings for compositing. This could be different from scene->r render settings
   * in case the render size or other settings needs to be overwritten. */
  virtual const RenderData &get_render_data() const;

  /* Get the name of the view currently being rendered. If the context is not multi-view, return an
   * empty string. */
  virtual StringRef get_view_name() const;

  /* Get the precision of the intermediate results of the compositor. */
  virtual ResultPrecision get_precision() const;

  /* Set an info message. This is called by the compositor evaluator to inform or warn the user
   * about something, typically an error. The implementation should display the message in an
   * appropriate place, which can be directly in the UI or just logged to the output stream. */
  virtual void set_info_message(StringRef message) const;

  /* Populates the given meta data from the render stamp information of the given render pass. */
  virtual void populate_meta_data_for_pass(const Scene *scene,
                                           int view_layer_id,
                                           const char *pass_name,
                                           MetaData &meta_data) const;

  /* Get a pointer to the render context of this context. A render context stores information about
   * the current render. It might be null if the compositor is not being evaluated as part of a
   * render pipeline. */
  virtual RenderContext *render_context() const;

  /* Returns a pointer to a nodes evaluation log of the context, this can be nullptr for context
   * that does not support logging. */
  virtual nodes::eval_log::NodesEvalLog *nodes_evaluation_log() const;

  /* Gets called after the evaluation of each compositor operation. See overrides for possible
   * uses. */
  virtual void evaluate_operation_post() const;

  /* Returns true if the compositor evaluation is canceled and that the evaluator should stop
   * executing as soon as possible. */
  virtual bool is_canceled() const;

  /* Get the normalized render percentage of the active scene. */
  float get_render_percentage() const;

  /* Get the current frame number of the active scene. */
  int get_frame_number() const;

  /* Get the current time in seconds of the active scene. */
  float get_time() const;

  /* Get the OIDN denoiser quality which should be used if the user doesn't explicitly set
   * denoising quality on a node. */
  eCompositorDenoiseQaulity get_denoise_quality() const;

  /* Get a GPU shader with the given info name and precision. */
  gpu::Shader *get_shader(const char *info_name, ResultPrecision precision);

  /* Get a GPU shader with the given info name and context's precision. */
  gpu::Shader *get_shader(const char *info_name);

  /* Create a result of the given type and precision. */
  Result create_result(ResultType type, ResultPrecision precision);

  /* Create a result of the given type using the context's precision. */
  Result create_result(ResultType type);

  /* Get a reference to the static cache manager of this context. */
  StaticCacheManager &cache_manager();
};

}  // namespace blender::compositor
