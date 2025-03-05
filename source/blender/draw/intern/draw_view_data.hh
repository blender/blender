/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * Engine data
 * Structure containing each draw engine instance data.
 */

#pragma once

#include "BLI_map.hh"
#include "BLI_math_vector_types.hh"

#include "DRW_render.hh"
#include "draw_manager_c.hh"

#define GPU_INFO_SIZE 512 /* IMA_MAX_RENDER_TEXT_SIZE */

namespace blender::draw {
class TextureFromPool;
class Manager;
}  // namespace blender::draw

struct DrawEngineType;
struct DRWTextStore;
struct GPUFrameBuffer;
struct GPUTexture;
struct GPUViewport;
struct ListBase;

struct ViewportEngineData {
  /* Not owning pointer to the draw engine. */
  DrawEngineType *draw_engine;

  /**
   * \brief Memory block that can be freely used by the draw engine.
   * When used the draw engine must implement #DrawEngineType.instance_free callback.
   */
  void *instance_data = nullptr;

  char info[GPU_INFO_SIZE] = {'\0'};

  /* we may want to put this elsewhere */
  DRWTextStore *text_draw_cache = nullptr;

  bool used = false;

  ViewportEngineData(DrawEngineType *engine_type) : draw_engine(engine_type) {}
};

struct ViewportEngineData_Info {
  int fbl_len;
  int txl_len;
  int psl_len;
  int stl_len;
};

/* Buffer and textures used by the viewport by default */
struct DefaultFramebufferList {
  GPUFrameBuffer *default_fb;
  GPUFrameBuffer *overlay_fb;
  GPUFrameBuffer *in_front_fb;
  GPUFrameBuffer *color_only_fb;
  GPUFrameBuffer *depth_only_fb;
  GPUFrameBuffer *overlay_only_fb;
};

struct DefaultTextureList {
  GPUTexture *color;
  GPUTexture *color_overlay;
  GPUTexture *depth;
  GPUTexture *depth_in_front;
};

struct DRWViewData {
 public:
  DefaultFramebufferList dfbl = {};
  DefaultTextureList dtxl = {};
  /** True indicates the textures inside dtxl are from the viewport and should not be freed. */
  bool from_viewport = false;
  /** Common size for texture in the engines texture list.
   * We free all texture lists if it changes. */
  blender::int2 texture_list_size = {0, 0};

  /* Engines running for this viewport. nullptr if not enabled. */
  /* TODO(fclem): Directly use each engine class. */
  ViewportEngineData eevee;
  ViewportEngineData workbench;
  ViewportEngineData external;
  ViewportEngineData image;
  ViewportEngineData grease_pencil;
  ViewportEngineData overlay;
  ViewportEngineData object_select;
  ViewportEngineData edit_select;
#ifdef WITH_DRAW_DEBUG
  ViewportEngineData edit_select_debug;
#endif
  ViewportEngineData compositor;

  /* Stores passes needed by the viewport compositor. Engines are expected to populate those in
   * every redraw using calls to the DRW_viewport_pass_texture_get function. The compositor can
   * then call the same function to retrieve the passes it needs, which are expected to be
   * initialized. Those textures are release when view data is reset. */
  blender::Map<std::string, std::unique_ptr<blender::draw::TextureFromPool>>
      viewport_compositor_passes;

  /** New per view/viewport manager. Null if not supported by current hardware. */
  blender::draw::Manager *manager = nullptr;

 public:
  DRWViewData();
  ~DRWViewData();

  void texture_list_size_validate(const blender::int2 &size);

  template<typename CallbackT> void foreach_engine(CallbackT callback)
  {
    /* IMPORTANT: Order here defines the draw order. */

    /* Render engines. Output to the render result framebuffer. Mutually exclusive. */
    callback(&eevee, eevee.draw_engine);
    callback(&workbench, workbench.draw_engine);
    callback(&external, external.draw_engine);
    callback(&image, image.draw_engine);
#ifdef WITH_DRAW_DEBUG
    callback(&edit_select_debug, edit_select_debug.draw_engine);
#endif
    /* Grease pencil. Merge its output to the render result framebuffer. */
    callback(&grease_pencil, grease_pencil.draw_engine);
    /* GPU compositor. Processes render result and output to the render result framebuffer. */
    callback(&compositor, compositor.draw_engine);
    /* Overlays. Draw on a separate overlay framebuffer. Can read render result. */
    callback(&overlay, overlay.draw_engine);

    /* Selection. Are always enabled alone and have no interaction with other engines. */
    callback(&object_select, object_select.draw_engine);
    callback(&edit_select, edit_select.draw_engine);
  }

  template<typename CallbackT> void foreach_enabled_engine(CallbackT callback)
  {
    foreach_engine([&](ViewportEngineData *data, DrawEngineType *engine) {
      if (!data->used) {
        return;
      }
      callback(data, engine);
    });
  }

 private:
  void clear(bool free_instance_data);
};

/* Returns a TextureFromPool stored in the given view data for the pass identified by the given
 * pass name. Engines should call this function for each of the passes needed by the viewport
 * compositor in every redraw, then it should allocate the texture and write the pass data to it.
 * The texture should cover the entire viewport. */
blender::draw::TextureFromPool &DRW_view_data_pass_texture_get(DRWViewData *view_data,
                                                               const char *pass_name);

void DRW_view_data_default_lists_from_viewport(DRWViewData *view_data, GPUViewport *viewport);
ViewportEngineData *DRW_view_data_engine_data_get_ensure(DRWViewData *view_data,
                                                         DrawEngineType *engine_type);
void DRW_view_data_use_engine(DRWViewData *view_data, DrawEngineType *engine_type);
void DRW_view_data_reset(DRWViewData *view_data);
void DRW_view_data_free_unused(DRWViewData *view_data);
void DRW_view_data_engines_view_update(DRWViewData *view_data);
DefaultFramebufferList *DRW_view_data_default_framebuffer_list_get(DRWViewData *view_data);
DefaultTextureList *DRW_view_data_default_texture_list_get(DRWViewData *view_data);
