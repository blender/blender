/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstring>
#include <string>

#include "BLI_listbase.h"
#include "BLI_math_vector_types.hh"
#include "BLI_memory_utils.hh"
#include "BLI_threads.h"
#include "BLI_vector.hh"

#include "MEM_guardedalloc.h"

#include "DNA_node_types.h"

#include "BKE_cryptomatte.hh"
#include "BKE_global.hh"
#include "BKE_image.hh"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_scene.hh"

#include "DRW_engine.hh"
#include "DRW_render.hh"

#include "IMB_imbuf.hh"

#include "COM_context.hh"
#include "COM_conversion_operation.hh"
#include "COM_domain.hh"
#include "COM_node_group_operation.hh"
#include "COM_realize_on_domain_operation.hh"
#include "COM_render_context.hh"
#include "COM_result.hh"

#include "RE_compositor.hh"
#include "RE_pipeline.h"

#include "WM_api.hh"

#include "GPU_context.hh"
#include "GPU_state.hh"
#include "GPU_texture_pool.hh"

#include "render_types.h"

namespace blender {

namespace render {

/**
 * Render Context Data
 *
 * Stored separately from the context so we can update it without losing any cached
 * data from the context.
 */
class ContextInputData {
 public:
  const Render *render;
  const Scene *scene;
  const RenderData *render_data;
  const bNodeTree *node_tree;
  std::string view_name;
  compositor::RenderContext *render_context;
  compositor::Profiler *profiler;
  compositor::NodeGroupOutputTypes needed_outputs;

  ContextInputData(const Render *render,
                   const Scene &scene,
                   const RenderData &render_data,
                   const bNodeTree &node_tree,
                   const char *view_name,
                   compositor::RenderContext *render_context,
                   compositor::Profiler *profiler,
                   compositor::NodeGroupOutputTypes needed_outputs)
      : render(render),
        scene(&scene),
        render_data(&render_data),
        node_tree(&node_tree),
        view_name(view_name),
        render_context(render_context),
        profiler(profiler),
        needed_outputs(needed_outputs)
  {
  }
};

/* Render Context Data */

class Context : public compositor::Context {
 private:
  /* Input data. */
  ContextInputData input_data_;

  /* Cached GPU and CPU passes that the compositor took ownership of. Those had their reference
   * count incremented when accessed and need to be freed/have their reference count decremented
   * when destroying the context. */
  Vector<gpu::Texture *> cached_gpu_passes_;
  Vector<ImBuf *> cached_cpu_passes_;

 public:
  Context(compositor::StaticCacheManager &cache_manager, const ContextInputData &input_data)
      : compositor::Context(cache_manager), input_data_(input_data)
  {
  }

  virtual ~Context()
  {
    for (gpu::Texture *pass : cached_gpu_passes_) {
      GPU_texture_free(pass);
    }
    for (ImBuf *pass : cached_cpu_passes_) {
      IMB_freeImBuf(pass);
    }
  }

  const Scene &get_scene() const override
  {
    return *input_data_.scene;
  }

  bool use_gpu() const override
  {
    return this->get_render_data().compositor_device == SCE_COMPOSITOR_DEVICE_GPU;
  }

  compositor::NodeGroupOutputTypes needed_outputs() const
  {
    return input_data_.needed_outputs;
  }

  const RenderData &get_render_data() const override
  {
    return *(input_data_.render_data);
  }

  int2 get_render_size() const
  {
    Render *render = RE_GetSceneRender(input_data_.scene);
    RenderResult *render_result = RE_AcquireResultRead(render);

    /* If a render result already exist, use its size, since the compositor operates on the render
     * settings at which the render happened. Otherwise, use the size from the render data. */
    int2 size;
    if (render_result) {
      size = int2(render_result->rectx, render_result->recty);
    }
    else {
      BKE_render_resolution(input_data_.render_data, true, &size.x, &size.y);
    }

    RE_ReleaseResult(render);

    return size;
  }

