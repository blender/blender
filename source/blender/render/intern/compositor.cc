/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstring>
#include <string>

#include "BLI_math_vector_types.hh"
#include "BLI_threads.h"
#include "BLI_vector.hh"

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"

#include "BKE_global.hh"
#include "BKE_image.h"
#include "BKE_node.hh"
#include "BKE_scene.hh"

#include "DRW_engine.hh"
#include "DRW_render.hh"

#include "IMB_imbuf.hh"

#include "DEG_depsgraph_query.hh"

#include "COM_context.hh"
#include "COM_domain.hh"
#include "COM_evaluator.hh"
#include "COM_render_context.hh"

#include "RE_compositor.hh"
#include "RE_pipeline.h"

#include "WM_api.hh"

#include "GPU_context.hh"

#include "render_types.h"

namespace blender::render {

/**
 * Render Texture Pool
 *
 * TODO: should share pool with draw manager. It needs some globals initialization figured out
 * there first.
 */
class TexturePool : public realtime_compositor::TexturePool {
 private:
  /** Textures that are not yet used and are available to be acquired. After evaluation, any
   * texture in this map should be freed because it was not acquired in the evaluation and is thus
   * unused. Textures removed from this map should be moved to the textures_in_use_ map when
   * acquired. */
  Map<realtime_compositor::TexturePoolKey, Vector<GPUTexture *>> available_textures_;
  /** Textures that were acquired in this compositor evaluation. After evaluation, those textures
   * are moved to the available_textures_ map to be acquired in the next evaluation. */
  Map<realtime_compositor::TexturePoolKey, Vector<GPUTexture *>> textures_in_use_;

 public:
  virtual ~TexturePool()
  {
    for (Vector<GPUTexture *> &available_textures : available_textures_.values()) {
      for (GPUTexture *texture : available_textures) {
        GPU_texture_free(texture);
      }
    }

    for (Vector<GPUTexture *> &textures_in_use : textures_in_use_.values()) {
      for (GPUTexture *texture : textures_in_use) {
        GPU_texture_free(texture);
      }
    }
  }

  GPUTexture *allocate_texture(int2 size, eGPUTextureFormat format) override
  {
    const realtime_compositor::TexturePoolKey key(size, format);
    Vector<GPUTexture *> &available_textures = available_textures_.lookup_or_add_default(key);
    GPUTexture *texture = nullptr;
    if (available_textures.is_empty()) {
      texture = GPU_texture_create_2d("compositor_texture_pool",
                                      size.x,
                                      size.y,
                                      1,
                                      format,
                                      GPU_TEXTURE_USAGE_GENERAL,
                                      nullptr);
    }
    else {
      texture = available_textures.pop_last();
    }

    textures_in_use_.lookup_or_add_default(key).append(texture);
    return texture;
  }

  /** Should be called after compositor evaluation to free unused textures and reset the texture
   * pool. */
  void free_unused_and_reset()
  {
    /* Free all textures in the available textures vectors. The fact that they still exist in those
     * vectors after evaluation means they were not acquired during the evaluation, and are thus
     * consequently no longer used. */
    for (Vector<GPUTexture *> &available_textures : available_textures_.values()) {
      for (GPUTexture *texture : available_textures) {
        GPU_texture_free(texture);
      }
    }

    /* Move all textures in-use to be available textures for the next evaluation. */
    available_textures_ = textures_in_use_;
    textures_in_use_.clear();
  }
};

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
  realtime_compositor::RenderContext *render_context;

  ContextInputData(const Scene &scene,
                   const RenderData &render_data,
                   const bNodeTree &node_tree,
                   const char *view_name,
                   realtime_compositor::RenderContext *render_context)
      : scene(&scene),
        render_data(&render_data),
        node_tree(&node_tree),
        view_name(view_name),
        render_context(render_context)
  {
  }
};

/* Render Context Data */

class Context : public realtime_compositor::Context {
 private:
  /* Input data. */
  ContextInputData input_data_;

  /* Output combined texture. */
  GPUTexture *output_texture_ = nullptr;

