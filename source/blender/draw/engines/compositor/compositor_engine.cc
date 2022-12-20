/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_listbase.h"
#include "BLI_math_vec_types.hh"
#include "BLI_string_ref.hh"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_ID_enums.h"
#include "DNA_scene_types.h"

#include "DEG_depsgraph_query.h"

#include "DRW_render.h"

#include "IMB_colormanagement.h"

#include "COM_context.hh"
#include "COM_evaluator.hh"
#include "COM_texture_pool.hh"

#include "GPU_texture.h"

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

  int2 get_output_size() override
  {
    return int2(float2(DRW_viewport_size_get()));
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
  /* Stores the viewport size at the time the last compositor evaluation happened. See the
   * update_viewport_size method for more information. */
  int2 last_viewport_size_;

 public:
  Engine(char *info_message)
      : context_(texture_pool_, info_message),
        evaluator_(context_),
        last_viewport_size_(context_.get_output_size())
  {
  }

  /* Update the viewport size and evaluate the compositor. */
  void draw()
  {
    update_viewport_size();
    evaluator_.evaluate();
  }

  /* If the size of the viewport changed from the last time the compositor was evaluated, update
   * the viewport size and reset the evaluator. That's because the evaluator compiles the node tree
   * in a manner that is specifically optimized for the size of the viewport. This should be called
   * before evaluating the compositor. */
  void update_viewport_size()
  {
    if (last_viewport_size_ == context_.get_output_size()) {
      return;
    }

    last_viewport_size_ = context_.get_output_size();

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
  blender::StringRef("Viewport compositor not supported on MacOS")
      .copy(compositor_data->info, GPU_INFO_SIZE);
  return;
#endif

  compositor_data->instance_data->draw();
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
    nullptr,                   /* next */
    nullptr,                   /* prev */
    N_("Compositor"),          /* idname */
    &compositor_data_size,     /* vedata_size */
    &compositor_engine_init,   /* engine_init */
    nullptr,                   /* engine_free */
    &compositor_engine_free,   /* instance_free */
    nullptr,                   /* cache_init */
    nullptr,                   /* cache_populate */
    nullptr,                   /* cache_finish */
    &compositor_engine_draw,   /* draw_scene */
    &compositor_engine_update, /* view_update */
    nullptr,                   /* id_update */
    nullptr,                   /* render_to_image */
    nullptr,                   /* store_metadata */
};
}