  compositor::Domain get_compositing_domain() const override
  {
    return compositor::Domain(this->get_render_size());
  }

  void write_output(const compositor::Result &result)
  {
    Render *render = RE_GetSceneRender(input_data_.scene);
    RenderResult *render_result = RE_AcquireResultWrite(render);

    if (render_result) {
      RenderView *render_view = RE_RenderViewGetByName(render_result,
                                                       input_data_.view_name.c_str());
      ImBuf *image_buffer = RE_RenderViewEnsureImBuf(render_result, render_view);
      render_result->have_combined = true;

      if (result.is_single_value()) {
        float *data = MEM_new_array_uninitialized<float>(
            4 * size_t(render_result->rectx) * size_t(render_result->recty), __func__);
        IMB_assign_float_buffer(image_buffer, data, IB_TAKE_OWNERSHIP);
        IMB_rectfill(image_buffer, result.get_single_value<compositor::Color>());
      }
      else if (this->use_gpu()) {
        GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
        float *output_buffer = static_cast<float *>(GPU_texture_read(result, GPU_DATA_FLOAT, 0));
        IMB_assign_float_buffer(image_buffer, output_buffer, IB_TAKE_OWNERSHIP);
      }
      else {
        float *data = MEM_new_array_uninitialized<float>(
            4 * size_t(render_result->rectx) * size_t(render_result->recty), __func__);
        IMB_assign_float_buffer(image_buffer, data, IB_TAKE_OWNERSHIP);
        std::memcpy(image_buffer->float_buffer.data,
                    result.cpu_data().data(),
                    render_result->rectx * render_result->recty * 4 * sizeof(float));
      }
    }
    RE_ReleaseResult(render);

    Image *image = BKE_image_ensure_viewer(G.main, IMA_TYPE_R_RESULT, "Render Result");
    BKE_image_partial_update_mark_full_update(image);
    BLI_thread_lock(LOCK_DRAW_IMAGE);
    BKE_image_signal(G.main, image, nullptr, IMA_SIGNAL_FREE);
    BLI_thread_unlock(LOCK_DRAW_IMAGE);
  }

  void write_viewer_image(const compositor::Result &viewer_result)
  {
    Image *image = BKE_image_ensure_viewer(G.main, IMA_TYPE_COMPOSITE, "Viewer Node");

    if (viewer_result.meta_data.is_non_color_data) {
      image->flag &= ~IMA_VIEW_AS_RENDER;
    }
    else {
      image->flag |= IMA_VIEW_AS_RENDER;
    }

    ImageUser image_user = {nullptr};
    image_user.multi_index = BKE_scene_multiview_view_id_get(input_data_.render_data,
                                                             input_data_.view_name.c_str());

    if (BKE_scene_multiview_is_render_view_first(input_data_.render_data,
                                                 input_data_.view_name.c_str()))
    {
      BKE_image_ensure_viewer_views(input_data_.render_data, image, &image_user);
    }

    BLI_thread_lock(LOCK_DRAW_IMAGE);

    void *lock;
    ImBuf *image_buffer = BKE_image_acquire_ibuf(image, &image_user, &lock);

    const int2 size = viewer_result.is_single_value() ? this->get_render_size() :
                                                        viewer_result.domain().data_size;
    if (image_buffer->x != size.x || image_buffer->y != size.y) {
      IMB_free_byte_pixels(image_buffer);
      IMB_free_float_pixels(image_buffer);
      image_buffer->x = size.x;
      image_buffer->y = size.y;
      IMB_alloc_float_pixels(image_buffer, 4, false);
      image_buffer->userflags |= IB_DISPLAY_BUFFER_INVALID;
    }

    if (!viewer_result.is_single_value()) {
      image_buffer->flags |= IB_has_display_window;
      const int2 display_offset = int2(viewer_result.domain().transformation.location());
      copy_v2_v2_int(image_buffer->display_size, viewer_result.domain().display_size);
      copy_v2_v2_int(image_buffer->display_offset, display_offset);
      copy_v2_v2_int(image_buffer->data_offset, viewer_result.domain().data_offset);
    }

    if (viewer_result.is_single_value()) {
      IMB_rectfill(image_buffer, viewer_result.get_single_value<compositor::Color>());
    }
    else if (this->use_gpu()) {
      GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
      float *output_buffer = static_cast<float *>(
          GPU_texture_read(viewer_result, GPU_DATA_FLOAT, 0));
      IMB_assign_float_buffer(image_buffer, output_buffer, IB_TAKE_OWNERSHIP);
    }
    else {
      std::memcpy(image_buffer->float_buffer.data,
                  viewer_result.cpu_data().data(),
                  size.x * size.y * 4 * sizeof(float));
    }

    BKE_image_partial_update_mark_full_update(image);
    BKE_image_release_ibuf(image, image_buffer, lock);
    BLI_thread_unlock(LOCK_DRAW_IMAGE);
  }

