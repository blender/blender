/* SPDX-FileCopyrightText: 2021 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "BLI_vector.hh"

#include "GPU_capabilities.h"
#include "GPU_viewport.h"

#include "DRW_render.h"

#include "draw_instance_data.h"

#include "draw_manager_text.h"

#include "draw_manager.h"
#include "draw_manager.hh"
#include "draw_view_data.h"

using namespace blender;

struct DRWViewData {
  DefaultFramebufferList dfbl = {};
  DefaultTextureList dtxl = {};
  /** True indicates the textures inside dtxl are from the viewport and should not be freed. */
  bool from_viewport = false;
  /** Common size for texture in the engines texture list.
   * We free all texture lists if it changes. */
  int texture_list_size[2] = {0, 0};

  double cache_time = 0.0;

  Vector<ViewportEngineData> engines;
  Vector<ViewportEngineData *> enabled_engines;

  /** New per view/viewport manager. Null if not supported by current hardware. */
  draw::Manager *manager = nullptr;

  DRWViewData()
  {
    /* Only for GL >= 4.3 implementation for now. */
    if (GPU_shader_storage_buffer_objects_support() && GPU_compute_shader_support()) {
      manager = new draw::Manager();
    }
  };

  ~DRWViewData()
  {
    delete manager;
  };
};

DRWViewData *DRW_view_data_create(ListBase *engine_types)
{
  const int engine_types_len = BLI_listbase_count(engine_types);

  DRWViewData *view_data = new DRWViewData();
  view_data->engines.reserve(engine_types_len);
  LISTBASE_FOREACH (DRWRegisteredDrawEngine *, type, engine_types) {
    ViewportEngineData engine = {};
    engine.engine_type = type;
    view_data->engines.append(engine);
  }
  return view_data;
}

void DRW_view_data_default_lists_from_viewport(DRWViewData *view_data, GPUViewport *viewport)
{
  int active_view = GPU_viewport_active_view_get(viewport);
  view_data->from_viewport = true;

  DefaultFramebufferList *dfbl = &view_data->dfbl;
  DefaultTextureList *dtxl = &view_data->dtxl;
  /* Depth texture is shared between both stereo views. */
  dtxl->depth = GPU_viewport_depth_texture(viewport);
  dtxl->color = GPU_viewport_color_texture(viewport, active_view);
  dtxl->color_overlay = GPU_viewport_overlay_texture(viewport, active_view);

  GPU_framebuffer_ensure_config(&dfbl->default_fb,
                                {
                                    GPU_ATTACHMENT_TEXTURE(dtxl->depth),
                                    GPU_ATTACHMENT_TEXTURE(dtxl->color),
                                });
  GPU_framebuffer_ensure_config(&dfbl->overlay_fb,
                                {
                                    GPU_ATTACHMENT_TEXTURE(dtxl->depth),
                                    GPU_ATTACHMENT_TEXTURE(dtxl->color_overlay),
                                });
  GPU_framebuffer_ensure_config(&dfbl->depth_only_fb,
                                {
                                    GPU_ATTACHMENT_TEXTURE(dtxl->depth),
                                    GPU_ATTACHMENT_NONE,
                                });
  GPU_framebuffer_ensure_config(&dfbl->color_only_fb,
                                {
                                    GPU_ATTACHMENT_NONE,
                                    GPU_ATTACHMENT_TEXTURE(dtxl->color),
                                });
  GPU_framebuffer_ensure_config(&dfbl->overlay_only_fb,
                                {
                                    GPU_ATTACHMENT_NONE,
                                    GPU_ATTACHMENT_TEXTURE(dtxl->color_overlay),
                                });
}

static void draw_viewport_engines_data_clear(ViewportEngineData *data, bool clear_instance_data)
{
  DrawEngineType *engine_type = data->engine_type->draw_engine;
  const DrawEngineDataSize *data_size = engine_type->vedata_size;

  for (int i = 0; data->fbl && i < data_size->fbl_len; i++) {
    GPU_FRAMEBUFFER_FREE_SAFE(data->fbl->framebuffers[i]);
  }
  for (int i = 0; data->txl && i < data_size->txl_len; i++) {
    GPU_TEXTURE_FREE_SAFE(data->txl->textures[i]);
  }
  for (int i = 0; data->stl && i < data_size->stl_len; i++) {
    MEM_SAFE_FREE(data->stl->storage[i]);
  }

  if (clear_instance_data && data->instance_data) {
    BLI_assert(engine_type->instance_free != nullptr);
    engine_type->instance_free(data->instance_data);
    data->instance_data = nullptr;
  }

  MEM_SAFE_FREE(data->fbl);
  MEM_SAFE_FREE(data->txl);
  MEM_SAFE_FREE(data->psl);
  MEM_SAFE_FREE(data->stl);

  if (data->text_draw_cache) {
    DRW_text_cache_destroy(data->text_draw_cache);
    data->text_draw_cache = nullptr;
  }
}

