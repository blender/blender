/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_listbase.h"
#include "BLI_math_vector_types.hh"
#include "BLI_rect.h"
#include "BLI_string_ref.hh"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_ID_enums.h"
#include "DNA_camera_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_vec_types.h"
#include "DNA_view3d_types.h"

#include "DEG_depsgraph_query.h"

#include "ED_view3d.h"

#include "DRW_render.h"

#include "IMB_colormanagement.h"

#include "COM_context.hh"
#include "COM_evaluator.hh"
#include "COM_texture_pool.hh"

#include "GPU_context.h"
#include "GPU_texture.h"

#include "compositor_engine.h" /* Own include. */

namespace blender::draw::compositor {

class TexturePool : public realtime_compositor::TexturePool {
 public:
  GPUTexture *allocate_texture(int2 size, eGPUTextureFormat format) override
  {
    DrawEngineType *owner = (DrawEngineType *)this;
    return DRW_texture_pool_query_2d(size.x, size.y, format, owner);
  }
};

class Context : public realtime_compositor::Context {
 private:
  /* A pointer to the info message of the compositor engine. This is a char array of size
   * GPU_INFO_SIZE. The message is cleared prior to updating or evaluating the compositor. */
  char *info_message_;

 public:
  Context(realtime_compositor::TexturePool &texture_pool, char *info_message)
      : realtime_compositor::Context(texture_pool), info_message_(info_message)
  {
  }

  const Scene *get_scene() const override
  {
    return DRW_context_state_get()->scene;
  }

  int2 get_render_size() const override
  {
    return int2(float2(DRW_viewport_size_get()));
  }

  /* Returns true if the viewport is in camera view and has an opaque passepartout, that is, the
   * area outside of the camera border is not visible. */
  bool is_opaque_camera_view() const
  {
    /* Check if the viewport is in camera view. */
    if (DRW_context_state_get()->rv3d->persp != RV3D_CAMOB) {
      return false;
    }

    /* Check if the camera object that is currently in view is an actual camera. It is possible for
     * a non camera object to be used as a camera, in which case, there will be no passepartout or
     * any other camera setting, so those pseudo cameras can be ignored. */
    Object *camera_object = DRW_context_state_get()->v3d->camera;
    if (camera_object->type != OB_CAMERA) {
      return false;
    }

    /* Check if the camera has passepartout active and is totally opaque. */
    Camera *cam = static_cast<Camera *>(camera_object->data);
    if (!(cam->flag & CAM_SHOWPASSEPARTOUT) || cam->passepartalpha != 1.0f) {
      return false;
    }

    return true;
  }

  rcti get_compositing_region() const override
  {
    const int2 viewport_size = int2(float2(DRW_viewport_size_get()));
    const rcti render_region = rcti{0, viewport_size.x, 0, viewport_size.y};

    /* If the camera view is not opaque, that means the content outside of the camera region is
     * visible to some extent, so it would make sense to include them in the compositing region.
     * Otherwise, we limit the compositing region to the visible camera region because anything
     * outside of the camera region will not be visible anyways. */
    if (!is_opaque_camera_view()) {
      return render_region;
    }

    rctf camera_border;
    ED_view3d_calc_camera_border(DRW_context_state_get()->scene,
                                 DRW_context_state_get()->depsgraph,
                                 DRW_context_state_get()->region,
                                 DRW_context_state_get()->v3d,
                                 DRW_context_state_get()->rv3d,
                                 &camera_border,
                                 false);

    rcti camera_region;
    BLI_rcti_rctf_copy_floor(&camera_region, &camera_border);

    rcti visible_camera_region;
    BLI_rcti_isect(&render_region, &camera_region, &visible_camera_region);

    return visible_camera_region;
  }

  GPUTexture *get_output_texture() override
  {
    return DRW_viewport_texture_list_get()->color;
  }

  GPUTexture *get_input_texture(int /*view_layer*/, eScenePassType /*pass_type*/) override
  {
    return get_output_texture();
  }

  StringRef get_view_name() override
  {
    const SceneRenderView *view = static_cast<SceneRenderView *>(
        BLI_findlink(&get_scene()->r.views, DRW_context_state_get()->v3d->multiview_eye));
    return view->name;
  }

  void set_info_message(StringRef message) const override
  {
    message.copy(info_message_, GPU_INFO_SIZE);
  }
};

class Engine {
 private:
  TexturePool texture_pool_;
  Context context_;
  realtime_compositor::Evaluator evaluator_;
  /* Stores the compositing region size at the time the last compositor evaluation happened. See
   * the update_compositing_region_size method for more information. */
  int2 last_compositing_region_size_;

 public:
  Engine(char *info_message)
      : context_(texture_pool_, info_message),
        evaluator_(context_),
        last_compositing_region_size_(context_.get_compositing_region_size())
  {
  }

  /* Update the compositing region size and evaluate the compositor. */
  void draw()
  {
    update_compositing_region_size();
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

}  // namespace blender::draw::compositor

using namespace blender::draw::compositor;

struct COMPOSITOR_Data {
  DrawEngineType *engine_type;
  DRWViewportEmptyList *fbl;
  DRWViewportEmptyList *txl;
  DRWViewportEmptyList *psl;
  DRWViewportEmptyList *stl;
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
  else {
    /* Realtime Compositor is not supported on macOS with the OpenGL backend. */
    blender::StringRef("Viewport compositor is only supported on MacOS with the Metal Backend.")
        .copy(compositor_data->info, GPU_INFO_SIZE);
    return;
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

extern "C" {

static const DrawEngineDataSize compositor_data_size = DRW_VIEWPORT_DATA_SIZE(COMPOSITOR_Data);

DrawEngineType draw_engine_compositor_type = {
    /*next*/ nullptr,
    /*prev*/ nullptr,
    /*idname*/ N_("Compositor"),
    /*vedata_size*/ &compositor_data_size,
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
}