  void write_viewer(compositor::Result &viewer_result) override
  {
    using namespace compositor;

    /* Realize the transforms if needed. */
    const InputDescriptor input_descriptor = {ResultType::Color,
                                              InputRealizationMode::OperationDomain};
    SimpleOperation *realization_operation = RealizeOnDomainOperation::construct_if_needed(
        *this, viewer_result, input_descriptor, viewer_result.domain());

    if (realization_operation) {
      Result realize_input = this->create_result(ResultType::Color, viewer_result.precision());
      realize_input.wrap_external(viewer_result);
      realization_operation->map_input_to_result(&realize_input);
      realization_operation->evaluate();

      Result &realized_viewer_result = realization_operation->get_result();
      this->write_viewer_image(realized_viewer_result);
      realized_viewer_result.release();
      delete realization_operation;
      return;
    }

    this->write_viewer_image(viewer_result);
  }

  compositor::ResultType get_pass_data_type(const RenderPass *pass)
  {
    switch (pass->channels) {
      case 1:
        return compositor::ResultType::Float;
      case 2:
        return compositor::ResultType::Float2;
      case 3:
        return compositor::ResultType::Float3;
      case 4:
        if (StringRef(pass->chan_id) == "XYZW") {
          return compositor::ResultType::Float4;
        }
        else {
          return compositor::ResultType::Color;
        }
      default:
        break;
    }

    BLI_assert_unreachable();
    return compositor::ResultType::Float;
  }

  compositor::ResultType get_pass_type(const RenderPass *pass)
  {
    switch (pass->channels) {
      case 1:
        return compositor::ResultType::Float;
      case 2:
        return compositor::ResultType::Float2;
      case 3:
        if (StringRef(pass->chan_id) == "RGB") {
          return compositor::ResultType::Color;
        }
        else {
          return compositor::ResultType::Float3;
        }
      case 4:
        if (StringRef(pass->chan_id) == "XYZW") {
          return compositor::ResultType::Float4;
        }
        else {
          return compositor::ResultType::Color;
        }
      default:
        break;
    }

    BLI_assert_unreachable();
    return compositor::ResultType::Float;
  }

  compositor::Result get_invalid_pass()
  {
    compositor::Result invalid_pass = this->create_result(compositor::ResultType::Color);
    invalid_pass.allocate_invalid();
    return invalid_pass;
  }

