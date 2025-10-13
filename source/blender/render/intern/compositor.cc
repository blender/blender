/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstring>
#include <string>

#include "BLI_listbase.h"
#include "BLI_math_vector_types.hh"
#include "BLI_threads.h"
#include "BLI_vector.hh"

#include "MEM_guardedalloc.h"

#include "BKE_cryptomatte.hh"
#include "BKE_global.hh"
#include "BKE_image.hh"
#include "BKE_node.hh"
#include "BKE_scene.hh"

#include "DRW_engine.hh"
#include "DRW_render.hh"

#include "IMB_imbuf.hh"

#include "COM_context.hh"
#include "COM_domain.hh"
#include "COM_evaluator.hh"
#include "COM_render_context.hh"

#include "RE_compositor.hh"
#include "RE_pipeline.h"

#include "WM_api.hh"

#include "GPU_context.hh"
#include "GPU_state.hh"
#include "GPU_texture_pool.hh"

#include "render_types.h"

namespace blender::render {

/**
 * Render Context Data
 *
 * Stored separately from the context so we can update it without losing any cached
 * data from the context.
 */
class ContextInputData {
 public:
  const Scene *scene;
  const RenderData *render_data;
  const bNodeTree *node_tree;
  std::string view_name;
  compositor::RenderContext *render_context;
  compositor::Profiler *profiler;
  compositor::OutputTypes needed_outputs;

  ContextInputData(const Scene &scene,
                   const RenderData &render_data,
                   const bNodeTree &node_tree,
                   const char *view_name,
                   compositor::RenderContext *render_context,
                   compositor::Profiler *profiler,
                   compositor::OutputTypes needed_outputs)
      : scene(&scene),
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

  /* Output combined result. */
  compositor::Result output_result_;

  /* Viewer output result. */
  compositor::Result viewer_output_result_;

  /* Cached GPU and CPU passes that the compositor took ownership of. Those had their reference
   * count incremented when accessed and need to be freed/have their reference count decremented
   * when destroying the context. */
  Vector<blender::gpu::Texture *> cached_gpu_passes_;
  Vector<ImBuf *> cached_cpu_passes_;

 public:
  Context(const ContextInputData &input_data)
      : compositor::Context(),
        input_data_(input_data),
        output_result_(this->create_result(compositor::ResultType::Color)),
        viewer_output_result_(this->create_result(compositor::ResultType::Color))
  {
  }

  virtual ~Context()
  {
    output_result_.release();
    viewer_output_result_.release();
    for (blender::gpu::Texture *pass : cached_gpu_passes_) {
      GPU_texture_free(pass);
    }
    for (ImBuf *pass : cached_cpu_passes_) {
      IMB_freeImBuf(pass);
    }
  }

  void update_input_data(const ContextInputData &input_data)
  {
    input_data_ = input_data;
  }

  const Scene &get_scene() const override
  {
    return *input_data_.scene;
  }

  const bNodeTree &get_node_tree() const override
  {
    return *input_data_.node_tree;
  }

  bool use_gpu() const override
  {
    return this->get_render_data().compositor_device == SCE_COMPOSITOR_DEVICE_GPU;
  }

  compositor::OutputTypes needed_outputs() const override
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

  Bounds<int2> get_compositing_region() const override
  {
    return Bounds<int2>(int2(0), this->get_render_size());
  }

  compositor::Result get_output(compositor::Domain /*domain*/) override
  {
    const int2 render_size = get_render_size();
    if (output_result_.is_allocated()) {
      /* If the allocated result have the same size as the render size, return it as is. */
      if (render_size == output_result_.domain().size) {
        return output_result_;
      }
      /* Otherwise, the size changed, so release its data and reset it, then we reallocate it on
       * the new render size below. */
      output_result_.release();
      output_result_ = this->create_result(compositor::ResultType::Color);
    }

    output_result_.allocate_texture(render_size, false);
    return output_result_;
  }

  compositor::Result get_viewer_output(compositor::Domain domain,
                                       const bool is_data,
                                       compositor::ResultPrecision precision) override
  {
    viewer_output_result_.set_transformation(domain.transformation);
    viewer_output_result_.meta_data.is_non_color_data = is_data;

    if (viewer_output_result_.is_allocated()) {
      /* If the allocated result have the same size and precision as requested, return it as is. */
      if (domain.size == viewer_output_result_.domain().size &&
          precision == viewer_output_result_.precision())
      {
        return viewer_output_result_;
      }

      /* Otherwise, the size or precision changed, so release its data and reset it, then we
       * reallocate it on the new domain below. */
      viewer_output_result_.release();
      viewer_output_result_ = this->create_result(compositor::ResultType::Color);
      viewer_output_result_.set_transformation(domain.transformation);
      viewer_output_result_.meta_data.is_non_color_data = is_data;
    }

    viewer_output_result_.set_precision(precision);
    viewer_output_result_.allocate_texture(domain, false);
    return viewer_output_result_;
  }

