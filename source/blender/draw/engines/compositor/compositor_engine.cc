/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_listbase.h"
#include "BLI_math_vector_types.hh"
#include "BLI_rect.h"
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

 public:
  Context(char *info_message) : compositor::Context(), info_message_(info_message) {}

  const Scene &get_scene() const override
  {
    return *DRW_context_state_get()->scene;
  }

  const bNodeTree &get_node_tree() const override
  {
    return *DRW_context_state_get()->scene->nodetree;
  }

  bool use_gpu() const override
  {
    return true;
  }

  eCompositorDenoiseQaulity get_denoise_quality() const override
  {
    return static_cast<eCompositorDenoiseQaulity>(
        this->get_render_data().compositor_denoise_preview_quality);
  }

  compositor::OutputTypes needed_outputs() const override
  {
    return compositor::OutputTypes::Composite | compositor::OutputTypes::Viewer;
  }

  /* The viewport compositor does not support viewer outputs, so treat viewers as composite
   * outputs. */
  bool treat_viewer_as_composite_output() const override
  {
    return true;
  }

  const RenderData &get_render_data() const override
  {
    return DRW_context_state_get()->scene->r;
  }

  int2 get_render_size() const override
  {
    return int2(DRW_viewport_size_get());
  }

  /* We limit the compositing region to the camera region if in camera view, while we use the
   * entire viewport otherwise. We also use the entire viewport when doing viewport rendering since
   * the viewport is already the camera region in that case. */
  rcti get_compositing_region() const override
  {
    const int2 viewport_size = int2(DRW_viewport_size_get());
    const rcti render_region = rcti{0, viewport_size.x, 0, viewport_size.y};

    if (DRW_context_state_get()->rv3d->persp != RV3D_CAMOB || DRW_state_is_viewport_image_render())
    {
      return render_region;
    }

    rctf camera_border;
    ED_view3d_calc_camera_border(DRW_context_state_get()->scene,
                                 DRW_context_state_get()->depsgraph,
                                 DRW_context_state_get()->region,
                                 DRW_context_state_get()->v3d,
                                 DRW_context_state_get()->rv3d,
                                 false,
                                 &camera_border);

    rcti camera_region;
    BLI_rcti_rctf_copy_floor(&camera_region, &camera_border);

    rcti visible_camera_region;
    BLI_rcti_isect(&render_region, &camera_region, &visible_camera_region);

    return visible_camera_region;
  }

  compositor::Result get_output_result() override
  {
    compositor::Result result = this->create_result(compositor::ResultType::Color,
                                                    compositor::ResultPrecision::Half);
    result.wrap_external(DRW_viewport_texture_list_get()->color);
    return result;
  }

  compositor::Result get_viewer_output_result(compositor::Domain /*domain*/,
                                              bool /*is_data*/,
                                              compositor::ResultPrecision /*precision*/) override
  {
    compositor::Result result = this->create_result(compositor::ResultType::Color,
                                                    compositor::ResultPrecision::Half);
    result.wrap_external(DRW_viewport_texture_list_get()->color);
    return result;
  }

  compositor::Result get_pass(const Scene *scene, int view_layer, const char *pass_name) override
  {
    if (DEG_get_original_id(const_cast<ID *>(&scene->id)) !=
        DEG_get_original_id(&DRW_context_state_get()->scene->id))
    {
      return compositor::Result(*this);
    }

    if (view_layer != 0) {
      return compositor::Result(*this);
    }

    /* The combined pass is a special case where we return the viewport color texture, because it
     * includes Grease Pencil objects since GP is drawn using their own engine. */
    if (STREQ(pass_name, RE_PASSNAME_COMBINED)) {
      GPUTexture *combined_texture = DRW_viewport_texture_list_get()->color;
      compositor::Result pass = compositor::Result(*this, GPU_texture_format(combined_texture));
      pass.wrap_external(combined_texture);
      return pass;
    }

    /* Return the pass that was written by the engine if such pass was found. */
    GPUTexture *pass_texture = DRW_viewport_pass_texture_get(pass_name).gpu_texture();
    if (pass_texture) {
      compositor::Result pass = compositor::Result(*this, GPU_texture_format(pass_texture));
      pass.wrap_external(pass_texture);
      return pass;
    }

    return compositor::Result(*this);
  }

  StringRef get_view_name() const override
  {
    const SceneRenderView *view = static_cast<SceneRenderView *>(
        BLI_findlink(&get_render_data().views, DRW_context_state_get()->v3d->multiview_eye));
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

  IDRecalcFlag query_id_recalc_flag(ID *id) const override
  {
    DrawEngineType *owner = &draw_engine_compositor_type;
    DrawData *draw_data = DRW_drawdata_ensure(id, owner, sizeof(DrawData), nullptr, nullptr);
    IDRecalcFlag recalc_flag = IDRecalcFlag(draw_data->recalc);
    draw_data->recalc = IDRecalcFlag(0);
    return recalc_flag;
  }
};