  compositor::Result get_pass(const Scene *scene, int view_layer_id, const char *name) override
  {
    /* Blender aliases the Image pass name to be the Combined pass, so we return the combined pass
     * in that case. */
    const char *pass_name = StringRef(name) == "Image" ? "Combined" : name;

    if (!scene) {
      return this->get_invalid_pass();
    }

    ViewLayer *view_layer = static_cast<ViewLayer *>(
        BLI_findlink(&scene->view_layers, view_layer_id));
    if (!view_layer) {
      return this->get_invalid_pass();
    }

    Render *render = RE_GetSceneRender(scene);
    if (!render) {
      return this->get_invalid_pass();
    }

    BLI_SCOPED_DEFER([&]() { RE_ReleaseResult(render); });

    RenderResult *render_result = RE_AcquireResultRead(render);
    if (!render_result) {
      return this->get_invalid_pass();
    }

    RenderLayer *render_layer = RE_GetRenderLayer(render_result, view_layer->name);
    if (!render_layer) {
      return this->get_invalid_pass();
    }

    RenderPass *render_pass = RE_pass_find_by_name(
        render_layer, pass_name, this->get_view_name().data());
    if (!render_pass) {
      return this->get_invalid_pass();
    }

    if (!render_pass || !render_pass->ibuf || !render_pass->ibuf->float_buffer.data) {
      return this->get_invalid_pass();
    }

    compositor::Result pass_data = compositor::Result(
        *this, this->get_pass_data_type(render_pass), compositor::ResultPrecision::Full);

    if (this->use_gpu()) {
      gpu::Texture *pass_texture = RE_pass_ensure_gpu_texture_cache(render, render_pass);
      /* Don't assume render will keep pass data stored, add our own reference. */
      GPU_texture_ref(pass_texture);
      pass_data.wrap_external(pass_texture);
      cached_gpu_passes_.append(pass_texture);
    }
    else {
      /* Don't assume render will keep pass data stored, add our own reference. */
      IMB_refImBuf(render_pass->ibuf);
      pass_data.wrap_external(render_pass->ibuf->float_buffer.data,
                              int2(render_pass->ibuf->x, render_pass->ibuf->y));
      cached_cpu_passes_.append(render_pass->ibuf);
    }

    compositor::Result pass = compositor::Result(
        *this, this->get_pass_type(render_pass), compositor::ResultPrecision::Full);
    if (pass.type() != pass_data.type()) {
      compositor::ConversionOperation conversion_operation(*this, pass_data.type(), pass.type());
      conversion_operation.map_input_to_result(&pass_data);
      conversion_operation.evaluate();
      pass.steal_data(conversion_operation.get_result());
    }
    else {
      pass.steal_data(pass_data);
    }

    /* We assume the given pass is a Cryptomatte pass and retrieve its layer name. If it wasn't a
     * Cryptomatte pass, the checks below will fail anyways. */
    const std::string combined_pass_name = std::string(view_layer->name) + "." + pass_name;
    StringRef cryptomatte_layer_name = bke::cryptomatte::BKE_cryptomatte_extract_layer_name(
        combined_pass_name);

    struct StampCallbackData {
      std::string cryptomatte_layer_name;
      compositor::MetaData *meta_data;
    };

    /* Go over the stamp data and add any Cryptomatte related meta data. */
    StampCallbackData callback_data = {cryptomatte_layer_name, &pass.meta_data};
    BKE_stamp_info_callback(
        &callback_data,
        render_result->stamp_data,
        [](void *user_data, const char *key, char *value, int /*value_maxncpy*/) {
          StampCallbackData *data = static_cast<StampCallbackData *>(user_data);

          const std::string manifest_key = bke::cryptomatte::BKE_cryptomatte_meta_data_key(
              data->cryptomatte_layer_name, "manifest");
          if (key == manifest_key) {
            data->meta_data->cryptomatte.manifest = value;
          }

          const std::string hash_key = bke::cryptomatte::BKE_cryptomatte_meta_data_key(
              data->cryptomatte_layer_name, "hash");
          if (key == hash_key) {
            data->meta_data->cryptomatte.hash = value;
          }

          const std::string conversion_key = bke::cryptomatte::BKE_cryptomatte_meta_data_key(
              data->cryptomatte_layer_name, "conversion");
          if (key == conversion_key) {
            data->meta_data->cryptomatte.conversion = value;
          }
        },
        false);

    return pass;
  }

  StringRef get_view_name() const override
  {
    return input_data_.view_name;
  }

