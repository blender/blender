/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_bounds.hh"
#include "BLI_listbase.h"
#include "BLI_math_vector_types.hh"
#include "BLI_string_ref.hh"
#include "BLI_utildefines.h"

#include "DNA_layer_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_vec_types.h"
#include "DNA_view3d_types.h"

#include "BKE_node.hh"
#include "BKE_node_runtime.hh"

#include "DEG_depsgraph_query.hh"

#include "ED_view3d.hh"

#include "DRW_gpu_wrapper.hh"
#include "DRW_render.hh"

#include "COM_context.hh"
#include "COM_domain.hh"
#include "COM_node_group_operation.hh"
#include "COM_realize_on_domain_operation.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

#include "GPU_context.hh"
#include "GPU_state.hh"
#include "GPU_texture.hh"

#include "draw_view_data.hh"

#include "compositor_engine.h" /* Own include. */

namespace blender::draw::compositor_engine {

class Context : public compositor::Context {
 private:
  const Scene *scene_;
  /* A pointer to the info message of the compositor engine. This is a char array of size
   * GPU_INFO_SIZE. The message is cleared prior to updating or evaluating the compositor. */
  char *info_message_;
  /* Identified if the output of the viewer was written. */
  bool viewer_was_written_ = false;

 public:
  Context(compositor::StaticCacheManager &cache_manager, const Scene *scene, char *info_message)
      : compositor::Context(cache_manager), scene_(scene), info_message_(info_message)
  {
    this->set_info_message("");
  }

  const Scene &get_scene() const override
  {
    return *scene_;
  }

  bool use_gpu() const override
  {
    return true;
  }

  /* The viewport compositor does not support viewer outputs, so treat viewers as composite
   * outputs. */
  bool treat_viewer_as_group_output() const override
  {
    return true;
  }

  /* In case the viewport has no camera region or is an image render, the domain covers the entire
   * viewport. But in case the camera region is not entirely visible in the viewport, the data size
   * of the domain will only cover the intersection of the viewport and the camera regions, while
   * the display size will cover the virtual extension of the camera region. */
  compositor::Domain get_compositing_domain() const override
  {
    const DRWContext *draw_ctx = DRW_context_get();

    /* No camera region or is a viewport render, the domain is the entire viewport. */
    if (draw_ctx->rv3d->persp != RV3D_CAMOB || draw_ctx->is_viewport_image_render()) {
      return compositor::Domain(int2(draw_ctx->viewport_size_get()));
    }

    rctf camera_border;
    ED_view3d_calc_camera_border(draw_ctx->scene,
                                 draw_ctx->depsgraph,
                                 draw_ctx->region,
                                 draw_ctx->v3d,
                                 draw_ctx->rv3d,
                                 false,
                                 &camera_border);

    const Bounds<int2> camera_region = Bounds<int2>(
        int2(int(camera_border.xmin), int(camera_border.ymin)),
        int2(int(camera_border.xmax), int(camera_border.ymax)));

    const Bounds<int2> render_region = Bounds<int2>(int2(0), int2(draw_ctx->viewport_size_get()));
    const Bounds<int2> border_region = bounds::intersect(render_region, camera_region).value();

    compositor::Domain domain = compositor::Domain(camera_region.size());
    domain.data_size = border_region.size();
    domain.data_offset = border_region.min - camera_region.min;
    return domain;
  }

  /* Get the bounds of the camera region in pixels relative to the viewport. In case the viewport
   * has no camera region or is an image render, return the bounds of the entire viewport. */
  Bounds<int2> get_camera_region() const
  {
    const DRWContext *draw_ctx = DRW_context_get();
    const int2 viewport_size = int2(draw_ctx->viewport_size_get());
    const Bounds<int2> render_region = Bounds<int2>(int2(0), viewport_size);

    /* No camera region or is a viewport render, the domain is the entire viewport. */
    if (draw_ctx->rv3d->persp != RV3D_CAMOB || draw_ctx->is_viewport_image_render()) {
      return render_region;
    }

    rctf camera_border;
    ED_view3d_calc_camera_border(draw_ctx->scene,
                                 draw_ctx->depsgraph,
                                 draw_ctx->region,
                                 draw_ctx->v3d,
                                 draw_ctx->rv3d,
                                 false,
                                 &camera_border);

    const Bounds<int2> camera_region = Bounds<int2>(
        int2(int(camera_border.xmin), int(camera_border.ymin)),
        int2(int(camera_border.xmax), int(camera_border.ymax)));

    return bounds::intersect(render_region, camera_region).value_or(Bounds<int2>(int2(0)));
  }