  compositor::Result get_pass(const Scene *scene, int view_layer_id, const char *name) override
  {
    /* Blender aliases the Image pass name to be the Combined pass, so we return the combined pass
     * in that case. */
    const char *pass_name = StringRef(name) == "Image" ? "Combined" : name;

    if (!scene) {
      return compositor::Result(*this);
    }

    ViewLayer *view_layer = static_cast<ViewLayer *>(
        BLI_findlink(&scene->view_layers, view_layer_id));
    if (!view_layer) {
      return compositor::Result(*this);
    }

    Render *render = RE_GetSceneRender(scene);
    if (!render) {
      return compositor::Result(*this);
    }

    RenderResult *render_result = RE_AcquireResultRead(render);
    if (!render_result) {
      RE_ReleaseResult(render);
      return compositor::Result(*this);
    }

    RenderLayer *render_layer = RE_GetRenderLayer(render_result, view_layer->name);
    if (!render_layer) {
      RE_ReleaseResult(render);
      return compositor::Result(*this);
    }

    RenderPass *render_pass = RE_pass_find_by_name(
        render_layer, pass_name, this->get_view_name().data());
    if (!render_pass) {
      RE_ReleaseResult(render);
      return compositor::Result(*this);
    }

    if (!render_pass || !render_pass->ibuf || !render_pass->ibuf->float_buffer.data) {
      RE_ReleaseResult(render);
      return compositor::Result(*this);
    }

    compositor::Result pass = compositor::Result(
        *this, this->result_type_from_pass(render_pass), compositor::ResultPrecision::Full);

    if (this->use_gpu()) {
      blender::gpu::Texture *pass_texture = RE_pass_ensure_gpu_texture_cache(render, render_pass);
      /* Don't assume render will keep pass data stored, add our own reference. */
      GPU_texture_ref(pass_texture);
      pass.wrap_external(pass_texture);
      cached_gpu_passes_.append(pass_texture);
    }
    else {
      /* Don't assume render will keep pass data stored, add our own reference. */
      IMB_refImBuf(render_pass->ibuf);
      pass.wrap_external(render_pass->ibuf->float_buffer.data,
                         int2(render_pass->ibuf->x, render_pass->ibuf->y));
      cached_cpu_passes_.append(render_pass->ibuf);
    }

    RE_ReleaseResult(render);
    return pass;
  }

  compositor::Result get_input(StringRef name) override
  {
    if (name == "Image") {
      return this->get_pass(&this->get_scene(), 0, name.data());
    }

    return this->create_result(compositor::ResultType::Color);
  }

  compositor::ResultType result_type_from_pass(const RenderPass *pass)
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

