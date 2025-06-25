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

#include "draw_context_private.hh"
#include "draw_manager.hh"
#include "draw_view_data.hh"

using namespace blender;

DRWViewData::DRWViewData()
{
  manager = new draw::Manager();
};

DRWViewData::~DRWViewData()
{
  this->clear(true);
  delete manager;
};

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

  if (free_instance_data) {
    foreach_engine([&](DrawEngine::Pointer &ptr) {
      if (ptr.instance) {
        ptr.free_instance();
      }
    });
  }
}

void DRWViewData::texture_list_size_validate(const blender::int2 &size)
{
  if (this->texture_list_size != size) {
    this->clear(false);
    copy_v2_v2_int(this->texture_list_size, size);
  }
}

void DRW_view_data_reset(DRWViewData *view_data)
{
  view_data->foreach_enabled_engine([&](DrawEngine &instance) { instance.used = false; });

  for (std::unique_ptr<draw::TextureFromPool> &texture :
       view_data->viewport_compositor_passes.values())
  {
    texture->release();
  }
  view_data->viewport_compositor_passes.clear();
}

void DRW_view_data_free_unused(DRWViewData *view_data)
{
  view_data->foreach_engine([&](DrawEngine::Pointer &ptr) {
    if (ptr.instance && ptr.instance->used == false) {
      ptr.free_instance();
    }
  });
}

draw::Manager *DRW_manager_get()
{
  BLI_assert(drw_get().view_data_active->manager);
  return drw_get().view_data_active->manager;
}