  compositor::ResultPrecision get_precision() const override
  {
    switch (input_data_.scene->r.compositor_precision) {
      case SCE_COMPOSITOR_PRECISION_AUTO:
        /* Auto uses full precision for final renders and half procession otherwise. */
        if (this->render_context()) {
          return compositor::ResultPrecision::Full;
        }
        else {
          return compositor::ResultPrecision::Half;
        }
      case SCE_COMPOSITOR_PRECISION_FULL:
        return compositor::ResultPrecision::Full;
    }

    BLI_assert_unreachable();
    return compositor::ResultPrecision::Full;
  }

  compositor::RenderContext *render_context() const override
  {
    return input_data_.render_context;
  }

  compositor::Profiler *profiler() const override
  {
    return input_data_.profiler;
  }

  void evaluate_operation_post() const override
  {
    /* If no render context exist, that means this is an interactive compositor evaluation due to
     * the user editing the node tree. In that case, we wait until the operation finishes executing
     * on the GPU before we continue to improve interactivity. The improvement comes from the fact
     * that the user might be rapidly changing values, so we need to cancel previous evaluations to
     * make editing faster, but we can't do that if all operations are submitted to the GPU all at
     * once, and we can't cancel work that was already submitted to the GPU. This does have a
     * performance penalty, but in practice, the improved interactivity is worth it according to
     * user feedback. */
    if (this->use_gpu() && !this->render_context()) {
      GPU_finish();
    }
  }

  bool is_canceled() const override
  {
    return input_data_.render->display->test_break();
  }