  void write_output(const compositor::Result &result)
  {
    /* Do not write the output if the viewer output was already written. */
    if (viewer_was_written_) {
      return;
    }

    gpu::Texture *output = DRW_context_get()->viewport_texture_list_get()->color;
    if (result.is_single_value()) {
      GPU_texture_clear(output, GPU_DATA_FLOAT, result.get_single_value<compositor::Color>());
      return;
    }

    gpu::Shader *shader = this->get_shader("compositor_write_output",
                                           compositor::ResultPrecision::Half);
    GPU_shader_bind(shader);

    const Bounds<int2> bounds = this->get_camera_region();
    GPU_shader_uniform_2iv(shader, "lower_bound", bounds.min);
    GPU_shader_uniform_2iv(shader, "upper_bound", bounds.max);

    result.bind_as_texture(shader, "input_tx");

    const int image_unit = GPU_shader_get_sampler_binding(shader, "output_img");
    GPU_texture_image_bind(output, image_unit);

    compositor::compute_dispatch_threads_at_least(shader, result.domain().data_size);

    result.unbind_as_texture();
    GPU_texture_image_unbind(output);
    GPU_shader_unbind();
  }

  void write_viewer(compositor::Result &viewer_result) override
  {
    using namespace compositor;

    /* Realize the on the compositing domain if needed. */
    const Domain compositing_domain = this->get_compositing_domain();
    const InputDescriptor input_descriptor = {ResultType::Color,
                                              InputRealizationMode::OperationDomain};
    SimpleOperation *realization_operation = RealizeOnDomainOperation::construct_if_needed(
        *this, viewer_result, input_descriptor, compositing_domain);

    if (realization_operation) {
      Result realize_input = this->create_result(ResultType::Color, viewer_result.precision());
      realize_input.wrap_external(viewer_result);
      realization_operation->map_input_to_result(&realize_input);
      realization_operation->evaluate();

      Result &realized_viewer_result = realization_operation->get_result();
      this->write_output(realized_viewer_result);
      realized_viewer_result.release();
      viewer_was_written_ = true;
      delete realization_operation;
      return;
    }

    this->write_output(viewer_result);
    viewer_was_written_ = true;
  }

  compositor::Result get_invalid_pass()
  {
    compositor::Result invalid_pass = this->create_result(compositor::ResultType::Color);
    invalid_pass.allocate_invalid();
    return invalid_pass;
  }

  /* Get the pass that corresponds to the given pass name. If no pass with the given name exists,
   * returns an unallocated result instead. */
  compositor::Result get_pass_result(const char *pass_name)
  {
    /* The combined pass is a special case where we return the viewport color texture, because it
     * includes Grease Pencil objects since GP is drawn using their own engine. */
    if (STREQ(pass_name, RE_PASSNAME_COMBINED)) {
      gpu::Texture *combined_texture = DRW_context_get()->viewport_texture_list_get()->color;
      compositor::Result pass = compositor::Result(*this, GPU_texture_format(combined_texture));
      pass.wrap_external(combined_texture);
      return pass;
    }

    /* Return the pass that was written by the engine if such pass was found. */
    gpu::Texture *pass_texture = DRW_viewport_pass_texture_get(pass_name).gpu_texture();
    if (pass_texture) {
      compositor::Result pass = compositor::Result(*this, GPU_texture_format(pass_texture));
      pass.wrap_external(pass_texture);
      return pass;
    }

    return this->create_result(compositor::ResultType::Color);
  }

