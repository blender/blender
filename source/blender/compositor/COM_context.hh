/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>

#include "BLI_bounds_types.hh"
#include "BLI_enum_flags.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_string_ref.hh"

#include "DNA_scene_types.h"

#include "DNA_sequence_types.h"
#include "GPU_shader.hh"

#include "COM_domain.hh"
#include "COM_meta_data.hh"
#include "COM_profiler.hh"
#include "COM_render_context.hh"
#include "COM_result.hh"
#include "COM_static_cache_manager.hh"

namespace blender::compositor {

/* Enumerates the possible outputs that the compositor can compute. */
enum class OutputTypes : uint8_t {
  None = 0,
  Composite = 1 << 0,
  Viewer = 1 << 1,
  FileOutput = 1 << 2,
  Previews = 1 << 3,
};
ENUM_OPERATORS(OutputTypes)

/* ------------------------------------------------------------------------------------------------
 * Context
 *
 * A Context is an abstract class that is implemented by the caller of the evaluator to provide the
 * necessary data and functionalities for the correct operation of the evaluator. This includes
 * providing input data like render passes and the active scene, as well as references to the data
 * where the output of the evaluator will be written. Finally, the class have an instance of a
 * static resource manager for acquiring cached resources efficiently. */
class Context {
 private:
  /* A static cache manager that can be used to acquire cached resources for the compositor
   * efficiently. */
  StaticCacheManager cache_manager_;

 public:
  /* Get the compositing scene. */
  virtual const Scene &get_scene() const = 0;

  /* Get the node tree used for compositing. */
  virtual const bNodeTree &get_node_tree() const = 0;

  /* Returns all output types that should be computed. */
  virtual OutputTypes needed_outputs() const = 0;

  /* Get the rectangular region representing the area of the input that the compositor will operate
   * on. Conversely, the compositor will only update the region of the output that corresponds to
   * the compositing region. In the base case, the compositing region covers the entirety of the
   * render region. In other cases, the compositing region might be a subset of the render region.
   * Callers should check the validity of the region through is_valid_compositing_region(), since
   * the region can be zero sized. */
  virtual Bounds<int2> get_compositing_region() const = 0;

  /* Get the result where the result of the compositor should be written. */
  virtual Result get_output(Domain domain) = 0;

  /* Get the result where the result of the compositor viewer should be written, given the domain
   * of the result to be viewed, its precision, and whether the output is a non-color data image
   * that should be displayed without view transform. */
  virtual Result get_viewer_output(Domain domain, bool is_data, ResultPrecision precision) = 0;

  /* Get the result where the given input is stored. */
  virtual Result get_input(StringRef name) = 0;

  /* True if the compositor should use GPU acceleration. */
  virtual bool use_gpu() const = 0;

  /* Get the strip that the compositing modifier is applied to. */
  virtual const Strip *get_strip() const;

  /* Get the result where the given pass is stored. */
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

  /* True if the compositor should treat viewers as composite outputs because it has no concept of
   * or support for viewers. */
  virtual bool treat_viewer_as_compositor_output() const;

  /* True if the compositor input/output should use output region/bounds setup in the context. */
  virtual bool use_context_bounds_for_input_output() const
  {
    return true;
  }

  /* Populates the given meta data from the render stamp information of the given render pass. */
  virtual void populate_meta_data_for_pass(const Scene *scene,
                                           int view_layer_id,
                                           const char *pass_name,
                                           MetaData &meta_data) const;

  /* Get a pointer to the render context of this context. A render context stores information about
   * the current render. It might be null if the compositor is not being evaluated as part of a
   * render pipeline. */
  virtual RenderContext *render_context() const;

  /* Get a pointer to the profiler of this context. It might be null if the compositor context does
   * not support profiling. */
  virtual Profiler *profiler() const;

  /* Gets called after the evaluation of each compositor operation. See overrides for possible
   * uses. */
  virtual void evaluate_operation_post() const;

  /* Returns true if the compositor evaluation is canceled and that the evaluator should stop
   * executing as soon as possible. */
  virtual bool is_canceled() const;

  /* Resets the context's internal structures like the cache manager. This should be called before
   * every evaluation. */
  void reset();

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
