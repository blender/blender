/* SPDX-FileCopyrightText: 2021 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * Engine data
 * Structure containing each draw engine instance data.
 */

#pragma once

#define GPU_INFO_SIZE 512 /* IMA_MAX_RENDER_TEXT */

#ifdef __cplusplus
extern "C" {
#endif

struct DRWRegisteredDrawEngine;
struct DrawEngineType;
struct GPUViewport;

/* NOTE: these structs are only here for reading the actual lists from the engine.
 * The actual length of them is stored in a ViewportEngineData_Info.
 * The length of 1 is just here to avoid compiler warning. */
typedef struct FramebufferList {
  struct GPUFrameBuffer *framebuffers[1];
} FramebufferList;

typedef struct TextureList {
  struct GPUTexture *textures[1];
} TextureList;

typedef struct PassList {
  struct DRWPass *passes[1];
} PassList;

/* Stores custom structs from the engine that have been MEM_(m/c)allocN'ed. */
typedef struct StorageList {
  void *storage[1];
} StorageList;

typedef struct ViewportEngineData {
  /* Not owning pointer to the draw engine. */
  struct DRWRegisteredDrawEngine *engine_type;

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
  struct DRWTextStore *text_draw_cache;

  /* Profiling data */
  double init_time;
  double render_time;
  double background_time;
} ViewportEngineData;

typedef struct ViewportEngineData_Info {
  int fbl_len;
  int txl_len;
  int psl_len;
  int stl_len;
} ViewportEngineData_Info;

/* Buffer and textures used by the viewport by default */
typedef struct DefaultFramebufferList {
  struct GPUFrameBuffer *default_fb;
  struct GPUFrameBuffer *overlay_fb;
  struct GPUFrameBuffer *in_front_fb;
  struct GPUFrameBuffer *color_only_fb;
  struct GPUFrameBuffer *depth_only_fb;
  struct GPUFrameBuffer *overlay_only_fb;
} DefaultFramebufferList;

typedef struct DefaultTextureList {
  struct GPUTexture *color;
  struct GPUTexture *color_overlay;
  struct GPUTexture *depth;
  struct GPUTexture *depth_in_front;
} DefaultTextureList;

typedef struct DRWViewData DRWViewData;

/**
 * Creates a view data with all possible engines type for this view.
 *
 * `engine_types` contains #DRWRegisteredDrawEngine.
 */
DRWViewData *DRW_view_data_create(ListBase *engine_types);
void DRW_view_data_free(DRWViewData *view_data);

void DRW_view_data_default_lists_from_viewport(DRWViewData *view_data,
                                               struct GPUViewport *viewport);
void DRW_view_data_texture_list_size_validate(DRWViewData *view_data, const int size[2]);
ViewportEngineData *DRW_view_data_engine_data_get_ensure(DRWViewData *view_data,
                                                         struct DrawEngineType *engine_type_);
void DRW_view_data_use_engine(DRWViewData *view_data, struct DrawEngineType *engine_type);
void DRW_view_data_reset(DRWViewData *view_data);
void DRW_view_data_free_unused(DRWViewData *view_data);
void DRW_view_data_engines_view_update(DRWViewData *view_data);
double *DRW_view_data_cache_time_get(DRWViewData *view_data);
DefaultFramebufferList *DRW_view_data_default_framebuffer_list_get(DRWViewData *view_data);
DefaultTextureList *DRW_view_data_default_texture_list_get(DRWViewData *view_data);

typedef struct DRWEngineIterator {
  int id, end;
  ViewportEngineData **engines;
} DRWEngineIterator;

/* Iterate over used engines of this view_data. */
void DRW_view_data_enabled_engine_iter_begin(DRWEngineIterator *iterator, DRWViewData *view_data);
ViewportEngineData *DRW_view_data_enabled_engine_iter_step(DRWEngineIterator *iterator);

#define DRW_ENABLED_ENGINE_ITER(view_data_, engine_, data_) \
  DRWEngineIterator iterator; \
  ViewportEngineData *data_; \
  struct DrawEngineType *engine_; \
  DRW_view_data_enabled_engine_iter_begin(&iterator, view_data_); \
  /* WATCH Comma operator trickery ahead! This tests engine_ == NULL. */ \
  while ((data_ = DRW_view_data_enabled_engine_iter_step(&iterator), \
          engine_ = (data_ != NULL) ? (struct DrawEngineType *)data_->engine_type->draw_engine : \
                                      NULL))

#ifdef __cplusplus
}
#endif
