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
#include "draw_context_private.hh"

#include "engines/compositor/compositor_engine.h"
#include "engines/eevee/eevee_engine.h"
#include "engines/external/external_engine.h"
#include "engines/gpencil/gpencil_engine.hh"
#include "engines/image/image_engine.h"
#include "engines/overlay/overlay_engine.h"
#include "engines/select/select_engine.hh"
#include "engines/workbench/workbench_engine.h"

#define GPU_INFO_SIZE 512 /* IMA_MAX_RENDER_TEXT_SIZE */

namespace blender::draw {
class TextureFromPool;
class Manager;
}  // namespace blender::draw

struct DRWTextStore;
namespace blender::gpu {
class FrameBuffer;
class Texture;
}  // namespace blender::gpu
struct GPUViewport;
struct ListBase;

/** Buffer and textures used by the viewport by default. */
struct DefaultFramebufferList {
  blender::gpu::FrameBuffer *default_fb;
  blender::gpu::FrameBuffer *overlay_fb;
  blender::gpu::FrameBuffer *in_front_fb;
  blender::gpu::FrameBuffer *color_only_fb;
  blender::gpu::FrameBuffer *depth_only_fb;
  blender::gpu::FrameBuffer *overlay_only_fb;
};

struct DefaultTextureList {
  blender::gpu::Texture *color;
  blender::gpu::Texture *color_overlay;
  blender::gpu::Texture *depth;
  blender::gpu::Texture *depth_in_front;
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

  /** Engines running for this viewport. nullptr if not enabled. */
  blender::eevee::Engine eevee;
  blender::workbench::Engine workbench;
  blender::draw::external::Engine external;
  blender::image_engine::Engine image;
  blender::draw::gpencil::Engine grease_pencil;
  blender::draw::overlay::Engine overlay;
  blender::draw::select::Engine object_select;
  blender::draw::edit_select::Engine edit_select;
#ifdef WITH_DRAW_DEBUG
  blender::draw::edit_select_debug::Engine edit_select_debug;
#endif
  blender::draw::compositor_engine::Engine compositor;

  /**
   * Stores passes needed by the viewport compositor. Engines are expected to populate those in
   * every redraw using calls to the #DRW_viewport_pass_texture_get function. The compositor can
   * then call the same function to retrieve the passes it needs, which are expected to be
   * initialized. Those textures are release when view data is reset.
   */
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

    /* Render engines. Output to the render result frame-buffer. Mutually exclusive. */

    callback(eevee);
    callback(workbench);
    callback(external);
    callback(image);
#ifdef WITH_DRAW_DEBUG
    callback(edit_select_debug);
#endif
    /** Grease pencil. Merge its output to the render result frame-buffer. */
    callback(grease_pencil);
    /** GPU compositor. Processes render result and output to the render result frame-buffer. */
    callback(compositor);
    /** Overlays. Draw on a separate overlay frame-buffer. Can read render result. */
    callback(overlay);

    /** Selection. Are always enabled alone and have no interaction with other engines. */
    callback(object_select);
    callback(edit_select);
  }

  template<typename CallbackT> void foreach_enabled_engine(CallbackT callback)
  {
    foreach_engine([&](DrawEngine::Pointer &ptr) {
      if (ptr.instance == nullptr || ptr.instance->used == false) {
        return;
      }
      callback(*ptr.instance);
    });
  }

 private:
  void clear(bool free_instance_data);
};

void DRW_view_data_default_lists_from_viewport(DRWViewData *view_data, GPUViewport *viewport);
void DRW_view_data_reset(DRWViewData *view_data);
void DRW_view_data_free_unused(DRWViewData *view_data);