  compositor::Result crop_pass(const compositor::Result &pass)
  {
    const char *shader_name = pass.type() == compositor::ResultType::Float ?
                                  "compositor_image_crop_float" :
                                  "compositor_image_crop_float4";
    gpu::Shader *shader = this->get_shader(shader_name, pass.precision());
    GPU_shader_bind(shader);

    /* The compositing space is limited to a subset of the pass texture, so only read that
     * compositing region into an appropriately sized result. */
    const int2 lower_bound = this->get_camera_region().min;
    GPU_shader_uniform_2iv(shader, "lower_bound", lower_bound);

    pass.bind_as_texture(shader, "input_tx");

    compositor::Result cropped_pass = this->create_result(pass.type(), pass.precision());
    cropped_pass.allocate_texture(this->get_compositing_domain());
    cropped_pass.bind_as_image(shader, "output_img");

    compositor::compute_dispatch_threads_at_least(shader, cropped_pass.domain().data_size);

    GPU_shader_unbind();
    pass.unbind_as_texture();
    cropped_pass.unbind_as_image();

    return cropped_pass;
  }

  compositor::Result get_pass(const Scene *scene, int view_layer_index, const char *name) override
  {
    /* Blender aliases the Image pass name to be the Combined pass, so we return the combined pass
     * in that case. */
    const char *pass_name = StringRef(name) == "Image" ? "Combined" : name;

    const Scene *original_scene = DEG_get_original(scene_);
    if (DEG_get_original(scene) != original_scene) {
      return this->get_invalid_pass();
    }

    ViewLayer *view_layer = static_cast<ViewLayer *>(
        BLI_findlink(&original_scene->view_layers, view_layer_index));
    if (StringRef(view_layer->name) != DRW_context_get()->view_layer->name) {
      return this->get_invalid_pass();
    }

    const compositor::Result pass = this->get_pass_result(pass_name);
    if (!pass.is_allocated()) {
      return this->get_invalid_pass();
    }

    /* The pass matches the compositing domain, return it as is. */
    const compositor::Domain compositing_domain = this->get_compositing_domain();
    if (this->get_camera_region().min == int2(0) &&
        compositing_domain.data_size == pass.domain().data_size)
    {
      return pass;
    }

    return this->crop_pass(pass);
  }

  StringRef get_view_name() const override
  {
    const SceneRenderView *view = static_cast<SceneRenderView *>(
        BLI_findlink(&get_render_data().views, DRW_context_get()->v3d->multiview_eye));
    return view->name;
  }

  compositor::ResultPrecision get_precision() const override
  {
    switch (get_scene().r.compositor_precision) {
      case SCE_COMPOSITOR_PRECISION_AUTO:
        return compositor::ResultPrecision::Half;
      case SCE_COMPOSITOR_PRECISION_FULL:
        return compositor::ResultPrecision::Full;
    }

    BLI_assert_unreachable();
    return compositor::ResultPrecision::Half;
  }

  void set_info_message(StringRef message) const override
  {
    message.copy_utf8_truncated(info_message_, GPU_INFO_SIZE);
  }

  compositor::NodeGroupOutputTypes needed_outputs() const
  {
    return compositor::NodeGroupOutputTypes::GroupOutputNode |
           compositor::NodeGroupOutputTypes::ViewerNode;
  }

