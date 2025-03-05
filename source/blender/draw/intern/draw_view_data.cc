/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include <memory>

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_vector.hh"

#include "GPU_viewport.hh"

#include "DRW_gpu_wrapper.hh"
#include "DRW_render.hh"

#include "draw_manager_text.hh"

#include "draw_manager.hh"
#include "draw_manager_c.hh"
#include "draw_view_data.hh"

#include "engines/compositor/compositor_engine.h"
#include "engines/eevee_next/eevee_engine.h"
#include "engines/external/external_engine.h"
#include "engines/gpencil/gpencil_engine.h"
#include "engines/image/image_engine.h"
#include "engines/overlay/overlay_engine.h"
#include "engines/select/select_engine.hh"
#include "engines/workbench/workbench_engine.h"

using namespace blender;

DRWViewData::DRWViewData()
    : eevee(DRW_engine_viewport_eevee_next_type.draw_engine),
      workbench(DRW_engine_viewport_workbench_type.draw_engine),
      external(&draw_engine_external_type),
      image(&draw_engine_image_type),
      grease_pencil(&draw_engine_gpencil_type),
      overlay(&draw_engine_overlay_next_type),
      object_select(&draw_engine_select_next_type),
      edit_select(&draw_engine_select_type),
#ifdef WITH_DRAW_DEBUG
      edit_select_debug(DRW_engine_viewport_select_type.draw_engine),
#endif
      compositor(&draw_engine_compositor_type)
{
  manager = new draw::Manager();
};

DRWViewData::~DRWViewData()
{
  this->clear(true);
  delete manager;
};

draw::TextureFromPool &DRW_view_data_pass_texture_get(DRWViewData *view_data,
                                                      const char *pass_name)
{
  return *view_data->viewport_compositor_passes.lookup_or_add_cb(
      pass_name, [&]() { return std::make_unique<draw::TextureFromPool>(pass_name); });
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
  DrawEngineType *engine_type = data->draw_engine;

  if (clear_instance_data && data->instance_data) {
    BLI_assert(engine_type->instance_free != nullptr);
    engine_type->instance_free(data->instance_data);
    data->instance_data = nullptr;
  }

  if (data->text_draw_cache) {
    DRW_text_cache_destroy(data->text_draw_cache);
    data->text_draw_cache = nullptr;
  }
}

void DRWViewData::clear(bool free_instance_data)
{
  GPU_FRAMEBUFFER_FREE_SAFE(this->dfbl.default_fb);
  GPU_FRAMEBUFFER_FREE_SAFE(this->dfbl.overlay_fb);
  GPU_FRAMEBUFFER_FREE_SAFE(this->dfbl.in_front_fb);
  GPU_FRAMEBUFFER_FREE_SAFE(this->dfbl.color_only_fb);
  GPU_FRAMEBUFFER_FREE_SAFE(this->dfbl.depth_only_fb);
  GPU_FRAMEBUFFER_FREE_SAFE(this->dfbl.overlay_only_fb);

  if (!this->from_viewport) {
    GPU_TEXTURE_FREE_SAFE(this->dtxl.color);
    GPU_TEXTURE_FREE_SAFE(this->dtxl.color_overlay);
    GPU_TEXTURE_FREE_SAFE(this->dtxl.depth);
  }
  GPU_TEXTURE_FREE_SAFE(this->dtxl.depth_in_front);

  foreach_engine([&](ViewportEngineData *data, DrawEngineType * /*engine*/) {
    draw_viewport_engines_data_clear(data, free_instance_data);
  });
}

void DRWViewData::texture_list_size_validate(const blender::int2 &size)
{
  if (this->texture_list_size != size) {
    this->clear(false);
    copy_v2_v2_int(this->texture_list_size, size);
  }
}

ViewportEngineData *DRW_view_data_engine_data_get_ensure(DRWViewData *view_data,
                                                         DrawEngineType *engine_type)
{
  ViewportEngineData *result = nullptr;
  view_data->foreach_engine([&](ViewportEngineData *data, DrawEngineType *engine) {
    if (engine_type == engine) {
      result = data;
    }
  });
  return result;
}

void DRW_view_data_use_engine(DRWViewData *view_data, DrawEngineType *engine_type)
{
  ViewportEngineData *engine = DRW_view_data_engine_data_get_ensure(view_data, engine_type);
  engine->used = true;
}

void DRW_view_data_reset(DRWViewData *view_data)
{
  view_data->foreach_enabled_engine(
      [&](ViewportEngineData *data, DrawEngineType * /*engine*/) { data->used = false; });

  for (std::unique_ptr<draw::TextureFromPool> &texture :
       view_data->viewport_compositor_passes.values())
  {
    texture->release();
  }
  view_data->viewport_compositor_passes.clear();
}

void DRW_view_data_free_unused(DRWViewData *view_data)
{
  view_data->foreach_engine([&](ViewportEngineData *data, DrawEngineType * /*engine*/) {
    if (data->used == false) {
      draw_viewport_engines_data_clear(data, false);
    }
  });
}

void DRW_view_data_engines_view_update(DRWViewData *view_data)
{
  view_data->foreach_enabled_engine([&](ViewportEngineData *data, DrawEngineType *engine) {
    if (engine->view_update) {
      engine->view_update(data);
    }
  });
}

DefaultFramebufferList *DRW_view_data_default_framebuffer_list_get(DRWViewData *view_data)
{
  return &view_data->dfbl;
}

DefaultTextureList *DRW_view_data_default_texture_list_get(DRWViewData *view_data)
{
  return &view_data->dtxl;
}

draw::Manager *DRW_manager_get()
{
  BLI_assert(drw_get().view_data_active->manager);
  return drw_get().view_data_active->manager;
}

void DRW_manager_begin_sync()
{
  if (drw_get().view_data_active->manager == nullptr) {
    return;
  }
  drw_get().view_data_active->manager->begin_sync();
}

void DRW_manager_end_sync()
{
  if (drw_get().view_data_active->manager == nullptr) {
    return;
  }
  drw_get().view_data_active->manager->end_sync();
}