  /* Viewer output texture. */
  GPUTexture *viewer_output_texture_ = nullptr;

  /* Cached textures that the compositor took ownership of. */
  Vector<GPUTexture *> textures_;

 public:
  Context(const ContextInputData &input_data, TexturePool &texture_pool)
      : realtime_compositor::Context(texture_pool), input_data_(input_data)
  {
  }

  virtual ~Context()
  {
    GPU_TEXTURE_FREE_SAFE(output_texture_);
    GPU_TEXTURE_FREE_SAFE(viewer_output_texture_);
    for (GPUTexture *texture : textures_) {
      GPU_texture_free(texture);
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

  bool use_file_output() const override
  {
    return this->render_context() != nullptr;
  }

  bool use_composite_output() const override
  {
    return true;
  }

  const RenderData &get_render_data() const override
  {
    return *(input_data_.render_data);
  }

  int2 get_render_size() const override
  {
    int width, height;
    BKE_render_resolution(input_data_.render_data, true, &width, &height);
    return int2(width, height);
  }

  rcti get_compositing_region() const override
  {
    const int2 render_size = get_render_size();
    const rcti render_region = rcti{0, render_size.x, 0, render_size.y};

    return render_region;
  }

  GPUTexture *get_output_texture() override
  {
    /* TODO: just a temporary hack, needs to get stored in RenderResult,
     * once that supports GPU buffers. */
    if (output_texture_ == nullptr) {
      const int2 size = get_render_size();
      output_texture_ = GPU_texture_create_2d(
          "compositor_output_texture",
          size.x,
          size.y,
          1,
          get_precision() == realtime_compositor::ResultPrecision::Half ? GPU_RGBA16F :
                                                                          GPU_RGBA32F,
          GPU_TEXTURE_USAGE_GENERAL,
          nullptr);
    }

    return output_texture_;
  }

  GPUTexture *get_viewer_output_texture(realtime_compositor::Domain domain,
                                        const bool is_data) override
  {
    /* Re-create texture if the viewer size changes. */
    const int2 size = domain.size;
    if (viewer_output_texture_) {
      const int current_width = GPU_texture_width(viewer_output_texture_);
      const int current_height = GPU_texture_height(viewer_output_texture_);

      if (current_width != size.x || current_height != size.y) {
        GPU_TEXTURE_FREE_SAFE(viewer_output_texture_);
        viewer_output_texture_ = nullptr;
      }
    }

    /* TODO: just a temporary hack, needs to get stored in RenderResult,
     * once that supports GPU buffers. */
    if (viewer_output_texture_ == nullptr) {
      viewer_output_texture_ = GPU_texture_create_2d(
          "compositor_viewer_output_texture",
          size.x,
          size.y,
          1,
          get_precision() == realtime_compositor::ResultPrecision::Half ? GPU_RGBA16F :
                                                                          GPU_RGBA32F,
          GPU_TEXTURE_USAGE_GENERAL,
          nullptr);
    }

    Image *image = BKE_image_ensure_viewer(G.main, IMA_TYPE_COMPOSITE, "Viewer Node");
    const float2 translation = domain.transformation.location();
    image->runtime.backdrop_offset[0] = translation.x;
    image->runtime.backdrop_offset[1] = translation.y;

    if (is_data) {
      image->flag &= ~IMA_VIEW_AS_RENDER;
    }
    else {
      image->flag |= IMA_VIEW_AS_RENDER;
    }

    return viewer_output_texture_;
  }

  GPUTexture *get_input_texture(const Scene *scene,
                                int view_layer_id,
                                const char *pass_name) override
  {
    Render *re = RE_GetSceneRender(scene);
    RenderResult *rr = nullptr;
    GPUTexture *input_texture = nullptr;

    if (re) {
      rr = RE_AcquireResultRead(re);
    }

    if (rr) {
      ViewLayer *view_layer = (ViewLayer *)BLI_findlink(&scene->view_layers, view_layer_id);
      if (view_layer) {
        RenderLayer *rl = RE_GetRenderLayer(rr, view_layer->name);
        if (rl) {
          RenderPass *rpass = RE_pass_find_by_name(rl, pass_name, get_view_name().data());

          if (rpass && rpass->ibuf && rpass->ibuf->float_buffer.data) {
            input_texture = RE_pass_ensure_gpu_texture_cache(re, rpass);

            if (input_texture) {
              /* Don't assume render keeps texture around, add our own reference. */
              GPU_texture_ref(input_texture);
              textures_.append(input_texture);
            }
          }
        }
      }
    }

    if (re) {
      RE_ReleaseResult(re);
      re = nullptr;
    }

    return input_texture;
  }

  StringRef get_view_name() override
  {
    return input_data_.view_name;
  }

  realtime_compositor::ResultPrecision get_precision() const override
  {
    switch (input_data_.scene->r.compositor_precision) {
      case SCE_COMPOSITOR_PRECISION_AUTO:
        /* Auto uses full precision for final renders and half procession otherwise. */
        if (this->render_context()) {
          return realtime_compositor::ResultPrecision::Full;
        }
        else {
          return realtime_compositor::ResultPrecision::Half;
        }
      case SCE_COMPOSITOR_PRECISION_FULL:
        return realtime_compositor::ResultPrecision::Full;
    }

    BLI_assert_unreachable();
    return realtime_compositor::ResultPrecision::Full;
  }

  void set_info_message(StringRef /*message*/) const override
  {
    /* TODO: ignored for now. Currently only used to communicate incomplete node support
     * which is already shown on the node itself.
     *
     * Perhaps this overall info message could be replaced by a boolean indicating
     * incomplete support, and leave more specific message to individual nodes? */
  }

  IDRecalcFlag query_id_recalc_flag(ID *id) const override
  {
    DrawEngineType *owner = (DrawEngineType *)this;
    DrawData *draw_data = DRW_drawdata_ensure(id, owner, sizeof(DrawData), nullptr, nullptr);
    IDRecalcFlag recalc_flag = IDRecalcFlag(draw_data->recalc);
    draw_data->recalc = IDRecalcFlag(0);
    return recalc_flag;
  }

  void output_to_render_result()
  {
    if (!output_texture_) {
      return;
    }

    Render *re = RE_GetSceneRender(input_data_.scene);
    RenderResult *rr = RE_AcquireResultWrite(re);

    if (rr) {
      RenderView *rv = RE_RenderViewGetByName(rr, input_data_.view_name.c_str());

      GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
      float *output_buffer = (float *)GPU_texture_read(output_texture_, GPU_DATA_FLOAT, 0);

      if (output_buffer) {
        ImBuf *ibuf = RE_RenderViewEnsureImBuf(rr, rv);
        IMB_assign_float_buffer(ibuf, output_buffer, IB_TAKE_OWNERSHIP);
      }

      /* TODO: z-buffer output. */

      rr->have_combined = true;
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
    if (!viewer_output_texture_) {
      return;
    }

    Image *image = BKE_image_ensure_viewer(G.main, IMA_TYPE_COMPOSITE, "Viewer Node");

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

    const int2 size = int2(GPU_texture_width(viewer_output_texture_),
                           GPU_texture_height(viewer_output_texture_));
    if (image_buffer->x != size.x || image_buffer->y != size.y) {
      imb_freerectImBuf(image_buffer);
      imb_freerectfloatImBuf(image_buffer);
      image_buffer->x = size.x;
      image_buffer->y = size.y;
      imb_addrectfloatImBuf(image_buffer, 4);
      image_buffer->userflags |= IB_DISPLAY_BUFFER_INVALID;
    }

    BKE_image_release_ibuf(image, image_buffer, lock);
    BLI_thread_unlock(LOCK_DRAW_IMAGE);

    GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
    float *output_buffer = (float *)GPU_texture_read(viewer_output_texture_, GPU_DATA_FLOAT, 0);

    std::memcpy(
        image_buffer->float_buffer.data, output_buffer, size.x * size.y * 4 * sizeof(float));

    MEM_freeN(output_buffer);

    BKE_image_partial_update_mark_full_update(image);
    if (input_data_.node_tree->runtime->update_draw) {
      input_data_.node_tree->runtime->update_draw(input_data_.node_tree->runtime->udh);
    }
  }

  realtime_compositor::RenderContext *render_context() const override
  {
    return input_data_.render_context;
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
    if (!this->render_context()) {
      GPU_finish();
    }
  }
};

/* Render Realtime Compositor */

class RealtimeCompositor {
 private:
  /* Render instance for GPU context to run compositor in. */
  Render &render_;

  std::unique_ptr<TexturePool> texture_pool_;
  std::unique_ptr<Context> context_;

 public:
  RealtimeCompositor(Render &render, const ContextInputData &input_data) : render_(render)
  {
    /* Create resources with GPU context enabled. */
    DRW_render_context_enable(&render_);
    texture_pool_ = std::make_unique<TexturePool>();
    context_ = std::make_unique<Context>(input_data, *texture_pool_);
    DRW_render_context_disable(&render_);
  }

  ~RealtimeCompositor()
  {
    /* Free resources with GPU context enabled. Cleanup may happen from the
     * main thread, and we must use the main context there. */
    if (BLI_thread_is_main()) {
      DRW_gpu_context_enable();
    }
    else {
      DRW_render_context_enable(&render_);
    }

    context_.reset();
    texture_pool_.reset();

    if (BLI_thread_is_main()) {
      DRW_gpu_context_disable();
    }
    else {
      DRW_render_context_disable(&render_);
    }
  }

  /* Evaluate the compositor and output to the scene render result. */
  void execute(const ContextInputData &input_data)
  {
    void *re_system_gpu_context = RE_system_gpu_context_get(&render_);
    if (!re_system_gpu_context) {
      /* In some cases like background mode and blocking rendering the system context of the render
       * engine might be nullptr, which forces some code paths which more tightly couple it with
       * the draw manager. For the compositor we want to have the least amount of coupling with the
       * draw manager, so ensure that the render engine has its own system GPU context. */
      RE_system_gpu_context_ensure(&render_);
      re_system_gpu_context = RE_system_gpu_context_get(&render_);
    }

    void *re_blender_gpu_context = RE_blender_gpu_context_ensure(&render_);

    GPU_render_begin();
    WM_system_gpu_context_activate(re_system_gpu_context);
    GPU_context_active_set(static_cast<GPUContext *>(re_blender_gpu_context));

    context_->update_input_data(input_data);

    /* Always recreate the evaluator, as this only runs on compositing node changes and
     * there is no reason to cache this. Unlike the viewport where it helps for navigation. */
    {
      realtime_compositor::Evaluator evaluator(*context_);
      evaluator.evaluate();
    }

    context_->output_to_render_result();
    context_->viewer_output_to_viewer_image();
    texture_pool_->free_unused_and_reset();

    GPU_flush();
    GPU_render_end();
    GPU_context_active_set(nullptr);
    WM_system_gpu_context_release(re_system_gpu_context);
  }
};

}  // namespace blender::render

void Render::compositor_execute(const Scene &scene,
                                const RenderData &render_data,
                                const bNodeTree &node_tree,
                                const char *view_name,
                                blender::realtime_compositor::RenderContext *render_context)
{
  std::unique_lock lock(gpu_compositor_mutex);

  blender::render::ContextInputData input_data(
      scene, render_data, node_tree, view_name, render_context);

  if (gpu_compositor == nullptr) {
    gpu_compositor = new blender::render::RealtimeCompositor(*this, input_data);
  }

  gpu_compositor->execute(input_data);
}

void Render::compositor_free()
{
  std::unique_lock lock(gpu_compositor_mutex);

  if (gpu_compositor != nullptr) {
    delete gpu_compositor;
    gpu_compositor = nullptr;
  }
}

void RE_compositor_execute(Render &render,
                           const Scene &scene,
                           const RenderData &render_data,
                           const bNodeTree &node_tree,
                           const char *view_name,
                           blender::realtime_compositor::RenderContext *render_context)
{
  render.compositor_execute(scene, render_data, node_tree, view_name, render_context);
}

void RE_compositor_free(Render &render)
{
  render.compositor_free();
}
