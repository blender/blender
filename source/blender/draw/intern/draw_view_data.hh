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

#define GPU_INFO_SIZE 512 /* IMA_MAX_RENDER_TEXT_SIZE */

struct DRWPass;
struct DRWTextStore;
struct DRWRegisteredDrawEngine;
struct DrawEngineType;
struct GPUViewport;

/* NOTE: these structs are only here for reading the actual lists from the engine.
 * The actual length of them is stored in a ViewportEngineData_Info.
 * The length of 1 is just here to avoid compiler warning. */
struct FramebufferList {
  GPUFrameBuffer *framebuffers[1];
};

struct TextureList {
  GPUTexture *textures[1];
};

struct PassList {
  DRWPass *passes[1];
};

/* Stores custom structs from the engine that have been MEM_(m/c)allocN'ed. */
struct StorageList {
  void *storage[1];
};

struct ViewportEngineData {
  /* Not owning pointer to the draw engine. */
  DRWRegisteredDrawEngine *engine_type;

  FramebufferList *fbl;
  TextureList *txl;
  PassList *psl;
  StorageList *stl;
  /**
   * \brief Memory block that can be freely used by the draw engine.
   * When used the draw engine must implement #DrawEngineType.instance_free callback.
   */
  void *instance_data;

  char info[GPU_INFO_SIZE];

  /* we may want to put this elsewhere */
  DRWTextStore *text_draw_cache;

  /* Profiling data */
  double init_time;
  double render_time;
  double background_time;
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

struct DRWViewData;

/**
 * Creates a view data with all possible engines type for this view.
 *
 * `engine_types` contains #DRWRegisteredDrawEngine.
 */
DRWViewData *DRW_view_data_create(ListBase *engine_types);
void DRW_view_data_free(DRWViewData *view_data);

void DRW_view_data_default_lists_from_viewport(DRWViewData *view_data, GPUViewport *viewport);
void DRW_view_data_texture_list_size_validate(DRWViewData *view_data, const int size[2]);
ViewportEngineData *DRW_view_data_engine_data_get_ensure(DRWViewData *view_data,
                                                         DrawEngineType *engine_type_);
void DRW_view_data_use_engine(DRWViewData *view_data, DrawEngineType *engine_type);
void DRW_view_data_reset(DRWViewData *view_data);
void DRW_view_data_free_unused(DRWViewData *view_data);
void DRW_view_data_engines_view_update(DRWViewData *view_data);
double *DRW_view_data_cache_time_get(DRWViewData *view_data);
DefaultFramebufferList *DRW_view_data_default_framebuffer_list_get(DRWViewData *view_data);
DefaultTextureList *DRW_view_data_default_texture_list_get(DRWViewData *view_data);

struct DRWEngineIterator {
  int id, end;
  ViewportEngineData **engines;
};

/* Iterate over used engines of this view_data. */
void DRW_view_data_enabled_engine_iter_begin(DRWEngineIterator *iterator, DRWViewData *view_data);
ViewportEngineData *DRW_view_data_enabled_engine_iter_step(DRWEngineIterator *iterator);

#define DRW_ENABLED_ENGINE_ITER(view_data_, engine_, data_) \
  DRWEngineIterator iterator; \
  ViewportEngineData *data_; \
  DrawEngineType *engine_; \
  DRW_view_data_enabled_engine_iter_begin(&iterator, view_data_); \
  /* WATCH Comma operator trickery ahead! This tests engine_ == nullptr. */ \
  while ((data_ = DRW_view_data_enabled_engine_iter_step(&iterator), \
          engine_ = (data_ != nullptr) ? (DrawEngineType *)data_->engine_type->draw_engine : \
                                         nullptr))