class Engine {
 private:
  Context context_;
  compositor::Evaluator evaluator_;
  /* Stores the compositing region size at the time the last compositor evaluation happened. See
   * the update_compositing_region_size method for more information. */
  int2 last_compositing_region_size_;

 public:
  Engine(char *info_message)
      : context_(info_message),
        evaluator_(context_),
        last_compositing_region_size_(context_.get_compositing_region_size())
  {
  }

  /* Update the compositing region size and evaluate the compositor. */
  void draw()
  {
    update_compositing_region_size();
    /* We temporally disable caching of node tree compilation by always resting the evaluator for
     * now. See pull request #134394 for more information. TODO: This should be cleaned up in the
     * future. */
    evaluator_.reset();
    evaluator_.evaluate();
  }

  /* If the size of the compositing region changed from the last time the compositor was evaluated,
   * update the last compositor region size and reset the evaluator. That's because the evaluator
   * compiles the node tree in a manner that is specifically optimized for the size of the
   * compositing region. This should be called before evaluating the compositor. */
  void update_compositing_region_size()
  {
    if (last_compositing_region_size_ == context_.get_compositing_region_size()) {
      return;
    }

    last_compositing_region_size_ = context_.get_compositing_region_size();

    evaluator_.reset();
  }

  /* If the compositor node tree changed, reset the evaluator. */
  void update(const Depsgraph *depsgraph)
  {
    if (DEG_id_type_updated(depsgraph, ID_NT)) {
      evaluator_.reset();
    }
  }
};

}  // namespace blender::draw::compositor_engine

using namespace blender::draw::compositor_engine;

struct COMPOSITOR_Data {
  DrawEngineType *engine_type;
  Engine *instance_data;
  char info[GPU_INFO_SIZE];
};

static void compositor_engine_init(void *data)
{
  COMPOSITOR_Data *compositor_data = static_cast<COMPOSITOR_Data *>(data);

  if (!compositor_data->instance_data) {
    compositor_data->instance_data = new Engine(compositor_data->info);
  }
}

static void compositor_engine_free(void *instance_data)
{
  Engine *engine = static_cast<Engine *>(instance_data);
  delete engine;
}

static void compositor_engine_draw(void *data)
{
  COMPOSITOR_Data *compositor_data = static_cast<COMPOSITOR_Data *>(data);

#if defined(__APPLE__)
  if (GPU_backend_get_type() == GPU_BACKEND_METAL) {
    /* NOTE(Metal): Isolate Compositor compute work in individual command buffer to improve
     * workload scheduling. When expensive compositor nodes are in the graph, these can stall out
     * the GPU for extended periods of time and sub-optimally schedule work for execution. */
    GPU_flush();
  }
#endif

  /* Execute Compositor render commands. */
  compositor_data->instance_data->draw();

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
}

static void compositor_engine_update(void *data)
{
  COMPOSITOR_Data *compositor_data = static_cast<COMPOSITOR_Data *>(data);

  /* Clear any info message that was set in a previous update. */
  compositor_data->info[0] = '\0';

  if (compositor_data->instance_data) {
    compositor_data->instance_data->update(DRW_context_state_get()->depsgraph);
  }
}

DrawEngineType draw_engine_compositor_type = {
    /*next*/ nullptr,
    /*prev*/ nullptr,
    /*idname*/ N_("Compositor"),
    /*engine_init*/ &compositor_engine_init,
    /*engine_free*/ nullptr,
    /*instance_free*/ &compositor_engine_free,
    /*cache_init*/ nullptr,
    /*cache_populate*/ nullptr,
    /*cache_finish*/ nullptr,
    /*draw_scene*/ &compositor_engine_draw,
    /*view_update*/ &compositor_engine_update,
    /*id_update*/ nullptr,
    /*render_to_image*/ nullptr,
    /*store_metadata*/ nullptr,
};