  void evaluate()
  {
    using namespace compositor;
    const NodeGroupOutputTypes needed_outputs = this->needed_outputs();
    const bNodeTree &node_group = *input_data_.node_tree;
    Map<bNodeInstanceKey, bke::bNodePreview> *node_previews =
        flag_is_set(needed_outputs, NodeGroupOutputTypes::NodePreviews) ?
            &node_group.runtime->previews :
            nullptr;
    NodeGroupOperation node_group_operation(*this,
                                            node_group,
                                            needed_outputs,
                                            node_previews,
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
          this->create_result(ResultType::Color, ResultPrecision::Full));
      if (input_socket == node_group.interface_inputs()[0]) {
        /* First socket is the combined pass. */
        Result combined_pass = this->get_pass(&this->get_scene(), 0, "Image");
        if (combined_pass.is_allocated()) {
          input_result->share_data(combined_pass);
        }
        else {
          input_result->allocate_invalid();
        }
        combined_pass.release();
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

      if (this->is_canceled()) {
        output_result.release();
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

/* Render Compositor */

class Compositor {
 private:
  /* Render instance for GPU context to run compositor in. */
  Render &render_;

  compositor::StaticCacheManager cache_manager_;

  /* Stores the execution device and precision used in the last evaluation of the compositor. Those
   * might be different from the current values returned by the context, since the user might have
   * changed them since the last evaluation. See the needs_to_be_recreated method for more info on
   * why those are needed. */
  bool last_evaluation_used_gpu_ = false;
  compositor::ResultPrecision last_evaluation_precision_ = compositor::ResultPrecision::Half;

 public:
  Compositor(Render &render) : render_(render) {}

  ~Compositor()
  {
    /* Use last_evaluation_used_gpu_ instead of the currently used device because we are freeing
     * resources from the last evaluation. See last_evaluation_used_gpu_ for more information. */
    if (last_evaluation_used_gpu_) {
      /* Free resources with GPU context enabled. Cleanup may happen from the main thread, and we
       * must use the main context there. */
      if (BLI_thread_is_main()) {
        DRW_gpu_context_enable();
      }
      else {
        DRW_render_context_enable(&render_);
      }
    }

    cache_manager_.free();

    /* See comment above on context enabling. */
    if (last_evaluation_used_gpu_) {
      if (BLI_thread_is_main()) {
        DRW_gpu_context_disable();
      }
      else {
        DRW_render_context_disable(&render_);
      }
    }
  }

  void execute(const ContextInputData &input_data)
  {
    Context context(cache_manager_, input_data);

    if (context.use_gpu()) {
      /* For main thread rendering in background mode, blocking rendering, or when we do not have a
       * render system GPU context, use the DRW context directly, while for threaded rendering when
       * we have a render system GPU context, use the render's system GPU context to avoid blocking
       * with the global DST. */
      void *re_system_gpu_context = RE_system_gpu_context_get(&render_);
      if (BLI_thread_is_main() || re_system_gpu_context == nullptr) {
        DRW_gpu_context_enable();
      }
      else {
        void *re_system_gpu_context = RE_system_gpu_context_get(&render_);
        WM_system_gpu_context_activate(re_system_gpu_context);

        void *re_blender_gpu_context = RE_blender_gpu_context_ensure(&render_);

        GPU_render_begin();
        GPU_context_active_set(static_cast<GPUContext *>(re_blender_gpu_context));
      }
    }

    {
      context.evaluate();

      /* Reset the cache, but only if the evaluation did not get canceled, because in that case, we
       * wouldn't want to invalidate the cache because not all operations that use cached resources
       * got the chance to mark their used resources as still in use. So we wait until a full
       * evaluation happen before we decide that some resources are no longer needed. */
      if (!context.is_canceled()) {
        context.cache_manager().reset();
      }

      last_evaluation_used_gpu_ = context.use_gpu();
      last_evaluation_precision_ = context.get_precision();
    }

    if (context.use_gpu()) {
      gpu::TexturePool::get().reset();

      void *re_system_gpu_context = RE_system_gpu_context_get(&render_);
      if (BLI_thread_is_main() || re_system_gpu_context == nullptr) {
        DRW_gpu_context_disable();
      }
      else {
        GPU_render_end();
        GPU_context_active_set(nullptr);
        void *re_system_gpu_context = RE_system_gpu_context_get(&render_);
        WM_system_gpu_context_release(re_system_gpu_context);
      }
    }
  }

  /* Returns true if the compositor should be freed and reconstructed, which is needed when the
   * compositor execution device or precision changed, because we either need to update all cached
   * resources for the new execution device and precision, or we simply recreate the entire
   * compositor, since it is much easier and safer. */
  bool needs_to_be_recreated(const ContextInputData &input_data)
  {
    Context context(cache_manager_, input_data);
    /* See last_evaluation_used_gpu_ and last_evaluation_precision_ for more information what how
     * they are different from the ones returned from the context. */
    return context.use_gpu() != last_evaluation_used_gpu_ ||
           context.get_precision() != last_evaluation_precision_;
  }
};

}  // namespace render

void Render::compositor_execute(const Scene &scene,
                                const RenderData &render_data,
                                const bNodeTree &node_tree,
                                const char *view_name,
                                compositor::RenderContext *render_context,
                                compositor::Profiler *profiler,
                                compositor::NodeGroupOutputTypes needed_outputs)
{
  std::unique_lock lock(this->compositor_mutex);

  render::ContextInputData input_data(
      this, scene, render_data, node_tree, view_name, render_context, profiler, needed_outputs);

  if (this->compositor && this->compositor->needs_to_be_recreated(input_data)) {
    /* Free it here and it will be recreated in the check below. */
    delete this->compositor;
    this->compositor = nullptr;
  }

  if (!this->compositor) {
    this->compositor = new render::Compositor(*this);
  }

  this->compositor->execute(input_data);
}

void Render::compositor_free()
{
  std::unique_lock lock(this->compositor_mutex);

  if (this->compositor != nullptr) {
    delete this->compositor;
    this->compositor = nullptr;
  }
}

void RE_compositor_execute(Render &render,
                           const Scene &scene,
                           const RenderData &render_data,
                           const bNodeTree &node_tree,
                           const char *view_name,
                           compositor::RenderContext *render_context,
                           compositor::Profiler *profiler,
                           compositor::NodeGroupOutputTypes needed_outputs)
{
  render.compositor_execute(
      scene, render_data, node_tree, view_name, render_context, profiler, needed_outputs);
}

void RE_compositor_free(Render &render)
{
  render.compositor_free();
}

}  // namespace blender