  void evaluate()
  {
    using namespace compositor;
    const bNodeTree &node_group = *DRW_context_get()->scene->compositing_node_group;
    NodeGroupOperation node_group_operation(*this,
                                            node_group,
                                            this->needed_outputs(),
                                            nullptr,
                                            node_group.active_viewer_key,
                                            bke::NODE_INSTANCE_KEY_BASE);

    /* Set the reference count for the outputs, only the first color output is actually needed,
     * while the rest are ignored. */
    node_group.ensure_interface_cache();
    for (const bNodeTreeInterfaceSocket *output_socket : node_group.interface_outputs()) {
      const bool is_fisrt_output = output_socket == node_group.interface_outputs().first();
      Result &output_result = node_group_operation.get_result(output_socket->identifier);
      const bool is_color = output_result.type() == ResultType::Color;
      output_result.set_reference_count(is_fisrt_output && is_color ? 1 : 0);
    }

    /* Map the inputs to the operation. */
    Vector<std::unique_ptr<Result>> inputs;
    for (const bNodeTreeInterfaceSocket *input_socket : node_group.interface_inputs()) {
      Result *input_result = new Result(
          this->create_result(ResultType::Color, ResultPrecision::Half));
      if (input_socket == node_group.interface_inputs()[0]) {
        /* First socket is the viewport combined pass. */
        gpu::Texture *combined_texture = DRW_context_get()->viewport_texture_list_get()->color;
        input_result->wrap_external(combined_texture);
      }
      else {
        /* The rest of the sockets are not supported. */
        input_result->allocate_invalid();
      }

      node_group_operation.map_input_to_result(input_socket->identifier, input_result);
      inputs.append(std::unique_ptr<Result>(input_result));
    }

    node_group_operation.evaluate();

    /* Write the outputs of the operation. */
    for (const bNodeTreeInterfaceSocket *output_socket : node_group.interface_outputs()) {
      Result &output_result = node_group_operation.get_result(output_socket->identifier);
      if (!output_result.should_compute()) {
        continue;
      }

      /* Realize the output on the compositing domain if needed. */
      const Domain compositing_domain = this->get_compositing_domain();
      const InputDescriptor input_descriptor = {ResultType::Color,
                                                InputRealizationMode::OperationDomain};
      SimpleOperation *realization_operation = RealizeOnDomainOperation::construct_if_needed(
          *this, output_result, input_descriptor, compositing_domain);
      if (realization_operation) {
        realization_operation->map_input_to_result(&output_result);
        realization_operation->evaluate();
        Result &realized_output_result = realization_operation->get_result();
        this->write_output(realized_output_result);
        realized_output_result.release();
        delete realization_operation;
        continue;
      }

      this->write_output(output_result);
      output_result.release();
    }
  }
};

class Instance : public DrawEngine {
 private:
  compositor::StaticCacheManager cache_manager_;

 public:
  StringRefNull name_get() final
  {
    return "Compositor";
  }

  void init() final {};
  void begin_sync() final {};
  void object_sync(draw::ObjectRef & /*ob_ref*/, draw::Manager & /*manager*/) final {};
  void end_sync() final {};

  void draw(Manager & /*manager*/) final
  {
    Context context(cache_manager_, DRW_context_get()->scene, this->info);
    if (context.get_camera_region().is_empty()) {
      return;
    }

    DRW_submission_start();

#if defined(__APPLE__)
    if (GPU_backend_get_type() == GPU_BACKEND_METAL) {
      /* NOTE(Metal): Isolate Compositor compute work in individual command buffer to improve
       * workload scheduling. When expensive compositor nodes are in the graph, these can stall out
       * the GPU for extended periods of time and sub-optimally schedule work for execution. */
      GPU_flush();
    }
#endif

    context.evaluate();
    context.cache_manager().reset();

#if defined(__APPLE__)
    /* NOTE(Metal): Following previous flush to break command stream, with compositor command
     * buffers potentially being heavy, we avoid issuing subsequent commands until compositor work
     * has completed. If subsequent work is prematurely queued up, the subsequent command buffers
     * will be blocked behind compositor work and may trigger a command buffer time-out error. As a
     * result, we should wait for compositor work to complete.
     *
     * This is not an efficient approach for peak performance, but a catch-all to prevent command
     * buffer failure, until the offending cases can be resolved. */
    if (GPU_backend_get_type() == GPU_BACKEND_METAL) {
      GPU_finish();
    }
#endif
    DRW_submission_end();
  }
};

DrawEngine *Engine::create_instance()
{
  return new Instance();
}

}  // namespace blender::draw::compositor_engine