static void draw_view_data_clear(DRWViewData *view_data, bool free_instance_data)
{
  GPU_FRAMEBUFFER_FREE_SAFE(view_data->dfbl.default_fb);
  GPU_FRAMEBUFFER_FREE_SAFE(view_data->dfbl.overlay_fb);
  GPU_FRAMEBUFFER_FREE_SAFE(view_data->dfbl.in_front_fb);
  GPU_FRAMEBUFFER_FREE_SAFE(view_data->dfbl.color_only_fb);
  GPU_FRAMEBUFFER_FREE_SAFE(view_data->dfbl.depth_only_fb);
  GPU_FRAMEBUFFER_FREE_SAFE(view_data->dfbl.overlay_only_fb);

  if (!view_data->from_viewport) {
    GPU_TEXTURE_FREE_SAFE(view_data->dtxl.color);
    GPU_TEXTURE_FREE_SAFE(view_data->dtxl.color_overlay);
    GPU_TEXTURE_FREE_SAFE(view_data->dtxl.depth);
  }
  GPU_TEXTURE_FREE_SAFE(view_data->dtxl.depth_in_front);

  for (ViewportEngineData &engine : view_data->engines) {
    draw_viewport_engines_data_clear(&engine, free_instance_data);
  }
}

void DRW_view_data_free(DRWViewData *view_data)
{
  draw_view_data_clear(view_data, true);
  delete view_data;
}

void DRW_view_data_texture_list_size_validate(DRWViewData *view_data, const int size[2])
{
  if (!equals_v2v2_int(view_data->texture_list_size, size)) {
    draw_view_data_clear(view_data, false);
    copy_v2_v2_int(view_data->texture_list_size, size);
  }
}

ViewportEngineData *DRW_view_data_engine_data_get_ensure(DRWViewData *view_data,
                                                         DrawEngineType *engine_type)
{
  for (ViewportEngineData &engine : view_data->engines) {
    if (engine.engine_type->draw_engine == engine_type) {
      if (engine.fbl == nullptr) {
        const DrawEngineDataSize *data_size = engine_type->vedata_size;
        engine.fbl = (FramebufferList *)MEM_calloc_arrayN(
            data_size->fbl_len, sizeof(GPUFrameBuffer *), "FramebufferList");
        engine.txl = (TextureList *)MEM_calloc_arrayN(
            data_size->txl_len, sizeof(GPUTexture *), "TextureList");
        engine.psl = (PassList *)MEM_calloc_arrayN(
            data_size->psl_len, sizeof(DRWPass *), "PassList");
        engine.stl = (StorageList *)MEM_calloc_arrayN(
            data_size->stl_len, sizeof(void *), "StorageList");
      }
      return &engine;
    }
  }
  return nullptr;
}

void DRW_view_data_use_engine(DRWViewData *view_data, DrawEngineType *engine_type)
{
  ViewportEngineData *engine = DRW_view_data_engine_data_get_ensure(view_data, engine_type);
  view_data->enabled_engines.append(engine);
}

void DRW_view_data_reset(DRWViewData *view_data)
{
  view_data->enabled_engines.clear();
}

void DRW_view_data_free_unused(DRWViewData *view_data)
{
  for (ViewportEngineData &engine : view_data->engines) {
    if (view_data->enabled_engines.first_index_of_try(&engine) == -1) {
      draw_viewport_engines_data_clear(&engine, false);
    }
  }
}

void DRW_view_data_engines_view_update(DRWViewData *view_data)
{
  for (ViewportEngineData &engine_data : view_data->engines) {
    DrawEngineType *draw_engine = engine_data.engine_type->draw_engine;
    if (draw_engine->view_update) {
      draw_engine->view_update(&engine_data);
    }
  }
}

double *DRW_view_data_cache_time_get(DRWViewData *view_data)
{
  return &view_data->cache_time;
}

DefaultFramebufferList *DRW_view_data_default_framebuffer_list_get(DRWViewData *view_data)
{
  return &view_data->dfbl;
}

DefaultTextureList *DRW_view_data_default_texture_list_get(DRWViewData *view_data)
{
  return &view_data->dtxl;
}

void DRW_view_data_enabled_engine_iter_begin(DRWEngineIterator *iterator, DRWViewData *view_data)
{
  iterator->id = 0;
  iterator->end = view_data->enabled_engines.size();
  iterator->engines = view_data->enabled_engines.data();
}

ViewportEngineData *DRW_view_data_enabled_engine_iter_step(DRWEngineIterator *iterator)
{
  if (iterator->id >= iterator->end) {
    return nullptr;
  }
  ViewportEngineData *engine = iterator->engines[iterator->id++];
  return engine;
}

draw::Manager *DRW_manager_get()
{
  BLI_assert(DST.view_data_active->manager);
  return reinterpret_cast<draw::Manager *>(DST.view_data_active->manager);
}

draw::ObjectRef DRW_object_ref_get(Object *object)
{
  BLI_assert(DST.view_data_active->manager);
  return {object, DST.dupli_source, DST.dupli_parent};
}

void DRW_manager_begin_sync()
{
  if (DST.view_data_active->manager == nullptr) {
    return;
  }
  reinterpret_cast<draw::Manager *>(DST.view_data_active->manager)->begin_sync();
}

void DRW_manager_end_sync()
{
  if (DST.view_data_active->manager == nullptr) {
    return;
  }
  reinterpret_cast<draw::Manager *>(DST.view_data_active->manager)->end_sync();
}