  void populate_meta_data_for_pass(const Scene *scene,
                                   int view_layer_id,
                                   const char *pass_name,
                                   compositor::MetaData &meta_data) const override
  {
    if (!scene) {
      return;
    }

    ViewLayer *view_layer = static_cast<ViewLayer *>(
        BLI_findlink(&scene->view_layers, view_layer_id));
    if (!view_layer) {
      return;
    }

    Render *render = RE_GetSceneRender(scene);
    if (!render) {
      return;
    }

    RenderResult *render_result = RE_AcquireResultRead(render);
    if (!render_result || !render_result->stamp_data) {
      RE_ReleaseResult(render);
      return;
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
    StampCallbackData callback_data = {cryptomatte_layer_name, &meta_data};
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

    RE_ReleaseResult(render);
  }

  void output_to_render_result()
  {
    if (!output_result_.is_allocated()) {
      return;
    }

    Render *re = RE_GetSceneRender(input_data_.scene);
    RenderResult *rr = RE_AcquireResultWrite(re);

    if (rr) {
      RenderView *rv = RE_RenderViewGetByName(rr, input_data_.view_name.c_str());
      ImBuf *ibuf = RE_RenderViewEnsureImBuf(rr, rv);
      rr->have_combined = true;

      if (this->use_gpu()) {
        GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
        float *output_buffer = static_cast<float *>(
            GPU_texture_read(output_result_, GPU_DATA_FLOAT, 0));
        IMB_assign_float_buffer(ibuf, output_buffer, IB_TAKE_OWNERSHIP);
      }
      else {
        float *data = MEM_malloc_arrayN<float>(4 * size_t(rr->rectx) * size_t(rr->recty),
                                               __func__);
        IMB_assign_float_buffer(ibuf, data, IB_TAKE_OWNERSHIP);
        std::memcpy(
            data, output_result_.cpu_data().data(), rr->rectx * rr->recty * 4 * sizeof(float));
      }
    }

    if (re) {
      RE_ReleaseResult(re);
      re = nullptr;
    }

    Image *image = BKE_image_ensure_viewer(G.main, IMA_TYPE_R_RESULT, "Render Result");
    BKE_image_partial_update_mark_full_update(image);
    BLI_thread_lock(LOCK_DRAW_IMAGE);
    BKE_image_signal(G.main, image, nullptr, IMA_SIGNAL_FREE);
    BLI_thread_unlock(LOCK_DRAW_IMAGE);
  }

  void viewer_output_to_viewer_image()
  {
    if (!viewer_output_result_.is_allocated()) {
      return;
    }

    Image *image = BKE_image_ensure_viewer(G.main, IMA_TYPE_COMPOSITE, "Viewer Node");
    const float2 translation = viewer_output_result_.domain().transformation.location();
    image->runtime->backdrop_offset[0] = translation.x;
    image->runtime->backdrop_offset[1] = translation.y;

    if (viewer_output_result_.meta_data.is_non_color_data) {
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

    const int2 size = viewer_output_result_.domain().size;
    if (image_buffer->x != size.x || image_buffer->y != size.y) {
      IMB_free_byte_pixels(image_buffer);
      IMB_free_float_pixels(image_buffer);
      image_buffer->x = size.x;
      image_buffer->y = size.y;
      IMB_alloc_float_pixels(image_buffer, 4);
      image_buffer->userflags |= IB_DISPLAY_BUFFER_INVALID;
    }

    BKE_image_release_ibuf(image, image_buffer, lock);
    BLI_thread_unlock(LOCK_DRAW_IMAGE);

    if (this->use_gpu()) {
      GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
      float *output_buffer = static_cast<float *>(
          GPU_texture_read(viewer_output_result_, GPU_DATA_FLOAT, 0));

      std::memcpy(
          image_buffer->float_buffer.data, output_buffer, size.x * size.y * 4 * sizeof(float));
      MEM_freeN(output_buffer);
    }
    else {
      std::memcpy(image_buffer->float_buffer.data,
                  viewer_output_result_.cpu_data().data(),
                  size.x * size.y * 4 * sizeof(float));
    }

    BKE_image_partial_update_mark_full_update(image);
    if (input_data_.node_tree->runtime->update_draw) {
      input_data_.node_tree->runtime->update_draw(input_data_.node_tree->runtime->udh);
    }
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
};

/* Render Compositor */

class Compositor {
 private:
  /* Render instance for GPU context to run compositor in. */
  Render &render_;

  std::unique_ptr<Context> context_;

  /* Stores the execution device and precision used in the last evaluation of the compositor. Those
   * might be different from the current values returned by the context, since the user might have
   * changed them since the last evaluation. See the needs_to_be_recreated method for more info on
   * why those are needed. */
  bool uses_gpu_;
  compositor::ResultPrecision used_precision_;

 public:
  Compositor(Render &render, const ContextInputData &input_data) : render_(render)
  {
    context_ = std::make_unique<Context>(input_data);

    uses_gpu_ = context_->use_gpu();
    used_precision_ = context_->get_precision();
  }

  ~Compositor()
  {
    /* Use uses_gpu_ instead of context_->use_gpu() because we are freeing resources from the last
     * evaluation. See uses_gpu_ for more information. */
    if (uses_gpu_) {
      /* Free resources with GPU context enabled. Cleanup may happen from the
       * main thread, and we must use the main context there. */
      if (BLI_thread_is_main()) {
        DRW_gpu_context_enable();
      }
      else {
        DRW_render_context_enable(&render_);
      }
    }

    context_.reset();

    /* See comment above on context enabling. */
    if (uses_gpu_) {
      if (BLI_thread_is_main()) {
        DRW_gpu_context_disable();
      }
      else {
        DRW_render_context_disable(&render_);
      }
    }
  }

  void update_input_data(const ContextInputData &input_data)
  {
    context_->update_input_data(input_data);
  }

  void execute()
  {
    if (context_->use_gpu()) {
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
      compositor::Evaluator evaluator(*context_);
      evaluator.evaluate();
    }

    context_->output_to_render_result();
    context_->viewer_output_to_viewer_image();

    if (context_->use_gpu()) {
      blender::gpu::TexturePool::get().reset();

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
   * and pooled resources for the new execution device and precision, or we simply recreate the
   * entire compositor, since it is much easier and safer. */
  bool needs_to_be_recreated()
  {
    /* See uses_gpu_ and used_precision_ for more information what how they are different from the
     * ones returned from the context. */
    return context_->use_gpu() != uses_gpu_ || context_->get_precision() != used_precision_;
  }
};

}  // namespace blender::render

void Render::compositor_execute(const Scene &scene,
                                const RenderData &render_data,
                                const bNodeTree &node_tree,
                                const char *view_name,
                                blender::compositor::RenderContext *render_context,
                                blender::compositor::Profiler *profiler,
                                blender::compositor::OutputTypes needed_outputs)
{
  std::unique_lock lock(this->compositor_mutex);

  blender::render::ContextInputData input_data(
      scene, render_data, node_tree, view_name, render_context, profiler, needed_outputs);

  if (this->compositor) {
    this->compositor->update_input_data(input_data);

    if (this->compositor->needs_to_be_recreated()) {
      /* Free it here and it will be recreated in the check below. */
      delete this->compositor;
      this->compositor = nullptr;
    }
  }

  if (!this->compositor) {
    this->compositor = new blender::render::Compositor(*this, input_data);
  }

  this->compositor->execute();
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
                           blender::compositor::RenderContext *render_context,
                           blender::compositor::Profiler *profiler,
                           blender::compositor::OutputTypes needed_outputs)
{
  render.compositor_execute(
      scene, render_data, node_tree, view_name, render_context, profiler, needed_outputs);
}

void RE_compositor_free(Render &render)
{
  render.compositor_free();
}
