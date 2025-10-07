/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_bounds.hh"
#include "BLI_listbase.h"
#include "BLI_math_vector_types.hh"
#include "BLI_string_ref.hh"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_ID.h"
#include "DNA_ID_enums.h"
#include "DNA_scene_types.h"
#include "DNA_vec_types.h"
#include "DNA_view3d_types.h"

#include "DEG_depsgraph_query.hh"

#include "ED_view3d.hh"

#include "DRW_gpu_wrapper.hh"
#include "DRW_render.hh"

#include "COM_context.hh"
#include "COM_domain.hh"
#include "COM_evaluator.hh"
#include "COM_result.hh"

#include "GPU_context.hh"
#include "GPU_state.hh"
#include "GPU_texture.hh"

#include "draw_view_data.hh"

#include "compositor_engine.h" /* Own include. */

namespace blender::draw::compositor_engine {

class Context : public compositor::Context {
 private:
  /* A pointer to the info message of the compositor engine. This is a char array of size
   * GPU_INFO_SIZE. The message is cleared prior to updating or evaluating the compositor. */
  char *info_message_;
  const Scene *scene_;

 public:
  Context(char *info_message) : compositor::Context(), info_message_(info_message) {}

  void set_scene(const Scene *scene)
  {
    scene_ = scene;
  }

  const Scene &get_scene() const override
  {
    return *scene_;
  }

  const bNodeTree &get_node_tree() const override
  {
    return *scene_->compositing_node_group;
  }

  bool use_gpu() const override
  {
    return true;
  }

  compositor::OutputTypes needed_outputs() const override
  {
    return compositor::OutputTypes::Composite | compositor::OutputTypes::Viewer;
  }

  /* The viewport compositor does not support viewer outputs, so treat viewers as composite
   * outputs. */
  bool treat_viewer_as_compositor_output() const override
  {
    return true;
  }

  /* We limit the compositing region to the camera region if in camera view, while we use the
   * entire viewport otherwise. We also use the entire viewport when doing viewport rendering since
   * the viewport is already the camera region in that case. */
  Bounds<int2> get_compositing_region() const override
  {
    const DRWContext *draw_ctx = DRW_context_get();
    const int2 viewport_size = int2(draw_ctx->viewport_size_get());
    const Bounds<int2> render_region = Bounds<int2>(int2(0), viewport_size);

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

    return blender::bounds::intersect(render_region, camera_region)
        .value_or(Bounds<int2>(int2(0)));
  }

  compositor::Result get_output(compositor::Domain /*domain*/) override
  {
    compositor::Result result = this->create_result(compositor::ResultType::Color,
                                                    compositor::ResultPrecision::Half);
    result.wrap_external(DRW_context_get()->viewport_texture_list_get()->color);
    return result;
  }

  compositor::Result get_viewer_output(compositor::Domain /*domain*/,
                                       bool /*is_data*/,
                                       compositor::ResultPrecision /*precision*/) override
  {
    compositor::Result result = this->create_result(compositor::ResultType::Color,
                                                    compositor::ResultPrecision::Half);
    result.wrap_external(DRW_context_get()->viewport_texture_list_get()->color);
    return result;
  }

  compositor::Result get_pass(const Scene *scene, int view_layer_index, const char *name) override
  {
    /* Blender aliases the Image pass name to be the Combined pass, so we return the combined pass
     * in that case. */
    const char *pass_name = StringRef(name) == "Image" ? "Combined" : name;

    const Scene *original_scene = DEG_get_original(scene_);
    if (DEG_get_original(scene) != original_scene) {
      return compositor::Result(*this);
    }

    ViewLayer *view_layer = static_cast<ViewLayer *>(
        BLI_findlink(&original_scene->view_layers, view_layer_index));
    if (StringRef(view_layer->name) != DRW_context_get()->view_layer->name) {
      return compositor::Result(*this);
    }

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

    return compositor::Result(*this);
  }

  compositor::Result get_input(StringRef name) override
  {
    if (name == "Image") {
      return this->get_pass(&this->get_scene(), 0, name.data());
    }

    return this->create_result(compositor::ResultType::Color);
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
};

class Instance : public DrawEngine {
 private:
  Context context_;

 public:
  Instance() : context_(this->info) {}

  StringRefNull name_get() final
  {
    return "Compositor";
  }

  void init() final {};
  void begin_sync() final {};
  void object_sync(blender::draw::ObjectRef & /*ob_ref*/,
                   blender::draw::Manager & /*manager*/) final {};
  void end_sync() final {};

  void draw(Manager & /*manager*/) final
  {
    DRW_submission_start();

#if defined(__APPLE__)
    if (GPU_backend_get_type() == GPU_BACKEND_METAL) {
      /* NOTE(Metal): Isolate Compositor compute work in individual command buffer to improve
       * workload scheduling. When expensive compositor nodes are in the graph, these can stall out
       * the GPU for extended periods of time and sub-optimally schedule work for execution. */
      GPU_flush();
    }
#endif

    /* Execute Compositor render commands. */
    {
      context_.set_scene(DRW_context_get()->scene);
      context_.set_info_message("");
      compositor::Evaluator evaluator(context_);
      evaluator.evaluate();
    }

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
